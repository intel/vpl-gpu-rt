// Copyright (c) 2012-2024 Intel Corporation
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

#include "mfx_h265_dec_decode.h"

#if defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_common.h"
#include "mfx_common_decode_int.h"

#include "umc_h265_mfx_supplier.h"
#include "umc_h265_mfx_utils.h"
#include "umc_h265_frame_list.h"

#include "umc_h265_va_supplier.h"
#include "umc_va_video_processing.h"

#if defined(MFX_ENABLE_PXP)
#include "mfx_pxp_video_accelerator.h"
#include "mfx_pxp_h265_supplier.h"
#endif // MFX_ENABLE_PXP

#include "libmfx_core_interface.h"

using namespace UMC_HEVC_DECODER;

#include "mfx_unified_h265d_logging.h"

inline
bool IsNeedToUseHWBuffering(eMFXHWType /*type*/)
{
    return false;
}

inline
mfxU16 UMC2MFX_PicStruct(int dps, bool extended)
{
    switch (dps)
    {
        case DPS_FRAME_H265:
            return MFX_PICSTRUCT_PROGRESSIVE;

        case DPS_TOP_H265:
            return MFX_PICSTRUCT_FIELD_TOP;
        case DPS_BOTTOM_H265:
            return MFX_PICSTRUCT_FIELD_BOTTOM;

        case DPS_TOP_BOTTOM_H265:
            return MFX_PICSTRUCT_PROGRESSIVE | (extended ? MFX_PICSTRUCT_FIELD_TFF: 0);
        case DPS_BOTTOM_TOP_H265:
            return MFX_PICSTRUCT_PROGRESSIVE | (extended ? MFX_PICSTRUCT_FIELD_BFF: 0);

        case DPS_TOP_BOTTOM_TOP_H265:
            return MFX_PICSTRUCT_PROGRESSIVE | (extended ? MFX_PICSTRUCT_FIELD_REPEATED | MFX_PICSTRUCT_FIELD_TFF: 0);
        case DPS_BOTTOM_TOP_BOTTOM_H265:
            return MFX_PICSTRUCT_PROGRESSIVE | (extended ? MFX_PICSTRUCT_FIELD_REPEATED | MFX_PICSTRUCT_FIELD_BFF: 0);

        case DPS_FRAME_DOUBLING_H265:
            return MFX_PICSTRUCT_PROGRESSIVE | (extended ? MFX_PICSTRUCT_FRAME_DOUBLING : 0);
        case DPS_FRAME_TRIPLING_H265:
            return MFX_PICSTRUCT_PROGRESSIVE | (extended ? MFX_PICSTRUCT_FRAME_TRIPLING : 0);

        case DPS_TOP_BOTTOM_PREV_H265:
            return MFX_PICSTRUCT_FIELD_TOP | (extended ? MFX_PICSTRUCT_FIELD_PAIRED_PREV : 0);
        case DPS_BOTTOM_TOP_PREV_H265:
            return MFX_PICSTRUCT_FIELD_BOTTOM | (extended ? MFX_PICSTRUCT_FIELD_PAIRED_PREV : 0);

        case DPS_TOP_BOTTOM_NEXT_H265:
            return MFX_PICSTRUCT_FIELD_TOP | (extended ? MFX_PICSTRUCT_FIELD_PAIRED_NEXT : 0);
        case DPS_BOTTOM_TOP_NEXT_H265:
            return MFX_PICSTRUCT_FIELD_BOTTOM | (extended ? MFX_PICSTRUCT_FIELD_PAIRED_NEXT : 0);

        default:
            return MFX_PICSTRUCT_UNKNOWN;
    }
}

inline
UMC::Status FillParam(MFXTaskSupplier_H265 * decoder, mfxVideoParam *par, bool full)
{
    UMC::Status umcRes = decoder->FillVideoParam(par, full);

    if (par->mfx.FrameInfo.FourCC == MFX_FOURCC_P010
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y210
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_P016
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y216
        || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y416)
        par->mfx.FrameInfo.Shift = 1;

    return umcRes;
}

struct ThreadTaskInfo265
{
    bool              is_decoding_done = false;
    mfxFrameSurface1 *surface_out = nullptr;
    mfxU32            taskID = 0; // for task ordering
    H265DecoderFrame *pFrame = nullptr;
};

enum
{
    ENABLE_DELAYED_DISPLAY_MODE = 1
};

