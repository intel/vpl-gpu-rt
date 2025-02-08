// Copyright (c) 2024 Intel Corporation
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
#ifndef _MFX_VPP_AI_FRAME_INTERPOLATION_H_
#define _MFX_VPP_AI_FRAME_INTERPOLATION_H_


#include "mfx_vpp_interface.h"
#include "asc.h"
#ifdef MFX_ENABLE_AI_VIDEO_FRAME_INTERPOLATION
#include "asc_ai_vfi.h"
#include "xe_ai_vfi.h"
#endif
#include "mfx_vpp_helper.h"

#include <queue>

class MfxVppHelper;

using namespace MfxHwVideoProcessing;
class MFXVideoFrameInterpolation
{
    enum Ratio {
        ratio_2x = 2,
        ratio_4x = 4,
        ratio_8x = 8,
        ratio_16x = 16,
        ratio_unsupported  = -1
    };

public:
    MFXVideoFrameInterpolation();
    virtual ~MFXVideoFrameInterpolation();

    static mfxStatus Query(VideoCORE* core);

    mfxStatus Init(
        VideoCORE* core,
        const mfxFrameInfo& inInfo,
        const mfxFrameInfo& outInfo,
        mfxU16 IOPattern,
        const mfxVideoSignalInfo& videoSignalInfo);

    mfxStatus UpdateTsAndGetStatus(
        mfxFrameSurface1* input,
        mfxFrameSurface1* output,
        mfxStatus* intSts);

    mfxStatus ReturnSurface(mfxFrameSurface1* out, mfxMemId internalVidMemId = 0);

private:
    mfxStatus ConfigureFrameRate(
        mfxU16 IOPattern,
        const mfxFrameInfo& inInfo,
        const mfxFrameInfo& outInfo);
    mfxStatus InitFrameInterpolator(VideoCORE* core, const mfxFrameInfo& outInfo);
    std::tuple<bool, bool> IsVppNeededForVfi(const mfxFrameInfo& inInfo, const mfxFrameInfo& outInfo);
    mfxStatus InitVppAndAllocateSurface(
        const mfxFrameInfo& inInfo,
        const mfxFrameInfo& outInfo,
        const mfxVideoSignalInfo& videoSignalInfo);

    mfxStatus InitScd(const mfxFrameInfo& inFrameInfo, const mfxFrameInfo& outFrameInfo);
    mfxStatus SceneChangeDetect(mfxFrameSurface1* input, bool isExternal, mfxU32& decision);

    mfxStatus DuplicateFrame();
    mfxStatus DoInterpolation();
    mfxStatus DoInterpolation(mfxU16 leftIdx, mfxU16 rightIdx);
    mfxStatus InterpolateAi(mfxFrameSurface1& bkw, mfxFrameSurface1& fwd, mfxFrameSurface1& out);

    mfxStatus AddTaskQueue(bool isSequenceEnd = false);

    VideoCORE* m_core;

    MfxHwVideoProcessing::RateRational m_frcRational[2];
    Ratio                              m_ratio;
    mfxU16                             m_outStamp;
    mfxU16                             m_outTick;
    bool                               m_sequenceEnd;

    mfxU16 m_IOPattern;

    mfxFrameSurface1      m_inputFwd;
    mfxFrameSurface1      m_inputBkwd;

    //scd related
    bool                          m_enableScd;
#ifdef MFX_ENABLE_AI_VIDEO_FRAME_INTERPOLATION
    ns_asc::ASC_AiVfi             m_scd;
#endif
    bool                          m_scdNeedCsc;
    std::unique_ptr<MfxVppHelper> m_vppForScd;
    mfxFrameSurface1              m_scdImage;
    mfxFrameAllocResponse         m_scdAllocation;

#ifdef MFX_ENABLE_AI_VIDEO_FRAME_INTERPOLATION
    xeAIVfi                       m_aiIntp;
#endif
    bool                          m_vppForFi;
    std::unique_ptr<MfxVppHelper> m_vppBeforeFi0;
    std::unique_ptr<MfxVppHelper> m_vppBeforeFi1;
    std::unique_ptr<MfxVppHelper> m_vppAfterFi;

    mfxFrameAllocResponse         m_rgbSurfForFiIn;
    mfxFrameSurface1              m_rgbSurfArray[17];
    mfxFrameAllocResponse         m_outSurfForFi;
    mfxFrameSurface1              m_fiOut;

    struct Task
    {
        mfxU32 taskIndex;
        mfxU16 outStamp;
        bool   isSequenceEnd;
    };
    class tsQueue 
    {
    public: 

        void Enqueue(Task t)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(t);
            m_cv.notify_one();
        }
        void Dequeue(Task& t)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return !m_queue.empty(); });
            t = m_queue.front();
            m_queue.pop();
        }
    private:
        std::queue<Task>            m_queue;
        std::mutex                  m_mutex;
        std::condition_variable     m_cv;
    } ;
    tsQueue                     m_taskQueue;
    mfxU32                      m_taskIndex;

    // To calculate the time stamp in mfxFrameData
    // The starting time of input frame n for frame [n, n+1] interpolation in units of 90KHz. Divide TimeStamp by 90,000 (90 KHz) to obtain the time in seconds
    // The value of MFX_TIMESTAMP_UNKNOWN indicates that there is no time stamp
    mfxU64 m_time_stamp_start;
    // The time interval of two interpolated frames [n, n+1] in units of 90KHz. Divide TimeStamp by 90,000 (90 KHz) to obtain the time in seconds
    mfxU64 m_time_stamp_interval;
};

#endif
