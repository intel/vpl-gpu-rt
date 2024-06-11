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

#include "asc.h"
#include "asc_cm.h"
#include "asc_defs.h"
#include "asc_cpu_dispatcher.h"
#include "libmfx_core_interface.h"
#ifdef MFX_ENABLE_ASC
#include "genx_scd_gen12lp_isa.h"
#endif
#include "tree.h"
#include "iofunctions.h"
#include "motion_estimation_engine.h"
#include <limits.h>
#include <algorithm>
#include <cmath>


static bool operator < (const mfxHDLPair & l, const mfxHDLPair & r)
{
    return (l.first == r.first) ? (l.second < r.second) : (l.first < r.first);
};


namespace ns_asc {

ASCimageData_Cm::ASCimageData_Cm() {
    gpuImage = nullptr;
    idxImage = nullptr;
}

ASC_Cm::ASC_Cm()
    : m_device(nullptr)
    , m_queue(nullptr)
    , m_pSurfaceCp(nullptr)
    , m_pIdxSurfCp(nullptr)
    , m_program(nullptr)
    , m_kernel_p(nullptr)
    , m_kernel_t(nullptr)
    , m_kernel_b(nullptr)
    , m_kernel_cp(nullptr)
    , m_threadSpace(nullptr)
    , m_threadSpaceCp(nullptr)
    , m_subSamplingEv(nullptr)
    , m_frameCopyEv(nullptr)
    , m_task(nullptr)
    , m_taskCp(nullptr)
    , m_cmDeviceAssigned(false)
    , m_tableCmRelations2()
    , m_tableCmIndex2()
{
}

mfxStatus ASC_Cm::VidSample_Alloc() {
    INT res = CM_SUCCESS;

    for (mfxI32 i = 0; i < ASCVIDEOSTATSBUF; i++)
    {
        MFX_SAFE_CALL(m_videoData[i]->layer.InitFrame(m_dataIn->layer));
        if (Query_ASCCmDevice())
        {
            res = m_device->CreateSurface2DUP(m_dataIn->layer->Extended_Width, m_dataIn->layer->Extended_Height, CM_SURFACE_FORMAT_A8, (void *)m_videoData[i]->layer.Image.data, static_cast<ASCVidSampleCm*>(m_videoData[i])->layer.gpuImage);
            SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
            res = static_cast<ASCVidSampleCm*>(m_videoData[i])->layer.gpuImage->GetIndex(static_cast<ASCVidSampleCm*>(m_videoData[i])->layer.idxImage);
            SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
        }
        else
        {
            static_cast<ASCVidSampleCm*>(m_videoData[i])->layer.gpuImage = nullptr;
            static_cast<ASCVidSampleCm*>(m_videoData[i])->layer.idxImage = nullptr;
        }
    }

    if (Query_ASCCmDevice())
    {
        mfxU32
            physicalSize = 0;
        res = m_device->GetSurface2DInfo(m_gpuwidth, m_gpuheight, CM_SURFACE_FORMAT_NV12, m_gpuImPitch, physicalSize);
        SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
        m_frameBkp = nullptr;
        m_frameBkp = (mfxU8*)memalign(0x1000, physicalSize);
        if (m_frameBkp == nullptr)
            return MFX_ERR_MEMORY_ALLOC;
        memset(m_frameBkp, 0, physicalSize);
        res = m_device->CreateSurface2DUP(m_gpuImPitch, m_gpuheight, CM_SURFACE_FORMAT_NV12, (void *)m_frameBkp, m_pSurfaceCp);
        SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
        res = m_pSurfaceCp->GetIndex(m_pIdxSurfCp);
        SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    }
    else
    {
        m_frameBkp = nullptr;
        m_pSurfaceCp = nullptr;
        m_pIdxSurfCp = nullptr;
    }
    return MFX_ERR_NONE;
}

void ASC_Cm::VidSample_dispose()
{
    for (mfxI32 i = ASCVIDEOSTATSBUF - 1; i >= 0; i--)
    {
        if (m_videoData[i] != nullptr)
        {
            m_videoData[i]->layer.Close();
            if (static_cast<ASCVidSampleCm*>(m_videoData[i])->layer.gpuImage) {
                m_device->DestroySurface2DUP(static_cast<ASCVidSampleCm*>(m_videoData[i])->layer.gpuImage);
                static_cast<ASCVidSampleCm*>(m_videoData[i])->layer.gpuImage = nullptr;
                static_cast<ASCVidSampleCm*>(m_videoData[i])->layer.idxImage = nullptr;
            }
            delete (m_videoData[i]);
        }
    }
    free(m_frameBkp);
}

mfxStatus ASC_Cm::alloc() {
    return VidSample_Alloc();
}

mfxStatus ASC_Cm::InitCPU() {
    return alloc();
}

mfxStatus ASC_Cm::InitGPUsurf(CmDevice* pCmDevice) {
#ifdef MFX_ENABLE_KERNELS
    INT res = CM_SUCCESS;
    m_subSamplingEv = nullptr;
    m_frameCopyEv   = nullptr;

    Reset_ASCCmDevice();
    m_device = pCmDevice;
    if (!m_device)
        res = CM_FAILURE;

    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    Set_ASCCmDevice();

    mfxU32 hwType = 0;
    size_t hwSize = sizeof(hwType);
    res = m_device->GetCaps(CAP_GPU_PLATFORM, hwSize, &hwType);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);

