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

#include "mfx_unified_h264d_logging.h"

#ifdef MFX_EVENT_TRACE_DUMP_SUPPORTED

void DecodeEventDataAVCSurfaceOutparam(
    DECODE_EVENTDATA_SURFACEOUT_AVC* pEventData,
    mfxFrameSurface1* surface_out,
    UMC::H264DecoderFrame* pFrame)
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
    pEventData->FrameOrder = pFrame->m_frameOrder;
    pEventData->DataFlag = surface_out->Data.DataFlag;
    pEventData->TimeStamp = (uint32_t)surface_out->Data.TimeStamp;
}

void DecodeEventDataAVCPicparam(
    DECODE_EVENTDATA_PICTUREPARAM_AVC* pEventData,
    DXVA_PicParams_H264* pPicParams_H264)
{
    pEventData->wFrameWidthInMbsMinus1 = pPicParams_H264->wFrameWidthInMbsMinus1;
    pEventData->wFrameHeightInMbsMinus1 = pPicParams_H264->wFrameHeightInMbsMinus1;
    pEventData->CurrPic.Index7Bits = pPicParams_H264->CurrPic.Index7Bits;
    pEventData->CurrPic.AssociatedFlag = pPicParams_H264->CurrPic.AssociatedFlag;
    pEventData->num_ref_frames = pPicParams_H264->num_ref_frames;
    pEventData->field_pic_flag = pPicParams_H264->field_pic_flag;
    pEventData->MbaffFrameFlag = pPicParams_H264->MbaffFrameFlag;
    pEventData->residual_colour_transform_flag = pPicParams_H264->redundant_pic_cnt_present_flag;
    pEventData->sp_for_switch_flag = pPicParams_H264->sp_for_switch_flag;
    pEventData->chroma_format_idc = pPicParams_H264->chroma_format_idc;
    pEventData->RefPicFlag = pPicParams_H264->RefPicFlag;
    pEventData->constrained_intra_pred_flag = pPicParams_H264->constrained_intra_pred_flag;
    pEventData->weighted_pred_flag = pPicParams_H264->weighted_pred_flag;
    pEventData->weighted_bipred_idc = pPicParams_H264->weighted_bipred_idc;
    pEventData->MbsConsecutiveFlag = pPicParams_H264->MbsConsecutiveFlag;
    pEventData->frame_mbs_only_flag = pPicParams_H264->frame_mbs_only_flag;
    pEventData->transform_8x8_mode_flag = pPicParams_H264->transform_8x8_mode_flag;
    pEventData->MinLumaBipredSize8x8Flag = pPicParams_H264->MinLumaBipredSize8x8Flag;
    pEventData->IntraPicFlag = pPicParams_H264->IntraPicFlag;
    pEventData->wBitFields = pPicParams_H264->wBitFields;
    pEventData->bit_depth_luma_minus8 = pPicParams_H264->bit_depth_luma_minus8;
    pEventData->bit_depth_chroma_minus8 = pPicParams_H264->bit_depth_chroma_minus8;
    pEventData->StatusReportFeedbackNumber = pPicParams_H264->StatusReportFeedbackNumber;
    for (uint32_t i = 0; i < 16; i++)
    {
        pEventData->RefFrameList[i].Index7Bits = pPicParams_H264->RefFrameList[i].Index7Bits;
        pEventData->RefFrameList[i].AssociatedFlag = pPicParams_H264->RefFrameList[i].AssociatedFlag;
    }
    for (uint32_t i = 0; i < 2; i++)
        pEventData->CurrFieldOrderCnt[i] = pPicParams_H264->CurrFieldOrderCnt[i];
    for (uint32_t j = 0; j < 16; j++)
    {
        for (uint32_t i = 0; i < 2; i++)
            pEventData->FieldOrderCntList[j][i] = pPicParams_H264->FieldOrderCntList[j][i];
    }
    pEventData->pic_init_qs_minus26 = pPicParams_H264->pic_init_qs_minus26;
    pEventData->chroma_qp_index_offset = pPicParams_H264->chroma_qp_index_offset;
    pEventData->second_chroma_qp_index_offset = pPicParams_H264->second_chroma_qp_index_offset;
    pEventData->ContinuationFlag = pPicParams_H264->ContinuationFlag;
    pEventData->pic_init_qp_minus26 = pPicParams_H264->pic_init_qp_minus26;
    pEventData->num_ref_idx_l0_active_minus1 = pPicParams_H264->num_ref_idx_l0_active_minus1;
    pEventData->num_ref_idx_l1_active_minus1 = pPicParams_H264->num_ref_idx_l1_active_minus1;
    for (uint32_t i = 0; i < 16; i++)
        pEventData->FrameNumList[i] = pPicParams_H264->FrameNumList[i];
    pEventData->UsedForReferenceFlags = pPicParams_H264->UsedForReferenceFlags;
    pEventData->NonExistingFrameFlags = pPicParams_H264->NonExistingFrameFlags;
    pEventData->frame_num = pPicParams_H264->frame_num;
    pEventData->log2_max_frame_num_minus4 = pPicParams_H264->log2_max_frame_num_minus4;
    pEventData->pic_order_cnt_type = pPicParams_H264->pic_order_cnt_type;
    pEventData->log2_max_pic_order_cnt_lsb_minus4 = pPicParams_H264->log2_max_pic_order_cnt_lsb_minus4;
    pEventData->delta_pic_order_always_zero_flag = pPicParams_H264->delta_pic_order_always_zero_flag;
    pEventData->direct_8x8_inference_flag = pPicParams_H264->direct_8x8_inference_flag;
    pEventData->entropy_coding_mode_flag = pPicParams_H264->entropy_coding_mode_flag;
    pEventData->pic_order_present_flag = pPicParams_H264->pic_order_present_flag;
    pEventData->num_slice_groups_minus1 = pPicParams_H264->num_slice_groups_minus1;
    pEventData->slice_group_map_type = pPicParams_H264->slice_group_map_type;
    pEventData->deblocking_filter_control_present_flag = pPicParams_H264->deblocking_filter_control_present_flag;
    pEventData->redundant_pic_cnt_present_flag = pPicParams_H264->redundant_pic_cnt_present_flag;
    pEventData->slice_group_change_rate_minus1 = pPicParams_H264->slice_group_change_rate_minus1;
}

