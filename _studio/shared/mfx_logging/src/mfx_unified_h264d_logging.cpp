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

#include "mfx_unified_h264d_logging.h"

void EventH264DecodeSurfaceOutparam(
    EVENTDATA_SURFACEOUT_H264D* pEventData,
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

void EventH264DecodePicparam(
    EVENTDATA_PICTUREPARAM_AVC* pEventData,
    VAPictureParameterBufferH264* pPicParams_H264)
{
    pEventData->CurrPic.picture_id = pPicParams_H264->CurrPic.picture_id;
    pEventData->CurrPic.frame_idx = pPicParams_H264->CurrPic.frame_idx;
    pEventData->CurrPic.flags = pPicParams_H264->CurrPic.flags;
    pEventData->CurrPic.TopFieldOrderCnt = pPicParams_H264->CurrPic.TopFieldOrderCnt;
    pEventData->CurrPic.BottomFieldOrderCnt = pPicParams_H264->CurrPic.BottomFieldOrderCnt;
    pEventData->picture_width_in_mbs_minus1 = pPicParams_H264->picture_width_in_mbs_minus1;
    pEventData->bit_depth_luma_minus8 = pPicParams_H264->bit_depth_luma_minus8;
    pEventData->bit_depth_chroma_minus8 = pPicParams_H264->bit_depth_chroma_minus8;
    pEventData->num_ref_frames = pPicParams_H264->num_ref_frames;
    pEventData->chroma_format_idc = pPicParams_H264->seq_fields.bits.chroma_format_idc;
    pEventData->residual_colour_transform_flag = pPicParams_H264->seq_fields.bits.residual_colour_transform_flag;
    pEventData->frame_mbs_only_flag = pPicParams_H264->seq_fields.bits.frame_mbs_only_flag;
    pEventData->mb_adaptive_frame_field_flag = pPicParams_H264->seq_fields.bits.mb_adaptive_frame_field_flag;
    pEventData->direct_8x8_inference_flag = pPicParams_H264->seq_fields.bits.direct_8x8_inference_flag;
    pEventData->MinLumaBiPredSize8x8 = pPicParams_H264->seq_fields.bits.MinLumaBiPredSize8x8;
    pEventData->log2_max_frame_num_minus4 = pPicParams_H264->seq_fields.bits.log2_max_frame_num_minus4;
    pEventData->pic_order_cnt_type = pPicParams_H264->seq_fields.bits.pic_order_cnt_type;
    pEventData->log2_max_pic_order_cnt_lsb_minus4 = pPicParams_H264->seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4;
    pEventData->delta_pic_order_always_zero_flag = pPicParams_H264->seq_fields.bits.delta_pic_order_always_zero_flag;
    pEventData->pic_init_qp_minus26 = pPicParams_H264->pic_init_qp_minus26;
    pEventData->pic_init_qs_minus26 = pPicParams_H264->pic_init_qs_minus26;
    pEventData->chroma_qp_index_offset = pPicParams_H264->chroma_qp_index_offset;
    pEventData->second_chroma_qp_index_offset = pPicParams_H264->second_chroma_qp_index_offset;
    pEventData->entropy_coding_mode_flag = pPicParams_H264->pic_fields.bits.entropy_coding_mode_flag;
    pEventData->weighted_pred_flag = pPicParams_H264->pic_fields.bits.weighted_pred_flag;
    pEventData->weighted_bipred_idc = pPicParams_H264->pic_fields.bits.weighted_bipred_idc;
    pEventData->transform_8x8_mode_flag = pPicParams_H264->pic_fields.bits.transform_8x8_mode_flag;
    pEventData->field_pic_flag = pPicParams_H264->pic_fields.bits.field_pic_flag;
    pEventData->constrained_intra_pred_flag = pPicParams_H264->pic_fields.bits.constrained_intra_pred_flag;
    pEventData->pic_order_present_flag = pPicParams_H264->pic_fields.bits.pic_order_present_flag;
    pEventData->deblocking_filter_control_present_flag = pPicParams_H264->pic_fields.bits.deblocking_filter_control_present_flag;
    pEventData->redundant_pic_cnt_present_flag = pPicParams_H264->pic_fields.bits.redundant_pic_cnt_present_flag;
    pEventData->reference_pic_flag = pPicParams_H264->pic_fields.bits.reference_pic_flag;
    pEventData->frame_num = pPicParams_H264->frame_num;
    for (uint32_t i = 0; i < 16; i++)
    {
            pEventData->ReferenceFrames[i].picture_id = pPicParams_H264->ReferenceFrames[i].picture_id;
            pEventData->ReferenceFrames[i].frame_idx = pPicParams_H264->ReferenceFrames[i].frame_idx;
            pEventData->ReferenceFrames[i].flags = pPicParams_H264->ReferenceFrames[i].flags;
            pEventData->ReferenceFrames[i].TopFieldOrderCnt = pPicParams_H264->ReferenceFrames[i].TopFieldOrderCnt;
            pEventData->ReferenceFrames[i].BottomFieldOrderCnt = pPicParams_H264->ReferenceFrames[i].BottomFieldOrderCnt;
    }
}

void EventH264DecodeSliceParam(
    EVENTDATA_SLICEPARAM_AVC* pEventData,
    VASliceParameterBufferH264* SliceParam)
{
    pEventData->slice_data_flag = SliceParam->slice_data_flag;
    pEventData->slice_data_size = SliceParam->slice_data_size;
    pEventData->slice_data_offset = SliceParam->slice_data_offset;
    pEventData->slice_data_bit_offset = SliceParam->slice_data_bit_offset;
    pEventData->first_mb_in_slice = SliceParam->first_mb_in_slice;
    pEventData->slice_type = SliceParam->slice_type;
    pEventData->direct_spatial_mv_pred_flag = SliceParam->direct_spatial_mv_pred_flag;
    pEventData->cabac_init_idc = SliceParam->cabac_init_idc;
    pEventData->slice_qp_delta = SliceParam->slice_qp_delta;
    pEventData->disable_deblocking_filter_idc = SliceParam->disable_deblocking_filter_idc;
    pEventData->luma_log2_weight_denom = SliceParam->luma_log2_weight_denom;
    pEventData->chroma_log2_weight_denom = SliceParam->chroma_log2_weight_denom;
    pEventData->num_ref_idx_l0_active_minus1 = SliceParam->num_ref_idx_l0_active_minus1;
    pEventData->num_ref_idx_l1_active_minus1 = SliceParam->num_ref_idx_l1_active_minus1;
    pEventData->slice_alpha_c0_offset_div2 = SliceParam->slice_alpha_c0_offset_div2;
    pEventData->slice_beta_offset_div2 = SliceParam->slice_beta_offset_div2;
    for (uint32_t i = 0; i < 32; i++)
    {
            pEventData->luma_weight_l0[i] = SliceParam->luma_weight_l0[i];
            pEventData->luma_offset_l0[i] = SliceParam->luma_offset_l0[i];
    }
    for (uint32_t i = 0; i < 32; i++)
    {
            pEventData->luma_weight_l1[i] = SliceParam->luma_weight_l1[i];
            pEventData->luma_offset_l1[i] = SliceParam->luma_offset_l1[i];
    }
    for (uint32_t i = 0; i < 32; i++)
    {
            pEventData->chroma_weight_l0[i][0] = SliceParam->chroma_weight_l0[i][0];
            pEventData->chroma_weight_l0[i][1] = SliceParam->chroma_weight_l0[i][1];
            pEventData->chroma_offset_l0[i][0] = SliceParam->chroma_offset_l0[i][0];
            pEventData->chroma_offset_l0[i][1] = SliceParam->chroma_offset_l0[i][1];
    }
    for (uint32_t i = 0; i < 32; i++)
    {
            pEventData->chroma_weight_l1[i][0] = SliceParam->chroma_weight_l1[i][0];
            pEventData->chroma_weight_l1[i][1] = SliceParam->chroma_weight_l1[i][1];
            pEventData->chroma_offset_l1[i][0] = SliceParam->chroma_offset_l1[i][0];
            pEventData->chroma_offset_l1[i][1] = SliceParam->chroma_offset_l1[i][1];
    }
    for (uint32_t i = 0; i < 32; i++)
    {
            pEventData->RefPicList0[i].picture_id = SliceParam->RefPicList0[i].picture_id;
            pEventData->RefPicList0[i].frame_idx = SliceParam->RefPicList0[i].frame_idx;
            pEventData->RefPicList0[i].flags = SliceParam->RefPicList0[i].flags;
            pEventData->RefPicList0[i].TopFieldOrderCnt = SliceParam->RefPicList0[i].TopFieldOrderCnt;
            pEventData->RefPicList0[i].BottomFieldOrderCnt = SliceParam->RefPicList0[i].BottomFieldOrderCnt;
            pEventData->RefPicList1[i].picture_id = SliceParam->RefPicList1[i].picture_id;
            pEventData->RefPicList1[i].frame_idx = SliceParam->RefPicList1[i].frame_idx;
            pEventData->RefPicList1[i].flags = SliceParam->RefPicList1[i].flags;
            pEventData->RefPicList1[i].TopFieldOrderCnt = SliceParam->RefPicList1[i].TopFieldOrderCnt;
            pEventData->RefPicList1[i].BottomFieldOrderCnt = SliceParam->RefPicList1[i].BottomFieldOrderCnt;
    }
}

void EventH264DecodeQmatrixParam(
    EVENTDATA_QMATRIX_H264D* pEventData,
    VAIQMatrixBufferH264* pQmatrix_H264)
{
    for (uint32_t i = 0; i < 6; i++)
    {
        for (uint32_t j = 0; j < 16; j++)
            pEventData->bScalingLists4x4[i][j] = pQmatrix_H264->ScalingList4x4[i][j];
    }
    for (uint32_t i = 0; i < 2; i++)
    {
        for (uint32_t j = 0; j < 64; j++)
            pEventData->bScalingLists8x8[i][j] = pQmatrix_H264->ScalingList8x8[i][j];
    }
}

void EventH264DecodeDpbInfo(
    EVENTDATA_DPBINFO_H264D* pEventData,
    UMC::H264DBPList* pDPBList)
{
    int32_t j = 0;
    int32_t dpbSize = pDPBList->GetDPBSize();
    int32_t start = j;
    for (UMC::H264DecoderFrame* pFrm = pDPBList->head(); pFrm && (j < dpbSize + start); pFrm = pFrm->future())
    {
        pEventData->DpbInfo[j].PicOrderCnt[0] = pFrm->m_PicOrderCnt[0];
        pEventData->DpbInfo[j].PicOrderCnt[1] = pFrm->m_PicOrderCnt[1];
        pEventData->DpbInfo[j].FrameId = pFrm->GetFrameData()->GetFrameMID();
        pEventData->DpbInfo[j].isShortTermRef = pFrm->isShortTermRef();
        pEventData->DpbInfo[j].isLongTermRef = pFrm->isLongTermRef();
        pEventData->DpbInfo[j].refCounter = pFrm->GetRefCounter();
        j++;
    }
    pEventData->eventCount = j;
}
