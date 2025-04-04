// Copyright (c) 2011-2021 Intel Corporation
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

#if defined (MFX_ENABLE_VPP)

#include <math.h>
#include "mfx_vpp_defs.h"
#include "mfx_vpp_vaapi.h"
#include "mfx_utils.h"
#include "libmfx_core_vaapi.h"
#include "ippcore.h"
#include "ippi.h"
#include <algorithm>

template<typename T>
bool SetPlaneROI(T value, T* pDst, int dstStep, IppiSize roiSize)
{
    if (!pDst || roiSize.width < 0 || roiSize.height < 0)
        return false;

    for(int h = 0; h < roiSize.height; h++ ) {
        std::fill(pDst, pDst + roiSize.width, value);
        pDst = (T *)((unsigned char*)pDst + dstStep);
    }

    return true;
}

enum QueryStatus
{
    VPREP_GPU_READY         =   0,
    VPREP_GPU_BUSY          =   1,
    VPREP_GPU_NOT_REACHED   =   2,
    VPREP_GPU_FAILED        =   3
};

//////////////////////////////////////////////////////////////////////////
using namespace MfxHwVideoProcessing;

static float convertValue(const float OldMin,const float OldMax,const float NewMin,const float NewMax,const float OldValue)
{
    if((0 == NewMin) && (0 == NewMax)) return OldValue; //workaround
    float oldRange = OldMax - OldMin;
    float newRange = NewMax - NewMin;
    return (((OldValue - OldMin) * newRange) / oldRange) + NewMin;
}

#define DEFAULT_HUE 0
#define DEFAULT_SATURATION 1
#define DEFAULT_CONTRAST 1
#define DEFAULT_BRIGHTNESS 0

#define VA_TOP_FIELD_WEAVE        0x00000002
#define VA_BOTTOM_FIELD_WEAVE     0x00000004

#define VPP_COMP_BACKGROUND_SURFACE_WIDTH  320
#define VPP_COMP_BACKGROUND_SURFACE_HEIGHT 256

VAAPIVideoProcessing::VAAPIVideoProcessing():
  m_bRunning(false)
, m_core(NULL)
, m_vaDisplay(0)
, m_vaConfig(VA_INVALID_ID)
, m_vaContextVPP(VA_INVALID_ID)
, m_denoiseFilterID(VA_INVALID_ID)
, m_detailFilterID(VA_INVALID_ID)
, m_deintFilterID(VA_INVALID_ID)
, m_procampFilterID(VA_INVALID_ID)
, m_frcFilterID(VA_INVALID_ID)
, m_deintFrameCount(0)
#ifdef MFX_ENABLE_VPP_FRC
, m_frcCyclicCounter(0)
#endif
, m_numFilterBufs(0)
, m_primarySurface4Composition(NULL)
, m_3dlutFilterID(VA_INVALID_ID)
, m_hvsDenoiseFilterID(VA_INVALID_ID)
, m_hdrtmFilterID(VA_INVALID_ID)
{

    for(int i = 0; i < VAProcFilterCount; i++)
        m_filterBufs[i] = VA_INVALID_ID;

    memset( (void*)&m_pipelineCaps, 0, sizeof(m_pipelineCaps));
    memset( (void*)&m_denoiseCaps, 0, sizeof(m_denoiseCaps));
    memset( (void*)&m_detailCaps, 0, sizeof(m_detailCaps));
    memset( (void*)&m_deinterlacingCaps, 0, sizeof(m_deinterlacingCaps));
#ifdef MFX_ENABLE_VPP_FRC
    memset( (void*)&m_frcCaps, 0, sizeof(m_frcCaps));
#endif
    memset( (void*)&m_procampCaps, 0, sizeof(m_procampCaps));
    memset( (void*)&m_hdrtm_caps, 0, sizeof(m_hdrtm_caps));

    m_cachedReadyTaskIndex.clear();
    m_feedbackCache.clear();

} // VAAPIVideoProcessing::VAAPIVideoProcessing()


VAAPIVideoProcessing::~VAAPIVideoProcessing()
{
    Close();

} // VAAPIVideoProcessing::~VAAPIVideoProcessing()

mfxStatus VAAPIVideoProcessing::CreateDevice(VideoCORE * core, mfxVideoParam* pParams, bool /*isTemporal*/)
{
    MFX_CHECK_NULL_PTR1(core);

    VAAPIVideoCORE_VPL* vaapi_core_vpl = reinterpret_cast<VAAPIVideoCORE_VPL*>(core->QueryCoreInterface(MFXIVAAPIVideoCORE_VPL_GUID));
    MFX_CHECK_NULL_PTR1(vaapi_core_vpl);

    MFX_SAFE_CALL(vaapi_core_vpl->GetVAService(&m_vaDisplay));

    MFX_SAFE_CALL(Init(&m_vaDisplay, pParams));

    m_cachedReadyTaskIndex.clear();

    m_core = core;

    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoProcessing::CreateDevice(VideoCORE * core, mfxInitParams* pParams)


mfxStatus VAAPIVideoProcessing::DestroyDevice(void)
{
    mfxStatus sts = Close();

    return sts;

} // mfxStatus VAAPIVideoProcessing::DestroyDevice(void)


mfxStatus VAAPIVideoProcessing::Close(void)
{
    VAStatus vaSts;
    if (m_primarySurface4Composition != NULL)
    {
        vaSts = vaDestroySurfaces(m_vaDisplay,m_primarySurface4Composition,1);
        std::ignore = MFX_STS_TRACE(vaSts);

        free(m_primarySurface4Composition);
        m_primarySurface4Composition = NULL;
    }

    mfxStatus sts = CheckAndDestroyVAbuffer(m_vaDisplay, m_denoiseFilterID);
    std::ignore = MFX_STS_TRACE(sts);

    sts = CheckAndDestroyVAbuffer(m_vaDisplay, m_detailFilterID);
    std::ignore = MFX_STS_TRACE(sts);

    sts = CheckAndDestroyVAbuffer(m_vaDisplay, m_procampFilterID);
    std::ignore = MFX_STS_TRACE(sts);

    sts = CheckAndDestroyVAbuffer(m_vaDisplay, m_deintFilterID);
    std::ignore = MFX_STS_TRACE(sts);

    sts = CheckAndDestroyVAbuffer(m_vaDisplay, m_frcFilterID);
    std::ignore = MFX_STS_TRACE(sts);

    sts = CheckAndDestroyVAbuffer(m_vaDisplay, m_3dlutFilterID);
    std::ignore = MFX_STS_TRACE(sts);

    sts = CheckAndDestroyVAbuffer(m_vaDisplay, m_hvsDenoiseFilterID);
    std::ignore = MFX_STS_TRACE(sts);

    sts = CheckAndDestroyVAbuffer(m_vaDisplay, m_hdrtmFilterID);
    std::ignore = MFX_STS_TRACE(sts);

    if (m_vaContextVPP != VA_INVALID_ID)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaDestroyContext");
        vaSts = vaDestroyContext( m_vaDisplay, m_vaContextVPP );
        std::ignore = MFX_STS_TRACE(vaSts);

        m_vaContextVPP = VA_INVALID_ID;
    }

    if (m_vaConfig != VA_INVALID_ID)
    {
        vaSts = vaDestroyConfig( m_vaDisplay, m_vaConfig );
        std::ignore = MFX_STS_TRACE(vaSts);

        m_vaConfig = VA_INVALID_ID;
    }

    for(int i = 0; i < VAProcFilterCount; i++)
        m_filterBufs[i] = VA_INVALID_ID;

    m_denoiseFilterID   = VA_INVALID_ID;
    m_deintFilterID     = VA_INVALID_ID;
    m_procampFilterID   = VA_INVALID_ID;

    m_3dlutFilterID     = VA_INVALID_ID;
    m_hvsDenoiseFilterID= VA_INVALID_ID;
    m_hdrtmFilterID     = VA_INVALID_ID;

    memset( (void*)&m_pipelineCaps, 0, sizeof(m_pipelineCaps));
    memset( (void*)&m_denoiseCaps, 0, sizeof(m_denoiseCaps));
    memset( (void*)&m_detailCaps, 0, sizeof(m_detailCaps));
    memset( (void*)&m_procampCaps,  0, sizeof(m_procampCaps));
    memset( (void*)&m_deinterlacingCaps, 0, sizeof(m_deinterlacingCaps));
    memset( (void*)&m_hdrtm_caps,  0, sizeof(m_hdrtm_caps));

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoProcessing::Close(void)

mfxStatus VAAPIVideoProcessing::Init(_mfxPlatformAccelerationService* pVADisplay, mfxVideoParam* pParams)
{
    if(false == m_bRunning)
    {
        MFX_CHECK_NULL_PTR1( pVADisplay );
        MFX_CHECK_NULL_PTR1( pParams );

        m_cachedReadyTaskIndex.clear();

        int va_max_num_entrypoints   = vaMaxNumEntrypoints(m_vaDisplay);
        MFX_CHECK(va_max_num_entrypoints, MFX_ERR_DEVICE_FAILED);

        std::unique_ptr<VAEntrypoint[]> va_entrypoints(new VAEntrypoint[va_max_num_entrypoints]);
        mfxI32 entrypointsCount = 0, entrypointsIndx = 0;

        VAStatus vaSts = vaQueryConfigEntrypoints(m_vaDisplay,
                                         VAProfileNone,
                                         va_entrypoints.get(),
                                         &entrypointsCount);
        MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        for( entrypointsIndx = 0; entrypointsIndx < entrypointsCount; entrypointsIndx++ )
        {
            if( VAEntrypointVideoProc == va_entrypoints[entrypointsIndx] )
            {
                m_bRunning = true;
                break;
            }
        }

        if( !m_bRunning )
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        vaSts = vaCreateConfig( m_vaDisplay,
                                VAProfileNone,
                                VAEntrypointVideoProc,
                                NULL,
                                0,
                                &m_vaConfig);
        MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        // Context
        int width = pParams->vpp.Out.Width;
        int height = pParams->vpp.Out.Height;

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaCreateContext");
            vaSts = vaCreateContext(m_vaDisplay,
                                    m_vaConfig,
                                    width,
                                    height,
                                    VA_PROGRESSIVE,
                                    0, 0,
                                    &m_vaContextVPP);
        }
        MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoProcessing::Init(_mfxPlatformAccelerationService* pVADisplay, mfxVideoParam* pParams)


mfxStatus VAAPIVideoProcessing::Register(
    mfxHDLPair* /*pSurfaces*/,
    mfxU32 /*num*/,
    BOOL /*bRegister*/)
{
    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoProcessing::Register(_mfxPlatformVideoSurface *pSurfaces, ...)


mfxStatus VAAPIVideoProcessing::QueryCapabilities(mfxVppCaps& caps)
{
    VAStatus vaSts;

    VAProcFilterType filters[VAProcFilterCount];
    mfxU32 num_filters = VAProcFilterCount;

    vaSts = vaQueryVideoProcFilters(m_vaDisplay, m_vaContextVPP, filters, &num_filters);
    MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU32 num_procamp_caps = VAProcColorBalanceCount;
    vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                               m_vaContextVPP,
                               VAProcFilterColorBalance,
                               &m_procampCaps, &num_procamp_caps);
    MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU32 num_denoise_caps = 1;
    vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                               m_vaContextVPP,
                               VAProcFilterNoiseReduction,
                               &m_denoiseCaps, &num_denoise_caps);
    MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU32 num_detail_caps = 1;
    vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                               m_vaContextVPP,
                               VAProcFilterSharpening,
                               &m_detailCaps, &num_detail_caps);
    MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU32 num_deinterlacing_caps = VAProcDeinterlacingCount;
    vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                               m_vaContextVPP,
                               VAProcFilterDeinterlacing,
                               &m_deinterlacingCaps, &num_deinterlacing_caps);
    MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

#ifdef MFX_ENABLE_VPP_FRC
    /* to check is FRC enabled or not*/
    /* first need to get number of modes supported by driver*/
    VAProcFilterCapFrameRateConversion tempFRC_Caps;
    mfxU32 num_frc_caps = 1;

    tempFRC_Caps.bget_custom_rates = 1;
    vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                               m_vaContextVPP,
                               VAProcFilterFrameRateConversion,
                               &tempFRC_Caps, &num_frc_caps);
    MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if (0 != tempFRC_Caps.frc_custom_rates) /* FRC is enabled, at least one mode */
    {
        caps.uFrameRateConversion = 1 ;
        /* Again, only two modes: 24p->60p and 30p->60p is available
         * But driver report 3, but 3rd is equal to 2rd,
         * So only 2 real modes*/
        if (tempFRC_Caps.frc_custom_rates > 2)
            tempFRC_Caps.frc_custom_rates = 2;
        caps.frcCaps.customRateData.resize(tempFRC_Caps.frc_custom_rates);
        /*to get details about each mode */
        tempFRC_Caps.bget_custom_rates = 0;

        for (mfxU32 ii = 0; ii < tempFRC_Caps.frc_custom_rates; ii++)
        {
            m_frcCaps[ii].frc_custom_rates = ii + 1;
            vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                                                m_vaContextVPP,
                                                VAProcFilterFrameRateConversion,
                                                &m_frcCaps[ii], &num_frc_caps);
            MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            caps.frcCaps.customRateData[ii].inputFramesOrFieldPerCycle = m_frcCaps[ii].input_frames;
            caps.frcCaps.customRateData[ii].outputIndexCountPerCycle = m_frcCaps[ii].output_frames;
            /* out frame rate*/
            caps.frcCaps.customRateData[ii].customRate.FrameRateExtN = m_frcCaps[ii].output_fps;
            /*input frame rate */
            caps.frcCaps.customRateData[ii].customRate.FrameRateExtD = m_frcCaps[ii].input_fps;
        }
    }
#endif // #ifdef MFX_ENABLE_VPP_FRC

    for( mfxU32 filtersIndx = 0; filtersIndx < num_filters; filtersIndx++ )
    {
        if (filters[filtersIndx])
        {
            switch (filters[filtersIndx])
            {
                case VAProcFilterNoiseReduction:
                    caps.uDenoiseFilter = 1;
                    break;
                case VAProcFilterSharpening:
                    caps.uDetailFilter = 1;
                    break;
                case VAProcFilterDeinterlacing:
                    for (mfxU32 i = 0; i < num_deinterlacing_caps; i++)
                    {
                        if (VAProcDeinterlacingBob == m_deinterlacingCaps[i].type)
                            caps.uSimpleDI = 1;
                        if (VAProcDeinterlacingWeave == m_deinterlacingCaps[i].type           ||
                            VAProcDeinterlacingMotionAdaptive == m_deinterlacingCaps[i].type  ||
                            VAProcDeinterlacingMotionCompensated == m_deinterlacingCaps[i].type)
                            caps.uAdvancedDI = 1;
                    }
                    break;
                case VAProcFilterColorBalance:
                    caps.uProcampFilter = 1;
                    break;
                case VAProcFilter3DLUT:
                    caps.u3DLut = 1;
                    break;
                case VAProcFilterHVSNoiseReduction:
                    caps.uDenoise2Filter= 1;
                    break;
                case VAProcFilterHighDynamicRangeToneMapping:
                    caps.uHdr10ToneMapping = 1;
                    break;
                default:
                    break;
            }
        }
    }

    if (caps.u3DLut == 1){
        mfxU32 num_3dlut_caps = 0;
        vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                                           m_vaContextVPP,
                                           VAProcFilter3DLUT,
                                           (void*)m_3dlutCaps.data(),
                                           &num_3dlut_caps);
        MFX_CHECK(((VA_STATUS_SUCCESS == vaSts) || (VA_STATUS_ERROR_MAX_NUM_EXCEEDED == vaSts)), MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(num_3dlut_caps != 0, MFX_ERR_UNSUPPORTED);
        m_3dlutCaps.resize(num_3dlut_caps);
        vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                                m_vaContextVPP,
                                VAProcFilter3DLUT,
                                (void*)m_3dlutCaps.data(),
                                &num_3dlut_caps);
        MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    if (caps.uDenoise2Filter)
    {
        mfxU32 num_denoise2_caps = 0;
        vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                                m_vaContextVPP,
                                VAProcFilterHVSNoiseReduction,
                                nullptr,
                                &num_denoise2_caps);
        MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(num_denoise2_caps != 0, MFX_ERR_UNSUPPORTED);
    }

    if (caps.uHdr10ToneMapping)
    {
        mfxU32 num_hdrtm_caps = VAProcHighDynamicRangeMetadataTypeCount;
        vaSts = vaQueryVideoProcFilterCaps(m_vaDisplay,
                                m_vaContextVPP,
                                VAProcFilterHighDynamicRangeToneMapping,
                                &m_hdrtm_caps, &num_hdrtm_caps);
        MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(num_hdrtm_caps != 0, MFX_ERR_UNSUPPORTED);
    }

    memset(&m_pipelineCaps,  0, sizeof(VAProcPipelineCaps));
    vaSts = vaQueryVideoProcPipelineCaps(m_vaDisplay,
                                 m_vaContextVPP,
                                 NULL,
                                 0,
                                 &m_pipelineCaps);
