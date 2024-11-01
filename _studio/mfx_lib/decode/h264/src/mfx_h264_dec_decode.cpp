// Copyright (c) 2004-2024 Intel Corporation
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

#include <algorithm> /* for std::find on Linux/Android */

#include "mfx_h264_dec_decode.h"

#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#include "mfx_common.h"
#include "mfx_common_decode_int.h"

// debug
#include "umc_h264_frame_list.h"

#include "umc_h264_va_supplier.h"
#include "umc_va_video_processing.h"


#if defined(MFX_ENABLE_PXP)
#include "mfx_pxp_video_accelerator.h"
#include "mfx_pxp_h264_supplier.h"
#endif // MFX_ENABLE_PXP

#include "libmfx_core_interface.h"

#include "mfx_utils.h"

#include "mfx_unified_h264d_logging.h"

inline bool IsNeedToUseHWBuffering(eMFXHWType /*type*/)
{
    return false;
}

inline bool IsBigSurfacePoolApplicable(eMFXHWType type)
{
    bool ret = false;

    (void)(type); //UNREFERENCED_PARAMETER
    return ret;
}

struct ThreadTaskInfo264
{
    mfxFrameSurface1* surface_out      = nullptr;
    bool              is_decoding_done = false;

    ThreadTaskInfo264(mfxFrameSurface1* out)
        : surface_out(out)
    {}
};

enum
{
    ENABLE_DELAYED_DISPLAY_MODE = 1
};

typedef std::vector<uint32_t> ViewIDsList;

inline bool AddDependency(uint32_t dependOnViewId, const ViewIDsList & targetList, ViewIDsList &dependencyList)
{
    ViewIDsList::const_iterator view_iter = std::find(targetList.begin(), targetList.end(), dependOnViewId);
    if (view_iter == targetList.end())
    {
        view_iter = std::find(dependencyList.begin(), dependencyList.end(), dependOnViewId);
        if (view_iter == dependencyList.end())
        {
            dependencyList.push_back(dependOnViewId);
        }

        return true;
    }

    return false;
}

