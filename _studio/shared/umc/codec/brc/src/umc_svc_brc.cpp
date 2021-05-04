// Copyright (c) 2012-2019 Intel Corporation
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

#include <math.h>
#include "umc_svc_brc.h"
#include "umc_h264_video_encoder.h"
#include "umc_h264_tables.h"

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE)

namespace UMC
{

SVCBRC::SVCBRC()
{
  mIsInit = 0;

  mNumTemporalLayers = 0;
  mRCfa = 0;
  mQuant = 0;
  mQuantP = 0;
  mQuantI = 0;
  mQuantB = 0;
  mPictureFlagsPrev = 0;
  mQuantUnderflow = 0;
  mQuantOverflow = 0;
  mMaxFrameSize = 0;
  mOversize = 0;
  mUndersize = 0;
  mQuantMax = 0;
  mPictureFlags = 0;
  mRCfap = 0;
  mRCMode = 0;
  mFrameType = NONE_PICTURE;
  mRCqa = 0;
  mRCq = 0;
  mRCqa0 = 0;
  mMinFrameSize = 0;
  mTid = 0;
  mQuantUpdated = 0;
  mRCqap = 0;
  mBitsEncoded = 0;
  mRCbap = 0;
  mRecodeInternal = 0;
}

SVCBRC::~SVCBRC()
{
  Close();
}

// Copy of mfx_h264_enc_common.cpp::LevelProfileLimitsNal
const unsigned long long LevelProfileLimits[4][16][6] = {
    {
        // BASE_PROFILE, MAIN_PROFILE, EXTENDED_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    76800,     210000,    64},
        /* 1b */ {    1485,    99,    152064,    153600,    420000,    64},
        /* 11 */ {    3000,    396,   345600,    230400,    600000,    128},
        /* 12 */ {    6000,    396,   912384,    460800,    1200000,   128},
        /* 13 */ {    11880,   396,   912384,    921600,    2400000,   128},
        /* 2  */ {    11880,   396,   912384,    2400000,   2400000,   128},
        /* 21 */ {    19800,   792,   1824768,   4800000,   4800000,   256},
        /* 22 */ {    20250,   1620,  3110400,   4800000,   4800000,   256},
        /* 3  */ {    40500,   1620,  3110400,   12000000,  12000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   16800000,  16800000,  512},
        /* 32 */ {    216000,  5120,  7864320,   24000000,  24000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  24000000,  30000000,  512},
        /* 41 */ {    245760,  8192,  12582912,  60000000,  75000000,  512},
        /* 42 */ {    522240,  8704,  13369344,  60000000,  75000000,  512},
        /* 5  */ {    589824,  22080, 42393600,  162000000, 162000000, 512},
        /* 51 */ {    983040,  36864, 70778880,  288000000, 288000000, 512},
    },
    {
        // HIGH_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    96000,     262500,    64},
        /* 1b */ {    1485,    99,    152064,    192000,    525000,    64},
        /* 11 */ {    3000,    396,   345600,    288000,    750000,    128},
        /* 12 */ {    6000,    396,   912384,    576000,    1500000,   128},
        /* 13 */ {    11880,   396,   912384,    1152000,   3000000,   128},
        /* 2  */ {    11880,   396,   912384,    3000000,   3000000,   128},
        /* 21 */ {    19800,   792,   1824768,   6000000,   6000000,   256},
        /* 22 */ {    20250,   1620,  3110400,   6000000,   6000000,   256},
        /* 3  */ {    40500,   1620,  3110400,   15000000,  15000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   21000000,  21000000,  512},
        /* 32 */ {    216000,  5120,  7864320,   30000000,  30000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  30000000,  37500000,  512},
        /* 41 */ {    245760,  8192,  12582912,  75000000,  93750000,  512},
        /* 42 */ {    522240,  8704,  13369344,  75000000,  93750000,  512},
        /* 5  */ {    589824,  22080, 42393600,  202500000, 202500000, 512},
        /* 51 */ {    983040,  36864, 70778880,  360000000, 360000000, 512},
    },
    {
        // HIGH10_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    230400,    630000,    64},
        /* 1b */ {    1485,    99,    152064,    460800,    1260000,   64},
        /* 11 */ {    3000,    396,   345600,    691200,    1800000,   128},
        /* 12 */ {    6000,    396,   912384,    1382400,   3600000,   128},
        /* 13 */ {    11880,   396,   912384,    2764800,   7200000,   128},
        /* 2  */ {    11880,   396,   912384,    7200000,   7200000,   128},
        /* 21 */ {    19800,   792,   1824768,   14400000,  14400000,  256},
        /* 22 */ {    20250,   1620,  3110400,   14400000,  14400000,  256},
        /* 3  */ {    40500,   1620,  3110400,   36000000,  36000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   50400000,  50400000,  512},
        /* 32 */ {    216000,  5120,  7864320,   72000000,  72000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  72000000,  90000000,  512},
        /* 41 */ {    245760,  8192,  12582912,  180000000, 225000000, 512},
        /* 42 */ {    522240,  8704,  13369344,  180000000, 225000000, 512},
        /* 5  */ {    589824,  22080, 42393600,  486000000, 486000000, 512},
        /* 51 */ {    983040,  36864, 70778880,  864000000, 864000000, 512},
    },
    {
        // HIGH422_PROFILE, HIGH444_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    307200,    840000,    64},
        /* 1b */ {    1485,    99,    152064,    614400,    1680000,   64},
        /* 11 */ {    3000,    396,   345600,    921600,    2400000,   128},
        /* 12 */ {    6000,    396,   912384,    1843200,   4800000,   128},
        /* 13 */ {    11880,   396,   912384,    3686400,   9600000,   128},
        /* 2  */ {    11880,   396,   912384,    9600000,   9600000,   128},
        /* 21 */ {    19800,   792,   1824768,   19200000,  19200000,  256},
        /* 22 */ {    20250,   1620,  3110400,   19200000,  19200000,  256},
        /* 3  */ {    40500,   1620,  3110400,   48000000,  48000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   67200000,  67200000,  512},
        /* 32 */ {    216000,  5120,  7864320,   96000000,  96000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  96000000,  120000000, 512},
        /* 41 */ {    245760,  8192,  12582912,  240000000, 300000000, 512},
        /* 42 */ {    522240,  8704,  13369344,  240000000, 300000000, 512},
        /* 5  */ {    589824,  22080, 42393600,  648000000, 648000000, 512},
        /* 51 */ {    983040,  36864, 70778880,  1152000000,1152000000,512},
    },
};

/*
static double QP2Qstep (int32_t QP)
{
  return (double)(0.85 * pow(2.0, (QP - 12.0) / 6.0));
}

static int32_t Qstep2QP (double Qstep)
{
  return (int32_t)(12.0 + 6.0 * log(Qstep/0.85) / log(2.0));
}
*/

Status SVCBRC::InitHRDLayer(int32_t tid)
{
  VideoBrcParams *pParams = &mParams[tid];
  int32_t profile_ind, level_ind;
  unsigned long long bufSizeBits = pParams->HRDBufferSizeBytes << 3;
  unsigned long long maxBitrate = pParams->maxBitrate;
  int32_t bitsPerFrame;

  bitsPerFrame = (int32_t)(pParams->targetBitrate / pParams->info.framerate);

  if (BRC_CBR == pParams->BRCMode)
    maxBitrate = pParams->maxBitrate = pParams->targetBitrate;
  if (maxBitrate < (unsigned long long)pParams->targetBitrate)
    maxBitrate = pParams->maxBitrate = 0;

  if (bufSizeBits > 0 && bufSizeBits < static_cast<unsigned long long>(bitsPerFrame << 1))
    bufSizeBits = (bitsPerFrame << 1);

  profile_ind = ConvertProfileToTable(pParams->profile);
  level_ind = ConvertLevelToTable(pParams->level);

  if (level_ind > H264_LIMIT_TABLE_LEVEL_51) // just in case svc brc is called with mvc level
      level_ind = H264_LIMIT_TABLE_LEVEL_51;

  if (pParams->targetBitrate > (int32_t)LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_BR])
    pParams->targetBitrate = (int32_t)LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_BR];
  if (static_cast<unsigned long long>(pParams->maxBitrate) > LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_BR])
    maxBitrate = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_BR];
  if (bufSizeBits > LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_CPB])
    bufSizeBits = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_CPB];

  if (pParams->maxBitrate <= 0 && pParams->HRDBufferSizeBytes <= 0) {
    if (profile_ind < 0) {
      profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
      level_ind = H264_LIMIT_TABLE_LEVEL_51;
    } else if (level_ind < 0)
      level_ind = H264_LIMIT_TABLE_LEVEL_51;
    maxBitrate = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR];
    bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];
  } else if (pParams->HRDBufferSizeBytes <= 0) {
    if (profile_ind < 0)
      bufSizeBits = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_CPB];
    else if (level_ind < 0) {
      for (; profile_ind < H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++)
        if (maxBitrate <= LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_BR])
          break;
      bufSizeBits = LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_CPB];
    } else {
      for (; profile_ind <= H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++) {
        for (; level_ind <= H264_LIMIT_TABLE_LEVEL_51; level_ind++) {
          if (maxBitrate <= LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR])
            break;
        }
        if (level_ind <= H264_LIMIT_TABLE_LEVEL_51)
          break;
        level_ind = 0;
      }
      if (profile_ind > H264_LIMIT_TABLE_HIGH_PROFILE) {
        profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
        level_ind = H264_LIMIT_TABLE_LEVEL_51;
      }
      bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];
    }
  } else if (pParams->maxBitrate <= 0) {
    if (profile_ind < 0)
      maxBitrate = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_BR];
    else if (level_ind < 0) {
      for (; profile_ind < H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++)
        if (bufSizeBits <= LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_CPB])
          break;
      maxBitrate = LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_51][H264_LIMIT_TABLE_MAX_BR];
    } else {
      for (; profile_ind <= H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++) {
        for (; level_ind <= H264_LIMIT_TABLE_LEVEL_51; level_ind++) {
          if (bufSizeBits <= LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB])
            break;
        }
        if (level_ind <= H264_LIMIT_TABLE_LEVEL_51)
          break;
        level_ind = 0;
      }
      if (profile_ind > H264_LIMIT_TABLE_HIGH_PROFILE) {
        profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
        level_ind = H264_LIMIT_TABLE_LEVEL_51;
      }
      maxBitrate = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR];
    }
  }

  if (maxBitrate < (unsigned long long)pParams->targetBitrate) {
    maxBitrate = (unsigned long long)pParams->targetBitrate;
    for (; profile_ind <= H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++) {
      for (; level_ind <= H264_LIMIT_TABLE_LEVEL_51; level_ind++) {
        if (maxBitrate <= LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR])
          break;
      }
      if (level_ind <= H264_LIMIT_TABLE_LEVEL_51)
        break;
      level_ind = 0;
    }
    if (profile_ind > H264_LIMIT_TABLE_HIGH_PROFILE) {
      profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
      level_ind = H264_LIMIT_TABLE_LEVEL_51;
    }
    bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];
  }
  pParams->HRDBufferSizeBytes = (int32_t)(bufSizeBits >> 3);
  pParams->maxBitrate = (int32_t)((maxBitrate >> 6) << 6);  // In H.264 HRD params bitrate is coded as value*2^(6+scale), we assume scale=0
  mHRD[tid].maxBitrate = pParams->maxBitrate;
  mHRD[tid].inputBitsPerFrame = mHRD[tid].maxInputBitsPerFrame = mHRD[tid].maxBitrate / pParams->info.framerate;

  pParams->HRDBufferSizeBytes = (pParams->HRDBufferSizeBytes >> 4) << 4; // coded in bits as value*2^(4+scale), assume scale<=3
  if (pParams->HRDInitialDelayBytes <= 0)
    pParams->HRDInitialDelayBytes = (BRC_CBR == pParams->BRCMode ? pParams->HRDBufferSizeBytes/2 : pParams->HRDBufferSizeBytes);
  else if (pParams->HRDInitialDelayBytes * 8 < bitsPerFrame)
    pParams->HRDInitialDelayBytes = bitsPerFrame / 8;
  if (pParams->HRDInitialDelayBytes > pParams->HRDBufferSizeBytes)
    pParams->HRDInitialDelayBytes = pParams->HRDBufferSizeBytes;
  mHRD[tid].bufSize = pParams->HRDBufferSizeBytes * 8;
  mHRD[tid].bufFullness = pParams->HRDInitialDelayBytes * 8;
  mHRD[tid].frameNum = 0;

  mHRD[tid].maxFrameSize = (int32_t)(mHRD[tid].bufFullness - 1);
  mHRD[tid].minFrameSize = (mParams[tid].BRCMode == BRC_VBR ? 0 : (int32_t)(mHRD[tid].bufFullness + 1 + 1 + mHRD[tid].inputBitsPerFrame - mHRD[tid].bufSize));

  mHRD[tid].minBufFullness = 0;
  mHRD[tid].maxBufFullness = mHRD[tid].bufSize - mHRD[tid].inputBitsPerFrame;

  return UMC_OK;
}