#ifdef MFX_ENABLE_VPP_ROTATION
    MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    if (m_pipelineCaps.rotation_flags & (1 << VA_ROTATION_90 ) &&
        m_pipelineCaps.rotation_flags & (1 << VA_ROTATION_180) &&
        m_pipelineCaps.rotation_flags & (1 << VA_ROTATION_270))
    {
        caps.uRotation = 1;
    }
#endif

    if (m_pipelineCaps.max_output_width && m_pipelineCaps.max_output_height)
    {
        caps.uMaxWidth = m_pipelineCaps.max_output_width;
        caps.uMaxHeight = m_pipelineCaps.max_output_height;
        caps.uMinWidth = m_pipelineCaps.min_output_width;
        caps.uMinHeight = m_pipelineCaps.min_output_height;
    }
    else
    {
        caps.uMaxWidth = 4096;
        caps.uMaxHeight = 4096;
        caps.uMinWidth = 16;
        caps.uMinHeight = 16;
    }

#if defined (MFX_ENABLE_MJPEG_WEAVE_DI_VPP)
    caps.uFieldWeavingControl = 1;
#else
    caps.uFieldWeavingControl = 0;
#endif

    // [FourCC]
    // should be changed by libva support
    for (auto fourcc : g_TABLE_SUPPORTED_FOURCC)
    {
        // Mark supported input
        switch(fourcc)
        {
        case MFX_FOURCC_NV12:
        case MFX_FOURCC_YV12:
        case MFX_FOURCC_YUY2:
        case MFX_FOURCC_UYVY:
        case MFX_FOURCC_RGB4:
        case MFX_FOURCC_BGR4:
#if defined (MFX_ENABLE_FOURCC_RGB565)
        case MFX_FOURCC_RGB565:
#endif
#ifdef MFX_ENABLE_RGBP
        case MFX_FOURCC_RGBP:
#endif
        case MFX_FOURCC_BGRP:
        case MFX_FOURCC_AYUV:
        case MFX_FOURCC_Y210:
        case MFX_FOURCC_Y410:
        case MFX_FOURCC_P016:
        case MFX_FOURCC_Y216:
        case MFX_FOURCC_Y416:
        case MFX_FOURCC_P010:
        // A2RGB10 supported as input in case of passthru copy
        case MFX_FOURCC_A2RGB10:
        case MFX_FOURCC_I420:
            caps.mFormatSupport[fourcc] |= MFX_FORMAT_SUPPORT_INPUT;
            break;
        default:
            break;
        }

        // Mark supported output
        switch(fourcc)
        {
        case MFX_FOURCC_NV12:
        case MFX_FOURCC_YV12:
        case MFX_FOURCC_YUY2:
        case MFX_FOURCC_UYVY:
        case MFX_FOURCC_RGB4:
        case MFX_FOURCC_BGR4:
        case MFX_FOURCC_A2RGB10:
        case MFX_FOURCC_AYUV:
        case MFX_FOURCC_Y210:
        case MFX_FOURCC_Y410:
#ifdef MFX_ENABLE_RGBP
        case MFX_FOURCC_RGBP:
#endif
        case MFX_FOURCC_BGRP:
        case MFX_FOURCC_P016:
        case MFX_FOURCC_Y216:
        case MFX_FOURCC_Y416:
        case MFX_FOURCC_P010:
        case MFX_FOURCC_I420:
            caps.mFormatSupport[fourcc] |= MFX_FORMAT_SUPPORT_OUTPUT;
            break;
        default:
            break;
        }
    }

    caps.uMirroring = 1;
    caps.uScaling = 1;

    eMFXPlatform platform = m_core->GetPlatformType();
    if (platform == MFX_PLATFORM_HARDWARE)
    {
        caps.uChromaSiting = 1;
        caps.uVideoSignalInfoInOut = 1;
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoProcessing::QueryCapabilities(mfxVppCaps& caps)

mfxStatus VAAPIVideoProcessing::QueryVariance(
            mfxU32 frameIndex,
            std::vector<mfxU32> &variance)
{
    (void)frameIndex;
    (void)variance;

    return MFX_ERR_UNSUPPORTED;
} // mfxStatus VAAPIVideoProcessing::QueryVariance(mfxU32 frameIndex, std::vector<mfxU32> &variance)

mfxStatus VAAPIVideoProcessing::ConfigHVSDenoise(mfxExecuteParams *pParams)
{
    VAStatus vaSts = VA_STATUS_SUCCESS;
    MFX_CHECK_NULL_PTR1(pParams);

    if (pParams->bdenoiseAdvanced)
    {
        VAProcFilterParameterBufferHVSNoiseReduction hvs_param = {};

        hvs_param.type              = VAProcFilterHVSNoiseReduction;
        switch(pParams->denoiseMode)
        {
        case MFX_DENOISE_MODE_INTEL_HVS_AUTO_BDRATE:
            hvs_param.mode      = VA_PROC_HVS_DENOISE_AUTO_BDRATE;
            break;
        case MFX_DENOISE_MODE_INTEL_HVS_AUTO_SUBJECTIVE:
            hvs_param.mode      = VA_PROC_HVS_DENOISE_AUTO_SUBJECTIVE;
            break;
        case MFX_DENOISE_MODE_INTEL_HVS_PRE_MANUAL:
            hvs_param.mode      = VA_PROC_HVS_DENOISE_MANUAL;
            hvs_param.strength  = (mfxU16)floor(16.0 / 100.0 * pParams->denoiseStrength);
            break;
        case MFX_DENOISE_MODE_INTEL_HVS_POST_MANUAL:
            hvs_param.mode      = VA_PROC_HVS_DENOISE_MANUAL;
            hvs_param.strength  = pParams->denoiseStrength;
            break;
        case MFX_DENOISE_MODE_INTEL_HVS_AUTO_ADJUST:
        case MFX_DENOISE_MODE_DEFAULT:
        default:
            hvs_param.mode      = VA_PROC_HVS_DENOISE_DEFAULT;
            break;
        }
        /* create hvs denoise fitler buffer */
        vaSts = vaCreateBuffer((void*)m_vaDisplay,
                                m_vaContextVPP,
                                VAProcFilterParameterBufferType,
                                sizeof(hvs_param),
                                1,
                                &hvs_param,
                                &m_hvsDenoiseFilterID);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        m_filterBufs[m_numFilterBufs++] = m_hvsDenoiseFilterID;
    }

    return MFX_ERR_NONE;
}

/// Setup VPP VAAPI driver parameters
/*!
 Setup VPP surfaces and VPP VAAPI parameters.

 \param[in] pParams
 structure with VPP parameters and associated input and reference surfaces.
 \param[out] deint
 VAProcFilterParameterBufferDeinterlacing structure containing deinterlace information (scene change, which field to display).
 \param[out] m_pipelineParam
 VAProcPipelineParameterBuffer structure containing input and reference frames.

 return mfxStatus
 MFX_ERR_NONE id successful
 */
mfxStatus VAAPIVideoProcessing::Execute(mfxExecuteParams *pParams)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VPP DDISubmitTask");

    bool bIsFirstField = true;
    bool bUseReference = false;
    VAStatus vaSts = VA_STATUS_SUCCESS;

    eMFXHWType hwType = m_core->GetHWType();

    // NOTE the following variables should be visible till vaRenderPicture/vaEndPicture,
    // not till vaCreateBuffer as the data they hold are passed to the driver via a pointer
    // to the memory and driver simply copies the pointer till further dereference in the
    // functions noted above. Thus, safer to simply make sure these variable are visible
    // thru the whole scope of the function.
    VARectangle input_region, output_region;

    MFX_CHECK_NULL_PTR1( pParams );
    MFX_CHECK_NULL_PTR1( pParams->targetSurface.hdl.first );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces[0].hdl.first );

    UMC::AutomaticUMCMutex guard(m_guard);

    /* There is a special case for composition */
    mfxStatus mfxSts = MFX_ERR_NONE;
    if (pParams->bComposite)
    {
        if ((pParams->iTilesNum4Comp > 0) || (isVideoWall(pParams)) )
        {
            mfxSts = Execute_Composition_TiledVideoWall(pParams);
            if (MFX_ERR_NONE == mfxSts)
                return mfxSts;
        }
        /* if there is errors in params for tiling usage it may work for usual composition */
        else
            mfxSts = Execute_Composition(pParams);
        return mfxSts;
    }

#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
    struct gpu_hang_trigger
    {
        VADisplay   disp;
        VABufferID  id;
        VAStatus    sts;

        gpu_hang_trigger(VADisplay disp, VAContextID ctx, bool on)
            : disp(disp), id(VA_INVALID_ID)
            , sts(VA_STATUS_SUCCESS)
        {
            if (!on)
                return;

            sts =  vaCreateBuffer(disp, ctx, VATriggerCodecHangBufferType,
                                             sizeof(unsigned int), 1, 0, &id);
            if (sts == VA_STATUS_SUCCESS)
            {
                unsigned int* trigger = NULL;
                sts = vaMapBuffer(disp, id, (void**)&trigger);
                if (sts != VA_STATUS_SUCCESS)
                    return;

                if (trigger)
                    *trigger = 1;
                else
                    sts = VA_STATUS_ERROR_UNKNOWN;

                vaUnmapBuffer(disp, id);
            }
        }

        ~gpu_hang_trigger()
        {
            if (id != VA_INVALID_ID)
                sts = vaDestroyBuffer(disp, id);
        }
    } trigger(m_vaDisplay, m_vaContextVPP, pParams->gpuHangTrigger);

    if (trigger.sts != VA_STATUS_SUCCESS)
        return MFX_ERR_DEVICE_FAILED;
