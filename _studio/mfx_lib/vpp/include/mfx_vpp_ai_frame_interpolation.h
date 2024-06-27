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

#define ENABLE_VFI 0

#include "mfx_vpp_interface.h"
#include "asc.h"
#ifdef MFX_ENABLE_AI_VIDEO_FRAME_INTERPOLATION
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
        ratio_unsupported  = -1
    };

public:
    MFXVideoFrameInterpolation();
    virtual ~MFXVideoFrameInterpolation();

    mfxStatus Init(VideoCORE* core, mfxFrameInfo& inInfo, mfxFrameInfo& outInfo, mfxU16 IOPattern, const mfxVideoSignalInfo& videoSignalInfo);

    mfxStatus UpdateTsAndGetStatus(
        mfxFrameSurface1* input,
        mfxFrameSurface1* output,
        mfxStatus* intSts);

    mfxStatus ReturnSurface(mfxU32 taskIndex, mfxFrameSurface1* out, mfxMemId internalVidMemId = 0);

    bool IsScdSupportedFormat(mfxU32 fourcc);

    mfxStatus AddTaskQueue(mfxU32 taskIndex);

private:
    mfxStatus InitScd(const mfxFrameInfo& frameInfo);
    mfxStatus SceneChangeDetect(mfxFrameSurface1* input, bool isExternal, mfxU32& decision);

    mfxStatus DuplicateFrame();
    mfxStatus DoInterpolation();
    mfxStatus DoInterpolation2x();
    mfxStatus DoInterpolation4x();
    mfxStatus InterpolateAi(mfxFrameSurface1* bkw, mfxFrameSurface1* fwd, mfxFrameSurface1* out);

    VideoCORE* m_core;

    MfxHwVideoProcessing::RateRational m_frcRational[2];
    Ratio                              m_ratio;
    mfxU16                             m_outStamp;
    mfxU16                             m_outTick;

    mfxU16 m_IOPattern;

    mfxFrameAllocResponse m_responseIn;
    mfxFrameAllocResponse m_responseOut;
    mfxFrameSurface1      m_inputFwd;
    mfxFrameSurface1      m_inputBkwd;
    mfxFrameSurface1      m_output[4];
    mfxMemId              m_memIdBkwd;
    mfxMemId              m_memIdFwd;

    //scd related
    bool                          m_enableScd;
    ns_asc::ASC                   m_scd;
    bool                          m_scdNeedCsc;
    std::unique_ptr<MfxVppHelper> m_vppForScd;
    mfxFrameSurface1              m_scdImage;
    mfxFrameAllocResponse         m_scdAllocation;

#if ENABLE_VFI
    xeAIVfi                       m_aiIntp;
#endif
    bool                          m_preWorkCscForFi;
    bool                          m_postWrokForFi;
    std::unique_ptr<MfxVppHelper> m_vppBeforeFi0;
    std::unique_ptr<MfxVppHelper> m_vppBeforeFi1;
    std::unique_ptr<MfxVppHelper> m_vppAfterFi;
    mfxFrameAllocResponse         m_rgbSurfForFiIn;
    mfxFrameAllocResponse         m_rgbSurfForFiOut;
    mfxFrameSurface1              m_rgbBkwd;
    mfxFrameSurface1              m_rgbfwd;
    mfxFrameSurface1              m_rgbFiOut;
    mfxFrameAllocResponse         m_outSurfForFi;
    mfxFrameSurface1              m_fiOut;


    // first  taskIndex
    // second timestamp
    using task = std::pair<mfxU32, mfxU16>;
    std::queue<task>            m_taskQueue;
};

#endif