#define SVCBRC_BUFSIZE2BPF_RATIO 30
#define SVCBRC_BPF2BUFSIZE_RATIO (1./SVCBRC_BUFSIZE2BPF_RATIO)

int32_t SVCBRC::GetInitQP()
{
    const double x0 = 0, y0 = 1.19, x1 = 1.75, y1 = 1.75;
    int32_t fsLuma;
    int32_t i;
    int32_t bpfSum, bpfAv, bpfMin, bpfTarget;
    int32_t bpfMin_ind;
    double bs2bpfMin, bf2bpfMin;

    fsLuma = mParams[0].info.clip_info.width * mParams[0].info.clip_info.height;

    bpfMin = bpfSum = mBRC[mNumTemporalLayers-1].mBitsDesiredFrame;
    bpfMin_ind  = mNumTemporalLayers-1;

    bs2bpfMin = bf2bpfMin = (double)MFX_MAX_32S;

    for (i = mNumTemporalLayers - 1; i >= 0; i--) {
        if (mHRD[i].bufSize > 0) {
            double bs2bpf = (double)mHRD[i].bufSize / mBRC[i].mBitsDesiredFrame;
            if (bs2bpf < bs2bpfMin) {
                bs2bpfMin = bs2bpf;
            }
        }
    }

    for (i = mNumTemporalLayers - 2; i >= 0; i--) {
        bpfSum += mBRC[i].mBitsDesiredFrame;
        if (mBRC[i].mBitsDesiredFrame < bpfMin) {
            bpfMin = mBRC[i].mBitsDesiredFrame;
            bpfMin_ind = i;
        }
        mBRC[i].mQuantI = mBRC[i].mQuantP = (int32_t)(1. / 1.2 * pow(10.0, (log10((double)fsLuma / mBRC[i].mBitsDesiredFrame) - x0) * (y1 - y0) / (x1 - x0) + y0) + 0.5);
    }
    bpfAv = bpfSum / mNumTemporalLayers;

    if (bpfMin_ind == mNumTemporalLayers - 1) {
        bpfTarget = bpfMin;
        for (i = mNumTemporalLayers - 2; i >= 0; i--) {
            if (mHRD[i].bufSize > 0) {
                if (bpfTarget < mHRD[i].minFrameSize)
                    bpfTarget = mHRD[i].minFrameSize;
            }
        }
    } else {
        double bpfTarget0, bpfTarget1, weight;
        bpfTarget0 = (3*mBRC[mNumTemporalLayers - 1].mBitsDesiredFrame + bpfAv) >> 2;
        if (bs2bpfMin > SVCBRC_BUFSIZE2BPF_RATIO) {
            bpfTarget = (int32_t)bpfTarget0;
        } else {
            if (mHRD[mNumTemporalLayers - 1].bufSize > 0) {
                double bs2bpfN_1 = mHRD[mNumTemporalLayers - 1].bufSize / mBRC[mNumTemporalLayers - 1].mBitsDesiredFrame;
                weight = bs2bpfMin / bs2bpfN_1;
                bpfTarget1 = weight * mBRC[mNumTemporalLayers - 1].mBitsDesiredFrame + (1 - weight) * mBRC[bpfMin_ind].mBitsDesiredFrame;
            } else {
                bpfTarget1 = mBRC[bpfMin_ind].mBitsDesiredFrame;
            }
            weight = bs2bpfMin / SVCBRC_BUFSIZE2BPF_RATIO;
            bpfTarget =  (int32_t)(weight * bpfTarget0  + (1 - weight) * bpfTarget1);
        }
    }


    for (i = mNumTemporalLayers - 1; i >= 0; i--) {
        if (mHRD[i].bufSize > 0) {
            if (bpfTarget > mHRD[i].maxFrameSize)
                bpfTarget = mHRD[i].maxFrameSize;
            if (bpfTarget < mHRD[i].minFrameSize)
                bpfTarget = mHRD[i].minFrameSize;
        }
    }

    mRCfa = bpfTarget;
  
    int32_t q = (int32_t)(1. / 1.2 * pow(10.0, (log10((double)fsLuma / bpfTarget) - x0) * (y1 - y0) / (x1 - x0) + y0) + 0.5);
    BRC_CLIP(q, 1, mQuantMax);

    for (i = mNumTemporalLayers - 1; i >= 0; i--) {
        mBRC[i].mRCq = mBRC[i].mQuant = mBRC[i].mQuantI = mBRC[i].mQuantP = mBRC[i].mQuantB = mBRC[i].mQuantPrev = q;
        mBRC[i].mRCqa = mBRC[i].mRCqa0 = 1. / (double)mBRC[i].mRCq;
        mBRC[i].mRCfa = mBRC[i].mBitsDesiredFrame; //??? bpfTarget ?
    }

    return q;
}


