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

#ifndef _MFX_CONFIG_ENCODE_H_
#define _MFX_CONFIG_ENCODE_H_

#define MFX_ENABLE_PARTIAL_BITSTREAM_OUTPUT

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE)
    #undef MFX_ENABLE_H264_PRIVATE_CTRL
    #define MFX_ENABLE_APQ_LQ
    #define MFX_ENABLE_H264_REPARTITION_CHECK
    #define MFX_ENABLE_H264_ROUNDING_OFFSET
    #if defined(MFX_ENABLE_MCTF) && defined(MFX_ENABLE_KERNELS)
        #define MFX_ENABLE_MCTF_IN_AVC
    #endif
    #if !defined(MFX_ENABLE_VIDEO_BRC_COMMON)
        #define MFX_ENABLE_VIDEO_BRC_COMMON
    #endif
    #if !defined(UMC_ENABLE_VIDEO_BRC)
        #define UMC_ENABLE_VIDEO_BRC
    #endif
    #if !defined(MFX_ENABLE_EXT_BRC)
        #define MFX_ENABLE_EXT_BRC
    #endif
#endif

#if defined(MFX_ENABLE_MVC_VIDEO_ENCODE)
    #define MFX_ENABLE_MVC_I_TO_P
    #define MFX_ENABLE_MVC_ADD_REF
    #undef MFX_ENABLE_AVC_BS
#endif

#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)
    #define MFX_ENABLE_HEVCE_INTERLACE
    #define MFX_ENABLE_HEVCE_ROI
    #if !defined(MFX_ENABLE_EXT_BRC)
        #define MFX_ENABLE_EXT_BRC
    #endif
#endif

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)
    #if !defined(MFX_ENABLE_VIDEO_BRC_COMMON)
        #define MFX_ENABLE_VIDEO_BRC_COMMON
    #endif
    #if !defined(UMC_ENABLE_VIDEO_BRC)
        #define UMC_ENABLE_VIDEO_BRC
    #endif
#endif

#define MFX_ENABLE_QVBR

#ifdef MFX_ENABLE_USER_ENCTOOLS
    #define MFX_ENABLE_ENCTOOLS
    #if defined(MFX_ENABLE_AENC)
        #define ENABLE_ADAPTIVE_ENCODE
    #endif
#endif

#endif // _MFX_CONFIG_ENCODE_H_
