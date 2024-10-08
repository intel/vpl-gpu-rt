// Copyright (c) 2012-2020 Intel Corporation
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

#if defined(_MSC_VER) //MSVC compiler issue: complains about 'std::copy' with parameters that may be unsafe
#pragma warning(push)
#pragma warning(disable:4996)
#endif

#include <algorithm> /* for std::find on Linux/Android */

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "mfx_common.h"
#include "mfxvideo++int.h"

#if defined(MFX_ENABLE_VP8_VIDEO_DECODE)

#include "mfx_common_decode_int.h"
#include "mfx_vp8_dec_decode_hw.h"
#include "mfx_enc_common.h"
#include "mfx_vpx_dec_common.h"
#include "libmfx_core_vaapi.h"

#include "umc_va_base.h"


#include <va/va.h>
#include <va/va_dec_vp8.h>


#include <iostream>
#include <sstream>
#include <fstream>

#include "mfx_vp8_dec_decode_common.h"

#define VP8_START_CODE_FOUND(ptr) ((ptr)[0] == 0x9d && (ptr)[1] == 0x01 && (ptr)[2] == 0x2a)

static void SetFrameType(const VP8Defs::vp8_FrameInfo &frame_info, mfxFrameSurface1 &surface_out)
{
    auto extFrameInfo = reinterpret_cast<mfxExtDecodedFrameInfo *>(GetExtendedBuffer(surface_out.Data.ExtParam, surface_out.Data.NumExtParam, MFX_EXTBUFF_DECODED_FRAME_INFO));
    if (extFrameInfo == nullptr)
        return;

    switch (frame_info.frameType)
    {
    case UMC::I_PICTURE:
        extFrameInfo->FrameType = MFX_FRAMETYPE_I;
        break;
    case UMC::P_PICTURE:
        extFrameInfo->FrameType = MFX_FRAMETYPE_P;
        break;
    default:
        extFrameInfo->FrameType = MFX_FRAMETYPE_UNKNOWN;
    }
}

VideoDECODEVP8_HW::VideoDECODEVP8_HW(VideoCORE *p_core, mfxStatus *sts)
    : m_is_initialized(false)
    , m_p_core(p_core)
    , m_init_w(0)
    , m_init_h(0)
    , m_in_framerate(0)
    , m_frameOrder((mfxU16)MFX_FRAMEORDER_UNKNOWN)
    , m_CodedCoeffTokenPartition(0)
    , m_firstFrame(true)
    , m_response()
    , m_response_alien()
    , m_stat()
    , m_request()
    , m_p_video_accelerator(NULL)
{
    UMC_SET_ZERO(m_bs);
    UMC_SET_ZERO(m_frame_info);
    UMC_SET_ZERO(m_refresh_info);
    UMC_SET_ZERO(m_frameProbs);
    UMC_SET_ZERO(m_frameProbs_saved);
    UMC_SET_ZERO(m_quantInfo);

    gold_indx = 0;
    altref_indx = 0;
    lastrefIndex = 0;

    if (sts)
        *sts = MFX_ERR_NONE;
}

VideoDECODEVP8_HW::~VideoDECODEVP8_HW()
{
    Close();
}

#ifdef _MSVC_LANG
#pragma warning (disable : 4189)
#endif

bool VideoDECODEVP8_HW::CheckHardwareSupport(VideoCORE *p_core, mfxVideoParam *p_video_param)
{
    // IsGuidSupported checks p_video_param for nullptr
    MFX_CHECK(p_core, false);

    if (p_core->IsGuidSupported(sDXVA_Intel_ModeVP8_VLD, p_video_param) != MFX_ERR_NONE)
    {
        return false;
    }

    return true;

} // bool VideoDECODEVP8_HW::CheckHardwareSupport(VideoCORE *p_core, mfxVideoParam *p_video_param)

mfxStatus VideoDECODEVP8_HW::Init(mfxVideoParam *p_video_param)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEVP8_HW::Init");
    mfxStatus sts = MFX_ERR_NONE;

    if (m_is_initialized)
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);

    eMFXVAType vatype = m_p_core->GetVAType();
    if(vatype == MFX_HW_D3D11)
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    eMFXPlatform platform = MFX_VP8_Utility::GetPlatform(m_p_core, p_video_param);
    MFX_CHECK(platform == MFX_PLATFORM_HARDWARE, MFX_ERR_UNSUPPORTED);

    eMFXHWType type = m_p_core->GetHWType();

    if (MFX_ERR_NONE > CheckVideoParamDecoders(p_video_param, type))
    {
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
    }

    if(CheckHardwareSupport(m_p_core, p_video_param) == false)
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    if (MFX_VPX_Utility::CheckVideoParam(p_video_param, MFX_CODEC_VP8) == false)
    {
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
    }

    m_on_init_video_params = *p_video_param;
    m_init_w = p_video_param->mfx.FrameInfo.Width;
    m_init_h = p_video_param->mfx.FrameInfo.Height;

    m_in_framerate = (m_on_init_video_params.mfx.FrameInfo.FrameRateExtN && m_on_init_video_params.mfx.FrameInfo.FrameRateExtD) ?
        (mfxF64) m_on_init_video_params.mfx.FrameInfo.FrameRateExtD / m_on_init_video_params.mfx.FrameInfo.FrameRateExtN : (mfxF64) 1 / 30;

    m_video_params = m_on_init_video_params;

    // allocate memory
    mfxFrameAllocRequest request;
    memset(&request, 0, sizeof(request));
    memset(&m_response, 0, sizeof(m_response));
    memset(&m_response_alien, 0, sizeof(m_response_alien));

    sts = MFX_VPX_Utility::QueryIOSurfInternal(&m_video_params, &request);
    MFX_CHECK_STS(sts);

    mfxFrameAllocRequest request_internal = request;
    request.AllocId = p_video_param->AllocId;

    MFX_CHECK_STS(sts);

    m_request = request;

    try
    {
        m_surface_source.reset(new SurfaceSource(m_p_core, *p_video_param, platform, request, request_internal, m_response, m_response_alien));
    }
    catch (const std::system_error& ex)
    {
        MFX_CHECK_STS(mfxStatus(ex.code().value()));
    }

    sts = m_p_core->CreateVA(&m_on_init_video_params, &request, &m_response, m_surface_source.get());
    MFX_CHECK_STS(sts);

    m_p_core->GetVA((mfxHDL*)&m_p_video_accelerator, MFX_MEMTYPE_FROM_DECODE);

    m_frameOrder = (mfxU16)0;
    m_firstFrame = true;
    m_is_initialized = true;

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8_HW::Init(mfxVideoParam *p_video_param)

