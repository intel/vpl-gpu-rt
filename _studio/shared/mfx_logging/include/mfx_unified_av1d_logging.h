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
#ifdef MFX_EVENT_TRACE_DUMP_SUPPORTED

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

typedef struct _EVENTDATA_DPBINFO {
    int32_t MemID;
    uint32_t Refvalid;
    int32_t RefCounter;
} EVENTDATA_DPBINFO;
typedef struct _DECODE_EVENTDATA_DPBINFO_AV1
{
    EVENTDATA_DPBINFO DpbInfo[6];
} DECODE_EVENTDATA_DPBINFO_AV1;

typedef struct _DECODE_EVENTDATA_SYNC_AV1
{
    uint32_t m_index;
    uint32_t isDecodingCompleted;
    uint32_t isDisplayable;
    uint32_t isOutputted;

} DECODE_EVENTDATA_SYNC_AV1;

typedef struct _EVENT_TILE {
    uint32_t cols;
    uint32_t rows;
    uint32_t context_update_id;
} EVENT_TILE;

typedef struct _EVENT_CODING {
    uint32_t use_128x128_superblock;
    uint32_t intra_edge_filter;
    uint32_t interintra_compound;
    uint32_t masked_compound;
    uint32_t warped_motion;
    uint32_t dual_filter;
    uint32_t jnt_comp;
    uint32_t screen_content_tools;
    uint32_t integer_mv;
    uint32_t cdef;
    uint32_t restoration;
    uint32_t film_grain;
    uint32_t intrabc;
    uint32_t high_precision_mv;
    uint32_t switchable_motion_mode;
    uint32_t filter_intra;
    uint32_t disable_frame_end_update_cdf;
    uint32_t disable_cdf_update;
    uint32_t reference_mode;
    uint32_t skip_mode;
    uint32_t reduced_tx_set;
    uint32_t superres;
    uint32_t tx_mode;
    uint32_t use_ref_frame_mvs;
    uint32_t enable_ref_frame_mvs;
    uint32_t reference_frame_update;
} EVENT_CODING;

typedef struct _EVENT_FORMAT {
    uint32_t frame_type;
    uint32_t show_frame;
    uint32_t showable_frame;
    uint32_t subsampling_x;
    uint32_t subsampling_y;
    uint32_t mono_chrome;
} EVENT_FORMAT;

typedef struct _EVENT_PicEntry_AV1_MSFT {

    uint32_t width;
    uint32_t height;
    uint32_t wmmat[6];
    uint32_t wminvalid;
    uint32_t wmtype;
    uint32_t Index;
} EVENT_PicEntry_AV1_MSFT;

typedef struct _EVENT_LOOP_FILTER {
    uint32_t filter_level[2];
    uint32_t filter_level_u;
    uint32_t filter_level_v;
    uint32_t sharpness_level;
    uint32_t mode_ref_delta_enabled;
    uint32_t mode_ref_delta_update;
    uint32_t delta_lf_multi;
    uint32_t delta_lf_present;
    int32_t  ref_deltas[8];
    uint32_t mode_deltas[2];
    uint32_t delta_lf_res;
    uint32_t frame_restoration_type[3];
    uint32_t log2_restoration_unit_size[3];
} EVENT_LOOP_FILTER;

typedef struct _EVENT_QUANTIZATION {
    uint32_t delta_q_present;
    uint32_t delta_q_res;
    uint32_t base_qindex;
    uint32_t y_dc_delta_q;
    uint32_t u_dc_delta_q;
    uint32_t v_dc_delta_q;
    uint32_t u_ac_delta_q;
    uint32_t v_ac_delta_q;
    uint32_t qm_y;
    uint32_t qm_u;
    uint32_t qm_v;
} EVENT_QUANTIZATION;

typedef struct __EVENT_Y_STRENGTH {
    uint32_t primary;
    uint32_t secondary;
} EVENT_STRENGTH;
// Cdef parameters
typedef struct _EVENT_CDEF {
    uint32_t damping;
    uint32_t bits;
} EVENT_CDEF;