#endif // #if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)

    // This works for ADI 30i->30p for now
    // Better way may involve enabling bEOS for 30i->30p mode
    if ((1 == pParams->refCount) && (pParams->bDeinterlace30i60p == false))
        m_deintFrameCount = 0;

    if (VA_INVALID_ID == m_deintFilterID)
    {
        if (pParams->iDeinterlacingAlgorithm)
        {
            VAProcFilterParameterBufferDeinterlacing deint;
            deint.type  = VAProcFilterDeinterlacing;
            deint.flags = 0;

            if (MFX_DEINTERLACING_BOB == pParams->iDeinterlacingAlgorithm)
            {
                deint.algorithm = VAProcDeinterlacingBob;
            }
            else if(0 == pParams->iDeinterlacingAlgorithm) // skip DI for progressive frames
            {
                deint.algorithm = VAProcDeinterlacingNone;
            }
            else
            {
                deint.algorithm = VAProcDeinterlacingMotionAdaptive;
            }

            // Get picture structure of ouput frame
            mfxDrvSurface* pRefSurf_frameInfo = &(pParams->pRefSurfaces[0]); // previous input frame
            mfxDrvSurface *pCurSurf_frameInfo = &(pParams->pRefSurfaces[pParams->bkwdRefCount]); // current input frame

            // PicStruc can take values: MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_REPEATED=0x10
            // default deint.flags = 0 is for top field in TFF frame.
            if (pCurSurf_frameInfo->frameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
                deint.flags = VA_DEINTERLACING_ONE_FIELD;
            else if (pCurSurf_frameInfo->frameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF)
                deint.flags = VA_DEINTERLACING_BOTTOM_FIELD_FIRST | VA_DEINTERLACING_BOTTOM_FIELD;

            /* Deinterlacer:
             * BOB is used in first output frame (pParams->refCount = 1) and
             * when scene change flag is set (pParams->scene > 0) ADI is
             * used otherwise.
             * ADI output is late by one field. Current and reference field
             * are fed into de-interlacer.
             **/

            /* ADI 30i->30p: To get first field of current frame only
             * For TFF cases, deint.flags required to set VA_DEINTERLACING_ONE_FIELD
             * ensure driver go 30i->30p mode and surface set correct
             * For BFF cases, deint.flags required to set all bits
             * ensure driver go 30i->30p mode and surface set correct
             **/
            if(deint.algorithm == VAProcDeinterlacingMotionAdaptive && (pParams->refCount > 1))
            {
                if (MFX_PICSTRUCT_FIELD_TFF & pCurSurf_frameInfo->frameInfo.PicStruct)
                    deint.flags = VA_DEINTERLACING_ONE_FIELD;
                else /* For BFF case required to set all bits  */
                    deint.flags = VA_DEINTERLACING_BOTTOM_FIELD_FIRST | VA_DEINTERLACING_BOTTOM_FIELD | VA_DEINTERLACING_ONE_FIELD;
            }

            /* For 30i->60p case we have to indicate
             * to driver which field Top or Bottom we a going to send.
             * ADI uses reference frame and start after first frame:
             *   m_deintFrameCount==1, if TFF, TOP=second field of reference, BOTTOM=first field of current
             * BOB, ADI_no_ref do not use reference frame and are used on first frame:
             *   m_deintFrameCount==0, if TFF, TOP=first field of current, BOTTOM=seconf field of current.
             */
            if (pParams->bDeinterlace30i60p == true)
            {
                mfxU32 refFramePicStruct = pRefSurf_frameInfo->frameInfo.PicStruct;
                mfxU32 currFramePicStruct = pCurSurf_frameInfo->frameInfo.PicStruct;
                bool isCurrentProgressive = false;
                bool isPreviousProgressive = false;
                bool isSameParity = false;

                // Deinterlace with reference can be used after first frame is processed
                if(pParams->refCount > 1 && m_deintFrameCount)
                    bUseReference = true;

                // Check if previous is progressive
                if ((refFramePicStruct == MFX_PICSTRUCT_PROGRESSIVE) ||
                (refFramePicStruct & MFX_PICSTRUCT_FIELD_REPEATED) ||
                (refFramePicStruct & MFX_PICSTRUCT_FRAME_DOUBLING) ||
                (refFramePicStruct & MFX_PICSTRUCT_FRAME_TRIPLING))
                {
                    isPreviousProgressive = true;
                }

                if ((currFramePicStruct == MFX_PICSTRUCT_PROGRESSIVE) ||
                    (currFramePicStruct & MFX_PICSTRUCT_FIELD_REPEATED) ||
                    (currFramePicStruct & MFX_PICSTRUCT_FRAME_DOUBLING) ||
                    (currFramePicStruct & MFX_PICSTRUCT_FRAME_TRIPLING))
                {
                    isCurrentProgressive = true;
                }

                // Use BOB when scene change occur
                if ( MFX_DEINTERLACING_ADVANCED_SCD == pParams->iDeinterlacingAlgorithm &&
                ( pParams->scene != VPP_NO_SCENE_CHANGE))
                    bUseReference = false;

                // Set up wich field to display for 30i->60p mode
                // ADI uses second Field on even frames
                // BOB, ADI_no_ref uses second Field on odd frames
                if (0 == (m_deintFrameCount %2) && bUseReference)
                {
                    bIsFirstField = false;
                }
                else if (1 == (m_deintFrameCount %2) && ((!bUseReference)))
                {
                    bIsFirstField = false;
                }

                // Set deinterlace flag depending on parity and field to display
                if (bIsFirstField) // output is second field of previous input
                {
                    if (MFX_PICSTRUCT_FIELD_TFF & pRefSurf_frameInfo->frameInfo.PicStruct)
                        deint.flags = 0;
                    else
                        deint.flags = VA_DEINTERLACING_BOTTOM_FIELD_FIRST | VA_DEINTERLACING_BOTTOM_FIELD;
                }
                else // output is first field of current input
                {
                    if (MFX_PICSTRUCT_FIELD_TFF & pCurSurf_frameInfo->frameInfo.PicStruct)
                        deint.flags = VA_DEINTERLACING_BOTTOM_FIELD;
                    else
                        deint.flags = VA_DEINTERLACING_BOTTOM_FIELD_FIRST;
                }

                // case where previous and current have different parity -> use ADI no ref on current
                // To avoid frame duplication

                if((refFramePicStruct & MFX_PICSTRUCT_FIELD_TFF) && (currFramePicStruct & MFX_PICSTRUCT_FIELD_TFF))
                    isSameParity = true;
                else if((refFramePicStruct & MFX_PICSTRUCT_FIELD_BFF) && (currFramePicStruct & MFX_PICSTRUCT_FIELD_BFF))
                    isSameParity = true;

                if((!isPreviousProgressive) && (!isCurrentProgressive) && (!isSameParity))
                {
                    // Current is BFF
                    if(!bIsFirstField && (currFramePicStruct & MFX_PICSTRUCT_FIELD_BFF))
                    {
                        deint.flags = VA_DEINTERLACING_BOTTOM_FIELD_FIRST | VA_DEINTERLACING_BOTTOM_FIELD;
                        pParams->refCount =1; // Force ADI no ref on first Field of current frame
                    }

                    // Current is TFF
                    if(!bIsFirstField && (currFramePicStruct & MFX_PICSTRUCT_FIELD_TFF))
                    {
                        deint.flags = 0;
                        pParams->refCount =1; // Force ADI no ref on first Field of current frame
                    }
                }
            } //  if ((30i->60p) && (pParams->refCount > 1)) /* 30i->60p mode only*/

            /* Process special case and scene change flag*/
            if ( MFX_DEINTERLACING_ADVANCED_SCD == pParams->iDeinterlacingAlgorithm &&
                ( pParams->scene != VPP_NO_SCENE_CHANGE))
            {
                if(pParams->bDeinterlace30i60p) // 30i->60p mode
                {
                    // In case of multiple scene changes, use BOB with same field to avoid out of frame order
                    if(VPP_MORE_SCENE_CHANGE_DETECTED == pParams->scene)
                    {
                        if (MFX_PICSTRUCT_FIELD_TFF & pCurSurf_frameInfo->frameInfo.PicStruct)
                            deint.flags = 0;
                        else /* BFF */
                            deint.flags = VA_DEINTERLACING_BOTTOM_FIELD_FIRST | VA_DEINTERLACING_BOTTOM_FIELD;
                    }
                }
                else /* BOB 30i->30p use First Field to generate output*/
                {
                    if (MFX_PICSTRUCT_FIELD_TFF & pCurSurf_frameInfo->frameInfo.PicStruct)
                        deint.flags = 0;
                    else /* Frame is BFF */
                        deint.flags = VA_DEINTERLACING_BOTTOM_FIELD_FIRST | VA_DEINTERLACING_BOTTOM_FIELD;
                }

                deint.flags |= VA_DEINTERLACING_SCD_ENABLE; // It forces BOB
            }

            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextVPP,
                                   VAProcFilterParameterBufferType,
                                   sizeof(deint), 1,
                                   &deint, &m_deintFilterID);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            m_filterBufs[m_numFilterBufs++] = m_deintFilterID;
        }
    }

    if (pParams->bEnableProcAmp)
    {
        /* Buffer was created earlier and it's time to refresh its value */
        mfxSts = RemoveBufferFromPipe(m_procampFilterID);
        MFX_CHECK_STS(mfxSts);
    }

    if (VA_INVALID_ID == m_procampFilterID)
    {
        if ( pParams->bEnableProcAmp )
        {
            VAProcFilterParameterBufferColorBalance procamp[4];

            procamp[0].type   = VAProcFilterColorBalance;
            procamp[0].attrib = VAProcColorBalanceBrightness;
            procamp[0].value  = 0;

            if ( pParams->Brightness)
            {
                procamp[0].value = convertValue(-100,
                                100,
                                m_procampCaps[VAProcColorBalanceBrightness-1].range.min_value,
                                m_procampCaps[VAProcColorBalanceBrightness-1].range.max_value,
                                pParams->Brightness);
            }

            procamp[1].type   = VAProcFilterColorBalance;
            procamp[1].attrib = VAProcColorBalanceContrast;
            procamp[1].value  = 1;

            if ( pParams->Contrast != 1 )
            {
                procamp[1].value = convertValue(0,
                                10,
                                m_procampCaps[VAProcColorBalanceContrast-1].range.min_value,
                                m_procampCaps[VAProcColorBalanceContrast-1].range.max_value,
                                pParams->Contrast);
            }

            procamp[2].type   = VAProcFilterColorBalance;
            procamp[2].attrib = VAProcColorBalanceHue;
            procamp[2].value  = 0;

            if ( pParams->Hue)
            {
                procamp[2].value = convertValue(-180,
                                180,
                                m_procampCaps[VAProcColorBalanceHue-1].range.min_value,
                                m_procampCaps[VAProcColorBalanceHue-1].range.max_value,
                                pParams->Hue);
            }

            procamp[3].type   = VAProcFilterColorBalance;
            procamp[3].attrib = VAProcColorBalanceSaturation;
            procamp[3].value  = 1;

            if ( pParams->Saturation != 1 )
            {
                procamp[3].value = convertValue(0,
                                10,
                                m_procampCaps[VAProcColorBalanceSaturation-1].range.min_value,
                                m_procampCaps[VAProcColorBalanceSaturation-1].range.max_value,
                                pParams->Saturation);
            }

            vaSts = vaCreateBuffer((void*)m_vaDisplay,
                                          m_vaContextVPP,
                                          VAProcFilterParameterBufferType,
                                          sizeof(procamp), 4,
                                          &procamp, &m_procampFilterID);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            m_filterBufs[m_numFilterBufs++] = m_procampFilterID;

            /* Clear enable flag. Since appropriate buffer has been created,
             * it will be used in consequent Execute calls.
             * If application will Reset or call Init/Close with setting procamp
             * with new value, this flag will be raised again thus new buffer with
             * new value will be created. */
            pParams->bEnableProcAmp = false;
        }
    }

    if (pParams->denoiseFactor || pParams->bDenoiseAutoAdjust)
    {
        /* Buffer was created earlier and it's time to refresh its value */
        mfxSts = RemoveBufferFromPipe(m_denoiseFilterID);
        MFX_CHECK_STS(mfxSts);
    }

    if (VA_INVALID_ID == m_denoiseFilterID)
    {
        if (pParams->denoiseFactor || pParams->bDenoiseAutoAdjust)
        {
            VAProcFilterParameterBuffer denoise;
            denoise.type  = VAProcFilterNoiseReduction;
            if(pParams->bDenoiseAutoAdjust)
                denoise.value = m_denoiseCaps.range.default_value;
            else
                denoise.value = (mfxU16)floor(
                                            convertValue(0,
                                              100,
                                              m_denoiseCaps.range.min_value,
                                              m_denoiseCaps.range.max_value,
                                              pParams->denoiseFactor) + 0.5);
            vaSts = vaCreateBuffer((void*)m_vaDisplay,
                          m_vaContextVPP,
                          VAProcFilterParameterBufferType,
                          sizeof(denoise), 1,
                          &denoise, &m_denoiseFilterID);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            m_filterBufs[m_numFilterBufs++] = m_denoiseFilterID;
            pParams->denoiseFactor = 0;
            pParams->bDenoiseAutoAdjust = false;
        }
    }

    if (pParams->bdenoiseAdvanced)
    {
        /* Buffer was created earlier and it's time to refresh its value */
        mfxSts = RemoveBufferFromPipe(m_hvsDenoiseFilterID);
        MFX_CHECK_STS(mfxSts);
    }

    if (VA_INVALID_ID == m_hvsDenoiseFilterID)
    {
        mfxSts = ConfigHVSDenoise(pParams);
        MFX_CHECK_STS(mfxSts);
    }

    if (VA_INVALID_ID == m_3dlutFilterID)
    {
        if (pParams->lut3DInfo.Enabled)
        {
            const mfxU16 lut17_seg_size = 17, lut17_mul_size = 32;
            const mfxU16 lut33_seg_size = 33, lut33_mul_size = 64;
            const mfxU16 lut65_seg_size = 65, lut65_mul_size = 128;

            VAProcFilterParameterBuffer3DLUT lut3d_param = {};

            lut3d_param.type            = VAProcFilter3DLUT;
            lut3d_param.bit_depth       = 16;
            lut3d_param.num_channel     = 4;
            switch(pParams->lut3DInfo.ChannelMapping)
            {
            case MFX_3DLUT_CHANNEL_MAPPING_RGB_RGB:
            case MFX_3DLUT_CHANNEL_MAPPING_DEFAULT:
                lut3d_param.channel_mapping = VA_3DLUT_CHANNEL_RGB_RGB;
                break;
            case MFX_3DLUT_CHANNEL_MAPPING_YUV_RGB:
                lut3d_param.channel_mapping = VA_3DLUT_CHANNEL_YUV_RGB;
                break;
            case MFX_3DLUT_CHANNEL_MAPPING_VUY_RGB:
                lut3d_param.channel_mapping = VA_3DLUT_CHANNEL_VUY_RGB;
                break;
            default:
                break;
            }

            if(pParams->lut3DInfo.BufferType == MFX_RESOURCE_VA_SURFACE)
            {
                lut3d_param.lut_surface     = *((VASurfaceID*)pParams->lut3DInfo.MemId);
                switch(pParams->lut3DInfo.MemLayout)
                {
                case MFX_3DLUT_MEMORY_LAYOUT_INTEL_17LUT:
                    lut3d_param.lut_size      = lut17_seg_size;
                    lut3d_param.lut_stride[0] = lut17_seg_size;
                    lut3d_param.lut_stride[1] = lut17_seg_size;
                    lut3d_param.lut_stride[2] = lut17_mul_size;
                    break;
                case MFX_3DLUT_MEMORY_LAYOUT_INTEL_33LUT:
                    lut3d_param.lut_size      = lut33_seg_size;
                    lut3d_param.lut_stride[0] = lut33_seg_size;
                    lut3d_param.lut_stride[1] = lut33_seg_size;
                    lut3d_param.lut_stride[2] = lut33_mul_size;
                    break;
                case MFX_3DLUT_MEMORY_LAYOUT_INTEL_65LUT:
                case MFX_3DLUT_MEMORY_LAYOUT_DEFAULT:
                    lut3d_param.lut_size      = lut65_seg_size;
                    lut3d_param.lut_stride[0] = lut65_seg_size;
                    lut3d_param.lut_stride[1] = lut65_seg_size;
                    lut3d_param.lut_stride[2] = lut65_mul_size;
                    break;
                default:
                    break;
                }
            }
            else if(pParams->lut3DInfo.BufferType == MFX_RESOURCE_SYSTEM_SURFACE)
            {
                // The current implementation only supports the size of 3 channels is same.
                mfxSts = ((pParams->lut3DInfo.Channel[0].Size == pParams->lut3DInfo.Channel[1].Size)) ? MFX_ERR_NONE : MFX_ERR_UNSUPPORTED;
                MFX_CHECK_STS(mfxSts);
                // The current implementation only supports 17, 33, 65 LUT size.
                mfxSts = ((pParams->lut3DInfo.Channel[0].Size == 17) ||
                          (pParams->lut3DInfo.Channel[0].Size == 33) ||
                          (pParams->lut3DInfo.Channel[0].Size == 65)) ? MFX_ERR_NONE : MFX_ERR_UNSUPPORTED;
                MFX_CHECK_STS(mfxSts);

                mfxU16 seg_size = lut65_seg_size, mul_size = lut65_mul_size;
                switch(pParams->lut3DInfo.Channel[0].Size)
                {
                case lut17_seg_size:
                    seg_size = lut17_seg_size;
                    mul_size = lut17_mul_size;
                    break;
                case lut33_seg_size:
                    seg_size = lut33_seg_size;
                    mul_size = lut33_mul_size;
                    break;
                case lut65_seg_size:
                default:
                    seg_size = lut65_seg_size;
                    mul_size = lut65_mul_size;
                    break;
                }
                lut3d_param.lut_size = seg_size;

                // create VA surface
                VASurfaceAttrib surface_attrib = {};
                surface_attrib.type            = VASurfaceAttribPixelFormat;
                surface_attrib.flags           = VA_SURFACE_ATTRIB_SETTABLE;
                surface_attrib.value.type      = VAGenericValueTypeInteger;
                surface_attrib.value.value.i   = VA_FOURCC_RGBA;

                VASurfaceID surface_id = VA_INVALID_ID;
                vaSts = vaCreateSurfaces(m_vaDisplay,
                                          VA_RT_FORMAT_RGB32,
                                          seg_size * mul_size,
                                          seg_size * 2,
                                          &surface_id,
                                          1,
                                          &surface_attrib,
                                          1);
                MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                vaSts = vaSyncSurface(m_vaDisplay, surface_id);
                MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                VAImage surface_image = {};
                vaSts = vaDeriveImage(m_vaDisplay, surface_id, &surface_image);
                MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                void* surface_p = nullptr;
                vaSts = vaMapBuffer(m_vaDisplay, surface_image.buf, &surface_p);
                MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                // The size of the surface is (seg_size * mul_size * seg_size * 2 * 4) bytes;
                memset(surface_p, 0, seg_size * mul_size * seg_size * 8);

                mfxU64 *lut_data    = (mfxU64 *)surface_p;
                mfxU16 *r_data      = (mfxU16 *)pParams->lut3DInfo.Channel[0].Data;
                mfxU16 *g_data      = (mfxU16 *)pParams->lut3DInfo.Channel[1].Data;
                mfxU16 *b_data      = (mfxU16 *)pParams->lut3DInfo.Channel[2].Data;
                mfxU32 index = 0;
                for (mfxU16 r_index = 0; r_index < seg_size; r_index++)
                {
                    for (mfxU16 g_index = 0; g_index < seg_size; g_index++)
                    {
                        for (mfxU16 b_index = 0; b_index < pParams->lut3DInfo.Channel[2].Size; b_index++)
                        {
                            mfxU16 r_temp = r_data[index];
                            mfxU16 g_temp = g_data[index];
                            mfxU16 b_temp = b_data[index];
                            lut_data[r_index * seg_size * mul_size + g_index * mul_size + b_index] = ((mfxU64)b_temp)<<32 | ((mfxU64)g_temp)<<16 | ((mfxU64)r_temp);
                            index++;
                        }
                    }
                }

                vaSts = vaUnmapBuffer(m_vaDisplay, surface_image.buf);
                MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                vaSts = vaDestroyImage(m_vaDisplay, surface_image.image_id);
                MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                lut3d_param.lut_surface     = surface_id;
            }
            /* create 3dlut fitler buffer */
            vaSts = vaCreateBuffer((void*)m_vaDisplay,
                                    m_vaContextVPP,
                                    VAProcFilterParameterBufferType,
                                    sizeof(lut3d_param),
                                    1,
                                    &lut3d_param,
                                    &m_3dlutFilterID);
            MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            m_filterBufs[m_numFilterBufs++] = m_3dlutFilterID;
        }
    }

    if (pParams->detailFactor || pParams->bDetailAutoAdjust)
    {
        /* Buffer was created earlier and it's time to refresh its value */
        mfxSts = RemoveBufferFromPipe(m_detailFilterID);
        MFX_CHECK_STS(mfxSts);
    }

    if (VA_INVALID_ID == m_detailFilterID)
    {
        if (pParams->detailFactor || pParams->bDetailAutoAdjust)
        {
            VAProcFilterParameterBuffer detail;
            detail.type  = VAProcFilterSharpening;
            if(pParams->bDetailAutoAdjust)
                detail.value = m_detailCaps.range.default_value;
            else
                detail.value = (mfxU16)floor(
                                           convertValue(0,
                                             100,
                                             m_detailCaps.range.min_value,
                                             m_detailCaps.range.max_value,
                                             pParams->detailFactor) + 0.5);
            vaSts = vaCreateBuffer((void*)m_vaDisplay,
                          m_vaContextVPP,
                          VAProcFilterParameterBufferType,
                          sizeof(detail), 1,
                          &detail, &m_detailFilterID);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            m_filterBufs[m_numFilterBufs++] = m_detailFilterID;
            pParams->detailFactor = 0;
            pParams->bDetailAutoAdjust = false;
        }
    }

    if (VA_INVALID_ID == m_frcFilterID)
    {
        if (pParams->bFRCEnable)
        {
#ifdef MFX_ENABLE_VPP_FRC
          VAProcFilterParameterBufferFrameRateConversion frcParams;

          memset(&frcParams, 0, sizeof(VAProcFilterParameterBufferFrameRateConversion));
          frcParams.type = VAProcFilterFrameRateConversion;
          if (30 == pParams->customRateData.customRate.FrameRateExtD)
          {
              frcParams.input_fps = 30;
              frcParams.output_fps = 60;
              frcParams.num_output_frames = 2;
              frcParams.repeat_frame = 0;
          }
          else if (24 == pParams->customRateData.customRate.FrameRateExtD)
          {
              frcParams.input_fps = 24;
              frcParams.output_fps = 60;
              frcParams.num_output_frames = 5;
              frcParams.repeat_frame = 0;
          }

          frcParams.cyclic_counter = m_frcCyclicCounter++;
          if ((frcParams.input_fps == 30) && (m_frcCyclicCounter == 2))
                m_frcCyclicCounter = 0;
          if ((frcParams.input_fps == 24) && (m_frcCyclicCounter == 5))
                m_frcCyclicCounter = 0;
          frcParams.output_frames = (VASurfaceID*)(pParams->targetSurface.hdl.first);
          vaSts = vaCreateBuffer((void*)m_vaDisplay,
                        m_vaContextVPP,
                        VAProcFilterParameterBufferType,
                        sizeof(frcParams), 1,
                        &frcParams, &m_frcFilterID);
          MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

          m_filterBufs[m_numFilterBufs++] = m_frcFilterID;
#else
          return MFX_ERR_UNSUPPORTED;
#endif
        }
    }

    /* pParams->refCount is total number of processing surfaces:
     * in case of composition this is primary + sub streams*/

    m_pipelineParam.resize(pParams->refCount);
    m_pipelineParamID.resize(pParams->refCount, VA_INVALID_ID);

    mfxDrvSurface* pRefSurf = &(pParams->pRefSurfaces[0]);
    memset(&m_pipelineParam[0], 0, sizeof(m_pipelineParam[0]));
    const char *driverVersion = vaQueryVendorString(m_vaDisplay);
    const char *standardVersion = "Intel iHD driver for Intel(R) Gen Graphics - 24.3.1";
    bool versionMatch = strcmp(driverVersion, standardVersion) >= 0;
    if (pParams->bFieldWeaving || ( (pParams->refCount > 1) && (0 != pParams->iDeinterlacingAlgorithm )))
    {
        if (versionMatch)
        {
            m_pipelineParam[0].num_forward_references = 1;
        }
        else
        {
            m_pipelineParam[0].num_backward_references = 1;
        }
        mfxDrvSurface* pRefSurf_1 = NULL;
        /* in pRefSurfaces
            * first is past frame references
            * then current src surface
            * and only after this is future frame references
            * */
        pRefSurf_1 = &(pParams->pRefSurfaces[0]); // point to previous frame
        VASurfaceID *ref_srf = (VASurfaceID *)(pRefSurf_1->hdl.first);
        if (versionMatch)
        {
            m_pipelineParam[0].forward_references = ref_srf;
        }
        else
        {
            m_pipelineParam[0].backward_references = ref_srf;
        }
    }
    /* FRC Interpolated case */
    if (0 != pParams->bFRCEnable)
    {
        if (30 == pParams->customRateData.customRate.FrameRateExtD)
        {
            if (versionMatch)
            {
                m_pipelineParam[0].num_backward_references = 2;
            }
            else
            {
                m_pipelineParam[0].num_forward_references = 2;
            }
        }
        else if (24 == pParams->customRateData.customRate.FrameRateExtD)
        {
            if (versionMatch)
            {
                m_pipelineParam[0].num_backward_references = 3;
            }
            else
            {
                m_pipelineParam[0].num_forward_references = 3;
            }
        }

        if (2 == pParams->refCount) /* may be End of Stream case */
        {
            mfxDrvSurface* pRefSurf_frc1;
            pRefSurf_frc1 = &(pParams->pRefSurfaces[1]);
            m_refForFRC[0] = *(VASurfaceID*)(pRefSurf_frc1->hdl.first);
            m_refForFRC[2] = m_refForFRC[1] = m_refForFRC[0];
        }
        if (3 == pParams->refCount)
        {
            mfxDrvSurface* pRefSurf_frc1;
            pRefSurf_frc1 = &(pParams->pRefSurfaces[1]);
            m_refForFRC[0] = *(VASurfaceID*)(pRefSurf_frc1->hdl.first);
            mfxDrvSurface* pRefSurf_frc2;
            pRefSurf_frc2 = &(pParams->pRefSurfaces[2]);
            m_refForFRC[1] = *(VASurfaceID*) (pRefSurf_frc2->hdl.first);
        }
        if (4 == pParams->refCount)
        {
            mfxDrvSurface* pRefSurf_frc1;
            pRefSurf_frc1 = &(pParams->pRefSurfaces[1]);
            m_refForFRC[0] = *(VASurfaceID*)(pRefSurf_frc1->hdl.first);
            mfxDrvSurface* pRefSurf_frc2;
            pRefSurf_frc2 = &(pParams->pRefSurfaces[2]);
            m_refForFRC[1] = *(VASurfaceID*) (pRefSurf_frc2->hdl.first);
            mfxDrvSurface* pRefSurf_frc3;
            pRefSurf_frc3 = &(pParams->pRefSurfaces[3]);
            m_refForFRC[2] = *(VASurfaceID*) (pRefSurf_frc3->hdl.first);
        }
        /* to pass ref list to pipeline */
        if (versionMatch)
        {
            m_pipelineParam[0].backward_references = m_refForFRC;
        }
        else
        {
            m_pipelineParam[0].forward_references = m_refForFRC;
        }
    } /*if (0 != pParams->bFRCEnable)*/

    /* SRC surface */
    mfxDrvSurface* pSrcInputSurf = &pRefSurf[pParams->bkwdRefCount];
    /* in 30i->60p mode, when scene change occurs, BOB is used and needs current input frame.
        * Current frame = ADI reference for odd output frames
        * Current frame = ADI input for even output frames
        * when mixed picture is progressive, even frame use reference frame for output due to ADI delay
    **/
    if(pParams->bDeinterlace30i60p && ((VPP_SCENE_NEW == pParams->scene)|| (pParams->iDeinterlacingAlgorithm == 0)))
    {
        if(m_deintFrameCount % 2)
            pSrcInputSurf = &(pParams->pRefSurfaces[0]); // point to reference frame
    }

    VASurfaceID *srf = (VASurfaceID *)(pSrcInputSurf->hdl.first);
    m_pipelineParam[0].surface = *srf;

