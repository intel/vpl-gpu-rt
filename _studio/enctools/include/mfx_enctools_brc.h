// Copyright (c) 2009-2021 Intel Corporation
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

#ifndef __MFX_ENCTOOLS_BRC_H__
#define __MFX_ENCTOOLS_BRC_H__

#include "mfxdefs.h"
#include <vector>
#include <memory>
#include <algorithm>
#include "mfx_enctools_utils.h"
#include <climits>

namespace EncToolsBRC
{

    constexpr mfxU8  LA_P_UPDATE_DIST = 8;
    constexpr mfxU8  MBQP_P_UPDATE_DIST = 7;
    constexpr mfxU8  MAX_GOP_REFDIST = 16;
    constexpr mfxU8  MIN_PAQ_QP = 10;   //do not allow PAQ to set QP below this value

    constexpr mfxF64 MIN_RACA = 0.25;
    constexpr mfxF64 MAX_RACA = 361.0;
    constexpr mfxF64 RACA_SCALE = 128.0;

    constexpr mfxU8 TOTAL_NUM_AV1_SEGMENTS_FOR_ENCTOOLS = 7;
/*
NalHrdConformance | VuiNalHrdParameters   |  Result
--------------------------------------------------------------
    off                  any                => MFX_BRC_NO_HRD
    default              off                => MFX_BRC_NO_HRD
    on                   off                => MFX_BRC_HRD_WEAK
    on (or default)      on (or default)    => MFX_BRC_HRD_STRONG
--------------------------------------------------------------
*/



struct BRC_FrameStruct
{
    mfxU16 frameType        = 0;
    mfxU16 pyrLayer         = 0;
    mfxU32 encOrder         = 0;
    mfxU32 dispOrder        = 0;
    mfxI32 qp               = 0;
    mfxI32 origSeqQp        = 0;
    mfxI32 frameSize        = 0;
    mfxI32 numRecode        = 0;
    mfxU16 sceneChange      = 0;
    mfxU16 longTerm         = 0;
    mfxU32 frameCmplx       = 0;
    mfxU32 LaAvgEncodedSize = 0;
    mfxU32 LaCurEncodedSize = 0;
    mfxU32 LaIDist          = 0;
    mfxI16 qpDelta          = MFX_QP_UNDEFINED;
    mfxU16 qpModulation     = MFX_QP_MODULATION_NOT_DEFINED;
    mfxI8  QPDeltaExplicitModulation      = 0;
    mfxU16 miniGopSize      = 0;
    mfxU16 QpMapNZ          = 0;
    mfxU16 PersistenceMapNZ = 0;
    mfxI16 QpMapBias        = 0;
    mfxU8  PersistenceMap[MFX_ENCTOOLS_PREENC_MAP_SIZE] = {};
};

class cBRCParams
{
public:
    mfxU16 rateControlMethod; // CBR or VBR

    mfxU16 HRDConformance;   // is HRD compliance  needed
    mfxU16 bRec;             // is Recoding possible
    mfxU16 bPanic;           // is Panic mode possible

    // HRD params
    mfxU32 bufferSizeInBytes;
    mfxU32 initialDelayInBytes;

    // Sliding window parameters
    mfxU32  WinBRCMaxAvgKbps;
    mfxU16  WinBRCSize;

    // RC params
    mfxU32 targetbps;
    mfxU32 maxbps;
    mfxF64 frameRate;
    mfxF64 inputBitsPerFrame;
    mfxF64 maxInputBitsPerFrame;
    mfxU32 maxFrameSizeInBits;

    // Frame size params
    mfxU16 width;
    mfxU16 height;
    mfxU16 cropWidth;
    mfxU16 cropHeight;
    mfxU16 chromaFormat;
    mfxU16 bitDepthLuma;
    mfxU32 mRawFrameSizeInBits;
    mfxU32 mRawFrameSizeInPixs;

    // GOP params
    mfxU16 gopPicSize;
    mfxU16 gopRefDist;
    bool   bPyr;
    bool   bFieldMode;

    //BRC accurancy params
    mfxF64 fAbPeriodLong;   // number on frames to calculate aberration from target frame
    mfxF64 fAbPeriodShort;  // number on frames to calculate aberration from target frame
    mfxF64 fAbPeriodLA;     // number on frames to calculate aberration from target frame (LA)
    mfxF64 dqAbPeriod;      // number on frames to calculate aberration from dequant
    mfxF64 bAbPeriod;       // number of frames to calculate aberration from target bitrate

