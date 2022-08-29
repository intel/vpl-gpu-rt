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

#include "mfx_unified_av1d_logging.h"

void EventAV1DecodeSurfaceOutparam(
    EVENTDATA_SURFACEOUT_AV1D* pEventData,
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
    pEventData->FrameOrder = surface_out->Data.FrameOrder;
    pEventData->PicStruct = surface_out->Info.PicStruct;
    pEventData->DataFlag = surface_out->Data.DataFlag;
    pEventData->TimeStamp = (uint32_t)surface_out->Data.TimeStamp;
}

void EventAV1DecodePicparam(
    EVENTDATA_PICTUREPARAM_AV1D* pEventData,
    VADecPictureParameterBufferAV1& picParam)
{
    pEventData->frame_width_minus1 = picParam.frame_width_minus1;
    pEventData->frame_height_minus1 = picParam.frame_height_minus1;
    pEventData->output_frame_width_in_tiles_minus_1 = picParam.output_frame_width_in_tiles_minus_1;
    pEventData->output_frame_height_in_tiles_minus_1 = picParam.output_frame_height_in_tiles_minus_1;
    pEventData->profile = picParam.profile;
    pEventData->seqInfo.still_picture = picParam.seq_info_fields.fields.still_picture;
    pEventData->seqInfo.use_128x128_superblock = picParam.seq_info_fields.fields.use_128x128_superblock;
    pEventData->seqInfo.enable_filter_intra = picParam.seq_info_fields.fields.enable_filter_intra;
    pEventData->seqInfo.enable_intra_edge_filter = picParam.seq_info_fields.fields.enable_intra_edge_filter;
    pEventData->seqInfo.enable_interintra_compound = picParam.seq_info_fields.fields.enable_interintra_compound;
    pEventData->seqInfo.enable_masked_compound = picParam.seq_info_fields.fields.enable_masked_compound;
    pEventData->seqInfo.enable_cdef = picParam.seq_info_fields.fields.enable_cdef;
    pEventData->seqInfo.enable_dual_filter = picParam.seq_info_fields.fields.enable_dual_filter;
    pEventData->seqInfo.enable_order_hint = picParam.seq_info_fields.fields.enable_order_hint;
    pEventData->seqInfo.enable_jnt_comp = picParam.seq_info_fields.fields.enable_jnt_comp;
    pEventData->seqInfo.mono_chrome = picParam.seq_info_fields.fields.mono_chrome;
    pEventData->seqInfo.color_range = picParam.seq_info_fields.fields.color_range;
    pEventData->seqInfo.subsampling_x = picParam.seq_info_fields.fields.subsampling_x;
    pEventData->seqInfo.subsampling_y = picParam.seq_info_fields.fields.subsampling_y;
    pEventData->seqInfo.chroma_sample_position = picParam.seq_info_fields.fields.chroma_sample_position;
    pEventData->seqInfo.film_grain_params_present = picParam.seq_info_fields.fields.film_grain_params_present;
    pEventData->matrix_coefficients = picParam.matrix_coefficients;
    pEventData->bit_depth_idx = picParam.bit_depth_idx;
    pEventData->order_hint_bits_minus_1 = picParam.order_hint_bits_minus_1;
    pEventData->picInfo.frame_type = picParam.pic_info_fields.bits.frame_type;
    pEventData->picInfo.show_frame = picParam.pic_info_fields.bits.show_frame;
    pEventData->picInfo.showable_frame = picParam.pic_info_fields.bits.showable_frame;
    pEventData->picInfo.error_resilient_mode = picParam.pic_info_fields.bits.error_resilient_mode;
    pEventData->picInfo.disable_cdf_update = picParam.pic_info_fields.bits.disable_cdf_update;
    pEventData->picInfo.allow_screen_content_tools = picParam.pic_info_fields.bits.allow_screen_content_tools;
    pEventData->picInfo.force_integer_mv = picParam.pic_info_fields.bits.force_integer_mv;
    pEventData->picInfo.allow_intrabc = picParam.pic_info_fields.bits.allow_intrabc;
    pEventData->picInfo.use_superres = picParam.pic_info_fields.bits.use_superres;
    pEventData->picInfo.allow_high_precision_mv = picParam.pic_info_fields.bits.allow_high_precision_mv;
    pEventData->picInfo.is_motion_mode_switchable = picParam.pic_info_fields.bits.is_motion_mode_switchable;
    pEventData->picInfo.disable_frame_end_update_cdf = picParam.pic_info_fields.bits.disable_frame_end_update_cdf;
    pEventData->picInfo.uniform_tile_spacing_flag = picParam.pic_info_fields.bits.uniform_tile_spacing_flag;
    pEventData->picInfo.allow_warped_motion = picParam.pic_info_fields.bits.allow_warped_motion;
    pEventData->picInfo.large_scale_tile = picParam.pic_info_fields.bits.large_scale_tile;
    pEventData->tile_count_minus_1 = picParam.tile_count_minus_1;
    pEventData->anchor_frames_num = picParam.anchor_frames_num;
    pEventData->order_hint = picParam.order_hint;
    pEventData->superres_scale_denominator = picParam.superres_scale_denominator;
    pEventData->interp_filter = picParam.interp_filter;
    pEventData->seg.enabled = picParam.seg_info.segment_info_fields.bits.enabled;
    pEventData->seg.update_map = picParam.seg_info.segment_info_fields.bits.update_map;
    pEventData->seg.temporal_update = picParam.seg_info.segment_info_fields.bits.temporal_update;
    pEventData->seg.update_data = picParam.seg_info.segment_info_fields.bits.update_data;
    for (uint8_t i = 0; i < 8; i++)
    {
        pEventData->seg.feature_mask[i] = picParam.seg_info.feature_mask[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            pEventData->seg_feature_data[i][j] = picParam.seg_info.feature_data[i][j];
        }
    }
    pEventData->current_frame = picParam.current_frame;
    pEventData->current_display_picture = picParam.current_display_picture;
    for (uint8_t i = 0; i < 8; i++)
    {
        pEventData->ref_frame_map[i] = picParam.ref_frame_map[i];
    }
    for (uint8_t i = 0; i < 7; i++)
    {
        pEventData->ref_frame_idx[i] = picParam.ref_frame_idx[i];
    }
    pEventData->primary_ref_frame = picParam.primary_ref_frame;
    pEventData->filter_level[0] = picParam.filter_level[0];
    pEventData->filter_level[1] = picParam.filter_level[1];
    pEventData->filter_level_u = picParam.filter_level_u;
    pEventData->filter_level_v = picParam.filter_level_v;
    pEventData->lfInfo.sharpness_level = picParam.loop_filter_info_fields.bits.sharpness_level;
    pEventData->lfInfo.mode_ref_delta_enabled = picParam.loop_filter_info_fields.bits.mode_ref_delta_enabled;
    pEventData->lfInfo.mode_ref_delta_update = picParam.loop_filter_info_fields.bits.mode_ref_delta_update;
    for (uint8_t i = 0; i < 8; i++)
    {
        pEventData->ref_deltas[i] = picParam.ref_deltas[i];
    }
    pEventData->mode_deltas[0] = picParam.mode_deltas[0];
    pEventData->mode_deltas[1] = picParam.mode_deltas[1];
    pEventData->base_qindex = picParam.base_qindex;
    pEventData->y_dc_delta_q = picParam.y_dc_delta_q;
    pEventData->u_dc_delta_q = picParam.u_dc_delta_q;
    pEventData->v_dc_delta_q = picParam.v_dc_delta_q;
    pEventData->u_ac_delta_q = picParam.u_ac_delta_q;
    pEventData->v_ac_delta_q = picParam.v_ac_delta_q;
    pEventData->cdef_damping_minus_3 = picParam.cdef_damping_minus_3;
    pEventData->cdef_bits = picParam.cdef_bits;
    for (uint8_t i = 0; i < 8; i++)
    {
        pEventData->cdef_y_strengths[i] = picParam.cdef_y_strengths[i];
        pEventData->cdef_uv_strengths[i] = picParam.cdef_uv_strengths[i];
    }
    for (uint8_t i = 0; i < 7; i++)
    {
        pEventData->wm[i].wmtype = picParam.wm[i].wmtype;
        for (uint8_t j = 0; j < 8; j++)
        {
            pEventData->wm[i].wmmat[j] = picParam.wm[i].wmmat[j];
        }
        pEventData->wm[i].invalid = picParam.wm[i].invalid;
    }
    pEventData->tile_cols = picParam.tile_cols;
    pEventData->tile_rows = picParam.tile_rows;
    for (uint8_t i = 0; i < 63; i++)
    {
        pEventData->width_in_sbs_minus_1[i] = picParam.width_in_sbs_minus_1[i];
        pEventData->height_in_sbs_minus_1[i] = picParam.height_in_sbs_minus_1[i];
    }
    pEventData->context_update_tile_id = picParam.context_update_tile_id;
}

