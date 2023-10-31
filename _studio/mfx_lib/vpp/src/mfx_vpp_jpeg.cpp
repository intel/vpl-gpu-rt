// Copyright (c) 2008-2020 Intel Corporation
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
#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)

#include <algorithm>

#include "umc_va_base.h"
#include "libmfx_core.h"
#include "mfx_task.h"
#include "mfx_vpp_defs.h"
#include "mfx_vpp_jpeg.h"

using namespace MfxHwVideoProcessing;

static void MemSetZero4mfxExecuteParams (mfxExecuteParams *pMfxExecuteParams )
{
    memset(&pMfxExecuteParams->targetSurface, 0, sizeof(mfxDrvSurface));
    pMfxExecuteParams->pRefSurfaces = NULL;
    pMfxExecuteParams->dstRects.clear();
    memset(&pMfxExecuteParams->customRateData, 0, sizeof(CustomRateData));
    pMfxExecuteParams->targetTimeStamp = 0;
    pMfxExecuteParams->refCount = 0;
    pMfxExecuteParams->bkwdRefCount = 0;
    pMfxExecuteParams->fwdRefCount = 0;
    pMfxExecuteParams->iDeinterlacingAlgorithm = 0;
    pMfxExecuteParams->bFMDEnable = 0;
    pMfxExecuteParams->bDenoiseAutoAdjust = 0;
    pMfxExecuteParams->bDetailAutoAdjust = 0;
    pMfxExecuteParams->denoiseFactor = 0;
    pMfxExecuteParams->detailFactor = 0;
    pMfxExecuteParams->iTargetInterlacingMode = 0;
    pMfxExecuteParams->bEnableProcAmp = false;
    pMfxExecuteParams->Brightness = 0;
    pMfxExecuteParams->Contrast = 0;
    pMfxExecuteParams->Hue = 0;
    pMfxExecuteParams->Saturation = 0;
    pMfxExecuteParams->bVarianceEnable = false;
    pMfxExecuteParams->bImgStabilizationEnable = false;
    pMfxExecuteParams->istabMode = 0;
    pMfxExecuteParams->bFRCEnable = false;
    pMfxExecuteParams->bComposite = false;
    pMfxExecuteParams->iBackgroundColor = 0;
    pMfxExecuteParams->statusReportID = 0;
    pMfxExecuteParams->bFieldWeaving = false;
    pMfxExecuteParams->iFieldProcessingMode = 0;

    pMfxExecuteParams->rotation = 0;
    pMfxExecuteParams->scalingMode = MFX_SCALING_MODE_DEFAULT;
    pMfxExecuteParams->bEOS = false;
} /*void MemSetZero4mfxExecuteParams (mfxExecuteParams *pMfxExecuteParams )*/


enum QueryStatus
{
    VPREP_GPU_READY         =   0,
    VPREP_GPU_BUSY          =   1,
    VPREP_GPU_NOT_REACHED   =   2,
    VPREP_GPU_FAILED        =   3
};

enum 
{
    CURRENT_TIME_STAMP              = 100000,
    FRAME_INTERVAL                  = 10000,
    ACCEPTED_DEVICE_ASYNC_DEPTH     = 0x06,

};

VideoVppJpeg::VideoVppJpeg(VideoCORE *core, bool useInternalMem)
    : m_pCore(core)
    , m_IOPattern()
    , m_bUseInternalMem(useInternalMem)
    , m_taskId(1)
#ifdef MFX_ENABLE_MJPEG_ROTATE_VPP
    , m_rotation(0)
#endif
    , m_surfaces()
    , m_guard()
    , AssocIdx()
    , m_ddi()
{
    memset(&m_response, 0, sizeof(mfxFrameAllocResponse));
} // VideoVppJpeg::VideoVppJpeg(VideoCORE *core)


VideoVppJpeg::~VideoVppJpeg()
{
    Close();

} // VideoVppJpeg::~VideoVppJpeg()

