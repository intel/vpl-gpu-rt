// Copyright (c) 2017-2021 Intel Corporation
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

#ifndef __MFX_VPP_VAAPI_H__
#define __MFX_VPP_VAAPI_H__

#include "umc_va_base.h"
#include "mfx_vpp_interface.h"
#include "mfx_platform_headers.h"

#include <va/va.h>
#include <va/va_vpp.h>

#include <assert.h>
#include <set>

// Driver-private filter type for AI super resolution, outside the public VAProcFilterType
// range (-1 is taken by the legacy VAProcFilter3PInfo). Not in upstream libva.
#define VA_PROC_FILTER_SUPER_RESOLUTION_PRIVATE (-2)
#define VA_SR_PRIVATE_PARAMS_VERSION 1

// Mirror of the iHD VP_DDI_SR_PARAMS layout (vp_sr_ddi_private.h); a 64-byte ABI contract.
typedef struct _VAProcFilterParameterBufferSuperResolutionPrivate
{
    int32_t  type;                       // VA_PROC_FILTER_SUPER_RESOLUTION_PRIVATE
    uint32_t version;                    // VA_SR_PRIVATE_PARAMS_VERSION
    uint32_t enable;                     // 0 / 1
    uint32_t scenario_mode;              // 0x00 default, 0x12 artifact removal (surveillance camera)
    uint32_t model_version;              // 0 default, 1 algorithm 1, 2 algorithm 2
    uint32_t input_frame_rate_numerator; // 0/0 if unknown
    uint32_t input_frame_rate_denominator;
    uint32_t va_reserved[9];             // must be zero
} VAProcFilterParameterBufferSuperResolutionPrivate;

static_assert(sizeof(VAProcFilterParameterBufferSuperResolutionPrivate) == 64,
              "VAProcFilterParameterBufferSuperResolutionPrivate is an ABI contract with iHD");

// Room for every filter buffer Execute can attach, including the private ones above.
#define VPP_MAX_FILTER_BUFS (VAProcFilterCount + 4)

namespace MfxHwVideoProcessing
{
    class VAAPIVideoProcessing : public DriverVideoProcessing
    {
    public:

        VAAPIVideoProcessing();

        virtual ~VAAPIVideoProcessing();

        virtual mfxStatus CreateDevice(VideoCORE * core, mfxVideoParam *pParams, bool isTemporal = false);

        virtual mfxStatus ReconfigDevice(mfxU32 /*indx*/) { return MFX_ERR_NONE; }

        virtual mfxStatus DestroyDevice( void );

        virtual mfxStatus Register(mfxHDLPair* pSurfaces,
                                   mfxU32 num,
                                   BOOL bRegister);

        virtual mfxStatus QueryTaskStatus(SynchronizedTask* pSyncTask);

        virtual mfxStatus QueryCapabilities( mfxVppCaps& caps );

        virtual mfxStatus QueryVariance(
            mfxU32 frameIndex,
            std::vector<mfxU32> &variance);

        virtual BOOL IsRunning() { return m_bRunning; }

        virtual mfxStatus Execute(mfxExecuteParams *pParams);

        virtual mfxStatus Execute_Composition(mfxExecuteParams *pParams);

        virtual mfxStatus Execute_Composition_TiledVideoWall(mfxExecuteParams *pParams);

        // Input layers the driver composites in one VA-API picture (batch).
        // 7 new layers + the previous batch's output = 8, the driver's
        // per-pass limit (VPHAL_COMP_MAX_LAYERS).
        static constexpr mfxU32 MAX_COMPOSITION_LAYERS_PER_BATCH = 7;

        static mfxU32 ComputeCompositionBatchCount(mfxU32 totalLayers, mfxU32 maxLayersPerBatch = MAX_COMPOSITION_LAYERS_PER_BATCH);

        static VASurfaceID* SelectCompositionBatchTarget(
            mfxU32 batchNumber,
            mfxU32 numBatches,
            VASurfaceID* outputSurface,
            VASurfaceID* stagingSurface);

    private:

        // VideoSignalInfo contains infor for backward reference, current and forward reference
        // frames. Indices in the info array are:
        //    { <indices for bwd refs>, <current>, <indices for fwd refs> }
        // For example, in case of ADI we will have: 1 bwd ref, 1 current, 0 fwd ref; indices will be:
        //    VideoSignalInfo[0] - for bwd ref
        //    VideoSignalInfo[1] - for cur reference
        inline int GetCurFrameSignalIdx(mfxExecuteParams* pParams)
        {
            if (!pParams) return -1;
            if ((size_t)pParams->bkwdRefCount < pParams->VideoSignalInfo.size()) {
                return pParams->bkwdRefCount;
            }
            return -1;
        }

        typedef struct _compositionStreamElement
        {
            mfxU16 index;
            BOOL   active;
            mfxU16 x;
            mfxU16 y;
            _compositionStreamElement()
                : index(0)
                , active(false)
                , x(0)
                , y(0)
            {};
        } compStreamElem;

        typedef struct vppTileVW{
            mfxU32 numChannels;
            VARectangle targerRect;
            mfxU8 channelIds[8];
        } m_tiledVideoWallParams;

        BOOL    isVideoWall(mfxExecuteParams *pParams);

        BOOL m_bRunning;

        VideoCORE* m_core;

        VADisplay   m_vaDisplay;
        VAConfigID  m_vaConfig;

    protected:
        VAContextID m_vaContextVPP;

    private:
        VAProcFilterCap m_denoiseCaps;
        VAProcFilterCap m_detailCaps;

        VAProcPipelineCaps           m_pipelineCaps;
        VAProcFilterCapColorBalance  m_procampCaps[VAProcColorBalanceCount];
        VAProcFilterCapDeinterlacing m_deinterlacingCaps[VAProcDeinterlacingCount];
#ifdef MFX_ENABLE_VPP_FRC
        VAProcFilterCapFrameRateConversion m_frcCaps[2]; /* only two modes, 24p->60p and 30p->60p */
        mfxU32 m_frcCyclicCounter;
#endif

        VABufferID m_denoiseFilterID;
        VABufferID m_detailFilterID;
        VABufferID m_deintFilterID;
        VABufferID m_procampFilterID;
        VABufferID m_frcFilterID;
        mfxU32     m_deintFrameCount;
        VASurfaceID m_refForFRC[5];

        VABufferID m_filterBufs[VPP_MAX_FILTER_BUFS];
        mfxU32 m_numFilterBufs;

        std::vector<VAProcPipelineParameterBuffer> m_pipelineParam;
        std::vector<VABufferID> m_pipelineParamID;

        std::set<mfxU32> m_cachedReadyTaskIndex;

        typedef struct
        {
            VASurfaceID surface;
            mfxU32 number;
        } ExtVASurface;

        VASurfaceID* m_primarySurface4Composition ;

        VASurfaceID m_compositionStagingSurface;
        mfxU32 m_compositionStagingSurfaceWidth;
        mfxU32 m_compositionStagingSurfaceHeight;

        std::vector<ExtVASurface> m_feedbackCache;

        UMC::Mutex m_guard;

        std::vector<VAProcFilterCap3DLUT> m_3dlutCaps;
        VABufferID m_3dlutFilterID;

        VABufferID m_hvsDenoiseFilterID;

        VABufferID m_hdrtmFilterID;
        VAProcFilterCapHighDynamicRange m_hdrtm_caps[VAProcHighDynamicRangeMetadataTypeCount];

        // AI super resolution through the driver-private filter buffer. Drivers without
        // it only get the legacy processing_mode=VA_PROC_MODE_HIGH_QUALITY request.
        bool       m_srPrivateFilterSupported;
        VABufferID m_srFilterID;
        VAProcFilterParameterBufferSuperResolutionPrivate m_srFilterParams;

        mfxStatus Init( _mfxPlatformAccelerationService* pVADisplay, mfxVideoParam *pParams);

        mfxStatus Close( void );

        mfxStatus RemoveBufferFromPipe(VABufferID & id);

        virtual mfxStatus ConfigHVSDenoise(mfxExecuteParams *pParams);

        mfxStatus ConfigSuperResolution(mfxExecuteParams *pParams);
    };

}; // namespace

#endif //__MFX_VPP_VAAPI_H__
#endif // MFX_ENABLE_VPP

/* EOF */