void DecodeEventDataAVCSliceParam(
    DECODE_EVENTDATA_SLICEPARAM_AVC* pEventData,
    DXVA_Slice_H264_Long* sliceParams)
{
    pEventData->BSNALunitDataLocation = sliceParams->BSNALunitDataLocation;
    pEventData->SliceBytesInBuffer = sliceParams->SliceBytesInBuffer;
    pEventData->wBadSliceChopping = sliceParams->wBadSliceChopping;
    pEventData->first_mb_in_slice = sliceParams->first_mb_in_slice;
    pEventData->NumMbsForSlice = sliceParams->NumMbsForSlice;
    pEventData->BitOffsetToSliceData = sliceParams->BitOffsetToSliceData;
    pEventData->slice_type = sliceParams->slice_type;
    pEventData->luma_log2_weight_denom = sliceParams->luma_log2_weight_denom;
    pEventData->chroma_log2_weight_denom = sliceParams->chroma_log2_weight_denom;
    pEventData->num_ref_idx_l0_active_minus1 = sliceParams->num_ref_idx_l0_active_minus1;
    pEventData->num_ref_idx_l1_active_minus1 = sliceParams->num_ref_idx_l1_active_minus1;
    pEventData->slice_alpha_c0_offset_div2 = sliceParams->slice_alpha_c0_offset_div2;
    pEventData->slice_beta_offset_div2 = sliceParams->slice_beta_offset_div2;
    for (uint32_t j = 0; j < 2; j++)
    {
        for (uint32_t i = 0; i < 32; i++)
        {
            pEventData->RefPicList[j][i].Index7Bits = sliceParams->RefPicList[j][i].Index7Bits;
            pEventData->RefPicList[j][i].AssociatedFlag = sliceParams->RefPicList[j][i].AssociatedFlag;
        }
    }
    pEventData->slice_qs_delta = sliceParams->slice_qs_delta;
    pEventData->slice_qp_delta = sliceParams->slice_qp_delta;
    pEventData->redundant_pic_cnt = sliceParams->redundant_pic_cnt;
    pEventData->direct_spatial_mv_pred_flag = sliceParams->direct_spatial_mv_pred_flag;
    pEventData->cabac_init_idc = sliceParams->cabac_init_idc;
    pEventData->disable_deblocking_filter_idc = sliceParams->disable_deblocking_filter_idc;
    pEventData->slice_id = sliceParams->slice_id;
}

void DecodeEventH264DpbInfo(
    DECODE_EVENTDATA_DPBINFO_AVC* pEventData,
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

void DecodeEventDataAVCQmatrixParam(
    EVENT_Qmatrix_H264* pEventData,
    DXVA_Qmatrix_H264* pQmatrix_H264)
{
    for (uint32_t i = 0; i < 6; i++)
    {
        for (uint32_t j = 0; j < 16; j++)
            pEventData->bScalingLists4x4[i][j] = pQmatrix_H264->bScalingLists4x4[i][j];
    }
    for (uint32_t i = 0; i < 2; i++)
    {
        for (uint32_t j = 0; j < 64; j++)
            pEventData->bScalingLists8x8[i][j] = pQmatrix_H264->bScalingLists8x8[i][j];
    }
}
#endif