    res = m_device->CreateQueue(m_queue);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);

    switch (hwType)
    {
    case PLATFORM_INTEL_BDW:
    case PLATFORM_INTEL_SKL:
    case PLATFORM_INTEL_KBL:
    case PLATFORM_INTEL_GLK:
    case PLATFORM_INTEL_CFL:
    case PLATFORM_INTEL_CNL:
    case PLATFORM_INTEL_BXT:
    case PLATFORM_INTEL_ICL:
    case PLATFORM_INTEL_ICLLP:
        return MFX_ERR_UNSUPPORTED;
    case PLATFORM_INTEL_TGLLP:
    case PLATFORM_INTEL_RKL:
    case PLATFORM_INTEL_DG1:
    case PLATFORM_INTEL_ADL_S:
    case PLATFORM_INTEL_ADL_P:
    case PLATFORM_INTEL_ADL_N:

        res = m_device->LoadProgram((void *)genx_scd_gen12lp, sizeof(genx_scd_gen12lp), m_program, "nojitter");
        break;
    default:
        res = CM_NOT_IMPLEMENTED;
    }
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
#else
    (void)pCmDevice;
    return MFX_ERR_UNSUPPORTED;
#endif
}

mfxStatus ASC_Cm::IO_Setup() {
    mfxStatus sts = MFX_ERR_NONE;
    sts = alloc();
    SCD_CHECK_MFX_ERR(sts);
    if (Query_ASCCmDevice())
    {
        SCD_CHECK_CM_ERR(m_device->CreateKernel(m_program, CM_KERNEL_FUNCTION(surfaceCopy_Y), m_kernel_cp), MFX_ERR_DEVICE_FAILED);
        m_threadsWidth = (UINT)ceil((double)m_gpuwidth / SCD_BLOCK_PIXEL_WIDTH);
        m_threadsHeight = (UINT)ceil((double)m_gpuheight / SCD_BLOCK_HEIGHT);
        SCD_CHECK_CM_ERR(m_device->CreateThreadSpace(m_threadsWidth, m_threadsHeight, m_threadSpaceCp), MFX_ERR_DEVICE_FAILED);
        SCD_CHECK_CM_ERR(m_kernel_cp->SetThreadCount(m_threadsWidth * m_threadsHeight), MFX_ERR_DEVICE_FAILED);
#ifndef CMRT_EMU
        SCD_CHECK_CM_ERR(m_kernel_cp->AssociateThreadSpace(m_threadSpaceCp), MFX_ERR_DEVICE_FAILED);
#endif
    }
    else
    {
        m_kernel_cp = nullptr;
        m_threadSpaceCp = nullptr;
    }
    return sts;
}

mfxStatus ASC_Cm::SetKernel(SurfaceIndex *idxFrom, SurfaceIndex *idxTo, CmTask **subSamplingTask, mfxU32 parity) {
    mfxU32 argIdx = 0;
    INT res;
    //Progressive Point subsampling kernel

    CmKernel
        **subKernel = nullptr;

    if (m_dataIn->interlaceMode == ASCprogressive_frame) {
        subKernel = &m_kernel_p;
    }
    else {
        if (parity == ASCTopField)
            subKernel = &m_kernel_t;
        else if (parity == ASCBottomField)
            subKernel = &m_kernel_b;
        else
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    int
        tmp_subWidth = subWidth,
        tmp_subHeight = subHeight;

    res = (*subKernel)->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxFrom);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = (*subKernel)->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxTo);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = (*subKernel)->SetKernelArg(argIdx++, sizeof(int), &m_gpuwidth);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = (*subKernel)->SetKernelArg(argIdx++, sizeof(int), &m_gpuheight);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = (*subKernel)->SetKernelArg(argIdx++, sizeof(int), &tmp_subWidth);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = (*subKernel)->SetKernelArg(argIdx++, sizeof(int), &tmp_subHeight);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    /*if (*subSamplingTask)
    res = (*subSamplingTask)->Reset();
    else
    */res = m_device->CreateTask(*subSamplingTask);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = (*subSamplingTask)->AddKernel((*subKernel));
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

