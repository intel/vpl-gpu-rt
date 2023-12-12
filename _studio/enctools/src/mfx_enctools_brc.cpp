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

#include "mfx_enctools.h"
#include "mfx_enctools_brc.h"

#include <algorithm>
#include <math.h>

#define MAX_DQP_LTR 4
#define MAX_MODEL_ERR 6
#define BRC_BUFK(verylowdelay) (verylowdelay? 4.0 : 2.5)
#define SCH_BUFK(verylowdelay) (verylowdelay? 6.0 : 3.5)
#define LTR_BUFK(verylowdelay) (verylowdelay? 8.0:  4.5)

static mfxF64 LTR_BUF(mfxU16 type, mfxU32 dqp, bool boost, bool schg, bool shstrt, bool verylowdelay)
{
    if (type == MFX_FRAMETYPE_IDR)
    {
        if ((schg && !boost) || !dqp)
        {
            return BRC_BUFK(verylowdelay);
        } 
        else
        {
            return LTR_BUFK(verylowdelay);
        }
    }
    else
    {
        if (shstrt)
        {
            return SCH_BUFK(verylowdelay);
        }
        else
        {
            return BRC_BUFK(verylowdelay);
        }
    }
}

#define DQFF0 1.0
#define DQFF1 1.66
#define DQF(type, dqp, boost, schg) \
((type == MFX_FRAMETYPE_IDR) ? ((dqp?pow(2, ((mfxF64)dqp / 6.0)) : 1.0) * ((schg && !boost) ? DQFF0 : DQFF1)) : 1.0)

#define FRM_RATIO(type, encorder, shstrt, pyr) \
((((encorder == 0 && !pyr) || type == MFX_FRAMETYPE_I) ? 6.0 : (shstrt || type == MFX_FRAMETYPE_IDR) ? 8.0 : 4.0) * ((pyr) ? 1.5 : 1.0))

#define BRC_CONST_MUL_P1 2.253264596
#define BRC_CONST_EXP_R_P1 0.42406423

#define ltrprintf(...)
//#define ltrprintf printf

#define BRC_SCENE_CHANGE_RATIO1 20.0
#define BRC_SCENE_CHANGE_RATIO2 5.0

#define BRC_QP_MODULATION_GOP8_FIXED MFX_QP_MODULATION_RESERVED0