VideoDECODEH265::VideoDECODEH265(VideoCORE *core, mfxStatus * sts)
    : VideoDECODE()
    , m_core(core)
    , m_isInit(false)
    , m_globalTask(false)
    , m_frameOrder((mfxU16)MFX_FRAMEORDER_UNKNOWN)
    , m_useDelayedDisplay(false)
    , m_va(0)
    , m_isFirstRun(true)
{
    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

VideoDECODEH265::~VideoDECODEH265(void)
{
    Close();
}

// Initialize decoder instance
mfxStatus VideoDECODEH265::Init(mfxVideoParam *par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEH265::Init");
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(!m_isInit, MFX_ERR_UNDEFINED_BEHAVIOR);

    m_globalTask = false;

    MFX_CHECK_NULL_PTR1(par);

    eMFXPlatform platform = MFX_Utility::GetPlatform_H265(m_core, par);

    MFX_CHECK(platform == MFX_PLATFORM_HARDWARE, MFX_ERR_UNSUPPORTED);

    eMFXHWType type = m_core->GetHWType();

    MFX_CHECK(CheckVideoParamDecoders(par, type) >= MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(MFX_Utility::CheckVideoParam_H265(par, type), MFX_ERR_INVALID_VIDEO_PARAM);

    m_vInitPar = *par;
    m_vFirstPar = *par;
    m_vFirstPar.mfx.NumThread = 0;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_vFirstPar);

    m_vPar = m_vFirstPar;
    m_vPar.CreateExtendedBuffer(MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    m_vPar.CreateExtendedBuffer(MFX_EXTBUFF_CODING_OPTION_SPSPPS);
    m_vPar.CreateExtendedBuffer(MFX_EXTBUFF_HEVC_PARAM);

    mfxU32 asyncDepth = CalculateAsyncDepth(par);
    m_vPar.mfx.NumThread = (mfxU16)CalculateNumThread(par);

    m_useDelayedDisplay = ENABLE_DELAYED_DISPLAY_MODE != 0 && IsNeedToUseHWBuffering(m_core->GetHWType()) && (asyncDepth != 1);

    bool useBigSurfacePoolWA = MFX_Utility::IsBugSurfacePoolApplicable(par);

    int32_t useInternal = m_vPar.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    /* There are following conditions for post processing via HW fixed function engine:
     * (1): Progressive only for ICL and Before, interlace is supported for ICL following patforms
     * (2): Supported on SKL (Core) and APL (Atom) platform and above
     * (3): Only video memory supported (so, OPAQ memory does not supported!)
     * */
    if (videoProcessing)
    {
        MFX_CHECK((m_vPar.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY),
                  MFX_ERR_UNSUPPORTED);

        bool is_fourcc_supported =
                 (  videoProcessing->Out.FourCC == MFX_FOURCC_RGB4
                 || videoProcessing->Out.FourCC == MFX_FOURCC_RGBP
                 || videoProcessing->Out.FourCC == MFX_FOURCC_NV12
                 || videoProcessing->Out.FourCC == MFX_FOURCC_P010
                 || videoProcessing->Out.FourCC == MFX_FOURCC_YUY2
                 || videoProcessing->Out.FourCC == MFX_FOURCC_AYUV
                 || videoProcessing->Out.FourCC == MFX_FOURCC_Y410
                 || videoProcessing->Out.FourCC == MFX_FOURCC_Y210
                 || videoProcessing->Out.FourCC == MFX_FOURCC_Y216
                 || videoProcessing->Out.FourCC == MFX_FOURCC_Y416
                 || videoProcessing->Out.FourCC == MFX_FOURCC_P016);
       MFX_CHECK(is_fourcc_supported,MFX_ERR_UNSUPPORTED);
        if (m_core->GetVAType() == MFX_HW_VAAPI)
            useInternal = true;
    }
#endif

    if (IsD3D9Simulation(*m_core))
        useInternal = true;

    // allocate memory
    mfxFrameAllocRequest request = {};
    mfxFrameAllocRequest request_internal;

    mfxStatus mfxSts = QueryIOSurfInternal(type, &m_vPar, &request);
    if (mfxSts != MFX_ERR_NONE)
        return mfxSts;

    if (useInternal)
        request.Type |= MFX_MEMTYPE_INTERNAL_FRAME;
    else
        request.Type |= MFX_MEMTYPE_EXTERNAL_FRAME;

    request_internal = request;

    // allocates internal surfaces:
    if (useInternal)
    {
        request = request_internal;

        if (par->mfx.FrameInfo.FourCC == MFX_FOURCC_P010
            || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y210
            || par->mfx.FrameInfo.FourCC == MFX_FOURCC_P016
            || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y216
            || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y416
            )
        {
            request.Info.Shift = request_internal.Info.Shift = 1;
        }
    }

    try
    {
        m_surface_source.reset(new SurfaceSource(m_core, *par, platform, request, request_internal, m_response, m_response_alien));
    }
    catch (const std::system_error& ex)
    {
        MFX_CHECK_STS(mfxStatus(ex.code().value()));
    }

    mfxSts = m_core->CreateVA(&m_vFirstPar, &request, &m_response, m_surface_source.get());
    MFX_CHECK(mfxSts >= MFX_ERR_NONE, mfxSts);

    UMC::Status umcSts = m_MemoryAllocator.InitMem(0, m_core);

    UMC::VideoDecoderParams umcVideoParams;
    ConvertMFXParamsToUMC(&m_vFirstPar, &umcVideoParams);
    umcVideoParams.numThreads = m_vPar.mfx.NumThread;
    umcVideoParams.info.bitrate = asyncDepth - umcVideoParams.numThreads; // buffered frames

    m_core->GetVA((mfxHDL*)&m_va, MFX_MEMTYPE_FROM_DECODE);
    umcVideoParams.pVideoAccelerator = m_va;

#if defined(MFX_ENABLE_PXP)
    if (m_va->GetProtectedVA())
        m_pH265VideoDecoder.reset(useBigSurfacePoolWA ? new VATaskSupplierBigSurfacePool<PXPH265Supplier>() : new PXPH265Supplier()); // HW
    else
#endif // MFX_ENABLE_PXP
    m_pH265VideoDecoder.reset(useBigSurfacePoolWA ? new VATaskSupplierBigSurfacePool<VATaskSupplier>() : new VATaskSupplier()); // HW

    m_pH265VideoDecoder->SetFrameAllocator(m_surface_source.get());
    static_cast<VATaskSupplier*>(m_pH265VideoDecoder.get())->SetVideoHardwareAccelerator(m_va);


#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    if (m_va->GetVideoProcessingVA())
    {
        umcSts = m_va->GetVideoProcessingVA()->Init(par, videoProcessing);
        MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_INVALID_VIDEO_PARAM);
    }
#endif
    umcVideoParams.lpMemoryAllocator = &m_MemoryAllocator;

    umcSts = m_pH265VideoDecoder->Init(&umcVideoParams);
    MFX_CHECK(umcSts == UMC::UMC_OK, ConvertUMCStatusToMfx(umcSts));

    m_isInit = true;

    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;
    m_isFirstRun = true;

    if (m_useDelayedDisplay)
    {
        static_cast<VATaskSupplier*>(m_pH265VideoDecoder.get())->SetBufferedFramesNumber(NUMBER_OF_ADDITIONAL_FRAMES);
    }

    m_pH265VideoDecoder->SetVideoParams(&m_vFirstPar);

    MFX_CHECK(platform == m_core->GetPlatformType(), MFX_ERR_UNSUPPORTED);

    MFX_CHECK(!isNeedChangeVideoParamWarning, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH265::QueryImplsDescription(
    VideoCORE& core,
    mfxDecoderDescription::decoder& caps,
    mfx::PODArraysHolder& ah)
{
    const mfxU16 SupportedProfiles[] =
    {
        MFX_PROFILE_HEVC_MAIN
        , MFX_PROFILE_HEVC_MAIN10
        , MFX_PROFILE_HEVC_MAINSP
        , MFX_PROFILE_HEVC_REXT
        , MFX_PROFILE_HEVC_SCC
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
        , { MFX_FOURCC_P016, MFX_CHROMAFORMAT_YUV420 }
        , { MFX_FOURCC_YUY2, MFX_CHROMAFORMAT_YUV422 }
        , { MFX_FOURCC_Y210, MFX_CHROMAFORMAT_YUV422 }
        , { MFX_FOURCC_Y216, MFX_CHROMAFORMAT_YUV422 }
        , { MFX_FOURCC_AYUV, MFX_CHROMAFORMAT_YUV444 }
        , { MFX_FOURCC_Y410, MFX_CHROMAFORMAT_YUV444 }
        , { MFX_FOURCC_Y416, MFX_CHROMAFORMAT_YUV444 }
    };

    caps.CodecID = MFX_CODEC_HEVC;
    caps.MaxcodecLevel = MFX_LEVEL_HEVC_62;

    mfxVideoParam par;
    memset(&par, 0, sizeof(par));
    par.mfx.CodecId = MFX_CODEC_HEVC;
    par.mfx.CodecLevel = caps.MaxcodecLevel;

    mfxStatus sts = MFX_ERR_NONE;
    for (mfxU16 profile : SupportedProfiles)
    {
        par.mfx.CodecProfile = profile;
        par.mfx.FrameInfo.ChromaFormat = 0;
        par.mfx.FrameInfo.FourCC = 0;

        sts = VideoDECODEH265::Query(&core, &par, &par);
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

                sts = VideoDECODEH265::Query(&core, &par, &par);
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

// Reset decoder with new parameters
mfxStatus VideoDECODEH265::Reset(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    m_globalTask = false;

    MFX_CHECK_NULL_PTR1(par);

    eMFXHWType type = m_core->GetHWType();

    eMFXPlatform platform = MFX_Utility::GetPlatform_H265(m_core, par);
    MFX_CHECK(platform == MFX_PLATFORM_HARDWARE, MFX_ERR_UNSUPPORTED);

    MFX_CHECK(CheckVideoParamDecoders(par, type) >= MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(MFX_Utility::CheckVideoParam_H265(par, type), MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(IsSameVideoParam(par, &m_vInitPar, type), MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    m_pH265VideoDecoder->Reset();

    UMC::Status umcSts = m_surface_source->Reset();
    MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);

    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;
    m_isFirstRun = true;

    memset(&m_stat, 0, sizeof(m_stat));
    m_vFirstPar = *par;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_vFirstPar);
    m_vPar = m_vFirstPar;
    m_vPar.CreateExtendedBuffer(MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    m_vPar.CreateExtendedBuffer(MFX_EXTBUFF_CODING_OPTION_SPSPPS);
    m_vPar.CreateExtendedBuffer(MFX_EXTBUFF_HEVC_PARAM);

    m_vPar.mfx.NumThread = (mfxU16)CalculateNumThread(par);


    m_pH265VideoDecoder->SetVideoParams(&m_vFirstPar);

    MFX_CHECK(!isNeedChangeVideoParamWarning, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

// Free decoder resources
mfxStatus VideoDECODEH265::Close(void)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEH265::Close");
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(m_isInit && m_pH265VideoDecoder.get(), MFX_ERR_NOT_INITIALIZED);


    m_pH265VideoDecoder->Close();
    m_surface_source->Close();

    m_isInit = false;
    m_isFirstRun = true;
    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;
    m_va = 0;
    memset(&m_stat, 0, sizeof(m_stat));

    return MFX_ERR_NONE;
}

// Returns decoder threading mode
mfxTaskThreadingPolicy VideoDECODEH265::GetThreadingPolicy(void)
{
    return MFX_TASK_THREADING_SHARED;
}

// MediaSDK DECODE_Query API function
mfxStatus VideoDECODEH265::Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);

    eMFXPlatform platform = MFX_Utility::GetPlatform_H265(core, in);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (platform == MFX_PLATFORM_HARDWARE)
    {
        type = core->GetHWType();
    }

    return MFX_Utility::Query_H265(core, in, out, type);
}

// MediaSDK DECODE_GetVideoParam API function
mfxStatus VideoDECODEH265::GetVideoParam(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(par);

    FillVideoParam(&m_vPar, true);

    par->mfx = m_vPar.mfx;

    par->Protected = m_vPar.Protected;
    par->IOPattern = m_vPar.IOPattern;
    par->AsyncDepth = m_vPar.AsyncDepth;


    mfxExtVideoSignalInfo * videoSignal = (mfxExtVideoSignalInfo *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    if (videoSignal)
    {
        mfxExtVideoSignalInfo * videoSignalInternal = m_vPar.GetExtendedBuffer<mfxExtVideoSignalInfo>(MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
        *videoSignal = *videoSignalInternal;
    }

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    if (videoProcessing)
    {
        mfxExtDecVideoProcessing * videoProcessingInternal = m_vPar.GetExtendedBuffer<mfxExtDecVideoProcessing>(MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
        *videoProcessing = *videoProcessingInternal;
    }
#endif

    mfxExtHEVCParam * hevcParam = (mfxExtHEVCParam *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_HEVC_PARAM);
    if (hevcParam)
    {
        mfxExtHEVCParam * hevcParamInternal = m_vPar.GetExtendedBuffer<mfxExtHEVCParam>(MFX_EXTBUFF_HEVC_PARAM);
        *hevcParam = *hevcParamInternal;
    }

    // sps/pps headers
    mfxExtCodingOptionSPSPPS * spsPps = (mfxExtCodingOptionSPSPPS *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS);
    if (spsPps)
    {
        mfxExtCodingOptionSPSPPS * spsPpsInternal = m_vPar.GetExtendedBuffer<mfxExtCodingOptionSPSPPS>(MFX_EXTBUFF_CODING_OPTION_SPSPPS);

        spsPps->SPSId = spsPpsInternal->SPSId;
        spsPps->PPSId = spsPpsInternal->PPSId;

        MFX_CHECK(spsPps->SPSBufSize >= spsPpsInternal->SPSBufSize && spsPps->PPSBufSize >= spsPpsInternal->PPSBufSize, MFX_ERR_NOT_ENOUGH_BUFFER);

        spsPps->SPSBufSize = spsPpsInternal->SPSBufSize;
        spsPps->PPSBufSize = spsPpsInternal->PPSBufSize;

        std::copy(spsPpsInternal->SPSBuffer, spsPpsInternal->SPSBuffer + spsPps->SPSBufSize, spsPps->SPSBuffer);
        std::copy(spsPpsInternal->PPSBuffer, spsPpsInternal->PPSBuffer + spsPps->PPSBufSize, spsPps->PPSBuffer);
    }

    par->mfx.FrameInfo.FrameRateExtN = m_vFirstPar.mfx.FrameInfo.FrameRateExtN;
    par->mfx.FrameInfo.FrameRateExtD = m_vFirstPar.mfx.FrameInfo.FrameRateExtD;

    if (!par->mfx.FrameInfo.FrameRateExtD && !par->mfx.FrameInfo.FrameRateExtN)
    {
        par->mfx.FrameInfo.FrameRateExtD = m_vPar.mfx.FrameInfo.FrameRateExtD;
        par->mfx.FrameInfo.FrameRateExtN = m_vPar.mfx.FrameInfo.FrameRateExtN;

        if (!par->mfx.FrameInfo.FrameRateExtD && !par->mfx.FrameInfo.FrameRateExtN)
        {
            par->mfx.FrameInfo.FrameRateExtN = 0;
            par->mfx.FrameInfo.FrameRateExtD = 0;
        }
    }

    par->mfx.FrameInfo.AspectRatioW = m_vFirstPar.mfx.FrameInfo.AspectRatioW;
    par->mfx.FrameInfo.AspectRatioH = m_vFirstPar.mfx.FrameInfo.AspectRatioH;

    if (!par->mfx.FrameInfo.AspectRatioH && !par->mfx.FrameInfo.AspectRatioW)
    {
        par->mfx.FrameInfo.AspectRatioH = m_vPar.mfx.FrameInfo.AspectRatioH;
        par->mfx.FrameInfo.AspectRatioW = m_vPar.mfx.FrameInfo.AspectRatioW;

        if (!par->mfx.FrameInfo.AspectRatioH && !par->mfx.FrameInfo.AspectRatioW)
        {
            par->mfx.FrameInfo.AspectRatioH = 1;
            par->mfx.FrameInfo.AspectRatioW = 1;
        }
    }

    return MFX_ERR_NONE;
}

// Decode bitstream header and exctract parameters from it
mfxStatus VideoDECODEH265::DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR2(bs, par);

    mfxStatus sts = CheckBitstream(bs);
    MFX_CHECK_STS(sts);

    MFXMediaDataAdapter in(bs);

    mfx_UMC_MemAllocator  tempAllocator;
    tempAllocator.InitMem(0, core);

    UMC::VideoDecoderParams avcInfo;
    avcInfo.m_pData = &in;

    MFX_AVC_Decoder_H265 decoder;

    decoder.SetMemoryAllocator(&tempAllocator);
    UMC::Status umcRes = MFX_Utility::DecodeHeader(&decoder, &avcInfo, bs, par);

    MFX_CHECK(umcRes != UMC::UMC_ERR_NOT_ENOUGH_DATA, MFX_ERR_MORE_DATA);
    MFX_CHECK(umcRes == UMC::UMC_OK, ConvertUMCStatusToMfx(umcRes));

    umcRes = FillParam(&decoder, par, false);
    MFX_CHECK(umcRes == UMC::UMC_OK, ConvertUMCStatusToMfx(umcRes));

    mfxExtCodingOptionVPS * extVps = (mfxExtCodingOptionVPS *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_VPS);
    if (extVps)
    {
        RawHeader_H265 *vps = decoder.GetVPS();

        if (vps->GetSize())
        {
            MFX_CHECK(extVps->VPSBufSize >= vps->GetSize(), MFX_ERR_NOT_ENOUGH_BUFFER);
            extVps->VPSBufSize = (mfxU16)vps->GetSize();
            std::copy(vps->GetPointer(), vps->GetPointer() + extVps->VPSBufSize, extVps->VPSBuffer);
        }
        else
        {
            extVps->VPSBufSize = 0;
        }
    }

    mfxExtCodingOptionSPSPPS * spsPps = (mfxExtCodingOptionSPSPPS *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS);
    if (spsPps)
    {
        RawHeader_H265 *sps = decoder.GetSPS();
        RawHeader_H265 *pps = decoder.GetPPS();

        if (sps->GetSize())
        {
            MFX_CHECK(spsPps->SPSBufSize >= sps->GetSize(), MFX_ERR_NOT_ENOUGH_BUFFER);
            spsPps->SPSBufSize = (mfxU16)sps->GetSize();
            std::copy(sps->GetPointer(), sps->GetPointer() + spsPps->SPSBufSize, spsPps->SPSBuffer);
        }
        else
        {
            spsPps->SPSBufSize = 0;
        }

        if (pps->GetSize())
        {
            MFX_CHECK(spsPps->PPSBufSize >= pps->GetSize(), MFX_ERR_NOT_ENOUGH_BUFFER);
            spsPps->PPSBufSize = (mfxU16)pps->GetSize();
            std::copy(pps->GetPointer(), pps->GetPointer() + spsPps->PPSBufSize, spsPps->PPSBuffer);
        }
        else
        {
            spsPps->PPSBufSize = 0;
        }
    }

    return MFX_ERR_NONE;
}

// MediaSDK DECODE_QueryIOSurf API function
mfxStatus VideoDECODEH265::QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEH265::QueryIOSurf");
    MFX_CHECK_NULL_PTR2(par, request);

    eMFXPlatform platform = MFX_Utility::GetPlatform_H265(core, par);

    MFX_CHECK(platform == MFX_PLATFORM_HARDWARE, MFX_ERR_UNSUPPORTED);
    eMFXHWType type = core->GetHWType();

    mfxVideoParam params;
    params = *par;
    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&params);

    auto const supportedMemoryType =
           (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        || (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    MFX_CHECK(supportedMemoryType, MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(!(
        par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY &&
        par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY),
        MFX_ERR_INVALID_VIDEO_PARAM);

    bool isInternalManaging = params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    bool IsD3D9SimWithVideoMem = IsD3D9Simulation(*core) && (params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY);
    if (IsD3D9SimWithVideoMem)
        isInternalManaging = true;

    mfxStatus sts = QueryIOSurfInternal(type, &params, request);
    MFX_CHECK_STS(sts);

    sts = UpdateCscOutputFormat(&params, request);
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
#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing* videoProcessing = (mfxExtDecVideoProcessing*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    if (videoProcessing)
    {
        // need to substitute output format
        // number of surfaces is same
        request->Info.FourCC = videoProcessing->Out.FourCC;
        request->Info.ChromaFormat = videoProcessing->Out.ChromaFormat;
        sts = UpdateCscOutputFormat(par, request);
        MFX_CHECK_STS(sts);

        request->Info.Width = videoProcessing->Out.Width;
        request->Info.Height = videoProcessing->Out.Height;
        request->Info.CropX = videoProcessing->Out.CropX;
        request->Info.CropY = videoProcessing->Out.CropY;
        request->Info.CropW = videoProcessing->Out.CropW;
        request->Info.CropH = videoProcessing->Out.CropH;
    }
#endif
    MFX_CHECK(platform == core->GetPlatformType(), MFX_ERR_UNSUPPORTED);

    MFX_CHECK(!isNeedChangeVideoParamWarning, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

// Actually calculate needed frames number
mfxStatus VideoDECODEH265::QueryIOSurfInternal(eMFXHWType type, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoDECODEH265::QueryIOSurfInternal");
    request->Info = par->mfx.FrameInfo;

    mfxU32 asyncDepth = CalculateAsyncDepth(par);
    bool useDelayedDisplay = (ENABLE_DELAYED_DISPLAY_MODE != 0) && IsNeedToUseHWBuffering(type) && (asyncDepth != 1);

    mfxExtHEVCParam * hevcParam = (mfxExtHEVCParam *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_HEVC_PARAM);

    if (hevcParam && (!hevcParam->PicWidthInLumaSamples || !hevcParam->PicHeightInLumaSamples)) //  not initialized
        hevcParam = 0;

    mfxI32 dpbSize = 0;

    uint32_t level_idc = par->mfx.CodecLevel;
    if (hevcParam)
        dpbSize = CalculateDPBSize(par->mfx.CodecProfile, level_idc, hevcParam->PicWidthInLumaSamples, hevcParam->PicHeightInLumaSamples, par->mfx.MaxDecFrameBuffering);
    else
        dpbSize = CalculateDPBSize(par->mfx.CodecProfile, level_idc, par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height, par->mfx.MaxDecFrameBuffering) + 1; //1 extra for avoid aligned size issue

    if (par->mfx.MaxDecFrameBuffering && par->mfx.MaxDecFrameBuffering < dpbSize)
        dpbSize = par->mfx.MaxDecFrameBuffering;

    mfxU32 numMin = dpbSize + 1 + asyncDepth;
    if (useDelayedDisplay) // equals if (m_useDelayedDisplay)
        numMin += NUMBER_OF_ADDITIONAL_FRAMES;
    request->NumFrameMin = (mfxU16)numMin;

    request->NumFrameSuggested = request->NumFrameMin;

    request->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;

    return MFX_ERR_NONE;
}

// MediaSDK DECODE_GetDecodeStat API function
mfxStatus VideoDECODEH265::GetDecodeStat(mfxDecodeStat *stat)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(stat);

    m_stat.NumSkippedFrame = m_pH265VideoDecoder->GetSkipInfo().numberOfSkippedFrames;
    m_stat.NumCachedFrame = 0;

    H265DBPList *dpb = m_pH265VideoDecoder->GetDPBList();
    MFX_CHECK(dpb, MFX_ERR_UNDEFINED_BEHAVIOR);

    H265DecoderFrame *pFrame = dpb->head();
    for (; pFrame; pFrame = pFrame->future())
    {
        if (!pFrame->wasOutputted() && !isAlmostDisposable(pFrame))
            m_stat.NumCachedFrame++;
    }

    *stat = m_stat;
    return MFX_ERR_NONE;
}

// Decoder threads entry point
static mfxStatus HEVCDECODERoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "HEVCDECODERoutine");
    mfxStatus sts = MFX_ERR_NONE;

    try
    {
        auto decoder = reinterpret_cast<VideoDECODEH265*>(pState);
        MFX_CHECK(decoder, MFX_ERR_UNDEFINED_BEHAVIOR);

        sts = decoder->RunThread(pParam, threadNumber);
        MFX_CHECK_STS(sts);
    }
    catch(...)
    {
        MFX_LTRACE_MSG_1(MFX_TRACE_LEVEL_INTERNAL, "exception handled");
        return MFX_ERR_NONE;
    }
    return sts;
}

// Threads complete proc callback
static mfxStatus HEVCCompleteProc(void *, void *pParam, mfxStatus )
{
    delete reinterpret_cast<ThreadTaskInfo265*>(pParam);
    return MFX_ERR_NONE;
}

// Decoder instance threads entry point. Do async tasks here
mfxStatus VideoDECODEH265::RunThread(void * params, mfxU32 threadNumber)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEH265::RunThread");
    ThreadTaskInfo265* info = reinterpret_cast<ThreadTaskInfo265*>(params);

    MFX_CHECK_NULL_PTR1(info);

    mfxStatus sts = MFX_TASK_WORKING;

    if (info->is_decoding_done)
    {
        return MFX_TASK_DONE;
    }

    if (!info->surface_out)
    {
        for (int32_t i = 0; i < 2 && sts == MFX_TASK_WORKING; i++)
        {
            sts = m_pH265VideoDecoder->RunThread(threadNumber);
        }

        UMC::AutomaticUMCMutex guard(m_mGuardRunThread);

        if (sts == MFX_TASK_BUSY && !m_pH265VideoDecoder->GetTaskBroker()->IsEnoughForStartDecoding(true))
            m_globalTask = false;

        return
            m_globalTask ? sts : MFX_TASK_DONE;
    }

    bool isDecoded;
    {
        UMC::AutomaticUMCMutex guard(m_mGuardRunThread);

        if (info->is_decoding_done)
            return MFX_TASK_DONE;

        isDecoded = m_pH265VideoDecoder->CheckDecoding(true, info->pFrame);
    }

    if (!isDecoded)
    {
        sts = m_pH265VideoDecoder->RunThread(threadNumber);
    }

    {
        UMC::AutomaticUMCMutex guard(m_mGuardRunThread);
        if (info->is_decoding_done)
            return MFX_TASK_DONE;

        isDecoded = m_pH265VideoDecoder->CheckDecoding(true, info->pFrame);
        if (isDecoded)
        {
            info->is_decoding_done = true;
        }
    }

    if (isDecoded)
    {
        if (!info->pFrame->wasDisplayed() && info->surface_out)
        {
            mfxStatus status = DecodeFrame(info->surface_out, info->pFrame);
            if (status != MFX_ERR_NONE && status != MFX_ERR_NOT_FOUND)
                return status;
        }

        return MFX_TASK_DONE;
    }

    return sts;
}

// Initialize threads callbacks
mfxStatus VideoDECODEH265::DecodeFrameCheck(mfxBitstream *bs,
                                              mfxFrameSurface1 *surface_work,
                                              mfxFrameSurface1 **surface_out,
                                              MFX_ENTRY_POINT *pEntryPoint)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);
    mfxStatus mfxSts = DecodeFrameCheck(bs, surface_work, surface_out);

    if (MFX_ERR_NONE == mfxSts || (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxSts) // It can be useful to run threads right after first frame receive
    {
        H265DecoderFrame *frame = nullptr;
        if (*surface_out)
        {
            mfxI32 index = m_surface_source->FindSurface(*surface_out);
            frame = m_pH265VideoDecoder->FindSurface((UMC::FrameMemID)index);
        }
        else
        {
            UMC::AutomaticUMCMutex mGuard(m_mGuardRunThread);

            H265DBPList *dpb = m_pH265VideoDecoder->GetDPBList();
            MFX_CHECK(dpb, MFX_ERR_UNDEFINED_BEHAVIOR);

            H265DecoderFrame *pFrame = dpb->head();
            for (; pFrame; pFrame = pFrame->future())
            {
                if (!pFrame->m_pic_output && !pFrame->IsDecoded())
                {
                    frame = pFrame;
                    break;
                }
            }

            if (!frame)
            {
                MFX_CHECK(m_pH265VideoDecoder->GetTaskBroker()->IsEnoughForStartDecoding(true) && !m_globalTask, MFX_WRN_DEVICE_BUSY);
                m_globalTask = true;
            }
        }

        ThreadTaskInfo265* info = new ThreadTaskInfo265();

        if (*surface_out)
            info->surface_out = *surface_out;

        info->pFrame = frame;
        pEntryPoint->pRoutine           = &HEVCDECODERoutine;
        pEntryPoint->pCompleteProc      = &HEVCCompleteProc;
        pEntryPoint->pState             = this;
        pEntryPoint->requiredNumThreads = m_vPar.mfx.NumThread;
        pEntryPoint->pParam             = info;

        return mfxSts;
    }

    return mfxSts;
}

// Check if there is enough data to start decoding in async mode
mfxStatus VideoDECODEH265::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEH265::DecodeFrameCheck");

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

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

    if (bs)
    {
       sts = CheckBitstream(bs);
       MFX_CHECK_STS(sts);
    }

    UMC::Status umcRes = UMC::UMC_OK;

    *surface_out = nullptr;

    if (surface_work)
    {
        bool isVideoProcCscEnabled = false;
#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
        mfxExtDecVideoProcessing* videoProcessing = (mfxExtDecVideoProcessing*)GetExtendedBuffer(m_vInitPar.ExtParam, m_vInitPar.NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
        if (videoProcessing && videoProcessing->Out.FourCC != m_vPar.mfx.FrameInfo.FourCC)
        {
            isVideoProcCscEnabled = true;
        }
#endif
        sts = isVideoProcCscEnabled ? CheckFrameInfoDecVideoProcCsc(&surface_work->Info, MFX_CODEC_HEVC) : CheckFrameInfoCodecs(&surface_work->Info, MFX_CODEC_HEVC);
        MFX_CHECK(sts == MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM)

        sts = CheckFrameData(surface_work);
        MFX_CHECK_STS(sts);
    }

#ifdef MFX_MAX_DECODE_FRAMES
    MFX_CHECK(m_stat.NumFrame < MFX_MAX_DECODE_FRAMES, MFX_ERR_UNDEFINED_BEHAVIOR);
#endif

    sts = MFX_ERR_UNDEFINED_BEHAVIOR;

#if defined(MFX_ENABLE_PROTECT)
#if defined(MFX_ENABLE_PXP)
    //Check protect VA is enabled or not
    if( bs && m_va->GetProtectedVA() )
#else
#endif // MFX_ENABLE_PXP
    {
        MFX_CHECK(m_va->GetProtectedVA(), MFX_ERR_UNSUPPORTED);
        MFX_CHECK((bs->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME), MFX_ERR_UNSUPPORTED);
        m_va->GetProtectedVA()->SetBitstream(bs);
    }
#endif // MFX_ENABLE_PROTECT

    try
    {
        bool force = false;

        UMC::Status umcFrameRes = UMC::UMC_OK;

        MFXMediaDataAdapter src(bs);

        mfxExtBuffer* extbuf = (bs) ? GetExtendedBuffer(bs->ExtParam, bs->NumExtParam, MFX_EXTBUFF_DECODE_ERROR_REPORT) : NULL;

        if (extbuf)
        {
            reinterpret_cast<mfxExtDecodeErrorReport *>(extbuf)->ErrorTypes = 0;
            src.SetExtBuffer(extbuf);
        }
        m_pH265VideoDecoder->SetVideoCore(m_core);

        for (;;)
        {
            sts = m_surface_source->SetCurrentMFXSurface(surface_work);
            MFX_CHECK_STS(sts);

            umcRes = !m_surface_source->HasFreeSurface() ?
                UMC::UMC_ERR_NEED_FORCE_OUTPUT : m_pH265VideoDecoder->AddSource(bs ? &src : 0);

            umcFrameRes = umcRes;

            if (umcRes == UMC::UMC_NTF_NEW_RESOLUTION || umcRes == UMC::UMC_WRN_REPOSITION_INPROGRESS || umcRes == UMC::UMC_ERR_UNSUPPORTED)
            {
                FillVideoParam(&m_vPar, true);
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

            if (umcRes == UMC::UMC_OK && !m_surface_source->HasFreeSurface())
            {
                sts = MFX_ERR_MORE_SURFACE;
                umcFrameRes = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
            }

            if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || umcRes == UMC::UMC_WRN_INFO_NOT_READY || umcRes == UMC::UMC_ERR_NEED_FORCE_OUTPUT)
            {
                force = (umcRes == UMC::UMC_ERR_NEED_FORCE_OUTPUT);
                sts = umcRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER ? (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK: MFX_WRN_DEVICE_BUSY;
            }

            if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA || umcRes == UMC::UMC_ERR_SYNC)
            {
                if (!bs || bs->DataFlag == MFX_BITSTREAM_EOS)
                    force = true;
                sts = MFX_ERR_MORE_DATA;
            }

            if (umcRes == UMC::UMC_ERR_DEVICE_FAILED)
            {
                sts = MFX_ERR_DEVICE_FAILED;
            }
            if (umcRes == UMC::UMC_ERR_GPU_HANG)
            {
                sts = MFX_ERR_GPU_HANG;
            }

            src.Save(bs);

            if (sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
                MFX_RETURN(sts);

            //return these errors immediatelly unless we have [input == 0]
            if (sts == MFX_ERR_DEVICE_FAILED || sts == MFX_ERR_GPU_HANG)
            {
                if (!bs || bs->DataFlag == MFX_BITSTREAM_EOS)
                    force = true;
                else
                    MFX_RETURN(sts);
            }
            umcRes = m_pH265VideoDecoder->RunDecoding();

            if (m_vInitPar.mfx.DecodedOrder)
                force = true;

            H265DecoderFrame *pFrame = nullptr;

#if defined(MFX_ENABLE_PXP)
            if (m_va->GetProtectedVA())
            {
                if (umcFrameRes != UMC::UMC_ERR_NOT_ENOUGH_BUFFER)
                {
                    pFrame = GetFrameToDisplay_H265(force);
                }
            }
            else
#endif
            pFrame = GetFrameToDisplay_H265(force);

            // return frame to display
            if (pFrame)
            {
                FillOutputSurface(surface_out, surface_work, pFrame);

                m_frameOrder = (mfxU16)pFrame->m_frameOrder;
                (*surface_out)->Data.FrameOrder = m_frameOrder;
                return MFX_ERR_NONE;
            }

            *surface_out = 0;

            if (umcFrameRes != UMC::UMC_OK)
                break;
        } // for (;;)
    }
    catch(const h265_exception & ex)
    {
        FillVideoParam(&m_vPar, false);

        if (ex.GetStatus() == UMC::UMC_ERR_ALLOC)
        {
            // check incompatibility of video params
            if (m_vInitPar.mfx.FrameInfo.Width != m_vPar.mfx.FrameInfo.Width ||
                m_vInitPar.mfx.FrameInfo.Height != m_vPar.mfx.FrameInfo.Height)
            {
                MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            }
        }
        MFX_RETURN(ConvertUMCStatusToMfx(ex.GetStatus()));
    }
    catch(const std::bad_alloc &)
    {
        MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
    }
    catch(...)
    {
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }

    MFX_RETURN(sts);
}

// Fill up resolution information if new header arrived
void VideoDECODEH265::FillVideoParam(mfxVideoParamWrapper *par, bool full)
{
    if (!m_pH265VideoDecoder.get())
        return;

    FillParam(m_pH265VideoDecoder.get(), par, full);

    RawHeader_H265 *sps = m_pH265VideoDecoder->GetSPS();
    RawHeader_H265 *pps = m_pH265VideoDecoder->GetPPS();

    mfxExtCodingOptionSPSPPS * spsPps = (mfxExtCodingOptionSPSPPS *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS);
    if (spsPps)
    {
        if (sps->GetSize())
        {
            spsPps->SPSBufSize = (mfxU16)sps->GetSize();
            spsPps->SPSBuffer = sps->GetPointer();
        }
        else
        {
            spsPps->SPSBufSize = 0;
        }

        if (pps->GetSize())
        {
            spsPps->PPSBufSize = (mfxU16)pps->GetSize();
            spsPps->PPSBuffer = pps->GetPointer();
        }
        else
        {
            spsPps->PPSBufSize = 0;
        }
    }
}

// Fill up frame parameters before returning it to application
void VideoDECODEH265::FillOutputSurface(mfxFrameSurface1 **surf_out, mfxFrameSurface1 *surface_work, H265DecoderFrame * pFrame)
{
    m_stat.NumFrame++;
    m_stat.NumError += pFrame->GetError() ? 1 : 0;
    const UMC::FrameData * fd = pFrame->GetFrameData();

    *surf_out = m_surface_source->GetSurface(fd->GetFrameMID(), surface_work, &m_vPar);
    assert(*surf_out);

    mfxFrameSurface1 *surface_out = *surf_out;

    surface_out->Info.FrameId.TemporalId = 0;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(m_vFirstPar.ExtParam, m_vFirstPar.NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    if (videoProcessing)
    {
        surface_out->Info.CropH = videoProcessing->Out.CropH;
        surface_out->Info.CropW = videoProcessing->Out.CropW;
        surface_out->Info.CropX = videoProcessing->Out.CropX;
        surface_out->Info.CropY = videoProcessing->Out.CropY;
        surface_out->Info.ChromaFormat = videoProcessing->Out.ChromaFormat;
    }
        else
#endif
    {
        surface_out->Info.CropH = (mfxU16)(pFrame->lumaSize().height - pFrame->m_crop_bottom - pFrame->m_crop_top);
        surface_out->Info.CropW = (mfxU16)(pFrame->lumaSize().width - pFrame->m_crop_right - pFrame->m_crop_left);
        surface_out->Info.CropX = (mfxU16)(pFrame->m_crop_left);
        surface_out->Info.CropY = (mfxU16)(pFrame->m_crop_top);

        switch(pFrame->m_chroma_format)
        {
        case 0:
            surface_out->Info.ChromaFormat = MFX_CHROMAFORMAT_YUV400;
            break;
        case 1:
            surface_out->Info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            break;
        case 2:
            surface_out->Info.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            break;
        case 3:
            surface_out->Info.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            break;
        default:
            assert(!"Unknown chroma format");
            surface_out->Info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        }
    }

    bool isShouldUpdate = !(m_vFirstPar.mfx.FrameInfo.AspectRatioH || m_vFirstPar.mfx.FrameInfo.AspectRatioW);

    surface_out->Info.AspectRatioH = isShouldUpdate ? (mfxU16)pFrame->m_aspect_height : m_vFirstPar.mfx.FrameInfo.AspectRatioH;
    surface_out->Info.AspectRatioW = isShouldUpdate ? (mfxU16)pFrame->m_aspect_width : m_vFirstPar.mfx.FrameInfo.AspectRatioW;

    isShouldUpdate = !(m_vFirstPar.mfx.FrameInfo.FrameRateExtD || m_vFirstPar.mfx.FrameInfo.FrameRateExtN);

    surface_out->Info.FrameRateExtD = isShouldUpdate ? m_vPar.mfx.FrameInfo.FrameRateExtD : m_vFirstPar.mfx.FrameInfo.FrameRateExtD;
    surface_out->Info.FrameRateExtN = isShouldUpdate ? m_vPar.mfx.FrameInfo.FrameRateExtN : m_vFirstPar.mfx.FrameInfo.FrameRateExtN;

    surface_out->Info.PicStruct =
        UMC2MFX_PicStruct(pFrame->m_DisplayPictureStruct_H265, !!m_vPar.mfx.ExtendedPicStruct);

    surface_out->Data.TimeStamp = GetMfxTimeStamp(pFrame->m_dFrameTime);
    surface_out->Data.FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;

    surface_out->Data.DataFlag = (mfxU16)(pFrame->m_isOriginalPTS ? MFX_FRAMEDATA_ORIGINAL_TIMESTAMP : 0);

    TRACE_BUFFER_EVENT(MFX_TRACE_API_HEVC_OUTPUTINFO_TASK, EVENT_TYPE_INFO, TR_KEY_DECODE_BASIC_INFO,
        surface_out, H265DecodeSurfaceOutparam, SURFACEOUT_H265D);

    SEI_Storer_H265 * storer = m_pH265VideoDecoder->GetSEIStorer();
    if (storer)
        storer->SetTimestamp(pFrame);

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

    mfxExtMasteringDisplayColourVolume* display_colour = (mfxExtMasteringDisplayColourVolume*)GetExtendedBuffer(surface_out->Data.ExtParam, surface_out->Data.NumExtParam, MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME);
    if (display_colour && pFrame->m_mastering_display.payLoadSize > 0)
    {
        for (size_t i = 0; i < 3; i++)
        {
            display_colour->DisplayPrimariesX[i] = (mfxU16)pFrame->m_mastering_display.SEI_messages.mastering_display.display_primaries[i][0];
            display_colour->DisplayPrimariesY[i] = (mfxU16)pFrame->m_mastering_display.SEI_messages.mastering_display.display_primaries[i][1];
        }
        display_colour->WhitePointX = (mfxU16)pFrame->m_mastering_display.SEI_messages.mastering_display.white_point[0];
        display_colour->WhitePointY = (mfxU16)pFrame->m_mastering_display.SEI_messages.mastering_display.white_point[1];
        display_colour->MaxDisplayMasteringLuminance = (mfxU32)pFrame->m_mastering_display.SEI_messages.mastering_display.max_luminance;
        display_colour->MinDisplayMasteringLuminance = (mfxU32)pFrame->m_mastering_display.SEI_messages.mastering_display.min_luminance;
        display_colour->InsertPayloadToggle = MFX_PAYLOAD_IDR;
    }
    else if(display_colour)
    {
        display_colour->InsertPayloadToggle = MFX_PAYLOAD_OFF;
    }
    mfxExtContentLightLevelInfo* content_light = (mfxExtContentLightLevelInfo*)GetExtendedBuffer(surface_out->Data.ExtParam, surface_out->Data.NumExtParam, MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO);
    if (content_light && pFrame->m_content_light_level_info.payLoadSize > 0)
    {
        content_light->MaxContentLightLevel = (mfxU16)pFrame->m_content_light_level_info.SEI_messages.content_light_level_info.max_content_light_level;
        content_light->MaxPicAverageLightLevel = (mfxU16)pFrame->m_content_light_level_info.SEI_messages.content_light_level_info.max_pic_average_light_level;
        content_light->InsertPayloadToggle = MFX_PAYLOAD_IDR;
    }
    else if(content_light)
    {
        content_light->InsertPayloadToggle = MFX_PAYLOAD_OFF;
    }

}

// Wait until a frame is ready to be output and set necessary surface flags
mfxStatus VideoDECODEH265::DecodeFrame(mfxFrameSurface1 *surface_out, H265DecoderFrame * pFrame)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VideoDECODEH265::DecodeFrame");
    MFX_CHECK_NULL_PTR1(surface_out);

    mfxI32 index;
    if (pFrame)
    {
        index = pFrame->GetFrameData()->GetFrameMID();
    }
    else
    {
        index = m_surface_source->FindSurface(surface_out);
        pFrame = m_pH265VideoDecoder->FindSurface((UMC::FrameMemID)index);
        MFX_CHECK(pFrame, MFX_ERR_NOT_FOUND);
    }

    surface_out->Data.Corrupted = 0;
    int32_t const error = pFrame->GetError();

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

    mfxStatus sts = m_surface_source->PrepareToOutput(surface_out, index, &m_vPar);

    pFrame->setWasDisplayed();

    TRACE_EVENT(MFX_TRACE_API_HEVC_DISPLAYINFO_TASK, EVENT_TYPE_INFO, TR_KEY_DECODE_BASIC_INFO, make_event_data(
        pFrame->m_PicOrderCnt, (uint32_t)pFrame->wasDisplayed(), (uint32_t)pFrame->wasOutputted()));

    return sts;
}

// Wait until a frame is ready to be output and set necessary surface flags
mfxStatus VideoDECODEH265::DecodeFrame(mfxBitstream *, mfxFrameSurface1 *, mfxFrameSurface1 *surface_out)
{
    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(surface_out);
    mfxStatus sts = DecodeFrame(surface_out);

    return sts;
}

// Returns closed caption data
mfxStatus VideoDECODEH265::GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts)
{
    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR3(ud, sz, ts);

    mfxStatus       MFXSts = MFX_ERR_NONE;

    UMC::MediaData data;
    UMC::Status umcRes = m_pH265VideoDecoder->GetUserData(&data);

    if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA)
        MFX_RETURN(MFX_ERR_MORE_DATA);

    MFX_CHECK(*sz >= data.GetDataSize(), MFX_ERR_NOT_ENOUGH_BUFFER);

    *sz = (mfxU32)data.GetDataSize();
    *ts = GetMfxTimeStamp(data.GetTime());
    mfxU8 *pDataPointer = reinterpret_cast <mfxU8*> (data.GetDataPointer());
    std::copy(pDataPointer, pDataPointer + *sz, ud);

    return MFXSts;
}

// Returns stored SEI messages
mfxStatus VideoDECODEH265::GetPayload( mfxU64 *ts, mfxPayload *payload )
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR3(ts, payload, payload->Data);

    SEI_Storer_H265 * storer = m_pH265VideoDecoder->GetSEIStorer();

    MFX_CHECK(storer, MFX_ERR_UNKNOWN);

    const SEI_Storer_H265::SEI_Message * msg = storer->GetPayloadMessage();

    if (msg)
    {
        MFX_CHECK(payload->BufSize >= msg->size, MFX_ERR_NOT_ENOUGH_BUFFER);

        *ts = GetMfxTimeStamp(msg->timestamp);

        std::copy(msg->data, msg->data + msg->size, payload->Data);

        payload->CtrlFlags =
            msg->nal_type == NAL_UT_SEI_SUFFIX ? MFX_PAYLOAD_CTRL_SUFFIX : 0;
        payload->NumBit = (mfxU32)(msg->size * 8);
        payload->Type = (mfxU16)msg->type;
    }
    else
    {
        payload->NumBit = 0;
        *ts = MFX_TIME_STAMP_INVALID;
    }

    return MFX_ERR_NONE;
}

// Find a next frame ready to be output from decoder
H265DecoderFrame * VideoDECODEH265::GetFrameToDisplay_H265(bool force)
{
    H265DecoderFrame * pFrame = m_pH265VideoDecoder->GetFrameToDisplayInternal(force);
    if (!pFrame)
    {
        return 0;
    }

    pFrame->setWasOutputted();
    m_pH265VideoDecoder->PostProcessDisplayFrame(pFrame);

    return pFrame;
}

// MediaSDK DECODE_SetSkipMode API function
mfxStatus VideoDECODEH265::SetSkipMode(mfxSkipMode mode)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    int32_t test_num = 0;
    mfxStatus sts = m_pH265VideoDecoder->ChangeVideoDecodingSpeed(test_num);
    MFX_CHECK_STS(sts);

    int32_t num = 0;

    switch(mode)
    {
    case MFX_SKIPMODE_NOSKIP:
        num = -10;
        break;
    case MFX_SKIPMODE_MORE:
        num = 1;
        break;
    case MFX_SKIPMODE_LESS:
        num = -1;
        break;

    default:
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    m_pH265VideoDecoder->ChangeVideoDecodingSpeed(num);

    MFX_CHECK(test_num != num, MFX_WRN_VALUE_NOT_CHANGED);
    return MFX_ERR_NONE;
}

// Check if new parameters are compatible with new parameters
bool VideoDECODEH265::IsSameVideoParam(mfxVideoParam * newPar, mfxVideoParam * oldPar, eMFXHWType type)
{
    auto const mask =
          MFX_IOPATTERN_OUT_SYSTEM_MEMORY
        | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    if ((newPar->IOPattern & mask) !=
        (oldPar->IOPattern & mask))
    {
        return false;
    }

    if (newPar->Protected != oldPar->Protected)
    {
        return false;
    }

    if (CalculateAsyncDepth(newPar) != CalculateAsyncDepth(oldPar))
    {
        return false;
    }

    mfxFrameAllocRequest requestOld;
    memset(&requestOld, 0, sizeof(requestOld));
    mfxFrameAllocRequest requestNew;
    memset(&requestNew, 0, sizeof(requestNew));

    mfxStatus mfxSts = QueryIOSurfInternal(type, oldPar, &requestOld);
    if (mfxSts != MFX_ERR_NONE)
        return false;

    mfxSts = QueryIOSurfInternal(type, newPar, &requestNew);
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

mfxStatus VideoDECODEH265::GetSurface(mfxFrameSurface1* & surface, mfxSurfaceHeader* import_surface)
{
    MFX_CHECK(m_surface_source, MFX_ERR_NOT_INITIALIZED);

    return m_surface_source->GetSurface(surface, import_surface);
}

mfxFrameSurface1 *VideoDECODEH265::GetInternalSurface(mfxFrameSurface1 *surface)
{
    return m_surface_source->GetInternalSurface(surface);
}

#endif // MFX_ENABLE_H265_VIDEO_DECODE
