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
    pEventData->FrameWidthInMbsMinus1 = pPicParams_H264->wFrameWidthInMbsMinus1;
    pEventData->FrameHeightInMbsMinus1 = pPicParams_H264->wFrameHeightInMbsMinus1;
    pEventData->m_CurrPic.m_Index7Bits = pPicParams_H264->CurrPic.Index7Bits;
    pEventData->m_CurrPic.m_AssociatedFlag = pPicParams_H264->CurrPic.AssociatedFlag;
    pEventData->m_num_ref_frames = pPicParams_H264->num_ref_frames;
    pEventData->m_field_pic_flag = pPicParams_H264->field_pic_flag;
    pEventData->m_MbaffFrameFlag = pPicParams_H264->MbaffFrameFlag;
    pEventData->m_residual_colour_transform_flag = pPicParams_H264->redundant_pic_cnt_present_flag;
    pEventData->m_sp_for_switch_flag = pPicParams_H264->sp_for_switch_flag;
    pEventData->m_chroma_format_idc = pPicParams_H264->chroma_format_idc;
    pEventData->m_RefPicFlag = pPicParams_H264->RefPicFlag;
    pEventData->m_constrained_intra_pred_flag = pPicParams_H264->constrained_intra_pred_flag;
    pEventData->m_weighted_pred_flag = pPicParams_H264->weighted_pred_flag;
    pEventData->m_weighted_bipred_idc = pPicParams_H264->weighted_bipred_idc;
    pEventData->m_MbsConsecutiveFlag = pPicParams_H264->MbsConsecutiveFlag;
    pEventData->m_frame_mbs_only_flag = pPicParams_H264->frame_mbs_only_flag;
    pEventData->m_transform_8x8_mode_flag = pPicParams_H264->transform_8x8_mode_flag;
    pEventData->m_MinLumaBipredSize8x8Flag = pPicParams_H264->MinLumaBipredSize8x8Flag;
    pEventData->m_IntraPicFlag = pPicParams_H264->IntraPicFlag;
    pEventData->BitFields = pPicParams_H264->wBitFields;
    pEventData->m_bit_depth_luma_minus8 = pPicParams_H264->bit_depth_luma_minus8;
    pEventData->m_bit_depth_chroma_minus8 = pPicParams_H264->bit_depth_chroma_minus8;
    pEventData->m_StatusReportFeedbackNumber = pPicParams_H264->StatusReportFeedbackNumber;
    for (uint32_t i = 0; i < 16; i++)
    {
        pEventData->m_RefFrameList[i].m_Index7Bits = pPicParams_H264->RefFrameList[i].Index7Bits;
        pEventData->m_RefFrameList[i].m_AssociatedFlag = pPicParams_H264->RefFrameList[i].AssociatedFlag;
    }
    for (uint32_t i = 0; i < 2; i++)
        pEventData->m_CurrFieldOrderCnt[i] = pPicParams_H264->CurrFieldOrderCnt[i];
    for (uint32_t j = 0; j < 16; j++)
    {
        for (uint32_t i = 0; i < 2; i++)
            pEventData->m_FieldOrderCntList[j][i] = pPicParams_H264->FieldOrderCntList[j][i];
    }
    pEventData->m_pic_init_qs_minus26 = pPicParams_H264->pic_init_qs_minus26;
    pEventData->m_chroma_qp_index_offset = pPicParams_H264->chroma_qp_index_offset;
    pEventData->m_second_chroma_qp_index_offset = pPicParams_H264->second_chroma_qp_index_offset;
    pEventData->m_ContinuationFlag = pPicParams_H264->ContinuationFlag;
    pEventData->m_pic_init_qp_minus26 = pPicParams_H264->pic_init_qp_minus26;
    pEventData->m_num_ref_idx_l0_active_minus1 = pPicParams_H264->num_ref_idx_l0_active_minus1;
    pEventData->m_num_ref_idx_l1_active_minus1 = pPicParams_H264->num_ref_idx_l1_active_minus1;
    for (uint32_t i = 0; i < 16; i++)
        pEventData->m_FrameNumList[i] = pPicParams_H264->FrameNumList[i];
    pEventData->m_UsedForReferenceFlags = pPicParams_H264->UsedForReferenceFlags;
    pEventData->m_NonExistingFrameFlags = pPicParams_H264->NonExistingFrameFlags;
    pEventData->m_frame_num = pPicParams_H264->frame_num;
    pEventData->m_log2_max_frame_num_minus4 = pPicParams_H264->log2_max_frame_num_minus4;
    pEventData->m_pic_order_cnt_type = pPicParams_H264->pic_order_cnt_type;
    pEventData->m_log2_max_pic_order_cnt_lsb_minus4 = pPicParams_H264->log2_max_pic_order_cnt_lsb_minus4;
    pEventData->m_delta_pic_order_always_zero_flag = pPicParams_H264->delta_pic_order_always_zero_flag;
    pEventData->m_direct_8x8_inference_flag = pPicParams_H264->direct_8x8_inference_flag;
    pEventData->m_entropy_coding_mode_flag = pPicParams_H264->entropy_coding_mode_flag;
    pEventData->m_pic_order_present_flag = pPicParams_H264->pic_order_present_flag;
    pEventData->m_num_slice_groups_minus1 = pPicParams_H264->num_slice_groups_minus1;
    pEventData->m_slice_group_map_type = pPicParams_H264->slice_group_map_type;
    pEventData->m_deblocking_filter_control_present_flag = pPicParams_H264->deblocking_filter_control_present_flag;
    pEventData->m_redundant_pic_cnt_present_flag = pPicParams_H264->redundant_pic_cnt_present_flag;
    pEventData->m_slice_group_change_rate_minus1 = pPicParams_H264->slice_group_change_rate_minus1;
}

