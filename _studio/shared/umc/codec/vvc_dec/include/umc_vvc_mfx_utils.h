// Copyright (c) 2022 Intel Corporation
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

#pragma once

#include "umc_defs.h"
#ifdef MFX_ENABLE_VVC_VIDEO_DECODE

#ifndef __UMC_VVC_UTILS_H_
#define __UMC_VVC_UTILS_H_

#include "umc_vvc_dec_defs.h"
#include "umc_vvc_frame.h"

namespace UMC_VVC_DECODER
{
    namespace MFX_Utility
    {
        //TODO, remove or add functions per VVC Spec
        UMC::Status FillVideoParam(const VVCVideoParamSet* pVps, const VVCSeqParamSet *pSps, const VVCPicParamSet *pPps, mfxVideoParam *par);
        UMC::Status UpdateVideoParam(const VVCSeqParamSet* pSps, const VVCPicParamSet* pPps, mfxVideoParam* par);
        eMFXPlatform GetPlatform_VVC(VideoCORE * core, mfxVideoParam * par);
        bool CheckGUID(VideoCORE * core, eMFXHWType type, mfxVideoParam const* param);
        bool CheckVideoParam_VVC(mfxVideoParam *in);
        mfxStatus Query_VVC(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
        void GetMfxFrameRate(uint8_t frame_rate_value, mfxU32 & frameRateN, mfxU32 & frameRateD);
        mfxU8 GetMfxCodecProfile(uint8_t profile);
        mfxU8 GetMfxCodecLevel(uint8_t level);
        UMC::Status getWinUnit(int chromaIdc, int& unitX, int& unitY);
    }
}

#endif //__UMC_VVC_UTILS_H_
#endif //MFX_ENABLE_VVC_VIDEO_DECODE
