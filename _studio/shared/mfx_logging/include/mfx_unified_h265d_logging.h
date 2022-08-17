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
#include "umc_h265_frame_list.h"
#include "mfx_unified_decode_logging.h"

typedef struct _EVENTDATA_SURFACEOUT_H265
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
    uint32_t  FrameOrder;
    uint32_t  DataFlag;
    uint32_t  TimeStamp;
} EVENTDATA_SURFACEOUT_H265D;

typedef struct _EVENT_PicEntry_HEVC
{
    uint32_t m_Index7Bits;
    uint32_t m_AssociatedFlag;
} EVENT_PicEntry_HEVC;

typedef struct _EVENTDATA_PICTUREPARAM_HEVC
{
    uint32_t m_log2_max_pic_order_cnt_lsb_minus4;
    uint32_t m_NoPicReorderingFlag;
    uint32_t m_NoBiPredFlag;
    EVENT_PicEntry_HEVC m_CurrPic;
    uint32_t m_sps_max_dec_pic_buffering_minus1;
    uint32_t m_log2_min_luma_coding_block_size_minus3;
    uint32_t m_log2_diff_max_min_transform_block_size;
    uint32_t m_log2_min_transform_block_size_minus2;
    uint32_t m_log2_diff_max_min_luma_coding_block_size;
    uint32_t m_max_transform_hierarchy_depth_intra;
    uint32_t m_max_transform_hierarchy_depth_inter;
    uint32_t m_num_short_term_ref_pic_sets;
    uint32_t m_num_long_term_ref_pics_sps;
    uint32_t m_num_ref_idx_l0_default_active_minus1;
    uint32_t m_num_ref_idx_l1_default_active_minus1;
    uint32_t m_init_qp_minus26;
    uint32_t m_ucNumDeltaPocsOfRefRpsIdx;
    uint32_t m_wNumBitsForShortTermRPSInSlice;
    uint32_t m_scaling_list_enabled_flag;
    uint32_t m_amp_enabled_flag;
    uint32_t m_sample_adaptive_offset_enabled_flag;
    uint32_t m_pcm_enabled_flag;
    uint32_t m_pcm_sample_bit_depth_luma_minus1;
    uint32_t m_pcm_sample_bit_depth_chroma_minus1;
    uint32_t m_log2_min_pcm_luma_coding_block_size_minus3;
    uint32_t m_log2_diff_max_min_pcm_luma_coding_block_size;
    uint32_t m_pcm_loop_filter_disabled_flag;
    uint32_t m_long_term_ref_pics_present_flag;
    uint32_t m_sps_temporal_mvp_enabled_flag;
    uint32_t m_strong_intra_smoothing_enabled_flag;
    uint32_t m_dependent_slice_segments_enabled_flag;
    uint32_t m_output_flag_present_flag;
    uint32_t m_num_extra_slice_header_bits;
    uint32_t m_sign_data_hiding_enabled_flag;
    uint32_t m_cabac_init_present_flag;
    uint32_t m_constrained_intra_pred_flag;
    uint32_t m_transform_skip_enabled_flag;
    uint32_t m_cu_qp_delta_enabled_flag;
    uint32_t m_pps_slice_chroma_qp_offsets_present_flag;
    uint32_t m_weighted_pred_flag;
    uint32_t m_weighted_bipred_flag;
    uint32_t m_transquant_bypass_enabled_flag;
    uint32_t m_tiles_enabled_flag;
    uint32_t m_entropy_coding_sync_enabled_flag;
    uint32_t m_uniform_spacing_flag;
    uint32_t m_loop_filter_across_tiles_enabled_flag;
    uint32_t m_pps_loop_filter_across_slices_enabled_flag;
    uint32_t m_deblocking_filter_override_enabled_flag;
    uint32_t m_pps_deblocking_filter_disabled_flag;
    uint32_t m_lists_modification_present_flag;
    uint32_t m_slice_segment_header_extension_present_flag;
    uint32_t m_IrapPicFlag;
    uint32_t m_IdrPicFlag;
    uint32_t m_IntraPicFlag;
    uint32_t m_pps_cb_qp_offset;
    uint32_t m_pps_cr_qp_offset;
    uint32_t m_num_tile_columns_minus1;
    uint32_t m_num_tile_rows_minus1;
    uint32_t m_column_width_minus1[19];
    uint32_t m_row_height_minus1[21];
    uint32_t m_diff_cu_qp_delta_depth;
    uint32_t m_pps_beta_offset_div2;
    uint32_t m_pps_tc_offset_div2;
    uint32_t m_log2_parallel_merge_level_minus2;
    uint32_t m_CurrPicOrderCntVal;
    int32_t  m_PicOrderCntValList[15];
    EVENT_PicEntry_HEVC	m_RefPicList[15];
    uint32_t m_RefPicSetStCurrBefore[8];
    uint32_t m_RefPicSetStCurrAfter[8];
    uint32_t m_RefPicSetLtCurr[8];
    uint32_t m_StatusReportFeedbackNumber;
} EVENTDATA_PICTUREPARAM_HEVC;

