// Copyright (c) 2021-2022 Intel Corporation
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

#ifndef _MFX_CONFIG_DECODE_H_
#define _MFX_CONFIG_DECODE_H_

#if !defined (DECODE_JPEG_ROTATION)
#if !defined(ANDROID) && defined(__linux__)
#define MFX_ENABLE_MJPEG_ROTATE_VPP
#endif
#endif

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE) || defined(MFX_ENABLE_AV1_VIDEO_DECODE)
#define UMC_ENABLE_VP9_AV1_DECODE
#endif

#if defined(MFX_ENABLE_AV1_VIDEO_DECODE)
#undef MFX_ENABLE_AV1_VIDEO_CODEC
#define MFX_ENABLE_AV1_VIDEO_CODEC
#endif


#define DECODE_DEFAULT_TIMEOUT  60000


#ifdef _DEBUG
#define ENABLE_JPEGD_TRACING
#endif
#undef ENABLE_JPEGD_ERROR_LOGGING
//#define ENABLE_JPEGD_ERROR_LOGGING
#undef ENABLE_JPEGD_TRACING


#endif // _MFX_CONFIG_DECODE_H_
