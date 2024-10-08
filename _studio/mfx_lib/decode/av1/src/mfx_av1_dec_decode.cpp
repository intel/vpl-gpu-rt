// Copyright (c) 2017-2024 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"

#if defined(MFX_ENABLE_AV1_VIDEO_DECODE)

#include "mfx_session.h"
#include "mfx_av1_dec_decode.h"

#include "mfx_task.h"

#include "mfx_common_int.h"
#include "mfx_common_decode_int.h"
#include "mfx_vpx_dec_common.h"
#include "umc_va_video_processing.h"

#include "mfx_umc_alloc_wrapper.h"

#include "umc_av1_dec_defs.h"
#include "umc_av1_frame.h"
#include "umc_av1_utils.h"

#include "libmfx_core_hw.h"

#include "umc_va_base.h"

#include "umc_av1_decoder_va.h"

#include <algorithm>

#include "libmfx_core_interface.h"


#include "mfx_unified_av1d_logging.h"
#if defined(MFX_ENABLE_PXP)
#include "umc_va_protected.h"
#endif // MFX_ENABLE_PXP

namespace MFX_VPX_Utility
{
    inline
    mfxU16 MatchProfile(mfxU32)
    {
        return MFX_PROFILE_AV1_MAIN;
    }


    inline
    bool CheckGUID(VideoCORE* core, eMFXHWType type, mfxVideoParam const* par)
    {
        mfxVideoParam vp = *par;
        mfxU16 profile = vp.mfx.CodecProfile & 0xFF;
        if (profile == MFX_PROFILE_UNKNOWN)
        {
            profile = MatchProfile(vp.mfx.FrameInfo.FourCC);;
            vp.mfx.CodecProfile = profile;
        }

        if (core->IsGuidSupported(DXVA_Intel_ModeAV1_VLD, &vp) != MFX_ERR_NONE)
            return false;

        switch (profile)
        {
            case MFX_PROFILE_AV1_MAIN:
                return true;
            default:
                return false;
        }
    }

    eMFXPlatform GetPlatform(VideoCORE* core, mfxVideoParam const* par)
    {
        assert(core);
        assert(par);

        if (!par)
            return MFX_PLATFORM_SOFTWARE;

        if(IsD3D9Simulation(*core))
            return MFX_PLATFORM_SOFTWARE;

        eMFXPlatform platform = core->GetPlatformType();
        return
            platform != MFX_PLATFORM_SOFTWARE && !CheckGUID(core, core->GetHWType(), par) ?
            MFX_PLATFORM_SOFTWARE : platform;
    }
}

static void SetFrameType(const UMC_AV1_DECODER::AV1DecoderFrame& frame, mfxFrameSurface1 &surface_out)
{
    auto extFrameInfo = reinterpret_cast<mfxExtDecodedFrameInfo *>(GetExtendedBuffer(surface_out.Data.ExtParam, surface_out.Data.NumExtParam, MFX_EXTBUFF_DECODED_FRAME_INFO));
    if (extFrameInfo == nullptr)
        return;

    const UMC_AV1_DECODER::FrameHeader& fh = frame.GetFrameHeader();
    switch (fh.frame_type)
    {
    case UMC_AV1_DECODER::KEY_FRAME:
        extFrameInfo->FrameType = MFX_FRAMETYPE_I;
        break;
    case UMC_AV1_DECODER::INTER_FRAME:
    case UMC_AV1_DECODER::INTRA_ONLY_FRAME:
    case UMC_AV1_DECODER::SWITCH_FRAME:
        extFrameInfo->FrameType = MFX_FRAMETYPE_P;
        break;
    default:
        extFrameInfo->FrameType = MFX_FRAMETYPE_UNKNOWN;
    }
}

