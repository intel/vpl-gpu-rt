// Copyright (c) 2021-2023 Intel Corporation
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

#ifndef __AENC_H__
#define __AENC_H__
#include "mfxstructures.h"
#include "mfx_config.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
        mfxU32 FrameWidth;
        mfxU32 FrameHeight;
        mfxU32 SrcFrameWidth;
        mfxU32 SrcFrameHeight;
        mfxI32 Pitch;
        mfxU32 ColorFormat; //FourCC, e.g. MFX_FOURCC_NV12
        mfxU32 StrictIFrame;
        mfxU32 GopPicSize; //GOP size, from I to I, excluding last I
        mfxU32 MinGopSize; //for adaptive I
        mfxU32 MaxGopSize; //for adaptive I, next condition should be satisfyed (MinGopSize < MaxGopSize - MaxMiniGopSize)
        mfxU32 MaxIDRDist; //IDR distance in frames
        mfxU32 MaxMiniGopSize;
        mfxU32 CodecId;
        mfxU32 NumRefP;
        //toolset
        mfxU32 AGOP;
        mfxU32 ALTR;
        mfxU32 AREF;
        mfxU32 APQ;
    } AEncParam;

#define AENC_MAP_SIZE 128

    typedef struct {
        mfxU32 POC;
        mfxU32 QpY;
        mfxU32 SceneChanged;
        mfxU32 RepeatedFrame;
        mfxU32 TemporalComplexity;
        mfxU32 LTR;
        mfxU32 MiniGopSize;
        mfxU32 PyramidLayer;
        mfxU32 Type; //FrameType, e.g. MFX_FRAMETYPE_I
        mfxI32 DeltaQP;
        mfxU32 ClassAPQ;
        mfxI8  QPDeltaExplicitModulation;
        mfxU32 FeaturesAPQ[4];
        mfxU32 SpatialComplexity;
        mfxU32 KeepInDPB;
        mfxU32 RemoveFromDPBSize;
        mfxU32 RemoveFromDPB[32];
        mfxU32 RefListSize;
        mfxU32 RefList[32];
        mfxU32 LongTermRefListSize;
        mfxU32 LongTermRefList[32];
        mfxU8  PMap[AENC_MAP_SIZE];
    } AEncFrame;

    mfxStatus MFX_CDECL AEncInit(mfxHDL* pthis, AEncParam param);
    void      MFX_CDECL AEncClose(mfxHDL pthis);
    mfxStatus MFX_CDECL AEncProcessFrame(mfxHDL pthis, mfxU32 POC, mfxU8* InFrame, mfxI32 pitch, AEncFrame* OutFrame);
    mfxU16    MFX_CDECL AEncGetIntraDecision(mfxHDL pthis, mfxU32 displayOrder);
    mfxU16    MFX_CDECL AEncGetPersistenceMap(mfxHDL pthis, mfxU32 displayOrder, mfxU8 PMap[AENC_MAP_SIZE]);
    mfxU16    MFX_CDECL AEncGetLastPQp(mfxHDL pthis);
    mfxI8     MFX_CDECL AEncAPQSelect(mfxHDL pthis, mfxU32 SC, mfxU32 TSC, mfxU32 MVSize, mfxU32 Contrast, mfxU32 PyramidLayer, mfxU32 BaseQp);
    void      MFX_CDECL AEncUpdateFrame(mfxHDL pthis, mfxU32 displayOrder, mfxU32 bits, mfxU32 QpY, mfxU32 Type);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