#ifdef MFX_ENABLE_VPP_ROTATION
    switch (pParams->rotation)
    {
    case MFX_ANGLE_90:
        if ( m_pipelineCaps.rotation_flags & (1 << VA_ROTATION_90))
        {
            m_pipelineParam[0].rotation_state = VA_ROTATION_90;
        }
        break;
    case MFX_ANGLE_180:
        if ( m_pipelineCaps.rotation_flags & (1 << VA_ROTATION_180))
        {
            m_pipelineParam[0].rotation_state = VA_ROTATION_180;
        }
        break;
    case MFX_ANGLE_270:
        if ( m_pipelineCaps.rotation_flags & (1 << VA_ROTATION_270))
        {
            m_pipelineParam[0].rotation_state = VA_ROTATION_270;
        }
        break;
    }
#endif

    /*
     * Execute mirroring for MIRROR_WO_EXEC only because MSDK will do
     * copy-with-mirror for others.
     */
    if (pParams->mirroringPosition == MIRROR_WO_EXEC) {
        switch (pParams->mirroring) {
        case MFX_MIRRORING_HORIZONTAL:
            m_pipelineParam[0].mirror_state = VA_MIRROR_HORIZONTAL;
            break;

        case MFX_MIRRORING_VERTICAL:
            m_pipelineParam[0].mirror_state = VA_MIRROR_VERTICAL;
            break;
        }
    }

    // source cropping
    mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);
    input_region.y   = inInfo->CropY;
    input_region.x   = inInfo->CropX;
    input_region.height = inInfo->CropH;
    input_region.width  = inInfo->CropW;
    m_pipelineParam[0].surface_region = &input_region;

    // destination cropping
    mfxFrameInfo *outInfo = &(pParams->targetSurface.frameInfo);
    output_region.y  = outInfo->CropY;
    output_region.x   = outInfo->CropX;
    output_region.height= outInfo->CropH;
    output_region.width  = outInfo->CropW;
    m_pipelineParam[0].output_region = &output_region;

    m_pipelineParam[0].output_background_color = 0xff000000; // black for ARGB