    //QP parameters
    mfxI32   quantOffset;
    mfxI32   quantMaxI;
    mfxI32   quantMinI;
    mfxI32   quantMaxP;
    mfxI32   quantMinP;
    mfxI32   quantMaxB;
    mfxI32   quantMinB;
    mfxU32   iDQp0;
    mfxU32   iDQp;
    mfxU32   mNumRefsInGop;
    bool     mIntraBoost;
    bool     mVeryLowDelay;
    mfxF64   mMinQstepCmplxKP;
    mfxF64   mMinQstepRateEP;
    mfxI32   mMinQstepCmplxKPUpdt;
    mfxF64   mMinQstepCmplxKPUpdtErr;

    mfxU16   mLaDepth;
    mfxU16   mLaQp;
    mfxU16   mLaScale;
    mfxU16   mHasALTR; // When mHasALTR, LTR marking decision (on/off) based on content.
    mfxU32   codecId;
    bool     mMBBRC;    // Enable Macroblock-CU level QP control (true/false)
    bool     lowPower;

public:
    cBRCParams() :
        rateControlMethod(0),
        HRDConformance(MFX_BRC_NO_HRD),
        bRec(0),
        bPanic(0),
        bufferSizeInBytes(0),
        initialDelayInBytes(0),
        WinBRCMaxAvgKbps(0),
        WinBRCSize(0),
        targetbps(0),
        maxbps(0),
        frameRate(0),
        inputBitsPerFrame(0),
        maxInputBitsPerFrame(0),
        maxFrameSizeInBits(0),
        width(0),
        height(0),
        cropWidth(0),
        cropHeight(0),
        chromaFormat(0),
        bitDepthLuma(0),
        mRawFrameSizeInBits(0),
        mRawFrameSizeInPixs(0),
        gopPicSize(0),
        gopRefDist(0),
        bPyr(0),
        bFieldMode(0),
        fAbPeriodLong(0),
        fAbPeriodShort(0),
        fAbPeriodLA(0),
        dqAbPeriod(0),
        bAbPeriod(0),
        quantOffset(0),
        quantMaxI(0),
        quantMinI(0),
        quantMaxP(0),
        quantMinP(0),
        quantMaxB(0),
        quantMinB(0),
        iDQp0(0),
        iDQp(0),
        mNumRefsInGop(0),
        mIntraBoost(0),
        mVeryLowDelay(0),
        mMinQstepCmplxKP(0),
        mMinQstepRateEP(0),
        mMinQstepCmplxKPUpdt(0),
        mMinQstepCmplxKPUpdtErr(0),
        mLaDepth(0),
        mLaQp(0),
        mLaScale(0),
        mHasALTR(0),
        codecId(0),
        mMBBRC(false),
        lowPower(0)
    {}

    mfxStatus Init(mfxEncToolsCtrl const & ctrl, bool bMBBRC, bool bFieldMode, bool bALTR);
    mfxStatus GetBRCResetType(mfxEncToolsCtrl const & ctrl, bool bMBBRC, bool bNewSequence, bool &bReset, bool &bSlidingWindowReset, bool bALTR);
};

struct sHrdInput
{
    bool   m_cbrFlag = false;
    mfxU32 m_bitrate = 0;
    mfxU32 m_maxCpbRemovalDelay = 0;
    mfxF64 m_clockTick = 0.0;
    mfxF64 m_cpbSize90k = 0.0;
    mfxF64 m_initCpbRemovalDelay = 0;

    void Init(cBRCParams par);
};

class HRDCodecSpec
{
private:
    mfxI32   m_overflowQuant  = 999;
    mfxI32   m_underflowQuant = 0;

public:
    mfxI32    GetMaxQuant() const { return m_overflowQuant - 1; }
    mfxI32    GetMinQuant() const { return m_underflowQuant + 1; }
    void      SetOverflowQuant(mfxI32 qp) { m_overflowQuant = qp; }
    void      SetUnderflowQuant(mfxI32 qp) { m_underflowQuant = qp; }
    void      ResetQuant() { m_overflowQuant = 999;  m_underflowQuant = 0;}