void DecodeEventDataAVCSliceParam(
    DECODE_EVENTDATA_SLICEPARAM_AVC* pEventData,
    DXVA_Slice_H264_Long* sliceParams)
{
    pEventData->m_BSNALunitDataLocation = sliceParams->BSNALunitDataLocation;
    pEventData->m_SliceBytesInBuffer = sliceParams->SliceBytesInBuffer;
    pEventData->m_wBadSliceChopping = sliceParams->wBadSliceChopping;
    pEventData->m_first_mb_in_slice = sliceParams->first_mb_in_slice;
    pEventData->m_NumMbsForSlice = sliceParams->NumMbsForSlice;
    pEventData->m_BitOffsetToSliceData = sliceParams->BitOffsetToSliceData;
    pEventData->m_slice_type = sliceParams->slice_type;
    pEventData->m_luma_log2_weight_denom = sliceParams->luma_log2_weight_denom;
    pEventData->m_chroma_log2_weight_denom = sliceParams->chroma_log2_weight_denom;
    pEventData->m_num_ref_idx_l0_active_minus1 = sliceParams->num_ref_idx_l0_active_minus1;
    pEventData->m_num_ref_idx_l1_active_minus1 = sliceParams->num_ref_idx_l1_active_minus1;
    pEventData->m_slice_alpha_c0_offset_div2 = sliceParams->slice_alpha_c0_offset_div2;
    pEventData->m_slice_beta_offset_div2 = sliceParams->slice_beta_offset_div2;
    for (uint32_t j = 0; j < 2; j++)
    {
        for (uint32_t i = 0; i < 32; i++)
        {
            pEventData->m_RefPicList[j][i].m_Index7Bits = sliceParams->RefPicList[j][i].Index7Bits;
            pEventData->m_RefPicList[j][i].m_AssociatedFlag = sliceParams->RefPicList[j][i].AssociatedFlag;
        }
    }
    pEventData->m_slice_qs_delta = sliceParams->slice_qs_delta;
    pEventData->m_slice_qp_delta = sliceParams->slice_qp_delta;
    pEventData->m_redundant_pic_cnt = sliceParams->redundant_pic_cnt;
    pEventData->m_direct_spatial_mv_pred_flag = sliceParams->direct_spatial_mv_pred_flag;
    pEventData->m_cabac_init_idc = sliceParams->cabac_init_idc;
    pEventData->m_disable_deblocking_filter_idc = sliceParams->disable_deblocking_filter_idc;
    pEventData->m_slice_id = sliceParams->slice_id;
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