typedef struct _EVENT_FEATURE_MASK {
    uint32_t alt_q;
    uint32_t alt_lf_y_v;
    uint32_t alt_lf_y_h;
    uint32_t alt_lf_u;
    uint32_t alt_lf_v;
    uint32_t ref_frame;
    uint32_t skip;
    uint32_t globalmv;
} EVENT_FEATURE_MASK;

typedef struct _EVENT_SEGMENTATION {
    uint32_t enabled;
    uint32_t update_map;
    uint32_t update_data;
    uint32_t temporal_update;
} EVENT_SEGMENTATION;

typedef struct _EVENT_FILM_GRAIN {
    uint32_t apply_grain;
    uint32_t scaling_shift_minus8;
    uint32_t chroma_scaling_from_luma;
    uint32_t ar_coeff_lag;
    uint32_t ar_coeff_shift_minus6;
    uint32_t grain_scale_shift;
    uint32_t overlap_flag;
    uint32_t clip_to_restricted_range;
    uint32_t matrix_coeff_is_identity;
    uint32_t grain_seed;
    uint32_t scaling_points_y[14][2];
    uint32_t num_y_points;
    uint32_t scaling_points_cb[10][2];
    uint32_t num_cb_points;
    uint32_t scaling_points_cr[10][2];
    uint32_t num_cr_points;
    uint32_t cb_mult;
    uint32_t cb_luma_mult;
    uint32_t cr_mult;
    uint32_t cr_luma_mult;
    uint32_t cb_offset;
    uint32_t cr_offset;
} EVENT_FILM_GRAIN;

typedef struct _DECODE_EVENTDATA_PICTUREPARAM_AV1 
{
    uint32_t width;
    uint32_t height;
    uint32_t max_width;
    uint32_t max_height;
    uint32_t CurrPicTextureIndex;
    uint32_t superres_denom;
    uint32_t bitdepth;
    uint32_t seq_profile;
    EVENT_TILE tiles;
    uint32_t tile_widths[64];
    uint32_t tile_heights[64];
    EVENT_CODING coding;
    EVENT_FORMAT format;
    uint32_t primary_ref_frame;
    uint32_t order_hint;
    uint32_t order_hint_bits;
    EVENT_PicEntry_AV1_MSFT frame_refs[7];
    uint32_t RefFrameMapTextureIndex[8];
    EVENT_LOOP_FILTER loop_filter;
    EVENT_QUANTIZATION quantization;
    EVENT_CDEF cdef;
    EVENT_STRENGTH y_strengths[8];
    EVENT_STRENGTH uv_strengths[8];
    uint32_t interp_filter;
    EVENT_SEGMENTATION segmentation;
    uint32_t segmentation_feature_data[8][8];
    EVENT_FEATURE_MASK feature_mask[8];
    EVENT_FILM_GRAIN film_grain;
    uint32_t film_grain_ar_coeffs_y[24];
    uint32_t film_grain_ar_coeffs_cb[25];
    uint32_t film_grain_ar_coeffs_cr[25];
    uint32_t StatusReportFeedbackNumber;
} DECODE_EVENTDATA_PICTUREPARAM_AV1;

typedef struct _DECODE_EVENTDATA_TILECONTROLPARAMS_AV1
{
    uint32_t DataOffset;
    uint32_t DataSize;
    uint32_t row;
    uint32_t column;
    uint32_t anchor_frame;
}DECODE_EVENTDATA_TILECONTROLPARAMS_AV1;

void DecodeEventDataAV1SurfaceOutparam(DECODE_EVENTDATA_SURFACEOUT_AV1* pEventData, mfxFrameSurface1* surface_out);


void DecodeEventDpbInfoAV1(DECODE_EVENTDATA_DPBINFO_AV1* pEventData, std::vector<UMC_AV1_DECODER::AV1DecoderFrame*> updated_refs);


#endif
#endif

