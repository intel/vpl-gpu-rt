// Copyright (c) 2018-2021 Intel Corporation
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

#if defined(MFX_ENABLE_MPEG2_VIDEO_DECODE)

#include <thread>
#include <assert.h>

#include "mfx_mpeg2_decode.h"
#include "mfx_task.h"
#include "mfx_umc_alloc_wrapper.h"
#include "libmfx_core_hw.h"

#include "umc_mpeg2_utils.h"
#include "umc_mpeg2_decoder_va.h"
#include "umc_va_base.h"

#include "libmfx_core_interface.h"
using UMC_MPEG2_DECODER::MPEG2DecoderFrame;

// Convert display PS to MFX PS
inline mfxU16 GetMFXPicStruct(const MPEG2DecoderFrame& frame, bool extended)
{
    mfxU16 picStruct;

    switch (frame.displayPictureStruct)
    {
    case UMC_MPEG2_DECODER::DPS_TOP:
        picStruct = MFX_PICSTRUCT_FIELD_TFF;
        break;
    case UMC_MPEG2_DECODER::DPS_BOTTOM:
        picStruct = MFX_PICSTRUCT_FIELD_BFF;
        break;
    case UMC_MPEG2_DECODER::DPS_TOP_BOTTOM:
    case UMC_MPEG2_DECODER::DPS_BOTTOM_TOP:
    {
        mfxU32 fieldFlag = (frame.displayPictureStruct == UMC_MPEG2_DECODER::DPS_TOP_BOTTOM) ? MFX_PICSTRUCT_FIELD_TFF : MFX_PICSTRUCT_FIELD_BFF;
        picStruct = (mfxU16)((frame.isProgressiveFrame) ? MFX_PICSTRUCT_PROGRESSIVE : fieldFlag);

        picStruct |= extended ? fieldFlag : 0;
    }
    break;
    case UMC_MPEG2_DECODER::DPS_TOP_BOTTOM_TOP:
    case UMC_MPEG2_DECODER::DPS_BOTTOM_TOP_BOTTOM:
    {
        picStruct = MFX_PICSTRUCT_PROGRESSIVE;

        if (extended)
        {
            picStruct |= MFX_PICSTRUCT_FIELD_REPEATED;
            picStruct |= (frame.displayPictureStruct == UMC_MPEG2_DECODER::DPS_TOP_BOTTOM_TOP) ? MFX_PICSTRUCT_FIELD_TFF : MFX_PICSTRUCT_FIELD_BFF;
        }
    }
    break;
    case UMC_MPEG2_DECODER::DPS_FRAME_DOUBLING:
        picStruct = MFX_PICSTRUCT_PROGRESSIVE;
        picStruct |= extended ? MFX_PICSTRUCT_FRAME_DOUBLING : 0;
        break;
    case UMC_MPEG2_DECODER::DPS_FRAME_TRIPLING:
        picStruct = MFX_PICSTRUCT_PROGRESSIVE;
        picStruct |= extended ? MFX_PICSTRUCT_FRAME_TRIPLING : 0;
        break;
    case UMC_MPEG2_DECODER::DPS_FRAME:
    default:
        picStruct = MFX_PICSTRUCT_PROGRESSIVE;
        break;
    }

    return picStruct;
}

// Fill frame types
void SetFrameType(const MPEG2DecoderFrame* frame, mfxFrameSurface1* surface)
{
    auto frameType = (mfxExtDecodedFrameInfo*)GetExtendedBuffer(surface->Data.ExtParam, surface->Data.NumExtParam, MFX_EXTBUFF_DECODED_FRAME_INFO);
    if (frameType)
    {
        switch (frame->GetAU(0)->GetType())
        {
        case UMC_MPEG2_DECODER::MPEG2_I_PICTURE:
            frameType->FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR;
            break;
        case UMC_MPEG2_DECODER::MPEG2_P_PICTURE:
            frameType->FrameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            break;
        case UMC_MPEG2_DECODER::MPEG2_B_PICTURE:
            frameType->FrameType = MFX_FRAMETYPE_B;
            break;
        }

        bool fillBottom = frame->GetAU(1)->IsFilled() || !frame->isProgressiveSequence;
        if (fillBottom)
        {
            auto type = frame->GetAU(1)->IsFilled() ? frame->GetAU(1)->GetType() : frame->GetAU(0)->GetType();
            switch (type)
            {
            case UMC_MPEG2_DECODER::MPEG2_I_PICTURE:
                frameType->FrameType |= MFX_FRAMETYPE_xI | MFX_FRAMETYPE_xREF;
                break;
            case UMC_MPEG2_DECODER::MPEG2_P_PICTURE:
                frameType->FrameType |= MFX_FRAMETYPE_xP | MFX_FRAMETYPE_xREF;
                break;
            case UMC_MPEG2_DECODER::MPEG2_B_PICTURE:
                frameType->FrameType |= MFX_FRAMETYPE_xB;
                break;
            }
        }
    }
}

