// Copyright (c) 2012-2019 Intel Corporation
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

#ifndef __VAAPI_EXT_INTERFACE_H__
#define __VAAPI_EXT_INTERFACE_H__

#include "mfx_config.h"

#ifdef MFX_EXTBUFF_GPU_HANG_ENABLE
#define VATriggerCodecHangBufferType ((VABufferType)-16)
#endif // MFX_EXTBUFF_GPU_HANG_ENABLE


#define VA_CODED_BUF_STATUS_HW_TEAR_DOWN 0x4000


#ifdef MFX_ENABLE_VP8_VIDEO_ENCODE
/* below is private VAAPI for VP8 Hybrid encoder */

// entry point for ENC + hybrid PAK
#define VAEntrypointHybridEncSlice          -1

// structure discribing MB data layout returned from driver
typedef struct _VAEncMbDataLayout
{
    unsigned char   MB_CODE_size;
    unsigned int    MB_CODE_offset;
    unsigned int    MB_CODE_stride;
    unsigned char   MV_number;
    unsigned int    MV_offset;
    unsigned int    MV_stride;
} VAEncMbDataLayout;

// this buffer representing VAEncMbDataLayout for function vpgQueryBufferAttributes()
#define VAEncMbDataBufferType               -4

#define FUNC_QUERY_BUFFER_ATTRIBUTES "hybridQueryBufferAttributes"
//#define FUNC_QUERY_BUFFER_ATTRIBUTES "media_QueryBufferAttributes"
//#define FUNC_QUERY_BUFFER_ATTRIBUTES "vpgQueryBufferAttributes"

// VAAPI private extension for quering buffer attributes for different buffer types
// at the moment supports only VAEncMbDataBufferType
typedef VAStatus (*hybridQueryBufferAttributes)(
     VADisplay      dpy,
     VAContextID    context,
     VABufferType   bufferType,
     void           *outputData,
     unsigned int   *outputDataLen);

#define VAEncMiscParameterTypeVP8HybridFrameUpdate -3

typedef struct _VAEncMiscParameterVP8HybridFrameUpdate
{
    // Previous frame bitstream size in bytes used when BRC is enabled.
    // If it is unknown, like for the first couple frames, it is set to 0.
    unsigned int prev_frame_size;
    // Depending on whether app is running in synchronous or asynchronouse mode,
    // it may have the immediate previous frame size or the 2 prior frame size.
    // BRC algorithm is different for the 2 due to different frame types, below flag
    // indicates if prev_frame_size is from 2 previous frames.
    bool         two_prev_frame_flag;

    // Previous frame costs to use for current frame.  If set to 0 the driver will use default values.
    // MBs encoded by Intra(0), LastRef(1), GoldRef(2), or AltRef(3).
    unsigned short ref_frame_cost[4];
    // Mode costs, one per segment. INTRA_NONPRED(0), INTRA_16x16(1), INTRA_8x8(2), INTRA_4x4(3)
    // Note 0-3 correspond to Universal VME Input Message DW2.0.  See BSPEC for details.
    unsigned short intra_mode_cost[4];
    // MB's encoded by 16x16(0), 16x8(1), 8x8(2), 4x4(3).
    unsigned short inter_mode_cost[4];
    // one per segment
    unsigned short intra_non_dc_penalty_16x16;
    // one per segment
    unsigned short intra_non_dc_penalty_4x4;

} VAEncMiscParameterVP8HybridFrameUpdate;

// Segment map parameters needed by BRC when app provides segmentation map.
#define VAEncMiscParameterTypeVP8SegmentMapParams   -4

typedef struct _VAEncMiscParameterVP8SegmentMapParams
{
    // Specifies the QIndex delta of Y1_ac value for BRC to use, one per segment,
    // when the application provides the VP8 MB Segmentation Buffer using
    // VAEncMacroblockMapBufferType.
    char yac_quantization_index_delta[4];
} VAEncMiscParameterVP8SegmentMapParams;
#endif // #ifdef MFX_ENABLE_VP8_VIDEO_ENCODE

#endif // __VAAPI_EXT_INTERFACE_H__