mfxStatus VideoVppJpeg::Init(const mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_bUseInternalMem)
    {
        mfxFrameAllocRequest request;
        memset(&request, 0, sizeof(request));
        memset(&m_response, 0, sizeof(m_response));
        
        mfxU32 asyncDepth = (par->AsyncDepth ? par->AsyncDepth : m_pCore->GetAutoAsyncDepth());
        
        request.Info = par->mfx.FrameInfo;        
        request.NumFrameMin = mfxU16 (asyncDepth);
        request.NumFrameSuggested = request.NumFrameMin;
        request.Type = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_INTERNAL_FRAME;

        sts = m_pCore->AllocFrames(&request, &m_response, true);
        MFX_CHECK_STS(sts);

        m_surfaces.resize(m_response.NumFrameActual);

        for (mfxU32 i = 0; i < m_response.NumFrameActual; i++)
        {
            m_surfaces[i].Data.MemId = m_response.mids[i];
            m_surfaces[i].Info       = request.Info;
        }
    }

    sts = MFX_ERR_NONE;
    if (!m_ddi.GetDevice())
    {
        sts = m_ddi.CreateDevice(m_pCore);
    }

    if (sts != 0)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    mfxVppCaps caps;
    caps = m_ddi.GetCaps();

#ifdef MFX_ENABLE_MJPEG_ROTATE_VPP
    if (caps.uRotation) {
        switch(par->mfx.Rotation)
        {
        case MFX_ROTATION_0:
            m_rotation = MFX_ANGLE_0;
            break;
        case MFX_ROTATION_90:
            m_rotation = MFX_ANGLE_90;
            break;
        case MFX_ROTATION_180:
            m_rotation = MFX_ANGLE_180;
            break;
        case MFX_ROTATION_270:
            m_rotation = MFX_ANGLE_270;
            break;
        }
    } else {
        if(MFX_ROTATION_0 != par->mfx.Rotation)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }
#endif

    if(par->vpp.In.Width > caps.uMaxWidth || par->vpp.In.Height > caps.uMaxHeight ||
       par->vpp.Out.Width > caps.uMaxWidth || par->vpp.Out.Height > caps.uMaxHeight)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if( par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
        FALSE == caps.uFieldWeavingControl)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    m_IOPattern = par->IOPattern;

    return sts;

} // mfxStatus VideoVppJpeg::Init(void)

