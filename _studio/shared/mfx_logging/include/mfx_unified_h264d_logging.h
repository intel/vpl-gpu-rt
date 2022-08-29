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

#include "umc_va_base.h"
#include "umc_h264_frame_list.h"
#include "mfx_unified_decode_logging.h"

typedef struct _EVENTDATA_SURFACEOUT_H264
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
} EVENTDATA_SURFACEOUT_H264D;

typedef struct _EVENT_PicEntry_H264
{
    uint32_t picture_id;
    uint32_t frame_idx;
    uint32_t flags;
    uint32_t TopFieldOrderCnt;
    uint32_t BottomFieldOrderCnt;
} EVENT_PicEntry_H264;

typedef struct _EVENTDATA_PICTUREPARAM_AVC
{
    EVENT_PicEntry_H264 CurrPic;
    uint32_t picture_width_in_mbs_minus1;
    uint32_t picture_height_in_mbs_minus1;
    uint32_t bit_depth_luma_minus8;
    uint32_t bit_depth_chroma_minus8;
    uint32_t num_ref_frames;
    uint32_t chroma_format_idc;
    uint32_t residual_colour_transform_flag;
    uint32_t frame_mbs_only_flag;
    uint32_t mb_adaptive_frame_field_flag;
    uint32_t direct_8x8_inference_flag;
    uint32_t MinLumaBiPredSize8x8;
    uint32_t log2_max_frame_num_minus4;
    uint32_t pic_order_cnt_type;
    uint32_t log2_max_pic_order_cnt_lsb_minus4;
    uint32_t delta_pic_order_always_zero_flag;
    uint32_t pic_init_qp_minus26;
    uint32_t pic_init_qs_minus26;
    uint32_t chroma_qp_index_offset;
    uint32_t second_chroma_qp_index_offset;
    uint32_t entropy_coding_mode_flag;
    uint32_t weighted_pred_flag;
    uint32_t weighted_bipred_idc;
    uint32_t transform_8x8_mode_flag;
    uint32_t field_pic_flag;
    uint32_t constrained_intra_pred_flag;
    uint32_t pic_order_present_flag;
    uint32_t deblocking_filter_control_present_flag;
    uint32_t redundant_pic_cnt_present_flag;
    uint32_t reference_pic_flag;
    uint32_t frame_num;
    EVENT_PicEntry_H264 ReferenceFrames[16];
} EVENTDATA_PICTUREPARAM_AVC;

typedef struct _EVENTDATA_SLICEPARAM_AVC
{
    uint32_t slice_data_flag;
    uint32_t slice_data_size;
    uint32_t slice_data_offset;
    uint32_t slice_data_bit_offset;
    uint32_t first_mb_in_slice;
    uint32_t slice_type;
    uint32_t direct_spatial_mv_pred_flag;
    uint32_t cabac_init_idc;
    uint32_t slice_qp_delta;
    uint32_t disable_deblocking_filter_idc;
    uint32_t luma_log2_weight_denom;
    uint32_t chroma_log2_weight_denom;
    uint32_t num_ref_idx_l0_active_minus1;
    uint32_t num_ref_idx_l1_active_minus1;
    uint32_t slice_alpha_c0_offset_div2;
    uint32_t slice_beta_offset_div2;
    uint32_t luma_weight_l0[32];
    uint32_t luma_offset_l0[32];
    uint32_t luma_weight_l1[32];
    uint32_t luma_offset_l1[32];
    uint32_t chroma_weight_l0[32][2];
    uint32_t chroma_offset_l0[32][2];
    uint32_t chroma_weight_l1[32][2];
    uint32_t chroma_offset_l1[32][2];
    EVENT_PicEntry_H264 RefPicList0[32];
    EVENT_PicEntry_H264 RefPicList1[32];
} EVENTDATA_SLICEPARAM_AVC;

typedef struct _EVENTDATA_H264DPBINFO
{
    uint32_t PicOrderCnt[2];
    int32_t FrameId;
    uint32_t isShortTermRef;
    uint32_t isLongTermRef;
    int32_t refCounter;
} EVENTDATA_H264DPBINFO;

typedef struct _EVENTDATA_DPBINFO_H264D
{
    uint32_t eventCount;
    EVENTDATA_H264DPBINFO DpbInfo[16];
} EVENTDATA_DPBINFO_H264D;

typedef struct _EVENTDATA_QMATRIX_H264D
{
    uint32_t  bScalingLists4x4[6][16];
    uint32_t  bScalingLists8x8[2][64];
} EVENTDATA_QMATRIX_H264D;

void EventH264DecodeSurfaceOutparam(EVENTDATA_SURFACEOUT_H264D* pEventData, mfxFrameSurface1* surface_out);

void EventH264DecodePicparam(EVENTDATA_PICTUREPARAM_AVC* pEventData, VAPictureParameterBufferH264* pPicParams_H264);

void EventH264DecodeSliceParam(EVENTDATA_SLICEPARAM_AVC* pEventData, VASliceParameterBufferH264* SliceParam);

void EventH264DecodeQmatrixParam(EVENTDATA_QMATRIX_H264D* pEventData, VAIQMatrixBufferH264* IQMatrixParam);

void EventH264DecodeDpbInfo(EVENTDATA_DPBINFO_H264D* pEventData, UMC::H264DBPList* pDPBList);

#endif