VideoDECODEAV1::VideoDECODEAV1(VideoCORE* core, mfxStatus* sts)
    : m_core(core)
    , m_first_run(true)
    , m_request()
    , m_response()
    , m_response_alien()
    , m_is_init(false)
    , m_in_framerate(0)
    , m_is_cscInUse(false)
    , m_anchorFramesSource(0)
    , m_va(nullptr)
{
    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

VideoDECODEAV1::~VideoDECODEAV1()
{
    if (m_is_init)
    {
        Close();
    }
}

inline
mfxU16 av1_mfx_profile_to_native_profile(mfxU16 mfx)
{
    switch (mfx)
    {
        case MFX_PROFILE_AV1_MAIN:
            return 0;
        case MFX_PROFILE_AV1_HIGH:
            return 1;
        case MFX_PROFILE_AV1_PRO:
            return 2;
        default:
            return mfx;
    }
}

mfxStatus VideoDECODEAV1::Init(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);

    std::lock_guard<std::mutex> guard(m_guard);

    MFX_CHECK(!m_decoder, MFX_ERR_UNDEFINED_BEHAVIOR);

    eMFXPlatform platform = MFX_VPX_Utility::GetPlatform(m_core, par);

    MFX_CHECK(platform == MFX_PLATFORM_HARDWARE, MFX_ERR_UNSUPPORTED);
    eMFXHWType type = m_core->GetHWType();

    MFX_CHECK(CheckVideoParamDecoders(par, type) >= MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(MFX_VPX_Utility::CheckVideoParam(par, MFX_CODEC_AV1), MFX_ERR_INVALID_VIDEO_PARAM);

    m_first_par = (mfxVideoParamWrapper)(*par);
    m_video_par = m_first_par;

    m_decoder.reset(new UMC_AV1_DECODER::AV1DecoderVA());

    m_request = {};
    m_response = {};
    m_response_alien = {};

    mfxStatus sts = MFX_VPX_Utility::QueryIOSurfInternal(par, &m_request);
    uint32_t dpb_size = std::max(8 + par->AsyncDepth, 8);
    if (dpb_size >= m_request.NumFrameSuggested)
    {
        m_request.NumFrameSuggested = mfxU16(dpb_size + 1);
    }

    MFX_CHECK_STS(sts);

    m_init_par = (mfxVideoParamWrapper)(*par);

    m_first_par = m_init_par;
    m_in_framerate = (m_first_par.mfx.FrameInfo.FrameRateExtD && m_first_par.mfx.FrameInfo.FrameRateExtN) ?
        ((mfxF64) m_first_par.mfx.FrameInfo.FrameRateExtD / m_first_par.mfx.FrameInfo.FrameRateExtN) : (mfxF64)1.0 / 30;
    m_decoder->SetInFrameRate(m_in_framerate);

    //mfxFrameAllocResponse response{};
    bool internal = par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    if (videoProcessing)
    {
#ifndef ENABLE_AV1D_POST_PROCESSING
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
#else
        MFX_CHECK(AV1DCaps::IsPostProcessSupported(m_core->GetHWType()) &&
            (m_video_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY),
            MFX_ERR_UNSUPPORTED);
        if (par->mfx.FrameInfo.FourCC != videoProcessing->Out.FourCC)//csc is in use
            m_is_cscInUse = true;

        bool is_fourcc_supported = false;
        is_fourcc_supported =
            (videoProcessing->Out.FourCC == MFX_FOURCC_RGB4
                || videoProcessing->Out.FourCC == MFX_FOURCC_RGBP
                || videoProcessing->Out.FourCC == MFX_FOURCC_NV12
                || videoProcessing->Out.FourCC == MFX_FOURCC_P010
                || videoProcessing->Out.FourCC == MFX_FOURCC_YUY2
                || videoProcessing->Out.FourCC == MFX_FOURCC_AYUV
                || videoProcessing->Out.FourCC == MFX_FOURCC_Y410
                || videoProcessing->Out.FourCC == MFX_FOURCC_Y210
                || videoProcessing->Out.FourCC == MFX_FOURCC_Y216
                || videoProcessing->Out.FourCC == MFX_FOURCC_Y416
                || videoProcessing->Out.FourCC == MFX_FOURCC_P016
                );
        
        MFX_CHECK(is_fourcc_supported, MFX_ERR_UNSUPPORTED);
        if (m_core->GetVAType() == MFX_HW_VAAPI)
            internal = 1;
#endif
    }
#endif

    // set surface request type
    m_request.Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;

    if (internal)
        m_request.Type |= MFX_MEMTYPE_INTERNAL_FRAME;
    else
        m_request.Type |= MFX_MEMTYPE_EXTERNAL_FRAME;

    mfxFrameAllocRequest request_internal = m_request;

    {
        if (!internal)
            m_request.AllocId = par->AllocId;
    }

    MFX_CHECK_STS(sts);

    try
    {
        m_surface_source.reset(new SurfaceSource(m_core, *par, platform, m_request, request_internal, m_response, m_response_alien));
    }
    catch (const std::system_error & ex)
    {
        MFX_CHECK_STS(mfxStatus(ex.code().value()));
    }

    UMC_AV1_DECODER::AV1DecoderParams vp{};
    vp.allocator = m_surface_source.get();
    vp.async_depth = par->AsyncDepth;
    vp.film_grain = par->mfx.FilmGrain ? 1 : 0; // 0 - film grain is forced off, 1 - film grain is controlled by apply_grain syntax parameter
    if (!vp.async_depth)
        vp.async_depth = MFX_AUTO_ASYNC_DEPTH_VALUE;
    vp.io_pattern = par->IOPattern;


    sts = m_core->CreateVA(par, &m_request, &m_response, m_surface_source.get());
    MFX_CHECK_STS(sts);

    m_core->GetVA((mfxHDL*)&m_va, MFX_MEMTYPE_FROM_DECODE);
    vp.pVideoAccelerator = m_va;

    ConvertMFXParamsToUMC(par, &vp);
    vp.info.profile = av1_mfx_profile_to_native_profile(par->mfx.CodecProfile);

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    if (m_va->GetVideoProcessingVA())
    {
        UMC::Status umcSts = m_va->GetVideoProcessingVA()->Init(par, videoProcessing);
        MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_INVALID_VIDEO_PARAM);
    }
#endif

    UMC::Status umcSts = m_decoder->Init(&vp);
    MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_NOT_INITIALIZED);

    m_first_run = true;
    m_is_init = true;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEAV1::QueryImplsDescription(
    VideoCORE&,
    mfxDecoderDescription::decoder& caps,
    mfx::PODArraysHolder& ah)
{
    const mfxU32 SupportedProfiles[] =
    {
        MFX_PROFILE_AV1_MAIN
        , MFX_PROFILE_AV1_HIGH
        , MFX_PROFILE_AV1_PRO
    };
    const mfxResourceType SupportedMemTypes[] =
    {
        MFX_RESOURCE_SYSTEM_SURFACE
        , MFX_RESOURCE_VA_SURFACE
    };
    const mfxU32 SupportedFourCC[] =
    {
        MFX_FOURCC_NV12
        , MFX_FOURCC_P010
    };

    caps.CodecID = MFX_CODEC_AV1;
    caps.MaxcodecLevel = MFX_LEVEL_AV1_63;

    for (mfxU32 profile : SupportedProfiles)
    {
        auto& pfCaps = ah.PushBack(caps.Profiles);
        pfCaps.Profile = profile;

        for (auto memType : SupportedMemTypes)
        {
            auto& memCaps = ah.PushBack(pfCaps.MemDesc);
            memCaps.MemHandleType = memType;
            memCaps.Width = { 16, 16384, 16 };
            memCaps.Height = { 16, 16384, 16 };

            for (auto fcc : SupportedFourCC)
            {
                ah.PushBack(memCaps.ColorFormats) = fcc;
                ++memCaps.NumColorFormats;
            }
            ++pfCaps.NumMemTypes;
        }
        ++caps.NumProfiles;
    }

    return MFX_ERR_NONE;
}


// Check if new parameters are compatible with old parameters
bool VideoDECODEAV1::IsNeedChangeVideoParam(mfxVideoParam * newPar, mfxVideoParam * oldPar, eMFXHWType /*type*/) const
{
    auto const mask =
          MFX_IOPATTERN_OUT_SYSTEM_MEMORY
        | MFX_IOPATTERN_OUT_VIDEO_MEMORY
        ;
    if ((newPar->IOPattern & mask) !=
        (oldPar->IOPattern & mask) )
    {
        return false;
    }

    if (CalculateAsyncDepth(newPar) != CalculateAsyncDepth(oldPar))
    {
        return false;
    }

    mfxFrameAllocRequest requestOld{};
    mfxFrameAllocRequest requestNew{};

    mfxStatus mfxSts = MFX_VPX_Utility::QueryIOSurfInternal(oldPar, &requestOld);

    if (mfxSts != MFX_ERR_NONE)
        return false;

    mfxSts = MFX_VPX_Utility::QueryIOSurfInternal(newPar, &requestNew);

    if (mfxSts != MFX_ERR_NONE)
        return false;

    if (newPar->mfx.FrameInfo.Height > oldPar->mfx.FrameInfo.Height)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.Width > oldPar->mfx.FrameInfo.Width)
    {
        return false;
    }

    if (m_response.NumFrameActual)
    {
        if (requestNew.NumFrameMin > m_response.NumFrameActual)
            return false;
    }
    else
    {
        if (requestNew.NumFrameMin > requestOld.NumFrameMin || requestNew.Type != requestOld.Type)
            return false;
    }

    if (newPar->mfx.FrameInfo.FourCC != oldPar->mfx.FrameInfo.FourCC)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.ChromaFormat != oldPar->mfx.FrameInfo.ChromaFormat)
    {
        return false;
    }


