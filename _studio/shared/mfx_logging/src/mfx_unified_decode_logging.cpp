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
#include "mfx_unified_decode_logging.h"

void EventDecodeInitParam(
    EVENTDATA_DECODE_INIT* pEventData,
    mfxVideoParam* par)
{
    pEventData->CodecId = par->mfx.CodecId;
    pEventData->frameWidth = par->mfx.FrameInfo.Width;
    pEventData->frameHeight = par->mfx.FrameInfo.Height;
    pEventData->BitDepthLuma = par->mfx.FrameInfo.BitDepthLuma;
    pEventData->BitDepthChroma = par->mfx.FrameInfo.BitDepthChroma,
    pEventData->frameCropX = par->mfx.FrameInfo.CropX;
    pEventData->frameCropY = par->mfx.FrameInfo.CropY;
    pEventData->frameCropH = par->mfx.FrameInfo.CropH;
    pEventData->frameCropW = par->mfx.FrameInfo.CropW;
    pEventData->framePicStruct = par->mfx.FrameInfo.PicStruct;
    pEventData->frameChromaFormat = par->mfx.FrameInfo.ChromaFormat;
    pEventData->frameAspectRatioW = par->mfx.FrameInfo.AspectRatioW;
    pEventData->frameAspectRatioH = par->mfx.FrameInfo.AspectRatioH;
    pEventData->frameRateExtD = par->mfx.FrameInfo.FrameRateExtD;
    pEventData->frameRateExtN = par->mfx.FrameInfo.FrameRateExtN;
    pEventData->CodecProfile = par->mfx.CodecProfile;
    pEventData->CodecLevel = par->mfx.CodecLevel;
    pEventData->MaxDecFrameBuffering = par->mfx.MaxDecFrameBuffering;
    pEventData->frameFourCC = par->mfx.FrameInfo.FourCC;
}

void EventDecodeQueryParam(
    EVENTDATA_DECODE_QUERY* pEventData,
    mfxFrameAllocRequest* request)
{
    pEventData->NumFrameSuggested = request->NumFrameSuggested;
    pEventData->NumFrameMin = request->NumFrameMin;
    pEventData->Type = request->Type;
    pEventData->Info.ChannelId = request->Info.ChannelId;
    pEventData->Info.BitDepthLuma = request->Info.BitDepthLuma;
    pEventData->Info.BitDepthChroma = request->Info.BitDepthChroma;
    pEventData->Info.Shift = request->Info.Shift;
    pEventData->Info.FourCC = request->Info.FourCC;
    pEventData->Info.Width = request->Info.Width;
    pEventData->Info.Height = request->Info.Height;
    pEventData->Info.CropX = request->Info.CropX;
    pEventData->Info.CropY = request->Info.CropY;
    pEventData->Info.CropW = request->Info.CropW;
    pEventData->Info.CropH = request->Info.CropH;
    pEventData->Info.BufferSize = (uint32_t)request->Info.BufferSize;
    pEventData->Info.FrameRateExtN = request->Info.FrameRateExtN;
    pEventData->Info.FrameRateExtD = request->Info.FrameRateExtD;
    pEventData->Info.AspectRatioW = request->Info.AspectRatioW;
    pEventData->Info.AspectRatioH = request->Info.AspectRatioH;
    pEventData->Info.PicStruct = request->Info.PicStruct;
    pEventData->Info.ChromaFormat = request->Info.ChromaFormat;
}
