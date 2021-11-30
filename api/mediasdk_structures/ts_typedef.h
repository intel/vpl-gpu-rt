// Copyright (c) 2017-2020 Intel Corporation
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

#pragma once

#define TYPEDEF_MEMBER(base, member, name) \
    struct name : std::decay<decltype(base::member)>::type {};

TYPEDEF_MEMBER(mfxExtOpaqueSurfaceAlloc,  In,                  mfxExtOpaqueSurfaceAlloc_InOut)
TYPEDEF_MEMBER(mfxExtAVCRefListCtrl,      PreferredRefList[0], mfxExtAVCRefListCtrl_Entry)
TYPEDEF_MEMBER(mfxExtPictureTimingSEI,    TimeStamp[0],        mfxExtPictureTimingSEI_TimeStamp)
TYPEDEF_MEMBER(mfxExtAvcTemporalLayers,   Layer[0],            mfxExtAvcTemporalLayers_Layer)
TYPEDEF_MEMBER(mfxExtAVCEncodedFrameInfo, UsedRefListL0[0],    mfxExtAVCEncodedFrameInfo_RefList)
#if _MSC_VER<1914
TYPEDEF_MEMBER(mfxExtVPPVideoSignalInfo,  In,                  mfxExtVPPVideoSignalInfo_InOut)
#else
typedef struct {
    mfxU16  TransferMatrix;
    mfxU16  NominalRange;
    mfxU16  reserved2[6];
} mfxExtVPPVideoSignalInfo_InOut;
#endif
TYPEDEF_MEMBER(mfxExtEncoderROI,          ROI[0],              mfxExtEncoderROI_Entry)
TYPEDEF_MEMBER(mfxExtDirtyRect,           Rect[0],             mfxExtDirtyRect_Entry)
TYPEDEF_MEMBER(mfxExtMoveRect,            Rect[0],             mfxExtMoveRect_Entry)
typedef union { mfxU32 n; char c[4]; } mfx4CC;
typedef mfxExtAVCRefLists::mfxRefPic mfxExtAVCRefLists_mfxRefPic;
typedef mfxExtEncoderIPCMArea::area mfxExtEncoderIPCMArea_area;
