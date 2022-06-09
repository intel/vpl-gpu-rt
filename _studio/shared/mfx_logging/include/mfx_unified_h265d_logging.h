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
#ifdef MFX_EVENT_TRACE_DUMP_SUPPORTED

#include "umc_va_base.h"
#include "umc_h265_frame_list.h"
#include "mfx_unified_decode_logging.h"

typedef struct _DECODE_EVENTDATA_SURFACEOUT_HEVC
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
} DECODE_EVENTDATA_SURFACEOUT_HEVC;

typedef struct _DECODE_EVENTDATA_SYNC_H265
{
    uint32_t PicOrderCnt;
    uint32_t m_index;
    uint32_t isDecodingCompleted;
    uint32_t m_isDisplayable;
    uint32_t m_wasOutputted;
} DECODE_EVENTDATA_SYNC_H265;

typedef struct _DECODE_EVENTDATA_outputFrame_h265
{
    uint32_t PicOrderCnt;
    uint32_t wasDisplayed;
    uint32_t wasOutputted;
} DECODE_EVENTDATA_outputFrame_h265;

typedef struct _EVENT_PicEntry_HEVC
{
    uint32_t Index7Bits;
    uint32_t AssociatedFlag;
} EVENT_PicEntry_HEVC;

typedef struct _DECODE_EVENTDATA_PICTUREPARAM_HEVC
{
    uint32_t log2_max_pic_order_cnt_lsb_minus4;
    uint32_t NoPicReorderingFlag;
    uint32_t NoBiPredFlag;
    EVENT_PicEntry_HEVC CurrPic;
    uint32_t sps_max_dec_pic_buffering_minus1;
    uint32_t log2_min_luma_coding_block_size_minus3;
    uint32_t log2_diff_max_min_transform_block_size;
    uint32_t log2_min_transform_block_size_minus2;
    uint32_t log2_diff_max_min_luma_coding_block_size;
    uint32_t max_transform_hierarchy_depth_intra;
    uint32_t max_transform_hierarchy_depth_inter;
    uint32_t num_short_term_ref_pic_sets;
    uint32_t num_long_term_ref_pics_sps;
    uint32_t num_ref_idx_l0_default_active_minus1;
    uint32_t num_ref_idx_l1_default_active_minus1;
    uint32_t init_qp_minus26;
    uint32_t ucNumDeltaPocsOfRefRpsIdx;
    uint32_t wNumBitsForShortTermRPSInSlice;
    uint32_t scaling_list_enabled_flag;
    uint32_t amp_enabled_flag;
    uint32_t sample_adaptive_offset_enabled_flag;
    uint32_t pcm_enabled_flag;
    uint32_t pcm_sample_bit_depth_luma_minus1;
    uint32_t pcm_sample_bit_depth_chroma_minus1;
    uint32_t log2_min_pcm_luma_coding_block_size_minus3;
    uint32_t log2_diff_max_min_pcm_luma_coding_block_size;
    uint32_t pcm_loop_filter_disabled_flag;
    uint32_t long_term_ref_pics_present_flag;
    uint32_t sps_temporal_mvp_enabled_flag;
    uint32_t strong_intra_smoothing_enabled_flag;
    uint32_t dependent_slice_segments_enabled_flag;
    uint32_t output_flag_present_flag;
    uint32_t num_extra_slice_header_bits;
    uint32_t sign_data_hiding_enabled_flag;
    uint32_t cabac_init_present_flag;
    uint32_t constrained_intra_pred_flag;
    uint32_t transform_skip_enabled_flag;
    uint32_t cu_qp_delta_enabled_flag;
    uint32_t pps_slice_chroma_qp_offsets_present_flag;
    uint32_t weighted_pred_flag;
    uint32_t weighted_bipred_flag;
    uint32_t transquant_bypass_enabled_flag;
    uint32_t tiles_enabled_flag;
    uint32_t entropy_coding_sync_enabled_flag;
    uint32_t uniform_spacing_flag;
    uint32_t loop_filter_across_tiles_enabled_flag;
    uint32_t pps_loop_filter_across_slices_enabled_flag;
    uint32_t deblocking_filter_override_enabled_flag;
    uint32_t pps_deblocking_filter_disabled_flag;
    uint32_t lists_modification_present_flag;
    uint32_t slice_segment_header_extension_present_flag;
    uint32_t IrapPicFlag;
    uint32_t IdrPicFlag;
    uint32_t IntraPicFlag;
    uint32_t pps_cb_qp_offset;
    uint32_t pps_cr_qp_offset;
    uint32_t num_tile_columns_minus1;
    uint32_t num_tile_rows_minus1;
    uint32_t column_width_minus1[19];
    uint32_t row_height_minus1[21];
    uint32_t diff_cu_qp_delta_depth;
    uint32_t pps_beta_offset_div2;
    uint32_t pps_tc_offset_div2;
    uint32_t log2_parallel_merge_level_minus2;
    uint32_t CurrPicOrderCntVal;
    int32_t  PicOrderCntValList[15];
    EVENT_PicEntry_HEVC	RefPicList[15];
    uint32_t RefPicSetStCurrBefore[8];
    uint32_t RefPicSetStCurrAfter[8];
    uint32_t RefPicSetLtCurr[8];
    uint32_t StatusReportFeedbackNumber;
} DECODE_EVENTDATA_PICTUREPARAM_HEVC;

