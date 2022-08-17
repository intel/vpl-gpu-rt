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

#include "mfx_unified_h264d_logging.h"

void EventH264DecodeSurfaceOutparam(
    EVENTDATA_SURFACEOUT_H264D* pEventData,
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
    pEventData->FrameOrder = surface_out->Data.FrameOrder;
    pEventData->PicStruct = surface_out->Info.PicStruct;
    pEventData->DataFlag = surface_out->Data.DataFlag;
    pEventData->TimeStamp = (uint32_t)surface_out->Data.TimeStamp;
}

void EventH264DecodeDpbInfo(
    EVENTDATA_DPBINFO_H264D* pEventData,
    UMC::H264DBPList* pDPBList)
{
    int32_t j = 0;
    int32_t dpbSize = pDPBList->GetDPBSize();
    int32_t start = j;
    for (UMC::H264DecoderFrame* pFrm = pDPBList->head(); pFrm && (j < dpbSize + start); pFrm = pFrm->future())
    {
        pEventData->DpbInfo[j].PicOrderCnt[0] = pFrm->m_PicOrderCnt[0];
        pEventData->DpbInfo[j].PicOrderCnt[1] = pFrm->m_PicOrderCnt[1];
        pEventData->DpbInfo[j].FrameId = pFrm->GetFrameData()->GetFrameMID();
        pEventData->DpbInfo[j].isShortTermRef = pFrm->isShortTermRef();
        pEventData->DpbInfo[j].isLongTermRef = pFrm->isLongTermRef();
        pEventData->DpbInfo[j].refCounter = pFrm->GetRefCounter();
        j++;
    }
    pEventData->eventCount = j;
}