mfxStatus Dependency(mfxExtMVCSeqDesc * mvcPoints, ViewIDsList & targetList, ViewIDsList &dependencyList)
{
    for (mfxU32 i = 0; i < targetList.size(); ++i)
    {
        mfxU32 viewId = targetList[i];

        // find view dependency
        mfxMVCViewDependency * refInfo = 0;
        for (mfxU32 k = 0; k < mvcPoints->NumView; k++)
        {
            if (mvcPoints->View[k].ViewId == viewId)
            {
                refInfo = &mvcPoints->View[k];
                break;
            }
        }

        MFX_CHECK(refInfo, MFX_ERR_INVALID_VIDEO_PARAM);

        bool wasAdded = false;

        for (mfxU32 j = 0; j < refInfo->NumAnchorRefsL0; j++)
        {
            wasAdded = wasAdded || AddDependency(refInfo->AnchorRefL0[j], targetList, dependencyList);
        }

        for (mfxU32 j = 0; j < refInfo->NumAnchorRefsL1; j++)
        {
            wasAdded = wasAdded || AddDependency(refInfo->AnchorRefL1[j], targetList, dependencyList);
        }

        for (mfxU32 j = 0; j < refInfo->NumNonAnchorRefsL0; j++)
        {
            wasAdded = wasAdded || AddDependency(refInfo->NonAnchorRefL0[j], targetList, dependencyList);
        }

        for (mfxU32 j = 0; j < refInfo->NumNonAnchorRefsL1; j++)
        {
            wasAdded = wasAdded || AddDependency(refInfo->NonAnchorRefL1[j], targetList, dependencyList);
        }

        if (wasAdded)
        {
            i = 0;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus Dependency(mfxExtMVCSeqDesc * mvcPoints, mfxExtMVCTargetViews * targetViews, ViewIDsList &targetList, ViewIDsList &dependencyList)
{
    targetList.reserve(targetViews->NumView);
    for (uint32_t i = 0; i < targetViews->NumView; i++)
    {
        targetList.push_back(targetViews->ViewId[i]);
    }

    mfxStatus sts = Dependency(mvcPoints, targetList, dependencyList);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);

    sts = Dependency(mvcPoints, dependencyList, dependencyList);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

mfxU32 CalculateRequiredView(mfxVideoParam *par)
{
    if (!IsMVCProfile(par->mfx.CodecProfile))
        return 1;

    mfxExtMVCSeqDesc * mvcPoints = (mfxExtMVCSeqDesc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
    if (!mvcPoints)
        return 1;

    mfxExtMVCTargetViews * targetViews = (mfxExtMVCTargetViews *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);

    ViewIDsList viewList;
    ViewIDsList dependencyList;

    if (!targetViews)
    {
        return mvcPoints->NumView;
    }

    mfxStatus sts = Dependency(mvcPoints, targetViews, viewList, dependencyList);
    if (sts < MFX_ERR_NONE)
        return 1;

    return (mfxU32)(viewList.size() + dependencyList.size());
}

VideoDECODEH264::VideoDECODEH264(VideoCORE *core, mfxStatus * sts)
    : VideoDECODE()
    , m_core(core)
    , m_isInit(false)
    , m_frameOrder((mfxU16)MFX_FRAMEORDER_UNKNOWN)
    , m_response()
    , m_response_alien()
    , m_useDelayedDisplay(false)
    , m_va(0)
    , m_globalTask(false)
    , m_isFirstRun(true)
{
    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

VideoDECODEH264::~VideoDECODEH264(void)
{
    Close();
}

mfxStatus VideoDECODEH264::Init(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(!m_isInit, MFX_ERR_UNDEFINED_BEHAVIOR);

    MFX_CHECK_NULL_PTR1(par);

    eMFXPlatform platform = MFX_Utility::GetPlatform(m_core, par);

    MFX_CHECK(platform == MFX_PLATFORM_HARDWARE, MFX_ERR_UNSUPPORTED);

    eMFXHWType type = m_core->GetHWType();

    mfxStatus mfxSts = CheckVideoParamDecoders(par, type);
    MFX_CHECK(mfxSts >= MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);

    bool videoParamIsValid = MFX_Utility::CheckVideoParam(par, type);
    MFX_CHECK(videoParamIsValid, MFX_ERR_INVALID_VIDEO_PARAM);

    if (m_core->GetVAType() == MFX_HW_VAAPI)
    {
        mfxU16 codecProfile = par->mfx.CodecProfile & 0xFF;
        MFX_CHECK(codecProfile != MFX_PROFILE_AVC_STEREO_HIGH &&
                  codecProfile != MFX_PROFILE_AVC_MULTIVIEW_HIGH,
                  MFX_ERR_INVALID_VIDEO_PARAM);

#ifdef MFX_ENABLE_SVC_VIDEO_DECODE
        MFX_CHECK(codecProfile != MFX_PROFILE_AVC_SCALABLE_BASELINE &&
                  codecProfile != MFX_PROFILE_AVC_SCALABLE_HIGH, MFX_ERR_INVALID_VIDEO_PARAM);
#endif
    }

    m_vInitPar = *par;
    m_vFirstPar = *par;
    m_vFirstPar.mfx.NumThread = 0;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_vFirstPar);

    m_vPar = m_vFirstPar;
    m_vPar.CreateExtendedBuffer(MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    m_vPar.CreateExtendedBuffer(MFX_EXTBUFF_CODING_OPTION_SPSPPS);

    mfxU32 asyncDepth = CalculateAsyncDepth(par);
    m_vPar.mfx.NumThread = (mfxU16)CalculateNumThread(par);

    m_useDelayedDisplay = ENABLE_DELAYED_DISPLAY_MODE != 0 && IsNeedToUseHWBuffering(m_core->GetHWType()) && (asyncDepth != 1);

    bool bUseBigSurfaceWA = IsBigSurfacePoolApplicable(type);

    int32_t useInternal = m_vPar.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    if (videoProcessing)
    {
         MFX_CHECK((m_vPar.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY),
            MFX_ERR_UNSUPPORTED);

         //PicStruct support differs, need to check per-platform
        if (H264DCaps::IsOnlyProgressivePicStructSupported(m_core->GetHWType()))
        {
             MFX_CHECK(m_vPar.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE, MFX_ERR_UNSUPPORTED);
        }

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

    // allocate memory
    mfxFrameAllocRequest request;
    mfxFrameAllocRequest request_internal;
    memset(&request, 0, sizeof(request));
    memset(&m_response, 0, sizeof(m_response));
    memset(&m_response_alien, 0, sizeof(m_response_alien));

    mfxSts = QueryIOSurfInternal(type, &m_vPar, &request);
    MFX_CHECK_STS(mfxSts);

    bool* isD3D9On11Core = QueryCoreInterface<bool>(m_core, MFXI_IS_CORED3D9ON11_GUID);
    if (isD3D9On11Core && (*isD3D9On11Core) == true)
        useInternal = true;

    request.Type |= useInternal ? MFX_MEMTYPE_INTERNAL_FRAME : MFX_MEMTYPE_EXTERNAL_FRAME;
    request_internal = request;
    try
    {
        m_surface_source.reset(new SurfaceSource(m_core, *par, platform, request, request_internal, m_response, m_response_alien));
    }
    catch (const std::system_error& ex)
    {
        MFX_CHECK_STS(mfxStatus(ex.code().value()));
    }

    mfxU16 oldProfile = m_vFirstPar.mfx.CodecProfile;
    m_vFirstPar.mfx.CodecProfile = GetChangedProfile(&m_vFirstPar);

    mfxSts = m_core->CreateVA(&m_vFirstPar, &request, &m_response, m_surface_source.get());
    MFX_CHECK(mfxSts >= MFX_ERR_NONE, mfxSts);

    UMC::Status umcSts = m_MemoryAllocator.InitMem(0, m_core);
    MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);

    UMC::H264VideoDecoderParams umcVideoParams;
    ConvertMFXParamsToUMC(&m_vFirstPar, &umcVideoParams);
    umcVideoParams.numThreads = m_vPar.mfx.NumThread;
    umcVideoParams.m_bufferedFrames = (asyncDepth > mfxU32(umcVideoParams.numThreads)) ? asyncDepth - umcVideoParams.numThreads : 0;

    m_core->GetVA((mfxHDL*)&m_va, MFX_MEMTYPE_FROM_DECODE);
    umcVideoParams.pVideoAccelerator = m_va;

#if defined(MFX_ENABLE_PXP)
    if (m_va->GetProtectedVA())
        m_pH264VideoDecoder.reset(bUseBigSurfaceWA ? new UMC::VATaskSupplierBigSurfacePool<UMC::PXPH264Supplier>() : new UMC::PXPH264Supplier()); // HW
    else
#endif // MFX_ENABLE_PXP
        m_pH264VideoDecoder.reset(bUseBigSurfaceWA ? new UMC::VATaskSupplierBigSurfacePool<UMC::VATaskSupplier>() : new UMC::VATaskSupplier()); // HW

    m_pH264VideoDecoder->SetFrameAllocator(m_surface_source.get());
    static_cast<UMC::VATaskSupplier*>(m_pH264VideoDecoder.get())->SetVideoHardwareAccelerator(m_va);


#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    if (m_va->GetVideoProcessingVA())
    {
        umcSts = m_va->GetVideoProcessingVA()->Init(par, videoProcessing);
        MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_INVALID_VIDEO_PARAM);
    }
#endif

    umcVideoParams.lpMemoryAllocator = &m_MemoryAllocator;

    umcVideoParams.m_ignore_level_constrain = par->mfx.IgnoreLevelConstrain;

    umcSts = m_pH264VideoDecoder->Init(&umcVideoParams);
    if (umcSts != UMC::UMC_OK)
    {
        MFX_RETURN(ConvertUMCStatusToMfx(umcSts));
    }

    m_vFirstPar.mfx.CodecProfile = oldProfile;
    SetTargetViewList(&m_vFirstPar);

    m_isInit = true;

    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;
    m_globalTask = false;
    m_isFirstRun = true;

    if (m_useDelayedDisplay)
    {
        ((UMC::VATaskSupplier*)m_pH264VideoDecoder.get())->SetBufferedFramesNumber(NUMBER_OF_ADDITIONAL_FRAMES);
    }

    m_pH264VideoDecoder->SetVideoParams(&m_vFirstPar);

    MFX_CHECK(platform == m_core->GetPlatformType(), MFX_ERR_UNSUPPORTED);

    if (isNeedChangeVideoParamWarning)
    {
        MFX_RETURN(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH264::QueryImplsDescription(
    VideoCORE& core,
    mfxDecoderDescription::decoder& caps,
    mfx::PODArraysHolder& ah)
{
    const mfxU16 SupportedProfiles[] =
    {
        MFX_PROFILE_AVC_BASELINE
        , MFX_PROFILE_AVC_MAIN
        , MFX_PROFILE_AVC_HIGH
        , MFX_PROFILE_AVC_HIGH_422
        , MFX_PROFILE_AVC_EXTENDED
#ifdef MFX_ENABLE_SVC_VIDEO_DECODE
        , MFX_PROFILE_AVC_SCALABLE_BASELINE
        , MFX_PROFILE_AVC_SCALABLE_HIGH
#endif
        , MFX_PROFILE_AVC_MULTIVIEW_HIGH
        , MFX_PROFILE_AVC_STEREO_HIGH
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

    caps.CodecID = MFX_CODEC_AVC;
    caps.MaxcodecLevel = MFX_LEVEL_AVC_62;
    mfxVideoParam par;
    memset(&par, 0, sizeof(par));
    par.mfx.CodecId = MFX_CODEC_AVC;
    par.mfx.CodecLevel = caps.MaxcodecLevel;

    mfxStatus sts = MFX_ERR_NONE;
    for (mfxU16 profile : SupportedProfiles)
    {
        par.mfx.CodecProfile = profile;
        // Set FourCC to pass IsNeedPartialAcceleration check
        par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

        sts = VideoDECODEH264::Query(&core, &par, &par);
        if (sts != MFX_ERR_NONE) continue;

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
                par.mfx.FrameInfo.FourCC = fcc;
                sts = VideoDECODEH264::Query(&core, &par, &par);
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

mfxStatus VideoDECODEH264::GetSurface(mfxFrameSurface1* & surface, mfxSurfaceHeader* import_surface)
{
    MFX_CHECK(m_surface_source, MFX_ERR_NOT_INITIALIZED);

    return m_surface_source->GetSurface(surface, import_surface);
}

mfxU16 VideoDECODEH264::GetChangedProfile(mfxVideoParam *par)
{
#ifdef MFX_ENABLE_SVC_VIDEO_DECODE
    if (IsSVCProfile(par->mfx.CodecProfile))
    {
        mfxExtSvcTargetLayer * svcTarget = (mfxExtSvcTargetLayer*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_TARGET_LAYER);
        if (svcTarget && !svcTarget->TargetDependencyID && !svcTarget->TargetQualityID)
        {
            return MFX_PROFILE_AVC_HIGH;
        }
    }
#endif

    if (!IsMVCProfile(par->mfx.CodecProfile))
        return par->mfx.CodecProfile;

    mfxExtMVCTargetViews * targetViews = (mfxExtMVCTargetViews *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);
    if (targetViews && targetViews->NumView == 1 && targetViews->ViewId[0] == 0)
    {
        return MFX_PROFILE_AVC_HIGH;
    }

    return par->mfx.CodecProfile;
}

mfxStatus VideoDECODEH264::SetTargetViewList(mfxVideoParam *par)
{
#ifdef MFX_ENABLE_SVC_VIDEO_DECODE
    if (IsSVCProfile(par->mfx.CodecProfile))
    {
        mfxExtSvcTargetLayer * svcTarget = (mfxExtSvcTargetLayer*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_TARGET_LAYER);
        if (svcTarget)
        {
            m_pH264VideoDecoder->SetSVCTargetLayer(svcTarget->TargetDependencyID, svcTarget->TargetQualityID, svcTarget->TargetTemporalID);
        }
        else
        {
            mfxExtSVCSeqDesc * svcDesc = (mfxExtSVCSeqDesc*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC);
            if (svcDesc)
            {
                mfxU32 maxDependencyId = 0;
                for (size_t i = 0; i < sizeof(svcDesc->DependencyLayer)/sizeof(svcDesc->DependencyLayer[0]); i++)
                {
                    if (svcDesc->DependencyLayer[i].Active)
                        maxDependencyId = (mfxU32)i;
                }

                m_pH264VideoDecoder->SetSVCTargetLayer(maxDependencyId, UMC::H264_MAX_QUALITY_ID, UMC::H264_MAX_TEMPORAL_ID);
            }
        }
    }
#endif

    ViewIDsList viewList;
    ViewIDsList dependencyList;

    mfxExtMVCSeqDesc * mvcPoints = (mfxExtMVCSeqDesc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
    if (!mvcPoints)
    {
        viewList.push_back(0); // base view only
        m_pH264VideoDecoder->SetViewList(viewList, dependencyList);
        return MFX_ERR_NONE;
    }

    mfxExtMVCTargetViews * targetViews = (mfxExtMVCTargetViews *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);

    if (targetViews)
    {
        mfxStatus sts = Dependency(mvcPoints, targetViews, viewList, dependencyList);
        MFX_CHECK(sts >= MFX_ERR_NONE, sts);

        m_pH264VideoDecoder->SetTemporalId(targetViews->TemporalId);
    }

    m_pH264VideoDecoder->SetViewList(viewList, dependencyList);
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH264::Reset(mfxVideoParam *par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEH264::Reset");

    UMC::AutomaticUMCMutex guard(m_mGuard);

    TRACE_EVENT(MFX_TRACE_API_DECODE_RESET_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(par ? par->mfx.FrameInfo.Width : 0,
        par ? par->mfx.FrameInfo.Height : 0, par ? par->mfx.CodecId : 0));

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(par);

    eMFXHWType type = m_core->GetHWType();

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * extVideoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(m_vFirstPar.ExtParam, m_vFirstPar.NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);

    if (extVideoProcessing != nullptr)
    {
        if (extVideoProcessing->Out.Width >= par->mfx.FrameInfo.Width ||
            extVideoProcessing->Out.Height >= par->mfx.FrameInfo.Height)
        {
            MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
        }
    }
#endif

    eMFXPlatform platform = MFX_Utility::GetPlatform(m_core, par);
    MFX_CHECK(platform == MFX_PLATFORM_HARDWARE, MFX_ERR_UNSUPPORTED);

    mfxStatus mfxSts = CheckVideoParamDecoders(par, type);
    MFX_CHECK(mfxSts >= MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);

    bool videoParamIsValid = MFX_Utility::CheckVideoParam(par, type);
    MFX_CHECK(videoParamIsValid, MFX_ERR_INVALID_VIDEO_PARAM);

    bool videoParamIsSame = IsSameVideoParam(par, &m_vInitPar, type);
    MFX_CHECK(videoParamIsSame, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    m_pH264VideoDecoder->Reset();

    SetTargetViewList(par);

    UMC::Status umcSts = m_surface_source->Reset();
    MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);

    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;
    m_globalTask = false;
    m_isFirstRun = true;

    memset(&m_stat, 0, sizeof(m_stat));
    m_vFirstPar = *par;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_vFirstPar);
    m_vPar = m_vFirstPar;
    m_vPar.CreateExtendedBuffer(MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    m_vPar.CreateExtendedBuffer(MFX_EXTBUFF_CODING_OPTION_SPSPPS);

    m_vPar.mfx.NumThread = (mfxU16)CalculateNumThread(par);

    m_pH264VideoDecoder->SetVideoParams(&m_vFirstPar);

    MFX_CHECK(platform == m_core->GetPlatformType(), MFX_ERR_UNSUPPORTED);

    if (isNeedChangeVideoParamWarning)
    {
        MFX_RETURN(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    }

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    /* in case of mfxExtDecVideoProcessing (SFC) usage
     * required to set new params for UMC::VideoProcessingVA
     * is mfxExtDecVideoProcessing attached or not - checked in IsSameVideoParam */
    auto videoProcessing = reinterpret_cast<mfxExtDecVideoProcessing *>(GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING));
    if (m_va->GetVideoProcessingVA())
    {
        umcSts = m_va->GetVideoProcessingVA()->Init(par, videoProcessing);
        MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_INVALID_VIDEO_PARAM);
    }
#endif //MFX_DEC_VIDEO_POSTPROCESS_DISABLE

    TRACE_EVENT(MFX_TRACE_API_DECODE_RESET_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(MFX_ERR_NONE));

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH264::Close(void)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(m_isInit && m_pH264VideoDecoder.get(), MFX_ERR_NOT_INITIALIZED);

    m_pH264VideoDecoder->Close();
    m_surface_source->Close();

    m_isInit = false;
    m_isFirstRun = true;
    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;
    m_va = 0;
    memset(&m_stat, 0, sizeof(m_stat));
    return MFX_ERR_NONE;
}

mfxTaskThreadingPolicy VideoDECODEH264::GetThreadingPolicy(void)
{
    return MFX_TASK_THREADING_SHARED;
}

mfxStatus VideoDECODEH264::Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEH264::Query");
    MFX_CHECK_NULL_PTR1(out);

    eMFXPlatform platform = MFX_Utility::GetPlatform(core, in);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (platform == MFX_PLATFORM_HARDWARE)
    {
        type = core->GetHWType();
    }

    mfxStatus mfxSts = MFX_Utility::Query(core, in, out, type);
    MFX_CHECK_STS(mfxSts);

    return mfxSts;
}

mfxStatus VideoDECODEH264::GetVideoParam(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    TRACE_EVENT(MFX_TRACE_API_DECODE_GETVIDEOPARAM_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(par ? par->mfx.FrameInfo.Width : 0,
        par ? par->mfx.FrameInfo.Height : 0, par ? par->mfx.CodecId : 0));

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

    // mvc headers
    mfxExtMVCSeqDesc * mvcSeqDesc = (mfxExtMVCSeqDesc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
    mfxExtMVCSeqDesc * mvcSeqDescInternal = (mfxExtMVCSeqDesc *)GetExtendedBuffer(m_vPar.ExtParam, m_vPar.NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
    if (mvcSeqDesc && mvcSeqDescInternal && mvcSeqDescInternal->NumView)
    {
        mvcSeqDesc->NumView   = mvcSeqDescInternal->NumView;
        mvcSeqDesc->NumViewId = mvcSeqDescInternal->NumViewId;
        mvcSeqDesc->NumOP     = mvcSeqDescInternal->NumOP;

        MFX_CHECK(mvcSeqDesc->NumViewAlloc   >= mvcSeqDescInternal->NumView   &&
                  mvcSeqDesc->NumViewIdAlloc >= mvcSeqDescInternal->NumViewId &&
                  mvcSeqDesc->NumOPAlloc     >= mvcSeqDescInternal->NumOP,
                  MFX_ERR_NOT_ENOUGH_BUFFER);

        std::copy(mvcSeqDescInternal->View,   mvcSeqDescInternal->View   + mvcSeqDescInternal->NumView,   mvcSeqDesc->View);
        std::copy(mvcSeqDescInternal->ViewId, mvcSeqDescInternal->ViewId + mvcSeqDescInternal->NumViewId, mvcSeqDesc->ViewId);
        std::copy(mvcSeqDescInternal->OP,     mvcSeqDescInternal->OP     + mvcSeqDescInternal->NumOP,     mvcSeqDesc->OP);

        mfxU16 * targetView = mvcSeqDesc->ViewId;
        for (mfxU32 i = 0; i < mvcSeqDesc->NumOP; i++)
        {
            mvcSeqDesc->OP[i].TargetViewId = targetView;
            targetView += mvcSeqDesc->OP[i].NumTargetViews;
        }
    }

    mfxExtMVCTargetViews * mvcTarget = (mfxExtMVCTargetViews *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);
    mfxExtMVCTargetViews * mvcTargetInternal = (mfxExtMVCTargetViews *)GetExtendedBuffer(m_vPar.ExtParam, m_vPar.NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);
    if (mvcTarget && mvcTargetInternal && mvcTargetInternal->NumView)
    {
        *mvcTarget = *mvcTargetInternal;
    }

#ifdef MFX_ENABLE_SVC_VIDEO_DECODE
    mfxExtSVCSeqDesc * svcSeqDesc = (mfxExtSVCSeqDesc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC);
    mfxExtSVCSeqDesc * svcSeqDescInternal = (mfxExtSVCSeqDesc *)GetExtendedBuffer(m_vPar.ExtParam, m_vPar.NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC);
    if (svcSeqDesc && svcSeqDescInternal)
    {
        *svcSeqDesc = *svcSeqDescInternal;
    }

    mfxExtSvcTargetLayer * svcTarget = (mfxExtSvcTargetLayer *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_TARGET_LAYER);
    mfxExtSvcTargetLayer * svcTargetInternal = (mfxExtSvcTargetLayer *)GetExtendedBuffer(m_vPar.ExtParam, m_vPar.NumExtParam, MFX_EXTBUFF_SVC_TARGET_LAYER);
    if (svcTarget && svcTargetInternal)
    {
        *svcTarget = *svcTargetInternal;
    }
#endif

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
            par->mfx.FrameInfo.FrameRateExtN = 30;
            par->mfx.FrameInfo.FrameRateExtD = 1;
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


    TRACE_EVENT(MFX_TRACE_API_DECODE_GETVIDEOPARAM_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(MFX_ERR_NONE));

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH264::DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEH264::DecodeHeader");
    MFX_CHECK_NULL_PTR2(bs, par);

    mfxStatus sts = CheckBitstream(bs);
    MFX_CHECK_STS(sts);

    MFXMediaDataAdapter in(bs);

    mfx_UMC_MemAllocator  tempAllocator;
    tempAllocator.InitMem(0, core);

    UMC::H264VideoDecoderParams avcInfo;
    avcInfo.m_pData = &in;

    MFX_AVC_Decoder decoder;

    decoder.SetMemoryAllocator(&tempAllocator);
    UMC::Status umcRes = MFX_Utility::DecodeHeader(&decoder, &avcInfo, bs, par);

    MFX_CHECK(umcRes != UMC::UMC_ERR_NOT_ENOUGH_DATA, MFX_ERR_MORE_DATA);

    MFX_CHECK_UMC_STS(umcRes);

    umcRes = MFX_Utility::FillVideoParamMVCEx(&decoder, par);
    MFX_CHECK_UMC_STS(umcRes);

    // sps/pps headers
    mfxExtCodingOptionSPSPPS * spsPps = (mfxExtCodingOptionSPSPPS *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS);
    if (spsPps)
    {
        UMC::RawHeader *sps = decoder.GetSPS();
        UMC::RawHeader *pps = decoder.GetPPS();

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

mfxStatus VideoDECODEH264::QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEH264::QueryIOSurf");
    MFX_CHECK_NULL_PTR2(par, request);

    eMFXPlatform platform = MFX_Utility::GetPlatform(core, par);

    MFX_CHECK(platform == MFX_PLATFORM_HARDWARE, MFX_ERR_UNSUPPORTED);

    eMFXHWType type = core->GetHWType();

    mfxVideoParam params;
    params = *par;
    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&params);

    if (   !(par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        && !(par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);

    if ((par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);

    int32_t isInternalManaging = params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    bool* isD3D9On11Core = QueryCoreInterface<bool>(core, MFXI_IS_CORED3D9ON11_GUID);
    if (isD3D9On11Core && (*isD3D9On11Core) == true && (params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        isInternalManaging = true;

    mfxStatus sts = QueryIOSurfInternal(type, &params, request);
    MFX_CHECK_STS(sts);

    sts = UpdateCscOutputFormat(par, request);
    MFX_CHECK_STS(sts);

    if (isInternalManaging)
    {
        request->NumFrameSuggested = request->NumFrameMin = (mfxU16)CalculateAsyncDepth(par);
        if (params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
            request->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }

    request->Type |= MFX_MEMTYPE_EXTERNAL_FRAME;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
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

    if (platform != core->GetPlatformType())
    {
        assert(platform == MFX_PLATFORM_SOFTWARE);
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    if (isNeedChangeVideoParamWarning)
    {
        MFX_RETURN(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH264::QueryIOSurfInternal(eMFXHWType type, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    request->Info = par->mfx.FrameInfo;

    mfxExtMVCSeqDesc * points = (mfxExtMVCSeqDesc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
    mfxU8 level_idc = (mfxU8)par->mfx.CodecLevel;

    if (IsMVCProfile(par->mfx.CodecProfile) && points && points->OP)
    {
        level_idc = mfxU8 (std::max(mfxU16(level_idc), points->OP->LevelIdc));
    }

    mfxU32 asyncDepth = CalculateAsyncDepth(par);
    bool useDelayedDisplay = (ENABLE_DELAYED_DISPLAY_MODE != 0) && IsNeedToUseHWBuffering(type) && (asyncDepth != 1);

    mfxI32 dpbSize = UMC::CalculateDPBSize(level_idc, par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height, 0);
    if (par->mfx.MaxDecFrameBuffering && par->mfx.MaxDecFrameBuffering < dpbSize)
        dpbSize = par->mfx.MaxDecFrameBuffering;

    mfxU32 numMin = dpbSize + 1 + asyncDepth;

    if (useDelayedDisplay) // equals if (m_useDelayedDisplay) - workaround
        numMin += NUMBER_OF_ADDITIONAL_FRAMES;
    numMin *= CalculateRequiredView(par);

#ifdef MFX_ENABLE_SVC_VIDEO_DECODE
    numMin *= IsSVCProfile(par->mfx.CodecProfile) ? 2 : 1;
#endif

    request->NumFrameMin = (mfxU16)numMin;

    request->NumFrameSuggested = request->NumFrameMin;

    request->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH264::GetDecodeStat(mfxDecodeStat *stat)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    TRACE_EVENT(MFX_TRACE_API_DECODE_GETSTAT_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(0));

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(stat);

    m_stat.NumSkippedFrame = m_pH264VideoDecoder->GetSkipInfo().numberOfSkippedFrames;
    m_stat.NumCachedFrame = 0;

    UMC::H264DBPList * lst = m_pH264VideoDecoder->GetDPBList(UMC::BASE_VIEW, 0);
    if (lst)
    {
        UMC::H264DecoderFrame *pFrame = lst->head();
        for (; pFrame; pFrame = pFrame->future())
        {
            if (!pFrame->wasOutputted() && !UMC::isAlmostDisposable(pFrame))
                m_stat.NumCachedFrame++;
        }
    }

    m_stat.reserved[0] = m_pH264VideoDecoder->IsExistHeadersError() ? 1 : 0;

    *stat = m_stat;

    TRACE_EVENT(MFX_TRACE_API_DECODE_GETSTAT_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(stat ? stat->NumFrame : 0, MFX_ERR_NONE));

    return MFX_ERR_NONE;
}

static mfxStatus AVCDECODERoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "AVCDECODERoutine");
    mfxStatus sts = MFX_ERR_NONE;

    try
    {
        auto decoder = reinterpret_cast<VideoDECODEH264*>(pState);
        MFX_CHECK(decoder, MFX_ERR_UNDEFINED_BEHAVIOR);

        auto task =
            reinterpret_cast<ThreadTaskInfo264*>(pParam);
        MFX_CHECK(task, MFX_ERR_UNDEFINED_BEHAVIOR);

        sts = decoder->RunThread(task, threadNumber);
    }
    catch(...)
    {
        MFX_LTRACE_MSG_1(MFX_TRACE_LEVEL_INTERNAL, "exception handled");
        return MFX_ERR_NONE;
    }
    return sts;
}

static mfxStatus AVCCompleteProc(void *, void *pParam, mfxStatus )
{
    delete reinterpret_cast<ThreadTaskInfo264*>(pParam);
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH264::RunThread(ThreadTaskInfo264* info, mfxU32 threadNumber)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEH264::RunThread");

    MFX_CHECK_NULL_PTR1(info);

    mfxStatus sts = MFX_TASK_WORKING;

    if (!info->surface_out)
    {
        for (int32_t i = 0; i < 2 && sts == MFX_TASK_WORKING; i++)
        {
            sts = m_pH264VideoDecoder->RunThread(threadNumber);
        }

        UMC::AutomaticUMCMutex guard(m_mGuard);
        if (sts == MFX_TASK_BUSY && !m_pH264VideoDecoder->GetTaskBroker()->IsEnoughForStartDecoding(true))
            m_globalTask = false;

        return m_globalTask ? sts : MFX_TASK_DONE;
    }

    UMC::H264DecoderFrame * pFrame = 0;
    bool isDecoded;
    {
        UMC::AutomaticUMCMutex guard(m_mGuard);

        if (info->is_decoding_done)
            return MFX_TASK_DONE;

        mfxI32 index = m_surface_source->FindSurface(info->surface_out);
        pFrame = m_pH264VideoDecoder->FindSurface((UMC::FrameMemID)index);
        MFX_CHECK(pFrame && pFrame->m_UID != -1, MFX_ERR_NOT_FOUND);

        isDecoded = m_pH264VideoDecoder->CheckDecoding(pFrame);
    }

    if (!isDecoded)
    {
        for (int32_t i = 0; i < 2 && sts == MFX_TASK_WORKING; i++)
        {
            sts = m_pH264VideoDecoder->RunThread(threadNumber, pFrame);
        }
    }

    {
        UMC::AutomaticUMCMutex guard(m_mGuard);
        if (info->is_decoding_done)
            return MFX_TASK_DONE;

        isDecoded = m_pH264VideoDecoder->CheckDecoding(pFrame);
        if (isDecoded)
        {
            info->is_decoding_done = true;
        }
    }

    if (isDecoded)
    {
        if (!pFrame->wasDisplayed())
        {
            sts = DecodeFrame(nullptr, nullptr, info->surface_out);
            MFX_CHECK(sts == MFX_ERR_NONE || sts == MFX_ERR_NOT_FOUND, sts);

            return sts;
        }

        return MFX_TASK_DONE;
    }

    MFX_CHECK(sts >= MFX_ERR_NONE, sts);
    return sts;
}

mfxStatus VideoDECODEH264::DecodeFrameCheck(mfxBitstream *bs,
                                              mfxFrameSurface1 *surface_work,
                                              mfxFrameSurface1 **surface_out,
                                              MFX_ENTRY_POINT *pEntryPoint)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEH264::DecodeFrameCheck");

    UMC::AutomaticUMCMutex guard(m_mGuard);

    mfxStatus mfxSts = DecodeFrameCheck(bs, surface_work, surface_out);

    if (MFX_ERR_NONE == mfxSts || MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxSts) // It can be useful to run threads right after first frame receive
    {
        if (!*surface_out)
        {
            if (!m_globalTask && m_pH264VideoDecoder->GetTaskBroker()->IsEnoughForStartDecoding(true))
                m_globalTask = true;
            else
                return MFX_WRN_DEVICE_BUSY;
        }

        ThreadTaskInfo264* info = new ThreadTaskInfo264{ *surface_out };

        pEntryPoint->pRoutine           = &AVCDECODERoutine;
        pEntryPoint->pCompleteProc      = &AVCCompleteProc;
        pEntryPoint->pState             = this;
        pEntryPoint->requiredNumThreads = m_vPar.mfx.NumThread;
        pEntryPoint->pParam             = info;
    }

    MFX_CHECK_STS(mfxSts);
    return mfxSts;
}

mfxStatus VideoDECODEH264::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);
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

    sts = bs ? CheckBitstream(bs) : MFX_ERR_NONE;

    MFX_CHECK_STS(sts);

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
        sts = isVideoProcCscEnabled ? CheckFrameInfoDecVideoProcCsc(&surface_work->Info, MFX_CODEC_AVC) : CheckFrameInfoCodecs(&surface_work->Info, MFX_CODEC_AVC);
        MFX_CHECK(sts == MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM)

        sts = CheckFrameData(surface_work);
        MFX_CHECK_STS(sts);
    }

    sts = m_surface_source->SetCurrentMFXSurface(surface_work);
    MFX_CHECK_STS(sts);

    sts = MFX_ERR_UNDEFINED_BEHAVIOR;

#if defined(MFX_ENABLE_PROTECT)
#if defined(MFX_ENABLE_PXP)
    //Check protect VA is enabled or not
    if( bs && m_va->GetProtectedVA())
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
            ((mfxExtDecodeErrorReport *)extbuf)->ErrorTypes = 0;
            src.SetExtBuffer(extbuf);
        }

        m_pH264VideoDecoder->SetVideoCore(m_core);

        for (;;)
        {
            umcRes = m_pH264VideoDecoder->AddSource(bs ? &src : 0);

            umcFrameRes = umcRes;

            src.Save(bs);

            if (umcRes == UMC::UMC_ERR_ALLOC)
            {
                sts = MFX_ERR_MORE_SURFACE;
                break;
            }

            if (umcRes == UMC::UMC_NTF_NEW_RESOLUTION || umcRes == UMC::UMC_WRN_REPOSITION_INPROGRESS || umcRes == UMC::UMC_ERR_UNSUPPORTED)
            {
                FillVideoParam(&m_vPar, true);
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

                MFX_CHECK(umcRes != UMC::UMC_NTF_NEW_RESOLUTION, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            }

            if (umcRes == UMC::UMC_ERR_INVALID_STREAM)
            {
                umcFrameRes = umcRes = UMC::UMC_OK;
            }

            if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || umcRes == UMC::UMC_WRN_INFO_NOT_READY || umcRes == UMC::UMC_ERR_NEED_FORCE_OUTPUT)
            {
                force = umcRes == UMC::UMC_ERR_NEED_FORCE_OUTPUT;
                sts = umcRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER ? MFX_ERR_MORE_DATA_SUBMIT_TASK: MFX_WRN_DEVICE_BUSY;
            }

            if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA || umcRes == UMC::UMC_ERR_SYNC)
            {
                if (!bs || bs->DataFlag == MFX_BITSTREAM_EOS)
                    force = true;

                sts = MFX_ERR_MORE_DATA;
            }

            if (umcRes == UMC::UMC_ERR_DEVICE_FAILED || umcRes == UMC::UMC_ERR_GPU_HANG)
            {
                //return these errors immediatelly unless we have [input == 0]
                sts = (umcRes == UMC::UMC_ERR_DEVICE_FAILED) ? MFX_ERR_DEVICE_FAILED : MFX_ERR_GPU_HANG;
                if (bs && bs->DataFlag != MFX_BITSTREAM_EOS)
                    MFX_RETURN(sts);
                force = true;
            }

            umcRes = m_pH264VideoDecoder->RunDecoding();

            if (m_vInitPar.mfx.DecodedOrder)
                force = true;

            UMC::H264DecoderFrame *pFrame = nullptr;

#if defined(MFX_ENABLE_PROTECT)
#if defined(MFX_ENABLE_PXP)
            //PXP macro on && secure decode
            if (m_va->GetProtectedVA())
            {
                if (umcFrameRes != UMC::UMC_ERR_NOT_ENOUGH_BUFFER)
                {
                    pFrame = GetFrameToDisplay(force);
                }
            }
            // PXP macro on && clear decode
            else
#endif // MFX_ENABLE_PXP
#endif // MFX_ENABLE_PROTECT
                pFrame = GetFrameToDisplay(force);

            // return frame to display
            if (pFrame)
            {
                FillOutputSurface(surface_out, surface_work, pFrame);

                m_frameOrder = (mfxU16)pFrame->m_frameOrder;
                (*surface_out)->Data.FrameOrder = m_frameOrder;
                return MFX_ERR_NONE;
            }

            if (umcFrameRes != UMC::UMC_OK)
                break;

        } // for (;;)
    }
    catch(const UMC::h264_exception & ex)
    {
        FillVideoParam(&m_vPar, false);

        if (ex.GetStatus() == UMC::UMC_ERR_ALLOC)
        {
            // check incompatibility of video params
            MFX_CHECK(
                m_vInitPar.mfx.FrameInfo.Width  == m_vPar.mfx.FrameInfo.Width &&
                m_vInitPar.mfx.FrameInfo.Height == m_vPar.mfx.FrameInfo.Height,
                MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        }

        mfxStatus mfxSts = ConvertUMCStatusToMfx(ex.GetStatus());
        MFX_CHECK_STS(mfxSts);

        return mfxSts;
    }
    catch(...)
    {
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }

    MFX_CHECK_STS(sts);
    return sts;
}

void VideoDECODEH264::FillVideoParam(mfxVideoParamWrapper *par, bool full)
{
    if (!m_pH264VideoDecoder.get())
        return;

    MFX_Utility::FillVideoParam(m_pH264VideoDecoder.get(), par, full);

    UMC::RawHeader *sps = m_pH264VideoDecoder->GetSPS();
    UMC::RawHeader *pps = m_pH264VideoDecoder->GetPPS();

    mfxExtCodingOptionSPSPPS * spsPps = reinterpret_cast<mfxExtCodingOptionSPSPPS *>(GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS));
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

    MFX_Utility::FillVideoParamMVCEx(m_pH264VideoDecoder.get(), par);
}

void VideoDECODEH264::CopySurfaceInfo(mfxFrameSurface1 *in, mfxFrameSurface1 *out)
{
    out->Info.FrameId.ViewId = in->Info.FrameId.ViewId;
    out->Info.FrameId.TemporalId = in->Info.FrameId.TemporalId;
    out->Info.FrameId.PriorityId = in->Info.FrameId.PriorityId;

#ifdef MFX_ENABLE_SVC_VIDEO_DECODE
    if (IsSVCProfile(m_vFirstPar.mfx.CodecProfile))
    {
        out->Info.FrameId.DependencyId = in->Info.FrameId.DependencyId;
        out->Info.FrameId.TemporalId = in->Info.FrameId.TemporalId;
        out->Info.FrameId.QualityId = in->Info.FrameId.QualityId;
        out->Info.FrameId.PriorityId = in->Info.FrameId.PriorityId;
    }
#endif

    out->Info.CropH = in->Info.CropH;
    out->Info.CropW = in->Info.CropW;
    out->Info.CropX = in->Info.CropX;
    out->Info.CropY = in->Info.CropY;

    out->Info.AspectRatioH = in->Info.AspectRatioH;
    out->Info.AspectRatioW = in->Info.AspectRatioW;

    out->Info.FrameRateExtD = in->Info.FrameRateExtD;
    out->Info.FrameRateExtN = in->Info.FrameRateExtN;
    out->Info.PicStruct = in->Info.PicStruct;

    out->Data.TimeStamp = in->Data.TimeStamp;
    out->Data.FrameOrder = in->Data.FrameOrder;
    out->Data.Corrupted = in->Data.Corrupted;
    out->Data.DataFlag = in->Data.DataFlag;
}

void VideoDECODEH264::FillOutputSurface(mfxFrameSurface1 **surf_out, mfxFrameSurface1 *surface_work, UMC::H264DecoderFrame * pFrame)
{
    m_stat.NumFrame++;
    m_stat.NumError += pFrame->GetError() ? 1 : 0;
    const UMC::FrameData * fd = pFrame->GetFrameData();

    *surf_out = m_surface_source->GetSurface(fd->GetFrameMID(), surface_work, &m_vPar);

    assert(*surf_out);

    mfxFrameSurface1 *surface_out = *surf_out;

    mfxExtDecodedFrameInfo * frameType = (mfxExtDecodedFrameInfo *)GetExtendedBuffer(surface_out->Data.ExtParam, surface_out->Data.NumExtParam, MFX_EXTBUFF_DECODED_FRAME_INFO);
    if (frameType)
    {
        if (pFrame->GetAU(0)->IsIntraAU())
        {
            frameType->FrameType = MFX_FRAMETYPE_I;
            if (pFrame->GetAU(0)->m_IsIDR)
                frameType->FrameType |= MFX_FRAMETYPE_IDR;
        }
        else if (pFrame->GetAU(0)->m_isBExist)
        {
            frameType->FrameType = MFX_FRAMETYPE_B;
        }
        else
            frameType->FrameType = MFX_FRAMETYPE_P;

        if (pFrame->GetAU(0)->IsReference())
            frameType->FrameType |= MFX_FRAMETYPE_REF;

        if (pFrame->GetAU(1)->GetStatus() > UMC::H264DecoderFrameInfo::STATUS_NOT_FILLED)
        {
            if (pFrame->GetAU(1)->IsIntraAU())
            {
                frameType->FrameType |= MFX_FRAMETYPE_xI;
                if (pFrame->GetAU(1)->m_IsIDR)
                    frameType->FrameType |= MFX_FRAMETYPE_xIDR;
            }
            else if (pFrame->GetAU(1)->m_isBExist)
            {
                frameType->FrameType |= MFX_FRAMETYPE_xB;
            }
            else
            {
                frameType->FrameType |= MFX_FRAMETYPE_xP;
            }

            if (pFrame->GetAU(1)->IsReference())
                frameType->FrameType |= MFX_FRAMETYPE_xREF;

        }
    }

    surface_out->Info.FrameId.ViewId = (mfxU16)pFrame->m_viewId;
    surface_out->Info.FrameId.TemporalId = 0;

#ifdef MFX_ENABLE_SVC_VIDEO_DECODE
    if (IsSVCProfile(m_vFirstPar.mfx.CodecProfile))
    {
        surface_out->Info.FrameId.DependencyId = (mfxU16)0;
        surface_out->Info.FrameId.QualityId = (mfxU16)0;
        surface_out->Info.FrameId.TemporalId = 0;
    }
#endif

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(m_vFirstPar.ExtParam, m_vFirstPar.NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    if (videoProcessing)
    {
        surface_out->Info.CropH = videoProcessing->Out.CropH;
        surface_out->Info.CropW = videoProcessing->Out.CropW;
        surface_out->Info.CropX = videoProcessing->Out.CropX;
        surface_out->Info.CropY = videoProcessing->Out.CropY;
        surface_out->Info.ChromaFormat = videoProcessing->Out.ChromaFormat;
    } else
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
        case 2:
            surface_out->Info.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            break;
        default:
            surface_out->Info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            break;
        }
    }

    bool isShouldUpdate = !(m_vFirstPar.mfx.FrameInfo.AspectRatioH || m_vFirstPar.mfx.FrameInfo.AspectRatioW);

    surface_out->Info.AspectRatioH = isShouldUpdate ? (mfxU16)pFrame->m_aspect_height : m_vFirstPar.mfx.FrameInfo.AspectRatioH;
    surface_out->Info.AspectRatioW = isShouldUpdate ? (mfxU16)pFrame->m_aspect_width : m_vFirstPar.mfx.FrameInfo.AspectRatioW;

    isShouldUpdate = !(m_vFirstPar.mfx.FrameInfo.FrameRateExtD || m_vFirstPar.mfx.FrameInfo.FrameRateExtN);

    surface_out->Info.FrameRateExtD = isShouldUpdate ? m_vPar.mfx.FrameInfo.FrameRateExtD : m_vFirstPar.mfx.FrameInfo.FrameRateExtD;
    surface_out->Info.FrameRateExtN = isShouldUpdate ? m_vPar.mfx.FrameInfo.FrameRateExtN : m_vFirstPar.mfx.FrameInfo.FrameRateExtN;

    surface_out->Info.PicStruct = 0;


    switch (pFrame->m_displayPictureStruct)
    {
    case UMC::DPS_TOP:
        surface_out->Info.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
        break;

    case UMC::DPS_BOTTOM:
        surface_out->Info.PicStruct = MFX_PICSTRUCT_FIELD_BFF;
        break;

    case UMC::DPS_TOP_BOTTOM:
    case UMC::DPS_BOTTOM_TOP:
        {
            mfxU32 fieldFlag = (pFrame->m_displayPictureStruct == UMC::DPS_TOP_BOTTOM) ? MFX_PICSTRUCT_FIELD_TFF : MFX_PICSTRUCT_FIELD_BFF;
            surface_out->Info.PicStruct = (mfxU16)((pFrame->m_PictureStructureForDec == UMC::FRM_STRUCTURE) ? MFX_PICSTRUCT_PROGRESSIVE : fieldFlag);

            if (m_vPar.mfx.ExtendedPicStruct)
            {
                surface_out->Info.PicStruct |= fieldFlag;
            }
        }
        break;

    case UMC::DPS_TOP_BOTTOM_TOP:
    case UMC::DPS_BOTTOM_TOP_BOTTOM:
        {
            surface_out->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

            if (m_vPar.mfx.ExtendedPicStruct)
            {
                surface_out->Info.PicStruct |= MFX_PICSTRUCT_FIELD_REPEATED;
                surface_out->Info.PicStruct |= (pFrame->m_displayPictureStruct == UMC::DPS_TOP_BOTTOM_TOP) ? MFX_PICSTRUCT_FIELD_TFF : MFX_PICSTRUCT_FIELD_BFF;
            }
        }
        break;

    case UMC::DPS_FRAME_DOUBLING:
        surface_out->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        if (m_vPar.mfx.ExtendedPicStruct)
        {
            surface_out->Info.PicStruct |= MFX_PICSTRUCT_FRAME_DOUBLING;
        }
        break;

    case UMC::DPS_FRAME_TRIPLING:
        surface_out->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        if (m_vPar.mfx.ExtendedPicStruct)
        {
            surface_out->Info.PicStruct |= MFX_PICSTRUCT_FRAME_TRIPLING;
        }
        break;

    case UMC::DPS_FRAME:
    default:
        surface_out->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        break;
    }

    surface_out->Data.TimeStamp = GetMfxTimeStamp(pFrame->m_dFrameTime);
    surface_out->Data.FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;

    surface_out->Data.DataFlag = (mfxU16)(pFrame->m_isOriginalPTS ? MFX_FRAMEDATA_ORIGINAL_TIMESTAMP : 0);

    TRACE_BUFFER_EVENT(MFX_TRACE_API_AVC_OUTPUTINFO_TASK, EVENT_TYPE_INFO, TR_KEY_DECODE_BASIC_INFO,
        surface_out, H264DecodeSurfaceOutparam, SURFACEOUT_H264D);

    UMC::SEI_Storer * storer = m_pH264VideoDecoder->GetSEIStorer();
    if (storer)
        storer->SetTimestamp(pFrame);
}

mfxStatus VideoDECODEH264::DecodeFrame(mfxFrameSurface1 *surface_out, UMC::H264DecoderFrame * pFrame)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VideoDECODEH264::DecodeFrame");
    MFX_CHECK_NULL_PTR1(surface_out);

    mfxI32 index;
    if (pFrame)
    {
        index = pFrame->GetFrameData()->GetFrameMID();
    }
    else
    {
        index = m_surface_source->FindSurface(surface_out);
        pFrame = m_pH264VideoDecoder->FindSurface((UMC::FrameMemID)index);
        MFX_CHECK(pFrame, MFX_ERR_NOT_FOUND);
    }

    int32_t const error = pFrame->GetError();
    if (error & UMC::ERROR_FRAME_DEVICE_FAILURE)
    {
        MFX_CHECK(error != UMC::UMC_ERR_GPU_HANG, MFX_ERR_DEVICE_FAILED);
        MFX_RETURN(MFX_ERR_GPU_HANG);
    }

    surface_out->Data.Corrupted = 0;
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

    mfxStatus sts = m_surface_source->PrepareToOutput(surface_out, index, &m_vPar);
    MFX_CHECK_STS(sts);

    UMC::AutomaticUMCMutex guard(m_mGuard);
    pFrame->setWasDisplayed();

    TRACE_EVENT(MFX_TRACE_API_AVC_DISPLAYINFO_TASK, EVENT_TYPE_INFO, TR_KEY_DECODE_BASIC_INFO, make_event_data(
        pFrame->m_PicOrderCnt[0], pFrame->m_PicOrderCnt[1], pFrame->FrameNum(), (uint32_t)pFrame->wasDisplayed(), (uint32_t)pFrame->wasOutputted()));

    return sts;
}

mfxStatus VideoDECODEH264::DecodeFrame(mfxBitstream *, mfxFrameSurface1 *, mfxFrameSurface1 *surface_out)
{
    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(surface_out);

    mfxStatus sts = DecodeFrame(surface_out);

    MFX_CHECK_STS(sts);
    return sts;
}

mfxStatus VideoDECODEH264::GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts)
{
    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR3(ud, sz, ts);

    mfxStatus       MFXSts = MFX_ERR_NONE;

    UMC::MediaData data;
    UMC::Status umcRes = m_pH264VideoDecoder->GetUserData(&data);

    MFX_CHECK(umcRes != UMC::UMC_ERR_NOT_ENOUGH_DATA, MFX_ERR_MORE_DATA);

    MFX_CHECK(*sz >= data.GetDataSize(), MFX_ERR_NOT_ENOUGH_BUFFER);

    *sz = (mfxU32)data.GetDataSize();
    *ts = GetMfxTimeStamp(data.GetTime());

    std::copy(reinterpret_cast<mfxU8 *>(data.GetDataPointer()), reinterpret_cast<mfxU8 *>(data.GetDataPointer()) + data.GetDataSize(), ud);

    MFX_CHECK_STS(MFXSts);
    return MFXSts;
}

mfxStatus VideoDECODEH264::GetPayload( mfxU64 *ts, mfxPayload *payload )
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    TRACE_EVENT(MFX_TRACE_API_DECODE_GETPAYLOAD_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(payload));

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR3(ts, payload, payload->Data);

    UMC::SEI_Storer * storer = m_pH264VideoDecoder->GetSEIStorer();

    MFX_CHECK(storer, MFX_ERR_UNKNOWN);

    const UMC::SEI_Storer::SEI_Message * msg = storer->GetPayloadMessage();

    if (msg)
    {
        MFX_CHECK(payload->BufSize >= msg->msg_size, MFX_ERR_NOT_ENOUGH_BUFFER);

        *ts = GetMfxTimeStamp(msg->timestamp);

        std::copy(msg->data, msg->data + msg->msg_size, payload->Data);

        payload->NumBit = (mfxU32)(msg->msg_size * 8);
        payload->Type = (mfxU16)msg->type;
    }
    else
    {
        payload->NumBit = 0;
        *ts = MFX_TIME_STAMP_INVALID;
    }

    TRACE_EVENT(MFX_TRACE_API_DECODE_GETPAYLOAD_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(MFX_ERR_NONE));

    return MFX_ERR_NONE;
}

UMC::H264DecoderFrame * VideoDECODEH264::GetFrameToDisplay(bool force)
{
    UMC::H264DecoderFrame * pFrame = 0;
    do
    {
        pFrame = m_pH264VideoDecoder->GetFrameToDisplayInternal(force);
        if (!pFrame)
        {
            break;
        }

        m_pH264VideoDecoder->PostProcessDisplayFrame(pFrame);

        if (pFrame->IsSkipped())
        {
            pFrame->setWasOutputted();
            pFrame->setWasDisplayed();
        }
    } while (pFrame->IsSkipped());

    if (pFrame)
    {
        pFrame->setWasOutputted();
    }

    return pFrame;
}

mfxStatus VideoDECODEH264::SetSkipMode(mfxSkipMode mode)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    TRACE_EVENT(MFX_TRACE_API_DECODE_SETSKIPMODE_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(mode));

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    int32_t test_num = 0;
    m_pH264VideoDecoder->ChangeVideoDecodingSpeed(test_num);

    int32_t num = 0;
    switch (mode)
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
            {
                MFX_RETURN(MFX_ERR_UNSUPPORTED);
            }
    }

    m_pH264VideoDecoder->ChangeVideoDecodingSpeed(num);

    TRACE_EVENT(MFX_TRACE_API_DECODE_SETSKIPMODE_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(num ? MFX_WRN_VALUE_NOT_CHANGED : MFX_ERR_NONE));

    return
        test_num == num ? MFX_WRN_VALUE_NOT_CHANGED : MFX_ERR_NONE;
}

bool VideoDECODEH264::IsSameVideoParam(mfxVideoParam * newPar, mfxVideoParam * oldPar, eMFXHWType type)
{
    auto const mask =
          MFX_IOPATTERN_OUT_SYSTEM_MEMORY
        | MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    if ((newPar->IOPattern & mask) !=
        (oldPar->IOPattern & mask) )
    {
        return false;
    }

    if (newPar->Protected != oldPar->Protected)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.FourCC != oldPar->mfx.FrameInfo.FourCC)
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

    if (newPar->mfx.FrameInfo.FourCC != oldPar->mfx.FrameInfo.FourCC)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.ChromaFormat != oldPar->mfx.FrameInfo.ChromaFormat)
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

    //420<->400 allowed, other chroma formats are unsupported
    /*if (newPar->mfx.FrameInfo.ChromaFormat != oldPar->mfx.FrameInfo.ChromaFormat)
    {
        if ((newPar->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 && newPar->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV400) ||
            (oldPar->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 && oldPar->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV400))
        return false;
    }*/

    if (CalculateRequiredView(newPar) != CalculateRequiredView(oldPar))
        return false;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * newVideoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(newPar->ExtParam, newPar->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    mfxExtDecVideoProcessing * oldVideoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(oldPar->ExtParam, oldPar->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);

    if ( ((newVideoProcessing) && (!oldVideoProcessing)) ||
         ((!newVideoProcessing) && (oldVideoProcessing)) )
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
             (newVideoProcessing->In.CropH <= newPar->mfx.FrameInfo.CropH) ))
            return false;

        /* Check output cropping */
        if (!((newVideoProcessing->Out.CropX <= newVideoProcessing->Out.CropW) &&
             (newVideoProcessing->Out.CropW <= newVideoProcessing->Out.Width) &&
             ((newVideoProcessing->Out.CropX + newVideoProcessing->Out.CropH)
                                                <= newVideoProcessing->Out.Width) &&
             (newVideoProcessing->Out.CropY <= newVideoProcessing->Out.CropH) &&
             (newVideoProcessing->Out.CropH <= newVideoProcessing->Out.Height) &&
             ((newVideoProcessing->Out.CropY + newVideoProcessing->Out.CropH )
                                                 <= newVideoProcessing->Out.Height) ))
            return false;

    }
#endif //MFX_DEC_VIDEO_POSTPROCESS_DISABLE

    return true;
}

mfxFrameSurface1 *VideoDECODEH264::GetInternalSurface(mfxFrameSurface1 *surface)
{
    return m_surface_source->GetInternalSurface(surface);
}

#endif // MFX_ENABLE_H264_VIDEO_DECODE