#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * newVideoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(newPar->ExtParam, newPar->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    mfxExtDecVideoProcessing * oldVideoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(oldPar->ExtParam, oldPar->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);

    if (((newVideoProcessing) && (!oldVideoProcessing)) ||
        ((!newVideoProcessing) && (oldVideoProcessing)))
        return false;
    else if (newVideoProcessing && oldVideoProcessing)
    {
        if (newVideoProcessing->Out.Width > oldVideoProcessing->Out.Width)
            return false;
        if (newVideoProcessing->Out.Height > oldVideoProcessing->Out.Height)
            return false;
        /* Check Input cropping */
        if (!((newVideoProcessing->In.CropX <= newVideoProcessing->In.CropW) &&
            (newVideoProcessing->In.CropW <= newPar->mfx.FrameInfo.CropW) &&
            (newVideoProcessing->In.CropY <= newVideoProcessing->In.CropH) &&
            (newVideoProcessing->In.CropH <= newPar->mfx.FrameInfo.CropH)))
            return false;

        /* Check output cropping */
        if (!((newVideoProcessing->Out.CropX <= newVideoProcessing->Out.CropW) &&
            (newVideoProcessing->Out.CropW <= newVideoProcessing->Out.Width) &&
            ((newVideoProcessing->Out.CropX + newVideoProcessing->Out.CropH)
                <= newVideoProcessing->Out.Width) &&
                (newVideoProcessing->Out.CropY <= newVideoProcessing->Out.CropH) &&
            (newVideoProcessing->Out.CropH <= newVideoProcessing->Out.Height) &&
            ((newVideoProcessing->Out.CropY + newVideoProcessing->Out.CropH)
                <= newVideoProcessing->Out.Height)))
            return false;
    }
#endif //MFX_DEC_VIDEO_POSTPROCESS_DISABLE

    return true;
}