mfxStatus ASC_Cm::SetKernel(SurfaceIndex *idxFrom, CmTask **subSamplingTask, mfxU32 parity) {
    return (SetKernel(idxFrom, static_cast<ASCVidSampleCm*>(m_videoData[ASCCurrent_Frame])->layer.idxImage, subSamplingTask, parity));
}

mfxStatus ASC_Cm::SetKernel(SurfaceIndex *idxFrom, mfxU32 parity) {
    return(SetKernel(idxFrom, &m_task, parity));
}

#ifndef CMRT_EMU
mfxStatus ASC_Cm::QueueFrame(SurfaceIndex *idxFrom, SurfaceIndex *idxTo, CmEvent **subSamplingEv, CmTask **subSamplingTask, mfxU32 parity)
#else
mfxStatus ASC_Cm::QueueFrame(SurfaceIndex *idxFrom, SurfaceIndex *idxTo, CmEvent **subSamplingEv, CmTask **subSamplingTask, CmThreadSpace *subThreadSpace, mfxU32 parity)
#endif
{
if (!m_ASCinitialized)
return MFX_ERR_NOT_INITIALIZED;
*subSamplingEv = NULL;// CM_NO_EVENT;
INT res;
res = SetKernel(idxFrom, idxTo, subSamplingTask, parity);
SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
#ifndef CMRT_EMU
res = m_queue->Enqueue(*subSamplingTask, *subSamplingEv);
#else
res = m_queue->Enqueue(*subSamplingTask, *subSamplingEv, subThreadSpace);
#endif
SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
return MFX_ERR_NONE;
}

#ifndef CMRT_EMU
mfxStatus ASC_Cm::QueueFrame(SurfaceIndex *idxFrom, CmEvent **subSamplingEv, CmTask **subSamplingTask, mfxU32 parity)
#else
mfxStatus ASC_Cm::QueueFrame(SurfaceIndex *idxFrom, CmEvent **subSamplingEv, CmTask **subSamplingTask, CmThreadSpace *subThreadSpace, mfxU32 parity)
#endif
{
#ifndef CMRT_EMU
    return(QueueFrame(idxFrom, static_cast<ASCVidSampleCm*>(m_videoData[ASCCurrent_Frame])->layer.idxImage, subSamplingEv, subSamplingTask, parity));
#else
    return(QueueFrame(idxFrom, m_videoData[ASCCurrent_Frame]->layer.idxImage, subSamplingEv, subSamplingTask, subThreadSpace, parity));
#endif
}

mfxStatus ASC_Cm::QueueFrame(SurfaceIndex *idxFrom, mfxU32 parity) {
    return(
#ifndef CMRT_EMU
        QueueFrame(idxFrom, &m_subSamplingEv, &m_task, parity)
#else
        QueueFrame(idxFrom, &m_subSamplingEv, &m_task, m_threadSpace, parity)
#endif
        );
}


mfxStatus ASC_Cm::QueueFrame(mfxHDLPair frameHDL, SurfaceIndex *idxTo, CmEvent **subSamplingEv, CmTask **subSamplingTask, mfxU32 parity)
{
    if (!m_ASCinitialized)
        return MFX_ERR_NOT_INITIALIZED;
    m_videoData[ASCCurrent_Frame]->frame_number = m_videoData[ASCReference_Frame]->frame_number + 1;

    CmSurface2D* p_surfaceFrom = nullptr;
    SurfaceIndex* idxFrom = nullptr;

    CreateCmSurface2D(frameHDL, p_surfaceFrom, idxFrom);

    mfxStatus sts = QueueFrame(idxFrom, idxTo, subSamplingEv, subSamplingTask, parity);
    SCD_CHECK_MFX_ERR(sts);

    return MFX_ERR_NONE;
}

