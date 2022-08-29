// Copyright (c) 2021 Intel Corporation
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

#include "mfx_unified_h265d_logging.h"

void EventH265DecodeSurfaceOutparam(
    EVENTDATA_SURFACEOUT_H265D* pEventData,
    mfxFrameSurface1* surface_out)
{
    pEventData->CropH = surface_out->Info.CropH;
    pEventData->CropW = surface_out->Info.CropW;
    pEventData->CropX = surface_out->Info.CropX;
    pEventData->CropY = surface_out->Info.CropY;
    pEventData->ChromaFormat = surface_out->Info.ChromaFormat;
    pEventData->AspectRatioH = surface_out->Info.AspectRatioH;
    pEventData->AspectRatioW = surface_out->Info.AspectRatioW;
    pEventData->FrameRateExtD = surface_out->Info.FrameRateExtD;
    pEventData->FrameRateExtN = surface_out->Info.FrameRateExtN;
    pEventData->PicStruct = surface_out->Info.PicStruct;
    pEventData->FrameOrder = surface_out->Data.FrameOrder;
    pEventData->DataFlag = surface_out->Data.DataFlag;
    pEventData->TimeStamp = (uint32_t)surface_out->Data.TimeStamp;
}

void EventH265DecodePicparam(
    EVENTDATA_PICTUREPARAM_HEVC* pEventData,
    VAPictureParameterBufferHEVC* pPicParam)
{
    pEventData->CurrPic.picture_id = pPicParam->CurrPic.picture_id;
    pEventData->CurrPic.pic_order_cnt = pPicParam->CurrPic.pic_order_cnt;
    pEventData->CurrPic.flags = pPicParam->CurrPic.flags;
    for (uint32_t i = 0; i < 15; i++)
    {
        pEventData->ReferenceFrames[i].picture_id = pPicParam->ReferenceFrames[i].picture_id;
        pEventData->ReferenceFrames[i].pic_order_cnt = pPicParam->ReferenceFrames[i].pic_order_cnt;
        pEventData->ReferenceFrames[i].flags = pPicParam->ReferenceFrames[i].flags;
    }
    pEventData->pic_width_in_luma_samples = pPicParam->pic_width_in_luma_samples;
    pEventData->pic_height_in_luma_samples = pPicParam->pic_height_in_luma_samples;
    pEventData->chroma_format_idc = pPicParam->pic_fields.bits.chroma_format_idc;
    pEventData->separate_colour_plane_flag = pPicParam->pic_fields.bits.separate_colour_plane_flag;
    pEventData->pcm_enabled_flag = pPicParam->pic_fields.bits.pcm_enabled_flag;
    pEventData->scaling_list_enabled_flag = pPicParam->pic_fields.bits.scaling_list_enabled_flag;
    pEventData->transform_skip_enabled_flag = pPicParam->pic_fields.bits.transform_skip_enabled_flag;
    pEventData->amp_enabled_flag = pPicParam->pic_fields.bits.amp_enabled_flag;
    pEventData->strong_intra_smoothing_enabled_flag = pPicParam->pic_fields.bits.strong_intra_smoothing_enabled_flag;
    pEventData->sign_data_hiding_enabled_flag = pPicParam->pic_fields.bits.sign_data_hiding_enabled_flag;
    pEventData->constrained_intra_pred_flag = pPicParam->pic_fields.bits.constrained_intra_pred_flag;
    pEventData->cu_qp_delta_enabled_flag = pPicParam->pic_fields.bits.cu_qp_delta_enabled_flag;
    pEventData->weighted_pred_flag = pPicParam->pic_fields.bits.weighted_pred_flag;
    pEventData->weighted_bipred_flag = pPicParam->pic_fields.bits.weighted_bipred_flag;
    pEventData->transquant_bypass_enabled_flag = pPicParam->pic_fields.bits.transquant_bypass_enabled_flag;
    pEventData->tiles_enabled_flag = pPicParam->pic_fields.bits.tiles_enabled_flag;
    pEventData->entropy_coding_sync_enabled_flag = pPicParam->pic_fields.bits.entropy_coding_sync_enabled_flag;
    pEventData->pps_loop_filter_across_slices_enabled_flag = pPicParam->pic_fields.bits.pps_loop_filter_across_slices_enabled_flag;
    pEventData->loop_filter_across_tiles_enabled_flag = pPicParam->pic_fields.bits.loop_filter_across_tiles_enabled_flag;
    pEventData->pcm_loop_filter_disabled_flag = pPicParam->pic_fields.bits.pcm_loop_filter_disabled_flag;
    pEventData->NoPicReorderingFlag = pPicParam->pic_fields.bits.NoPicReorderingFlag;
    pEventData->NoBiPredFlag = pPicParam->pic_fields.bits.NoBiPredFlag;
    pEventData->sps_max_dec_pic_buffering_minus1 = pPicParam->sps_max_dec_pic_buffering_minus1;
    pEventData->bit_depth_luma_minus8 = pPicParam->bit_depth_luma_minus8;
    pEventData->bit_depth_chroma_minus8 = pPicParam->bit_depth_chroma_minus8;
    pEventData->pcm_sample_bit_depth_luma_minus1 = pPicParam->pcm_sample_bit_depth_luma_minus1;
    pEventData->pcm_sample_bit_depth_chroma_minus1 = pPicParam->pcm_sample_bit_depth_chroma_minus1;
    pEventData->log2_min_luma_coding_block_size_minus3 = pPicParam->log2_min_luma_coding_block_size_minus3;
    pEventData->log2_diff_max_min_luma_coding_block_size = pPicParam->log2_diff_max_min_luma_coding_block_size;
    pEventData->log2_min_transform_block_size_minus2  = pPicParam->log2_min_transform_block_size_minus2;
    pEventData->log2_diff_max_min_transform_block_size = pPicParam->log2_diff_max_min_transform_block_size;
    pEventData->log2_min_pcm_luma_coding_block_size_minus3 = pPicParam->log2_min_pcm_luma_coding_block_size_minus3;
    pEventData->log2_diff_max_min_pcm_luma_coding_block_size = pPicParam->log2_diff_max_min_pcm_luma_coding_block_size;
    pEventData->max_transform_hierarchy_depth_intra = pPicParam->max_transform_hierarchy_depth_intra;
    pEventData->max_transform_hierarchy_depth_inter = pPicParam->max_transform_hierarchy_depth_inter;
    pEventData->init_qp_minus26 = pPicParam->init_qp_minus26;
    pEventData->diff_cu_qp_delta_depth = pPicParam->diff_cu_qp_delta_depth;
    pEventData->pps_cb_qp_offset = pPicParam->pps_cb_qp_offset;
    pEventData->pps_cr_qp_offset = pPicParam->pps_cr_qp_offset;
    pEventData->log2_parallel_merge_level_minus2 = pPicParam->log2_parallel_merge_level_minus2;
    pEventData->num_tile_columns_minus1 = pPicParam->num_tile_columns_minus1;
    pEventData->num_tile_rows_minus1 = pPicParam->num_tile_rows_minus1;
    for (uint32_t i = 0; i < 19; i++)
    {
        pEventData->column_width_minus1[i] = pPicParam->column_width_minus1[i];
    }
    for (uint32_t i = 0; i < 21; i++)
    {
        pEventData->row_height_minus1[i] = pPicParam->row_height_minus1[i];
    }
    pEventData->lists_modification_present_flag = pPicParam->slice_parsing_fields.bits.lists_modification_present_flag;
    pEventData->long_term_ref_pics_present_flag = pPicParam->slice_parsing_fields.bits.long_term_ref_pics_present_flag;
    pEventData->sps_temporal_mvp_enabled_flag = pPicParam->slice_parsing_fields.bits.sps_temporal_mvp_enabled_flag;
    pEventData->cabac_init_present_flag = pPicParam->slice_parsing_fields.bits.cabac_init_present_flag;
    pEventData->output_flag_present_flag = pPicParam->slice_parsing_fields.bits.output_flag_present_flag;
    pEventData->dependent_slice_segments_enabled_flag = pPicParam->slice_parsing_fields.bits.dependent_slice_segments_enabled_flag;
    pEventData->pps_slice_chroma_qp_offsets_present_flag = pPicParam->slice_parsing_fields.bits.pps_slice_chroma_qp_offsets_present_flag;
    pEventData->sample_adaptive_offset_enabled_flag = pPicParam->slice_parsing_fields.bits.sample_adaptive_offset_enabled_flag;
    pEventData->deblocking_filter_override_enabled_flag = pPicParam->slice_parsing_fields.bits.deblocking_filter_override_enabled_flag;
    pEventData->pps_disable_deblocking_filter_flag = pPicParam->slice_parsing_fields.bits.pps_disable_deblocking_filter_flag;
    pEventData->slice_segment_header_extension_present_flag = pPicParam->slice_parsing_fields.bits.slice_segment_header_extension_present_flag;
    pEventData->RapPicFlag = pPicParam->slice_parsing_fields.bits.RapPicFlag;
    pEventData->IdrPicFlag = pPicParam->slice_parsing_fields.bits.IdrPicFlag;
    pEventData->IntraPicFlag = pPicParam->slice_parsing_fields.bits.IntraPicFlag;
    pEventData->ReservedBits = pPicParam->slice_parsing_fields.bits.ReservedBits;
    pEventData->log2_max_pic_order_cnt_lsb_minus4 = pPicParam->log2_max_pic_order_cnt_lsb_minus4;
    pEventData->num_short_term_ref_pic_sets = pPicParam->num_short_term_ref_pic_sets;
    pEventData->num_long_term_ref_pic_sps = pPicParam->num_long_term_ref_pic_sps;
    pEventData->num_ref_idx_l0_default_active_minus1 = pPicParam->num_ref_idx_l0_default_active_minus1;
    pEventData->num_ref_idx_l1_default_active_minus1 = pPicParam->num_ref_idx_l1_default_active_minus1;
    pEventData->pps_beta_offset_div2 = pPicParam->pps_beta_offset_div2;
    pEventData->pps_tc_offset_div2 = pPicParam->pps_tc_offset_div2;
    pEventData->num_extra_slice_header_bits = pPicParam->num_extra_slice_header_bits;
    pEventData->st_rps_bits = pPicParam->st_rps_bits;
}

