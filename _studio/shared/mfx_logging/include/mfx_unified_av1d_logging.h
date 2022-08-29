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
#ifndef _MFX_UNIFIED_AV1D_LOGGING_H_
#define _MFX_UNIFIED_AV1D_LOGGING_H_

#include "mfx_config.h"

#include "umc_va_base.h"
#include "umc_av1_frame.h"
#include "mfx_unified_decode_logging.h"

typedef struct _EVENTDATA_SURFACEOUT_AV1D
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
    uint32_t  FrameOrder;
    uint32_t  PicStruct;
    uint32_t  DataFlag;
    uint32_t  TimeStamp;
} EVENTDATA_SURFACEOUT_AV1D;

typedef struct _DECODE_EVENTDATA_AV1DPBINFO
{
    int32_t MemID;
    uint32_t Refvalid;
    int32_t RefCounter;
} DECODE_EVENTDATA_AV1DPBINFO;

typedef struct _EVENTDATA_DPBINFO_AV1D
{
    DECODE_EVENTDATA_AV1DPBINFO DpbInfo[6];
} EVENTDATA_DPBINFO_AV1D;

typedef struct _EVENT_SEG_INFO_FIELDS
{
    uint32_t still_picture;
    uint32_t use_128x128_superblock;
    uint32_t enable_filter_intra;
    uint32_t enable_intra_edge_filter;
    uint32_t enable_interintra_compound;
    uint32_t enable_masked_compound;
    uint32_t enable_cdef;
    uint32_t enable_dual_filter;
    uint32_t enable_order_hint;
    uint32_t enable_jnt_comp;
    uint32_t mono_chrome;
    uint32_t color_range;
    uint32_t subsampling_x;
    uint32_t subsampling_y;
    uint32_t chroma_sample_position;
    uint32_t film_grain_params_present;
} EVENT_SEG_INFO_FIELDS;

typedef struct _EVENT_PIC_INFO_FIELDS
{
    uint32_t frame_type;
    uint32_t show_frame;
    uint32_t showable_frame;
    uint32_t error_resilient_mode;
    uint32_t disable_cdf_update;
    uint32_t allow_screen_content_tools;
    uint32_t force_integer_mv;
    uint32_t allow_intrabc;
    uint32_t use_superres;
    uint32_t allow_high_precision_mv;
    uint32_t is_motion_mode_switchable;
    uint32_t disable_frame_end_update_cdf;
    uint32_t uniform_tile_spacing_flag;
    uint32_t allow_warped_motion;
    uint32_t large_scale_tile;
} EVENT_PIC_INFO_FIELDS;

typedef struct _EVENT_SEGMENTATION
{
    uint32_t enabled;
    uint32_t update_map;
    uint32_t temporal_update;
    uint32_t update_data;
    uint32_t feature_mask[8];
} EVENT_SEGMENTATION;

typedef struct _EVENT_LOOP_FILTER
{
    uint32_t sharpness_level;
    uint32_t mode_ref_delta_enabled;
    uint32_t mode_ref_delta_update;
} EVENT_LOOP_FILTER;

typedef struct _EVENT_PicEntry_AV1
{
    uint32_t wmtype;
    int32_t wmmat[8];
    uint32_t invalid;
} EVENT_PicEntry_AV1;

typedef struct _EVENTDATA_PICTUREPARAM_AV1D
{
    uint32_t frame_width_minus1;
    uint32_t frame_height_minus1;
    uint32_t output_frame_width_in_tiles_minus_1;
    uint32_t output_frame_height_in_tiles_minus_1;
    uint32_t profile;
    EVENT_SEG_INFO_FIELDS seqInfo;
    uint32_t matrix_coefficients;
    uint32_t bit_depth_idx;
    uint32_t order_hint_bits_minus_1;
    EVENT_PIC_INFO_FIELDS picInfo;
    uint32_t tile_count_minus_1;
    uint32_t anchor_frames_num;
    uint32_t order_hint;
    uint32_t superres_scale_denominator;
    uint32_t interp_filter;
    EVENT_SEGMENTATION seg;
    uint32_t seg_feature_data[8][8];
    uint32_t current_frame;
    uint32_t current_display_picture;
    uint32_t ref_frame_map[8];
    uint32_t ref_frame_idx[7];
    uint32_t primary_ref_frame;
    uint32_t filter_level[2];
    uint32_t filter_level_u;
    uint32_t filter_level_v;
    EVENT_LOOP_FILTER lfInfo;
    int32_t ref_deltas[8];
    int32_t mode_deltas[2];
    uint32_t base_qindex;
    int32_t y_dc_delta_q;
    int32_t u_dc_delta_q;
    int32_t v_dc_delta_q;
    int32_t u_ac_delta_q;
    int32_t v_ac_delta_q;
    uint32_t cdef_damping_minus_3;
    uint32_t cdef_bits;
    uint32_t cdef_y_strengths[8];
    uint32_t cdef_uv_strengths[8];
    EVENT_PicEntry_AV1 wm[7];
    uint32_t tile_cols;
    uint32_t tile_rows;
    uint32_t width_in_sbs_minus_1[63];
    uint32_t height_in_sbs_minus_1[63];
    uint32_t context_update_tile_id;
} EVENTDATA_PICTUREPARAM_AV1D;

typedef struct _EVENTDATA_TILECONTROLPARAMS_AV1D
{
    uint32_t slice_data_offset;
    uint32_t slice_data_size;
    uint32_t slice_data_flag;
    uint32_t tile_row;
    uint32_t tile_column;
    uint32_t tg_start;
    uint32_t tg_end;
    uint32_t anchor_frame_idx;
    uint32_t tile_idx_in_tile_list;
} EVENTDATA_TILECONTROLPARAMS_AV1D;

void EventAV1DecodeSurfaceOutparam(EVENTDATA_SURFACEOUT_AV1D* pEventData, mfxFrameSurface1* surface_out);

void EventAV1DecodePicparam(EVENTDATA_PICTUREPARAM_AV1D* pEventData, VADecPictureParameterBufferAV1& picParam);

void EventAV1DecodeTileControlparam(EVENTDATA_TILECONTROLPARAMS_AV1D* pEventData, VASliceParameterBufferAV1& tileControlParam);

void EventAV1DecodeDpbInfo(EVENTDATA_DPBINFO_AV1D* pEventData, std::vector<UMC_AV1_DECODER::AV1DecoderFrame*> updated_refs);


#endif