mfxStatus ASC_Cm::QueueFrame(mfxHDLPair frameHDL, CmEvent **subSamplingEv, CmTask **subSamplingTask, mfxU32 parity)
{
    if (!m_ASCinitialized)
        return MFX_ERR_NOT_INITIALIZED;
    m_videoData[ASCCurrent_Frame]->frame_number = m_videoData[ASCReference_Frame]->frame_number + 1;

    CmSurface2D* p_surfaceFrom = nullptr;
    SurfaceIndex* idxFrom = nullptr;

    CreateCmSurface2D(frameHDL, p_surfaceFrom, idxFrom);

    mfxStatus sts = QueueFrame(idxFrom, subSamplingEv, subSamplingTask, parity);
    SCD_CHECK_MFX_ERR(sts);

    return MFX_ERR_NONE;
}

mfxStatus ASC_Cm::QueueFrame(mfxHDLPair frameHDL, mfxU32 parity)
{
    if (!m_ASCinitialized)
        return MFX_ERR_NOT_INITIALIZED;
    m_videoData[ASCCurrent_Frame]->frame_number = m_videoData[ASCReference_Frame]->frame_number + 1;

    CmSurface2D* p_surfaceFrom = nullptr;
    SurfaceIndex* idxFrom = nullptr;

    CreateCmSurface2D(frameHDL, p_surfaceFrom, idxFrom);

    mfxStatus sts = QueueFrame(idxFrom, parity);
    SCD_CHECK_MFX_ERR(sts);

    return MFX_ERR_NONE;
}

mfxStatus ASC_Cm::RunFrame(SurfaceIndex *idxFrom, mfxU32 parity) {
    if (!m_ASCinitialized)
        return MFX_ERR_NOT_INITIALIZED;
    CmEvent* e = NULL;// CM_NO_EVENT;
    INT res;
    res = SetKernel(idxFrom, parity);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
#ifndef CMRT_EMU
    res = m_queue->Enqueue(m_task, e);
#else
    res = m_queue->Enqueue(m_task, e, m_threadSpace);
#endif
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = e->WaitForTaskFinished();
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_device->DestroyTask(m_task);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_queue->DestroyEvent(e);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);

    AscFrameAnalysis();

    return MFX_ERR_NONE;
}

mfxStatus ASC_Cm::RunFrame(mfxHDLPair frameHDL, mfxU32 parity)
{
    if (!m_ASCinitialized)
        return MFX_ERR_NOT_INITIALIZED;
    m_videoData[ASCCurrent_Frame]->frame_number = m_videoData[ASCReference_Frame]->frame_number + 1;

    CmSurface2D* p_surfaceFrom = 0;

    SurfaceIndex *idxFrom = nullptr;
    CreateCmSurface2D(frameHDL, p_surfaceFrom, idxFrom);

    mfxStatus sts = RunFrame(idxFrom, parity);
    SCD_CHECK_MFX_ERR(sts);

    return MFX_ERR_NONE;
}

mfxStatus ASC_Cm::CreateCmSurface2D(mfxHDLPair pSrcPair, CmSurface2D* & pCmSurface2D, SurfaceIndex* &pCmSrcIndex)
{
    INT cmSts = 0;
    std::map<mfxHDLPair, CmSurface2D *>::iterator it;
    std::map<CmSurface2D *, SurfaceIndex *>::iterator it_idx;
    it = m_tableCmRelations2.find(pSrcPair);
    if (m_tableCmRelations2.end() == it)
    {
        //UMC::AutomaticUMCMutex guard(m_guard);
        {
            cmSts = m_device->CreateSurface2D(pSrcPair, pCmSurface2D);
            SCD_CHECK_CM_ERR(cmSts, MFX_ERR_DEVICE_FAILED);
            m_tableCmRelations2.insert(std::pair<mfxHDLPair, CmSurface2D *>(pSrcPair, pCmSurface2D));
        }

        cmSts = pCmSurface2D->GetIndex(pCmSrcIndex);
        SCD_CHECK_CM_ERR(cmSts, MFX_ERR_DEVICE_FAILED);
        m_tableCmIndex2.insert(std::pair<CmSurface2D *, SurfaceIndex *>(pCmSurface2D, pCmSrcIndex));
    }
    else
    {
        pCmSurface2D = it->second;
        it_idx = m_tableCmIndex2.find(pCmSurface2D);
        if (it_idx == m_tableCmIndex2.end())
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        else
            pCmSrcIndex = it_idx->second;
    }

    return MFX_ERR_NONE;
}