Status SVCBRC::InitLayer(VideoBrcParams *brcParams, int32_t tid)
{
    Status status = UMC_OK;
    if (!brcParams)
        return UMC_ERR_NULL_PTR;
    if (brcParams->targetBitrate <= 0 || brcParams->info.framerate <= 0)
        return UMC_ERR_INVALID_PARAMS;

    VideoBrcParams  *pmParams = &mParams[tid];
    *pmParams = *brcParams;

    if (pmParams->frameRateExtN && pmParams->frameRateExtD)
        pmParams->info.framerate = (double)pmParams->frameRateExtN /  pmParams->frameRateExtD;

    mIsInit = true;
    return status;
}


Status SVCBRC::Init(BaseCodecParams *params, int32_t numTemporalLayers)
{
  VideoBrcParams *brcParams = DynamicCast<VideoBrcParams>(params);
  Status status = UMC_OK;
  int32_t i;
  if (!brcParams)
      return UMC_ERR_INVALID_PARAMS;
  
  if (numTemporalLayers > MAX_TEMP_LEVELS)
      return UMC_ERR_INVALID_PARAMS;

  mNumTemporalLayers = numTemporalLayers;

  for (i = 0; i < numTemporalLayers; i++) {
      status = InitLayer(brcParams + i, i);
      if (status != UMC_OK)
          return status;
  }

  for (i = 0; i < numTemporalLayers; i++) {
      if (mParams[i].HRDBufferSizeBytes != 0) {
          status = InitHRDLayer(i);
          if (status != UMC_OK)
              return status;
//          mHRD[i].mBF = (long long)mParams[i].HRDInitialDelayBytes * mParams[i].frameRateExtN;
//          mHRD[i].mBFsaved = mHRD[i].mBF;
      } else {
          mHRD[i].bufSize = 0;
      }

      mBRC[i].mBitsDesiredTotal = mBRC[i].mBitsEncodedTotal = 0;
      mBRC[i].mQuantUpdated = 1;
      mBRC[i].mBitsDesiredFrame = (int32_t)((double)mParams[i].targetBitrate / mParams[i].info.framerate);

/*
      level_ind = ConvertLevelToTable(mParams[i].level);
      if (level_ind < 0)
          return UMC_ERR_INVALID_PARAMS;

      if (level_ind >= H264_LIMIT_TABLE_LEVEL_31 && level_ind <= H264_LIMIT_TABLE_LEVEL_4)
          bitsPerMB = 96.; // 384 / minCR; minCR = 4
      else
          bitsPerMB = 192.; // minCR = 2

      maxMBPS = (int32_t)LevelProfileLimits[0][level_ind][H264_LIMIT_TABLE_MAX_MBPS];
      numMBPerFrame = ((mParams[i].info.clip_info.width + 15) >> 4) * ((mParams.info.clip_info.height + 15) >> 4);
      // numMBPerFrame should include ref layers as well - see G10.2.1 !!! ???

      tmpf = (double)numMBPerFrame;
      if (tmpf < maxMBPS / 172.)
          tmpf = maxMBPS / 172.;

      mMaxBitsPerPic = (unsigned long long)(tmpf * bitsPerMB) * 8;
      mMaxBitsPerPicNot0 = (unsigned long long)((double)maxMBPS / mFramerate * bitsPerMB) * 8;
*/

  }

  mQuantUpdated = 1;
  mQuantMax = 51;
  mQuantUnderflow = -1;
  mQuantOverflow = mQuantMax + 1;

  int32_t q = GetInitQP();

  mRCqap = 100;
  mRCfap = 100;
  mRCbap = 100;
  mQuant = mRCq = q;
  mRCqa = mRCqa0 = 1. / (double)mRCq;

  for (i = 0; i < mNumTemporalLayers; i++) {
      mBRC[i].mRCqap = mRCqap;
      mBRC[i].mRCfap = mRCfap;
      mBRC[i].mRCbap = mRCbap;
  }


  mPictureFlags = mPictureFlagsPrev = BRC_FRAME;
  mIsInit = true;
  mRecodeInternal = 0;

  return status;
}