void EventAV1DecodeTileControlparam(
    EVENTDATA_TILECONTROLPARAMS_AV1D* pEventData,
    VASliceParameterBufferAV1& tileControlParam)
{
    pEventData->slice_data_offset = tileControlParam.slice_data_offset;
    pEventData->slice_data_size = tileControlParam.slice_data_size;
    pEventData->slice_data_flag = tileControlParam.slice_data_flag;
    pEventData->tile_row = tileControlParam.tile_row;
    pEventData->tile_column = tileControlParam.tile_column;
    pEventData->tg_start = tileControlParam.tg_start;
    pEventData->tg_end = tileControlParam.tg_end;
    pEventData->anchor_frame_idx = tileControlParam.anchor_frame_idx;
    pEventData->tile_idx_in_tile_list = tileControlParam.tile_idx_in_tile_list;
}

void EventAV1DecodeDpbInfo(
    EVENTDATA_DPBINFO_AV1D* pEventData,
    std::vector<UMC_AV1_DECODER::AV1DecoderFrame*> updated_refs)
{
    for (uint8_t i = 0; i < 6; ++i)
    {
        pEventData->DpbInfo[i].MemID = updated_refs[i]->GetMemID();
        pEventData->DpbInfo[i].Refvalid = updated_refs[i]->RefValid();
        pEventData->DpbInfo[i].RefCounter = updated_refs[i]->GetRefCounter();
    }
}