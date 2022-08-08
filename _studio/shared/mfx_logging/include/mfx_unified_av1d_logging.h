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

typedef struct _DECODE_EVENTDATA_SURFACEOUT_AV1
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
    uint32_t  TimeStamp;
} DECODE_EVENTDATA_SURFACEOUT_AV1;

typedef struct _DECODE_EVENTDATA_OUTPUTFRAME_AV1
{
    uint32_t MemID;
    uint32_t wasDisplayed;
    uint32_t wasOutputted;
} DECODE_EVENTDATA_OUTPUTFRAME_AV1;

typedef struct _EVENTDATA_AV1DPBINFO {
    int32_t MemID;
    uint32_t Refvalid;
    int32_t RefCounter;
} EVENTDATA_AV1DPBINFO;
typedef struct _DECODE_EVENTDATA_DPBINFO_AV1
{
    EVENTDATA_AV1DPBINFO DpbInfo[6];
} DECODE_EVENTDATA_DPBINFO_AV1;

typedef struct _DECODE_EVENTDATA_SYNC_AV1
{
    uint32_t m_index;
    uint32_t isDecodingCompleted;
    uint32_t isDisplayable;
    uint32_t isOutputted;
    uint32_t event_sts;

} DECODE_EVENTDATA_SYNC_AV1;

typedef struct _EVENT_TILE {
    uint32_t m_cols;
    uint32_t m_rows;
    uint32_t m_context_update_id;
} EVENT_TILE;

typedef struct _EVENT_CODING {
    uint32_t m_use_128x128_superblock;
    uint32_t m_intra_edge_filter;
    uint32_t m_interintra_compound;
    uint32_t m_masked_compound;
    uint32_t m_warped_motion;
    uint32_t m_dual_filter;
    uint32_t m_jnt_comp;
    uint32_t m_screen_content_tools;
    uint32_t m_integer_mv;
    uint32_t m_cdef;
    uint32_t m_restoration;
    uint32_t m_film_grain;
    uint32_t m_intrabc;
    uint32_t m_high_precision_mv;
    uint32_t m_switchable_motion_mode;
    uint32_t m_filter_intra;
    uint32_t m_disable_frame_end_update_cdf;
    uint32_t m_disable_cdf_update;
    uint32_t m_reference_mode;
    uint32_t m_skip_mode;
    uint32_t m_reduced_tx_set;
    uint32_t m_superres;
    uint32_t m_tx_mode;
    uint32_t m_use_ref_frame_mvs;
    uint32_t m_enable_ref_frame_mvs;
    uint32_t m_reference_frame_update;
} EVENT_CODING;

typedef struct _EVENT_FORMAT {
    uint32_t m_frame_type;
    uint32_t m_show_frame;
    uint32_t m_showable_frame;
    uint32_t m_subsampling_x;
    uint32_t m_subsampling_y;
    uint32_t m_mono_chrome;
} EVENT_FORMAT;

typedef struct _EVENT_PicEntry_AV1_MSFT {

    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_wmmat[6];
    uint32_t m_wminvalid;
    uint32_t m_wmtype;
    uint32_t m_Index;
} EVENT_PicEntry_AV1_MSFT;

typedef struct _EVENT_LOOP_FILTER {
    uint32_t m_filter_level[2];
    uint32_t m_filter_level_u;
    uint32_t m_filter_level_v;
    uint32_t m_sharpness_level;
    uint32_t m_mode_ref_delta_enabled;
    uint32_t m_mode_ref_delta_update;
    uint32_t m_delta_lf_multi;
    uint32_t m_delta_lf_present;
    int32_t  m_ref_deltas[8];
    uint32_t m_mode_deltas[2];
    uint32_t m_delta_lf_res;
    uint32_t m_frame_restoration_type[3];
    uint32_t m_log2_restoration_unit_size[3];
} EVENT_LOOP_FILTER;

typedef struct _EVENT_QUANTIZATION {
    uint32_t m_delta_q_present;
    uint32_t m_delta_q_res;
    uint32_t m_base_qindex;
    uint32_t m_y_dc_delta_q;
    uint32_t m_u_dc_delta_q;
    uint32_t m_v_dc_delta_q;
    uint32_t m_u_ac_delta_q;
    uint32_t m_v_ac_delta_q;
    uint32_t m_qm_y;
    uint32_t m_qm_u;
    uint32_t m_qm_v;
} EVENT_QUANTIZATION;

