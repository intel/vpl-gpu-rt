// Copyright (c) 2012-2018 Intel Corporation
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

#ifndef CMRT_EMU
#include "mfx_common.h"
#include "cmrt_cross_platform.h"
#include "cmvm.h"

#define SIZE_OF_CURBEDATA 160u

extern "C"
{
    void EncMB_I(
        vector<mfxU8, SIZE_OF_CURBEDATA> laCURBEData,
        SurfaceIndex SrcSurfIndexRaw,
        SurfaceIndex SrcSurfIndex,
        SurfaceIndex VMEInterPredictionSurfIndex,
        SurfaceIndex MBDataSurfIndex,
        SurfaceIndex FwdFrmMBDataSurfIndex) {}

    void EncMB_P(
        vector<mfxU8, SIZE_OF_CURBEDATA> laCURBEData,
        SurfaceIndex SrcSurfIndexRaw,
        SurfaceIndex SrcSurfIndex,
        SurfaceIndex VMEInterPredictionSurfIndex,
        SurfaceIndex MBDataSurfIndex,
        SurfaceIndex FwdFrmMBDataSurfIndex) {}

    void EncMB_B(
        vector<mfxU8, SIZE_OF_CURBEDATA> laCURBEData,
        SurfaceIndex SrcSurfIndexRaw,
        SurfaceIndex SrcSurfIndex,
        SurfaceIndex VMEInterPredictionSurfIndex,
        SurfaceIndex MBDataSurfIndex,
        SurfaceIndex FwdFrmMBDataSurfIndex) {}

    void DownSampleMB2X() {}

    void DownSampleMB4X() {}

    void HistogramFrame(
        SurfaceIndex INBUF,
        SurfaceIndex OUTBUF,
        uint max_h_pos,
        uint max_v_pos,
        uint off_h,
        uint off_v) {}

    void HistogramFields(
        SurfaceIndex INBUF,
        SurfaceIndex OUTBUF,
        uint max_h_pos,
        uint max_v_pos,
        uint off_h,
        uint off_v) {}
}

#endif // CMRT_EMU