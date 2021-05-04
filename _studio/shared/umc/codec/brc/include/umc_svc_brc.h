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

#include "umc_defs.h"

#ifndef _UMC_SVC_BRC_H_
#define _UMC_SVC_BRC_H_

#include "umc_h264_video_encoder.h"
#ifndef _UMC_VIDEO_BRC_H_
#include "umc_video_brc.h"
#endif

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE)

/*
typedef struct _BRCSVC_Params
{
} BRCSVC_Params;
*/

#define BRC_CLIP(a, l, r) if (a < (l)) a = l; else if (a > (r)) a = r
#define BRC_CLIPL(a, l) if (a < (l)) a = l
#define BRC_CLIPR(a, r) if (a > (r)) a = r
#define BRC_ABS(a) ((a) >= 0 ? (a) : -(a))

namespace UMC
{

typedef struct _BRCSVC_HRD_State
{
    uint32_t bufSize;
    double bufFullness;
    double prevBufFullness;
    double maxBitrate;
    double inputBitsPerFrame;
    double maxInputBitsPerFrame;
    double minBufFullness;
    double maxBufFullness;
    int32_t frameNum;
    int32_t minFrameSize;
    int32_t maxFrameSize;
    long long mBF;
    long long mBFsaved;
} BRCSVC_HRDState;

typedef struct _BRC_SVCLayer_State
{
    int32_t  mBitsDesiredFrame;
    long long  mBitsEncodedTotal, mBitsDesiredTotal;
    int32_t  mQuant, mQuantI, mQuantP, mQuantB, mQuantPrev, mQuantOffset, mQPprev;
    int32_t  mRCfap, mRCqap, mRCbap, mRCq;
    double  mRCqa, mRCfa, mRCqa0;
    int32_t  mQuantIprev, mQuantPprev, mQuantBprev;
    int32_t  mQuantUpdated;
    int32_t mBitsEncodedP, mBitsEncodedPrev;
} BRC_SVCLayer_State;


class SVCBRC : public VideoBrc {

public:

  SVCBRC();
  virtual ~SVCBRC();

  // Initialize with specified parameter(s)
  Status Init(BaseCodecParams *init, int32_t numTemporalLayers = 1);
  Status InitLayer(VideoBrcParams *params, int32_t tid);
  Status InitHRDLayer(int32_t tid);

  // Close all resources
  Status Close();

  Status Reset(BaseCodecParams *init, int32_t numTemporalLayers = 1);

  Status SetParams(BaseCodecParams* params, int32_t tid = 0);
  Status GetParams(BaseCodecParams* params, int32_t tid = 0);
  Status GetHRDBufferFullness(double *hrdBufFullness, int32_t recode = 0, int32_t tid = 0);
  Status PreEncFrame(FrameType frameType, int32_t recode = 0, int32_t tid = 0);
  BRCStatus PostPackFrame(FrameType frameType, int32_t bitsEncodedFrame, int32_t payloadBits = 0, int32_t recode = 0, int32_t poc = 0);
  BRCStatus UpdateAndCheckHRD(int32_t tid, int32_t bitsEncodedFrame, int32_t payloadBits, int32_t recode);

  int32_t GetQP(FrameType frameType, int32_t tid = -1);
  Status SetQP(int32_t qp, FrameType frameType, int32_t tid);

  Status SetPictureFlags(FrameType frameType, int32_t picture_structure, int32_t repeat_first_field = 0, int32_t top_field_first = 0, int32_t second_field = 0);

  Status GetMinMaxFrameSize(int32_t *minFrameSizeInBits, int32_t *maxFrameSizeInBits);

//  Status GetInitialCPBRemovalDelay(uint32_t *initial_cpb_removal_delay, int32_t tid, int32_t recode = 0);

protected:

  VideoBrcParams mParams[MAX_TEMP_LEVELS];
  bool   mIsInit;
  BRCSVC_HRDState mHRD[MAX_TEMP_LEVELS];
  BRC_SVCLayer_State mBRC[MAX_TEMP_LEVELS];
  int32_t mNumTemporalLayers;
/*
  long long  mBitsEncodedTotal, mBitsDesiredTotal;
  int32_t  mQuantI, mQuantP, mQuantB, mQuantMax, mQuantPrev, mQuantOffset, mQPprev;
  int32_t  mRCfap, mRCqap, mRCbap, mRCq;
  double  mRCqa, mRCfa, mRCqa0;
  int32_t  mQuantIprev, mQuantPprev, mQuantBprev;
*/
  int32_t mTid; // current frame temporal layer ID
  int32_t mRCMode;
  int32_t mQuant;
  int32_t mQuantI, mQuantP, mQuantB;
  FrameType mFrameType;
  int32_t  mBitsEncoded;
  BrcPictureFlags  mPictureFlags, mPictureFlagsPrev;
  int32_t mQuantUpdated;
  int32_t mQuantUnderflow;
  int32_t mQuantOverflow;

  int32_t mRecodeInternal;
  int32_t GetInitQP();
  BRCStatus UpdateQuant(int32_t tid, int32_t bEncoded, int32_t totalPicBits);
//  BRCStatus UpdateQuant_ScCh(int32_t bEncoded, int32_t totalPicBits);
  BRCStatus UpdateQuantHRD(int32_t tid, int32_t bEncoded, BRCStatus sts, int32_t payloadBits = 0);
  Status InitHRD();

  int32_t mQuantMax;
  int32_t  mRCfap, mRCqap, mRCbap, mRCq;
  double  mRCqa, mRCfa, mRCqa0;

  int32_t mMaxFrameSize, mMinFrameSize;
  int32_t mOversize, mUndersize;

//  unsigned long long mMaxBitsPerPic, mMaxBitsPerPicNot0;
/*
  int32_t mSceneChange;
  int32_t mBitsEncodedP, mBitsEncodedPrev;
  int32_t mPoc, mSChPoc;
  int32_t mRCfapMax, mRCqapMax;
  uint32_t mMaxBitrate;
  double mRCfsize;
  int32_t mScChFrameCnt;
*/

/*
  double instant_rate_thresholds[N_INST_RATE_THRESHLDS]; // constant, calculated at init, thresholds for instant bitrate
  double deviation_thresholds[N_DEV_THRESHLDS]; // constant, calculated at init, buffer fullness/deviation thresholds
  double sizeRatio[3], sizeRatio_field[3], sRatio[3];
*/

};

} // namespace UMC
#endif

#endif //defined(MFX_ENABLE_H264_VIDEO_ENCODE)
