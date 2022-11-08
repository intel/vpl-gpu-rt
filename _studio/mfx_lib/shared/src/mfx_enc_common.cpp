// Copyright (c) 2008-2019 Intel Corporation
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
#include "ipps.h"

#include <cmath>
#include <algorithm>
#include <limits.h>
#include "mfx_enc_common.h"
#include "mfx_utils.h"
#ifdef MFX_ENABLE_ENCTOOLS
#include "mfxenctools-int.h"
#endif


//----MFX data -> UMC data--------------------------------------------
Ipp8u CalculateMAXBFrames (mfxU8 GopRefDist)
{
    if(GopRefDist)
        return (GopRefDist - 1);
    else
        return 0;
}

Ipp16u CalculateUMCGOPLength (mfxU16 GOPSize, mfxU8 targetUsage)
{
    if(GOPSize)
        return (GOPSize);
    else
        switch (targetUsage)
        {
        case MFX_TARGETUSAGE_BEST_QUALITY:
            return 200;
        case 2:
            return 200;
        case 3:
             return 200;
        case 4:
            return 20;
        case 5:
            return 10;
        case 6:
            return 4;
        case MFX_TARGETUSAGE_BEST_SPEED:
            return 1;
        }
    return 1;
}
bool SetUFParameters(mfxU8 TargetUsages, bool& mixed,Ipp32u& twoRef )
{
    switch (TargetUsages)
    {
    case MFX_TARGETUSAGE_BEST_QUALITY:
        mixed=0;twoRef=2;
        break;
    case 2:
        mixed=0;twoRef=2;
        break;
    case 3:
        mixed=0;twoRef=2;
        break;
    case 4:
        mixed=0;twoRef=1;
        break;
    case 5:
        mixed=0;twoRef=1;
        break;
    case 6:
        mixed=0;twoRef=1;
        break;
    case MFX_TARGETUSAGE_BEST_SPEED:
        mixed=0;twoRef=1;
        break;
    default:
        return false;
    }
    return true;
}
bool SetPROParameters (mfxU8 TargetUsages,Ipp8u &MESpeed, bool &UseFB, bool &FastFB, bool &bIntensityCompensation,
                       bool &bChangeInterpolationType, bool &bChangeVLCTables,bool &bTrellisQuantization, bool &bUsePadding,
                       bool &bVSTransform, bool &deblocking, mfxU8 &smoothing, bool &fastUVMC)
{
    switch (TargetUsages)
    {
    case MFX_TARGETUSAGE_BEST_QUALITY:
        MESpeed=25; UseFB=1; FastFB=0; bIntensityCompensation=0;
        bChangeInterpolationType=1;bTrellisQuantization = 1;
        bUsePadding  = 1; bChangeVLCTables =1;
        bVSTransform = 1; deblocking = 1;  smoothing = 0; fastUVMC = 0;
        break;
    case 2:
        MESpeed=25; UseFB=1; FastFB=0; bIntensityCompensation=0;
        bChangeInterpolationType=1;bTrellisQuantization = 1;
        bUsePadding  = 1; bChangeVLCTables =1;
        bVSTransform = 0; deblocking = 1;  smoothing = 0; fastUVMC = 0;
        break;
    case 3:
        MESpeed=25; UseFB=1; FastFB=0; bIntensityCompensation=0;
        bChangeInterpolationType=0;bTrellisQuantization = 0;
        bUsePadding  = 1; bChangeVLCTables =0;
        bVSTransform = 0; deblocking = 1;  smoothing = 0; fastUVMC = 0;
        break;
    case 4:
        MESpeed=25; UseFB=1; FastFB=0; bIntensityCompensation=0;
        bChangeInterpolationType=0;bTrellisQuantization = 0;
        bUsePadding  = 1; bChangeVLCTables =0;
        bVSTransform = 0; deblocking = 1;  smoothing = 0; fastUVMC = 0;
        break;
    case 5:
        MESpeed=25; UseFB=0; FastFB=0; bIntensityCompensation=0;
        bChangeInterpolationType=0;bTrellisQuantization = 0;
        bUsePadding  = 1; bChangeVLCTables =0;
        bVSTransform = 0; deblocking = 1;  smoothing = 0; fastUVMC = 0;
        break;
    case 6:
        MESpeed=25; UseFB=0; FastFB=0; bIntensityCompensation=0;
        bChangeInterpolationType=0;bTrellisQuantization = 0;
        bUsePadding  = 0; bChangeVLCTables =0;
        bVSTransform = 0; deblocking = 1;  smoothing = 0; fastUVMC = 1;
        break;
    case MFX_TARGETUSAGE_BEST_SPEED:
        MESpeed=25; UseFB=0; FastFB=0; bIntensityCompensation=0;
        bChangeInterpolationType=0;bTrellisQuantization = 0;
        bUsePadding  = 0; bChangeVLCTables =0;
        bVSTransform = 0; deblocking = 0;  smoothing = 0; fastUVMC = 1;
        break;
    default:
        return false;
    }
    return true;
}