mfxStatus ASC_Cm::CreateCmKernels() {
    INT res;
    m_kernel_p = NULL;
    m_kernel_t = NULL;
    m_kernel_b = NULL;
    m_threadSpace = NULL;
    m_threadSpaceCp = NULL;

    m_threadsWidth = subWidth / OUT_BLOCK;
    m_threadsHeight = subHeight;

    res = m_device->CreateThreadSpace(m_threadsWidth, m_threadsHeight, m_threadSpace);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_device->CreateKernel(m_program, CM_KERNEL_FUNCTION(SubSamplePoint_p), m_kernel_p);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_kernel_p->SetThreadCount(m_threadsWidth * m_threadsHeight);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_device->CreateKernel(m_program, CM_KERNEL_FUNCTION(SubSamplePoint_t), m_kernel_t);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_kernel_t->SetThreadCount(m_threadsWidth * m_threadsHeight);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_device->CreateKernel(m_program, CM_KERNEL_FUNCTION(SubSamplePoint_b), m_kernel_b);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_kernel_b->SetThreadCount(m_threadsWidth * m_threadsHeight);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
#ifndef CMRT_EMU
    res = m_kernel_p->AssociateThreadSpace(m_threadSpace);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_kernel_t->AssociateThreadSpace(m_threadSpace);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_kernel_b->AssociateThreadSpace(m_threadSpace);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
#endif
    return MFX_ERR_NONE;
}

mfxStatus ASC_Cm::CopyFrameSurface(mfxHDLPair frameHDL) {
    if (!m_ASCinitialized)
        return MFX_ERR_NOT_INITIALIZED;
    CmSurface2D* p_surfaceFrom = nullptr;
    mfxStatus sts;
    INT res;
    SurfaceIndex *idxFrom = nullptr;

    sts = CreateCmSurface2D(frameHDL, p_surfaceFrom, idxFrom);
    SCD_CHECK_MFX_ERR(sts);

    m_frameCopyEv = NULL;// CM_NO_EVENT;
    mfxU32 argIdx = 0;
    //Copy pixels kernel
    res = m_kernel_cp->SetKernelArg(argIdx++, sizeof(SurfaceIndex), idxFrom);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_kernel_cp->SetKernelArg(argIdx++, sizeof(SurfaceIndex), m_pIdxSurfCp);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);

    mfxU32
        width_dword = (UINT)ceil((double)m_gpuwidth / 4);
    res = m_kernel_cp->SetKernelArg(argIdx++, sizeof(mfxU32), &width_dword);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_kernel_cp->SetKernelArg(argIdx++, sizeof(mfxI32), &m_gpuheight);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_kernel_cp->SetKernelArg(argIdx++, sizeof(mfxU32), &m_gpuImPitch);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);

    res = m_device->CreateTask(m_taskCp);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_taskCp->AddKernel(m_kernel_cp);      //progressive
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
#ifndef CMRT_EMU
    res = m_queue->Enqueue(m_taskCp, m_frameCopyEv);
#else
    res = m_queue->Enqueue(m_taskCp, m_frameCopyEv, m_threadSpaceCp);
#endif
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_frameCopyEv->WaitForTaskFinished();
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_device->DestroyTask(m_taskCp);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    res = m_queue->DestroyEvent(m_frameCopyEv);
    SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    return sts;
}

void ASC_Cm::Reset_ASCCmDevice() {
    m_cmDeviceAssigned = false;
}

void ASC_Cm::Set_ASCCmDevice() {
    m_cmDeviceAssigned = true;
}

bool ASC_Cm::Query_ASCCmDevice() {
    return m_cmDeviceAssigned;
}

mfxStatus ASC_Cm::AssignResources(mfxU8 position, mfxU8 *pixelData)
{
    if (!IsASCinitialized())
        return MFX_ERR_DEVICE_FAILED;
    if (pixelData == nullptr)
        return MFX_ERR_DEVICE_FAILED;
    m_videoData[position]->layer.Image.Y = pixelData;
    return MFX_ERR_NONE;
}

