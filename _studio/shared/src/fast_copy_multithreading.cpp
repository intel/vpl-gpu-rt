// Copyright (c) 2009-2020 Intel Corporation
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

#include "fast_copy.h"

#include "umc_event.h"
#include <thread>
#include "libmfx_allocator.h"

struct FC_TASK
{
    // pointers to source and destination
    Ipp8u *pS;
    Ipp8u *pD;

    // size of chunk to copy
    Ipp32u chunkSize;

    // pitches and frame size
    IppiSize roi;
    Ipp32u srcPitch, dstPitch;
    int flag;

    // event handles
    UMC::Event EventStart;
    UMC::Event EventEnd;
};

class FastCopyMultithreading : public FastCopy
{
public:

    // constructor
    FastCopyMultithreading(void);

    // destructor
    virtual ~FastCopyMultithreading(void);

    // initialize available functionality
    mfxStatus Initialize(void);

    // release object
    mfxStatus Release(void);

    // copy memory by streaming
    mfxStatus Copy(mfxU8 *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, IppiSize roi, int flag);

protected:

   // synchronize threads
    mfxStatus Synchronize(void);

    static mfxU32 __STDCALL CopyByThread(void *object);
    static IppBool m_bCopyQuit;

    // handles
    std::thread *m_pThreads;
    Ipp32u m_numThreads;

    FC_TASK *m_tasks;
};

IppBool FastCopyMultithreading::m_bCopyQuit = ippFalse;

FastCopyMultithreading::FastCopyMultithreading(void)
{
    m_pThreads = NULL;
    m_tasks = NULL;

    m_numThreads = 0;

} // FastCopy::FastCopy(void)

FastCopyMultithreading::~FastCopyMultithreading(void)
{
    Release();

} // FastCopy::~FastCopy(void)

mfxStatus FastCopyMultithreading::Initialize(void)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 i = 0;

    // release object before allocation
    Release();

    if (MFX_ERR_NONE == sts)
    {
        // streaming is on
        m_numThreads = 1;
    }
    if (MFX_ERR_NONE == sts)
    {
        m_pThreads = new std::thread[m_numThreads];
        m_tasks = new FC_TASK[m_numThreads];

        if (!m_pThreads || !m_tasks) sts = MFX_ERR_MEMORY_ALLOC;
    }
    // initialize events
    for (i = 0; (MFX_ERR_NONE == sts) && (i < m_numThreads - 1); i += 1)
    {
        if (UMC::UMC_OK != m_tasks[i].EventStart.Init(0, 0))
        {
            sts = MFX_ERR_UNKNOWN;
        }
        if (UMC::UMC_OK != m_tasks[i].EventEnd.Init(0, 0))
        {
            sts = MFX_ERR_UNKNOWN;
        }
    }
    // run threads
    for (i = 0; (MFX_ERR_NONE == sts) && (i < m_numThreads - 1); i += 1)
    {
        m_pThreads[i] = std::thread([this, i]() { CopyByThread((void *)(m_tasks + i)); });
    }

    return MFX_ERR_NONE;
} // mfxStatus FastCopy::Initialize(void)

mfxStatus FastCopyMultithreading::Release(void)
{
    m_bCopyQuit = ippTrue;

    if ((m_numThreads > 1) && m_tasks && m_pThreads)
    {
        // set event
        for (mfxU32 i = 0; i < m_numThreads - 1; i += 1)
        {
            m_tasks[i].EventStart.Set();
            if (m_pThreads[i].joinable())
                m_pThreads[i].join();
        }
    }

    m_numThreads = 0;

    if (m_pThreads)
    {
        delete [] m_pThreads;
        m_pThreads = NULL;
    }

    if (m_tasks)
    {
        delete [] m_tasks;
        m_tasks = NULL;
    }

    m_bCopyQuit = ippFalse;

    return MFX_ERR_NONE;

} // mfxStatus FastCopy::Release(void)

mfxStatus FastCopyMultithreading::Copy(mfxU8 *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, IppiSize roi, int flag)
{
    if (NULL == pDst || NULL == pSrc)
    {
        return MFX_ERR_NULL_PTR;
    }

    mfxU32 partSize = roi.height / m_numThreads;
    mfxU32 rest = roi.height % m_numThreads;

    roi.height = partSize;

    // distribute tasks
    for (mfxU32 i = 0; i < m_numThreads - 1; i += 1)
    {
        m_tasks[i].pS = pSrc + i * (partSize * srcPitch);
        m_tasks[i].pD = pDst + i * (partSize * dstPitch);
        m_tasks[i].srcPitch = srcPitch;
        m_tasks[i].dstPitch = dstPitch;
        m_tasks[i].roi = roi;
        m_tasks[i].flag = flag;

        m_tasks[i].EventStart.Set();
    }

    if (rest != 0)
    {
        roi.height = rest;
    }

    pSrc = pSrc + (m_numThreads - 1) * (partSize * srcPitch);
    pDst = pDst + (m_numThreads - 1) * (partSize * dstPitch);

    FastCopy::Copy(pDst, dstPitch, pSrc, srcPitch, roi, flag);

    Synchronize();

    return MFX_ERR_NONE;

} // mfxStatus FastCopy::Copy(mfxU8 *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, IppiSize roi)

mfxStatus FastCopyMultithreading::Synchronize(void)
{
    for (mfxU32 i = 0; i < m_numThreads - 1; i += 1)
    {
        m_tasks[i].EventEnd.Wait();
    }

    return MFX_ERR_NONE;

} // mfxStatus FastCopy::Synchronize(void)

// thread function
mfxU32 __STDCALL FastCopyMultithreading::CopyByThread(void *arg)
{
    FC_TASK *task = (FC_TASK *)arg;
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "ThreadName=FastCopy");

    // wait to event
    task->EventStart.Wait();

    while (!m_bCopyQuit)
    {
        mfxU32 srcPitch = task->srcPitch;
        mfxU32 dstPitch = task->dstPitch;
        IppiSize roi = task->roi;

        Ipp8u *pSrc = task->pS;
        Ipp8u *pDst = task->pD;

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "FastCopy::Copy");
            FastCopy::Copy(pDst, dstPitch, pSrc, srcPitch, roi, task->flag);
        }

        // done copy
        task->EventEnd.Set();

        // wait for the next frame
        task->EventStart.Wait();
    }

    return 0;

} // mfxU32 __stdcall FastCopyMultithreading::CopyByThread(void *arg)