namespace EncToolsBRC {

struct CompareByDisplayOrder
{
    mfxU32 m_DispOrder;
    CompareByDisplayOrder(mfxU32 frameOrder) : m_DispOrder(frameOrder) {}
    bool operator ()(BRC_FrameStruct const & frameStruct) const { return frameStruct.dispOrder == m_DispOrder; }
};

static mfxU32 hevcBitRateScale(mfxU32 bitrate)
{
    mfxU32 bit_rate_scale = 0;
    while (bit_rate_scale < 16 && (bitrate & ((1 << (6 + bit_rate_scale + 1)) - 1)) == 0)
        bit_rate_scale++;
    return bit_rate_scale;
}

static mfxU32 hevcCbpSizeScale(mfxU32 cpbSize)
{
    mfxU32 cpb_size_scale = 2;
    while (cpb_size_scale < 16 && (cpbSize & ((1 << (4 + cpb_size_scale + 1)) - 1)) == 0)
        cpb_size_scale++;
    return cpb_size_scale;
}
const mfxU32 h264_h265_au_cpb_removal_delay_length_minus1 = 23;
const mfxU32 h264_bit_rate_scale = 4;
const mfxU32 h264_cpb_size_scale = 2;

mfxI32 GetRawFrameSize(mfxU32 lumaSize, mfxU16 chromaFormat, mfxU16 bitDepthLuma)
{
    mfxI32 frameSize = lumaSize;

    if (chromaFormat == MFX_CHROMAFORMAT_YUV420)
        frameSize += lumaSize / 2;
    else if (chromaFormat == MFX_CHROMAFORMAT_YUV422)
        frameSize += lumaSize;
    else if (chromaFormat == MFX_CHROMAFORMAT_YUV444)
        frameSize += lumaSize * 2;

    frameSize = frameSize * bitDepthLuma / 8;
    return frameSize * 8; //frame size in bits
}

mfxStatus cBRCParams::Init(mfxEncToolsCtrl const & ctrl, bool bMBBRC, bool fieldMode, bool bALTR)
{
    MFX_CHECK(ctrl.RateControlMethod == MFX_RATECONTROL_CBR ||
              ctrl.RateControlMethod == MFX_RATECONTROL_VBR,
              MFX_ERR_UNDEFINED_BEHAVIOR);

    bFieldMode= fieldMode;
    codecId   = ctrl.CodecId;
    lowPower  = IsOn(ctrl.LowPower);
    targetbps = ctrl.TargetKbps * 1000;
    maxbps    = ctrl.MaxKbps * 1000;

    maxbps = (ctrl.RateControlMethod == MFX_RATECONTROL_CBR) ?
        targetbps : ((maxbps >= targetbps) ? maxbps : targetbps);

    mfxU32 bit_rate_scale = (ctrl.CodecId == MFX_CODEC_AVC) ?
        h264_bit_rate_scale : hevcBitRateScale(maxbps);
    mfxU32 cpb_size_scale = (ctrl.CodecId == MFX_CODEC_AVC) ?
        h264_cpb_size_scale : hevcCbpSizeScale(maxbps);

    rateControlMethod  = ctrl.RateControlMethod;
    maxbps =    ((maxbps >> (6 + bit_rate_scale)) << (6 + bit_rate_scale));

    HRDConformance = ctrl.HRDConformance;
    bufferSizeInBytes = ctrl.BufferSizeInKB * 1000; // Bs Size Limit even when HRD off
    if (HRDConformance != MFX_BRC_NO_HRD)
    {
        bufferSizeInBytes = ((ctrl.BufferSizeInKB * 1000) >> (cpb_size_scale + 1)) << (cpb_size_scale + 1);
        initialDelayInBytes = ((ctrl.InitialDelayInKB * 1000) >> (cpb_size_scale + 1)) << (cpb_size_scale + 1);
        bRec = 1;
        bPanic = (HRDConformance == MFX_BRC_HRD_STRONG) ? 1 : 0;
    }
    else if(ctrl.MaxDelayInFrames > ctrl.MaxGopRefDist) {
        bRec = 1; // Needed for LPLA
    }

    MFX_CHECK (ctrl.FrameInfo.FrameRateExtD != 0 &&
               ctrl.FrameInfo.FrameRateExtN != 0,
               MFX_ERR_UNDEFINED_BEHAVIOR);

    frameRate = (mfxF64)ctrl.FrameInfo.FrameRateExtN / (mfxF64)ctrl.FrameInfo.FrameRateExtD;

    width  = ctrl.FrameInfo.Width;
    height = ctrl.FrameInfo.Height;
    cropWidth = ctrl.FrameInfo.CropW ? ctrl.FrameInfo.CropW : width;
    cropHeight = ctrl.FrameInfo.CropH ? ctrl.FrameInfo.CropH : height;

    chromaFormat = ctrl.FrameInfo.ChromaFormat == 0 ?  MFX_CHROMAFORMAT_YUV420 : ctrl.FrameInfo.ChromaFormat ;
    bitDepthLuma = ctrl.FrameInfo.BitDepthLuma == 0 ?  8 : ctrl.FrameInfo.BitDepthLuma;
    mfxI32 bitdepthOffset = (6 * (bitDepthLuma - 8));
    quantOffset   = IsOn(ctrl.LowPower) ? 0 : bitdepthOffset;

    inputBitsPerFrame    = targetbps / frameRate;
    maxInputBitsPerFrame = maxbps / frameRate;
    gopPicSize = ctrl.MaxGopSize*(bFieldMode ? 2 : 1);
    gopRefDist = ctrl.MaxGopRefDist*(bFieldMode ? 2 : 1);

    bPyr = (ctrl.BRefType == MFX_B_REF_PYRAMID);
    maxFrameSizeInBits  = std::max(std::max (ctrl.MaxFrameSizeInBytes[0], ctrl.MaxFrameSizeInBytes[1]),
                                   ctrl.MaxFrameSizeInBytes[2])*8 ;

    if (gopRefDist <= 3) {
        fAbPeriodShort = 6;
    } else {
        fAbPeriodShort = std::max(16, gopRefDist*2);
    }
    // P update, future deviation control, only for LA NO HRD
    mfxU32 CodecId = ctrl.CodecId;
    mfxI32 reaction_mult = (HRDConformance == MFX_BRC_NO_HRD
                            && ctrl.MaxDelayInFrames > ctrl.MaxGopRefDist
                            && (CodecId == MFX_CODEC_HEVC || CodecId == MFX_CODEC_AVC || CodecId == MFX_CODEC_AV1)) ? 
                               (CodecId == MFX_CODEC_HEVC || CodecId == MFX_CODEC_AV1 ? 4 : 3) : 1;

    fAbPeriodLong = 120 * reaction_mult;
    dqAbPeriod = 120 * reaction_mult;
    bAbPeriod = 120 * reaction_mult;
    fAbPeriodLA = std::max(16, gopRefDist*2);
    mRawFrameSizeInBits = GetRawFrameSize(width * height, chromaFormat, IsOn(ctrl.LowPower) ? 8 : bitDepthLuma);
    mRawFrameSizeInPixs = mRawFrameSizeInBits / (IsOn(ctrl.LowPower) ? 8 : bitDepthLuma);

    if (maxFrameSizeInBits)
    {
        bRec = 1;
        bPanic = 1;
    }
    mfxI32 minQPI = IsOn(ctrl.LowPower) ? 10 : 1;
    mfxI32 maxQPI = IsOn(ctrl.LowPower) ? 51 : (51 + bitdepthOffset);
    mfxI32 minQPP = IsOn(ctrl.LowPower) ? 10 : 1;
    mfxI32 maxQPP = IsOn(ctrl.LowPower) ? 51 : (51 + bitdepthOffset);
    mfxI32 minQPB = IsOn(ctrl.LowPower) ? 10 : 1;
    mfxI32 maxQPB = IsOn(ctrl.LowPower) ? 51 : (51 + bitdepthOffset);
    mfxI32 minmaxOffset = IsOn(ctrl.LowPower) ? bitdepthOffset : 0;

    quantMinI = (ctrl.MinQPLevel[0] != 0) ?
        std::max((mfxI32)ctrl.MinQPLevel[0]-minmaxOffset, minQPI) : minQPI;
    quantMaxI = (ctrl.MaxQPLevel[0] != 0) ?
        std::min((mfxI32)ctrl.MaxQPLevel[0]-minmaxOffset, maxQPI) : maxQPI;

    quantMinP = (ctrl.MinQPLevel[1] != 0) ?
        std::max((mfxI32)ctrl.MinQPLevel[1]-minmaxOffset, minQPP) : minQPP;
    quantMaxP = (ctrl.MaxQPLevel[1] != 0) ?
        std::min((mfxI32)ctrl.MaxQPLevel[1]-minmaxOffset, maxQPP) : maxQPP;

    quantMinB = (ctrl.MinQPLevel[2] != 0) ?
        std::max((mfxI32)ctrl.MinQPLevel[2]-minmaxOffset, minQPB) : minQPB;
    quantMaxB = (ctrl.MaxQPLevel[2] != 0) ?
        std::min((mfxI32)ctrl.MaxQPLevel[2]-minmaxOffset, maxQPB) : maxQPB;

    WinBRCMaxAvgKbps = ctrl.WinBRCMaxAvgKbps;
    WinBRCSize = ctrl.WinBRCSize;

    iDQp0 = 0;

    mNumRefsInGop = (mfxU32)(std::max(1.0, (!bPyr ? (mfxF64)gopPicSize / (mfxF64)gopRefDist : (mfxF64)gopPicSize / 2.0)));

    mfxF64 maxFrameRatio = 1.5874 * FRM_RATIO(MFX_FRAMETYPE_IDR, 0, 0, bPyr);

    mIntraBoost = (mNumRefsInGop > maxFrameRatio * 8.0) ? 1 : 0;

    mVeryLowDelay = (HRDConformance != MFX_BRC_NO_HRD && (bufferSizeInBytes * 8.0) / targetbps < 0.12) ? 1 : 0;

    mfxF64 maxFrameSize = mRawFrameSizeInBits;
    if (maxFrameSizeInBits) {
        maxFrameSize = std::min<mfxF64>(maxFrameSize, maxFrameSizeInBits);
    }
    if (HRDConformance != MFX_BRC_NO_HRD) {
        mfxF64 bufOccupy = LTR_BUF(MFX_FRAMETYPE_IDR, 1, mIntraBoost, 1, 0, mVeryLowDelay);
        maxFrameSize = std::min(maxFrameSize, bufOccupy / 9.* (initialDelayInBytes * 8.0) + (9.0 - bufOccupy) / 9.*inputBitsPerFrame);
    }

    mfxF64 minFrameRatio = FRM_RATIO(MFX_FRAMETYPE_IDR, 0, 0, bPyr);
    maxFrameRatio = std::min({maxFrameRatio, maxFrameSize / inputBitsPerFrame, mfxF64(mNumRefsInGop)});
    mfxF64 dqp = std::max(0.0, 6.0 * (log(maxFrameRatio / minFrameRatio) / log(2.0)));
    iDQp0 = (mfxU32)(dqp + 0.5);
    if (iDQp0 < 1) iDQp0 = 1;
    if (iDQp0 > MAX_DQP_LTR) iDQp0 = MAX_DQP_LTR;

    // MaxFrameSize violation prevention
    mMinQstepCmplxKP = BRC_CONST_MUL_P1;
    mMinQstepRateEP = BRC_CONST_EXP_R_P1;
    mMinQstepCmplxKPUpdt = 0;
    mMinQstepCmplxKPUpdtErr = 0.16;
    mLaQp = ctrl.LaQp;
    mLaScale = ctrl.LaScale;
    mLaDepth = ctrl.MaxDelayInFrames;
    mHasALTR = ((ctrl.CodecId == MFX_CODEC_AVC) || (ctrl.CodecId == MFX_CODEC_AV1)) && bALTR;   // check if codec support ALTR
    mMBBRC = (CodecId == MFX_CODEC_HEVC || CodecId == MFX_CODEC_AVC || CodecId == MFX_CODEC_AV1) && bMBBRC;
    return MFX_ERR_NONE;
}

mfxStatus   cBRCParams::GetBRCResetType(mfxEncToolsCtrl const &  ctrl,  bool bNewSequence, bool bMBBRC, bool &bBRCReset, bool &bSlidingWindowReset, bool bALTR)
{
    bBRCReset = false;
    bSlidingWindowReset = false;

    if (bNewSequence)
        return MFX_ERR_NONE;

    cBRCParams new_par;
    mfxStatus sts = new_par.Init(ctrl, bMBBRC,false, bALTR);
    MFX_CHECK_STS(sts);

    MFX_CHECK(new_par.rateControlMethod == rateControlMethod, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) ;
    MFX_CHECK(new_par.HRDConformance == HRDConformance, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) ;
    MFX_CHECK(new_par.frameRate == frameRate, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(new_par.width == width, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(new_par.height == height, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(new_par.chromaFormat == chromaFormat, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(new_par.bitDepthLuma == bitDepthLuma, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    if (HRDConformance == MFX_BRC_HRD_STRONG)
    {
        MFX_CHECK(new_par.bufferSizeInBytes == bufferSizeInBytes, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(new_par.initialDelayInBytes == initialDelayInBytes, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(new_par.targetbps == targetbps, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(new_par.maxbps == maxbps, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    }
    else if (new_par.targetbps != targetbps || new_par.maxbps != maxbps)
    {
        bBRCReset = true;
    }

    if (new_par.WinBRCMaxAvgKbps != WinBRCMaxAvgKbps)
    {
        bBRCReset = true;
        bSlidingWindowReset = true;
    }

    if (new_par.maxFrameSizeInBits != maxFrameSizeInBits) bBRCReset = true;
    if (new_par.gopPicSize != gopPicSize) bBRCReset = true;
    if (new_par.gopRefDist != gopRefDist) bBRCReset = true;
    if (new_par.bPyr != bPyr) bBRCReset = true;
    if (new_par.quantMaxI != quantMaxI) bBRCReset = true;
    if (new_par.quantMinI != quantMinI) bBRCReset = true;
    if (new_par.quantMaxP != quantMaxP) bBRCReset = true;
    if (new_par.quantMinP != quantMinP) bBRCReset = true;
    if (new_par.quantMaxB != quantMaxB) bBRCReset = true;
    if (new_par.quantMinB != quantMinB) bBRCReset = true;

    return MFX_ERR_NONE;
}

enum
{
    MFX_BRC_RECODE_NONE           = 0,
    MFX_BRC_RECODE_QP             = 1,
    MFX_BRC_RECODE_PANIC          = 2,
};

mfxF64 const QSTEP[88] =
{
      0.630,  0.707,  0.794,  0.891,  1.000,   1.122,   1.260,   1.414,   1.587,   1.782,   2.000,   2.245,   2.520,
      2.828,  3.175,  3.564,  4.000,  4.490,   5.040,   5.657,   6.350,   7.127,   8.000,   8.980,  10.079,  11.314,
     12.699, 14.254, 16.000, 17.959, 20.159,  22.627,  25.398,  28.509,  32.000,  35.919,  40.317,  45.255,  50.797,
     57.018, 64.000, 71.838, 80.635, 90.510, 101.594, 114.035, 128.000, 143.675, 161.270, 181.019, 203.187, 228.070,
     256.000, 287.350, 322.540, 362.039, 406.375, 456.140, 512.000, 574.701, 645.080, 724.077, 812.749, 912.280,
     1024.000, 1149.401, 1290.159, 1448.155, 1625.499, 1824.561, 2048.000, 2298.802, 2580.318, 2896.309, 3250.997, 3649.121,
     4096.000, 4597.605, 5160.637, 5792.619, 6501.995, 7298.242, 8192.000, 9195.209, 10321.273, 11585.238, 13003.989, 14596.485
};

inline
mfxI32 QStep2QpFloor(mfxF64 qstep, mfxI32 qpoffset = 0) // QSTEP[qp] <= qstep, return 0<=qp<=51+mQuantOffset
{
    mfxU8 qp = mfxU8(std::upper_bound(QSTEP, QSTEP + 51 + qpoffset, qstep) - QSTEP);
    return qp > 0 ? qp - 1 : 0;
}

inline
mfxI32 Qstep2QP(mfxF64 qstep, mfxI32 qpoffset = 0) // return 0<=qp<=51+mQuantOffset
{
    mfxI32 qp = QStep2QpFloor(qstep, qpoffset);
    // prevent going QSTEP index out of bounds
    if (qp >= (mfxI32)(sizeof(QSTEP)/sizeof(QSTEP[0])) - 1)
        return 0;
    return (qp == 51 + qpoffset || qstep < (QSTEP[qp] + QSTEP[qp + 1]) / 2) ? qp : qp + 1;
}

inline
mfxF64 QP2Qstep(mfxI32 qp, mfxI32 qpoffset = 0)
{
    return QSTEP[std::min(51 + qpoffset, qp)];
}

inline
mfxU16 CheckHrdAndUpdateQP(HRDCodecSpec &hrd, mfxU32 frameSizeInBits, mfxU32 eo, bool bIdr, mfxI32 currQP)
{
    if (frameSizeInBits > hrd.GetMaxFrameSizeInBits(eo, bIdr))
    {
        hrd.SetUnderflowQuant(currQP);
        return MFX_BRC_BIG_FRAME;
    }
    else if (frameSizeInBits < hrd.GetMinFrameSizeInBits(eo, bIdr))
    {
        hrd.SetOverflowQuant(currQP);
        return MFX_BRC_SMALL_FRAME;
    }
    return MFX_BRC_OK;
}

inline
mfxI32 GetFrameTargetSize(mfxU32 brcSts, mfxI32 minFrameSize, mfxI32 maxFrameSize)
{
    if (brcSts != MFX_BRC_BIG_FRAME && brcSts != MFX_BRC_SMALL_FRAME) return 0;
    return (brcSts == MFX_BRC_BIG_FRAME) ? maxFrameSize * 3 / 4 : minFrameSize * 5 / 4;
}

static
mfxI32 GetNewQP(mfxF64 totalFrameBits, mfxF64 targetFrameSizeInBits, mfxI32 minQP , mfxI32 maxQP, mfxI32 qp , mfxI32 qp_offset, mfxF64 f_pow, bool bStrict = false, bool bLim = true)
{
    mfxF64 qstep = 0, qstep_new = 0;
    mfxI32 qp_new = qp;

    qstep = QP2Qstep(qp, qp_offset);
    qstep_new = qstep * pow(totalFrameBits / targetFrameSizeInBits, f_pow);
    qp_new = Qstep2QP(qstep_new, qp_offset);

    if (totalFrameBits < targetFrameSizeInBits) // overflow
    {
        if (qp <= minQP)
        {
            return qp; // QP change is impossible
        }
        if (bLim)
            qp_new  = std::max(qp_new, (minQP + qp + 1) >> 1);
        if (bStrict)
            qp_new  = std::min(qp_new, qp - 1);
    }
    else // underflow
    {
        if (qp >= maxQP)
        {
            return qp; // QP change is impossible
        }
        if (bLim)
            qp_new  = std::min(qp_new, (maxQP + qp + 1) >> 1);
        if (bStrict)
            qp_new  = std::max(qp_new, qp + 1);
    }
    return mfx::clamp(qp_new, minQP, maxQP);
}

// Get QP Offset for given frame and Adaptive Pyramid QP class 
// level = Pyramid level or Layer, value [1-3]
// isRef = zero for non-reference frame
// qpMod = Adaptive Pyramid QP class, value [0-4]
// QP Offset is realtive QuantB.
// QuantB = QuantP+1
// qpMod=0, can be for used non 8GOP and/or non Pyramid cases.
static
mfxI32 GetOffsetAPQ(mfxI32 level, mfxU16 isRef, mfxU16 qpMod, mfxI8 qpQPDeltaExplicitModulation, mfxU32 codecId)
{
    mfxI32 qp = 0;
    level = std::max(mfxI32(1), std::min(mfxI32(4), level));

    if (qpMod == MFX_QP_MODULATION_EXPLICIT) {
        qp = (int)qpQPDeltaExplicitModulation;
    }
    else {
        if (codecId == MFX_CODEC_HEVC || codecId == MFX_CODEC_AV1) {
            if (qpMod == MFX_QP_MODULATION_HIGH || qpMod == BRC_QP_MODULATION_GOP8_FIXED) {
                switch (level) {
                case 4:
                    qp += 1;
                case 3:
                    qp += (codecId == MFX_CODEC_HEVC) ? 1 : 2;
                case 2:
                    qp += (codecId == MFX_CODEC_HEVC) ? 2 : 1;
                case 1:
                default:
                    qp += (codecId == MFX_CODEC_HEVC) ? 2 : 2;
                    break;
                }
            }
            else if (qpMod == MFX_QP_MODULATION_MIXED) {
                switch (level) {
                case 4:
                    qp += 1;
                case 3:
                    qp += 2;
                case 2:
                    qp += 1;
                case 1:
                default:
                    qp += 4;
                    break;
                }
            }
            else if (qpMod == MFX_QP_MODULATION_MEDIUM) {
                switch (level) {
                case 4:
                    qp += 1;
                case 3:
                    qp += 1;
                case 2:
                    qp += 1;
                case 1:
                default:
                    qp += 2;
                    break;
                }
            }
            else if (qpMod == MFX_QP_MODULATION_LOW) {
                switch (level) {
                case 4:
                    qp += 1;
                case 3:
                    qp += 1;
                case 2:
                    qp += 1;
                case 1:
                default:
                    qp += 1;
                    break;
                }
            }
            else {
                qp += (level > 0 ? level - 1 : 0);
                if (level && !isRef) qp += 1;
            }
        }
        else if (codecId == MFX_CODEC_AVC) {
            if (qpMod == MFX_QP_MODULATION_HIGH) {
                switch (level) {
                case 4:
                    qp += 1;
                case 3:
                    qp += 2;
                case 2:
                    qp += 1;
                case 1:
                default:
                    qp += 3;
                    break;
                }
            }
            else if (qpMod == MFX_QP_MODULATION_MEDIUM) {
                switch (level) {
                case 4:
                    qp += 1;
                case 3:
                    qp += 2;
                case 2:
                    qp += 1;
                case 1:
                default:
                    qp += 2;
                    break;
                }
            }
            else if (qpMod == MFX_QP_MODULATION_LOW) {
                switch (level) {
                case 4:
                    qp += 1;
                case 3:
                    qp += 1;
                case 2:
                    qp += 1;
                case 1:
                default:
                    qp += 1;
                    break;
                }
            }
            else {
                qp += (level > 0 ? level - 1 : 0);
                if (level && !isRef) qp += 1;
            }
        }
        else {
            qp += (level > 0 ? level - 1 : 0);
            if (level && !isRef) qp += 1;
        }
    }
    return qp;
}

static
void SetQPParams(mfxI32 qp, mfxU32 type, BRC_Ctx& ctx, mfxU32 /* rec_num */, mfxI32 minQuant, mfxI32 maxQuant, mfxU32 level, mfxU32 iDQp, mfxU16 isRef, mfxU16 qpMod, mfxI8 qpExp, mfxI32 qpDeltaP, mfxU32 codecId)
{
    if (type == MFX_FRAMETYPE_IDR)
    {
        ctx.QuantIDR = qp;
        ctx.QuantI = qp + iDQp;
        ctx.QuantP = qp + 1 + iDQp;
        ctx.QuantB = qp + 2 + iDQp;
    }
    else if (type == MFX_FRAMETYPE_I)
    {
        ctx.QuantIDR = qp - iDQp;
        ctx.QuantI = qp;
        ctx.QuantP = qp + 1;
        ctx.QuantB = qp + 2;
    }
    else if (type == MFX_FRAMETYPE_P)
    {
        qp -= level;
        qp -= qpDeltaP;
        ctx.QuantIDR = qp - 1 - iDQp;
        ctx.QuantI = qp - 1;
        ctx.QuantP = qp;
        ctx.QuantB = qp + 1;
    }
    else if (type == MFX_FRAMETYPE_B)
    {
        qp -= GetOffsetAPQ(level, isRef, qpMod, qpExp, codecId);
        ctx.QuantIDR = qp - 2 - iDQp;
        ctx.QuantI = qp - 2;
        ctx.QuantP = qp - 1;
        ctx.QuantB = qp;
    }
    ctx.QuantIDR = mfx::clamp(ctx.QuantIDR, minQuant, maxQuant);
    ctx.QuantI   = mfx::clamp(ctx.QuantI,   minQuant, maxQuant);
    ctx.QuantP   = mfx::clamp(ctx.QuantP,   minQuant, maxQuant);
    ctx.QuantB   = mfx::clamp(ctx.QuantB,   minQuant, maxQuant);
    //printf("ctx.QuantIDR %d, QuantI %d, ctx.QuantP %d, ctx.QuantB  %d, level %d iDQp %d\n", ctx.QuantIDR, ctx.QuantI, ctx.QuantP, ctx.QuantB, level, iDQp);
}

inline
void UpdateQPParams(mfxI32 qp, mfxU32 type , BRC_Ctx  &ctx, mfxU32 rec_num, mfxI32 minQuant, mfxI32 maxQuant, mfxU32 level, mfxU32 iDQp, mfxU16 isRef, mfxU16 qpMod, mfxI8 qpExp, mfxI32 qpDeltaP, mfxU32 codecId)
{
    ctx.Quant = qp;
    if (ctx.LastIQpSetOrder > ctx.encOrder) return;
    if (ctx.LastQpUpdateOrder > ctx.encOrder) return;
    ctx.LastQpUpdateOrder = ctx.encOrder;
    SetQPParams(qp, type, ctx, rec_num, minQuant, maxQuant, level, iDQp, isRef, qpMod, qpExp, qpDeltaP, codecId);
}

inline
mfxU16 GetFrameType(mfxU16 m_frameType, mfxU16 level, mfxU16 gopRefDist, mfxU32 codecId)
{
    if (m_frameType & MFX_FRAMETYPE_IDR)
        return MFX_FRAMETYPE_IDR;
    else if ((m_frameType & MFX_FRAMETYPE_I) && codecId == MFX_CODEC_HEVC)
        return MFX_FRAMETYPE_IDR;   // For CRA
    else if (m_frameType & MFX_FRAMETYPE_I)
        return MFX_FRAMETYPE_I;
    else if (m_frameType & MFX_FRAMETYPE_P)
        return MFX_FRAMETYPE_P;
    else if ((m_frameType & MFX_FRAMETYPE_REF) && (level == 0 || gopRefDist == 1))
        return MFX_FRAMETYPE_P; //low delay B
    else
        return MFX_FRAMETYPE_B;
}

inline
bool isFrameBeforeIntra(mfxU32 order, mfxU32 intraOrder, mfxU32 gopPicSize, mfxU32 gopRefDist)
{
    mfxI32 distance0 = gopPicSize * 3 / 4;
    mfxI32 distance1 = gopPicSize - gopRefDist * 3;
    return (order - intraOrder) > (mfxU32)(std::max(distance0, distance1));
}

static
mfxStatus SetRecodeParams(mfxU16 brcStatus, mfxI32 qp, mfxI32 qp_new, mfxI32 minQP, mfxI32 maxQP, BRC_Ctx &ctx, mfxBRCFrameStatus* status)
{
    ctx.bToRecode = 1;

    if (brcStatus == MFX_BRC_BIG_FRAME || brcStatus == MFX_BRC_PANIC_BIG_FRAME )
    {
         MFX_CHECK(qp_new >= qp, MFX_ERR_UNDEFINED_BEHAVIOR);
         ctx.Quant = qp_new;
         ctx.QuantMax = maxQP;
         if (brcStatus == MFX_BRC_BIG_FRAME && qp_new > qp)
         {
            ctx.QuantMin = std::max(qp + 1, minQP); //limit QP range for recoding
            status->BRCStatus = MFX_BRC_BIG_FRAME;

         }
         else
         {
             ctx.QuantMin = minQP;
             ctx.bPanic = 1;
             status->BRCStatus = MFX_BRC_PANIC_BIG_FRAME;
         }

    }
    else if (brcStatus == MFX_BRC_SMALL_FRAME || brcStatus == MFX_BRC_PANIC_SMALL_FRAME)
    {
         MFX_CHECK(qp_new <= qp, MFX_ERR_UNDEFINED_BEHAVIOR);

         ctx.Quant = qp_new;
         ctx.QuantMin = minQP; //limit QP range for recoding

         if (brcStatus == MFX_BRC_SMALL_FRAME && qp_new < qp)
         {
            ctx.QuantMax = std::min(qp - 1, maxQP);
            status->BRCStatus = MFX_BRC_SMALL_FRAME;
         }
         else
         {
            ctx.QuantMax = maxQP;
            status->BRCStatus = MFX_BRC_PANIC_SMALL_FRAME;
            ctx.bPanic = 1;
         }
    }
    //printf("recode %d, qp %d new %d, status %d\n", ctx.encOrder, qp, qp_new, status->BRCStatus);
    return MFX_ERR_NONE;
}

static
mfxI32 GetNewQPTotal(mfxF64 bo, mfxF64 dQP, mfxI32 minQP , mfxI32 maxQP, mfxI32 qp, bool bPyr, bool bSC)
{
    mfxU8 mode = (!bPyr) ;

    bo  = mfx::clamp(bo, -1.0, 1.0);
    dQP = mfx::clamp(dQP, 1./maxQP, 1./minQP);

    mfxF64 ndQP = dQP + (1. / maxQP - dQP) * bo;
    ndQP = mfx::clamp(ndQP, 1. / maxQP, 1. / minQP);
    mfxI32 quant_new = (mfxI32) (1. / ndQP + 0.5);

    //printf("   GetNewQPTotal: bo %f, quant %d, quant_new %d, mode %d\n", bo, qp, quant_new, mode);
    if (!bSC)
    {
        if (mode == 0) // low: qp_diff [-2; 2]
        {
            if (quant_new >= qp + 5)
                quant_new = qp + 2;
            else if (quant_new > qp + 1)
                quant_new = qp + 1;
            else if (quant_new <= qp - 5)
                quant_new = qp - 2;
            else if (quant_new < qp - 1)
                quant_new = qp - 1;
        }
        else // (mode == 1) midle: qp_diff [-3; 3]
        {
            if (quant_new >= qp + 5)
                quant_new = qp + 3;
            else if (quant_new > qp + 2)
                quant_new = qp + 2;
            else if (quant_new <= qp - 5)
                quant_new = qp - 3;
            else if (quant_new < qp - 2)
                quant_new = qp - 2;
        }
    }
    else
    {
        quant_new = mfx::clamp(quant_new, qp - 5, qp + 5);
    }
    return mfx::clamp(quant_new, minQP, maxQP);
}

// Reduce AB period before intra and increase it after intra (to avoid intra frame affect on the bottom of hrd)
static
mfxF64 GetAbPeriodCoeff(mfxU32 numInGop, mfxU32 gopPicSize, mfxU32 SC)
{
    const mfxU32 maxForCorrection = 30;
    mfxF64 maxValue = (SC) ? 1.3 : 1.5;
    const mfxF64 minValue = 1.0;

    mfxU32 numForCorrection = std::min(gopPicSize /2, maxForCorrection);
    mfxF64 k[maxForCorrection] = {0};

    if (numInGop >= gopPicSize || gopPicSize < 2)
        return 1.0;

    for (mfxU32 i = 0; i < numForCorrection; i ++)
    {
        k[i] = maxValue - (maxValue - minValue)*i/numForCorrection;
    }
    if (numInGop < gopPicSize/2)
    {
        return k [numInGop < numForCorrection ? numInGop : numForCorrection - 1];
    }
    else
    {
        mfxU32 n = gopPicSize - 1 - numInGop;
        return 1.0/ k[n < numForCorrection ? n : numForCorrection - 1];
    }
}

static
void ResetMinQForMaxFrameSize(cBRCParams* par, mfxU32 type)
{
    if (type == MFX_FRAMETYPE_IDR || type == MFX_FRAMETYPE_I || type == MFX_FRAMETYPE_P) {
        par->mMinQstepCmplxKPUpdt = 0;
        par->mMinQstepCmplxKPUpdtErr = 0.16;
        par->mMinQstepCmplxKP = BRC_CONST_MUL_P1;
        par->mMinQstepRateEP = BRC_CONST_EXP_R_P1;
    }
}

static
mfxI32 GetMinQForMaxFrameSize(const cBRCParams& par, mfxF64 targetBits, mfxU32 type)
{
    mfxI32 qp = 0;
    if (type == MFX_FRAMETYPE_P) {
        if (par.mMinQstepCmplxKPUpdt > 2 && par.mMinQstepCmplxKPUpdtErr < 0.69) {
            mfxI32 rawSize = par.mRawFrameSizeInPixs;
            mfxF64 BitsDesiredFrame = targetBits * (1.0 - 0.165 - std::min(0.115, par.mMinQstepCmplxKPUpdtErr/3.0));
            mfxF64 R = (mfxF64)rawSize / BitsDesiredFrame;
            mfxF64 QstepScale = pow(R, par.mMinQstepRateEP) * par.mMinQstepCmplxKP;
            QstepScale = std::min(128.0, QstepScale);
            mfxF64 minqp = 6.0*log(QstepScale) / log(2.0) + 12.0;
            minqp = std::max(0.0, minqp);
            qp = (mfxU32)(minqp + 0.5);
            qp = mfx::clamp(qp, 1, 51);
        }
    }
    return qp;
}

static
void UpdateMinQForMaxFrameSize(cBRCParams* par, mfxI32 bits, mfxI32 qp, const BRC_Ctx& ctx, mfxU32 type, bool shstrt, mfxU16 brcSts)
{
    if (type == MFX_FRAMETYPE_I || type == MFX_FRAMETYPE_IDR) {
        mfxI32 rawSize = par->mRawFrameSizeInPixs;
        mfxF64 R = (mfxF64)rawSize / (mfxF64)bits;
        mfxF64 QstepScaleComputed = pow(R, par->mMinQstepRateEP) * par->mMinQstepCmplxKP;
        mfxF64 QstepScaleReal = pow(2.0, ((mfxF64)qp - 12.0) / 6.0);
        if (QstepScaleComputed > QstepScaleReal) {
            // Next P Frame atleast as complex as I Frame
            mfxF64 dS = log(QstepScaleReal) - log(QstepScaleComputed);
            par->mMinQstepCmplxKPUpdtErr = std::max<mfxF64>((par->mMinQstepCmplxKPUpdtErr + abs(dS)) / 2, abs(dS));
            mfxF64 upDlt = 0.5;
            dS = mfx::clamp(dS, -0.5, 1.0);
            par->mMinQstepCmplxKP = par->mMinQstepCmplxKP*(1.0 + upDlt*dS);
            //par->mMinQstepCmplxKPUpdt++;
            par->mMinQstepRateEP = mfx::clamp(par->mMinQstepRateEP + mfx::clamp(0.01 * (log(QstepScaleReal) - log(QstepScaleComputed))*log(R), -0.1, 0.2), 0.125, 1.0);

            // Sanity Check / Force
            if (qp < 50) {
                mfxF64 rateQstepNew = pow(R, par->mMinQstepRateEP);
                mfxF64 QstepScaleUpdtComputed = rateQstepNew * par->mMinQstepCmplxKP;
                mfxI32 qp_now = (mfxI32)(6.0*log(QstepScaleUpdtComputed) / log(2.0) + 12.0);
                if (qp < qp_now -1) {
                    qp_now = qp + 2;
                    QstepScaleUpdtComputed = pow(2.0, ((mfxF64)qp_now - 12.0) / 6.0);
                    par->mMinQstepCmplxKP = QstepScaleUpdtComputed / rateQstepNew;
                    par->mMinQstepCmplxKPUpdtErr = 0.16;
                }
            }
        }
    } else if (type == MFX_FRAMETYPE_P) {
        if (ctx.LastIQpSetOrder < ctx.encOrder) {
            mfxI32 rawSize = par->mRawFrameSizeInPixs;
            mfxF64 R = (mfxF64)rawSize / (mfxF64)bits;
            mfxF64 QstepScaleComputed = pow(R, par->mMinQstepRateEP) * par->mMinQstepCmplxKP;
            mfxF64 QstepScaleReal = pow(2.0, ((mfxF64)qp - 12.0) / 6.0);
            mfxF64 dS = log(QstepScaleReal) - log(QstepScaleComputed);
            par->mMinQstepCmplxKPUpdtErr = std::max<mfxF64>((par->mMinQstepCmplxKPUpdtErr + abs(dS)) / 2, abs(dS));
            mfxF64 upDlt = mfx::clamp(1.3042 * pow(R, -0.922), 0.025, 0.5);
            if (shstrt || par->mMinQstepCmplxKPUpdt <= 2 || par->mMinQstepCmplxKPUpdtErr > 0.69) upDlt = 0.5;
            else if (brcSts != MFX_BRC_OK || par->mMinQstepCmplxKPUpdtErr > 0.41) upDlt = std::max(0.125, upDlt);
            dS = mfx::clamp(dS, -0.5, 1.0);
            par->mMinQstepCmplxKP = par->mMinQstepCmplxKP*(1.0 + upDlt*dS);
            par->mMinQstepCmplxKPUpdt++;
            par->mMinQstepRateEP = mfx::clamp(par->mMinQstepRateEP + mfx::clamp(0.01 * (log(QstepScaleReal) - log(QstepScaleComputed))*log(R), -0.1, 0.2), 0.125, 1.0);
        }
    }
}

mfxI32 BRC_EncToolBase::GetCurQP(mfxU32 type, mfxI32 layer, mfxU16 isRef, mfxU16 qpMod, mfxI8 qpExp, mfxI32 qpDeltaP) const
{
    mfxI32 qp = 0;
    if (type == MFX_FRAMETYPE_IDR)
    {
        qp = m_ctx.QuantIDR;
        qp = mfx::clamp(qp, m_par.quantMinI, m_par.quantMaxI);
    }
    else if (type == MFX_FRAMETYPE_I)
    {
        qp = m_ctx.QuantI;
        qp = mfx::clamp(qp, m_par.quantMinI, m_par.quantMaxI);
    }
    else if (type == MFX_FRAMETYPE_P)
    {
        qp = m_ctx.QuantP + layer;
        qp += qpDeltaP;
        qp = mfx::clamp(qp, m_par.quantMinP, m_par.quantMaxP);
    }
    else
    {
        qp = m_ctx.QuantB;
        qp += GetOffsetAPQ(layer, isRef, qpMod, qpExp, m_par.codecId);
        qp = mfx::clamp(qp, m_par.quantMinB, m_par.quantMaxB);
    }
    //printf("GetCurQP IDR %d I %d P %d B %d, min %d max %d type %d \n", m_ctx.QuantIDR, m_ctx.QuantI, m_ctx.QuantP, m_ctx.QuantB, m_par.quantMinI, m_par.quantMaxI, type);

    return qp;
}

mfxF64 BRC_EncToolBase::ResetQuantAb(mfxI32 qp, mfxU32 type, mfxI32 layer, mfxU16 isRef, mfxF64 fAbLong, mfxU32 eo, bool bIdr, mfxU16 qpMod, mfxI8 qpExp, mfxI32 qpDeltaP, bool bNoNewQp) const
{
    mfxI32 seqQP_new = GetSeqQP(qp, type, layer, isRef, qpMod, qpExp, qpDeltaP);
    mfxF64 dQuantAb_new = 1.0 / seqQP_new;
    mfxF64 bAbPreriod = m_par.bAbPeriod;

    mfxF64 totDev = m_ctx.totalDeviation;

    mfxF64 HRDDevFactor = 0.0;
    mfxF64 maxFrameSizeHrd = 0.0;
    mfxF64 HRDDev = 0.0;
    if (m_par.HRDConformance != MFX_BRC_NO_HRD)
    {
        HRDDevFactor = m_hrdSpec->GetBufferDeviationFactor(eo);
        HRDDev = m_hrdSpec->GetBufferDeviation(eo);
        maxFrameSizeHrd = m_hrdSpec->GetMaxFrameSizeInBits(eo, bIdr);
    }

    mfxF64 lf = 1.0 / pow(m_par.inputBitsPerFrame / fAbLong, 1.0 + HRDDevFactor);

    if (m_par.HRDConformance != MFX_BRC_NO_HRD && totDev > 0)
    {
        if (m_par.rateControlMethod == MFX_RATECONTROL_VBR)
        {
            totDev = std::max(totDev, HRDDev);
        }
        bAbPreriod = (mfxF64)(m_par.bPyr ? 4 : 3)*(mfxF64)maxFrameSizeHrd / m_par.inputBitsPerFrame*GetAbPeriodCoeff(m_ctx.encOrder - m_ctx.LastIDREncOrder, m_par.gopPicSize, m_ctx.LastIDRSceneChange);
        bAbPreriod = mfx::clamp(bAbPreriod, m_par.bAbPeriod / 10, m_par.bAbPeriod);
    }

    if (!bNoNewQp) 
    {
        mfxI32 quant_new = GetNewQPTotal(totDev / bAbPreriod / m_par.inputBitsPerFrame, 
                                            dQuantAb_new, m_ctx.QuantMin, m_ctx.QuantMax, 
                                            seqQP_new, m_par.bPyr && m_par.bRec, false);
        seqQP_new += (seqQP_new - quant_new);
    }
    mfxF64 dQuantAb =  lf * (1.0 / seqQP_new);
    return dQuantAb;
}

mfxI32 BRC_EncToolBase::GetSeqQP(mfxI32 qp, mfxU32 type, mfxI32 layer, mfxU16 isRef, mfxU16 qpMod, mfxI8 qpExp, mfxI32 qpDeltaP) const
{
    mfxI32 pqp = 0;
    if (type == MFX_FRAMETYPE_IDR) {
        pqp = qp + m_par.iDQp + 1;
    } else if (type == MFX_FRAMETYPE_I) {
        pqp = qp + 1;
    } else if (type == MFX_FRAMETYPE_P) {
        pqp = qp - layer - qpDeltaP;
    } else {
        qp -= GetOffsetAPQ(layer, isRef, qpMod, qpExp, m_par.codecId);
        pqp = qp - 1;
    }
    pqp = mfx::clamp(pqp, m_par.quantMinP, m_par.quantMaxP);

    return pqp;
}

mfxI32 BRC_EncToolBase::GetPicQP(mfxI32 pqp, mfxU32 type, mfxI32 layer, mfxU16 isRef, mfxU16 qpMod, mfxI8 qpExp, mfxI32 qpDeltaP) const
{
    mfxI32 qp = 0;

    if (type == MFX_FRAMETYPE_IDR)
    {
        qp = pqp - 1 - m_par.iDQp;
        qp = mfx::clamp(qp, m_par.quantMinI, m_par.quantMaxI);
    }
    else if (type == MFX_FRAMETYPE_I)
    {
        qp = pqp - 1;
        qp = mfx::clamp(qp, m_par.quantMinI, m_par.quantMaxI);
    }
    else if (type == MFX_FRAMETYPE_P)
    {
        qp =pqp + layer;
        qp += qpDeltaP;
        qp = mfx::clamp(qp, m_par.quantMinP, m_par.quantMaxP);
    }
    else
    {
        qp = pqp + 1;
        qp += GetOffsetAPQ(layer, isRef, qpMod, qpExp, m_par.codecId);
        qp = mfx::clamp(qp, m_par.quantMinB, m_par.quantMaxB);
    }

    return qp;
}


mfxStatus BRC_EncToolBase::Init(mfxEncToolsCtrl const & ctrl, bool bMBBRC, bool bALTR)
{
    MFX_CHECK(!m_bInit, MFX_ERR_UNDEFINED_BEHAVIOR);
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_par.Init(ctrl, bMBBRC, isFieldMode(ctrl), bALTR);
    MFX_CHECK_STS(sts);

    if (m_par.HRDConformance != MFX_BRC_NO_HRD)
    {
        if (m_par.codecId == MFX_CODEC_AVC)
            m_hrdSpec.reset(new H264_HRD());
        else
            m_hrdSpec.reset(new HEVC_HRD());
        m_hrdSpec->Init(m_par);

    }
    m_ctx = {};

    m_ctx.fAbLong = m_par.inputBitsPerFrame;
    m_ctx.fAbShort = m_par.inputBitsPerFrame;
    m_ctx.fAbLA = m_par.inputBitsPerFrame;

    mfxI32 rawSize = GetRawFrameSize(m_par.width * m_par.height, m_par.chromaFormat, m_par.quantOffset ? m_par.bitDepthLuma : 8);
    mfxI32 qp = GetNewQP(rawSize, m_par.inputBitsPerFrame, m_par.quantMinI, m_par.quantMaxI, 1, m_par.quantOffset, 0.5, false, false);

    UpdateQPParams(qp, MFX_FRAMETYPE_IDR, m_ctx, 0, m_par.quantMinI, m_par.quantMaxI, 0, m_par.iDQp, MFX_FRAMETYPE_REF, 0, 0, 0, m_par.codecId);

    m_ctx.dQuantAb = qp > 0 ? 1.0 / qp : 1.0; //kw

    if (m_par.WinBRCSize)
    {
        m_avg.reset(new AVGBitrate(m_par.WinBRCSize, (mfxU32)(m_par.WinBRCMaxAvgKbps*1000.0 / m_par.frameRate), (mfxU32)m_par.inputBitsPerFrame));
        MFX_CHECK_NULL_PTR1(m_avg.get());
    }

    m_bInit = true;
    return sts;
}

mfxStatus BRC_EncToolBase::Reset(mfxEncToolsCtrl const & ctrl, bool bMBBRC, bool bALTR)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);

    mfxExtEncoderResetOption  * pRO = (mfxExtEncoderResetOption *)Et_GetExtBuffer(ctrl.ExtParam, ctrl.NumExtParam, MFX_EXTBUFF_ENCODER_RESET_OPTION);
    if (pRO && pRO->StartNewSequence == MFX_CODINGOPTION_ON)
    {
        Close();
        sts = Init(ctrl, bMBBRC, bALTR);
    }
    else
    {
        bool brcReset = false;
        bool slidingWindowReset = false;

        sts = m_par.GetBRCResetType(ctrl, false, bMBBRC, brcReset, slidingWindowReset, bALTR);
        MFX_CHECK_STS(sts);

        if (brcReset)
        {
            sts = m_par.Init(ctrl, bMBBRC, isFieldMode(ctrl), bALTR);
            MFX_CHECK_STS(sts);

            m_ctx.Quant = (mfxI32)(1. / m_ctx.dQuantAb * pow(m_ctx.fAbLong / m_par.inputBitsPerFrame, 0.32) + 0.5);
            m_ctx.Quant = mfx::clamp(m_ctx.Quant, m_par.quantMinI, m_par.quantMaxI);

            UpdateQPParams(m_ctx.Quant, MFX_FRAMETYPE_IDR, m_ctx, 0, m_par.quantMinI, m_par.quantMaxI, 0, m_par.iDQp, MFX_FRAMETYPE_REF, 0, 0, 0, m_par.codecId);

            m_ctx.dQuantAb = 1. / m_ctx.Quant;
            m_ctx.fAbLong = m_par.inputBitsPerFrame;
            m_ctx.fAbShort = m_par.inputBitsPerFrame;
            m_ctx.fAbLA = m_par.inputBitsPerFrame;

            if (slidingWindowReset)
            {
                m_avg.reset(new AVGBitrate(m_par.WinBRCSize, (mfxU32)(m_par.WinBRCMaxAvgKbps*1000.0 / m_par.frameRate), (mfxU32)m_par.inputBitsPerFrame));
                MFX_CHECK_NULL_PTR1(m_avg.get());
            }
        }
    }
    return sts;
}



mfxStatus BRC_EncToolBase::UpdateFrame(mfxU32 dispOrder, mfxEncToolsBRCStatus *pFrameSts)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(pFrameSts);

    auto frameStructItr = std::find_if(m_FrameStruct.begin(), m_FrameStruct.end(), CompareByDisplayOrder(dispOrder));
    if (frameStructItr == m_FrameStruct.end())
        return MFX_ERR_UNDEFINED_BEHAVIOR; // BRC hasn't processed the frame

    BRC_FrameStruct frameStruct = *frameStructItr;

    mfxI32 bitsEncoded = frameStruct.frameSize * 8;
    mfxI32 qpY = frameStruct.qp + m_par.quantOffset;
    if (frameStructItr->QpMapNZ)
    {
        qpY = mfx::clamp((mfxI32)qpY - frameStructItr->QpMapBias, 1, 51);
    }
    mfxI32 layer = frameStruct.pyrLayer;
    mfxU16 picType = GetFrameType(frameStruct.frameType, frameStruct.pyrLayer, m_par.gopRefDist, m_par.codecId);
    mfxU16 isRef = frameStruct.frameType & MFX_FRAMETYPE_REF;
    mfxU16 isIntra = frameStruct.frameType & (MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I);
    bool bIdr = (picType ==  MFX_FRAMETYPE_IDR);
    mfxF64 qstep = QP2Qstep(qpY, m_par.quantOffset);
    mfxU32 ParFrameCmplx = 0; 
    mfxU16 ParSceneChange = (ParFrameCmplx || frameStruct.LaAvgEncodedSize) ? frameStruct.sceneChange : 0;

    mfxU16 ParQpModulation = frameStruct.qpModulation;
    mfxU16 miniGoPSize = frameStruct.miniGopSize == 0 ? m_par.gopRefDist : frameStruct.miniGopSize;
    if (ParQpModulation == MFX_QP_MODULATION_NOT_DEFINED
        && miniGoPSize >= 8 && m_par.bPyr
        && (m_par.codecId == MFX_CODEC_HEVC || m_par.codecId == MFX_CODEC_AV1)) {
        ParQpModulation = BRC_QP_MODULATION_GOP8_FIXED;
    }

    mfxI32 ParQpDeltaP = 0;
    if(picType == MFX_FRAMETYPE_P) 
        ParQpDeltaP = (frameStruct.qpDelta == MFX_QP_UNDEFINED) ? 0 : std::max(-4, std::min(2, (mfxI32)frameStruct.qpDelta));
    mfxU16 ParLongTerm = frameStruct.longTerm;

    mfxF64 fAbLong = m_ctx.fAbLong + (bitsEncoded - m_ctx.fAbLong) / m_par.fAbPeriodLong;
    mfxF64 fAbShort = m_ctx.fAbShort + (bitsEncoded - m_ctx.fAbShort) / m_par.fAbPeriodShort;
    mfxF64 fAbLA = m_ctx.fAbLA + (bitsEncoded - m_ctx.fAbLA) / m_par.fAbPeriodLA;
    mfxF64 eRate = bitsEncoded * sqrt(qstep);

    mfxF64 e2pe = 0;
    bool bMaxFrameSizeMode = m_par.maxFrameSizeInBits != 0 &&
        m_par.maxFrameSizeInBits < m_par.inputBitsPerFrame * 2 &&
        m_ctx.totalDeviation < (-1)*m_par.inputBitsPerFrame*m_par.frameRate;

    if (isIntra) {
        e2pe = (m_ctx.eRateSH == 0) ? (BRC_SCENE_CHANGE_RATIO2 + 1) : eRate / m_ctx.eRateSH;
        if (ParSceneChange && e2pe <= BRC_SCENE_CHANGE_RATIO2 && m_ctx.eRate)
            e2pe = eRate / m_ctx.eRate;
    }
    else {
        e2pe = (m_ctx.eRate == 0) ? (BRC_SCENE_CHANGE_RATIO2 + 1) : eRate / m_ctx.eRate;
    }
    mfxU32 frameSizeLim = 0xfffffff; // sliding window limitation or external frame size limitation
    // HEVC / AV1 do not have external re-encode loop to prevent bitstream copy size violation.
    // For NO_HRD this will ensure encoded picture fits in bitstream memory.
    // Bitstsream size is bufferSize if specified.
    mfxU32 bitstreamSizeLim = frameSizeLim;
    if(m_par.codecId != MFX_CODEC_AVC && m_par.HRDConformance == MFX_BRC_NO_HRD && m_par.bufferSizeInBytes)
        bitstreamSizeLim = m_par.bufferSizeInBytes * 8;

    bool  bSHStart = false;
    bool  bNeedUpdateQP = false;
    mfxU16 &brcSts = pFrameSts->FrameStatus.BRCStatus;

    brcSts = MFX_BRC_OK;

    if (m_par.bRec && m_ctx.bToRecode && (m_ctx.encOrder != frameStruct.encOrder)) // || frameStruct.numRecode == 0))
    {
        //printf("++++++++++++++++++++++++++++++++++\n");
        // Frame must be recoded, but encoder calls BR for another frame
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if (frameStruct.numRecode == 0)
    {
        // Set context for new frame
        if (isIntra) {
            m_ctx.LastIEncOrder = frameStruct.encOrder;
            if (bIdr)
            {
                m_ctx.LastIDREncOrder = frameStruct.encOrder;
                m_ctx.LastIDRSceneChange = ParSceneChange;
            }
        }
        m_ctx.encOrder = frameStruct.encOrder;
        m_ctx.poc = frameStruct.dispOrder;
        m_ctx.bToRecode = 0;
        m_ctx.bPanic = 0;

        if (isIntra)
        {
            m_ctx.QuantMin = m_par.quantMinI;
            m_ctx.QuantMax = m_par.quantMaxI;
        }
        else if (picType == MFX_FRAMETYPE_P)
        {
            m_ctx.QuantMin = m_par.quantMinP;
            m_ctx.QuantMax = m_par.quantMaxP;
        }
        else
        {
            m_ctx.QuantMin = m_par.quantMinB;
            m_ctx.QuantMax = m_par.quantMaxB;
        }
        m_ctx.Quant = qpY;

        if (m_ctx.SceneChange && (m_ctx.poc > m_ctx.SChPoc + 1 || m_ctx.poc == 0))
            m_ctx.SceneChange &= ~16;

        bNeedUpdateQP = true;

        if (m_par.HRDConformance != MFX_BRC_NO_HRD)
        {
            m_hrdSpec->ResetQuant();
        }

        //printf("m_ctx.SceneChange %d, m_ctx.poc %d, m_ctx.SChPoc, m_ctx.poc %d \n", m_ctx.SceneChange, m_ctx.poc, m_ctx.SChPoc, m_ctx.poc);
    }
    if (m_par.HRDConformance != MFX_BRC_NO_HRD)
    {
        brcSts = CheckHrdAndUpdateQP(*m_hrdSpec.get(), bitsEncoded, frameStruct.encOrder, bIdr, qpY);

        MFX_CHECK(brcSts == MFX_BRC_OK || (!m_ctx.bPanic), MFX_ERR_NOT_ENOUGH_BUFFER);
        if (brcSts == MFX_BRC_OK && !m_ctx.bPanic)
            bNeedUpdateQP = true;
        pFrameSts->FrameStatus.MinFrameSize = (m_hrdSpec->GetMinFrameSizeInBits(frameStruct.encOrder,bIdr) + 7) >> 3;
        //printf("%d: poc %d, size %d QP %d (%d %d), HRD sts %d, maxFrameSize %d, type %d \n",frame_par->EncodedOrder, frame_par->DisplayOrder, bitsEncoded, m_ctx.Quant, m_ctx.QuantMin, m_ctx.QuantMax, brcSts,  m_hrd.GetMaxFrameSize(), frame_par->FrameType);
    }
    bool bCalcSetI = (isIntra && (ParFrameCmplx > 0 || frameStruct.LaAvgEncodedSize) && frameStruct.encOrder == m_ctx.LastIEncOrder         // We could set Qp
                    && ((ParSceneChange > 0 || frameStruct.encOrder == 0) && m_ctx.LastIQpSet == m_ctx.LastIQpMin));                        // We did set Qp and/or was SceneChange
    if ((e2pe > BRC_SCENE_CHANGE_RATIO2  && bitsEncoded > 4 * m_par.inputBitsPerFrame) ||
        bCalcSetI
        )
    {
        // scene change, resetting BRC statistics
        m_ctx.fAbLong = m_par.inputBitsPerFrame;
        m_ctx.fAbShort = m_par.inputBitsPerFrame;
        fAbLong = m_ctx.fAbLong + (bitsEncoded - m_ctx.fAbLong) / m_par.fAbPeriodLong;
        fAbShort = m_ctx.fAbShort + (bitsEncoded - m_ctx.fAbShort) / m_par.fAbPeriodShort;
        m_ctx.SceneChange |= 1;
        if (picType != MFX_FRAMETYPE_B)
        {
            bSHStart = true;
            bool bNoNewQp = false;
            m_ctx.dQuantAb = ResetQuantAb(qpY, picType, layer, isRef, fAbLong, frameStruct.encOrder, bIdr, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, bNoNewQp);
            m_ctx.SceneChange |= 16;
            m_ctx.eRateSH = eRate;
            m_ctx.SChPoc = frameStruct.dispOrder;
            //printf("!!!!!!!!!!!!!!!!!!!!! %d m_ctx.SceneChange %d, order %d\n", frameStruct.encOrder, m_ctx.SceneChange, frameStruct.dispOrder);
            if (picType == MFX_FRAMETYPE_P && bitsEncoded > 4 * m_par.inputBitsPerFrame) ResetMinQForMaxFrameSize(&m_par, picType);
        }
    }
    else if (!isIntra && !frameStruct.numRecode && frameStruct.LaAvgEncodedSize && m_ctx.la.LastLaQpUpdateEncOrder == frameStruct.encOrder)
    {
        m_ctx.dQuantAb = 1.0 / ((1.0 / m_ctx.dQuantAb) + (qpY - frameStruct.origSeqQp) * 0.5);
    }

    if (m_avg.get())
    {
        frameSizeLim = std::min(frameSizeLim, m_avg->GetMaxFrameSize(m_ctx.bPanic, bSHStart || isIntra, frameStruct.numRecode));
    }
    if (m_par.maxFrameSizeInBits)
    {
        frameSizeLim = std::min(frameSizeLim, m_par.maxFrameSizeInBits);
    }
    //printf("frameSizeLim %d (%d)\n", frameSizeLim, bitsEncoded);
    if (!m_ctx.bPanic)
        UpdateMinQForMaxFrameSize(&m_par, bitsEncoded, qpY, m_ctx, picType, bSHStart, brcSts);

    if (frameStruct.numRecode < 2)
    // Check other conditions for recoding (update qp if it is needed)
    {
        mfxF64 targetFrameSize = std::max<mfxF64>(m_par.inputBitsPerFrame, fAbLong);
        mfxF64 dqf = (m_par.bFieldMode) ? 1.0 : DQF(picType, m_par.iDQp, ((picType == MFX_FRAMETYPE_IDR) ? m_par.mIntraBoost : false), (ParSceneChange || m_ctx.encOrder == 0));
        mfxF64 maxFrameSizeByRatio = dqf * FRM_RATIO(picType, m_ctx.encOrder, bSHStart, m_par.bPyr) * targetFrameSize;
        // Aref
        if (picType == MFX_FRAMETYPE_P && ParLongTerm && frameStruct.qpDelta<0) maxFrameSizeByRatio *= 1.58;
        if (frameStruct.QpMapNZ) maxFrameSizeByRatio *= 1.16; // some boost expected
        if (m_par.rateControlMethod == MFX_RATECONTROL_CBR && m_par.HRDConformance != MFX_BRC_NO_HRD) {
            mfxF64 bufferDeviation = m_hrdSpec->GetBufferDeviation(frameStruct.encOrder);
            mfxF64 dev = -1.0*maxFrameSizeByRatio - bufferDeviation;
            if (dev > 0) maxFrameSizeByRatio += (std::min)(maxFrameSizeByRatio, (dev / (isIntra ? 2.0 : 4.0)));
        }
        else if(isIntra && m_par.bRec) {
            mfxF64 devI = -m_ctx.totalDeviation;
            if (devI > 0)
                maxFrameSizeByRatio += std::min(maxFrameSizeByRatio, devI);
            else
                maxFrameSizeByRatio -= std::min(maxFrameSizeByRatio / 2.0, -devI);
        }
        // Upper limit of Max Frame size factor based on various factors
        mfxF64 maxFactor = 1.66 * 8 * 1.58 * 1.16 * 2.0; // Max of (Boost * FrameRatio * LTR factor * QpMap factor * Deviation factor)
        mfxI32 quantMax = m_ctx.QuantMax;
        mfxI32 quantMin = m_ctx.QuantMin;
        mfxI32 quant = qpY;

        mfxF64 maxFrameSize = std::min<mfxF64>(maxFrameSizeByRatio, frameSizeLim);

        if (m_par.HRDConformance != MFX_BRC_NO_HRD)
        {
            mfxF64 maxFrameSizeHrd = m_hrdSpec->GetMaxFrameSizeInBits(frameStruct.encOrder,bIdr);
            mfxF64 bufOccupy = LTR_BUF(picType, m_par.iDQp, ((picType == MFX_FRAMETYPE_IDR) ? m_par.mIntraBoost : false), ParSceneChange, bSHStart, m_par.mVeryLowDelay);
            mfxF64 maxFrameSizeHRDBalanced = bufOccupy / 9.* maxFrameSizeHrd + (9.0 - bufOccupy) / 9.*targetFrameSize;
            if (m_ctx.encOrder == 0)
            {
                // modify buf limits for VCM like encode for init only
                mfxF64 maxFrameSizeGood = 6.5 * m_par.inputBitsPerFrame;
                mfxF64 maxFrameSizeHighMark = 8.0 / 9.* maxFrameSizeHrd + 1.0 / 9.*m_par.inputBitsPerFrame;
                mfxF64 maxFrameSizeInit = mfx::clamp(maxFrameSizeGood, maxFrameSizeHRDBalanced, maxFrameSizeHighMark);
                maxFrameSize = std::min(maxFrameSize, maxFrameSizeInit);
            }
            else
                maxFrameSize = std::min(maxFrameSize, maxFrameSizeHRDBalanced);

            quantMax = std::min(m_hrdSpec->GetMaxQuant(), quantMax);
            quantMin = std::max(m_hrdSpec->GetMinQuant(), quantMin);

        }
        maxFrameSize = std::max(maxFrameSize, targetFrameSize);
        bool bMaxFrameSizeRecode = (!ParSceneChange || bitsEncoded > (mfxI32)(maxFactor*targetFrameSize) 
                                    || m_par.HRDConformance != MFX_BRC_NO_HRD 
                                    || (m_par.maxFrameSizeInBits && bitsEncoded>(mfxI32)m_par.maxFrameSizeInBits));
        if (bitsEncoded > maxFrameSize && quant < quantMax && bMaxFrameSizeRecode)
        {
            mfxI32 quant_new = GetNewQP(bitsEncoded, (mfxU32)maxFrameSize, quantMin, quantMax, quant, m_par.quantOffset, 1);
            if (quant_new > quant)
            {
                bNeedUpdateQP = false;
                //printf("    recode 1-0: %d:  k %5f bitsEncoded %d maxFrameSize %d (%d, %d), targetSize %d, fAbLong %f, inputBitsPerFrame %f, qp %d new %d, layer %d\n", 
                //    frameStruct.encOrder, bitsEncoded/maxFrameSize, (int)bitsEncoded, (int)maxFrameSize,(int) maxFrameSizeByRatio, (int) frameSizeLim, (int)targetFrameSize, fAbLong, m_par.inputBitsPerFrame, quant, quant_new, layer);
                if (quant_new > GetCurQP(picType, layer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP))
                {
                    UpdateQPParams(bMaxFrameSizeMode ? quant_new - 1 : quant_new, picType, m_ctx, 0, quantMin, quantMax, layer, m_par.iDQp, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, m_par.codecId);
                    fAbLong = m_ctx.fAbLong = m_par.inputBitsPerFrame;
                    fAbShort = m_ctx.fAbShort = m_par.inputBitsPerFrame;
                    m_ctx.dQuantAb = ResetQuantAb(quant_new, picType, layer, isRef, fAbLong, frameStruct.encOrder, bIdr, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, false);
                }

                if (m_par.bRec)
                {
                    SetRecodeParams(MFX_BRC_BIG_FRAME, quant, quant_new, quantMin, quantMax, m_ctx, &pFrameSts->FrameStatus);
                    frameStruct.numRecode++;
                    return sts;
                }
            } //(quant_new > quant)
        } //bitsEncoded >  maxFrameSize

        mfxF64 lFR = std::min(m_par.gopPicSize - 1, 4);
        mfxF64 lowFrameSizeI = std::min(maxFrameSize, lFR *(mfxF64)m_par.inputBitsPerFrame);
        // Did we set the qp?
        if (isIntra && (ParFrameCmplx > 0 || frameStruct.LaAvgEncodedSize)                              // We could set Qp
            && frameStruct.encOrder == m_ctx.LastIEncOrder && m_ctx.LastIQpSet == m_ctx.LastIQpMin      // We did set Qp
            && frameStruct.numRecode == 0 && bitsEncoded <  (lowFrameSizeI / 2.0) && quant > quantMin)  // We can & should recode
        {
            // too small; do something
            mfxI32 quant_new = GetNewQP(bitsEncoded, (mfxU32)lowFrameSizeI, quantMin, quantMax, quant, m_par.quantOffset, 0.78, false, true);
            if (quant_new < quant)
            {
                bNeedUpdateQP = false;
                if (quant_new < GetCurQP(picType, layer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP))
                {
                    UpdateQPParams(bMaxFrameSizeMode ? quant_new - 1 : quant_new, picType, m_ctx, 0, quantMin, quantMax, layer, m_par.iDQp, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, m_par.codecId);
                    fAbLong = m_ctx.fAbLong = m_par.inputBitsPerFrame;
                    fAbShort = m_ctx.fAbShort = m_par.inputBitsPerFrame;
                    m_ctx.dQuantAb = ResetQuantAb(quant_new, picType, layer, isRef, fAbLong, frameStruct.encOrder, bIdr, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, false);
                }

                if (m_par.bRec)
                {
                    SetRecodeParams(MFX_BRC_SMALL_FRAME, quant, quant_new, quantMin, quantMax, m_ctx, &pFrameSts->FrameStatus);
                    frameStruct.numRecode++;
                    return sts;
                }
            } //(quant_new < quant)
        }

        if (bitsEncoded > maxFrameSize && quant == quantMax &&
            !isIntra && m_par.bPanic &&
            (!m_ctx.bPanic) && isFrameBeforeIntra(m_ctx.encOrder, m_ctx.LastIEncOrder, m_par.gopPicSize, m_par.gopRefDist))
        {
            //skip frames before intra
            SetRecodeParams(MFX_BRC_PANIC_BIG_FRAME, quant, quant, quantMin, quantMax, m_ctx, &pFrameSts->FrameStatus);
            frameStruct.numRecode++;
            return sts;
        }
        if (m_par.HRDConformance != MFX_BRC_NO_HRD && frameStruct.numRecode == 0 && (quant < quantMax))
        {
            mfxF64 maxFrameSizeHrd = m_hrdSpec->GetMaxFrameSizeInBits(frameStruct.encOrder, bIdr);
            mfxF64 FAMax = 1./9. * maxFrameSizeHrd + 8./9. * fAbLong;

            if (fAbShort > FAMax)
            {
                mfxI32 quant_new = GetNewQP(fAbShort, FAMax, quantMin, quantMax, quant, m_par.quantOffset, 0.5);
                //printf("============== recode 2-0: %d:  FAMax %f, fAbShort %f, quant_new %d\n",frameStruct.encOrder, FAMax, fAbShort, quant_new);

                if (quant_new > quant)
                {
                    bNeedUpdateQP = false;
                    if (quant_new > GetCurQP(picType, layer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP))
                    {
                        UpdateQPParams(quant_new, picType, m_ctx, 0, quantMin, quantMax, layer, m_par.iDQp, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, m_par.codecId);
                        fAbLong = m_ctx.fAbLong = m_par.inputBitsPerFrame;
                        fAbShort = m_ctx.fAbShort = m_par.inputBitsPerFrame;
                        m_ctx.dQuantAb = ResetQuantAb(quant_new, picType, layer, isRef, fAbLong, frameStruct.encOrder, bIdr, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, false);
                    }
                    if (m_par.bRec)
                    {
                        SetRecodeParams(MFX_BRC_BIG_FRAME, quant, quant_new, quantMin, quantMax, m_ctx, &pFrameSts->FrameStatus);
                        frameStruct.numRecode++;
                        return sts;
                    }
                } //quant_new > quant
            }
        }//m_par.HRDConformance
    }
    if ((((m_par.HRDConformance != MFX_BRC_NO_HRD && brcSts != MFX_BRC_OK) || (bitsEncoded > (mfxI32)frameSizeLim)) && m_par.bRec)
        || (bitsEncoded > (mfxI32)bitstreamSizeLim))
    {
        mfxI32 quant = qpY;
        mfxI32 quant_new = quant;
        if (bitsEncoded > (mfxI32)frameSizeLim)
        {
            brcSts = MFX_BRC_BIG_FRAME;
            quant_new = GetNewQP(bitsEncoded, frameSizeLim, m_ctx.QuantMin, m_ctx.QuantMax, quant, m_par.quantOffset, 1, true);
        }
        else if (bitsEncoded > (mfxI32)bitstreamSizeLim)
        {
            brcSts = MFX_BRC_BIG_FRAME;
            quant_new = GetNewQP(bitsEncoded, bitstreamSizeLim, m_ctx.QuantMin, m_ctx.QuantMax, quant, m_par.quantOffset, 1, true);
        }
        else if (brcSts == MFX_BRC_BIG_FRAME || brcSts == MFX_BRC_SMALL_FRAME)
        {
            mfxF64 targetSize = GetFrameTargetSize(brcSts,
                m_hrdSpec->GetMinFrameSizeInBits(frameStruct.encOrder, bIdr),
                m_hrdSpec->GetMaxFrameSizeInBits(frameStruct.encOrder, bIdr));
            quant_new = GetNewQP(bitsEncoded, targetSize, m_ctx.QuantMin , m_ctx.QuantMax,quant,m_par.quantOffset, 1, true);
        }
        if (quant_new != quant)
        {
            if (brcSts == MFX_BRC_SMALL_FRAME)
            {
                quant_new = std::max(quant_new, quant - 2);
                brcSts = MFX_BRC_PANIC_SMALL_FRAME;
            }
            // Idea is to check a sign mismatch, 'true' if both are negative or positive
            if ((quant_new - qpY) * (quant_new - GetCurQP(picType, layer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP)) > 0)
            {
                UpdateQPParams(quant_new, picType, m_ctx, 0, m_ctx.QuantMin, m_ctx.QuantMax, layer, m_par.iDQp, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, m_par.codecId);
                m_ctx.dQuantAb = ResetQuantAb(quant_new, picType, layer, isRef, fAbLong, frameStruct.encOrder, bIdr, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, false);
            }
            bNeedUpdateQP = false;
        }
        SetRecodeParams(brcSts, quant, quant_new, m_ctx.QuantMin, m_ctx.QuantMax, m_ctx, &pFrameSts->FrameStatus);
        //printf("===================== recode 1-0: HRD recode: quant_new %d\n", quant_new);
    }
    else
    {
        // no recoding is needed. Save context params

        mfxF64 k = 1. / GetSeqQP(qpY, picType, layer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP);
        mfxF64 dqAbPeriod = m_par.dqAbPeriod;
        if (m_ctx.bToRecode)
            dqAbPeriod = (k < m_ctx.dQuantAb) ? 16 : 25;

        if (bNeedUpdateQP)
        {
            m_ctx.dQuantAb += (k - m_ctx.dQuantAb) / dqAbPeriod;
            m_ctx.dQuantAb = mfx::clamp(m_ctx.dQuantAb, 1. / m_ctx.QuantMax, 1.);

            m_ctx.fAbLong = fAbLong;
            m_ctx.fAbShort = fAbShort;
            m_ctx.fAbLA = fAbLA;
        }

        bool oldScene = false;
        if ((m_ctx.SceneChange & 16) && (m_ctx.poc < m_ctx.SChPoc) && (e2pe < .01) && (mfxF64)bitsEncoded < 1.5*fAbLong)
            oldScene = true;
        //printf("-- m_ctx.eRate %f,  eRate %f, e2pe %f\n", m_ctx.eRate,  eRate, e2pe );

        if (!m_ctx.bPanic && frameStruct.numRecode < 100)
        {
            if (picType != MFX_FRAMETYPE_B)
            {
                m_ctx.LastNonBFrameSize = bitsEncoded;
                if (isIntra)
                {
                    m_ctx.eRateSH = eRate;
                    if (ParSceneChange)
                        m_ctx.eRate = m_par.inputBitsPerFrame * sqrt(QP2Qstep(GetCurQP(MFX_FRAMETYPE_P, 0, MFX_FRAMETYPE_REF, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP), m_par.quantOffset));
                }
                else
                {
                    m_ctx.eRate = eRate;
                    if (m_ctx.eRate > m_ctx.eRateSH) m_ctx.eRateSH = m_ctx.eRate;
                }
            }

            if (isIntra)
            {
                m_ctx.LastIFrameSize = bitsEncoded;
                m_ctx.LastIQpAct = qpY;
            }
        }

        if (m_avg.get())
        {
            m_avg->UpdateSlidingWindow(bitsEncoded, m_ctx.encOrder, m_ctx.bPanic, bSHStart || isIntra, frameStruct.numRecode, qpY);
        }

        m_ctx.totalDeviation += ((mfxF64)bitsEncoded - m_par.inputBitsPerFrame);

        //printf("------------------ %d (%d)) Total deviation %f, old scene %d, bNeedUpdateQP %d, m_ctx.Quant %d, type %d, m_ctx.fAbLong %f m_par.inputBitsPerFrame %f\n", frameStruct.encOrder, frameStruct.dispOrder,m_ctx.totalDeviation, oldScene , bNeedUpdateQP, m_ctx.Quant,picType, m_ctx.fAbLong, m_par.inputBitsPerFrame);
        if (m_par.HRDConformance != MFX_BRC_NO_HRD)
        {
            m_hrdSpec->Update(bitsEncoded, frameStruct.encOrder, bIdr);
        }

        if (!m_ctx.bPanic && (!oldScene) && bNeedUpdateQP)
        {
            mfxI32 quant_new = qpY;

            //Update QP

            mfxF64 totDev = m_ctx.totalDeviation;
            mfxF64 HRDDevFactor = 0.0;
            mfxF64 HRDDev = 0.0;
            mfxF64 maxFrameSizeHrd = 0.0;
            if (m_par.HRDConformance != MFX_BRC_NO_HRD)
            {

                HRDDevFactor = m_hrdSpec->GetBufferDeviationFactor(frameStruct.encOrder);
                HRDDev = m_hrdSpec->GetBufferDeviation(frameStruct.encOrder);
                maxFrameSizeHrd = m_hrdSpec->GetMaxFrameSizeInBits(frameStruct.encOrder, bIdr);
            }

            mfxF64 dequant_new = m_ctx.dQuantAb*pow(m_par.inputBitsPerFrame / m_ctx.fAbLong, 1.0 + HRDDevFactor);

            mfxF64 bAbPreriod = m_par.bAbPeriod;

            if (m_par.HRDConformance != MFX_BRC_NO_HRD)
            {
                if (m_par.rateControlMethod == MFX_RATECONTROL_VBR && m_par.maxbps > m_par.targetbps )
                {
                    totDev = std::max(totDev, HRDDev);
                }
                else
                {
                    totDev = HRDDev;
                }
                if (totDev > 0)
                {
                    bAbPreriod = (mfxF64)(m_par.bPyr ? 4 : 3)*(mfxF64)maxFrameSizeHrd / fAbShort * GetAbPeriodCoeff(m_ctx.encOrder - m_ctx.LastIDREncOrder, m_par.gopPicSize, m_ctx.LastIDRSceneChange);
                    bAbPreriod = mfx::clamp(bAbPreriod, m_par.bAbPeriod / 10, m_par.bAbPeriod);
                }
            }

            if (frameStruct.LaAvgEncodedSize && m_ctx.la.LastLaPBitsAvg[0] && m_par.HRDConformance == MFX_BRC_NO_HRD)
            {
                mfxF64 laRatio = (mfxF64)frameStruct.LaAvgEncodedSize / (mfxF64)m_ctx.la.LastLaPBitsAvg[0];
                mfxF64 fAbFuture = laRatio * fAbLA; // future dev assuming no qp change
                mfxF64 futureDev = (fAbFuture - m_par.inputBitsPerFrame) * m_par.mLaDepth;
                //printf("%d TotalDev %lf FutureDev %lf laRatio %lf LaAvgEncodedSize %d LastLaPBitsAvg %d\n", 
                //    frameStruct.encOrder, totDev, futureDev, laRatio, frameStruct.LaAvgEncodedSize, m_ctx.LastLaPBitsAvg[0]);
                totDev = totDev + futureDev;
            }
            quant_new = GetNewQPTotal(totDev / bAbPreriod / (mfxF64)m_par.inputBitsPerFrame, dequant_new, m_ctx.QuantMin, m_ctx.QuantMax, GetSeqQP(qpY, picType, layer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP), m_par.bPyr && m_par.bRec, bSHStart && m_ctx.bToRecode == 0);
            quant_new = GetPicQP(quant_new, picType, layer, isRef,ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP);
            //printf("    ===%d quant old %d quant_new %d, bitsEncoded %d m_ctx.QuantMin %d m_ctx.QuantMax %d\n", frameStruct.encOrder, m_ctx.Quant, quant_new, bitsEncoded, m_ctx.QuantMin, m_ctx.QuantMax);

            if (bMaxFrameSizeMode)
            {
                mfxF64 targetMax = ((mfxF64)m_par.maxFrameSizeInBits*((bSHStart || isIntra) ? 0.95 : 0.9));
                mfxF64 targetMin = ((mfxF64)m_par.maxFrameSizeInBits*((bSHStart || isIntra) ? 0.9 : 0.8 /*0.75 : 0.5*/));
                mfxI32 QuantNewMin = GetNewQP(bitsEncoded, targetMax, m_ctx.QuantMin, m_ctx.QuantMax, qpY, m_par.quantOffset, 1, false, false);
                mfxI32 QuantNewMax = GetNewQP(bitsEncoded, targetMin, m_ctx.QuantMin, m_ctx.QuantMax, qpY, m_par.quantOffset, 1, false, false);
                mfxI32 quant_corrected = qpY;

                if (quant_corrected < QuantNewMin - 3)
                    quant_corrected += 2;
                if (quant_corrected < QuantNewMin)
                    quant_corrected++;
                else if (quant_corrected > QuantNewMax + 3)
                    quant_corrected -= 2;
                else if (quant_corrected > QuantNewMax)
                    quant_corrected--;

                //printf("   QuantNewMin %d, QuantNewMax %d, m_ctx.Quant %d, new %d (%d)\n", QuantNewMin, QuantNewMax, m_ctx.Quant, quant_corrected, quant_new);

                quant_new = mfx::clamp(quant_corrected, m_ctx.QuantMin, m_ctx.QuantMax);
            }

            if ((quant_new - qpY)* (quant_new - GetCurQP(picType, layer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP)) > 0) // this check is actual for async scheme
            {
                //printf("   +++ Update QP %d: totalDeviation %f, bAbPreriod %f (%f), QP %d (%d %d), qp_new %d (qpY %d), type %d, dequant_new %f (%f) , m_ctx.fAbLong %f, m_par.inputBitsPerFrame %f\n",
                //    frameStruct.encOrder, totDev, bAbPreriod, GetAbPeriodCoeff(m_ctx.encOrder - m_ctx.LastIEncOrder, m_par.gopPicSize, m_ctx.LastIDRSceneChange), m_ctx.Quant, m_ctx.QuantMin, m_ctx.QuantMax,quant_new, qpY, picType, 1.0/dequant_new, 1.0/m_ctx.dQuantAb, m_ctx.fAbLong, m_par.inputBitsPerFrame);
                UpdateQPParams(quant_new, picType, m_ctx, 0, m_ctx.QuantMin, m_ctx.QuantMax, layer, m_par.iDQp, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, m_par.codecId);
            }
        }
        m_ctx.bToRecode = 0;

    }

    return sts;
}


const mfxF64 COEFF_INTRA[2] = { -0.107510, 0.694515 };

static inline void get_coeff_intra(mfxF64 *pCoeff)
{
    pCoeff[0] = COEFF_INTRA[0];
    pCoeff[1] = COEFF_INTRA[1];
}
// RACA = Spatial Complexity measure
// RACA = Row diff Abs + Column diff Abs
#define PWR_RACA 0.751

static mfxF64 getScaledIntraBits(mfxF64 targetBits, mfxF64 rawSize, mfxF64 raca)
{
    if (raca < MIN_RACA)  raca = MIN_RACA;
    mfxF64 SC = pow(raca, PWR_RACA);
    mfxF64 dBits = log((targetBits / rawSize) / SC);

    return dBits;
}

static mfxI32 compute_first_qp_intra(mfxI32 targetBits, mfxI32 rawSize, mfxF64 raca)
{
    mfxF64 dBits = getScaledIntraBits(targetBits, rawSize, raca);
    mfxF64 coeffIntra[2];
    get_coeff_intra(coeffIntra);

    mfxF64 qpNew = (dBits - coeffIntra[1]) / coeffIntra[0];
    mfxI32 qp = (mfxI32)(qpNew + 0.5);
    if (qp < 1) qp = 1;
    return qp;
}

static mfxI32 compute_new_qp_intra(mfxI32 targetBits, mfxI32 rawSize, mfxF64 raca, mfxI32 iBits, mfxF64 icmplx, mfxI32 iqp)
{
    mfxF64 coeffIntra1[2], coeffIntra2[2];

    mfxF64 qp_hat = getScaledIntraBits(iBits, rawSize, icmplx);
    get_coeff_intra(coeffIntra1);
    qp_hat = (qp_hat - coeffIntra1[1]) / coeffIntra1[0];

    mfxF64 dQp = iqp - qp_hat;
    dQp = mfx::clamp(dQp, (-1.0 * MAX_MODEL_ERR), (1.0 * MAX_MODEL_ERR));

    mfxF64 qp_pred = getScaledIntraBits(targetBits, rawSize, raca);
    get_coeff_intra(coeffIntra2);

    qp_pred = (qp_pred - coeffIntra2[1]) / coeffIntra2[0];

    mfxF64 qpNew = qp_pred + dQp;

    mfxI32 qp = (mfxI32)(qpNew + 0.5);
    if (qp < 1) qp = 1;
    return qp;
}
mfxStatus BRC_EncToolBase::GetHRDPos(mfxU32 dispOrder, mfxEncToolsBRCHRDPos *pHRDPos)
{
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(pHRDPos);

    auto frameStructItr = std::find_if(m_FrameStruct.begin(), m_FrameStruct.end(), CompareByDisplayOrder(dispOrder));
    if (frameStructItr == m_FrameStruct.end())
        return MFX_ERR_UNDEFINED_BEHAVIOR; // BRC hasn't processed the frame
    BRC_FrameStruct frameStruct = *frameStructItr;

    if (m_par.HRDConformance != MFX_BRC_NO_HRD)
    {
        pHRDPos->InitialCpbRemovalDelay = m_hrdSpec->GetInitCpbRemovalDelay(frameStruct.encOrder);
        pHRDPos->InitialCpbRemovalDelayOffset = m_hrdSpec->GetInitCpbRemovalDelayOffset(frameStruct.encOrder);
    }
    return MFX_ERR_NONE;
}

mfxStatus BRC_EncToolBase::DiscardFrame(mfxU32 dispOrder){
    auto frameStructItr = std::find_if(m_FrameStruct.begin(), m_FrameStruct.end(), CompareByDisplayOrder(dispOrder));
    if(frameStructItr != m_FrameStruct.end()){
        m_FrameStruct.erase(frameStructItr);
    }
    return MFX_ERR_NONE;
}

static mfxU8 SelectQp(mfxF64 erate[52], mfxF64 budget)
{
    for (mfxU8 qp = 1; qp < 52; qp++)
        if (erate[qp] < budget)
            return (erate[qp - 1] + erate[qp] < 2 * budget) ? qp - 1 : qp;
    return 51;
}

mfxI32 BRC_EncToolBase::GetLaQpEst(mfxU32 LaAvgEncodedSize, mfxF64 inputBitsPerFrame, const BRC_FrameStruct& frameStruct, bool updateState)
{
    mfxU32 laScaledSize = LaAvgEncodedSize << (m_par.mLaScale << 1);
    mfxF32 laQ = ((mfxF32)(m_par.mLaQp) - ((logf((mfxF32)inputBitsPerFrame / (mfxF32)laScaledSize) / logf(2.0f)) * 6.0f));
    mfxI32 laQp = (mfxI32)(laQ); // IPPP

    if (m_par.gopRefDist > 1)
    {
        if (m_par.mLaScale) 
        {
            if (m_par.codecId == MFX_CODEC_AVC) 
            {
                if(m_par.mLaQp != 26 || !frameStruct.frameCmplx || !m_par.lowPower) {
                    laQp = (mfxI32)(0.679f*laQ + 0.465f); // NN V L
                } else {
                   mfxF32 R = (mfxF32) ((1.5 * m_par.width * m_par.height) / (inputBitsPerFrame * m_par.frameRate / 1000.0));
                   mfxF32 SC = (mfxF32) frameStruct.frameCmplx;
                   mfxF32 C  = std::max(SC  / (128*64), 0.03f);
                   mfxF32 B = (mfxF32)laScaledSize / (mfxF32) (1.5 * m_par.width * m_par.height);
                   mfxF32 F  = (mfxF32) (std::max(std::min(m_par.frameRate, 61.0), 23.0));
                   const mfxF32 Alpha=0.094160103f, Beta=0.70605035f, Gamma=0.551621897f, Delta=0.042768353f, Epsilon=0.457860107f;
                   mfxF32 Q = Alpha * powf(R, Beta) * powf(F, Epsilon) * powf(B, Gamma) * powf(C, Delta);
                   mfxF32 Qp = 6.f * (logf(Q)/logf(2.f)) + 2.f;
                   laQp = (mfxI32) (Qp);
                }
            }
            else 
            {
                if(m_par.mLaQp != 26 || !frameStruct.frameCmplx || !m_par.lowPower) {
                    laQp = (mfxI32)(0.6634f*laQ - 0.035f); // NN V L
                } else {
                   mfxF32 R = (mfxF32) ((1.5 * m_par.width * m_par.height) / (inputBitsPerFrame * m_par.frameRate / 1000.0));
                   mfxF32 SC = (mfxF32) frameStruct.frameCmplx;
                   mfxF32 C  = std::max(SC  / (128*64), 0.03f);
                   mfxF32 B = (mfxF32)laScaledSize / (mfxF32) (1.5 * m_par.width * m_par.height);
                   mfxF32 F  = (mfxF32) (std::max(std::min(m_par.frameRate, 61.0), 23.0));
                   const mfxF32 Alpha=0.095227005f, Beta=0.661596921f, Gamma=0.486806893f, Delta=0.078518546f, Epsilon=0.446383738f;
                   mfxF32 Q = Alpha * powf(R, Beta) * powf(F, Epsilon) * powf(B, Gamma) * powf(C, Delta);
                   mfxF32 Qp = 6.f * (logf(Q)/logf(2.f)) + 3.f;
                   laQp = (mfxI32) (Qp);
                }
            }
        }
        else 
        {
            laQp = (mfxI32)(0.776f * laQ + 4.6f);
        }
    }
    if (updateState) {
        m_ctx.la.LastLaQpCalc = laQp;
        m_ctx.la.SetLaQpCalcOrder(frameStruct.encOrder, frameStruct.dispOrder);
        m_ctx.la.LastLaQpCalcHasI = (frameStruct.frameType & (MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I)) || frameStruct.LaIDist;
    }
    return laQp;
}

mfxStatus BRC_EncToolBase::ProcessFrame(mfxU32 dispOrder, mfxEncToolsBRCQuantControl *pFrameQp, 
    mfxEncToolsHintQPMap* qpMapHint)
{
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(pFrameQp);

    auto frameStructItr = std::find_if(m_FrameStruct.begin(), m_FrameStruct.end(), CompareByDisplayOrder(dispOrder));
    if (frameStructItr == m_FrameStruct.end())
        return MFX_ERR_UNDEFINED_BEHAVIOR; // BRC hasn't processed the frame
    BRC_FrameStruct frameStruct = *frameStructItr;

    mfxU16 ParSceneChange = frameStruct.sceneChange;
    mfxU16 ParLongTerm = frameStruct.longTerm;

    mfxU32 ParFrameCmplx = 0; // Used only for LA models for now
    mfxU16 ParQpModulation = frameStruct.qpModulation;
    mfxI32 ParQpDeltaP = 0;
    mfxU16 miniGoPSize = frameStruct.miniGopSize == 0 ? m_par.gopRefDist : frameStruct.miniGopSize;
    if (ParQpModulation == MFX_QP_MODULATION_NOT_DEFINED
        && miniGoPSize >= 8 && m_par.bPyr
        && (m_par.codecId == MFX_CODEC_HEVC || m_par.codecId == MFX_CODEC_AV1)) {
        ParQpModulation = BRC_QP_MODULATION_GOP8_FIXED;
    }

    mfxI32 qp = 0;
    mfxI32 qpMin = 1;
    mfxU16 type = GetFrameType(frameStruct.frameType, frameStruct.pyrLayer, m_par.gopRefDist, m_par.codecId);
    bool  bIdr = (type == MFX_FRAMETYPE_IDR);
    mfxU16 isRef = frameStruct.frameType & MFX_FRAMETYPE_REF;
    mfxU16 isIntra = frameStruct.frameType & (MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I);

    if (type == MFX_FRAMETYPE_P)
        ParQpDeltaP = (frameStruct.qpDelta == MFX_QP_UNDEFINED) ? 0 : std::max(-4, std::min(2, (mfxI32)frameStruct.qpDelta));

    mfxF64 HRDDevFactor = 0.0;
    mfxF64 HRDDev = 0.0;
    mfxF64 maxFrameSizeHrd = 0.0;

    if (m_par.HRDConformance != MFX_BRC_NO_HRD)
    {
        HRDDevFactor = m_hrdSpec->GetBufferDeviationFactor(frameStruct.encOrder);
        HRDDev = m_hrdSpec->GetBufferDeviation(frameStruct.encOrder);
        maxFrameSizeHrd = m_hrdSpec->GetMaxFrameSizeInBits(frameStruct.encOrder, bIdr);
    }
    frameStructItr->origSeqQp = GetCurQP(MFX_FRAMETYPE_P, frameStruct.pyrLayer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP);
    if (!m_bDynamicInit) {
        if (isIntra) {
            // Init DQP
            if (!ParLongTerm && m_par.mHasALTR) {
                m_par.iDQp = 0;
            }
            else if ((!ParFrameCmplx && !frameStruct.LaAvgEncodedSize) || (frameStruct.LaIDist && frameStruct.LaIDist<32)) { // longterm but no complexity measure
                m_par.iDQp = 1;
            }
            else {
                m_par.iDQp = m_par.iDQp0;
            }
            ltrprintf("DQp0 %d LongTerm %d ParFrameCmplx %d AvgEncodedSize %d IDist %d\n", m_par.iDQp, ParLongTerm, ParFrameCmplx, frameStruct.LaAvgEncodedSize, frameStruct.LaIDist);
            // Init Min Qp
            if (ParFrameCmplx > 0) {
                mfxF64 raca = (mfxF64)ParFrameCmplx / RACA_SCALE;
                // MaxFrameSize
                mfxF64 maxFrameSize = m_par.mRawFrameSizeInBits;
                if (m_par.maxFrameSizeInBits) {
                    maxFrameSize = std::min<mfxF64>(maxFrameSize, m_par.maxFrameSizeInBits);
                }
                if (m_par.HRDConformance != MFX_BRC_NO_HRD) {
                    mfxF64 bufOccupy = LTR_BUF(type, m_par.iDQp, m_par.mIntraBoost, 1, 0, m_par.mVeryLowDelay);
                    maxFrameSize = std::min(maxFrameSize, (bufOccupy / 9.* (m_par.initialDelayInBytes * 8.0) + (9.0 - bufOccupy) / 9.*m_par.inputBitsPerFrame));
                }
                // Set Intra QP
                mfxF64 dqf = DQF(type, m_par.iDQp, m_par.mIntraBoost, 1);
                mfxF64 targetFrameSize = dqf * FRM_RATIO(type, 0, 0, m_par.bPyr) * (mfxF64)m_par.inputBitsPerFrame;
                targetFrameSize = std::min(maxFrameSize, targetFrameSize);
                mfxI32 qp0 = compute_first_qp_intra((mfxI32)targetFrameSize, m_par.mRawFrameSizeInPixs, raca);
                if (targetFrameSize < 6.5 * m_par.inputBitsPerFrame && qp0 > 3)
                    qp0 -= 3; // use re-encoding for best results (maxFrameSizeGood)
                else if (raca == MIN_RACA && qp0 > 3)
                    qp0 -= 3; // uncertainty; use re-encoding for best results
                qp0 = mfx::clamp(qp0, m_par.quantMinI, m_par.quantMaxI);
                ltrprintf("Qp0 %d\n", qp0);
                UpdateQPParams(qp0, MFX_FRAMETYPE_IDR, m_ctx, 0, m_par.quantMinI, m_par.quantMaxI, 0, m_par.iDQp, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, m_par.codecId);
                qpMin = qp0;
            }
            else if (ParLongTerm) {
                mfxI32 qp0 = m_ctx.QuantIDR;
                UpdateQPParams(qp0, MFX_FRAMETYPE_IDR, m_ctx, 0, m_par.quantMinI, m_par.quantMaxI, 0, m_par.iDQp, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, m_par.codecId);
                qpMin = qp0;
            }
        }

        if (frameStruct.LaAvgEncodedSize) {
            mfxI32 laQp = GetLaQpEst(frameStruct.LaAvgEncodedSize, m_par.inputBitsPerFrame, frameStruct, true);
            laQp = std::max(laQp, m_ctx.QuantP - (m_ctx.QuantP/2));
            mfxI32 qp0 = mfx::clamp(std::max(laQp - 1 - (mfxI32)m_par.iDQp, 1), m_par.quantMinI, m_par.quantMaxI);
            ltrprintf("Dynamic Init LA %d Qp %d P Qp %d\n", frameStruct.LaAvgEncodedSize, laQp, m_ctx.QuantP);
            UpdateQPParams(qp0, MFX_FRAMETYPE_IDR, m_ctx, 0, m_par.quantMinI, m_par.quantMaxI, 0, m_par.iDQp, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, m_par.codecId);
            qpMin = m_ctx.QuantIDR;
            m_ctx.la.SetLaQpUpdateOrder(frameStruct.encOrder, frameStruct.dispOrder);
        }
        m_bDynamicInit = true;
    }

    if (frameStruct.encOrder == m_ctx.encOrder || frameStruct.numRecode)
    {
        qp = m_ctx.Quant;
    }
    else
    {
        if (isIntra)
        {
            // Set DQp if IDR
            if (type == MFX_FRAMETYPE_IDR) {
                if (!ParLongTerm && m_par.mHasALTR) {
                    m_par.iDQp = 0;
                }
                else if ((!ParFrameCmplx && !frameStruct.LaAvgEncodedSize) || (frameStruct.LaIDist && frameStruct.LaIDist<32) || !m_par.mHasALTR) {
                    m_par.iDQp = 1;
                }
                else if (ParSceneChange) {
                    m_par.iDQp = m_par.iDQp0;
                }
                ltrprintf("DQp0 %d LongTerm %d ParFrameCmplx %d AvgEncodedSize %d IDist %d SceneChange %d\n", m_par.iDQp, ParLongTerm, ParFrameCmplx, frameStruct.LaAvgEncodedSize, frameStruct.LaIDist, ParSceneChange);
            }

            mfxF64 maxFrameSize = m_par.mRawFrameSizeInBits;
            if (m_par.maxFrameSizeInBits) {
                maxFrameSize = std::min<mfxF64>(maxFrameSize, m_par.maxFrameSizeInBits);
            }
            if (m_par.HRDConformance != MFX_BRC_NO_HRD) {
                mfxF64 hrdMaxFrameSize = m_par.initialDelayInBytes * 8;
                if (maxFrameSizeHrd > 0)
                    hrdMaxFrameSize =  std::min(hrdMaxFrameSize, maxFrameSizeHrd);
                mfxF64 bufOccupy = LTR_BUF(type, m_par.iDQp, ((type == MFX_FRAMETYPE_IDR) ? m_par.mIntraBoost : false), (ParSceneChange || (m_ctx.LastIQpSet && m_ctx.QuantP > ((mfxI32)m_ctx.LastIQpSet + (mfxI32)m_par.iDQp + 1))), 0, m_par.mVeryLowDelay);
                maxFrameSize = std::min(maxFrameSize, (bufOccupy / 9.* hrdMaxFrameSize + (9.0 - bufOccupy) / 9.*m_par.inputBitsPerFrame));
            }

            if (type == MFX_FRAMETYPE_IDR && m_par.mHasALTR) {
                // Re-Determine LTR  iDQP
                if (!ParLongTerm)
                    m_par.iDQp = 0;
                else
                {
                    mfxF64 maxFrameRatio = 2 * FRM_RATIO(type, frameStruct.encOrder, 0, m_par.bPyr);
                    mfxF64 minFrameRatio = FRM_RATIO(type, 0, 0, m_par.bPyr);
                    maxFrameRatio = std::min(maxFrameRatio, (maxFrameSize / m_par.inputBitsPerFrame));
                    mfxU32 mNumRefsInGop = m_par.mNumRefsInGop;
                    if (m_ctx.LastIQpSetOrder) {
                        mfxU32 pastRefsInGop = (mfxU32)(std::max(1.0, (!m_par.bPyr ? (mfxF64)(frameStruct.encOrder - m_ctx.LastIQpSetOrder) / (mfxF64)m_par.gopRefDist : (mfxF64)(frameStruct.encOrder - m_ctx.LastIQpSetOrder) / 2.0)));
                        mNumRefsInGop = std::min(mNumRefsInGop, pastRefsInGop);
                    }
                    maxFrameRatio = std::min<mfxF64>(maxFrameRatio, mNumRefsInGop);
                    mfxF64 dqpmax = std::max(0.0, 6.0 * (log(maxFrameRatio / minFrameRatio) / log(2.0)));
                    mfxU32 iDQpMax = (mfxU32)(dqpmax + 0.5);
                    if((!ParFrameCmplx && !frameStruct.LaAvgEncodedSize) || (frameStruct.LaIDist && frameStruct.LaIDist<32))
                        iDQpMax = mfx::clamp(iDQpMax, 1u, 2u);
                    else if (ParSceneChange)
                        iDQpMax = mfx::clamp(iDQpMax, 1u, m_par.iDQp0);
                    else
                        iDQpMax = mfx::clamp<mfxU32>(iDQpMax, 1u, MAX_DQP_LTR);
                    m_par.iDQp = iDQpMax;
                    ltrprintf("FR %lf DQp %d\n", maxFrameRatio, m_par.iDQp);
                }
            }

            // Determine Min Qp
            if (ParFrameCmplx > 0)
            {
                mfxF64 raca = (mfxF64)ParFrameCmplx / RACA_SCALE;
                mfxF64 dqf = DQF(type, m_par.iDQp, ((type == MFX_FRAMETYPE_IDR) ? m_par.mIntraBoost : false), ParSceneChange);
                mfxF64 targetFrameSize = dqf * FRM_RATIO(type, frameStruct.encOrder, 0, m_par.bPyr) * m_par.inputBitsPerFrame;
                if (m_par.rateControlMethod == MFX_RATECONTROL_CBR && m_par.HRDConformance) {
                    // CBR HRD Buffer overflow has priority
                    mfxF64 dev = -1.0*targetFrameSize - HRDDev;
                    if (dev > 0)
                        targetFrameSize += std::min(targetFrameSize, (dev / 2.0));
                }

                targetFrameSize = std::min(maxFrameSize, targetFrameSize);
                mfxF64 CmplxRatio = 1.0;
                if (m_ctx.LastICmplx) CmplxRatio = ParFrameCmplx / m_ctx.LastICmplx;
                if (!ParSceneChange && m_ctx.LastICmplx && m_ctx.LastIQpAct && m_ctx.LastIFrameSize && CmplxRatio > 0.5 && CmplxRatio < 2.0)
                {
                    qpMin = compute_new_qp_intra((mfxI32)targetFrameSize, m_par.mRawFrameSizeInPixs, raca, m_ctx.LastIFrameSize, (mfxF64)m_ctx.LastICmplx / RACA_SCALE, m_ctx.LastIQpAct);
                    if (raca == MIN_RACA && qpMin > 3)
                        qpMin -= 3; // uncertainty; use re-encoding for best results
                }
                else
                {
                    qpMin = compute_first_qp_intra((mfxI32)targetFrameSize, m_par.mRawFrameSizeInPixs, raca);
                    if (targetFrameSize < 6.5 * m_par.inputBitsPerFrame && qpMin>3)
                        qpMin -= 3; // uncertainty; use re-encoding for best results
                    else if (raca == MIN_RACA && qpMin > 3)
                        qpMin -= 3; // uncertainty; use re-encoding for best results
                }
                mfxI32 curQp = GetCurQP(type, frameStruct.pyrLayer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP);
                qpMin = std::min(qpMin, curQp + 6);
                ltrprintf("Min QpI %d Schg %d\n", qpMin, ParSceneChange);
            }
            else if (frameStruct.LaAvgEncodedSize) {
                mfxF64 dqf = DQF(type, m_par.iDQp, ((type == MFX_FRAMETYPE_IDR) ? m_par.mIntraBoost : false), ParSceneChange);
                mfxF64 ratio = dqf * FRM_RATIO(type, frameStruct.encOrder, 0, m_par.bPyr);
                mfxF64 targetFrameSize = ratio * m_par.inputBitsPerFrame;
                if (m_par.rateControlMethod == MFX_RATECONTROL_CBR && m_par.HRDConformance) {
                    // CBR HRD Buffer overflow has priority
                    mfxF64 devI = -1.0*targetFrameSize - HRDDev;
                    if (devI > 0)
                        targetFrameSize += std::min(targetFrameSize, (devI / 2.0));
                }
                else {
                    mfxF64 devI = - m_ctx.totalDeviation;
                    if (devI > 0)
                        targetFrameSize += std::min(targetFrameSize, devI);
                    else
                        targetFrameSize -= std::min(targetFrameSize/2.0, -devI);
                }

                targetFrameSize = std::min(maxFrameSize, targetFrameSize);

                mfxF64 modelScale = 0.457120157;
                mfxF64 IBitsRatio = 1.0;
                if (m_ctx.la.LastLaIBits) IBitsRatio = (mfxF64)frameStruct.LaCurEncodedSize / (mfxF64)m_ctx.la.LastLaIBits;
                if (!ParSceneChange && m_ctx.la.LastLaIBits && m_ctx.LastIQpAct && m_ctx.LastIFrameSize && IBitsRatio > 0.5 && IBitsRatio < 2.0) {
                    mfxF64 lastibits = (mfxF64)(m_ctx.la.LastLaIBits * (1 << (m_par.mLaScale << 1)));
                    mfxF64 ibits_pred = lastibits * pow(2.0, ((mfxF64)m_par.mLaQp - (mfxF64)m_ctx.LastIQpAct) / 6.0);
                    modelScale = (mfxF64)m_ctx.LastIFrameSize / pow(ibits_pred, 1.013753126);
                    modelScale = std::max(0.1, std::min(modelScale, 1.0));
                }
                mfxF64 ibits = (mfxF64)(frameStruct.LaCurEncodedSize * (1 << (m_par.mLaScale << 1)));
                mfxF64 EstRate[52] = { 0.0 };
                for (mfxU32 qpest = 0; qpest < 52; qpest++)
                {
                    mfxF64 ibits_qp = (ibits*pow(2.0, ((mfxF64)m_par.mLaQp - (mfxF64)qpest) / 6.0));
                    ibits_qp = pow(ibits_qp, 1.013753126)*modelScale;
                    EstRate[qpest] = ibits_qp;
                }
                mfxI32 minlaQp = SelectQp(EstRate, targetFrameSize);
                ltrprintf("Selected LA MinQp %d iDQp %d IQ %d, PQ %d modelScale %lf\n", minlaQp, m_par.iDQp, m_ctx.QuantIDR, m_ctx.QuantP, modelScale);
                qpMin = minlaQp;
                mfxI32 curQp = GetCurQP(type, frameStruct.pyrLayer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP);
                qpMin = std::min(qpMin, curQp + 6);
                ltrprintf("Min QpI %d Schg %d\n", qpMin, ParSceneChange);
            }

            if (type == MFX_FRAMETYPE_IDR && frameStruct.LaAvgEncodedSize && ParSceneChange) {
                mfxF64 dev = 0;
                if (m_par.rateControlMethod == MFX_RATECONTROL_CBR && m_par.HRDConformance) {
                    dev = std::min(m_par.inputBitsPerFrame * m_par.mLaDepth / 2, abs(HRDDev) / 2); // upto 50% boost or damp
                    if (HRDDev < 0) dev *= -1;
                }
                else {
                    dev = std::min(m_par.inputBitsPerFrame * m_par.mLaDepth / 2, abs(m_ctx.totalDeviation) / 2); // upto 50% boost or damp
                    if (m_ctx.totalDeviation < 0) dev *= -1;
                }
                mfxF64 inputBitsPerFrameAdj = (m_par.inputBitsPerFrame * m_par.mLaDepth - dev) / m_par.mLaDepth;
                mfxI32 laQp = GetLaQpEst(frameStruct.LaAvgEncodedSize, inputBitsPerFrameAdj, frameStruct, true);

                mfxI32 curPQp = GetCurQP(MFX_FRAMETYPE_P, frameStruct.pyrLayer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP);
                {
                    laQp = std::max(laQp, curPQp - (curPQp / 4));
                    laQp = std::min(curPQp + 6, laQp); // for quality
                    ltrprintf("Dynamic SChg %d LA Qp %d P Qp %d\n", frameStruct.dispOrder, laQp, curPQp);
                    UpdateQPParams(laQp, MFX_FRAMETYPE_P, m_ctx, 0, m_par.quantMinI, m_par.quantMaxI, 0, m_par.iDQp, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, m_par.codecId);
                    qpMin = m_ctx.QuantIDR;
                    m_ctx.la.SetLaQpUpdateOrder(frameStruct.encOrder, frameStruct.dispOrder);
                }
            }
        }
        else //if (type == MFX_FRAMETYPE_P)
        {
            mfxU16 ltype = MFX_FRAMETYPE_P;
            mfxF64 maxFrameSize = m_par.mRawFrameSizeInBits;
            if (m_par.maxFrameSizeInBits) {
                maxFrameSize = std::min<mfxF64>(maxFrameSize, m_par.maxFrameSizeInBits);
            }
            if (m_par.HRDConformance != MFX_BRC_NO_HRD) {
                mfxF64 hrdMaxFrameSize = m_par.initialDelayInBytes * 8;
                if (maxFrameSizeHrd > 0) hrdMaxFrameSize = std::min(hrdMaxFrameSize, (mfxF64)maxFrameSizeHrd);

                mfxF64 bufOccupy = LTR_BUF(ltype, m_par.iDQp, false, ParSceneChange, ParSceneChange, m_par.mVeryLowDelay);
                maxFrameSize = std::min(maxFrameSize, (bufOccupy / 9.* hrdMaxFrameSize + (9.0 - bufOccupy) / 9.*m_par.inputBitsPerFrame));
            }

            mfxF64 targetFrameSize = FRM_RATIO(ltype, frameStruct.encOrder, 0, m_par.bPyr) * m_par.inputBitsPerFrame;
            if (m_par.bPyr && m_par.gopRefDist >= 8)
                targetFrameSize *= ((ParQpModulation == MFX_QP_MODULATION_HIGH
                                    || ParQpModulation == BRC_QP_MODULATION_GOP8_FIXED) ? 2.0 :
                                    (( ParQpModulation != MFX_QP_MODULATION_LOW) ? 1.66 : 1.0));
            // Aref
            if (type == MFX_FRAMETYPE_P && ParLongTerm && ParQpDeltaP<0) targetFrameSize *= 1.58;

            if (m_par.rateControlMethod == MFX_RATECONTROL_CBR && m_par.HRDConformance != MFX_BRC_NO_HRD) {
                mfxF64 dev = -1.0*targetFrameSize - HRDDev;
                if (dev > 0)
                    targetFrameSize += std::min(targetFrameSize, (dev / 4.0));
            }
            else {
                mfxF64 dev = -m_ctx.totalDeviation;
                if (dev > 0)
                    targetFrameSize += std::min(targetFrameSize, (dev / 2.0));
            }
            targetFrameSize = std::min(maxFrameSize, targetFrameSize);
            qpMin = GetMinQForMaxFrameSize(m_par, targetFrameSize, ltype);

            // LA P Update
            if (type == MFX_FRAMETYPE_P && frameStruct.LaAvgEncodedSize && m_par.HRDConformance == MFX_BRC_NO_HRD)
            {
                mfxU32 lastLaPBitsAvg = 0;
                bool LaHasI = isIntra || frameStruct.LaIDist;
                if (m_ctx.la.LastLaQpCalcHasI == LaHasI) {
                    lastLaPBitsAvg = m_ctx.la.GetLastCalcLaBitsAvg(frameStruct.encOrder);
                }

                if (ParSceneChange)
                {
                    mfxF64 dev = std::min(m_par.inputBitsPerFrame * m_par.mLaDepth / 2, abs(m_ctx.totalDeviation) / 2) * (m_ctx.totalDeviation < 0 ? -1 : 1); // upto 50% boost or damp
                    mfxF64 inputBitsPerFrameAdj = (m_par.inputBitsPerFrame * m_par.mLaDepth - dev) / m_par.mLaDepth;
                    mfxI32 lastLaQp = m_ctx.la.LastLaQpCalc;
                    mfxI32 laQp = GetLaQpEst(frameStruct.LaAvgEncodedSize, inputBitsPerFrameAdj, frameStruct, true);
                    mfxI32 curPQp = GetCurQP(MFX_FRAMETYPE_P, frameStruct.pyrLayer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP);
                    mfxI32 diffQp = laQp - (lastLaQp ? lastLaQp : laQp);
                    laQp = curPQp + diffQp;
                    laQp = std::max(laQp, curPQp - (curPQp / 4));
                    laQp = std::min(curPQp + 6, laQp); // for quality
                    if(laQp != curPQp)
                    {
                        UpdateQPParams(laQp, MFX_FRAMETYPE_P, m_ctx, 0, m_par.quantMinI, m_par.quantMaxI, 0, m_par.iDQp, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, m_par.codecId);
                        qpMin = m_ctx.QuantIDR;
                        if(curPQp != GetCurQP(MFX_FRAMETYPE_P, frameStruct.pyrLayer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP))
                            m_ctx.la.SetLaQpUpdateOrder(frameStruct.encOrder, frameStruct.dispOrder);
                    }
                }
                else if (m_ctx.la.IsCalcLaQpDist(frameStruct.dispOrder) && lastLaPBitsAvg && frameStruct.LaAvgEncodedSize > 1.33*lastLaPBitsAvg && frameStruct.LaCurEncodedSize > lastLaPBitsAvg)   // future Inc
                {
                    mfxF64 dev = std::min(m_par.inputBitsPerFrame * m_par.mLaDepth / 2, abs(m_ctx.totalDeviation) / 2) * (m_ctx.totalDeviation < 0 ? -1 : 1);
                    mfxF64 inputBitsPerFrameAdj = (m_par.inputBitsPerFrame * m_par.mLaDepth - dev) / m_par.mLaDepth;
                    mfxI32 lastLaQp = m_ctx.la.LastLaQpCalc;
                    mfxI32 laQp = GetLaQpEst(frameStruct.LaAvgEncodedSize, inputBitsPerFrameAdj, frameStruct, true);

                    if (m_ctx.la.IsUpdateLaQpDist(frameStruct.dispOrder))
                    {
                        mfxF64 devTh = m_par.inputBitsPerFrame * -2.0;
                        mfxI32 curPQp = GetCurQP(MFX_FRAMETYPE_P, frameStruct.pyrLayer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP);
                        mfxI32 diffQp = laQp - (lastLaQp ? lastLaQp : laQp);
                        laQp = curPQp + diffQp;
                        laQp = std::max(laQp, curPQp); // inc
                        laQp = std::min(curPQp + (dev < devTh ? 1: 6), laQp); // for quality
                        if(laQp != curPQp)
                        {
                            UpdateQPParams(laQp, MFX_FRAMETYPE_P, m_ctx, 0, m_par.quantMinI, m_par.quantMaxI, 0, m_par.iDQp, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, m_par.codecId);
                            if(curPQp != GetCurQP(MFX_FRAMETYPE_P, frameStruct.pyrLayer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP))
                                m_ctx.la.SetLaQpUpdateOrder(frameStruct.encOrder, frameStruct.dispOrder);
                        }
                    }
                }
                else if (m_ctx.la.IsCalcLaQpDist(frameStruct.dispOrder) && lastLaPBitsAvg && lastLaPBitsAvg > 1.33*frameStruct.LaAvgEncodedSize && frameStruct.LaCurEncodedSize < lastLaPBitsAvg)  // future dec
                {
                    mfxF64 dev = std::min(m_par.inputBitsPerFrame * m_par.mLaDepth / 2, abs(m_ctx.totalDeviation) / 2) * (m_ctx.totalDeviation < 0 ? -1 : 1);
                    mfxF64 inputBitsPerFrameAdj = (m_par.inputBitsPerFrame * m_par.mLaDepth - dev) / m_par.mLaDepth;
                    mfxI32 lastLaQp = m_ctx.la.LastLaQpCalc;
                    mfxI32 laQp = GetLaQpEst(frameStruct.LaAvgEncodedSize, inputBitsPerFrameAdj, frameStruct, true);

                    if (m_ctx.la.IsUpdateLaQpDist(frameStruct.dispOrder))
                    {
                        mfxF64 devTh = m_par.inputBitsPerFrame * 2.0;
                        mfxI32 curPQp = GetCurQP(MFX_FRAMETYPE_P, frameStruct.pyrLayer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP);
                        mfxI32 diffQp = laQp - (lastLaQp ? lastLaQp : laQp);
                        laQp = curPQp + diffQp;
                        laQp = std::max(laQp, std::max(curPQp - (curPQp / 4), curPQp - (dev > devTh ? 1: 6)));
                        laQp = std::min(curPQp, laQp); // dec
                        if(laQp != curPQp)
                        {
                            UpdateQPParams(laQp, MFX_FRAMETYPE_P, m_ctx, 0, m_par.quantMinI, m_par.quantMaxI, 0, m_par.iDQp, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, m_par.codecId);
                            if(curPQp != GetCurQP(MFX_FRAMETYPE_P, frameStruct.pyrLayer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP))
                                m_ctx.la.SetLaQpUpdateOrder(frameStruct.encOrder, frameStruct.dispOrder);
                        }
                    }
                }
                else if (m_ctx.la.IsCalcLaQpDist(frameStruct.dispOrder))
                {
                    mfxF64 dev = std::min(m_par.inputBitsPerFrame * m_par.mLaDepth / 2, abs(m_ctx.totalDeviation) / 2) * (m_ctx.totalDeviation < 0 ? -1 : 1);
                    mfxF64 inputBitsPerFrameAdj = (m_par.inputBitsPerFrame * m_par.mLaDepth - dev) / m_par.mLaDepth;
                    GetLaQpEst(frameStruct.LaAvgEncodedSize, inputBitsPerFrameAdj, frameStruct, true);
                }
            }
        }

        qp = GetCurQP(type, frameStruct.pyrLayer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP);

        // Max Frame Size recode prevention
        if (qp < qpMin)
        {
            if (type != MFX_FRAMETYPE_B)
            {
                SetQPParams(qpMin, type, m_ctx, 0, m_par.quantMinI, m_par.quantMaxI, 0, m_par.iDQp, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP, m_par.codecId);
                qp = GetCurQP(type, frameStruct.pyrLayer, isRef, ParQpModulation, frameStruct.QPDeltaExplicitModulation, ParQpDeltaP);
            }
            else
            {
                qp = qpMin;
            }
        }
        else
            qpMin = std::min(qp - (isIntra ? 0 : 1), qpMin);
    }
    frameStruct.qp = qp - m_par.quantOffset;

    //printf("EncOrder %d recode %d type %d ctrl->QpY %d, qp %d PyrLayer %d QpMod %d quantOffset %d 
    //        Cmplx %ld LaCurEncodedSize %d LaAvgEncodedSize %d Deviation %f\n",
    //        frameStruct.encOrder, frameStruct.numRecode, type, frameStruct.qp, qp, frameStruct.pyrLayer, ParQpModulation, m_par.quantOffset,
    //        ParFrameCmplx, frameStruct.LaCurEncodedSize, frameStruct.LaAvgEncodedSize, m_ctx.totalDeviation);

    if (isIntra) {
        m_ctx.LastIQpSetOrder = frameStruct.encOrder;
        m_ctx.LastIQpMin = qpMin - m_par.quantOffset;
        m_ctx.LastIQpSet = frameStruct.qp;
        m_ctx.LastIQpAct = 0;
        m_ctx.LastICmplx = ParFrameCmplx;

        m_ctx.LastIFrameSize = 0;
        ResetMinQForMaxFrameSize(&m_par, type);
    }
    if (frameStruct.LaCurEncodedSize) 
    {
        m_ctx.la.SaveLaBits(isIntra, frameStruct.LaCurEncodedSize, (mfxU32) m_par.fAbPeriodLA, !frameStruct.numRecode);
    }
    pFrameQp->QpY = frameStruct.qp;
    pFrameQp->NumDeltaQP = 0;

    // PAQ QpMapNZ & QpMap Average return by Base FillQpMap, other implementations can override these.
    frameStructItr->QpMapNZ = FillQpMap(*frameStructItr, pFrameQp->QpY, qpMapHint, frameStructItr->QpMapBias);
    if (frameStructItr->QpMapNZ) 
    {
        pFrameQp->QpY = mfx::clamp((mfxI32)pFrameQp->QpY + frameStructItr->QpMapBias, 1, 51); // Allow slice Qp to change 1-51 for lambda
    }

    return MFX_ERR_NONE;
}


mfxStatus BRC_EncToolBase::ReportEncResult(mfxU32 dispOrder, mfxEncToolsBRCEncodeResult const & pEncRes)
{
    auto frameStruct = std::find_if(m_FrameStruct.begin(), m_FrameStruct.end(), CompareByDisplayOrder(dispOrder));
    if (frameStruct == m_FrameStruct.end())
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR; // BRC gets encoding results for the frame it hasn't set QP for
    }
    (*frameStruct).frameSize = pEncRes.CodedFrameSize;
    (*frameStruct).qp = pEncRes.QpY;
    (*frameStruct).numRecode = pEncRes.NumRecodesDone;
    return MFX_ERR_NONE;
}


mfxStatus BRC_EncToolBase::SetFrameStruct(mfxU32 dispOrder, mfxEncToolsBRCFrameParams  const & pFrameStruct)
{
    auto frameStruct = std::find_if(m_FrameStruct.begin(), m_FrameStruct.end(), CompareByDisplayOrder(dispOrder));
    if (frameStruct == m_FrameStruct.end())
    {
        BRC_FrameStruct frStruct;
        frStruct.dispOrder = dispOrder;
        frStruct.frameType = pFrameStruct.FrameType;
        frStruct.pyrLayer = pFrameStruct.PyramidLayer;
        frStruct.encOrder = pFrameStruct.EncodeOrder;
        frStruct.longTerm = pFrameStruct.LongTerm;
        frStruct.sceneChange = pFrameStruct.SceneChange;
        frStruct.frameCmplx = pFrameStruct.SpatialComplexity;
        frStruct.PersistenceMapNZ = pFrameStruct.PersistenceMapNZ;
        memcpy(frStruct.PersistenceMap, pFrameStruct.PersistenceMap, sizeof(frStruct.PersistenceMap));
        m_FrameStruct.push_back(frStruct);
        frameStruct = m_FrameStruct.end() - 1;
    }
    else
    {
        (*frameStruct).frameType = pFrameStruct.FrameType;
        (*frameStruct).pyrLayer = pFrameStruct.PyramidLayer;
        (*frameStruct).encOrder = pFrameStruct.EncodeOrder; // or check if it's the same, otherwise - error ?
        (*frameStruct).numRecode++;  // ??? check
        (*frameStruct).longTerm = pFrameStruct.LongTerm;
        (*frameStruct).sceneChange = pFrameStruct.SceneChange;
        (*frameStruct).frameCmplx = pFrameStruct.SpatialComplexity;
        (*frameStruct).PersistenceMapNZ = pFrameStruct.PersistenceMapNZ;
        memcpy((*frameStruct).PersistenceMap, pFrameStruct.PersistenceMap, sizeof((*frameStruct).PersistenceMap));

    }
    return MFX_ERR_NONE;
}

mfxStatus BRC_EncToolBase::ReportBufferHints(mfxU32 dispOrder, mfxEncToolsBRCBufferHint const & pBufHints)
{
    auto frameStruct = std::find_if(m_FrameStruct.begin(), m_FrameStruct.end(), CompareByDisplayOrder(dispOrder));
    if (frameStruct == m_FrameStruct.end())
    {
        BRC_FrameStruct frStruct;
        frStruct.dispOrder = dispOrder;
        frStruct.LaAvgEncodedSize        = pBufHints.AvgEncodedSizeInBits;
        frStruct.LaCurEncodedSize        = pBufHints.CurEncodedSizeInBits;
        frStruct.LaIDist                 = pBufHints.DistToNextI;
        m_FrameStruct.push_back(frStruct);
        frameStruct = m_FrameStruct.end() - 1;
    }
    else
    {
        (*frameStruct).LaAvgEncodedSize        = pBufHints.AvgEncodedSizeInBits;
        (*frameStruct).LaCurEncodedSize        = pBufHints.CurEncodedSizeInBits;
        (*frameStruct).LaIDist                 = pBufHints.DistToNextI;
    }
    return MFX_ERR_NONE;
}

mfxStatus BRC_EncToolBase::ReportGopHints(mfxU32 dispOrder, mfxEncToolsHintPreEncodeGOP const & pGopHints)
{
    auto frameStruct = std::find_if(m_FrameStruct.begin(), m_FrameStruct.end(), CompareByDisplayOrder(dispOrder));
    if (frameStruct == m_FrameStruct.end())
    {
        BRC_FrameStruct frStruct;
        frStruct.dispOrder = dispOrder;
        frStruct.qpDelta = pGopHints.QPDelta;
        frStruct.qpModulation = pGopHints.QPModulation;
        frStruct.QPDeltaExplicitModulation = pGopHints.QPDeltaExplicitModulation;
        frStruct.miniGopSize = pGopHints.MiniGopSize;
        m_FrameStruct.push_back(frStruct);
        frameStruct = m_FrameStruct.end() - 1;
    }
    else
    {
        (*frameStruct).qpDelta = pGopHints.QPDelta;
        (*frameStruct).qpModulation = pGopHints.QPModulation;
        (*frameStruct).QPDeltaExplicitModulation = pGopHints.QPDeltaExplicitModulation;
        (*frameStruct).miniGopSize = pGopHints.MiniGopSize;
    }
    return MFX_ERR_NONE;

}

void HEVC_HRD::Init(cBRCParams const &par)
{
    m_hrdInput.Init(par);
    m_prevAuCpbRemovalDelayMinus1 = -1;
    m_prevAuCpbRemovalDelayMsb = 0;
    m_prevAuFinalArrivalTime = 0;
    m_prevBpAuNominalRemovalTime = (mfxU32)m_hrdInput.m_initCpbRemovalDelay;
    m_prevBpEncOrder = 0;
}

void HEVC_HRD::Reset(cBRCParams const &par)
{
    sHrdInput hrdInput;
    hrdInput.Init(par);
    m_hrdInput.m_bitrate = hrdInput.m_bitrate;
    m_hrdInput.m_cpbSize90k = hrdInput.m_cpbSize90k;
}

void HEVC_HRD::Update(mfxU32 sizeInbits, mfxU32 eo, bool bSEI)
{
    mfxF64 auNominalRemovalTime = 0.0;
    mfxF64 initCpbRemovalDelay = GetInitCpbRemovalDelay(eo);
    if (eo > 0)
    {
        mfxU32 auCpbRemovalDelayMinus1 = (eo - m_prevBpEncOrder) - 1;
        // (D-1)
        mfxU32 auCpbRemovalDelayMsb = 0;

        if (!bSEI && (eo - m_prevBpEncOrder) != 1)
        {
            auCpbRemovalDelayMsb = ((mfxI32)auCpbRemovalDelayMinus1 <= m_prevAuCpbRemovalDelayMinus1)
                ? m_prevAuCpbRemovalDelayMsb + m_hrdInput.m_maxCpbRemovalDelay
                : m_prevAuCpbRemovalDelayMsb;
        }

        m_prevAuCpbRemovalDelayMsb = auCpbRemovalDelayMsb;
        m_prevAuCpbRemovalDelayMinus1 = auCpbRemovalDelayMinus1;

        // (D-2)
        mfxU32 auCpbRemovalDelayValMinus1 = auCpbRemovalDelayMsb + auCpbRemovalDelayMinus1;
        // (C-10, C-11)
        auNominalRemovalTime = m_prevBpAuNominalRemovalTime + m_hrdInput.m_clockTick * (auCpbRemovalDelayValMinus1 + 1);
    }
    else // (C-9)
        auNominalRemovalTime = m_hrdInput.m_initCpbRemovalDelay;

    // (C-3)
    mfxF64 initArrivalTime = m_prevAuFinalArrivalTime;

    if (!m_hrdInput.m_cbrFlag)
    {
        mfxF64 initArrivalEarliestTime = (bSEI)
            // (C-7)
            ? auNominalRemovalTime - initCpbRemovalDelay
            // (C-6)
            : auNominalRemovalTime - m_hrdInput.m_cpbSize90k;
        // (C-4)
        initArrivalTime = std::max<mfxF64>(m_prevAuFinalArrivalTime, initArrivalEarliestTime * m_hrdInput.m_bitrate);
    }
    // (C-8)
    mfxF64 auFinalArrivalTime = initArrivalTime + (mfxF64)sizeInbits * 90000;

    m_prevAuFinalArrivalTime = auFinalArrivalTime;

    if (bSEI)
    {
        m_prevBpAuNominalRemovalTime = auNominalRemovalTime;
        m_prevBpEncOrder = eo;
    }

}

mfxU32 HEVC_HRD::GetInitCpbRemovalDelay(mfxU32 eo) const
{
    mfxF64 auNominalRemovalTime;

    if (eo > 0)
    {
        // (D-1)
        mfxU32 auCpbRemovalDelayMsb = 0;
        mfxU32 auCpbRemovalDelayMinus1 = eo - m_prevBpEncOrder - 1;

        // (D-2)
        mfxU32 auCpbRemovalDelayValMinus1 = auCpbRemovalDelayMsb + auCpbRemovalDelayMinus1;
        // (C-10, C-11)
        auNominalRemovalTime = m_prevBpAuNominalRemovalTime + m_hrdInput.m_clockTick * (auCpbRemovalDelayValMinus1 + 1);

        // (C-17)
        mfxF64 deltaTime90k = auNominalRemovalTime - m_prevAuFinalArrivalTime / m_hrdInput.m_bitrate;

        return (m_hrdInput.m_cbrFlag
            // (C-19)
            ? (mfxU32)(deltaTime90k)
            // (C-18)
            : (mfxU32)std::min(deltaTime90k, m_hrdInput.m_cpbSize90k));
    }

    return  (mfxU32)m_hrdInput.m_initCpbRemovalDelay;
}
inline mfxF64 GetTargetDelay(mfxF64 cpbSize90k, mfxF64 initCpbRemovalDelay, bool bVBR)
{
    return  bVBR ?
        std::max(std::min(3.0*cpbSize90k / 4.0, initCpbRemovalDelay), cpbSize90k / 2.0) :
        std::min(cpbSize90k / 2.0, initCpbRemovalDelay);
}
mfxF64 HEVC_HRD::GetBufferDeviation(mfxU32 eo)  const
{
    mfxU32 delay = GetInitCpbRemovalDelay(eo);
    mfxF64 targetDelay = GetTargetDelay(m_hrdInput.m_cpbSize90k, m_hrdInput.m_initCpbRemovalDelay, !m_hrdInput.m_cbrFlag);
    return (targetDelay - delay) / 90000.0*m_hrdInput.m_bitrate;
}
mfxF64 HEVC_HRD::GetBufferDeviationFactor(mfxU32 eo)  const
{
    mfxU32 delay = GetInitCpbRemovalDelay(eo);
    mfxF64 targetDelay = GetTargetDelay(m_hrdInput.m_cpbSize90k, m_hrdInput.m_initCpbRemovalDelay, !m_hrdInput.m_cbrFlag);
    return abs((targetDelay - delay) / targetDelay);
}
mfxU32 HEVC_HRD::GetMaxFrameSizeInBits(mfxU32 eo, bool /*bSEI*/)  const
{
    return (mfxU32)(GetInitCpbRemovalDelay(eo) / 90000.0*m_hrdInput.m_bitrate);
}
mfxU32 HEVC_HRD::GetMinFrameSizeInBits(mfxU32 eo, bool /*bSEI*/)  const
{
    mfxU32 delay = GetInitCpbRemovalDelay(eo);
    if ((!m_hrdInput.m_cbrFlag) || ((delay + m_hrdInput.m_clockTick + 16.0) < m_hrdInput.m_cpbSize90k))
        return 0;
    return (mfxU32)((delay + m_hrdInput.m_clockTick + 16.0 - m_hrdInput.m_cpbSize90k) / 90000.0*m_hrdInput.m_bitrate + 0.99999);
}


H264_HRD::H264_HRD() :
    m_trn_cur(0)
    , m_taf_prv(0)
{
}

void H264_HRD::Init(cBRCParams const &par)
{
    m_hrdInput.Init(par);
    m_hrdInput.m_clockTick *= (1.0 / 90000.0);

    m_taf_prv = 0.0;
    m_trn_cur = m_hrdInput.m_initCpbRemovalDelay / 90000.0;
    m_trn_cur = GetInitCpbRemovalDelay(0) / 90000.0;
}

void H264_HRD::Reset(cBRCParams const &par)
{
    sHrdInput hrdInput;
    hrdInput.Init(par);
    m_hrdInput.m_bitrate = hrdInput.m_bitrate;
    m_hrdInput.m_cpbSize90k = hrdInput.m_cpbSize90k;
}

void H264_HRD::Update(mfxU32 sizeInbits, mfxU32 eo, bool bSEI)
{
    // const bool interlace = false; //BRC is frame level only
    mfxU32 initDelay = GetInitCpbRemovalDelay(eo);

    double tai_earliest = bSEI
        ? m_trn_cur - (initDelay / 90000.0)
        : m_trn_cur - (m_hrdInput.m_cpbSize90k / 90000.0);

    double tai_cur = (!m_hrdInput.m_cbrFlag)
        ? std::max(m_taf_prv, tai_earliest)
        : m_taf_prv;

    m_taf_prv = tai_cur + (mfxF64)sizeInbits / m_hrdInput.m_bitrate;
    m_trn_cur += m_hrdInput.m_clockTick;

}

mfxU32 H264_HRD::GetInitCpbRemovalDelay(mfxU32 /* eo */)  const
{

    double delay = std::max(0.0, m_trn_cur - m_taf_prv);
    mfxU32 initialCpbRemovalDelay = mfxU32(90000 * delay + 0.5);

    return (mfxU32)(initialCpbRemovalDelay == 0
        ? 1 // should not be equal to 0
        : initialCpbRemovalDelay > m_hrdInput.m_cpbSize90k && (!m_hrdInput.m_cbrFlag)
        ? m_hrdInput.m_cpbSize90k  // should not exceed hrd buffer
        : initialCpbRemovalDelay);
}
mfxF64 H264_HRD::GetBufferDeviation(mfxU32 eo)  const
{
    mfxU32 delay = GetInitCpbRemovalDelay(eo);
    mfxF64 targetDelay = GetTargetDelay(m_hrdInput.m_cpbSize90k, m_hrdInput.m_initCpbRemovalDelay, !m_hrdInput.m_cbrFlag);
    //printf("%d) GetBufferDeviation %f (%d, target %d)\n", eo, (targetDelay - delay) / 90000.0*m_hrdInput.m_bitrate, delay, (int)targetDelay);

    return (targetDelay - delay) / 90000.0*m_hrdInput.m_bitrate;
}
mfxF64 H264_HRD::GetBufferDeviationFactor(mfxU32 eo)  const
{
    mfxU32 delay = GetInitCpbRemovalDelay(eo);
    mfxF64 targetDelay = GetTargetDelay(m_hrdInput.m_cpbSize90k, m_hrdInput.m_initCpbRemovalDelay, !m_hrdInput.m_cbrFlag);
    return abs((targetDelay - delay) / targetDelay);
}


mfxU32 H264_HRD::GetInitCpbRemovalDelayOffset(mfxU32 eo)  const
{
    // init_cpb_removal_delay + init_cpb_removal_delay_offset should be constant
    return mfxU32(m_hrdInput.m_cpbSize90k - GetInitCpbRemovalDelay(eo));
}
mfxU32 H264_HRD::GetMinFrameSizeInBits(mfxU32 eo, bool /*bSEI*/)  const
{
    mfxU32 delay = GetInitCpbRemovalDelay(eo);
    if ((!m_hrdInput.m_cbrFlag) || ((delay + m_hrdInput.m_clockTick * 90000) < m_hrdInput.m_cpbSize90k))
        return 0;

    return (mfxU32)((delay + m_hrdInput.m_clockTick*90000.0 - m_hrdInput.m_cpbSize90k) / 90000.0*m_hrdInput.m_bitrate) + 16;

}
mfxU32 H264_HRD::GetMaxFrameSizeInBits(mfxU32 eo, bool bSEI)  const
{
    mfxU32 initDelay = GetInitCpbRemovalDelay(eo);

    double tai_earliest = (bSEI)
        ? m_trn_cur - (initDelay / 90000.0)
        : m_trn_cur - (m_hrdInput.m_cpbSize90k / 90000.0);

    double tai_cur = (!m_hrdInput.m_cbrFlag)
        ? std::max(m_taf_prv, tai_earliest)
        : m_taf_prv;

    mfxU32 maxFrameSize = (mfxU32)((m_trn_cur - tai_cur)*m_hrdInput.m_bitrate);

    return  maxFrameSize;
}

void sHrdInput::Init(cBRCParams par)
{
    m_cbrFlag = (par.rateControlMethod == MFX_RATECONTROL_CBR);
    m_bitrate = par.maxbps;
    m_maxCpbRemovalDelay = 1 << (h264_h265_au_cpb_removal_delay_length_minus1 + 1);
    m_clockTick = 90000. / par.frameRate;
    m_cpbSize90k = mfxU32(90000. * par.bufferSizeInBytes*8.0 / m_bitrate);
    m_initCpbRemovalDelay = 90000. * 8. * par.initialDelayInBytes / m_bitrate;
}

mfxU16 BRC_EncTool::FillQpMap(const BRC_FrameStruct& frameStruct, mfxU32 frameQp, mfxEncToolsHintQPMap* qpMapHint, mfxI16& qpMapBias)
{
    const auto type = GetFrameType(frameStruct.frameType, frameStruct.pyrLayer, m_par.gopRefDist, m_par.codecId);
    const mfxU16 isIntra = frameStruct.frameType & (MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I);

    mfxI8 QpMap[MFX_ENCTOOLS_PREENC_MAP_SIZE] = {};
    mfxU16 QpMapNZ = 0;
    qpMapBias= 0;
    // PAQ
    if (m_par.mMBBRC && frameStruct.PersistenceMapNZ)
    {
        if (type != MFX_FRAMETYPE_B && 
           (isIntra || frameStruct.dispOrder >= m_ctx.la.LastMBQpSetDispOrder + MBQP_P_UPDATE_DIST ||
           (frameStruct.numRecode && frameStruct.QpMapNZ)))
        {
            // delta QP table
            mfxI32 qp_y = (mfxI32) frameQp;
            mfxI32 qp_d = 2; 

            if (qp_y > 30)
                qp_d = 6;
            else if (qp_y > 25)
                qp_d = 5;
            else if (qp_y > 20)
                qp_d = 4;
            else if (qp_y > 15)
                qp_d = 3;

            mfxU16 count = 0;
            for (mfxU32 i = 0; i < MFX_ENCTOOLS_PREENC_MAP_SIZE; i++)
            {
                QpMap[i] = (mfxI8) (-1 * std::min(qp_d, (frameStruct.PersistenceMap[i] + 1) / 3));
                qpMapBias += QpMap[i];
                if (QpMap[i]) count++;
            }
            QpMapNZ = count;
            qpMapBias = qpMapBias / MFX_ENCTOOLS_PREENC_MAP_SIZE; // Avg
        }
    }

    if (qpMapHint && qpMapHint->ExtQpMap.QP)
    {
        qpMapHint->QpMapFilled = QpMapNZ;
        if (qpMapHint->QpMapFilled) {

            mfxU32 iw = 16;
            mfxU32 ih = 8;
            mfxU32 ibw = m_par.cropWidth / iw;
            mfxU32 ibh = m_par.cropHeight / ih;

            mfxU32 mapBw = qpMapHint->ExtQpMap.BlockSize;
            mfxU32 mapBh = qpMapHint->ExtQpMap.BlockSize;
            mfxU32 wInBlk = qpMapHint->QpMapPitch;
            mfxU32 hInBlk = qpMapHint->ExtQpMap.NumQPAlloc/ wInBlk;

            mfxU16 count = 0;
            mfxI32 minQp = 1;
            mfxI32 maxQp = 51;
            if (type == MFX_FRAMETYPE_IDR || type == MFX_FRAMETYPE_I)
            {
                minQp = m_par.quantMinI;
                maxQp = m_par.quantMaxI;
            }
            else if (type == MFX_FRAMETYPE_P)
            {
                minQp = m_par.quantMinP;
                maxQp = m_par.quantMaxP;
            }
            else
            {
                minQp = m_par.quantMinB;
                maxQp = m_par.quantMaxB;
            }

            // Map 16x8 map to Width/blSize x Height/blSize
            for (mfxU32 i = 0; i < hInBlk; i++)
            {
                for (mfxU32 j = 0; j < wInBlk; j++)
                {
                    // (x,y) is QP lookup location in 16x8 map for pixel center of current block (j,i)
                    mfxU32 y = std::min((i * mapBh + mapBh / 2) / ibh, ih - 1);
                    mfxU32 x = std::min((j * mapBw + mapBw / 2) / ibw, iw - 1);

                    qpMapHint->ExtQpMap.QP[i * wInBlk + j] = mfxI8(
                        mfx::clamp((mfxI32)frameQp + (mfxI32)QpMap[y * iw + x], minQp, maxQp)
                    );

                    if (qpMapHint->ExtQpMap.QP[i * wInBlk + j] != frameQp) count++;
                }
            }
            QpMapNZ = count;
            qpMapHint->QpMapFilled = count;

            if(qpMapHint->QpMapFilled)
                m_ctx.la.LastMBQpSetDispOrder = frameStruct.dispOrder;
        }
    }

    return QpMapNZ;
}

} //namespace EncToolsBRC

mfxStatus ExtBRC::GetFrameCtrl(mfxBRCFrameParam* frame_par, mfxBRCFrameCtrl* ctrl)
{
    mfxEncToolsTaskParam par;
    par.DisplayOrder = frame_par->DisplayOrder;

    std::vector<mfxExtBuffer*> extParams;

    mfxEncToolsBRCFrameParams extFrameStruct;

    extFrameStruct.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_BRC_FRAME_PARAM ;
    extFrameStruct.Header.BufferSz = sizeof(extFrameStruct);
    extFrameStruct.EncodeOrder = frame_par->EncodedOrder;
    extFrameStruct.FrameType = frame_par->FrameType;
    extFrameStruct.PyramidLayer = frame_par->PyramidLayer;
    extFrameStruct.LongTerm = frame_par->LongTerm;

    extParams.push_back((mfxExtBuffer *)&extFrameStruct);

    par.ExtParam = &extParams[0];
    par.NumExtParam = (mfxU16)extParams.size();

    mfxStatus sts;
    sts = Submit(&par);
    MFX_CHECK_STS(sts);

    extParams.clear();

    mfxEncToolsBRCQuantControl extFrameQP;
    extFrameQP.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_BRC_QUANT_CONTROL;
    extFrameQP.Header.BufferSz = sizeof(extFrameQP);

    extParams.push_back((mfxExtBuffer *)&extFrameQP);
    par.ExtParam = &extParams[0];
    par.NumExtParam = (mfxU16)extParams.size();

    sts = Query(&par, 5000);
    MFX_CHECK_STS(sts);

    ctrl->QpY = extFrameQP.QpY;
    return sts;
}

mfxStatus ExtBRC::Update(mfxBRCFrameParam* frame_par, mfxBRCFrameCtrl* frame_ctrl, mfxBRCFrameStatus* status)
{
    mfxEncToolsTaskParam par;
    par.DisplayOrder = frame_par->DisplayOrder;

    std::vector<mfxExtBuffer*> extParams;
    mfxEncToolsBRCEncodeResult extEncRes;
    extEncRes.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_BRC_ENCODE_RESULT;
    extEncRes.Header.BufferSz = sizeof(extEncRes);
    extEncRes.CodedFrameSize = frame_par->CodedFrameSize;
    extEncRes.QpY = (mfxU16)frame_ctrl->QpY;
    extEncRes.NumRecodesDone = frame_par->NumRecode;

    extParams.push_back((mfxExtBuffer *)&extEncRes);

    par.ExtParam = &extParams[0];
    par.NumExtParam = (mfxU16)extParams.size();

    mfxStatus sts;
    sts = Submit(&par);
    MFX_CHECK_STS(sts);

    extParams.clear();
    mfxEncToolsBRCStatus extSts;
    extSts.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_BRC_STATUS;
    extSts.Header.BufferSz = sizeof(extSts);
    extSts.FrameStatus = *status;

    extParams.push_back((mfxExtBuffer *)&extSts);
    par.ExtParam = &extParams[0];
    par.NumExtParam = (mfxU16)extParams.size();

    sts = Query(&par, 5000);

    return sts;
}
