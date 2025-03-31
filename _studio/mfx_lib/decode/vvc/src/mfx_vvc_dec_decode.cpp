// Copyright (c) 2022-2024 Intel Corporation
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

#if defined(MFX_ENABLE_VVC_VIDEO_DECODE)

#include "mfx_session.h"
#include "mfx_vvc_dec_decode.h"
#include "mfx_task.h"
#include "mfx_common_int.h"
#include "mfx_common_decode_int.h"
#include "mfx_umc_alloc_wrapper.h"
#include "umc_vvc_dec_defs.h"
#include "umc_vvc_frame.h"

#include "libmfx_core_hw.h"
#include "libmfx_core_interface.h"

#include "umc_va_base.h"

#if defined(MFX_ENABLE_PXP)
#include "umc_va_protected.h"
#include "mfx_pxp_video_accelerator.h"
#include "mfx_pxp_vvc_supplier.h"
#endif // MFX_ENABLE_PXP

#include <algorithm>

VideoDECODEVVC::VideoDECODEVVC(VideoCORE *core, mfxStatus *sts)
    : m_core(core)
    , m_request()
    , m_response()
    , m_response_alien()
    , m_stat()
    , m_isInit(false)
    , m_isFirstRun(true)
    , m_useDelayedDisplay(false)
    , m_frameOrder((mfxU16)MFX_FRAMEORDER_UNKNOWN)
    , m_is_cscInUse(false)
{
    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

VideoDECODEVVC::~VideoDECODEVVC()
{
    if (m_isInit)
    {
        Close();
    }
}

mfxStatus VideoDECODEVVC::Init(mfxVideoParam *par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEVVC::Init");
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(!m_decoder, MFX_ERR_UNDEFINED_BEHAVIOR);

    std::lock_guard<std::mutex> guard(m_guard);

    eMFXPlatform platform = MFX_PLATFORM_HARDWARE;
    eMFXHWType type = m_core->GetHWType();

    MFX_CHECK(CheckVideoParamDecoders(par, type) >= MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(UMC_VVC_DECODER::MFX_Utility::CheckVideoParam_VVC(par), MFX_ERR_INVALID_VIDEO_PARAM);

    m_initPar = (mfxVideoParamWrapper)(*par);
    m_firstPar = m_initPar;
    m_videoPar = m_firstPar;

    m_request = {};
    m_response = {};
    m_response_alien = {};

    bool internal = par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    if (IsD3D9Simulation(*m_core))
    {
        internal = true;
    }

    mfxStatus sts = QueryIOSurfInternal(par, &m_request);
    MFX_CHECK_STS(sts);

    if (internal)
        m_request.Type |= MFX_MEMTYPE_INTERNAL_FRAME;
    else
        m_request.Type |= MFX_MEMTYPE_EXTERNAL_FRAME;

    mfxFrameAllocRequest request_internal = m_request;

    if (!internal)
    {
        m_request.AllocId = par->AllocId;
    }

    MFX_CHECK_STS(sts);

    try
    {
        m_surface_source.reset(new SurfaceSource(m_core, *par, platform, m_request, request_internal, m_response, m_response_alien));
    }
    catch (const std::system_error& ex)
    {
        MFX_CHECK_STS(mfxStatus(ex.code().value()));
    }

    UMC_VVC_DECODER::VVCDecoderParams vp = {};
    vp.allocator = m_surface_source.get();
    vp.async_depth = par->AsyncDepth;
    if (!vp.async_depth)
        vp.async_depth = MFX_AUTO_ASYNC_DEPTH_VALUE;
    vp.io_pattern = par->IOPattern;

    sts = m_core->CreateVA(par, &m_request, &m_response, m_surface_source.get());
    MFX_CHECK_STS(sts);

    m_core->GetVA((mfxHDL*)&vp.pVideoAccelerator, MFX_MEMTYPE_FROM_DECODE);

#if defined(MFX_ENABLE_PXP)
    if (vp.pVideoAccelerator->GetProtectedVA())
        m_decoder.reset(new UMC_VVC_DECODER::PXPVVCSupplier());
    else
#endif // MFX_ENABLE_PXP
        m_decoder.reset(new UMC_VVC_DECODER::VVCDecoderVA());

    ConvertMFXParamsToUMC(par, &vp);
    vp.info.profile = par->mfx.CodecProfile;
    vp.info.bitrate = CalculateAsyncDepth(par);

    UMC::Status umcSts = m_decoder->Init(&vp);
    MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_NOT_INITIALIZED);

    m_decoder->SetVideoParams(m_firstPar);

    m_isFirstRun = true;
    m_isInit = true;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVVC::QueryImplsDescription(
    VideoCORE& core,
    mfxDecoderDescription::decoder& caps,
    mfx::PODArraysHolder& ah)
{
    const mfxU16 SupportedProfiles[] =
    {
        MFX_PROFILE_VVC_MAIN10
    };
    const mfxResourceType SupportedMemTypes[] =
    {
        MFX_RESOURCE_SYSTEM_SURFACE
        , MFX_RESOURCE_VA_SURFACE
    };
    const mfxU32 SupportedFourCCChromaFormat[][2] =
    {
        { MFX_FOURCC_NV12, MFX_CHROMAFORMAT_YUV420 }
        , { MFX_FOURCC_P010, MFX_CHROMAFORMAT_YUV420 }
    };

    caps.CodecID = MFX_CODEC_VVC;
    caps.MaxcodecLevel = MFX_LEVEL_VVC_155;

    mfxVideoParam par;
    memset(&par, 0, sizeof(par));
    par.mfx.CodecId = MFX_CODEC_VVC;
    par.mfx.CodecLevel = caps.MaxcodecLevel;

    mfxStatus sts = MFX_ERR_NONE;
    for (mfxU16 profile : SupportedProfiles)
    {
        par.mfx.CodecProfile = profile;
        par.mfx.FrameInfo.ChromaFormat = 0;
        par.mfx.FrameInfo.FourCC = 0;

        sts = VideoDECODEVVC::Query(&core, &par, &par);
        if (sts != MFX_ERR_NONE) continue;

        auto& pfCaps = ah.PushBack(caps.Profiles);
        pfCaps.Profile = profile;

        for (auto memType : SupportedMemTypes)
        {
            auto& memCaps = ah.PushBack(pfCaps.MemDesc);
            memCaps.MemHandleType = memType;
            memCaps.Width = { 16, 16384, 16 };
            memCaps.Height = { 16, 16384, 16 };

            for (auto fccChroma : SupportedFourCCChromaFormat)
            {
                par.mfx.FrameInfo.FourCC = fccChroma[0];
                par.mfx.FrameInfo.ChromaFormat = mfxU16(fccChroma[1]);

                sts = VideoDECODEVVC::Query(&core, &par, &par);
                if (sts != MFX_ERR_NONE) continue;

                ah.PushBack(memCaps.ColorFormats) = fccChroma[0];
                ++memCaps.NumColorFormats;
            }
            ++pfCaps.NumMemTypes;
        }
        ++caps.NumProfiles;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVVC::Reset(mfxVideoParam *par)
{

    std::lock_guard<std::mutex> guard(m_guard);

    TRACE_EVENT(MFX_TRACE_API_DECODE_RESET_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(par ? par->mfx.FrameInfo.Width : 0,
        par ? par->mfx.FrameInfo.Height : 0, par ? par->mfx.CodecId : 0));

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    m_decoder->Reset();
    m_decoder->SetVideoParams(*par);

    TRACE_EVENT(MFX_TRACE_API_DECODE_RESET_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(MFX_ERR_NONE));

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVVC::Close()
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEVVC::Close");
    std::lock_guard<std::mutex> guard(m_guard);

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    m_decoder->Close();
    m_surface_source->Close();

    m_request = {};
    m_response = {};
    m_response_alien = {};

    m_isInit = false;

    return MFX_ERR_NONE;
}

mfxTaskThreadingPolicy VideoDECODEVVC::GetThreadingPolicy(void)
{
    return MFX_TASK_THREADING_SHARED;
}

mfxStatus VideoDECODEVVC::Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);

    eMFXPlatform platform = UMC_VVC_DECODER::MFX_Utility::GetPlatform_VVC(core, in);

    if (platform == MFX_PLATFORM_SOFTWARE)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return UMC_VVC_DECODER::MFX_Utility::Query_VVC(core, in, out);
}

// Actually calculate needed frames number
mfxStatus VideoDECODEVVC::QueryIOSurfInternal(mfxVideoParam* par, mfxFrameAllocRequest* request)
{
    request->Info = par->mfx.FrameInfo;

    mfxU32 asyncDepth = CalculateAsyncDepth(par);

    mfxI32 dpbSize = UMC_VVC_DECODER::VVC_MAX_NUM_REF;

    if (par->mfx.MaxDecFrameBuffering && par->mfx.MaxDecFrameBuffering < dpbSize)
        dpbSize = par->mfx.MaxDecFrameBuffering;

    mfxU32 numMin = dpbSize + 1 + asyncDepth;

    if (par->mfx.CodecLevel == MFX_LEVEL_VVC_155)
        numMin = (mfxU16)std::max(2 + asyncDepth, (mfxU32)3);

    request->NumFrameMin = (mfxU16)numMin;

    request->NumFrameSuggested = request->NumFrameMin;

    request->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVVC::QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
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

    bool isInternalManaging = par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    bool IsD3D9SimWithVideoMem = IsD3D9Simulation(*core) && (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY);
    if (IsD3D9SimWithVideoMem)
        isInternalManaging = true;

    mfxStatus sts = QueryIOSurfInternal(par, request);
    MFX_CHECK_STS(sts);

    sts = UpdateCscOutputFormat(par, request);
    MFX_CHECK_STS(sts);

    if (isInternalManaging)
    {
        request->NumFrameSuggested = request->NumFrameMin = (mfxU16)CalculateAsyncDepth(par);

        if (!IsD3D9SimWithVideoMem)
        {
            request->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
        }
    }

    request->Type |= MFX_MEMTYPE_EXTERNAL_FRAME;

    return MFX_ERR_NONE;
}

inline UMC::Status FillParam(mfxVideoParam* par)
{
    if (par->mfx.FrameInfo.FourCC == MFX_FOURCC_P010
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y210
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_P016
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y216
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y416)
        par->mfx.FrameInfo.Shift = 1;

    return UMC::UMC_OK;
}

mfxStatus VideoDECODEVVC::DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par)
{
    MFX_CHECK(core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_NULL_PTR3(bs, bs->Data, par);

    mfxStatus sts = CheckBitstream(bs);
    MFX_CHECK_STS(sts);

    try
    {
        UMC::Status umcRes = UMC_VVC_DECODER::VVCDecoder::DecodeHeader(bs, par);
        MFX_CHECK(umcRes != UMC::UMC_ERR_NOT_ENOUGH_DATA, MFX_ERR_MORE_DATA);
        MFX_CHECK(umcRes == UMC::UMC_OK, ConvertStatusUmc2Mfx(umcRes));

        umcRes = FillParam(par);
        MFX_CHECK(umcRes == UMC::UMC_OK, ConvertUMCStatusToMfx(umcRes));
    }
    catch (UMC_VVC_DECODER::vvc_exception & ex)
    {
        return ConvertStatusUmc2Mfx(ex.GetStatus());
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVVC::GetVideoParam(mfxVideoParam *par)
{
    TRACE_EVENT(MFX_TRACE_API_DECODE_GETVIDEOPARAM_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(par ? par->mfx.FrameInfo.Width : 0,
        par ? par->mfx.FrameInfo.Height : 0, par ? par->mfx.CodecId : 0));

    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    std::lock_guard<std::mutex> guard(m_guard);

    TRACE_EVENT(MFX_TRACE_API_DECODE_GETVIDEOPARAM_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(MFX_ERR_NONE));

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVVC::GetDecodeStat(mfxDecodeStat *stat)
{
    TRACE_EVENT(MFX_TRACE_API_DECODE_GETSTAT_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(0));

    MFX_CHECK_NULL_PTR1(stat);

    TRACE_EVENT(MFX_TRACE_API_DECODE_GETSTAT_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(stat ? stat->NumFrame : 0));

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVVC::DecodeFrame(mfxFrameSurface1 *surface_out, VVCDecoderFrame *frame)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoDECODEVVC::DecodeFrame");
    MFX_CHECK_NULL_PTR1(surface_out);

    mfxI32 index;
    if (frame)
    {
        index = frame->GetFrameData()->GetFrameMID();
    }
    else
    {
        index = m_surface_source->FindSurface(surface_out);
        frame = m_decoder->FindFrameByMemID((UMC::FrameMemID)index);
        MFX_CHECK(frame, MFX_ERR_NOT_FOUND);
    }

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

    mfxStatus sts = m_surface_source->PrepareToOutput(surface_out, index, &m_videoPar);

    frame->SetDisplayed();

    TRACE_EVENT(MFX_TRACE_API_HEVC_DISPLAYINFO_TASK, EVENT_TYPE_INFO, TR_KEY_DECODE_BASIC_INFO, make_event_data(
        frame->m_PicOrderCnt, (uint32_t)frame->IsDisplayed(), (uint32_t)frame->IsOutputted()));

    return sts;
}

mfxStatus VideoDECODEVVC::QueryFrame(mfxThreadTask task)
{
    MFX_CHECK_NULL_PTR1(task);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    auto info =
        reinterpret_cast<ThreadTaskInfoVVC*>(task);
    UMC_VVC_DECODER::VVCDecoderFrame* frame = NULL;
    mfxFrameSurface1* surface_out = info->surface_out;
    MFX_CHECK_NULL_PTR1(surface_out);
    UMC::FrameMemID id = m_surface_source->FindSurface(surface_out);
    frame = m_decoder->FindFrameByMemID(id);
    MFX_CHECK(frame, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(frame->IsDecodingStarted(), MFX_ERR_UNDEFINED_BEHAVIOR);
    if (!frame->IsDecodingCompleted())
    {
        m_decoder->QueryFrames();
    }

    MFX_CHECK(frame->IsDecodingCompleted(), MFX_TASK_WORKING);
    mfxStatus sts = DecodeFrame(surface_out, frame);

    MFX_CHECK_STS(sts);
    return MFX_TASK_DONE;
}

mfxStatus VideoDECODEVVC::DecodeRoutine(void* state, void* param, mfxU32, mfxU32)
{
    auto decoder = reinterpret_cast<VideoDECODEVVC*>(state);
    MFX_CHECK(decoder, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxThreadTask task =
        reinterpret_cast<ThreadTaskInfoVVC*>(param);

    MFX_CHECK(task, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxStatus sts = decoder->QueryFrame(task);
    MFX_CHECK_STS(sts);
    return sts;
}

mfxStatus VideoDECODEVVC::CompleteProc(void*, void* param, mfxStatus)
{
    auto info =
        reinterpret_cast<ThreadTaskInfoVVC*>(param);
    delete info;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVVC::CheckFrameInfo(mfxFrameInfo& info)
{
    MFX_SAFE_CALL(CheckFrameInfoCommon(&info, MFX_CODEC_VVC));

    switch (info.FourCC)
    {
    case MFX_FOURCC_NV12:
        break;
    case MFX_FOURCC_P010:
        MFX_CHECK(info.Shift == 1, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        break;
    default:
        MFX_CHECK_STS(MFX_ERR_INVALID_VIDEO_PARAM);
    }

    switch (info.ChromaFormat)
    {
    case MFX_CHROMAFORMAT_YUV420:
        break;
    default:
        MFX_CHECK_STS(MFX_ERR_INVALID_VIDEO_PARAM);
    }

    return MFX_ERR_NONE;
}

// Fill up frame parameters before returning it to application
void VideoDECODEVVC::FillOutputSurface(mfxFrameSurface1** surf_out, mfxFrameSurface1* surface_work, UMC_VVC_DECODER::VVCDecoderFrame* pFrame)
{
    m_stat.NumFrame++;
    m_stat.NumError += pFrame->GetError() ? 1 : 0;
    const UMC::FrameData* fd = pFrame->GetFrameData();

    *surf_out = m_surface_source->GetSurface(fd->GetFrameMID(), surface_work, &m_videoPar);
    assert(*surf_out);

    mfxFrameSurface1* surface_out = *surf_out;

    surface_out->Info.FrameId.TemporalId = 0;

    surface_out->Info.CropH = (mfxU16)(pFrame->m_lumaSize.height - pFrame->m_cropBottom - pFrame->m_cropTop);
    surface_out->Info.CropW = (mfxU16)(pFrame->m_lumaSize.width - pFrame->m_cropRight - pFrame->m_cropLeft);
    surface_out->Info.CropX = (mfxU16)(pFrame->m_cropLeft);
    surface_out->Info.CropY = (mfxU16)(pFrame->m_cropTop);

    surface_out->Data.FrameOrder = (mfxU16)pFrame->m_frameOrder;

    switch (pFrame->m_chromaFormat)
    {
    case MFX_CHROMAFORMAT_YUV400:
        surface_out->Info.ChromaFormat = MFX_CHROMAFORMAT_YUV400;
        break;
    case MFX_CHROMAFORMAT_YUV420:
        surface_out->Info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        break;
    case MFX_CHROMAFORMAT_YUV422:
        surface_out->Info.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        break;
    case MFX_CHROMAFORMAT_YUV444:
        surface_out->Info.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        break;
    default:
        assert(!"Unknown chroma format");
        surface_out->Info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    }

    bool isShouldUpdate = !(m_firstPar.mfx.FrameInfo.AspectRatioH || m_firstPar.mfx.FrameInfo.AspectRatioW);

    surface_out->Info.AspectRatioH = isShouldUpdate ? (mfxU16)pFrame->m_aspectHeight : m_firstPar.mfx.FrameInfo.AspectRatioH;
    surface_out->Info.AspectRatioW = isShouldUpdate ? (mfxU16)pFrame->m_aspectWidth : m_firstPar.mfx.FrameInfo.AspectRatioW;

    isShouldUpdate = !(m_firstPar.mfx.FrameInfo.FrameRateExtD || m_firstPar.mfx.FrameInfo.FrameRateExtN);

    surface_out->Info.FrameRateExtD = isShouldUpdate ? m_videoPar.mfx.FrameInfo.FrameRateExtD : m_firstPar.mfx.FrameInfo.FrameRateExtD;
    surface_out->Info.FrameRateExtN = isShouldUpdate ? m_videoPar.mfx.FrameInfo.FrameRateExtN : m_firstPar.mfx.FrameInfo.FrameRateExtN;

    // field currently not support, return unknown
    surface_out->Info.PicStruct = (m_videoPar.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? m_videoPar.mfx.FrameInfo.PicStruct: MFX_PICSTRUCT_UNKNOWN;

    surface_out->Data.TimeStamp = GetMfxTimeStamp(pFrame->m_dFrameTime);

    surface_out->Data.DataFlag = (mfxU16)(pFrame->m_isOriginalPTS ? MFX_FRAMEDATA_ORIGINAL_TIMESTAMP : 0);

    mfxExtDecodedFrameInfo* info = (mfxExtDecodedFrameInfo*)GetExtendedBuffer(surface_out->Data.ExtParam, surface_out->Data.NumExtParam, MFX_EXTBUFF_DECODED_FRAME_INFO);
    if (info)
    {
        switch (pFrame->m_FrameType)
        {
        case UMC::I_PICTURE:
            info->FrameType = MFX_FRAMETYPE_I;
            if (pFrame->GetAU()->m_IsIDR)
                info->FrameType |= MFX_FRAMETYPE_IDR;

            break;

        case UMC::P_PICTURE:
            info->FrameType = MFX_FRAMETYPE_P;
            break;

        case UMC::B_PICTURE:
            info->FrameType = MFX_FRAMETYPE_B;
            break;

        default:
            assert(!"Unknown frame type");
            info->FrameType = MFX_FRAMETYPE_UNKNOWN;
        }

        if (pFrame->m_isUsedAsReference)
            info->FrameType |= MFX_FRAMETYPE_REF;
    }
}

mfxStatus VideoDECODEVVC::SubmitFrame(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out)
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

    mfxStatus sts = MFX_ERR_NONE;

    if (surface_work)
    {
        bool workSfsIsEmpty = IsSurfaceEmpty(*surface_work);

        MFX_CHECK(!workSfsIsEmpty, MFX_ERR_LOCK_MEMORY);

        if (m_is_cscInUse != true)
        {
            sts = CheckFrameInfo(surface_work->Info);
            MFX_CHECK_STS(sts);
        }

        sts = CheckFrameData(surface_work);
        MFX_CHECK_STS(sts);
    }

#if defined(MFX_ENABLE_PXP)
    //Check protect VA is enabled or not
    auto va = (dynamic_cast<UMC_VVC_DECODER::VVCDecoderVA*>(m_decoder.get()))->GetVA();
    if (bs && va && va->GetProtectedVA())
    {
        MFX_CHECK((bs->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME), MFX_ERR_UNSUPPORTED);
        va->GetProtectedVA()->SetBitstream(bs);
    }
#endif // MFX_ENABLE_PXP

    if (bs)
    {
        sts = CheckBitstream(bs);
        MFX_CHECK_STS(sts);
    }

    try
    {
        bool force = false;
        MFXMediaDataAdapter src(bs);

        for (;;)
        {
            sts = m_surface_source->SetCurrentMFXSurface(surface_work);
            MFX_CHECK_STS(sts);

            UMC::Status umcRes = m_surface_source->HasFreeSurface() ?
                m_decoder->AddSource(bs ? &src : 0) : UMC::UMC_ERR_NEED_FORCE_OUTPUT;

            UMC::Status umcFrameRes = umcRes;

            if (umcRes == UMC::UMC_NTF_NEW_RESOLUTION ||
                umcRes == UMC::UMC_WRN_REPOSITION_INPROGRESS ||
                umcRes == UMC::UMC_ERR_UNSUPPORTED)
            {
                m_decoder->FillVideoParam(&m_videoPar);
                FillParam(&m_videoPar);
            }

            if (umcRes == UMC::UMC_WRN_REPOSITION_INPROGRESS)
            {
                if (!m_isFirstRun)
                {
                    sts = MFX_WRN_VIDEO_PARAM_CHANGED;
                }
                else
                {
                    umcFrameRes = umcRes = UMC::UMC_OK;
                    m_isFirstRun = false;
                }
            }

            if (umcRes == UMC::UMC_ERR_INVALID_STREAM)
            {
                umcFrameRes = umcRes = UMC::UMC_OK;
            }

            if (umcRes == UMC::UMC_NTF_NEW_RESOLUTION)
            {
                sts = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
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
                if (!bs || bs->DataFlag == MFX_BITSTREAM_EOS)
                    force = true;
                sts = MFX_ERR_DEVICE_FAILED;
                break;

            case UMC::UMC_ERR_GPU_HANG:
                if (!bs || bs->DataFlag == MFX_BITSTREAM_EOS)
                    force = true;
                sts = MFX_ERR_GPU_HANG;
                break;

            case UMC::UMC_ERR_NOT_ENOUGH_BUFFER:
            case UMC::UMC_WRN_INFO_NOT_READY:
            case UMC::UMC_ERR_NEED_FORCE_OUTPUT:
                force = (umcRes == UMC::UMC_ERR_NEED_FORCE_OUTPUT);
                sts = umcRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER ?
                    (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK : MFX_WRN_DEVICE_BUSY;
                break;

            case UMC::UMC_ERR_NOT_ENOUGH_DATA:
            case UMC::UMC_ERR_SYNC:
                if (!bs || bs->DataFlag == MFX_BITSTREAM_EOS)
                    force = true;
                sts = MFX_ERR_MORE_DATA;
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

            if (sts == MFX_ERR_DEVICE_FAILED ||
                sts == MFX_ERR_GPU_HANG ||
                sts == MFX_ERR_UNDEFINED_BEHAVIOR)
            {
                MFX_CHECK_STS(sts);
            }
            umcRes = m_decoder->RunDecoding();

            UMC_VVC_DECODER::VVCDecoderFrame* frame = nullptr;

            frame = GetFrameToDisplay(force);

            // return frame to display
            if (frame)
            {
                FillOutputSurface(surface_out, surface_work, frame);

                return MFX_ERR_NONE;
            }

            *surface_out = 0;

            if (umcFrameRes != UMC::UMC_OK)
            {
                break;
            }
        } // for (;;)
    }
    catch (UMC_VVC_DECODER::vvc_exception const& ex)
    {
        sts = ConvertUMCStatusToMfx(ex.GetStatus());
    }
    catch (std::bad_alloc const&)
    {
        sts = MFX_ERR_MEMORY_ALLOC;
    }
    catch (...)
    {
        sts = MFX_ERR_UNKNOWN;
    }

    if (sts == MFX_ERR_MORE_DATA)
        return sts;

    MFX_CHECK_STS(sts);

    return sts;
}

mfxStatus VideoDECODEVVC::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, MFX_ENTRY_POINT *entry_point)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);
    MFX_CHECK_NULL_PTR1(entry_point);

    std::lock_guard<std::mutex> guard(m_guard);
    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = SubmitFrame(bs, surface_work, surface_out);

    if (sts == MFX_ERR_MORE_DATA || sts == MFX_ERR_MORE_SURFACE)
        return sts;

    MFX_CHECK_STS(sts);

    std::unique_ptr<ThreadTaskInfoVVC> info(new ThreadTaskInfoVVC);

    info->copyfromframe = UMC::FRAME_MID_INVALID;
    info->surface_work = surface_work;
    if (*surface_out)
        info->surface_out = *surface_out;

    if (m_decoder->GetRepeatedFrame() != UMC::FRAME_MID_INVALID) {
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

mfxStatus VideoDECODEVVC::SetSkipMode(mfxSkipMode /*mode*/)
{
    std::lock_guard<std::mutex> guard(m_guard);

    TRACE_EVENT(MFX_TRACE_API_DECODE_SETSKIPMODE_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(0));

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    TRACE_EVENT(MFX_TRACE_API_DECODE_SETSKIPMODE_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(MFX_ERR_NONE));

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVVC::GetPayload(mfxU64* /*time_stamp*/, mfxPayload* /*pPayload*/)
{
    MFX_RETURN(MFX_ERR_UNSUPPORTED);
}

mfxStatus VideoDECODEVVC::RunThread(void * params)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEVVC::RunThread");
    ThreadTaskInfoVVC* info = reinterpret_cast<ThreadTaskInfoVVC*>(params);

    MFX_CHECK_NULL_PTR1(info);
    return MFX_ERR_NONE;
}


UMC_VVC_DECODER::VVCDecoderFrame *VideoDECODEVVC::GetFrameToDisplay(bool force)
{
    (void)(force);

    assert(m_decoder);

    UMC_VVC_DECODER::VVCDecoderFrame* frame
        = m_decoder->GetFrameToDisplay(force);

    if (!frame)
        return nullptr;

    frame->SetOutputted();
    m_decoder->PostProcessDisplayFrame(frame);
    frame->ShowAsExisting(false);

    return frame;
}

mfxStatus VideoDECODEVVC::GetSurface(mfxFrameSurface1* &surface, mfxSurfaceHeader* import_surface)
{
    MFX_CHECK(m_surface_source, MFX_ERR_NOT_INITIALIZED);

    return m_surface_source->GetSurface(surface, import_surface);
}

mfxStatus VideoDECODEVVC::GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts)
{

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR3(ud, sz, ts);

    return MFX_ERR_NONE;
}

#endif // MFX_ENABLE_VVC_VIDEO_DECODE