    virtual ~HRDCodecSpec() {}
    virtual void Init(cBRCParams const &par) = 0;
    virtual void Reset(cBRCParams const &par) = 0;
    virtual void Update(mfxU32 sizeInbits, mfxU32 encOrder, bool bSEI) = 0;
    virtual mfxU32 GetInitCpbRemovalDelay(mfxU32 encOrder) const = 0;
    virtual mfxU32 GetInitCpbRemovalDelayOffset(mfxU32 encOrder) const = 0;
    virtual mfxU32 GetMaxFrameSizeInBits(mfxU32 encOrder, bool bSEI) const = 0;
    virtual mfxU32 GetMinFrameSizeInBits(mfxU32 encOrder, bool bSEI) const = 0;
    virtual mfxF64 GetBufferDeviation(mfxU32 encOrder) const = 0;
    virtual mfxF64 GetBufferDeviationFactor(mfxU32 encOrder) const = 0;
};

class HEVC_HRD: public HRDCodecSpec
{
public:
    HEVC_HRD() :
          m_prevAuCpbRemovalDelayMinus1(0)
        , m_prevAuCpbRemovalDelayMsb(0)
        , m_prevAuFinalArrivalTime(0)
        , m_prevBpAuNominalRemovalTime(0)
        , m_prevBpEncOrder(0)
    {}
    virtual ~HEVC_HRD() {}
    void Init(cBRCParams const&par) override;
    void Reset(cBRCParams const &par) override;
    void Update(mfxU32 sizeInbits, mfxU32 eo,  bool bSEI) override;
    mfxU32 GetInitCpbRemovalDelay(mfxU32 eo)  const override;
    mfxU32 GetInitCpbRemovalDelayOffset(mfxU32 eo)  const override
    {
        return mfxU32(m_hrdInput.m_cpbSize90k - GetInitCpbRemovalDelay(eo));
    }
    mfxU32 GetMaxFrameSizeInBits(mfxU32 eo, bool bSEI)  const override;
    mfxU32 GetMinFrameSizeInBits(mfxU32 eo, bool bSEI)  const override;
    mfxF64 GetBufferDeviation(mfxU32 eo)  const override;
    mfxF64 GetBufferDeviationFactor(mfxU32 eo)  const override;

protected:
    sHrdInput m_hrdInput;
    mfxI32 m_prevAuCpbRemovalDelayMinus1;
    mfxU32 m_prevAuCpbRemovalDelayMsb;
    mfxF64 m_prevAuFinalArrivalTime;
    mfxF64 m_prevBpAuNominalRemovalTime;
    mfxU32 m_prevBpEncOrder;
};

class H264_HRD: public HRDCodecSpec
{
public:
    H264_HRD();
    virtual ~H264_HRD() {}
    void Init(cBRCParams const &par) override;
    void Reset(cBRCParams const &par) override;
    void Update(mfxU32 sizeInbits, mfxU32 eo, bool bSEI) override;
    mfxU32 GetInitCpbRemovalDelay(mfxU32 eo)  const override;
    mfxU32 GetInitCpbRemovalDelayOffset(mfxU32 eo)  const override;
    mfxU32 GetMaxFrameSizeInBits(mfxU32 eo, bool bSEI)  const override;
    mfxU32 GetMinFrameSizeInBits(mfxU32 eo, bool bSEI)  const override;
    mfxF64 GetBufferDeviation(mfxU32 eo)  const override;
    mfxF64 GetBufferDeviationFactor(mfxU32 eo)  const override;


private:
    sHrdInput m_hrdInput;
    double m_trn_cur;   // nominal removal time
    double m_taf_prv;   // final arrival time of prev unit

};

struct LA_Ctx
{
LA_Ctx():
    LastLaPBitsAvg(LA_P_UPDATE_DIST + MAX_GOP_REFDIST, 0),
    LastLaQpCalc(0),
    LastLaQpCalcEncOrder(0),
    LastLaQpCalcDispOrder(0),
    LastLaQpCalcHasI(0),
    LastLaQpUpdateEncOrder(0),
    LastLaQpUpdateDispOrder(0),
    LastLaIBits(0),
    LastMBQpSetEncOrder(0),
    LastMBQpSetDispOrder(0)
    {}