mfxStatus ASC_Cm::AssignResources(mfxU8 position, CmSurface2DUP *inputFrame, mfxU8 *pixelData)
{
    if (!IsASCinitialized())
        return MFX_ERR_DEVICE_FAILED;
    if (inputFrame == nullptr)
        return MFX_ERR_DEVICE_FAILED;
    //std::swap(m_videoData[position]->layer.gpuImage, inputFrame);
    static_cast<ASCVidSampleCm*>(m_videoData[position])->layer.gpuImage = inputFrame;
    static_cast<ASCVidSampleCm*>(m_videoData[position])->layer.gpuImage->GetIndex(static_cast<ASCVidSampleCm*>(m_videoData[position])->layer.idxImage);
    if (pixelData == nullptr)
        return MFX_ERR_DEVICE_FAILED;
    /*std::swap(m_videoData[position]->layer.Image.data, pixelData);
    m_videoData[position]->layer.Image.Y = m_videoData[position]->layer.Image.data;*/
    m_videoData[position]->layer.Image.Y = pixelData;
    return MFX_ERR_NONE;
}

mfxStatus ASC_Cm::SwapResources(mfxU8 position, CmSurface2DUP **inputFrame, mfxU8 **pixelData)
{
    if (!IsASCinitialized())
        return MFX_ERR_DEVICE_FAILED;
    if (inputFrame == nullptr)
        return MFX_ERR_DEVICE_FAILED;
    std::swap(static_cast<ASCVidSampleCm*>(m_videoData[position])->layer.gpuImage, *inputFrame);
    static_cast<ASCVidSampleCm*>(m_videoData[position])->layer.gpuImage->GetIndex(static_cast<ASCVidSampleCm*>(m_videoData[position])->layer.idxImage);
    if (pixelData == nullptr)
        return MFX_ERR_DEVICE_FAILED;
    std::swap(m_videoData[position]->layer.Image.data, *pixelData);
    m_videoData[position]->layer.Image.Y = m_videoData[position]->layer.Image.data;
    return MFX_ERR_NONE;
}

mfxStatus ASC_Cm::QueueFrameProgressive(SurfaceIndex* idxSurf) {
    mfxStatus sts = QueueFrame(idxSurf, ASCTopField);
    return sts;
}

mfxStatus ASC_Cm::QueueFrameProgressive(SurfaceIndex* idxSurf, CmEvent *subSamplingEv, CmTask *subSamplingTask) {
    mfxStatus sts = QueueFrame(idxSurf, &subSamplingEv, &subSamplingTask, ASCTopField);
    return sts;
}

mfxStatus ASC_Cm::QueueFrameProgressive(mfxHDLPair surface, SurfaceIndex *idxTo, CmEvent **subSamplingEv, CmTask **subSamplingTask)
{
    mfxStatus sts = QueueFrame(surface, idxTo, subSamplingEv, subSamplingTask, ASCTopField);
    return sts;
}

mfxStatus ASC_Cm::QueueFrameProgressive(mfxHDLPair surface, CmEvent **subSamplingEv, CmTask **subSamplingTask) {
    mfxStatus sts = QueueFrame(surface, subSamplingEv, subSamplingTask, ASCTopField);
    return sts;
}

mfxStatus ASC_Cm::QueueFrameProgressive(mfxHDLPair surface) {
    mfxStatus sts = QueueFrame(surface, ASCTopField);
    return sts;
}

mfxStatus ASC_Cm::QueueFrameInterlaced(SurfaceIndex* idxSurf) {
    mfxStatus sts = QueueFrame(idxSurf, m_dataIn->currentField);
    m_dataReady = (sts == MFX_ERR_NONE);
    SetNextField();
    return sts;
}

mfxStatus ASC_Cm::QueueFrameInterlaced(mfxHDLPair surface)
{
    mfxStatus sts = QueueFrame(surface, m_dataIn->currentField);
    SetNextField();
    return sts;
}

mfxStatus ASC_Cm::ProcessQueuedFrame(CmEvent **subSamplingEv, CmTask **subSamplingTask, CmSurface2DUP **inputFrame, mfxU8 **pixelData)
{
    if (!m_ASCinitialized)
        return MFX_ERR_NOT_INITIALIZED;
    INT res;
    if (*subSamplingEv) {
        res = (*subSamplingEv)->WaitForTaskFinished();
        if (res == CM_EXCEED_MAX_TIMEOUT)
            return MFX_ERR_GPU_HANG;
        SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
        if(inputFrame != nullptr && pixelData != nullptr)
            MFX_SAFE_CALL(SwapResources(ASCCurrent_Frame, inputFrame, pixelData))
        AscFrameAnalysis();
        res = m_queue->DestroyEvent(*subSamplingEv);
        SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
        res = m_device->DestroyTask(*subSamplingTask);
        SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
        m_dataReady = (res == CM_SUCCESS);
    }
    else {
        return MFX_ERR_DEVICE_FAILED;
    }
    return MFX_ERR_NONE;
}

