// Copyright (c) 2019-2022 Intel Corporation
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
#include "mfxenctools_dl_int.h"

struct MfxFrameSize
{
    mfxU32 dispOrder;
    mfxU32 encodedFrameSize;
    mfxU32 frameType;
};


#if defined (MFX_ENABLE_ENCTOOLS_LPLA)

struct MfxLookAheadReport
{
    mfxU32 StatusReportFeedbackNumber;
    mfxU8  CqmHint;
    mfxU8  IntraHint;
    mfxU8  MiniGopSize;
    mfxU8  PyramidQpHint;
    mfxU32 TargetFrameSize;
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

#endif // MFX_ENABLE_ENCTOOLS_LPLA

class LPLA_EncTool
{
public:
    LPLA_EncTool(void* module) :
        m_bInit(false),
        m_device(nullptr),
        m_pAllocator(nullptr),
        m_mfxSession(module),
        m_pmfxENC(nullptr),
        m_curDispOrder(-1),
        m_lookAheadScale (0),
        m_lookAheadDepth (0),
        m_lastIFrameNumber(0),
        m_lastIDRFrameNumber(0),
        m_lastIPFrameNumber(0),
        m_nextPisIntra(false),
        m_GopPicSize(0),
        m_GopRefDist(0),
        m_IdrInterval(1),
        m_codecId(0),
        m_hRTModule(module)
    {
        m_bitstream  = {};
        m_encParams  = {};
#if defined (MFX_ENABLE_ENCTOOLS_LPLA)
        m_curEncodeHints = {};
#endif
        m_frameSizes = {};
        m_config = {};
    }

    virtual ~LPLA_EncTool () {
        Close();
    }

    virtual mfxStatus Init(mfxEncToolsCtrl const & ctrl, mfxExtEncToolsConfig const & pConfig);
    virtual mfxStatus Reset(mfxEncToolsCtrl const & ctrl, mfxExtEncToolsConfig const & pConfig);
    virtual mfxStatus Close();
    virtual MFXDLVideoSession* GetEncSession();
    virtual mfxStatus SaveEncodedFrameSize(mfxFrameSurface1* surface, mfxU16 FrameType);
    virtual mfxStatus Submit(mfxFrameSurface1* surface, mfxU16 FrameType, mfxSyncPoint* pEncSyncp);
    virtual mfxStatus Query(mfxU32 dispOrder, mfxEncToolsBRCBufferHint *pBufHint);
#if defined (MFX_ENABLE_ENCTOOLS_LPLA)
    virtual mfxStatus Query(mfxU32 dispOrder, mfxEncToolsHintQuantMatrix *pCqmHint);
    virtual mfxStatus Query(mfxU32 dispOrder, mfxEncToolsHintPreEncodeGOP *pPreEncGOP);
#endif
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
    MFXDLVideoSession             m_mfxSession;
    MFXDLVideoENCODE*             m_pmfxENC;
    mfxBitstream                  m_bitstream;
#if defined (MFX_ENABLE_ENCTOOLS_LPLA)
    std::list<MfxLookAheadReport> m_encodeHints;
    MfxLookAheadReport            m_curEncodeHints;
#endif
    mfxI32                        m_curDispOrder;
    mfxVideoParam                 m_encParams;
#if defined (MFX_ENABLE_ENCTOOLS_LPLA)
    mfxExtLplaParam               m_extBufLPLA;
#endif
    mfxExtHEVCParam               m_extBufHevcParam;
    mfxExtCodingOption3           m_extBufCO3;
    mfxExtCodingOption2           m_extBufCO2;
#if defined (MFX_ENABLE_ENCTOOLS_LPLA)
    mfxExtLpLaStatus              m_lplaHints;
#endif
    mfxU32                        m_lookAheadScale;
    mfxU32                        m_lookAheadDepth;
    mfxU32                        m_lastIFrameNumber;
    mfxU32                        m_lastIDRFrameNumber;
    mfxU32                        m_lastIPFrameNumber;
    bool                          m_nextPisIntra;
    mfxU16                        m_GopPicSize;
    mfxU16                        m_GopRefDist;
    mfxU16                        m_IdrInterval;
    std::list<MfxFrameSize>       m_frameSizes;
    mfxExtEncToolsConfig          m_config;
    mfxU32                        m_codecId;

    void* m_hRTModule = nullptr;
};
#endif
