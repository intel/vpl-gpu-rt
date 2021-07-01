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

#if defined(ANDROID)
    #include "mfx_android_defs.h"

    #define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
#else
    // mfxconfig.h is auto-generated file containing mediasdk per-component
    // enable defines
    #include "mfxconfig.h"

#endif

// Here follows per-codec feature enable options which as of now we don't
// want to expose on build system level since they are too detailed.
#define SYNCHRONIZATION_BY_VA_MAP_BUFFER
#if !defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)
    #define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
#endif

#define CMAPIUPDATE

#define UMC_ENABLE_FIO_READER
#define UMC_ENABLE_VC1_SPLITTER

#if !defined(NDEBUG)
#define MFX_ENV_CFG_ENABLE
#endif

#if defined(MFX_ENABLE_CPLIB)
#define MFX_ENABLE_CP
#endif

#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE) || defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE)
#define MFX_ENABLE_MJPEG_VIDEO_CODEC
#endif

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE) || defined (UMC_ENABLE_VC1_SPLITTER) || defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
#define MFX_ENABLE_VC1_VIDEO_CODEC
#endif

// Per component configs
#include "mfx_config_decode.h"
#include "mfx_config_encode.h"
#include "mfx_config_vpp.h"

#endif // _MFX_CONFIG_H_
