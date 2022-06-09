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

#include "mfx_unified_av1d_logging.h"

#ifdef MFX_EVENT_TRACE_DUMP_SUPPORTED

void DecodeEventDataAV1SurfaceOutparam(
    DECODE_EVENTDATA_SURFACEOUT_AV1* pEventData,
    mfxFrameSurface1* surface_out)
{
    pEventData->CropH = surface_out->Info.CropH;
    pEventData->CropW = surface_out->Info.CropW;
    pEventData->CropX = surface_out->Info.CropX;
    pEventData->CropY = surface_out->Info.CropY;
    pEventData->ChromaFormat = surface_out->Info.ChromaFormat;
    pEventData->AspectRatioH = surface_out->Info.AspectRatioH;
    pEventData->AspectRatioW = surface_out->Info.AspectRatioW;
    pEventData->FrameRateExtD = surface_out->Info.FrameRateExtD;
    pEventData->FrameRateExtN = surface_out->Info.FrameRateExtN;
    pEventData->PicStruct = surface_out->Info.PicStruct;
    pEventData->DataFlag = surface_out->Data.DataFlag;
    pEventData->TimeStamp = (uint32_t)surface_out->Data.TimeStamp;
}
void DecodeEventDpbInfoAV1(
    DECODE_EVENTDATA_DPBINFO_AV1* pEventData,
    std::vector<UMC_AV1_DECODER::AV1DecoderFrame*> updated_refs)
{
    for (uint8_t i = 0; i < 6; ++i)
    {
        pEventData->DpbInfo[i].MemID = updated_refs[i]->GetMemID();
        pEventData->DpbInfo[i].Refvalid = updated_refs[i]->RefValid();
        pEventData->DpbInfo[i].RefCounter = updated_refs[i]->GetRefCounter();
    }
}
#endif