// Copyright (c) 2008-2018 Intel Corporation
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

#ifndef _MFX_H264_EX_PARAM_BUF_H_
#define _MFX_H264_EX_PARAM_BUF_H_

#include "mfxvideo.h"

/*------------------- CUCs definition ---------------------------------------*/

/*
    MFX_CUC_VC1_FRAMELABEL
    FrameCUC Extended buffer
    Buffer for storing indexes of inner ME frames.
    Not necessary. Used only for performance optimization.
    Structure: ExtVC1FrameLabel
    MFX_LABEL_FRAMELIST
*/
#define  MFX_CUC_H264_FRAMELABEL MFX_MAKEFOURCC('C','U','1','6')


/*--------------- Additional structures ---------------------------------------*/

typedef struct
{
    mfxI16 index;
    mfxU16 coef;
} MFX_H264_Coefficient;

/*---------Extended buffer descriptions ----------------------------------------*/

struct ExtH264ResidCoeffs
{
    mfxU32         CucId; // MFX_CUC_VC1_FRAMELABEL
    mfxU32         CucSz;
    mfxU32         numIndexes;
    MFX_H264_Coefficient* Coeffs;
};

#endif //_MFX_H264_EX_PARAM_BUF_H_