mfxStatus VideoDECODEVP8_HW::QueryImplsDescription(
    VideoCORE&,
    mfxDecoderDescription::decoder& caps,
    mfx::PODArraysHolder& ah)
{
    const mfxU32 SupportedProfiles[] =
    {
        MFX_PROFILE_VP8_0
        , MFX_PROFILE_VP8_1
        , MFX_PROFILE_VP8_2
        , MFX_PROFILE_VP8_3
    };
    const mfxResourceType SupportedMemTypes[] =
    {
        MFX_RESOURCE_SYSTEM_SURFACE
        , MFX_RESOURCE_VA_SURFACE
    };
    const mfxU32 SupportedFourCC[] =
    {
        MFX_FOURCC_NV12
    };

    caps.CodecID = MFX_CODEC_VP8;
    caps.MaxcodecLevel = MFX_LEVEL_UNKNOWN;

    for (mfxU32 profile : SupportedProfiles)
    {
        auto& pfCaps = ah.PushBack(caps.Profiles);
        pfCaps.Profile = profile;

        for (auto memType : SupportedMemTypes)
        {
            auto& memCaps = ah.PushBack(pfCaps.MemDesc);
            memCaps.MemHandleType = memType;
            memCaps.Width = { 16, 4096, 16 };
            memCaps.Height = { 16, 4096, 16 };

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

static bool IsSameVideoParam(mfxVideoParam *newPar, mfxVideoParam *oldPar)
{
    if (newPar->IOPattern != oldPar->IOPattern)
        return false;

    if (newPar->Protected != oldPar->Protected)
        return false;

    int32_t asyncDepth = std::min<int32_t>(newPar->AsyncDepth, MFX_MAX_ASYNC_DEPTH_VALUE);
    if (asyncDepth != oldPar->AsyncDepth)
        return false;

    if (newPar->mfx.FrameInfo.Height != oldPar->mfx.FrameInfo.Height)
        return false;

    if (newPar->mfx.FrameInfo.Width != oldPar->mfx.FrameInfo.Width)
        return false;

    if (newPar->mfx.FrameInfo.ChromaFormat != oldPar->mfx.FrameInfo.ChromaFormat)
        return false;

    if (newPar->mfx.NumThread > oldPar->mfx.NumThread && oldPar->mfx.NumThread > 0) //  need more surfaces for efficient decoding
        return false;

    return true;
}

mfxStatus VideoDECODEVP8_HW::Reset(mfxVideoParam *p_video_param)
{
    if(m_is_initialized == false)
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(p_video_param);

    eMFXHWType type = m_p_core->GetHWType();

    if (MFX_ERR_NONE > CheckVideoParamDecoders(p_video_param, type))
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);

    if (MFX_VPX_Utility::CheckVideoParam(p_video_param, MFX_CODEC_VP8) == false)
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);

    if (!IsSameVideoParam(p_video_param, &m_on_init_video_params))
        MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    // sw acceleration is not supported
    if (MFX_PLATFORM_SOFTWARE == m_p_core->GetPlatformType())
        MFX_RETURN(MFX_ERR_UNSUPPORTED);

    if (m_surface_source->Reset() != UMC::UMC_OK)
        MFX_RETURN(MFX_ERR_MEMORY_ALLOC);

    m_frameOrder = (mfxU16)0;
    memset(&m_stat, 0, sizeof(m_stat));

    m_on_init_video_params = *p_video_param;
    m_video_params = m_on_init_video_params;

    m_in_framerate = (m_on_init_video_params.mfx.FrameInfo.FrameRateExtN && m_on_init_video_params.mfx.FrameInfo.FrameRateExtD) ?
        (mfxF64) m_on_init_video_params.mfx.FrameInfo.FrameRateExtD / m_on_init_video_params.mfx.FrameInfo.FrameRateExtN : (mfxF64) 1 / 30;

    if(CheckHardwareSupport(m_p_core, p_video_param) == false)
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    gold_indx = 0;
    altref_indx = 0;
    lastrefIndex = 0;
    m_bs.DataLength = 0;

    for(size_t i = 0; i < m_frames.size(); i++)
    {
        m_surface_source.get()->DecreaseReference(m_frames[i].memId);
    }

    m_firstFrame = true;
    m_frames.clear();
    m_memIdReadyToFree.clear();


    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8_HW::Reset(mfxVideoParam *p_video_param)

mfxStatus VideoDECODEVP8_HW::Close()
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEVP8_HW::Close");
    if(m_is_initialized == false)
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);

    m_is_initialized = false;
    m_surface_source->Close();

    m_frameOrder = (mfxU16)0;
    m_p_video_accelerator = 0;
    memset(&m_stat, 0, sizeof(m_stat));

    if (m_bs.Data)
    {
        delete[] m_bs.Data;
        m_bs.DataLength = 0;
    }

    gold_indx = 0;
    altref_indx = 0;
    lastrefIndex = 0;
    m_firstFrame = true;

    return MFX_ERR_NONE;
} // mfxStatus VideoDECODEVP8_HW::Close()

mfxStatus VideoDECODEVP8_HW::Query(VideoCORE *p_core, mfxVideoParam *p_in, mfxVideoParam *p_out)
{
    MFX_CHECK_NULL_PTR1(p_out);

    eMFXHWType type = p_core->GetHWType();

    if (!CheckHardwareSupport(p_core, p_in ? p_in : p_out))
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    return MFX_VP8_Utility::Query(p_core, p_in, p_out, type);

} // mfxStatus VideoDECODEVP8_HW::Query(VideoCORE *p_core, mfxVideoParam *p_in, mfxVideoParam *p_out)