    std::vector<mfxU32> LastLaPBitsAvg;  // History of Moving Avg of LA bits of last P frames
    mfxU32 LastLaQpCalc;                 // Last LaQp calculated
    mfxU32 LastLaQpCalcEncOrder;
    mfxU32 LastLaQpCalcDispOrder;
    bool   LastLaQpCalcHasI;
    mfxU32 LastLaQpUpdateEncOrder;
    mfxU32 LastLaQpUpdateDispOrder;
    mfxU32 LastLaIBits;                 // LA bits of Last Intra Frame
    mfxU32 LastMBQpSetEncOrder;
    mfxU32 LastMBQpSetDispOrder;

    bool IsCalcLaQpDist(mfxU32 dispOrder)
    {
        return dispOrder >= LastLaQpCalcDispOrder + LA_P_UPDATE_DIST;
    }
    bool IsUpdateLaQpDist(mfxU32 dispOrder)
    {
        return dispOrder >= LastLaQpUpdateDispOrder + LA_P_UPDATE_DIST;
    }
    void SetLaQpCalcOrder(mfxU32 encOrder, mfxU32 dispOrder)
    {
        LastLaQpCalcEncOrder = encOrder;
        LastLaQpCalcDispOrder = dispOrder;
    }
    void SetLaQpUpdateOrder(mfxU32 encOrder, mfxU32 dispOrder) 
    { 
        LastLaQpUpdateEncOrder = encOrder;
        LastLaQpUpdateDispOrder = dispOrder;
    }
    mfxU32 GetLastCalcLaBitsAvg(mfxU32 encOrder)
    {
        return LastLaPBitsAvg[mfx::clamp((mfxI32)encOrder - (mfxI32)LastLaQpCalcEncOrder - 1, 0, (mfxI32)LastLaPBitsAvg.size() - 1)];
    }
    void SaveLaBits(bool intra, mfxU32 LaCurEncodedSize, mfxU32 period, bool rotate)
    {
        if (intra) LastLaIBits = LaCurEncodedSize;
        if (rotate) std::rotate(LastLaPBitsAvg.rbegin(), LastLaPBitsAvg.rbegin() + 1, LastLaPBitsAvg.rend());
        if (LastLaPBitsAvg[1])
        {
            LastLaPBitsAvg[0] = (mfxI32)LastLaPBitsAvg[1] +
                ((mfxI32)LaCurEncodedSize - (mfxI32)LastLaPBitsAvg[1]) / (mfxI32)period;
        }
        else
        {
            LastLaPBitsAvg[0] = LaCurEncodedSize;
        }
    }
};

struct BRC_Ctx
{
    BRC_Ctx() :
        QuantIDR(0),
        QuantI(0),
        QuantP(0),
        QuantB(0),
        Quant(0),
        QuantMin(0),
        QuantMax(0),
        bToRecode(false),
        bPanic(false),
        encOrder(0),
        poc(0),
        SceneChange(0),
        SChPoc(0),
        LastIEncOrder(0),
        LastIDREncOrder(0),
        LastIDRSceneChange(0),
        LastIQpAct(0),
        LastIFrameSize(0),
        LastICmplx(0),
        LastIQpSetOrder(0),
        LastQpUpdateOrder(0),
        LastIQpMin(0),
        LastIQpSet(0),
        LastNonBFrameSize(0),
        la(),
        fAbLong(0),
        fAbShort(0),
        fAbLA(0),
        dQuantAb(0),
        totalDeviation(0),
        eRate(0),
        eRateSH(0)
    {}

    mfxI32 QuantIDR;  //currect qp for intra frames
    mfxI32 QuantI;  //currect qp for intra frames
    mfxI32 QuantP;  //currect qp for P frames
    mfxI32 QuantB;  //currect qp for B frames

    mfxI32 Quant;           // qp for last encoded frame
    mfxI32 QuantMin;        // qp Min for last encoded frame (is used for recoding)
    mfxI32 QuantMax;        // qp Max for last encoded frame (is used for recoding)

    bool   bToRecode;       // last frame is needed in recoding
    bool   bPanic;          // last frame is needed in panic
    mfxU32 encOrder;        // encoding order of last encoded frame
    mfxU32 poc;             // poc of last encoded frame
    mfxI32 SceneChange;     // scene change parameter of last encoded frame
    mfxU32 SChPoc;          // poc of frame with scene change
    mfxU32 LastIEncOrder;   // encoded order of last intra frame
    mfxU32 LastIDREncOrder;   // encoded order of last idr frame
    mfxU32 LastIDRSceneChange; // last idr was scene change
    mfxU32 LastIQpAct;      // Qp of last intra frame
    mfxU32 LastIFrameSize; // encoded frame size of last non B frame (is used for sceneChange)
    mfxF64 LastICmplx;      // Cmplx of last intra frame

