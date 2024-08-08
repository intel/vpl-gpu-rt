// Copyright (c) 2009-2023 Intel Corporation
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
#include "libmfx_core.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE)

#include <limits>
#include <limits.h>

#include <va/va.h>
#include "libmfx_core_interface.h"
#include "libmfx_core_factory.h"

#include "mfx_h264_encode_hw_utils.h"

#include "mfx_brc_common.h"
#include "umc_h264_brc.h"
#include "mfx_enc_common.h"
#include "umc_h264_common.h"

using namespace MfxHwH264Encode;

const mfxU32 DEFAULT_CPB_IN_SECONDS = 2000;          //  BufferSizeInBits = DEFAULT_CPB_IN_SECONDS * MaxKbps
const mfxU32 MAX_BITRATE_RATIO = mfxU32(1.5 * 1000); //  MaxBps = MAX_BITRATE_RATIO * TargetKbps;

const mfxU32 DEFAULT_ASYNC_DEPTH_TO_WA_D3D9_128_SURFACE_LIMIT = 10; //  Use this value in case of d3d9 and when par.AsyncDepth parameter is set to a very big value

namespace
{
    static const mfxU8 EXTENDED_SAR = 0xff;

    static const mfxU32 MAX_H_MV = 2048;

    static const struct { mfxU16 w, h; } TABLE_E1[] =
    {
        {   0,   0 }, {   1,   1 }, {  12,  11 }, {  10,  11 }, {  16,  11 },
        {  40,  33 }, {  24,  11 }, {  20,  11 }, {  32,  11 }, {  80,  33 },
        {  18,  11 }, {  15,  11 }, {  64,  33 }, { 160,  99 }, {   4,   3 },
        {   3,   2 }, {   2,   1 }
    };

    const mfxU16 AVBR_ACCURACY_MIN = 1;
    const mfxU16 AVBR_ACCURACY_MAX = 65535;

    const mfxU16 AVBR_CONVERGENCE_MIN = 1;
    const mfxU16 AVBR_CONVERGENCE_MAX = 65535;

    bool CheckTriStateOption(mfxU16 & opt)
    {
        if (opt != MFX_CODINGOPTION_UNKNOWN &&
            opt != MFX_CODINGOPTION_ON &&
            opt != MFX_CODINGOPTION_OFF)
        {
            opt = MFX_CODINGOPTION_UNKNOWN;
            return false;
        }

        return true;
    }
#ifdef MFX_ENABLE_H264_REPARTITION_CHECK
    bool CheckTriStateOptionWithAdaptive(mfxU16 & opt)
    {
        if (opt != MFX_CODINGOPTION_UNKNOWN &&
            opt != MFX_CODINGOPTION_ON &&
            opt != MFX_CODINGOPTION_OFF &&
            opt != MFX_CODINGOPTION_ADAPTIVE)
        {
            opt = MFX_CODINGOPTION_UNKNOWN;
            return false;
        }

        return true;
    }
#endif

    bool CheckTriStateOptionForOff(mfxU16 & opt)
    {
        if (opt !=  MFX_CODINGOPTION_OFF)
        {
            opt = MFX_CODINGOPTION_OFF;
            return false;
        }

        return true;
    }

    inline void SetDefaultOn(mfxU16 & opt)
    {
        if (opt ==  MFX_CODINGOPTION_UNKNOWN)
        {
            opt = MFX_CODINGOPTION_ON;
        }
    }

    inline void SetDefaultOff(mfxU16 & opt)
    {
        if (opt ==  MFX_CODINGOPTION_UNKNOWN)
        {
            opt = MFX_CODINGOPTION_OFF;
        }
    }

    template <class T, class U>
    bool CheckFlag(T & opt, U deflt)
    {
        if (opt > 1)
        {
            opt = static_cast<T>(deflt);
            return false;
        }

        return true;
    }

    template <class T, class U>
    bool CheckRange(T & opt, U min, U max)
    {
        if (opt < min)
        {
            opt = static_cast<T>(min);
            return false;
        }

        if (opt > max)
        {
            opt = static_cast<T>(max);
            return false;
        }

        return true;
    }

    template <class T, class U>
    bool CheckRangeDflt(T & opt, U min, U max, U deflt)
    {
        if (opt < static_cast<T>(min) || opt > static_cast<T>(max))
        {
            opt = static_cast<T>(deflt);
            return false;
        }

        return true;
    }

    struct FunctionQuery {};
    struct FunctionInit {};

    template <class T, class U>
    mfxU32 CheckAgreement(FunctionQuery, T & lowerPriorityValue, U higherPriorityValue)
    {
        if (lowerPriorityValue != 0 &&
            lowerPriorityValue != higherPriorityValue)
            return lowerPriorityValue = higherPriorityValue, 1;
        else
            return 0;
    }

    template <class T, class U>
    mfxU32 CheckAgreement(FunctionInit, T & lowerPriorityValue, U higherPriorityValue)
    {
        if (lowerPriorityValue == 0)
            return lowerPriorityValue = higherPriorityValue, 0; // assignment, not a correction
        else if (lowerPriorityValue != higherPriorityValue)
            return lowerPriorityValue = higherPriorityValue, 1; // correction
        else
            return 0; // already equal
    }

    bool CheckMbAlignment(mfxU32 & opt)
    {
        if (opt & 0x0f)
        {
            opt = opt & 0xfffffff0;
            return false;
        }

        return true;
    }

    bool CheckMbAlignmentAndUp(mfxU32 & opt)
    {
        if (opt & 0x0f)
        {
            opt = (opt & 0xfffffff0) + 0x10;
            return false;
        }

        return true;
    }

    bool IsValidCodingLevel(mfxU16 level)
    {
        return
            level == MFX_LEVEL_AVC_1  ||
            level == MFX_LEVEL_AVC_11 ||
            level == MFX_LEVEL_AVC_12 ||
            level == MFX_LEVEL_AVC_13 ||
            level == MFX_LEVEL_AVC_1b ||
            level == MFX_LEVEL_AVC_2  ||
            level == MFX_LEVEL_AVC_21 ||
            level == MFX_LEVEL_AVC_22 ||
            level == MFX_LEVEL_AVC_3  ||
            level == MFX_LEVEL_AVC_31 ||
            level == MFX_LEVEL_AVC_32 ||
            level == MFX_LEVEL_AVC_4  ||
            level == MFX_LEVEL_AVC_41 ||
            level == MFX_LEVEL_AVC_42 ||
            level == MFX_LEVEL_AVC_5  ||
            level == MFX_LEVEL_AVC_51 ||
            level == MFX_LEVEL_AVC_52;
    }

    bool IsValidCodingProfile(mfxU16 profile)
    {
        return
            IsAvcBaseProfile(profile)       ||
            IsAvcHighProfile(profile)       ||
            (IsMvcProfile(profile) && (profile != MFX_PROFILE_AVC_MULTIVIEW_HIGH)) || // Multiview high isn't supported by MSDK
            profile == MFX_PROFILE_AVC_MAIN
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
            || IsSvcProfile(profile)
#endif
            ;
    }

    inline mfxU16 GetMaxSupportedLevel()
    {
        return MFX_LEVEL_AVC_52;
    }

    mfxU16 GetNextProfile(mfxU16 profile)
    {
        switch (profile)
        {
        case MFX_PROFILE_AVC_BASELINE:    return MFX_PROFILE_AVC_MAIN;
        case MFX_PROFILE_AVC_MAIN:        return MFX_PROFILE_AVC_HIGH;
        case MFX_PROFILE_AVC_HIGH:        return MFX_PROFILE_UNKNOWN;
        case MFX_PROFILE_AVC_STEREO_HIGH: return MFX_PROFILE_UNKNOWN;
        default: assert(!"bad profile");
            return MFX_PROFILE_UNKNOWN;
        }
    }

    mfxU16 GetLevelLimitByDpbSize(mfxVideoParam const & par)
    {
        mfxU32 dpbSize = par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height * 3 / 2;

        dpbSize *= par.mfx.NumRefFrame;
        assert(dpbSize > 0);

        if (dpbSize <=   152064) return MFX_LEVEL_AVC_1;
        if (dpbSize <=   345600) return MFX_LEVEL_AVC_11;
        if (dpbSize <=   912384) return MFX_LEVEL_AVC_12;
        if (dpbSize <=  1824768) return MFX_LEVEL_AVC_21;
        if (dpbSize <=  3110400) return MFX_LEVEL_AVC_22;
        if (dpbSize <=  6912000) return MFX_LEVEL_AVC_31;
        if (dpbSize <=  7864320) return MFX_LEVEL_AVC_32;
        if (dpbSize <= 12582912) return MFX_LEVEL_AVC_4;
        if (dpbSize <= 13369344) return MFX_LEVEL_AVC_42;
        if (dpbSize <= 42393600) return MFX_LEVEL_AVC_5;
        if (dpbSize <= 70778880) return MFX_LEVEL_AVC_51;

        return MFX_LEVEL_UNKNOWN;
    }

    mfxU16 GetLevelLimitByFrameSize(mfxVideoParam const & par)
    {
        mfxU32 numMb = par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height / 256;

        if (numMb <=    99) return MFX_LEVEL_AVC_1;
        if (numMb <=   396) return MFX_LEVEL_AVC_11;
        if (numMb <=   792) return MFX_LEVEL_AVC_21;
        if (numMb <=  1620) return MFX_LEVEL_AVC_22;
        if (numMb <=  3600) return MFX_LEVEL_AVC_31;
        if (numMb <=  5120) return MFX_LEVEL_AVC_32;
        if (numMb <=  8192) return MFX_LEVEL_AVC_4;
        if (numMb <=  8704) return MFX_LEVEL_AVC_42;
        if (numMb <= 22080) return MFX_LEVEL_AVC_5;
        if (numMb <= 36864) return MFX_LEVEL_AVC_51;

        return MFX_LEVEL_UNKNOWN;
    }

    mfxU16 GetLevelLimitByMbps(mfxVideoParam const & par)
    {
        mfxU32 numMb = par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height / 256;
        mfxF64 fR = mfxF64(par.mfx.FrameInfo.FrameRateExtN) / par.mfx.FrameInfo.FrameRateExtD;
        mfxF64 mbps = numMb * fR;

        if (mbps <=   1485)  return MFX_LEVEL_AVC_1;
        if (mbps <=   3000)  return MFX_LEVEL_AVC_11;
        if (mbps <=   6000)  return MFX_LEVEL_AVC_12;
        if (mbps <=  11880)  return MFX_LEVEL_AVC_13;
        if (mbps <=  19800)  return MFX_LEVEL_AVC_21;
        if (mbps <=  20250)  return MFX_LEVEL_AVC_22;
        if (mbps <=  40500)  return MFX_LEVEL_AVC_3;
        if (mbps <= 108000)  return MFX_LEVEL_AVC_31;
        if (mbps <= 216000)  return MFX_LEVEL_AVC_32;
        if (mbps <= 245760)  return MFX_LEVEL_AVC_4;
        if (mbps <= 522240)  return MFX_LEVEL_AVC_42;
        if (mbps <= 589824)  return MFX_LEVEL_AVC_5;
        if (mbps <= 983040)  return MFX_LEVEL_AVC_51;
        if (mbps <= 2073600) return MFX_LEVEL_AVC_52;

        return MFX_LEVEL_UNKNOWN;
    }

    mfxU16 GetLevelLimitByMaxBitrate(mfxU16 profile, mfxU32 kbps)
    {
        mfxU32 brFactor = IsAvcHighProfile(profile) ? 1500 : 1200;
        mfxU32 br = 1000 * kbps;

        if (br <=     64 * brFactor) return MFX_LEVEL_AVC_1;
        if (br <=    128 * brFactor) return MFX_LEVEL_AVC_1b;
        if (br <=    192 * brFactor) return MFX_LEVEL_AVC_11;
        if (br <=    384 * brFactor) return MFX_LEVEL_AVC_12;
        if (br <=    768 * brFactor) return MFX_LEVEL_AVC_13;
        if (br <=   2000 * brFactor) return MFX_LEVEL_AVC_2;
        if (br <=   4000 * brFactor) return MFX_LEVEL_AVC_21;
        if (br <=  10000 * brFactor) return MFX_LEVEL_AVC_3;
        if (br <=  14000 * brFactor) return MFX_LEVEL_AVC_31;
        if (br <=  20000 * brFactor) return MFX_LEVEL_AVC_32;
        if (br <=  50000 * brFactor) return MFX_LEVEL_AVC_41;
        if (br <= 135000 * brFactor) return MFX_LEVEL_AVC_5;
        if (br <= 240000 * brFactor) return MFX_LEVEL_AVC_51;

        return MFX_LEVEL_UNKNOWN;
    }

    mfxU16 GetLevelLimitByBufferSize(mfxU16 profile, mfxU32 bufferSizeInKb)
    {
        mfxU32 brFactor = IsAvcHighProfile(profile) ? 1500 : 1200;
        mfxU32 bufSize = 8000 * bufferSizeInKb;

        if (bufSize <=    175 * brFactor) return MFX_LEVEL_AVC_1;
        if (bufSize <=    350 * brFactor) return MFX_LEVEL_AVC_1b;
        if (bufSize <=    500 * brFactor) return MFX_LEVEL_AVC_11;
        if (bufSize <=   1000 * brFactor) return MFX_LEVEL_AVC_12;
        if (bufSize <=   2000 * brFactor) return MFX_LEVEL_AVC_13;
        if (bufSize <=   4000 * brFactor) return MFX_LEVEL_AVC_21;
        if (bufSize <=  10000 * brFactor) return MFX_LEVEL_AVC_3;
        if (bufSize <=  14000 * brFactor) return MFX_LEVEL_AVC_31;
        if (bufSize <=  20000 * brFactor) return MFX_LEVEL_AVC_32;
        if (bufSize <=  25000 * brFactor) return MFX_LEVEL_AVC_4;
        if (bufSize <=  62500 * brFactor) return MFX_LEVEL_AVC_41;
        if (bufSize <= 135000 * brFactor) return MFX_LEVEL_AVC_5;
        if (bufSize <= 240000 * brFactor) return MFX_LEVEL_AVC_51;

        return MFX_LEVEL_UNKNOWN;
    }

    // calculate possible minimum level for encoding of input stream
    mfxU16 GetMinLevelForAllParameters(MfxVideoParam const & par)
    {
        mfxExtSpsHeader const & extSps = GetExtBufferRef(par);

        if (par.mfx.FrameInfo.Width  == 0 ||
            par.mfx.FrameInfo.Height == 0)
        {
            // input information isn't enough to determine required level
            return 0;
        }

        mfxU16 maxSupportedLevel = GetMaxSupportedLevel();
        mfxU16 level = GetLevelLimitByFrameSize(par);

        if (level == 0 || level == maxSupportedLevel)
        {
            // level is already maximum possible, return it
            return maxSupportedLevel;
        }

        if (extSps.vui.flags.timingInfoPresent == 0 ||
            par.mfx.FrameInfo.FrameRateExtN    == 0 ||
            par.mfx.FrameInfo.FrameRateExtD    == 0)
        {
            // no information about frame rate
            return level;
        }

        mfxU16 levelMbps = GetLevelLimitByMbps(par);

        if (levelMbps == 0 || levelMbps == maxSupportedLevel)
        {
            // level is already maximum possible, return it
            return maxSupportedLevel;
        }

        if (levelMbps > level)
            level = levelMbps;

        if (par.mfx.NumRefFrame      != 0 &&
            par.mfx.FrameInfo.Width  != 0 &&
            par.mfx.FrameInfo.Height != 0)
        {
            mfxU16 levelDpbs = GetLevelLimitByDpbSize(par);

            if (levelDpbs == 0 || levelDpbs == maxSupportedLevel)
            {
                // level is already maximum possible, return it
                return maxSupportedLevel;
            }


            if (levelDpbs > level)
                level = levelDpbs;
        }

        mfxU16 profile = par.mfx.CodecProfile;
        mfxU32 kbps = par.calcParam.targetKbps;
        if (
            par.mfx.RateControlMethod == MFX_RATECONTROL_VCM ||
            par.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
            par.mfx.RateControlMethod == MFX_RATECONTROL_WIDI_VBR ||
            par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR ||
            par.mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD)
        {
            if (par.calcParam.maxKbps >= kbps)
                kbps = par.calcParam.maxKbps;
            else
                kbps = par.calcParam.targetKbps * MAX_BITRATE_RATIO / 1000;
        }
        mfxU16 levelBr = GetLevelLimitByMaxBitrate(profile, kbps);

        if (levelBr == 0 || levelBr == maxSupportedLevel)
        {
            // level is already maximum possible, return it
            return maxSupportedLevel;
        }

        if (levelBr > level)
            level = levelBr;

        profile = par.mfx.CodecProfile;
        mfxU32 cpb = par.calcParam.bufferSizeInKB;
        mfxU16 levelCPM = GetLevelLimitByBufferSize(profile, cpb);
        if (levelCPM == 0 || levelCPM == maxSupportedLevel)
        {
            // level is already maximum possible, return it
            return maxSupportedLevel;
        }

        if (levelCPM > level)
            level = levelCPM;

        return level;
    }
    // calculate minimum level required for encoding of input stream (with given resolution and frame rate)
    // input stream can't be encoded with lower level regardless of any encoding parameters
    mfxU16 GetMinLevelForResolutionAndFramerate(MfxVideoParam const & par)
    {
        mfxExtSpsHeader const & extSps = GetExtBufferRef(par);

        if (par.mfx.FrameInfo.Width         == 0 ||
            par.mfx.FrameInfo.Height        == 0)
        {
            // input information isn't enough to determine required level
            return 0;
        }

        mfxU16 maxSupportedLevel = GetMaxSupportedLevel();
        mfxU16 level = GetLevelLimitByFrameSize(par);

        if (level == 0 || level == maxSupportedLevel)
        {
            // level is already maximum possible, return it
            return maxSupportedLevel;
        }

        if (extSps.vui.flags.timingInfoPresent == 0 ||
            par.mfx.FrameInfo.FrameRateExtN    == 0 ||
            par.mfx.FrameInfo.FrameRateExtD    == 0)
        {
            // no information about frame rate
            return level;
        }

        mfxU16 levelMbps = GetLevelLimitByMbps(par);

        if (levelMbps == 0 || levelMbps == maxSupportedLevel)
        {
            // level is already maximum possible, return it
            return maxSupportedLevel;
        }

        if (levelMbps > level)
            level = levelMbps;

        return level;
    }

    mfxU16 GetMaxNumRefFrame(mfxU16 level, mfxU16 width, mfxU16 height)
    {
        mfxU32 maxDpbSize = 0;
        if (level == MFX_LEVEL_UNKNOWN)
            level = MFX_LEVEL_AVC_52;

        switch (level)
        {
        case MFX_LEVEL_AVC_1 : maxDpbSize =   152064; break;
        case MFX_LEVEL_AVC_1b: maxDpbSize =   152064; break;
        case MFX_LEVEL_AVC_11: maxDpbSize =   345600; break;
        case MFX_LEVEL_AVC_12: maxDpbSize =   912384; break;
        case MFX_LEVEL_AVC_13: maxDpbSize =   912384; break;
        case MFX_LEVEL_AVC_2 : maxDpbSize =   912384; break;
        case MFX_LEVEL_AVC_21: maxDpbSize =  1824768; break;
        case MFX_LEVEL_AVC_22: maxDpbSize =  3110400; break;
        case MFX_LEVEL_AVC_3 : maxDpbSize =  3110400; break;
        case MFX_LEVEL_AVC_31: maxDpbSize =  6912000; break;
        case MFX_LEVEL_AVC_32: maxDpbSize =  7864320; break;
        case MFX_LEVEL_AVC_4 : maxDpbSize = 12582912; break;
        case MFX_LEVEL_AVC_41: maxDpbSize = 12582912; break;
        case MFX_LEVEL_AVC_42: maxDpbSize = 13369344; break;
        case MFX_LEVEL_AVC_5 : maxDpbSize = 42393600; break;
        case MFX_LEVEL_AVC_51: maxDpbSize = 70778880; break;
        case MFX_LEVEL_AVC_52: maxDpbSize = 70778880; break;
        default: assert(!"bad CodecLevel");
        }

        mfxU32 frameSize = width * height * 3 / 2;
        return mfxU16(mfx::clamp(maxDpbSize / frameSize, 1u, 16u));
    }

    mfxU16 GetMaxNumRefFrame(mfxVideoParam const & par)
    {
        return GetMaxNumRefFrame(par.mfx.CodecLevel, par.mfx.FrameInfo.Width, par.mfx.FrameInfo.Height);
    }

    mfxU16 GetMinNumRefFrameForPyramid(mfxU16 GopRefDist)
    {
        mfxU16 refIP = (GopRefDist > 1 ? 2 : 1);
        mfxU16 refB  = GopRefDist ? (GopRefDist - 1) / 2 : 0;

        for (mfxU16 x = refB; x > 2;)
        {
            x     = (x - 1) / 2;
            refB -= x;
        }

        return refIP + refB;
    }

    mfxU16 GetMinNumRefFrameForPyramid(mfxVideoParam const & par)
    {
        return GetMinNumRefFrameForPyramid(par.mfx.GopRefDist);
    }

    mfxU32 GetMaxBitrate(mfxVideoParam const & par)
    {
        mfxU32 brFactor = IsAvcHighProfile(par.mfx.CodecProfile) ? 1500 : 1200;

        mfxU16 level = par.mfx.CodecLevel;
        if (level == MFX_LEVEL_UNKNOWN)
            level = MFX_LEVEL_AVC_52;

        switch (level)
        {
        case MFX_LEVEL_AVC_1 : return     64 * brFactor;
        case MFX_LEVEL_AVC_1b: return    128 * brFactor;
        case MFX_LEVEL_AVC_11: return    192 * brFactor;
        case MFX_LEVEL_AVC_12: return    384 * brFactor;
        case MFX_LEVEL_AVC_13: return    768 * brFactor;
        case MFX_LEVEL_AVC_2 : return   2000 * brFactor;
        case MFX_LEVEL_AVC_21: return   4000 * brFactor;
        case MFX_LEVEL_AVC_22: return   4000 * brFactor;
        case MFX_LEVEL_AVC_3 : return  10000 * brFactor;
        case MFX_LEVEL_AVC_31: return  14000 * brFactor;
        case MFX_LEVEL_AVC_32: return  20000 * brFactor;
        case MFX_LEVEL_AVC_4 : return  20000 * brFactor;
        case MFX_LEVEL_AVC_41: return  50000 * brFactor;
        case MFX_LEVEL_AVC_42: return  50000 * brFactor;
        case MFX_LEVEL_AVC_5 : return 135000 * brFactor;
        case MFX_LEVEL_AVC_51: return 240000 * brFactor;
        case MFX_LEVEL_AVC_52: return 240000 * brFactor;
        default: assert(!"bad CodecLevel"); return 0;
        }
    }

    mfxU32 GetMaxPerViewBitrate(MfxVideoParam const & par)
    {
        mfxU32 brFactor = IsMvcProfile(par.mfx.CodecProfile) ? 1500 :
            IsAvcHighProfile(par.mfx.CodecProfile) ? 1500 : 1200;

        mfxU16 level = IsMvcProfile(par.mfx.CodecProfile) ? par.calcParam.mvcPerViewPar.codecLevel : par.mfx.CodecLevel;
        if (level == MFX_LEVEL_UNKNOWN)
            level = MFX_LEVEL_AVC_52;

        switch (level)
        {
        case MFX_LEVEL_AVC_1 : return     64 * brFactor;
        case MFX_LEVEL_AVC_1b: return    128 * brFactor;
        case MFX_LEVEL_AVC_11: return    192 * brFactor;
        case MFX_LEVEL_AVC_12: return    384 * brFactor;
        case MFX_LEVEL_AVC_13: return    768 * brFactor;
        case MFX_LEVEL_AVC_2 : return   2000 * brFactor;
        case MFX_LEVEL_AVC_21: return   4000 * brFactor;
        case MFX_LEVEL_AVC_22: return   4000 * brFactor;
        case MFX_LEVEL_AVC_3 : return  10000 * brFactor;
        case MFX_LEVEL_AVC_31: return  14000 * brFactor;
        case MFX_LEVEL_AVC_32: return  20000 * brFactor;
        case MFX_LEVEL_AVC_4 : return  20000 * brFactor;
        case MFX_LEVEL_AVC_41: return  50000 * brFactor;
        case MFX_LEVEL_AVC_42: return  50000 * brFactor;
        case MFX_LEVEL_AVC_5 : return 135000 * brFactor;
        case MFX_LEVEL_AVC_51: return 240000 * brFactor;
        case MFX_LEVEL_AVC_52: return 240000 * brFactor;
        default: assert(!"bad CodecLevel"); return 0;
        }
    }

    mfxU32 GetMaxBufferSize(mfxVideoParam const & par)
    {
        mfxU32 brFactor = IsAvcHighProfile(par.mfx.CodecProfile) ? 1500 : 1200;

        mfxU16 level = par.mfx.CodecLevel;
        if (level == MFX_LEVEL_UNKNOWN)
            level = MFX_LEVEL_AVC_52;

        switch (level)
        {
        case MFX_LEVEL_AVC_1 : return    175 * brFactor;
        case MFX_LEVEL_AVC_1b: return    350 * brFactor;
        case MFX_LEVEL_AVC_11: return    500 * brFactor;
        case MFX_LEVEL_AVC_12: return   1000 * brFactor;
        case MFX_LEVEL_AVC_13: return   2000 * brFactor;
        case MFX_LEVEL_AVC_2 : return   2000 * brFactor;
        case MFX_LEVEL_AVC_21: return   4000 * brFactor;
        case MFX_LEVEL_AVC_22: return   4000 * brFactor;
        case MFX_LEVEL_AVC_3 : return  10000 * brFactor;
        case MFX_LEVEL_AVC_31: return  14000 * brFactor;
        case MFX_LEVEL_AVC_32: return  20000 * brFactor;
        case MFX_LEVEL_AVC_4 : return  25000 * brFactor;
        case MFX_LEVEL_AVC_41: return  62500 * brFactor;
        case MFX_LEVEL_AVC_42: return  62500 * brFactor;
        case MFX_LEVEL_AVC_5 : return 135000 * brFactor;
        case MFX_LEVEL_AVC_51: return 240000 * brFactor;
        case MFX_LEVEL_AVC_52: return 240000 * brFactor;
        default: assert(!"bad CodecLevel"); return 0;
        }
    }

    mfxU32 GetMaxPerViewBufferSize(MfxVideoParam const & par)
    {
        mfxU32 brFactor = IsMvcProfile(par.mfx.CodecProfile) ? 1500 :
            IsAvcHighProfile(par.mfx.CodecProfile) ? 1500 : 1200;

        mfxU16 level = IsMvcProfile(par.mfx.CodecProfile) ? par.calcParam.mvcPerViewPar.codecLevel : par.mfx.CodecLevel;
        if (level == MFX_LEVEL_UNKNOWN)
            level = MFX_LEVEL_AVC_52;

        switch (level)
        {
        case MFX_LEVEL_AVC_1 : return    175 * brFactor;
        case MFX_LEVEL_AVC_1b: return    350 * brFactor;
        case MFX_LEVEL_AVC_11: return    500 * brFactor;
        case MFX_LEVEL_AVC_12: return   1000 * brFactor;
        case MFX_LEVEL_AVC_13: return   2000 * brFactor;
        case MFX_LEVEL_AVC_2 : return   2000 * brFactor;
        case MFX_LEVEL_AVC_21: return   4000 * brFactor;
        case MFX_LEVEL_AVC_22: return   4000 * brFactor;
        case MFX_LEVEL_AVC_3 : return  10000 * brFactor;
        case MFX_LEVEL_AVC_31: return  14000 * brFactor;
        case MFX_LEVEL_AVC_32: return  20000 * brFactor;
        case MFX_LEVEL_AVC_4 : return  25000 * brFactor;
        case MFX_LEVEL_AVC_41: return  62500 * brFactor;
        case MFX_LEVEL_AVC_42: return  62500 * brFactor;
        case MFX_LEVEL_AVC_5 : return 135000 * brFactor;
        case MFX_LEVEL_AVC_51: return 240000 * brFactor;
        case MFX_LEVEL_AVC_52: return 240000 * brFactor;
        default: assert(!"bad CodecLevel"); return 0;
        }
    }


    mfxU32 GetMaxMbps(mfxVideoParam const & par)
    {
        switch (par.mfx.CodecLevel)
        {
        case MFX_LEVEL_AVC_1 :
        case MFX_LEVEL_AVC_1b: return   1485;
        case MFX_LEVEL_AVC_11: return   3000;
        case MFX_LEVEL_AVC_12: return   6000;
        case MFX_LEVEL_AVC_13:
        case MFX_LEVEL_AVC_2 : return  11800;
        case MFX_LEVEL_AVC_21: return  19800;
        case MFX_LEVEL_AVC_22: return  20250;
        case MFX_LEVEL_AVC_3 : return  40500;
        case MFX_LEVEL_AVC_31: return 108000;
        case MFX_LEVEL_AVC_32: return 216000;
        case MFX_LEVEL_AVC_4 :
        case MFX_LEVEL_AVC_41: return 245760;
        case MFX_LEVEL_AVC_42: return 522240;
        case MFX_LEVEL_AVC_5 : return 589824;
        case MFX_LEVEL_AVC_51: return 983040;
        case MFX_LEVEL_AVC_52: return 2073600;
        default: assert(!"bad CodecLevel"); return 0;
        }
    }

    mfxU32 GetMinCr(mfxU32 level)
    {
        return level >= MFX_LEVEL_AVC_31 && level <= MFX_LEVEL_AVC_42 ? 4 : 2; // AVCHD spec requires MinCR = 4 for levels  4.1, 4.2
    }

    mfxU32 GetFirstMaxFrameSize(mfxVideoParam const & par)
    {
        mfxU32 picSizeInMbs = par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height / 256;
        return 384 * std::max<mfxU32>(picSizeInMbs, GetMaxMbps(par) / 172) / GetMinCr(par.mfx.CodecLevel);
    }

    mfxU32 GetMaxFrameSize(mfxVideoParam const & par)
    {
        mfxF64 frameRate = mfxF64(par.mfx.FrameInfo.FrameRateExtN) / par.mfx.FrameInfo.FrameRateExtD;
        return mfxU32(384 * GetMaxMbps(par) / frameRate / GetMinCr(par.mfx.CodecLevel));
    }

    mfxStatus CheckMaxFrameSize(MfxVideoParam & par, MFX_ENCODE_CAPS const & hwCaps)
    {
        mfxExtCodingOption2 & extOpt2 = GetExtBufferRef(par);
        mfxExtCodingOption3 & extOpt3 = GetExtBufferRef(par);

        if (extOpt2.MaxFrameSize == 0 && extOpt3.MaxFrameSizeI == 0 && extOpt3.MaxFrameSizeP == 0)
            return MFX_ERR_NONE;

        bool changed = false;
        bool unsupported = false;
        bool IsEnabledSwBrc = false;
#if defined(MFX_ENABLE_EXT_BRC)
        IsEnabledSwBrc = isSWBRC(par);
#else
        IsEnabledSwBrc = bRateControlLA(par.mfx.RateControlMethod);
#endif
        if ((par.mfx.RateControlMethod == MFX_RATECONTROL_CBR || par.mfx.RateControlMethod == MFX_RATECONTROL_CQP) || // max frame size supported only for VBR based methods
            (hwCaps.ddi_caps.UserMaxFrameSizeSupport == 0 && !IsEnabledSwBrc))
        {
            if (extOpt2.MaxFrameSize != 0 || extOpt3.MaxFrameSizeI != 0 || extOpt3.MaxFrameSizeP != 0)
                changed = true;
            extOpt2.MaxFrameSize = 0;
            extOpt3.MaxFrameSizeI = 0;
            extOpt3.MaxFrameSizeP = 0;
        }
        else
        {
            if (par.calcParam.cqpHrdMode == 0 && par.calcParam.targetKbps != 0 &&
                par.mfx.FrameInfo.FrameRateExtN != 0 && par.mfx.FrameInfo.FrameRateExtD != 0)
            {
                mfxF64 frameRate = mfxF64(par.mfx.FrameInfo.FrameRateExtN) / par.mfx.FrameInfo.FrameRateExtD;
                mfxU32 avgFrameSizeInBytes = par.calcParam.TCBRCTargetFrameSize ?
                    par.calcParam.TCBRCTargetFrameSize :
                    mfxU32(par.calcParam.targetKbps * 1000 / frameRate / 8);

                if ((extOpt2.MaxFrameSize < avgFrameSizeInBytes) && (extOpt2.MaxFrameSize != 0))
                {
                    changed = true;
                    extOpt2.MaxFrameSize = 0;
                }
                if ((extOpt3.MaxFrameSizeI < avgFrameSizeInBytes) && (extOpt3.MaxFrameSizeI != 0))
                {
                    changed = true;
                    extOpt3.MaxFrameSizeI = 0;
                    extOpt3.MaxFrameSizeP = 0;
                }
            }
            if ((extOpt2.MaxFrameSize != 0 && extOpt3.MaxFrameSizeI != 0) &&
                extOpt3.MaxFrameSizeI != extOpt2.MaxFrameSize)
            {
                extOpt2.MaxFrameSize = 0;
                changed = true;
            }
            if (extOpt3.MaxFrameSizeI == 0 && extOpt3.MaxFrameSizeP != 0)
            {
                extOpt3.MaxFrameSizeP = 0;
                unsupported = true;
            }
        }

        if (extOpt3.ScenarioInfo == MFX_SCENARIO_GAME_STREAMING && (extOpt2.MaxFrameSize != 0 ||
                                                                    extOpt3.MaxFrameSizeI != 0 ||
                                                                    extOpt3.MaxFrameSizeP != 0 ))
        {
            changed = true;
            extOpt2.MaxFrameSize = 0;
            extOpt3.MaxFrameSizeI = 0;
            extOpt3.MaxFrameSizeP = 0;
        }

        if (!CheckTriStateOption(extOpt3.AdaptiveMaxFrameSize)) changed = true;

        if(!hwCaps.AdaptiveMaxFrameSizeSupport && IsOn(extOpt3.AdaptiveMaxFrameSize))
        {
            extOpt3.AdaptiveMaxFrameSize = MFX_CODINGOPTION_UNKNOWN;
            unsupported = true;
        }

        if (hwCaps.ddi_caps.UserMaxFrameSizeSupport == 0 && !IsEnabledSwBrc && IsOn(extOpt3.AdaptiveMaxFrameSize))
        {
            extOpt3.AdaptiveMaxFrameSize = MFX_CODINGOPTION_UNKNOWN;
            unsupported = true;
        }

        if (hwCaps.ddi_caps.UserMaxFrameSizeSupport == 1 && !IsEnabledSwBrc &&
            (extOpt3.MaxFrameSizeP == 0 || IsOn(par.mfx.LowPower)) &&
            IsOn(extOpt3.AdaptiveMaxFrameSize))
        {
            extOpt3.AdaptiveMaxFrameSize = MFX_CODINGOPTION_UNKNOWN;
            changed = true;
        }
        return unsupported ? MFX_ERR_UNSUPPORTED : (changed ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE);
    }

    mfxU32 GetMaxVmv(mfxU32 level)
    {
        switch (level)
        {
        case MFX_LEVEL_AVC_1 :
        case MFX_LEVEL_AVC_1b: return  64;
        case MFX_LEVEL_AVC_11:
        case MFX_LEVEL_AVC_12:
        case MFX_LEVEL_AVC_13:
        case MFX_LEVEL_AVC_2 : return 128;
        case MFX_LEVEL_AVC_21:
        case MFX_LEVEL_AVC_22:
        case MFX_LEVEL_AVC_3 : return 256;
        case MFX_LEVEL_AVC_31:
        case MFX_LEVEL_AVC_32:
        case MFX_LEVEL_AVC_4 :
        case MFX_LEVEL_AVC_41:
        case MFX_LEVEL_AVC_42:
        case MFX_LEVEL_AVC_5 :
        case MFX_LEVEL_AVC_51:
        case MFX_LEVEL_AVC_52: return 512;
        default: assert(!"bad CodecLevel"); return 0;
        }
    }

    mfxU16 GetDefaultAsyncDepth(MfxVideoParam const & par)
    {
//        mfxExtCodingOption2 const * extOpt2 = GetExtBuffer(par);

        if (par.mfx.EncodedOrder)
            return 1;

        //if (IsOn(extOpt2->ExtBRC))
        //    return 1;

        mfxU32 picSize = par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height;
        if (picSize < 200000)
            return 6; // CIF
        else if (picSize < 500000)
            return 5; // SD
        else if (picSize < 900000)
            return 4; // between SD and HD
        else
            return 3; // HD
    }

    mfxU8 GetDefaultPicOrderCount(MfxVideoParam const & par)
    {
        if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
            return 0;
        if (par.mfx.GopRefDist > 1)
            return 0;

        if (par.calcParam.numTemporalLayer > 1)
        {
            if (par.mfx.NumRefFrame < par.calcParam.scale[par.calcParam.numTemporalLayer - 2])
                return 0; // more than 1 consecutive non-reference frames

            if (par.calcParam.scale[par.calcParam.numTemporalLayer - 1] /
                par.calcParam.scale[par.calcParam.numTemporalLayer - 2] > 2)
                return 0; // more than 1 consecutive non-reference frames
        }

        return 2;
    }

    mfxU8 GetDefaultLog2MaxPicOrdCntMinus4(MfxVideoParam const & par)
    {
        mfxU32 numReorderFrames = GetNumReorderFrames(par);
        mfxU32 maxPocDiff = (numReorderFrames * par.mfx.GopRefDist + 1) * 2;

        if (par.calcParam.numTemporalLayer > 0 || par.calcParam.tempScalabilityMode)
        {
            mfxU32 maxScale = 0;

            if (par.calcParam.numTemporalLayer > 0 && par.calcParam.numTemporalLayer <= 8)
                maxScale = par.calcParam.scale[par.calcParam.numTemporalLayer - 1];

            // for tempScalabilityMode number of temporal layers should be changed dynamically w/o IDR insertion
            // to assure this first SPS in bitstream should contain maximum possible log2_max_frame_num_minus4
            if (par.calcParam.tempScalabilityMode)
                maxScale = 8; // 8 is maximum scale for 4 temporal layers

            maxPocDiff = std::max(maxPocDiff, 2 * maxScale);
        }

        mfxU32 log2MaxPoc = CeilLog2(2 * maxPocDiff - 1);
        return mfxU8(std::max(log2MaxPoc, 4u) - 4);
    }

    mfxU16 GetDefaultNumRefFrames(mfxU32 targetUsage, eMFXHWType platform)
    {
        mfxU16 const DEFAULT_BY_TU[][8] = {
            { 0, 3, 3, 3, 2, 1, 1, 1 },
            { 0, 2, 2, 2, 2, 2, 2, 2 }
        };

        if (H264ECaps::IsAltRefSupported(platform))
            return DEFAULT_BY_TU[1][targetUsage];

        return DEFAULT_BY_TU[0][targetUsage];
    }

    mfxU16 GetMaxNumRefActivePL0(mfxU32 targetUsage,
                                        eMFXHWType platform,
                                        bool isLowPower,
                                        const mfxFrameInfo& info)
    {

        constexpr mfxU16 DEFAULT_BY_TU[][8] = {
            { 0, 8, 6, 3, 3, 3, 1, 1 }, // VME progressive < 4k or interlaced
            { 0, 4, 4, 3, 3, 3, 1, 1 }, // VME progressive >= 4k
            { 0, 3, 3, 2, 2, 2, 1, 1 }  // VDEnc
        };

        if (!isLowPower)
        {
            if ((info.Width < 3840 && info.Height < 2160) ||
                (info.PicStruct != MFX_PICSTRUCT_PROGRESSIVE))
            {
                return DEFAULT_BY_TU[0][targetUsage];
            }
            else //progressive >= 4K
            {
                return DEFAULT_BY_TU[1][targetUsage];
            }
        }
        else
        {

            if (H264ECaps::IsAltRefSupported(platform))
                return 3;

            return DEFAULT_BY_TU[2][targetUsage];
        }
    }

    mfxU16 GetDefaultNumRefActivePL0(const mfxInfoMFX& mfx, eMFXHWType platform)
    {

        if (H264ECaps::IsAltRefSupported(platform))
        {
            constexpr mfxU16 DEFAULT_BY_TU[][8] = {
                { 0, 2, 2, 2, 2, 2, 1, 1 }, // without B refs
                { 0, 2, 2, 2, 2, 2, 2, 2 }  // with B refs
            };
            return DEFAULT_BY_TU[mfx.GopRefDist > 2][mfx.TargetUsage];
        }

        return GetMaxNumRefActivePL0(mfx.TargetUsage, platform, IsOn(mfx.LowPower), mfx.FrameInfo);
    }

    mfxU16 GetMaxNumRefActiveBL0(mfxU32 targetUsage,
                                        eMFXHWType /*platform*/,
                                        bool isLowPower)
    {
        if (!isLowPower)
        {
            constexpr mfxU16 DEFAULT_BY_TU[][8] = {
                { 0, 4, 4, 2, 2, 2, 1, 1 }
            };
            return DEFAULT_BY_TU[0][targetUsage];
        }
        else
        {
            return 1;
        }
    }

    mfxU16 GetDefaultNumRefActiveBL0(const mfxInfoMFX& mfx, eMFXHWType platform)
    {
        return GetMaxNumRefActiveBL0(mfx.TargetUsage, platform, IsOn(mfx.LowPower));
    }

    mfxU16 GetMaxNumRefActiveBL1(mfxU32 targetUsage,
                                     mfxU16 picStruct,
                                     bool isLowPower)
    {
        if (picStruct != MFX_PICSTRUCT_PROGRESSIVE && !isLowPower)
        {
            constexpr mfxU16 DEFAULT_BY_TU[] = { 0, 2, 2, 2, 2, 2, 1, 1 };
            return DEFAULT_BY_TU[targetUsage];
        }
        else
        {
            return 1;
        }
    }

    mfxU16 GetDefaultNumRefActiveBL1(const mfxInfoMFX& mfx, eMFXHWType /*platform*/)
    {
        return GetMaxNumRefActiveBL1(mfx.TargetUsage, mfx.FrameInfo.PicStruct, IsOn(mfx.LowPower));
    }

    mfxU16 GetDefaultIntraPredBlockSize(
        MfxVideoParam const & par)
    {
        mfxU8 minTUForTransform8x8 = 7;
        return (IsAvcBaseProfile(par.mfx.CodecProfile)       ||
            par.mfx.CodecProfile == MFX_PROFILE_AVC_MAIN ||
            par.mfx.TargetUsage > minTUForTransform8x8)
                ? mfxU16(MFX_BLOCKSIZE_MIN_16X16)
                : mfxU16(MFX_BLOCKSIZE_MIN_4X4);
    }

    mfxU32 GetCpbSizeValue(mfxU32 kbyte, mfxU32 scale)
    {
        return (8000 * kbyte) >> (4 + scale);
    }

    mfxU32 GetMaxCodedFrameSizeInKB(MfxVideoParam const & par)
    {
        mfxU64 mvcMultiplier = 1;
        const mfxU32 maxMBBytes = 3200 / 8;

        if (IsMvcProfile(par.mfx.CodecProfile))
        {
            mfxExtMVCSeqDesc const & extMvc = GetExtBufferRef(par);
            mfxExtCodingOption const & extOpt = GetExtBufferRef(par);
            if (extOpt.ViewOutput != MFX_CODINGOPTION_ON) // in case of ViewOutput bitstream should contain one view (not all views)
                mvcMultiplier = extMvc.NumView ? extMvc.NumView : 1;
        }
        mfxU32 frameSize = par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height;
        return mfxU32(std::min<size_t>(UINT_MAX, size_t(frameSize * mvcMultiplier / (16u * 16u) * maxMBBytes + 999u) / 1000u));
    }

    mfxU32 CheckAgreementOfFrameRate(
        FunctionQuery,
        mfxU32 & frameRateExtN,
        mfxU32 & frameRateExtD,
        mfxU32   timeScale,
        mfxU32   numUnitsInTick)
    {
        if (frameRateExtN != 0 &&
            frameRateExtD != 0 &&
            mfxU64(frameRateExtN) * numUnitsInTick * 2 != mfxU64(frameRateExtD) * timeScale)
        {
            frameRateExtN = timeScale;
            frameRateExtD = numUnitsInTick * 2;
            return 1;
        }

        return 0;
    }

    mfxU32 CheckAgreementOfFrameRate(
        FunctionInit,
        mfxU32 & frameRateExtN,
        mfxU32 & frameRateExtD,
        mfxU32   timeScale,
        mfxU32   numUnitsInTick)
    {
        if (frameRateExtN == 0 || frameRateExtD == 0)
        {
            frameRateExtN = timeScale;
            frameRateExtD = numUnitsInTick * 2;
            return 0; // initialized, no error
        }
        else if (mfxU64(frameRateExtN) * numUnitsInTick * 2 != mfxU64(frameRateExtD) * timeScale)
        {
            frameRateExtN = timeScale;
            frameRateExtD = numUnitsInTick * 2;
            return 1; // modified
        }
        else
        {
            return 0; // equal, no error
        }
    }

    inline mfxU16 FlagToTriState(mfxU16 flag)
    {
        return flag ? mfxU16(MFX_CODINGOPTION_ON)
            : mfxU16(MFX_CODINGOPTION_OFF);
    }

    template <class TFunc>
    bool CheckAgreementOfSequenceLevelParameters(MfxVideoParam & par, mfxExtSpsHeader const & sps)
    {
        mfxU32 changed = 0;

        mfxFrameInfo & fi = par.mfx.FrameInfo;
        mfxExtCodingOption  & extOpt  = GetExtBufferRef(par);
        mfxExtCodingOption2 & extOpt2 = GetExtBufferRef(par);
        mfxExtCodingOption3 & extOpt3 = GetExtBufferRef(par);


        TFunc f;

        changed |= CheckAgreement(f, par.mfx.CodecProfile, sps.profileIdc);
        changed |= CheckAgreement(f, par.mfx.CodecLevel,   sps.levelIdc);
        changed |= CheckAgreement(f, par.mfx.NumRefFrame,  sps.maxNumRefFrames);
        changed |= CheckAgreement(f, fi.ChromaFormat,      sps.chromaFormatIdc);

        mfxU16 const cropUnitX = CROP_UNIT_X[fi.ChromaFormat];
        mfxU16 const cropUnitY = CROP_UNIT_Y[fi.ChromaFormat] * (2 - sps.frameMbsOnlyFlag);

        changed |= CheckAgreement(f, fi.Width,     mfxU16(16 * (sps.picWidthInMbsMinus1 + 1)));
        changed |= CheckAgreement(f, fi.Height,    mfxU16(16 * (sps.picHeightInMapUnitsMinus1 + 1) * (2 - sps.frameMbsOnlyFlag)));
        changed |= CheckAgreement(f, fi.PicStruct, mfxU16(sps.frameMbsOnlyFlag ? mfxU16(MFX_PICSTRUCT_PROGRESSIVE) : fi.PicStruct));
        changed |= CheckAgreement(f, fi.CropX,     mfxU16(sps.frameCropLeftOffset * cropUnitX));
        changed |= CheckAgreement(f, fi.CropY,     mfxU16(sps.frameCropTopOffset * cropUnitY));
        changed |= CheckAgreement(f, fi.CropW,     mfxU16(fi.Width  - (sps.frameCropLeftOffset + sps.frameCropRightOffset) * cropUnitX));
        changed |= CheckAgreement(f, fi.CropH,     mfxU16(fi.Height - (sps.frameCropTopOffset  + sps.frameCropBottomOffset) * cropUnitY));

        mfxU16 disableVui = FlagToTriState(!sps.vuiParametersPresentFlag);
        changed |= CheckAgreement(f, extOpt2.DisableVUI, disableVui);

        mfxU16 aspectRatioPresent   = FlagToTriState(sps.vui.flags.aspectRatioInfoPresent);
        mfxU16 timingInfoPresent    = FlagToTriState(sps.vui.flags.timingInfoPresent);
        mfxU16 overscanInfoPresent  = FlagToTriState(sps.vui.flags.overscanInfoPresent);
        mfxU16 bitstreamRestriction = FlagToTriState(sps.vui.flags.bitstreamRestriction);

        changed |= CheckAgreement(f, extOpt3.AspectRatioInfoPresent, aspectRatioPresent);
        changed |= CheckAgreement(f, extOpt3.TimingInfoPresent,      timingInfoPresent);
        changed |= CheckAgreement(f, extOpt3.OverscanInfoPresent,    overscanInfoPresent);
        changed |= CheckAgreement(f, extOpt3.BitstreamRestriction,   bitstreamRestriction);

        if (sps.vuiParametersPresentFlag)
        {
            if (sps.vui.flags.timingInfoPresent)
            {
                mfxU16 fixedFrameRate = FlagToTriState(sps.vui.flags.fixedFrameRate);
                changed |= CheckAgreement(f, extOpt2.FixedFrameRate, fixedFrameRate);
                changed |= CheckAgreementOfFrameRate(f, fi.FrameRateExtN, fi.FrameRateExtD, sps.vui.timeScale, sps.vui.numUnitsInTick);
            }

            if (sps.vui.flags.aspectRatioInfoPresent)
            {
                AspectRatioConverter arConv(sps.vui.aspectRatioIdc, sps.vui.sarWidth, sps.vui.sarHeight);
                changed |= CheckAgreement(f, fi.AspectRatioW, arConv.GetSarWidth());
                changed |= CheckAgreement(f, fi.AspectRatioH, arConv.GetSarHeight());
            }

            if (sps.vui.flags.nalHrdParametersPresent)
            {
                mfxU16 rcmethod   = sps.vui.nalHrdParameters.cbrFlag[0] ? mfxU16(MFX_RATECONTROL_CBR) : mfxU16(MFX_RATECONTROL_VBR);
                mfxU16 maxkbps    = mfxU16(((
                    (sps.vui.nalHrdParameters.bitRateValueMinus1[0] + 1) <<
                    (6 + sps.vui.nalHrdParameters.bitRateScale)) + 999) / 1000);
                mfxU16 buffersize = mfxU16((((sps.vui.nalHrdParameters.cpbSizeValueMinus1[0] + 1) <<
                    (4 + sps.vui.nalHrdParameters.cpbSizeScale)) + 7999) / 8000);
                mfxU16 lowDelayHrd = FlagToTriState(sps.vui.flags.lowDelayHrd);

                changed |= CheckAgreement(f, par.mfx.RateControlMethod,    rcmethod);
                changed |= CheckAgreement(f, par.calcParam.maxKbps,        maxkbps);
                changed |= CheckAgreement(f, par.calcParam.bufferSizeInKB, buffersize);
                changed |= CheckAgreement(f, extOpt3.LowDelayHrd,          lowDelayHrd);

            }
        }

        mfxU16 picTimingSei = sps.vui.flags.picStructPresent
            ? mfxU16(MFX_CODINGOPTION_ON)
            : mfxU16(MFX_CODINGOPTION_OFF);
        mfxU16 vuiNalHrdParameters = sps.vui.flags.nalHrdParametersPresent
            ? mfxU16(MFX_CODINGOPTION_ON)
            : mfxU16(MFX_CODINGOPTION_OFF);
        mfxU16 vuiVclHrdParameters = sps.vui.flags.vclHrdParametersPresent
            ? mfxU16(MFX_CODINGOPTION_ON)
            : mfxU16(MFX_CODINGOPTION_OFF);

        if (sps.vui.flags.bitstreamRestriction)
            changed |= CheckAgreement(f, extOpt.MaxDecFrameBuffering, sps.vui.maxDecFrameBuffering);

        changed |= CheckAgreement(f, extOpt.PicTimingSEI,         picTimingSei);
        changed |= CheckAgreement(f, extOpt.VuiNalHrdParameters,  vuiNalHrdParameters);
        changed |= CheckAgreement(f, extOpt.VuiVclHrdParameters,  vuiVclHrdParameters);

        return changed == 0;
    }

    template <class TFunc>
    bool CheckAgreementOfPictureLevelParameters(mfxVideoParam & par, mfxExtPpsHeader const & pps)
    {
        mfxU32 changed = 0;

        mfxExtCodingOption * extOpt = GetExtBuffer(par);

        mfxU16 intraPredBlockSize = pps.transform8x8ModeFlag
            ? mfxU16(MFX_BLOCKSIZE_MIN_8X8)
            : mfxU16(MFX_BLOCKSIZE_MIN_16X16);
        mfxU16 cavlc = pps.entropyCodingModeFlag
            ? mfxU16(MFX_CODINGOPTION_OFF)
            : mfxU16(MFX_CODINGOPTION_ON);

        TFunc f;

        changed |= CheckAgreement(f, extOpt->IntraPredBlockSize, intraPredBlockSize);
        changed |= CheckAgreement(f, extOpt->CAVLC,              cavlc);

        return changed == 0;
    }

    template <class T>
    void InheritOption(T optInit, T & optReset)
    {
        if (optReset == 0)
            optReset = optInit;
    }
}

mfxU32 MfxHwH264Encode::CalcNumSurfRaw(MfxVideoParam const & video)
{
    mfxExtCodingOption2 const & extOpt2 = GetExtBufferRef(video);
    mfxExtCodingOption3 const & extOpt3 = GetExtBufferRef(video);
    return video.AsyncDepth + video.mfx.GopRefDist - 1 +
            std::max(1u, mfxU32(extOpt2.LookAheadDepth)) + (video.AsyncDepth - 1) +
            (IsOn(extOpt2.UseRawRef) ? video.mfx.NumRefFrame : 0) + ((extOpt2.MaxSliceSize != 0 || IsOn(extOpt3.FadeDetection)) ? 1 : 0);
}

mfxU32 MfxHwH264Encode::CalcNumSurfRecon(MfxVideoParam const & video)
{
    mfxExtCodingOption2 const & extOpt2 = GetExtBufferRef(video);

    if (IsOn(extOpt2.UseRawRef))
        return video.mfx.NumRefFrame + (video.AsyncDepth - 1) +
            (video.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY ? video.mfx.GopRefDist : 1);
    else
        return video.mfx.NumRefFrame + video.AsyncDepth;
}

mfxU32 MfxHwH264Encode::CalcNumSurfBitstream(MfxVideoParam const & video)
{
    return (IsFieldCodingPossible(video) ? video.AsyncDepth * 2 : video.AsyncDepth);
}

mfxU16 MfxHwH264Encode::GetMaxNumSlices(MfxVideoParam const & par)
{
    mfxExtCodingOption3 & extOpt3 = GetExtBufferRef(par);
    return std::max({extOpt3.NumSliceI, extOpt3.NumSliceP, extOpt3.NumSliceB});
}

mfxU8 MfxHwH264Encode::GetNumReorderFrames(MfxVideoParam const & video)
{
    mfxExtCodingOption2 & extOpt2 = GetExtBufferRef(video);
    mfxU8 numReorderFrames = video.mfx.GopRefDist > 1 ? 1 : 0;

    if (video.mfx.GopRefDist > 2 && extOpt2.BRefType == MFX_B_REF_PYRAMID)
    {
        numReorderFrames = (mfxU8)std::max(CeilLog2(video.mfx.GopRefDist - 1), 1u);
    }

    return numReorderFrames;
}

mfxU32 MfxHwH264Encode::CalcNumTasks(MfxVideoParam const & video)
{
    assert(video.mfx.GopRefDist > 0);
    assert(video.AsyncDepth > 0);
    mfxExtCodingOption2 const & extOpt2 = GetExtBufferRef(video);

    return video.mfx.GopRefDist + (video.AsyncDepth - 1) + std::max(1u, mfxU32(extOpt2.LookAheadDepth)) +
        (IsOn(extOpt2.UseRawRef) ? video.mfx.NumRefFrame : 0);
}

mfxI64 MfxHwH264Encode::CalcDTSFromPTS(
    mfxFrameInfo const & info,
    mfxU16               dpbOutputDelay,
    mfxU64               timeStamp)
{
    if (timeStamp != static_cast<mfxU64>(MFX_TIMESTAMP_UNKNOWN))
    {
        mfxF64 tcDuration90KHz = (mfxF64)info.FrameRateExtD / (info.FrameRateExtN * 2) * 90000; // calculate tick duration
        return mfxI64(timeStamp - tcDuration90KHz * dpbOutputDelay); // calculate DTS from PTS
    }

    return MFX_TIMESTAMP_UNKNOWN;
}

mfxU32 MfxHwH264Encode::GetMaxBitrateValue(mfxU32 kbps, mfxU32 scale)
{
    return (1000 * kbps) >> (6 + scale);
}

mfxU8 MfxHwH264Encode::GetCabacInitIdc(mfxU32 targetUsage)
{
    assert(targetUsage >= 1);
    assert(targetUsage <= 7);
    (void)targetUsage;
    //const mfxU8 CABAC_INIT_IDC_TABLE[] = { 0, 2, 2, 1, 0, 0, 0, 0 };
    return 0;
    //return CABAC_INIT_IDC_TABLE[targetUsage];
}

mfxU32 MfxHwH264Encode::GetPPyrSize(MfxVideoParam const & video, mfxU32 miniGopSize, bool bEncToolsLA)
{
    mfxExtCodingOption3 const & extOpt3 = GetExtBufferRef(video);
    mfxExtCodingOptionDDI const & extDdi = GetExtBufferRef(video);

    mfxU32 pyrSize = 1;

    if (video.mfx.GopRefDist == 1 &&
        extOpt3.PRefType == MFX_P_REF_PYRAMID &&
        !IsOn(extOpt3.ExtBrcAdaptiveLTR) &&
        extDdi.NumActiveRefP != 1)
    {
        if (bEncToolsLA)
            pyrSize = (miniGopSize <= 1) ? 1 : 4;
        else
            pyrSize =  DEFAULT_PPYR_INTERVAL;
    }

    return pyrSize;

}
bool MfxHwH264Encode::IsExtBrcSceneChangeSupported(
    MfxVideoParam const & video,
    eMFXHWType    platform)
{
    bool extbrcsc = false;
    // extbrc API change dependency
    mfxExtCodingOption2 const & extOpt2 = GetExtBufferRef(video);
    extbrcsc = (H264ECaps::IsVmeSupported(platform) &&
        IsOn(extOpt2.ExtBRC) &&
        (video.mfx.RateControlMethod == MFX_RATECONTROL_CBR || video.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
        && (video.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) && !video.mfx.EncodedOrder && extOpt2.LookAheadDepth == 0);
    return extbrcsc;
}

bool MfxHwH264Encode::IsCmNeededForSCD(
    MfxVideoParam const & video)
{
    bool useCm = false;
    // If frame in Sys memory then Cm is not needed
    useCm = !(video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY);

    return useCm;
}

bool MfxHwH264Encode::IsDenoiserSupported(
    MfxVideoParam const & video,
    eMFXHWType            platform)
{
    (void)video;
    bool
        isSupported = false;
#if defined(MFX_ENABLE_MCTF_IN_AVC)

    auto isDenoiserInPipeSupported = (video.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||video.mfx.RateControlMethod == MFX_RATECONTROL_VBR || video.mfx.RateControlMethod == MFX_RATECONTROL_CQP) &&
        ((video.mfx.FrameInfo.FourCC == MFX_FOURCC_NV12) ||(video.mfx.FrameInfo.FourCC == MFX_FOURCC_YV12)) &&
        (video.mfx.FrameInfo.BitDepthLuma == 0 || video.mfx.FrameInfo.BitDepthLuma == 8) &&
        (video.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) &&
        (video.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV420) &&
        !video.mfx.EncodedOrder;

    mfxExtCodingOption2 const & extOpt2 = GetExtBufferRef(video);
    auto isMCTFsupported = (video.mfx.FrameInfo.Width <= 3840) &&
        (video.vpp.In.Height <= 2160) &&
        (video.mfx.GopRefDist == 8) &&
        IsOn(extOpt2.ExtBRC) &&
        IsExtBrcSceneChangeSupported(video, platform);


    isSupported = isDenoiserInPipeSupported &&
        (isMCTFsupported
            );
#endif
    return isSupported;
}

bool MfxHwH264Encode::IsAdaptiveLtrOn(
    MfxVideoParam const & video)
{
    bool altr = false;
    mfxExtCodingOption3 const & extOpt3 = GetExtBufferRef(video);
    altr = IsOn(extOpt3.ExtBrcAdaptiveLTR);
    return altr;
}

// determine and return mode of Query operation (valid modes are 1, 2, 3, 4 - see MSDK spec for details)
mfxU8 MfxHwH264Encode::DetermineQueryMode(mfxVideoParam * in)
{
    if (in == 0)
        return 1;
    else
    {
        mfxExtEncoderCapability * caps = GetExtBuffer(*in);
        mfxExtEncoderResetOption * resetOpt = GetExtBuffer(*in);
        if (caps)
        {
            if (resetOpt)
            {
                // attached mfxExtEncoderCapability indicates mode 4. In this mode mfxExtEncoderResetOption shouldn't be attached
                return 0;
            }
            // specific mode to notify encoder to check guid only
            if (0x667 == caps->reserved[0])
            {
                return 5;
            }

            return 4;
        }else if (resetOpt)
            return 3;
        else
            return 2;
    }
}

/*
Setting default value for LowPower option.
By default LowPower is OFF (using DualPipe)
For platforms that hasn't VME: use LowPower by default i.e. if LowPower is Unknown then LowPower is ON

Return value:
MFX_WRN_INCOMPATIBLE_VIDEO_PARAM - if initial value of par.mfx.LowPower is not equal to MFX_CODINGOPTION_ON, MFX_CODINGOPTION_OFF or MFX_CODINGOPTION_UNKNOWN
MFX_ERR_NONE - if no errors
*/
mfxStatus MfxHwH264Encode::SetLowPowerDefault(MfxVideoParam& par, const eMFXHWType& platform)
{
    mfxStatus sts = CheckTriStateOption(par.mfx.LowPower) ? MFX_ERR_NONE : MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

    if (!H264ECaps::IsVmeSupported(platform))
    {   // DualPipe (aka VME) is not available
        par.mfx.LowPower = MFX_CODINGOPTION_ON;
        return sts;
    }

    // By default, platforms with 2 encoders (VDEnc & VME) will use VME
    // Therefore, garbage & UNKNOWN values will be overridden to OFF
    SetDefaultOff(par.mfx.LowPower);
    return sts;
}

/*
Return cached caps if they are.
Select GUID depends on LowPower option.
creates AuxilliaryDevice
request and cache EncodercCaps

Return value:
MFX_ERR_UNDEFINED_BEHAVIOR -  failed to get EncodeHWCaps iterface from Core
MFX_ERR_DEVICE_FAILED  failed to create DDIEncoder/AuxilliaryDevice of
    requestcaps call fails
MFX_ERR_NONE - if no errors
*/
mfxStatus MfxHwH264Encode::QueryHwCaps(VideoCORE* core, MFX_ENCODE_CAPS & hwCaps, mfxVideoParam * par)
{
    GUID guid = MSDK_Private_Guid_Encode_AVC_Query;

    if(IsOn(par->mfx.LowPower))
    {
        guid = MSDK_Private_Guid_Encode_AVC_LowPower_Query;
    }

    mfxU32 width  = par->mfx.FrameInfo.Width == 0 ? 1920: par->mfx.FrameInfo.Width;
    mfxU32 height = par->mfx.FrameInfo.Height == 0 ? 1088: par->mfx.FrameInfo.Height;

    EncodeHWCaps* pEncodeCaps = QueryCoreInterface<EncodeHWCaps>(core);
    if (!pEncodeCaps)
    {
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }
    else
    {
        if (pEncodeCaps->GetHWCaps<MFX_ENCODE_CAPS>(guid, &hwCaps) == MFX_ERR_NONE)
            return MFX_ERR_NONE;
    }
    std::unique_ptr<DriverEncoder> ddi;

    ddi.reset(CreatePlatformH264Encoder(core));
    if (ddi.get() == 0)
        MFX_RETURN(Error(MFX_ERR_DEVICE_FAILED));


    mfxStatus sts = ddi->CreateAuxilliaryDevice(core, guid, width, height, true);
    MFX_CHECK_STS(sts);

    sts = ddi->QueryEncodeCaps(hwCaps);
    MFX_CHECK_STS(sts);

    MFX_RETURN(pEncodeCaps->SetHWCaps<MFX_ENCODE_CAPS>(guid, &hwCaps));
}

mfxStatus MfxHwH264Encode::QueryMbProcRate(VideoCORE* core, mfxVideoParam const & par, mfxU32 (&mbPerSec)[16], const mfxVideoParam * in)
{
    mfxU32 width  = in->mfx.FrameInfo.Width == 0 ? 1920: in->mfx.FrameInfo.Width;
    mfxU32 height = in->mfx.FrameInfo.Height == 0 ? 1088: in->mfx.FrameInfo.Height;

    GUID guid = MSDK_Private_Guid_Encode_AVC_Query;
    if(IsOn(in->mfx.LowPower))
        guid = MSDK_Private_Guid_Encode_AVC_LowPower_Query;

    EncodeHWCaps* pEncodeCaps = QueryCoreInterface<EncodeHWCaps>(core, MFXIHWMBPROCRATE_GUID);
    if (!pEncodeCaps)
    {
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }
    else
    {
        if (pEncodeCaps->GetHWCaps<mfxU32>(guid, mbPerSec, 16) == MFX_ERR_NONE &&
            mbPerSec[(par.mfx.TargetUsage?par.mfx.TargetUsage:4) - 1] != 0) //check if MbPerSec for particular TU was already queried or need to save
            return MFX_ERR_NONE;
    }

    std::unique_ptr<DriverEncoder> ddi;

    ddi.reset(CreatePlatformH264Encoder(core));
    if (ddi.get() == 0)
        MFX_RETURN(Error(MFX_ERR_DEVICE_FAILED));


    mfxStatus sts = ddi->CreateAuxilliaryDevice(core, guid, width, height, true);
    MFX_CHECK_STS(sts);

    mfxU32 tempMbPerSec[16] = {0, };
    sts = ddi->QueryMbPerSec(par, tempMbPerSec);

    MFX_CHECK_STS(sts);
    mbPerSec[(par.mfx.TargetUsage?par.mfx.TargetUsage:4) - 1] = tempMbPerSec[0];

    MFX_RETURN(pEncodeCaps->SetHWCaps<mfxU32>(guid, mbPerSec, 16));
}

mfxStatus MfxHwH264Encode::QueryGuid(VideoCORE* core, GUID guid)
{
    std::unique_ptr<DriverEncoder> ddi;

    ddi.reset(CreatePlatformH264Encoder(core));
    if (ddi.get() == 0)
        MFX_RETURN(Error(MFX_ERR_DEVICE_FAILED));

    return ddi->QueryHWGUID(core, guid, true);
}

mfxStatus MfxHwH264Encode::ReadSpsPpsHeaders(MfxVideoParam & par)
{
    mfxExtCodingOptionSPSPPS & extBits = GetExtBufferRef(par);

    try
    {
        if (extBits.SPSBuffer)
        {
            InputBitstream reader(extBits.SPSBuffer, extBits.SPSBufSize);
            mfxExtSpsHeader & extSps = GetExtBufferRef(par);
            ReadSpsHeader(reader, extSps);

            if (extBits.PPSBuffer)
            {
                InputBitstream pps_reader(extBits.PPSBuffer, extBits.PPSBufSize);
                mfxExtPpsHeader & extPps = GetExtBufferRef(par);
                ReadPpsHeader(pps_reader, extSps, extPps);
            }
        }
    }
    catch (std::exception &)
    {
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
    }

    return MFX_ERR_NONE;
}

mfxU16 MfxHwH264Encode::GetFrameWidth(MfxVideoParam & par)
{
    mfxExtCodingOptionSPSPPS & extBits = GetExtBufferRef(par);
    if (extBits.SPSBuffer)
    {
        mfxExtSpsHeader & extSps = GetExtBufferRef(par);
        return mfxU16(16 * (extSps.picWidthInMbsMinus1 + 1));
    }
    else
    {
        return par.mfx.FrameInfo.Width;
    }
}

mfxU16 MfxHwH264Encode::GetFrameHeight(MfxVideoParam & par)
{
    mfxExtCodingOptionSPSPPS & extBits = GetExtBufferRef(par);
    if (extBits.SPSBuffer)
    {
        mfxExtSpsHeader & extSps = GetExtBufferRef(par);
        return mfxU16(16 * (extSps.picHeightInMapUnitsMinus1 + 1) * (2 - extSps.frameMbsOnlyFlag));
    }
    else
    {
        return par.mfx.FrameInfo.Height;
    }
}


mfxStatus MfxHwH264Encode::CorrectCropping(MfxVideoParam& par)
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxU16 horzCropUnit = CROP_UNIT_X[par.mfx.FrameInfo.ChromaFormat];
    mfxU16 vertCropUnit = CROP_UNIT_Y[par.mfx.FrameInfo.ChromaFormat];
    vertCropUnit *= IsFieldCodingPossible(par) ? 2 : 1;

    mfxU16 misalignment = par.mfx.FrameInfo.CropX & (horzCropUnit - 1);
    if (misalignment > 0)
    {
        par.mfx.FrameInfo.CropX += horzCropUnit - misalignment;
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

        if (par.mfx.FrameInfo.CropW >= horzCropUnit - misalignment)
            par.mfx.FrameInfo.CropW -= horzCropUnit - misalignment;
        else
            par.mfx.FrameInfo.CropW = 0;
    }

    misalignment = par.mfx.FrameInfo.CropW & (horzCropUnit - 1);
    if (misalignment > 0)
    {
        par.mfx.FrameInfo.CropW = par.mfx.FrameInfo.CropW - misalignment;
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    misalignment = par.mfx.FrameInfo.CropY & (vertCropUnit - 1);
    if (misalignment > 0)
    {
        par.mfx.FrameInfo.CropY += vertCropUnit - misalignment;
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

        if (par.mfx.FrameInfo.CropH >= vertCropUnit - misalignment)
            par.mfx.FrameInfo.CropH -= vertCropUnit - misalignment;
        else
            par.mfx.FrameInfo.CropH = 0;
    }

    misalignment = par.mfx.FrameInfo.CropH & (vertCropUnit - 1);
    if (misalignment > 0)
    {
        par.mfx.FrameInfo.CropH = par.mfx.FrameInfo.CropH - misalignment;
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    MFX_RETURN(sts);
}

bool MfxHwH264Encode::IsRunTimeOnlyExtBuffer(mfxU32 id)
{
    return
           id == MFX_EXTBUFF_AVC_REFLIST_CTRL
#if defined (MFX_ENABLE_H264_ROUNDING_OFFSET)
        || id == MFX_EXTBUFF_AVC_ROUNDING_OFFSET
#endif
#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
        || id == MFX_EXTBUFF_GPU_HANG
#endif
        || id == MFX_EXTBUFF_ENCODED_FRAME_INFO
        || id == MFX_EXTBUFF_MBQP
#if defined (MFX_ENABLE_H264_PRIVATE_CTRL)
        || id == MFX_EXTBUFF_AVC_ENCODE_CTRL
#endif
        || id == MFX_EXTBUFF_MB_FORCE_INTRA
        || id == MFX_EXTBUFF_MB_DISABLE_SKIP_MAP
#ifndef MFX_AVC_ENCODING_UNIT_DISABLE
        || id == MFX_EXTBUFF_ENCODED_UNITS_INFO
#endif
        ;
}

bool MfxHwH264Encode::IsRunTimeExtBufferIdSupported(MfxVideoParam const & video, mfxU32 id)
{
    (void)video;

    return
          (id == MFX_EXTBUFF_AVC_REFLIST_CTRL
        || id == MFX_EXTBUFF_AVC_REFLISTS
#if defined (MFX_ENABLE_H264_ROUNDING_OFFSET)
        || id == MFX_EXTBUFF_AVC_ROUNDING_OFFSET
#endif
        || id == MFX_EXTBUFF_ENCODED_FRAME_INFO
        || id == MFX_EXTBUFF_PICTURE_TIMING_SEI
        || id == MFX_EXTBUFF_CODING_OPTION2
        || id == MFX_EXTBUFF_CODING_OPTION3
        || id == MFX_EXTBUFF_ENCODER_ROI
        || id == MFX_EXTBUFF_MBQP
        || id == MFX_EXTBUFF_MOVING_RECTANGLES
        || id == MFX_EXTBUFF_DIRTY_RECTANGLES
        || id == MFX_EXTBUFF_MB_FORCE_INTRA
        || id == MFX_EXTBUFF_MB_DISABLE_SKIP_MAP
#if defined (MFX_ENABLE_H264_PRIVATE_CTRL)
        || id == MFX_EXTBUFF_AVC_ENCODE_CTRL
#endif
        || id == MFX_EXTBUFF_PRED_WEIGHT_TABLE
#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
        || id == MFX_EXTBUFF_GPU_HANG
#endif
        || id == MFX_EXTBUFF_MULTI_FRAME_CONTROL
        || id == MFX_EXTBUFF_PICTURE_TIMING_SEI
        );
}

bool MfxHwH264Encode::IsRuntimeOutputExtBufferIdSupported(MfxVideoParam const & video, mfxU32 id)
{
    (void)video;

    return
            id == MFX_EXTBUFF_ENCODED_FRAME_INFO
#ifndef MFX_AVC_ENCODING_UNIT_DISABLE
            || id == MFX_EXTBUFF_ENCODED_UNITS_INFO
#endif
            ;
}

bool MfxHwH264Encode::IsRunTimeExtBufferPairAllowed(MfxVideoParam const & video, mfxU32 id)
{
    (void)video;

    return (id == MFX_EXTBUFF_AVC_REFLISTS
#if defined (MFX_ENABLE_H264_ROUNDING_OFFSET)
            || id == MFX_EXTBUFF_AVC_ROUNDING_OFFSET
#endif
        || id == MFX_EXTBUFF_PICTURE_TIMING_SEI
           )
           ;
}

bool MfxHwH264Encode::IsRunTimeExtBufferPairRequired(MfxVideoParam const & video, mfxU32 id)
{
    (void)id;
    (void)video;
    return false;
}

bool MfxHwH264Encode::IsVideoParamExtBufferIdSupported(mfxU32 id)
{
    return
           (id == MFX_EXTBUFF_CODING_OPTION
        || id == MFX_EXTBUFF_CODING_OPTION_SPSPPS
#if defined(__MFXBRC_H__)
        || id == MFX_EXTBUFF_BRC
#endif
#if defined(MFX_ENABLE_ENCTOOLS)
        || id == MFX_EXTBUFF_ENCTOOLS
        || id == MFX_EXTBUFF_ENCTOOLS_CONFIG
        || id == MFX_EXTBUFF_ENCTOOLS_DEVICE
        || id == MFX_EXTBUFF_ENCTOOLS_ALLOCATOR
#endif

        || id == MFX_EXTBUFF_MVC_SEQ_DESC
        || id == MFX_EXTBUFF_VIDEO_SIGNAL_INFO
        || id == MFX_EXTBUFF_PICTURE_TIMING_SEI
        || id == MFX_EXTBUFF_AVC_TEMPORAL_LAYERS
        || id == MFX_EXTBUFF_CODING_OPTION2
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
        || id == MFX_EXTBUFF_SVC_SEQ_DESC
        || id == MFX_EXTBUFF_SVC_RATE_CONTROL
#endif
        || id == MFX_EXTBUFF_ENCODER_RESET_OPTION
        || id == MFX_EXTBUFF_ENCODER_CAPABILITY
        || id == MFX_EXTBUFF_ENCODER_ROI
        || id == MFX_EXTBUFF_CODING_OPTION3
        || id == MFX_EXTBUFF_CHROMA_LOC_INFO
        || id == MFX_EXTBUFF_PRED_WEIGHT_TABLE
        || id == MFX_EXTBUFF_DIRTY_RECTANGLES
        || id == MFX_EXTBUFF_MOVING_RECTANGLES
        || id == MFX_EXTBUFF_MULTI_FRAME_PARAM
        || id == MFX_EXTBUFF_MULTI_FRAME_CONTROL
#if defined(MFX_ENABLE_ENCODE_QUALITYINFO)
        || id == MFX_EXTBUFF_ENCODED_QUALITY_INFO_MODE
#endif
#if defined(MFX_ENABLE_PARTIAL_BITSTREAM_OUTPUT)
        || id == MFX_EXTBUFF_PARTIAL_BITSTREAM_PARAM
#endif
        || id == MFX_EXTBUFF_ALLOCATION_HINTS
        || id == MFX_EXTBUFF_VPP_DENOISE2

       );
}

mfxStatus MfxHwH264Encode::CheckExtBufferId(mfxVideoParam const & par)
{
    for (mfxU32 i = 0; i < par.NumExtParam; i++)
    {
        if (par.ExtParam[i] == 0)
            MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);

        if (!IsVideoParamExtBufferIdSupported(par.ExtParam[i]->BufferId))
            MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);

        // check if buffer presents twice in video param
        {
            if (mfx::GetExtBuffer(
                par.ExtParam + i + 1,
                par.NumExtParam - i - 1,
                par.ExtParam[i]->BufferId) != 0)
            {
                MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
            }
        }
    }

    return MFX_ERR_NONE;
}

namespace
{
    mfxStatus CheckWidthAndHeight(mfxU32 width, mfxU32 height, mfxU32 picStruct)
    {
        MFX_CHECK(width  > 0, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(height > 0, MFX_ERR_INVALID_VIDEO_PARAM);

        MFX_CHECK((width  & 1) == 0, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK((height & 1) == 0, MFX_ERR_INVALID_VIDEO_PARAM);

        // repeat flags are for EncodeFrameAsync
        if (picStruct & MFX_PICSTRUCT_PART2)
            picStruct = MFX_PICSTRUCT_UNKNOWN;

        // more then one flag was set
        if (mfxU32 picStructPart1 = picStruct & MFX_PICSTRUCT_PART1)
            if (picStructPart1 & (picStructPart1 - 1))
                picStruct = MFX_PICSTRUCT_UNKNOWN;

        if ((picStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0)
            MFX_CHECK((height & 31) == 0, MFX_ERR_INVALID_VIDEO_PARAM);

        return MFX_ERR_NONE;
    }
};

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
mfxU32 MfxHwH264Encode::GetLastDid(mfxExtSVCSeqDesc const & extSvc)
{
    mfxI32 i = 7;
    for (; i >= 0; i--)
        if (extSvc.DependencyLayer[i].Active)
            break;
    return mfxU32(i);
}
#endif

mfxStatus MfxHwH264Encode::CheckWidthAndHeight(MfxVideoParam const & par)
{
    mfxExtCodingOptionSPSPPS const & extBits = GetExtBufferRef(par);

    if (extBits.SPSBuffer)
        // width/height/picStruct are always valid
        // when they come from SPSPPS buffer
        // no need to check them
        return MFX_ERR_NONE;

    mfxU16 width     = par.mfx.FrameInfo.Width;
    mfxU16 height    = par.mfx.FrameInfo.Height;
    mfxU16 picStruct = par.mfx.FrameInfo.PicStruct;

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    if (IsSvcProfile(par.mfx.CodecProfile))
    {
        mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(par);
        mfxU32 lastDid = GetLastDid(*extSvc);
        if (lastDid > 7)
            MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
        width  = extSvc->DependencyLayer[lastDid].Width;
        height = extSvc->DependencyLayer[lastDid].Height;
        if (width == 0 || height == 0)
            MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
    }
#endif

    return ::CheckWidthAndHeight(width, height, picStruct);
}

mfxStatus MfxHwH264Encode::CopySpsPpsToVideoParam(
    MfxVideoParam & par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MfxHwH264Encode::CopySpsPpsToVideoParam");
    mfxExtCodingOptionSPSPPS & extBits = GetExtBufferRef(par);

    bool changed = false;

    if (extBits.SPSBuffer)
    {
        mfxExtSpsHeader const & extSps = GetExtBufferRef(par);
        if (!CheckAgreementOfSequenceLevelParameters<FunctionInit>(par, extSps))
            changed = true;
    }


    if (extBits.PPSBuffer)
    {
        mfxExtPpsHeader const & extPps = GetExtBufferRef(par);
        if (!CheckAgreementOfPictureLevelParameters<FunctionInit>(par, extPps))
            changed = true;
    }

    MFX_RETURN(changed ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE);
}

// check encoding configuration for number of H264 spec violations allowed by MSDK
// it's required to report correct status to application
mfxStatus MfxHwH264Encode::CheckForAllowedH264SpecViolations(
    MfxVideoParam const & par)
{
    if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE
        && par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN
        && par.mfx.CodecLevel > MFX_LEVEL_AVC_41)
    {
        MFX_RETURN(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    }

    return MFX_ERR_NONE;
}

mfxStatus MfxHwH264Encode::CheckVideoParam(
    MfxVideoParam &         par,
    MFX_ENCODE_CAPS const & hwCaps,
    bool                    setExtAlloc,
    eMFXHWType              platform,
    eMFXVAType              vaType,
    eMFXGTConfig            config,
    bool                    bInit)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MfxHwH264Encode::CheckVideoParam");
    mfxStatus checkSts = MFX_ERR_NONE;

    mfxExtCodingOptionSPSPPS & extBits = GetExtBufferRef(par);
    mfxExtSpsHeader          & extSps  = GetExtBufferRef(par);
    mfxExtCodingOption3      & extOpt3 = GetExtBufferRef(par);

    // first check mandatory parameters
    MFX_CHECK(
        extBits.SPSBuffer != 0 ||
        (par.mfx.FrameInfo.Width > 0 && par.mfx.FrameInfo.Height > 0), MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(
        (extSps.vui.timeScale            > 0 && extSps.vui.numUnitsInTick       > 0) ||
        (par.mfx.FrameInfo.FrameRateExtN > 0 && par.mfx.FrameInfo.FrameRateExtD > 0),
        MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(par.mfx.TargetUsage            <= 7, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(par.mfx.FrameInfo.ChromaFormat != 0, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(par.IOPattern                  != 0, MFX_ERR_INVALID_VIDEO_PARAM);


#if defined(MFX_ENABLE_EXT_BRC)
    if (bInit)
    {
        mfxExtBRC           & extBRC  = GetExtBufferRef(par);
        mfxExtCodingOption2 & extOpt2 = GetExtBufferRef(par);
        if ((extBRC.pthis || extBRC.Init || extBRC.Close || extBRC.GetFrameCtrl || extBRC.Update || extBRC.Reset) &&
            (!extBRC.pthis || !extBRC.Init || !extBRC.Close || !extBRC.GetFrameCtrl || !extBRC.Update || !extBRC.Reset))
        {
            extOpt2.ExtBRC      = 0;
            extBRC.pthis        = 0;
            extBRC.Init         = 0;
            extBRC.Close        = 0;
            extBRC.GetFrameCtrl = 0;
            extBRC.Update       = 0;
            extBRC.Reset        = 0;
            MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
        }
    }
 #else
    bInit;
#endif

    mfxStatus sts = MFX_ERR_NONE;

    if (IsMvcProfile(par.mfx.CodecProfile))
    {
        mfxExtCodingOption & extOpt = GetExtBufferRef(par);
        mfxExtMVCSeqDesc * extMvc   = GetExtBuffer(par);
        sts = CheckAndFixMVCSeqDesc(extMvc, extOpt.ViewOutput == MFX_CODINGOPTION_ON);
        if (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts)
        {
            checkSts = sts;
        }
        else if (sts < MFX_ERR_NONE)
        {
            MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
        }
        sts = CheckVideoParamMvcQueryLike(par); // check and correct mvc per-view bitstream, buffer, level
        switch (sts)
        {
        case MFX_ERR_UNSUPPORTED:
            MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
        case MFX_ERR_INVALID_VIDEO_PARAM:
        case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
            MFX_RETURN(sts);
        case MFX_WRN_INCOMPATIBLE_VIDEO_PARAM:
            checkSts = sts;
            break;
        default:
            break;
        }
    }

    sts = CheckVideoParamQueryLike(par, hwCaps, platform, vaType, config);
    switch (sts)
    {
    case MFX_ERR_UNSUPPORTED:
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
    case MFX_ERR_INVALID_VIDEO_PARAM:
    case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
        MFX_RETURN(sts);
    case MFX_WRN_INCOMPATIBLE_VIDEO_PARAM:
        checkSts = sts;
        break;
    default:
        break;
    }

    if (par.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY)
    {
        MFX_CHECK(setExtAlloc, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    auto const supportedMemoryType =
           (par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY);

    MFX_CHECK(supportedMemoryType || (par.Protected == 0), MFX_ERR_INVALID_VIDEO_PARAM);

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_ICQ &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_LA_ICQ
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
         && !IsSvcProfile(par.mfx.CodecProfile)
#endif
        )
        MFX_CHECK(par.calcParam.targetKbps > 0, MFX_ERR_INVALID_VIDEO_PARAM);

    if (extOpt3.NumSliceI || extOpt3.NumSliceP || extOpt3.NumSliceB)
    {
        // application should set number of slices for all 3 slice types at once
        MFX_CHECK(extOpt3.NumSliceI > 0, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(extOpt3.NumSliceP > 0, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(extOpt3.NumSliceB > 0, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    SetDefaults(par, hwCaps, setExtAlloc, platform, vaType, config);

    sts = CheckForAllowedH264SpecViolations(par);
    if (sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
        checkSts = sts;

#if defined(MFX_ENABLE_ENCTOOLS_LPLA)
    mfxExtCodingOption2 & extOpt2 = GetExtBufferRef(par);
    // for game streaming scenario, if option enable lowpower lookahead, check encoder's capability
    if (IsLpLookaheadSupported(extOpt3.ScenarioInfo, extOpt2.LookAheadDepth, par.mfx.RateControlMethod))
    {
        MFX_CHECK(hwCaps.ddi_caps.LookaheadBRCSupport, MFX_ERR_INVALID_VIDEO_PARAM);
    }
#endif

    MFX_RETURN(checkSts);
}

/*
 * utils for debug purposes
 */
struct Bool
{
    explicit Bool(bool val) : m_val(val) {}
    Bool(Bool const & rhs) : m_val(rhs.m_val) {}
    Bool & operator =(Bool const & rhs) { m_val = rhs.m_val; return *this; }
    operator bool() { return m_val; }

    Bool & operator =(bool val)
    {
        //__asm { int 3 }
        m_val = val;
        return *this;
    }

private:
    bool m_val;
};

mfxStatus MfxHwH264Encode::CheckAndFixRectQueryLike(
    MfxVideoParam const & par,
    mfxRectDesc *         rect)
{
    mfxStatus checkSts = MFX_ERR_NONE;

    if (rect->Left == 0 && rect->Right == 0 && rect->Top == 0 && rect->Bottom == 0)
        MFX_RETURN(checkSts);

    checkSts = CheckAndFixOpenRectQueryLike(par, rect);

    MFX_RETURN(checkSts);
}

/*
The function does following
align down Left Top corner to Mb  i.e. 15->0
align up Right and Bottom corner to Mb  i.e. 15->16

checks that aligned rect is 'open '  i.e. Left < Right and Top < Bottom
check that Right and Bottom fits in Frame
i.e. Right(aligned up) <= par.mfx.FrameInfo.Width( must be aligned)

returns:
MFX_WRN_INCOMPATIBLE_VIDEO_PARAM - if any has been aligned
MFX_ERR_UNSUPPORTED - if Left < Right and Top < Bottom
MFX_ERR_UNSUPPORTED - if Right or Bottom
*/
mfxStatus MfxHwH264Encode::CheckAndFixOpenRectQueryLike(
    MfxVideoParam const & par,
    mfxRectDesc *         rect)
{
    mfxStatus checkSts = MFX_ERR_NONE;

    // check that rectangle is aligned to MB, correct it if not
    if (!CheckMbAlignment(rect->Left))          checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    if (!CheckMbAlignment(rect->Top))           checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    if (!CheckMbAlignmentAndUp(rect->Right))    checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    if (!CheckMbAlignmentAndUp(rect->Bottom))   checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

    // check that rectangle dimensions don't conflict with each other
    if (par.mfx.FrameInfo.Width)
    {
        MFX_CHECK(CheckRangeDflt(rect->Left, mfxU32(0), mfxU32(par.mfx.FrameInfo.Width - 16), mfxU32(0)), MFX_ERR_UNSUPPORTED);
        if (rect->Right > par.mfx.FrameInfo.Width)
        {
            rect->Right = par.mfx.FrameInfo.Width;
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    if (rect->Right <= rect->Left)
    {
        rect->Left = 0;
        rect->Right = 0;
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    if (par.mfx.FrameInfo.Height)
    {
        MFX_CHECK(CheckRangeDflt(rect->Top, mfxU32(0), mfxU32(par.mfx.FrameInfo.Height - 16), mfxU32(0)), MFX_ERR_UNSUPPORTED);
        if (rect->Bottom > par.mfx.FrameInfo.Height)
        {
            rect->Bottom = par.mfx.FrameInfo.Height;
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    if (rect->Bottom <= rect->Top)
    {
        rect->Top = 0;
        rect->Bottom = 0;
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    MFX_RETURN(checkSts);
}

mfxStatus MfxHwH264Encode::CheckAndFixRoiQueryLike(
    MfxVideoParam const & par,
    mfxRoiDesc *          roi,
    mfxU16                roiMode)
{
    mfxStatus checkSts = CheckAndFixOpenRectQueryLike(par, (mfxRectDesc*)roi);

    // check QP
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        if (!CheckRangeDflt(roi->ROIValue, -51, 51, 0))
            checkSts = MFX_ERR_UNSUPPORTED;
    }
    else
    {
        if (roiMode == MFX_ROI_MODE_QP_DELTA && !CheckRangeDflt(roi->ROIValue, -51, 51, 0))
            checkSts = MFX_ERR_UNSUPPORTED;

        else if (roiMode == MFX_ROI_MODE_PRIORITY && !CheckRangeDflt(roi->ROIValue, -3, 3, 0))
            checkSts = MFX_ERR_UNSUPPORTED;
    }

    MFX_RETURN(checkSts);
}

mfxStatus MfxHwH264Encode::CheckAndFixMovingRectQueryLike(
    MfxVideoParam const & par,
    mfxMovingRectDesc *   rect)
{
    mfxStatus checkSts = CheckAndFixRectQueryLike(par, (mfxRectDesc*)rect);

    if (par.mfx.FrameInfo.Width)
        if(!CheckRangeDflt(rect->SourceLeft, mfxU32(0), mfxU32(par.mfx.FrameInfo.Width - 1), mfxU32(0))) checkSts = MFX_ERR_UNSUPPORTED;

    if (par.mfx.FrameInfo.Height)
        if(!CheckRangeDflt(rect->SourceTop, mfxU32(0), mfxU32(par.mfx.FrameInfo.Height - 1), mfxU32(0))) checkSts = MFX_ERR_UNSUPPORTED;

    MFX_RETURN(checkSts);
}

//typedef bool Bool;

mfxStatus MfxHwH264Encode::CheckVideoParamQueryLike(
    MfxVideoParam &         par,
    MFX_ENCODE_CAPS const & hwCaps,
    eMFXHWType              platform,
    eMFXVAType              vaType,
    eMFXGTConfig            config)
{
    Bool unsupported(false);
    Bool changed(false);
    Bool warning(false);
    (void)config;
    mfxExtCodingOption *       extOpt       = GetExtBuffer(par);
    mfxExtCodingOptionDDI *    extDdi       = GetExtBuffer(par);
    mfxExtVideoSignalInfo *    extVsi       = GetExtBuffer(par);
    mfxExtCodingOptionSPSPPS * extBits      = GetExtBuffer(par);
    mfxExtPictureTimingSEI *   extPt        = GetExtBuffer(par);
    mfxExtSpsHeader *          extSps       = GetExtBuffer(par);
    mfxExtPpsHeader *          extPps       = GetExtBuffer(par);
    mfxExtMVCSeqDesc *         extMvc       = GetExtBuffer(par);
    mfxExtAvcTemporalLayers *  extTemp      = GetExtBuffer(par);
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    mfxExtSVCSeqDesc *         extSvc       = GetExtBuffer(par);
#endif
    mfxExtCodingOption2 *      extOpt2      = GetExtBuffer(par);
    mfxExtEncoderROI *         extRoi       = GetExtBuffer(par);
    mfxExtCodingOption3 *      extOpt3      = GetExtBuffer(par);
    mfxExtChromaLocInfo *      extCli       = GetExtBuffer(par);
    mfxExtDirtyRect  *         extDirtyRect = GetExtBuffer(par);
    mfxExtMoveRect *           extMoveRect  = GetExtBuffer(par);
    mfxExtPredWeightTable *    extPwt       = GetExtBuffer(par);
#if defined(MFX_ENABLE_ENCTOOLS)
    mfxExtEncToolsConfig *     extConfig    = GetExtBuffer(par);
#endif
    mfxExtBRC *                extBRC       = GetExtBuffer(par);


    bool sliceRowAlligned = true;

    // check HW capabilities
    if (par.mfx.FrameInfo.Width  > hwCaps.ddi_caps.MaxPicWidth ||
        par.mfx.FrameInfo.Height > hwCaps.ddi_caps.MaxPicHeight)
        MFX_RETURN(Error(MFX_ERR_UNSUPPORTED));

    if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART1) != MFX_PICSTRUCT_PROGRESSIVE)
    {
        if (((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART1) == MFX_PICSTRUCT_FIELD_TFF) ||
            ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART1) == MFX_PICSTRUCT_FIELD_BFF))
        {   // TFF, BFF
            if (hwCaps.ddi_caps.NoInterlacedField)
            {
                par.mfx.FrameInfo.PicStruct = 0;
                MFX_RETURN(MFX_ERR_UNSUPPORTED);
            }
        }
        else
        {   // UNKNOWN or garbage
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            if (H264ECaps::IsVmeSupported(platform) && IsOn(par.mfx.LowPower))
                changed = true;
        }
    }
    if (hwCaps.ddi_caps.MaxNum_TemporalLayer != 0 &&
        hwCaps.ddi_caps.MaxNum_TemporalLayer < par.calcParam.numTemporalLayer)
        MFX_RETURN(Error(MFX_ERR_UNSUPPORTED));

    if (!CheckTriStateOption(par.mfx.LowPower)) changed = true;

    if (IsOn(par.mfx.LowPower))
    {
#if defined(MFX_ENABLE_AVCE_VDENC_B_FRAMES)
        if (par.mfx.GopRefDist > 1
            && !H264ECaps::IsVDEncBFrameSupported(platform)
            )
#else
        if (par.mfx.GopRefDist > 1)
#endif
        {
            changed = true;
            par.mfx.GopRefDist = 1;
        }

        if (par.mfx.RateControlMethod != 0 &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_CBR &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_VBR &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_QVBR &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_VCM &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_ICQ)
        {
            if (H264ECaps::IsRateControlLASupported(platform) &&
                bRateControlLA(par.mfx.RateControlMethod)) // non LA modes are used instead
            {
                changed = true;
                if ((par.mfx.RateControlMethod == MFX_RATECONTROL_LA) ||
                    (par.mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD))
                    par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
                else if (par.mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ)
                    par.mfx.RateControlMethod = MFX_RATECONTROL_ICQ;
            }
            else
            {
                unsupported = true;
                par.mfx.RateControlMethod = 0;
            }
        }

        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP
            && par.calcParam.cqpHrdMode == 0)
        {
            if (!CheckRange(par.mfx.QPI, 10, 51)) changed = true;
            if (!CheckRange(par.mfx.QPP, 10, 51)) changed = true;
            if (!CheckRange(par.mfx.QPB, 10, 51)) changed = true;
        }
    }

    if (par.mfx.GopRefDist > 1 && hwCaps.ddi_caps.SliceIPOnly)
    {
        changed = true;
        par.mfx.GopRefDist = 1;
    }

    // sps/pps parameters if present overrides corresponding fields of VideoParam
    if (extBits->SPSBuffer)
    {
        if (!CheckAgreementOfSequenceLevelParameters<FunctionQuery>(par, *extSps))
            changed = true;

        if (extBits->PPSBuffer)
            if (!CheckAgreementOfPictureLevelParameters<FunctionQuery>(par, *extPps))
                changed = true;
    }

    if (par.Protected != 0)
    {
        unsupported = true;
        par.Protected = 0;
    }

    if (par.IOPattern != 0)
    {
        if ((par.IOPattern & MFX_IOPATTERN_IN_MASK) != par.IOPattern)
        {
            changed = true;
            par.IOPattern &= MFX_IOPATTERN_IN_MASK;
        }

        if (   par.IOPattern != MFX_IOPATTERN_IN_VIDEO_MEMORY
            && par.IOPattern != MFX_IOPATTERN_IN_SYSTEM_MEMORY
            )
        {
            changed = true;
            par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        }
    }

    if (par.mfx.TargetUsage > 7)
    {
        changed = true;
        par.mfx.TargetUsage = 7;
    }


    if (par.mfx.GopPicSize > 0 &&
        par.mfx.GopRefDist > 0 &&
        par.mfx.GopRefDist > par.mfx.GopPicSize)
    {
        changed = true;
        par.mfx.GopRefDist = par.mfx.GopPicSize - 1;
    }

    if (par.mfx.GopRefDist > 1)
    {
        if (IsAvcBaseProfile(par.mfx.CodecProfile) ||
            par.mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH)
        {
            changed = true;
            par.mfx.GopRefDist = 1;
        }
    }

    if (par.mfx.RateControlMethod != 0 &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_CBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_VBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_WIDI_VBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_VCM &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_ICQ &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_QVBR &&
        !bRateControlLA(par.mfx.RateControlMethod))
    {
        changed = true;
        par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    }

    if(MFX_HW_VAAPI == vaType &&
       par.mfx.RateControlMethod != 0 &&
       par.mfx.RateControlMethod != MFX_RATECONTROL_CBR &&
       par.mfx.RateControlMethod != MFX_RATECONTROL_VBR &&
       par.mfx.RateControlMethod != MFX_RATECONTROL_VCM &&
       par.mfx.RateControlMethod != MFX_RATECONTROL_QVBR &&
       par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
       par.mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
       par.mfx.RateControlMethod != MFX_RATECONTROL_ICQ &&
       !bRateControlLA(par.mfx.RateControlMethod))
    {
        unsupported = true;
        par.mfx.RateControlMethod = 0;
    }

    bool laEnabled = true;
    // LA and FD supports only with VME
    if (!H264ECaps::IsVmeSupported(platform))
    {
        laEnabled = false;
        if (bRateControlLA(par.mfx.RateControlMethod))
        {
            unsupported = true;
            par.mfx.RateControlMethod = 0;
        }

        if (IsOn(extOpt3->FadeDetection))
        {
            unsupported = true;
            extOpt3->FadeDetection = 0;
        }
    }

    if (extOpt2->MaxSliceSize &&
        !(IsDriverSliceSizeControlEnabled(par, hwCaps) ||                                                           // driver slice control condition
          (SliceDividerType(hwCaps.ddi_caps.SliceStructure) == SliceDividerType::ARBITRARY_MB_SLICE && laEnabled))) // sw slice control condition
    {
        unsupported = true;
        extOpt2->MaxSliceSize = 0;
    }

    if (bRateControlLA(par.mfx.RateControlMethod) && IsOn(extOpt->CAVLC))
    {
        extOpt->CAVLC = MFX_CODINGOPTION_OFF;
        changed = true;
    }

    if (extOpt2->MaxSliceSize)
    {
        if (par.mfx.GopRefDist > 1)
        {
            changed = true;
            par.mfx.GopRefDist = 1;
        }
        if (par.mfx.RateControlMethod != MFX_RATECONTROL_LA &&
            !IsDriverSliceSizeControlEnabled(par, hwCaps))
        {
            par.mfx.RateControlMethod = MFX_RATECONTROL_LA;
            changed = true;
        }
        if (extOpt2->LookAheadDepth > 1)
        {
            extOpt2->LookAheadDepth = 1;
            changed = true;
        }
        if (IsOn(extOpt->FieldOutput))
        {
            unsupported = true;
            extOpt->FieldOutput = MFX_CODINGOPTION_UNKNOWN;
        }
        if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0)
        {
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            changed = true;
        }
        if (extDdi->LookAheadDependency != 0)
        {
            extDdi->LookAheadDependency = 0;
            changed = true;
        }
        if (par.AsyncDepth > 1 && par.mfx.LowPower != MFX_CODINGOPTION_ON)
        {
            par.AsyncDepth  = 1;
            changed = true;
        }
        if (extOpt2->NumMbPerSlice)
        {
            extOpt2->NumMbPerSlice = 0;
            unsupported = true;
        }
    }
    else if (extOpt2->LookAheadDepth > 0)
    {
        if (!bRateControlLA(par.mfx.RateControlMethod))
        {
#if defined(MFX_ENABLE_ENCTOOLS_LPLA)
            if (extOpt3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING)
            {
                if (!hwCaps.ddi_caps.LookaheadBRCSupport)
                    unsupported = true;
            }
            else
#endif
#if defined(MFX_ENABLE_ENCTOOLS)
            if(!(H264EncTools::isEncToolNeeded(par)) && !IsOn(extOpt2->ExtBRC))
#endif
            {
                changed = true;
                extOpt2->LookAheadDepth = 0;
            }
        }
        else if (par.mfx.GopRefDist > 0 && extOpt2->LookAheadDepth < 2 * par.mfx.GopRefDist)
        {
            changed = true;
            extOpt2->LookAheadDepth = 2 * par.mfx.GopRefDist;
        }

        if(bRateControlLA(par.mfx.RateControlMethod) && extOpt2->LookAheadDepth < 2 * par.mfx.NumRefFrame)
        {
            changed = true;
            extOpt2->LookAheadDepth =  2 * par.mfx.NumRefFrame;
        }
    }

    /* max allowed combination */
    if (par.mfx.FrameInfo.PicStruct > (MFX_PICSTRUCT_PART1|MFX_PICSTRUCT_PART2))
    { /* */
        unsupported = true;
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
    }

    if (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART2)
    { // repeat/double/triple flags are for EncodeFrameAsync
        changed = true;
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
    }

    mfxU16 picStructPart1 = par.mfx.FrameInfo.PicStruct;
    if (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART1)
    {
        if (picStructPart1 & (picStructPart1 - 1))
        { // more then one flag set
            changed = true;
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
        }
    }

    if (par.mfx.FrameInfo.Width & 1)
    {
        unsupported = true;
        par.mfx.FrameInfo.Width = 0;
    }

    if (par.mfx.FrameInfo.Height & 1)
    {
        unsupported = true;
        par.mfx.FrameInfo.Height = 0;
    }

    if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0 &&
        (par.mfx.FrameInfo.Height & 31) != 0)
    {
        unsupported = true;
        par.mfx.FrameInfo.PicStruct = 0;
        par.mfx.FrameInfo.Height = 0;
    }

    // driver doesn't support resolution 16xH
    if (par.mfx.FrameInfo.Width == 16)
    {
        unsupported = true;
        par.mfx.FrameInfo.Width = 0;
    }

    // driver doesn't support resolution Wx16
    if (par.mfx.FrameInfo.Height == 16)
    {
        unsupported = true;
        par.mfx.FrameInfo.Height = 0;
    }

    if (par.mfx.FrameInfo.Width > 0)
    {
        if (par.mfx.FrameInfo.CropX > par.mfx.FrameInfo.Width)
        {
            if (extBits->SPSBuffer)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            unsupported = true;
            par.mfx.FrameInfo.CropX = 0;
        }

        if (par.mfx.FrameInfo.CropX + par.mfx.FrameInfo.CropW > par.mfx.FrameInfo.Width)
        {
            if (extBits->SPSBuffer)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            unsupported = true;
            par.mfx.FrameInfo.CropW = par.mfx.FrameInfo.Width - par.mfx.FrameInfo.CropX;
        }
    }

    if (par.mfx.FrameInfo.Height > 0)
    {
        if (par.mfx.FrameInfo.CropY > par.mfx.FrameInfo.Height)
        {
            if (extBits->SPSBuffer)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            unsupported = true;
            par.mfx.FrameInfo.CropY = 0;
        }

        if (par.mfx.FrameInfo.CropY + par.mfx.FrameInfo.CropH > par.mfx.FrameInfo.Height)
        {
            if (extBits->SPSBuffer)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            unsupported = true;
            par.mfx.FrameInfo.CropH = par.mfx.FrameInfo.Height - par.mfx.FrameInfo.CropY;
        }
    }

    // WA for MVCe with d3d9. AsyncDepth parameter is checked here
    if (vaType == MFX_HW_D3D9 && par.AsyncDepth > 0 && IsMvcProfile(par.mfx.CodecProfile))
    {
        mfxU32 numEncs = (extOpt->ViewOutput == MFX_CODINGOPTION_ON) ? 2 : 1;
        mfxU32 numView = (extMvc->NumView == 0) ? 1 : extMvc->NumView;
        mfxU32 numSurfBitstream = mfxU32(CalcNumSurfBitstream(par) * (numEncs > 1 ? 1 : numView));
        mfxU32 numSurfRecon = mfxU32(CalcNumSurfRecon(par) * (numEncs > 1 ? 1 : numView));
        if (numSurfBitstream > 128 || numSurfRecon > 128)
        {
            changed = true;
            par.AsyncDepth = DEFAULT_ASYNC_DEPTH_TO_WA_D3D9_128_SURFACE_LIMIT;
        }
    }

    if (CorrectCropping(par) != MFX_ERR_NONE)
    { // cropping read from sps header always has correct alignment
        changed = true;
    }


    if (extOpt2->NumMbPerSlice != 0)
    {
        bool fieldCoding = (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0;
        mfxU16 widthInMbs  = par.mfx.FrameInfo.Width / 16;
        mfxU16 heightInMbs = par.mfx.FrameInfo.Height / 16 / (fieldCoding ? 2 : 1);

        if (   (SliceDividerType(hwCaps.ddi_caps.SliceStructure) == SliceDividerType::ONESLICE)
            || (SliceDividerType(hwCaps.ddi_caps.SliceStructure)  < SliceDividerType::ARBITRARY_MB_SLICE && (extOpt2->NumMbPerSlice % (par.mfx.FrameInfo.Width >> 4)))
            || (SliceDividerType(hwCaps.ddi_caps.SliceStructure) == SliceDividerType::ROW2ROW && ((extOpt2->NumMbPerSlice / widthInMbs) & ((extOpt2->NumMbPerSlice / widthInMbs) - 1)))
            || (widthInMbs * heightInMbs) < extOpt2->NumMbPerSlice)
        {
            extOpt2->NumMbPerSlice = 0;
            unsupported = true;
        }
    }

    if (par.mfx.NumSlice !=0 &&
      (extOpt3->NumSliceI != 0 || extOpt3->NumSliceP != 0 || extOpt3->NumSliceB != 0))
    {
        if(par.mfx.NumSlice != extOpt3->NumSliceI &&
           par.mfx.NumSlice != extOpt3->NumSliceP &&
           par.mfx.NumSlice != extOpt3->NumSliceB)
        {
            changed = true;
            par.mfx.NumSlice = 0;
        }
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_VCM &&
        hwCaps.ddi_caps.VCMBitrateControl == 0)
    {
        changed = true;
        par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
    }

    if (extOpt3->LowDelayBRC == MFX_CODINGOPTION_ON) {
        if (par.mfx.RateControlMethod != MFX_RATECONTROL_VBR && par.mfx.RateControlMethod != MFX_RATECONTROL_QVBR &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_VCM) {
            extOpt3->LowDelayBRC = MFX_CODINGOPTION_OFF;
            changed = true;
        }
        else {
            if (extOpt3->WinBRCMaxAvgKbps || extOpt3->WinBRCSize) {
                extOpt3->WinBRCMaxAvgKbps = 0;
                extOpt3->WinBRCSize = 0;
                changed = true;
            }
            if (par.mfx.GopRefDist != 0 && par.mfx.GopRefDist != 1) {
                par.mfx.GopRefDist = 1;
                changed = true;
            }
        }
    }

    if (par.mfx.NumSlice         != 0 &&
        par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        if (par.mfx.NumSlice > par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height / 256)
        {
            unsupported = true;
            par.mfx.NumSlice = 0;
        }

        bool fieldCoding = (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0;

        SliceDivider divider = MakeSliceDivider(
            (extOpt2->MaxSliceSize != 0) ? SliceDividerType::ARBITRARY_MB_SLICE : SliceDividerType(hwCaps.ddi_caps.SliceStructure),
            extOpt2->NumMbPerSlice,
            par.mfx.NumSlice,
            par.mfx.FrameInfo.Width / 16,
            par.mfx.FrameInfo.Height / 16 / (fieldCoding ? 2 : 1));

        if (par.mfx.NumSlice != divider.GetNumSlice())
        {
            changed = true;
            par.mfx.NumSlice = (mfxU16)divider.GetNumSlice();
        }
        if (extOpt3->NumSliceP == 0)
        {
            do
            {
                if (divider.GetNumMbInSlice() % (par.mfx.FrameInfo.Width / 16) != 0)
                    sliceRowAlligned = false;
            } while (divider.Next());
        }
    }

    if (extOpt3->NumSliceI       != 0 &&
        par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        if (extOpt3->NumSliceI > par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height / 256)
        {
            unsupported = true;
            extOpt3->NumSliceI = 0;
        }

        bool fieldCoding = (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0;

        SliceDivider divider = MakeSliceDivider(
            (extOpt2->MaxSliceSize != 0) ? SliceDividerType::ARBITRARY_MB_SLICE : SliceDividerType(hwCaps.ddi_caps.SliceStructure),
            extOpt2->NumMbPerSlice,
            extOpt3->NumSliceI,
            par.mfx.FrameInfo.Width / 16,
            par.mfx.FrameInfo.Height / 16 / (fieldCoding ? 2 : 1));

        if (extOpt3->NumSliceI != divider.GetNumSlice())
        {
            changed = true;
            extOpt3->NumSliceI = (mfxU16)divider.GetNumSlice();
        }
    }

    if (extOpt3->NumSliceP       != 0 &&
        par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        if (extOpt3->NumSliceP > par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height / 256)
        {
            unsupported = true;
            extOpt3->NumSliceP = 0;
        }

        bool fieldCoding = (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0;

        SliceDivider divider = MakeSliceDivider(
            (extOpt2->MaxSliceSize != 0) ? SliceDividerType::ARBITRARY_MB_SLICE : SliceDividerType(hwCaps.ddi_caps.SliceStructure),
            extOpt2->NumMbPerSlice,
            extOpt3->NumSliceP,
            par.mfx.FrameInfo.Width / 16,
            par.mfx.FrameInfo.Height / 16 / (fieldCoding ? 2 : 1));

        if (extOpt3->NumSliceP != divider.GetNumSlice())
        {
            changed = true;
            extOpt3->NumSliceP = (mfxU16)divider.GetNumSlice();
        }

        do
        {
            if (divider.GetNumMbInSlice() % (par.mfx.FrameInfo.Width / 16) != 0)
                sliceRowAlligned = false;
        } while (divider.Next());
    }

    if (extOpt3->NumSliceB       != 0 &&
        par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        if (extOpt3->NumSliceB > par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height / 256)
        {
            unsupported = true;
            extOpt3->NumSliceB = 0;
        }

        bool fieldCoding = (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0;

        SliceDivider divider = MakeSliceDivider(
            (extOpt2->MaxSliceSize != 0) ? SliceDividerType::ARBITRARY_MB_SLICE : SliceDividerType(hwCaps.ddi_caps.SliceStructure),
            extOpt2->NumMbPerSlice,
            extOpt3->NumSliceB,
            par.mfx.FrameInfo.Width / 16,
            par.mfx.FrameInfo.Height / 16 / (fieldCoding ? 2 : 1));

        if (extOpt3->NumSliceB != divider.GetNumSlice())
        {
            changed = true;
            extOpt3->NumSliceB = (mfxU16)divider.GetNumSlice();
        }
    }

    if (par.mfx.FrameInfo.ChromaFormat != 0 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV422 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
    {
        if (extBits->SPSBuffer)
            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

        changed = true;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    }

    if (par.mfx.FrameInfo.FourCC != 0 &&
        par.mfx.FrameInfo.FourCC != MFX_FOURCC_NV12 &&
        par.mfx.FrameInfo.FourCC != MFX_FOURCC_RGB4 &&
        par.mfx.FrameInfo.FourCC != MFX_FOURCC_BGR4 &&
        par.mfx.FrameInfo.FourCC != MFX_FOURCC_YUY2 &&
        par.mfx.FrameInfo.FourCC != MFX_FOURCC_AYUV)
    {
        unsupported = true;
        par.mfx.FrameInfo.FourCC = 0;
    }

    if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_NV12 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_MONOCHROME)
    {
        if (extBits->SPSBuffer)
            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

        changed = true;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    }

    if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_YUY2 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV422)
    {
        if (extBits->SPSBuffer)
            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

        changed = true;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
    }

    if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_AYUV &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
    {
        if (extBits->SPSBuffer)
            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

        changed = true;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
    }

    if ((par.mfx.FrameInfo.FourCC == MFX_FOURCC_RGB4 || par.mfx.FrameInfo.FourCC == MFX_FOURCC_BGR4) &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
    {
        if (extBits->SPSBuffer)
            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

        changed = true;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
    }

    if (hwCaps.ddi_caps.Color420Only &&
        (par.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV422 ||
         par.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV444))
    {
        unsupported = true;
        par.mfx.FrameInfo.ChromaFormat = 0;
    }

    if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
        (par.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV422 ||
         par.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV444))
    {
        changed = true;
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    if (!CheckTriStateOption(extOpt2->DisableVUI))         changed = true;
    if (!CheckTriStateOption(extOpt->VuiNalHrdParameters)) changed = true;
    if (!CheckTriStateOption(extOpt->VuiVclHrdParameters)) changed = true;
    if (!CheckTriStateOption(extOpt2->FixedFrameRate))     changed = true;
    if (!CheckTriStateOption(extOpt3->LowDelayHrd))        changed = true;

    if (IsOn(extOpt2->DisableVUI))
    {
        bool contradictoryVuiParam = false;
        if (!CheckTriStateOptionForOff(extOpt3->TimingInfoPresent))      contradictoryVuiParam = true;
        if (!CheckTriStateOptionForOff(extOpt3->OverscanInfoPresent))    contradictoryVuiParam = true;
        if (!CheckTriStateOptionForOff(extOpt3->AspectRatioInfoPresent)) contradictoryVuiParam = true;
        if (!CheckTriStateOptionForOff(extOpt3->BitstreamRestriction))   contradictoryVuiParam = true;
        if (!CheckTriStateOptionForOff(extOpt->VuiNalHrdParameters))     contradictoryVuiParam = true;
        if (!CheckTriStateOptionForOff(extOpt->VuiVclHrdParameters))     contradictoryVuiParam = true;
        if (!CheckTriStateOptionForOff(extOpt->PicTimingSEI))            contradictoryVuiParam = true;

        if (contradictoryVuiParam)
        {
            if (extBits->SPSBuffer)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            changed = true;
        }
    }

    if (IsOff(extOpt3->OverscanInfoPresent))
    {
        if (IsOn(extOpt3->OverscanAppropriate))
        {
            changed = true;
            extOpt3->OverscanAppropriate = MFX_CODINGOPTION_OFF;
        }
    }
    if (IsOff(extOpt3->BitstreamRestriction))
    {
        if (IsOn(extOpt3->MotionVectorsOverPicBoundaries))
        {
            changed = true;
            extOpt3->MotionVectorsOverPicBoundaries = MFX_CODINGOPTION_OFF;
        }
    }
    if (IsOff(extOpt3->TimingInfoPresent))
    {
        if (IsOn(extOpt2->FixedFrameRate))
        {
            changed = true;
            extOpt2->FixedFrameRate = MFX_CODINGOPTION_OFF;
        }
    }

    if ((par.mfx.FrameInfo.FrameRateExtN == 0) !=
        (par.mfx.FrameInfo.FrameRateExtD == 0))
    {
        if (extBits->SPSBuffer && !IsOff(extOpt3->TimingInfoPresent))
            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

        unsupported = true;
        par.mfx.FrameInfo.FrameRateExtN = 0;
        par.mfx.FrameInfo.FrameRateExtD = 0;
    }

    if (!IsOff(extOpt3->TimingInfoPresent) &&
        par.mfx.FrameInfo.FrameRateExtN != 0 &&
        par.mfx.FrameInfo.FrameRateExtD != 0 &&
        par.mfx.FrameInfo.FrameRateExtN > mfxU64(172) * par.mfx.FrameInfo.FrameRateExtD)
    {
        if (extBits->SPSBuffer) // frame_rate read from sps
            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

        unsupported = true;
        par.mfx.FrameInfo.FrameRateExtN = 0;
        par.mfx.FrameInfo.FrameRateExtD = 0;
    }

    if (IsOff(extOpt3->TimingInfoPresent) &&
        IsOn(extOpt2->FixedFrameRate))
    {
        // if timing_info_present_flag is 0, fixed_frame_rate_flag is inferred to be 0 (Annex E.2.1)
        if (extBits->SPSBuffer)
            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

        changed = true;
        extOpt2->FixedFrameRate = MFX_CODINGOPTION_OFF;
    }

    if (!IsOff(extOpt3->TimingInfoPresent))
    {
        if (IsOn(extOpt2->FixedFrameRate) &&
            IsOn(extOpt3->LowDelayHrd))
        {
            if (extBits->SPSBuffer)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            changed = true;
            extOpt3->LowDelayHrd = MFX_CODINGOPTION_OFF;
        }
    }

    if (par.mfx.CodecProfile != MFX_PROFILE_UNKNOWN && !IsValidCodingProfile(par.mfx.CodecProfile))
    {
        if (extBits->SPSBuffer)
            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

        if (par.mfx.CodecProfile == MFX_PROFILE_AVC_MULTIVIEW_HIGH)
            MFX_RETURN(Error(MFX_ERR_UNSUPPORTED));

        changed = true;
        par.mfx.CodecProfile = MFX_PROFILE_UNKNOWN;
    }

    if (par.mfx.CodecLevel != MFX_LEVEL_UNKNOWN && !IsValidCodingLevel(par.mfx.CodecLevel))
    {
        if (extBits->SPSBuffer)
            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

        changed = true;
        par.mfx.CodecLevel = MFX_LEVEL_UNKNOWN;
    }

    if (par.mfx.NumRefFrame > 16)
    {
        par.mfx.NumRefFrame = 16;
        changed = true;
    }

    if (par.mfx.NumRefFrame != 0)
    {
        if ((par.mfx.NumRefFrame & 1) &&
            (hwCaps.ddi_caps.HeaderInsertion) &&
            (par.Protected || !IsOff(extOpt->NalHrdConformance)))
        {
            // when driver writes headers it can write only even values of num_ref_frames
            if (extBits->SPSBuffer)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            changed = true;
            par.mfx.NumRefFrame++;
        }
    }

    if (par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        mfxU16 minLevel = GetLevelLimitByFrameSize(par);
        if (minLevel == 0)
        {
            if (extBits->SPSBuffer)
            {
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
            }
            else if (par.mfx.CodecLevel != 0)
            {
                changed = true; // warning should be returned if frame size exceeds maximum value supported by AVC standard
                if (par.mfx.CodecLevel < GetMaxSupportedLevel())
                    par.mfx.CodecLevel = GetMaxSupportedLevel();
            }
        }
        else if (par.mfx.CodecLevel != 0 && par.mfx.CodecLevel < minLevel)
        {
            if (extBits->SPSBuffer)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            changed = true;
            par.mfx.CodecLevel = minLevel;
        }
    }

    if (!IsOff(extOpt3->TimingInfoPresent)  &&
        par.mfx.FrameInfo.Width         != 0 &&
        par.mfx.FrameInfo.Height        != 0 &&
        par.mfx.FrameInfo.FrameRateExtN != 0 &&
        par.mfx.FrameInfo.FrameRateExtD != 0)
    {
        mfxU16 minLevel = GetLevelLimitByMbps(par);
        if (minLevel == 0)
        {
            if (extBits->SPSBuffer)
            {
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
            }
            else if (par.mfx.CodecLevel != 0)
            {
                changed = true; // warning should be returned if MB processing rate exceeds maximum value supported by AVC standard
                if (par.mfx.CodecLevel < GetMaxSupportedLevel())
                    par.mfx.CodecLevel = GetMaxSupportedLevel();
            }
        }
        else if (par.mfx.CodecLevel != 0 && par.mfx.CodecLevel < minLevel)
        {
            if (extBits->SPSBuffer)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            changed = true;
            par.mfx.CodecLevel = minLevel;
        }
    }

    if (par.mfx.NumRefFrame      != 0 &&
        par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        mfxU16 minLevel = GetLevelLimitByDpbSize(par);
        if (minLevel == 0)
        {
            if (extBits->SPSBuffer)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            changed = false;
            par.mfx.CodecLevel = MFX_LEVEL_AVC_52;
            par.mfx.NumRefFrame = GetMaxNumRefFrame(par);
        }
        else if (par.mfx.CodecLevel != 0 && par.mfx.CodecLevel < minLevel)
        {
            if (extBits->SPSBuffer)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            changed = false;
            par.mfx.CodecLevel = minLevel;
        }
    }

    if (IsAvcHighProfile(par.mfx.CodecProfile) && (par.mfx.CodecProfile & MASK_CONSTRAINT_SET0123_FLAG))
    {
        changed = true;
        par.mfx.CodecProfile = par.mfx.CodecProfile & ~MASK_CONSTRAINT_SET0123_FLAG;
    }

    if (!CheckTriStateOption(extOpt->RateDistortionOpt))        changed = true;
    if (!CheckTriStateOption(extOpt->EndOfSequence))            changed = true;
    if (!CheckTriStateOption(extOpt->FramePicture))             changed = true;
    if (!CheckTriStateOption(extOpt->CAVLC))                    changed = true;
    if (!CheckTriStateOption(extOpt->RefPicListReordering))     changed = true;
    if (!CheckTriStateOption(extOpt->ResetRefList))             changed = true;
    if (!CheckTriStateOption(extOpt->RefPicMarkRep))            changed = true;
    if (!CheckTriStateOption(extOpt->FieldOutput))              changed = true;
    if (!CheckTriStateOption(extOpt->AUDelimiter))              changed = true;
    if (!CheckTriStateOption(extOpt->EndOfStream))              changed = true;
    if (!CheckTriStateOption(extOpt->PicTimingSEI))             changed = true;
    if (!CheckTriStateOption(extOpt->SingleSeiNalUnit))         changed = true;
    if (!CheckTriStateOption(extOpt->NalHrdConformance))        changed = true;
    if (!CheckTriStateOption(extDdi->RefRaw))                   changed = true;
    if (!CheckTriStateOption(extDdi->DirectSpatialMvPredFlag))  changed = true;
    if (!CheckTriStateOption(extDdi->Hme))                      changed = true;
    if (!CheckTriStateOption(extOpt2->BitrateLimit))            changed = true;
    if (!CheckTriStateOption(extOpt2->MBBRC))                   changed = true;
    //if (!CheckTriStateOption(extOpt2->ExtBRC))                  changed = true;
    if (!CheckTriStateOption(extOpt2->RepeatPPS))               changed = true;
    if (!CheckTriStateOption(extOpt2->UseRawRef))               changed = true;

    if (!CheckTriStateOption(extOpt3->EnableQPOffset))              changed = true;
    if (!CheckTriStateOption(extOpt3->DirectBiasAdjustment))        changed = true;
    if (!CheckTriStateOption(extOpt3->GlobalMotionBiasAdjustment))  changed = true;

    if (   IsOn(extOpt3->EnableQPOffset)
        && (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP
            || (par.mfx.GopRefDist > 1 && extOpt2->BRefType == MFX_B_REF_OFF)
            || (par.mfx.GopRefDist == 1 && extOpt3->PRefType == MFX_P_REF_SIMPLE)))
    {
        extOpt3->EnableQPOffset = MFX_CODINGOPTION_OFF;
        changed = true;
    }

    if (IsOn(extOpt3->EnableQPOffset))
    {
        mfxU16 minQP = 1;
        mfxU16 maxQP = 51;
        mfxI16 QPX = (par.mfx.GopRefDist == 1) ? par.mfx.QPP : par.mfx.QPB;

        if (QPX)
        {
            for (mfxI16 i = 0; i < 8; i++)
                if (!CheckRange(extOpt3->QPOffset[i], minQP - QPX, maxQP - QPX))
                    changed = true;
        }
    }

    if (!CheckRangeDflt(extOpt3->MVCostScalingFactor, 0, 3, 0)) changed = true;

    if (extOpt3->MVCostScalingFactor && !IsOn(extOpt3->GlobalMotionBiasAdjustment))
    {
        changed = true;
        extOpt3->MVCostScalingFactor = 0;
    }

    if (extOpt2->BufferingPeriodSEI > MFX_BPSEI_IFRAME)
    {
        changed = true;
        extOpt2->BufferingPeriodSEI = 0;
    }

    if (IsMvcProfile(par.mfx.CodecProfile) && IsOn(extOpt->FieldOutput))
    {
        unsupported = true;
        extOpt->FieldOutput = MFX_CODINGOPTION_UNKNOWN;
    }


    if (par.mfx.RateControlMethod == MFX_RATECONTROL_VCM &&
        par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
    {
        changed = true;
        par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
    }
    if (IsOn(extOpt2->MBBRC))
    {
        if ((isSWBRC(par) && hwCaps.ddi_caps.QpAdjustmentSupport == 0 && hwCaps.ddi_caps.MbQpDataSupport == 0) ||
            (!isSWBRC(par) && (hwCaps.ddi_caps.MBBRCSupport == 0 && hwCaps.ddi_caps.ICQBRCSupport == 0)))
        {
            changed = true;
            extOpt2->MBBRC = MFX_CODINGOPTION_OFF;
        }
    }

    if (extOpt2->BRefType > 2)
    {
        changed = true;
        extOpt2->BRefType = MFX_B_REF_UNKNOWN;
    }

    if (extOpt2->BRefType && extOpt2->BRefType != MFX_B_REF_OFF &&
        par.mfx.GopRefDist != 0 && par.mfx.GopRefDist < 3)
    {
        changed = true;
        extOpt2->BRefType = MFX_B_REF_OFF;
    }

    if (extOpt->IntraPredBlockSize != MFX_BLOCKSIZE_UNKNOWN &&
        extOpt->IntraPredBlockSize != MFX_BLOCKSIZE_MIN_16X16 &&
        extOpt->IntraPredBlockSize != MFX_BLOCKSIZE_MIN_8X8 &&
        extOpt->IntraPredBlockSize != MFX_BLOCKSIZE_MIN_4X4)
    {
        changed = true;
        extOpt->IntraPredBlockSize = MFX_BLOCKSIZE_UNKNOWN;
    }

    if (extOpt->InterPredBlockSize != MFX_BLOCKSIZE_UNKNOWN &&
        extOpt->InterPredBlockSize != MFX_BLOCKSIZE_MIN_16X16 &&
        extOpt->InterPredBlockSize != MFX_BLOCKSIZE_MIN_8X8 &&
        extOpt->InterPredBlockSize != MFX_BLOCKSIZE_MIN_4X4)
    {
        changed = true;
        extOpt->InterPredBlockSize = MFX_BLOCKSIZE_UNKNOWN;
    }

    if (extOpt->MVPrecision != MFX_MVPRECISION_UNKNOWN &&
        extOpt->MVPrecision != MFX_MVPRECISION_QUARTERPEL &&
        extOpt->MVPrecision != MFX_MVPRECISION_HALFPEL &&
        extOpt->MVPrecision != MFX_MVPRECISION_INTEGER)
    {
        changed = true;
        extOpt->MVPrecision = MFX_MVPRECISION_UNKNOWN;
    }

    if (extOpt->MaxDecFrameBuffering != 0)
    {
        if (par.mfx.CodecLevel       != 0 &&
            par.mfx.FrameInfo.Width  != 0 &&
            par.mfx.FrameInfo.Height != 0)
        {
            mfxU16 maxDpbSize = GetMaxNumRefFrame(par);
            if (extOpt->MaxDecFrameBuffering > maxDpbSize)
            {
                if (extBits->SPSBuffer)
                    MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                changed = true;
                extOpt->MaxDecFrameBuffering = maxDpbSize;
            }
        }

        if (par.mfx.NumRefFrame != 0)
        {
            if (extOpt->MaxDecFrameBuffering < par.mfx.NumRefFrame)
            {
                if (extBits->SPSBuffer)
                    MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                changed = true;
                extOpt->MaxDecFrameBuffering = par.mfx.NumRefFrame;
            }
        }
    }

    if ((IsOff(extOpt->CAVLC)) &&
        (IsAvcBaseProfile(par.mfx.CodecProfile) || hwCaps.ddi_caps.NoCabacSupport))
    {
        if (extBits->SPSBuffer)
            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

        changed = true;
        extOpt->CAVLC = MFX_CODINGOPTION_ON;
    }

    if (extOpt->IntraPredBlockSize >= MFX_BLOCKSIZE_MIN_8X8)
    {
        if (IsAvcBaseProfile(par.mfx.CodecProfile) || par.mfx.CodecProfile == MFX_PROFILE_AVC_MAIN)
        {
            if (extBits->PPSBuffer && extPps->transform8x8ModeFlag)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            changed = true;
            extOpt->IntraPredBlockSize = MFX_BLOCKSIZE_MIN_16X16;
        }
    }

    if ((par.mfx.CodecProfile != MFX_PROFILE_UNKNOWN) && (par.mfx.CodecProfile != MFX_PROFILE_AVC_HIGH)
        && IsOn(extDdi->Transform8x8Mode))
    {
        unsupported = true;
        extDdi->Transform8x8Mode = MFX_CODINGOPTION_UNKNOWN;
    }

    if (par.calcParam.cqpHrdMode)
    {
        if (IsOn(extOpt->RecoveryPointSEI))
        {
            changed = true;
            extOpt->RecoveryPointSEI = MFX_CODINGOPTION_OFF;
        }
        if (IsOn(extOpt->PicTimingSEI))
        {
            changed = true;
            extOpt->PicTimingSEI = MFX_CODINGOPTION_OFF;
        }
        if (extOpt2->BufferingPeriodSEI != MFX_BPSEI_DEFAULT)
        {
            changed = true;
            extOpt2->BufferingPeriodSEI = MFX_BPSEI_DEFAULT;
        }
    }

    if ((IsOn(extOpt->VuiNalHrdParameters) ||IsOn(extOpt->NalHrdConformance)) &&
        par.mfx.RateControlMethod != 0 &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_CBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_VBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_QVBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_LA_HRD &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_VCM &&
        par.calcParam.cqpHrdMode == 0)
    {
        changed = true;
        extOpt->NalHrdConformance = MFX_CODINGOPTION_OFF;
        extOpt->VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
    }

    if (IsOn(extOpt->VuiNalHrdParameters)
        && IsOff(extOpt->NalHrdConformance)
        && par.calcParam.cqpHrdMode == 0)
    {
        changed = true;
        extOpt->VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
    }

    if (IsOn(extOpt->VuiVclHrdParameters) &&
        IsOff(extOpt->NalHrdConformance))
    {
        changed = true;
        extOpt->VuiVclHrdParameters = MFX_CODINGOPTION_OFF;
    }

    if (IsOn(extOpt->VuiVclHrdParameters) &&
        par.mfx.RateControlMethod != 0 &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_VBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_QVBR)
    {
        changed = true;
        extOpt->VuiVclHrdParameters = MFX_CODINGOPTION_OFF;
    }

    if(IsOff(extOpt->VuiNalHrdParameters) &&
       IsOff(extOpt->VuiVclHrdParameters) &&
       IsOn(extOpt3->LowDelayHrd))
    {
        changed = true;
        extOpt3->LowDelayHrd = MFX_CODINGOPTION_OFF;
    }

    if (!IsOn(extOpt2->DisableVUI) &&
        IsOff(extOpt->VuiNalHrdParameters) &&
        IsOff(extOpt->VuiVclHrdParameters) &&
        extOpt2->FixedFrameRate != MFX_CODINGOPTION_UNKNOWN &&
        extOpt3->LowDelayHrd != MFX_CODINGOPTION_UNKNOWN &&
        IsOn(extOpt2->FixedFrameRate) != IsOff(extOpt3->LowDelayHrd))
    {
        // when low_delay_hrd_flag isn't present in bitstream, it's value should be inferred to be equal to 1 - fixed_frame_rate_flag (Annex E.2.1)
        changed = true;
        if (IsOn(extOpt2->FixedFrameRate))
            extOpt3->LowDelayHrd = MFX_CODINGOPTION_OFF;
    }

    if (extDdi->WeightedBiPredIdc > 2)
    {
        changed = true;
        extDdi->WeightedBiPredIdc = 0;
    }

    if (extDdi->CabacInitIdcPlus1 > 3)
    {
        changed = true;
        extDdi->CabacInitIdcPlus1 = 0;
    }

    if (extOpt2->LookAheadDS > MFX_LOOKAHEAD_DS_4x) // invalid LookAheadDS
    {
        extOpt2->LookAheadDS = MFX_LOOKAHEAD_DS_UNKNOWN;
        changed = true;
    }

    if (extOpt2->LookAheadDS == MFX_LOOKAHEAD_DS_4x)
    {
        extOpt2->LookAheadDS = MFX_LOOKAHEAD_DS_2x;
        changed = true;
    }

    if (extOpt2->LookAheadDS > MFX_LOOKAHEAD_DS_OFF)
    {
        par.calcParam.widthLa  = mfx::align2_value(mfxU16(par.mfx.FrameInfo.Width  / LaDSenumToFactor(extOpt2->LookAheadDS)), 16);
        par.calcParam.heightLa = mfx::align2_value(mfxU16(par.mfx.FrameInfo.Height / LaDSenumToFactor(extOpt2->LookAheadDS)), 16);
    } else
    {
        par.calcParam.widthLa  = par.mfx.FrameInfo.Width;
        par.calcParam.heightLa = par.mfx.FrameInfo.Height;
    }

    if (!CheckRangeDflt(extVsi->VideoFormat,             0,   8, 5)) changed = true;
    if (!CheckRangeDflt(extVsi->ColourPrimaries,         0, 255, 2)) changed = true;
    if (!CheckRangeDflt(extVsi->TransferCharacteristics, 0, 255, 2)) changed = true;
    if (!CheckRangeDflt(extVsi->MatrixCoefficients,      0, 255, 2)) changed = true;
    if (!CheckFlag(extVsi->VideoFullRange, 0))                       changed = true;
    if (!CheckFlag(extVsi->ColourDescriptionPresent, 0))             changed = true;

    if (!CheckRangeDflt(extCli->ChromaLocInfoPresentFlag,       0, 1, 0)) changed = true;
    if (!CheckRangeDflt(extCli->ChromaSampleLocTypeTopField,    0, 5, 0)) changed = true;
    if (!CheckRangeDflt(extCli->ChromaSampleLocTypeBottomField, 0, 5, 0)) changed = true;

    if (   (IsOn(extOpt2->DisableVUI) && extCli->ChromaLocInfoPresentFlag)
        || (!extCli->ChromaLocInfoPresentFlag && (extCli->ChromaSampleLocTypeTopField || extCli->ChromaSampleLocTypeBottomField)))
    {
        extCli->ChromaLocInfoPresentFlag       = 0;
        extCli->ChromaSampleLocTypeTopField    = 0;
        extCli->ChromaSampleLocTypeBottomField = 0;
        changed = true;
    }

    // Keep the behavior on VME and set to OFF from ACM since we decided to deprecate this parameter on VDEnc.
    if (H264ECaps::IsVmeSupported(platform)) {
        if (extOpt2->BitrateLimit == MFX_CODINGOPTION_UNKNOWN)
            extOpt2->BitrateLimit = MFX_CODINGOPTION_ON;
    } else {
        if (extOpt2->BitrateLimit == MFX_CODINGOPTION_ON)
            changed = true;
        extOpt2->BitrateLimit = MFX_CODINGOPTION_OFF;
    }

    if (par.calcParam.cqpHrdMode == 0)
    {
        // regular check for compatibility of profile/level and BRC parameters
        if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP && par.calcParam.targetKbps != 0)
        {
            if (!IsOff(extOpt2->BitrateLimit)        &&
                par.mfx.FrameInfo.Width         != 0 &&
                par.mfx.FrameInfo.Height        != 0 &&
                par.mfx.FrameInfo.FrameRateExtN != 0 &&
                par.mfx.FrameInfo.FrameRateExtD != 0)
            {
                mfxF64 rawDataBitrate = 12.0 * par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height *
                    par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD;
                mfxU32 minTargetKbps = mfxU32(std::min<mfxF64>(0xffffffff, rawDataBitrate / 1000.0 / 700.0));

                if (par.calcParam.targetKbps < minTargetKbps)
                {
                    changed = true;
                    par.calcParam.targetKbps = minTargetKbps;
                }
            }

            if ((!IsOff(extOpt->NalHrdConformance) && extBits->SPSBuffer == 0) || IsOn(extOpt->VuiNalHrdParameters) || IsOn(extOpt->VuiVclHrdParameters))
            {
                mfxU16 profile = std::max<mfxU16>(MFX_PROFILE_AVC_BASELINE, par.mfx.CodecProfile & MASK_PROFILE_IDC);
                for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
                {
                    if (mfxU16 minLevel = GetLevelLimitByMaxBitrate(profile, par.calcParam.targetKbps))
                    {
                        if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0 && par.mfx.CodecLevel < minLevel)
                        {
                            if (extBits->SPSBuffer)
                                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                            changed = true;
                            par.mfx.CodecLevel   = minLevel;
                            par.mfx.CodecProfile = profile;
                        }
                        break;
                    }
                }

                if (profile == MFX_PROFILE_UNKNOWN)
                {
                    if (extBits->SPSBuffer)
                        MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                    if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0)
                    {
                        par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
                        par.mfx.CodecLevel = MFX_LEVEL_AVC_52;
                    }

                    changed = true;
                    par.calcParam.targetKbps = GetMaxBitrate(par) / 1000;
                }
            }
        }

        if (par.calcParam.targetKbps != 0 && par.calcParam.maxKbps != 0)
        {
            if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
            {
                if (par.calcParam.maxKbps != par.calcParam.targetKbps)
                {
                    changed = true;
                    if (extBits->SPSBuffer && (
                        extSps->vui.flags.nalHrdParametersPresent ||
                        extSps->vui.flags.vclHrdParametersPresent))
                        par.calcParam.targetKbps = par.calcParam.maxKbps;
                    else
                        par.calcParam.maxKbps = par.calcParam.targetKbps;
                }
            }
            else if (
                par.mfx.RateControlMethod == MFX_RATECONTROL_VCM ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_WIDI_VBR ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD)
            {
                if (par.calcParam.maxKbps < par.calcParam.targetKbps)
                {
                    if (extBits->SPSBuffer && (
                        extSps->vui.flags.nalHrdParametersPresent ||
                        extSps->vui.flags.vclHrdParametersPresent))
                        MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                    changed = true;
                    par.calcParam.maxKbps = par.calcParam.targetKbps;
                }
            }
        }

        if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP && par.calcParam.maxKbps != 0)
        {
            mfxU16 profile = std::max<mfxU16>(MFX_PROFILE_AVC_BASELINE, par.mfx.CodecProfile & MASK_PROFILE_IDC);
            for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
            {
                if (mfxU16 minLevel = GetLevelLimitByMaxBitrate(profile, par.calcParam.maxKbps))
                {
                    if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0 && par.mfx.CodecLevel < minLevel)
                    {
                        if (extBits->SPSBuffer)
                            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                        changed = true;
                        par.mfx.CodecLevel = minLevel;
                        par.mfx.CodecProfile = profile;
                    }
                    break;
                }
            }

            if (profile == MFX_PROFILE_UNKNOWN)
            {
                if (extBits->SPSBuffer)
                    MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0)
                {
                    par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
                    par.mfx.CodecLevel = MFX_LEVEL_AVC_52;
                }

                changed = true;
                par.calcParam.maxKbps = GetMaxBitrate(par) / 1000;
            }
        }

        if (par.calcParam.bufferSizeInKB != 0 && bRateControlLA(par.mfx.RateControlMethod) &&(par.mfx.RateControlMethod != MFX_RATECONTROL_LA_HRD))
        {
            mfxU32 uncompressedSizeInKb = GetMaxCodedFrameSizeInKB(par);
            if (par.calcParam.bufferSizeInKB < uncompressedSizeInKb)
            {
                changed = true;
                par.calcParam.bufferSizeInKB = uncompressedSizeInKb;
            }
        }

        if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR &&
            hwCaps.AVBRSupport == 0)
        {
            par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
            par.mfx.Accuracy = 0;
            par.calcParam.initialDelayInKB = 0;
            par.mfx.Convergence = 0;
            par.calcParam.maxKbps = 0;

            changed = true;
        }

        if (par.calcParam.bufferSizeInKB != 0)
        {
            if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            {
                mfxU32 uncompressedSizeInKb = GetMaxCodedFrameSizeInKB(par);
                if (par.calcParam.bufferSizeInKB < uncompressedSizeInKb)
                {
                    changed = true;
                    par.calcParam.bufferSizeInKB = uncompressedSizeInKb;
                }
            }
            else
            {
                mfxF64 avgFrameSizeInKB = 0;
                if (par.mfx.RateControlMethod       != MFX_RATECONTROL_AVBR &&
                    par.mfx.FrameInfo.FrameRateExtN != 0 &&
                    par.mfx.FrameInfo.FrameRateExtD != 0 &&
                    par.calcParam.targetKbps        != 0)
                {
                    mfxF64 frameRate = mfxF64(par.mfx.FrameInfo.FrameRateExtN) / par.mfx.FrameInfo.FrameRateExtD;
                    avgFrameSizeInKB = par.calcParam.targetKbps / frameRate / 8;

                    if (par.calcParam.bufferSizeInKB < 2 * avgFrameSizeInKB)
                    {
                        if (extBits->SPSBuffer && (
                            extSps->vui.flags.nalHrdParametersPresent ||
                            extSps->vui.flags.vclHrdParametersPresent))
                            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                        changed = true;
                        par.calcParam.bufferSizeInKB = mfxU32(2 * avgFrameSizeInKB + 1);
                    }
                }

                mfxU16 profile = std::max<mfxU16>(MFX_PROFILE_AVC_BASELINE, par.mfx.CodecProfile & MASK_PROFILE_IDC);
                for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
                {
                    if (mfxU16 minLevel = GetLevelLimitByBufferSize(profile, par.calcParam.bufferSizeInKB))
                    {
                        if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0 && par.mfx.CodecLevel < minLevel)
                        {
                            if (extBits->SPSBuffer)
                                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                            changed = true;
                            par.mfx.CodecLevel = minLevel;
                        }
                        break;
                    }
                }

                if (profile == MFX_PROFILE_UNKNOWN)
                {
                    if (extBits->SPSBuffer)
                        MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                    if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0)
                    {
                        par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
                        par.mfx.CodecLevel = MFX_LEVEL_AVC_52;
                    }

                    changed = true;
                    par.calcParam.bufferSizeInKB = mfxU16(std::min<mfxU32>(GetMaxBufferSize(par) / 8000, USHRT_MAX));
                }

                if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
                    par.mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
                    par.calcParam.initialDelayInKB != 0)
                {
                    if (par.calcParam.initialDelayInKB >= par.calcParam.bufferSizeInKB)
                    {
                        changed = true;
                        par.calcParam.initialDelayInKB = par.calcParam.bufferSizeInKB / 2;
                    }

                    if (avgFrameSizeInKB > 0 && par.calcParam.initialDelayInKB < mfxU32(avgFrameSizeInKB))
                    {
                        changed = true;
                        par.calcParam.initialDelayInKB = std::min(par.calcParam.bufferSizeInKB, mfxU32(avgFrameSizeInKB));
                    }
                }
            }
        }
    }
    else
    {
        // special check for compatibility of profile/level and BRC parameters for cqpHrdMode
        mfxU16 profile = std::max<mfxU16>(MFX_PROFILE_AVC_BASELINE, par.mfx.CodecProfile & MASK_PROFILE_IDC);
        for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
        {
            if (mfxU16 minLevel = GetLevelLimitByMaxBitrate(profile, par.calcParam.decorativeHrdParam.targetKbps))
            {
                if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0 && par.mfx.CodecLevel < minLevel)
                {
                    changed = true;
                    par.mfx.CodecLevel   = minLevel;
                    par.mfx.CodecProfile = profile;
                }
                break;
            }

            if (profile == MFX_PROFILE_UNKNOWN)
            {
                if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0)
                {
                    par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
                    par.mfx.CodecLevel = MFX_LEVEL_AVC_52;
                }
            }
        }

        if (par.calcParam.decorativeHrdParam.maxKbps < par.calcParam.decorativeHrdParam.targetKbps)
        {
            changed = true;
            par.calcParam.decorativeHrdParam.maxKbps = par.calcParam.decorativeHrdParam.targetKbps;
        }

        profile = std::max<mfxU16>(MFX_PROFILE_AVC_BASELINE, par.mfx.CodecProfile & MASK_PROFILE_IDC);
        for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
        {
            if (mfxU16 minLevel = GetLevelLimitByMaxBitrate(profile, par.calcParam.decorativeHrdParam.maxKbps))
            {
                if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0 && par.mfx.CodecLevel < minLevel)
                {
                    changed = true;
                    par.mfx.CodecLevel = minLevel;
                    par.mfx.CodecProfile = profile;
                }
                break;
            }
        }

        if (profile == MFX_PROFILE_UNKNOWN)
        {
            if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0)
            {
                par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
                par.mfx.CodecLevel = MFX_LEVEL_AVC_52;
            }
        }

        profile = std::max<mfxU16>(MFX_PROFILE_AVC_BASELINE, par.mfx.CodecProfile & MASK_PROFILE_IDC);
        for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
        {
            if (mfxU16 minLevel = GetLevelLimitByBufferSize(profile, par.calcParam.decorativeHrdParam.bufferSizeInKB))
            {
                if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0 && par.mfx.CodecLevel < minLevel)
                {
                    changed = true;
                    par.mfx.CodecLevel = minLevel;
                }
                break;
            }
        }

        if (profile == MFX_PROFILE_UNKNOWN)
        {
            if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0)
            {
                par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
                par.mfx.CodecLevel = MFX_LEVEL_AVC_52;
            }
            changed = true;

        }

        if (par.calcParam.decorativeHrdParam.initialDelayInKB > par.calcParam.decorativeHrdParam.bufferSizeInKB)
        {
            changed = true;
            par.calcParam.decorativeHrdParam.initialDelayInKB = par.calcParam.decorativeHrdParam.bufferSizeInKB / 2;
        }
    }

    mfxStatus sts = CheckMaxFrameSize(par, hwCaps);
    if (sts == MFX_ERR_UNSUPPORTED)
        unsupported = true;
    else if (sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
        changed = true;
#if defined(MFX_ENABLE_ENCTOOLS)
    if (H264EncTools::isEncToolNeeded(par))
    {
        if ((IsOn(extOpt2->ExtBRC) && extOpt2->LookAheadDepth == 0) || par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        {
            changed = true;
            ResetEncToolsPar(*extConfig, MFX_CODINGOPTION_OFF);
        } else {
            sts = H264EncTools::Query(par);
            if (sts == MFX_ERR_UNSUPPORTED)
                unsupported = true;
        else if (sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
            changed = true;
        }
    }
#endif
#if defined(MFX_ENABLE_EXT_BRC)
    if (IsOn(extOpt2->ExtBRC) && par.mfx.RateControlMethod != 0 && par.mfx.RateControlMethod != MFX_RATECONTROL_CBR && par.mfx.RateControlMethod != MFX_RATECONTROL_VBR)
    {
        extOpt2->ExtBRC = MFX_CODINGOPTION_OFF;
        changed = true;
    }

    if ((!IsOn(extOpt2->ExtBRC)) && (extBRC->pthis || extBRC->Init || extBRC->Close || extBRC->GetFrameCtrl || extBRC->Update || extBRC->Reset) )
    {
        extBRC->pthis = 0;
        extBRC->Init = 0;
        extBRC->Close = 0;
        extBRC->GetFrameCtrl = 0;
        extBRC->Update = 0;
        extBRC->Reset = 0;
        changed = true;
    }
    if ((extBRC->pthis  || extBRC->Init || extBRC->Close   || extBRC->GetFrameCtrl  || extBRC->Update  || extBRC->Reset) &&
        (!extBRC->pthis || !extBRC->Init || !extBRC->Close || !extBRC->GetFrameCtrl || !extBRC->Update || !extBRC->Reset))
    {
        extOpt2->ExtBRC = 0;
        extBRC->pthis = 0;
        extBRC->Init = 0;
        extBRC->Close = 0;
        extBRC->GetFrameCtrl = 0;
        extBRC->Update = 0;
        extBRC->Reset = 0;
        changed = true;
    }

#endif

    if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
    {
        if (par.mfx.CodecLevel != 0 && par.mfx.CodecLevel < MFX_LEVEL_AVC_21)
        {
            if (extBits->SPSBuffer) // level came from sps header, override picstruct
                par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            else // level came from videoparam, override level
                par.mfx.CodecLevel = MFX_LEVEL_AVC_21;
            changed = true;
        }

        mfxU16 levelToCheck = par.mfx.CodecLevel;

        if (levelToCheck == 0)
            levelToCheck = GetMinLevelForAllParameters(par);

        if (levelToCheck > MFX_LEVEL_AVC_41)
        {
            if (GetMinLevelForResolutionAndFramerate(par) <= MFX_LEVEL_AVC_41)
            {
                // it's possible to encode stream with level lower than 4.2
                // correct encoding parameters to satisfy H264 spec (table A-4, frame_mbs_only_flag)
                changed = true;
                par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            }
            else
            {
                // it's impossible to encode stream with level lower than 4.2
                // allow H264 spec violation ((table A-4, frame_mbs_only_flag)) and return warning
                warning = true;
            }
        }
    }

    if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
    {
        if (IsAvcBaseProfile(par.mfx.CodecProfile) ||
            par.mfx.CodecProfile == MFX_PROFILE_AVC_PROGRESSIVE_HIGH ||
            par.mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH)
        {
            changed = true;
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        }
    }

    if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0 && par.mfx.NumRefFrame == 1)
    {
        par.mfx.NumRefFrame ++; // HSW and IVB don't support 1 reference frame for interlaced encoding
        changed = true;
    }

    if (IsOn(extOpt->FieldOutput))
    {
        if (par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE || IsOn(extOpt->FramePicture))
        {
            changed = true;
            extOpt->FieldOutput = MFX_CODINGOPTION_OFF;
        }
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP
        && par.calcParam.cqpHrdMode == 0)
    {
        if (!CheckRange(par.mfx.QPI, 0, 51)) changed = true;
        if (!CheckRange(par.mfx.QPP, 0, 51)) changed = true;
        if (!CheckRange(par.mfx.QPB, 0, 51)) changed = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR &&
        hwCaps.CBRSupport == 0)
    {
        par.mfx.RateControlMethod = 0;
        unsupported = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_VBR &&
        hwCaps.VBRSupport == 0)
    {
        par.mfx.RateControlMethod = 0;
        unsupported = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP &&
        hwCaps.CQPSupport == 0)
    {
        par.mfx.RateControlMethod = 0;
        unsupported = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ &&
        hwCaps.ddi_caps.ICQBRCSupport == 0)
    {
        par.mfx.RateControlMethod = 0;
        unsupported = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR &&
        hwCaps.ddi_caps.QVBRBRCSupport == 0)
    {
        par.mfx.RateControlMethod = 0;
        unsupported = true;
    }

    if ( ((par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ) || (par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR)) &&
         (extOpt2->MBBRC == MFX_CODINGOPTION_OFF) )
    {
        // for ICQ or QVBR BRC mode MBBRC is ignored by driver and always treated as ON
        // need to change extOpt2->MBBRC respectively to notify application about it
        extOpt2->MBBRC = MFX_CODINGOPTION_ON;
        changed = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ ||
        par.mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ)
    {
        if (!CheckRange(par.mfx.ICQQuality, 0, 51)) changed = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
    {
        if (!CheckRange(extOpt3->QVBRQuality, 0, 51)) changed = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        if (!CheckRange(par.mfx.Accuracy,    AVBR_ACCURACY_MIN,    AVBR_ACCURACY_MAX))    changed = true;
        if (!CheckRange(par.mfx.Convergence, AVBR_CONVERGENCE_MIN, AVBR_CONVERGENCE_MAX)) changed = true;
    }

    for (mfxU32 i = 0; i < 3; i++)
    {
        mfxU32 maxTimeOffset = (1 << 24) - 1;

        mfxU32 maxNFrames = (par.mfx.FrameInfo.FrameRateExtN > 0 && par.mfx.FrameInfo.FrameRateExtD > 0)
            ? (par.mfx.FrameInfo.FrameRateExtN - 1) / par.mfx.FrameInfo.FrameRateExtD
            : 255;

        if (extPt->TimeStamp[i].CtType != 0xffff)
        {
            if (!CheckRangeDflt(extPt->TimeStamp[i].CtType, 0, 3, 2))
                changed = true;
        }

        if (!CheckRangeDflt(extPt->TimeStamp[i].CountingType, 0, 31, 0))    changed = true;
        if (!CheckRange(extPt->TimeStamp[i].NFrames, 0u, maxNFrames))       changed = true;
        if (!CheckRange(extPt->TimeStamp[i].SecondsValue, 0, 59))           changed = true;
        if (!CheckRange(extPt->TimeStamp[i].MinutesValue, 0, 59))           changed = true;
        if (!CheckRange(extPt->TimeStamp[i].HoursValue, 0, 23))             changed = true;
        if (!CheckRange(extPt->TimeStamp[i].TimeOffset, 0u, maxTimeOffset)) changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].ClockTimestampFlag, 0))         changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].NuitFieldBasedFlag, 1))         changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].FullTimestampFlag, 1))          changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].DiscontinuityFlag, 0))          changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].CntDroppedFlag, 0))             changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].SecondsFlag, 0))                changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].MinutesFlag, 0))                changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].HoursFlag, 0))                  changed = true;
    }

    if (!CheckRangeDflt(extBits->SPSId, 0,  31, 0)) changed = true;
    if (!CheckRangeDflt(extBits->PPSId, 0, 255, 0)) changed = true;

    if (extBits->SPSBuffer)
    {
        if (extSps->seqParameterSetId > 31                                  ||
            !IsValidCodingProfile(extSps->profileIdc)                       ||
            !IsValidCodingLevel(extSps->levelIdc)                           ||
            extSps->chromaFormatIdc != 1                                    ||
            extSps->bitDepthLumaMinus8 != 0                                 ||
            extSps->bitDepthChromaMinus8 != 0                               ||
            extSps->qpprimeYZeroTransformBypassFlag != 0                    ||
            extSps->picOrderCntType == 1                                    ||
            /*extSps->gapsInFrameNumValueAllowedFlag != 0                     ||*/
            extSps->mbAdaptiveFrameFieldFlag != 0                           ||
            extSps->vui.flags.vclHrdParametersPresent != 0                  ||
            extSps->vui.nalHrdParameters.cpbCntMinus1 > 0                   ||
            extSps->vui.numReorderFrames > extSps->vui.maxDecFrameBuffering)
            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

        // the following fields aren't supported by snb/ivb_win7 drivers directly and requires sps patching
        // patching is not possible whe protection is on
        if (par.Protected != 0)
            if (extSps->nalRefIdc        != 1 ||
                extSps->constraints.set0 != 0 ||
                extSps->constraints.set1 != 0 ||
                extSps->constraints.set2 != 0 ||
                extSps->constraints.set3 != 0 ||
                extSps->constraints.set4 != 0 ||
                extSps->constraints.set5 != 0 ||
                extSps->constraints.set6 != 0 ||
                extSps->constraints.set7 != 0)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

        if (extBits->PPSBuffer)
        {
            if (extPps->seqParameterSetId != extSps->seqParameterSetId ||
                extPps->numSliceGroupsMinus1 > 0                       ||
                extPps->numRefIdxL0DefaultActiveMinus1 > 31            ||
                extPps->numRefIdxL1DefaultActiveMinus1 > 31            ||
                //Check of weightedPredFlag is actually not needed, as it was read from 1 bit in extBits->PPSBuffer
                extPps->weightedPredFlag > 1                           ||
                extPps->weightedBipredIdc > 2                          ||
                extPps->picInitQpMinus26 < -26                         ||
                extPps->picInitQpMinus26 > 25                          ||
                extPps->picInitQsMinus26 != 0                          ||
                extPps->chromaQpIndexOffset < -12                      ||
                extPps->chromaQpIndexOffset > 12                       ||
                extPps->deblockingFilterControlPresentFlag == 0        ||
                extPps->redundantPicCntPresentFlag != 0                ||
                extPps->secondChromaQpIndexOffset < -12                ||
                extPps->secondChromaQpIndexOffset > 12)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            // the following fields aren't supported by snb/ivb_win7 drivers directlyand requires pps patching
            // patching is not possible whe protection is on
            if (par.Protected != 0)
                if (extSps->nalRefIdc != 1)
                    MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
        }

#ifdef MFX_ENABLE_AVC_CUSTOM_QMATRIX
        if (extSps->seqScalingMatrixPresentFlag)
        {
            // Rec. ITU-T H.264 (04/2017), chapter A.2 Profiles, customized Scaling Matrices is supported
            // only for High profiles and protected content isn't supported
            if (!IsAvcHighProfile(par.mfx.CodecProfile) || par.Protected != 0)
            {
                extSps->seqScalingMatrixPresentFlag = 0;
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
            }
            //levelIdx == 3 isn't supported
            if (extSps->levelIdc == 3)
            {
                extSps->seqScalingMatrixPresentFlag = 0;
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
            }
            //if seqScalingMatrixPresent flag isn't 0 at least one matrix has to be provided
            //More information Rec. ITU-T H.264, 7.4.2.1.1 Sequence parameter set data semantics
            mfxU8 sum = 0;
            for (mfxU8 i = 0; i < 8 /*((extSps->levelIdc != 3) ? 8 : 12)*/; ++i)
            {
                sum |= extSps->seqScalingListPresentFlag[i];
                if (extSps->seqScalingListPresentFlag[i] > 1) //bitfield
                {
                    extSps->seqScalingListPresentFlag[i] = 1;
                    changed = true;
                }
            }
            if (!sum)
            {
                extSps->seqScalingMatrixPresentFlag = 0;
                changed = true;
            }
        }

        if (extBits->PPSBuffer && extPps->picScalingMatrixPresentFlag)
        {
            // Rec. ITU-T H.264 (04/2017), chapter A.2 Profiles, customized Scaling Matrices is supported
            // only for High profiles and protected content isn't supported
            if (!IsAvcHighProfile(par.mfx.CodecProfile) || par.Protected != 0)
            {
                extPps->picScalingMatrixPresentFlag = 0;
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
            }
            //transform8x8ModeFlag has to be 1, other values is unsupported
            if (extPps->transform8x8ModeFlag != 1)
            {
                extPps->picScalingMatrixPresentFlag = 0;
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
            }
            else
            {
                //if picScalingMatrixPresent flag isn't 0 at least one matrix has to be provided
                //More information Rec. ITU-T H.264, 7.4.2.2 Picture parameter set RBSP semantics
                bool sum = 0;
                for (mfxU8 i = 0; i < 8 /*6 + 2*(!!extPps->transform8x8ModeFlag)*/; ++i)
                {
                    sum |= extPps->picScalingListPresentFlag[i];
                }
                if (!sum)
                {
                    extPps->picScalingMatrixPresentFlag = 0;
                    changed = true;
                }
            }
        }
#endif
    }


    if (!CheckTriStateOption(extOpt3->ExtBrcAdaptiveLTR)) changed = true;

#ifdef MFX_AUTOLTR_FEATURE_DISABLE
    if (IsOn(extOpt3->ExtBrcAdaptiveLTR))
    {
        extOpt3->ExtBrcAdaptiveLTR = MFX_CODINGOPTION_OFF;
        changed = true;
    }
#else // else MFX_AUTOLTR_FEATURE_DISABLE == MFX_AUTOLTR_FEATURE_ENABLED
    if (IsOn(extOpt3->ExtBrcAdaptiveLTR) && IsOff(extOpt2->ExtBRC))
    {
        extOpt3->ExtBrcAdaptiveLTR = MFX_CODINGOPTION_OFF;
        changed = true;
    }

    if (IsOn(extOpt3->ExtBrcAdaptiveLTR) && par.mfx.RateControlMethod != 0 && par.mfx.RateControlMethod != MFX_RATECONTROL_CBR && par.mfx.RateControlMethod != MFX_RATECONTROL_VBR)
    {
        extOpt3->ExtBrcAdaptiveLTR = MFX_CODINGOPTION_OFF;
        changed = true;
    }

    if (IsOn(extOpt3->ExtBrcAdaptiveLTR) && (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0)
    {
        extOpt3->ExtBrcAdaptiveLTR = MFX_CODINGOPTION_OFF;
        changed = true;
    }

    if (IsOn(extOpt3->ExtBrcAdaptiveLTR) && par.mfx.NumRefFrame != 0)
    {
        mfxU16 nrfMin = (par.mfx.GopRefDist > 1 ? 2 : 1);
        bool bPyr = (extOpt2->BRefType == MFX_B_REF_PYRAMID);
        if (bPyr) nrfMin = GetMinNumRefFrameForPyramid(par);

        if (par.mfx.NumRefFrame <= nrfMin)
        {
            extOpt3->ExtBrcAdaptiveLTR = MFX_CODINGOPTION_OFF;
            changed = true;
        }
    }

    if (IsOn(extOpt3->ExtBrcAdaptiveLTR) && extDdi->NumActiveRefP != 0)
    {
        if (extDdi->NumActiveRefP <= 1)
        {
            extOpt3->ExtBrcAdaptiveLTR = MFX_CODINGOPTION_OFF;
            changed = true;
        }
    }
#endif //MFX_AUTOLTR_FEATURE_DISABLE

#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
    if (!CheckTriStateOption(extOpt3->AdaptiveCQM)) changed = true;
    if (IsOn(extOpt3->AdaptiveCQM) && !isAdaptiveCQMSupported(extOpt3->ScenarioInfo, IsOn(par.mfx.LowPower)))
    {
        extOpt3->AdaptiveCQM = MFX_CODINGOPTION_OFF;
        changed = true;
    }
#endif

    if (IsMvcProfile(par.mfx.CodecProfile) && MFX_ERR_UNSUPPORTED == CheckMVCSeqDescQueryLike(extMvc))
    {
        unsupported = true;
    }

    par.SyncCalculableToVideoParam();
    par.AlignCalcWithBRCParamMultiplier();

    if (par.calcParam.numTemporalLayer > 0 && par.mfx.EncodedOrder != 0)
    {
        changed = true;
        memset(extTemp->Layer, 0, sizeof(extTemp->Layer)); // unnamed structures can't be used in templates
        Zero(par.calcParam.scale);
        Zero(par.calcParam.tid);
        par.calcParam.numTemporalLayer = 0;
        par.calcParam.tempScalabilityMode = 0;
    }

    if (par.calcParam.tempScalabilityMode && par.mfx.GopRefDist > 1)
    {
        changed = true;
        par.mfx.GopRefDist = 1;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_VCM && par.mfx.GopRefDist > 1)
    {
        changed = true;
        par.mfx.GopRefDist = 1;
    }

    if (extTemp->BaseLayerPID + par.calcParam.numTemporalLayer > 64)
    {
        unsupported = true;
        extTemp->BaseLayerPID = 0;
    }

    if (par.calcParam.numTemporalLayer > 0 &&
        IsAvcProfile(par.mfx.CodecProfile) &&
        extTemp->Layer[0].Scale != 1)
    {
        unsupported = true;
        extTemp->Layer[0].Scale = 0;
    }

    for (mfxU32 i = 1; i < par.calcParam.numTemporalLayer; i++)
    {
        if (par.calcParam.scale[i] <= par.calcParam.scale[i - 1] || // increasing
            par.calcParam.scale[i] %  par.calcParam.scale[i - 1])   // divisible
        {
            unsupported = true;
            extTemp->Layer[par.calcParam.tid[i]].Scale = 0;
        }

        if (par.calcParam.tempScalabilityMode &&
            par.calcParam.scale[i] != par.calcParam.scale[i - 1] * 2)
        {
            unsupported = true;
            extTemp->Layer[par.calcParam.tid[i]].Scale = 0;
        }
    }

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    if (hwCaps.ddi_caps.MaxNum_QualityLayer == 0 && hwCaps.ddi_caps.MaxNum_DependencyLayer == 0)
    {
        for (mfxU32 i = 0; i < par.calcParam.numDependencyLayer; i++)
        {
            mfxU32 did = par.calcParam.did[i];

            if (extSvc->DependencyLayer[did].BasemodePred != 0 &&
                extSvc->DependencyLayer[did].BasemodePred != MFX_CODINGOPTION_OFF)
            {
                extSvc->DependencyLayer[did].BasemodePred = MFX_CODINGOPTION_OFF;
                changed = true;
            }
            if (extSvc->DependencyLayer[did].MotionPred != 0 &&
                extSvc->DependencyLayer[did].MotionPred != MFX_CODINGOPTION_OFF)
            {
                extSvc->DependencyLayer[did].MotionPred = MFX_CODINGOPTION_OFF;
                changed = true;
            }
            if (extSvc->DependencyLayer[did].ResidualPred != 0 &&
                extSvc->DependencyLayer[did].ResidualPred != MFX_CODINGOPTION_OFF)
            {
                extSvc->DependencyLayer[did].ResidualPred = MFX_CODINGOPTION_OFF;
                changed = true;
            }
        }
    }
#endif // #ifdef MFX_ENABLE_SVC_VIDEO_ENCODE

    if (extOpt2->IntRefType > 3 || (extOpt2->IntRefType && hwCaps.ddi_caps.RollingIntraRefresh == 0))
    {
        extOpt2->IntRefType = 0;
        unsupported = true;
    }

    if (!H264ECaps::IsVmeSupported(platform) &&
        (extOpt2->IntRefType > MFX_REFRESH_HORIZONTAL))
    {
        extOpt2->IntRefType = MFX_REFRESH_HORIZONTAL;
        changed = true;
    }

    if (extOpt2->IntRefType && par.mfx.GopRefDist > 1)
    {
        extOpt2->IntRefType = 0;
        changed = true;
    }

    if (extOpt2->IntRefType && par.mfx.NumRefFrame > 1 && par.calcParam.tempScalabilityMode == 0)
    {
        extOpt2->IntRefType = 0;
        changed = true;
    }

    if (extOpt2->IntRefType && par.calcParam.numTemporalLayer && par.calcParam.tempScalabilityMode == 0)
    {
        extOpt2->IntRefType = 0;
        changed = true;
    }

    if (extOpt2->IntRefQPDelta < -51 || extOpt2->IntRefQPDelta > 51)
    {
        extOpt2->IntRefQPDelta = 0;
        changed = true;
    }

    if (extOpt2->IntRefCycleSize != 0 &&
        par.mfx.GopPicSize != 0 &&
        extOpt2->IntRefCycleSize >= par.mfx.GopPicSize)
    {
        // refresh cycle length shouldn't be greater or equal to GOP size
        extOpt2->IntRefType = 0;
        extOpt2->IntRefCycleSize = 0;
        changed = true;
    }

    if (extOpt3->IntRefCycleDist != 0 &&
        par.mfx.GopPicSize != 0 &&
        extOpt3->IntRefCycleDist >= par.mfx.GopPicSize)
    {
        // refresh period length shouldn't be greater or equal to GOP size
        extOpt2->IntRefType = 0;
        extOpt3->IntRefCycleDist = 0;
        changed = true;
    }

    if (extOpt3->IntRefCycleDist != 0 &&
        extOpt2->IntRefCycleSize != 0 &&
        extOpt2->IntRefCycleSize > extOpt3->IntRefCycleDist)
    {
        // refresh period shouldn't be greater than refresh cycle size
        extOpt3->IntRefCycleDist = 0;
        changed = true;
    }

    if (extOpt2->IntRefType == MFX_REFRESH_SLICE)
    {
        if (extOpt2->IntRefCycleSize && !extOpt3->NumSliceP && !par.mfx.NumSlice)
        {
            extOpt2->IntRefType = MFX_REFRESH_HORIZONTAL;
            changed = true;
        }
        if (extOpt2->IntRefCycleSize && extOpt3->NumSliceP && (extOpt2->IntRefCycleSize != extOpt3->NumSliceP))
        {
            extOpt2->IntRefCycleSize = extOpt3->NumSliceP;
            changed = true;
        }
        if ((extOpt3->NumSliceP != 0) && (par.mfx.GopPicSize != 0) && (extOpt3->NumSliceP > par.mfx.GopPicSize))
        {
            extOpt2->IntRefType = 0;
            changed = true;
        }
        if ((extOpt3->IntRefCycleDist != 0) && (extOpt3->NumSliceP != 0) && (extOpt3->NumSliceP > extOpt3->IntRefCycleDist))
        {
            // refresh period shouldn't be greater than refresh cycle size
            extOpt3->IntRefCycleDist = 0;
            changed = true;
        }
        if ((extOpt2->MaxSliceSize) || (extOpt2->NumMbPerSlice) || (!sliceRowAlligned))
        {
            extOpt2->IntRefType = 0;
            extOpt2->IntRefCycleSize = 0;
            unsupported = true;
        }
    }

    if (extOpt2->Trellis & ~(MFX_TRELLIS_OFF | MFX_TRELLIS_I | MFX_TRELLIS_P | MFX_TRELLIS_B))
    {
        extOpt2->Trellis &= (MFX_TRELLIS_OFF | MFX_TRELLIS_I | MFX_TRELLIS_P | MFX_TRELLIS_B);
        changed = true;
    }

    if ((extOpt2->Trellis & MFX_TRELLIS_OFF) && (extOpt2->Trellis & ~MFX_TRELLIS_OFF))
    {
        extOpt2->Trellis = MFX_TRELLIS_OFF;
        changed = true;
    }

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    if (extOpt2->Trellis && IsSvcProfile(par.mfx.CodecProfile))
    {
        extOpt2->Trellis = 0;
        changed = true;
    }
#endif

    /*if (extOpt2->Trellis && hwCaps.EnhancedEncInput == 0)
    {
        extOpt2->Trellis = 0;
        unsupported = true;
    }*/

    if (extOpt2->BRefType        != 0 &&
        extOpt2->BRefType        != MFX_B_REF_OFF &&
        par.mfx.CodecLevel       != 0 &&
        par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        const mfxU16 nfxMaxByLevel    = GetMaxNumRefFrame(par);
        const mfxU16 nfxMinForPyramid = GetMinNumRefFrameForPyramid(par);

        if (nfxMinForPyramid > nfxMaxByLevel)
        {
            // max dpb size is not enougn for pyramid
            changed = true;
            extOpt2->BRefType = MFX_B_REF_OFF;
        }
        else
        {
            if (par.mfx.NumRefFrame != 0 &&
                par.mfx.NumRefFrame < nfxMinForPyramid)
            {
                changed = true;
                par.mfx.NumRefFrame = nfxMinForPyramid;
            }
        }
    }

    if (par.calcParam.numTemporalLayer >  1 &&
        par.mfx.CodecLevel             != 0 &&
        par.mfx.FrameInfo.Width        != 0 &&
        par.mfx.FrameInfo.Height       != 0)
    {
        mfxU16 const nrfMaxByLevel     = GetMaxNumRefFrame(par);
        mfxU16 const nrfMinForTemporal = mfxU16(1 << (par.calcParam.numTemporalLayer - 2));

        if (nrfMinForTemporal > nrfMaxByLevel)
        {
            // max dpb size is not enougn for requested number of temporal layers
            changed = true;
            par.calcParam.numTemporalLayer = 1;
        }
        else
        {
            if (par.mfx.NumRefFrame != 0 &&
                par.mfx.NumRefFrame < nrfMinForTemporal)
            {
                changed = true;
                par.mfx.NumRefFrame = nrfMinForTemporal;
            }
        }
    }

    if (extRoi->NumROI && extRoi->ROIMode != MFX_ROI_MODE_QP_DELTA && extRoi->ROIMode != MFX_ROI_MODE_PRIORITY)
    {
        unsupported = true;
        extRoi->NumROI = 0;
    }

    if ((extRoi->NumROI && (extRoi->ROIMode == MFX_ROI_MODE_QP_DELTA ||
        extRoi->ROIMode == MFX_ROI_MODE_PRIORITY ))&&
        hwCaps.ddi_caps.ROIBRCDeltaQPLevelSupport == 0)
    {
        unsupported = true;
        extRoi->NumROI = 0;
    }

    if (extRoi->NumROI)
    {
        if (extRoi->NumROI > hwCaps.ddi_caps.MaxNumOfROI)
        {
            if (hwCaps.ddi_caps.MaxNumOfROI == 0)
                unsupported = true;
            else
                changed = true;

            extRoi->NumROI = hwCaps.ddi_caps.MaxNumOfROI;
        }
    }

    for (mfxU16 i = 0; i < extRoi->NumROI; i++)
    {
        sts = CheckAndFixRoiQueryLike(par, (mfxRoiDesc*)(&(extRoi->ROI[i])), extRoi->ROIMode);
        if (sts < MFX_ERR_NONE)
            unsupported = true;
        else if (sts != MFX_ERR_NONE)
            changed = true;
    }

    if (extDirtyRect->NumRect > MFX_MAX_DIRTY_RECT_COUNT)
    {
        changed = true;
        extDirtyRect->NumRect = MFX_MAX_DIRTY_RECT_COUNT;
    }

    if (extDirtyRect->NumRect && hwCaps.ddi_caps.DirtyRectSupport == 0)
    {
        unsupported = true;
        extDirtyRect->NumRect = 0;
    }

    for (mfxU16 i = 0; i < extDirtyRect->NumRect; i++)
    {
        sts = CheckAndFixRectQueryLike(par, (mfxRectDesc*)(&(extDirtyRect->Rect[i])));
        if (sts < MFX_ERR_NONE)
            unsupported = true;
        else if (sts != MFX_ERR_NONE)
            changed = true;
    }

    if (extMoveRect->NumRect > MFX_MAX_MOVE_RECT_COUNT)
    {
        changed = true;
        extMoveRect->NumRect = MFX_MAX_MOVE_RECT_COUNT;
    }

    if (extMoveRect->NumRect && hwCaps.ddi_caps.MoveRectSupport == 0)
    {
        unsupported = true;
        extMoveRect->NumRect = 0;
    }

    for (mfxU16 i = 0; i < extMoveRect->NumRect; i++)
    {
        sts = CheckAndFixMovingRectQueryLike(par, (mfxMovingRectDesc*)(&(extMoveRect->Rect[i])));
        if (sts < MFX_ERR_NONE)
            unsupported = true;
        else if (sts != MFX_ERR_NONE)
            changed = true;
    }

    if (extPwt)
    {
        mfxU16 maxLuma[2] = {0};
        mfxU16 maxChroma[2] = {0};
        bool out_of_caps = false;
        if (0 == hwCaps.ddi_caps.NoWeightedPred)
        {
// On linux, WP is FEI specific feature. So when legay encoder calls Query(), do not
// enable the flag of this capability.
            if (hwCaps.ddi_caps.LumaWeightedPred)
            {
                maxLuma[0] = std::min<mfxU16>(hwCaps.ddi_caps.MaxNum_WeightedPredL0, 32);
                maxLuma[1] = std::min<mfxU16>(hwCaps.ddi_caps.MaxNum_WeightedPredL1, 32);
            }
            if (hwCaps.ddi_caps.ChromaWeightedPred)
            {
                maxChroma[0] = std::min<mfxU16>(hwCaps.ddi_caps.MaxNum_WeightedPredL0, 32);
                maxChroma[1] = std::min<mfxU16>(hwCaps.ddi_caps.MaxNum_WeightedPredL1, 32);
            }
        }

        for (mfxU16 lx = 0; lx < 2; lx++)
        {
            for (mfxU16 i = 0; i < 32 && !out_of_caps && extPwt->LumaWeightFlag[lx][i]; i++)
                out_of_caps = (i >= maxLuma[lx]);
            for (mfxU16 i = 0; i < 32 && !out_of_caps && extPwt->ChromaWeightFlag[lx][i]; i++)
                out_of_caps = (i >= maxChroma[lx]);
        }

        if (out_of_caps)
        {
            Zero(extPwt->LumaWeightFlag);
            Zero(extPwt->ChromaWeightFlag);

            for (mfxU16 lx = 0; lx < 2; lx++)
            {
                for (mfxU16 i = 0; i < maxLuma[lx]; i++)
                    extPwt->LumaWeightFlag[lx][i] = 1;
                for (mfxU16 i = 0; i < maxChroma[lx]; i++)
                    extPwt->ChromaWeightFlag[lx][i] = 1;
            }

            changed = true;
        }

        if (extPwt->LumaLog2WeightDenom && extPwt->LumaLog2WeightDenom != 6)
        {
            extPwt->LumaLog2WeightDenom = 6;
            changed = true;
        }
        if (extPwt->ChromaLog2WeightDenom && extPwt->ChromaLog2WeightDenom != 6)
        {
            extPwt->ChromaLog2WeightDenom = 6;
            changed = true;
        }
    }

    if (!CheckRangeDflt(extOpt2->SkipFrame, 0, 3, 0)) changed = true;

    if ( extOpt2->SkipFrame && hwCaps.ddi_caps.SkipFrame == 0 && par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        extOpt2->SkipFrame = 0;
        changed = true;
    }

    bool mfxRateControlHwSupport =
        hwCaps.ddi_caps.FrameSizeToleranceSupport &&
        (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
         par.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
         par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR);

    bool slidingWindowSupported  =
            par.mfx.RateControlMethod == MFX_RATECONTROL_LA
        ||  par.mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD
        || (mfxRateControlHwSupport && !IsOn(extOpt2->ExtBRC));

    if (extOpt3->WinBRCMaxAvgKbps || extOpt3->WinBRCSize)
    {
        if (!slidingWindowSupported)
        {
            extOpt3->WinBRCMaxAvgKbps = 0;
            extOpt3->WinBRCSize = 0;
            par.calcParam.WinBRCMaxAvgKbps = 0;
            unsupported = true;
        }
        else if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR && CommonCaps::IsCBRSlidingWinSupported(platform))
        {
            mfxU16 framerate = 0;
            if (par.mfx.FrameInfo.FrameRateExtN != 0 && par.mfx.FrameInfo.FrameRateExtD != 0)
            {
                framerate = (mfxU16)ceil((mfxF64)par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD);
            }
            else
            {
                CalculateMFXFramerate((mfxF64)framerate, &par.mfx.FrameInfo.FrameRateExtN, &par.mfx.FrameInfo.FrameRateExtD);
            }
            mfxU16 maxWinBRCSize = 60;
            mfxU16 minWinBRCSize = std::min<mfxU16>(mfxU16(framerate * 0.5), maxWinBRCSize);
            
            if (extOpt3->WinBRCSize) //check WinBRCSize range and clip it to [framerate*0.5, 60]
            {
                if (!CheckRange(extOpt3->WinBRCSize, minWinBRCSize, maxWinBRCSize))
                    changed = true;
            }
            else //set default as framerate
            {
                extOpt3->WinBRCSize = std::min<mfxU16>(framerate, maxWinBRCSize);
                changed = true;
            }

            mfxU32 minWinBRCMaxAvgKbps = static_cast<mfxU32>(par.mfx.TargetKbps * 1.1);
            mfxU32 maxWinBRCMaxAvgKbps = static_cast<mfxU32>(par.mfx.TargetKbps * 2.0);
            mfxU32 WinBRCMaxAvgKbps = (mfxU32)extOpt3->WinBRCMaxAvgKbps;

            if (extOpt3->WinBRCMaxAvgKbps) //check WinBRCMaxAvgKbps range and clip it
            {
                if (!CheckRange(WinBRCMaxAvgKbps, minWinBRCMaxAvgKbps, maxWinBRCMaxAvgKbps))
                {
                    par.calcParam.WinBRCMaxAvgKbps = WinBRCMaxAvgKbps * std::max<mfxU16>(par.mfx.BRCParamMultiplier, 1);
                    changed = true;
                }
            }
            else //set default as 1.3x/1.7x target bitrate for OBS/other scenario
            {
                par.calcParam.WinBRCMaxAvgKbps = extOpt3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING ? static_cast<mfxU32>(par.calcParam.targetKbps * 1.3) : static_cast<mfxU32>(par.calcParam.targetKbps * 1.7);
                changed = true;
            }
        }
        else if (extOpt3->WinBRCSize == 0)
        {
            warning = true;
        }
        else if (mfxRateControlHwSupport && !IsOn(extOpt2->ExtBRC))
        {
            if (par.mfx.FrameInfo.FrameRateExtN != 0 && par.mfx.FrameInfo.FrameRateExtD != 0)
            {
                mfxU16 iframerate = (mfxU16)ceil((mfxF64)par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD);
                if (extOpt3->WinBRCSize != iframerate)
                {
                    extOpt3->WinBRCSize = iframerate;
                    changed = true;
                }
            }
            else
            {
                CalculateMFXFramerate((mfxF64)extOpt3->WinBRCSize, &par.mfx.FrameInfo.FrameRateExtN, &par.mfx.FrameInfo.FrameRateExtD);
                changed = true;
            }
            if (par.calcParam.maxKbps)
            {
                if (par.calcParam.WinBRCMaxAvgKbps != par.calcParam.maxKbps)
                {
                    par.calcParam.WinBRCMaxAvgKbps = (mfxU16)par.calcParam.maxKbps;
                    changed = true;
                }
            }
            else if (par.calcParam.WinBRCMaxAvgKbps)
            {
                if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR &&
                    par.calcParam.targetKbps &&
                    par.calcParam.WinBRCMaxAvgKbps != par.calcParam.targetKbps)
                {
                    par.calcParam.WinBRCMaxAvgKbps = par.calcParam.targetKbps;
                    changed = true;
                }

                if (par.calcParam.targetKbps && par.calcParam.WinBRCMaxAvgKbps < par.calcParam.targetKbps)
                {
                    extOpt3->WinBRCMaxAvgKbps = 0;
                    par.calcParam.WinBRCMaxAvgKbps = 0;
                    extOpt3->WinBRCSize = 0;
                    unsupported = true;
                }
                else
                {
                    par.calcParam.maxKbps = par.calcParam.WinBRCMaxAvgKbps;
                    changed = true;
                }
            }
            else
            {
                warning = true;
            }
        }
        else if (par.calcParam.targetKbps && par.calcParam.WinBRCMaxAvgKbps < par.calcParam.targetKbps)
        {
            extOpt3->WinBRCMaxAvgKbps = 0;
            par.calcParam.WinBRCMaxAvgKbps = 0;
            extOpt3->WinBRCSize = 0;
            unsupported = true;
        }
    }

    if (   extOpt2->MinQPI || extOpt2->MaxQPI
        || extOpt2->MinQPP || extOpt2->MaxQPP
        || extOpt2->MinQPB || extOpt2->MaxQPB)
    {
        if (!CheckRangeDflt(extOpt2->MaxQPI, 0, 51, 0)) changed = true;
        if (!CheckRangeDflt(extOpt2->MaxQPP, 0, 51, 0)) changed = true;
        if (!CheckRangeDflt(extOpt2->MaxQPB, 0, 51, 0)) changed = true;
        if (!CheckRangeDflt(extOpt2->MinQPI, 0, (extOpt2->MaxQPI ? extOpt2->MaxQPI : 51), 0)) changed = true;
        if (!CheckRangeDflt(extOpt2->MinQPP, 0, (extOpt2->MaxQPP ? extOpt2->MaxQPP : 51), 0)) changed = true;
        if (!CheckRangeDflt(extOpt2->MinQPB, 0, (extOpt2->MaxQPB ? extOpt2->MaxQPB : 51), 0)) changed = true;
    }

    if (!CheckTriStateOption(extOpt3->BRCPanicMode)) changed = true;
    if (IsOff(extOpt3->BRCPanicMode)
     && (bRateControlLA(par.mfx.RateControlMethod)
     || (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
     || (vaType != MFX_HW_VAAPI))) // neither CQP nor LA BRC modes nor Windows support BRC panic mode disabling
    {
        extOpt3->BRCPanicMode = MFX_CODINGOPTION_UNKNOWN;
        unsupported = true;
    }

    if (!CheckTriStateOption(extOpt3->EnableMBQP)) changed = true;

    if (IsOn(extOpt3->EnableMBQP) && !(hwCaps.ddi_caps.MbQpDataSupport && (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP 
     || par.mfx.RateControlMethod == MFX_RATECONTROL_CBR || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR || isSWBRC(par))))
    {
        extOpt3->EnableMBQP = MFX_CODINGOPTION_OFF;
        changed = true;
    }

    if (!CheckTriStateOption(extOpt3->EnableMBForceIntra)) changed = true;

    // at the moment LINUX , IOTG, OPEN SRC -  feature unsupported
    if (IsOn(extOpt3->EnableMBForceIntra))
    {
        extOpt3->EnableMBForceIntra = 0;
        changed = true;
    }

    if (!CheckTriStateOption(extOpt3->MBDisableSkipMap)) changed = true;

    if (IsOn(extOpt3->MBDisableSkipMap) && vaType != MFX_HW_VAAPI)
    {
        extOpt3->MBDisableSkipMap = MFX_CODINGOPTION_OFF;
        changed = true;
    }

    if (!CheckRangeDflt(extOpt2->DisableDeblockingIdc, 0, 2, 0)) changed = true;
    if (!CheckTriStateOption(extOpt2->EnableMAD)) changed = true;

    if (!CheckTriStateOption(extOpt2->AdaptiveI)) changed = true;
    if (IsOn(extOpt2->AdaptiveI) && (par.mfx.GopOptFlag & MFX_GOP_STRICT))
    {
        extOpt2->AdaptiveI = MFX_CODINGOPTION_OFF;
        changed = true;
    }
    if (IsOn(extOpt2->AdaptiveI) &&
        (!(IsExtBrcSceneChangeSupported(par, platform) && !(extBRC->pthis))
#if defined(MFX_ENABLE_ENCTOOLS)
        && IsOff(extConfig->AdaptiveI)
#endif
       ))
    {
        extOpt2->AdaptiveI = MFX_CODINGOPTION_OFF;
        changed = true;
    }
    if (IsOn(extOpt2->AdaptiveI) && (par.mfx.GopOptFlag & MFX_GOP_STRICT))
    {
        extOpt2->AdaptiveI = MFX_CODINGOPTION_OFF;
        changed = true;
    }

    if (IsOn(extOpt3->ExtBrcAdaptiveLTR) &&
        (!(IsExtBrcSceneChangeSupported(par, platform) && !(extBRC->pthis)))
#if defined(MFX_ENABLE_ENCTOOLS)
        && (extOpt2->LookAheadDepth == 0)
#endif
        )
    {
        extOpt3->ExtBrcAdaptiveLTR = MFX_CODINGOPTION_OFF;
        changed = true;
    }

    if (!CheckTriStateOption(extOpt2->AdaptiveB)) changed = true;
    if (IsOn(extOpt2->AdaptiveB) &&
        (!(IsExtBrcSceneChangeSupported(par, platform) && !(extBRC->pthis))
#if defined(MFX_ENABLE_ENCTOOLS)
            && IsOff(extConfig->AdaptiveB) && (extOpt2->LookAheadDepth == 0)
#endif
            ))
    {
        extOpt2->AdaptiveB = MFX_CODINGOPTION_OFF;
        changed = true;
    }

    if (IsOn(extOpt2->AdaptiveB) && (par.mfx.GopOptFlag & MFX_GOP_STRICT))
    {
        extOpt2->AdaptiveB = MFX_CODINGOPTION_OFF;
        changed = true;
    }

    if (extOpt3->PRefType == MFX_P_REF_PYRAMID &&  par.mfx.GopRefDist > 1)
    {
        extOpt3->PRefType = MFX_P_REF_DEFAULT;
        changed = true;
    }

    if (!CheckRangeDflt(extOpt3->WeightedPred,
            (mfxU16)MFX_WEIGHTED_PRED_UNKNOWN,
            (mfxU16)MFX_WEIGHTED_PRED_EXPLICIT,
            (mfxU16)MFX_WEIGHTED_PRED_DEFAULT))
            changed = true;

    if (!CheckRangeDflt(extOpt3->WeightedBiPred,
            (mfxU16)MFX_WEIGHTED_PRED_UNKNOWN,
            (mfxU16)MFX_WEIGHTED_PRED_IMPLICIT,
            (mfxU16)MFX_WEIGHTED_PRED_DEFAULT))
            changed = true;

    if (    hwCaps.ddi_caps.NoWeightedPred
        && (extOpt3->WeightedPred == MFX_WEIGHTED_PRED_EXPLICIT
        || extOpt3->WeightedBiPred == MFX_WEIGHTED_PRED_EXPLICIT))
    {
        extOpt3->WeightedPred = MFX_WEIGHTED_PRED_DEFAULT;
        extOpt3->WeightedBiPred = MFX_WEIGHTED_PRED_DEFAULT;
        unsupported = true;
    }

    if (!CheckTriStateOption(extOpt3->FadeDetection)) changed = true;

    // disable WP for legacy encoder on linux
    if (
        (extOpt3->WeightedPred   == MFX_WEIGHTED_PRED_EXPLICIT ||
         extOpt3->WeightedBiPred == MFX_WEIGHTED_PRED_EXPLICIT ||
         extOpt3->WeightedBiPred == MFX_WEIGHTED_PRED_IMPLICIT))
    {
        extOpt3->WeightedPred   = MFX_WEIGHTED_PRED_DEFAULT;
        extOpt3->WeightedBiPred = MFX_WEIGHTED_PRED_DEFAULT;
        changed = true;
    }

#ifndef MFX_AVC_ENCODING_UNIT_DISABLE
    if (!CheckTriStateOption(extOpt3->EncodedUnitsInfo))  changed = true;
    if ((par.calcParam.numTemporalLayer > 1 || IsMvcProfile(par.mfx.CodecProfile)) && IsOn(extOpt3->EncodedUnitsInfo))
    {
        extOpt3->EncodedUnitsInfo = MFX_CODINGOPTION_OFF;
        unsupported = true;
    }
#endif

#ifdef MFX_ENABLE_H264_REPARTITION_CHECK
    if (!CheckTriStateOptionWithAdaptive(extOpt3->RepartitionCheckEnable))
    {
        changed = true;
    }
    if (!hwCaps.ddi_caps.ForceRepartitionCheckSupport && IsOn(extOpt3->RepartitionCheckEnable))
    {
        extOpt3->RepartitionCheckEnable = MFX_CODINGOPTION_UNKNOWN;
        unsupported = true;
    }
#endif // MFX_ENABLE_H264_REPARTITION_CHECK


    MFX_RETURN(unsupported
        ? MFX_ERR_UNSUPPORTED
        : (changed || warning)
            ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
            : MFX_ERR_NONE);
}

// checks MVC per-view parameters (bitrates, buffer size, initial delay, level)
mfxStatus MfxHwH264Encode::CheckVideoParamMvcQueryLike(MfxVideoParam & par)
{
    bool changed     = false;

    mfxExtCodingOptionSPSPPS & extBits = GetExtBufferRef(par);
    mfxExtSpsHeader          & extSps  = GetExtBufferRef(par);

// first of all allign CodecLevel with general (not per-view) parameters: resolution, framerate, DPB size
    if (par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        mfxU16 minLevel = GetLevelLimitByFrameSize(par);
        if (par.calcParam.mvcPerViewPar.codecLevel != 0 && par.calcParam.mvcPerViewPar.codecLevel < minLevel)
        {
            if (extBits.SPSBuffer)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            changed = true;
            par.calcParam.mvcPerViewPar.codecLevel = minLevel;
        }
        if (par.calcParam.mvcPerViewPar.codecLevel == 0)
            par.calcParam.mvcPerViewPar.codecLevel = minLevel;
    }

    if (extSps.vui.flags.timingInfoPresent   &&
        par.mfx.FrameInfo.Width         != 0 &&
        par.mfx.FrameInfo.Height        != 0 &&
        par.mfx.FrameInfo.FrameRateExtN != 0 &&
        par.mfx.FrameInfo.FrameRateExtD != 0)
    {
        mfxU16 minLevel = GetLevelLimitByMbps(par);
        if (par.calcParam.mvcPerViewPar.codecLevel != 0 && par.calcParam.mvcPerViewPar.codecLevel < minLevel)
        {
            if (extBits.SPSBuffer)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            changed = true;
            par.calcParam.mvcPerViewPar.codecLevel = minLevel;
        }
    }

    if (par.mfx.NumRefFrame      != 0 &&
        par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        mfxU16 minLevel = GetLevelLimitByDpbSize(par);
        if (par.calcParam.mvcPerViewPar.codecLevel != 0 && par.calcParam.mvcPerViewPar.codecLevel < minLevel)
        {
            if (extBits.SPSBuffer)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            changed = false;
            par.calcParam.mvcPerViewPar.codecLevel = minLevel;
        }
    }

    // check MVC per-view parameters (bitrates, buffer size, initial delay, level)
    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP && par.calcParam.mvcPerViewPar.targetKbps != 0)
    {
        if (par.mfx.FrameInfo.Width         != 0 &&
            par.mfx.FrameInfo.Height        != 0 &&
            par.mfx.FrameInfo.FrameRateExtN != 0 &&
            par.mfx.FrameInfo.FrameRateExtD != 0)
        {
            mfxF64 rawDataBitrate = 12.0 * par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height *
                par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD;
            mfxU32 minTargetKbps = mfxU32(std::min<mfxF64>(0xffffffff, rawDataBitrate / 1000.0 / 500.0));

            if (par.calcParam.mvcPerViewPar.targetKbps < minTargetKbps)
            {
                changed = true;
                par.calcParam.mvcPerViewPar.targetKbps = minTargetKbps;
            }
        }

        if (extSps.vui.flags.nalHrdParametersPresent || extSps.vui.flags.vclHrdParametersPresent)
        {
            mfxU16 profile = MFX_PROFILE_AVC_HIGH;
            for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
            {
                if (mfxU16 minLevel = GetLevelLimitByMaxBitrate(profile, par.calcParam.mvcPerViewPar.targetKbps))
                {
                    if (par.calcParam.mvcPerViewPar.codecLevel != 0 && par.calcParam.mvcPerViewPar.codecLevel < minLevel)
                    {
                        if (extBits.SPSBuffer)
                            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                        changed = true;
                        par.calcParam.mvcPerViewPar.codecLevel   = minLevel;
                    }
                    break;
                }
            }

            if (profile == MFX_PROFILE_UNKNOWN)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
        }
    }

    if (par.calcParam.mvcPerViewPar.targetKbps != 0 && par.calcParam.mvcPerViewPar.maxKbps != 0)
    {
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
        {
            if (par.calcParam.mvcPerViewPar.maxKbps != par.calcParam.mvcPerViewPar.targetKbps)
            {
                changed = true;
                if (extSps.vui.flags.nalHrdParametersPresent || extSps.vui.flags.vclHrdParametersPresent)
                    par.calcParam.mvcPerViewPar.targetKbps = par.calcParam.mvcPerViewPar.maxKbps;
                else
                    par.calcParam.mvcPerViewPar.maxKbps = par.calcParam.mvcPerViewPar.targetKbps;
            }
        }
        else if (
            par.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
            par.mfx.RateControlMethod == MFX_RATECONTROL_WIDI_VBR)
        {
            if (par.calcParam.mvcPerViewPar.maxKbps < par.calcParam.mvcPerViewPar.targetKbps)
            {
                if (extBits.SPSBuffer && (
                    extSps.vui.flags.nalHrdParametersPresent ||
                    extSps.vui.flags.vclHrdParametersPresent))
                    MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                changed = true;
                par.calcParam.mvcPerViewPar.maxKbps = par.calcParam.mvcPerViewPar.targetKbps;
            }
        }
    }

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP && par.calcParam.mvcPerViewPar.maxKbps != 0)
    {
        mfxU16 profile = MFX_PROFILE_AVC_HIGH;
        for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
        {
            if (mfxU16 minLevel = GetLevelLimitByMaxBitrate(profile, par.calcParam.mvcPerViewPar.maxKbps))
            {
                if (par.calcParam.mvcPerViewPar.codecLevel != 0 && par.calcParam.mvcPerViewPar.codecLevel < minLevel)
                {
                    if (extBits.SPSBuffer)
                        MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                    changed = true;
                    par.calcParam.mvcPerViewPar.codecLevel = minLevel;
                }
                break;
            }
        }

        if (profile == MFX_PROFILE_UNKNOWN)
            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));
    }

    if (par.calcParam.mvcPerViewPar.bufferSizeInKB != 0)
    {
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        {
            mfxU32 uncompressedSizeInKb = GetMaxCodedFrameSizeInKB(par);
            if (par.calcParam.mvcPerViewPar.bufferSizeInKB < uncompressedSizeInKb)
            {
                changed = true;
                par.calcParam.mvcPerViewPar.bufferSizeInKB = uncompressedSizeInKb;
            }
        }
        else
        {
            mfxF64 avgFrameSizeInKB = 0;
            if (par.mfx.RateControlMethod       != MFX_RATECONTROL_AVBR &&
                par.mfx.FrameInfo.FrameRateExtN != 0 &&
                par.mfx.FrameInfo.FrameRateExtD != 0 &&
                par.calcParam.mvcPerViewPar.targetKbps != 0)
            {
                mfxF64 frameRate = mfxF64(par.mfx.FrameInfo.FrameRateExtN) / par.mfx.FrameInfo.FrameRateExtD;
                avgFrameSizeInKB = par.calcParam.mvcPerViewPar.targetKbps / frameRate / 8;

                if (par.calcParam.mvcPerViewPar.bufferSizeInKB < 2 * avgFrameSizeInKB)
                {
                    if (extBits.SPSBuffer && (
                        extSps.vui.flags.nalHrdParametersPresent ||
                        extSps.vui.flags.vclHrdParametersPresent))
                        MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                    changed = true;
                    par.calcParam.mvcPerViewPar.bufferSizeInKB = mfxU16(2 * avgFrameSizeInKB + 1);
                }
            }

            mfxU16 profile = MFX_PROFILE_AVC_HIGH;
            for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
            {
                if (mfxU16 minLevel = GetLevelLimitByBufferSize(profile, par.calcParam.mvcPerViewPar.bufferSizeInKB))
                {
                    if (par.calcParam.mvcPerViewPar.codecLevel != 0 && par.calcParam.mvcPerViewPar.codecLevel < minLevel)
                    {
                        if (extBits.SPSBuffer)
                            MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

                        changed = true;
                        par.calcParam.mvcPerViewPar.codecLevel = minLevel;
                    }
                    break;
                }
            }

            if (profile == MFX_PROFILE_UNKNOWN)
                MFX_RETURN(Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM));

            if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
                par.mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
                par.calcParam.mvcPerViewPar.initialDelayInKB != 0)
            {
                if (par.calcParam.mvcPerViewPar.initialDelayInKB > par.calcParam.mvcPerViewPar.bufferSizeInKB)
                {
                    changed = true;
                    par.calcParam.mvcPerViewPar.initialDelayInKB = par.calcParam.mvcPerViewPar.bufferSizeInKB / 2;
                }

                if (avgFrameSizeInKB != 0 && par.calcParam.mvcPerViewPar.initialDelayInKB < avgFrameSizeInKB)
                {
                    changed = true;
                    par.calcParam.mvcPerViewPar.initialDelayInKB = mfxU16(std::min<mfxF64>(par.calcParam.mvcPerViewPar.bufferSizeInKB, avgFrameSizeInKB));
                }
            }
        }
    }

    MFX_RETURN(changed ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE);
}

// check mfxExtMVCSeqDesc as in Query
mfxStatus MfxHwH264Encode::CheckMVCSeqDescQueryLike(mfxExtMVCSeqDesc * mvcSeqDesc)
{
    bool unsupported = false;
    if (mvcSeqDesc->NumView > 0 && mvcSeqDesc->NumView != 2)
    {
        unsupported = true;
        mvcSeqDesc->NumView = 0;
    }

    if (mvcSeqDesc->NumOP > 1024)
    {
        unsupported = true;
        mvcSeqDesc->NumOP = 0;
    }

    if (mvcSeqDesc->NumOP > 0 && mvcSeqDesc->NumViewId > 1024 * mvcSeqDesc->NumOP)
    {
        unsupported = true;
        mvcSeqDesc->NumViewId = 0;
    }

    if (mvcSeqDesc->NumViewAlloc > 0 && (mvcSeqDesc->NumViewAlloc < mvcSeqDesc->NumView))
    {
        unsupported = true;
        mvcSeqDesc->NumViewAlloc = 0;
    }

    MFX_RETURN(unsupported ? MFX_ERR_UNSUPPORTED : MFX_ERR_NONE);
}

// check mfxExtMVCSeqDesc before encoding
mfxStatus MfxHwH264Encode::CheckAndFixMVCSeqDesc(mfxExtMVCSeqDesc * mvcSeqDesc, bool isViewOutput)
{
    if (mvcSeqDesc == nullptr)
    {
        MFX_RETURN(MFX_ERR_NULL_PTR);
    }

    bool unsupported = false;
    bool changed = false;
    if (mvcSeqDesc->NumView > 2 || mvcSeqDesc->NumView < 2)
    {
        unsupported = true;
        mvcSeqDesc->NumView = 0;
    }

    if (mvcSeqDesc->NumOP > 1024)
    {
        unsupported = true;
        mvcSeqDesc->NumOP = 0;
    }

    if (mvcSeqDesc->NumOP > 0 && mvcSeqDesc->NumViewId > 1024 * mvcSeqDesc->NumOP)
    {
        unsupported = true;
        mvcSeqDesc->NumViewId = 0;
    }

    if (mvcSeqDesc->NumViewAlloc > 0 && mvcSeqDesc->NumViewAlloc < mvcSeqDesc->NumView)
    {
        changed = true;
        mvcSeqDesc->NumViewAlloc = 0;
        mvcSeqDesc->View = 0;
    }

    if (mvcSeqDesc->NumViewAlloc > 0)
    {
        if (mvcSeqDesc->View == 0)
        {
            unsupported = true;
        }
        else
        {
            if (mvcSeqDesc->View[0].ViewId != 0 && isViewOutput)
            {
                 changed = true;
                mvcSeqDesc->View[0].ViewId = 0;
            }
        }
    }

    if (mvcSeqDesc->NumViewIdAlloc > 0 && mvcSeqDesc->NumViewIdAlloc < mvcSeqDesc->NumViewId)
    {
        changed = true;
        mvcSeqDesc->NumViewId = 0;
        mvcSeqDesc->NumViewIdAlloc = 0;
        mvcSeqDesc->ViewId = 0;
    }

    if (mvcSeqDesc->NumViewIdAlloc > 0)
    {
        if (mvcSeqDesc->ViewId == 0)
        {
            unsupported = true;
        }
        else
        {
            if (mvcSeqDesc->ViewId[0] != 0 && isViewOutput)
            {
                changed = true;
                mvcSeqDesc->ViewId[0] = 0;
            }
        }
    }

    if (mvcSeqDesc->NumOPAlloc > 0 &&  mvcSeqDesc->NumOPAlloc < mvcSeqDesc->NumOP)
    {
        changed = true;
        mvcSeqDesc->NumOP = 0;
        mvcSeqDesc->NumOPAlloc = 0;
        mvcSeqDesc->OP = 0;
    }

    if (mvcSeqDesc->NumOPAlloc > 0 && mvcSeqDesc->OP == 0)
    {
        unsupported = true;
    }

    MFX_RETURN(unsupported ? MFX_ERR_UNSUPPORTED :
        changed ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE);
}

void MfxHwH264Encode::InheritDefaultValues(
    MfxVideoParam const & parInit,
    MfxVideoParam &       parReset,
    MFX_ENCODE_CAPS const & hwCaps,
    mfxVideoParam const * parResetIn)
{
    mfxExtCodingOption const & extOptInit = GetExtBufferRef(parInit);
    mfxExtCodingOption *       extOptReset = GetExtBuffer(parReset);
    mfxExtCodingOption3 const & extOpt3Init = GetExtBufferRef(parInit);
    mfxExtCodingOption3 *       extOpt3Reset = GetExtBuffer(parReset);
    mfxExtCodingOption2 const & extOpt2Init = GetExtBufferRef(parInit);
    mfxExtCodingOption2 *       extOpt2Reset = GetExtBuffer(parReset);

    mfxU32 TCBRCTargetFrameSize = 0;

    InheritOption(extOptInit.NalHrdConformance, extOptReset->NalHrdConformance);
    InheritOption(extOpt3Init.LowDelayBRC, extOpt3Reset->LowDelayBRC);
    InheritOption(parInit.mfx.RateControlMethod, parReset.mfx.RateControlMethod);
    InheritOption(parInit.mfx.FrameInfo.FrameRateExtN, parReset.mfx.FrameInfo.FrameRateExtN);
    InheritOption(parInit.mfx.FrameInfo.FrameRateExtD, parReset.mfx.FrameInfo.FrameRateExtD);
    InheritOption(parInit.mfx.BRCParamMultiplier, parReset.mfx.BRCParamMultiplier);

    InheritOption(extOpt3Init.ScenarioInfo, extOpt3Reset->ScenarioInfo);
    InheritOption(extOpt3Init.AdaptiveMaxFrameSize, extOpt3Reset->AdaptiveMaxFrameSize);
    InheritOption(parInit.mfx.LowPower, parReset.mfx.LowPower);
    InheritOption(extOpt2Init.LookAheadDepth, extOpt2Reset->LookAheadDepth);

    if (IsTCBRC(parReset, hwCaps))
    {
        TCBRCTargetFrameSize = GetAvgFrameSizeInBytes(parReset, false);
        if (extOpt2Reset->MaxFrameSize == 0 && parReset.mfx.MaxKbps != 0)
            extOpt2Reset->MaxFrameSize = GetAvgFrameSizeInBytes(parReset, true);

        // TCBRC Reset is a special case - Below BRC params are used only to calculate TCBRCTargetFrameSize
        // and after that, they must be restored to parInit params.
        // This is needed to avoid real heavy BRC reset
        //
        // For case, when TCBRCTargetFrameSize will be greater than extOpt2.MaxFrameSize or extOpt3.MaxFrameSizeI
        // real BRC Reset will be called
        parReset.mfx.TargetKbps         = parInit.mfx.TargetKbps;
        parReset.mfx.MaxKbps            = parInit.mfx.MaxKbps;
        parReset.mfx.InitialDelayInKB   = parInit.mfx.InitialDelayInKB;
        parReset.mfx.BufferSizeInKB     = parInit.mfx.BufferSizeInKB;
        parReset.mfx.BRCParamMultiplier = parInit.mfx.BRCParamMultiplier;
    }

    InheritOption(parInit.AsyncDepth,             parReset.AsyncDepth);
    InheritOption(parInit.mfx.CodecId,            parReset.mfx.CodecId);
    InheritOption(parInit.mfx.CodecProfile,       parReset.mfx.CodecProfile);
    InheritOption(parInit.mfx.CodecLevel,         parReset.mfx.CodecLevel);
    InheritOption(parInit.mfx.NumThread,          parReset.mfx.NumThread);
    InheritOption(parInit.mfx.TargetUsage,        parReset.mfx.TargetUsage);
    InheritOption(parInit.mfx.GopPicSize,         parReset.mfx.GopPicSize);
    InheritOption(parInit.mfx.GopRefDist,         parReset.mfx.GopRefDist);
    InheritOption(parInit.mfx.GopOptFlag,         parReset.mfx.GopOptFlag);
    InheritOption(parInit.mfx.IdrInterval,        parReset.mfx.IdrInterval);
    InheritOption(parInit.mfx.BufferSizeInKB,     parReset.mfx.BufferSizeInKB);
    InheritOption(parInit.mfx.NumSlice,           parReset.mfx.NumSlice);
    InheritOption(parInit.mfx.NumRefFrame,        parReset.mfx.NumRefFrame);
    InheritOption(parInit.mfx.LowPower,           parReset.mfx.LowPower);

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_CBR && parReset.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
    {
        InheritOption(parInit.mfx.InitialDelayInKB, parReset.mfx.InitialDelayInKB);
        InheritOption(parInit.mfx.TargetKbps,       parReset.mfx.TargetKbps);
    }

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_VBR && parReset.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
    {
        InheritOption(parInit.mfx.InitialDelayInKB, parReset.mfx.InitialDelayInKB);
        InheritOption(parInit.mfx.TargetKbps,       parReset.mfx.TargetKbps);
        InheritOption(parInit.mfx.MaxKbps,          parReset.mfx.MaxKbps);
    }

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_CQP && parReset.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        InheritOption(parInit.mfx.QPI, parReset.mfx.QPI);
        InheritOption(parInit.mfx.QPP, parReset.mfx.QPP);
        InheritOption(parInit.mfx.QPB, parReset.mfx.QPB);
    }

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_AVBR && parReset.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        InheritOption(parInit.mfx.Accuracy,    parReset.mfx.Accuracy);
        InheritOption(parInit.mfx.Convergence, parReset.mfx.Convergence);
    }

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_ICQ && parReset.mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ)
        InheritOption(parInit.mfx.ICQQuality, parReset.mfx.ICQQuality);

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_VCM && parReset.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
    {
        InheritOption(parInit.mfx.InitialDelayInKB, parReset.mfx.InitialDelayInKB);
        InheritOption(parInit.mfx.TargetKbps,       parReset.mfx.TargetKbps);
        InheritOption(parInit.mfx.MaxKbps,          parReset.mfx.MaxKbps);
    }


    InheritOption(parInit.mfx.FrameInfo.FourCC,         parReset.mfx.FrameInfo.FourCC);
    InheritOption(parInit.mfx.FrameInfo.FourCC,         parReset.mfx.FrameInfo.FourCC);
    InheritOption(parInit.mfx.FrameInfo.Width,          parReset.mfx.FrameInfo.Width);
    InheritOption(parInit.mfx.FrameInfo.Height,         parReset.mfx.FrameInfo.Height);
    InheritOption(parInit.mfx.FrameInfo.CropX,          parReset.mfx.FrameInfo.CropX);
    InheritOption(parInit.mfx.FrameInfo.CropY,          parReset.mfx.FrameInfo.CropY);
    InheritOption(parInit.mfx.FrameInfo.CropW,          parReset.mfx.FrameInfo.CropW);
    InheritOption(parInit.mfx.FrameInfo.CropH,          parReset.mfx.FrameInfo.CropH);
    InheritOption(parInit.mfx.FrameInfo.AspectRatioW,   parReset.mfx.FrameInfo.AspectRatioW);
    InheritOption(parInit.mfx.FrameInfo.AspectRatioH,   parReset.mfx.FrameInfo.AspectRatioH);


    InheritOption(extOptInit.RateDistortionOpt,     extOptReset->RateDistortionOpt);
    InheritOption(extOptInit.MECostType,            extOptReset->MECostType);
    InheritOption(extOptInit.MESearchType,          extOptReset->MESearchType);
    InheritOption(extOptInit.MVSearchWindow.x,      extOptReset->MVSearchWindow.x);
    InheritOption(extOptInit.MVSearchWindow.y,      extOptReset->MVSearchWindow.y);
    InheritOption(extOptInit.EndOfSequence,         extOptReset->EndOfSequence);
    InheritOption(extOptInit.FramePicture,          extOptReset->FramePicture);
    InheritOption(extOptInit.CAVLC,                 extOptReset->CAVLC);
    InheritOption(extOptInit.SingleSeiNalUnit,      extOptReset->SingleSeiNalUnit);
    InheritOption(extOptInit.VuiVclHrdParameters,   extOptReset->VuiVclHrdParameters);
    InheritOption(extOptInit.RefPicListReordering,  extOptReset->RefPicListReordering);
    InheritOption(extOptInit.ResetRefList,          extOptReset->ResetRefList);
    InheritOption(extOptInit.RefPicMarkRep,         extOptReset->RefPicMarkRep);
    InheritOption(extOptInit.FieldOutput,           extOptReset->FieldOutput);
    InheritOption(extOptInit.IntraPredBlockSize,    extOptReset->IntraPredBlockSize);
    InheritOption(extOptInit.InterPredBlockSize,    extOptReset->InterPredBlockSize);
    InheritOption(extOptInit.MVPrecision,           extOptReset->MVPrecision);
    InheritOption(extOptInit.MaxDecFrameBuffering,  extOptReset->MaxDecFrameBuffering);
    InheritOption(extOptInit.AUDelimiter,           extOptReset->AUDelimiter);
    InheritOption(extOptInit.EndOfStream,           extOptReset->EndOfStream);
    InheritOption(extOptInit.PicTimingSEI,          extOptReset->PicTimingSEI);
    InheritOption(extOptInit.VuiNalHrdParameters,   extOptReset->VuiNalHrdParameters);


    if (!parResetIn || !(mfxExtCodingOption2 const *)GetExtBuffer(*parResetIn)) //user should be able to disable IntraRefresh via Reset()
    {
        InheritOption(extOpt2Init.IntRefType,      extOpt2Reset->IntRefType);
        InheritOption(extOpt2Init.IntRefCycleSize, extOpt2Reset->IntRefCycleSize);
    }
    InheritOption(extOpt2Init.DisableVUI,      extOpt2Reset->DisableVUI);
    InheritOption(extOpt2Init.SkipFrame,       extOpt2Reset->SkipFrame);
    InheritOption(extOpt3Init.PRefType,        extOpt3Reset->PRefType);

#if defined(MFX_ENABLE_EXT_BRC)
     InheritOption(extOpt2Init.ExtBRC,  extOpt2Reset->ExtBRC);
#endif


    InheritOption(extOpt3Init.NumSliceI, extOpt3Reset->NumSliceI);
    InheritOption(extOpt3Init.NumSliceP, extOpt3Reset->NumSliceP);
    InheritOption(extOpt3Init.NumSliceB, extOpt3Reset->NumSliceB);
    if (!parResetIn || !(mfxExtCodingOption3 const *)GetExtBuffer(*parResetIn))
        InheritOption(extOpt3Init.IntRefCycleDist, extOpt3Reset->IntRefCycleDist);

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_QVBR && parReset.mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
    {
        InheritOption(parInit.mfx.InitialDelayInKB, parReset.mfx.InitialDelayInKB);
        InheritOption(parInit.mfx.TargetKbps,       parReset.mfx.TargetKbps);
        InheritOption(parInit.mfx.MaxKbps,          parReset.mfx.MaxKbps);
        InheritOption(extOpt3Init.QVBRQuality,      extOpt3Reset->QVBRQuality);
    }


#if defined(MFX_ENABLE_EXT_BRC)

    mfxExtBRC & extBRCInit  = GetExtBufferRef(parInit);
    mfxExtBRC & extBRCReset = GetExtBufferRef(parReset);

    if (!extBRCReset.pthis &&
        !extBRCReset.Init &&
        !extBRCReset.Reset &&
        !extBRCReset.Close &&
        !extBRCReset.GetFrameCtrl &&
        !extBRCReset.Update)
    {
        extBRCReset = extBRCInit;
    }

#endif
#if defined(MFX_ENABLE_ENCTOOLS)
    mfxExtEncToolsConfig &configInit = GetExtBufferRef(parInit);
    mfxExtEncToolsConfig &configReset = GetExtBufferRef(parReset);

    if (!IsEncToolsOptOn(configReset) &&
        IsEncToolsOptOn(configInit))
        ResetEncToolsPar(configReset, 0);

    InheritOption(configInit.AdaptiveI, configReset.AdaptiveI);
    InheritOption(configInit.AdaptiveB, configReset.AdaptiveB);
    InheritOption(configInit.AdaptiveRefP, configReset.AdaptiveRefP);
    InheritOption(configInit.AdaptiveRefB, configReset.AdaptiveRefB);
    InheritOption(configInit.SceneChange, configReset.SceneChange);
    InheritOption(configInit.AdaptiveLTR, configReset.AdaptiveLTR);
    InheritOption(configInit.AdaptivePyramidQuantP, configReset.AdaptivePyramidQuantP);
    InheritOption(configInit.AdaptivePyramidQuantB, configReset.AdaptivePyramidQuantB);
    InheritOption(configInit.BRCBufferHints, configReset.BRCBufferHints);
    InheritOption(configInit.BRC, configReset.BRC);
#endif

    parReset.SyncVideoToCalculableParam();
    parReset.calcParam.TCBRCTargetFrameSize = TCBRCTargetFrameSize;

    // not inherited:
    // InheritOption(parInit.mfx.FrameInfo.PicStruct,      parReset.mfx.FrameInfo.PicStruct);
    // InheritOption(parInit.IOPattern,                    parReset.IOPattern);
    // InheritOption(parInit.mfx.FrameInfo.ChromaFormat,   parReset.mfx.FrameInfo.ChromaFormat);
}


namespace
{
    bool IsDyadic(mfxU32 tempScales[8], mfxU32 numTempLayers)
    {
        if (numTempLayers > 0)
            for (mfxU32 i = 1; i < numTempLayers; ++i)
                if (tempScales[i] != 2 * tempScales[i - 1])
                    return false;
        return true;
    }

    bool IsPowerOf2(mfxU32 n)
    {
        return (n & (n - 1)) == 0;
    }
};

bool IsHRDBasedBRCMethod(mfxU16  RateControlMethod)
{
    return  RateControlMethod != MFX_RATECONTROL_CQP && RateControlMethod != MFX_RATECONTROL_AVBR &&
            RateControlMethod != MFX_RATECONTROL_ICQ &&
            RateControlMethod != MFX_RATECONTROL_LA && RateControlMethod != MFX_RATECONTROL_LA_ICQ;
}
bool MfxHwH264Encode::isSWBRC(MfxVideoParam const & par)
{
    mfxExtCodingOption2 &extOpt2 = GetExtBufferRef(par);
    return (bRateControlLA(par.mfx.RateControlMethod) || (IsOn(extOpt2.ExtBRC) && (extOpt2.LookAheadDepth == 0) && (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR)));
}


bool MfxHwH264Encode::isAdaptiveQP(MfxVideoParam const & video)
{
    if ((video.mfx.GopRefDist == 8) &&
        (video.mfx.FrameInfo.PicStruct == 0 || video.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) &&
        video.calcParam.numTemporalLayer == 0
        )
    {
        mfxExtCodingOption3 &extOpt3 = GetExtBufferRef(video);
        return IsOff(extOpt3.EnableQPOffset);
    }
    return false;
}

bool MfxHwH264Encode::isAdaptiveCQMSupported(mfxU16 scenarioInfo, bool isLowPowerOn)
{
    return ((scenarioInfo == MFX_SCENARIO_GAME_STREAMING) || (scenarioInfo == MFX_SCENARIO_REMOTE_GAMING)) && isLowPowerOn;
}

void MfxHwH264Encode::SetDefaults(
    MfxVideoParam &         par,
    MFX_ENCODE_CAPS const & hwCaps,
    bool                    setExtAlloc,
    eMFXHWType              platform,
    eMFXVAType              vaType,
    eMFXGTConfig            config)
{
    mfxExtCodingOption *       extOpt  = GetExtBuffer(par);
    mfxExtCodingOption2 *      extOpt2 = GetExtBuffer(par);
    mfxExtCodingOption3 *      extOpt3 = GetExtBuffer(par);
    mfxExtCodingOptionDDI *    extDdi  = GetExtBuffer(par);
    mfxExtVideoSignalInfo *    extVsi  = GetExtBuffer(par);
    mfxExtCodingOptionSPSPPS * extBits = GetExtBuffer(par);
    mfxExtSpsHeader *          extSps  = GetExtBuffer(par);
    mfxExtPpsHeader *          extPps  = GetExtBuffer(par);
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    mfxExtSVCSeqDesc *         extSvc  = GetExtBuffer(par);
    mfxExtSVCRateControl *     extRc   = GetExtBuffer(par);
#endif
    mfxExtChromaLocInfo*       extCli  = GetExtBuffer(par);
#if defined(MFX_ENABLE_ENCTOOLS)
    mfxExtEncToolsConfig *extConfig = GetExtBuffer(par);
#endif

    if (extOpt2->UseRawRef)
        extDdi->RefRaw = extOpt2->UseRawRef;
    else if (!extOpt2->UseRawRef)
        extOpt2->UseRawRef = extDdi->RefRaw;

    if (extOpt2->UseRawRef == MFX_CODINGOPTION_UNKNOWN)
        extOpt2->UseRawRef = MFX_CODINGOPTION_OFF;

    if (IsOn(par.mfx.LowPower))
    {
        if (par.mfx.GopRefDist == 0)
        {
            // on DG2+ 3 B-frames with B-pyramid is default
            par.mfx.GopRefDist = H264ECaps::IsVDEncBFrameSupported(platform) ? 4 : 1;
        }

        if (par.mfx.FrameInfo.PicStruct == 0)
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    if (extOpt2->MaxSliceSize)
    {
        if (par.mfx.GopRefDist == 0)
            par.mfx.GopRefDist = 1;
        if (par.mfx.FrameInfo.PicStruct == 0)
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        if (par.AsyncDepth == 0)
            par.AsyncDepth = 1;
        if (!IsDriverSliceSizeControlEnabled(par, hwCaps))
        {
            if (par.mfx.RateControlMethod == 0)
                par.mfx.RateControlMethod = MFX_RATECONTROL_LA;
            if (extOpt2->LookAheadDepth == 0)
                extOpt2->LookAheadDepth = 1;
            if (extOpt2->LookAheadDS == MFX_LOOKAHEAD_DS_UNKNOWN)
                extOpt2->LookAheadDS = MFX_LOOKAHEAD_DS_2x;
        }
        else
        {
            if (par.mfx.RateControlMethod == 0)
                par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
        }
    }

    mfxU8 fieldCodingPossible = IsFieldCodingPossible(par);

    if (par.IOPattern == 0)
        par.IOPattern = setExtAlloc
            ? mfxU16(MFX_IOPATTERN_IN_VIDEO_MEMORY)
            : mfxU16(MFX_IOPATTERN_IN_SYSTEM_MEMORY);

    if (par.AsyncDepth == 0)
        par.AsyncDepth = GetDefaultAsyncDepth(par);

    if (par.mfx.TargetUsage == 0)
        par.mfx.TargetUsage = 4;

    if (par.mfx.NumSlice == 0)
    {
        if (extOpt2->NumMbPerSlice != 0) {
            par.mfx.NumSlice = (
                (par.mfx.FrameInfo.Width / 16) *
                (par.mfx.FrameInfo.Height / 16 / (fieldCodingPossible ? 2 : 1)) +
                extOpt2->NumMbPerSlice - 1) / extOpt2->NumMbPerSlice;
        }
        else
            par.mfx.NumSlice = 1;
    }

    if (par.mfx.RateControlMethod == 0)
        par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP &&
        par.calcParam.cqpHrdMode == 0)
    {
        mfxU16 maxQP = 51;
        mfxU16 minQP = IsOn(par.mfx.LowPower) ? 10 : 1;   // 10 is min QP for VDENC
        if (!par.mfx.QPI)
            par.mfx.QPI = mfxU16(std::max<mfxI32>(par.mfx.QPP - 1, minQP) * !!par.mfx.QPP);
        if (!par.mfx.QPI)
            par.mfx.QPI = mfxU16(std::max<mfxI32>(par.mfx.QPB - 2, minQP) * !!par.mfx.QPB);
        if (!par.mfx.QPI)
            par.mfx.QPI = std::max<mfxU16>(minQP, (maxQP + 1) / 2);
        if (!par.mfx.QPP)
            par.mfx.QPP = std::min<mfxU16>(par.mfx.QPI + 1, maxQP);
        if (!par.mfx.QPB)
            par.mfx.QPB = std::min<mfxU16>(par.mfx.QPP + 1, maxQP);
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        if (par.mfx.Accuracy == 0)
            par.mfx.Accuracy = 100;

        if (par.mfx.Convergence == 0)
            par.mfx.Convergence = AVBR_CONVERGENCE_MAX;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ || par.mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ)
    {
        if (par.mfx.ICQQuality == 0)
            par.mfx.ICQQuality = 26;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
    {
        if (extOpt3->QVBRQuality == 0)
            extOpt3->QVBRQuality = 26;
    }

    if (par.mfx.GopRefDist == 0)
    {
        if (IsAvcBaseProfile(par.mfx.CodecProfile) ||
            par.mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH ||
            (par.calcParam.numTemporalLayer > 0 && par.calcParam.tempScalabilityMode) ||
            hwCaps.ddi_caps.SliceIPOnly)
        {
            par.mfx.GopRefDist = 1;
        }
        else if (par.calcParam.numTemporalLayer > 1)
        { // svc temporal layers
            par.mfx.GopRefDist = 1;
            extOpt2->BRefType = MFX_B_REF_OFF;

            if (IsDyadic(par.calcParam.scale, par.calcParam.numTemporalLayer) &&
                par.mfx.GopPicSize % par.calcParam.scale[par.calcParam.numTemporalLayer - 1] == 0)
            {
                if (par.calcParam.numTemporalLayer == 2)
                {
                    par.mfx.GopRefDist = 2;
                    extOpt2->BRefType = MFX_B_REF_OFF;
                }
                else
                {
                    par.mfx.GopRefDist = 4;
                    extOpt2->BRefType = MFX_B_REF_PYRAMID;
                }
            }
        }
        else
        {
            par.mfx.GopRefDist = (IsOn(extOpt2->AdaptiveI) ||
                                  IsOn(extOpt2->AdaptiveB) ||
#if defined(MFX_ENABLE_ENCTOOLS)
                                  IsOn(extConfig->AdaptiveI) ||
                                  IsOn(extConfig->AdaptiveB) ||
#endif
                                  IsAdaptiveLtrOn(par))? 8 : 3;
        }
    }

    if (par.mfx.GopPicSize > 0 && par.mfx.GopPicSize <= par.mfx.GopRefDist)
        par.mfx.GopRefDist = par.mfx.GopPicSize;

    if (  (par.mfx.RateControlMethod == MFX_RATECONTROL_LA
        || par.mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD)
        && (extOpt3->WinBRCMaxAvgKbps || extOpt3->WinBRCSize))
    {
        if (!extOpt3->WinBRCMaxAvgKbps)
        {
            extOpt3->WinBRCMaxAvgKbps = par.mfx.TargetKbps * 2;
            par.calcParam.WinBRCMaxAvgKbps = par.calcParam.targetKbps * 2;
        }
        if (!extOpt3->WinBRCSize)
        {
            extOpt3->WinBRCSize = (mfxU16)(par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD);
            extOpt3->WinBRCSize = (!extOpt3->WinBRCSize) ? 30 : extOpt3->WinBRCSize;
        }
    }


    //WA for MVC quality problem on progressive content.
    if (IsMvcProfile(par.mfx.CodecProfile)) {
        extDdi->NumActiveRefP = extDdi->NumActiveRefBL0 = extDdi->NumActiveRefBL1 = 1;
    }
    if (extDdi->NumActiveRefP == 0)
    {
        {
            mfxU16 maxNumActivePL0 = GetMaxNumRefActivePL0(par.mfx.TargetUsage, platform, IsOn(par.mfx.LowPower), par.mfx.FrameInfo);
            extDdi->NumActiveRefP = extOpt3->NumRefActiveP[0] ? std::min(maxNumActivePL0, extOpt3->NumRefActiveP[0])
                : GetDefaultNumRefActivePL0(par.mfx, platform);
        }
    }

    if (par.mfx.GopRefDist > 1)
    {
        if (extDdi->NumActiveRefBL0 == 0)
        {
            {
                mfxU16 maxNumActiveBL0 = GetMaxNumRefActiveBL0(par.mfx.TargetUsage, platform, IsOn(par.mfx.LowPower));
                extDdi->NumActiveRefBL0 = extOpt3->NumRefActiveBL0[0] ? std::min(maxNumActiveBL0, extOpt3->NumRefActiveBL0[0])
                    : GetDefaultNumRefActiveBL0(par.mfx, platform);
            }
        } /* if (extDdi->NumActiveRefBL0 == 0) */

        if (extDdi->NumActiveRefBL1 == 0)
        {
            {
                mfxU16 maxNumActiveBL1 = GetMaxNumRefActiveBL1(par.mfx.TargetUsage, par.mfx.FrameInfo.PicStruct, IsOn(par.mfx.LowPower));
                extDdi->NumActiveRefBL1 = extOpt3->NumRefActiveBL1[0] ? std::min(maxNumActiveBL1, extOpt3->NumRefActiveBL1[0])
                    : GetDefaultNumRefActiveBL1(par.mfx, platform);
            }
        } /* if (extDdi->NumActiveRefBL1 == 0) */
    }

    if (par.mfx.GopPicSize == 0)
    {
        mfxU32 maxScale = (par.calcParam.numTemporalLayer) > 0
            ? par.calcParam.scale[par.calcParam.numTemporalLayer - 1]
            : 1;
        par.mfx.GopPicSize = mfxU16((256 + maxScale - 1) / maxScale * maxScale);
    }

    if (extOpt2->BRefType == MFX_B_REF_UNKNOWN)
    {
        assert(par.mfx.GopRefDist > 0);
        assert(par.mfx.GopPicSize > 0);

        if (IsDyadic(par.calcParam.scale, par.calcParam.numTemporalLayer) &&
            par.mfx.GopRefDist >= 4 &&
            (!par.mfx.NumRefFrame || par.mfx.NumRefFrame >= GetMinNumRefFrameForPyramid(par)) &&
            (!IsMvcProfile(par.mfx.CodecProfile) || (IsPowerOf2(par.mfx.GopRefDist) && (par.mfx.GopPicSize % par.mfx.GopRefDist) == 0)) &&
            par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
        {
            extOpt2->BRefType = MFX_B_REF_PYRAMID;
        }
        else
        {
            extOpt2->BRefType = MFX_B_REF_OFF;
        }
    }

    if (par.mfx.FrameInfo.FourCC == 0)
        par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

    if (par.mfx.FrameInfo.ChromaFormat == 0)
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

    if (par.mfx.FrameInfo.CropW == 0)
        par.mfx.FrameInfo.CropW = par.mfx.FrameInfo.Width - par.mfx.FrameInfo.CropX;

    if (par.mfx.FrameInfo.CropH == 0)
        par.mfx.FrameInfo.CropH = par.mfx.FrameInfo.Height - par.mfx.FrameInfo.CropY;

    if (par.mfx.FrameInfo.AspectRatioW == 0)
        par.mfx.FrameInfo.AspectRatioW = 1;

    if (par.mfx.FrameInfo.AspectRatioH == 0)
        par.mfx.FrameInfo.AspectRatioH = 1;

    if (extOpt->InterPredBlockSize == MFX_BLOCKSIZE_UNKNOWN)
        extOpt->InterPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;

    if (extOpt->MVPrecision == MFX_MVPRECISION_UNKNOWN)
        extOpt->MVPrecision = MFX_MVPRECISION_QUARTERPEL;

    if (extOpt->RateDistortionOpt == MFX_CODINGOPTION_UNKNOWN)
        extOpt->RateDistortionOpt = MFX_CODINGOPTION_OFF;

    if (extOpt->RateDistortionOpt == MFX_CODINGOPTION_UNKNOWN)
        extOpt->RateDistortionOpt = MFX_CODINGOPTION_OFF;

    if (extOpt->EndOfSequence == MFX_CODINGOPTION_UNKNOWN)
        extOpt->EndOfSequence = MFX_CODINGOPTION_OFF;

    if (extOpt->RefPicListReordering == MFX_CODINGOPTION_UNKNOWN)
        extOpt->RefPicListReordering = MFX_CODINGOPTION_OFF;

    if (extOpt->ResetRefList == MFX_CODINGOPTION_UNKNOWN)
        extOpt->ResetRefList = MFX_CODINGOPTION_OFF;

    if (extOpt->RefPicMarkRep == MFX_CODINGOPTION_UNKNOWN)
        extOpt->RefPicMarkRep = MFX_CODINGOPTION_OFF;

    if (extOpt->FieldOutput == MFX_CODINGOPTION_UNKNOWN)
        extOpt->FieldOutput = MFX_CODINGOPTION_OFF;

    if (extOpt->AUDelimiter == MFX_CODINGOPTION_UNKNOWN)
        extOpt->AUDelimiter = (par.calcParam.tempScalabilityMode)
            ? mfxU16(MFX_CODINGOPTION_OFF)
            : mfxU16(MFX_CODINGOPTION_ON);

    if (extOpt->EndOfStream == MFX_CODINGOPTION_UNKNOWN)
        extOpt->EndOfStream = MFX_CODINGOPTION_OFF;

    {
        SetDefaultOn(extOpt->PicTimingSEI);
    }

    if (extOpt->ViewOutput == MFX_CODINGOPTION_UNKNOWN)
        extOpt->ViewOutput = MFX_CODINGOPTION_OFF;

    if (extOpt->VuiNalHrdParameters == MFX_CODINGOPTION_UNKNOWN)
        extOpt->VuiNalHrdParameters =
            par.mfx.RateControlMethod == MFX_RATECONTROL_CQP ||
            par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR ||
            (bRateControlLA(par.mfx.RateControlMethod) && par.mfx.RateControlMethod != MFX_RATECONTROL_LA_HRD) ||
            IsOn(extOpt2->DisableVUI)
                ? mfxU16(MFX_CODINGOPTION_OFF)
                : mfxU16(MFX_CODINGOPTION_ON);

    if (extOpt->VuiVclHrdParameters == MFX_CODINGOPTION_UNKNOWN)
        extOpt->VuiVclHrdParameters = MFX_CODINGOPTION_OFF;

    if (extOpt->NalHrdConformance == MFX_CODINGOPTION_UNKNOWN)
        extOpt->NalHrdConformance = IsOn(extOpt->VuiNalHrdParameters) || IsOn(extOpt->VuiVclHrdParameters)
            ? mfxU16(MFX_CODINGOPTION_ON)
            : mfxU16(MFX_CODINGOPTION_OFF);

    if (extOpt->CAVLC == MFX_CODINGOPTION_UNKNOWN)
        extOpt->CAVLC = (IsAvcBaseProfile(par.mfx.CodecProfile) || hwCaps.ddi_caps.NoCabacSupport)
            ? mfxU16(MFX_CODINGOPTION_ON)
            : mfxU16(MFX_CODINGOPTION_OFF);

    if (extOpt->SingleSeiNalUnit == MFX_CODINGOPTION_UNKNOWN)
        extOpt->SingleSeiNalUnit = MFX_CODINGOPTION_ON;

    if (extDdi->MEInterpolationMethod == ENC_INTERPOLATION_TYPE_NONE)
        extDdi->MEInterpolationMethod = ENC_INTERPOLATION_TYPE_AVC6TAP;

    if (extDdi->RefRaw == MFX_CODINGOPTION_UNKNOWN)
        extDdi->RefRaw = MFX_CODINGOPTION_OFF;

    if (extDdi->DirectSpatialMvPredFlag == MFX_CODINGOPTION_UNKNOWN)
        extDdi->DirectSpatialMvPredFlag = MFX_CODINGOPTION_ON;

    if (extDdi->Hme == MFX_CODINGOPTION_UNKNOWN)
        extDdi->Hme = MFX_CODINGOPTION_ON;

#if defined(MFX_ENABLE_EXT_BRC)
    if (extOpt2->ExtBRC == MFX_CODINGOPTION_UNKNOWN)
        extOpt2->ExtBRC = MFX_CODINGOPTION_OFF;
#endif

    if (extOpt2->LookAheadDepth == 0)
    {
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ)
            extOpt2->LookAheadDepth = std::max<mfxU16>(10, 2 * par.mfx.GopRefDist);
        else if (bRateControlLA(par.mfx.RateControlMethod))
            extOpt2->LookAheadDepth = std::max<mfxU16>(40, 2 * par.mfx.GopRefDist);

    }
    if (extDdi->LookAheadDependency == 0)
    {
        if (bIntRateControlLA(par.mfx.RateControlMethod) && par.mfx.RateControlMethod != MFX_RATECONTROL_LA_ICQ)
        {
            extDdi->LookAheadDependency = std::min<mfxU16>(10, extOpt2->LookAheadDepth / 4);
            extDdi->LookAheadDependency = mfx::clamp<mfxU16>(extDdi->LookAheadDependency, std::min<mfxU16>(par.mfx.GopRefDist + 1, extOpt2->LookAheadDepth * 2 / 3), (extOpt2->LookAheadDepth * 2 / 3));
        }
        else
        {
            extDdi->LookAheadDependency = extOpt2->LookAheadDepth;
        }
    }

    if (extOpt2->LookAheadDS == MFX_LOOKAHEAD_DS_UNKNOWN) // default: use LA 2X for TU3-7 and LA 1X for TU1-2
    {
        if (par.mfx.TargetUsage > 2 ||
            (par.mfx.FrameInfo.Width >= 1920 && (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE)) || // customer's issue
            (par.mfx.FrameInfo.Width > 3000) )
            extOpt2->LookAheadDS = MFX_LOOKAHEAD_DS_2x;
        else
            extOpt2->LookAheadDS = MFX_LOOKAHEAD_DS_OFF;
    }

    if ((extOpt2->LookAheadDS != MFX_LOOKAHEAD_DS_OFF) &&
        (extOpt2->LookAheadDS != MFX_LOOKAHEAD_DS_2x) &&
        (extOpt2->LookAheadDS != MFX_LOOKAHEAD_DS_4x))
        extOpt2->LookAheadDS = MFX_LOOKAHEAD_DS_OFF;

    if (extDdi->QpUpdateRange == 0)
        extDdi->QpUpdateRange = 10;

    if (extDdi->RegressionWindow == 0)
        extDdi->RegressionWindow = 20;

    if (extOpt2->RepeatPPS == MFX_CODINGOPTION_UNKNOWN)
        extOpt2->RepeatPPS = MFX_CODINGOPTION_ON;

    mfxExtBRC const & extBRC = GetExtBufferRef(par);

    if (extOpt3->PRefType == MFX_P_REF_DEFAULT)
    {
        if (par.mfx.GopRefDist == 1 &&
            par.calcParam.numTemporalLayer == 0 &&
            extDdi->NumActiveRefP != 1 &&
            (par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) &&
            ((IsExtBrcSceneChangeSupported(par, platform) && !extBRC.pthis)
#if defined(MFX_ENABLE_ENCTOOLS_LPLA)
             || IsLpLookaheadSupported(extOpt3->ScenarioInfo, extOpt2->LookAheadDepth, par.mfx.RateControlMethod)
#endif
            ))
            extOpt3->PRefType = MFX_P_REF_PYRAMID;
        else if (par.mfx.GopRefDist == 1)
            extOpt3->PRefType = MFX_P_REF_SIMPLE;
    }

    if (!extOpt3->EnableQPOffset)
    {
        mfxU16 minQP = 1;
        mfxU16 maxQP = 51;
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP
            && (extOpt2->BRefType == MFX_B_REF_PYRAMID || extOpt3->PRefType == MFX_P_REF_PYRAMID))
        {
            extOpt3->EnableQPOffset = MFX_CODINGOPTION_ON;
            mfxI16 QPX = (par.mfx.GopRefDist == 1) ? par.mfx.QPP : par.mfx.QPB;

            for (mfxI16 i = 0; i < 8; i++)
                extOpt3->QPOffset[i] = mfx::clamp<mfxI16>(i + (par.mfx.GopRefDist > 1), (mfxI16)minQP - QPX, (mfxI16)maxQP - QPX);
        }
        else
            extOpt3->EnableQPOffset = MFX_CODINGOPTION_OFF;
    }

    if (IsOff(extOpt3->EnableQPOffset))
        Zero(extOpt3->QPOffset);

    if (extOpt2->MinQPI && !extOpt2->MaxQPI)
        extOpt2->MaxQPI = 51;
    if (extOpt2->MinQPP && !extOpt2->MaxQPP)
        extOpt2->MaxQPP = 51;
    if (extOpt2->MinQPB && !extOpt2->MaxQPB)
        extOpt2->MaxQPB = 51;

    extDdi->MaxMVs = 32;
    extDdi->SkipCheck = 1;
    extDdi->DirectCheck = 1;
    extDdi->BiDirSearch = 1;
    extDdi->FieldPrediction = fieldCodingPossible;
    extDdi->MVPrediction = 1;

    if (extDdi->StrengthN == 0)
        extDdi->StrengthN = 220;

#if defined(MFX_ENABLE_PARTIAL_BITSTREAM_OUTPUT)
    mfxExtPartialBitstreamParam const * extPBP = GetExtBuffer(par);
    // Override the BRC precision to 1 (two pass BRC in case of IPCM) when PartialOuput is requested
    if (extPBP->Granularity != MFX_PARTIAL_BITSTREAM_NONE)
        extDdi->BRCPrecision = 1;
#endif

    if (par.mfx.NumRefFrame == 0)
    {
        mfxU16 const nrfAdapt           =
            (IsOn(extOpt3->ExtBrcAdaptiveLTR)
#if defined(MFX_ENABLE_ENCTOOLS)
                || (!IsOff(extConfig->AdaptiveLTR))
#endif
            ) ? 2 : 0;
        mfxU16 const nrfMin             = (par.mfx.GopRefDist > 1 ? 2 : 1) + nrfAdapt;
        mfxU16 const nrfDefault         = std::max<mfxU16>(nrfMin, GetDefaultNumRefFrames(par.mfx.TargetUsage, platform) + nrfAdapt);
        mfxU16 const nrfMaxByCaps       = mfx::clamp<mfxU16>(hwCaps.ddi_caps.MaxNum_Reference, 1, 8) * 2;
        mfxU16 const nrfMaxByLevel      = GetMaxNumRefFrame(par);
        mfxU16 const nrfMinForPyramid   = GetMinNumRefFrameForPyramid(par) + nrfAdapt;
        mfxU16 const nrfMinForTemporal  = mfxU16(nrfMin + par.calcParam.numTemporalLayer - 1);
        mfxU16 const nrfMinForInterlace = 2; // HSW and IVB don't support 1 reference frame for interlace

        if (par.calcParam.numTemporalLayer > 1)
            par.mfx.NumRefFrame = nrfMinForTemporal;
        else if (extOpt2->BRefType != MFX_B_REF_OFF)
            par.mfx.NumRefFrame = nrfMinForPyramid;
        else if (extOpt2->IntRefType)
            par.mfx.NumRefFrame = 1;
        else if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0)
            par.mfx.NumRefFrame = std::min({std::max(nrfDefault, nrfMinForInterlace), nrfMaxByLevel, nrfMaxByCaps});
        else if (extOpt3->PRefType == MFX_P_REF_PYRAMID)
            par.mfx.NumRefFrame = std::max<mfxU16>((mfxU16)par.calcParam.PPyrInterval, nrfDefault);
        else
            par.mfx.NumRefFrame = std::min({nrfDefault, nrfMaxByLevel, nrfMaxByCaps});

        if (IsOn(par.mfx.LowPower) && (extOpt3->PRefType != MFX_P_REF_PYRAMID))
        {
            par.mfx.NumRefFrame = std::min<mfxU16>(hwCaps.ddi_caps.MaxNum_Reference, par.mfx.NumRefFrame);
        }
        par.calcParam.PPyrInterval = std::min<mfxU32>(par.calcParam.PPyrInterval, par.mfx.NumRefFrame);
    }

    if (extOpt->IntraPredBlockSize == MFX_BLOCKSIZE_UNKNOWN)
        extOpt->IntraPredBlockSize = GetDefaultIntraPredBlockSize(par);

    if (par.mfx.CodecProfile == MFX_PROFILE_UNKNOWN)
    {
        par.mfx.CodecProfile = MFX_PROFILE_AVC_BASELINE;

        if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_TFF) ||
            (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF))
            par.mfx.CodecProfile = MFX_PROFILE_AVC_MAIN;

        if (par.mfx.GopRefDist > 1)
            par.mfx.CodecProfile = MFX_PROFILE_AVC_MAIN;

        if (IsOff(extOpt->CAVLC))
            par.mfx.CodecProfile = MFX_PROFILE_AVC_MAIN;

        if (extOpt->IntraPredBlockSize >= MFX_BLOCKSIZE_MIN_8X8)
            par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
    }

    if (par.calcParam.cqpHrdMode == 0)
    {
        if (par.calcParam.maxKbps == 0)
        {
            if (par.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_WIDI_VBR ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_VCM ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD)
            {
                mfxU32 maxBps = par.calcParam.targetKbps * MAX_BITRATE_RATIO;
                if (IsOn(extOpt->NalHrdConformance) ||
                    IsOn(extOpt->VuiVclHrdParameters))
                    maxBps = std::min(maxBps, GetMaxBitrate(par));

                par.calcParam.maxKbps = maxBps / 1000;
                assert(par.calcParam.maxKbps >= par.calcParam.targetKbps);
            }
            else if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
            {
                par.calcParam.maxKbps = par.calcParam.targetKbps;
            }
        }

        if (par.calcParam.bufferSizeInKB == 0)
        {
            if (bRateControlLA(par.mfx.RateControlMethod) && (par.mfx.RateControlMethod != MFX_RATECONTROL_LA_HRD))
            {
                par.calcParam.bufferSizeInKB = GetMaxCodedFrameSizeInKB(par);
            }
            else
            {
                mfxU32 bufferSizeInBits = std::min(
                    GetMaxBufferSize(par),                           // limit by spec
                    par.calcParam.maxKbps * DEFAULT_CPB_IN_SECONDS); // limit by common sense

                par.calcParam.bufferSizeInKB = !IsHRDBasedBRCMethod(par.mfx.RateControlMethod)
                    ? GetMaxCodedFrameSizeInKB(par)
                    : bufferSizeInBits / 8000;
            }
            par.calcParam.bufferSizeInKB = std::max(par.calcParam.bufferSizeInKB, par.calcParam.initialDelayInKB);

        }

        if (par.calcParam.initialDelayInKB == 0 && IsHRDBasedBRCMethod(par.mfx.RateControlMethod))
        {
            par.calcParam.initialDelayInKB = (par.mfx.RateControlMethod == MFX_RATECONTROL_VBR && isSWBRC(par)) ?
                3 * par.calcParam.bufferSizeInKB / 4 :
                par.calcParam.bufferSizeInKB / 2;
        }

        // Check default value of BufferSizeInKB at extremely low bitrate to ensure there is enough space to write bitstream
        if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_ICQ &&
            !isSWBRC(par) &&
            par.mfx.BufferSizeInKB == 0 &&
            par.mfx.FrameInfo.Width != 0 &&
            par.mfx.FrameInfo.Height != 0 &&
            par.mfx.FrameInfo.FrameRateExtN != 0 &&
            par.mfx.FrameInfo.FrameRateExtD != 0)
        {
            mfxF64 rawDataBitrate = 12.0 * par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height *
                par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD;
            mfxU32 minBufferSizeInKB = mfxU32(std::min<mfxF64>(0xffffffff, rawDataBitrate / 8 / 1000.0 / 1400.0));
            if (par.calcParam.bufferSizeInKB < minBufferSizeInKB) {
                par.calcParam.bufferSizeInKB = minBufferSizeInKB;
                par.calcParam.initialDelayInKB = minBufferSizeInKB / 2;
            }
        }
    }

    if (par.mfx.CodecLevel == MFX_LEVEL_UNKNOWN)
        par.mfx.CodecLevel = GetMinLevelForAllParameters(par);

    if (extOpt2->BufferingPeriodSEI == MFX_BPSEI_DEFAULT
        && IsOn(extOpt->RecoveryPointSEI)
        && extOpt2->IntRefType == 0
        )
    {
        extOpt2->BufferingPeriodSEI = MFX_BPSEI_IFRAME;
    }

    SetDefaultOff(extOpt2->DisableVUI);
    SetDefaultOn(extOpt3->AspectRatioInfoPresent);
    SetDefaultOff(extOpt3->OverscanInfoPresent);
    SetDefaultOn(extOpt3->TimingInfoPresent);
    SetDefaultOn(extOpt2->FixedFrameRate);
    SetDefaultOff(extOpt3->LowDelayHrd);
    SetDefaultOn(extOpt3->BitstreamRestriction);
    SetDefaultOff(extOpt->RecoveryPointSEI);
    SetDefaultOff(extOpt3->DirectBiasAdjustment);
    SetDefaultOff(extOpt3->GlobalMotionBiasAdjustment);
    SetDefaultOff(extOpt3->LowDelayBRC);

#ifdef MFX_ENABLE_H264_REPARTITION_CHECK
    if (extOpt3->RepartitionCheckEnable == MFX_CODINGOPTION_UNKNOWN)
        extOpt3->RepartitionCheckEnable = MFX_CODINGOPTION_ADAPTIVE;
#endif // MFX_ENABLE_H264_REPARTITION_CHECK

    if (extOpt3->ExtBrcAdaptiveLTR == MFX_CODINGOPTION_UNKNOWN
#if defined(MFX_ENABLE_ENCTOOLS)
        && IsOff(extConfig->AdaptiveLTR)
#endif
        )
    {
        extOpt3->ExtBrcAdaptiveLTR = MFX_CODINGOPTION_OFF;
        #ifndef MFX_AUTOLTR_FEATURE_DISABLE
        // remove check when sample extbrc is same as implicit extbrc
        // currently added for no behaviour change in sample extbrc
        if (IsExtBrcSceneChangeSupported(par, platform) && !extBRC.pthis)
        {
            extOpt3->ExtBrcAdaptiveLTR = MFX_CODINGOPTION_ON;
            // make sure to call CheckVideoParamQueryLike
            // or add additional conditions above (num ref & num active)
        }
        #endif
    }

    if (extOpt2->AdaptiveI == MFX_CODINGOPTION_UNKNOWN)
    {
        if ((IsExtBrcSceneChangeSupported(par, platform) && !extBRC.pthis)
#if defined(MFX_ENABLE_ENCTOOLS)
           || (!IsOff(extConfig->AdaptiveI)
#if defined(MFX_ENABLE_ENCTOOLS_LPLA)
               && (!IsLpLookaheadSupported(extOpt3->ScenarioInfo, extOpt2->LookAheadDepth, par.mfx.RateControlMethod)
                   || IsOn(extConfig->AdaptiveI))
#endif
               )
#endif
            )
            extOpt2->AdaptiveI = MFX_CODINGOPTION_ON;
        else
            extOpt2->AdaptiveI = MFX_CODINGOPTION_OFF;
    }

    if (extOpt2->AdaptiveB == MFX_CODINGOPTION_UNKNOWN)
    {
        if ((IsExtBrcSceneChangeSupported(par, platform) && !extBRC.pthis)
#if defined(MFX_ENABLE_ENCTOOLS)
            || (!IsOff(extConfig->AdaptiveB)
#if defined(MFX_ENABLE_ENCTOOLS_LPLA)
                && (!IsLpLookaheadSupported(extOpt3->ScenarioInfo, extOpt2->LookAheadDepth, par.mfx.RateControlMethod)
                    || IsOn(extConfig->AdaptiveB))
#endif
                )
#endif
            )
            extOpt2->AdaptiveB = MFX_CODINGOPTION_ON;
        else
            extOpt2->AdaptiveB = MFX_CODINGOPTION_OFF;
    }

#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
    if (isAdaptiveCQMSupported(extOpt3->ScenarioInfo, IsOn(par.mfx.LowPower)))
    {
        if (extOpt3->AdaptiveCQM == MFX_CODINGOPTION_UNKNOWN)
            extOpt3->AdaptiveCQM = MFX_CODINGOPTION_ON;
    }
    else
        extOpt3->AdaptiveCQM = MFX_CODINGOPTION_OFF;
#endif

#if defined(MFX_ENABLE_ENCTOOLS_LPLA)
    // Closed GOP for GS/LPLA unless IdrInterval is set to max
    // Open GOP with arbitrary IdrInterval if GOP is strict (GopOptFlag == MFX_GOP_STRICT)
    if (extOpt3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING && IsOn(par.mfx.LowPower) && extOpt2->LookAheadDepth > 0
        && par.mfx.GopOptFlag == 0 && par.mfx.IdrInterval != USHRT_MAX)
    {
        par.mfx.GopOptFlag = MFX_GOP_CLOSED;
    }
#endif

    CheckVideoParamQueryLike(par, hwCaps, platform, vaType, config);

    if (extOpt3->NumSliceI == 0 && extOpt3->NumSliceP == 0 && extOpt3->NumSliceB == 0)
        extOpt3->NumSliceI = extOpt3->NumSliceP = extOpt3->NumSliceB = par.mfx.NumSlice;

    if (extOpt2->MBBRC == MFX_CODINGOPTION_UNKNOWN)
    {
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
        {
            // for ICQ BRC mode MBBRC is ignored by driver and always treated as ON
            // need to change extOpt2->MBBRC respectively to notify application about it
            extOpt2->MBBRC = MFX_CODINGOPTION_ON;
        }
        else
        {
            extOpt2->MBBRC = MFX_CODINGOPTION_OFF;

            // turn on MBBRC by default for enctools if
            //  - entools is ON
            //  - look-ahead
            //  - hw supports MBdata
            //  - VBR or CBR
            const mfxExtCodingOption3* pCO3 = (mfxExtCodingOption3*)mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_CODING_OPTION3);
            bool bNonGS = !(pCO3 && pCO3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING);
            if(IsEnctoolsLABRC(par) && hwCaps.ddi_caps.MbQpDataSupport && bNonGS)
                extOpt2->MBBRC = MFX_CODINGOPTION_ON;
        }
    }

    if (extOpt2->IntRefType && extOpt2->IntRefCycleSize == 0)
    {
        if (extOpt2->IntRefType == MFX_REFRESH_SLICE)
        {
            extOpt2->IntRefCycleSize = extOpt3->NumSliceP;
        }
        else
        {
            // set intra refresh cycle to 1 sec by default
            extOpt2->IntRefCycleSize =
                (mfxU16)((par.mfx.FrameInfo.FrameRateExtN + par.mfx.FrameInfo.FrameRateExtD - 1) / par.mfx.FrameInfo.FrameRateExtD);
        }
    }

    if (extOpt3->EnableMBQP == MFX_CODINGOPTION_UNKNOWN)
        extOpt3->EnableMBQP = MFX_CODINGOPTION_OFF;

    if (extOpt3->MBDisableSkipMap == MFX_CODINGOPTION_UNKNOWN)
        extOpt3->MBDisableSkipMap = MFX_CODINGOPTION_OFF;

    if (extOpt3->EnableMBForceIntra == MFX_CODINGOPTION_UNKNOWN)
        extOpt3->EnableMBForceIntra = MFX_CODINGOPTION_OFF;

    if (IsMvcProfile(par.mfx.CodecProfile))
    {
        CheckVideoParamMvcQueryLike(par);

        if (par.calcParam.cqpHrdMode == 0)
        {
            if (par.calcParam.mvcPerViewPar.maxKbps == 0)
            {
                if (par.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
                    par.mfx.RateControlMethod == MFX_RATECONTROL_WIDI_VBR ||
                    par.mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD)
                {
                    mfxU32 maxBps = par.calcParam.mvcPerViewPar.targetKbps * MAX_BITRATE_RATIO;
                    if (IsOn(extOpt->NalHrdConformance) ||
                        IsOn(extOpt->VuiVclHrdParameters))
                        maxBps = std::min(maxBps, GetMaxPerViewBitrate(par));

                    par.calcParam.mvcPerViewPar.maxKbps = maxBps / 1000;
                    assert(par.calcParam.mvcPerViewPar.maxKbps >= par.calcParam.mvcPerViewPar.targetKbps);
                }
                else if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
                {
                    par.calcParam.mvcPerViewPar.maxKbps = par.calcParam.mvcPerViewPar.targetKbps;
                }
            }

            if (par.calcParam.mvcPerViewPar.bufferSizeInKB == 0)
            {
                mfxU32 bufferSizeInBits = std::min(
                    GetMaxPerViewBufferSize(par),                                  // limit by spec
                    par.calcParam.mvcPerViewPar.maxKbps * DEFAULT_CPB_IN_SECONDS); // limit by common sense

                par.calcParam.mvcPerViewPar.bufferSizeInKB = !IsHRDBasedBRCMethod(par.mfx.RateControlMethod)
                    ? GetMaxCodedFrameSizeInKB(par)
                    : bufferSizeInBits / 8000;
                par.calcParam.mvcPerViewPar.bufferSizeInKB = std::max(par.calcParam.mvcPerViewPar.bufferSizeInKB,
                    2 * par.calcParam.mvcPerViewPar.initialDelayInKB);
            }
            // HRD buffer size can be different for the same level for AVC and MVC profiles.
            // So in case of MVC we need to copy MVC-specific buffer size to calcParam.bufferSizeInKB to assure that application will get enough size for bitstream buffer allocation
            par.calcParam.bufferSizeInKB = par.calcParam.mvcPerViewPar.bufferSizeInKB;
            par.calcParam.initialDelayInKB = std::min(par.calcParam.initialDelayInKB, par.calcParam.bufferSizeInKB);

            if (par.calcParam.mvcPerViewPar.initialDelayInKB == 0 && IsHRDBasedBRCMethod(par.mfx.RateControlMethod))
            {
                par.calcParam.mvcPerViewPar.initialDelayInKB = par.calcParam.mvcPerViewPar.bufferSizeInKB / 2;
            }
        }
    }

    if (extOpt->MaxDecFrameBuffering == 0)
    {
        extOpt->MaxDecFrameBuffering = par.mfx.NumRefFrame;
    }

    if (extDdi->DisablePSubMBPartition == MFX_CODINGOPTION_UNKNOWN)
    {
        extDdi->DisablePSubMBPartition = MFX_CODINGOPTION_OFF;

        // check restriction A.3.3 (f)
        if (IsAvcBaseProfile(par.mfx.CodecProfile) && par.mfx.CodecLevel <= MFX_LEVEL_AVC_3)
            extDdi->DisablePSubMBPartition = MFX_CODINGOPTION_ON;
    }

    if (extDdi->DisableBSubMBPartition == MFX_CODINGOPTION_UNKNOWN)
    {
        extDdi->DisableBSubMBPartition = MFX_CODINGOPTION_OFF;

        // check restriction A.3.3 (f)
        if (IsAvcBaseProfile(par.mfx.CodecProfile) && par.mfx.CodecLevel <= MFX_LEVEL_AVC_3)
            extDdi->DisableBSubMBPartition = MFX_CODINGOPTION_ON;
    }

    if (extDdi->WeightedBiPredIdc == 0)
    {
        extDdi->WeightedBiPredIdc = (par.mfx.GopRefDist == 3 && !IsMvcProfile(par.mfx.CodecProfile) && extOpt2->BRefType == MFX_B_REF_OFF)
            ? 2  // explicit weighted biprediction (when 2 B frames in a row)
            : 0; // no weighted biprediction
    }

    if (extDdi->CabacInitIdcPlus1 == 0)
    {
        extDdi->CabacInitIdcPlus1 = GetCabacInitIdc(par.mfx.TargetUsage) + 1;
    }

    bool IsEnabledSwBrc = false;
#if defined(MFX_ENABLE_EXT_BRC)
    IsEnabledSwBrc = isSWBRC(par);
#else
    IsEnabledSwBrc = bRateControlLA(par.mfx.RateControlMethod);
#endif

    if (par.calcParam.TCBRCTargetFrameSize == 0)
    {
        par.calcParam.TCBRCTargetFrameSize = IsTCBRC(par, hwCaps) ? GetAvgFrameSizeInBytes(par, false) : 0;
    }

    if ((hwCaps.ddi_caps.UserMaxFrameSizeSupport != 0 || IsEnabledSwBrc) &&
        (par.mfx.RateControlMethod != MFX_RATECONTROL_CBR &&
         par.mfx.RateControlMethod != MFX_RATECONTROL_CQP) &&
        extOpt2->MaxFrameSize == 0)
    {
        extOpt2->MaxFrameSize = std::max(extOpt3->MaxFrameSizeI, extOpt3->MaxFrameSizeP);
        if (!extOpt2->MaxFrameSize)
        {
            extOpt2->MaxFrameSize = (IsOn(extOpt3->LowDelayBRC)) ?
                GetAvgFrameSizeInBytes(par, true):
                std::min(GetMaxFrameSize(par), GetFirstMaxFrameSize(par));
        }
    }

    if (extOpt3->AdaptiveMaxFrameSize == MFX_CODINGOPTION_UNKNOWN )
        extOpt3->AdaptiveMaxFrameSize = 
            (IsOn(par.mfx.LowPower) && hwCaps.AdaptiveMaxFrameSizeSupport)
            ? mfxU16(MFX_CODINGOPTION_ON) : mfxU16(MFX_CODINGOPTION_OFF);

    if (extOpt3->BRCPanicMode == MFX_CODINGOPTION_UNKNOWN)
        extOpt3->BRCPanicMode =
            extOpt->NalHrdConformance == MFX_CODINGOPTION_ON ||
            bRateControlLA(par.mfx.RateControlMethod) ||
            par.mfx.RateControlMethod == MFX_RATECONTROL_CQP ||
            vaType != MFX_HW_VAAPI ||
            ((extOpt2->MaxFrameSize != 0 || extOpt3->WinBRCSize != 0) &&
            isSWBRC(par))
        ? mfxU16(MFX_CODINGOPTION_ON)
        : mfxU16(MFX_CODINGOPTION_OFF);

    par.ApplyDefaultsToMvcSeqDesc();

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    bool svcSupportedByHw = (hwCaps.ddi_caps.MaxNum_QualityLayer || hwCaps.ddi_caps.MaxNum_DependencyLayer);
    for (mfxU32 i = 0; i < par.calcParam.numDependencyLayer; i++)
    {
        mfxU32 did = par.calcParam.did[i];

        if (extSvc->DependencyLayer[did].CropH == 0)
            extSvc->DependencyLayer[did].CropH = extSvc->DependencyLayer[did].Height - extSvc->DependencyLayer[did].CropY;
        if (extSvc->DependencyLayer[did].CropW == 0)
            extSvc->DependencyLayer[did].CropW = extSvc->DependencyLayer[did].Width  - extSvc->DependencyLayer[did].CropX;

        if (extSvc->DependencyLayer[did].ScanIdxPresent == 0)
            extSvc->DependencyLayer[did].ScanIdxPresent = MFX_CODINGOPTION_OFF;

        if (IsOff(extSvc->DependencyLayer[did].ScanIdxPresent))
        {
            for (mfxU32 qid = 0; qid < extSvc->DependencyLayer[did].QualityNum; qid++)
            {
                extSvc->DependencyLayer[did].QualityLayer[qid].ScanIdxStart = 0;
                extSvc->DependencyLayer[did].QualityLayer[qid].ScanIdxEnd   = 15;
            }
        }

        if (extSvc->DependencyLayer[did].BasemodePred == 0)
            extSvc->DependencyLayer[did].BasemodePred = svcSupportedByHw
                ? mfxU16(MFX_CODINGOPTION_ADAPTIVE)
                : mfxU16(MFX_CODINGOPTION_OFF);
        if (extSvc->DependencyLayer[did].MotionPred == 0)
            extSvc->DependencyLayer[did].MotionPred = svcSupportedByHw
                ? mfxU16(MFX_CODINGOPTION_ADAPTIVE)
                : mfxU16(MFX_CODINGOPTION_OFF);
        if (extSvc->DependencyLayer[did].ResidualPred == 0)
            extSvc->DependencyLayer[did].ResidualPred = svcSupportedByHw
                ? mfxU16(MFX_CODINGOPTION_ADAPTIVE)
                : mfxU16(MFX_CODINGOPTION_OFF);
    }

    if (!IsSvcProfile(par.mfx.CodecProfile))
    {
        extSvc->TemporalScale[0]                                = 1;
        extSvc->DependencyLayer[0].Active                       = 1;
        extSvc->DependencyLayer[0].Width                        = par.mfx.FrameInfo.Width;
        extSvc->DependencyLayer[0].Height                       = par.mfx.FrameInfo.Height;
        extSvc->DependencyLayer[0].CropX                        = par.mfx.FrameInfo.CropX;
        extSvc->DependencyLayer[0].CropY                        = par.mfx.FrameInfo.CropY;
        extSvc->DependencyLayer[0].CropW                        = par.mfx.FrameInfo.CropW;
        extSvc->DependencyLayer[0].CropH                        = par.mfx.FrameInfo.CropH;
        extSvc->DependencyLayer[0].GopPicSize                   = par.mfx.GopPicSize;
        extSvc->DependencyLayer[0].GopRefDist                   = par.mfx.GopRefDist;
        extSvc->DependencyLayer[0].GopOptFlag                   = par.mfx.GopOptFlag;
        extSvc->DependencyLayer[0].IdrInterval                  = par.mfx.IdrInterval;
        extSvc->DependencyLayer[0].BasemodePred                 = MFX_CODINGOPTION_OFF;
        extSvc->DependencyLayer[0].MotionPred                   = MFX_CODINGOPTION_OFF;
        extSvc->DependencyLayer[0].ResidualPred                 = MFX_CODINGOPTION_OFF;
        extSvc->DependencyLayer[0].ScanIdxPresent               = MFX_CODINGOPTION_OFF;
        extSvc->DependencyLayer[0].TemporalNum                  = 1;
        extSvc->DependencyLayer[0].TemporalId[0]                = 0;
        extSvc->DependencyLayer[0].QualityNum                   = 1;

        extRc->RateControlMethod = par.mfx.RateControlMethod;
        extRc->NumLayers         = 1;
        switch (extRc->RateControlMethod)
        {
        case MFX_RATECONTROL_CBR:
        case MFX_RATECONTROL_VBR:
        case MFX_RATECONTROL_LA:
        case MFX_RATECONTROL_VCM:
        case MFX_RATECONTROL_QVBR:
        case MFX_RATECONTROL_LA_HRD:

            extRc->Layer[0].CbrVbr.TargetKbps       = par.mfx.TargetKbps;
            extRc->Layer[0].CbrVbr.InitialDelayInKB = par.mfx.InitialDelayInKB;
            extRc->Layer[0].CbrVbr.BufferSizeInKB   = par.mfx.BufferSizeInKB;
            extRc->Layer[0].CbrVbr.MaxKbps          = par.mfx.MaxKbps;
            break;
        case MFX_RATECONTROL_CQP:
            extRc->Layer[0].Cqp.QPI = par.mfx.QPI;
            extRc->Layer[0].Cqp.QPP = par.mfx.QPP;
            extRc->Layer[0].Cqp.QPB = par.mfx.QPB;
            break;
        case MFX_RATECONTROL_AVBR:
            extRc->Layer[0].Avbr.TargetKbps  = par.mfx.TargetKbps;
            extRc->Layer[0].Avbr.Convergence = par.mfx.Convergence;
            extRc->Layer[0].Avbr.Accuracy    = par.mfx.Accuracy;
            break;
        case MFX_RATECONTROL_ICQ:
        case MFX_RATECONTROL_LA_ICQ:
            break;
        default:
            assert(0);
            break;
        }
    }
#endif // #ifdef MFX_ENABLE_SVC_VIDEO_ENCODE

    if (extBits->SPSBuffer == 0)
    {
        mfxFrameInfo const & fi = par.mfx.FrameInfo;


        extSps->nalRefIdc                       = par.calcParam.tempScalabilityMode ? 3 : 1;
        extSps->nalUnitType                     = 7;
        extSps->profileIdc                      = mfxU8(par.mfx.CodecProfile & MASK_PROFILE_IDC);
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
        if (IsSvcProfile(extSps->profileIdc))
            extSps->profileIdc                  = MFX_PROFILE_AVC_HIGH;
#endif
        extSps->constraints.set0                = mfxU8(!!(par.mfx.CodecProfile & MASK_CONSTRAINT_SET0_FLAG));
        extSps->constraints.set1                = mfxU8(!!(par.mfx.CodecProfile & MASK_CONSTRAINT_SET1_FLAG));
        if (par.calcParam.numTemporalLayer > 0 && IsAvcBaseProfile(par.mfx.CodecProfile))
            extSps->constraints.set1            = 1; // tempScalabilityMode requires constraint base profile
        extSps->constraints.set2                = mfxU8(!!(par.mfx.CodecProfile & MASK_CONSTRAINT_SET2_FLAG));
        extSps->constraints.set3                = mfxU8(!!(par.mfx.CodecProfile & MASK_CONSTRAINT_SET3_FLAG));
        extSps->constraints.set4                = mfxU8(!!(par.mfx.CodecProfile & MASK_CONSTRAINT_SET4_FLAG));
        extSps->constraints.set5                = mfxU8(!!(par.mfx.CodecProfile & MASK_CONSTRAINT_SET5_FLAG));
        extSps->constraints.set6                = 0;
        extSps->constraints.set7                = 0;
        extSps->levelIdc                        = IsMvcProfile(par.mfx.CodecProfile) ? mfxU8(par.calcParam.mvcPerViewPar.codecLevel) : mfxU8(par.mfx.CodecLevel);
        extSps->seqParameterSetId               = mfxU8(extBits->SPSId);
        extSps->chromaFormatIdc                 = mfxU8(MFX_CHROMAFORMAT_YUV420);//in case if RGB or YUY2 passed we still need encode in 420
        extSps->separateColourPlaneFlag         = 0;
        extSps->bitDepthLumaMinus8              = 0;
        extSps->bitDepthChromaMinus8            = 0;
        extSps->qpprimeYZeroTransformBypassFlag = 0;
        extSps->seqScalingMatrixPresentFlag     = 0;
        extSps->log2MaxFrameNumMinus4           = 4;//GetDefaultLog2MaxFrameNumMinux4(par);
        extSps->picOrderCntType                 = (extOpt2->SkipFrame == MFX_SKIPFRAME_INSERT_DUMMY) ? 0  : GetDefaultPicOrderCount(par);
        extSps->log2MaxPicOrderCntLsbMinus4     = extOpt2->SkipFrame ? ((mfxU8)CeilLog2(2 * par.mfx.GopPicSize) + 1 - 4) : GetDefaultLog2MaxPicOrdCntMinus4(par);
        extSps->log2MaxPicOrderCntLsbMinus4     = std::min<mfxU8>(12, extSps->log2MaxPicOrderCntLsbMinus4);
        extSps->deltaPicOrderAlwaysZeroFlag     = 1;
        extSps->maxNumRefFrames                 = mfxU8(par.mfx.NumRefFrame);
        extSps->gapsInFrameNumValueAllowedFlag  = par.calcParam.numTemporalLayer > 1
            || par.calcParam.tempScalabilityMode; // for tempScalabilityMode change of temporal structure shouldn't change SPS.
        extSps->frameMbsOnlyFlag                = (fi.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 0;
        extSps->picWidthInMbsMinus1             = mfx::CeilDiv<mfxU16>(fi.Width,  16) - 1;
        extSps->picHeightInMapUnitsMinus1       = mfx::CeilDiv<mfxU16>(fi.Height, 16) / (2 - extSps->frameMbsOnlyFlag) - 1;
        extSps->direct8x8InferenceFlag          = 1;

        //Fix cropping for ARGB and YUY2 formats, need to redesign in case real 444 or 422 support in AVC.
        mfxU16 croma_format = fi.ChromaFormat;
        if(croma_format == MFX_CHROMAFORMAT_YUV444 || croma_format == MFX_CHROMAFORMAT_YUV422)
            croma_format = MFX_CHROMAFORMAT_YUV420;
        mfxU16 cropUnitX = CROP_UNIT_X[croma_format];
        mfxU16 cropUnitY = CROP_UNIT_Y[croma_format] * (2 - extSps->frameMbsOnlyFlag);

        extSps->frameCropLeftOffset   = (fi.CropX / cropUnitX);
        extSps->frameCropRightOffset  = (mfx::align2_value(fi.Width,  16) - fi.CropW - fi.CropX) / cropUnitX;
        extSps->frameCropTopOffset    = (fi.CropY / cropUnitY);
        extSps->frameCropBottomOffset = (mfx::align2_value(fi.Height, 16) - fi.CropH - fi.CropY) / cropUnitY;
        extSps->frameCroppingFlag     =
            extSps->frameCropLeftOffset || extSps->frameCropRightOffset ||
            extSps->frameCropTopOffset  || extSps->frameCropBottomOffset;

        extSps->vuiParametersPresentFlag = !IsOn(extOpt2->DisableVUI);

        if (extSps->vuiParametersPresentFlag)
        {
            AspectRatioConverter arConv(fi.AspectRatioW, fi.AspectRatioH);
            extSps->vui.flags.aspectRatioInfoPresent = (fi.AspectRatioH && fi.AspectRatioW) && !IsOff(extOpt3->AspectRatioInfoPresent);
            extSps->vui.aspectRatioIdc               = arConv.GetSarIdc();
            extSps->vui.sarWidth                     = arConv.GetSarWidth();
            extSps->vui.sarHeight                    = arConv.GetSarHeight();

            extSps->vui.flags.overscanInfoPresent = !IsOff(extOpt3->OverscanInfoPresent);
            extSps->vui.flags.overscanAppropriate = !IsOff(extOpt3->OverscanAppropriate);

            extSps->vui.videoFormat                    = mfxU8(extVsi->VideoFormat);
            extSps->vui.flags.videoFullRange           = extVsi->VideoFullRange;
            extSps->vui.flags.colourDescriptionPresent = extVsi->ColourDescriptionPresent;
            extSps->vui.colourPrimaries                = mfxU8(extVsi->ColourPrimaries);
            extSps->vui.transferCharacteristics        = mfxU8(extVsi->TransferCharacteristics);
            extSps->vui.matrixCoefficients             = mfxU8(extVsi->MatrixCoefficients);
            extSps->vui.flags.videoSignalTypePresent   =
                extSps->vui.videoFormat                    != 5 ||
                extSps->vui.flags.videoFullRange           != 0 ||
                extSps->vui.flags.colourDescriptionPresent != 0;

            extSps->vui.flags.chromaLocInfoPresent     = !!extCli->ChromaLocInfoPresentFlag;
            extSps->vui.chromaSampleLocTypeTopField    = mfxU8(extCli->ChromaSampleLocTypeTopField);
            extSps->vui.chromaSampleLocTypeBottomField = mfxU8(extCli->ChromaSampleLocTypeBottomField);

            extSps->vui.flags.timingInfoPresent = !IsOff(extOpt3->TimingInfoPresent);
            extSps->vui.numUnitsInTick          = fi.FrameRateExtD;
            extSps->vui.timeScale               = fi.FrameRateExtN * 2;
            extSps->vui.flags.fixedFrameRate    = !IsOff(extOpt2->FixedFrameRate);


            extSps->vui.flags.nalHrdParametersPresent = IsOn(extOpt->VuiNalHrdParameters);
            extSps->vui.flags.vclHrdParametersPresent = IsOn(extOpt->VuiVclHrdParameters);

            extSps->vui.nalHrdParameters.cpbCntMinus1 = 0;
            extSps->vui.nalHrdParameters.bitRateScale = SCALE_FROM_DRIVER;
            extSps->vui.nalHrdParameters.cpbSizeScale = 2;

            if (par.calcParam.cqpHrdMode)
            {
                extSps->vui.nalHrdParameters.bitRateValueMinus1[0] = GetMaxBitrateValue(par.calcParam.decorativeHrdParam.maxKbps) - 1;
                extSps->vui.nalHrdParameters.cpbSizeValueMinus1[0] = GetCpbSizeValue(par.calcParam.decorativeHrdParam.bufferSizeInKB, 2) - 1;
                extSps->vui.nalHrdParameters.cbrFlag[0] = par.calcParam.cqpHrdMode == 1 ? 1 : 0;
            }
            else
            {
                // extSps isn't syncronized with m_video after next assignment for ViewOutput mode. Need to Init ExtSps HRD for MVC keeping them syncronized
                if (IsMvcProfile(par.mfx.CodecProfile))
                {
                    extSps->vui.nalHrdParameters.bitRateValueMinus1[0] = GetMaxBitrateValue(par.calcParam.mvcPerViewPar.maxKbps) - 1;
                    extSps->vui.nalHrdParameters.cpbSizeValueMinus1[0] = GetCpbSizeValue(par.calcParam.mvcPerViewPar.bufferSizeInKB, 2) - 1;
                }
                else
                {
                    extSps->vui.nalHrdParameters.bitRateValueMinus1[0] = GetMaxBitrateValue(par.calcParam.maxKbps) - 1;
                    extSps->vui.nalHrdParameters.cpbSizeValueMinus1[0] = GetCpbSizeValue(par.calcParam.bufferSizeInKB, 2) - 1;
                }
            extSps->vui.nalHrdParameters.cbrFlag[0]                         = (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR);
            }
            extSps->vui.nalHrdParameters.initialCpbRemovalDelayLengthMinus1 = 23;
            extSps->vui.nalHrdParameters.cpbRemovalDelayLengthMinus1        = 23;
            extSps->vui.nalHrdParameters.dpbOutputDelayLengthMinus1         = 23;
            extSps->vui.nalHrdParameters.timeOffsetLength                   = 24;

            extSps->vui.vclHrdParameters.cpbCntMinus1                       = 0;
            extSps->vui.vclHrdParameters.bitRateScale                       = SCALE_FROM_DRIVER;
            extSps->vui.vclHrdParameters.cpbSizeScale                       = 2;
            if (IsMvcProfile(par.mfx.CodecProfile))
            {
                extSps->vui.vclHrdParameters.bitRateValueMinus1[0] = GetMaxBitrateValue(par.calcParam.mvcPerViewPar.maxKbps) - 1;
                extSps->vui.vclHrdParameters.cpbSizeValueMinus1[0] = GetCpbSizeValue(par.calcParam.mvcPerViewPar.bufferSizeInKB, 2) - 1;
            }
            else
            {
                extSps->vui.vclHrdParameters.bitRateValueMinus1[0] = GetMaxBitrateValue(par.calcParam.maxKbps) - 1;
                extSps->vui.vclHrdParameters.cpbSizeValueMinus1[0] = GetCpbSizeValue(par.calcParam.bufferSizeInKB, 2) - 1;
            }
            extSps->vui.vclHrdParameters.cbrFlag[0]                         = (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR);
            extSps->vui.vclHrdParameters.initialCpbRemovalDelayLengthMinus1 = 23;
            extSps->vui.vclHrdParameters.cpbRemovalDelayLengthMinus1        = 23;
            extSps->vui.vclHrdParameters.dpbOutputDelayLengthMinus1         = 23;
            extSps->vui.vclHrdParameters.timeOffsetLength                   = 24;

            extSps->vui.flags.lowDelayHrd                                   = IsOn(extOpt3->LowDelayHrd);;
            extSps->vui.flags.picStructPresent                              = IsOn(extOpt->PicTimingSEI);

            extSps->vui.flags.bitstreamRestriction           = !IsOff(extOpt3->BitstreamRestriction);
            extSps->vui.flags.motionVectorsOverPicBoundaries = !IsOff(extOpt3->MotionVectorsOverPicBoundaries);
            extSps->vui.maxBytesPerPicDenom                  = 2;
            extSps->vui.maxBitsPerMbDenom                    = 1;
            extSps->vui.log2MaxMvLengthHorizontal            = mfxU8(CeilLog2(4 * MAX_H_MV - 1));
            extSps->vui.log2MaxMvLengthVertical              = mfxU8(CeilLog2(4 * GetMaxVmv(par.mfx.CodecLevel) - 1));
            extSps->vui.numReorderFrames                     = GetNumReorderFrames(par);
            extSps->vui.maxDecFrameBuffering                 = mfxU8(extOpt->MaxDecFrameBuffering);

            extSps->vuiParametersPresentFlag =
                extSps->vui.flags.aspectRatioInfoPresent  ||
                extSps->vui.flags.overscanInfoPresent     ||
                extSps->vui.flags.videoSignalTypePresent  ||
                extSps->vui.flags.chromaLocInfoPresent    ||
                extSps->vui.flags.timingInfoPresent       ||
                extSps->vui.flags.nalHrdParametersPresent ||
                extSps->vui.flags.vclHrdParametersPresent ||
                extSps->vui.flags.picStructPresent        ||
                extSps->vui.flags.bitstreamRestriction;
        }

        if (extBits->PPSBufSize == 0)
        {
            extPps->nalRefIdc = par.calcParam.tempScalabilityMode ? 3 : 1;
            extPps->picParameterSetId = mfxU8(extBits->PPSId);
            extPps->seqParameterSetId = mfxU8(extBits->SPSId);
            extPps->entropyCodingModeFlag = IsOff(extOpt->CAVLC);
            extPps->bottomFieldPicOrderInframePresentFlag = 0;
            extPps->numSliceGroupsMinus1 = 0;
            extPps->sliceGroupMapType = 0;
            extPps->numRefIdxL0DefaultActiveMinus1 = 0;
            extPps->numRefIdxL1DefaultActiveMinus1 = 0;
            extPps->weightedPredFlag = mfxU8(extOpt3->WeightedPred == MFX_WEIGHTED_PRED_EXPLICIT);
            extPps->weightedBipredIdc = extOpt3->WeightedBiPred ? mfxU8(extOpt3->WeightedBiPred - 1) : mfxU8(extDdi->WeightedBiPredIdc);
            extPps->picInitQpMinus26 = 0;
            extPps->picInitQsMinus26 = 0;
            extPps->chromaQpIndexOffset = 0;
            extPps->deblockingFilterControlPresentFlag = 1;
            extPps->constrainedIntraPredFlag = 0;
            extPps->redundantPicCntPresentFlag = 0;
            extPps->moreRbspData =
                !IsAvcBaseProfile(par.mfx.CodecProfile) &&
                par.mfx.CodecProfile != MFX_PROFILE_AVC_MAIN;
            extPps->transform8x8ModeFlag = extDdi->Transform8x8Mode == MFX_CODINGOPTION_UNKNOWN ?
                extOpt->IntraPredBlockSize > MFX_BLOCKSIZE_MIN_16X16 : IsOn(extDdi->Transform8x8Mode);
            extPps->picScalingMatrixPresentFlag = 0;
            extPps->secondChromaQpIndexOffset = 0;
        }

#ifdef MFX_ENABLE_AVC_CUSTOM_QMATRIX
        if (IsOn(extOpt3->AdaptiveCQM)&&
            isAdaptiveCQMSupported(extOpt3->ScenarioInfo, IsOn(par.mfx.LowPower)))
        {
            std::vector<mfxExtPpsHeader> &extCqmPps = par.GetCqmPps();
            for (mfxU8 j = 0; j < extCqmPps.size(); j++)
            {
                extCqmPps[j] = *extPps;
                extCqmPps[j].picParameterSetId = j + 1;
                FillCustomScalingLists(&(extCqmPps[j].scalingList4x4[0][0]), extOpt3->ScenarioInfo, j+1);
                extCqmPps[j].picScalingMatrixPresentFlag = 1;
                for (mfxU8 i = 0; i < sizeof(extCqmPps[j].picScalingListPresentFlag) / sizeof(extCqmPps[j].picScalingListPresentFlag[0]); ++i)
                    extCqmPps[j].picScalingListPresentFlag[i] = (i < ((extSps->levelIdc != 3) ? 8 : 12)) ? 1 : 0;
            }

        }
#endif

    }

#ifndef MFX_AVC_ENCODING_UNIT_DISABLE
    SetDefaultOff(extOpt3->EncodedUnitsInfo);
#endif


    par.SyncCalculableToVideoParam();
    par.AlignCalcWithBRCParamMultiplier();
}

mfxStatus MfxHwH264Encode::CheckPayloads(
    const mfxPayload* const* payload,
    mfxU16 numPayload)
{
    const mfxU8 SUPPORTED_SEI[] = {
        0, // 00 buffering_period
        0, // 01 pic_timing
        1, // 02 pan_scan_rect
        1, // 03 filler_payload
        1, // 04 user_data_registered_itu_t_t35
        1, // 05 user_data_unregistered
        1, // 06 recovery_point
        0, // 07 dec_ref_pic_marking_repetition
        0, // 08 spare_pic
        1, // 09 scene_info
        0, // 10 sub_seq_info
        0, // 11 sub_seq_layer_characteristics
        0, // 12 sub_seq_characteristics
        1, // 13 full_frame_freeze
        1, // 14 full_frame_freeze_release
        1, // 15 full_frame_snapshot
        1, // 16 progressive_refinement_segment_start
        1, // 17 progressive_refinement_segment_end
        0, // 18 motion_constrained_slice_group_set
        1, // 19 film_grain_characteristics
        1, // 20 deblocking_filter_display_preference
        1, // 21 stereo_video_info
        0, // 22 post_filter_hint
        0, // 23 tone_mapping_info
        0, // 24 scalability_info
        0, // 25 sub_pic_scalable_layer
        0, // 26 non_required_layer_rep
        0, // 27 priority_layer_info
        0, // 28 layers_not_present
        0, // 29 layer_dependency_change
        0, // 30 scalable_nesting
        0, // 31 base_layer_temporal_hrd
        0, // 32 quality_layer_integrity_check
        0, // 33 redundant_pic_property
        0, // 34 tl0_dep_rep_index
        0, // 35 tl_switching_point
        0, // 36 parallel_decoding_info
        0, // 37 mvc_scalable_nesting
        0, // 38 view_scalability_info
        0, // 39 multiview_scene_info
        0, // 40 multiview_acquisition_info
        0, // 41 non_required_view_component
        0, // 42 view_dependency_change
        0, // 43 operation_points_not_present
        0, // 44 base_view_temporal_hrd
        1, // 45 frame_packing_arrangement
    };

    for (mfxU16 i = 0; i < numPayload; ++i)
    {
        // Particular payload can absent
        // Check only present payloads
        if (payload[i] && payload[i]->NumBit > 0)
        {
            MFX_CHECK_NULL_PTR1(payload[i]->Data);

            // Check buffer size
            MFX_CHECK(
                payload[i]->NumBit <= 8u * payload[i]->BufSize,
                MFX_ERR_UNDEFINED_BEHAVIOR);

            // Sei messages type constraints
            MFX_CHECK(
                payload[i]->Type < (sizeof SUPPORTED_SEI / sizeof SUPPORTED_SEI[0]) &&
                SUPPORTED_SEI[payload[i]->Type] == 1,
                MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MfxHwH264Encode::CheckRunTimeExtBuffers(
    MfxVideoParam const   & video,
    mfxEncodeCtrl         * ctrl,
    mfxFrameSurface1      * surface,
    mfxBitstream          * bs,
    MFX_ENCODE_CAPS const & caps)
{
    MFX_CHECK_NULL_PTR3(ctrl, surface, bs);
    mfxStatus checkSts = MFX_ERR_NONE;

    for (mfxU32 i = 0; i < bs->NumExtParam; i++)
    {
        MFX_CHECK_NULL_PTR1(bs->ExtParam[i]);

        if (!IsRuntimeOutputExtBufferIdSupported(video, bs->ExtParam[i]->BufferId))
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM; // don't return error in runtime, just ignore unsupported ext buffer and return warning
    }

    for (mfxU32 i = 0; i < ctrl->NumExtParam; i++)
    {
        MFX_CHECK_NULL_PTR1(ctrl->ExtParam[i]);

        if (!IsRunTimeExtBufferIdSupported(video, ctrl->ExtParam[i]->BufferId))
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM; // don't return error in runtime, just ignore unsupported ext buffer and return warning

        bool buffer_pair = mfx::GetExtBuffer(
                            ctrl->ExtParam + i + 1,
                            ctrl->NumExtParam - i - 1,
                            ctrl->ExtParam[i]->BufferId)
                            ||
                           mfx::GetExtBuffer(
                            ctrl->ExtParam,
                            i,
                            ctrl->ExtParam[i]->BufferId);

        // In single-field mode only one buffer is required for one field encoding
        bool buffer_pair_allowed = IsRunTimeExtBufferPairAllowed(video, ctrl->ExtParam[i]->BufferId);

        // if initialized PicStruct is UNKNOWN, to check runtime PicStruct
        bool buffer_pair_required = IsRunTimeExtBufferPairRequired(video, ctrl->ExtParam[i]->BufferId);

        if (buffer_pair_required)
        {
            switch (video.mfx.FrameInfo.PicStruct)
            {
            case MFX_PICSTRUCT_PROGRESSIVE:
                buffer_pair_required = false;
                break;
            case MFX_PICSTRUCT_UNKNOWN:
                // for runtime PicStruct, it may be set as (MFX_PICSTRUCT_PROGRESSIVE |
                // MFX+PICSTRUCT_FIELD_TFF/BFF) for progressive frames
                if (surface->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE)
                {
                    buffer_pair_required = false;
                }
                break;
            default:
                break;
            }
        }

        if (buffer_pair && !buffer_pair_allowed)
        {
            // Ignore second buffer and return warning
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
        else if (!buffer_pair && buffer_pair_required)
        {
            // Return error if only one per-field buffer provided
            MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        }
    }

    mfxU16 PicStruct = (video.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN) ? video.mfx.FrameInfo.PicStruct : surface->Info.PicStruct;
    mfxU16 NumFields = (PicStruct & MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 2;

    {
        mfxStatus sts = CheckFEIRunTimeExtBuffersContent(video, ctrl, surface, bs);
        MFX_CHECK(sts >= MFX_ERR_NONE, sts);
        if (sts != MFX_ERR_NONE)
            checkSts = sts;
    }

#if defined (MFX_ENABLE_H264_ROUNDING_OFFSET)
    for (mfxU16 fieldId = 0; fieldId < NumFields; fieldId++)
    {
        mfxExtAVCRoundingOffset* extRoundingOffset =
                reinterpret_cast<mfxExtAVCRoundingOffset*>(mfx::GetExtBuffer(ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_AVC_ROUNDING_OFFSET, fieldId));
        if (extRoundingOffset)
        {
            if (IsOn(extRoundingOffset->EnableRoundingIntra))
            {
                if (extRoundingOffset->RoundingOffsetIntra > 7)
                {
                    checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                    extRoundingOffset->RoundingOffsetIntra = extRoundingOffset->RoundingOffsetIntra % 7;
                }
            }

            if (IsOn(extRoundingOffset->EnableRoundingInter))
            {
                if (extRoundingOffset->RoundingOffsetInter > 7)
                {
                    checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                    extRoundingOffset->RoundingOffsetInter = extRoundingOffset->RoundingOffsetInter % 7;
                }
            }
        }
    }
#endif

    mfxExtAVCRefListCtrl const * extRefListCtrl = GetExtBuffer(*ctrl);
    if (extRefListCtrl && video.calcParam.numTemporalLayer > 0 && video.calcParam.tempScalabilityMode == 0)
        checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;


    mfxExtAVCRefLists const * extRefLists = GetExtBuffer(*ctrl);
    if (extRefLists && video.calcParam.numTemporalLayer > 0 && video.calcParam.tempScalabilityMode == 0)
        checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

    // check timestamp from mfxExtPictureTimingSEI
    for (mfxU16 fieldId = 0; fieldId < NumFields; fieldId++)
    {
        mfxExtPictureTimingSEI const* extPt = reinterpret_cast<mfxExtPictureTimingSEI*>(mfx::GetExtBuffer(ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_PICTURE_TIMING_SEI, fieldId));

        if (extPt)
        {
            for (mfxU32 i = 0; i < 3; i++)
            {
                // return warning if there is any 0xffff template except for CtType
                if (extPt->TimeStamp[i].CountingType       == 0xffff ||
                    extPt->TimeStamp[i].NFrames            == 0xffff ||
                    extPt->TimeStamp[i].SecondsValue       == 0xffff ||
                    extPt->TimeStamp[i].MinutesValue       == 0xffff ||
                    extPt->TimeStamp[i].HoursValue         == 0xffff ||
                    extPt->TimeStamp[i].TimeOffset         == 0xffff ||
                    extPt->TimeStamp[i].ClockTimestampFlag == 0xffff ||
                    extPt->TimeStamp[i].NuitFieldBasedFlag == 0xffff ||
                    extPt->TimeStamp[i].FullTimestampFlag  == 0xffff ||
                    extPt->TimeStamp[i].DiscontinuityFlag  == 0xffff ||
                    extPt->TimeStamp[i].CntDroppedFlag     == 0xffff ||
                    extPt->TimeStamp[i].SecondsFlag        == 0xffff ||
                    extPt->TimeStamp[i].MinutesFlag        == 0xffff ||
                    extPt->TimeStamp[i].HoursFlag          == 0xffff)
                {
                    checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                }
            }
        }
    }

    // check ROI
    mfxExtEncoderROI const * extRoi = GetExtBuffer(*ctrl);

    if (extRoi) {
        // check below should have the same logic as in MfxHwH264Encode::ConfigureTask
        // the difference that ConfigureTask doesn't copy bad arg to DdiTask
        // CheckRunTimeExtBuffers just reports MFX_WRN_INCOMPATIBLE_VIDEO_PARAM for the same args

        mfxU16 const MaxNumOfROI = caps.ddi_caps.MaxNumOfROI;
        mfxU16 actualNumRoi = extRoi->NumROI;
        if (extRoi->NumROI)
        {
            if (extRoi->NumROI > MaxNumOfROI)
            {
                checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                actualNumRoi = MaxNumOfROI;
            }

            if (extRoi->ROIMode != MFX_ROI_MODE_QP_DELTA && extRoi->ROIMode != MFX_ROI_MODE_PRIORITY)
            {
                checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                actualNumRoi = 0;
            }

            if (extRoi->ROIMode == MFX_ROI_MODE_QP_DELTA && caps.ddi_caps.ROIBRCDeltaQPLevelSupport == 0)
            {
                checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                actualNumRoi = 0;
            }
        }

        for (mfxU16 i = 0; i < actualNumRoi; i++)
        {
                mfxRoiDesc task_roi = {extRoi->ROI[i].Left,  extRoi->ROI[i].Top,
                                       extRoi->ROI[i].Right, extRoi->ROI[i].Bottom, extRoi->ROI[i].Priority};

                // check runtime ROI
                mfxStatus sts = CheckAndFixRoiQueryLike(video, &task_roi, extRoi->ROIMode);
                if (sts != MFX_ERR_NONE)
                {
                    checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                }
        }
    }

#if defined(MFX_ENABLE_AVCE_DIRTY_RECTANGLE)
    mfxExtDirtyRect const * extDirtyRect = GetExtBuffer(*ctrl);

    if (extDirtyRect)
    {
        mfxU16 actualNumRect = extDirtyRect->NumRect;

        if (actualNumRect == 0)
        {
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        if (actualNumRect > MFX_MAX_DIRTY_RECT_COUNT)
        {
            actualNumRect = MFX_MAX_DIRTY_RECT_COUNT;
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        for (mfxU16 i = 0; i < actualNumRect; i++)
        {
            mfxStatus sts = CheckAndFixRectQueryLike(video, (mfxRectDesc*)(&(extDirtyRect->Rect[i])));
            if (sts != MFX_ERR_NONE)
                checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }
#else
    mfxExtDirtyRect const * extDirtyRect = GetExtBuffer(*ctrl);

    if (extDirtyRect)
    {
        checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

#endif

#if defined(MFX_ENABLE_AVCE_MOVE_RECTANGLE)
    mfxExtMoveRect const * extMoveRect = GetExtBuffer(*ctrl);

    if (extMoveRect)
    {
        mfxU16 actualNumRect = extMoveRect->NumRect;

        if (actualNumRect == 0)
        {
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        if (actualNumRect > MFX_MAX_MOVE_RECT_COUNT)
        {
            actualNumRect = MFX_MAX_MOVE_RECT_COUNT;
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        for (mfxU16 i = 0; i < actualNumRect; i++)
        {
            mfxStatus sts = CheckAndFixMovingRectQueryLike(video, (mfxMovingRectDesc*)(&(extMoveRect->Rect[i])));
            if (sts != MFX_ERR_NONE)
                checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }
#else
    mfxExtMoveRect const * extMoveRect = GetExtBuffer(*ctrl);

    if (extMoveRect)
    {
        checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

#endif

    mfxExtPredWeightTable *extPwt = GetExtBuffer(*ctrl);
    if (extPwt)
    {
        if (extPwt->LumaLog2WeightDenom && (extPwt->LumaLog2WeightDenom != 6))
        {
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
        if (extPwt->ChromaLog2WeightDenom && (extPwt->ChromaLog2WeightDenom != 6))
        {
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    MFX_RETURN(checkSts);
}

mfxStatus MfxHwH264Encode::CheckFEIRunTimeExtBuffersContent(
    MfxVideoParam const & video,
    mfxEncodeCtrl       * ctrl,
    mfxFrameSurface1    * surface,
    mfxBitstream        * bs)
{
    MFX_CHECK_NULL_PTR3(ctrl, surface, bs);
    mfxStatus checkSts = MFX_ERR_NONE;

    (void)video;
    (void)bs;

    MFX_RETURN(checkSts);
}

mfxStatus MfxHwH264Encode::CheckRunTimePicStruct(
    mfxU16 runtPs,
    mfxU16 initPs)
{
    static mfxU16 const PRG  = MFX_PICSTRUCT_PROGRESSIVE;
    static mfxU16 const TFF  = MFX_PICSTRUCT_FIELD_TFF;
    static mfxU16 const BFF  = MFX_PICSTRUCT_FIELD_BFF;
    static mfxU16 const UNK  = MFX_PICSTRUCT_UNKNOWN;
    static mfxU16 const DBL  = MFX_PICSTRUCT_FRAME_DOUBLING;
    static mfxU16 const TRPL = MFX_PICSTRUCT_FRAME_TRIPLING;
    static mfxU16 const REP  = MFX_PICSTRUCT_FIELD_REPEATED;

    if ((initPs == PRG && runtPs == UNK)         ||
        (initPs == PRG && runtPs == PRG)         ||
        (initPs == PRG && runtPs == (PRG | DBL)) ||
        (initPs == PRG && runtPs == (PRG | TRPL)))
        return MFX_ERR_NONE;

    if ((initPs == BFF && runtPs == UNK) ||
        (initPs == BFF && runtPs == BFF) ||
        (initPs == UNK && runtPs == BFF) ||
        (initPs == TFF && runtPs == UNK) ||
        (initPs == TFF && runtPs == TFF) ||
        (initPs == UNK && runtPs == TFF))
        return MFX_ERR_NONE;

    if ((initPs == UNK && runtPs == (PRG | BFF))       ||
        (initPs == UNK && runtPs == (PRG | TFF))       ||
        (initPs == UNK && runtPs == (PRG | BFF | REP)) ||
        (initPs == UNK && runtPs == (PRG | TFF | REP)) ||
        (initPs == UNK && runtPs == PRG)               ||
        (initPs == BFF && runtPs == PRG)               ||
        (initPs == TFF && runtPs == PRG))
        return MFX_ERR_NONE;

    if (initPs == UNK && runtPs == UNK)
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);

    MFX_RETURN(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
}

bool MfxHwH264Encode::IsRecoveryPointSeiMessagePresent(
    const mfxPayload* const* payload,
    mfxU16 numPayload,
    mfxU32 payloadLayout)
{
    if (!payload)
        return false;

    mfxU32 step = payloadLayout == 0 /*MFX_PAYLOAD_LAYOUT_ALL*/ ? 1 : 2;
    mfxU32 i    = payloadLayout == 2 /*MFX_PAYLOAD_LAYOUT_ODD*/ ? 1 : 0;

    for (; i < numPayload; i += step)
        if (payload[i] && payload[i]->NumBit > 0 && payload[i]->Type == 6)
            break;

    return i < numPayload;
}

mfxStatus MfxHwH264Encode::CopyFrameDataBothFields(
    VideoCORE *               core,
    mfxMemId                  dstMid,
    const mfxFrameSurface1&   srcSurf,
    mfxFrameInfo const&       info,
    const mfxU16&             inMemType)
{
    mfxFrameSurface1 sysSurf = MakeSurface(info, srcSurf);
    mfxFrameSurface1 vidSurf = MakeSurface(info, dstMid);

    mfxStatus sts = core->DoFastCopyWrapper(
        &vidSurf,
        MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_FROM_ENCODE,
        &sysSurf,
        inMemType);
    MFX_CHECK_STS(sts);

    MFX_RETURN(sts);
}

#if defined(MFX_ENABLE_ENCTOOLS_LPLA)
bool MfxHwH264Encode::IsLpLookaheadSupported(mfxU16 scenario, mfxU16 lookaheadDepth, mfxU16 rateContrlMethod)
{
    if (scenario == MFX_SCENARIO_GAME_STREAMING && lookaheadDepth > 0 &&
        (rateContrlMethod == MFX_RATECONTROL_CBR || rateContrlMethod == MFX_RATECONTROL_VBR))
    {
        return true;
    }
    return false;
}
#endif


mfxU8 MfxHwH264Encode::ConvertFrameTypeMfx2Ddi(mfxU32 type)
{
    switch (type & MFX_FRAMETYPE_IPB)
    {
    case MFX_FRAMETYPE_I: return CODING_TYPE_I;
    case MFX_FRAMETYPE_P: return CODING_TYPE_P;
    case MFX_FRAMETYPE_B: return CODING_TYPE_B;
    default: assert(!"Unsupported frame type"); return 0;
    }
}

ENCODE_FRAME_SIZE_TOLERANCE MfxHwH264Encode::ConvertLowDelayBRCMfx2Ddi(mfxU16 type, bool bTCBRC)
{
    switch (type) {
        case MFX_CODINGOPTION_ON:
            return bTCBRC ? eFrameSizeTolerance_Normal : eFrameSizeTolerance_ExtremelyLow;
        default:
            return eFrameSizeTolerance_Normal;
    }
}

mfxU8 MfxHwH264Encode::ConvertMfxFrameType2SliceType(mfxU8 type)
{
    switch (type & MFX_FRAMETYPE_IPB)
    {
    case MFX_FRAMETYPE_I: return SLICE_TYPE_I + 5;
    case MFX_FRAMETYPE_P: return SLICE_TYPE_P + 5;
    case MFX_FRAMETYPE_B: return SLICE_TYPE_B + 5;
    default: assert("bad codingType"); return 0xff;
    }
}


mfxU32 SliceDivider::GetFirstMbInSlice() const
{
    assert(m_currSliceFirstMbRow * m_numMbInRow < 0x10000000);
    return m_currSliceFirstMbRow * m_numMbInRow;
}

mfxU32 SliceDivider::GetNumMbInSlice() const
{
    assert(m_currSliceNumMbRow * m_numMbInRow < 0x10000000);
    return m_currSliceNumMbRow * m_numMbInRow;
}

mfxU32 SliceDivider::GetNumSlice() const
{
    assert(0 < m_numSlice && m_numSlice < 0x10000000);
    return m_numSlice;
}


SliceDividerArbitraryRowSlice::SliceDividerArbitraryRowSlice(
    mfxU32 numSlice,
    mfxU32 widthInMbs,
    mfxU32 heightInMbs)
{
    m_pfNext              = &SliceDividerArbitraryRowSlice::Next;
    m_numSlice            = mfx::clamp(numSlice, 1u, heightInMbs);
    m_numMbInRow          = widthInMbs;
    m_numMbRow            = heightInMbs;
    m_leftSlice           = m_numSlice;
    m_leftMbRow           = m_numMbRow;
    m_currSliceFirstMbRow = 0;
    m_currSliceNumMbRow   = mfx::CeilDiv(m_leftMbRow, m_leftSlice);
}

bool SliceDividerArbitraryRowSlice::Next(SliceDividerState & state)
{
    state.m_leftMbRow -= state.m_currSliceNumMbRow;
    state.m_leftSlice -= 1;

    if (state.m_leftSlice == 0)
    {
        assert(state.m_leftMbRow == 0);
        return false;
    }
    else
    {
        state.m_currSliceFirstMbRow += state.m_currSliceNumMbRow;
        state.m_currSliceNumMbRow = mfx::CeilDiv(state.m_leftMbRow, state.m_leftSlice);
        assert(state.m_currSliceNumMbRow != 0);
        return true;
    }
}


SliceDividerOneSlice::SliceDividerOneSlice(
    mfxU32 /*numSlice*/,
    mfxU32 widthInMbs,
    mfxU32 heightInMbs)
{
    m_pfNext              = &SliceDividerOneSlice::Next;
    m_numSlice            = 1;
    m_numMbInRow          = widthInMbs;
    m_numMbRow            = heightInMbs;
    m_leftSlice           = 1;
    m_leftMbRow           = heightInMbs;
    m_currSliceFirstMbRow = 0;
    m_currSliceNumMbRow   = heightInMbs;
}

bool SliceDividerOneSlice::Next(SliceDividerState & state)
{
    state.m_leftMbRow = 0;
    state.m_leftSlice = 0;
    return false;
}


namespace
{
    // nearest but less than n
    mfxU32 GetNearestPowerOf2(mfxU32 n)
    {
        mfxU32 mask = 0x80000000;
        for (; mask > 0; mask >>= 1)
            if (n & mask)
                break;
        return mask;
    }
};

SliceDividerRow2Row::SliceDividerRow2Row(
    mfxU32 numSlice,
    mfxU32 widthInMbs,
    mfxU32 heightInMbs)
{
    m_pfNext              = &SliceDividerRow2Row::Next;
    m_numSlice            = std::max(numSlice, 1u);
    m_numMbInRow          = widthInMbs;
    m_numMbRow            = heightInMbs;
    m_leftSlice           = m_numSlice;
    m_leftMbRow           = m_numMbRow;
    m_currSliceFirstMbRow = 0;
    m_currSliceNumMbRow   = m_leftMbRow / m_leftSlice;

    // check if frame is divisible to requested number of slices
    mfxU32 numMbRowInSlice = std::max(1u, m_numMbRow / m_numSlice);
    numMbRowInSlice = GetNearestPowerOf2(numMbRowInSlice) << 1;
    if ((m_numMbRow + numMbRowInSlice - 1) / numMbRowInSlice < m_numSlice)
        numMbRowInSlice >>= 1; // As number of slices can only be increased, try smaller slices

    m_numSlice = (m_numMbRow + numMbRowInSlice - 1) / numMbRowInSlice;
    m_leftSlice = m_numSlice;
    m_currSliceNumMbRow = std::min<mfxU32>(numMbRowInSlice, m_leftMbRow);
}

bool SliceDividerRow2Row::Next(SliceDividerState & state)
{
    state.m_leftMbRow -= state.m_currSliceNumMbRow;
    state.m_leftSlice -= 1;

    if (state.m_leftSlice == 0)
    {
        assert(state.m_leftMbRow == 0);
        return false;
    }
    else
    {
        state.m_currSliceFirstMbRow += state.m_currSliceNumMbRow;
        if (state.m_currSliceNumMbRow > state.m_leftMbRow)
            state.m_currSliceNumMbRow = state.m_leftMbRow;
        assert(state.m_currSliceNumMbRow != 0);
        return true;
    }
}


SliceDividerRowSlice::SliceDividerRowSlice(
    mfxU32 numSlice,
    mfxU32 widthInMbs,
    mfxU32 heightInMbs)
{
    m_pfNext              = &SliceDividerRowSlice::Next;
    m_numSlice            = mfx::clamp(numSlice, 1u, heightInMbs);
    m_numMbInRow          = widthInMbs;
    m_numMbRow            = heightInMbs;
    m_currSliceFirstMbRow = 0;
    m_leftMbRow           = heightInMbs;

    mfxU32 H  = heightInMbs;      // frame height
    mfxU32 n  = m_numSlice;       // num slices
    mfxU32 sh = (H + n - 1) / n;  // slice height

    // check if frame is divisible to requested number of slices
    // so that size of last slice is no bigger than other slices
    while (sh * (n - 1) >= H)
    {
        n++;
        sh = (H + n - 1) / n;
    }

    m_currSliceNumMbRow   = sh;
    m_numSlice            = n;
    m_leftSlice           = n;
}

bool SliceDividerRowSlice::Next(SliceDividerState & state)
{
    state.m_leftMbRow -= state.m_currSliceNumMbRow;
    state.m_leftSlice -= 1;

    if (state.m_leftSlice == 0)
    {
        assert(state.m_leftMbRow == 0);
        return false;
    }
    else
    {
        state.m_currSliceFirstMbRow += state.m_currSliceNumMbRow;
        if (state.m_currSliceNumMbRow > state.m_leftMbRow)
            state.m_currSliceNumMbRow = state.m_leftMbRow;
        assert(state.m_currSliceNumMbRow != 0);
        return true;
    }
}

SliceDividerTemporalScalability::SliceDividerTemporalScalability(
    mfxU32 sliceSizeInMbs,
    mfxU32 widthInMbs,
    mfxU32 heightInMbs)
{
    m_pfNext              = &SliceDividerTemporalScalability::Next;
    m_numMbInRow          = 1;
    m_numMbRow            = heightInMbs*widthInMbs;
    m_currSliceFirstMbRow = 0;
    m_leftMbRow           = m_numMbRow;
    m_numSlice            = (m_numMbRow + sliceSizeInMbs - 1) / sliceSizeInMbs;
    m_currSliceNumMbRow   = sliceSizeInMbs;
    m_leftSlice           = m_numSlice;
}

bool SliceDividerTemporalScalability::Next(SliceDividerState & state)
{
    state.m_leftMbRow -= state.m_currSliceNumMbRow;
    state.m_leftSlice -= 1;

    if (state.m_leftSlice == 0)
    {
        assert(state.m_leftMbRow == 0);
        return false;
    }
    else
    {
        state.m_currSliceFirstMbRow += state.m_currSliceNumMbRow;
        if (state.m_currSliceNumMbRow > state.m_leftMbRow)
            state.m_currSliceNumMbRow = state.m_leftMbRow;
        assert(state.m_currSliceNumMbRow != 0);
        return true;
    }
}

SliceDividerLowPowerTemporalScalability::SliceDividerLowPowerTemporalScalability(
    mfxU32 sliceSizeInMbs,
    mfxU32 widthInMbs,
    mfxU32 heightInMbs)
{
    m_pfNext              = &SliceDividerLowPowerTemporalScalability::Next;
    m_numMbInRow          = 1;
    m_numMbRow            = heightInMbs*widthInMbs;
    m_currSliceFirstMbRow = 0;
    m_leftMbRow           = m_numMbRow;
    m_numSlice            = (m_numMbRow + sliceSizeInMbs - 1) / sliceSizeInMbs;
    m_currSliceNumMbRow   = sliceSizeInMbs;
    m_leftSlice           = m_numSlice;
}

bool SliceDividerLowPowerTemporalScalability::Next(SliceDividerState & state)
{
    state.m_leftMbRow -= state.m_currSliceNumMbRow;
    state.m_leftSlice -= 1;

    if (state.m_leftSlice == 0)
    {
        assert(state.m_leftMbRow == 0);
        return false;
    }
    else
    {
        state.m_currSliceFirstMbRow = 0; //state.m_currSliceNumMbRow;
        if (state.m_currSliceNumMbRow > state.m_leftMbRow)
            state.m_currSliceNumMbRow = state.m_leftMbRow;
        assert(state.m_currSliceNumMbRow != 0);
        return true;
    }
}

SliceDividerLowPower::SliceDividerLowPower(
    mfxU32 numSlice,
    mfxU32 widthInMbs,
    mfxU32 heightInMbs)
{
    m_pfNext              = &SliceDividerLowPower::Next;
    m_numSlice            = mfx::clamp(numSlice, 1u, heightInMbs);
    m_numMbInRow          = widthInMbs;
    m_numMbRow            = heightInMbs;
    m_currSliceFirstMbRow = 0;
    m_leftMbRow           = heightInMbs;

    mfxU32 H  = heightInMbs;      // frame height
    mfxU32 n  = m_numSlice;       // num slices
    mfxU32 sh = (H + n - 1) / n;  // slice height

    // check if frame is divisible to requested number of slices
    // so that size of last slice is no bigger than other slices
    while (sh * (n - 1) >= H)
    {
        n++;
        sh = (H + n - 1) / n;
    }

    m_currSliceNumMbRow   = sh;
    m_numSlice            = n;
    m_leftSlice           = n;
}

bool SliceDividerLowPower::Next(SliceDividerState & state)
{
    state.m_leftMbRow -= state.m_currSliceNumMbRow;
    state.m_leftSlice -= 1;

    if (state.m_leftSlice == 0)
    {
        assert(state.m_leftMbRow == 0);
        return false;
    }
    else
    {
        state.m_currSliceFirstMbRow = 0;//state.m_currSliceNumMbRow;
        if (state.m_currSliceNumMbRow > state.m_leftMbRow)
            state.m_currSliceNumMbRow = state.m_leftMbRow;
        assert(state.m_currSliceNumMbRow != 0);
        return true;
    }
}

SliceDivider MfxHwH264Encode::MakeSliceDivider(
    SliceDividerType sliceHwCaps,
    mfxU32  sliceSizeInMbs,
    mfxU32  numSlice,
    mfxU32  widthInMbs,
    mfxU32  heightInMbs,
    bool isLowPower)
{
    if(isLowPower){
        if(sliceHwCaps != SliceDividerType::ONESLICE && sliceSizeInMbs > 0)
            return SliceDividerLowPowerTemporalScalability(sliceSizeInMbs, widthInMbs, heightInMbs);

        return SliceDividerLowPower(numSlice, widthInMbs, heightInMbs);
    }

    if(sliceHwCaps != SliceDividerType::ONESLICE && sliceSizeInMbs > 0)
        return SliceDividerTemporalScalability(sliceSizeInMbs, widthInMbs, heightInMbs);

    switch (sliceHwCaps)
    {
    case SliceDividerType::ROW2ROW:  return SliceDividerRow2Row(numSlice, widthInMbs, heightInMbs);
    case SliceDividerType::ROWSLICE:  return SliceDividerRowSlice(numSlice, widthInMbs, heightInMbs);
    case SliceDividerType::ARBITRARY_ROW_SLICE:  return SliceDividerArbitraryRowSlice(numSlice, widthInMbs, heightInMbs);
    //If arbitrary slice size supported by HW make legacy HSW division, need to implement arbitrary case
    case SliceDividerType::ARBITRARY_MB_SLICE:  return SliceDividerRowSlice(numSlice, widthInMbs, heightInMbs);
    default: return SliceDividerOneSlice(numSlice, widthInMbs, heightInMbs);
    }
}


namespace
{
    bool TestSliceDivider(
        mfxU32 requestedNumSlice,
        mfxU32 width,
        mfxU32 height,
        mfxU32 refNumSlice,
        mfxU32 refNumMbRowsInSlice,
        mfxU32 refNumMbRowsInLastSlice)
    {
        mfxU32 numMbsInRow = (width + 15) / 16;
        SliceDivider sd = SliceDividerRow2Row(requestedNumSlice, numMbsInRow, (height + 15) / 16);

        mfxU32 numSlice = sd.GetNumSlice();
        if (numSlice != refNumSlice)
            return false;

        mfxU32 predictedPirstMbInSlice = 0;
        for (mfxU32 i = 0; i < numSlice; i++)
        {
            mfxU32 firstMbInSlice = sd.GetFirstMbInSlice();
            mfxU32 numMbInSlice = sd.GetNumMbInSlice();

            if (firstMbInSlice != predictedPirstMbInSlice)
                return false;

            bool hasNext = sd.Next();
            if (hasNext != (i + 1 < numSlice))
                return false;

            if (hasNext)
            {
                if (numMbInSlice != refNumMbRowsInSlice * numMbsInRow)
                    return false;
            }
            else
            {
                if (numMbInSlice != refNumMbRowsInLastSlice * numMbsInRow)
                    return false;
            }

            predictedPirstMbInSlice += numMbInSlice;
        }

        return true;
    }

    bool TestSliceDividerWithReport(
        mfxU32 requestedNumSlice,
        mfxU32 width,
        mfxU32 height,
        mfxU32 refNumSlice,
        mfxU32 refNumMbRowsInSlice,
        mfxU32 refNumMbRowsInLastSlice)
    {
        if (!TestSliceDivider(requestedNumSlice, width, height, refNumSlice, refNumMbRowsInSlice, refNumMbRowsInLastSlice))
        {
            //std::cout << "Failed with requestedNumSlice=" << requestedNumSlice << " width=" << width << " height=" << height << std::endl;
            return false;
        }

        //std::cout << "Passed with requestedNumSlice=" << requestedNumSlice << " width=" << width << " height=" << height << std::endl;
        return true;
    }
}

void SliceDividerRow2Row::Test()
{
    TestSliceDividerWithReport( 2,  720,  480,  2, 16, 14);
    TestSliceDividerWithReport( 4,  720,  480,  4,  8,  6);
    TestSliceDividerWithReport( 8,  720,  480,  8,  4,  2);
    TestSliceDividerWithReport(16,  720,  480, 30,  1,  1);
    TestSliceDividerWithReport( 2, 1920, 1080,  2, 64,  4);
    TestSliceDividerWithReport( 4, 1920, 1080,  5, 16,  4);
    TestSliceDividerWithReport( 8, 1920, 1080,  9,  8,  4);
    TestSliceDividerWithReport(16, 1920, 1080, 17,  4,  4);
    TestSliceDividerWithReport( 8, 1280,  720, 12,  4,  1);
    TestSliceDividerWithReport( 8,  480,  320, 10,  2,  2);

    TestSliceDividerWithReport( 2,  720,  480 / 2,  2,  8,  7);
    TestSliceDividerWithReport( 4,  720,  480 / 2,  4,  4,  3);
    TestSliceDividerWithReport( 8,  720,  480 / 2,  8,  2,  1);
    TestSliceDividerWithReport(16,  720,  480 / 2, 15,  1,  1);
    TestSliceDividerWithReport( 2, 1920, 1080 / 2,  2, 32,  2);
    TestSliceDividerWithReport( 4, 1920, 1080 / 2,  5,  8,  2);
    TestSliceDividerWithReport( 8, 1920, 1080 / 2,  9,  4,  2);
    TestSliceDividerWithReport(16, 1920, 1080 / 2, 17,  2,  2);
    TestSliceDividerWithReport( 8, 1280,  720 / 2, 12,  2,  1);
    TestSliceDividerWithReport( 8,  480,  320 / 2, 10,  1,  1);
}

MfxVideoParam::MfxVideoParam()
    : mfxVideoParam()
    , m_extOpt()
    , m_extOpt2()
    , m_extOpt3()
    , m_extOptSpsPps()
    , m_extVideoSignal()
    , m_extMvcSeqDescr()
    , m_extPicTiming()
    , m_extTempLayers()
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    , m_extSvcSeqDescr()
    , m_extSvcRateCtrl()
#endif
    , m_extEncResetOpt()
    , m_extEncRoi()
    , m_extChromaLoc()
    , m_extPwt()
    , m_extDirtyRect()
    , m_extMoveRect()
    , m_extOptDdi()
    , m_extSps()
    , m_extPps()
#if defined(__MFXBRC_H__)
    , m_extBRC()
#if defined(MFX_ENABLE_ENCTOOLS)
    , m_encTools()
    , m_encToolsConfig()
    , m_extDevice()
    , m_extAllocator()
#endif
#endif
#if defined(MFX_ENABLE_PARTIAL_BITSTREAM_OUTPUT)
    , m_po()
#endif
    , calcParam()
{
    memset(m_extParam, 0, sizeof(m_extParam));
}

MfxVideoParam::MfxVideoParam(MfxVideoParam const & par)
{
    Construct(par);
    calcParam = par.calcParam;
}

MfxVideoParam::MfxVideoParam(mfxVideoParam const & par)
{
    Construct(par);
    SyncVideoToCalculableParam();
}

MfxVideoParam& MfxVideoParam::operator=(MfxVideoParam const & par)
{
    Construct(par);
#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
    std::copy(par.m_extCqmPps.begin(), par.m_extCqmPps.end(), m_extCqmPps.begin());
#endif
    calcParam = par.calcParam;
    return *this;
}

MfxVideoParam& MfxVideoParam::operator=(mfxVideoParam const & par)
{
    Construct(par);
    SyncVideoToCalculableParam();
    return *this;
}

void MfxVideoParam::SyncVideoToCalculableParam()
{
    mfxU32 multiplier = std::max<mfxU16>(mfx.BRCParamMultiplier, 1);

    calcParam.PPyrInterval = (mfx.NumRefFrame > 0) ? std::min<mfxU32>(DEFAULT_PPYR_INTERVAL, mfx.NumRefFrame) : DEFAULT_PPYR_INTERVAL;

    calcParam.bufferSizeInKB   = mfx.BufferSizeInKB   * multiplier;
    if (IsOn(m_extOpt.VuiNalHrdParameters)
        && !IsOn(m_extOpt.VuiVclHrdParameters)
        && IsOff(m_extOpt.NalHrdConformance)
        && mfx.RateControlMethod == MFX_RATECONTROL_CQP
        && mfx.FrameInfo.FrameRateExtN != 0
        && mfx.FrameInfo.FrameRateExtD != 0
        && mfx.BufferSizeInKB != 0
        && mfx.InitialDelayInKB != 0
        && mfx.TargetKbps != 0)
    {
        calcParam.cqpHrdMode = mfx.MaxKbps > 0 ? 2 : 1;
    }

    if (calcParam.cqpHrdMode)
    {
        calcParam.decorativeHrdParam.bufferSizeInKB  = calcParam.bufferSizeInKB;
        calcParam.decorativeHrdParam.initialDelayInKB = mfx.InitialDelayInKB * multiplier;
        calcParam.decorativeHrdParam.targetKbps       = mfx.TargetKbps       * multiplier;
        calcParam.decorativeHrdParam.maxKbps          = mfx.MaxKbps > 0 ? mfx.MaxKbps       * multiplier : calcParam.decorativeHrdParam.targetKbps;
    }

    if (mfx.RateControlMethod != MFX_RATECONTROL_CQP
        && mfx.RateControlMethod != MFX_RATECONTROL_ICQ
        && mfx.RateControlMethod != MFX_RATECONTROL_LA_ICQ)
    {
        calcParam.initialDelayInKB = mfx.InitialDelayInKB * multiplier;
        calcParam.targetKbps       = mfx.TargetKbps       * multiplier;
        calcParam.maxKbps          = mfx.MaxKbps          * multiplier;
    }
    else
    {
        calcParam.bufferSizeInKB = calcParam.initialDelayInKB = calcParam.maxKbps = 0;
    }
    if (   mfx.RateControlMethod == MFX_RATECONTROL_LA
        || mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD
        || mfx.RateControlMethod == MFX_RATECONTROL_CBR)
        calcParam.WinBRCMaxAvgKbps = m_extOpt3.WinBRCMaxAvgKbps * multiplier;

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    if (IsSvcProfile(mfx.CodecProfile))
    {
        calcParam.numTemporalLayer = 0;
        calcParam.tid[0]   = 0;
        calcParam.scale[0] = 1;

        for (mfxU32 did = 8; did > 0; did--)
        {
            if (m_extSvcSeqDescr.DependencyLayer[did - 1].Active)
            {
                // 'numTemporalLayers' is used as temporal fix for array bound violation (calcParam.tid[], calcParam.scale[])
                // need to implement correct checking for 'TemporalNum' (this param is used before calling CheckVideoParamQueryLike())
                mfxU16 numTemporalLayers = std::min<mfxU16>(m_extSvcSeqDescr.DependencyLayer[did - 1].TemporalNum, 8);
                for (mfxU32 tidx = 0; tidx < numTemporalLayers; tidx++)
                {
                    mfxU32 tid = m_extSvcSeqDescr.DependencyLayer[did - 1].TemporalId[tidx];
                    calcParam.tid[calcParam.numTemporalLayer]   = tid;
                    calcParam.scale[calcParam.numTemporalLayer] = m_extSvcSeqDescr.TemporalScale[tid];
                    calcParam.numTemporalLayer++;
                }
                break;
            }
        }
    }
    else
#endif
    {
        calcParam.numTemporalLayer = 0;
        calcParam.tid[0]   = 0;
        calcParam.scale[0] = 1;
        for (mfxU32 i = 0; i < 8; i++)
        {
            if (m_extTempLayers.Layer[i].Scale != 0)
            {
                calcParam.tid[calcParam.numTemporalLayer]   = i;
                calcParam.scale[calcParam.numTemporalLayer] = m_extTempLayers.Layer[i].Scale;
                calcParam.numTemporalLayer++;
            }
        }

        if (calcParam.numTemporalLayer)
            calcParam.tempScalabilityMode = 1;

        calcParam.numDependencyLayer = 1;
        calcParam.numLayersTotal     = 1;
    }

    if (IsMvcProfile(mfx.CodecProfile))
    {
        mfxExtMVCSeqDesc * extMvc = GetExtBuffer(*this);
        if (extMvc && extMvc->NumView)
        {
            calcParam.mvcPerViewPar.bufferSizeInKB   = calcParam.bufferSizeInKB / extMvc->NumView;
            if (mfx.RateControlMethod != MFX_RATECONTROL_CQP
                && mfx.RateControlMethod != MFX_RATECONTROL_ICQ
                && mfx.RateControlMethod != MFX_RATECONTROL_LA_ICQ)
            {
                calcParam.mvcPerViewPar.initialDelayInKB = calcParam.initialDelayInKB / extMvc->NumView;
                calcParam.mvcPerViewPar.targetKbps       = calcParam.targetKbps / extMvc->NumView;
                calcParam.mvcPerViewPar.maxKbps          = calcParam.maxKbps / extMvc->NumView;
            }
            else
            {
                calcParam.mvcPerViewPar.initialDelayInKB = calcParam.mvcPerViewPar.targetKbps = calcParam.mvcPerViewPar.maxKbps = 0;
            }
        }
        calcParam.mvcPerViewPar.codecLevel       = mfx.CodecLevel;
    }

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    if (IsSvcProfile(mfx.CodecProfile))
    {
        calcParam.numLayersTotal     = 0;
        calcParam.numDependencyLayer = 0;
        for (mfxU32 i = 0; i < 8; i++)
        {
            if (m_extSvcSeqDescr.DependencyLayer[i].Active)
            {
                calcParam.did[calcParam.numDependencyLayer] = i;
                calcParam.numLayerOffset[calcParam.numDependencyLayer] = calcParam.numLayersTotal;
                calcParam.numLayersTotal +=
                    m_extSvcSeqDescr.DependencyLayer[i].QualityNum *
                    m_extSvcSeqDescr.DependencyLayer[i].TemporalNum;
                calcParam.numDependencyLayer++;
            }
        }

        calcParam.dqId1Exists = m_extSvcSeqDescr.DependencyLayer[calcParam.did[0]].QualityNum > 1 ? 1 : 0;
    }
#endif
}

void MfxVideoParam::SyncCalculableToVideoParam()
{
    mfxU32 maxVal32 = calcParam.bufferSizeInKB;

    if (mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        maxVal32 = std::max(maxVal32, calcParam.targetKbps);

        if (mfx.RateControlMethod != MFX_RATECONTROL_AVBR) {
            if (mfx.RateControlMethod == MFX_RATECONTROL_VBR || mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
                maxVal32 = std::max({maxVal32, calcParam.maxKbps, calcParam.initialDelayInKB});
            else
                maxVal32 = std::max({maxVal32, calcParam.maxKbps, calcParam.initialDelayInKB, calcParam.WinBRCMaxAvgKbps});
        }
    }

    mfx.BRCParamMultiplier = mfxU16((maxVal32 + 0x10000) / 0x10000);
    if (calcParam.cqpHrdMode == 0 || calcParam.bufferSizeInKB)
    {
        mfx.BufferSizeInKB     = mfxU16(calcParam.bufferSizeInKB / mfx.BRCParamMultiplier);
    }

    if (mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
        mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
        mfx.RateControlMethod == MFX_RATECONTROL_VCM ||
        mfx.RateControlMethod == MFX_RATECONTROL_AVBR||
        mfx.RateControlMethod == MFX_RATECONTROL_QVBR ||
        (bRateControlLA(mfx.RateControlMethod) && (mfx.RateControlMethod != MFX_RATECONTROL_LA_ICQ)))
    {
        mfx.TargetKbps = mfxU16(calcParam.targetKbps / mfx.BRCParamMultiplier);

        if (mfx.RateControlMethod != MFX_RATECONTROL_AVBR)
        {
            mfx.InitialDelayInKB = mfxU16(calcParam.initialDelayInKB / mfx.BRCParamMultiplier);
            mfx.MaxKbps          = mfxU16(calcParam.maxKbps / mfx.BRCParamMultiplier);
        }
    }
    if (   mfx.RateControlMethod == MFX_RATECONTROL_LA
        || mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD
        || mfx.RateControlMethod == MFX_RATECONTROL_CBR)
        m_extOpt3.WinBRCMaxAvgKbps = mfxU16(calcParam.WinBRCMaxAvgKbps / mfx.BRCParamMultiplier);
}

void MfxVideoParam::AlignCalcWithBRCParamMultiplier()
{
    if(!mfx.BRCParamMultiplier)
        return;

    if (mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        calcParam.bufferSizeInKB   = mfx.BufferSizeInKB   * mfx.BRCParamMultiplier;
        calcParam.initialDelayInKB = mfx.InitialDelayInKB * mfx.BRCParamMultiplier;
        calcParam.targetKbps       = mfx.TargetKbps       * mfx.BRCParamMultiplier;
        calcParam.maxKbps          = mfx.MaxKbps          * mfx.BRCParamMultiplier;
        calcParam.WinBRCMaxAvgKbps = m_extOpt3.WinBRCMaxAvgKbps * mfx.BRCParamMultiplier;
    }
}

void MfxVideoParam::Construct(mfxVideoParam const & par)
{
    mfxVideoParam & base = *this;
    base = par;

    Zero(m_extParam);
    Zero(calcParam);

    NumExtParam = 0;

#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
    m_extCqmPps.resize(CQM_HINT_NUM_CUST_MATRIX);
#endif

#define CONSTRUCT_EXT_BUFFER(type, name)        \
    InitExtBufHeader(name);                     \
    if (type * opts = GetExtBuffer(par))        \
    {    name = *opts; }              \
    m_extParam[NumExtParam++] = &name.Header;
#define CONSTRUCT_EXT_BUFFER_EX(type, name, field)        \
    InitExtBufHeader(name[field]);                        \
    if (type * opts = GetExtBuffer(par, field))           \
    name[field] = *opts;                                  \
    m_extParam[NumExtParam++] = &name[field].Header;
#define CONSTRUCT_EXT_BUFFER_DEF(type, name, defValue) \
    InitExtBufHeader(name, defValue);                     \
    if (type * opts = GetExtBuffer(par))        \
    {    name = *opts; }              \
    m_extParam[NumExtParam++] = &name.Header;

    CONSTRUCT_EXT_BUFFER(mfxExtCodingOption,         m_extOpt);
    CONSTRUCT_EXT_BUFFER(mfxExtCodingOptionSPSPPS,   m_extOptSpsPps);
    CONSTRUCT_EXT_BUFFER(mfxExtVideoSignalInfo,      m_extVideoSignal);
    // perform deep copy of the mfxExtMVCSeqDesc buffer
    InitExtBufHeader(m_extMvcSeqDescr);
    if (mfxExtMVCSeqDesc * buffer = GetExtBuffer(par))
        ConstructMvcSeqDesc(*buffer);
    m_extParam[NumExtParam++] = &m_extMvcSeqDescr.Header;

    CONSTRUCT_EXT_BUFFER(mfxExtPictureTimingSEI,     m_extPicTiming);
    CONSTRUCT_EXT_BUFFER(mfxExtAvcTemporalLayers,    m_extTempLayers);
#if defined(MFX_ENABLE_ENCODE_QUALITYINFO)
    CONSTRUCT_EXT_BUFFER(mfxExtQualityInfoMode,      m_extQualityInfoMode);
#endif
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    CONSTRUCT_EXT_BUFFER(mfxExtSVCSeqDesc,           m_extSvcSeqDescr);
    CONSTRUCT_EXT_BUFFER(mfxExtSVCRateControl,       m_extSvcRateCtrl);
#endif
    CONSTRUCT_EXT_BUFFER(mfxExtCodingOptionDDI,      m_extOptDdi);
    CONSTRUCT_EXT_BUFFER(mfxExtSpsHeader,            m_extSps);
    CONSTRUCT_EXT_BUFFER(mfxExtPpsHeader,            m_extPps);
    CONSTRUCT_EXT_BUFFER(mfxExtCodingOption2,        m_extOpt2);
    CONSTRUCT_EXT_BUFFER(mfxExtEncoderResetOption,   m_extEncResetOpt);
    CONSTRUCT_EXT_BUFFER(mfxExtEncoderROI,           m_extEncRoi);
    CONSTRUCT_EXT_BUFFER(mfxExtCodingOption3,        m_extOpt3);
    CONSTRUCT_EXT_BUFFER(mfxExtChromaLocInfo,        m_extChromaLoc);
    CONSTRUCT_EXT_BUFFER(mfxExtPredWeightTable,      m_extPwt);
    CONSTRUCT_EXT_BUFFER(mfxExtDirtyRect,            m_extDirtyRect);
    CONSTRUCT_EXT_BUFFER(mfxExtMoveRect,             m_extMoveRect);

#if defined(__MFXBRC_H__)
    CONSTRUCT_EXT_BUFFER(mfxExtBRC,                  m_extBRC);
#endif
#if defined(MFX_ENABLE_ENCTOOLS)
    mfxU16 et_default = MFX_CODINGOPTION_OFF;
    mfxExtEncToolsConfig *pConf = GetExtBuffer(par);
    if (!pConf)
    {
        // AEnc EncTools on for all GopRefDist
        if (m_extOpt2.LookAheadDepth > 0 && !bRateControlLA(mfx.RateControlMethod) && m_extOpt2.ExtBRC == MFX_CODINGOPTION_ON && m_extOpt3.ScenarioInfo == MFX_SCENARIO_UNKNOWN)
        {
            et_default = MFX_CODINGOPTION_UNKNOWN;
        }
#if defined(MFX_ENABLE_ENCTOOLS_LPLA)
        if (IsLpLookaheadSupported(m_extOpt3.ScenarioInfo, m_extOpt2.LookAheadDepth, mfx.RateControlMethod))
        {
            et_default = MFX_CODINGOPTION_UNKNOWN;
        }
#endif
    }
    CONSTRUCT_EXT_BUFFER(mfxEncTools, m_encTools);
    CONSTRUCT_EXT_BUFFER_DEF(mfxExtEncToolsConfig, m_encToolsConfig, et_default);
    CONSTRUCT_EXT_BUFFER(mfxEncToolsCtrlExtDevice, m_extDevice);
    CONSTRUCT_EXT_BUFFER(mfxEncToolsCtrlExtAllocator, m_extAllocator);

#endif

    CONSTRUCT_EXT_BUFFER(mfxExtVPPDenoise2, m_extVppHVS);



#if defined(MFX_ENABLE_PARTIAL_BITSTREAM_OUTPUT)
    CONSTRUCT_EXT_BUFFER(mfxExtPartialBitstreamParam , m_po);
#endif


#undef CONSTRUCT_EXT_BUFFER
#undef CONSTRUCT_EXT_BUFFER_EX

    ExtParam = m_extParam;
}

void MfxVideoParam::ConstructMvcSeqDesc(
    mfxExtMVCSeqDesc const & desc)
{
    m_extMvcSeqDescr.NumView        = desc.NumView;
    m_extMvcSeqDescr.NumViewAlloc   = desc.NumViewAlloc;
    m_extMvcSeqDescr.View           = 0;
    m_extMvcSeqDescr.NumViewId      = desc.NumViewId;
    m_extMvcSeqDescr.NumViewIdAlloc = desc.NumViewIdAlloc;
    m_extMvcSeqDescr.ViewId         = 0;
    m_extMvcSeqDescr.NumOP          = desc.NumOP;
    m_extMvcSeqDescr.NumOPAlloc     = desc.NumOPAlloc;
    m_extMvcSeqDescr.OP             = 0;
    m_extMvcSeqDescr.NumRefsTotal   = desc.NumRefsTotal;

    if (desc.View)
    {// potentially NumView and NumViewAlloc may differ, so reallocate for NumViewAlloc views
        m_storageView.resize(desc.NumViewAlloc);
        std::copy(desc.View,   desc.View   + desc.NumView,   m_storageView.begin());
        m_extMvcSeqDescr.View           = &m_storageView[0];

        if (desc.ViewId && desc.OP)
        {
            m_storageOp.resize(desc.NumOPAlloc);
            m_storageViewId.resize(desc.NumViewIdAlloc);

            std::copy(desc.OP,     desc.OP     + desc.NumOP,     m_storageOp.begin());
            std::copy(desc.ViewId, desc.ViewId + desc.NumViewId, m_storageViewId.begin());

            for (size_t i = 0; i < m_storageOp.size(); ++i)
            {
                ptrdiff_t offset = desc.OP[i].TargetViewId - desc.ViewId;
                m_storageOp[i].TargetViewId = &m_storageViewId[0] + offset;
            }

            m_extMvcSeqDescr.ViewId         = &m_storageViewId[0];
            m_extMvcSeqDescr.OP             = &m_storageOp[0];
        }
    }
}

void MfxVideoParam::ApplyDefaultsToMvcSeqDesc()
{
    if (false == IsMvcProfile(mfx.CodecProfile))
    {
        m_extMvcSeqDescr.NumView = 1;
        return;
    }

    if (m_extMvcSeqDescr.NumView == 0)
        m_extMvcSeqDescr.NumView = 2;

    if (m_extMvcSeqDescr.View == 0)
    {
        m_extMvcSeqDescr.NumViewAlloc = m_extMvcSeqDescr.NumView;
        m_storageView.resize(m_extMvcSeqDescr.NumView);
        Zero(m_storageView);
        for (size_t i = 0; i < m_storageView.size(); i++)
            m_storageView[i].ViewId = mfxU16(i);
        m_extMvcSeqDescr.View           = &m_storageView[0];
    }

    if (m_extMvcSeqDescr.ViewId == 0)
    {
        m_extMvcSeqDescr.NumViewId = m_extMvcSeqDescr.NumViewIdAlloc = m_extMvcSeqDescr.NumView;
        m_storageViewId.resize(m_extMvcSeqDescr.NumViewId);
        Zero(m_storageViewId);
        for (size_t i = 0; i < m_extMvcSeqDescr.NumViewIdAlloc; i++)
            m_storageViewId[i] = mfxU16(i);
        m_extMvcSeqDescr.ViewId         = &m_storageViewId[0];
    }

    if (m_extMvcSeqDescr.OP == 0)
    {
        m_extMvcSeqDescr.NumOP = 1;
        m_extMvcSeqDescr.NumOPAlloc = 1;
        m_storageOp.resize(m_extMvcSeqDescr.NumOP);
        Zero(m_storageOp);

        m_storageOp[0].TemporalId     = 0;
        m_storageOp[0].LevelIdc       = mfx.CodecLevel;
        m_storageOp[0].NumViews       = mfxU16(m_storageViewId.size());
        m_storageOp[0].NumTargetViews = mfxU16(m_storageViewId.size());
        m_storageOp[0].TargetViewId   = &m_storageViewId[0];

        m_extMvcSeqDescr.OP             = m_storageOp.size() ? &m_storageOp[0] : 0;
    }
    else
    {
        for (mfxU32 i = 0; i < m_extMvcSeqDescr.NumOP; i++)
        {
            if (m_extMvcSeqDescr.OP[i].LevelIdc == 0)
                m_extMvcSeqDescr.OP[i].LevelIdc = mfx.CodecLevel;
        }
    }
}

AspectRatioConverter::AspectRatioConverter(mfxU16 sarw, mfxU16 sarh) :
    m_sarIdc(0),
    m_sarWidth(0),
    m_sarHeight(0)
{
    if (sarw != 0 && sarh != 0)
    {
        for (mfxU8 i = 1; i < sizeof(TABLE_E1) / sizeof(TABLE_E1[0]); i++)
        {
            if ((sarw % TABLE_E1[i].w) == 0 &&
                (sarh % TABLE_E1[i].h) == 0 &&
                (sarw / TABLE_E1[i].w) == (sarh / TABLE_E1[i].h))
            {
                m_sarIdc    = i;
                m_sarWidth  = TABLE_E1[i].w;
                m_sarHeight = TABLE_E1[i].h;
                return;
            }
        }

        m_sarIdc    = EXTENDED_SAR;
        m_sarWidth  = sarw;
        m_sarHeight = sarh;
    }
}

AspectRatioConverter::AspectRatioConverter(mfxU8 sarIdc, mfxU16 sarw, mfxU16 sarh)
{
    if (sarIdc < sizeof(TABLE_E1) / sizeof(TABLE_E1[0]))
    {
        m_sarIdc    = sarIdc;
        m_sarWidth  = TABLE_E1[sarIdc].w;
        m_sarHeight = TABLE_E1[sarIdc].h;
    }
    else
    {
        m_sarIdc    = EXTENDED_SAR;
        m_sarWidth  = sarw;
        m_sarHeight = sarh;
    }
}

namespace
{
#   ifdef min
#       undef min
#   endif
#   ifdef max
#       undef max
#   endif

    template <class Tfrom>
    class RangeChecker
    {
    public:
        RangeChecker(Tfrom from) : m_from(from) {}

        template <class Tto> operator Tto() const
        {
            if (std::numeric_limits<Tto>::min() <= m_from && m_from <= std::numeric_limits<Tto>::max())
                return static_cast<Tto>(m_from);
            throw InvalidBitstream();
        }

    private:
        Tfrom m_from;
    };

    class InputBitstreamCheckedRange
    {
    public:
        InputBitstreamCheckedRange(InputBitstream & impl) : m_impl(impl) {}

        mfxU32 NumBitsRead() const { return m_impl.NumBitsRead(); }
        mfxU32 NumBitsLeft() const { return m_impl.NumBitsLeft(); }

        mfxU8 GetBit() { return static_cast<mfxU8>(m_impl.GetBit()); }

        RangeChecker<mfxU32> GetBits(mfxU32 nbits) { return m_impl.GetBits(nbits); }
        RangeChecker<mfxU32> GetUe()               { return m_impl.GetUe(); }
        RangeChecker<mfxI32> GetSe()               { return m_impl.GetSe(); }

    private:
        void operator =(InputBitstreamCheckedRange const &);
        InputBitstream & m_impl;
    };

    void ReadScalingList(
        InputBitstreamCheckedRange & reader,
        mfxU8 *                      scalingList,
        mfxU32                       size)
    {
        mfxU8 lastScale = 8;
        mfxU8 nextScale = 8;
        const mfxI32* scan;

        if (size == 16)
            scan = UMC_H264_ENCODER::dec_single_scan[0];
        else
            scan = UMC_H264_ENCODER::dec_single_scan_8x8[0];

        for (mfxU32 i = 0; i < size; i++)
        {
            if (nextScale != 0)
            {
                mfxI32 deltaScale = reader.GetSe();
                if (deltaScale < -128 || deltaScale > 127)
                    throw InvalidBitstream();

                nextScale = mfxU8((lastScale + deltaScale + 256) & 0xff);
            }

            scalingList[scan[i]] = (nextScale == 0) ? lastScale : nextScale;
            lastScale = scalingList[scan[i]];
        }
    }

    void ReadHrdParameters(
        InputBitstreamCheckedRange & reader,
        HrdParameters &              hrd)
    {
        hrd.cpbCntMinus1                        = reader.GetUe();

        if (hrd.cpbCntMinus1 > 31)
            throw InvalidBitstream();

        hrd.bitRateScale                        = reader.GetBits(4);
        hrd.cpbSizeScale                        = reader.GetBits(4);

        for (mfxU32 i = 0; i <= hrd.cpbCntMinus1; i++)
        {
            hrd.bitRateValueMinus1[i]           = reader.GetUe();
            hrd.cpbSizeValueMinus1[i]           = reader.GetUe();
            hrd.cbrFlag[i]                      = reader.GetBit();
        }

        hrd.initialCpbRemovalDelayLengthMinus1  = reader.GetBits(5);
        hrd.cpbRemovalDelayLengthMinus1         = reader.GetBits(5);
        hrd.dpbOutputDelayLengthMinus1          = reader.GetBits(5);
        hrd.timeOffsetLength                    = reader.GetBits(5);
    }

    void WriteHrdParameters(
        OutputBitstream &     writer,
        HrdParameters const & hrd)
    {
        writer.PutUe(hrd.cpbCntMinus1);
        writer.PutBits(hrd.bitRateScale, 4);
        writer.PutBits(hrd.cpbSizeScale, 4);

        for (mfxU32 i = 0; i <= hrd.cpbCntMinus1; i++)
        {
            writer.PutUe(hrd.bitRateValueMinus1[i]);
            writer.PutUe(hrd.cpbSizeValueMinus1[i]);
            writer.PutBit(hrd.cbrFlag[i]);
        }

        writer.PutBits(hrd.initialCpbRemovalDelayLengthMinus1, 5);
        writer.PutBits(hrd.cpbRemovalDelayLengthMinus1, 5);
        writer.PutBits(hrd.dpbOutputDelayLengthMinus1, 5);
        writer.PutBits(hrd.timeOffsetLength, 5);
    }

    bool MoreRbspData(InputBitstream reader)
    {
        mfxU32 bitsLeft = reader.NumBitsLeft();

        if (bitsLeft == 0)
            return false;

        if (reader.GetBit() == 0)
            return true;

        --bitsLeft;
        for (; bitsLeft > 0; --bitsLeft)
            if (reader.GetBit() == 1)
                return true;

        return false;
    }
};

void MfxHwH264Encode::ReadSpsHeader(
    InputBitstream &  is,
    mfxExtSpsHeader & sps)
{
    // set defaults for optional fields
    Zero(sps.vui);
    sps.chromaFormatIdc               = 1;
    sps.vui.videoFormat               = 5;
    sps.vui.colourPrimaries           = 2;
    sps.vui.transferCharacteristics   = 2;
    sps.vui.matrixCoefficients        = 2;
    sps.vui.flags.fixedFrameRate      = 1;

    InputBitstreamCheckedRange reader(is);

    mfxU32 unused                                           = reader.GetBit(); // forbiddenZeroBit
    std::ignore = unused;

    sps.nalRefIdc                                           = reader.GetBits(2);
    if (sps.nalRefIdc == 0)
        throw InvalidBitstream();

    sps.nalUnitType                                         = reader.GetBits(5);
    if (sps.nalUnitType != 7)
        throw InvalidBitstream();

    sps.profileIdc                                          = reader.GetBits(8);
    sps.constraints.set0                                    = reader.GetBit();
    sps.constraints.set1                                    = reader.GetBit();
    sps.constraints.set2                                    = reader.GetBit();
    sps.constraints.set3                                    = reader.GetBit();
    sps.constraints.set4                                    = reader.GetBit();
    sps.constraints.set5                                    = reader.GetBit();
    sps.constraints.set6                                    = reader.GetBit();
    sps.constraints.set7                                    = reader.GetBit();
    sps.levelIdc                                            = reader.GetBits(8);
    sps.seqParameterSetId                                   = reader.GetUe();

    if (sps.profileIdc == 100 || sps.profileIdc == 110 || sps.profileIdc == 122 ||
        sps.profileIdc == 244 || sps.profileIdc ==  44 || sps.profileIdc ==  83 ||
        sps.profileIdc ==  86 || sps.profileIdc == 118 || sps.profileIdc == 128)
    {
        sps.chromaFormatIdc                                 = reader.GetUe();
        if (sps.chromaFormatIdc == 3)
            unused                                          = reader.GetBit(); // separateColourPlaneFlag

        sps.bitDepthLumaMinus8                              = reader.GetUe();
        sps.bitDepthChromaMinus8                            = reader.GetUe();

        sps.qpprimeYZeroTransformBypassFlag                 = reader.GetBit();
        sps.seqScalingMatrixPresentFlag                     = reader.GetBit();

        if (sps.seqScalingMatrixPresentFlag)
        {
            for (mfxU32 i = 0; i < ((sps.chromaFormatIdc != 3) ? 8u : 12u); i++)
            {
                sps.seqScalingListPresentFlag[i]            = reader.GetBit();
                if (sps.seqScalingListPresentFlag[i])
                {
                    (i < 6)
                        ? ReadScalingList(reader, sps.scalingList4x4[i],     16)
                        : ReadScalingList(reader, sps.scalingList8x8[i - 6], 64);
                }
            }
        }
    }

    sps.log2MaxFrameNumMinus4                               = reader.GetUe();
    sps.picOrderCntType                                     = reader.GetUe();

    if (sps.picOrderCntType == 0)
    {
        sps.log2MaxPicOrderCntLsbMinus4                     = reader.GetUe();
    }
    else if (sps.picOrderCntType == 1)
    {
        sps.deltaPicOrderAlwaysZeroFlag                     = reader.GetBit();
        sps.offsetForNonRefPic                              = reader.GetSe();
        sps.offsetForTopToBottomField                       = reader.GetSe();
        sps.numRefFramesInPicOrderCntCycle                  = reader.GetUe();

        for (mfxU32 i = 0; i < sps.numRefFramesInPicOrderCntCycle; i++)
            sps.offsetForRefFrame[i]                        = reader.GetSe();
    }

    sps.maxNumRefFrames                                     = reader.GetUe();
    sps.gapsInFrameNumValueAllowedFlag                      = reader.GetBit();
    sps.picWidthInMbsMinus1                                 = reader.GetUe();
    sps.picHeightInMapUnitsMinus1                           = reader.GetUe();
    sps.frameMbsOnlyFlag                                    = reader.GetBit();

    if (!sps.frameMbsOnlyFlag)
        sps.mbAdaptiveFrameFieldFlag                        = reader.GetBit();

    sps.direct8x8InferenceFlag                              = reader.GetBit();
    sps.frameCroppingFlag                                   = reader.GetBit();

    if (sps.frameCroppingFlag)
    {
        sps.frameCropLeftOffset                             = reader.GetUe();
        sps.frameCropRightOffset                            = reader.GetUe();
        sps.frameCropTopOffset                              = reader.GetUe();
        sps.frameCropBottomOffset                           = reader.GetUe();
    }

    sps.vuiParametersPresentFlag                            = reader.GetBit();
    if (sps.vuiParametersPresentFlag)
    {
        sps.vui.flags.aspectRatioInfoPresent                = reader.GetBit();
        if (sps.vui.flags.aspectRatioInfoPresent)
        {
            sps.vui.aspectRatioIdc                          = reader.GetBits(8);
            if (sps.vui.aspectRatioIdc == 255)
            {
                sps.vui.sarWidth                            = reader.GetBits(16);
                sps.vui.sarHeight                           = reader.GetBits(16);
            }
        }

        sps.vui.flags.overscanInfoPresent                   = reader.GetBit();
        if (sps.vui.flags.overscanInfoPresent)
            sps.vui.flags.overscanAppropriate               = reader.GetBit();

        sps.vui.flags.videoSignalTypePresent                = reader.GetBit();
        if (sps.vui.flags.videoSignalTypePresent)
        {
            sps.vui.videoFormat                             = reader.GetBits(3);
            sps.vui.flags.videoFullRange                    = reader.GetBit();
            sps.vui.flags.colourDescriptionPresent          = reader.GetBit();

            if (sps.vui.flags.colourDescriptionPresent)
            {
                sps.vui.colourPrimaries                     = reader.GetBits(8);
                sps.vui.transferCharacteristics             = reader.GetBits(8);
                sps.vui.matrixCoefficients                  = reader.GetBits(8);
            }
        }

        sps.vui.flags.chromaLocInfoPresent                  = reader.GetBit();
        if (sps.vui.flags.chromaLocInfoPresent)
        {
            sps.vui.chromaSampleLocTypeTopField             = reader.GetUe();
            sps.vui.chromaSampleLocTypeBottomField          = reader.GetUe();
        }

        sps.vui.flags.timingInfoPresent                     = reader.GetBit();
        if (sps.vui.flags.timingInfoPresent)
        {
            sps.vui.numUnitsInTick                          = reader.GetBits(32);
            sps.vui.timeScale                               = reader.GetBits(32);
            sps.vui.flags.fixedFrameRate                    = reader.GetBit();
        }

        sps.vui.flags.nalHrdParametersPresent               = reader.GetBit();
        if (sps.vui.flags.nalHrdParametersPresent)
            ReadHrdParameters(reader, sps.vui.nalHrdParameters);

        sps.vui.flags.vclHrdParametersPresent               = reader.GetBit();
        if (sps.vui.flags.vclHrdParametersPresent)
            ReadHrdParameters(reader, sps.vui.vclHrdParameters);

        if (sps.vui.flags.nalHrdParametersPresent || sps.vui.flags.vclHrdParametersPresent)
            sps.vui.flags.lowDelayHrd                       = reader.GetBit();

        sps.vui.flags.picStructPresent                      = reader.GetBit();
        sps.vui.flags.bitstreamRestriction                  = reader.GetBit();

        if (sps.vui.flags.bitstreamRestriction)
        {
            sps.vui.flags.motionVectorsOverPicBoundaries    = reader.GetBit();
            sps.vui.maxBytesPerPicDenom                     = reader.GetUe();
            sps.vui.maxBitsPerMbDenom                       = reader.GetUe();
            sps.vui.log2MaxMvLengthHorizontal               = reader.GetUe();
            sps.vui.log2MaxMvLengthVertical                 = reader.GetUe();
            sps.vui.numReorderFrames                        = reader.GetUe();
            sps.vui.maxDecFrameBuffering                    = reader.GetUe();
        }
    }
}

mfxU8 MfxHwH264Encode::ReadSpsIdOfPpsHeader(
    InputBitstream is)
{
    InputBitstreamCheckedRange reader(is);
    /*mfxU8 ppsId = */reader.GetUe();

    mfxU8 spsId = reader.GetUe();
    if (spsId > 31)
        throw InvalidBitstream();

    return spsId;
}

void MfxHwH264Encode::ReadPpsHeader(
    InputBitstream &        is,
    mfxExtSpsHeader const & sps,
    mfxExtPpsHeader &       pps)
{
    InputBitstreamCheckedRange reader(is);

    mfxU32 unused                                               = reader.GetBit(); // forbiddenZeroBit
    std::ignore = unused;

    pps.nalRefIdc                                               = reader.GetBits(2);
    if (pps.nalRefIdc == 0)
        throw InvalidBitstream();

    mfxU8 nalUnitType                                           = reader.GetBits(5);
    if (nalUnitType != 8)
        throw InvalidBitstream();

    pps.picParameterSetId                                       = reader.GetUe();
    pps.seqParameterSetId                                       = reader.GetUe();

    if (pps.seqParameterSetId != sps.seqParameterSetId)
        throw InvalidBitstream();

    pps.entropyCodingModeFlag                                   = reader.GetBit();

    pps.bottomFieldPicOrderInframePresentFlag                   = reader.GetBit();;

    pps.numSliceGroupsMinus1                                    = reader.GetUe();
    if (pps.numSliceGroupsMinus1 > 0)
    {
        if (pps.numSliceGroupsMinus1 > 7)
            throw InvalidBitstream();

        pps.sliceGroupMapType                                   = reader.GetUe();
        if (pps.sliceGroupMapType == 0)
        {
            for (mfxU32 i = 0; i <= pps.numSliceGroupsMinus1; i++)
                pps.sliceGroupInfo.t0.runLengthMinus1[i]        = reader.GetUe();
        }
        else if (pps.sliceGroupMapType == 2)
        {
            for (mfxU32 i = 0; i < pps.numSliceGroupsMinus1; i++)
            {
                pps.sliceGroupInfo.t2.topLeft[i]                = reader.GetUe();
                pps.sliceGroupInfo.t2.bottomRight[i]            = reader.GetUe();
            }
        }
        else if (
            pps.sliceGroupMapType == 3 ||
            pps.sliceGroupMapType == 4 ||
            pps.sliceGroupMapType == 5)
        {
            pps.sliceGroupInfo.t3.sliceGroupChangeDirectionFlag = reader.GetBit();
            pps.sliceGroupInfo.t3.sliceGroupChangeRate          = reader.GetUe();
        }
        else if (pps.sliceGroupMapType == 6)
        {
            pps.sliceGroupInfo.t6.picSizeInMapUnitsMinus1       = reader.GetUe();
            for (mfxU32 i = 0; i <= pps.sliceGroupInfo.t6.picSizeInMapUnitsMinus1; i++)
                unused                                          = reader.GetBits(CeilLog2(pps.numSliceGroupsMinus1 + 1)); // sliceGroupId
        }
    }

    pps.numRefIdxL0DefaultActiveMinus1                          = reader.GetUe();
    pps.numRefIdxL1DefaultActiveMinus1                          = reader.GetUe();

    pps.weightedPredFlag                                        = reader.GetBit();
    pps.weightedBipredIdc                                       = reader.GetBits(2);
    pps.picInitQpMinus26                                        = reader.GetSe();
    pps.picInitQsMinus26                                        = reader.GetSe();
    pps.chromaQpIndexOffset                                     = reader.GetSe();;

    pps.deblockingFilterControlPresentFlag                      = reader.GetBit();
    pps.constrainedIntraPredFlag                                = reader.GetBit();
    pps.redundantPicCntPresentFlag                              = reader.GetBit();

    pps.moreRbspData = MoreRbspData(is);
    if (pps.moreRbspData)
    {
        pps.transform8x8ModeFlag                                = reader.GetBit();
        pps.picScalingMatrixPresentFlag                         = reader.GetBit();
        if (pps.picScalingMatrixPresentFlag)
        {
            for (mfxU32 i = 0; i < 6 + ((sps.chromaFormatIdc != 3) ? 2u : 6u) * pps.transform8x8ModeFlag; i++)
            {
                mfxU32 picScalingListPresentFlag                = reader.GetBit();
                if (picScalingListPresentFlag)
                {
                    (i < 6)
                        ? ReadScalingList(reader, pps.scalingList4x4[i],     16)
                        : ReadScalingList(reader, pps.scalingList8x8[i - 6], 64);
                }
            }
        }

        pps.secondChromaQpIndexOffset                           = reader.GetSe();
    }
}

namespace
{
    void WriteSpsData(
        OutputBitstream &       writer,
        mfxExtSpsHeader const & sps)
    {
        writer.PutBits(sps.profileIdc, 8);
        writer.PutBit(sps.constraints.set0);
        writer.PutBit(sps.constraints.set1);
        writer.PutBit(sps.constraints.set2);
        writer.PutBit(sps.constraints.set3);
        writer.PutBit(sps.constraints.set4);
        writer.PutBit(sps.constraints.set5);
        writer.PutBit(sps.constraints.set6);
        writer.PutBit(sps.constraints.set7);
        writer.PutBits(sps.levelIdc, 8);
        writer.PutUe(sps.seqParameterSetId);
        if (sps.profileIdc == 100 || sps.profileIdc == 110 || sps.profileIdc == 122 ||
            sps.profileIdc == 244 || sps.profileIdc ==  44 || sps.profileIdc ==  83 ||
            sps.profileIdc ==  86 || sps.profileIdc == 118 || sps.profileIdc == 128)
        {
            writer.PutUe(sps.chromaFormatIdc);
            if (sps.chromaFormatIdc == 3)
                writer.PutBit(sps.separateColourPlaneFlag);
            writer.PutUe(sps.bitDepthLumaMinus8);
            writer.PutUe(sps.bitDepthChromaMinus8);
            writer.PutBit(sps.qpprimeYZeroTransformBypassFlag);
            writer.PutBit(sps.seqScalingMatrixPresentFlag);
            if (sps.seqScalingMatrixPresentFlag)
            {
#ifdef MFX_ENABLE_AVC_CUSTOM_QMATRIX
                for (mfxU8 i = 0; i < ((sps.levelIdc != 3) ? 8 : 12); ++i)
                {
                     //Put scaling list present flag
                     writer.PutBit(sps.seqScalingListPresentFlag[i]);
                     if (sps.seqScalingListPresentFlag[i])
                     {
                         if (i < 6)
                             WriteScalingList(writer, &sps.scalingList4x4[i][0], 16);
                        else
                             WriteScalingList(writer, &sps.scalingList8x8[i - 6][0], 64);
                     }
                }
#else
                assert("seq_scaling_matrix is unsupported");
#endif
            }
        }
        writer.PutUe(sps.log2MaxFrameNumMinus4);
        writer.PutUe(sps.picOrderCntType);
        if (sps.picOrderCntType == 0)
        {
            writer.PutUe(sps.log2MaxPicOrderCntLsbMinus4);
        }
        else if (sps.picOrderCntType == 1)
        {
            writer.PutBit(sps.deltaPicOrderAlwaysZeroFlag);
            writer.PutSe(sps.offsetForNonRefPic);
            writer.PutSe(sps.offsetForTopToBottomField);
            writer.PutUe(sps.numRefFramesInPicOrderCntCycle);
            for (mfxU32 i = 0; i < sps.numRefFramesInPicOrderCntCycle; i++)
                writer.PutSe(sps.offsetForRefFrame[i]);
        }
        writer.PutUe(sps.maxNumRefFrames);
        writer.PutBit(sps.gapsInFrameNumValueAllowedFlag);
        writer.PutUe(sps.picWidthInMbsMinus1);
        writer.PutUe(sps.picHeightInMapUnitsMinus1);
        writer.PutBit(sps.frameMbsOnlyFlag);
        if (!sps.frameMbsOnlyFlag)
            writer.PutBit(sps.mbAdaptiveFrameFieldFlag);
        writer.PutBit(sps.direct8x8InferenceFlag);
        writer.PutBit(sps.frameCroppingFlag);
        if (sps.frameCroppingFlag)
        {
            writer.PutUe(sps.frameCropLeftOffset);
            writer.PutUe(sps.frameCropRightOffset);
            writer.PutUe(sps.frameCropTopOffset);
            writer.PutUe(sps.frameCropBottomOffset);
        }
        writer.PutBit(sps.vuiParametersPresentFlag);
        if (sps.vuiParametersPresentFlag)
        {
            writer.PutBit(sps.vui.flags.aspectRatioInfoPresent);
            if (sps.vui.flags.aspectRatioInfoPresent)
            {
                writer.PutBits(sps.vui.aspectRatioIdc, 8);
                if (sps.vui.aspectRatioIdc == 255)
                {
                    writer.PutBits(sps.vui.sarWidth, 16);
                    writer.PutBits(sps.vui.sarHeight, 16);
                }
            }
            writer.PutBit(sps.vui.flags.overscanInfoPresent);
            if (sps.vui.flags.overscanInfoPresent)
                writer.PutBit(sps.vui.flags.overscanAppropriate);
            writer.PutBit(sps.vui.flags.videoSignalTypePresent);
            if (sps.vui.flags.videoSignalTypePresent)
            {
                writer.PutBits(sps.vui.videoFormat, 3);
                writer.PutBit(sps.vui.flags.videoFullRange);
                writer.PutBit(sps.vui.flags.colourDescriptionPresent);
                if (sps.vui.flags.colourDescriptionPresent)
                {
                    writer.PutBits(sps.vui.colourPrimaries, 8);
                    writer.PutBits(sps.vui.transferCharacteristics, 8);
                    writer.PutBits(sps.vui.matrixCoefficients, 8);
                }
            }
            writer.PutBit(sps.vui.flags.chromaLocInfoPresent);
            if (sps.vui.flags.chromaLocInfoPresent)
            {
                writer.PutUe(sps.vui.chromaSampleLocTypeTopField);
                writer.PutUe(sps.vui.chromaSampleLocTypeBottomField);
            }
            writer.PutBit(sps.vui.flags.timingInfoPresent);
            if (sps.vui.flags.timingInfoPresent)
            {
                writer.PutBits(sps.vui.numUnitsInTick, 32);
                writer.PutBits(sps.vui.timeScale, 32);
                writer.PutBit(sps.vui.flags.fixedFrameRate);
            }
            writer.PutBit(sps.vui.flags.nalHrdParametersPresent);
            if (sps.vui.flags.nalHrdParametersPresent)
                WriteHrdParameters(writer, sps.vui.nalHrdParameters);
            writer.PutBit(sps.vui.flags.vclHrdParametersPresent);
            if (sps.vui.flags.vclHrdParametersPresent)
                WriteHrdParameters(writer, sps.vui.vclHrdParameters);
            if (sps.vui.flags.nalHrdParametersPresent || sps.vui.flags.vclHrdParametersPresent)
                writer.PutBit(sps.vui.flags.lowDelayHrd);
            writer.PutBit(sps.vui.flags.picStructPresent);
            writer.PutBit(sps.vui.flags.bitstreamRestriction);
            if (sps.vui.flags.bitstreamRestriction)
            {
                writer.PutBit(sps.vui.flags.motionVectorsOverPicBoundaries);
                writer.PutUe(sps.vui.maxBytesPerPicDenom);
                writer.PutUe(sps.vui.maxBitsPerMbDenom);
                writer.PutUe(sps.vui.log2MaxMvLengthHorizontal);
                writer.PutUe(sps.vui.log2MaxMvLengthVertical);
                writer.PutUe(sps.vui.numReorderFrames);
                writer.PutUe(sps.vui.maxDecFrameBuffering);
            }
        }
    }

    void WriteSpsMvcExtension(
        OutputBitstream &        writer,
        mfxExtMVCSeqDesc const & extMvc)
    {
        writer.PutUe(extMvc.NumView - 1);                       // num_views_minus1
        for (mfxU32 i = 0; i < extMvc.NumView; i++)
            writer.PutUe(extMvc.View[i].ViewId);                // view_id[i]
        for (mfxU32 i = 1; i < extMvc.NumView; i++)
        {
            writer.PutUe(extMvc.View[i].NumAnchorRefsL0);       // num_anchor_refs_l0[i]
            for (mfxU32 j = 0; j < extMvc.View[i].NumAnchorRefsL0; j++)
                writer.PutUe(extMvc.View[i].AnchorRefL0[j]);    // anchor_ref_l0[i][j]
            writer.PutUe(extMvc.View[i].NumAnchorRefsL1);       // num_anchor_refs_l1[i]
            for (mfxU32 j = 0; j < extMvc.View[i].NumAnchorRefsL1; j++)
                writer.PutUe(extMvc.View[i].AnchorRefL1[j]);    // anchor_ref_l1[i][j]
        }
        for (mfxU32 i = 1; i < extMvc.NumView; i++)
        {
            writer.PutUe(extMvc.View[i].NumNonAnchorRefsL0);    // num_non_anchor_refs_l0[i]
            for (mfxU32 j = 0; j < extMvc.View[i].NumNonAnchorRefsL0; j++)
                writer.PutUe(extMvc.View[i].NonAnchorRefL0[j]); // non_anchor_ref_l0[i][j]
            writer.PutUe(extMvc.View[i].NumNonAnchorRefsL1);    // num_non_anchor_refs_l1[i]
            for (mfxU32 j = 0; j < extMvc.View[i].NumNonAnchorRefsL1; j++)
                writer.PutUe(extMvc.View[i].NonAnchorRefL1[j]); // non_anchor_ref_l1[i][j]
        }
        writer.PutUe(extMvc.NumOP - 1);                         // num_level_values_signalled_minus1
        for (mfxU32 i = 0; i < extMvc.NumOP; i++)
        {
            writer.PutBits(extMvc.OP[i].LevelIdc, 8);           // level_idc[i]
            writer.PutUe(0);                                    // num_applicable_ops_minus1[i]
            for (mfxU32 j = 0; j < 1; j++)
            {
                writer.PutBits(extMvc.OP[i].TemporalId, 3);     // applicable_op_temporal_id[i][j]
                writer.PutUe(extMvc.OP[i].NumTargetViews - 1);  // applicable_op_num_target_views_minus1[i][j]
                for (mfxU32 k = 0; k < extMvc.OP[i].NumTargetViews; k++)
                    writer.PutUe(extMvc.OP[i].TargetViewId[k]); // applicable_op_target_view_id[i][j][k]
                writer.PutUe(extMvc.OP[i].NumViews - 1);        // applicable_op_num_views_minus1[i][j]
            }
        }
    }

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    void WriteSpsSvcExtension(
        OutputBitstream &          writer,
        mfxExtSpsHeader const &    sps,
        mfxExtSpsSvcHeader const & extSvc)
    {
        mfxU32 chromaArrayType = sps.separateColourPlaneFlag ? 0 : sps.chromaFormatIdc;

        writer.PutBit(extSvc.interLayerDeblockingFilterControlPresentFlag); // inter_layer_deblocking_filter_control_present_flag
        writer.PutBits(extSvc.extendedSpatialScalabilityIdc, 2);            // extended_spatial_scalability_idc
        if (chromaArrayType == 1 || chromaArrayType == 2)
            writer.PutBit(extSvc.chromaPhaseXPlus1Flag);                    // chroma_phase_x_plus1_flag
        if (chromaArrayType == 1)
            writer.PutBits(extSvc.chromaPhaseYPlus1, 2);                    // chroma_phase_y_plus1
        if (extSvc.extendedSpatialScalabilityIdc == 1)
        {
            if (chromaArrayType > 0)
            {
                writer.PutBit(extSvc.seqRefLayerChromaPhaseXPlus1Flag);     // seq_ref_layer_chroma_phase_x_plus1_flag
                writer.PutBits(extSvc.seqRefLayerChromaPhaseYPlus1, 2);     // seq_ref_layer_chroma_phase_y_plus1
            }
            writer.PutSe(extSvc.seqScaledRefLayerLeftOffset);               // seq_scaled_ref_layer_left_offset
            writer.PutSe(extSvc.seqScaledRefLayerTopOffset);                // seq_scaled_ref_layer_top_offset
            writer.PutSe(extSvc.seqScaledRefLayerRightOffset);              // seq_scaled_ref_layer_right_offset
            writer.PutSe(extSvc.seqScaledRefLayerBottomOffset);             // seq_scaled_ref_layer_bottom_offset
        }
        writer.PutBit(extSvc.seqTcoeffLevelPredictionFlag);                 // seq_tcoeff_level_prediction_flag
        if (extSvc.seqTcoeffLevelPredictionFlag)
            writer.PutBit(extSvc.adaptiveTcoeffLevelPredictionFlag);        // adaptive_tcoeff_level_prediction_flag
        writer.PutBit(extSvc.sliceHeaderRestrictionFlag);                   // slice_header_restriction_flag
    }
#endif // #ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
}

mfxU32 MfxHwH264Encode::WriteSpsHeader(
    OutputBitstream &       writer,
    mfxExtSpsHeader const & sps)
{
    mfxU32 initNumBits = writer.GetNumBits();

    const mfxU8 header[4] = { 0, 0, 0, 1 };
    writer.PutRawBytes(header, header + sizeof header / sizeof header[0]);

    writer.PutBit(0); // forbiddenZeroBit
    writer.PutBits(sps.nalRefIdc, 2);
    writer.PutBits(NALU_SPS, 5); // nalUnitType
    WriteSpsData(writer, sps);
    writer.PutTrailingBits();

    return writer.GetNumBits() - initNumBits;
}

mfxU32 MfxHwH264Encode::WriteSpsHeader(
    OutputBitstream &        writer,
    mfxExtSpsHeader const &  sps,
    mfxExtBuffer const &     spsExt)
{
    mfxU32 initNumBits = writer.GetNumBits();

    const mfxU8 header[4] = { 0, 0, 0, 1 };
    writer.PutRawBytes(header, header + sizeof header / sizeof header[0]);

    writer.PutBit(0);                   // forbidden_zero_bit
    writer.PutBits(sps.nalRefIdc, 2);   // nal_ref_idc
    writer.PutBits(sps.nalUnitType, 5); // nal_unit_type

    WriteSpsData(writer, sps);

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    if (IsSvcProfile(sps.profileIdc))
    {
        assert(spsExt.BufferId == MFX_EXTBUFF_SPS_SVC_HEADER);
        mfxExtSpsSvcHeader const & extSvc = (mfxExtSpsSvcHeader const &)spsExt;

        WriteSpsSvcExtension(writer, sps, extSvc);
        writer.PutBit(0);               // svc_vui_parameters_present_flag
    }
    else
#endif
    if (IsMvcProfile(sps.profileIdc))
    {
        assert(spsExt.BufferId == MFX_EXTBUFF_MVC_SEQ_DESC);
        mfxExtMVCSeqDesc const & extMvc = (mfxExtMVCSeqDesc const &)spsExt;

        writer.PutBit(1);               // bit_equal_to_one
        WriteSpsMvcExtension(writer, extMvc);
        writer.PutBit(0);               // mvc_vui_parameters_present_flag
    }
    writer.PutBit(0);                   // additional_extension2_flag
    writer.PutTrailingBits();

    return writer.GetNumBits() - initNumBits;
}

mfxU32 MfxHwH264Encode::WritePpsHeader(
    OutputBitstream &       writer,
    mfxExtPpsHeader const & pps)
{
    mfxU32 initNumBits = writer.GetNumBits();

    const mfxU8 header[4] = { 0, 0, 0, 1 };
    writer.PutRawBytes(header, header + sizeof header / sizeof header[0]);

    writer.PutBit(0); // forbiddenZeroBit
    writer.PutBits(pps.nalRefIdc, 2);
    writer.PutBits(NALU_PPS, 5); // nalUnitType
    writer.PutUe(pps.picParameterSetId);
    writer.PutUe(pps.seqParameterSetId);
    writer.PutBit(pps.entropyCodingModeFlag);
    writer.PutBit(pps.bottomFieldPicOrderInframePresentFlag);
    writer.PutUe(pps.numSliceGroupsMinus1);
    if (pps.numSliceGroupsMinus1 > 0)
    {
        writer.PutUe(pps.sliceGroupMapType);
        if (pps.sliceGroupMapType == 0)
        {
            for (mfxU32 i = 0; i <= pps.numSliceGroupsMinus1; i++)
                writer.PutUe(pps.sliceGroupInfo.t0.runLengthMinus1[i]);
        }
        else if (pps.sliceGroupMapType == 2)
        {
            for (mfxU32 i = 0; i < pps.numSliceGroupsMinus1; i++)
            {
                writer.PutUe(pps.sliceGroupInfo.t2.topLeft[i]);
                writer.PutUe(pps.sliceGroupInfo.t2.bottomRight[i]);
            }
        }
        else if (
            pps.sliceGroupMapType == 3 ||
            pps.sliceGroupMapType == 4 ||
            pps.sliceGroupMapType == 5)
        {
            writer.PutBit(pps.sliceGroupInfo.t3.sliceGroupChangeDirectionFlag);
            writer.PutUe(pps.sliceGroupInfo.t3.sliceGroupChangeRate);
        }
        else if (pps.sliceGroupMapType == 6)
        {
            writer.PutUe(pps.sliceGroupInfo.t6.picSizeInMapUnitsMinus1);
            assert("unsupprted slice_group_map_type = 6");
            for (mfxU32 i = 0; i <= pps.sliceGroupInfo.t6.picSizeInMapUnitsMinus1; i++)
                writer.PutBits(1, CeilLog2(pps.numSliceGroupsMinus1 + 1));
        }
    }
    writer.PutUe(pps.numRefIdxL0DefaultActiveMinus1);
    writer.PutUe(pps.numRefIdxL1DefaultActiveMinus1);
    writer.PutBit(pps.weightedPredFlag);
    writer.PutBits(pps.weightedBipredIdc, 2);
    writer.PutSe(pps.picInitQpMinus26);
    writer.PutSe(pps.picInitQsMinus26);
    writer.PutSe(pps.chromaQpIndexOffset);
    writer.PutBit(pps.deblockingFilterControlPresentFlag);
    writer.PutBit(pps.constrainedIntraPredFlag);
    writer.PutBit(pps.redundantPicCntPresentFlag);
    if (pps.moreRbspData)
    {
        writer.PutBit(pps.transform8x8ModeFlag);
        writer.PutBit(pps.picScalingMatrixPresentFlag);
        if (pps.picScalingMatrixPresentFlag)
        {
            //for(int i=0; i < 6 + ((sps.chroma_format_idc != 3) ? 2:6)*pps.transform8x8ModeFlag; i++){
            for(int i=0; i < 6+2*(!!pps.transform8x8ModeFlag); i++){
                //Put scaling list present flag
                writer.PutBit(pps.picScalingListPresentFlag[i]);
                if( pps.picScalingListPresentFlag[i] ){
                   if( i<6 )
                       WriteScalingList(writer, &pps.scalingList4x4[i][0], 16);
                   else
                       WriteScalingList(writer, &pps.scalingList8x8[i-6][0], 64);
                }
            }
        }
        writer.PutSe(pps.secondChromaQpIndexOffset);
    }
    writer.PutTrailingBits();

    return writer.GetNumBits() - initNumBits;
}

void MfxHwH264Encode::WriteScalingList(
    OutputBitstream &       writer,
    const mfxU8* scalingList,
    mfxI32 sizeOfScalingList)
{
    int16_t lastScale, nextScale;
    int32_t j;

    int16_t delta_scale;
    const int32_t* scan;

    lastScale=nextScale=8;

    if( sizeOfScalingList == 16 )
        scan = UMC_H264_ENCODER::dec_single_scan[0];
    else
        scan = UMC_H264_ENCODER::dec_single_scan_8x8[0];

    for( j = 0; j<sizeOfScalingList; j++ ){
         if( nextScale != 0 ){
            delta_scale = (int16_t)(scalingList[scan[j]]-lastScale);
            writer.PutSe(delta_scale);
            nextScale = scalingList[scan[j]];
         }
         lastScale = (nextScale==0) ? lastScale:nextScale;
    }
}

void MfxHwH264Encode::WriteRefPicListModification(
    OutputBitstream &       writer,
    ArrayRefListMod const & refListMod)
{
    writer.PutBit(refListMod.Size() > 0);       // ref_pic_list_modification_flag_l0
    if (refListMod.Size() > 0)
    {
        for (mfxU32 i = 0; i < refListMod.Size(); i++)
        {
            writer.PutUe(refListMod[i].m_idc);  // modification_of_pic_nums_idc
            writer.PutUe(refListMod[i].m_diff); // abs_diff_pic_num_minus1 or
                                                // long_term_pic_num or
                                                // abs_diff_view_idx_minus1
        }

        writer.PutUe(RPLM_END);
    }
}

void MfxHwH264Encode::WriteDecRefPicMarking(
    OutputBitstream &            writer,
    DecRefPicMarkingInfo const & marking,
    mfxU32                       idrPicFlag)
{
    if (idrPicFlag)
    {
        writer.PutBit(marking.no_output_of_prior_pics_flag);    // no_output_of_prior_pics_flag
        writer.PutBit(marking.long_term_reference_flag);        // long_term_reference_flag
    }
    else
    {
        writer.PutBit(marking.mmco.Size() > 0);                 // adaptive_ref_pic_marking_mode_flag
        if (marking.mmco.Size())
        {
            for (mfxU32 i = 0; i < marking.mmco.Size(); i++)
            {
                writer.PutUe(marking.mmco[i]);                  // memory_management_control_operation
                writer.PutUe(marking.value[2 * i]);             // difference_of_pic_nums_minus1 or
                                                                // long_term_pic_num or
                                                                // long_term_frame_idx or
                                                                // max_long_term_frame_idx_plus1
                if (marking.mmco[i] == MMCO_ST_TO_LT)
                    writer.PutUe(marking.value[2 * i + 1]);     // long_term_frame_idx
            }

            writer.PutUe(MMCO_END);
        }
    }
}

mfxU32 MfxHwH264Encode::WriteAud(
    OutputBitstream & writer,
    mfxU32            frameType)
{
    mfxU32 initNumBits = writer.GetNumBits();

    mfxU8 const header[4] = { 0, 0, 0, 1 };
    writer.PutRawBytes(header, header + sizeof header / sizeof header[0]);
    writer.PutBit(0);
    writer.PutBits(0, 2);
    writer.PutBits(NALU_AUD, 5);
    writer.PutBits(ConvertFrameTypeMfx2Ddi(frameType) - 1, 3);
    writer.PutTrailingBits();

    return writer.GetNumBits() - initNumBits;
}

bool MfxHwH264Encode::IsAvcProfile(mfxU32 profile)
{
    return
        IsAvcBaseProfile(profile)           ||
        profile == MFX_PROFILE_AVC_MAIN     ||
        profile == MFX_PROFILE_AVC_EXTENDED ||
        IsAvcHighProfile(profile);
}

bool MfxHwH264Encode::IsAvcBaseProfile(mfxU32 profile)
{
    return
        profile == MFX_PROFILE_AVC_BASELINE ||
        profile == MFX_PROFILE_AVC_CONSTRAINED_BASELINE;
}

bool MfxHwH264Encode::IsAvcHighProfile(mfxU32 profile)
{
    return
        profile == MFX_PROFILE_AVC_HIGH ||
        profile == MFX_PROFILE_AVC_CONSTRAINED_HIGH ||
        profile == MFX_PROFILE_AVC_PROGRESSIVE_HIGH;
}

bool MfxHwH264Encode::IsMvcProfile(mfxU32 profile)
{
    return
        profile == MFX_PROFILE_AVC_STEREO_HIGH ||
        profile == MFX_PROFILE_AVC_MULTIVIEW_HIGH;
}

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
bool MfxHwH264Encode::IsSvcProfile(mfxU32 profile)
{
    return
        profile == MFX_PROFILE_AVC_SCALABLE_BASELINE ||
        profile == MFX_PROFILE_AVC_SCALABLE_HIGH;
}
#endif

bool MfxHwH264Encode::operator ==(
    mfxExtSpsHeader const & lhs,
    mfxExtSpsHeader const & rhs)
{
    // part from the beginning of sps to nalHrdParameters
    mfxU8 const * lhsBegin1 = (mfxU8 const *)&lhs;
    mfxU8 const * rhsBegin1 = (mfxU8 const *)&rhs;
    mfxU8 const * lhsEnd1   = (mfxU8 const *)&lhs.vui.nalHrdParameters;

    // part from the end of vclHrdParameters to the end of sps
    mfxU8 const * lhsBegin2 = (mfxU8 const *)&lhs.vui.maxBytesPerPicDenom;
    mfxU8 const * rhsBegin2 = (mfxU8 const *)&rhs.vui.maxBytesPerPicDenom;
    mfxU8 const * lhsEnd2   = (mfxU8 const *)&lhs + sizeof(lhs);

    if (memcmp(lhsBegin1, rhsBegin1, lhsEnd1 - lhsBegin1) != 0 ||
        memcmp(lhsBegin2, rhsBegin2, lhsEnd2 - lhsBegin2) != 0)
        return false;

    if (lhs.vui.flags.nalHrdParametersPresent)
        if (!Equal(lhs.vui.nalHrdParameters, rhs.vui.nalHrdParameters))
            return false;

    if (lhs.vui.flags.vclHrdParametersPresent)
        if (!Equal(lhs.vui.vclHrdParameters, rhs.vui.vclHrdParameters))
            return false;

    return true;
}

mfxU8 * MfxHwH264Encode::PackPrefixNalUnitSvc(
    mfxU8 *         begin,
    mfxU8 *         end,
    bool            emulationControl,
    DdiTask const & task,
    mfxU32          fieldId,
    mfxU32          nalUnitType)
{
    OutputBitstream obs(begin, end, false);

    mfxU32 idrFlag   = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) ? 1 : 0;
    mfxU32 nalRefIdc = task.m_nalRefIdc[fieldId];

    mfxU32 useRefBasePicFlag = (task.m_type[fieldId] & MFX_FRAMETYPE_KEYPIC) ? 1 : 0;

    obs.PutBits(1, 24);                     // 001
    obs.PutBits(0, 1);                      // forbidden_zero_flag
    obs.PutBits(nalRefIdc, 2);              // nal_ref_idc
    obs.PutBits(nalUnitType, 5);            // nal_unit_type
    obs.PutBits(1, 1);                      // svc_extension_flag
    obs.PutBits(idrFlag, 1);                // idr_flag
    obs.PutBits(task.m_pid, 6);             // priority_id
    obs.PutBits(1, 1);                      // no_inter_layer_pred_flag
    obs.PutBits(task.m_did, 3);             // dependency_id
    obs.PutBits(task.m_qid, 4);             // quality_id
    obs.PutBits(task.m_tid, 3);             // temporal_id
    obs.PutBits(useRefBasePicFlag, 1);      // use_ref_base_pic_flag
    obs.PutBits(1, 1);                      // discardable_flag
    obs.PutBits(1, 1);                      // output_flag
    obs.PutBits(0x3, 2);                    // reserved_three_2bits

    OutputBitstream obs1(begin + obs.GetNumBits() / 8, end, emulationControl);
    if (nalRefIdc && nalUnitType == 14)
    {
        mfxU32 additional_prefix_nal_unit_extension_flag = 0;

        obs1.PutBit(task.m_storeRefBasePicFlag);
        if ((useRefBasePicFlag || task.m_storeRefBasePicFlag) && !idrFlag)
            WriteDecRefPicMarking(obs1, task.m_decRefPicMrk[fieldId], idrFlag);

        obs1.PutBit(additional_prefix_nal_unit_extension_flag);
        assert(additional_prefix_nal_unit_extension_flag == 0);
        obs1.PutTrailingBits();
    }

    return begin + obs.GetNumBits() / 8 + obs1.GetNumBits() / 8;
}

namespace
{
    ENCODE_PACKEDHEADER_DATA MakePackedByteBuffer(mfxU8 * data, mfxU32 size, mfxU32 skipEmulCount)
    {
        ENCODE_PACKEDHEADER_DATA desc = {};
        desc.pData                  = data;
        desc.BufferSize             = size;
        desc.DataLength             = size;
        desc.SkipEmulationByteCount = skipEmulCount;
        return desc;
    }

    void PrepareSpsPpsHeaders(
        MfxVideoParam const &               par,
        std::vector<mfxExtSpsHeader> &      sps,
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
        std::vector<mfxExtSpsSvcHeader> &   subset,
#endif
        std::vector<mfxExtPpsHeader> &      pps)
    {
        mfxExtSpsHeader const & extSps = GetExtBufferRef(par);
        mfxExtPpsHeader const & extPps = GetExtBufferRef(par);

        mfxU16 numViews             = extSps.profileIdc == MFX_PROFILE_AVC_STEREO_HIGH ? 2 : 1;
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
        mfxU32 numDep               = par.calcParam.numDependencyLayer;
        mfxU32 firstDid             = par.calcParam.did[0];
        mfxU32 lastDid              = par.calcParam.did[numDep - 1];

        mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(par);
        mfxU32 numQualityAtLastDep  = extSvc->DependencyLayer[lastDid].QualityNum;
#endif
        mfxU16 heightMul = 2 - extSps.frameMbsOnlyFlag;

        // prepare sps for base layer
        sps[0] = extSps;
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
        if (IsSvcProfile(par.mfx.CodecProfile)) // force SPS id to 0 for SVC profile only. For other profiles should be able to encode custom SPS id
            sps[0].seqParameterSetId         = 0;

        sps[0].picWidthInMbsMinus1       = mfx::CeilDiv<mfxU16>(extSvc->DependencyLayer[firstDid].Width,  16) - 1;
        sps[0].picHeightInMapUnitsMinus1 = mfx::CeilDiv<mfxU16>(extSvc->DependencyLayer[firstDid].Height, 16) / heightMul - 1;
#else
        sps[0].picWidthInMbsMinus1       = mfx::CeilDiv<mfxU16>(par.mfx.FrameInfo.Width,  16) - 1;
        sps[0].picHeightInMapUnitsMinus1 = mfx::CeilDiv<mfxU16>(par.mfx.FrameInfo.Height, 16) / heightMul - 1;
#endif

        if (numViews > 1)
        {
            // MVC requires SPS and number of Subset SPS.
            // HeaderPacker prepares 2 SPS NAL units:
            // one for base view (sps_id = 0, profile_idc = 100)
            // and another for all other views (sps_id = 1, profile_idc = 128).
            // Second SPS will be re-packed to SubsetSPS after return from driver.
            for (mfxU16 view = 0; view < numViews; view++)
            {
                sps[view] = extSps;
                pps[view] = extPps;

                if (numViews > 1 && view == 0) // MVC base view
                    sps[view].profileIdc = MFX_PROFILE_AVC_HIGH;

                sps[view].seqParameterSetId = mfxU8(!!view) & 0x1f;
                pps[view].picParameterSetId = mfxU8(!!view);
                pps[view].seqParameterSetId = sps[view].seqParameterSetId;
            }
            return;
        }

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
        // prepare sps for enhanced spatial layers
        for (mfxU32 i = 0, spsidx = 1; i < numDep; i++, spsidx++)
        {
            mfxU32 did = par.calcParam.did[i];
            if (i == 0 && extSvc->DependencyLayer[did].QualityNum == 1)
                continue; // don't need a subset sps for did = 0

            sps[spsidx] = extSps;
            sps[spsidx].nalUnitType               = 15;
            sps[spsidx].profileIdc                = mfxU8(par.mfx.CodecProfile & MASK_PROFILE_IDC);
            sps[spsidx].seqParameterSetId         = mfxU8(i);
            sps[spsidx].picWidthInMbsMinus1       = mfx::CeilDiv<mfxU16>(extSvc->DependencyLayer[did].Width,  16) - 1;
            sps[spsidx].picHeightInMapUnitsMinus1 = mfx::CeilDiv<mfxU16>(extSvc->DependencyLayer[did].Height, 16) / heightMul - 1;

            InitExtBufHeader(subset[spsidx - 1]);
            subset[spsidx - 1].interLayerDeblockingFilterControlPresentFlag = 1;
            subset[spsidx - 1].extendedSpatialScalabilityIdc                = 1;
            subset[spsidx - 1].chromaPhaseXPlus1Flag                        = 0;
            subset[spsidx - 1].chromaPhaseYPlus1                            = 0;
            subset[spsidx - 1].seqRefLayerChromaPhaseXPlus1Flag             = 0;
            subset[spsidx - 1].seqRefLayerChromaPhaseYPlus1                 = 0;
            subset[spsidx - 1].seqScaledRefLayerLeftOffset                  = extSvc->DependencyLayer[did].ScaledRefLayerOffsets[0];
            subset[spsidx - 1].seqScaledRefLayerTopOffset                   = extSvc->DependencyLayer[did].ScaledRefLayerOffsets[1];
            subset[spsidx - 1].seqScaledRefLayerRightOffset                 = extSvc->DependencyLayer[did].ScaledRefLayerOffsets[2];
            subset[spsidx - 1].seqScaledRefLayerBottomOffset                = extSvc->DependencyLayer[did].ScaledRefLayerOffsets[3];
            subset[spsidx - 1].sliceHeaderRestrictionFlag                   = 0;
            subset[spsidx - 1].seqTcoeffLevelPredictionFlag                 = mfxU8(extSvc->DependencyLayer[did].QualityLayer[0].TcoeffPredictionFlag);
            subset[spsidx - 1].adaptiveTcoeffLevelPredictionFlag            = 0;
        }
#endif // #ifdef MFX_ENABLE_SVC_VIDEO_ENCODE

        pps[0] = extPps;

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
        // prepare pps for base and enhanced spatial layers
        for (mfxU32 i = 0; i < numDep; i++)
        {
            mfxU32 did = par.calcParam.did[i];
            mfxU32 simulcast =
                IsOff(extSvc->DependencyLayer[did].BasemodePred) &&
                IsOff(extSvc->DependencyLayer[did].MotionPred) &&
                IsOff(extSvc->DependencyLayer[did].ResidualPred) ? 1 : 0;

            pps[i] = extPps;
            if (IsSvcProfile(par.mfx.CodecProfile)) // force SPS/PPS id to 0 for SVC profile only. For other profiles should be able to encode custom SPS/PPS
            {
                pps[i].seqParameterSetId = mfxU8(i);
                pps[i].picParameterSetId = mfxU8(i);
            }

            pps[i].constrainedIntraPredFlag = ((i == numDep - 1 && numQualityAtLastDep == 1) || simulcast) ? 0 : 1;
        }

        // pack pps for enhanced quality layer of highest spatial layer if exists
        if (numQualityAtLastDep > 1)
        {
            pps.back() = extPps;
            pps.back().seqParameterSetId        = mfxU8(numDep - 1);
            pps.back().picParameterSetId        = mfxU8(numDep);
            pps.back().constrainedIntraPredFlag = 0;
        }
#endif // #ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    }
};

void HeaderPacker::Init(
    MfxVideoParam const &     par,
    MFX_ENCODE_CAPS const &   hwCaps,
    bool                      emulPrev)
{
    mfxExtCodingOptionDDI const & extDdi  = GetExtBufferRef(par);
    mfxExtSpsHeader const       & extSps  = GetExtBufferRef(par);
    mfxExtCodingOption2 const   & extOpt2 = GetExtBufferRef(par);

    mfxU16 numViews = extSps.profileIdc == MFX_PROFILE_AVC_STEREO_HIGH ? 2 : 1;

    mfxU32 numSpsHeaders = numViews;
    mfxU32 numPpsHeaders = numViews;

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    mfxExtSVCSeqDesc const *      extSvc = GetExtBuffer(par);
    mfxU32 numDep   = par.calcParam.numDependencyLayer;
    mfxU32 firstDid = par.calcParam.did[0];
    mfxU32 lastDid  = par.calcParam.did[numDep - 1];

    mfxU32 numQualityAtFirstDep = extSvc->DependencyLayer[firstDid].QualityNum;
    mfxU32 numQualityAtLastDep  = extSvc->DependencyLayer[lastDid].QualityNum;

    mfxU32 additionalSpsForFirstSpatialLayer = (numQualityAtFirstDep > 1);
    mfxU32 additionalPpsForLastSpatialLayer  = (numQualityAtLastDep > 1);

    numSpsHeaders = std::max<mfxU32>(numDep + additionalSpsForFirstSpatialLayer, numViews);
    numPpsHeaders = std::max<mfxU32>(numDep + additionalPpsForLastSpatialLayer,  numViews);
#endif // #ifdef MFX_ENABLE_SVC_VIDEO_ENCODE

    mfxU32 maxNumSlices = GetMaxNumSlices(par);

    m_sps.resize(numSpsHeaders);
    m_pps.resize(numPpsHeaders);
    m_packedSps.resize(numSpsHeaders);
    m_packedPps.resize(numPpsHeaders);
    m_packedSlices.resize(maxNumSlices);
    m_headerBuffer.resize(SPSPPS_BUFFER_SIZE);
    m_sliceBuffer.resize(SLICE_BUFFER_SIZE);

    Zero(m_sps);
    Zero(m_pps);

    Zero(m_packedAud);
    Zero(m_packedSps);
    Zero(m_packedPps);
    Zero(m_packedSlices);
    Zero(m_spsIdx);
    Zero(m_ppsIdx);
    Zero(m_refDqId);
    Zero(m_simulcast);

    m_emulPrev = emulPrev;
    m_isMVC = numViews > 1;

    m_numMbPerSlice = extOpt2.NumMbPerSlice;

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    m_subset.resize(numSpsHeaders / numViews - 1);
    Zero(m_subset);
    PrepareSpsPpsHeaders(par, m_sps, m_subset, m_pps);
#else
    PrepareSpsPpsHeaders(par, m_sps, m_pps);
#endif

#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
    const mfxExtCodingOption3 &extOpt3 = GetExtBufferRef(par);
    if (IsOn(extOpt3.AdaptiveCQM))
    {
        const std::vector<mfxExtPpsHeader> &extCqmPps = par.GetCqmPps();
        if (m_cqmPps.empty())
        {
            m_cqmPps.resize(extCqmPps.size());
            Zero(m_cqmPps);
        }
        if (m_packedCqmPps.empty())
        {
            m_packedCqmPps.resize(extCqmPps.size());
            Zero(m_packedCqmPps);
        }
        for (mfxU32 i = 0; i < extCqmPps.size(); i++)
            m_cqmPps[i] = extCqmPps[i];
    }
#endif

    // prepare data for slice level
#ifndef MFX_ENABLE_SVC_VIDEO_ENCODE
    m_needPrefixNalUnit       = (par.calcParam.numTemporalLayer > 0) && (par.mfx.LowPower != MFX_CODINGOPTION_ON);//LowPower limitation for temporal scalability we need to patch bitstream with SVC NAL after encoding
#else
    m_needPrefixNalUnit       = (IsSvcProfile(par.mfx.CodecProfile) || (par.calcParam.numTemporalLayer > 0)) && (par.mfx.LowPower != MFX_CODINGOPTION_ON);//LowPower limitation for temporal scalability we need to patch bitstream with SVC NAL after encoding

    for (mfxU32 i = 0; i < numDep; i++)
    {
        mfxU32 did     = par.calcParam.did[i];
        mfxU32 prevDid = i > 0 ? par.calcParam.did[i - 1] : 0;

        m_simulcast[did] =
            IsOff(extSvc->DependencyLayer[did].BasemodePred) &&
            IsOff(extSvc->DependencyLayer[did].MotionPred) &&
            IsOff(extSvc->DependencyLayer[did].ResidualPred) ? 1 : 0;
        m_refDqId[did] = did > 0
            ? mfxU8(16 * prevDid + extSvc->DependencyLayer[prevDid].QualityNum - 1)
            : 0;

        for (mfxU32 qid = 0; qid < extSvc->DependencyLayer[did].QualityNum; qid++)
        {
            m_spsIdx[did][qid] = mfxU8(did == 0 ? (!!qid) : (did + additionalSpsForFirstSpatialLayer));
            m_ppsIdx[did][qid] = mfxU8(did);
        }
    }

    if (additionalPpsForLastSpatialLayer)
        m_ppsIdx[lastDid][numQualityAtLastDep - 1]++;
#endif // #ifdef MFX_ENABLE_SVC_VIDEO_ENCODE

    m_cabacInitIdc            = extDdi.CabacInitIdcPlus1 - 1;
    m_directSpatialMvPredFlag = extDdi.DirectSpatialMvPredFlag;

    // pack headers
    OutputBitstream obs(Begin(m_headerBuffer), End(m_headerBuffer), m_emulPrev);

    ENCODE_PACKEDHEADER_DATA * bufDesc  = Begin(m_packedSps);
    mfxU8 *                    bufBegin = Begin(m_headerBuffer);
    mfxU32                     numBits  = 0;

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    // pack scalability sei
    Zero(m_packedScalabilitySei);
    if (IsSvcProfile(par.mfx.CodecProfile))
    {
        numBits = PutScalableInfoSeiMessage(obs, par);
        assert(numBits % 8 == 0);
        m_packedScalabilitySei = MakePackedByteBuffer(bufBegin, numBits / 8, m_emulPrev ? 0 : 4);
        bufBegin += numBits / 8;
    }
#endif // #ifdef MFX_ENABLE_SVC_VIDEO_ENCODE

    // pack sps for base and enhanced spatial layers with did > 0
    for (size_t i = 0; i < m_sps.size(); i++)
    {
#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
        numBits = (i == 0 || (numViews > 1))
            ? WriteSpsHeader(obs, m_sps[i])
            : WriteSpsHeader(obs, m_sps[i], m_subset[i - 1].Header);
#else
        numBits = WriteSpsHeader(obs, m_sps[i]);
#endif
        *bufDesc++ = MakePackedByteBuffer(bufBegin, numBits / 8, m_emulPrev ? 0 : 4);
        bufBegin += numBits / 8;
    }

    // pack pps for base and enhanced spatial layers
    bufDesc = Begin(m_packedPps);
    for (size_t i = 0; i < m_pps.size(); i++)
    {
        numBits = WritePpsHeader(obs, m_pps[i]);
        *bufDesc++ = MakePackedByteBuffer(bufBegin, numBits / 8, m_emulPrev ? 0 : 4);
        bufBegin += numBits / 8;
    }

#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
    // pack extended pps for adaptive CQM
    if (!m_packedCqmPps.empty())
    {
        bufDesc = Begin(m_packedCqmPps);
        for (size_t i = 0; i < m_cqmPps.size(); i++)
        {
            numBits = WritePpsHeader(obs, m_cqmPps[i]);
            *bufDesc++ = MakePackedByteBuffer(bufBegin, numBits / 8, m_emulPrev ? 0 : 4);
            bufBegin += numBits / 8;
        }
    }
#endif

    m_hwCaps = hwCaps;

    m_longStartCodes = IsOn(extDdi.LongStartCodes) && !IsOn(par.mfx.LowPower);

    m_isLowPower = IsOn(par.mfx.LowPower);
}

void HeaderPacker::ResizeSlices(mfxU32 num)
{
    m_packedSlices.resize(num);
    Zero(m_packedSlices);
}

#ifndef MFX_AVC_ENCODING_UNIT_DISABLE
/*
    Filling information about headers in HeadersInfo.
*/
void HeaderPacker::GetHeadersInfo(std::vector<mfxEncodedUnitInfo> &HeadersInfo, DdiTask const& task, mfxU32 fid)
{
    std::vector<ENCODE_PACKEDHEADER_DATA>::iterator it;
    mfxU32 offset = 0;
    if (task.m_insertAud[fid])
    {
        HeadersInfo.emplace_back();
        HeadersInfo.back().Type = NALU_AUD;
        HeadersInfo.back().Size = m_packedAud.DataLength;
        HeadersInfo.back().Offset = offset;
        offset += HeadersInfo.back().Size;
    }
    if (task.m_insertSps[fid])
    {
        for (it = m_packedSps.begin(); it < m_packedSps.end(); ++it)
        {
            HeadersInfo.emplace_back();
            HeadersInfo.back().Type = NALU_SPS;
            HeadersInfo.back().Size = it->DataLength;
            HeadersInfo.back().Offset = offset;
            offset += HeadersInfo.back().Size;
        }
    }
    if (task.m_insertPps[fid])
    {
        for (it = m_packedPps.begin(); it < m_packedPps.end(); ++it)
        {
            HeadersInfo.emplace_back();
            HeadersInfo.back().Type = NALU_PPS;
            HeadersInfo.back().Size = it->DataLength;
            HeadersInfo.back().Offset = offset;
            offset += HeadersInfo.back().Size;
        }
    }
}
#endif

ENCODE_PACKEDHEADER_DATA const & HeaderPacker::PackAud(
    DdiTask const & task,
    mfxU32          fieldId)
{
    mfxU8 * audBegin = m_packedPps.back().pData + m_packedPps.back().DataLength;
#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
    if (!m_packedCqmPps.empty())
        for (auto it = m_packedCqmPps.begin(); it < m_packedCqmPps.end(); it++)
            audBegin += it->DataLength;
#endif

    OutputBitstream obs(audBegin, End(m_headerBuffer), m_emulPrev);
    mfxU32 numBits = WriteAud(obs, task.m_type[fieldId]);
    m_packedAud = MakePackedByteBuffer(audBegin, numBits / 8, m_emulPrev ? 0 : 4);

    return m_packedAud;
}

std::vector<ENCODE_PACKEDHEADER_DATA> const & HeaderPacker::PackSlices(
    DdiTask const & task,
    mfxU32          fieldId)
{
    const mfxU32 maxSliceHeaderSize = 50;  //maximum coded slice header size in bytes
    size_t numSlices = task.m_SliceInfo.size() ?
        task.m_SliceInfo.size() :
        task.m_numSlice[fieldId];

    if (task.m_SliceInfo.size())
    {
        m_numMbPerSlice = 0;
    }

    if (numSlices)
    {
        m_packedSlices.resize(numSlices);
        if (m_sliceBuffer.size() < (numSlices * maxSliceHeaderSize))
        {
            m_sliceBuffer.resize(numSlices * maxSliceHeaderSize);
        }
    }
    Zero(m_sliceBuffer);
    Zero(m_packedSlices);

    mfxU8 * sliceBufferBegin = Begin(m_sliceBuffer);
    mfxU8 * sliceBufferEnd   = End(m_sliceBuffer);

    for (mfxU32 i = 0; i < m_packedSlices.size(); i++)
    {
        mfxU8 * endOfPrefix = m_needPrefixNalUnit && task.m_did == 0 && task.m_qid == 0
            ? PackPrefixNalUnitSvc(sliceBufferBegin, sliceBufferEnd, true, task, fieldId)
            : sliceBufferBegin;

        OutputBitstream obs(endOfPrefix, sliceBufferEnd, false); // pack without emulation control

        if (task.m_SliceInfo.size())
            WriteSlice(obs, task, fieldId, task.m_SliceInfo[i].startMB, task.m_SliceInfo[i].numMB);
        else
            WriteSlice(obs, task, fieldId, i);


        m_packedSlices[i].pData                  = sliceBufferBegin;
        m_packedSlices[i].DataLength             = mfxU32((endOfPrefix - sliceBufferBegin) * 8 + obs.GetNumBits()); // for slices length is in bits
        m_packedSlices[i].BufferSize             = (m_packedSlices[i].DataLength + 7) / 8;
        m_packedSlices[i].SkipEmulationByteCount = mfxU32(endOfPrefix - sliceBufferBegin + 3);

        sliceBufferBegin += m_packedSlices[i].BufferSize;
    }

    if (task.m_AUStartsFromSlice[fieldId])
        m_packedSlices[0].SkipEmulationByteCount = 4;

    return m_packedSlices;
}

void WritePredWeightTable(
    OutputBitstream &       obs,
    MFX_ENCODE_CAPS const & hwCaps,
    DdiTask const &         task,
    mfxU32                  fieldId,
    mfxU32                  chromaArrayType)
{
    // Transform field parity to field number before buffer request (PWT attached according to field order, not parity)
    // However in case of FEI single field mode, only one buffer is attached.
    mfxU32 fieldNum = task.m_singleFieldMode ? 0 : task.m_fid[fieldId];
    const mfxExtPredWeightTable* pPWT = GetExtBuffer(task.m_ctrl, fieldNum);
#ifdef MFX_ENABLE_FADE_DETECTION
    if (!pPWT)
        pPWT = &task.m_pwt[fieldId];
    else if ((pPWT->LumaLog2WeightDenom && pPWT->LumaLog2WeightDenom != 6) ||
        (pPWT->ChromaLog2WeightDenom && pPWT->ChromaLog2WeightDenom != 6))
        pPWT = &task.m_pwt[fieldId];
#endif
    mfxU32 nRef[2] = {
        std::max(1u, task.m_list0[fieldId].Size()),
        std::max(1u, task.m_list1[fieldId].Size())
    };
    mfxU32 maxWeights[2] = { hwCaps.ddi_caps.MaxNum_WeightedPredL0, hwCaps.ddi_caps.MaxNum_WeightedPredL1 };
    bool present;

    obs.PutUe(pPWT->LumaLog2WeightDenom);

    if (chromaArrayType != 0)
        obs.PutUe(pPWT->ChromaLog2WeightDenom);

    for (mfxU32 lx = 0; lx <= (mfxU32)!!(task.m_type[fieldId] & MFX_FRAMETYPE_B); lx++)
    {
        for (mfxU32 i = 0; i < nRef[lx]; i++)
        {
            present = !!pPWT->LumaWeightFlag[lx][i] && hwCaps.ddi_caps.LumaWeightedPred;

            if (i < maxWeights[lx])
            {
                obs.PutBit(present);

                if (present)
                {
                    obs.PutSe(pPWT->Weights[lx][i][0][0]);
                    obs.PutSe(pPWT->Weights[lx][i][0][1]);
                }
            }
            else
            {
                obs.PutBit(0);
            }

            if (chromaArrayType != 0)
            {
                present = !!pPWT->ChromaWeightFlag[lx][i] && hwCaps.ddi_caps.ChromaWeightedPred;

                if (i < maxWeights[lx])
                {
                    obs.PutBit(present);

                    if (present)
                    {
                        for (mfxU32 j = 1; j < 3; j++)
                        {
                            obs.PutSe(pPWT->Weights[lx][i][j][0]);
                            obs.PutSe(pPWT->Weights[lx][i][j][1]);
                        }
                    }
                }
                else
                {
                    obs.PutBit(0);
                }
            }
        }
    }
}

mfxU32 HeaderPacker::WriteSlice(
    OutputBitstream & obs,
    DdiTask const &   task,
    mfxU32            fieldId,
    mfxU32            sliceId)
{
    mfxU32 sliceType    = ConvertMfxFrameType2SliceType(task.m_type[fieldId]) % 5;
    mfxU32 refPicFlag   = !!(task.m_type[fieldId] & MFX_FRAMETYPE_REF);
    mfxU32 idrPicFlag   = !!(task.m_type[fieldId] & MFX_FRAMETYPE_IDR);
    mfxU32 nalRefIdc    = task.m_nalRefIdc[fieldId];
    mfxU32 nalUnitType  = (task.m_did == 0 && task.m_qid == 0) ? (idrPicFlag ? NALU_IDR : NALU_NON_IDR) : NALU_CODED_SLICE_EXT;
    mfxU32 fieldPicFlag = task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE;

    mfxExtSpsHeader const & sps = task.m_viewIdx ? m_sps[task.m_viewIdx] : m_sps[m_spsIdx[task.m_did][task.m_qid]];
    mfxExtPpsHeader const & pps =
#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
        task.m_adaptiveCQMHint > 0 && task.m_adaptiveCQMHint <= static_cast<mfxU32>(m_cqmPps.size()) ? m_cqmPps[task.m_adaptiveCQMHint - 1] :
#endif
        task.m_viewIdx ? m_pps[task.m_viewIdx] : m_pps[m_ppsIdx[task.m_did][task.m_qid]];

    // if frame_mbs_only_flag = 0 and current task implies encoding of progressive frame
    // then picture height in MBs isn't equal to PicHeightInMapUnits. Multiplier required
    mfxU32 picHeightMultiplier = (sps.frameMbsOnlyFlag == 0) && (fieldPicFlag == 0) ? 2 : 1;
    mfxU32 picHeightInMBs      = (sps.picHeightInMapUnitsMinus1 + 1) * picHeightMultiplier;

    SliceDivider divider = MakeSliceDivider(
        SliceDividerType(m_hwCaps.ddi_caps.SliceStructure),
        m_numMbPerSlice,
        (mfxU32)m_packedSlices.size(),
        sps.picWidthInMbsMinus1 + 1,
        picHeightInMBs);

    mfxU32 firstMbInSlice = 0;

    if (!m_isLowPower)
    {
        for (mfxU32 i = 0; i <= sliceId; i++, divider.Next())
            firstMbInSlice = divider.GetFirstMbInSlice();
    }

    mfxU32 sliceHeaderRestrictionFlag = 0;

    mfxU8 startcode[4] = { 0, 0, 0, 1};
    mfxU8 * pStartCode = startcode;
#if !defined(ANDROID)
    if (!m_longStartCodes)
    {
        if (task.m_AUStartsFromSlice[fieldId] == false || sliceId > 0)
            pStartCode++;
    }
#endif
    obs.PutRawBytes(pStartCode, startcode + sizeof startcode);
    obs.PutBit(0);
    obs.PutBits(nalRefIdc, 2);
    obs.PutBits(nalUnitType, 5);

    mfxU32 noInterLayerPredFlag = (task.m_qid == 0) ? m_simulcast[task.m_did] : 0;
    if (nalUnitType == 20)
    {
        mfxU32 useRefBasePicFlag = (task.m_type[fieldId] & MFX_FRAMETYPE_KEYPIC) ? 1 : 0;
        obs.PutBits(1, 1);          // svc_extension_flag
        obs.PutBits(idrPicFlag, 1);
        obs.PutBits(task.m_pid, 6);
        obs.PutBits(noInterLayerPredFlag, 1);
        obs.PutBits(task.m_did, 3);
        obs.PutBits(task.m_qid, 4);
        obs.PutBits(task.m_tid, 3);
        obs.PutBits(useRefBasePicFlag, 1);      // use_ref_base_pic_flag
        obs.PutBits(1, 1);          // discardable_flag
        obs.PutBits(1, 1);          // output_flag
        obs.PutBits(0x3, 2);        // reserved_three_2bits
    }

    obs.PutUe(firstMbInSlice);
    obs.PutUe(sliceType + 5);
    obs.PutUe(pps.picParameterSetId);
    obs.PutBits(task.m_frameNum, sps.log2MaxFrameNumMinus4 + 4);
    if (!sps.frameMbsOnlyFlag)
    {
        obs.PutBit(fieldPicFlag);
        if (fieldPicFlag)
            obs.PutBit(fieldId);
    }
    if (idrPicFlag)
        obs.PutUe(task.m_idrPicId);
    if (sps.picOrderCntType == 0)
    {
        obs.PutBits(task.GetPoc(fieldId), sps.log2MaxPicOrderCntLsbMinus4 + 4);
        if (pps.bottomFieldPicOrderInframePresentFlag && !fieldPicFlag)
            obs.PutSe(0); // delta_pic_order_cnt_bottom
    }
    if (sps.picOrderCntType == 1 && !sps.deltaPicOrderAlwaysZeroFlag)
    {
        obs.PutSe(0); // delta_pic_order_cnt[0]
        if (pps.bottomFieldPicOrderInframePresentFlag && !fieldPicFlag)
            obs.PutSe(0); // delta_pic_order_cnt[1]
    }
    if (task.m_qid == 0)
    {
        if (sliceType == SLICE_TYPE_B)
            obs.PutBit(IsOn(m_directSpatialMvPredFlag));
        if (sliceType != SLICE_TYPE_I)
        {
            mfxU32 numRefIdxL0ActiveMinus1 = std::max(1u, task.m_list0[fieldId].Size()) - 1;
            mfxU32 numRefIdxL1ActiveMinus1 = std::max(1u, task.m_list1[fieldId].Size()) - 1;
            mfxU32 numRefIdxActiveOverrideFlag =
                (numRefIdxL0ActiveMinus1 != pps.numRefIdxL0DefaultActiveMinus1) ||
                (numRefIdxL1ActiveMinus1 != pps.numRefIdxL1DefaultActiveMinus1 && sliceType == SLICE_TYPE_B);

            obs.PutBit(numRefIdxActiveOverrideFlag);
            if (numRefIdxActiveOverrideFlag)
            {
                obs.PutUe(numRefIdxL0ActiveMinus1);
                if (sliceType == SLICE_TYPE_B)
                    obs.PutUe(numRefIdxL1ActiveMinus1);
            }
        }
        if (sliceType != SLICE_TYPE_I)
            WriteRefPicListModification(obs, task.m_refPicList0Mod[fieldId]);
        if (sliceType == SLICE_TYPE_B)
            WriteRefPicListModification(obs, task.m_refPicList1Mod[fieldId]);
        if ((pps.weightedPredFlag  == 1 && sliceType == SLICE_TYPE_P) ||
            (pps.weightedBipredIdc == 1 && sliceType == SLICE_TYPE_B))
        {
            mfxU32 chromaArrayType = sps.separateColourPlaneFlag ? 0 : sps.chromaFormatIdc;
            WritePredWeightTable(obs, m_hwCaps, task, fieldId, chromaArrayType);
        }
        if (refPicFlag || task.m_nalRefIdc[fieldId])
        {
            WriteDecRefPicMarking(obs, task.m_decRefPicMrk[fieldId], idrPicFlag);
            mfxU32 storeRefBasePicFlag = 0;
            if (nalUnitType == 20 && !sliceHeaderRestrictionFlag)
                obs.PutBit(storeRefBasePicFlag);
        }
    }
    if (pps.entropyCodingModeFlag && sliceType != SLICE_TYPE_I)
        obs.PutUe(m_cabacInitIdc);
    obs.PutSe(task.m_cqpValue[fieldId] - (pps.picInitQpMinus26 + 26));
    if (pps.deblockingFilterControlPresentFlag)
    {
        mfxU32 disableDeblockingFilterIdc = task.m_disableDeblockingIdc[fieldId][sliceId];
        mfxI32 sliceAlphaC0OffsetDiv2     = task.m_sliceAlphaC0OffsetDiv2[fieldId][sliceId];
        mfxI32 sliceBetaOffsetDiv2        = task.m_sliceBetaOffsetDiv2[fieldId][sliceId];

        obs.PutUe(disableDeblockingFilterIdc);
        if (disableDeblockingFilterIdc != 1)
        {
            obs.PutSe(sliceAlphaC0OffsetDiv2);
            obs.PutSe(sliceBetaOffsetDiv2);
        }
    }

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    if (nalUnitType == 20)
    {
        mfxU32 sliceSkipFlag = 0;
        mfxExtSpsSvcHeader const & subset = m_subset[m_spsIdx[task.m_did][task.m_qid] - 1];

        if (!noInterLayerPredFlag && task.m_qid == 0)
        {
            mfxU32 interLayerDeblockingFilterIdc    = (task.m_did || task.m_qid) ? 0 : 1;
            mfxU32 interLayerSliceAlphaC0OffsetDiv2 = 0;
            mfxU32 interLayerSliceBetaOffsetDiv2    = 0;
            mfxU32 constrainedIntraResamplingFlag   = 0;

            obs.PutUe(m_refDqId[task.m_did]);

            if (subset.interLayerDeblockingFilterControlPresentFlag)
            {
                obs.PutUe(interLayerDeblockingFilterIdc);
                if (interLayerDeblockingFilterIdc != 1)
                {
                    obs.PutSe(interLayerSliceAlphaC0OffsetDiv2);
                    obs.PutSe(interLayerSliceBetaOffsetDiv2);
                }
            }
            obs.PutBit(constrainedIntraResamplingFlag);
            if (subset.extendedSpatialScalabilityIdc == 2)
            {
                if (sps.chromaFormatIdc > 0)
                {
                    obs.PutBit(subset.seqRefLayerChromaPhaseXPlus1Flag);
                    obs.PutBits(subset.seqRefLayerChromaPhaseYPlus1, 2);
                }
                obs.PutSe(subset.seqScaledRefLayerLeftOffset);
                obs.PutSe(subset.seqScaledRefLayerTopOffset);
                obs.PutSe(subset.seqScaledRefLayerRightOffset);
                obs.PutSe(subset.seqScaledRefLayerBottomOffset);
            }
        }

        if (!noInterLayerPredFlag)
        {
            sliceSkipFlag = 0;
            mfxU32 adaptiveBaseModeFlag              = !m_simulcast[task.m_did];
            mfxU32 defaultBaseModeFlag               = 0;
            mfxU32 adaptiveMotionPredictionFlag      = !m_simulcast[task.m_did];
            mfxU32 defaultMotionPredictionFlag       = 0;
            mfxU32 adaptiveResidualPredictionFlag    = !m_simulcast[task.m_did];
            mfxU32 defaultResidualPredictionFlag     = 0;
            mfxU32 adaptiveTcoeffLevelPredictionFlag = 0;

            obs.PutBit(sliceSkipFlag);
            if (sliceSkipFlag)
                obs.PutUe(divider.GetNumMbInSlice() - 1);
            else
            {
                obs.PutBit(adaptiveBaseModeFlag);
                if (!adaptiveBaseModeFlag)
                    obs.PutBit(defaultBaseModeFlag);
                if (!defaultBaseModeFlag)
                {
                    obs.PutBit(adaptiveMotionPredictionFlag);
                    if (!adaptiveMotionPredictionFlag)
                        obs.PutBit(defaultMotionPredictionFlag);
                }
                obs.PutBit(adaptiveResidualPredictionFlag);
                if (!adaptiveResidualPredictionFlag)
                    obs.PutBit(defaultResidualPredictionFlag);
            }
            if (adaptiveTcoeffLevelPredictionFlag)
                obs.PutBit(subset.seqTcoeffLevelPredictionFlag);
        }

        if (!sliceHeaderRestrictionFlag && !sliceSkipFlag)
        {
            mfxU32 scanIdxStart = 0;
            mfxU32 scanIdxEnd = 15;
            obs.PutBits(scanIdxStart, 4);
            obs.PutBits(scanIdxEnd, 4);
        }
    }
#endif // #ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    return obs.GetNumBits();
}

mfxU32 HeaderPacker::WriteSlice(
    OutputBitstream & obs,
    DdiTask const &   task,
    mfxU32            fieldId,
    mfxU32            firstMbInSlice,
    mfxU32            numMbInSlice)
{
    mfxU32 sliceType    = ConvertMfxFrameType2SliceType(task.m_type[fieldId]) % 5;
    mfxU32 refPicFlag   = !!(task.m_type[fieldId] & MFX_FRAMETYPE_REF);
    mfxU32 idrPicFlag   = !!(task.m_type[fieldId] & MFX_FRAMETYPE_IDR);
    mfxU32 nalRefIdc    = task.m_nalRefIdc[fieldId];
    mfxU32 nalUnitType  = (task.m_did == 0 && task.m_qid == 0) ? (idrPicFlag ? 5 : 1) : 20;
    mfxU32 fieldPicFlag = task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE;

    mfxExtSpsHeader const & sps = task.m_viewIdx ? m_sps[task.m_viewIdx] : m_sps[m_spsIdx[task.m_did][task.m_qid]];
    mfxExtPpsHeader const & pps =
#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
        IsCustMatrix(task.m_adaptiveCQMHint) ? m_cqmPps[(mfxU32)task.m_adaptiveCQMHint - 1] :
#endif
        task.m_viewIdx ? m_pps[task.m_viewIdx] : m_pps[m_ppsIdx[task.m_did][task.m_qid]];

    // if frame_mbs_only_flag = 0 and current task implies encoding of progressive frame
    // then picture height in MBs isn't equal to PicHeightInMapUnits. Multiplier required
    //mfxU32 picHeightMultiplier = (sps.frameMbsOnlyFlag == 0) && (fieldPicFlag == 0) ? 2 : 1;
    //mfxU32 picHeightInMBs      = (sps.picHeightInMapUnitsMinus1 + 1) * picHeightMultiplier;

    mfxU32 sliceHeaderRestrictionFlag = 0;
#if defined(ANDROID)
    mfxU8 startcode[4] = { 0, 0, 0, 1 };
#else
    mfxU8 startcode[3] = { 0, 0, 1 };

    if (m_longStartCodes)
        obs.PutFillerBytes(0x00, 1);
#endif
    obs.PutRawBytes(startcode, startcode + sizeof startcode);
    obs.PutBit(0);
    obs.PutBits(nalRefIdc, 2);
    obs.PutBits(nalUnitType, 5);

    mfxU32 noInterLayerPredFlag = (task.m_qid == 0) ? m_simulcast[task.m_did] : 0;
    if (nalUnitType == 20)
    {
        mfxU32 useRefBasePicFlag = (task.m_type[fieldId] & MFX_FRAMETYPE_KEYPIC) ? 1 : 0;
        obs.PutBits(1, 1);          // svc_extension_flag
        obs.PutBits(idrPicFlag, 1);
        obs.PutBits(task.m_pid, 6);
        obs.PutBits(noInterLayerPredFlag, 1);
        obs.PutBits(task.m_did, 3);
        obs.PutBits(task.m_qid, 4);
        obs.PutBits(task.m_tid, 3);
        obs.PutBits(useRefBasePicFlag, 1);      // use_ref_base_pic_flag
        obs.PutBits(1, 1);          // discardable_flag
        obs.PutBits(1, 1);          // output_flag
        obs.PutBits(0x3, 2);        // reserved_three_2bits
    }

    obs.PutUe(firstMbInSlice);
    obs.PutUe(sliceType + 5);
    obs.PutUe(pps.picParameterSetId);
    obs.PutBits(task.m_frameNum, sps.log2MaxFrameNumMinus4 + 4);
    if (!sps.frameMbsOnlyFlag)
    {
        obs.PutBit(fieldPicFlag);
        if (fieldPicFlag)
            obs.PutBit(fieldId);
    }
    if (idrPicFlag)
        obs.PutUe(task.m_idrPicId);
    if (sps.picOrderCntType == 0)
    {
        obs.PutBits(task.GetPoc(fieldId), sps.log2MaxPicOrderCntLsbMinus4 + 4);
        if (pps.bottomFieldPicOrderInframePresentFlag && !fieldPicFlag)
            obs.PutSe(0); // delta_pic_order_cnt_bottom
    }
    if (sps.picOrderCntType == 1 && !sps.deltaPicOrderAlwaysZeroFlag)
    {
        obs.PutSe(0); // delta_pic_order_cnt[0]
        if (pps.bottomFieldPicOrderInframePresentFlag && !fieldPicFlag)
            obs.PutSe(0); // delta_pic_order_cnt[1]
    }
    if (task.m_qid == 0)
    {
        if (sliceType == SLICE_TYPE_B)
            obs.PutBit(IsOn(m_directSpatialMvPredFlag));
        if (sliceType != SLICE_TYPE_I)
        {
            mfxU32 numRefIdxL0ActiveMinus1 = std::max(1u, task.m_list0[fieldId].Size()) - 1;
            mfxU32 numRefIdxL1ActiveMinus1 = std::max(1u, task.m_list1[fieldId].Size()) - 1;
            mfxU32 numRefIdxActiveOverrideFlag =
                (numRefIdxL0ActiveMinus1 != pps.numRefIdxL0DefaultActiveMinus1) ||
                (numRefIdxL1ActiveMinus1 != pps.numRefIdxL1DefaultActiveMinus1 && sliceType == SLICE_TYPE_B);

            obs.PutBit(numRefIdxActiveOverrideFlag);
            if (numRefIdxActiveOverrideFlag)
            {
                obs.PutUe(numRefIdxL0ActiveMinus1);
                if (sliceType == SLICE_TYPE_B)
                    obs.PutUe(numRefIdxL1ActiveMinus1);
            }
        }
        if (sliceType != SLICE_TYPE_I)
            WriteRefPicListModification(obs, task.m_refPicList0Mod[fieldId]);
        if (sliceType == SLICE_TYPE_B)
            WriteRefPicListModification(obs, task.m_refPicList1Mod[fieldId]);
        if ((pps.weightedPredFlag  == 1 && sliceType == SLICE_TYPE_P) ||
            (pps.weightedBipredIdc == 1 && sliceType == SLICE_TYPE_B))
        {
            mfxU32 chromaArrayType = sps.separateColourPlaneFlag ? 0 : sps.chromaFormatIdc;
            WritePredWeightTable(obs, m_hwCaps, task, fieldId, chromaArrayType);
        }
        if (refPicFlag)
        {
            WriteDecRefPicMarking(obs, task.m_decRefPicMrk[fieldId], idrPicFlag);
            mfxU32 storeRefBasePicFlag = 0;
            if (nalUnitType == 20 && !sliceHeaderRestrictionFlag)
                obs.PutBit(storeRefBasePicFlag);
        }
    }
    if (pps.entropyCodingModeFlag && sliceType != SLICE_TYPE_I)
        obs.PutUe(m_cabacInitIdc);
    obs.PutSe(task.m_cqpValue[fieldId] - (pps.picInitQpMinus26 + 26));
    if (pps.deblockingFilterControlPresentFlag)
    {
        // task.m_disableDeblockingIdc[fieldId] is initialized for fixed number of slices
        // it can't be used to feed slice header generation for adaptive slice mode
        // always use slice id 0
        // the same for task.m_sliceAlphaC0OffsetDiv2 and task.m_sliceBetaOffsetDiv2
        mfxU32 disableDeblockingFilterIdc = task.m_disableDeblockingIdc[fieldId][0];
        mfxI32 sliceAlphaC0OffsetDiv2     = task.m_sliceAlphaC0OffsetDiv2[fieldId][0];
        mfxI32 sliceBetaOffsetDiv2        = task.m_sliceBetaOffsetDiv2[fieldId][0];

        obs.PutUe(disableDeblockingFilterIdc);
        if (disableDeblockingFilterIdc != 1)
        {
            obs.PutSe(sliceAlphaC0OffsetDiv2);
            obs.PutSe(sliceBetaOffsetDiv2);
        }
    }

#ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    if (nalUnitType == 20)
    {
        mfxU32 sliceSkipFlag = 0;
        mfxExtSpsSvcHeader const & subset = m_subset[m_spsIdx[task.m_did][task.m_qid] - 1];

        if (!noInterLayerPredFlag && task.m_qid == 0)
        {
            mfxU32 interLayerDeblockingFilterIdc    = (task.m_did || task.m_qid) ? 0 : 1;
            mfxU32 interLayerSliceAlphaC0OffsetDiv2 = 0;
            mfxU32 interLayerSliceBetaOffsetDiv2    = 0;
            mfxU32 constrainedIntraResamplingFlag   = 0;

            obs.PutUe(m_refDqId[task.m_did]);

            if (subset.interLayerDeblockingFilterControlPresentFlag)
            {
                obs.PutUe(interLayerDeblockingFilterIdc);
                if (interLayerDeblockingFilterIdc != 1)
                {
                    obs.PutSe(interLayerSliceAlphaC0OffsetDiv2);
                    obs.PutSe(interLayerSliceBetaOffsetDiv2);
                }
            }
            obs.PutBit(constrainedIntraResamplingFlag);
            if (subset.extendedSpatialScalabilityIdc == 2)
            {
                if (sps.chromaFormatIdc > 0)
                {
                    obs.PutBit(subset.seqRefLayerChromaPhaseXPlus1Flag);
                    obs.PutBits(subset.seqRefLayerChromaPhaseYPlus1, 2);
                }
                obs.PutSe(subset.seqScaledRefLayerLeftOffset);
                obs.PutSe(subset.seqScaledRefLayerTopOffset);
                obs.PutSe(subset.seqScaledRefLayerRightOffset);
                obs.PutSe(subset.seqScaledRefLayerBottomOffset);
            }
        }

        if (!noInterLayerPredFlag)
        {
            sliceSkipFlag = 0;
            mfxU32 adaptiveBaseModeFlag              = !m_simulcast[task.m_did];
            mfxU32 defaultBaseModeFlag               = 0;
            mfxU32 adaptiveMotionPredictionFlag      = !m_simulcast[task.m_did];
            mfxU32 defaultMotionPredictionFlag       = 0;
            mfxU32 adaptiveResidualPredictionFlag    = !m_simulcast[task.m_did];
            mfxU32 defaultResidualPredictionFlag     = 0;
            mfxU32 adaptiveTcoeffLevelPredictionFlag = 0;

            obs.PutBit(sliceSkipFlag);
            if (sliceSkipFlag)
                obs.PutUe(numMbInSlice - 1);
            else
            {
                obs.PutBit(adaptiveBaseModeFlag);
                if (!adaptiveBaseModeFlag)
                    obs.PutBit(defaultBaseModeFlag);
                if (!defaultBaseModeFlag)
                {
                    obs.PutBit(adaptiveMotionPredictionFlag);
                    if (!adaptiveMotionPredictionFlag)
                        obs.PutBit(defaultMotionPredictionFlag);
                }
                obs.PutBit(adaptiveResidualPredictionFlag);
                if (!adaptiveResidualPredictionFlag)
                    obs.PutBit(defaultResidualPredictionFlag);
            }
            if (adaptiveTcoeffLevelPredictionFlag)
                obs.PutBit(subset.seqTcoeffLevelPredictionFlag);
        }

        if (!sliceHeaderRestrictionFlag && !sliceSkipFlag)
        {
            mfxU32 scanIdxStart = 0;
            mfxU32 scanIdxEnd = 15;
            obs.PutBits(scanIdxStart, 4);
            obs.PutBits(scanIdxEnd, 4);
        }
    }
#endif // #ifdef MFX_ENABLE_SVC_VIDEO_ENCODE
    (void)numMbInSlice;

    return obs.GetNumBits();
}


const mfxU8 M_N[2][3][3][2] = { // [isB][ctxIdx][cabac_init_idc][m, n]
    {// m and n values for 3 CABAC contexts 11-13 (mb_skip_flag for P frames)
        {{23, 33}, {22, 25}, {29, 16}},
        {{23,  2}, {34,  0}, {25,  0}},
        {{21,  0}, {16,  0}, {14,  0}}
    },
    {// m and n values for 3 CABAC contexts 24-26 (mb_skip_flag for B frames)
        {{18, 64}, {26, 34}, {20, 40}},
        {{ 9, 43}, {19, 22}, {20, 10}},
        {{29,  0}, {40,  0}, {29,  0}}
    },
};

const mfxI8 M_N_mbt_b[9][3][2] = { // [ctxIdx][cabac_init_idc][m, n]
    // m and n values for CABAC contexts 27-35 (mb_type for B frames)
    {{ 26,  67}, { 57,   2}, { 54,   0}},
    {{ 16,  90}, { 41,  36}, { 37,  42}},
    {{  9, 104}, { 26,  69}, { 12,  97}},
    {{-46, 127}, {-45, 127}, {-32, 127}},
    {{-20, 104}, {-15, 101}, {-22, 117}},
    {{  1,  67}, { -4,  76}, { -2,  74}},
    {{-13,  78}, { -6,  71}, { -4,  85}},
    {{-11,  65}, {-13,  79}, {-24, 102}},
    {{  1,  62}, {  5,  52}, {  5,  57}}
};


const mfxI8 M_N_mvd_b[14][3][2] = { // [ctxIdx][cabac_init_idc][m, n]
    // m and n values for CABAC contexts 40-53 (mvd_lx for B frames)
    {{ -3,  69}, { -2,  69}, {-11,  89}},
    {{ -6,  81}, { -5,  82}, {-15, 103}},
    {{-11,  96}, {-10,  96}, {-21, 116}},
    {{  6,  55}, {  2,  59}, { 19,  57}},
    {{  7,  67}, {  2,  75}, { 20,  58}},
    {{ -5,  86}, { -3,  87}, {  4,  84}},
    {{  2,  88}, { -3, 100}, {  6,  96}},
    {{  0,  58}, {  1,  56}, {  1,  63}},
    {{ -3,  76}, { -3,  74}, { -5,  85}},
    {{-10,  94}, { -6,  85}, {-13, 106}},
    {{  5,  54}, {  0,  59}, {  5,  63}},
    {{  4,  69}, { -3,  81}, {  6,  75}},
    {{ -3,  81}, { -7,  86}, { -3,  90}},
    {{  0,  88}, { -5,  95}, { -1, 101}}
};

const mfxI8 M_N_ref_b[6][3][2] = { // [ctxIdx][cabac_init_idc][m, n]
    // m and n values for CABAC contexts 54-59 (ref_idx_lx for B frames)
    {{ -7,  67}, { -1,  66}, {  3,  55}},
    {{ -5,  74}, { -1,  77}, { -4,  79}},
    {{ -4,  74}, {  1,  70}, { -2,  75}},
    {{ -5,  80}, { -2,  86}, {-12,  97}},
    {{ -7,  72}, { -5,  72}, { -7,  50}},
    {{  1,  58}, {  0,  61}, {  1,  60}},
};

const mfxI8 M_N_cbp_b[12][3][2] = { // [ctxIdx][cabac_init_idc][m, n]
    // m and n values for CABAC contexts 73-84 (coded_block_pattern for B frames)
    {{-27, 126}, {-39, 127}, {-36, 127}},
    {{-28,  98}, {-18,  91}, {-17,  91}},
    {{-25, 101}, {-17,  96}, {-14,  95}},
    {{-23,  67}, {-26,  81}, {-25,  84}},
    {{-28,  82}, {-35,  98}, {-25,  86}},
    {{-20,  94}, {-24, 102}, {-12,  89}},
    {{-16,  83}, {-23,  97}, {-17,  91}},
    {{-22, 110}, {-27, 119}, {-31, 127}},
    {{-21,  91}, {-24,  99}, {-14,  76}},
    {{-18, 102}, {-21, 110}, {-18, 103}},
    {{-13,  93}, {-18, 102}, {-13,  90}},
    {{-29, 127}, {-36, 127}, {-37, 127}}
};

inline void InitCabacContext(mfxI16 m, mfxI16 n, mfxU16 sliceQP, mfxU8& ctx)
{
    mfxI16 preCtxState = mfx::clamp<mfxI16>(( ( m * sliceQP ) >> 4 ) + n, 1, 126);
    if (preCtxState <= 63)
        ctx = mfxU8(63 - preCtxState); // MPS = 0
    else
        ctx = mfxU8((preCtxState - 64) | (1 << 6)); // MPS = 1
}

#define INIT_CABAC_CONTEXTS(idc, QP, MN, ctx) \
    for (mfxU32 i = 0; i < sizeof(ctx); i++) \
        InitCabacContext(MN[i][idc][0], MN[i][idc][1], mfxU16(QP), ctx[i]);


ENCODE_PACKEDHEADER_DATA const & HeaderPacker::PackSkippedSlice(
            DdiTask const & task,
            mfxU32          fieldId)
{
    Zero(m_packedSlices);

    mfxU8 * sliceBufferBegin = Begin(m_sliceBuffer);
    mfxU8 * sliceBufferEnd   = End(m_sliceBuffer);

    mfxU8 * endOfPrefix = m_needPrefixNalUnit && task.m_did == 0 && task.m_qid == 0
            ? PackPrefixNalUnitSvc(sliceBufferBegin, sliceBufferEnd, true, task, fieldId)
            : sliceBufferBegin;

    CabacPackerSimple packer(endOfPrefix, sliceBufferEnd, m_emulPrev);
    WriteSlice(packer, task, fieldId, 0);

    mfxExtSpsHeader const & sps = task.m_viewIdx ? m_sps[task.m_viewIdx] : m_sps[m_spsIdx[task.m_did][task.m_qid]];
    mfxExtPpsHeader const & pps =
#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
        IsCustMatrix(task.m_adaptiveCQMHint) ? m_cqmPps[(mfxU32)task.m_adaptiveCQMHint - 1] :
#endif
        task.m_viewIdx ? m_pps[task.m_viewIdx] : m_pps[m_ppsIdx[task.m_did][task.m_qid]];

    mfxU32 picHeightMultiplier = (sps.frameMbsOnlyFlag == 0) && (task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE) ? 2 : 1;
    mfxU32 picHeightInMB       = (sps.picHeightInMapUnitsMinus1 + 1) * picHeightMultiplier;
    mfxU32 picWidthInMB        = (sps.picWidthInMbsMinus1 + 1);
    mfxU32 picSizeInMB         = picWidthInMB * picHeightInMB;

    if (pps.entropyCodingModeFlag)
    {
        mfxU32 numAlignmentBits = (8 - (packer.GetNumBits() & 0x7)) & 0x7;
        packer.PutBits(0xff, numAlignmentBits);

        // encode dummy MB data

        mfxU8 cabacContexts[3]; // 3 CABAC contexts for mb_skip_flag
        mfxU32 sliceQP = task.m_cqpValue[fieldId];

        INIT_CABAC_CONTEXTS(m_cabacInitIdc, sliceQP, M_N[!!(task.m_type[fieldId] & MFX_FRAMETYPE_B)], cabacContexts);

        mfxU8 ctx276 = 63; // ctx for end_of_slice_flag: MPS = 0, pStateIdx = 63 (non-adapting prob state)

        if ((task.m_type[fieldId] & MFX_FRAMETYPE_B) && (task.m_type[fieldId] & MFX_FRAMETYPE_REF))
        {
            mfxU8 ctxMBT[9];
            mfxU8 ctxREF[6];
            mfxU8 ctxMVD[14];
            mfxU8 ctxCBP[12];

            INIT_CABAC_CONTEXTS(m_cabacInitIdc, sliceQP, M_N_mbt_b, ctxMBT);
            INIT_CABAC_CONTEXTS(m_cabacInitIdc, sliceQP, M_N_ref_b, ctxREF);
            INIT_CABAC_CONTEXTS(m_cabacInitIdc, sliceQP, M_N_mvd_b, ctxMVD);
            INIT_CABAC_CONTEXTS(m_cabacInitIdc, sliceQP, M_N_cbp_b, ctxCBP);

            for (mfxU32 uMB = 0; uMB < (picSizeInMB-1); uMB ++)
            {
                bool mbA = !!(uMB % picWidthInMB); //is left MB available
                bool mbB = (uMB >= picWidthInMB);   //is above MB available

                packer.EncodeBin(&cabacContexts[mbA + mbB], 0); // encode mb_skip_flag = 0

                // mb_type = 1 (B_L0_16x16) 1 0 0
                packer.EncodeBin(&ctxMBT[mbA + mbB], 1);
                packer.EncodeBin(&ctxMBT[3], 0);
                packer.EncodeBin(&ctxMBT[5], 0);

                if (task.m_list0[fieldId].Size() > 1)
                    packer.EncodeBin(&ctxREF[0], 0); // ref_idx_l0 = 0

                packer.EncodeBin(&ctxMVD[0], 0); // mvd_l0[][][0] = 0
                packer.EncodeBin(&ctxMVD[7], 0); // mvd_l0[][][1] = 0

                // coded_block_pattern = 0 (0 0 0 0, 0)
                packer.EncodeBin(&ctxCBP[mbA + 2 * mbB], 0);
                packer.EncodeBin(&ctxCBP[  1 + 2 * mbB], 0);
                packer.EncodeBin(&ctxCBP[mbA + 2 * 1  ], 0);
                packer.EncodeBin(&ctxCBP[  1 + 2 * 1  ], 0);
                packer.EncodeBin(&ctxCBP[4], 0);

                packer.EncodeBin(&ctx276, 0); // end_of_slice_flag = 0
            }

            packer.EncodeBin(&cabacContexts[2], 0); // encode mb_skip_flag = 0

            // mb_type = 1 (B_L0_16x16) 1 0 0
            packer.EncodeBin(&ctxMBT[2], 1);
            packer.EncodeBin(&ctxMBT[3], 0);
            packer.EncodeBin(&ctxMBT[5], 0);

            if (task.m_list0[fieldId].Size() > 1)
                packer.EncodeBin(&ctxREF[0], 0); // ref_idx_l0 = 0

            packer.EncodeBin(&ctxMVD[0], 0); // mvd_l0[][][0] = 0
            packer.EncodeBin(&ctxMVD[7], 0); // mvd_l0[][][1] = 0

            // coded_block_pattern = 0 (0 0 0 0, 0)
            packer.EncodeBin(&ctxCBP[3], 0);
            packer.EncodeBin(&ctxCBP[3], 0);
            packer.EncodeBin(&ctxCBP[3], 0);
            packer.EncodeBin(&ctxCBP[3], 0);
            packer.EncodeBin(&ctxCBP[4], 0);

        }
        else
        {
            for (mfxU32 uMB = 0; uMB < (picSizeInMB-1); uMB ++)
            {
                packer.EncodeBin(&cabacContexts[0], 1); // encode mb_skip_flag = 1 for every MB.
                packer.EncodeBin(&ctx276, 0); // encode end_of_slice_flag = 0 for every MB
            }

            packer.EncodeBin(&cabacContexts[0], 1); // encode mb_skip_flag = 1 for every MB.
        }

        packer.TerminateEncode();
    }
    else
    {
        if ((task.m_type[fieldId] & MFX_FRAMETYPE_B) && (task.m_type[fieldId] & MFX_FRAMETYPE_REF))
        {
            for (mfxU32 uMB = 0; uMB < picSizeInMB; uMB ++)
            {
                packer.PutUe(0); // mb_skip_run = 0
                packer.PutUe(1); // mb_type = 1 (B_L0_16x16)

                if (task.m_list0[fieldId].Size() > 1)
                    packer.PutBit(1);// ref_idx_l0 = 0

                packer.PutSe(0); // mvd_l0[][][0] = 0
                packer.PutSe(0); // mvd_l0[][][1] = 0

                packer.PutUe(0); // coded_block_pattern = 0
            }
        }
        else
        {
            packer.PutUe(picSizeInMB); // write mb_skip_run = picSizeInMBs
        }

        packer.PutTrailingBits();

        assert(packer.GetNumBits() % 8 == 0);
    }

    m_packedSlices[0].pData                  = sliceBufferBegin;
    m_packedSlices[0].DataLength             = mfxU32((endOfPrefix - sliceBufferBegin) + (packer.GetNumBits() / 8));
    m_packedSlices[0].BufferSize             = m_packedSlices[0].DataLength;
    m_packedSlices[0].SkipEmulationByteCount = m_emulPrev ? 0 : (mfxU32(endOfPrefix - sliceBufferBegin) + 3);

    return m_packedSlices[0];
}




#endif //MFX_ENABLE_H264_VIDEO_..._HW