typedef struct __EVENT_Y_STRENGTH {
    uint32_t m_primary;
    uint32_t m_secondary;
} EVENT_STRENGTH;
// Cdef parameters
typedef struct _EVENT_CDEF {
    uint32_t m_damping;
    uint32_t m_bits;
} EVENT_CDEF;

typedef struct _EVENT_FEATURE_MASK {
    uint32_t m_alt_q;
    uint32_t m_alt_lf_y_v;
    uint32_t m_alt_lf_y_h;
    uint32_t m_alt_lf_u;
    uint32_t m_alt_lf_v;
    uint32_t m_ref_frame;
    uint32_t m_skip;
    uint32_t m_globalmv;
} EVENT_FEATURE_MASK;

typedef struct _EVENT_SEGMENTATION {
    uint32_t m_enabled;
    uint32_t m_update_map;
    uint32_t m_update_data;
    uint32_t m_temporal_update;
} EVENT_SEGMENTATION;

typedef struct _EVENT_FILM_GRAIN {
    uint32_t m_apply_grain;
    uint32_t m_scaling_shift_minus8;
    uint32_t m_chroma_scaling_from_luma;
    uint32_t m_ar_coeff_lag;
    uint32_t m_ar_coeff_shift_minus6;
    uint32_t m_grain_scale_shift;
    uint32_t m_overlap_flag;
    uint32_t m_clip_to_restricted_range;
    uint32_t m_matrix_coeff_is_identity;
    uint32_t m_grain_seed;
    uint32_t m_scaling_points_y[14][2];
    uint32_t m_num_y_points;
    uint32_t m_scaling_points_cb[10][2];
    uint32_t m_num_cb_points;
    uint32_t m_scaling_points_cr[10][2];
    uint32_t m_num_cr_points;
    uint32_t m_cb_mult;
    uint32_t m_cb_luma_mult;
    uint32_t m_cr_mult;
    uint32_t m_cr_luma_mult;
    uint32_t m_cb_offset;
    uint32_t m_cr_offset;
} EVENT_FILM_GRAIN;

typedef struct _DECODE_EVENTDATA_PICTUREPARAM_AV1 
{
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_max_width;
    uint32_t m_max_height;
    uint32_t m_CurrPicTextureIndex;
    uint32_t m_superres_denom;
    uint32_t m_bitdepth;
    uint32_t m_seq_profile;
    EVENT_TILE m_tiles;
    uint32_t m_tile_widths[64];
    uint32_t m_tile_heights[64];
    EVENT_CODING m_coding;
    EVENT_FORMAT m_format;
    uint32_t m_primary_ref_frame;
    uint32_t m_order_hint;
    uint32_t m_order_hint_bits;
    EVENT_PicEntry_AV1_MSFT m_frame_refs[7];
    uint32_t m_RefFrameMapTextureIndex[8];
    EVENT_LOOP_FILTER m_loop_filter;
    EVENT_QUANTIZATION m_quantization;
    EVENT_CDEF m_cdef;
    EVENT_STRENGTH m_y_strengths[8];
    EVENT_STRENGTH m_uv_strengths[8];
    uint32_t m_interp_filter;
    EVENT_SEGMENTATION m_segmentation;
    uint32_t m_segmentation_feature_data[8][8];
    EVENT_FEATURE_MASK m_feature_mask[8];
    EVENT_FILM_GRAIN m_film_grain;
    uint32_t m_film_grain_ar_coeffs_y[24];
    uint32_t m_film_grain_ar_coeffs_cb[25];
    uint32_t m_film_grain_ar_coeffs_cr[25];
    uint32_t m_StatusReportFeedbackNumber;
} DECODE_EVENTDATA_PICTUREPARAM_AV1;

typedef struct _DECODE_EVENTDATA_TILECONTROLPARAMS_AV1
{
    uint32_t m_DataOffset;
    uint32_t m_DataSize;
    uint32_t m_row;
    uint32_t m_column;
    uint32_t m_anchor_frame;
}DECODE_EVENTDATA_TILECONTROLPARAMS_AV1;

void DecodeEventDataAV1SurfaceOutparam(DECODE_EVENTDATA_SURFACEOUT_AV1* pEventData, mfxFrameSurface1* surface_out);


void DecodeEventDpbInfoAV1(DECODE_EVENTDATA_DPBINFO_AV1* pEventData, std::vector<UMC_AV1_DECODER::AV1DecoderFrame*> updated_refs);


#endif