Status SVCBRC::Reset(BaseCodecParams *params,  int32_t numTemporalLayers)
{
  VideoBrcParams *brcParams = DynamicCast<VideoBrcParams>(params);
//  VideoBrcParams tmpParams;

  if (NULL == brcParams)
    return UMC_ERR_NULL_PTR;
  else // tmp !!!
    return Init(params, numTemporalLayers);

}


Status SVCBRC::Close()
{
  Status status = UMC_OK;
  if (!mIsInit)
    return UMC_ERR_NOT_INITIALIZED;
  mIsInit = false;
  return status;
}


Status SVCBRC::SetParams(BaseCodecParams* params, int32_t tid)
{
    return Init(params, tid);
}

Status SVCBRC::GetParams(BaseCodecParams* params, int32_t tid)
{
    VideoBrcParams *brcParams = DynamicCast<VideoBrcParams>(params);
    VideoEncoderParams *videoParams = DynamicCast<VideoEncoderParams>(params);


    // ??? 
    if (tid < 0 || tid >= mNumTemporalLayers) {
        for (tid = 0; tid < mNumTemporalLayers; tid++) {
            if (NULL != brcParams) {
                brcParams[tid] = mParams[tid];
            } else if (NULL != videoParams) {
                params[tid] = *(VideoEncoderParams*)&(mParams[tid]);
            } else {
                params[tid] = *(BaseCodecParams*)&(mParams[tid]);
            }
        }
        return UMC_OK;
    }

    if (NULL != brcParams) {
        *brcParams = mParams[tid];
    } else if (NULL != videoParams) {
        *params = *(VideoEncoderParams*)&(mParams[tid]);
    } else {
        *params = *(BaseCodecParams*)&(mParams[tid]);
    }
    return UMC_OK;
};