void EventH265DecodeSliceparam(
    EVENTDATA_SLICEPARAM_HEVC* pEventData,
    VASliceParameterBufferHEVC* sliceHdr)
{
    pEventData->slice_data_byte_offset = sliceHdr->slice_data_byte_offset;
    pEventData->slice_segment_address = sliceHdr->slice_segment_address;
    for (uint32_t i = 0; i < 2; i++)
    {
        for (uint32_t j = 0; j < 15; j++)
        {
            pEventData->RefPicList[i][j] = sliceHdr->RefPicList[i][j];
        }
    }
    pEventData->LastSliceOfPic = sliceHdr->LongSliceFlags.fields.LastSliceOfPic;
    pEventData->dependent_slice_segment_flag = sliceHdr->LongSliceFlags.fields.dependent_slice_segment_flag;
    pEventData->slice_type = sliceHdr->LongSliceFlags.fields.slice_type;
    pEventData->color_plane_id = sliceHdr->LongSliceFlags.fields.color_plane_id;
    pEventData->slice_sao_luma_flag = sliceHdr->LongSliceFlags.fields.slice_sao_luma_flag;
    pEventData->slice_sao_chroma_flag = sliceHdr->LongSliceFlags.fields.slice_sao_chroma_flag;
    pEventData->mvd_l1_zero_flag = sliceHdr->LongSliceFlags.fields.mvd_l1_zero_flag;
    pEventData->cabac_init_flag = sliceHdr->LongSliceFlags.fields.cabac_init_flag;
    pEventData->slice_temporal_mvp_enabled_flag = sliceHdr->LongSliceFlags.fields.slice_temporal_mvp_enabled_flag;
    pEventData->slice_deblocking_filter_disabled_flag = sliceHdr->LongSliceFlags.fields.slice_deblocking_filter_disabled_flag;
    pEventData->collocated_from_l0_flag = sliceHdr->LongSliceFlags.fields.collocated_from_l0_flag;
    pEventData->slice_loop_filter_across_slices_enabled_flag = sliceHdr->LongSliceFlags.fields.slice_loop_filter_across_slices_enabled_flag;
    pEventData->collocated_ref_idx = sliceHdr->collocated_ref_idx;
    pEventData->num_ref_idx_l0_active_minus1 = sliceHdr->num_ref_idx_l0_active_minus1;
    pEventData->num_ref_idx_l1_active_minus1 = sliceHdr->num_ref_idx_l1_active_minus1;
    pEventData->slice_qp_delta = sliceHdr->slice_qp_delta;
    pEventData->slice_cb_qp_offset = sliceHdr->slice_cb_qp_offset;
    pEventData->slice_cr_qp_offset = sliceHdr->slice_cr_qp_offset;
    pEventData->slice_beta_offset_div2 = sliceHdr->slice_beta_offset_div2;
    pEventData->slice_tc_offset_div2 = sliceHdr->slice_tc_offset_div2;
    pEventData->luma_log2_weight_denom = sliceHdr->luma_log2_weight_denom;
    pEventData->delta_chroma_log2_weight_denom = sliceHdr->delta_chroma_log2_weight_denom;
    for (uint32_t i = 0; i < 15; i++)
    {
        pEventData->luma_offset_l0[i] = sliceHdr->luma_offset_l0[i];
        pEventData->delta_luma_weight_l0[i] = sliceHdr->delta_luma_weight_l0[i];
    }
    for (uint32_t i = 0; i < 15; i++)
    {
        for (uint32_t j = 0; j < 2; j++)
        {
            pEventData->delta_chroma_weight_l0[i][j] = sliceHdr->delta_chroma_weight_l0[i][j];
            pEventData->ChromaOffsetL0[i][j] = sliceHdr->ChromaOffsetL0[i][j];
        }
    }
    for (uint32_t i = 0; i < 15; i++)
    {
        pEventData->luma_offset_l1[i]  = sliceHdr->luma_offset_l1[i];
        pEventData->delta_luma_weight_l1[i] = sliceHdr->delta_luma_weight_l1[i];
    }
    for (uint32_t i = 0; i < 15; i++)
    {
        for (uint32_t j = 0; j < 2; j++)
        {
            pEventData->delta_chroma_weight_l1[i][j] = sliceHdr->delta_chroma_weight_l1[i][j];
            pEventData->ChromaOffsetL1[i][j] = sliceHdr->ChromaOffsetL1[i][j];
        }
    }
    pEventData->five_minus_max_num_merge_cand = sliceHdr->five_minus_max_num_merge_cand;
}