mfxStatus VideoVppJpeg::Close()
{
    mfxStatus sts = MFX_ERR_NONE;

    m_taskId = 1;

    m_ddi.Close();

    if(m_bUseInternalMem)
    {
        m_surfaces.clear();

        if (m_response.NumFrameActual)
        {
            sts = m_pCore->FreeFrames(&m_response, true);
            MFX_CHECK_STS(sts);
        }

        m_bUseInternalMem = false;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoVppJpeg::Close()

mfxStatus VideoVppJpeg::BeginHwJpegProcessing(mfxFrameSurface1 *pInputSurface, 
                                                  mfxFrameSurface1 *pOutputSurface, 
                                                  mfxU16* taskId)
{
    mfxStatus sts = MFX_ERR_NONE;
    
    mfxHDLPair hdl;
    mfxHDLPair in;
    mfxHDLPair out;

    MfxHwVideoProcessing::mfxDrvSurface m_pExecuteSurface;
    MfxHwVideoProcessing::mfxExecuteParams m_executeParams;

    memset(&m_pExecuteSurface, 0, sizeof(MfxHwVideoProcessing::mfxDrvSurface));

    if(m_bUseInternalMem)
    {
        mfxI32 index = -1; 
        for (mfxU32 i = 0; i < m_surfaces.size(); i++)
        {
            if (!m_surfaces[i].Data.Locked)
            {
                index = i;
            }
        }
        if (index == -1)
        {
            return MFX_ERR_LOCK_MEMORY;
        }
        m_pCore->IncreasePureReference(m_surfaces[index].Data.Locked);
        MFX_SAFE_CALL(m_pCore->GetFrameHDL(m_surfaces[index], hdl));

        {
            UMC::AutomaticUMCMutex guard(m_guard);
            AssocIdx.insert(std::pair<mfxMemId, mfxI32>(pInputSurface->Data.MemId, index));
        }
    }
    else
    {
        MFX_SAFE_CALL(m_pCore->GetExternalFrameHDL(*pOutputSurface, hdl));
    }

    out = hdl;

    // register output surface
    sts = (m_ddi)->Register(&out, 1, TRUE);
    MFX_CHECK_STS(sts);

    MFX_SAFE_CALL(m_pCore->GetFrameHDL(*pInputSurface, hdl));
    in = hdl;

    // register input surface
    sts = (m_ddi)->Register(&in, 1, TRUE);
    MFX_CHECK_STS(sts);

    // set input surface
    m_pExecuteSurface.hdl = static_cast<mfxHDLPair>(in);
    m_pExecuteSurface.frameInfo = pInputSurface->Info;

    // prepare the primary video sample
    m_pExecuteSurface.startTimeStamp = (mfxU64)m_taskId * CURRENT_TIME_STAMP;
    m_pExecuteSurface.endTimeStamp  = (mfxU64)m_taskId * CURRENT_TIME_STAMP + FRAME_INTERVAL;
    
    m_executeParams.targetTimeStamp = (mfxU64)m_taskId * CURRENT_TIME_STAMP;

    m_executeParams.targetSurface.memId = pOutputSurface->Data.MemId;
    m_executeParams.targetSurface.hdl = static_cast<mfxHDLPair>(out);
    m_executeParams.targetSurface.frameInfo = pOutputSurface->Info;

    m_executeParams.refCount = 1;
    m_executeParams.pRefSurfaces = &m_pExecuteSurface;
#ifdef MFX_ENABLE_MJPEG_ROTATE_VPP
    m_executeParams.rotation = m_rotation;
#endif
    m_executeParams.statusReportID = m_taskId;
    *taskId = m_taskId;
    m_taskId += 1;

    sts = (m_ddi)->Execute(&m_executeParams);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus VideoVppJpeg::BeginHwJpegProcessing(mfxFrameSurface1 *pInputSurface, mfxFrameSurface1 *pOutputSurface, mfxU16* taskId)

mfxStatus VideoVppJpeg::BeginHwJpegProcessing(mfxFrameSurface1 *pInputSurfaceTop, 
                                                  mfxFrameSurface1 *pInputSurfaceBottom, 
                                                  mfxFrameSurface1 *pOutputSurface, 
                                                  mfxU16* taskId)
{
    mfxStatus sts = MFX_ERR_NONE;
    
    mfxHDLPair hdl;
    mfxHDLPair inTop,inBottom;
    mfxHDLPair out;

    MfxHwVideoProcessing::mfxDrvSurface m_pExecuteSurface[2];
    MfxHwVideoProcessing::mfxExecuteParams m_executeParams;

    memset(&m_pExecuteSurface[0], 0, sizeof(MfxHwVideoProcessing::mfxDrvSurface));
    memset(&m_pExecuteSurface[1], 0, sizeof(MfxHwVideoProcessing::mfxDrvSurface));
    MemSetZero4mfxExecuteParams(&m_executeParams);

    if(m_bUseInternalMem)
    {
        mfxI32 index = -1; 
        for (mfxU32 i = 0; i < m_surfaces.size(); i++)
        {
            if (!m_surfaces[i].Data.Locked)
            {
                index = i;
            }
        }
        if (index == -1)
        {
            return MFX_ERR_LOCK_MEMORY;
        }
        m_pCore->IncreasePureReference(m_surfaces[index].Data.Locked);
        MFX_SAFE_CALL(m_pCore->GetFrameHDL(m_surfaces[index], hdl));

        {
            UMC::AutomaticUMCMutex guard(m_guard);
            AssocIdx.insert(std::pair<mfxMemId, mfxI32>(pInputSurfaceTop->Data.MemId, index));
        }
    }
    else
    {
        MFX_SAFE_CALL(m_pCore->GetExternalFrameHDL(*pOutputSurface, hdl));
    }

    out = hdl;

    // register output surface
    sts = (m_ddi)->Register(&out, 1, TRUE);
    MFX_CHECK_STS(sts);

    MFX_SAFE_CALL(m_pCore->GetFrameHDL(*pInputSurfaceTop, hdl));
    inTop = hdl;
    MFX_SAFE_CALL(m_pCore->GetFrameHDL(*pInputSurfaceBottom, hdl));
    inBottom = hdl;

    // register input surfaces
    sts = (m_ddi)->Register(&inTop, 1, TRUE);
    MFX_CHECK_STS(sts);
    sts = (m_ddi)->Register(&inBottom, 1, TRUE);
    MFX_CHECK_STS(sts);

    // set input surface
    m_pExecuteSurface[0].hdl = static_cast<mfxHDLPair>(inBottom);
    m_pExecuteSurface[0].frameInfo = pInputSurfaceBottom->Info;
    m_pExecuteSurface[1].hdl = static_cast<mfxHDLPair>(inTop);
    m_pExecuteSurface[1].frameInfo = pInputSurfaceTop->Info;

    // prepare the primary video sample
    m_pExecuteSurface[0].startTimeStamp = m_taskId + 1;
    m_pExecuteSurface[0].endTimeStamp  = m_taskId + 1;
    m_pExecuteSurface[1].startTimeStamp = m_taskId;
    m_pExecuteSurface[1].endTimeStamp  = m_taskId;

    m_executeParams.targetTimeStamp = m_taskId;

    m_executeParams.targetSurface.hdl = static_cast<mfxHDLPair>(out);
    m_executeParams.targetSurface.frameInfo = pOutputSurface->Info;

    m_executeParams.refCount = 2;
    m_executeParams.pRefSurfaces = m_pExecuteSurface;

    m_executeParams.statusReportID = m_taskId;

    m_executeParams.bFieldWeaving = true;
    m_executeParams.bkwdRefCount = 1;

    *taskId = m_taskId;
    m_taskId += 1;

    sts = (m_ddi)->Execute(&m_executeParams);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus VideoVppJpeg::BeginHwJpegProcessing(mfxFrameSurface1 *pInputSurfaceTop, mfxFrameSurface1 *pInputSurfaceBottom, mfxFrameSurface1 *pOutputSurface, mfxU16* taskId)

mfxStatus VideoVppJpeg::EndHwJpegProcessing(mfxFrameSurface1 *pInputSurface, mfxFrameSurface1 *pOutputSurface)
{
    mfxStatus sts = MFX_ERR_NONE;
    
    mfxHDLPair hdl;
    mfxHDLPair in;
    mfxHDLPair out;
    mfxI32 index = -1;
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_PRIVATE, "EndHwJpegProcessing");

    if(m_bUseInternalMem)
    {
        {
            UMC::AutomaticUMCMutex guard(m_guard);

            auto it = AssocIdx.find(pInputSurface->Data.MemId);
            if(it != AssocIdx.end())
            {
                index = it->second;
                AssocIdx.erase(it);
            }
        }
        MFX_CHECK(index >= 0, MFX_ERR_INVALID_VIDEO_PARAM);
        
        MFX_SAFE_CALL(m_pCore->GetFrameHDL(m_surfaces[index], hdl));
    }
    else
    {
        MFX_SAFE_CALL(m_pCore->GetExternalFrameHDL(*pOutputSurface, hdl));
    }
    out = hdl;
    
    MFX_SAFE_CALL(m_pCore->GetFrameHDL(*pInputSurface, hdl));
    in = hdl;

    if (m_bUseInternalMem)
    {
        mfxU16 outMemType = static_cast<mfxU16>((m_IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY ? MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_DXVA2_DECODER_TARGET) |
                                                MFX_MEMTYPE_EXTERNAL_FRAME);
        MFX_CHECK(index >= 0, MFX_ERR_INVALID_VIDEO_PARAM);

        sts = m_pCore->DoFastCopyWrapper(pOutputSurface, outMemType,
                                         &m_surfaces[index], MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET);
        MFX_CHECK_STS(sts);
    }

    // unregister output surface
    sts = (m_ddi)->Register(&out, 1, FALSE);
    MFX_CHECK_STS(sts);

    // unregister input surface
    sts = (m_ddi)->Register(&in, 1, FALSE);
    MFX_CHECK_STS(sts);

    if(m_bUseInternalMem)
    {
        MFX_CHECK(index >= 0, MFX_ERR_INVALID_VIDEO_PARAM);
        m_pCore->DecreasePureReference(m_surfaces[index].Data.Locked);
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoVppJpeg::EndHwJpegProcessing(mfxFrameSurface1 *pInputSurface, mfxFrameSurface1 *pOutputSurface)

mfxStatus VideoVppJpeg::EndHwJpegProcessing(mfxFrameSurface1 *pInputSurfaceTop, 
                                                mfxFrameSurface1 *pInputSurfaceBottom, 
                                                mfxFrameSurface1 *pOutputSurface)
{
    mfxStatus sts = MFX_ERR_NONE;
    
    mfxHDLPair hdl;
    mfxHDLPair inTop,inBottom;
    mfxHDLPair out;
    mfxI32 index = -1;

    if(m_bUseInternalMem)
    {
        {
            UMC::AutomaticUMCMutex guard(m_guard);

            auto it = AssocIdx.find(pInputSurfaceTop->Data.MemId);
            if(it != AssocIdx.end())
            {
                index = it->second;
                AssocIdx.erase(it);
            }
        }
        MFX_CHECK(index >= 0, MFX_ERR_INVALID_VIDEO_PARAM);
        
        MFX_SAFE_CALL(m_pCore->GetFrameHDL(m_surfaces[index], hdl));
    }
    else
    {
        MFX_SAFE_CALL(m_pCore->GetExternalFrameHDL(*pOutputSurface, hdl));
    }
    out = hdl;

    MFX_SAFE_CALL(m_pCore->GetFrameHDL(*pInputSurfaceTop, hdl));
    inTop = hdl;
    MFX_SAFE_CALL(m_pCore->GetFrameHDL(*pInputSurfaceBottom, hdl));
    inBottom = hdl;

    if (m_bUseInternalMem)
    {
        mfxU16 outMemType = static_cast<mfxU16>((m_IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY ? MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_DXVA2_DECODER_TARGET) |
                                                MFX_MEMTYPE_EXTERNAL_FRAME);
        sts = m_pCore->DoFastCopyWrapper(pOutputSurface, 
                                         outMemType,
                                         &m_surfaces[index],
                                         MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET
                                         );
        MFX_CHECK_STS(sts);
    }

    // unregister output surface
    sts = (m_ddi)->Register(&out, 1, FALSE);
    MFX_CHECK_STS(sts);

    // unregister input surface
    sts = (m_ddi)->Register(&inTop, 1, FALSE);
    MFX_CHECK_STS(sts);
    sts = (m_ddi)->Register(&inBottom, 1, FALSE);
    MFX_CHECK_STS(sts);

    if(m_bUseInternalMem)
    {
        m_pCore->DecreasePureReference(m_surfaces[index].Data.Locked);
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoVppJpeg::EndHwJpegProcessing(mfxFrameSurface1 *pInputSurfaceTop, mfxFrameSurface1 *pInputSurfaceBottom, mfxFrameSurface1 *pOutputSurface)

mfxStatus VideoVppJpeg::QueryTaskRoutine(mfxU16 taskId)
{
    mfxStatus sts = MFX_ERR_NONE;

    SynchronizedTask task={};
    task.taskIndex = taskId;

    sts = (m_ddi)->QueryTaskStatus(&task);

    return sts;

} // mfxStatus VideoVppJpeg::QueryTask(mfxU16 taskId)

#endif // MFX_ENABLE_MJPEG_VIDEO_DECODE
#endif // MFX_ENABLE_VPP
/* EOF */