#ifdef MFX_ENABLE_VPP_VIDEO_SIGNAL
#define ENABLE_VPP_VIDEO_SIGNAL(X) X
#else
#define ENABLE_VPP_VIDEO_SIGNAL(X)
#endif

    mfxU32  refFourcc = pRefSurf->frameInfo.FourCC;
    switch (refFourcc)
    {
    case MFX_FOURCC_RGB4:
#ifdef MFX_ENABLE_RGBP
    case MFX_FOURCC_RGBP:
#endif
    case MFX_FOURCC_BGRP:
    case MFX_FOURCC_RGB565:
        m_pipelineParam[0].surface_color_standard = VAProcColorStandardNone;
        ENABLE_VPP_VIDEO_SIGNAL(m_pipelineParam[0].input_color_properties.color_range = VA_SOURCE_RANGE_FULL);
        break;
    case MFX_FOURCC_NV12:
    default:
        m_pipelineParam[0].surface_color_standard = VAProcColorStandardBT601;
        ENABLE_VPP_VIDEO_SIGNAL(m_pipelineParam[0].input_color_properties.color_range = VA_SOURCE_RANGE_REDUCED);
        break;
    }

    mfxU32  targetFourcc = pParams->targetSurface.frameInfo.FourCC;
    switch (targetFourcc)
    {
    case MFX_FOURCC_RGB4:
#ifdef MFX_ENABLE_RGBP
    case MFX_FOURCC_RGBP:
#endif
    case MFX_FOURCC_BGRP:
    case MFX_FOURCC_RGB565:
        m_pipelineParam[0].output_color_standard = VAProcColorStandardNone;
        ENABLE_VPP_VIDEO_SIGNAL(m_pipelineParam[0].output_color_properties.color_range = VA_SOURCE_RANGE_FULL);
        break;
    case MFX_FOURCC_NV12:
        m_pipelineParam[0].output_color_standard = VAProcColorStandardBT601;
        if(refFourcc == MFX_FOURCC_RGBP)
        {
            ENABLE_VPP_VIDEO_SIGNAL(m_pipelineParam[0].output_color_properties.color_range = VA_SOURCE_RANGE_FULL);
        }
        else
        {
            ENABLE_VPP_VIDEO_SIGNAL(m_pipelineParam[0].output_color_properties.color_range = VA_SOURCE_RANGE_REDUCED);
        }
        break;
    default:
        m_pipelineParam[0].output_color_standard = VAProcColorStandardBT601;
        ENABLE_VPP_VIDEO_SIGNAL(m_pipelineParam[0].output_color_properties.color_range = VA_SOURCE_RANGE_REDUCED);
        break;
    }

    m_pipelineParam[0].input_color_properties.chroma_sample_location  = VA_CHROMA_SITING_UNKNOWN;
    m_pipelineParam[0].output_color_properties.chroma_sample_location = VA_CHROMA_SITING_UNKNOWN;

    /* It needs interlaced flag passed only for
        * deinterlacing and scaling. All other filters must
        * use progressive even for interlaced content.
        */
    bool forceProgressive = true;
    if (pParams->iDeinterlacingAlgorithm ||
        inInfo->CropH != outInfo->CropH    ||
        inInfo->CropW != outInfo->CropW)
    {
        forceProgressive = false;
    }

    switch (pRefSurf->frameInfo.PicStruct)
    {
        case MFX_PICSTRUCT_PROGRESSIVE:
            m_pipelineParam[0].filter_flags = VA_FRAME_PICTURE;
            break;
        case MFX_PICSTRUCT_FIELD_TFF:
            m_pipelineParam[0].filter_flags = forceProgressive ? VA_FRAME_PICTURE : VA_TOP_FIELD;
            break;
        case MFX_PICSTRUCT_FIELD_BFF:
            m_pipelineParam[0].filter_flags = forceProgressive ? VA_FRAME_PICTURE : VA_BOTTOM_FIELD;
            break;
    }

    // If field weaving is perform on driver
    // Kernel don't uses these parameters
    if (pParams->bFieldWeavingExt)
    {
        // TFF and BFF are output frame types for field weaving
        switch (pParams->targetSurface.frameInfo.PicStruct)
        {
            case MFX_PICSTRUCT_FIELD_TFF:
            {
                m_pipelineParam[0].filter_flags = VA_TOP_FIELD_WEAVE;
                m_pipelineParam[0].input_surface_flag = VA_TOP_FIELD;
                m_pipelineParam[0].output_surface_flag = VA_TOP_FIELD_FIRST;
                break;
            }
            case MFX_PICSTRUCT_FIELD_BFF:
            {
                m_pipelineParam[0].filter_flags = VA_BOTTOM_FIELD_WEAVE;
                m_pipelineParam[0].input_surface_flag = VA_BOTTOM_FIELD;
                m_pipelineParam[0].output_surface_flag = VA_BOTTOM_FIELD_FIRST;
                break;
            }
        }
    }

    // If field splitting is perform on driver
    if (pParams->bFieldSplittingExt)
    {
        // TFF and BFF are input frame types for field splitting
        switch (pRefSurf->frameInfo.PicStruct)
        {
            case MFX_PICSTRUCT_FIELD_TFF:
            {
                m_pipelineParam[0].input_surface_flag = VA_TOP_FIELD_FIRST;
                m_pipelineParam[0].output_surface_flag = (pParams->statusReportID % 2 == 0) ? VA_TOP_FIELD : VA_BOTTOM_FIELD;
                break;
            }
            case MFX_PICSTRUCT_FIELD_BFF:
            {
                m_pipelineParam[0].input_surface_flag = VA_BOTTOM_FIELD_FIRST;
                m_pipelineParam[0].output_surface_flag = (pParams->statusReportID % 2 == 0) ? VA_BOTTOM_FIELD : VA_TOP_FIELD;
                break;
            }
        }
    }

    m_pipelineParam[0].filters      = m_filterBufs;
    m_pipelineParam[0].num_filters  = m_numFilterBufs;

    int index = GetCurFrameSignalIdx(pParams);
    if ((index >= 0) && pParams->VideoSignalInfo[index].enabled)
    {
#ifdef MFX_ENABLE_VPP_VIDEO_SIGNAL
        if(pParams->VideoSignalInfo[index].TransferMatrix != MFX_TRANSFERMATRIX_UNKNOWN)
        {
            m_pipelineParam[0].surface_color_standard = (MFX_TRANSFERMATRIX_BT709 == pParams->VideoSignalInfo[index].TransferMatrix ? VAProcColorStandardBT709 : VAProcColorStandardBT601);
        }

        if(pParams->VideoSignalInfo[index].NominalRange != MFX_NOMINALRANGE_UNKNOWN)
        {
            m_pipelineParam[0].input_color_properties.color_range = (MFX_NOMINALRANGE_0_255 == pParams->VideoSignalInfo[index].NominalRange) ? VA_SOURCE_RANGE_FULL : VA_SOURCE_RANGE_REDUCED;
        }
    }

    if (pParams->VideoSignalInfoOut.enabled)
    {
        if(pParams->VideoSignalInfoOut.TransferMatrix != MFX_TRANSFERMATRIX_UNKNOWN)
        {
            m_pipelineParam[0].output_color_standard = (MFX_TRANSFERMATRIX_BT709 == pParams->VideoSignalInfoOut.TransferMatrix ? VAProcColorStandardBT709 : VAProcColorStandardBT601);
        }

        if(pParams->VideoSignalInfoOut.NominalRange != MFX_NOMINALRANGE_UNKNOWN)
        {
            m_pipelineParam[0].output_color_properties.color_range = (MFX_NOMINALRANGE_0_255 == pParams->VideoSignalInfoOut.NominalRange) ? VA_SOURCE_RANGE_FULL : VA_SOURCE_RANGE_REDUCED;
        }
#else
        return MFX_ERR_UNSUPPORTED;
#endif // #ifdef MFX_ENABLE_VPP_VIDEO_SIGNAL
    }

    if (pParams->m_inVideoSignalInfo.enabled)
    {
        // Video Range
        m_pipelineParam[0].input_color_properties.color_range = (pParams->m_inVideoSignalInfo.VideoFullRange) ? VA_SOURCE_RANGE_FULL : VA_SOURCE_RANGE_REDUCED;
        switch (pParams->m_inVideoSignalInfo.ColourPrimaries)
        {
        case 9:    // BT.2020
            m_pipelineParam[0].surface_color_standard = VAProcColorStandardBT2020;
            break;
        case 1:    // BT.709
            m_pipelineParam[0].surface_color_standard = VAProcColorStandardBT709;
            break;
        case 6:
        default:
            m_pipelineParam[0].surface_color_standard = VAProcColorStandardBT601;
            break;
        }
        m_pipelineParam[0].input_color_properties.colour_primaries = pParams->m_inVideoSignalInfo.ColourPrimaries;
        m_pipelineParam[0].input_color_properties.transfer_characteristics = pParams->m_inVideoSignalInfo.TransferCharacteristics;
    }

    if (pParams->m_outVideoSignalInfo.enabled)
    {
        // Video Range
        m_pipelineParam[0].output_color_properties.color_range = (pParams->m_outVideoSignalInfo.VideoFullRange) ? VA_SOURCE_RANGE_FULL : VA_SOURCE_RANGE_REDUCED;
        switch (pParams->m_outVideoSignalInfo.ColourPrimaries)
        {
        case 9:    // BT.2020
            m_pipelineParam[0].output_color_standard = VAProcColorStandardBT2020;
            break;
        case 1:    // BT.709
            m_pipelineParam[0].output_color_standard = VAProcColorStandardBT709;
            break;
        case 6:
        default:
            m_pipelineParam[0].output_color_standard = VAProcColorStandardBT601;
            break;
        } 
        m_pipelineParam[0].output_color_properties.colour_primaries = pParams->m_outVideoSignalInfo.ColourPrimaries;
        m_pipelineParam[0].output_color_properties.transfer_characteristics = pParams->m_outVideoSignalInfo.TransferCharacteristics;
    }

    m_pipelineParam[0].input_color_properties.chroma_sample_location  = VA_CHROMA_SITING_UNKNOWN;
    m_pipelineParam[0].output_color_properties.chroma_sample_location = VA_CHROMA_SITING_UNKNOWN;

    VAHdrMetaData      out_metadata     = {};
    VAHdrMetaDataHDR10 outHDR10MetaData = {};

    auto SetHdrMetaData = [](const mfxExecuteParams::HDR10MetaData& data)
    {
        mfxU32 kLuminanceFixedPoint = 10000;
        //In VAAPI, maxMasteringLuminance and minMasteringLuminance are in the unit of 0.0001 nits.
        VAHdrMetaDataHDR10 retHDR10MetaData =
        {
            {data.displayPrimariesX[0], data.displayPrimariesX[1], data.displayPrimariesX[2]},
            {data.displayPrimariesY[0], data.displayPrimariesY[1], data.displayPrimariesY[2]},
            data.whitePoint[0], 
            data.whitePoint[1],
            // According to the description about mfxExtMasteringDisplayColourVolume,
            // `MaxDisplayMasteringLuminance` is in units of 1 candela per square meter.
            // `MinDisplayMasteringLuminance` is in units of 0.0001 candela per square meter
            // so only data.maxMasteringLuminance is multiplied by kLuminanceFixedPoint
            data.maxMasteringLuminance * kLuminanceFixedPoint,
            data.minMasteringLuminance,
            data.maxContentLightLevel,
            data.maxFrameAverageLightLevel
        };
        return retHDR10MetaData;
    };

    VAHdrMetaDataHDR10 inHDR10MetaData = {};
    if(pParams->inHDR10MetaData.enabled)
    {
        if (m_hdrtmFilterID != VA_INVALID_ID)
        {
            mfxSts = RemoveBufferFromPipe(m_hdrtmFilterID);
            MFX_CHECK_STS(mfxSts);
        }

        inHDR10MetaData = SetHdrMetaData(pParams->inHDR10MetaData);
        VAProcFilterParameterBufferHDRToneMapping hdrtm_param = {};
        hdrtm_param.type                                      = VAProcFilterHighDynamicRangeToneMapping;
        hdrtm_param.data.metadata_type                        = VAProcHighDynamicRangeMetadataHDR10;
        hdrtm_param.data.metadata                             = &inHDR10MetaData;
        hdrtm_param.data.metadata_size                        = sizeof(VAHdrMetaDataHDR10);

        vaSts = vaCreateBuffer((void*)m_vaDisplay, 
                                    m_vaContextVPP, 
                                    VAProcFilterParameterBufferType, 
                                    sizeof(hdrtm_param), 
                                    1, 
                                    (void*)&hdrtm_param, 
                                    &m_hdrtmFilterID);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        m_filterBufs[m_numFilterBufs++] = m_hdrtmFilterID;
        m_pipelineParam[0].num_filters  = m_numFilterBufs;
        m_pipelineParam[0].surface_color_standard = VAProcColorStandardExplicit;
    }

    if (pParams->outHDR10MetaData.enabled)
    {
        outHDR10MetaData = SetHdrMetaData(pParams->outHDR10MetaData);

        out_metadata.metadata_type = VAProcHighDynamicRangeMetadataHDR10;
        out_metadata.metadata      = &outHDR10MetaData;
        out_metadata.metadata_size = sizeof(VAHdrMetaDataHDR10);

        m_pipelineParam[0].output_hdr_metadata = &out_metadata;
        m_pipelineParam[0].output_color_standard = VAProcColorStandardExplicit;
    }

    mfxU32 interpolation[4] = {
        VA_FILTER_INTERPOLATION_DEFAULT,            // MFX_INTERPOLATION_DEFAULT                = 0,
        VA_FILTER_INTERPOLATION_NEAREST_NEIGHBOR,   // MFX_INTERPOLATION_NEAREST_NEIGHBOR       = 1,
        VA_FILTER_INTERPOLATION_BILINEAR,           // MFX_INTERPOLATION_BILINEAR               = 2,
        VA_FILTER_INTERPOLATION_ADVANCED            // MFX_INTERPOLATION_ADVANCED               = 3
    };
    mfxSts = (pParams->interpolationMethod > MFX_INTERPOLATION_ADVANCED) ? MFX_ERR_UNSUPPORTED : MFX_ERR_NONE;
    MFX_CHECK_STS(mfxSts);
    /* Scaling params */
    switch (pParams->scalingMode)
    {
    case MFX_SCALING_MODE_LOWPOWER:
        /* VA_FILTER_SCALING_DEFAULT means the following:
            *  First priority is HW fixed function scaling engine. If it can't work, revert to AVS
            *  If scaling ratio between 1/8...8 -> HW fixed function
            *  If scaling ratio between 1/16...1/8 or larger than 8 -> AVS
            *  If scaling ratio is less than 1/16 -> bilinear
            */
        m_pipelineParam[0].filter_flags |= VA_FILTER_SCALING_DEFAULT;
        m_pipelineParam[0].filter_flags |= interpolation[pParams->interpolationMethod];
        break;
    case MFX_SCALING_MODE_QUALITY:
        /*  VA_FILTER_SCALING_HQ means the following:
            *  If scaling ratio is less than 1/16 -> bilinear
            *  For all other cases, AVS is used.
            */
        m_pipelineParam[0].filter_flags |= VA_FILTER_SCALING_HQ;
        m_pipelineParam[0].filter_flags |= interpolation[pParams->interpolationMethod];
        mfxSts = ((pParams->interpolationMethod == MFX_INTERPOLATION_DEFAULT) || (pParams->interpolationMethod == MFX_INTERPOLATION_ADVANCED)) ? MFX_ERR_NONE : MFX_ERR_UNSUPPORTED;
        break;
    case MFX_SCALING_MODE_INTEL_GEN_COMPUTE:
        m_pipelineParam[0].filter_flags |= VA_FILTER_SCALING_HQ;
        m_pipelineParam[0].filter_flags |= interpolation[pParams->interpolationMethod];
        m_pipelineParam[0].pipeline_flags |= VA_PROC_PIPELINE_FAST;
        break;
    case MFX_SCALING_MODE_INTEL_GEN_VDBOX:
        /* In VPP RT, invalid parameters. */
        mfxSts = MFX_ERR_INVALID_VIDEO_PARAM;
        break;
    case MFX_SCALING_MODE_INTEL_GEN_VEBOX:
        m_pipelineParam[0].filter_flags |= VA_FILTER_SCALING_DEFAULT;
        m_pipelineParam[0].filter_flags |= interpolation[pParams->interpolationMethod];
        break;
    case MFX_SCALING_MODE_DEFAULT:
    default:
            /* Force AVS by default for all platforms except BXT */
        m_pipelineParam[0].filter_flags |= VA_FILTER_SCALING_HQ;
        m_pipelineParam[0].filter_flags |= interpolation[pParams->interpolationMethod];
        mfxSts = ((pParams->interpolationMethod == MFX_INTERPOLATION_DEFAULT) || (pParams->interpolationMethod == MFX_INTERPOLATION_ADVANCED)) ? MFX_ERR_NONE : MFX_ERR_UNSUPPORTED;
        break;
    }
    MFX_CHECK_STS(mfxSts);

        uint8_t& chromaSitingMode = m_pipelineParam[0].input_color_properties.chroma_sample_location;
        chromaSitingMode = VA_CHROMA_SITING_UNKNOWN;

        switch (pParams->chromaSiting)
        {
        case MFX_CHROMA_SITING_HORIZONTAL_LEFT | MFX_CHROMA_SITING_VERTICAL_TOP:
            //Option A : Chroma samples are aligned horizontally and vertically with multiples of the luma samples
            chromaSitingMode = VA_CHROMA_SITING_HORIZONTAL_LEFT | VA_CHROMA_SITING_VERTICAL_TOP;
            break;
        case MFX_CHROMA_SITING_HORIZONTAL_LEFT | MFX_CHROMA_SITING_VERTICAL_CENTER:
            //Option AB : Chroma samples are vertically centered between, but horizontally aligned with luma samples.
            chromaSitingMode = VA_CHROMA_SITING_HORIZONTAL_LEFT | VA_CHROMA_SITING_VERTICAL_CENTER;
            break;
        case MFX_CHROMA_SITING_HORIZONTAL_LEFT | MFX_CHROMA_SITING_VERTICAL_BOTTOM:
            //Option B : Chroma samples are horizontally aligned and vertically 1 pixel offset to the bottom.
            chromaSitingMode = VA_CHROMA_SITING_HORIZONTAL_LEFT | VA_CHROMA_SITING_VERTICAL_BOTTOM;
            break;
        case MFX_CHROMA_SITING_HORIZONTAL_CENTER | MFX_CHROMA_SITING_VERTICAL_CENTER:
            //Option ABCD : Chroma samples are centered between luma samples both horizontally and vertically.
            chromaSitingMode = VA_CHROMA_SITING_HORIZONTAL_CENTER | VA_CHROMA_SITING_VERTICAL_CENTER;
            break;
        case MFX_CHROMA_SITING_HORIZONTAL_CENTER | MFX_CHROMA_SITING_VERTICAL_TOP:
            //Option AC : Chroma samples are vertically aligned with, and horizontally centered between luma
            chromaSitingMode = VA_CHROMA_SITING_HORIZONTAL_CENTER | VA_CHROMA_SITING_VERTICAL_TOP;
            break;
        case MFX_CHROMA_SITING_HORIZONTAL_CENTER | MFX_CHROMA_SITING_VERTICAL_BOTTOM:
            //Option BD : Chroma samples are horizontally 0.5 pixel offset to the right and vertically 1 pixel offset to the bottom.
            chromaSitingMode = VA_CHROMA_SITING_HORIZONTAL_CENTER | VA_CHROMA_SITING_VERTICAL_BOTTOM;
            break;
        case MFX_CHROMA_SITING_UNKNOWN:
        default:
            break;
        }
        m_pipelineParam[0].output_color_properties.chroma_sample_location = chromaSitingMode;

if (pParams->mirroringExt)
{
    // First priority is HW fixed function scaling engine. If it can't work, revert to AVS
    m_pipelineParam[0].filter_flags = VA_FILTER_SCALING_DEFAULT;

    switch (pParams->mirroring)
    {
    case MFX_MIRRORING_VERTICAL:
        m_pipelineParam[0].mirror_state = VA_MIRROR_VERTICAL;
        break;
    case MFX_MIRRORING_HORIZONTAL:
        m_pipelineParam[0].mirror_state = VA_MIRROR_HORIZONTAL;
        break;
    }
}

    // Additional parameters for interlaced cases
    {
        if (pParams->targetSurface.frameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF &&
            pRefSurf->frameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF)
        {
            m_pipelineParam[0].input_surface_flag = VA_TOP_FIELD_FIRST;
            m_pipelineParam[0].output_surface_flag = VA_TOP_FIELD_FIRST;
        }
        if (pParams->targetSurface.frameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF &&
            pRefSurf->frameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF)
        {
            m_pipelineParam[0].input_surface_flag = VA_BOTTOM_FIELD_FIRST;
            m_pipelineParam[0].output_surface_flag = VA_BOTTOM_FIELD_FIRST;
        }
    }

    vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextVPP,
                        VAProcPipelineParameterBufferType,
                        sizeof(VAProcPipelineParameterBuffer),
                        1,
                        &m_pipelineParam[0],
                        &m_pipelineParamID[0]);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    // increase deinterlacer frame count after frame has been processed
    m_deintFrameCount ++;

    // for ADI 30i->60p, EOS is received on second field after vpp_reset
    // reset m_deintFrameCount to zero after processing second field
    if((pParams->bEOS) && (pParams->bDeinterlace30i60p == true))
        m_deintFrameCount = 0;

    VASurfaceID *outputSurface = (VASurfaceID*)(pParams->targetSurface.hdl.first);

    MFX_LTRACE_2(MFX_TRACE_LEVEL_HOTSPOTS, "A|VPP|FILTER|PACKET_START|", "%d|%d", m_vaContextVPP, 0);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaBeginPicture");
        vaSts = vaBeginPicture(m_vaDisplay,
                            m_vaContextVPP,
                            *outputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
    if (trigger.id != VA_INVALID_ID)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaRenderPicture");
        vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &trigger.id, 1);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