/*

Status SVCBRC::SetParams(BaseCodecParams* params)
{
  return Init(params);
}
*/

Status SVCBRC::SetPictureFlags(FrameType, int32_t picture_structure, int32_t, int32_t, int32_t)
{
  switch (picture_structure & PS_FRAME) {
  case (PS_TOP_FIELD):
    mPictureFlags = BRC_TOP_FIELD;
    break;
  case (PS_BOTTOM_FIELD):
    mPictureFlags = BRC_BOTTOM_FIELD;
    break;
  case (PS_FRAME):
  default:
    mPictureFlags = BRC_FRAME;
  }
  return UMC_OK;
}

int32_t SVCBRC::GetQP(FrameType frameType, int32_t tid)
{
    if (tid < 0) {
        return mQuant;
    } else if (tid < mNumTemporalLayers)
        return ((frameType == I_PICTURE) ? mBRC[tid].mQuantI : (frameType == B_PICTURE) ? mBRC[tid].mQuantB : mBRC[tid].mQuantP);
    else
        return -1;
}

Status SVCBRC::SetQP(int32_t qp, FrameType frameType, int32_t tid)
{
    if (tid < 0)
        mQuant = qp;
    else if (tid < mNumTemporalLayers) {
        mBRC[tid].mQuant = qp;

        if (B_PICTURE == frameType) {
            mBRC[tid].mQuantB = qp;
            BRC_CLIP(mBRC[tid].mQuantB, 1, mQuantMax);
        } else {
            mBRC[tid].mRCq = qp;
            BRC_CLIP(mBRC[tid].mRCq, 1, mQuantMax);
            mBRC[tid].mQuantI = mBRC[tid].mQuantP = mRCq;
        }
    } else
        return UMC_ERR_INVALID_PARAMS;

    return UMC_OK;
}

#define BRC_SCENE_CHANGE_RATIO1 10.0
#define BRC_SCENE_CHANGE_RATIO2 5.0


#define BRC_QP_DELTA_WMAX 5
#define  BRC_QP_DELTA_WMIN 5

BRCStatus SVCBRC::PreEncFrame(FrameType picType, int32_t, int32_t tid)
{ 
    int32_t maxfs, minfs, maxfsi, minfsi;
    int32_t maxfsind, minfsind;
    int32_t i;
    int32_t qp, qpmax, qpmin, qpav, qpi, qpminind, qpmaxind, qptop;
    double fqpav;
    BRCStatus Sts = BRC_OK;
    int32_t cnt;

    mTid = tid;
    if (mHRD[tid].bufSize > 0) {
        maxfs = (int32_t)(mHRD[tid].bufFullness - mHRD[tid].minBufFullness);
        minfs = (int32_t)(mHRD[tid].inputBitsPerFrame - mHRD[tid].maxBufFullness + mHRD[tid].bufFullness);
        maxfsind = minfsind = tid;
        qpmaxind = qpminind = tid;
        qpav = qpmax = qpmin = (picType == I_PICTURE) ? mBRC[tid].mQuantI : (picType == B_PICTURE) ? mBRC[tid].mQuantB : mBRC[tid].mQuantP;
        cnt = 1;
    } else {
        maxfs = MFX_MAX_32S;
        minfs = 0;
        maxfsind = minfsind = mNumTemporalLayers-1;
        cnt = 0;
        qpav = (picType == I_PICTURE) ? mBRC[mNumTemporalLayers-1].mQuantI : (picType == B_PICTURE) ? mBRC[mNumTemporalLayers-1].mQuantB : mBRC[mNumTemporalLayers-1].mQuantP;
        qpmax = 101;
        qpmin = -1;
        qpmaxind = qpminind = tid;
    }

    for (i = tid + 1; i < mNumTemporalLayers; i++) {

        if (mHRD[i].bufSize > 0) {

            maxfsi = (int32_t)(mHRD[i].bufFullness - mHRD[i].minBufFullness);
            minfsi = (int32_t)(mHRD[i].inputBitsPerFrame - mHRD[i].maxBufFullness + mHRD[i].bufFullness);
            qpi = (picType == I_PICTURE) ? mBRC[i].mQuantI : (picType == B_PICTURE) ? mBRC[i].mQuantB : mBRC[i].mQuantP;

            if (maxfsi <= maxfs) {
                maxfs = maxfsi;
                maxfsind = i;
            }
            if (minfsi >= minfs) {
                minfs = minfsi;
                minfsind = i;
            }
            if (qpi >= qpmax) {
                qpmax = qpi;
                qpmaxind = i;
            }
            if (qpi <= qpmin) {
                qpmin = qpi;
                qpminind = i;
            }
            qpav += qpi;
            cnt++;
        }
    }

    if (cnt == 0) {
        mQuant = (picType == I_PICTURE) ? mBRC[mNumTemporalLayers-1].mQuantI : (picType == B_PICTURE) ? mBRC[mNumTemporalLayers-1].mQuantB : mBRC[mNumTemporalLayers-1].mQuantP;
        mMaxFrameSize = MFX_MAX_32S;
        mMinFrameSize = 0;
        return Sts;
    }

    fqpav = (double)qpav / cnt;
    qptop = picType == I_PICTURE ? mBRC[mNumTemporalLayers-1].mQuantI : (picType == B_PICTURE ? mBRC[mNumTemporalLayers-1].mQuantB : mBRC[mNumTemporalLayers-1].mQuantP);
    qp = (qptop * 3 + (int32_t)fqpav + 3) >> 2; // ???

    for (i = mNumTemporalLayers - 2; i >= tid; i--) {
        if (mHRD[i].bufSize > 0) {
            maxfsi = (int32_t)(mHRD[i].bufFullness - mHRD[i].minBufFullness);
            minfsi = (int32_t)(mHRD[i].inputBitsPerFrame - mHRD[i].maxBufFullness + mHRD[i].bufFullness);
            qpi = (picType == I_PICTURE) ? mBRC[i].mQuantI : (picType == B_PICTURE) ? mBRC[i].mQuantB : mBRC[i].mQuantP;

            if (qpi > qp) {
                if (maxfsi < mHRD[i].inputBitsPerFrame)
                    qp = qpi;
                else if (maxfsi < 2*mHRD[i].inputBitsPerFrame)
                    qp = (qp + qpi + 1) >> 1;

            }
            if (qpi < qp) {
                if (minfsi > (double)(mHRD[i].inputBitsPerFrame * 0.5))
                    qp = qpi;
                else if (minfsi > (double)(mHRD[i].inputBitsPerFrame * 0.25))
                    qp = (qp + qpi) >> 1;
            }
        }
    }

    int32_t qpt = (picType == I_PICTURE) ? mBRC[maxfsind].mQuantI : (picType == B_PICTURE) ? mBRC[maxfsind].mQuantB : mBRC[maxfsind].mQuantP;
    if (qp < qpt) {
        if (maxfs < 4*mHRD[maxfsind].inputBitsPerFrame)
            qp = (qp + qpt + 1) >> 1;
    }
    qpt = (picType == I_PICTURE) ? mBRC[minfsind].mQuantI : (picType == B_PICTURE) ? mBRC[minfsind].mQuantB : mBRC[minfsind].mQuantP;
    if (qp > qpt) {
        if (minfs > 0) // ???
            qp = (qp + qpt) >> 1;
    }
    
    if (qp < qpmax - BRC_QP_DELTA_WMAX) {
        if ((int32_t)(mHRD[qpmaxind].bufFullness - mHRD[qpmaxind].minBufFullness) < 4*mHRD[qpmaxind].inputBitsPerFrame)
            qp = (qp + qpmax + 1) >> 1;
    }

    if (qp > qpmin + BRC_QP_DELTA_WMIN) {
        if ((mHRD[qpminind].maxBufFullness  - (int32_t)mHRD[qpminind].bufFullness) < 2*mHRD[qpminind].inputBitsPerFrame)
            qp = (qp + qpmin + 1) >> 1;
    }

    mQuant = qp;
    mMaxFrameSize = maxfs;
    mMinFrameSize = minfs;

    return Sts;
}