    mfxU32 LastIQpSetOrder; // Qp of last intra frame
    mfxU32 LastQpUpdateOrder;   // When using UpdateQpParams
    mfxU32 LastIQpMin;      // Qp of last intra frame
    mfxU32 LastIQpSet;      // Qp of last intra frame

    mfxU32 LastNonBFrameSize; // encoded frame size of last non B frame (is used for sceneChange)
    LA_Ctx la;
    mfxF64 fAbLong;         // frame aberration (long period)
    mfxF64 fAbShort;        // frame aberration (short period)
    mfxF64 fAbLA;           // frame aberration (LA period)
    mfxF64 dQuantAb;        // dequant aberration
    mfxF64 totalDeviation;   // deviation from  target bitrate (total)

    mfxF64 eRate;               // eRate of last encoded frame, this parameter is used for scene change calculation
    mfxF64 eRateSH;             // eRate of last encoded scene change frame, this parameter is used for scene change calculation
};

class AVGBitrate
{
public:
    AVGBitrate(mfxU32 windowSize, mfxU32 maxBitPerFrame, mfxU32 avgBitPerFrame, bool bLA = false):
        m_maxWinBits(maxBitPerFrame*windowSize),
        m_maxWinBitsLim(0),
        m_avgBitPerFrame(std::min(avgBitPerFrame, maxBitPerFrame)),
        m_currPosInWindow(windowSize - 1),
        m_lastFrameOrder(mfxU32(-1)),
        m_bLA(bLA)

    {
        windowSize = windowSize > 0 ? windowSize : 1; // kw
        m_slidingWindow.resize(windowSize);
        for (mfxU32 i = 0; i < windowSize; i++)
        {
            m_slidingWindow[i] = maxBitPerFrame / 3; //initial value to prevent big first frames
        }
        m_maxWinBitsLim = GetMaxWinBitsLim();
    }
    virtual ~AVGBitrate()
    {
        //printf("------------ AVG Bitrate: %d ( %d), NumberOfErrors %d\n", m_MaxBitReal, m_MaxBitReal_temp, m_NumberOfErrors);
    }
    void UpdateSlidingWindow(mfxU32  sizeInBits, mfxU32  FrameOrder, bool bPanic, bool bSH, mfxU32 recode, mfxU32 /* qp */)
    {
        mfxU32 windowSize = (mfxU32)m_slidingWindow.size();
        bool   bNextFrame = FrameOrder != m_lastFrameOrder;

        if (bNextFrame)
        {
            m_lastFrameOrder = FrameOrder;
            m_currPosInWindow = (m_currPosInWindow + 1) % windowSize;
        }
        m_slidingWindow[m_currPosInWindow] = sizeInBits;

        if (bNextFrame)
        {
            if (bPanic || bSH)
            {
                m_maxWinBitsLim = mfx::clamp((GetLastFrameBits(windowSize,false) + m_maxWinBits) / 2, GetMaxWinBitsLim(), m_maxWinBits);
            }
            else
            {
                if (recode)
                    m_maxWinBitsLim = mfx::clamp(GetLastFrameBits(windowSize,false) + GetStep() / 2, m_maxWinBitsLim, m_maxWinBits);
                else if ((m_maxWinBitsLim > GetMaxWinBitsLim() + GetStep()) &&
                    (m_maxWinBitsLim - GetStep() > (GetLastFrameBits(windowSize - 1, false) + sizeInBits)))
                    m_maxWinBitsLim -= GetStep();
            }

        }
    }