#endif

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaRenderPicture");
        vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamID[0], 1);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaEndPicture");
        vaSts = vaEndPicture(m_vaDisplay, m_vaContextVPP);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    MFX_LTRACE_2(MFX_TRACE_LEVEL_HOTSPOTS, "A|VPP|FILTER|PACKET_END|", "%d|%d", m_vaContextVPP, 0);

    for (VABufferID& id : m_pipelineParamID)
    {
        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, id);
        MFX_CHECK_STS(mfxSts);
    }

    mfxSts = RemoveBufferFromPipe(m_deintFilterID);
    MFX_CHECK_STS(mfxSts);
    // (3) info needed for sync operation
    //-------------------------------------------------------
    {
        // UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback; // {surface & number_of_task}
        currentFeedback.surface = *outputSurface;
        currentFeedback.number = pParams->statusReportID;
        m_feedbackCache.push_back(currentFeedback);
    }

    mfxSts = RemoveBufferFromPipe(m_frcFilterID);
    MFX_CHECK_STS(mfxSts);

    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoProcessing::Execute(FASTCOMP_BLT_PARAMS *pVideoCompositingBlt)

mfxStatus VAAPIVideoProcessing::RemoveBufferFromPipe(VABufferID& id)
{
    if (id != VA_INVALID_ID)
    {
        VABufferID tmp = id;
        mfxStatus sts = CheckAndDestroyVAbuffer(m_vaDisplay, id);
        MFX_CHECK_STS(sts);

        std::remove(m_filterBufs, m_filterBufs + m_numFilterBufs, tmp);
        m_filterBufs[m_numFilterBufs] = VA_INVALID_ID;
        --m_numFilterBufs;
    }

    return MFX_ERR_NONE;
}


BOOL    VAAPIVideoProcessing::isVideoWall(mfxExecuteParams *pParams)
{
    BOOL result = false;
    mfxU16 outputWidth;
    mfxU16 outputHeight;
    mfxU16 layerWidth  = 0;
    mfxU16 layerHeight = 0;
    mfxU16 numX = 0;
    mfxU16 numY = 0;
    std::vector <mfxU32> indexVW;
    std::map<mfxU16, compStreamElem> layout;

    mfxU32 layerCount = (mfxU32) pParams->fwdRefCount + 1;

    if ( layerCount % MAX_STREAMS_PER_TILE != 0 )
    {
        /* Number of streams must be multiple of 8. That's just for simplicity.
         * Need to handle the case when number is not multiple of 8*/
        return result;
    }

    indexVW.reserve(layerCount);
    outputWidth  = pParams->targetSurface.frameInfo.CropW;
    outputHeight = pParams->targetSurface.frameInfo.CropH;

    /**/
    layerWidth = pParams->dstRects[0].DstW;
    numX = outputWidth / layerWidth;
    layerHeight = pParams->dstRects[0].DstH;
    numY = outputHeight / layerHeight;

    // Set up perfect layout of the video wall
    for ( unsigned int j = 0; j < layerCount; j++ )
    {
        mfxU16 render_order = (j/numX)*numX + (j % numX);
        compStreamElem element;
        element.x  = (j % numX)*layerWidth;
        element.y  = (j/numX)*layerHeight;
        layout.insert(std::pair<mfxU16, compStreamElem>(render_order, element));
    }

    for ( unsigned int i = 0; i < layerCount; i++)
    {
        std::map<mfxU16, compStreamElem>::iterator it;
        DstRect rect = pParams->dstRects[i];

        if ( outputWidth % rect.DstW != 0 )
        {
            /* Layer width should fit output */
            return result;
        }

        if ( layerWidth != rect.DstW)
        {
            /* All layers must have the same width */
            return result;
        }

        if ( outputHeight % rect.DstH != 0 )
        {
            /* Layer height should fit output */
            return result;
        }

        if ( layerHeight != rect.DstH)
        {
            /* All layers must have the same height */
            return result;
        }

        mfxU16 render_order = 0;
        render_order  = rect.DstX/(outputWidth/numX);
        render_order += numX * (rect.DstY/(outputHeight/numY));
        it = layout.find(render_order);
        if ( layout.end() == it )
        {
            /* All layers must have the same height */
            return result;
        }
        else if (it->second.active)
        {
            /* Slot is busy with another layer already */
            return result;
        }
        else if (it->second.x != rect.DstX || it->second.y != rect.DstY)
        {
            /* Layers should be ordered in proper way allowing partial rendering on output */
            return result;
        }
        else
        {
            it->second.active = true;
            it->second.index  = i;
            indexVW.push_back(render_order);
        }
    }

    if ( numX > 1 && numX % 2 != 0 )
    {
        /* Number of layers on X-axis is not correct */
        return result;
    }

    if ( numY % 2 != 0 )
    {
        /* Number of layers on Y-axis is not correct */
        return result;
    }

    result = true;

    pParams->iTilesNum4Comp = layerCount / MAX_STREAMS_PER_TILE;
    for ( unsigned int i = 0; i < layerCount; i++)
    {
        pParams->dstRects[i].TileId = indexVW[i] / MAX_STREAMS_PER_TILE;
    }

    return result;
}

#ifdef MFX_ENABLE_VPP_COMPOSITION
mfxStatus VAAPIVideoProcessing::Execute_Composition_TiledVideoWall(mfxExecuteParams *pParams)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VAAPIVideoProcessing::Execute_Composition_TiledVideoWall");

    mfxStatus sts;
    VAStatus vaSts = VA_STATUS_SUCCESS;
    std::vector<VABlendState> blend_state;

    MFX_CHECK_NULL_PTR1( pParams );
    MFX_CHECK_NULL_PTR1( pParams->targetSurface.hdl.first );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces[0].hdl.first );

    if ( 0 == pParams->iTilesNum4Comp )
    {
        return MFX_ERR_UNKNOWN;
    }
    mfxU32 layerCount = (mfxU32) pParams->fwdRefCount + 1;

    std::vector<m_tiledVideoWallParams> tilingParams;
    tilingParams.resize(pParams->iTilesNum4Comp);
    for ( unsigned int i = 0; i < tilingParams.size(); i++)
    {
        tilingParams[i].targerRect.x = tilingParams[i].targerRect.y = 0x7fff;
        tilingParams[i].targerRect.width = tilingParams[i].targerRect.height = 0;
    }

    m_pipelineParam.resize(pParams->refCount + 1);
    m_pipelineParamID.resize(pParams->refCount + 1, VA_INVALID_ID);
    blend_state.resize(pParams->refCount + 1);
    std::vector<VARectangle> input_region;
    input_region.resize(pParams->refCount + 1);
    std::vector<VARectangle> output_region;
    output_region.resize(pParams->refCount + 1);

    /* Initial set up for layers */
    for ( unsigned int i = 0; i < layerCount; i++)
    {
        mfxDrvSurface* pRefSurf = &(pParams->pRefSurfaces[i]);
        VASurfaceID* srf        = (VASurfaceID*)(pRefSurf->hdl.first);

        m_pipelineParam[i].surface = *srf;

        // source cropping
        mfxU32  refFourcc = pRefSurf->frameInfo.FourCC;
        switch (refFourcc)
        {
            case MFX_FOURCC_RGB4:
                m_pipelineParam[i].surface_color_standard = VAProcColorStandardNone;
                break;
            case MFX_FOURCC_NV12:
            default:
                m_pipelineParam[i].surface_color_standard = VAProcColorStandardBT601;
                break;
        }

        mfxU32  targetFourcc = pParams->targetSurface.frameInfo.FourCC;
        switch (targetFourcc)
        {
            case MFX_FOURCC_RGB4:
                m_pipelineParam[i].output_color_standard = VAProcColorStandardNone;
                break;
            case MFX_FOURCC_NV12:
            default:
                m_pipelineParam[i].output_color_standard = VAProcColorStandardBT601;
                break;
        }

        if(pParams->VideoSignalInfo[i].enabled)
        {
            if(pParams->VideoSignalInfo[i].TransferMatrix != MFX_TRANSFERMATRIX_UNKNOWN)
            {
                m_pipelineParam[i].surface_color_standard = (MFX_TRANSFERMATRIX_BT709 == pParams->VideoSignalInfo[i].TransferMatrix) ? VAProcColorStandardBT709 : VAProcColorStandardBT601;
            }

            if(pParams->VideoSignalInfo[i].NominalRange != MFX_NOMINALRANGE_UNKNOWN)
            {
                m_pipelineParam[i].input_color_properties.color_range = (MFX_NOMINALRANGE_0_255 == pParams->VideoSignalInfo[i].NominalRange) ? VA_SOURCE_RANGE_FULL : VA_SOURCE_RANGE_REDUCED;
            }
        }

        switch (pRefSurf->frameInfo.PicStruct)
        {
            case MFX_PICSTRUCT_PROGRESSIVE:
                m_pipelineParam[i].filter_flags = VA_FRAME_PICTURE;
                break;
            case MFX_PICSTRUCT_FIELD_TFF:
                m_pipelineParam[i].filter_flags = VA_TOP_FIELD;
                break;
            case MFX_PICSTRUCT_FIELD_BFF:
                m_pipelineParam[i].filter_flags = VA_BOTTOM_FIELD;
                break;
        }

        /* to process input parameters of sub stream:
         * crop info and original size*/
        mfxFrameInfo *inInfo              = &(pRefSurf->frameInfo);
        input_region[i].y                 = inInfo->CropY;
        input_region[i].x                 = inInfo->CropX;
        input_region[i].height            = inInfo->CropH;
        input_region[i].width             = inInfo->CropW;
        m_pipelineParam[i].surface_region = &input_region[i];

        /* to process output parameters of sub stream:
         *  position and destination size */
        output_region[i].y               = pParams->dstRects[i].DstY;
        output_region[i].x               = pParams->dstRects[i].DstX;
        output_region[i].height          = pParams->dstRects[i].DstH;
        output_region[i].width           = pParams->dstRects[i].DstW;
        m_pipelineParam[i].output_region = &output_region[i];

        mfxU32 currTileId = pParams->dstRects[i].TileId;

        if (((currTileId > (pParams->iTilesNum4Comp - 1) )) ||
            (tilingParams[currTileId].numChannels >=8))
        {
            // Maximum number channels in tile is 8
            // fallback case
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
        else
        {
            tilingParams[currTileId].channelIds[tilingParams[currTileId].numChannels] = i;
            tilingParams[currTileId].numChannels++;
            /* lets define tiles working rectangle */
            if (tilingParams[currTileId].targerRect.x > output_region[i].x)
                tilingParams[currTileId].targerRect.x = output_region[i].x;
            if (tilingParams[currTileId].targerRect.y > output_region[i].y)
                tilingParams[currTileId].targerRect.y = output_region[i].y;
            if (tilingParams[currTileId].targerRect.width <
                    (output_region[i].x + output_region[i].width) )
                tilingParams[currTileId].targerRect.width = output_region[i].x + output_region[i].width;
            if (tilingParams[currTileId].targerRect.height <
                    (output_region[i].y + output_region[i].height) )
                tilingParams[currTileId].targerRect.height = output_region[i].y + output_region[i].height;
        }

        /* Global alpha and luma key can not be enabled together*/
        if (pParams->dstRects[i].GlobalAlphaEnable !=0)
        {
            blend_state[i].flags = VA_BLEND_GLOBAL_ALPHA;
            blend_state[i].global_alpha = ((float)pParams->dstRects[i].GlobalAlpha) /255;
        }
        /* Luma color key  for YUV surfaces only.
         * And Premultiplied alpha blending for RGBA surfaces only.
         * So, these two flags can't combine together  */
        if ((pParams->dstRects[i].LumaKeyEnable    != 0) &&
            (pParams->dstRects[i].PixelAlphaEnable == 0) )
        {
            blend_state[i].flags |= VA_BLEND_LUMA_KEY;
            blend_state[i].min_luma = ((float)pParams->dstRects[i].LumaKeyMin/255);
            blend_state[i].max_luma = ((float)pParams->dstRects[i].LumaKeyMax/255);
        }
        if ((pParams->dstRects[i].LumaKeyEnable    == 0 ) &&
            (pParams->dstRects[i].PixelAlphaEnable != 0 ) )
        {
            blend_state[i].flags |= VA_BLEND_PREMULTIPLIED_ALPHA;
        }
        if ((pParams->dstRects[i].GlobalAlphaEnable != 0) ||
            (pParams->dstRects[i].LumaKeyEnable     != 0) ||
            (pParams->dstRects[i].PixelAlphaEnable  != 0))
        {
            m_pipelineParam[i].blend_state = &blend_state[i];
        }

        m_pipelineParam[i].pipeline_flags |= VA_PROC_PIPELINE_SUBPICTURES;
        m_pipelineParam[i].filter_flags   |= VA_FILTER_SCALING_HQ;

        m_pipelineParam[i].filters      = 0;
        m_pipelineParam[i].num_filters  = 0;
        m_pipelineParam[i].output_background_color = 0xff000000;

        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextVPP,
                            VAProcPipelineParameterBufferType,
                            sizeof(VAProcPipelineParameterBuffer),
                            1,
                            &m_pipelineParam[i],
                            &m_pipelineParamID[i]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    VASurfaceID *outputSurface = (VASurfaceID*)(pParams->targetSurface.hdl.first);
    VAProcPipelineParameterBuffer outputparam = {};
    VABufferID vpp_pipeline_outbuf = VA_INVALID_ID;

    MFX_CHECK_NULL_PTR1(outputSurface);

    /* Process by groups. Video wall case assumes
     * that surfaces has the same output dimensions
     * We split output by several horizontal tiles */
    for (unsigned int currTile = 0; currTile < tilingParams.size(); currTile++)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaBeginPicture");
        vaSts = vaBeginPicture(m_vaDisplay,
                               m_vaContextVPP,
                               *outputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        outputparam.surface = *outputSurface;
        // The targerRect.width and targerRect.height here actually storing the x2 and y2
        // value. Deduct x and y respectively to get the exact targerRect.width and
        // targerRect.height
        tilingParams[currTile].targerRect.width  -= tilingParams[currTile].targerRect.x;
        tilingParams[currTile].targerRect.height -= tilingParams[currTile].targerRect.y;

        outputparam.output_region  = &tilingParams[currTile].targerRect;
        outputparam.surface_region = &tilingParams[currTile].targerRect;
        outputparam.output_background_color = 0;

        vaSts = vaCreateBuffer(m_vaDisplay,
                                  m_vaContextVPP,
                                   VAProcPipelineParameterBufferType,
                                   sizeof(VAProcPipelineParameterBuffer),
                                   1,
                                   &outputparam,
                                   &vpp_pipeline_outbuf);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &vpp_pipeline_outbuf, 1);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        for (unsigned int i = 0; i < tilingParams[currTile].numChannels; i++)
        {
            vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamID[tilingParams[currTile].channelIds[i]], 1);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaEndPicture");
            vaSts = vaEndPicture(m_vaDisplay, m_vaContextVPP);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }

        sts = CheckAndDestroyVAbuffer(m_vaDisplay, vpp_pipeline_outbuf);
        MFX_CHECK_STS(sts);
    }

    for (VABufferID& id : m_pipelineParamID)
    {
        sts = CheckAndDestroyVAbuffer(m_vaDisplay, id);
        MFX_CHECK_STS(sts);
    }

    // (3) info needed for sync operation
    //-------------------------------------------------------
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback; // {surface & number_of_task}
        currentFeedback.surface = *outputSurface;
        currentFeedback.number  = pParams->statusReportID;
        m_feedbackCache.push_back(currentFeedback);
    }

    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoProcessing::Execute_Composition_TileVideoWall(mfxExecuteParams *pParams)

