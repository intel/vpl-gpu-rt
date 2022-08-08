// Copyright (c) 2021 Intel Corporation
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

#include "mfx_unified_vp9d_logging.h"

void DecodeEventVP9DataSurfaceOutparam(
    DECODE_EVENTDATA_SURFACEOUT_VP9* pEventData,
    mfxFrameSurface1** surface_out)
{
    pEventData->CropH = (*surface_out)->Info.CropH;
    pEventData->CropW = (*surface_out)->Info.CropW;
    pEventData->CropX = (*surface_out)->Info.CropX;
    pEventData->CropY = (*surface_out)->Info.CropY;
    pEventData->ChromaFormat = (*surface_out)->Info.ChromaFormat;
    pEventData->AspectRatioH = (*surface_out)->Info.AspectRatioH;
    pEventData->AspectRatioW = (*surface_out)->Info.AspectRatioW;
    pEventData->FrameRateExtD = (*surface_out)->Info.FrameRateExtD;
    pEventData->FrameRateExtN = (*surface_out)->Info.FrameRateExtN;
    pEventData->PicStruct = (*surface_out)->Info.PicStruct;
    pEventData->DataFlag = (*surface_out)->Data.DataFlag;
    pEventData->FrameOrder = (*surface_out)->Data.FrameOrder;
    pEventData->TimeStamp = (uint32_t)(*surface_out)->Data.TimeStamp;
}
void DecodeEventVP9DpbInfo(
    DECODE_EVENTDATA_DPBINFO_VP9* pEventData,
    std::vector<UMC_VP9_DECODER::VP9DecoderFrame> m_submittedFrames)
{
    pEventData->eventCount = m_submittedFrames.size();
    for (uint32_t i = 0; i < m_submittedFrames.size(); i++)
    {
        pEventData->DpbInfo[i].FrameId = m_submittedFrames[i].currFrame;
        pEventData->DpbInfo[i].isDecoded = m_submittedFrames[i].isDecoded;
    }
}