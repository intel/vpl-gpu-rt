// Copyright (c) 2004-2019 Intel Corporation
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

#include "umc_defs.h"

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)

#ifndef _UMC_VC1_DEC_VA_DEFS_H_
#define _UMC_VC1_DEC_VA_DEFS_H_

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA


typedef struct _DXVA_ExtPicInfo
{
    BYTE                bBScaleFactor;
    BYTE                bPQuant;
    BYTE                bAltPQuant;

    union
    {
        BYTE            bPictureFlags;
        struct PictureParamsMain
        {
            BYTE        FrameCodingMode     :2;
            BYTE        PictureType         :3;
            BYTE        CondOverFlag        :2;
            BYTE        Reserved1bit        :1;
        };
    };

    union 
    {
        BYTE            bPQuantFlags;
        struct  QuantParams
        {
            BYTE        PQuantUniform       :1;
            BYTE        HalfQP              :1;
            BYTE        AltPQuantConfig     :2;
            BYTE        AltPQuantEdgeMask   :4;
        };
    };
union 
    {
        BYTE            bMvRange;
        struct MVParams
        {
            BYTE        ExtendedMVrange     :2;
            BYTE        ExtendedDMVrange    :2;
            BYTE        Reserved4bits       :4;
        };
    };

    union 
    {
        WORD            wMvReference;
        struct ReferenceParams
        {
            WORD        ReferenceDistance           :4;
            WORD        BwdReferenceDistance        :4;

            WORD        NumberOfReferencePictures   :1;
            WORD        ReferenceFieldPicIndicator  :1;
            WORD        FastUVMCflag                :1;
            WORD        FourMvSwitch                :1;
            WORD        UnifiedMvMode               :2;
            WORD        IntensityCompensationField  :2;
        };
    };


    union
    {
        WORD            wTransformFlags;
        struct TblParams
        {
            WORD        CBPTable            :3;
            WORD        TransDCtable        :1;
            WORD        TransACtable        :2;
            WORD        TransACtable2       :2;

            WORD        MbModeTable         :3;
            WORD        TTMBF               :1;
            WORD        TTFRM               :2;
            WORD        Reserved2bits       :2;
        };
    };

    union
    {
        BYTE            bMvTableFlags;
        struct MvTblPArams
        {
            BYTE        TwoMVBPtable            :2;
            BYTE        FourMVBPtable           :2;
            BYTE        MvTable                 :3;
            BYTE        reserved1bit            :1;
        };
    };

    union
    {
        BYTE            bRawCodingFlag;
        struct BPParams
        {
            BYTE        FieldTX             :1;
            BYTE        ACpred              :1;
            BYTE        Overflags           :1;
            BYTE        DirectMB            :1;
            BYTE        SkipMB              :1;
            BYTE        MvTypeMB            :1;
            BYTE        ForwardMB           :1;
            BYTE        IsBitplanePresent   :1;
        };
    };
} DXVA_ExtPicInfo;

#endif

#endif //_UMC_VC1_DEC_VA_DEFS_H_
#endif //MFX_ENABLE_VC1_VIDEO_DECODE
