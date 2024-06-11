// Copyright (c) 2022 Intel Corporation
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
#ifndef _ASC_CM_H_
#define _ASC_CM_H_

#include <map>
#include "asc.h"
#include "asc_structures.h"
#include "cmrt_cross_platform.h"

namespace ns_asc {

class ASCimageData_Cm: public ASCimageData
{
public:
    ASCimageData_Cm();
    CmSurface2DUP
        *gpuImage;
    SurfaceIndex
        *idxImage;
};

typedef struct ASCvideoBufferCm: public ASCVidSample
{
    ASCimageData_Cm
        layer;
    mfxI32
        frame_number,
        forward_reference,
        backward_reference;
} ASCVidSampleCm;

class ASC_Cm: public ASC
{
public:
    ASC_Cm();
    virtual ~ASC_Cm(){}
    virtual void Close();
    using ASC::Init;
    virtual mfxStatus Init(mfxI32 Width,
        mfxI32 Height,
        mfxI32 Pitch,
        mfxU32 PicStruct,
#ifdef MFX_ENABLE_KERNELS
        CmDevice* pCmDevice,
#endif
        bool isCmSupported);
protected:
    CmDevice
        *m_device;
    CmQueue
        *m_queue;
    CmSurface2DUP
        *m_pSurfaceCp;
    SurfaceIndex
        *m_pIdxSurfCp;
    CmProgram
        *m_program;
    CmKernel
        *m_kernel_p,
        *m_kernel_t,
        *m_kernel_b,
        *m_kernel_cp;
    CmThreadSpace
        *m_threadSpace,
        *m_threadSpaceCp;
    CmEvent
        *m_subSamplingEv,
        *m_frameCopyEv;
    CmTask
        *m_task,
        *m_taskCp;

    bool
        m_cmDeviceAssigned;

    // these maps will be used by m_pCmCopy to track already created surfaces
    std::map<mfxHDLPair, CmSurface2D *> m_tableCmRelations2;
    std::map<CmSurface2D *, SurfaceIndex *> m_tableCmIndex2;

    mfxStatus VidSample_Alloc();

    virtual void VidSample_dispose();
    mfxStatus alloc();
    mfxStatus InitCPU();
    mfxStatus InitGPUsurf(CmDevice* pCmDevice);
    mfxStatus IO_Setup();

    bool Query_resize_Event();

    mfxStatus SetKernel(SurfaceIndex *idxFrom, SurfaceIndex *idxTo, CmTask **subSamplingTask, mfxU32 parity);
    mfxStatus SetKernel(SurfaceIndex *idxFrom, CmTask **subSamplingTask, mfxU32 parity);
    mfxStatus SetKernel(SurfaceIndex *idxFrom, mfxU32 parity);
    
    mfxStatus QueueFrame(mfxHDLPair frameHDL, SurfaceIndex *idxTo, CmEvent **subSamplingEv, CmTask **subSamplingTask, mfxU32 parity);
    mfxStatus QueueFrame(mfxHDLPair frameHDL, CmEvent **subSamplingEv, CmTask **subSamplingTask, mfxU32 parity);

    mfxStatus QueueFrame(mfxHDLPair frameHDL, mfxU32 parity);
    mfxStatus QueueFrame(SurfaceIndex *idxFrom, mfxU32 parity);
#ifndef CMRT_EMU
    mfxStatus QueueFrame(SurfaceIndex *idxFrom, SurfaceIndex *idxTo, CmEvent **subSamplingEv, CmTask **subSamplingTask, mfxU32 parity);
    mfxStatus QueueFrame(SurfaceIndex *idxFrom, CmEvent **subSamplingEv, CmTask **subSamplingTask, mfxU32 parity);
#else
    mfxStatus QueueFrame(SurfaceIndex *idxFrom, CmEvent **subSamplingEv, CmTask **subSamplingTask, CmThreadSpace *subThreadSpace, mfxU32 parity);
#endif

    mfxStatus RunFrame(SurfaceIndex *idxFrom, mfxU32 parity);
    mfxStatus RunFrame(mfxHDLPair frameHDL, mfxU32 parity);

    mfxStatus CreateCmSurface2D(mfxHDLPair pSrcPair, CmSurface2D* & pCmSurface2D, SurfaceIndex* &pCmSrcIndex);
    mfxStatus CreateCmKernels();
    mfxStatus CopyFrameSurface(mfxHDLPair frameHDL);

    void Reset_ASCCmDevice();
    void Set_ASCCmDevice();
public:
    bool Query_ASCCmDevice();
    
    mfxStatus AssignResources(mfxU8 position, mfxU8 *pixelData);
    mfxStatus AssignResources(mfxU8 position, CmSurface2DUP *inputFrame, mfxU8 *pixelData);
    mfxStatus SwapResources(mfxU8 position, CmSurface2DUP **inputFrame, mfxU8 **pixelData);
    
    mfxStatus QueueFrameProgressive(mfxHDLPair surface, SurfaceIndex *idxTo, CmEvent **subSamplingEv, CmTask **subSamplingTask);
    mfxStatus QueueFrameProgressive(mfxHDLPair surface, CmEvent **taskEvent, CmTask **subSamplingTask);
    mfxStatus QueueFrameProgressive(mfxHDLPair surface);
    mfxStatus QueueFrameProgressive(SurfaceIndex* idxSurf);
    mfxStatus QueueFrameProgressive(SurfaceIndex* idxSurf, CmEvent* subSamplingEv, CmTask* subSamplingTask);

    mfxStatus QueueFrameInterlaced(mfxHDLPair surface);
    mfxStatus QueueFrameInterlaced(SurfaceIndex* idxSurf);

    mfxStatus ProcessQueuedFrame(CmEvent **subSamplingEv, CmTask **subSamplingTask, CmSurface2DUP **inputFrame, mfxU8 **pixelData);
    mfxStatus ProcessQueuedFrame();
    mfxStatus ProcessQueuedFrame(mfxU8** pixelData);
    
    mfxStatus PutFrameProgressive(mfxHDLPair surface);
    mfxStatus PutFrameProgressive(mfxHDL surface);
    mfxStatus PutFrameProgressive(SurfaceIndex* idxSurf);
    
    mfxStatus PutFrameInterlaced(mfxHDLPair surface);
    mfxStatus PutFrameInterlaced(mfxHDL surface);
    mfxStatus PutFrameInterlaced(SurfaceIndex* idxSurf);
    mfxStatus PutFrameInterlaced(mfxU8 *frame, mfxI32 Pitch);
    using ASC::RunFrame;
    using ASC::PutFrameProgressive;

    virtual mfxStatus calc_RaCa_Surf(mfxHDLPair surface, mfxF64& rscs);
//private:
//    ASCVidSampleCm** m_videoData;
};
};

#endif //_ASC_CM_H_