mfxStatus VAAPIVideoProcessing::Execute_Composition(mfxExecuteParams *pParams)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VAAPIVideoProcessing::Execute_Composition");

    VAStatus vaSts = VA_STATUS_SUCCESS;
    VASurfaceAttrib attrib;
    VAImage imagePrimarySurface;
    mfxU8* pPrimarySurfaceBuffer;
    std::vector<VABlendState> blend_state;

    MFX_CHECK_NULL_PTR1( pParams );
    MFX_CHECK_NULL_PTR1( pParams->targetSurface.hdl.first );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces[0].hdl.first );

    mfxU32 refCount = (mfxU32) pParams->fwdRefCount;
    bool hasResize = false;

    for(mfxU32 i = 0; i < refCount; i++)
    {
        // Check if there is a resize for input streams
        mfxFrameInfo *surf_info = &(pParams->pRefSurfaces[i].frameInfo);
        if (surf_info->CropW != pParams->dstRects[i].DstW ||
            surf_info->CropH != pParams->dstRects[i].DstH){
            hasResize = true;
            break;
        }
    }

    if ((m_primarySurface4Composition == NULL) && (pParams->bBackgroundRequired))
    {
        mfxDrvSurface* pRefSurf = &(pParams->targetSurface);
        mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);

        m_primarySurface4Composition = (VASurfaceID*)calloc(1,1*sizeof(VASurfaceID));
        /* required to check, is memory allocated o not  */
        if (m_primarySurface4Composition == NULL)
            return MFX_ERR_MEMORY_ALLOC;

        attrib.type = VASurfaceAttribPixelFormat;
        attrib.value.type = VAGenericValueTypeInteger;

        unsigned int rt_format;

        // default format is NV12
        if (inInfo->FourCC == MFX_FOURCC_RGB4)
        {
            attrib.value.value.i = VA_FOURCC_ARGB;
            rt_format = VA_RT_FORMAT_RGB32;
        }
        else if(inInfo->FourCC == MFX_FOURCC_P010
                || inInfo->FourCC == MFX_FOURCC_P210
                || inInfo->FourCC == MFX_FOURCC_Y210
                || inInfo->FourCC == MFX_FOURCC_Y410)
        {
            attrib.value.value.i = VA_FOURCC_P010; // We're going to flood fill this surface, so let's use most common 10-bit format
            rt_format = VA_RT_FORMAT_YUV420_10BPP;
        }
        else if(inInfo->FourCC == MFX_FOURCC_P016
                || inInfo->FourCC == MFX_FOURCC_Y216
                || inInfo->FourCC == MFX_FOURCC_Y416)
        {
            attrib.value.value.i = VA_FOURCC_P016; // We're going to flood fill this surface, so let's use most common 10-bit format
            rt_format = VA_RT_FORMAT_YUV420_10BPP;
        }
        else
        {
            attrib.value.value.i = VA_FOURCC_NV12;
            rt_format = VA_RT_FORMAT_YUV420;
        }
        attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;

        // Check what resolution is better. If input surfaces are going to be resized, then
        // it's better to allocate small surface for background. If there is no resize for
        // input streams, then it's better to allocate surface with the resolution equal to
        // the output stream to eliminate resize for background surface.
        mfxU32 width  = hasResize ? VPP_COMP_BACKGROUND_SURFACE_WIDTH  : inInfo->Width;
        mfxU32 height = hasResize ? VPP_COMP_BACKGROUND_SURFACE_HEIGHT : inInfo->Height;

        vaSts = vaCreateSurfaces(m_vaDisplay, rt_format, width, height,
                m_primarySurface4Composition, 1, &attrib, 1);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        for ( int iPrSurfCount = 0; iPrSurfCount < 1; iPrSurfCount++)
        {
            vaSts = vaDeriveImage(m_vaDisplay, m_primarySurface4Composition[iPrSurfCount], &imagePrimarySurface);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            vaSts = vaMapBuffer(m_vaDisplay, imagePrimarySurface.buf, (void **) &pPrimarySurfaceBuffer);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            IppiSize roiSize;
            roiSize.width = imagePrimarySurface.width;
            roiSize.height = imagePrimarySurface.height;

            /* We need to fill up empty surface by background color...
             * iBackgroundColor is now U64, with 16 bits per channel (see mfx_vpp_hw.cpp)
             * it is easy for ARGB format as Initial background value ARGB*/
            if (imagePrimarySurface.format.fourcc == VA_FOURCC_ARGB)
            {
                Ipp32u A, R, G, B;
                Ipp32u iBackgroundColorRGBA;

                A = (Ipp32u)((pParams->iBackgroundColor >> 48) & 0x00ff);
                R = (Ipp32u)((pParams->iBackgroundColor >> 32) & 0x00ff);
                G = (Ipp32u)((pParams->iBackgroundColor >> 16) & 0x00ff);
                B = (Ipp32u)((pParams->iBackgroundColor >>  0) & 0x00ff);

                iBackgroundColorRGBA = (A << 24) | (R << 16) | (G << 8) | (B << 0);

                bool setPlaneSts = SetPlaneROI<Ipp32u>(iBackgroundColorRGBA, (Ipp32u *)pPrimarySurfaceBuffer, imagePrimarySurface.pitches[0], roiSize);
                MFX_CHECK(setPlaneSts, MFX_ERR_DEVICE_FAILED);
            }
            /* A bit more complicated for NV12 as you need to do conversion ARGB => NV12 */
            if (imagePrimarySurface.format.fourcc == VA_FOURCC_NV12)
            {
                Ipp32u Y = (Ipp32u)((pParams->iBackgroundColor >> 32) & 0x00ff);
                Ipp32u U = (Ipp32u)((pParams->iBackgroundColor >> 16) & 0x00ff);
                Ipp32u V = (Ipp32u)((pParams->iBackgroundColor >>  0) & 0x00ff);

                uint8_t valueY = (uint8_t) Y;
                int16_t valueUV = (int16_t)((V<<8)  + U); // Keep in mind that short is stored in memory using little-endian notation

                bool setPlaneSts = SetPlaneROI<Ipp8u>(valueY, pPrimarySurfaceBuffer, imagePrimarySurface.pitches[0], roiSize);
                MFX_CHECK(setPlaneSts, MFX_ERR_DEVICE_FAILED);

                // NV12 format -> need to divide height 2 times less
                roiSize.height = roiSize.height/2;
                // "UV" this is short (16 bit) value already
                // so need to divide width 2 times less too!
                roiSize.width = roiSize.width/2;
                setPlaneSts = SetPlaneROI<Ipp16s>(valueUV, (Ipp16s *)(pPrimarySurfaceBuffer + imagePrimarySurface.offsets[1]),
                                                imagePrimarySurface.pitches[1], roiSize);
                MFX_CHECK(setPlaneSts, MFX_ERR_DEVICE_FAILED);
            }

            if (imagePrimarySurface.format.fourcc == VA_FOURCC_P010
                || imagePrimarySurface.format.fourcc == VA_FOURCC_P016)
            {
                uint32_t Y=0;
                uint32_t U=0;
                uint32_t V=0;
                if(imagePrimarySurface.format.fourcc == VA_FOURCC_P010 )
                {
                    Y = (uint32_t)((pParams->iBackgroundColor >> 26) & 0xffC0);
                    U = (uint32_t)((pParams->iBackgroundColor >> 10) & 0xffC0);
                    V = (uint32_t)((pParams->iBackgroundColor <<  6) & 0xffC0);
                }
                else
                {
                    // 12 bit depth is used for these CCs
                    Y = (uint32_t)((pParams->iBackgroundColor >> 28) & 0xfff0);
                    U = (uint32_t)((pParams->iBackgroundColor >> 12) & 0xfff0);
                    V = (uint32_t)((pParams->iBackgroundColor <<  4) & 0xfff0);
                }

                uint16_t valueY = (uint16_t)Y;
                uint32_t valueUV = (int32_t)((V << 16) + U); // Keep in mind that short is stored in memory using little-endian notation

                bool setPlaneSts = SetPlaneROI<uint16_t>(valueY, (uint16_t*)pPrimarySurfaceBuffer, imagePrimarySurface.pitches[0], roiSize);
                MFX_CHECK(setPlaneSts, MFX_ERR_DEVICE_FAILED);

                // NV12 format -> need to divide height 2 times less
                roiSize.height = roiSize.height / 2;
                // "UV" encodes 2 pixels in a row
                // so need to divide width 2 times
                roiSize.width = roiSize.width / 2;
                setPlaneSts = SetPlaneROI<uint32_t>(valueUV, (uint32_t *)(pPrimarySurfaceBuffer + imagePrimarySurface.offsets[1]),
                    imagePrimarySurface.pitches[1], roiSize);
                MFX_CHECK(setPlaneSts, MFX_ERR_DEVICE_FAILED);
            }

            vaSts = vaUnmapBuffer(m_vaDisplay, imagePrimarySurface.buf);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            vaSts = vaDestroyImage(m_vaDisplay, imagePrimarySurface.image_id);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        } // for ( int iPrSurfCount = 0; iPrSurfCount < 3; iPrSurfCount++)
    }

    /* pParams->refCount is total number of processing surfaces:
     * in case of composition this is primary + sub streams*/

    mfxU32 SampleCount = 1;
    mfxU32 refIdx = 0;

    m_pipelineParam.resize(pParams->refCount + 1);
    m_pipelineParamID.resize(pParams->refCount + 1, VA_INVALID_ID);
    blend_state.resize(pParams->refCount + 1);
    std::vector<VARectangle> input_region;
    input_region.resize(pParams->refCount + 1);
    std::vector<VARectangle> output_region;
    output_region.resize(pParams->refCount + 1);
    VASurfaceID *outputSurface = (VASurfaceID*)(pParams->targetSurface.hdl.first);

    for( refIdx = 0; refIdx < SampleCount; refIdx++ )
    {
        mfxDrvSurface* pRefSurf = &(pParams->targetSurface);
        memset(&m_pipelineParam[refIdx], 0, sizeof(m_pipelineParam[refIdx]));

        //VASurfaceID* srf_1 = (VASurfaceID*)(pRefSurf->hdl.first);
        //m_pipelineParam[refIdx].surface = *srf_1;
        /* First "primary" surface should be our allocated empty surface filled by background color.
         * Else we can not process first input surface as usual one */
        if (pParams->bBackgroundRequired)
            m_pipelineParam[refIdx].surface = m_primarySurface4Composition[0];
        //VASurfaceID *outputSurface = (VASurfaceID*)(pParams->targetSurface.hdl.first);
        //m_pipelineParam[refIdx].surface = *outputSurface;

        // source cropping
        //mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);
        mfxFrameInfo *outInfo = &(pParams->targetSurface.frameInfo);
        input_region[refIdx].y   = 0;
        input_region[refIdx].x   = 0;
        input_region[refIdx].height = outInfo->CropH;
        input_region[refIdx].width  = outInfo->CropW;
        m_pipelineParam[refIdx].surface_region = &input_region[refIdx];

        // destination cropping
        //mfxFrameInfo *outInfo = &(pParams->targetSurface.frameInfo);
        output_region[refIdx].y  = 0; //outInfo->CropY;
        output_region[refIdx].x   = 0; //outInfo->CropX;
        output_region[refIdx].height= outInfo->CropH;
        output_region[refIdx].width  = outInfo->CropW;
        m_pipelineParam[refIdx].output_region = &output_region[refIdx];

        /* Actually as background color managed by "m_primarySurface4Composition" surface
         * this param will not make sense */
        //m_pipelineParam[refIdx].output_background_color = pParams->iBackgroundColor;

        mfxU32  refFourcc = pRefSurf->frameInfo.FourCC;
        switch (refFourcc)
        {
        case MFX_FOURCC_RGB4:
            m_pipelineParam[refIdx].surface_color_standard = VAProcColorStandardNone;
            break;
        case MFX_FOURCC_NV12:
        default:
            m_pipelineParam[refIdx].surface_color_standard = VAProcColorStandardBT601;
            break;
        }

        mfxU32  targetFourcc = pParams->targetSurface.frameInfo.FourCC;
        switch (targetFourcc)
        {
        case MFX_FOURCC_RGB4:
            m_pipelineParam[refIdx].output_color_standard = VAProcColorStandardNone;
            break;
        case MFX_FOURCC_NV12:
        default:
            m_pipelineParam[refIdx].output_color_standard = VAProcColorStandardBT601;
            break;
        }

        if(refIdx > 0 && pParams->VideoSignalInfo[refIdx-1].enabled)
        {
            if(pParams->VideoSignalInfo[refIdx-1].TransferMatrix != MFX_TRANSFERMATRIX_UNKNOWN)
            {
                m_pipelineParam[refIdx].surface_color_standard = (MFX_TRANSFERMATRIX_BT709 == pParams->VideoSignalInfo[refIdx-1].TransferMatrix) ? VAProcColorStandardBT709 : VAProcColorStandardBT601;
            }

            if(pParams->VideoSignalInfo[refIdx-1].NominalRange != MFX_NOMINALRANGE_UNKNOWN)
            {
                m_pipelineParam[refIdx].input_color_properties.color_range = (MFX_NOMINALRANGE_0_255 == pParams->VideoSignalInfo[refIdx-1].NominalRange) ? VA_SOURCE_RANGE_FULL : VA_SOURCE_RANGE_REDUCED;
            }
        }

        if (pParams->VideoSignalInfoOut.enabled)
        {
            if(pParams->VideoSignalInfoOut.TransferMatrix != MFX_TRANSFERMATRIX_UNKNOWN)
            {
                m_pipelineParam[refIdx].output_color_standard = (MFX_TRANSFERMATRIX_BT709 == pParams->VideoSignalInfoOut.TransferMatrix ? VAProcColorStandardBT709 : VAProcColorStandardBT601);
            }

            if(pParams->VideoSignalInfoOut.NominalRange != MFX_NOMINALRANGE_UNKNOWN)
            {
                m_pipelineParam[refIdx].output_color_properties.color_range = (MFX_NOMINALRANGE_0_255 == pParams->VideoSignalInfoOut.NominalRange) ? VA_SOURCE_RANGE_FULL : VA_SOURCE_RANGE_REDUCED;
            }
        }
        m_pipelineParam[refIdx].input_color_properties.chroma_sample_location  = VA_CHROMA_SITING_UNKNOWN;
        m_pipelineParam[refIdx].output_color_properties.chroma_sample_location = VA_CHROMA_SITING_UNKNOWN;

        switch (pRefSurf->frameInfo.PicStruct)
        {
            case MFX_PICSTRUCT_PROGRESSIVE:
            default:
                m_pipelineParam[refIdx].filter_flags = VA_FRAME_PICTURE;
                break;
        }

        m_pipelineParam[refIdx].filters  = m_filterBufs;
        m_pipelineParam[refIdx].num_filters  = m_numFilterBufs;
        /* Special case for composition:
         * as primary surface processed as sub-stream
         * pipeline and filter properties should be *_FAST */
        if (pParams->bComposite)
        {
            m_pipelineParam[refIdx].num_filters  = 0;
            m_pipelineParam[refIdx].pipeline_flags |= VA_PROC_PIPELINE_SUBPICTURES;
            m_pipelineParam[refIdx].filter_flags   |= VA_FILTER_SCALING_HQ;
        }
    }

    {
        MFX_LTRACE_2(MFX_TRACE_LEVEL_HOTSPOTS, "A|VPP|COMP|PACKET_START|", "%d|%d", m_vaContextVPP, 0);
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaBeginPicture");
            vaSts = vaBeginPicture(m_vaDisplay,
                                m_vaContextVPP,
                                *outputSurface);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }

    if (pParams->bBackgroundRequired)
    {
        refIdx = 0;
        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextVPP,
                            VAProcPipelineParameterBufferType,
                            sizeof(VAProcPipelineParameterBuffer),
                            1,
                            &m_pipelineParam[refIdx],
                            &m_pipelineParamID[refIdx]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaRenderPicture");
            for( refIdx = 0; refIdx < SampleCount; refIdx++ )
            {
                vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamID[refIdx], 1);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        }
    } // if (pParams->bBackgroundRequired)

    unsigned int uBeginPictureCounter = 0;
    std::vector<VAProcPipelineParameterBuffer> m_pipelineParamComp;
    std::vector<VABufferID> m_pipelineParamCompID;
    /* for new buffers for Begin Picture*/
    m_pipelineParamComp.resize(pParams->fwdRefCount/7);
    m_pipelineParamCompID.resize(pParams->fwdRefCount/7, VA_INVALID_ID);

    /* pParams->fwdRefCount actually is number of sub stream*/
    for( refIdx = 1; refIdx <= (refCount + 1); refIdx++ )
    {
        /*for frames 8, 15, 22, 29,... */
        if ((refIdx != 1) && ((refIdx %7) == 1) )
        {
            {
                vaSts = vaBeginPicture(m_vaDisplay,
                                    m_vaContextVPP,
                                    *outputSurface);
            }
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            /*to copy initial properties of primary surface... */
            m_pipelineParamComp[uBeginPictureCounter] = m_pipelineParam[0];
            /* ... and to In-place output*/
            //m_pipelineParamComp[uBeginPictureCounter].surface = m_primarySurface4Composition[uInputIndex];
            m_pipelineParamComp[uBeginPictureCounter].surface = *outputSurface;
            //m_pipelineParam[0].surface = *outputSurface;
            /* As used IN-PLACE variant of Composition
             * this values does not used*/
            //uOutputIndex++;
            //uInputIndex++;
            //if (uOutputIndex > 2)
            //    uOutputIndex = 0;
            //if (uInputIndex > 2)
            //    uInputIndex = 0;

            switch (pParams->targetSurface.frameInfo.FourCC)
            {
            case MFX_FOURCC_RGB4:
                m_pipelineParamComp[uBeginPictureCounter].surface_color_standard = VAProcColorStandardNone;
                break;
            case MFX_FOURCC_NV12:
            default:
                m_pipelineParamComp[uBeginPictureCounter].surface_color_standard = (MFX_TRANSFERMATRIX_BT709 == pParams->VideoSignalInfoOut.TransferMatrix) ? VAProcColorStandardBT709 : VAProcColorStandardBT601;
                break;
            }

            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextVPP,
                                VAProcPipelineParameterBufferType,
                                sizeof(VAProcPipelineParameterBuffer),
                                1,
                                &m_pipelineParamComp[uBeginPictureCounter],
                                &m_pipelineParamCompID[uBeginPictureCounter]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaBeginPicture");
            vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamCompID[uBeginPictureCounter], 1);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            uBeginPictureCounter++;
        }

        m_pipelineParam[refIdx] = m_pipelineParam[0];

        mfxDrvSurface* pRefSurf = &(pParams->pRefSurfaces[refIdx-1]);

        VASurfaceID* srf_2 = (VASurfaceID*)(pRefSurf->hdl.first);

        m_pipelineParam[refIdx].surface = *srf_2;

        mfxU32  refFourcc = pRefSurf->frameInfo.FourCC;
        switch (refFourcc)
        {
        case MFX_FOURCC_RGB4:
            m_pipelineParam[refIdx].surface_color_standard = VAProcColorStandardNone;
            break;
        case MFX_FOURCC_NV12:
        default:
            m_pipelineParam[refIdx].surface_color_standard = VAProcColorStandardBT601;
            break;
        }

        if(refIdx > 0 && pParams->VideoSignalInfo[refIdx-1].enabled)
        {
            if(pParams->VideoSignalInfo[refIdx-1].TransferMatrix != MFX_TRANSFERMATRIX_UNKNOWN)
            {
                m_pipelineParam[refIdx].surface_color_standard = (MFX_TRANSFERMATRIX_BT709 == pParams->VideoSignalInfo[refIdx-1].TransferMatrix ? VAProcColorStandardBT709 : VAProcColorStandardBT601);
            }

            if(pParams->VideoSignalInfo[refIdx-1].NominalRange != MFX_NOMINALRANGE_UNKNOWN)
            {
                m_pipelineParam[refIdx].input_color_properties.color_range = (MFX_NOMINALRANGE_0_255 == pParams->VideoSignalInfo[refIdx-1].NominalRange) ? VA_SOURCE_RANGE_FULL : VA_SOURCE_RANGE_REDUCED;
            }
        }

        if (pParams->VideoSignalInfoOut.enabled)
        {
            if(pParams->VideoSignalInfoOut.TransferMatrix != MFX_TRANSFERMATRIX_UNKNOWN)
            {
                m_pipelineParam[refIdx].output_color_standard = (MFX_TRANSFERMATRIX_BT709 == pParams->VideoSignalInfoOut.TransferMatrix ? VAProcColorStandardBT709 : VAProcColorStandardBT601);
            }

            if(pParams->VideoSignalInfoOut.NominalRange != MFX_NOMINALRANGE_UNKNOWN)
            {
                m_pipelineParam[refIdx].output_color_properties.color_range = (MFX_NOMINALRANGE_0_255 == pParams->VideoSignalInfoOut.NominalRange) ? VA_SOURCE_RANGE_FULL : VA_SOURCE_RANGE_REDUCED;
            }
        }
        m_pipelineParam[refIdx].input_color_properties.chroma_sample_location  = VA_CHROMA_SITING_UNKNOWN;
        m_pipelineParam[refIdx].output_color_properties.chroma_sample_location = VA_CHROMA_SITING_UNKNOWN;

        if (pParams->m_inVideoSignalInfo.enabled)
        {
            // Video Range
            m_pipelineParam[0].input_color_properties.color_range = (pParams->m_inVideoSignalInfo.VideoFullRange) ? VA_SOURCE_RANGE_FULL : VA_SOURCE_RANGE_REDUCED;
            switch (pParams->m_inVideoSignalInfo.ColourPrimaries)
            {
            case 9:    // BT.2020
                m_pipelineParam[0].surface_color_standard = VAProcColorStandardBT2020;
                break;
            case 1:    // BT.709
                m_pipelineParam[0].surface_color_standard = VAProcColorStandardBT709;
                break;
            case 6:
            default:
                m_pipelineParam[0].surface_color_standard = VAProcColorStandardBT601;
                break;
            }
        }

        if (pParams->m_outVideoSignalInfo.enabled)
        {
            // Video Range
            m_pipelineParam[0].output_color_properties.color_range = (pParams->m_outVideoSignalInfo.VideoFullRange) ? VA_SOURCE_RANGE_FULL : VA_SOURCE_RANGE_REDUCED;
            switch (pParams->m_outVideoSignalInfo.ColourPrimaries)
            {
            case 9:    // BT.2020
                m_pipelineParam[0].output_color_standard = VAProcColorStandardBT2020;
                break;
            case 1:    // BT.709
                m_pipelineParam[0].output_color_standard = VAProcColorStandardBT709;
                break;
            case 6:
            default:
                m_pipelineParam[0].output_color_standard = VAProcColorStandardBT601;
                break;
            }
        }

        /* to process input parameters of sub stream:
         * crop info and original size*/
        mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);
        input_region[refIdx].y   = inInfo->CropY;
        input_region[refIdx].x   = inInfo->CropX;
        input_region[refIdx].height = inInfo->CropH;
        input_region[refIdx].width  = inInfo->CropW;
        m_pipelineParam[refIdx].surface_region = &input_region[refIdx];

        /* to process output parameters of sub stream:
         *  position and destination size */
        output_region[refIdx].y  = pParams->dstRects[refIdx-1].DstY;
        output_region[refIdx].x   = pParams->dstRects[refIdx-1].DstX;
        output_region[refIdx].height= pParams->dstRects[refIdx-1].DstH;
        output_region[refIdx].width  = pParams->dstRects[refIdx-1].DstW;
        m_pipelineParam[refIdx].output_region = &output_region[refIdx];

        /* Global alpha and luma key can not be enabled together*/
        /* Global alpha and luma key can not be enabled together*/
        if (pParams->dstRects[refIdx-1].GlobalAlphaEnable !=0)
        {
            blend_state[refIdx].flags = VA_BLEND_GLOBAL_ALPHA;
            blend_state[refIdx].global_alpha = ((float)pParams->dstRects[refIdx-1].GlobalAlpha) /255;
        }
        /* Luma color key  for YUV surfaces only.
         * And Premultiplied alpha blending for RGBA surfaces only.
         * So, these two flags can't combine together  */
        if ((pParams->dstRects[refIdx-1].LumaKeyEnable != 0) &&
            (pParams->dstRects[refIdx-1].PixelAlphaEnable == 0) )
        {
            blend_state[refIdx].flags |= VA_BLEND_LUMA_KEY;
            blend_state[refIdx].min_luma = ((float)pParams->dstRects[refIdx-1].LumaKeyMin/255);
            blend_state[refIdx].max_luma = ((float)pParams->dstRects[refIdx-1].LumaKeyMax/255);
        }
        if ((pParams->dstRects[refIdx-1].LumaKeyEnable == 0 ) &&
            (pParams->dstRects[refIdx-1].PixelAlphaEnable != 0 ) )
        {
            /* Per-pixel alpha case. Having VA_BLEND_PREMULTIPLIED_ALPHA as a parameter
             * leads to using BLEND_PARTIAL approach by driver that may produce
             * "white line"-like artifacts on transparent-opaque borders.
             * Setting nothing here triggers using a BLEND_SOURCE approach that is used on
             * Windows and looks to be free of such kind of artifacts */
            blend_state[refIdx].flags |= 0;
        }
        if ((pParams->dstRects[refIdx-1].GlobalAlphaEnable != 0) ||
                (pParams->dstRects[refIdx-1].LumaKeyEnable != 0) ||
                (pParams->dstRects[refIdx-1].PixelAlphaEnable != 0))
        {
            m_pipelineParam[refIdx].blend_state = &blend_state[refIdx];
        }

        //m_pipelineParam[refIdx].pipeline_flags = ?? //VA_PROC_PIPELINE_FAST or VA_PROC_PIPELINE_SUBPICTURES
        m_pipelineParam[refIdx].pipeline_flags  |= VA_PROC_PIPELINE_FAST;
        m_pipelineParam[refIdx].filter_flags    |= VA_FILTER_SCALING_FAST;

        m_pipelineParam[refIdx].filters  = m_filterBufs;
        m_pipelineParam[refIdx].num_filters  = 0;

        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextVPP,
                            VAProcPipelineParameterBufferType,
                            sizeof(VAProcPipelineParameterBuffer),
                            1,
                            &m_pipelineParam[refIdx],
                            &m_pipelineParamID[refIdx]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaRenderPicture");
        vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamID[refIdx], 1);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        /*for frames 7, 14, 21, ...
         * or for the last frame*/
        if ( ((refIdx % 7) ==0) || ((refCount + 1) == refIdx) )
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaEndPicture");
            vaSts = vaEndPicture(m_vaDisplay, m_vaContextVPP);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    } /* for( refIdx = 1; refIdx <= (pParams->fwdRefCount); refIdx++ )*/
    MFX_LTRACE_2(MFX_TRACE_LEVEL_HOTSPOTS, "A|VPP|COMP|PACKET_END|", "%d|%d", m_vaContextVPP, 0);

    mfxStatus sts;
    for (VABufferID& id : m_pipelineParamCompID)
    {
        sts = CheckAndDestroyVAbuffer(m_vaDisplay, id);
        MFX_CHECK_STS(sts);
    }

    for (VABufferID& id : m_pipelineParamID)
    {
        sts = CheckAndDestroyVAbuffer(m_vaDisplay, id);
        MFX_CHECK_STS(sts);
    }

    // (3) info needed for sync operation
    //-------------------------------------------------------
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback; // {surface & number_of_task}
        currentFeedback.surface = *outputSurface;
        currentFeedback.number = pParams->statusReportID;
        m_feedbackCache.push_back(currentFeedback);
    }

    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoProcessing::Execute_Composition(mfxExecuteParams *pParams)