mfxStatus VideoDECODEVP8_HW::QueryIOSurf(VideoCORE *p_core, mfxVideoParam *p_video_param, mfxFrameAllocRequest *p_request)
{
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR3(p_core, p_video_param, p_request);

    mfxVideoParam p_params = *p_video_param;

    if (   !(p_params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        && !(p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
    }

    if ((p_params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        && (p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
    }

    if(p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        p_request->Info = p_params.mfx.FrameInfo;
        p_request->NumFrameMin = 1;
        p_request->NumFrameSuggested = p_request->NumFrameMin + (p_params.AsyncDepth ? p_params.AsyncDepth : MFX_AUTO_ASYNC_DEPTH_VALUE);
        p_request->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        sts = MFX_VPX_Utility::QueryIOSurfInternal(p_video_param, p_request);
    }

    if (!CheckHardwareSupport(p_core, p_video_param))
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    return sts;

} // mfxStatus VideoDECODEVP8_HW::QueryIOSurf(VideoCORE *p_core, mfxVideoParam *p_par, mfxFrameAllocRequest *p_request)

mfxStatus VideoDECODEVP8_HW::DecodeFrameCheck(mfxBitstream * /*p_bs*/, mfxFrameSurface1 * /*p_surface_work*/, mfxFrameSurface1 ** /*pp_surface_out*/)
{
    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8_HW::DecodeFrameCheck(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface_work, mfxFrameSurface1 **pp_surface_out)

mfxStatus VideoDECODEVP8_HW::GetOutputSurface(mfxFrameSurface1 **pp_surface_out, mfxFrameSurface1 *p_surface_work, UMC::FrameMemID index)
{
    mfxFrameSurface1 *p_native_surface = m_surface_source->GetSurface(index, p_surface_work, &m_video_params);

    if (!p_native_surface)
    {
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    *pp_surface_out = p_native_surface;

    return MFX_ERR_NONE;
}

#define VP8_START_CODE_FOUND(ptr) ((ptr)[0] == 0x9d && (ptr)[1] == 0x01 && (ptr)[2] == 0x2a)

mfxStatus VideoDECODEVP8_HW::PreDecodeFrame(mfxBitstream *p_bs, mfxU32& w, mfxU32& h)
{
    mfxU8 *p_bitstream = p_bs->Data + p_bs->DataOffset;
    mfxU8 *p_bitstream_end = p_bs->Data + p_bs->DataOffset + p_bs->DataLength;

    while (p_bitstream < p_bitstream_end)
    {
        if (VP8_START_CODE_FOUND(p_bitstream)) // (0x9D && 0x01 && 0x2A)
        {
            break;
        }

        p_bitstream += 1;
    }

    w = ((p_bitstream[4] << 8) | p_bitstream[3]) & 0x3fff;
    h = ((p_bitstream[6] << 8) | p_bitstream[5]) & 0x3fff;

    w = (w + 15) & ~0x0f;
    h = (h + 15) & ~0x0f;

    if (m_init_w != w || m_init_h != h)
    {
        MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8_HW::PreDecodeFrame(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface)

mfxStatus VideoDECODEVP8_HW::ConstructFrame(mfxBitstream *p_in, mfxBitstream *p_out, VP8DecodeCommon::IVF_FRAME& frame)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoDECODEVP8_HW::ConstructFrame");
    MFX_CHECK_NULL_PTR1(p_out);

    if (p_in->DataLength == 0)
    {
        MFX_RETURN(MFX_ERR_MORE_DATA);
    }

    mfxU8 *p_bs_start = p_in->Data + p_in->DataOffset;

    if (p_out->Data)
    {
        delete[] p_out->Data;
        p_out->DataLength = 0;
    }

    p_out->Data = new uint8_t[p_in->DataLength];

    std::copy(p_bs_start, p_bs_start + p_in->DataLength, p_out->Data);

    p_out->DataLength = p_in->DataLength;
    p_out->DataOffset = 0;

    frame.frame_size = p_in->DataLength;

    MoveBitstreamData(*p_in, p_in->DataLength);

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8_HW::ConstructFrame(mfxBitstream *p_in, mfxBitstream *p_out, IVF_FRAME& frame)

UMC::FrameMemID VideoDECODEVP8_HW::GetMemIdToUnlock()
{

    size_t size = m_frames.size();

    if(size == 1)
    {
        return -1;
    }

    UMC::FrameMemID memId = -1;
    sFrameInfo info = {};

    std::vector<sFrameInfo>::iterator i;
    std::vector<UMC::FrameMemID>::iterator freeMemIdPos;

    for(i = m_frames.begin();i != m_frames.end() && (i + 1) != m_frames.end();i++)
    {
        freeMemIdPos = std::find(m_memIdReadyToFree.begin(), m_memIdReadyToFree.end(), i->memId);

        if(i->currIndex != gold_indx && i->currIndex != altref_indx && freeMemIdPos != m_memIdReadyToFree.end())
        {
            info = *i;
            memId = info.memId;
            m_frames.erase(i);
            m_memIdReadyToFree.erase(freeMemIdPos);
            break;
        }
    }

    if(memId == -1) return -1;

    return memId;
}

mfxStatus MFX_CDECL VP8DECODERoutine(void *p_state, void * /*pp_param*/, mfxU32 /*thread_number*/, mfxU32)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VP8DECODERoutine");
    mfxStatus sts = MFX_ERR_NONE;
    VideoDECODEVP8_HW::VP8DECODERoutineData& data = *(VideoDECODEVP8_HW::VP8DECODERoutineData*)p_state;
    VideoDECODEVP8_HW& decoder = *data.decoder;


    {
    UMC::Status status = decoder.m_p_video_accelerator->SyncTask(data.memId);
    if (status != UMC::UMC_OK && status != UMC::UMC_ERR_TIMEOUT)
    {
        mfxStatus CriticalErrorStatus = (status == UMC::UMC_ERR_GPU_HANG) ? MFX_ERR_GPU_HANG : MFX_ERR_DEVICE_FAILED;
        decoder.SetCriticalErrorOccured(CriticalErrorStatus);
        return CriticalErrorStatus;
    }
    }

    if (decoder.m_video_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY && data.surface_work)
    {
        sts = decoder.m_surface_source->PrepareToOutput(data.surface_work, data.memId, &decoder.m_on_init_video_params);
    }

    UMC::AutomaticUMCMutex guard(decoder.m_mGuard);

    decoder.m_memIdReadyToFree.push_back(data.memId);

    UMC::FrameMemID memIdToUnlock = -1;
    while ((memIdToUnlock = decoder.GetMemIdToUnlock()) != -1)
    {
        decoder.m_surface_source.get()->DecreaseReference(memIdToUnlock);
    }

    delete &data;

    return sts;
}

static mfxStatus VP8CompleteProc(void *, void * /*pp_param*/, mfxStatus)
{
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP8_HW::DecodeFrameCheck(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface_work, mfxFrameSurface1 **pp_surface_out, MFX_ENTRY_POINT * p_entry_point)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoDECODEVP8_HW::DecodeFrameCheck");
    mfxStatus sts = MFX_ERR_NONE;

    if(m_is_initialized == false)
    {
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);
    }

    if (NeedToReturnCriticalStatus(p_bs))
        return ReturningCriticalStatus();

    bool allow_null_work_surface = SupportsVPLFeatureSet(*m_p_core);

    if (allow_null_work_surface)
    {
        MFX_CHECK_NULL_PTR1(pp_surface_out);
    }
    else
    {
        MFX_CHECK_NULL_PTR2(p_surface_work, pp_surface_out);
    }

    if (p_surface_work)
    {
        sts = CheckFrameInfoCodecs(&p_surface_work->Info, MFX_CODEC_VP8);
        MFX_CHECK_STS(sts);

        sts = CheckFrameData(p_surface_work);
        MFX_CHECK_STS(sts);
    }

    UMC::FrameMemID memId;
    do
    {
        sts = p_bs ? CheckBitstream(p_bs) : MFX_ERR_NONE;
        MFX_CHECK_STS(sts);

        if (p_bs == NULL)
            MFX_RETURN(MFX_ERR_MORE_DATA);

        bool show_frame;
        UMC::FrameType frame_type;

        if (p_bs->DataLength == 0)
            MFX_RETURN(MFX_ERR_MORE_DATA);

        sts = m_surface_source->SetCurrentMFXSurface(p_surface_work);
        MFX_CHECK_STS(sts);

        if (!m_surface_source->HasFreeSurface())
        {
            return MFX_WRN_DEVICE_BUSY;
        }

        mfxU8 *pTemp = p_bs->Data + p_bs->DataOffset;
        frame_type = (pTemp[0] & 1) ? UMC::P_PICTURE : UMC::I_PICTURE; // 1 bits
        show_frame = (pTemp[0] >> 4) & 0x1;

        mfxU32 bs_width = 0;
        mfxU32 bs_height = 0;
        if (frame_type == UMC::I_PICTURE)
        {
            sts = PreDecodeFrame(p_bs, bs_width, bs_height);
            MFX_CHECK_STS(sts);
        }

        if (m_firstFrame && frame_type != UMC::I_PICTURE)
        {
            MoveBitstreamData(*p_bs, p_bs->DataLength);
            MFX_RETURN(MFX_ERR_MORE_DATA);
        }

        m_firstFrame = false;

        VP8DecodeCommon::IVF_FRAME frame;
        memset(&frame, 0, sizeof(VP8DecodeCommon::IVF_FRAME));

        sts = ConstructFrame(p_bs, &m_bs, frame);
        MFX_CHECK_STS(sts);

        *pp_surface_out = 0;

        sts = DecodeFrameHeader(&m_bs);
        MFX_CHECK_STS(sts);

        UMC::VideoDataInfo vdInfo;
        vdInfo.Init(m_frame_info.frameSize.width, m_frame_info.frameSize.height, UMC::NV12, 8);
        UMC::Status umc_sts = m_surface_source->Alloc(&memId, &vdInfo, 0);
        MFX_CHECK(umc_sts == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);

        if (!p_surface_work && m_on_init_video_params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        {
            p_surface_work = m_surface_source->GetSurfaceByIndex(memId);
            MFX_CHECK_NULL_PTR1(p_surface_work);
        }

        if (p_surface_work)
        {
            p_surface_work->Info.CropW = p_surface_work->Info.CropW ? p_surface_work->Info.CropW : m_on_init_video_params.mfx.FrameInfo.CropW;
            p_surface_work->Info.CropH = p_surface_work->Info.CropH ? p_surface_work->Info.CropH : m_on_init_video_params.mfx.FrameInfo.CropH;

            MFX_CHECK(p_surface_work->Info.Width >= bs_width && p_surface_work->Info.Height >= bs_height, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
        }

        sFrameInfo info;
        info.frameType = m_frame_info.frameType;
        info.memId = memId;
        info.currIndex = static_cast<mfxU16>(memId);
        info.goldIndex = gold_indx;
        info.altrefIndex = altref_indx;
        info.lastrefIndex = lastrefIndex;

        if (m_frame_info.frameType == UMC::I_PICTURE)
        {
            gold_indx = altref_indx = lastrefIndex = info.currIndex;
        }
        else
        {
            mfxU16 oldgold_indx = gold_indx;

            switch (m_refresh_info.copy2Golden)
            {
            case 1:
                gold_indx = lastrefIndex;
                break;

            case 2:
                gold_indx = altref_indx;
                break;

            case 0:
            default:
                break;
            }

            switch (m_refresh_info.copy2Altref)
            {
            case 1:
                altref_indx = lastrefIndex;
                break;

            case 2:
                altref_indx = oldgold_indx;
                break;

            case 0:
            default:
                break;
            }

            if ((m_refresh_info.refreshRefFrame & 2) != 0)
                gold_indx = info.currIndex;

            if ((m_refresh_info.refreshRefFrame & 1) != 0)
                altref_indx = info.currIndex;

            if (m_refresh_info.refreshLastFrame)
                lastrefIndex = info.currIndex;

        }

        m_frames.push_back(info);

        m_surface_source->IncreaseReference(memId);

        PackHeaders(&m_bs);


        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VP8 decode DDISubmitTask");

            if (m_p_video_accelerator->BeginFrame(memId, 0) == UMC::UMC_OK)
            {
                m_p_video_accelerator->Execute();
                m_p_video_accelerator->EndFrame();
            }
        }

        if (show_frame)
        {
            sts = GetOutputSurface(pp_surface_out, p_surface_work, memId);
            MFX_CHECK_STS(sts);

            (*pp_surface_out)->Info.CropW = (*pp_surface_out)->Info.CropW ? (*pp_surface_out)->Info.CropW : m_on_init_video_params.mfx.FrameInfo.CropW;
            (*pp_surface_out)->Info.CropH = (*pp_surface_out)->Info.CropH ? (*pp_surface_out)->Info.CropH : m_on_init_video_params.mfx.FrameInfo.CropH;

            MFX_CHECK((*pp_surface_out)->Info.Width >= bs_width && (*pp_surface_out)->Info.Height >= bs_height, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)

            SetFrameType(m_frame_info, **pp_surface_out);
        }
        else
        {
            sts = MFX_ERR_MORE_DATA_SUBMIT_TASK;
            if (p_surface_work)
            {
                pp_surface_out = nullptr;
            }
            else
            {
                m_memIdReadyToFree.push_back(memId);

                UMC::FrameMemID memIdToUnlock = -1;
                while ((memIdToUnlock = GetMemIdToUnlock()) != -1)
                {
                    m_surface_source.get()->DecreaseReference(memIdToUnlock);
                }
            }
        }
    } while (sts == MFX_ERR_MORE_DATA_SUBMIT_TASK && !p_surface_work);

    if (pp_surface_out)
    {
        (*pp_surface_out)->Data.Corrupted = 0;
        (*pp_surface_out)->Data.FrameOrder = m_frameOrder;
        //(*pp_surface_out)->Data.FrameOrder = p_surface_work->Data.FrameOrder;
        m_frameOrder++;

        (*pp_surface_out)->Data.TimeStamp = p_bs->TimeStamp;
        (*pp_surface_out)->Info.FrameRateExtD = m_on_init_video_params.mfx.FrameInfo.FrameRateExtD;
        (*pp_surface_out)->Info.FrameRateExtN = m_on_init_video_params.mfx.FrameInfo.FrameRateExtN;
        (*pp_surface_out)->Info.AspectRatioW = 1;
        (*pp_surface_out)->Info.AspectRatioH = 1;
        (*pp_surface_out)->Info.PicStruct = m_on_init_video_params.mfx.FrameInfo.PicStruct;
    }

    p_entry_point->pRoutine = &VP8DECODERoutine;
    p_entry_point->pCompleteProc = &VP8CompleteProc;

    VP8DECODERoutineData* routineData = new VP8DECODERoutineData;
    routineData->decoder = this;
    routineData->memId = memId;
    if(pp_surface_out)
        routineData->surface_work = *pp_surface_out;

    p_entry_point->pState = routineData;
    p_entry_point->requiredNumThreads = 1;

    return sts;

} // mfxStatus VideoDECODEVP8_HW::DecodeFrameCheck(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface_work, mfxFrameSurface1 **pp_surface_out, MFX_ENTRY_POINT *p_entry_point)

void VideoDECODEVP8_HW::UpdateSegmentation(MFX_VP8_BoolDecoder &dec)
{
    uint32_t bits;

    int32_t i;
    int32_t j;
    int32_t res;

    bits = dec.decode(2);

    m_frame_info.updateSegmentMap  = (uint8_t)bits >> 1;
    m_frame_info.updateSegmentData = (uint8_t)bits & 1;

    if (m_frame_info.updateSegmentData)
    {
        m_frame_info.segmentAbsMode = (uint8_t)dec.decode();
        UMC_SET_ZERO(m_frame_info.segmentFeatureData);

        for (i = 0; i < VP8Defs::VP8_NUM_OF_MB_FEATURES; i++)
        {
            for (j = 0; j < VP8_MAX_NUM_OF_SEGMENTS; j++)
            {
                bits = (uint8_t)dec.decode();

                if (bits)
                {
                    bits = (uint8_t)dec.decode(8 - i); // 7 bits for ALT_QUANT, 6 - for ALT_LOOP_FILTER; +sign
                    res = bits >> 1;

                    if (bits & 1)
                        res = -res;

                    m_frame_info.segmentFeatureData[i][j] = (int8_t)res;
                }
            }
        }
    }

    if (m_frame_info.updateSegmentMap)
    {
        for (i = 0; i < VP8_NUM_OF_SEGMENT_TREE_PROBS; i++)
        {
            bits = (uint8_t)dec.decode();

            if (bits)
                m_frame_info.segmentTreeProbabilities[i] = (uint8_t)dec.decode(8);
            else
                m_frame_info.segmentTreeProbabilities[i] = 255;
        }
    }
} // VideoDECODEVP8_HW::UpdateSegmentation()

void VideoDECODEVP8_HW::UpdateLoopFilterDeltas(MFX_VP8_BoolDecoder &dec)
{
  uint8_t  bits;
  int32_t i;
  int32_t res;

    for (i = 0; i < VP8Defs::VP8_NUM_OF_REF_FRAMES; i++)
    {
        if (dec.decode())
        {
            bits = (uint8_t)dec.decode(7);
            res = bits >> 1;

            if (bits & 1)
                res = -res;

            m_frame_info.refLoopFilterDeltas[i] = (int8_t)res;
        }
    }

    for (i = 0; i < VP8_NUM_OF_MODE_LF_DELTAS; i++)
    {
        if (dec.decode())
        {
            bits = (uint8_t)dec.decode(7);
            res = bits >> 1;

            if (bits & 1)
                res = -res;

            m_frame_info.modeLoopFilterDeltas[i] = (int8_t)res;
        }
    }
} // VideoDECODEVP8_HW::UpdateLoopFilterDeltas()

#define DECODE_DELTA_QP(dec, res, shift) \
{ \
  uint32_t mask = (1 << (shift - 1)); \
  int32_t val; \
  if (bits & mask) \
  { \
    bits = (bits << 5) | dec.decode(5); \
    val  = (int32_t)((bits >> shift) & 0xF); \
    res  = (bits & mask) ? -val : val; \
  } \
}


void VideoDECODEVP8_HW::DecodeInitDequantization(MFX_VP8_BoolDecoder &dec)
{
  m_quantInfo.yacQP = (int32_t)dec.decode(7);

  m_quantInfo.ydcDeltaQP  = 0;
  m_quantInfo.y2dcDeltaQP = 0;
  m_quantInfo.y2acDeltaQP = 0;
  m_quantInfo.uvdcDeltaQP = 0;
  m_quantInfo.uvacDeltaQP = 0;

  uint32_t bits = (uint8_t)dec.decode(5);

  if (bits)
  {
    DECODE_DELTA_QP(dec, m_quantInfo.ydcDeltaQP,  5)
    DECODE_DELTA_QP(dec, m_quantInfo.y2dcDeltaQP, 4)
    DECODE_DELTA_QP(dec, m_quantInfo.y2acDeltaQP, 3)
    DECODE_DELTA_QP(dec, m_quantInfo.uvdcDeltaQP, 2)
    DECODE_DELTA_QP(dec, m_quantInfo.uvacDeltaQP, 1)
  }

  int32_t qp;

  for(int32_t segment_id = 0; segment_id < VP8_MAX_NUM_OF_SEGMENTS; segment_id++)
  {
    if (m_frame_info.segmentationEnabled)
    {
      if (m_frame_info.segmentAbsMode)
        qp = m_frame_info.segmentFeatureData[VP8Defs::VP8_ALT_QUANT][segment_id];
      else
      {
        qp = m_quantInfo.yacQP + m_frame_info.segmentFeatureData[VP8Defs::VP8_ALT_QUANT][segment_id];
        qp = mfx::clamp(qp, 0, VP8_MAX_QP);
      }
    }
    else
      qp = m_quantInfo.yacQP;


    m_quantInfo.yacQ[segment_id]  = qp;
    m_quantInfo.ydcQ[segment_id]  = mfx::clamp(qp + m_quantInfo.ydcDeltaQP,  0, VP8_MAX_QP);

    m_quantInfo.y2acQ[segment_id] = mfx::clamp(qp + m_quantInfo.y2acDeltaQP, 0, VP8_MAX_QP);
    m_quantInfo.y2dcQ[segment_id] = mfx::clamp(qp + m_quantInfo.y2dcDeltaQP, 0, VP8_MAX_QP);

    m_quantInfo.uvacQ[segment_id] = mfx::clamp(qp + m_quantInfo.uvacDeltaQP, 0, VP8_MAX_QP);
    m_quantInfo.uvdcQ[segment_id] = mfx::clamp(qp + m_quantInfo.uvdcDeltaQP, 0, VP8_MAX_QP);


  }
}

mfxStatus VideoDECODEVP8_HW::DecodeFrameHeader(mfxBitstream *in)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoDECODEVP8_HW::DecodeFrameHeader");
    using namespace VP8Defs;

    try
    {
        mfxU8* data_in     = 0;
        mfxU8* data_in_end = 0;
        mfxU8  version;
        int width       = 0;
        int height      = 0;

        data_in = (uint8_t*)in->Data;

        if(!data_in)
            MFX_RETURN(MFX_ERR_NULL_PTR);

        data_in_end = data_in + in->DataLength;

        //suppose that Intraframes -> I_PICTURE ( == VP8_KEY_FRAME)
        //             Interframes -> P_PICTURE
        m_frame_info.frameType = (data_in[0] & 1) ? UMC::P_PICTURE : UMC::I_PICTURE; // 1 bits
        version = (data_in[0] >> 1) & 0x7; // 3 bits
        m_frame_info.version = version;
        m_frame_info.showFrame = (data_in[0] >> 4) & 0x01; // 1 bits

        switch (version)
        {
            case 1:
            case 2:
                m_frame_info.interpolationFlags = VP8_BILINEAR_INTERP;
                break;

            case 3:
                m_frame_info.interpolationFlags = VP8_BILINEAR_INTERP | VP8_CHROMA_FULL_PEL;
                break;

            case 0:
            default:
                m_frame_info.interpolationFlags = 0;
                break;
        }

        mfxU32 first_partition_size = (data_in[0] >> 5) |           // 19 bit
                                      (data_in[1] << 3) |
                                      (data_in[2] << 11);

        m_frame_info.firstPartitionSize = first_partition_size;
        m_frame_info.partitionSize[VP8_FIRST_PARTITION] = first_partition_size;

        data_in   += 3;

        if (!m_refresh_info.refreshProbabilities)
        {
            m_frameProbs = m_frameProbs_saved;

            std::copy(reinterpret_cast<const char*>(vp8_default_mv_contexts),
                      reinterpret_cast<const char*>(vp8_default_mv_contexts) + sizeof(vp8_default_mv_contexts),
                      reinterpret_cast<char*>(m_frameProbs.mvContexts));
        }

        if (first_partition_size > in->DataLength - 10)
             return MFX_ERR_MORE_DATA;

        if (m_frame_info.frameType == UMC::I_PICTURE)  // if VP8_KEY_FRAME
        {
            if (!(VP8_START_CODE_FOUND(data_in))) // (0x9D && 0x01 && 0x2A)
                MFX_RETURN(MFX_ERR_UNKNOWN);

            width               = ((data_in[4] << 8) | data_in[3]) & 0x3FFF;
            m_frame_info.h_scale = data_in[4] >> 6;
            height              = ((data_in[6] << 8) | data_in[5]) & 0x3FFF;
            m_frame_info.v_scale = data_in[6] >> 6;

            m_frame_info.frameSize.width  = width;
            m_frame_info.frameSize.height = height;

            width  = (m_frame_info.frameSize.width  + 15) & ~0xF;
            height = (m_frame_info.frameSize.height + 15) & ~0xF;

            if (width != m_frame_info.frameWidth || height != m_frame_info.frameHeight)
            {
                m_frame_info.frameWidth  = (int16_t)width;
                m_frame_info.frameHeight = (int16_t)height;
            }

            data_in += 7;

            std::copy(reinterpret_cast<const char*>(vp8_default_coeff_probs),
                      reinterpret_cast<const char*>(vp8_default_coeff_probs) + sizeof(vp8_default_coeff_probs),
                      reinterpret_cast<char*>(m_frameProbs.coeff_probs));

            UMC_SET_ZERO(m_frame_info.segmentFeatureData);
            m_frame_info.segmentAbsMode = 0;

            UMC_SET_ZERO(m_frame_info.refLoopFilterDeltas);
            UMC_SET_ZERO(m_frame_info.modeLoopFilterDeltas);

            m_refresh_info.refreshRefFrame = 3; // refresh alt+gold
            m_refresh_info.copy2Golden = 0;
            m_refresh_info.copy2Altref = 0;

            // restore default probabilities for Inter frames
            for (int i = 0; i < VP8Defs::VP8_NUM_MB_MODES_Y - 1; i++)
                m_frameProbs.mbModeProbY[i] = VP8Defs::vp8_mb_mode_y_probs[i];

            for (int i = 0; i < VP8Defs::VP8_NUM_MB_MODES_UV - 1; i++)
                m_frameProbs.mbModeProbUV[i] = VP8Defs::vp8_mb_mode_uv_probs[i];

            // restore default MV contexts
            std::copy(reinterpret_cast<const char*>(VP8Defs::vp8_default_mv_contexts),
                      reinterpret_cast<const char*>(VP8Defs::vp8_default_mv_contexts) + sizeof(VP8Defs::vp8_default_mv_contexts),
                      reinterpret_cast<char*>(m_frameProbs.mvContexts));

        }

        if ((int32_t) (data_in_end - data_in) < 4)
            throw vp8_exception(MFX_ERR_MORE_DATA);

        m_boolDecoder[VP8_FIRST_PARTITION].init(data_in, (int32_t) (data_in_end - data_in));

        if (m_frame_info.frameType == UMC::I_PICTURE)  // if VP8_KEY_FRAME
        {
            uint32_t bits = m_boolDecoder[VP8_FIRST_PARTITION].decode(2);

            m_frame_info.color_space_type = (uint8_t)(bits >> 1);
            m_frame_info.clamping_type    = (uint8_t)(bits & 1);

            // supported only color_space_type == 0
            // see "VP8 Data Format and Decoding Guide" ch.9.2
            if(m_frame_info.color_space_type)
                MFX_RETURN(MFX_ERR_UNSUPPORTED);
        }

        m_frame_info.segmentationEnabled = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode();

        if (m_frame_info.segmentationEnabled)
        {
            UpdateSegmentation(m_boolDecoder[VP8_FIRST_PARTITION]);
        }
        else
        {
            m_frame_info.updateSegmentData = 0;
            m_frame_info.updateSegmentMap  = 0;
        }

        mfxU8 bits = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode(7);

        m_frame_info.loopFilterType  = bits >> 6;
        m_frame_info.loopFilterLevel = bits & 0x3F;

        bits = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode(4);

        m_frame_info.sharpnessLevel     = bits >> 1;
        m_frame_info.mbLoopFilterAdjust = bits & 1;

        m_frame_info.modeRefLoopFilterDeltaUpdate = 0;

        if (m_frame_info.mbLoopFilterAdjust)
        {
            m_frame_info.modeRefLoopFilterDeltaUpdate = (uint8_t)m_boolDecoder[VP8Defs::VP8_FIRST_PARTITION].decode();
            if (m_frame_info.modeRefLoopFilterDeltaUpdate)
            {
                UpdateLoopFilterDeltas(m_boolDecoder[VP8_FIRST_PARTITION]);
            }
        }

        mfxU32 partitions;

        bits = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode(2);

        m_CodedCoeffTokenPartition = bits;

        partitions = 1 << bits;
        m_frame_info.numTokenPartitions = 1 << bits;

        m_frame_info.numPartitions = m_frame_info.numTokenPartitions;
        partitions =  m_frame_info.numPartitions;
        mfxU8 *pTokenPartition = data_in + first_partition_size;

        if (partitions > 1)
        {
            m_frame_info.partitionStart[0] = pTokenPartition + (partitions - 1) * 3;

            if (m_frame_info.partitionStart[0] > data_in_end)
                 MFX_RETURN(MFX_ERR_MORE_DATA);

            for (uint32_t i = 0; i < partitions - 1; i++)
            {
                m_frame_info.partitionSize[i] = (int32_t)(pTokenPartition[0]) |
                                                 (pTokenPartition[1] << 8) |
                                                 (pTokenPartition[2] << 16);
                pTokenPartition += 3;

                m_frame_info.partitionStart[i+1] = m_frame_info.partitionStart[i] + m_frame_info.partitionSize[i];

                if (m_frame_info.partitionStart[i+1] > data_in_end)
                    MFX_RETURN(MFX_ERR_MORE_DATA); //???

                m_boolDecoder[i + 1].init(m_frame_info.partitionStart[i], m_frame_info.partitionSize[i]);
            }
        }
        else
        {
            m_frame_info.partitionStart[0] = pTokenPartition;
        }

        m_frame_info.partitionSize[partitions - 1] = int32_t(data_in_end - m_frame_info.partitionStart[partitions - 1]);

        m_boolDecoder[partitions].init(m_frame_info.partitionStart[partitions - 1], m_frame_info.partitionSize[partitions - 1]);

        DecodeInitDequantization(m_boolDecoder[VP8_FIRST_PARTITION]);

        if (m_frame_info.frameType != UMC::I_PICTURE) // data in header for non-KEY frames
        {
            m_refresh_info.refreshRefFrame = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode(2);

            if (!(m_refresh_info.refreshRefFrame & 2))
                m_refresh_info.copy2Golden = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode(2);

            if (!(m_refresh_info.refreshRefFrame & 1))
                m_refresh_info.copy2Altref = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode(2);

            uint8_t bias = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode(2);

            m_refresh_info.refFrameBiasTable[1] = (bias & 1)^(bias >> 1); // ALTREF and GOLD (3^2 = 1)
            m_refresh_info.refFrameBiasTable[2] = (bias & 1);             // ALTREF and LAST
            m_refresh_info.refFrameBiasTable[3] = (bias >> 1);            // GOLD and LAST
        }

        m_refresh_info.refreshProbabilities = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode();

        if (!m_refresh_info.refreshProbabilities)
            m_frameProbs_saved = m_frameProbs;

        if (m_frame_info.frameType != UMC::I_PICTURE)
        {
            m_refresh_info.refreshLastFrame = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode();
        }
        else
            m_refresh_info.refreshLastFrame = 1;

        for (int i = 0; i < VP8_NUM_COEFF_PLANES; i++)
        {
            for (int j = 0; j < VP8_NUM_COEFF_BANDS; j++)
            {
                for (int k = 0; k < VP8_NUM_LOCAL_COMPLEXITIES; k++)
                {
                    for (int l = 0; l < VP8_NUM_COEFF_NODES; l++)
                    {
                        mfxU8 prob = vp8_coeff_update_probs[i][j][k][l];
                        mfxU8 flag = (uint8_t)m_boolDecoder[VP8Defs::VP8_FIRST_PARTITION].decode(1, prob);

                        if (flag)
                            m_frameProbs.coeff_probs[i][j][k][l] = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode(8);
                    }
                }
            }
        }

        m_frame_info.mbSkipEnabled = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode();
        m_frame_info.skipFalseProb = 0;

        if (m_frame_info.mbSkipEnabled)
            m_frame_info.skipFalseProb = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode(8);

        if (m_frame_info.frameType != UMC::I_PICTURE)
        {
            m_frame_info.intraProb = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode(8);
            m_frame_info.lastProb  = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode(8);
            m_frame_info.goldProb  = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode(8);

            if (m_boolDecoder[VP8_FIRST_PARTITION].decode())
            {
                int i = 0;

                do
                {
                    m_frameProbs.mbModeProbY[i] = uint8_t(m_boolDecoder[VP8_FIRST_PARTITION].decode(8));
                }
                while (++i < 4);
            }

            if (m_boolDecoder[VP8_FIRST_PARTITION].decode())
            {
                int i = 0;

                do
                {
                    m_frameProbs.mbModeProbUV[i] = uint8_t(m_boolDecoder[VP8_FIRST_PARTITION].decode(8));
                }
                while (++i < 3);
            }

            int i = 0;

            do
            {
                mfxU8 *up = (uint8_t *)&vp8_mv_update_probs[i];
                mfxU8 *p = m_frameProbs.mvContexts[i];
                mfxU8 *pstop = p + 19;

                do
                {
                    if (m_boolDecoder[VP8_FIRST_PARTITION].decode(1, *up++))
                    {
                        const uint8_t x = (uint8_t)m_boolDecoder[VP8_FIRST_PARTITION].decode(7);

                        *p = x ? x << 1 : 1;
                    }
                }
                while (++p < pstop);
            }
            while (++i < 2);
        }

    // Header info consumed bits
    m_frame_info.entropyDecSize = m_boolDecoder[VP8_FIRST_PARTITION].pos() * 8 - 3 * 8 - m_boolDecoder[VP8_FIRST_PARTITION].bitcount();

    // Subtract completely consumed bytes + current byte. Current is completely consumed if bitcount is 8.
    m_frame_info.firstPartitionSize = first_partition_size - ((m_frame_info.entropyDecSize + 7) >> 3);

    }
    catch(const vp8_exception & ex)
    {
        mfxStatus sts = (mfxStatus)ex.GetStatus();
        MFX_RETURN(sts);
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP8_HW::GetFrame(UMC::MediaData* /*in*/, UMC::FrameData** /*out*/)
{
    return MFX_ERR_NONE;
}

mfxTaskThreadingPolicy VideoDECODEVP8_HW::GetThreadingPolicy()
{
    return MFX_TASK_THREADING_INTRA;
}

mfxStatus VideoDECODEVP8_HW::GetVideoParam(mfxVideoParam *pPar)
{
    if (!m_is_initialized)
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(pPar);

    pPar->mfx = m_on_init_video_params.mfx;

    pPar->Protected = m_on_init_video_params.Protected;
    pPar->IOPattern = m_on_init_video_params.IOPattern;
    pPar->AsyncDepth = m_on_init_video_params.AsyncDepth;

    pPar->mfx.FrameInfo.FrameRateExtD = m_on_init_video_params.mfx.FrameInfo.FrameRateExtD;
    pPar->mfx.FrameInfo.FrameRateExtN = m_on_init_video_params.mfx.FrameInfo.FrameRateExtN;

    pPar->mfx.FrameInfo.AspectRatioH = m_on_init_video_params.mfx.FrameInfo.AspectRatioH;
    pPar->mfx.FrameInfo.AspectRatioW = m_on_init_video_params.mfx.FrameInfo.AspectRatioW;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP8_HW::GetDecodeStat(mfxDecodeStat *pStat)
{
    if (!m_is_initialized)
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(pStat);

    m_stat.NumSkippedFrame = 0;
    m_stat.NumCachedFrame = 0;

    *pStat = m_stat;

    return MFX_ERR_NONE;

}

mfxStatus VideoDECODEVP8_HW::DecodeFrame(mfxBitstream *, mfxFrameSurface1 *, mfxFrameSurface1 *)
{
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP8_HW::GetUserData(mfxU8 *pUserData, mfxU32 *pSize, mfxU64 *pTimeStamp)
{
    if (!m_is_initialized)
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR3(pUserData, pSize, pTimeStamp);

    MFX_RETURN(MFX_ERR_UNSUPPORTED);
}

mfxStatus VideoDECODEVP8_HW::GetPayload(mfxU64 *pTimeStamp, mfxPayload *pPayload)
{
    if (!m_is_initialized)
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR3(pTimeStamp, pPayload, pPayload->Data);

    MFX_RETURN(MFX_ERR_UNSUPPORTED);
}

mfxStatus VideoDECODEVP8_HW::SetSkipMode(mfxSkipMode /*mode*/)
{
    if (!m_is_initialized)
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP8_HW::GetSurface(mfxFrameSurface1* & surface, mfxSurfaceHeader* import_surface)
{
    MFX_CHECK(m_surface_source, MFX_ERR_NOT_INITIALIZED);

    return m_surface_source->GetSurface(surface, import_surface);
}

//////////////////////////////////////////////////////////////////////////////
// MFX_VP8_BoolDecoder

const int MFX_VP8_BoolDecoder::range_normalization_shift[64] =
{
  7, 6, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

#endif //MFX_ENABLE_VP8_VIDEO_DECODE
