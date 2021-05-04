// Copyright (c) 2019-2021 Intel Corporation
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

#ifndef __MFX_ENCTOOLS_LPLA_H__
#define __MFX_ENCTOOLS_LPLA_H__

#include "mfxdefs.h"
#include <vector>
#include <memory>
#include <list>
#include <algorithm>
#include "aenc.h"
#include "mfx_enctools_utils.h"

#if defined (MFX_ENABLE_ENCTOOLS_LPLA)

struct MfxLookAheadReport
{
    mfxU32 StatusReportFeedbackNumber;
    mfxU8  CqmHint;
    mfxU8  IntraHint;
    mfxU8  MiniGopSize;
    mfxU8  PyramidQpHint;
    mfxU32 TargetFrameSize;
    mfxU32 TargetBufferFullnessInBit;
};

enum
{
    CQM_HINT_USE_FLAT_MATRIX  = 0,   //use flat matrix
    CQM_HINT_USE_CUST_MATRIX1 = 1,   //use customized matrix
    CQM_HINT_USE_CUST_MATRIX2 = 2,
    CQM_HINT_USE_CUST_MATRIX3 = 3,
    CQM_HINT_USE_CUST_MATRIX4 = 4,
    CQM_HINT_NUM_CUST_MATRIX  = 4,
    CQM_HINT_INVALID          = 0xFF  //invalid hint
};

class LPLA_EncTool
{
public:
    LPLA_EncTool() :
        m_bInit(false),
        m_device(nullptr),
        m_pAllocator(nullptr),
        m_pmfxENC(nullptr),
        m_curDispOrder(-1),
        m_lookAheadScale (0),
        m_lastIFrameNumber(0),
        m_lastIDRFrameNumber(0),
        m_GopPicSize(0),
        m_IdrInterval(1)
    {
        m_bitstream  = {};
        m_encParams  = {};
        m_curEncodeHints = {};
    }

    virtual ~LPLA_EncTool () { Close(); }

    virtual mfxStatus Init(mfxEncToolsCtrl const & ctrl, mfxExtEncToolsConfig const & pConfig);
    virtual mfxStatus Close();
    virtual mfxStatus Submit(mfxFrameSurface1 * surface);
    virtual mfxStatus Query(mfxU32 dispOrder, mfxEncToolsBRCBufferHint *pBufHint);
    virtual mfxStatus Query(mfxU32 dispOrder, mfxEncToolsHintQuantMatrix *pCqmHint);
    virtual mfxStatus Query(mfxU32 dispOrder, mfxEncToolsHintPreEncodeGOP *pPreEncGOP);
    virtual mfxStatus InitSession();
    virtual mfxStatus InitEncParams(mfxEncToolsCtrl const & ctrl, mfxExtEncToolsConfig const & pConfig);
    virtual mfxStatus ConfigureExtBuffs(mfxEncToolsCtrl const & ctrl, mfxExtEncToolsConfig const & pConfig);

    void SetAllocator(mfxFrameAllocator * pAllocator)
    {
        m_pAllocator = pAllocator;
    }
    void GetDownScaleParams(mfxFrameInfo & fInfo, mfxU32 & downscale)
    {
        fInfo = m_encParams.mfx.FrameInfo;
        downscale = m_lookAheadScale;
    }

protected:
    bool                          m_bInit;
    mfxHDL                        m_device;
    mfxU32                        m_deviceType;
    mfxFrameAllocator*            m_pAllocator;
    MFXVideoSession               m_mfxSession;
    MFXVideoENCODE*               m_pmfxENC;
    mfxBitstream                  m_bitstream;
    std::list<MfxLookAheadReport> m_encodeHints;
    MfxLookAheadReport            m_curEncodeHints;
    mfxI32                        m_curDispOrder;
    mfxVideoParam                 m_encParams;
    mfxExtLplaParam               m_extBufLPLA;
    mfxExtHEVCParam               m_extBufHevcParam;
    mfxExtCodingOption3           m_extBufCO3;
    mfxExtLpLaStatus              m_lplaHints;
    mfxU32                        m_lookAheadScale;
    mfxU32                        m_lastIFrameNumber;
    mfxU32                        m_lastIDRFrameNumber;
    mfxU16                        m_GopPicSize;
    mfxU16                        m_IdrInterval;
    mfxExtEncToolsConfig          m_config;
};
#endif
#endif


