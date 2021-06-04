// Copyright (c) 2016-2021 Intel Corporation
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

#ifndef _MFX_CONFIG_H_
#define _MFX_CONFIG_H_

#include "mfxdefs.h"

#include <va/va_version.h>

#undef  UMC_VA_LINUX
#define UMC_VA_LINUX

#if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN)
    #if defined(HEVCE_EVALUATION)
        #define MFX_MAX_ENCODE_FRAMES 1000
    #endif
    #if defined(HEVCD_EVALUATION)
        #define MFX_MAX_DECODE_FRAMES 1000
    #endif
#endif

#if defined(ANDROID)
    #include "mfx_android_defs.h"

    #define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
#else
    // mfxconfig.h is auto-generated file containing mediasdk per-component
    // enable defines
    #include "mfxconfig.h"

    #if defined(AS_H264LA_PLUGIN)
        #define MFX_ENABLE_LA_H264_VIDEO_HW
    #endif
#endif

#define MFX_PRIVATE_AVC_ENCODE_CTRL_DISABLE

// closed source fixed-style defines
#if !defined(ANDROID) && defined(__linux__)
    #define MFX_ENABLE_MJPEG_ROTATE_VPP
#endif


#define DECODE_DEFAULT_TIMEOUT  60000

// Here follows per-codec feature enable options which as of now we don't
// want to expose on build system level since they are too detailed.
#if defined(MFX_ENABLE_MPEG2_VIDEO_DECODE)
    #define MFX_ENABLE_HW_ONLY_MPEG2_DECODER
#endif //MFX_ENABLE_MPEG2_VIDEO_DECODE

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE)
    #define MFX_ENABLE_H264_VIDEO_ENCODE_HW
    #if MFX_VERSION >= 1023
        #define MFX_ENABLE_H264_REPARTITION_CHECK
    #endif
    #if MFX_VERSION >= 1027
        #define MFX_ENABLE_H264_ROUNDING_OFFSET
    #endif
    #if defined(MFX_ENABLE_MCTF) && defined(MFX_ENABLE_KERNELS)
        #define MFX_ENABLE_MCTF_IN_AVC
    #endif
#endif

#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)
    #define MFX_ENABLE_HEVCE_INTERLACE
    #define MFX_ENABLE_HEVCE_ROI
#endif


    #if defined(MFX_ENABLE_MPEG2_VIDEO_DECODE)
        #define MFX_ENABLE_HW_ONLY_MPEG2_DECODER
    #endif

    #if defined(MFX_ENABLE_VP9_VIDEO_ENCODE)
        #define MFX_ENABLE_VP9_VIDEO_ENCODE_HW
    #endif


    #define SYNCHRONIZATION_BY_VA_MAP_BUFFER
    #if !defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)
        #define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
    #endif

#if defined(MFX_ENABLE_VP9_VIDEO_ENCODE)
    #define MFX_ENABLE_VP9_VIDEO_ENCODE_HW
#endif


#if defined(MFX_ENABLE_VPP)
    #define MFX_ENABLE_VPP_COMPOSITION
    #define MFX_ENABLE_VPP_ROTATION
    #define MFX_ENABLE_VPP_VIDEO_SIGNAL

        #define MFX_ENABLE_DENOISE_VIDEO_VPP
        #define MFX_ENABLE_MJPEG_WEAVE_DI_VPP
        #define MFX_ENABLE_MJPEG_ROTATE_VPP
        #define MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP
        #define MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION

    #if MFX_VERSION >= MFX_VERSION_NEXT
        #define MFX_ENABLE_VPP_RUNTIME_HSBC
    #endif

    #undef MFX_ENABLE_VPP_FIELD_WEAVE_SPLIT

    //#define MFX_ENABLE_VPP_FRC
#endif

#if defined(MFX_ENABLE_ASC)
    #define MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP
#endif

#if MFX_VERSION >= 1028
    #define MFX_ENABLE_RGBP
    #define MFX_ENABLE_FOURCC_RGB565
#endif

#if MFX_VERSION >= 1031
    #define MFX_ENABLE_PARTIAL_BITSTREAM_OUTPUT
#endif

#define MFX_ENABLE_QVBR

#define CMAPIUPDATE

#define UMC_ENABLE_FIO_READER
#define UMC_ENABLE_VC1_SPLITTER

#if !defined(NDEBUG)
#define MFX_ENV_CFG_ENABLE
#endif

#if defined(MFX_ENABLE_CPLIB)
#define MFX_ENABLE_CP
#endif
#endif // _MFX_CONFIG_H_