mfxStatus ASC_Cm::ProcessQueuedFrame()
{
    return ProcessQueuedFrame(&m_subSamplingEv, &m_task, nullptr, nullptr);
}

mfxStatus ASC_Cm::ProcessQueuedFrame(mfxU8** pixelData)
{
    MFX_CHECK(IsASCinitialized(), MFX_ERR_NOT_INITIALIZED);
    
    MFX_CHECK_NULL_PTR2(pixelData, *pixelData);

    memcpy(m_videoData[ASCCurrent_Frame]->layer.Image.data, *pixelData, subWidth * subHeight);
    m_videoData[ASCCurrent_Frame]->layer.Image.Y = m_videoData[ASCCurrent_Frame]->layer.Image.data;

    AscFrameAnalysis();
    return MFX_ERR_NONE;
}

mfxStatus ASC_Cm::PutFrameProgressive(SurfaceIndex* idxSurf) {
    mfxStatus sts = RunFrame(idxSurf, ASCTopField);
    m_dataReady = (sts == MFX_ERR_NONE);
    return sts;
}

mfxStatus ASC_Cm::PutFrameProgressive(mfxHDLPair surface)
{
    mfxStatus sts = RunFrame(surface, ASCTopField);
    m_dataReady = (sts == MFX_ERR_NONE);
    return sts;
}

mfxStatus ASC_Cm::PutFrameProgressive(mfxHDL surface)
{
    mfxHDLPair
        surfPair = { surface, nullptr };
    mfxStatus sts = PutFrameProgressive(surfPair);
    return sts;
}

mfxStatus ASC_Cm::PutFrameInterlaced(mfxU8 *frame, mfxI32 Pitch) {
    mfxStatus sts;

    if (Pitch > 0) {
        sts = SetPitch(Pitch);
        SCD_CHECK_MFX_ERR(sts);
    }

    sts = RunFrame(frame, m_dataIn->currentField);
    m_dataReady = (sts == MFX_ERR_NONE);
    SetNextField();
    return sts;
}

mfxStatus ASC_Cm::PutFrameInterlaced(SurfaceIndex* idxSurf) {
    mfxStatus sts = RunFrame(idxSurf, m_dataIn->currentField);
    m_dataReady = (sts == MFX_ERR_NONE);
    SetNextField();
    return sts;
}

mfxStatus ASC_Cm::PutFrameInterlaced(mfxHDLPair surface)
{
    mfxStatus sts = RunFrame(surface, m_dataIn->currentField);
    m_dataReady = (sts == MFX_ERR_NONE);
    SetNextField();
    return sts;
}

mfxStatus ASC_Cm::PutFrameInterlaced(mfxHDL surface)
{
    mfxHDLPair
        surfPair = { surface, nullptr };
    mfxStatus sts = PutFrameInterlaced(surfPair);
    return sts;
}

mfxStatus ASC_Cm::calc_RaCa_Surf(mfxHDLPair surface, mfxF64& rscs) {
    if (!Query_ASCCmDevice())
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    mfxStatus sts = CopyFrameSurface(surface);
    SCD_CHECK_MFX_ERR(sts);

    sts = calc_RaCa_pic(m_frameBkp, m_gpuwidth, m_gpuheight, m_gpuImPitch, rscs);
    SCD_CHECK_MFX_ERR(sts);

    return sts;
}

