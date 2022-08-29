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

#include "mfx_unified_vp9d_logging.h"

void EventVP9DecodeSurfaceOutparam(
    EVENTDATA_SURFACEOUT_VP9D* pEventData,
    mfxFrameSurface1** surface_out)
{
    pEventData->CropH = (*surface_out)->Info.CropH;
    pEventData->CropW = (*surface_out)->Info.CropW;
    pEventData->CropX = (*surface_out)->Info.CropX;
    pEventData->CropY = (*surface_out)->Info.CropY;
    pEventData->ChromaFormat = (*surface_out)->Info.ChromaFormat;
    pEventData->AspectRatioH = (*surface_out)->Info.AspectRatioH;
    pEventData->AspectRatioW = (*surface_out)->Info.AspectRatioW;
    pEventData->FrameRateExtD = (*surface_out)->Info.FrameRateExtD;
    pEventData->FrameRateExtN = (*surface_out)->Info.FrameRateExtN;
    pEventData->PicStruct = (*surface_out)->Info.PicStruct;
    pEventData->DataFlag = (*surface_out)->Data.DataFlag;
    pEventData->FrameOrder = (*surface_out)->Data.FrameOrder;
    pEventData->TimeStamp = (uint32_t)(*surface_out)->Data.TimeStamp;
}

void EventVP9DecodePicparam(
    EVENTDATA_PICTUREPARAM_VP9* pEventData,
    VADecPictureParameterBufferVP9* picParam)
{
    pEventData->frame_width = picParam->frame_width;
    pEventData->frame_height = picParam->frame_height;
    for (uint32_t i = 0; i < 8; i++)
        pEventData->reference_frames[i] = picParam->reference_frames[i];
    pEventData->subsampling_x = picParam->pic_fields.bits.subsampling_x;
    pEventData->subsampling_y = picParam->pic_fields.bits.subsampling_y;
    pEventData->frame_type = picParam->pic_fields.bits.frame_type;
    pEventData->show_frame = picParam->pic_fields.bits.show_frame;
    pEventData->error_resilient_mode = picParam->pic_fields.bits.error_resilient_mode;
    pEventData->intra_only = picParam->pic_fields.bits.intra_only;
    pEventData->allow_high_precision_mv = picParam->pic_fields.bits.allow_high_precision_mv;
    pEventData->mcomp_filter_type = picParam->pic_fields.bits.mcomp_filter_type;
    pEventData->frame_parallel_decoding_mode = picParam->pic_fields.bits.frame_parallel_decoding_mode;
    pEventData->reset_frame_context = picParam->pic_fields.bits.reset_frame_context;
    pEventData->refresh_frame_context = picParam->pic_fields.bits.refresh_frame_context;
    pEventData->frame_context_idx = picParam->pic_fields.bits.frame_context_idx;
    pEventData->segmentation_enabled = picParam->pic_fields.bits.segmentation_enabled;
    pEventData->segmentation_temporal_update = picParam->pic_fields.bits.segmentation_temporal_update;
    pEventData->segmentation_update_map = picParam->pic_fields.bits.segmentation_update_map;
    pEventData->last_ref_frame = picParam->pic_fields.bits.last_ref_frame;
    pEventData->last_ref_frame_sign_bias = picParam->pic_fields.bits.last_ref_frame_sign_bias;
    pEventData->golden_ref_frame = picParam->pic_fields.bits.golden_ref_frame;
    pEventData->golden_ref_frame_sign_bias = picParam->pic_fields.bits.golden_ref_frame_sign_bias;
    pEventData->alt_ref_frame = picParam->pic_fields.bits.alt_ref_frame;
    pEventData->alt_ref_frame_sign_bias = picParam->pic_fields.bits.alt_ref_frame_sign_bias;
    pEventData->lossless_flag = picParam->pic_fields.bits.lossless_flag;
    pEventData->filter_level = picParam->filter_level;
    pEventData->sharpness_level = picParam->sharpness_level;
    pEventData->log2_tile_rows = picParam->log2_tile_rows;
    pEventData->log2_tile_columns = picParam->log2_tile_columns;
    pEventData->frame_header_length_in_bytes = picParam->frame_header_length_in_bytes;
    pEventData->first_partition_size = picParam->first_partition_size;
    for (uint32_t i = 0; i < 7; i++)
        pEventData->mb_segment_tree_probs[i] = picParam->mb_segment_tree_probs[i];
    for (uint32_t i = 0; i < 3; i++)
        pEventData->segment_pred_probs[i] = picParam->segment_pred_probs[i];
    pEventData->profile = picParam->profile;
    pEventData->bit_depth = picParam->bit_depth;
}

void EventVP9DecodeSegmentParam(
    EVENTDATA_SLICEPARAM_VP9* pEventData,
    VASliceParameterBufferVP9* sliceParam)
{
    pEventData->slice_data_size = sliceParam->slice_data_size;
    pEventData->slice_data_offset = sliceParam->slice_data_offset;
    pEventData->slice_data_flag = sliceParam->slice_data_flag;
    for (uint32_t i = 0; i < 8; i++)
    {
        pEventData->seg_param[i].segment_reference_enabled = sliceParam->seg_param[i].segment_flags.fields.segment_reference_enabled;
        pEventData->seg_param[i].segment_reference = sliceParam->seg_param[i].segment_flags.fields.segment_reference;
        pEventData->seg_param[i].segment_reference_skipped = sliceParam->seg_param[i].segment_flags.fields.segment_reference_skipped;
        for (uint32_t j = 0; j < 4; j++)
        {
            pEventData->seg_param[i].filter_level[j][0] = sliceParam->seg_param[i].filter_level[j][0];
            pEventData->seg_param[i].filter_level[j][1] = sliceParam->seg_param[i].filter_level[j][1];
        }
        pEventData->seg_param[i].luma_ac_quant_scale = sliceParam->seg_param[i].luma_ac_quant_scale;
        pEventData->seg_param[i].luma_dc_quant_scale = sliceParam->seg_param[i].luma_dc_quant_scale;
        pEventData->seg_param[i].chroma_ac_quant_scale = sliceParam->seg_param[i].chroma_ac_quant_scale;
        pEventData->seg_param[i].chroma_dc_quant_scale = sliceParam->seg_param[i].chroma_dc_quant_scale;
    }
}

void EventVP9DecodeDpbInfo(
    EVENTDATA_DPBINFO_VP9D* pEventData,
    std::vector<UMC_VP9_DECODER::VP9DecoderFrame> m_submittedFrames)
{
    pEventData->eventCount = m_submittedFrames.size();
    for (uint32_t i = 0; i < m_submittedFrames.size(); i++)
    {
        pEventData->DpbInfo[i].FrameId = m_submittedFrames[i].currFrame;
        pEventData->DpbInfo[i].isDecoded = m_submittedFrames[i].isDecoded;
    }
}