BRCStatus SVCBRC::UpdateAndCheckHRD(int32_t tid, int32_t frameBits, int32_t payloadbits, int32_t recode)
{
    BRCStatus ret = BRC_OK;
    BRCSVC_HRDState *pHRD = &mHRD[tid];

    if (!(recode & (BRC_EXT_FRAMESKIP - 1))) { // BRC_EXT_FRAMESKIP == 16
        pHRD->prevBufFullness = pHRD->bufFullness;
    } else { // frame is being recoded - restore buffer state
        pHRD->bufFullness = pHRD->prevBufFullness;
    }

    pHRD->maxFrameSize = (int32_t)(pHRD->bufFullness - 1);
    pHRD->minFrameSize = (mParams[tid].BRCMode == BRC_VBR ? 0 : (int32_t)(pHRD->bufFullness + 2 + pHRD->inputBitsPerFrame - pHRD->bufSize));
    if (pHRD->minFrameSize < 0)
        pHRD->minFrameSize = 0;

    if (frameBits > pHRD->maxFrameSize) {
        ret = BRC_ERR_BIG_FRAME;
    } else if (frameBits < pHRD->minFrameSize) {
        ret = BRC_ERR_SMALL_FRAME;
    } else {
         pHRD->bufFullness += pHRD->inputBitsPerFrame - frameBits;
    }

    if (BRC_OK != ret) {
        if ((recode & BRC_EXT_FRAMESKIP) || BRC_RECODE_EXT_PANIC == recode || BRC_RECODE_PANIC == recode) // no use in changing QP
            ret |= BRC_NOT_ENOUGH_BUFFER;
        else {
            ret = UpdateQuantHRD(tid, frameBits, ret, payloadbits);
        }
        mBRC[tid].mQuantUpdated = 0;
    }

    return ret;
}

Status SVCBRC::GetMinMaxFrameSize(int32_t *minFrameSizeInBits, int32_t *maxFrameSizeInBits) {
    int32_t i, minfs = 0, maxfs = MFX_MAX_32S;
    for (i = mTid; i < mNumTemporalLayers; i++) {
        if (mHRD[i].bufSize > 0) {
            if (mHRD[i].minFrameSize > minfs)
                minfs = mHRD[i].minFrameSize;
            if (mHRD[i].maxFrameSize < maxfs)
                maxfs = mHRD[i].maxFrameSize;
        }
    }
    if (minFrameSizeInBits)
        *minFrameSizeInBits = minfs;
    if (maxFrameSizeInBits)
        *maxFrameSizeInBits = maxfs;
    return UMC_OK;
};