void ASC_Cm::Close() {
    if (m_videoData != nullptr) {
        VidSample_dispose();
        delete[] m_videoData;
        m_videoData = nullptr;
    }

    if (m_support != nullptr) {
        VidRead_dispose();
        delete m_support;
        m_support = nullptr;
    }

    if (m_dataIn != nullptr) {
        delete m_dataIn->layer;
        delete m_dataIn;
        m_dataIn = nullptr;
    }

    if (m_device) {
        for (auto& surf : m_tableCmRelations2) {
            CmSurface2D* temp = surf.second;
            m_device->DestroySurface(temp);
        }
        m_tableCmRelations2.clear();
        m_tableCmIndex2.clear();

        if (m_kernel_p)  m_device->DestroyKernel(m_kernel_p);
        if (m_kernel_t)  m_device->DestroyKernel(m_kernel_t);
        if (m_kernel_b)  m_device->DestroyKernel(m_kernel_b);
        if (m_kernel_cp) m_device->DestroyKernel(m_kernel_cp);
        if (m_program)   m_device->DestroyProgram(m_program);
        if (m_pSurfaceCp) m_device->DestroySurface2DUP(m_pSurfaceCp);
        if (m_threadSpace)   m_device->DestroyThreadSpace(m_threadSpace);
        if (m_threadSpaceCp) m_device->DestroyThreadSpace(m_threadSpaceCp);
        if (m_task) m_device->DestroyTask(m_task);
        if (m_taskCp) m_device->DestroyTask(m_taskCp);
    }

    m_pSurfaceCp = nullptr;
    m_kernel_p = nullptr;
    m_kernel_t = nullptr;
    m_kernel_b = nullptr;
    m_kernel_cp = nullptr;
    m_program = nullptr;
    m_device = nullptr;
    m_threadSpace = nullptr;
    m_threadSpaceCp = nullptr;
}

bool ASC_Cm::Query_resize_Event() {
    return (m_subSamplingEv != nullptr);
}
mfxStatus ASC_Cm::Init(mfxI32 Width,
    mfxI32 Height,
    mfxI32 Pitch,
    mfxU32 PicStruct,
#ifdef MFX_ENABLE_KERNELS
    CmDevice* pCmDevice,
#endif
    bool isCmSupported)
{
    mfxStatus sts = MFX_ERR_NONE;
    INT res;
    m_device = nullptr;
    m_queue = nullptr;
    m_program = nullptr;

    m_task = nullptr;
    m_taskCp = nullptr;

    m_AVX2_available = CpuFeature_AVX2();
    m_SSE4_available = CpuFeature_SSE41();

    ASC_CPU_DISP_INIT_C(GainOffset);
    ASC_CPU_DISP_INIT_SSE4_C(RsCsCalc_4x4);
    ASC_CPU_DISP_INIT_C(RsCsCalc_bound);
    ASC_CPU_DISP_INIT_C(RsCsCalc_diff);
    ASC_CPU_DISP_INIT_SSE4_C(ImageDiffHistogram);
    ASC_CPU_DISP_INIT_AVX2_SSE4_C(ME_SAD_8x8_Block_Search);
    ASC_CPU_DISP_INIT_SSE4_C(Calc_RaCa_pic);

    InitStruct();
    try
    {
        m_dataIn = new ASCVidData;
    }
    catch (...)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    m_dataIn->layer = nullptr;
    try
    {
        m_dataIn->layer = new ASCImDetails;
        //ASCVidSampleCm*
        m_videoData = new ASCVidSample * [ASCVIDEOSTATSBUF];
        for (mfxU8 i = 0; i < ASCVIDEOSTATSBUF; i++)
            m_videoData[i] = nullptr;
        m_support = new ASCVidRead;
    }
    catch (...)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
#ifdef MFX_ENABLE_KERNELS
    if (pCmDevice && isCmSupported)
    {
        res = InitGPUsurf(pCmDevice);
        SCD_CHECK_CM_ERR(res, MFX_ERR_DEVICE_FAILED);
    }
#else
    (void)res;
#endif
    for (mfxI32 i = 0; i < ASCVIDEOSTATSBUF; i++)
    {
        try
        {
            m_videoData[i] = new ASCVidSampleCm;
        }
        catch (...)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
    }
    Params_Init();

    sts = SetDimensions(Width, Height, Pitch);
    SCD_CHECK_MFX_ERR(sts);

    m_gpuwidth = Width;
    m_gpuheight = Height;

    VidSample_Init();
    Setup_Environment();

    sts = IO_Setup();
    SCD_CHECK_MFX_ERR(sts);

    sts = VidRead_Init();
    SCD_CHECK_MFX_ERR(sts);
    SetUltraFastDetection();

    if (Query_ASCCmDevice() && isCmSupported) {
        sts = CreateCmKernels();
        SCD_CHECK_MFX_ERR(sts);
    }

    sts = SetInterlaceMode((PicStruct & MFX_PICSTRUCT_FIELD_TFF) ? ASCtopfieldfirst_frame :
        (PicStruct & MFX_PICSTRUCT_FIELD_BFF) ? ASCbotfieldFirst_frame :
        ASCprogressive_frame);
    SCD_CHECK_MFX_ERR(sts);
    m_dataReady = false;
    m_ASCinitialized = (sts == MFX_ERR_NONE);
    return sts;
}


}