    mfxU32 GetMaxFrameSize(bool bPanic, bool bSH, mfxU32 recode) const
    {
        mfxU32 winBits = GetLastFrameBits(GetWindowSize() - 1, !bPanic);

        mfxU32 maxWinBitsLim = m_maxWinBitsLim;
        if (bSH)
            maxWinBitsLim = (m_maxWinBits + m_maxWinBitsLim) / 2;
        if (bPanic)
            maxWinBitsLim = m_maxWinBits;
        maxWinBitsLim = std::min(maxWinBitsLim + recode * GetStep() / 2, m_maxWinBits);

        mfxU32 maxFrameSize = winBits >= m_maxWinBitsLim ?
            mfxU32(std::max((mfxI32)m_maxWinBits - (mfxI32)winBits, 1)) :
            maxWinBitsLim - winBits;

        return maxFrameSize;
    }

    mfxU32 GetWindowSize() const
    {
        return (mfxU32)m_slidingWindow.size();
    }

    mfxI32 GetBudget(mfxU32 numFrames) const
    {
        numFrames = std::min(mfxU32(m_slidingWindow.size()), numFrames);
        return ((mfxI32)m_maxWinBitsLim - (mfxI32)GetLastFrameBits((mfxU32)m_slidingWindow.size() - numFrames, true));
    }

protected:

    mfxU32                      m_maxWinBits;
    mfxU32                      m_maxWinBitsLim;
    mfxU32                      m_avgBitPerFrame;

    mfxU32                      m_currPosInWindow;
    mfxU32                      m_lastFrameOrder;
    bool                        m_bLA;
    std::vector<mfxU32>         m_slidingWindow;


    mfxU32 GetLastFrameBits(mfxU32 numFrames, bool bCheckSkip) const
    {
        mfxU32 size = 0;
        numFrames = numFrames < m_slidingWindow.size() ? numFrames : (mfxU32)m_slidingWindow.size();
        for (mfxU32 i = 0; i < numFrames; i++)
        {
            mfxU32 frame_size = m_slidingWindow[(m_currPosInWindow + m_slidingWindow.size() - i) % m_slidingWindow.size()];
            if (bCheckSkip && (frame_size < m_avgBitPerFrame / 3))
                frame_size = m_avgBitPerFrame / 3;
            size += frame_size;
            //printf("GetLastFrames: %d) %d sum %d\n",i,m_slidingWindow[(m_currPosInWindow + m_slidingWindow.size() - i) % m_slidingWindow.size() ], size);
        }
        return size;
    }
    mfxU32 GetStep() const
    {
        return  (m_maxWinBits / GetWindowSize() - m_avgBitPerFrame) / (m_bLA ? 4 : 2);
    }

    mfxU32 GetMaxWinBitsLim() const
    {
        return m_maxWinBits - GetStep() * GetWindowSize();
    }
};


class BRC_EncToolBase
    : public IEncToolsBRC
{
public:

    BRC_EncToolBase() :
        m_bInit(false),
        m_par(),
        m_hrdSpec(),
        m_bDynamicInit(false),
        m_ctx(),
        m_SkipCount(0),
        m_ReEncodeCount(0)
    {}

    ~BRC_EncToolBase() override { Close(); }

    mfxStatus Init(mfxEncToolsCtrl const & ctrl, bool bMBBRC, bool bALTR) override;
    mfxStatus Reset(mfxEncToolsCtrl const & ctrl, bool bMBBRC, bool bALTR) override;
    void Close() override
    {
        m_bInit = false;
        m_bDynamicInit = false;
        m_FrameStruct.resize(0);
    }

    mfxStatus ReportEncResult(mfxU32 dispOrder, mfxEncToolsBRCEncodeResult const & pEncRes) override;
    mfxStatus SetFrameStruct(mfxU32 dispOrder, mfxEncToolsBRCFrameParams  const & pFrameStruct) override;
    mfxStatus ReportBufferHints(mfxU32 dispOrder, mfxEncToolsBRCBufferHint const & pBufHints) override;
    mfxStatus ReportGopHints(mfxU32 dispOrder, mfxEncToolsHintPreEncodeGOP const & pGopHints) override;
    mfxStatus ProcessFrame(mfxU32 dispOrder, mfxEncToolsBRCQuantControl *pFrameQp, mfxEncToolsHintQPMap* qpMapHint) override;
    mfxStatus UpdateFrame(mfxU32 dispOrder, mfxEncToolsBRCStatus *pFrameSts) override;
    mfxStatus GetHRDPos(mfxU32 dispOrder, mfxEncToolsBRCHRDPos *pHRDPos) override;
    mfxStatus DiscardFrame(mfxU32 dispOrder) override;

protected:

    bool m_bInit;
    cBRCParams m_par;
    std::unique_ptr < HRDCodecSpec> m_hrdSpec;
    bool       m_bDynamicInit;
    BRC_Ctx    m_ctx;
    std::unique_ptr<AVGBitrate> m_avg;
    mfxU32     m_SkipCount;
    mfxU32     m_ReEncodeCount;
    std::vector<BRC_FrameStruct> m_FrameStruct;

    virtual mfxU16 FillQpMap(const BRC_FrameStruct&, mfxU32 /*frameQp*/, mfxEncToolsHintQPMap*, mfxI16&) = 0;

    mfxI32 GetCurQP(mfxU32 type, mfxI32 layer, mfxU16 isRef, mfxU16 qpMod, mfxI8 qpExp, mfxI32 qpDeltaP) const;
    mfxI32 GetSeqQP(mfxI32 qp, mfxU32 type, mfxI32 layer, mfxU16 isRef, mfxU16 qpMod, mfxI8 qpExp, mfxI32 qpDeltaP) const;
    mfxI32 GetPicQP(mfxI32 qp, mfxU32 type, mfxI32 layer, mfxU16 isRef, mfxU16 qpMod, mfxI8 qpExp, mfxI32 qpDeltaP) const;
    mfxF64 ResetQuantAb(mfxI32 qp, mfxU32 type, mfxI32 layer, mfxU16 isRef, mfxF64 fAbLong, mfxU32 eo, bool bIdr, mfxU16 qpMod, mfxI8 qpExp, mfxI32 qpDeltaP, bool bNoNewQp) const;
    mfxI32 GetLaQpEst(mfxU32 LaAvgEncodedSize, mfxF64 inputBitsPerFrame, const BRC_FrameStruct& frameStruct, bool updateState);
};

class BRC_EncTool : public BRC_EncToolBase
{
public:

    mfxU16 FillQpMap(const BRC_FrameStruct& frameStruct, mfxU32 frameQp, mfxEncToolsHintQPMap* qpMap, mfxI16& qpMapBias) override;
};

/*
    - Current AV1 enctools is based on HEVC enctools
    - Following 2 LUTs are for the conversion of HEVC QP and AV1 dc_q_idx.  
*/

// HEVC QP to AV1 dc_q_index
mfxU8 const HEVC_QP_2_AV1_DC_Q_IDX[52] = 
{
    1,   1,   1,   1,   2,   3,   4,   5,   7,   9,   11,  13,  16,  19,  21,  26, 
    29,  34,  39,  45,  52,  59,  68,  78,  89,  98,  106, 114, 122, 130, 138, 145,
    153, 162, 170, 178, 186, 195, 203, 211, 219, 226, 231, 236, 241, 245, 248, 251,
    252, 253, 254, 255
};

// AV1 dc_q_index to HEVC QP
mfxU8 const AV1_DC_Q_IDX_2_HEVC_QP[256] = 
{
    0, 4, 4, 5, 6, 7, 8, 8, 8, 9, 9, 10,11,11,11,11,
    12,12,13,13,14,14,14,14,15,15,15,16,16,16,16,16,
    17,17,17,17,17,17,17,18,18,18,18,19,19,19,19,19,
    20,20,20,20,20,20,20,20,21,21,21,21,21,21,21,22,
    22,22,22,22,22,22,22,22,22,23,23,23,23,23,23,23,
    23,23,23,23,23,24,24,24,24,24,24,24,24,24,24,24,
    25,25,25,25,25,25,26,26,26,26,26,26,26,26,27,27,
    27,27,27,27,27,27,27,28,28,28,28,28,28,28,29,29,
    29,29,29,29,29,29,29,30,30,30,30,30,30,30,31,31,
    31,31,31,31,31,31,32,32,32,32,32,32,32,32,33,33,
    33,33,33,33,33,33,34,34,34,34,34,34,34,34,35,35,
    35,35,35,35,35,35,35,36,36,36,36,36,36,36,36,37,
    37,37,37,37,37,37,37,38,38,38,38,38,38,38,38,38,
    39,39,39,39,39,39,39,39,40,40,40,40,40,40,40,41,
    41,41,41,41,41,42,42,42,42,42,43,43,43,43,43,44,
    44,44,44,45,45,45,45,46,46,46,46,47,47,47,48,48
};

}


#endif


