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

#ifndef _MFX_UNIFIED_LOGGING_H_
#define _MFX_UNIFIED_LOGGING_H_

#include "mfx_config.h"

#include "umc_va_base.h"
#include "umc_vp9_frame.h"
#include "mfx_unified_decode_logging.h"

typedef struct _EVENTDATA_SURFACEOUT_VP9D
{
    uint32_t  CropH;
    uint32_t  CropW;
    uint32_t  CropX;
    uint32_t  CropY;
    uint32_t  ChromaFormat;
    uint32_t  AspectRatioH;
    uint32_t  AspectRatioW;
    uint32_t  FrameRateExtD;
    uint32_t  FrameRateExtN;
    uint32_t  PicStruct;
    uint32_t  DataFlag;
    uint32_t  FrameOrder;
    uint32_t  TimeStamp;
} EVENTDATA_SURFACEOUT_VP9D;

typedef struct _EVENTDATA_PICTUREPARAM_VP9
{
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t reference_frames[8];
    uint32_t subsampling_x;
    uint32_t subsampling_y;
    uint32_t frame_type;
    uint32_t show_frame;
    uint32_t error_resilient_mode;
    uint32_t intra_only;
    uint32_t allow_high_precision_mv;
    uint32_t mcomp_filter_type;
    uint32_t frame_parallel_decoding_mode;
    uint32_t reset_frame_context;
    uint32_t refresh_frame_context;
    uint32_t frame_context_idx;
    uint32_t segmentation_enabled;
    uint32_t segmentation_temporal_update;
    uint32_t segmentation_update_map;
    uint32_t last_ref_frame;
    uint32_t last_ref_frame_sign_bias;
    uint32_t golden_ref_frame;
    uint32_t golden_ref_frame_sign_bias;
    uint32_t alt_ref_frame;
    uint32_t alt_ref_frame_sign_bias;
    uint32_t lossless_flag;
    uint32_t filter_level;
    uint32_t sharpness_level;
    uint32_t log2_tile_rows;
    uint32_t log2_tile_columns;
    uint32_t frame_header_length_in_bytes;
    uint32_t first_partition_size;
    uint32_t mb_segment_tree_probs[7];
    uint32_t segment_pred_probs[3];
    uint32_t profile;
    uint32_t bit_depth;
} EVENTDATA_PICTUREPARAM_VP9;

typedef struct  _VASegmentParameterVP9_VA
{
    uint32_t segment_reference_enabled;
    uint32_t segment_reference;
    uint32_t segment_reference_skipped;
    uint32_t filter_level[4][2];
    uint32_t luma_ac_quant_scale;
    uint32_t luma_dc_quant_scale;
    uint32_t chroma_ac_quant_scale;
    uint32_t chroma_dc_quant_scale;
} VASegmentParameterVP9_VA;

typedef struct _EVENTDATA_SLICEPARAM_VP9
{
    uint32_t slice_data_size;
    uint32_t slice_data_offset;
    uint32_t slice_data_flag;
    VASegmentParameterVP9_VA seg_param[8];
} EVENTDATA_SLICEPARAM_VP9;

typedef struct _EVENTDATA_OUTPUTFRAME_VP9
{
    uint32_t MemID;
    uint32_t wasDisplayed;
} EVENTDATA_OUTPUTFRAME_VP9;

typedef struct _EVENTDATA_DPBINFO
{
    uint32_t FrameId;
    uint32_t isDecoded;
} EVENTDATA_DPBINFO;

typedef struct _EVENTDATA_DPBINFO_VP9D
{
    uint32_t eventCount;
    EVENTDATA_DPBINFO DpbInfo[16];
} EVENTDATA_DPBINFO_VP9D;

void EventVP9DecodeSurfaceOutparam(EVENTDATA_SURFACEOUT_VP9D* pEventData, mfxFrameSurface1** surface_out);

void EventVP9DecodePicparam(EVENTDATA_PICTUREPARAM_VP9* pEventData, VADecPictureParameterBufferVP9* picParam);

void EventVP9DecodeSegmentParam(EVENTDATA_SLICEPARAM_VP9* pEventData, VASliceParameterBufferVP9* sliceParam);

void EventVP9DecodeDpbInfo(EVENTDATA_DPBINFO_VP9D* pEventData, std::vector<UMC_VP9_DECODER::VP9DecoderFrame> m_submittedFrames);

#endif