void EventH265DecodeQmatrixParam(
    EVENTDATA_Qmatrix_HEVC* pEventData,
    VAIQMatrixBufferHEVC* qmatrix)
{
    for (uint32_t i = 0; i < 6; i++)
    {
        for (uint32_t j = 0; j < 16; j++)
        {
            pEventData->ScalingList4x4[i][j] = qmatrix->ScalingList4x4[i][j];
        }
        for (uint32_t j = 0; j < 64; j++)
        {
            pEventData->ScalingList8x8[i][j] = qmatrix->ScalingList8x8[i][j];
            pEventData->ScalingList16x16[i][j] = qmatrix->ScalingList16x16[i][j];
        }
        pEventData->ScalingListDC16x16[i] = qmatrix->ScalingListDC16x16[i];
    }
    for (uint32_t i = 0; i < 2; i++)
    {
        for (uint32_t j = 0; j < 64; j++)
            pEventData->ScalingList32x32[i][j] = qmatrix->ScalingList32x32[i][j];
        pEventData->ScalingListDC32x32[i] = qmatrix->ScalingListDC32x32[i];
    }
}

void EventH265DecodeDpbInfo(
    EVENTDATA_DPBINFO_HEVC* pEventData,
    VAPictureParameterBufferHEVC* pPicParam)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < 15; i++)
    {
        pEventData->ReferenceFrames[i].picture_id = pPicParam->ReferenceFrames[i].picture_id;
        pEventData->ReferenceFrames[i].pic_order_cnt = pPicParam->ReferenceFrames[i].pic_order_cnt;
        pEventData->ReferenceFrames[i].flags = pPicParam->ReferenceFrames[i].flags;
        count++;
    }
    pEventData->count = count;
}