// Reference picture list data structure
typedef struct _EVENT_RefPicListModification
{
    uint32_t ref_pic_list_modification_flag_l0;
    uint32_t ref_pic_list_modification_flag_l1;

    uint32_t list_entry_l0[17];
    uint32_t list_entry_l1[17];
} EVENT_RefPicListModification;

typedef struct _DECODE_EVENTDATA_SLICEPARAM_HEVC
    {
    uint32_t   nal_unit_type;
    uint32_t   nuh_temporal_id;
    uint32_t   first_slice_segment_in_pic_flag;
    uint32_t   no_output_of_prior_pics_flag;
    uint32_t   slice_pic_parameter_set_id;
    uint32_t   dependent_slice_segment_flag;
    uint32_t   slice_segment_address;
    uint32_t   slice_type;
    uint32_t   pic_output_flag;
    uint32_t   colour_plane_id;
    uint32_t   slice_pic_order_cnt_lsb;
    uint32_t   short_term_ref_pic_set_sps_flag;
    uint32_t   slice_temporal_mvp_enabled_flag;
    uint32_t   slice_sao_luma_flag;
    uint32_t   slice_sao_chroma_flag;
    uint32_t   num_ref_idx_active_override_flag;
    uint32_t   num_ref_idx_l0_active;
    uint32_t   num_ref_idx_l1_active;
    uint32_t   mvd_l1_zero_flag;
    uint32_t   cabac_init_flag;
    uint32_t   collocated_from_l0_flag;
    uint32_t   collocated_ref_idx;
    uint32_t   luma_log2_weight_denom;
    uint32_t   chroma_log2_weight_denom;
    uint32_t   max_num_merge_cand;
    uint32_t   use_integer_mv_flag;
    uint32_t   slice_qp_delta;
    uint32_t   slice_cb_qp_offset;
    uint32_t   slice_cr_qp_offset;
    uint32_t   slice_act_y_qp_offset;
    uint32_t   slice_act_cb_qp_offset;
    uint32_t   slice_act_cr_qp_offset;
    uint32_t   cu_chroma_qp_offset_enabled_flag;
    uint32_t   deblocking_filter_override_flag;
    uint32_t   slice_deblocking_filter_disabled_flag;
    uint32_t   slice_beta_offset;
    uint32_t   slice_tc_offset;
    uint32_t   slice_loop_filter_across_slices_enabled_flag;
    uint32_t   num_entry_point_offsets;
    uint32_t   m_HeaderBitstreamOffset;
    uint32_t   m_sliceSegmentCurStartCUAddr;
    uint32_t   m_sliceSegmentCurEndCUAddr;
    uint32_t   SliceQP;
    uint32_t   SliceCurStartCUAddr;
    uint32_t   m_CheckLDC;
    uint32_t   m_numRefIdx[3];
    EVENT_RefPicListModification m_RefPicListModification;
    uint32_t   m_poc;
    uint32_t   m_numPicStCurr0;
    uint32_t   m_numPicStCurr1;
    uint32_t   m_numPicLtCurr;
    uint32_t   m_RpsPOCCurrList0[17];
    uint32_t   m_RefPOCList[2][17];
    uint32_t   IdrPicFlag;
    uint32_t   wNumBitsForShortTermRPSInSlice;
} DECODE_EVENTDATA_SLICEPARAM_HEVC;

typedef struct _EVENT_Qmatrix_HEVC
{
    uint32_t ucScalingLists0[6][16];
    uint32_t ucScalingLists1[6][64];
    uint32_t ucScalingLists2[6][64];
    uint32_t ucScalingLists3[2][64];
    uint32_t ucScalingListDCCoefSizeID2[6];
    uint32_t ucScalingListDCCoefSizeID3[2];
} EVENT_Qmatrix_HEVC;

typedef struct _EVENTDATA_DPBINFO_h265
{
    uint32_t PicOrderCnt;
    uint32_t FrameId;
    uint32_t isShortTermRef;
    uint32_t isLongTermRef;
    uint32_t refCounter;
} EVENTDATA_DPBINFO_h265;

typedef struct _DECODE_EVENTDATA_DPBINFO_HEVC
{
    uint32_t eventCount;
    EVENTDATA_DPBINFO_h265 DpbInfo[16];
} DECODE_EVENTDATA_DPBINFO_HEVC;

void DecodeEventDataHEVCSurfaceOutparam(DECODE_EVENTDATA_SURFACEOUT_HEVC* pEventData, mfxFrameSurface1* surface_out, UMC_HEVC_DECODER::H265DecoderFrame* pFrame);
#endif
#endif