// Reference picture list data structure
typedef struct _EVENT_RefPicListModification
{
    uint32_t m_ref_pic_list_modification_flag_l0;
    uint32_t m_ref_pic_list_modification_flag_l1;
    uint32_t m_list_entry_l0[17];
    uint32_t m_list_entry_l1[17];
} EVENT_RefPicListModification;

typedef struct _EVENTDATA_SLICEPARAM_HEVC
    {
    uint32_t   m_nal_unit_type;
    uint32_t   m_nuh_temporal_id;
    uint32_t   m_first_slice_segment_in_pic_flag;
    uint32_t   m_no_output_of_prior_pics_flag;
    uint32_t   m_slice_pic_parameter_set_id;
    uint32_t   m_dependent_slice_segment_flag;
    uint32_t   m_slice_segment_address;
    uint32_t   m_slice_type;
    uint32_t   m_pic_output_flag;
    uint32_t   m_colour_plane_id;
    uint32_t   m_slice_pic_order_cnt_lsb;
    uint32_t   m_short_term_ref_pic_set_sps_flag;
    uint32_t   m_slice_temporal_mvp_enabled_flag;
    uint32_t   m_slice_sao_luma_flag;
    uint32_t   m_slice_sao_chroma_flag;
    uint32_t   m_num_ref_idx_active_override_flag;
    uint32_t   m_num_ref_idx_l0_active;
    uint32_t   m_num_ref_idx_l1_active;
    uint32_t   m_mvd_l1_zero_flag;
    uint32_t   m_cabac_init_flag;
    uint32_t   m_collocated_from_l0_flag;
    uint32_t   m_collocated_ref_idx;
    uint32_t   m_luma_log2_weight_denom;
    uint32_t   m_chroma_log2_weight_denom;
    uint32_t   m_max_num_merge_cand;
    uint32_t   m_use_integer_mv_flag;
    uint32_t   m_slice_qp_delta;
    uint32_t   m_slice_cb_qp_offset;
    uint32_t   m_slice_cr_qp_offset;
    uint32_t   m_slice_act_y_qp_offset;
    uint32_t   m_slice_act_cb_qp_offset;
    uint32_t   m_slice_act_cr_qp_offset;
    uint32_t   m_cu_chroma_qp_offset_enabled_flag;
    uint32_t   m_deblocking_filter_override_flag;
    uint32_t   m_slice_deblocking_filter_disabled_flag;
    uint32_t   m_slice_beta_offset;
    uint32_t   m_slice_tc_offset;
    uint32_t   m_slice_loop_filter_across_slices_enabled_flag;
    uint32_t   m_num_entry_point_offsets;
    uint32_t   HeaderBitstreamOffset;
    uint32_t   sliceSegmentCurStartCUAddr;
    uint32_t   sliceSegmentCurEndCUAddr;
    uint32_t   m_SliceQP;
    uint32_t   m_SliceCurStartCUAddr;
    uint32_t   CheckLDC;
    uint32_t   numRefIdx[3];
    EVENT_RefPicListModification RefPicListModification;
    uint32_t   poc;
    uint32_t   numPicStCurr0;
    uint32_t   numPicStCurr1;
    uint32_t   numPicLtCurr;
    uint32_t   RpsPOCCurrList0[17];
    uint32_t   RefPOCList[2][17];
    uint32_t   m_IdrPicFlag;
    uint32_t   NumBitsForShortTermRPSInSlice;
} EVENTDATA_SLICEPARAM_HEVC;

typedef struct _EVENTDATA_Qmatrix_H265D
{
    uint32_t ScalingLists0[6][16];
    uint32_t ScalingLists1[6][64];
    uint32_t ScalingLists2[6][64];
    uint32_t ScalingLists3[2][64];
    uint32_t ScalingListDCCoefSizeID2[6];
    uint32_t ScalingListDCCoefSizeID3[2];
} EVENTDATA_Qmatrix_H265D;

typedef struct _EVENTDATA_DPBINFO_h265
{
    uint32_t PicOrderCnt;
    uint32_t FrameId;
    uint32_t isShortTermRef;
    uint32_t isLongTermRef;
    uint32_t refCounter;
} EVENTDATA_DPBINFO_h265;

typedef struct _EVENTDATA_DPBINFO_H265D
{
    uint32_t eventCount;
    EVENTDATA_DPBINFO_h265 DpbInfo[16];
} EVENTDATA_DPBINFO_H265D;

void EventH265DecodeSurfaceOutparam(EVENTDATA_SURFACEOUT_H265D* pEventData, mfxFrameSurface1* surface_out);
#endif