BRCStatus SVCBRC::PostPackFrame(FrameType picType, int32_t totalFrameBits, int32_t payloadBits, int32_t repack, int32_t /* poc */)
{
  BRCStatus Sts = BRC_OK;
  int32_t bitsEncoded = totalFrameBits - payloadBits;

  mBitsEncoded = bitsEncoded;

  mOversize = bitsEncoded - mMaxFrameSize;
  mUndersize = mMinFrameSize - bitsEncoded;

  if (mOversize > 0)
      Sts |= BRC_BIG_FRAME;
  if (mUndersize > 0)
      Sts |= BRC_SMALL_FRAME;

  if (Sts & BRC_BIG_FRAME) {
      if (mQuant < mQuantUnderflow - 1) {
          mQuant++;
          mQuantUpdated = 0;
          return BRC_ERR_BIG_FRAME; // ???
      }
  } else if (Sts & BRC_SMALL_FRAME) {
      if (mQuant > mQuantOverflow + 1) {
          mQuant--;
          mQuantUpdated = 0;
          return BRC_ERR_SMALL_FRAME;
      }
  }

  Sts = BRC_OK;
  int32_t quant = mQuant;
  int32_t i;
  int32_t tid = mTid;

  for (i = tid; i < mNumTemporalLayers; i++) {
      if (mHRD[i].bufSize > 0) {
          BRCStatus sts = UpdateAndCheckHRD(i, totalFrameBits, payloadBits, repack);
          Sts |= sts;
          if ((sts & BRC_ERR_BIG_FRAME) && (mBRC[i].mQuant > quant))
                quant = mBRC[i].mQuant;
          else if ((sts & BRC_ERR_SMALL_FRAME) && (mBRC[i].mQuant < quant))
                quant = mBRC[i].mQuant;
      }
  }

  if (Sts != BRC_OK) {
      if (((Sts & BRC_ERR_BIG_FRAME) && (Sts & BRC_ERR_SMALL_FRAME)) || (Sts & BRC_NOT_ENOUGH_BUFFER)) {
          return BRC_NOT_ENOUGH_BUFFER;
      }
      if (Sts & BRC_ERR_BIG_FRAME)
          mQuantUnderflow = mQuant;
      if (Sts & BRC_ERR_SMALL_FRAME)
          mQuantOverflow = mQuant;

      if (quant != mQuant)
          mQuant = quant;
      else
          mQuant = quant + ((Sts & BRC_ERR_BIG_FRAME) ? 1 : -1);

      mQuantUpdated = 0;
      return Sts;
  }


  int32_t j;
  if (mHRD[tid].bufSize > 0) {
      mHRD[tid].minBufFullness = 0;
      mHRD[tid].maxBufFullness = mHRD[tid].bufSize - mHRD[tid].inputBitsPerFrame;
  }

  for (i = tid; i < mNumTemporalLayers; i++) {
      if (mHRD[i].bufSize > 0) {
          mHRD[i].minFrameSize = (mParams[i].BRCMode == BRC_VBR ? 0 : (int32_t)(mHRD[i].bufFullness + 1 + 1 + mHRD[i].inputBitsPerFrame - mHRD[i].bufSize));

          for (j = i + 1; j < mNumTemporalLayers; j++) {
              if (mHRD[j].bufSize > 0) {
                  double minbf, maxbf;
                  minbf = mHRD[i].minFrameSize - mHRD[j].inputBitsPerFrame;
                  maxbf = mHRD[j].bufSize - mHRD[j].inputBitsPerFrame + mHRD[i].bufFullness - mHRD[j].inputBitsPerFrame;

                  if (minbf > mHRD[j].minBufFullness)
                      mHRD[j].minBufFullness = minbf;

                  if (maxbf < mHRD[j].maxBufFullness)
                      mHRD[j].maxBufFullness = maxbf;
              }
          }
      }
  }

  for (i = tid; i < mNumTemporalLayers; i++) {
      if (mHRD[i].bufSize > 0) {
          if (mHRD[i].bufFullness - mHRD[i].inputBitsPerFrame < mHRD[i].minBufFullness)
              Sts |= BRC_BIG_FRAME;
          if (mHRD[i].bufFullness - mHRD[i].inputBitsPerFrame > mHRD[i].maxBufFullness)
              Sts |= BRC_SMALL_FRAME;
      }
  }

  if ((Sts & BRC_BIG_FRAME) && (Sts & BRC_SMALL_FRAME))
      Sts = BRC_OK; // don't do anything, everything is already bad

  if (Sts & BRC_BIG_FRAME) {
      if (mQuant < mQuantUnderflow - 1) {
          mQuant++;
          mQuantUpdated = 0;
          return BRC_ERR_BIG_FRAME; // ???
      }
  } else if (Sts & BRC_SMALL_FRAME) {
      if (mQuant > mQuantOverflow + 1) {
          mQuant--;
          mQuantUpdated = 0;
          return BRC_ERR_SMALL_FRAME;
      }
  }
  Sts = BRC_OK;

  mQuantOverflow = mQuantMax + 1;
  mQuantUnderflow = -1;

  mFrameType = picType;

  {
    if (repack != BRC_RECODE_PANIC && repack != BRC_RECODE_EXT_PANIC) {

      for (i = mNumTemporalLayers - 1; i >= tid; i--) {
          UpdateQuant(i, bitsEncoded, totalFrameBits);
      }

      mQuantP = mQuantI = mRCq = mQuant; // ???
    }
    mQuantUpdated = 1;
  }

  return Sts;
};