// Set MPEG2 group of pictures timecode
void SetTimeCode(const MPEG2DecoderFrame* frame, mfxFrameSurface1* surface)
{
    auto timeCode = (mfxExtTimeCode*)GetExtendedBuffer(surface->Data.ExtParam, surface->Data.NumExtParam, MFX_EXTBUFF_TIME_CODE);
    const auto group = frame->group;
    if (!timeCode || !group)
        return;

    timeCode->DropFrameFlag = group->drop_frame_flag;
    timeCode->TimeCodePictures = group->time_code_pictures;
    timeCode->TimeCodeHours = group->time_code_hours;
    timeCode->TimeCodeMinutes = group->time_code_minutes;
    timeCode->TimeCodeSeconds = group->time_code_seconds;
}

VideoDECODEMPEG2::VideoDECODEMPEG2(VideoCORE* core, mfxStatus* sts)
    : m_core(core)
    , m_first_run(true)
    , m_allow_null_work_surface(false)
    , m_response()
    , m_response_alien()
    , m_stat()
{
    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

VideoDECODEMPEG2::~VideoDECODEMPEG2()
{
    Close();
}

// Initialize decoder
mfxStatus VideoDECODEMPEG2::Init(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(!m_decoder, MFX_ERR_UNDEFINED_BEHAVIOR);

    std::lock_guard<std::mutex> guard(m_guard);

    eMFXPlatform platform = UMC_MPEG2_DECODER::GetPlatform_MPEG2(m_core, par);

    eMFXHWType type = platform == MFX_PLATFORM_HARDWARE ? m_core->GetHWType() : MFX_HW_UNKNOWN;

    // Checking general init params
    mfxStatus mfxSts = CheckVideoParamDecoders(par, type);
    MFX_CHECK(mfxSts >= MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(UMC_MPEG2_DECODER::MFX_PROFILE_MPEG1 != par->mfx.CodecProfile, MFX_ERR_UNSUPPORTED);

    // MPEG2 specific param checking
    MFX_CHECK(UMC_MPEG2_DECODER::CheckVideoParam(par), MFX_ERR_INVALID_VIDEO_PARAM);

    m_init_video_par = *par;
    m_first_video_par = *par;
    m_first_video_par.mfx.NumThread = (mfxU16)CalculateNumThread(par);

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_first_video_par);

    m_video_par = m_first_video_par;
    m_video_par.CreateExtendedBuffer(MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    m_video_par.CreateExtendedBuffer(MFX_EXTBUFF_CODING_OPTION_SPSPPS);

    MFX_CHECK(platform != MFX_PLATFORM_SOFTWARE, MFX_ERR_UNSUPPORTED);

    m_decoder.reset(new UMC_MPEG2_DECODER::MPEG2DecoderVA());

    // Internal or external memory
    bool internal = par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    mfxFrameAllocRequest request{};
    mfxFrameAllocRequest request_internal{};
    m_response = {};
    m_response_alien = {};

    // Number of surfaces required
    mfxSts = QueryIOSurfInternal(&m_video_par, &request);
    MFX_CHECK_STS(mfxSts);

    if (IsD3D9Simulation(*m_core))
        internal = true;

    request.Type |= internal ? MFX_MEMTYPE_INTERNAL_FRAME : MFX_MEMTYPE_EXTERNAL_FRAME;
    request_internal = request;

    m_response_alien = m_response;

    try
    {
        m_surface_source.reset(new SurfaceSource(m_core, *par, platform, request, request_internal, m_response, m_response_alien));
    }
    catch (const std::system_error& ex)
    {
        MFX_CHECK_STS(mfxStatus(ex.code().value()));
    }

    UMC_MPEG2_DECODER::MPEG2DecoderParams vp{};
    vp.allocator = m_surface_source.get();
    vp.async_depth = CalculateAsyncDepth(par);

    mfxSts = m_core->CreateVA(par, &request, &m_response, m_surface_source.get());
    MFX_CHECK_STS(mfxSts);

    m_core->GetVA((mfxHDL*)&vp.pVideoAccelerator, MFX_MEMTYPE_FROM_DECODE);

    ConvertMFXParamsToUMC(par, &vp);

    // UMC decoder initialization
    UMC::Status umcSts = m_decoder->Init(&vp);
    MFX_CHECK(UMC::UMC_OK == umcSts, MFX_ERR_NOT_INITIALIZED);

    m_first_run = true;

    m_decoder->SetVideoParams(m_first_video_par);

    return isNeedChangeVideoParamWarning ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::QueryImplsDescription(
    VideoCORE& core,
    mfxDecoderDescription::decoder& caps,
    mfx::PODArraysHolder& ah)
{
    const mfxU16 SupportedProfiles[] =
    {
        MFX_PROFILE_MPEG2_SIMPLE
        , MFX_PROFILE_MPEG2_MAIN
        , MFX_PROFILE_MPEG2_HIGH
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

    caps.CodecID = MFX_CODEC_MPEG2;
    caps.MaxcodecLevel = MFX_LEVEL_MPEG2_HIGH1440;

    mfxVideoParam par;
    memset(&par, 0, sizeof(par));
    par.mfx.CodecId = MFX_CODEC_MPEG2;
    par.mfx.CodecLevel = caps.MaxcodecLevel;

    mfxStatus sts = MFX_ERR_NONE;
    for (mfxU16 profile : SupportedProfiles)
    {
        par.mfx.CodecProfile = profile;
        // Set FourCC to pass Query check
        par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

        sts = VideoDECODEMPEG2::Query(&core, &par, &par);
        if (sts != MFX_ERR_NONE) continue;

        auto& pfCaps = ah.PushBack(caps.Profiles);
        pfCaps.Profile = profile;

        for (auto memType : SupportedMemTypes)
        {
            auto& memCaps = ah.PushBack(pfCaps.MemDesc);
            memCaps.MemHandleType = memType;
            memCaps.Width = { 16, 2048, 16 };
            memCaps.Height = { 16, 2048, 16 };

            for (auto fcc : SupportedFourCC)
            {
                par.mfx.FrameInfo.FourCC = fcc;
                sts = VideoDECODEMPEG2::Query(&core, &par, &par);
                if (sts != MFX_ERR_NONE) continue;

                ah.PushBack(memCaps.ColorFormats) = fcc;
                ++memCaps.NumColorFormats;
            }
            ++pfCaps.NumMemTypes;
        }
        ++caps.NumProfiles;
    }

    return MFX_ERR_NONE;
}

// Reset decoder
mfxStatus VideoDECODEMPEG2::Reset(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    std::lock_guard<std::mutex> guard(m_guard);

    eMFXHWType type = m_core->GetHWType();

    mfxStatus mfxSts = CheckVideoParamDecoders(par, type);
    MFX_CHECK(mfxSts >= MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(UMC_MPEG2_DECODER::CheckVideoParam(par), MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(IsSameVideoParam(par, &m_init_video_par, type), MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    eMFXPlatform platform = UMC_MPEG2_DECODER::GetPlatform_MPEG2(m_core, par);
    MFX_CHECK(platform == MFX_PLATFORM_HARDWARE, MFX_ERR_UNSUPPORTED);

    m_decoder->Reset();

    MFX_CHECK(m_surface_source->Reset() == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);

    m_first_run = true;

    m_stat = {};
    m_first_video_par = *par;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_first_video_par);
    m_video_par = m_first_video_par;
    m_video_par.CreateExtendedBuffer(MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    m_video_par.CreateExtendedBuffer(MFX_EXTBUFF_CODING_OPTION_SPSPPS);

    m_video_par.mfx.NumThread = (mfxU16)CalculateNumThread(par);

    m_decoder->SetVideoParams(m_first_video_par);

    return isNeedChangeVideoParamWarning ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

// Close decoder
mfxStatus VideoDECODEMPEG2::Close(void)
{
    std::lock_guard<std::mutex> guard(m_guard);

    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    m_decoder->Close();
    m_surface_source->Close();

    m_first_run = true;

    m_stat = {};

    return MFX_ERR_NONE;
}

// Check if new parameters are compatible with new parameters
bool VideoDECODEMPEG2::IsSameVideoParam(mfxVideoParam * newPar, mfxVideoParam * oldPar, eMFXHWType/* type*/) const
{
    auto const mask =
          MFX_IOPATTERN_OUT_SYSTEM_MEMORY
        | MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    if ((newPar->IOPattern & mask) !=
        (oldPar->IOPattern & mask))
        return false;

    MFX_CHECK(newPar->Protected == oldPar->Protected, false);
    MFX_CHECK(newPar->mfx.FrameInfo.FourCC == oldPar->mfx.FrameInfo.FourCC, false);
    MFX_CHECK(CalculateAsyncDepth(newPar) == CalculateAsyncDepth(oldPar), false);

    mfxFrameAllocRequest requestOld{};
    mfxFrameAllocRequest requestNew{};

    MFX_CHECK(MFX_ERR_NONE == QueryIOSurfInternal(oldPar, &requestOld), false);
    MFX_CHECK(MFX_ERR_NONE == QueryIOSurfInternal(newPar, &requestNew), false);

    MFX_CHECK(newPar->mfx.FrameInfo.Height <= oldPar->mfx.FrameInfo.Height, false);
    MFX_CHECK(newPar->mfx.FrameInfo.Width <= oldPar->mfx.FrameInfo.Width, false);
    MFX_CHECK(newPar->mfx.FrameInfo.ChromaFormat == oldPar->mfx.FrameInfo.ChromaFormat, false);

    if (m_response.NumFrameActual)
    {
        MFX_CHECK(requestNew.NumFrameMin <= m_response.NumFrameActual, false);
    }
    else
    {
        if (requestNew.NumFrameMin > requestOld.NumFrameMin || requestNew.Type != requestOld.Type)
            return false;
    }

    return true;
}

mfxTaskThreadingPolicy VideoDECODEMPEG2::GetThreadingPolicy()
{
    return MFX_TASK_THREADING_SHARED;
}

mfxStatus VideoDECODEMPEG2::Query(VideoCORE* core, mfxVideoParam* in, mfxVideoParam* out)
{
    return UMC_MPEG2_DECODER::Query(core, in, out);
}

mfxStatus VideoDECODEMPEG2::QueryIOSurf(VideoCORE* core, mfxVideoParam* par, mfxFrameAllocRequest* request)
{
    MFX_CHECK_NULL_PTR2(par, request);

    eMFXPlatform platform = UMC_MPEG2_DECODER::GetPlatform_MPEG2(core, par);

    if (platform != MFX_PLATFORM_HARDWARE)
        MFX_RETURN(MFX_ERR_UNSUPPORTED);

    mfxVideoParam params(*par);
    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&params);

    if (   !(par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        && !(par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);

    if ((par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);

    mfxStatus sts = QueryIOSurfInternal(&params, request);
    MFX_CHECK_STS(sts);

    bool isInternalManaging = (params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    bool IsD3D9SimWithVideoMem = IsD3D9Simulation(*core) && (params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY);
    if (IsD3D9SimWithVideoMem)
        isInternalManaging = true;

    if (isInternalManaging)
    {
        request->NumFrameSuggested = request->NumFrameMin = (mfxU16)(CalculateAsyncDepth(par) + 1); // "+1" because we release the latest displayed surface in _sync_ part when another surface comes
        if (!IsD3D9SimWithVideoMem)
            request->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }

    request->Type |= MFX_MEMTYPE_EXTERNAL_FRAME;

    if (platform != core->GetPlatformType())
    {
        assert(platform == MFX_PLATFORM_SOFTWARE);
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    return isNeedChangeVideoParamWarning ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

// Actually calculate needed frames number
mfxStatus VideoDECODEMPEG2::QueryIOSurfInternal(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoDECODEMPEG2::QueryIOSurfInternal");
    request->Info = par->mfx.FrameInfo;

    mfxU16 dpbSize = UMC_MPEG2_DECODER::NUM_REF_FRAMES;
    mfxU16 numMin = (mfxU16)(dpbSize + CalculateAsyncDepth(par) // DPB + AsyncDepth +
        + 1); // input surface for DPB dumping (because we release the latest displayed surface in _sync_ part when another surface comes)
    request->NumFrameMin = numMin;
    request->NumFrameSuggested = request->NumFrameMin;

    request->Type = (MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE);

    return MFX_ERR_NONE;
}

// Decode stream headers and fill video parameters
mfxStatus VideoDECODEMPEG2::DecodeHeader(VideoCORE* core, mfxBitstream* bs, mfxVideoParam* par)
{
    MFX_CHECK(core, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxStatus sts = CheckBitstream(bs);
    MFX_CHECK_STS(sts);

    try
    {
        UMC::Status res = UMC_MPEG2_DECODER::MPEG2Decoder::DecodeHeader(*bs, *par);
        MFX_CHECK(UMC::UMC_OK == res, ConvertStatusUmc2Mfx(res));
    }
    catch (UMC_MPEG2_DECODER::mpeg2_exception & ex)
    {
        return ConvertStatusUmc2Mfx(ex.GetStatus());
    }

    return MFX_ERR_NONE;
}

// MediaSDK DECODE_GetVideoParam API function
mfxStatus VideoDECODEMPEG2::GetVideoParam(mfxVideoParam *par)
{
    std::lock_guard<std::mutex> guard(m_guard);

    MFX_CHECK(m_decoder, MFX_ERR_UNDEFINED_BEHAVIOR);

    MFX_CHECK_NULL_PTR1(par);

    FillVideoParam(&m_video_par, true);

    par->mfx = m_video_par.mfx;

    par->Protected = m_video_par.Protected;
    par->IOPattern = m_video_par.IOPattern;
    par->AsyncDepth = m_video_par.AsyncDepth;

    auto videoSignal = (mfxExtVideoSignalInfo*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    if (videoSignal)
    {
        auto videoSignalInternal = m_video_par.GetExtendedBuffer<mfxExtVideoSignalInfo>(MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
        *videoSignal = *videoSignalInternal;
    }

    // Sequence header
    auto mfxSeq = (mfxExtCodingOptionSPSPPS*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS);
    if (mfxSeq)
    {
        auto spsPpsInternal = m_video_par.GetExtendedBuffer<mfxExtCodingOptionSPSPPS>(MFX_EXTBUFF_CODING_OPTION_SPSPPS);

        mfxSeq->SPSId = 0;

        if (mfxSeq->SPSBufSize < spsPpsInternal->SPSBufSize ||
            mfxSeq->PPSBufSize < spsPpsInternal->PPSBufSize)
            MFX_RETURN(MFX_ERR_NOT_ENOUGH_BUFFER);

        mfxSeq->SPSBufSize = spsPpsInternal->SPSBufSize;

        std::copy(spsPpsInternal->SPSBuffer, spsPpsInternal->SPSBuffer + mfxSeq->SPSBufSize, mfxSeq->SPSBuffer);
    }

    par->mfx.FrameInfo.FrameRateExtN = m_first_video_par.mfx.FrameInfo.FrameRateExtN;
    par->mfx.FrameInfo.FrameRateExtD = m_first_video_par.mfx.FrameInfo.FrameRateExtD;

    if (!par->mfx.FrameInfo.FrameRateExtD && !par->mfx.FrameInfo.FrameRateExtN)
    {
        par->mfx.FrameInfo.FrameRateExtD = m_video_par.mfx.FrameInfo.FrameRateExtD;
        par->mfx.FrameInfo.FrameRateExtN = m_video_par.mfx.FrameInfo.FrameRateExtN;

        if (!par->mfx.FrameInfo.FrameRateExtD && !par->mfx.FrameInfo.FrameRateExtN)
        {
            par->mfx.FrameInfo.FrameRateExtN = 0;
            par->mfx.FrameInfo.FrameRateExtD = 0;
        }
    }

    par->mfx.FrameInfo.AspectRatioW = m_first_video_par.mfx.FrameInfo.AspectRatioW;
    par->mfx.FrameInfo.AspectRatioH = m_first_video_par.mfx.FrameInfo.AspectRatioH;

    if (!par->mfx.FrameInfo.AspectRatioH && !par->mfx.FrameInfo.AspectRatioW)
    {
        par->mfx.FrameInfo.AspectRatioH = m_video_par.mfx.FrameInfo.AspectRatioH;
        par->mfx.FrameInfo.AspectRatioW = m_video_par.mfx.FrameInfo.AspectRatioW;

        if (!par->mfx.FrameInfo.AspectRatioH && !par->mfx.FrameInfo.AspectRatioW)
        {
            par->mfx.FrameInfo.AspectRatioH = 1;
            par->mfx.FrameInfo.AspectRatioW = 1;
        }
    }

    return MFX_ERR_NONE;
}

// MediaSDK DECODE_GetDecodeStat API function
mfxStatus VideoDECODEMPEG2::GetDecodeStat(mfxDecodeStat *stat)
{
    std::lock_guard<std::mutex> guard(m_guard);

    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(stat);

    m_stat.NumSkippedFrame = m_decoder->GetNumSkipFrames();
    m_stat.NumCachedFrame = m_decoder->GetNumCachedFrames();

    *stat = m_stat;
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMPEG2::DecodeFrameCheck(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, MFX_ENTRY_POINT* entry_point)
{
    MFX_CHECK_NULL_PTR1(entry_point);

    std::lock_guard<std::mutex> guard(m_guard);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(surface_out);

    mfxStatus sts = bs ? CheckBitstream(bs) : MFX_ERR_NONE;
    MFX_CHECK_STS(sts);

    // in case of EOS (flushing) decoder may return buffered surface
    // without surface_work
    if (surface_work != nullptr)
    {
        sts = CheckFrameInfoCodecs(&surface_work->Info, MFX_CODEC_MPEG2);
        MFX_CHECK_STS(sts);

        sts = CheckFrameData(surface_work);
        MFX_CHECK_STS(sts);
    }
    else
    {
        bool allow_null_work_surface = SupportsVPLFeatureSet(*m_core);

        /*
        In case 1.x core and nullptr surface_work we need to return back to user one of surfaces which were previously decoded.
        In case of system memory these surfaces are video surfaces which were allocated by decoder. And we can't return them because user wait system surface.
        So, we can allow surface_work to be nullptr only for video memory or for new core (model 3).
        */
        MFX_CHECK(allow_null_work_surface || m_init_video_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY, MFX_ERR_UNSUPPORTED)

        bool empty_bitstream = !bs || bs->DataFlag == MFX_BITSTREAM_EOS;

        if (!empty_bitstream && allow_null_work_surface)
        {
            m_allow_null_work_surface = true;
        }

        if (!m_allow_null_work_surface)
        {
            MFX_CHECK(empty_bitstream, MFX_ERR_NULL_PTR);
        }
    }

    mfxThreadTask task;
    sts = SubmitFrame(bs, surface_work, surface_out, &task);
    MFX_CHECK_STS(sts);

    entry_point->pRoutine = &DecodeRoutine;
    entry_point->pCompleteProc = &CompleteProc;
    entry_point->pState = this;
    entry_point->requiredNumThreads = 1;
    entry_point->pParam = task;

    return sts;
}

// MediaSDK DECODE_SetSkipMode API function
mfxStatus VideoDECODEMPEG2::SetSkipMode(mfxSkipMode mode)
{
    std::lock_guard<std::mutex> guard(m_guard);

    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    int32_t current_speed = 0; // don't change, just check current value
    m_decoder->ChangeVideoDecodingSpeed(current_speed); // ChangeVideoDecodingSpeed updates the argument with new value

    int32_t new_speed = 0;
    switch (mode)
    {
    case MFX_SKIPMODE_NOSKIP:
        new_speed = -10; // restore default
        break;

    case MFX_SKIPMODE_MORE:
        new_speed = 1;
        break;

    case MFX_SKIPMODE_LESS:
        new_speed = -1;
        break;
    default:
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    m_decoder->ChangeVideoDecodingSpeed(new_speed);

    return
        current_speed == new_speed ? MFX_WRN_VALUE_NOT_CHANGED : MFX_ERR_NONE;
}

// Return stream payload
mfxStatus VideoDECODEMPEG2::GetPayload(mfxU64 *ts, mfxPayload *payload)
{
    std::lock_guard<std::mutex> guard(m_guard);

    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR3(ts, payload, payload->Data);

    const auto payloads = m_decoder->GetPayloadStorage();
    MFX_CHECK(payloads, MFX_ERR_UNKNOWN);

    const auto msg = payloads->GetPayloadMessage();

    static const mfxU16 USER_DATA_START_CODE = 0x000001B2;

    if (msg)
    {
        if (payload->BufSize < msg->msg_size)
            MFX_RETURN(MFX_ERR_NOT_ENOUGH_BUFFER);

        *ts = GetMfxTimeStamp(msg->timestamp);

        std::copy(msg->data, msg->data + msg->msg_size, payload->Data);

        payload->NumBit = (mfxU32)(msg->msg_size * 8);
        payload->Type = USER_DATA_START_CODE;
    }
    else
    {
        payload->NumBit = 0;
        *ts = MFX_TIME_STAMP_INVALID;
    }

    return MFX_ERR_NONE;
}

// Initialize threads callbacks
mfxStatus VideoDECODEMPEG2::SubmitFrame(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, mfxThreadTask* task)
{
    MFX_CHECK_NULL_PTR1(task);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = SubmitFrame(bs, surface_work, surface_out);
    MFX_CHECK_STS(sts);

    std::unique_ptr<TaskInfo> info(new TaskInfo); // to prevent memory leak

    if (surface_work)
        info->surface_work = surface_work;
    if (*surface_out)
        info->surface_out = *surface_out;

    *task = reinterpret_cast<mfxThreadTask>(info.release());

    return MFX_ERR_NONE;
}

// Wait until a frame is ready to be output and set necessary surface flags
mfxStatus VideoDECODEMPEG2::DecodeFrame(mfxFrameSurface1 *surface_out, MPEG2DecoderFrame* frame) const
{
    MFX_CHECK(surface_out, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(frame, MFX_ERR_UNDEFINED_BEHAVIOR);

    const auto error = frame->GetError();
    surface_out->Data.Corrupted = 0;

    // Set corruption flags
    if (error & UMC::ERROR_FRAME_DEVICE_FAILURE)
    {
        surface_out->Data.Corrupted |= MFX_CORRUPTION_MAJOR;
        return error == UMC::UMC_ERR_GPU_HANG ? MFX_ERR_GPU_HANG : MFX_ERR_DEVICE_FAILED;
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

    const auto id = frame->GetFrameData()->GetFrameMID();
    mfxStatus sts = m_surface_source->PrepareToOutput(surface_out, id, &m_video_par); // Copy to system memory if needed
    frame->SetDisplayed();

    return sts;
}

// Decoder instance threads entry point. Do async tasks here
mfxStatus VideoDECODEMPEG2::QueryFrame(mfxThreadTask task)
{
    MFX_CHECK_NULL_PTR1(task);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    auto info = reinterpret_cast<TaskInfo*>(task);

    auto surface_out = info->surface_out;
    MFX_CHECK(surface_out, MFX_ERR_UNDEFINED_BEHAVIOR);

    const auto id = m_surface_source->FindSurface(surface_out);
    auto frame = m_decoder->FindFrameByMemID(id);
    MFX_CHECK(frame, MFX_ERR_UNDEFINED_BEHAVIOR);

    MFX_CHECK(frame->DecodingStarted(), MFX_ERR_UNDEFINED_BEHAVIOR);

    if (!frame->DecodingCompleted())
    {
        m_decoder->QueryFrames(*frame);
    }

    MFX_CHECK(frame->DecodingCompleted(), MFX_TASK_WORKING);

    mfxStatus sts = DecodeFrame(surface_out, frame);
    MFX_CHECK_STS(sts);

    return MFX_TASK_DONE;
}

// Check if there is enough data to start decoding in async mode
mfxStatus VideoDECODEMPEG2::SubmitFrame(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out)
{
    mfxStatus sts = MFX_ERR_NONE;
    try
    {
        bool exit = false;

        MFXMediaDataAdapter src(bs);

        for (;;)
        {
            sts = m_surface_source->SetCurrentMFXSurface(surface_work);
            MFX_CHECK_STS(sts);

            UMC::Status umcRes = (m_surface_source->HasFreeSurface() || surface_work == nullptr) ?
                m_decoder->AddSource(bs ? &src : nullptr) :
                UMC::UMC_ERR_NEED_FORCE_OUTPUT; // Exit with MFX_WRN_DEVICE_BUSY

            src.Save(bs); // Update input mfxBitstream offset

            exit = (umcRes != UMC::UMC_OK);

            if (umcRes == UMC::UMC_NTF_NEW_RESOLUTION ||
                umcRes == UMC::UMC_WRN_REPOSITION_INPROGRESS ||
                umcRes == UMC::UMC_ERR_UNSUPPORTED)
            {
                FillVideoParam(&m_video_par, true); // Update current mfx video params
            }

            switch (umcRes)
            {
            case UMC::UMC_ERR_INVALID_STREAM:
                umcRes = UMC::UMC_OK;
                exit = false;
                break;

            case UMC::UMC_NTF_NEW_RESOLUTION:
                MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            case UMC::UMC_ERR_DEVICE_FAILED: // return errors immediatelly
                MFX_RETURN(MFX_ERR_DEVICE_FAILED);

            case UMC::UMC_ERR_GPU_HANG: // return errors immediatelly
                MFX_RETURN(MFX_ERR_GPU_HANG);
            }

            if (umcRes == UMC::UMC_WRN_REPOSITION_INPROGRESS) // New sequence header
            {
                if (!m_first_run)
                {
                    sts = MFX_WRN_VIDEO_PARAM_CHANGED;
                }
                else
                {
                    umcRes = UMC::UMC_OK;
                    exit = false;
                    m_first_run = false;
                }
            }

            if (umcRes == UMC::UMC_OK && !m_surface_source->HasFreeSurface())
            {
                sts = MFX_ERR_MORE_SURFACE;
                exit = true;
            }
            else if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || umcRes == UMC::UMC_WRN_INFO_NOT_READY || umcRes == UMC::UMC_ERR_NEED_FORCE_OUTPUT)
            {
                sts = umcRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER ? (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK : MFX_WRN_DEVICE_BUSY;
            }
            else if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA || umcRes == UMC::UMC_ERR_SYNC)
            {
                sts = MFX_ERR_MORE_DATA;
            }

            auto frame = GetFrameToDisplay();
            if (frame)
            {
                m_stat.NumFrame++;
                m_stat.NumError += frame->GetError() ? 1 : 0;
                return FillOutputSurface(surface_work, surface_out, frame);
            }

            if (exit)
                break;

        } // for (;;)
    }
    catch (const UMC_MPEG2_DECODER::mpeg2_exception & ex)
    {
        FillVideoParam(&m_video_par, false);

        if (ex.GetStatus() == UMC::UMC_ERR_ALLOC)
        {
            // check incompatibility of video params
            if (m_init_video_par.mfx.FrameInfo.Width != m_video_par.mfx.FrameInfo.Width ||
                m_init_video_par.mfx.FrameInfo.Height != m_video_par.mfx.FrameInfo.Height)
            {
                MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            }
        }

        return ConvertUMCStatusToMfx(ex.GetStatus());
    }
    catch (std::bad_alloc const&)
    {
        MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
    }
    catch (...)
    {
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }

    return sts;
}

// Decoder threads entry point
mfxStatus VideoDECODEMPEG2::DecodeRoutine(void* state, void* param, mfxU32, mfxU32)
{
    auto decoder = reinterpret_cast<VideoDECODEMPEG2*>(state);
    MFX_CHECK(decoder, MFX_ERR_UNDEFINED_BEHAVIOR);

    auto task = reinterpret_cast<TaskInfo*>(param);
    MFX_CHECK(task, MFX_ERR_UNDEFINED_BEHAVIOR);

    auto sts = decoder->QueryFrame(task);
    return sts;
}

// Threads complete proc callback
mfxStatus VideoDECODEMPEG2::CompleteProc(void*, void* param, mfxStatus)
{
    auto info = reinterpret_cast<TaskInfo*>(param);
    delete info;

    return MFX_ERR_NONE;
}

// Fill up resolution information if new header arrived
void VideoDECODEMPEG2::FillVideoParam(mfxVideoParamWrapper *par, bool full)
{
    if (!m_decoder)
        return;

    m_decoder->FillVideoParam(par, full);

    const auto seq = m_decoder->GetSeqAndSeqExtHdr();

    auto mfxSeq = (mfxExtCodingOptionSPSPPS*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS);
    if (!mfxSeq)
        return;

    if (seq->GetSize())
    {
        mfxSeq->SPSBufSize = (mfxU16)seq->GetSize();
        mfxSeq->SPSBuffer = seq->GetPointer();
    }
    else
    {
        mfxSeq->SPSBufSize = 0;
    }
}

// Fill up frame parameters before returning it to application
mfxStatus VideoDECODEMPEG2::FillOutputSurface(mfxFrameSurface1* surface_work, mfxFrameSurface1** surf_out, MPEG2DecoderFrame* frame) const
{
    const auto fd = frame->GetFrameData();

    *surf_out = m_surface_source->GetSurface(fd->GetFrameMID(), surface_work, &m_video_par);
    MFX_CHECK(*surf_out != nullptr, MFX_ERR_MEMORY_ALLOC);

    auto surface_out = *surf_out;

    SetFrameType(frame, surface_out);

    surface_out->Info.FrameId.ViewId = 0;
    surface_out->Info.FrameId.TemporalId = 0;

    surface_out->Info.CropX = 0;
    surface_out->Info.CropY = 0;
    surface_out->Info.CropW = (mfxU16)frame->horizontalSize;
    surface_out->Info.CropH = (mfxU16)frame->verticalSize;

    surface_out->Info.AspectRatioH = m_first_video_par.mfx.FrameInfo.AspectRatioH;
    surface_out->Info.AspectRatioW = m_first_video_par.mfx.FrameInfo.AspectRatioW;

    bool bShouldUpdate = !m_first_video_par.mfx.FrameInfo.AspectRatioH || !m_first_video_par.mfx.FrameInfo.AspectRatioW;

    surface_out->Info.AspectRatioH = bShouldUpdate ? (mfxU16)frame->aspectHeight : m_first_video_par.mfx.FrameInfo.AspectRatioH;
    surface_out->Info.AspectRatioW = bShouldUpdate ? (mfxU16)frame->aspectWidth : m_first_video_par.mfx.FrameInfo.AspectRatioW;

    bShouldUpdate = !(m_first_video_par.mfx.FrameInfo.FrameRateExtD || m_first_video_par.mfx.FrameInfo.FrameRateExtN);

    surface_out->Info.FrameRateExtD = bShouldUpdate ? m_video_par.mfx.FrameInfo.FrameRateExtD : m_first_video_par.mfx.FrameInfo.FrameRateExtD;
    surface_out->Info.FrameRateExtN = bShouldUpdate ? m_video_par.mfx.FrameInfo.FrameRateExtN : m_first_video_par.mfx.FrameInfo.FrameRateExtN;

    surface_out->Info.PicStruct = GetMFXPicStruct(*frame, m_video_par.mfx.ExtendedPicStruct);

    surface_out->Data.TimeStamp = GetMfxTimeStamp(frame->dFrameTime);
    surface_out->Data.FrameOrder = frame->displayOrder;
    surface_out->Data.DataFlag = (mfxU16)(frame->isOriginalPTS ? MFX_FRAMEDATA_ORIGINAL_TIMESTAMP : 0);

    SetTimeCode(frame, surface_out);

    auto payloads = m_decoder->GetPayloadStorage();
    if (payloads)
        payloads->SetTimestamp(frame);
    return MFX_ERR_NONE;
}

// Find a next frame ready to be output from decoder
MPEG2DecoderFrame* VideoDECODEMPEG2::GetFrameToDisplay()
{
    MPEG2DecoderFrame* frame = nullptr;
    do
    {
        frame = m_decoder->GetFrameToDisplay();
        if (!frame)
        {
            break;
        }

        m_decoder->PostProcessDisplayFrame(frame);

        if (frame->IsSkipped())
        {
            frame->SetOutputted();
            frame->SetDisplayed();
        }
    } while (frame->IsSkipped());

    if (frame)
    {
        frame->SetOutputted();
    }

    return frame;
}

mfxStatus VideoDECODEMPEG2::GetSurface(mfxFrameSurface1* & surface, mfxSurfaceHeader* import_surface)
{
    MFX_CHECK(m_surface_source, MFX_ERR_NOT_INITIALIZED);

    return m_surface_source->GetSurface(surface, import_surface);
}

#endif