#else
mfxStatus VAAPIVideoProcessing::Execute_Composition_TiledVideoWall(mfxExecuteParams *pParams)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VAAPIVideoProcessing::Execute_Composition(mfxExecuteParams *pParams)
{
    return MFX_ERR_UNSUPPORTED;
}
#endif //#ifdef MFX_ENABLE_VPP_COMPOSITION

mfxStatus VAAPIVideoProcessing::QueryTaskStatus(SynchronizedTask* pSyncTask)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VPP DDIWaitTaskSync");
#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)
    VASurfaceID waitSurface = VA_INVALID_SURFACE;
    mfxU32 indxSurf = 0;

    // (1) find params (sutface & number) are required by feedbackNumber
    //-----------------------------------------------
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        for (indxSurf = 0; indxSurf < m_feedbackCache.size(); indxSurf++)
        {
            if (m_feedbackCache[indxSurf].number == pSyncTask->taskIndex)
            {
                waitSurface = m_feedbackCache[indxSurf].surface;
                break;
            }
        }
        if (VA_INVALID_SURFACE == waitSurface)
        {
            return MFX_ERR_UNKNOWN;
        }

        m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
    }

#if !defined(ANDROID)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaSyncSurface");
        VAStatus vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
        if (vaSts == VA_STATUS_ERROR_HW_BUSY)
            return MFX_ERR_GPU_HANG;
        else
            MFX_CHECK(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
#endif

    return MFX_TASK_DONE;
#else

    FASTCOMP_QUERY_STATUS queryStatus;

    // (1) find params (sutface & number) are required by feedbackNumber
    //-----------------------------------------------
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        std::vector<ExtVASurface>::iterator iter = m_feedbackCache.begin();
        while(iter != m_feedbackCache.end())
        {
            ExtVASurface currentFeedback = *iter;
            VASurfaceStatus surfSts = VASurfaceSkipped;

            VAStatus vaSts = vaQuerySurfaceStatus(m_vaDisplay,  currentFeedback.surface, &surfSts);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            switch (surfSts)
            {
                case VASurfaceReady:
                    queryStatus.QueryStatusID = currentFeedback.number;
                    queryStatus.Status = VPREP_GPU_READY;
                    m_cachedReadyTaskIndex.insert(queryStatus.QueryStatusID);
                    iter = m_feedbackCache.erase(iter);
                    break;
                case VASurfaceRendering:
                case VASurfaceDisplaying:
                    ++iter;
                    break;
                case VASurfaceSkipped:
                default:
                    assert(!"bad feedback status");
                    return MFX_ERR_DEVICE_FAILED;
            }
        }

        std::set<mfxU32>::iterator iterator = m_cachedReadyTaskIndex.find(pSyncTask->taskIndex);

        if (m_cachedReadyTaskIndex.end() == iterator)
        {
            return MFX_TASK_BUSY;
        }

        m_cachedReadyTaskIndex.erase(iterator);
    }

    return MFX_TASK_DONE;
#endif
} // mfxStatus VAAPIVideoProcessing::QueryTaskStatus(SynchronizedTask* pSyncTask)

#endif // #if defined (MFX_VPP_ENABLE)
/* EOF */