BRCStatus SVCBRC::UpdateQuant(int32_t tid, int32_t bEncoded, int32_t totalPicBits)
{
  BRCStatus Sts = BRC_OK;
  double  bo, qs, dq;
  int32_t  quant;
  int32_t isfield = ((mPictureFlags & BRC_FRAME) != BRC_FRAME) ? 1 : 0;
  long long totalBitsDeviation;
  BRC_SVCLayer_State *pBRC = &mBRC[tid];
  uint32_t bitsPerPic = (uint32_t)pBRC->mBitsDesiredFrame >> isfield;

  if (isfield)
    pBRC->mRCfa *= 0.5;

  quant = mQuant;
  if (mQuantUpdated)
      pBRC->mRCqa += (1. / quant - pBRC->mRCqa) / mRCqap;
  else {
      if (pBRC->mQuantUpdated)
          pBRC->mRCqa += (1. / quant - pBRC->mRCqa) / (mRCqap > 25 ? 25 : mRCqap);
      else
          pBRC->mRCqa += (1. / quant - pBRC->mRCqa) / (mRCqap > 10 ? 10 : mRCqap);
  }

  if (mRecodeInternal) {
    pBRC->mRCfa = bitsPerPic;
    pBRC->mRCqa = mRCqa0;
  }
  mRecodeInternal = 0;

  pBRC->mBitsEncodedTotal += totalPicBits;
  pBRC->mBitsDesiredTotal += bitsPerPic;
  totalBitsDeviation = pBRC->mBitsEncodedTotal - pBRC->mBitsDesiredTotal;
  
  if (mFrameType != I_PICTURE || mParams[tid].BRCMode == BRC_CBR || mQuantUpdated == 0)
    pBRC->mRCfa += (bEncoded - pBRC->mRCfa) / pBRC->mRCfap;

  pBRC->mQuantB = ((pBRC->mQuantP + pBRC->mQuantPrev) * 563 >> 10) + 1; \
  BRC_CLIP(pBRC->mQuantB, 1, mQuantMax);

  if (pBRC->mQuantUpdated == 0)
    if (pBRC->mQuantB < quant)
      pBRC->mQuantB = quant;
  qs = pow(bitsPerPic / pBRC->mRCfa, 2.0);
  dq = pBRC->mRCqa * qs;

  bo = (double)totalBitsDeviation / pBRC->mRCbap / pBRC->mBitsDesiredFrame; // ??? bitsPerPic ?
  BRC_CLIP(bo, -1.0, 1.0);

  dq = dq + (1./mQuantMax - dq) * bo;
  BRC_CLIP(dq, 1./mQuantMax, 1./1.);
  quant = (int) (1. / dq + 0.5);


  if (quant >= pBRC->mRCq + 5)
    quant = pBRC->mRCq + 3;
  else if (quant >= pBRC->mRCq + 3)
    quant = pBRC->mRCq + 2;
  else if (quant > pBRC->mRCq + 1)
    quant = pBRC->mRCq + 1;
  else if (quant <= pBRC->mRCq - 5)
    quant = pBRC->mRCq - 3;
  else if (quant <= pBRC->mRCq - 3)
    quant = pBRC->mRCq - 2;
  else if (quant < pBRC->mRCq - 1)
    quant = pBRC->mRCq - 1;

  pBRC->mRCq = quant;

  if (isfield)
    pBRC->mRCfa *= 2;


  if (mFrameType != B_PICTURE) {
      pBRC->mQuantPrev = pBRC->mQuantP;
      pBRC->mBitsEncodedP = mBitsEncoded;
  }

  pBRC->mQuant = pBRC->mQuantP = pBRC->mQuantI =  pBRC->mRCq;
  pBRC->mQuantUpdated = 1;

  return Sts;
}


BRCStatus SVCBRC::UpdateQuantHRD(int32_t tid, int32_t totalFrameBits, BRCStatus sts, int32_t payloadBits)
{
  int32_t quant, quant_prev;
  double qs;
  BRCSVC_HRDState *pHRD = &mHRD[tid];
  int32_t wantedBits = (sts == BRC_ERR_BIG_FRAME ? pHRD->maxFrameSize : pHRD->minFrameSize);
  int32_t bEncoded = totalFrameBits - payloadBits;

  wantedBits -= payloadBits;
  if (wantedBits <= 0) // possible only if BRC_ERR_BIG_FRAME
    return (sts | BRC_NOT_ENOUGH_BUFFER);

  quant_prev = quant = mQuant;
  if (sts & BRC_ERR_BIG_FRAME)
      mQuantUnderflow = quant;
  else if (sts & BRC_ERR_SMALL_FRAME)
      mQuantOverflow = quant;

  qs = pow((double)bEncoded/wantedBits, 1.5);
  quant = (int32_t)(quant * qs + 0.5);

  if (quant == quant_prev)
    quant += (sts == BRC_ERR_BIG_FRAME ? 1 : -1);

  BRC_CLIP(quant, 1, mQuantMax);

  if (quant < quant_prev) {
      while (quant <= mQuantUnderflow)
          quant++;
  } else {
      while (quant >= mQuantOverflow)
          quant--;
  }

  if (quant == quant_prev)
    return (sts | BRC_NOT_ENOUGH_BUFFER);

  mBRC[tid].mQuant = quant;

  switch (mFrameType) {
  case (I_PICTURE):
    mBRC[tid].mQuantI = quant;
    break;
  case (B_PICTURE):
    mBRC[tid].mQuantB = quant;
    break;
  case (P_PICTURE):
  default:
    mBRC[tid].mQuantP = quant;
  }
  return sts;
}

Status SVCBRC::GetHRDBufferFullness(double *hrdBufFullness, int32_t recode, int32_t tid)
{
    *hrdBufFullness = (recode & (BRC_EXT_FRAMESKIP - 1)) ? mHRD[tid].prevBufFullness : mHRD[tid].bufFullness;

    return UMC_OK;
}

/*
Status SVCBRC::GetInitialCPBRemovalDelay(int32_t tid, uint32_t *initial_cpb_removal_delay, int32_t recode)
{
  uint32_t cpb_rem_del_u32;
  unsigned long long cpb_rem_del_u64, temp1_u64, temp2_u64;

  if (BRC_VBR == mRCMode) {
    if (recode)
      mBF = mBFsaved;
    else
      mBFsaved = mBF;
  }

  temp1_u64 = (unsigned long long)mBF * 90000;
  temp2_u64 = (unsigned long long)mMaxBitrate * mParams.frameRateExtN;
  cpb_rem_del_u64 = temp1_u64 / temp2_u64;
  cpb_rem_del_u32 = (uint32_t)cpb_rem_del_u64;

  if (BRC_VBR == mRCMode) {
    mBF = temp2_u64 * cpb_rem_del_u32 / 90000;
    temp1_u64 = (unsigned long long)cpb_rem_del_u32 * mMaxBitrate;
    uint32_t dec_buf_ful = (uint32_t)(temp1_u64 / (90000/8));
    if (recode)
      mHRD.prevBufFullness = (double)dec_buf_ful;
    else
      mHRD.bufFullness = (double)dec_buf_ful;
  }

  *initial_cpb_removal_delay = cpb_rem_del_u32;
  return UMC_OK;
}
*/
}

#endif //defined(MFX_ENABLE_H264_VIDEO_ENCODE)
