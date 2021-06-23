// Copyright (c) 2014-2021 Intel Corporation
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
#include "mfx_task.h"

#ifndef _MFX_LOWPOWER_LOOKAHEAD_H_
#define _MFX_LOWPOWER_LOOKAHEAD_H_

struct mfxLplastatus
{
    mfxU8 ValidInfo = 0;
    mfxU8 CqmHint = 0xFF;
    mfxU32 TargetFrameSize = 0;
#if defined(MFX_ENABLE_ENCTOOLS_LPLA)
    mfxU8 MiniGopSize = 0;
    mfxU8 QpModulation = 0;
#endif
    mfxU32 AvgEncodedBits = 0;
    mfxU32 CurEncodedBits = 0;
    mfxU16 DistToNextI = 0;
};

#if defined (MFX_ENABLE_LP_LOOKAHEAD)

class VideoVPPMain;

class MfxLpLookAhead
{
public:
    MfxLpLookAhead(VideoCORE *core) : m_core(core) {}

    virtual ~MfxLpLookAhead() { Close(); }

    virtual mfxStatus Init(mfxVideoParam* param);

    virtual mfxStatus Reset(mfxVideoParam* param);

    virtual mfxStatus Close();

    virtual mfxStatus Submit(mfxFrameSurface1 * surface);

    virtual mfxStatus Query(mfxLplastatus& laStatus);

    virtual mfxStatus SetStatus(mfxLplastatus *laStatus);

protected:
    bool NeedDownScaling(const mfxVideoParam& par);
    mfxStatus CreateVpp(const mfxVideoParam& par);
    void DestroyVpp();

protected:
    bool                   m_bInitialized = false;
    bool                   m_taskSubmitted= false;
    VideoCORE*             m_core         = nullptr;
    VideoENCODE*           m_pEnc         = nullptr;
    mfxBitstream           m_bitstream    = {};
    std::list<mfxLplastatus> m_lplastatus;

    mfxExtBuffer*          m_extBuffer = nullptr;
    mfxExtVPPScaling       m_scalingConfig = {};

    MFX_ENTRY_POINT        m_entryPoint      = {};
    bool                   m_bNeedDownscale  = false;
    VideoVPPMain*          m_pVpp            = nullptr;
    mfxFrameAllocResponse  m_dsResponse      = {};
    mfxFrameSurface1       m_dsSurface       = {};

    const mfxU16           m_dsRatio     = 4;
    const mfxU16           m_minDsWidth  = 1280; // if resolution smaller than min, no downscaling required
    const mfxU16           m_minDsHeight = 720;
    mfxU16                 m_dstWidth    = 480; // target down scaled size
    mfxU16                 m_dstHeight   = 270;
};

#endif

#endif // !_MFX_LOWPOWER_LOOKAHEAD_H_