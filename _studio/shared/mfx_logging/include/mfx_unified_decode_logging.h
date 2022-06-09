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
#ifdef MFX_EVENT_TRACE_DUMP_SUPPORTED

#include "umc_va_base.h"

typedef enum
{
    ETW_OFF = 0,
    ETW_API = 1,
    ETW_INFO = 2,
    ETW_BUFFER = 3,
    ETW_API_INFO = 4,
    ETW_API_BUFFER = 5,
    ETW_INFO_BUFFER = 6,
    ETW_ALL = 7,
} mfxEtwLog;

extern mfxEtwLog EtwLogConfig;

typedef struct _DECODE_EVENTDATA_INIT
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
} DECODE_EVENTDATA_INIT;

typedef struct _EventFrameInfo
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
} EventFrameInfo;

typedef struct _DECODE_EVENTDATA_QUERY
{
    uint32_t  NumFrameSuggested;
    uint32_t  NumFrameMin;
    uint32_t  Type;
    EventFrameInfo    Info;
} DECODE_EVENTDATA_QUERY;

typedef struct _DECODE_EVENTDATA_BITSTREAM
{
    uint8_t Data[32];
} DECODE_EVENTDATA_BITSTREAM;

bool SetEtwTaceFromEnv();

void DecodeEventDataInitParam(DECODE_EVENTDATA_INIT* pEventData, mfxVideoParam* par);

void DecodeEventDataQueryParam(DECODE_EVENTDATA_QUERY* pEventData, mfxFrameAllocRequest* request);

void DecodeEventBitstreamInfo(DECODE_EVENTDATA_BITSTREAM* pEventData, UMC::UMCVACompBuffer const* pCompBuffer);
#endif
#endif