Ipp32u CalculateUMCBitrate(mfxU16    TargetKbps)
{
    return TargetKbps*1000;
}

Ipp64f CalculateUMCFramerate( mfxU32 FrameRateExtN, mfxU32 FrameRateExtD)
{
    if (FrameRateExtN && FrameRateExtD)
        return (Ipp64f)FrameRateExtN / FrameRateExtD;
    else
        return 0;
}

void CalculateMFXFramerate(Ipp64f framerate, mfxU32* FrameRateExtN, mfxU32* FrameRateExtD)
{
  mfxU32 fr;
  if (!FrameRateExtN || !FrameRateExtD)
    return;

  fr = (mfxU32)(framerate + .5);
  if (fabs(fr - framerate) < 0.0001) {
    *FrameRateExtN = fr;
    *FrameRateExtD = 1;
    return;
  }

  fr = (mfxU32)(framerate * 1.001 + .5);
  if (fabs(fr * 1000 - framerate * 1001) < 10) {
    *FrameRateExtN = fr * 1000;
    *FrameRateExtD = 1001;
    return;
  }
  // can do more

  *FrameRateExtN = (mfxU32)(framerate * 10000 + .5);
  *FrameRateExtD = 10000;
  return;
}


UMC::FrameType GetFrameType (mfxU16 FrameOrder, mfxInfoMFX* info)
{
  mfxU16 GOPSize = info->GopPicSize;
  mfxU16 IPDist = info->GopRefDist;
  mfxU16 pos = (GOPSize)? FrameOrder %(GOPSize):FrameOrder;


  if (pos == 0 || IPDist == 0)
  {
      return UMC::I_PICTURE;
  }
  else
  {
      pos = pos % (IPDist);
      return (pos != 0) ? UMC::B_PICTURE : UMC::P_PICTURE;
  }
}


//----UMC data -> MFX data--------------------------------------------

mfxU16 CalculateMFXGOPLength (Ipp16u GOPSize)
{
    if(!GOPSize)
        return GOPSize;
    else
        return 15;
}

mfxU8 CalculateGopRefDist(mfxU8 BNum)
{
    return (BNum + 1);
}

mfxU32 TranslateMfxFRCodeMPEG2(mfxFrameInfo *info, mfxU32 *codeN, mfxU32* codeD)
{
  mfxU32 n = info->FrameRateExtN, d = info->FrameRateExtD, code = 0;
  mfxU32 flag1001 = 0;

  if (!n || !d) return 0;
  if(d%1001 == 0 && n%1000 == 0) {
    n /= 1000;
    d /= 1001;
    flag1001 = 1;
  }
  switch(n) {
    case 48: case 72: case 96:
    case 24 : code = 2 - flag1001; n /= 24; break;
    case 90:
    case 30 : code = 5 - flag1001; n /= 30; break;
    case 120: case 180: case 240:
    case 60 : code = 8 - flag1001; n /= 60; break;
    case 75:
    case 25 : code = 3; n /= 25; break;
    case 100: case 150: case 200:
    case 50 : code = 6; n /= 50; break;
    default: code = 0;
  }
  if( code != 0 && d <= 0x20 &&
    ((code !=3 && code != 6) || !flag1001) ) {
      *codeN = n-1;
      *codeD = d-1;
      return code;
  }
  // can do more
  return 0;
}

