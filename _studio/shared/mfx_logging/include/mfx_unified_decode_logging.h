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
#ifndef _MFX_UNIFIED_DECODE_LOGGING_H_
#define _MFX_UNIFIED_DECODE_LOGGING_H_

#include "mfx_config.h"

#include "umc_va_base.h"

typedef struct _EVENTDATA_DECODE_INIT
{
    uint32_t CodecId;
    uint32_t frameWidth;
    uint32_t frameHeight;
    uint32_t BitDepthLuma;
    uint32_t BitDepthChroma;
    uint32_t frameCropX;
    uint32_t frameCropY;
    uint32_t frameCropH;
    uint32_t frameCropW;
    uint32_t framePicStruct;
    uint32_t frameChromaFormat;
    uint32_t frameAspectRatioW;
    uint32_t frameAspectRatioH;
    uint32_t frameRateExtD;
    uint32_t frameRateExtN;
    uint32_t CodecProfile;
    uint32_t CodecLevel;
    uint32_t MaxDecFrameBuffering;
    uint32_t frameFourCC;
} EVENTDATA_DECODE_INIT;

typedef struct _EventFrameDecodeInfo
{
    uint32_t  ChannelId;
    uint32_t  BitDepthLuma;
    uint32_t  BitDepthChroma;
    uint32_t  Shift;
    uint32_t  FourCC;
    uint32_t  Width;
    uint32_t  Height;
    uint32_t  CropX;
    uint32_t  CropY;
    uint32_t  CropW;
    uint32_t  CropH;
    uint32_t  BufferSize;
    uint32_t  FrameRateExtN;
    uint32_t  FrameRateExtD;
    uint32_t  AspectRatioW;
    uint32_t  AspectRatioH;
    uint32_t  PicStruct;
    uint32_t  ChromaFormat;
} EventFrameDecodeInfo;

typedef struct _EVENTDATA_DECODE_QUERY
{
    uint32_t  NumFrameSuggested;
    uint32_t  NumFrameMin;
    uint32_t  Type;
    EventFrameDecodeInfo  Info;
} EVENTDATA_DECODE_QUERY;

typedef struct _EVENTDATA_DECODE_BITSTREAM
{
    uint8_t Data[32];
} EVENTDATA_DECODE_BITSTREAM;

void EventDecodeInitParam(EVENTDATA_DECODE_INIT* pEventData, mfxVideoParam* par);

void EventDecodeQueryParam(EVENTDATA_DECODE_QUERY* pEventData, mfxFrameAllocRequest* request);

#endif //_MFX_UNIFIED_DECODE_LOGGING_H_