mfxStatus VideoDECODEAV1::Reset(mfxVideoParam* par)
{
    std::lock_guard<std::mutex> guard(m_guard);

    MFX_CHECK_NULL_PTR1(par);

    MFX_CHECK(m_is_init, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);

    eMFXHWType type = m_core->GetHWType();
#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(m_first_par.ExtParam, m_first_par.NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);

    if (videoProcessing != nullptr)
    {
        // hardware resize is enabled
        bool hardwareUpscale =
            videoProcessing->Out.Width >= par->mfx.FrameInfo.Width ||
            videoProcessing->Out.Height >= par->mfx.FrameInfo.Height;

        if (hardwareUpscale)
        {
            // for now only downscale is supported
            // at least Windows DirectX 11 provides only downscale interface
            // ID3D11VideoContext1->DecoderEnableDownsampling()
            MFX_RETURN( MFX_ERR_INVALID_VIDEO_PARAM);
        }
    }
#endif
    eMFXPlatform platform = MFX_VPX_Utility::GetPlatform(m_core, par);
    MFX_CHECK(platform == MFX_PLATFORM_HARDWARE, MFX_ERR_UNSUPPORTED);

    MFX_CHECK(CheckVideoParamDecoders(par, type) >= MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(MFX_VPX_Utility::CheckVideoParam(par, MFX_CODEC_AV1, platform), MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(IsNeedChangeVideoParam(par, &m_init_par, type), MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    MFX_CHECK(m_surface_source->Reset() == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);

    m_first_par = *par;

    m_in_framerate = (m_first_par.mfx.FrameInfo.FrameRateExtD && m_first_par.mfx.FrameInfo.FrameRateExtN) ?
        ((mfxF64)m_first_par.mfx.FrameInfo.FrameRateExtD / m_first_par.mfx.FrameInfo.FrameRateExtN) : (mfxF64)1.0 / 30;

    m_decoder->SetInFrameRate(m_in_framerate);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEAV1::Close()
{
    std::lock_guard<std::mutex> guard(m_guard);

    MFX_CHECK(m_is_init, MFX_ERR_NOT_INITIALIZED);

    m_decoder.reset();
    m_surface_source->Close();

    m_request = {};
    m_response = {};
    m_response_alien = {};

    m_is_init = false;
    m_va = nullptr;

    return MFX_ERR_NONE;
}

mfxTaskThreadingPolicy VideoDECODEAV1::GetThreadingPolicy()
{
    return MFX_TASK_THREADING_SHARED;
}

inline
mfxStatus CheckLevel(mfxVideoParam* in, mfxVideoParam* out)
{
    MFX_CHECK_NULL_PTR1(out);

    mfxStatus sts = MFX_ERR_NONE;

    if (in)
    {
        switch(in->mfx.CodecLevel)
        {
        case MFX_LEVEL_AV1_2:
        case MFX_LEVEL_AV1_21:
        case MFX_LEVEL_AV1_22:
        case MFX_LEVEL_AV1_23:
        case MFX_LEVEL_AV1_3:
        case MFX_LEVEL_AV1_31:
        case MFX_LEVEL_AV1_32:
        case MFX_LEVEL_AV1_33:
        case MFX_LEVEL_AV1_4:
        case MFX_LEVEL_AV1_41:
        case MFX_LEVEL_AV1_42:
        case MFX_LEVEL_AV1_43:
        case MFX_LEVEL_AV1_5:
        case MFX_LEVEL_AV1_51:
        case MFX_LEVEL_AV1_52:
        case MFX_LEVEL_AV1_53:
        case MFX_LEVEL_AV1_6:
        case MFX_LEVEL_AV1_61:
        case MFX_LEVEL_AV1_62:
        case MFX_LEVEL_AV1_63:
            out->mfx.CodecLevel = in->mfx.CodecLevel;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }
    }
    else
        out->mfx.CodecLevel = MFX_LEVEL_AV1_2;

    return sts;
}

mfxStatus VideoDECODEAV1::Query(VideoCORE* core, mfxVideoParam* in, mfxVideoParam* out)
{
    MFX_CHECK(core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_NULL_PTR1(out);

    if (in)
    {
        MFX_CHECK((in->mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF | MFX_PICSTRUCT_FIELD_SINGLE)) == 0, MFX_ERR_UNSUPPORTED);
        MFX_CHECK(in->Protected == 0, MFX_ERR_UNSUPPORTED);
        MFX_CHECK(in->Protected == 0, MFX_ERR_UNSUPPORTED);
        MFX_CHECK(in->mfx.ExtendedPicStruct == 0, MFX_ERR_UNSUPPORTED);
    }

    mfxStatus sts = MFX_VPX_Utility::Query(core, in, out, MFX_CODEC_AV1, core->GetHWType());
    MFX_CHECK_STS(sts);

    eMFXPlatform platform = MFX_VPX_Utility::GetPlatform(core, out);
    if (platform != core->GetPlatformType())
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    sts = CheckLevel(in, out);
    MFX_CHECK_STS(sts);

    if (in)
        out->mfx.FilmGrain = in->mfx.FilmGrain;
    else
        out->mfx.FilmGrain = 1;

    MFX_CHECK_STS(sts);
    return sts;
}

mfxStatus VideoDECODEAV1::QueryIOSurf(VideoCORE* core, mfxVideoParam* par, mfxFrameAllocRequest* request)
{
    MFX_CHECK(core, MFX_ERR_UNDEFINED_BEHAVIOR)
    MFX_CHECK_NULL_PTR2(par, request);

    auto const supportedMemoryType =
           (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        || (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    MFX_CHECK(supportedMemoryType, MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(!(
        par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY &&
        par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY),
        MFX_ERR_INVALID_VIDEO_PARAM);

    mfxStatus sts = MFX_ERR_NONE;
    if (!(par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        sts = MFX_VPX_Utility::QueryIOSurfInternal(par, request);
        MFX_CHECK_STS(sts);
        uint32_t dpb_size = std::max(8 + par->AsyncDepth, 8);
        if (dpb_size >= request->NumFrameSuggested)
        {
            request->NumFrameSuggested = mfxU16(dpb_size + 1);
        }
    }
    else
    {
        request->Info = par->mfx.FrameInfo;
        request->NumFrameMin = 1;
        request->NumFrameSuggested = request->NumFrameMin + (par->AsyncDepth ? par->AsyncDepth : MFX_AUTO_ASYNC_DEPTH_VALUE);
        request->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }

    sts = UpdateCscOutputFormat(par, request);
    MFX_CHECK_STS(sts);

    request->Type |= MFX_MEMTYPE_EXTERNAL_FRAME;

    return sts;
}

inline
UMC::Status FillParam(mfxVideoParam *par)
{
    if (par->mfx.FrameInfo.FourCC == MFX_FOURCC_P010
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y210
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_P016
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y216
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y416)
        par->mfx.FrameInfo.Shift = 1;

    return UMC::UMC_OK;
}

mfxStatus VideoDECODEAV1::DecodeHeader(VideoCORE* core, mfxBitstream* bs, mfxVideoParam* par)
{
    MFX_CHECK(core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_NULL_PTR3(bs, bs->Data, par);

    mfxStatus sts = MFX_ERR_NONE;

    try
    {
        MFXMediaDataAdapter in(bs);

        UMC_AV1_DECODER::AV1DecoderParams vp;
        sts = ConvertStatusUmc2Mfx(UMC_AV1_DECODER::AV1Decoder::DecodeHeader(&in, vp));
        if (sts == MFX_ERR_MORE_DATA || sts == MFX_ERR_MORE_SURFACE)
            return sts;
        MFX_CHECK_STS(sts);

        sts = FillVideoParam(&vp, par);
        if (sts == MFX_ERR_MORE_DATA || sts == MFX_ERR_MORE_SURFACE)
            return sts;
        MFX_CHECK_STS(sts);
    }
    catch (UMC_AV1_DECODER::av1_exception const &ex)
    {
        sts = ConvertStatusUmc2Mfx(ex.GetStatus());
    }


    return sts;
}

mfxStatus VideoDECODEAV1::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    std::lock_guard<std::mutex> guard(m_guard);

    mfxStatus sts = MFX_ERR_NONE;
    UMC_AV1_DECODER::AV1DecoderParams vp;

    UMC::Status umcRes = m_decoder->GetInfo(&vp);
    MFX_CHECK(umcRes == UMC::UMC_OK, MFX_ERR_UNKNOWN);

    // fills par->mfx structure
    sts = FillVideoParam(&vp, par);
    MFX_CHECK_STS(sts);

    par->AsyncDepth = static_cast<mfxU16>(vp.async_depth);
    par->IOPattern = static_cast<mfxU16>(vp.io_pattern  & (
          MFX_IOPATTERN_OUT_VIDEO_MEMORY
        | MFX_IOPATTERN_OUT_SYSTEM_MEMORY));

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    if (videoProcessing)
    {
        mfxExtDecVideoProcessing * videoProcessingInternal = m_video_par.GetExtendedBuffer<mfxExtDecVideoProcessing>(MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
        *videoProcessing = *videoProcessingInternal;
    }
#endif

    par->mfx.FrameInfo.FrameRateExtN = m_init_par.mfx.FrameInfo.FrameRateExtN;
    par->mfx.FrameInfo.FrameRateExtD = m_init_par.mfx.FrameInfo.FrameRateExtD;

    if (!par->mfx.FrameInfo.AspectRatioH && !par->mfx.FrameInfo.AspectRatioW)
    {
        if (m_init_par.mfx.FrameInfo.AspectRatioH || m_init_par.mfx.FrameInfo.AspectRatioW)
        {
            par->mfx.FrameInfo.AspectRatioH = m_init_par.mfx.FrameInfo.AspectRatioH;
            par->mfx.FrameInfo.AspectRatioW = m_init_par.mfx.FrameInfo.AspectRatioW;
        }
        else
        {
            par->mfx.FrameInfo.AspectRatioH = 1;
            par->mfx.FrameInfo.AspectRatioW = 1;
        }
    }

    switch (par->mfx.FrameInfo.FourCC)
    {
    case MFX_FOURCC_P010:
    case MFX_FOURCC_Y210:
    case MFX_FOURCC_P016:
    case MFX_FOURCC_Y216:
    case MFX_FOURCC_Y416:
        par->mfx.FrameInfo.Shift = 1;
    default:
        break;
    }

    return MFX_ERR_NONE;
}


mfxStatus VideoDECODEAV1::GetDecodeStat(mfxDecodeStat* stat)
{
    MFX_CHECK_NULL_PTR1(stat);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEAV1::DecodeFrameCheck(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, MFX_ENTRY_POINT* entry_point)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);
    MFX_CHECK_NULL_PTR1(entry_point);

    std::lock_guard<std::mutex> guard(m_guard);
    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

#if defined(MFX_ENABLE_PXP)
    //Check protect VA is enabled or not
    if( bs && m_va && m_va->GetProtectedVA())
    {
        MFX_CHECK((bs->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME), MFX_ERR_UNSUPPORTED);
        m_va->GetProtectedVA()->SetBitstream(bs);
    }
#endif // MFX_ENABLE_PXP

    mfxStatus sts = SubmitFrame(bs, surface_work, surface_out);

    if (sts == MFX_ERR_MORE_DATA || sts == MFX_ERR_MORE_SURFACE)
        return sts;

    MFX_CHECK_STS(sts);

    std::unique_ptr<TaskInfo> info(new TaskInfo);

    info->copyfromframe = UMC::FRAME_MID_INVALID;
    info->surface_work = surface_work;
    if (*surface_out)
        info->surface_out = *surface_out;

    if(m_decoder->GetRepeatedFrame() != UMC::FRAME_MID_INVALID){
        info->copyfromframe = m_decoder->GetRepeatedFrame();
    }

    mfxThreadTask task =
        reinterpret_cast<mfxThreadTask>(info.release());

    entry_point->pRoutine = &DecodeRoutine;
    entry_point->pCompleteProc = &CompleteProc;
    entry_point->pState = this;
    entry_point->requiredNumThreads = 1;
    entry_point->pParam = task;
    return sts;
}

mfxStatus VideoDECODEAV1::SetSkipMode(mfxSkipMode /*mode*/)
{
    MFX_RETURN(MFX_ERR_UNSUPPORTED);
}

mfxStatus VideoDECODEAV1::GetPayload(mfxU64* /*time_stamp*/, mfxPayload* /*pPayload*/)
{
    MFX_RETURN(MFX_ERR_UNSUPPORTED);
}

mfxStatus VideoDECODEAV1::DecodeFrame(mfxFrameSurface1 *surface_out, AV1DecoderFrame* frame)
{
    MFX_CHECK(surface_out, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(frame, MFX_ERR_UNDEFINED_BEHAVIOR);
    surface_out->Data.Corrupted = 0;
    int32_t const error = frame->GetError();

    if (error & UMC::ERROR_FRAME_DEVICE_FAILURE)
    {
        surface_out->Data.Corrupted |= MFX_CORRUPTION_MAJOR;
        MFX_CHECK(error != UMC::UMC_ERR_GPU_HANG, MFX_ERR_GPU_HANG);
        MFX_RETURN(MFX_ERR_DEVICE_FAILED);
    }
    else
    {
        if (error & UMC::ERROR_FRAME_MINOR)
            surface_out->Data.Corrupted |= MFX_CORRUPTION_MINOR;

        if (error & UMC::ERROR_FRAME_MAJOR)
            surface_out->Data.Corrupted |= MFX_CORRUPTION_MAJOR;

        if (error & UMC::ERROR_FRAME_REFERENCE_FRAME)
            surface_out->Data.Corrupted |= MFX_CORRUPTION_REFERENCE_FRAME;

        if (error & UMC::ERROR_FRAME_DPB)
            surface_out->Data.Corrupted |= MFX_CORRUPTION_REFERENCE_LIST;

        if (error & UMC::ERROR_FRAME_RECOVERY)
            surface_out->Data.Corrupted |= MFX_CORRUPTION_MAJOR;

        if (error & UMC::ERROR_FRAME_TOP_FIELD_ABSENT)
            surface_out->Data.Corrupted |= MFX_CORRUPTION_ABSENT_TOP_FIELD;

        if (error & UMC::ERROR_FRAME_BOTTOM_FIELD_ABSENT)
            surface_out->Data.Corrupted |= MFX_CORRUPTION_ABSENT_BOTTOM_FIELD;
    }

    UMC::FrameMemID id = frame->GetFrameData()->GetFrameMID();
    mfxStatus sts = m_surface_source->PrepareToOutput(surface_out, id, &m_video_par);
    frame->Displayed(true);

    TRACE_EVENT(MFX_TRACE_API_AV1_DISPLAYINFO_TASK, EVENT_TYPE_INFO, TR_KEY_DECODE_BASIC_INFO, make_event_data(
        id, (uint32_t)frame->Displayed(), (uint32_t)frame->Outputted()));

    return sts;
}

mfxStatus VideoDECODEAV1::QueryFrame(mfxThreadTask task)
{
    MFX_CHECK_NULL_PTR1(task);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    auto info =
        reinterpret_cast<TaskInfo*>(task);
    UMC_AV1_DECODER::AV1DecoderFrame* frame = NULL;
    mfxFrameSurface1* surface_out = info->surface_out;
    MFX_CHECK_NULL_PTR1(surface_out);
    MFX_CHECK(surface_out, MFX_ERR_INVALID_HANDLE);

    if(info->copyfromframe != UMC::FRAME_MID_INVALID)
    {
        frame = m_decoder->DecodeFrameID(info->copyfromframe);
        MFX_CHECK(frame, MFX_ERR_UNDEFINED_BEHAVIOR);
        frame->Repeated(false);
    }
    else
    {
        MFX_CHECK(surface_out, MFX_ERR_UNDEFINED_BEHAVIOR);
        UMC::FrameMemID id = m_surface_source->FindSurface(surface_out);
        frame = m_decoder->FindFrameByMemID(id);
        MFX_CHECK(frame, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(frame->DecodingStarted(), MFX_ERR_UNDEFINED_BEHAVIOR);
        if (!frame->DecodingCompleted())
        {
            m_decoder->QueryFrames();
        }

        MFX_CHECK(frame->DecodingCompleted(), MFX_TASK_WORKING);
    }
    mfxStatus sts = DecodeFrame(surface_out, frame);

    m_decoder->Flush();

    MFX_CHECK_STS(sts);
    return MFX_TASK_DONE;
}

static mfxStatus CheckFrameInfo(mfxFrameInfo &info)
{
    MFX_SAFE_CALL(CheckFrameInfoCommon(&info, MFX_CODEC_AV1));

    switch (info.FourCC)
    {
    case MFX_FOURCC_NV12:
        MFX_CHECK(info.ChromaFormat == MFX_CHROMAFORMAT_YUV420, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(info.Shift == 0, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        break;
    case MFX_FOURCC_P010:
        MFX_CHECK(info.ChromaFormat == MFX_CHROMAFORMAT_YUV420, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(info.Shift == 1, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        break;
    default:
        MFX_CHECK_STS(MFX_ERR_INVALID_VIDEO_PARAM);
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEAV1::SubmitFrame(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);
    bool allow_null_work_surface = SupportsVPLFeatureSet(*m_core);

    if (allow_null_work_surface)
    {
        MFX_CHECK_NULL_PTR1(surface_out);
    }
    else
    {
        MFX_CHECK_NULL_PTR2(surface_work, surface_out);
    }

    if (surface_work)
    {
        bool workSfsIsEmpty = IsSurfaceEmpty(*surface_work);

        MFX_CHECK(!workSfsIsEmpty, MFX_ERR_LOCK_MEMORY);

        mfxStatus sts = MFX_ERR_NONE;
        if (m_is_cscInUse != true)
        {
            sts = CheckFrameInfo(surface_work->Info);
            MFX_CHECK_STS(sts);
        }

        sts = CheckFrameData(surface_work);
        MFX_CHECK_STS(sts);
    }

    if (!bs)
        MFX_RETURN(MFX_ERR_MORE_DATA);

    mfxStatus sts = CheckBitstream(bs);
    MFX_CHECK_STS(sts);

    try
    {
        MFXMediaDataAdapter src(bs);
        m_decoder->SetVideoCore(m_core);

        for (;;)
        {
            sts = m_surface_source->SetCurrentMFXSurface(surface_work);
            MFX_CHECK_STS(sts);
            m_decoder->SetVideoCore(m_core);
            UMC::Status umcRes = m_surface_source->HasFreeSurface() ?
                m_decoder->GetFrame(bs ? &src : 0, nullptr) : UMC::UMC_ERR_NEED_FORCE_OUTPUT;

            UMC::Status umcFrameRes = umcRes;

            if (umcRes == UMC::UMC_NTF_NEW_RESOLUTION ||
                 umcRes == UMC::UMC_WRN_REPOSITION_INPROGRESS ||
                 umcRes == UMC::UMC_ERR_UNSUPPORTED)
            {
                 UMC_AV1_DECODER::AV1DecoderParams vp;
                 umcRes = m_decoder->GetInfo(&vp);
                 FillVideoParam(&vp, &m_video_par);
                 // Realloc surface here for LST case which DRC happended in the last frame
                 if (surface_work && vp.lst_mode &&
                     (m_video_par.mfx.FrameInfo.Width > surface_work->Info.Width ||
                      m_video_par.mfx.FrameInfo.Height > surface_work->Info.Height))
                 {
                     MFX_RETURN(MFX_ERR_REALLOC_SURFACE);
                 }
                 m_video_par.AsyncDepth = static_cast<mfxU16>(vp.async_depth);
            }

            if (umcFrameRes == UMC::UMC_NTF_NEW_RESOLUTION)
            {
                MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            }

            if (umcRes == UMC::UMC_ERR_INVALID_STREAM)
            {
                umcRes = UMC::UMC_OK;
            }

            switch (umcRes)
            {
                case UMC::UMC_OK:
                    if (!m_surface_source->HasFreeSurface())
                    {
                        sts = MFX_ERR_MORE_SURFACE;
                        umcFrameRes = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
                    }
                    break;

                case UMC::UMC_NTF_NEW_RESOLUTION:
                    MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                case UMC::UMC_ERR_DEVICE_FAILED:
                    sts = MFX_ERR_DEVICE_FAILED;
                    break;

                case UMC::UMC_ERR_GPU_HANG:
                    sts = MFX_ERR_GPU_HANG;
                    break;

                case UMC::UMC_ERR_NOT_ENOUGH_BUFFER:
                case UMC::UMC_WRN_INFO_NOT_READY:
                case UMC::UMC_ERR_NEED_FORCE_OUTPUT:
                    sts = umcRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER ? (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK: MFX_WRN_DEVICE_BUSY;
                    break;

                case UMC::UMC_ERR_NOT_ENOUGH_DATA:
                case UMC::UMC_ERR_SYNC:
                    sts = MFX_ERR_MORE_DATA;
                    break;
                case UMC::UMC_ERR_INVALID_PARAMS:
                    sts = MFX_ERR_INVALID_VIDEO_PARAM;
                    break;

                default:
                    if (sts < 0 || umcRes < 0)
                        sts = MFX_ERR_UNDEFINED_BEHAVIOR;
                    break;
            }


            src.Save(bs);

            if (sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
            {
                MFX_CHECK_STS(sts);
            }

            if (sts == MFX_ERR_DEVICE_FAILED || sts == MFX_ERR_GPU_HANG || sts == MFX_ERR_UNDEFINED_BEHAVIOR)
            {
                MFX_CHECK_STS(sts);
            }

            UMC_AV1_DECODER::AV1DecoderFrame* frame = m_decoder->GetCurrFrame();
            if (frame && bs->TimeStamp != static_cast<mfxU64>(MFX_TIMESTAMP_UNKNOWN))
                frame->SetFrameTime(GetUmcTimeStamp(bs->TimeStamp));

            frame = GetFrameToDisplay();
            if (frame)
            {
                if (frame->Skipped())
                {
                    sts = MFX_ERR_MORE_DATA;
                    break;
                }
                sts = FillOutputSurface(surface_out, surface_work, frame);
                return MFX_ERR_NONE;
            }

            if (umcFrameRes != UMC::UMC_OK)
            {
                break;
            }

        } // for (;;)
    }
    catch(UMC_AV1_DECODER::av1_exception const &ex)
    {
        sts = ConvertUMCStatusToMfx(ex.GetStatus());
    }
    catch (std::bad_alloc const&)
    {
        sts = MFX_ERR_MEMORY_ALLOC;
    }
    catch(...)
    {
        sts = MFX_ERR_UNKNOWN;
    }

    if (sts == MFX_ERR_MORE_DATA)
        return sts;

    MFX_CHECK_STS(sts);

    return sts;
}

mfxStatus VideoDECODEAV1::DecodeRoutine(void* state, void* param, mfxU32, mfxU32)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "AV1DECODERoutine");
    mfxStatus sts = MFX_ERR_NONE;

    try
    {
        auto decoder = reinterpret_cast<VideoDECODEAV1*>(state);
        MFX_CHECK(decoder, MFX_ERR_UNDEFINED_BEHAVIOR);

        mfxThreadTask task =
            reinterpret_cast<TaskInfo*>(param);

        MFX_CHECK(task, MFX_ERR_UNDEFINED_BEHAVIOR);

        sts = decoder->QueryFrame(task);
        MFX_CHECK_STS(sts);
    }
    catch(...)
    {
        MFX_LTRACE_MSG_1(MFX_TRACE_LEVEL_INTERNAL, "exception handled");
        return MFX_ERR_NONE;
    }

    return sts;
}

mfxStatus VideoDECODEAV1::CompleteProc(void*, void* param, mfxStatus)
{
    auto info =
        reinterpret_cast<TaskInfo*>(param);
    delete info;

    return MFX_ERR_NONE;
}

inline
mfxU16 color_format2bit_depth(UMC::ColorFormat format)
{
    switch (format)
    {
        case UMC::NV12:
        case UMC::YUY2:
        case UMC::AYUV: return 8;

        case UMC::P010:
        case UMC::Y210:
        case UMC::Y410:  return 10;

        case UMC::P016:
        case UMC::Y216:
        case UMC::Y416:  return 12;

        default:         return 0;
    }
}

inline
mfxU16 color_format2chroma_format(UMC::ColorFormat format)
{
    switch (format)
    {
        case UMC::NV12:
        case UMC::P010:
        case UMC::P016: return MFX_CHROMAFORMAT_YUV420;

        case UMC::YUY2:
        case UMC::Y210:
        case UMC::Y216: return MFX_CHROMAFORMAT_YUV422;

        case UMC::AYUV:
        case UMC::Y410:
        case UMC::Y416: return MFX_CHROMAFORMAT_YUV444;

        default:        return MFX_CHROMAFORMAT_YUV420;
    }
}

inline
mfxU16 av1_native_profile_to_mfx_profile(mfxU16 native)
{
    switch (native)
    {
    case 0: return MFX_PROFILE_AV1_MAIN;
    case 1: return MFX_PROFILE_AV1_HIGH;
    case 2: return MFX_PROFILE_AV1_PRO;
    default: return 0;
    }
}

mfxStatus VideoDECODEAV1::FillVideoParam(UMC_AV1_DECODER::AV1DecoderParams const* vp, mfxVideoParam* par)
{
    assert(vp);
    assert(par);

    mfxVideoParam p{};
    ConvertUMCParamsToMFX(vp, &p);

    p.mfx.FrameInfo.BitDepthLuma =
    p.mfx.FrameInfo.BitDepthChroma =
        color_format2bit_depth(vp->info.color_format);

    p.mfx.FrameInfo.ChromaFormat =
        color_format2chroma_format(vp->info.color_format);

    par->mfx.FrameInfo            = p.mfx.FrameInfo;
    par->mfx.CodecProfile         = av1_native_profile_to_mfx_profile(p.mfx.CodecProfile);
    par->mfx.CodecLevel           = p.mfx.CodecLevel;
    par->mfx.DecodedOrder         = p.mfx.DecodedOrder;
    par->mfx.MaxDecFrameBuffering = p.mfx.MaxDecFrameBuffering;

    par->mfx.FilmGrain = static_cast<mfxU16>(vp->film_grain);

    if (par->mfx.FrameInfo.FourCC == MFX_FOURCC_P010
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y210
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_P016
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y216
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y416)
        par->mfx.FrameInfo.Shift = 1;

    // video signal section
    mfxExtVideoSignalInfo * videoSignal = (mfxExtVideoSignalInfo *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    if (videoSignal)
    {
        videoSignal->VideoFormat = static_cast<mfxU16>(vp->info.color_format);
        videoSignal->VideoFullRange = static_cast<mfxU16>(vp->color_range);
        videoSignal->ColourDescriptionPresent = static_cast<mfxU16>(vp->color_description_present_flag);
        videoSignal->ColourPrimaries = static_cast<mfxU16>(vp->color_primaries);
        videoSignal->TransferCharacteristics = static_cast<mfxU16>(vp->transfer_characteristics);
        videoSignal->MatrixCoefficients = static_cast<mfxU16>(vp->matrix_coefficients);
    }

    if (vp->framerate_n && vp->framerate_d)
    {
        par->mfx.FrameInfo.FrameRateExtN = vp->framerate_n;
        par->mfx.FrameInfo.FrameRateExtD = vp->framerate_d;
    }
    else // If no frame rate info in bitstream, will set to default 0
    {
        par->mfx.FrameInfo.FrameRateExtN = 0;
        par->mfx.FrameInfo.FrameRateExtD = 0;
    }

    return MFX_ERR_NONE;
}

UMC_AV1_DECODER::AV1DecoderFrame* VideoDECODEAV1::GetFrameToDisplay()
{
    assert(m_decoder);

    UMC_AV1_DECODER::AV1DecoderFrame* frame
        = m_decoder->GetFrameToDisplay();

    if (!frame)
        return nullptr;

    frame->Outputted(true);
    frame->ShowAsExisting(false);

    return frame;
}

inline void CopyFilmGrainParam(mfxExtAV1FilmGrainParam &extBuf, UMC_AV1_DECODER::FilmGrainParams const& par)
{
    extBuf.FilmGrainFlags = 0;

    if (par.apply_grain)
        extBuf.FilmGrainFlags |= MFX_FILM_GRAIN_APPLY;

    extBuf.GrainSeed = (mfxU16)par.grain_seed;

    if (par.update_grain)
        extBuf.FilmGrainFlags |= MFX_FILM_GRAIN_UPDATE;

    extBuf.RefIdx = (mfxU8)par.film_grain_params_ref_idx;

    extBuf.NumYPoints = (mfxU8)par.num_y_points;
    for (int i = 0; i < UMC_AV1_DECODER::MAX_POINTS_IN_SCALING_FUNCTION_LUMA; i++)
    {
        extBuf.PointY[i].Value = (mfxU8)par.point_y_value[i];
        extBuf.PointY[i].Scaling = (mfxU8)par.point_y_scaling[i];
    }

    if (par.chroma_scaling_from_luma)
        extBuf.FilmGrainFlags |= MFX_FILM_GRAIN_CHROMA_SCALING_FROM_LUMA;

    extBuf.NumCbPoints = (mfxU8)par.num_cb_points;
    extBuf.NumCrPoints = (mfxU8)par.num_cr_points;
    for (int i = 0; i < UMC_AV1_DECODER::MAX_POINTS_IN_SCALING_FUNCTION_CHROMA; i++)
    {
        extBuf.PointCb[i].Value = (mfxU8)par.point_cb_value[i];
        extBuf.PointCb[i].Scaling = (mfxU8)par.point_cb_scaling[i];
        extBuf.PointCr[i].Value = (mfxU8)par.point_cr_value[i];
        extBuf.PointCr[i].Scaling = (mfxU8)par.point_cr_scaling[i];
    }

    extBuf.GrainScalingMinus8 = (mfxU8)par.grain_scaling - 8;
    extBuf.ArCoeffLag = (mfxU8)par.ar_coeff_lag;

    for (int i = 0; i < UMC_AV1_DECODER::MAX_AUTOREG_COEFFS_LUMA; i++)
        extBuf.ArCoeffsYPlus128[i] = (mfxU8)(par.ar_coeffs_y[i] + 128);

    for (int i = 0; i < UMC_AV1_DECODER::MAX_AUTOREG_COEFFS_CHROMA; i++)
    {
        extBuf.ArCoeffsCbPlus128[i] = (mfxU8)(par.ar_coeffs_cb[i] + 128);
        extBuf.ArCoeffsCrPlus128[i] = (mfxU8)(par.ar_coeffs_cr[i] + 128);
    }

    extBuf.ArCoeffShiftMinus6 = (mfxU8)par.ar_coeff_shift - 6;
    extBuf.GrainScaleShift = (mfxU8)par.grain_scale_shift;
    extBuf.CbMult = (mfxU8)par.cb_mult;
    extBuf.CbLumaMult = (mfxU8)par.cb_luma_mult;
    extBuf.CbOffset = (mfxU16)par.cb_offset;
    extBuf.CrMult = (mfxU8)par.cr_mult;
    extBuf.CrLumaMult = (mfxU8)par.cr_luma_mult;
    extBuf.CrOffset = (mfxU16)par.cr_offset;

    if (par.overlap_flag)
        extBuf.FilmGrainFlags |= MFX_FILM_GRAIN_OVERLAP;

    if (par.clip_to_restricted_range)
        extBuf.FilmGrainFlags |= MFX_FILM_GRAIN_CLIP_TO_RESTRICTED_RANGE;
}

mfxStatus VideoDECODEAV1::FillOutputSurface(mfxFrameSurface1** surf_out, mfxFrameSurface1* surface_work, UMC_AV1_DECODER::AV1DecoderFrame* pFrame)
{
    MFX_CHECK_NULL_PTR2(pFrame, surf_out);

    UMC::FrameData const* fd = pFrame->GetFrameData();
    MFX_CHECK(fd, MFX_ERR_DEVICE_FAILED);

    mfxVideoParam vp;
    *surf_out = m_surface_source->GetSurface(fd->GetFrameMID(), surface_work, &vp);
    MFX_CHECK(*surf_out != nullptr, MFX_ERR_INVALID_HANDLE);

    mfxFrameSurface1* surface_out = *surf_out;
    MFX_CHECK(surface_out, MFX_ERR_INVALID_HANDLE);

    SetFrameType(*pFrame, *surface_out);

    surface_out->Info.FrameId.TemporalId = 0;

    UMC::VideoDataInfo const* vi = fd->GetInfo();
    MFX_CHECK(vi, MFX_ERR_DEVICE_FAILED);

    surface_out->Data.TimeStamp = GetMfxTimeStamp(pFrame->FrameTime());
    surface_out->Data.FrameOrder = pFrame->FrameOrder();
    surface_out->Data.Corrupted = 0;
    surface_out->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    surface_out->Info.CropW = static_cast<mfxU16>(m_anchorFramesSource > 0 ? pFrame->GetRenderWidth() : pFrame->GetUpscaledWidth());
    surface_out->Info.CropH = static_cast<mfxU16>(m_anchorFramesSource > 0 ? pFrame->GetRenderHeight() : pFrame->GetFrameHeight());
    surface_out->Info.AspectRatioW = 1;
    surface_out->Info.AspectRatioH = 1;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(m_video_par.ExtParam, m_video_par.NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    if (videoProcessing)
    {
        surface_out->Info.CropH = videoProcessing->Out.CropH;
        surface_out->Info.CropW = videoProcessing->Out.CropW;
        surface_out->Info.CropX = videoProcessing->Out.CropX;
        surface_out->Info.CropY = videoProcessing->Out.CropY;
    }
#endif

    bool isShouldUpdate = !(m_first_par.mfx.FrameInfo.FrameRateExtD || m_first_par.mfx.FrameInfo.FrameRateExtN);

    surface_out->Info.FrameRateExtD = isShouldUpdate ? m_init_par.mfx.FrameInfo.FrameRateExtD : m_first_par.mfx.FrameInfo.FrameRateExtD;
    surface_out->Info.FrameRateExtN = isShouldUpdate ? m_init_par.mfx.FrameInfo.FrameRateExtN : m_first_par.mfx.FrameInfo.FrameRateExtN;

    mfxExtAV1FilmGrainParam* extFilmGrain = (mfxExtAV1FilmGrainParam*)GetExtendedBuffer(surface_out->Data.ExtParam, surface_out->Data.NumExtParam, MFX_EXTBUFF_AV1_FILM_GRAIN_PARAM);
    if (extFilmGrain)
    {
        UMC_AV1_DECODER::FrameHeader const& fh = pFrame->GetFrameHeader();
        CopyFilmGrainParam(*extFilmGrain, fh.film_grain_params);
    }

    const UMC_AV1_DECODER::FrameHeader& fh = pFrame->GetFrameHeader();
    // extract HDR MasteringDisplayColourVolume info
    mfxExtMasteringDisplayColourVolume* display_colour = (mfxExtMasteringDisplayColourVolume*)GetExtendedBuffer(surface_out->Data.ExtParam, surface_out->Data.NumExtParam, MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME);
    if (display_colour && fh.meta_data.hdr_mdcv.existence)
    {
        for (size_t i = 0; i < 3; i++)
        {
            display_colour->DisplayPrimariesX[i] = (mfxU16)fh.meta_data.hdr_mdcv.display_primaries[i][0];
            display_colour->DisplayPrimariesY[i] = (mfxU16)fh.meta_data.hdr_mdcv.display_primaries[i][1];
        }
        display_colour->WhitePointX = (mfxU16)fh.meta_data.hdr_mdcv.white_point[0];
        display_colour->WhitePointY = (mfxU16)fh.meta_data.hdr_mdcv.white_point[1];
        display_colour->MaxDisplayMasteringLuminance = (mfxU32)fh.meta_data.hdr_mdcv.max_luminance;
        display_colour->MinDisplayMasteringLuminance = (mfxU32)fh.meta_data.hdr_mdcv.min_luminance;
        display_colour->InsertPayloadToggle = MFX_PAYLOAD_IDR;
    }
    else if(display_colour)
    {
        display_colour->InsertPayloadToggle = MFX_PAYLOAD_OFF;
    }

    // extract HDR ContentLightLevel info
    mfxExtContentLightLevelInfo* content_light = (mfxExtContentLightLevelInfo*)GetExtendedBuffer(surface_out->Data.ExtParam, surface_out->Data.NumExtParam, MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO);
    if (content_light && fh.meta_data.hdr_cll.existence)
    {
        content_light->MaxContentLightLevel = (mfxU16)fh.meta_data.hdr_cll.max_content_light_level;
        content_light->MaxPicAverageLightLevel = (mfxU16)fh.meta_data.hdr_cll.max_pic_average_light_level;
        content_light->InsertPayloadToggle = MFX_PAYLOAD_IDR;
    }
    else if(content_light)
    {
        content_light->InsertPayloadToggle = MFX_PAYLOAD_OFF;
    }

    TRACE_BUFFER_EVENT(MFX_TRACE_API_AV1_OUTPUTINFO_TASK, EVENT_TYPE_INFO, TR_KEY_DECODE_BASIC_INFO,
        surface_out, AV1DecodeSurfaceOutparam, SURFACEOUT_AV1D);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEAV1::GetSurface(mfxFrameSurface1* & surface, mfxSurfaceHeader* import_surface)
{
    MFX_CHECK(m_surface_source, MFX_ERR_NOT_INITIALIZED);

    return m_surface_source->GetSurface(surface, import_surface);
}

#endif