mfxExtCodingOption* GetExtCodingOptions(mfxExtBuffer** ebuffers, mfxU32 nbuffers)
{
    return (mfxExtCodingOption*)mfx::GetExtBuffer(ebuffers, nbuffers, MFX_EXTBUFF_CODING_OPTION);
}

mfxExtVideoSignalInfo* GetExtVideoSignalInfo(mfxExtBuffer** ebuffers, mfxU32 nbuffers)
{
    return (mfxExtVideoSignalInfo *)mfx::GetExtBuffer(ebuffers, nbuffers, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
}

mfxExtChromaLocInfo* GetExtChromaLocInfo(mfxExtBuffer** ebuffers, mfxU32 nbuffers)
{
    return (mfxExtChromaLocInfo*)mfx::GetExtBuffer(ebuffers, nbuffers, MFX_EXTBUFF_CHROMA_LOC_INFO);
}

//----------work with marker-----------------------------
mfxStatus SetFrameLockMarker(mfxFrameData* pFrame, mfxU8 LockMarker)
{
    MFX_CHECK_NULL_PTR1(pFrame);

    pFrame->Locked = LockMarker;

    return MFX_ERR_NONE;
}

mfxU8 GetFrameLockMarker(mfxFrameData* pFrame)
{
    return (mfxU8)pFrame->Locked;
}

const Rational RATETAB[8]=
{
    {24000, 1001},
    {   24,    1},
    {   25,    1},
    {30000, 1001},
    {   30,    1},
    {   50,    1},
    {60000, 1001},
    {   60,    1},
};

static const Rational SORTED_RATIO[] =
{
    {1,32},{1,31},{1,30},{1,29},{1,28},{1,27},{1,26},{1,25},{1,24},{1,23},{1,22},{1,21},{1,20},{1,19},{1,18},{1,17},
    {1,16},{2,31},{1,15},{2,29},{1,14},{2,27},{1,13},{2,25},{1,12},{2,23},{1,11},{3,32},{2,21},{3,31},{1,10},{3,29},
    {2,19},{3,28},{1, 9},{3,26},{2,17},{3,25},{1, 8},{4,31},{3,23},{2,15},{3,22},{4,29},{1, 7},{4,27},{3,20},{2,13},
    {3,19},{4,25},{1, 6},{4,23},{3,17},{2,11},{3,16},{4,21},{1, 5},{4,19},{3,14},{2, 9},{3,13},{4,17},{1, 4},{4,15},
    {3,11},{2, 7},{3,10},{4,13},{1, 3},{4,11},{3, 8},{2, 5},{3, 7},{4, 9},{1, 2},{4, 7},{3, 5},{2, 3},{3, 4},{4, 5},
    {1,1},{4,3},{3,2},{2,1},{3,1},{4,1}
};

class FR_Compare
{
public:

    bool operator () (Rational rfirst, Rational rsecond)
    {
        mfxF64 first = (mfxF64) rfirst.n/rfirst.d;
        mfxF64 second = (mfxF64) rsecond.n/rsecond.d;
        return ( first < second );
    }
};


void ConvertFrameRateMPEG2(mfxU32 FrameRateExtD, mfxU32 FrameRateExtN, mfxI32 &frame_rate_code, mfxI32 &frame_rate_extension_n, mfxI32 &frame_rate_extension_d)
{
    Rational convertedFR;
    Rational bestFR = {INT_MAX, 1};
    mfxF64 minDifference = MFX_MAXABS_64F;

    for (mfxU32 i = 0; i < sizeof(RATETAB) / sizeof(RATETAB[0]); i++)
    {
        if (FrameRateExtN * RATETAB[i].d == FrameRateExtD * RATETAB[i].n )
        {
            frame_rate_code = i + 1;
            frame_rate_extension_n = 0;
            frame_rate_extension_d = 0;

            return;
        }
    }

    for (mfxU32 i = 0; i < sizeof(RATETAB) / sizeof(RATETAB[0]); i++)
    {
        convertedFR.n = (mfxI64) FrameRateExtN * RATETAB[i].d;
        convertedFR.d = (mfxI64) FrameRateExtD * RATETAB[i].n;

        const Rational* it_lower = std::lower_bound(
            SORTED_RATIO,
            SORTED_RATIO + sizeof(SORTED_RATIO) / sizeof(SORTED_RATIO[0]),
            convertedFR,
            FR_Compare());

        if (it_lower == SORTED_RATIO + sizeof(SORTED_RATIO) / sizeof(SORTED_RATIO[0]))  // exceed max FR
        {
            it_lower--;
        }
        else if (it_lower != SORTED_RATIO) // min FR not reached
        {
            if ( std::abs( (mfxF64) (it_lower - 1)->n / (it_lower - 1)->d - (mfxF64) convertedFR.n / convertedFR.d) <
                std::abs((mfxF64)it_lower->n / it_lower->d - (mfxF64)convertedFR.n / convertedFR.d))
            {
                it_lower--;
            }
        }

        if (minDifference > std::abs((mfxF64)convertedFR.n / convertedFR.d - (mfxF64)it_lower->n / it_lower->d))
        {
            minDifference = std::abs((mfxF64)convertedFR.n / convertedFR.d - (mfxF64)it_lower->n / it_lower->d);
            frame_rate_code = i + 1;
            bestFR = *it_lower;
        }
    }

    for (mfxU32 i = 0; i < sizeof(RATETAB) / sizeof(RATETAB[0]); i++)
    {// check that exist equal RATETAB with FR = 1:1
        if (RATETAB[i].d * RATETAB[frame_rate_code - 1].n * bestFR.n == RATETAB[i].n * RATETAB[frame_rate_code - 1].d * bestFR.d)
        {
            frame_rate_code = i + 1;
            frame_rate_extension_n = 0;
            frame_rate_extension_d = 0;
            return;
        }
    }

    frame_rate_extension_n = (mfxI32) bestFR.n - 1;
    frame_rate_extension_d = (mfxI32) bestFR.d - 1;
    return;
}

mfxStatus CheckFrameRateMPEG2(mfxU32 &FrameRateExtD, mfxU32 &FrameRateExtN)
{
    mfxI32 frame_rate_code        = 0;
    mfxI32 frame_rate_extension_n = 0;
    mfxI32 frame_rate_extension_d = 0;
    mfxF64 input_ratio = (mfxF64) FrameRateExtN / FrameRateExtD;
    mfxF64 difference_ratio = 0;

    ConvertFrameRateMPEG2(FrameRateExtD, FrameRateExtN, frame_rate_code, frame_rate_extension_n, frame_rate_extension_d);
    difference_ratio =  fabs(input_ratio - (mfxF64)(frame_rate_extension_n + 1) / (frame_rate_extension_d + 1) * RATETAB[frame_rate_code - 1].n / RATETAB[frame_rate_code - 1].d);

    if (difference_ratio < input_ratio / 50000)
    { //difference less than 0.05%
       return MFX_ERR_NONE;
    }
    else
    {
       FrameRateExtD = (mfxU32) ((frame_rate_extension_d + 1) * RATETAB[frame_rate_code - 1].d);
       FrameRateExtN = (mfxU32) ((frame_rate_extension_n + 1) * RATETAB[frame_rate_code - 1].n);

       if (difference_ratio < input_ratio / 1000)
           return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
       else //difference more than 0.1%
           return MFX_ERR_INVALID_VIDEO_PARAM;
    }
}
mfxStatus CheckAspectRatioMPEG2 (mfxU16 &aspectRatioW, mfxU16 &aspectRatioH, mfxU32 frame_width, mfxU32 frame_heigth, mfxU16 cropW, mfxU16 cropH)
{
    mfxU32 width = (cropW != 0) ? cropW : frame_width;
    mfxU32 height =(cropH != 0) ? cropH : frame_heigth;

    if ((aspectRatioW == 0 && aspectRatioH == 0) || (aspectRatioW == 1 && aspectRatioH == 1))
        return MFX_ERR_NONE;
    if (aspectRatioW == 0 || aspectRatioH == 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    if (width != 0 && height != 0)
    {
        mfxU64 k   = ((mfxU64)aspectRatioW * width * 100000) / (aspectRatioH * height);

        if ((aspectRatioW * width * 3 == aspectRatioH * height * 4) ||
            (aspectRatioW * width * 9 == aspectRatioH * height * 16) ||
            (aspectRatioW * width * 100 == aspectRatioH * height * 221) )
        {
            return MFX_ERR_NONE;
        }

        if (k > (400000/3 - 133) && k < (400000/3 + 133))
        {
            return MFX_ERR_NONE;
        }
        else if ( k > ( 1600000/9 - 177) && k < (1600000/9 + 177) )
        {
            return MFX_ERR_NONE;
        }
        else if (k > (22100000/100 - 221) && k < (22100000/100 + 221))
        {
             return MFX_ERR_NONE;
        }
        else
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }
    else
    {
        if (width == 0 && height == 0)
            return MFX_ERR_NONE;
        else
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }
}
mfxU8 GetAspectRatioCode (mfxU32 dispAspectRatioW, mfxU32 dispAspectRatioH)
{
    if (dispAspectRatioH == 0)
    {
        return 1;
    }
    mfxU64 k  = ((mfxU64)dispAspectRatioW*1000)/(dispAspectRatioH);

     if  (k <= (4000/3  + 1)  && k >= (4000/3 - 1))
     {
         return 2;
     }
     if (k <= (16000/9  + 1)  && k >= (16000/9 - 1))
     {
         return 3;
     }
     if (k <= (221000/100  + 1)  && k >= (221000/100 - 1))
     {
         return 4;
     }
     return 1;
}

mfxStatus AllocInternalEncBuffer(VideoCORE* pCore, const mfxU16& numFrameMin, const mfxVideoParam& par, mfxFrameAllocResponse& response)
{
    MFX_CHECK(pCore && par.IOPattern != MFX_IOPATTERN_IN_SYSTEM_MEMORY, MFX_ERR_NONE);

    mfxFrameAllocRequest request = {};
    request.Info = par.mfx.FrameInfo;
    request.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME;
    request.NumFrameMin = request.NumFrameSuggested = numFrameMin;

    mfxStatus sts = pCore->AllocFrames(&request, &response);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

bool CorrectProfileLevelMpeg2(mfxU16 &profile, mfxU16 & level, mfxU32 w, mfxU32 h, mfxF64 frame_rate, mfxU32 bitrate, mfxU32 GopRefDist)
{
    mfxU16 oldLevel   = level;
    mfxU16 oldProfile = profile;

    if (MFX_LEVEL_MPEG2_HIGH !=  level && MFX_LEVEL_MPEG2_HIGH1440 !=  level && MFX_LEVEL_MPEG2_MAIN !=  level &&  MFX_LEVEL_MPEG2_LOW !=  level)
        level = MFX_LEVEL_MPEG2_MAIN;

    if (MFX_PROFILE_MPEG2_SIMPLE != profile
        && MFX_PROFILE_MPEG2_MAIN != profile
        )
    {
        profile = MFX_PROFILE_MPEG2_MAIN;
    }

    if (MFX_PROFILE_MPEG2_HIGH == profile)
    {
        if (w > 1440 || h > 1152 || frame_rate*w*h > 62668800.0 || bitrate > 80000000)
        {
            level = MFX_LEVEL_MPEG2_HIGH;
        }
        else if ((w > 720 || h > 576 || frame_rate > 30 || frame_rate*w*h > 14745600.0 || bitrate > 20000000) && (level != MFX_LEVEL_MPEG2_HIGH))
        {
            level = MFX_LEVEL_MPEG2_HIGH1440;
        }
    }
    else
    {
        if (w > 1440 || h > 1152 || frame_rate*w*h > 47001600.0 || bitrate > 60000000)
        {
            level = MFX_LEVEL_MPEG2_HIGH;
        }
        else if ((w > 720 || h > 576 || frame_rate > 30 || frame_rate*w*h > 10368000.0 || bitrate > 15000000) && (level != MFX_LEVEL_MPEG2_HIGH))
        {
            level = MFX_LEVEL_MPEG2_HIGH1440;
        }
        else if ((w > 352 || h > 288 || frame_rate*w*h > 3041280.0 || bitrate > 4000000) && (level != MFX_LEVEL_MPEG2_HIGH && level != MFX_LEVEL_MPEG2_HIGH1440))
        {
            level = MFX_LEVEL_MPEG2_MAIN;
        }
    }

    if (MFX_PROFILE_MPEG2_SIMPLE == profile && ((MFX_LEVEL_MPEG2_MAIN != level && MFX_LEVEL_MPEG2_LOW != level) || (GopRefDist >1)))
    {
        profile = MFX_PROFILE_MPEG2_MAIN;
    }

    if (MFX_PROFILE_MPEG2_HIGH == profile && MFX_LEVEL_MPEG2_LOW == level )
    {
        profile = MFX_PROFILE_MPEG2_MAIN;
    }
    return (((oldLevel!=0) && (oldLevel!=level)) || ((oldProfile!=0) && (oldProfile!=profile)));

}
mfxStatus InputSurfaces::Reset(mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxU32 ioPattern = par->IOPattern & (
        MFX_IOPATTERN_IN_VIDEO_MEMORY
        | MFX_IOPATTERN_IN_SYSTEM_MEMORY
    );
    if (ioPattern & (ioPattern - 1))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    MFX_INTERNAL_CPY(&m_Info,&par->mfx.FrameInfo,sizeof(mfxFrameInfo));

    bool bSysMemFrames = (par->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY) != 0;
    MFX_CHECK(bSysMemFrames == m_bSysMemFrames || !m_bInitialized, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    m_bSysMemFrames = bSysMemFrames;

    m_bInitialized = true;
    return sts;
}
mfxStatus InputSurfaces::Close()
{
    if (m_response.NumFrameActual != 0)
    {
        m_pCore->FreeFrames (&m_response);
    }
    m_bSysMemFrames = false;
    m_bInitialized = false;

    memset(&m_request,  0, sizeof(mfxFrameAllocRequest));
    memset(&m_response, 0, sizeof (mfxFrameAllocResponse));

    return MFX_ERR_NONE;
}

mfxStatus CheckExtVideoSignalInfo(mfxExtVideoSignalInfo * videoSignalInfo)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (videoSignalInfo->VideoFormat > 7)
    {
        videoSignalInfo->VideoFormat = 5; // unspecified video format
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (videoSignalInfo->ColourDescriptionPresent > 1)
    {
        videoSignalInfo->ColourDescriptionPresent = 0;
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (videoSignalInfo->ColourDescriptionPresent)
    {
        if (videoSignalInfo->ColourPrimaries > 255)
        {
            videoSignalInfo->ColourPrimaries = 2; // unspecified image characteristics
            sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        if (videoSignalInfo->TransferCharacteristics > 255)
        {
            videoSignalInfo->TransferCharacteristics = 2; // unspecified image characteristics
            sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        if (videoSignalInfo->MatrixCoefficients > 255)
        {
            videoSignalInfo->MatrixCoefficients = 2; // unspecified image characteristics
            sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    return sts;
}

mfxStatus CheckExtChromaLocInfo(mfxExtChromaLocInfo* chromaLocInfo)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (chromaLocInfo->ChromaLocInfoPresentFlag)
    {
        if (chromaLocInfo->ChromaSampleLocTypeTopField > 255)
        {
            chromaLocInfo->ChromaSampleLocTypeTopField = 0;
            sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        if (chromaLocInfo->ChromaSampleLocTypeBottomField > 255)
        {
            chromaLocInfo->ChromaSampleLocTypeBottomField = 0;
            sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    return sts;
}

typedef std::remove_reference<decltype (mfxExtEncoderROI::ROI[0])>::type RectData;
mfxStatus FillMBMapViaROI(const mfxExtEncoderROI& roi,
    mfxI8* pMbMap,
    mfxU32 width, mfxU32 height, mfxU32 pitch,
    mfxU32 block_width, mfxU32 block_height, 
    mfxI8 QpY)
{
    MFX_CHECK_NULL_PTR1(pMbMap);
    mfxU32 x = 0, y = 0;
    auto IsXYInRect = [&](const RectData& r)
    {
        return (x * block_width) >= r.Left
            && (x * block_width) < r.Right
            && (y * block_height) >= r.Top
            && (y * block_height) < r.Bottom; 
    };
    auto NextQP = [&]()
    {
        auto pEnd = roi.ROI + roi.NumROI;
        auto pRect = std::find_if(roi.ROI, pEnd, IsXYInRect);

        ++x;

        if (pRect != pEnd)
            return mfxI8(QpY + pRect->DeltaQP);

        return mfxI8(QpY);
    };
    auto FillQpMapRow = [&](mfxI8& qpMapRow)
    {
        x = 0;
        std::generate_n(&qpMapRow, width, NextQP);
        ++y;
    };
    auto itQpMapRowsBegin = mfx::MakeStepIter(pMbMap, pitch);
    auto itQpMapRowsEnd = mfx::MakeStepIter(pMbMap + height * pitch);

    std::for_each(itQpMapRowsBegin, itQpMapRowsEnd, FillQpMapRow);

    return MFX_ERR_NONE;
}
template<class T> inline T CeilDiv(T x, T y) { return (x + y - 1) / y; }
mfxStatus FillCUQPData(mfxI8 QpY,
    mfxI8* pMbMap,
    mfxU32 pitch, mfxU32 height_aligned)
{
    MFX_CHECK_NULL_PTR1(pMbMap);
    // Fill all LCU blocks: HW hevc averages QP
    for (mfxU32 i = 0; i < height_aligned; i++)
    {
        for (mfxU32 j = 0; j < pitch; j++)
        {
            pMbMap[i * pitch + j] = QpY;
        }
    }
    return MFX_ERR_NONE;
}
mfxStatus FillMBQPBuffer(
    const mfxU8* mbqpInputBuffer,
    mfxU32 mbqpInputBufferSize,
    mfxU32 picWidth, mfxU32 picHeight,
    mfxI8* pMbMap,
    mfxU32 pitch, mfxU32 height_aligned,
    mfxU32 block_width, mfxU32 block_height)
{ 
    

    mfxU32 drBlkW = block_width;  // block size of driver
    mfxU32 drBlkH = block_height; // block size of driver
    mfxU32 inBlkSize = 16;   //input block size

    mfxU32 inputW = CeilDiv(picWidth, inBlkSize);
    mfxU32 inputH = CeilDiv(picHeight, inBlkSize);

    MFX_CHECK(mbqpInputBufferSize >= inputW * inputH, MFX_ERR_UNDEFINED_BEHAVIOR);

    // Fill all LCU blocks: HW hevc averages QP
    for (mfxU32 i = 0; i < height_aligned; i++)
    {
        for (mfxU32 j = 0; j < pitch; j++)
        {
            mfxU32 y = std::min(i * drBlkH / inBlkSize, inputH - 1);
            mfxU32 x = std::min(j * drBlkW / inBlkSize, inputW - 1);

            pMbMap[i * pitch + j] = mbqpInputBuffer[y * inputW + x];
        }
    }
    return MFX_ERR_NONE;
}
mfxStatus FillCUQPData(const mfxExtMBQP* mbqpInput,
    mfxU32 picWidth, mfxU32 picHeight,
    mfxI8* pMbMap,
    mfxU32 pitch, mfxU32 height_aligned,
    mfxU32 block_width, mfxU32 block_height)
{
    MFX_CHECK_NULL_PTR1(pMbMap);
    MFX_CHECK_NULL_PTR1(mbqpInput);
    MFX_CHECK_NULL_PTR1(mbqpInput->QP);

    return FillMBQPBuffer(mbqpInput->QP, mbqpInput->NumQPAlloc,
        picWidth, picHeight, pMbMap, pitch, height_aligned, block_width, block_height); 
}
bool IsSWBRCMode(const mfxVideoParam& par)
{     
    const mfxExtCodingOption2* pCO2 = (mfxExtCodingOption2*)mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_CODING_OPTION2);
#ifdef MFX_ENABLE_ENCTOOLS
    const mfxExtEncToolsConfig* pCfg = (mfxExtEncToolsConfig*)mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_ENCTOOLS_CONFIG);
#endif
    return
        ((((pCO2 && IsOn(pCO2->ExtBRC))
#ifdef MFX_ENABLE_ENCTOOLS
            || (pCfg && IsOn(pCfg->BRC))
#endif
            )
            && (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR
                || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR)) ||
            par.mfx.RateControlMethod == MFX_RATECONTROL_LA ||
            par.mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD ||
            par.mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ
            );
}
static bool ROIViaMBQP(const mfxVideoParam par, mfxU32 maxNumOfROI, mfxU32 ROIDeltaQPSupport)
{
    mfxExtEncoderROI *extRoi = (mfxExtEncoderROI*)mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_ENCODER_ROI);
    return extRoi && extRoi->NumROI > 0 && (!maxNumOfROI || !ROIDeltaQPSupport) && IsSWBRCMode(par);
}

bool IsEnctoolsLABRC(const mfxVideoParam& par)
{
#if !defined(MFX_ENABLE_ENCTOOLS)
    std::ignore = par;
#else
    const mfxExtCodingOption2* pCO2 = (mfxExtCodingOption2*)mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_CODING_OPTION2);
    const mfxExtCodingOption3* pCO3 = (mfxExtCodingOption3*)mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_CODING_OPTION3);
    const mfxExtEncToolsConfig* pCfg = (mfxExtEncToolsConfig*)mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_ENCTOOLS_CONFIG);

    if (
        (
            ((par.mfx.GopRefDist == 2 || par.mfx.GopRefDist == 4 || par.mfx.GopRefDist == 8)
                && pCO2 && pCO2->ExtBRC == MFX_CODINGOPTION_ON && pCO2->LookAheadDepth > par.mfx.GopRefDist
                && !(pCO3 && pCO3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING)
                )
            || (pCfg && IsOn(pCfg->BRC) && pCO2 && pCO2->LookAheadDepth > par.mfx.GopRefDist)
            )
        && (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
        )
        return true;
#endif
    return false;
}
static bool IsEnctoolsALQOffset(const mfxVideoParam& par)
{
#if !defined(MFX_ENABLE_ENCTOOLS)
    std::ignore = par;
#else
    const mfxExtEncToolsConfig* pCfg = (mfxExtEncToolsConfig*)mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_ENCTOOLS_CONFIG);

    if (((par.mfx.GopRefDist == 8) &&   (pCfg && IsOn(pCfg->BRC) && (IsOn(pCfg->AdaptivePyramidQuantB) || IsOn(pCfg->AdaptivePyramidQuantP)))
        && (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR))
        && !IsOn(par.mfx.LowPower))
        return true;
#endif
    return false;
}
bool IsEnctoolsLAGS(const mfxVideoParam& par)
{
#if !defined(MFX_ENABLE_ENCTOOLS)
    std::ignore = par;
#else
    const mfxExtCodingOption2* pCO2 = (mfxExtCodingOption2*)mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_CODING_OPTION2);
    const mfxExtCodingOption3* pCO3 = (mfxExtCodingOption3*)mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_CODING_OPTION3);
    const mfxExtEncToolsConfig* pCfg = (mfxExtEncToolsConfig*)mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_ENCTOOLS_CONFIG);
    
    if (((pCO2 && pCO2->LookAheadDepth > 0) || (pCfg && IsOn(pCfg->BRCBufferHints)))
        && ((pCO3 && pCO3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING) || !IsSWBRCMode(par))
        && (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR))
        return true;
#endif
    return false;
}
MBQPMode GetMBQPMode(const mfxVideoParam& par,  mfxU32 maxNumOfROI, mfxU32 ROIDeltaQPSupport, bool MbQpDataSupport, bool bFieldMode)
{
    const mfxExtCodingOption2* pCO2 = (mfxExtCodingOption2*)mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_CODING_OPTION2);
    const mfxExtCodingOption3* pCO3 = (mfxExtCodingOption3*)mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_CODING_OPTION3);

    if (!MbQpDataSupport)
        return MBQPMode_None;
    else if (pCO3 && IsOn(pCO3->EnableMBQP))
        return MBQPMode_ExternalMap;
    else if (pCO2 && IsOn(pCO2->MBBRC) && IsEnctoolsLABRC(par) && (!bFieldMode))
        return MBQPMode_FromEncToolsBRC;
    else if (pCO2 && IsOn(pCO2->MBBRC) && IsEnctoolsLAGS(par) && (!bFieldMode))
        return MBQPMode_FromEncToolsLA;
    else if (ROIViaMBQP(par, maxNumOfROI, ROIDeltaQPSupport) && (!bFieldMode))
        return MBQPMode_ForROI;
    else if (IsEnctoolsALQOffset(par) && (!bFieldMode))
        return MBQPMode_ForALQOffset;


    return MBQPMode_None;
}

