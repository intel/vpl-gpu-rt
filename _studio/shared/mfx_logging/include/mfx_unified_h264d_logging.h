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

#ifndef _MFX_UNIFIED_H264D_LOGGING_H_
#define _MFX_UNIFIED_H264D_LOGGING_H_

#include "mfx_config.h"
#ifdef MFX_EVENT_TRACE_DUMP_SUPPORTED

#include "umc_va_base.h"
#include "umc_h264_frame_list.h"
#include "mfx_unified_decode_logging.h"

typedef struct _DECODE_EVENTDATA_SURFACEOUT_AVC
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
} DECODE_EVENTDATA_SURFACEOUT_AVC;

typedef struct _DECODE_EVENTDATA_OUTPUTFRAME_h264
{
    uint32_t PicOrderCnt[2];
    uint32_t frameNum;
    uint32_t wasDisplayed;
    uint32_t wasOutputted;
} DECODE_EVENTDATA_OUTPUTFRAME_h264;

typedef struct _DECODE_EVENTDATA_SYNC_H264
{
    uint32_t PicOrderCnt[2];
    uint32_t m_index;
    uint32_t isDecodingCompleted;
    uint32_t wasDisplayed;
    uint32_t wasOutputted;
} DECODE_EVENTDATA_SYNC_H264;

typedef struct _EVENT_PicEntry_H264 {
    uint32_t  Index7Bits;
    uint32_t  AssociatedFlag;
} EVENT_PicEntry_H264;

typedef struct _DECODE_EVENTDATA_PICTUREPARAM_AVC
{
    uint32_t  wFrameWidthInMbsMinus1;
    uint32_t  wFrameHeightInMbsMinus1;
    EVENT_PicEntry_H264  CurrPic;
    uint32_t  num_ref_frames;
    uint32_t  field_pic_flag;
    uint32_t  MbaffFrameFlag;
    uint32_t  residual_colour_transform_flag;
    uint32_t  sp_for_switch_flag;
    uint32_t  chroma_format_idc;
    uint32_t  RefPicFlag;
    uint32_t  constrained_intra_pred_flag;
    uint32_t  weighted_pred_flag;
    uint32_t  weighted_bipred_idc;
    uint32_t  MbsConsecutiveFlag;
    uint32_t  frame_mbs_only_flag;
    uint32_t  transform_8x8_mode_flag;
    uint32_t  MinLumaBipredSize8x8Flag;
    uint32_t  IntraPicFlag;
    uint32_t  wBitFields;
    uint32_t  bit_depth_luma_minus8;
    uint32_t  bit_depth_chroma_minus8;
    uint32_t  StatusReportFeedbackNumber;
    EVENT_PicEntry_H264  RefFrameList[16];
    uint32_t  CurrFieldOrderCnt[2];
    uint32_t  FieldOrderCntList[16][2];
    uint32_t  pic_init_qs_minus26;
    uint32_t  chroma_qp_index_offset;
    uint32_t  second_chroma_qp_index_offset;
    uint32_t  ContinuationFlag;
    uint32_t  pic_init_qp_minus26;
    uint32_t  num_ref_idx_l0_active_minus1;
    uint32_t  num_ref_idx_l1_active_minus1;
    uint32_t  FrameNumList[16];
    uint32_t  UsedForReferenceFlags;
    uint32_t  NonExistingFrameFlags;
    uint32_t  frame_num;
    uint32_t  log2_max_frame_num_minus4;
    uint32_t  pic_order_cnt_type;
    uint32_t  log2_max_pic_order_cnt_lsb_minus4;
    uint32_t  delta_pic_order_always_zero_flag;
    uint32_t  direct_8x8_inference_flag;
    uint32_t  entropy_coding_mode_flag;
    uint32_t  pic_order_present_flag;
    uint32_t  num_slice_groups_minus1;
    uint32_t  slice_group_map_type;
    uint32_t  deblocking_filter_control_present_flag;
    uint32_t  redundant_pic_cnt_present_flag;
    uint32_t  slice_group_change_rate_minus1;
}DECODE_EVENTDATA_PICTUREPARAM_AVC;

typedef struct _DECODE_EVENTDATA_SLICEPARAM_AVC
{
    uint32_t BSNALunitDataLocation;
    uint32_t SliceBytesInBuffer;
    uint32_t wBadSliceChopping;
    uint32_t first_mb_in_slice;
    uint32_t NumMbsForSlice;
    uint32_t BitOffsetToSliceData;
    uint32_t slice_type;
    uint32_t luma_log2_weight_denom;
    uint32_t chroma_log2_weight_denom;
    uint32_t num_ref_idx_l0_active_minus1;
    uint32_t num_ref_idx_l1_active_minus1;
    uint32_t slice_alpha_c0_offset_div2;
    uint32_t slice_beta_offset_div2;
    EVENT_PicEntry_H264 RefPicList[2][32];
    uint32_t slice_qs_delta;
    uint32_t slice_qp_delta;
    uint32_t redundant_pic_cnt;
    uint32_t direct_spatial_mv_pred_flag;
    uint32_t cabac_init_idc;
    uint32_t disable_deblocking_filter_idc;
    uint32_t slice_id;
}DECODE_EVENTDATA_SLICEPARAM_AVC;

typedef struct _EVENTDATA_DPBINFO
{
    uint32_t PicOrderCnt[2];
    int32_t FrameId;
    uint32_t isShortTermRef;
    uint32_t isLongTermRef;
    int32_t refCounter;
} EVENTDATA_DPBINFO;

typedef struct _DECODE_EVENTDATA_DPBINFO_AVC
{
    uint32_t eventCount;
    EVENTDATA_DPBINFO DpbInfo[16];
} DECODE_EVENTDATA_DPBINFO_AVC;

typedef struct _EVENT_Qmatrix_H264 {
    uint32_t  bScalingLists4x4[6][16];
    uint32_t  bScalingLists8x8[2][64];
} EVENT_Qmatrix_H264;

void DecodeEventDataAVCSurfaceOutparam(DECODE_EVENTDATA_SURFACEOUT_AVC* pEventData, mfxFrameSurface1* surface_out, UMC::H264DecoderFrame* pFrame);
void DecodeEventH264DpbInfo(DECODE_EVENTDATA_DPBINFO_AVC* pEventData, UMC::H264DBPList* pDPBList);

void DecodeEventDataAVCQmatrixParam(EVENT_Qmatrix_H264* pEventData, DXVA_Qmatrix_H264* pQmatrix_H264);

#endif
#endif

