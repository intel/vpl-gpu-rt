// Copyright (c) 2021-2022 Intel Corporation
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

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include<algorithm>
#include "mfx_common_int.h"
#include "av1ehw_ddi.h"
#include "av1ehw_base_va_lin.h"
#include "av1ehw_base_va_packer_lin.h"
#include "av1ehw_struct_vaapi.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;
using namespace AV1EHW::Linux::Base;

void InitSPS(
    const ExtBuffer::Param<mfxVideoParam>& par
    , const SH& bs_sh
    , VAEncSequenceParameterBufferAV1& sps)
{
    sps = {};

    sps.seq_profile   = static_cast<mfxU8>(bs_sh.seq_profile);
    sps.seq_level_idx = static_cast<mfxU8>(bs_sh.seq_level_idx[0]);

    sps.intra_period = par.mfx.GopPicSize;
    sps.ip_period    = par.mfx.GopRefDist;

    mfxU32 bNeedRateParam =
        par.mfx.RateControlMethod == MFX_RATECONTROL_CBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR;

    sps.bits_per_second = bNeedRateParam * TargetKbps(par.mfx) * 1000;

    sps.order_hint_bits_minus_1 = static_cast<mfxU8>(bs_sh.order_hint_bits_minus1);

    sps.seq_fields.bits.still_picture              = bs_sh.still_picture;
    sps.seq_fields.bits.enable_filter_intra        = bs_sh.enable_filter_intra;
    sps.seq_fields.bits.enable_intra_edge_filter   = bs_sh.enable_intra_edge_filter;
    sps.seq_fields.bits.enable_interintra_compound = bs_sh.enable_interintra_compound;
    sps.seq_fields.bits.enable_masked_compound     = bs_sh.enable_masked_compound;
    sps.seq_fields.bits.enable_warped_motion       = bs_sh.enable_warped_motion;
    sps.seq_fields.bits.enable_dual_filter         = bs_sh.enable_dual_filter;
    sps.seq_fields.bits.enable_order_hint          = bs_sh.enable_order_hint;
    sps.seq_fields.bits.enable_jnt_comp            = bs_sh.enable_jnt_comp;
    sps.seq_fields.bits.enable_ref_frame_mvs       = bs_sh.enable_ref_frame_mvs;
    sps.seq_fields.bits.enable_superres            = bs_sh.enable_superres;
    sps.seq_fields.bits.enable_cdef                = bs_sh.enable_cdef;
    sps.seq_fields.bits.enable_restoration         = bs_sh.enable_restoration;

    const mfxExtCodingOption2& CO2 = ExtBuffer::Get(par);
#if VA_CHECK_VERSION(1, 16, 0)
    sps.hierarchical_flag = CO2.BRefType == MFX_B_REF_PYRAMID;
#else
    sps.reserved8b = CO2.BRefType == MFX_B_REF_PYRAMID;
#endif

}

void InitPPS(
    const ExtBuffer::Param<mfxVideoParam>& /*par*/
    , const FH& bs_fh
    , VAEncPictureParameterBufferAV1& pps)
{
    pps = {};

    std::fill(pps.reference_frames, pps.reference_frames + NUM_REF_FRAMES, VA_INVALID_ID);

    //frame size
    pps.frame_height_minus_1  = static_cast<mfxU16>(bs_fh.FrameHeight - 1);
    pps.frame_width_minus_1 = static_cast<mfxU16>(bs_fh.UpscaledWidth - 1);

    //quantizer
    pps.y_dc_delta_q = static_cast<mfxI8>(bs_fh.quantization_params.DeltaQYDc);
    pps.u_dc_delta_q = static_cast<mfxI8>(bs_fh.quantization_params.DeltaQUDc);
    pps.u_ac_delta_q = static_cast<mfxI8>(bs_fh.quantization_params.DeltaQUAc);
    pps.v_dc_delta_q = static_cast<mfxI8>(bs_fh.quantization_params.DeltaQVDc);
    pps.v_ac_delta_q = static_cast<mfxI8>(bs_fh.quantization_params.DeltaQVAc);

    //other params
    pps.picture_flags.bits.error_resilient_mode = bs_fh.error_resilient_mode;
    pps.interpolation_filter = static_cast<mfxU8>(bs_fh.interpolation_filter);
    pps.picture_flags.bits.use_superres = bs_fh.use_superres;
    pps.picture_flags.bits.allow_high_precision_mv = bs_fh.allow_high_precision_mv;
    pps.picture_flags.bits.reduced_tx_set = bs_fh.reduced_tx_set;
    pps.picture_flags.bits.palette_mode_enable = bs_fh.allow_screen_content_tools;

    //description for tx_mod
    pps.mode_control_flags.bits.tx_mode = bs_fh.TxMode;
    pps.temporal_id = 0;
    pps.superres_scale_denominator = static_cast<mfxU8>(bs_fh.SuperresDenom);

    //q_matrix
    pps.qmatrix_flags.bits.using_qmatrix = bs_fh.quantization_params.using_qmatrix;
    pps.qmatrix_flags.bits.qm_y = bs_fh.quantization_params.qm_y;
    pps.qmatrix_flags.bits.qm_u = bs_fh.quantization_params.qm_u;
    pps.qmatrix_flags.bits.qm_v = bs_fh.quantization_params.qm_v;

}

inline void InitTileGroupsBuffer(
    const TileGroupInfos& infos
    , std::vector<VAEncTileGroupBufferAV1>& tile_groups)
{
    tile_groups.resize(infos.size());

    for (mfxU16 i = 0; i < infos.size(); i++)
    {
        VAEncTileGroupBufferAV1& tg = tile_groups[i];
        tg = {};

        tg.tg_start = static_cast<mfxU8>(infos[i].TgStart);
        tg.tg_end   = static_cast<mfxU8>(infos[i].TgEnd);
    }
}

inline void FillSearchIdx(
    VARefFrameCtrlAV1& refFrameCtrl, mfxU8 idx, mfxU8 refFrame)
{
    switch (idx)
    {
    case 0:
        refFrameCtrl.fields.search_idx0 = refFrame;
        break;
    case 1:
        refFrameCtrl.fields.search_idx1 = refFrame;
        break;
    case 2:
        refFrameCtrl.fields.search_idx2 = refFrame;
        break;
    case 3:
        refFrameCtrl.fields.search_idx3 = refFrame;
        break;
    case 4:
        refFrameCtrl.fields.search_idx4 = refFrame;
        break;
    case 5:
        refFrameCtrl.fields.search_idx5 = refFrame;
        break;
    case 6:
        refFrameCtrl.fields.search_idx6 = refFrame;
        break;
    default:
        assert(!"Invalid index");
    }
}

inline void FillRefCtrlL0(
    const TaskCommonPar& task
    , VAEncPictureParameterBufferAV1& pps)
{
    mfxU8 idx = 0;
    for (mfxU8 refFrame = LAST_FRAME; refFrame < BWDREF_FRAME; refFrame++)
    {
        if(IsValidRefFrame(task.RefList, refFrame)) // Assume search order is same as ref_list order
            FillSearchIdx(pps.ref_frame_ctrl_l0, idx++, refFrame);
    }

    if(IsP(task.FrameType) && !task.isLDB) //not RAB frame
    {
        mfxU8 refFrame = ALTREF_FRAME;
        if (IsValidRefFrame(task.RefList, refFrame))
            FillSearchIdx(pps.ref_frame_ctrl_l0, idx, refFrame);
    }
}

inline void FillRefCtrlL1(
    const TaskCommonPar& task
    , VAEncPictureParameterBufferAV1& pps)
{
    if (IsB(task.FrameType) || task.isLDB)
    {
        mfxU8 idx = 0;
        for (mfxU8 refFrame = BWDREF_FRAME; refFrame < MAX_REF_FRAMES; refFrame++)
        {
            if(IsValidRefFrame(task.RefList, refFrame))
                FillSearchIdx(pps.ref_frame_ctrl_l1, idx++, refFrame);
        }
    }
}

inline void FillRefParams(
    const TaskCommonPar& task
    , const FH& bs_fh
    , const std::vector<VASurfaceID>& rec
    , VAEncPictureParameterBufferAV1& pps)
{
    if(pps.picture_flags.bits.frame_type == KEY_FRAME)
        return;

    std::transform(task.DPB.begin(), task.DPB.end(), pps.reference_frames,
        [&rec](DpbType::const_reference dpbFrm) -> VASurfaceID
        {
            return rec.at(dpbFrm->Rec.Idx);
        });

    std::transform(bs_fh.ref_frame_idx, bs_fh.ref_frame_idx + REFS_PER_FRAME, pps.ref_frame_idx,
        [](const int32_t idx) -> mfxU8
        {
            return static_cast<mfxU8>(idx);
        });

    FillRefCtrlL0(task, pps);
    FillRefCtrlL1(task, pps);
}

inline void FillTile(
    const FH& bs_fh
    , VAEncPictureParameterBufferAV1& pps)
{
    auto& ti = bs_fh.tile_info;
    pps.tile_cols = static_cast<mfxU8>(ti.TileCols);
    for (mfxU16 i = 0; i < pps.tile_cols; i++)
    {
        pps.width_in_sbs_minus_1[i] = static_cast<mfxU16>(ti.TileWidthInSB[i] - 1);
    }

    pps.tile_rows = static_cast<mfxU8>(ti.TileRows);
    for (mfxU16 i = 0; i < pps.tile_rows; i++)
    {
        pps.height_in_sbs_minus_1[i] = static_cast<mfxU16>(ti.TileHeightInSB[i] - 1);
    }

    pps.context_update_tile_id = static_cast<mfxU8>(ti.context_update_tile_id);
}

inline void FillCDEF(
    const SH& bs_sh
    , const FH& bs_fh
    , VAEncPictureParameterBufferAV1& pps)
{
    if(!bs_sh.enable_cdef)
        return;

    auto& cdef = bs_fh.cdef_params;
    pps.cdef_damping_minus_3 = static_cast<mfxU8>(cdef.cdef_damping - 3);
    pps.cdef_bits = static_cast<mfxU8>(cdef.cdef_bits);

    for (mfxU8 i = 0; i < CDEF_MAX_STRENGTHS; i++)
    {
        pps.cdef_y_strengths[i]  = static_cast<mfxU8>(cdef.cdef_y_pri_strength[i] * CDEF_STRENGTH_DIVISOR + cdef.cdef_y_sec_strength[i]);
        pps.cdef_uv_strengths[i] = static_cast<mfxU8>(cdef.cdef_uv_pri_strength[i] * CDEF_STRENGTH_DIVISOR + cdef.cdef_uv_sec_strength[i]);
    }
}

static mfxU8 MapSegIdBlockSizeToDDI(mfxU16 size)
{
    switch (size)
    {
    case MFX_AV1_SEGMENT_ID_BLOCK_SIZE_8x8:
        return BLOCK_8x8;
    case MFX_AV1_SEGMENT_ID_BLOCK_SIZE_32x32:
        return BLOCK_32x32;
    case MFX_AV1_SEGMENT_ID_BLOCK_SIZE_64x64:
        return BLOCK_64x64;
    case MFX_AV1_SEGMENT_ID_BLOCK_SIZE_16x16:
    default:
        return BLOCK_16x16;
    }
}

static void InitSegParam(
    const mfxExtAV1Segmentation& seg
    , const FH& bs_fh
    , VAEncPictureParameterBufferAV1& pps)
{
    pps.segments = VAEncSegParamAV1{};

    if (!bs_fh.segmentation_params.segmentation_enabled)
        return;

    auto& segFlags = pps.segments.seg_flags.bits;
    segFlags.segmentation_enabled = bs_fh.segmentation_params.segmentation_enabled;

    pps.segments.segment_number = static_cast<mfxU8>(seg.NumSegments);
    pps.seg_id_block_size       = MapSegIdBlockSizeToDDI(seg.SegmentIdBlockSize);

    segFlags.segmentation_update_map      = bs_fh.segmentation_params.segmentation_update_map;
    segFlags.segmentation_temporal_update = bs_fh.segmentation_params.segmentation_temporal_update;

    std::transform(bs_fh.segmentation_params.FeatureMask, bs_fh.segmentation_params.FeatureMask + AV1_MAX_NUM_OF_SEGMENTS,
        pps.segments.feature_mask,
        [](mfxU32 x) -> mfxU8{
            return static_cast<mfxU8>(x);
        });

    for (mfxU8 i = 0; i < AV1_MAX_NUM_OF_SEGMENTS; i++)
    {
        std::transform(bs_fh.segmentation_params.FeatureData[i], bs_fh.segmentation_params.FeatureData[i] + SEG_LVL_MAX,
            pps.segments.feature_data[i],
            [](int32_t x) -> int16_t{
                return static_cast<int16_t>(x);
            });
    }
}

static void InitSegMap(
    const mfxExtAV1Segmentation& seg
    , std::vector<mfxU8>& segment_map)
{
    segment_map.resize(seg.NumSegmentIdAlloc);
    if (seg.SegmentIds)
        std::copy_n(seg.SegmentIds, seg.NumSegmentIdAlloc, segment_map.begin());
}

static void InitSegMap(
    const ExtBuffer::Param<mfxVideoParam>& par
    , std::vector<mfxU8>& segment_map)
{
    const mfxExtAV1Segmentation& segPar = ExtBuffer::Get(par);
    if (segPar.NumSegments == 0)
    {
        segment_map.clear();
        return;
    }

    InitSegMap(segPar, segment_map);
}

inline uint16_t MapLRTypeToDDI(RestorationType lrType)
{
    switch (lrType)
    {
    case RESTORE_NONE:
        return 0;
    case RESTORE_WIENER:
        return 1;
    default:
        ThrowAssert(true, "Only RESTORE_NONE and RESTORE_WIENER are supported");
        return 0;
    }
}

void UpdatePPS(
    const TaskCommonPar& task
    , const std::vector<VASurfaceID>& rec
    , const SH& bs_sh
    , const FH& bs_fh
    ,std::vector<VAEncTileGroupBufferAV1>& tile_groups_task
    ,std::vector<VAEncTileGroupBufferAV1>& tile_groups_global
    , VAEncPictureParameterBufferAV1& pps)
{
    pps.picture_flags.bits.frame_type = bs_fh.frame_type;
    pps.base_qindex     = static_cast<mfxU8>(bs_fh.quantization_params.base_q_idx);
    pps.min_base_qindex = task.MinBaseQIndex;
    pps.max_base_qindex = task.MaxBaseQIndex;

    pps.y_dc_delta_q = static_cast<mfxI8>(bs_fh.quantization_params.DeltaQYDc);
    pps.u_dc_delta_q = static_cast<mfxI8>(bs_fh.quantization_params.DeltaQUDc);
    pps.u_ac_delta_q = static_cast<mfxI8>(bs_fh.quantization_params.DeltaQUAc);
    pps.v_dc_delta_q = static_cast<mfxI8>(bs_fh.quantization_params.DeltaQVDc);
    pps.v_ac_delta_q = static_cast<mfxI8>(bs_fh.quantization_params.DeltaQVAc);

    pps.order_hint = static_cast<mfxU8>(bs_fh.order_hint);

    //DPB and refs management
    pps.primary_ref_frame = static_cast<mfxU8>(bs_fh.primary_ref_frame);
    pps.ref_frame_ctrl_l0.value = 0;
    pps.ref_frame_ctrl_l1.value = 0;
    FillRefParams(task, bs_fh, rec, pps);

    //loop filter
    auto& lf = bs_fh.loop_filter_params;
    pps.filter_level[0] = static_cast<mfxU8>(lf.loop_filter_level[0]);
    pps.filter_level[1] = static_cast<mfxU8>(lf.loop_filter_level[1]);
    pps.filter_level_u  = static_cast<mfxU8>(lf.loop_filter_level[2]);
    pps.filter_level_v  = static_cast<mfxU8>(lf.loop_filter_level[3]);
    pps.loop_filter_flags.bits.sharpness_level        = lf.loop_filter_sharpness;
    pps.loop_filter_flags.bits.mode_ref_delta_enabled = lf.loop_filter_delta_enabled;
    pps.loop_filter_flags.bits.mode_ref_delta_update  = lf.loop_filter_delta_update;

    std::copy(lf.loop_filter_ref_deltas, lf.loop_filter_ref_deltas + TOTAL_REFS_PER_FRAME, pps.ref_deltas);
    std::copy(lf.loop_filter_mode_deltas, lf.loop_filter_mode_deltas + MAX_MODE_LF_DELTAS, pps.mode_deltas);

    //block-level deltas
    pps.mode_control_flags.bits.delta_q_present  = bs_fh.delta_q_present;
    pps.mode_control_flags.bits.delta_lf_present = bs_fh.delta_lf_present;
    pps.mode_control_flags.bits.delta_lf_multi   = bs_fh.delta_lf_multi;

    pps.mode_control_flags.bits.reference_mode = bs_fh.reference_select ?
        REFERENCE_MODE_SELECT : SINGLE_REFERENCE;
    pps.mode_control_flags.bits.skip_mode_present = bs_fh.skip_mode_present;

    FillTile(bs_fh, pps);
    FillCDEF(bs_sh, bs_fh, pps);

    //loop restoration
    pps.loop_restoration_flags.bits.yframe_restoration_type  = MapLRTypeToDDI(bs_fh.lr_params.lr_type[0]);
    pps.loop_restoration_flags.bits.cbframe_restoration_type = MapLRTypeToDDI(bs_fh.lr_params.lr_type[1]);
    pps.loop_restoration_flags.bits.crframe_restoration_type = MapLRTypeToDDI(bs_fh.lr_params.lr_type[2]);
    pps.loop_restoration_flags.bits.lr_unit_shift            = bs_fh.lr_params.lr_unit_shift;
    pps.loop_restoration_flags.bits.lr_uv_shift              = bs_fh.lr_params.lr_uv_shift;

    //context
    pps.picture_flags.bits.disable_cdf_update = bs_fh.disable_cdf_update;
    pps.picture_flags.bits.disable_frame_end_update_cdf = bs_fh.disable_frame_end_update_cdf;
    pps.picture_flags.bits.disable_frame_recon = (bs_fh.refresh_frame_flags == 0);

    //Tile Groups
    if(!tile_groups_task.empty())
        pps.num_tile_groups_minus1 = static_cast<mfxU8>(tile_groups_task.size() - 1);
    else
        pps.num_tile_groups_minus1 = static_cast<mfxU8>(tile_groups_global.size() - 1);

    pps.tile_group_obu_hdr_info.bits.obu_extension_flag = 0;
    pps.tile_group_obu_hdr_info.bits.obu_has_size_field = 1;
    pps.tile_group_obu_hdr_info.bits.temporal_id = 0;
    pps.tile_group_obu_hdr_info.bits.spatial_id  = 0;

    //other params
    pps.picture_flags.bits.error_resilient_mode = bs_fh.error_resilient_mode;
    pps.picture_flags.bits.enable_frame_obu     = (task.InsertHeaders & INSERT_FRM_OBU) ? 1 : 0;
    pps.picture_flags.bits.allow_intrabc        = bs_fh.allow_intrabc;
    pps.reconstructed_frame = rec.at(task.Rec.Idx);

    //offsets
    auto& offsets = task.Offsets;
    pps.size_in_bits_frame_hdr_obu     = offsets.FrameHeaderOBUSizeInBits;
    pps.byte_offset_frame_hdr_obu_size = offsets.FrameHeaderOBUSizeByteOffset;
    pps.bit_offset_loopfilter_params   = offsets.LoopFilterParamsBitOffset;
    pps.bit_offset_qindex              = offsets.QIndexBitOffset;
    pps.bit_offset_segmentation        = offsets.SegmentationBitOffset;
    pps.bit_offset_cdef_params         = offsets.CDEFParamsBitOffset;
    pps.size_in_bits_cdef_params       = offsets.CDEFParamsSizeInBits;

    //q_matrix
    pps.qmatrix_flags.bits.using_qmatrix = bs_fh.quantization_params.using_qmatrix;
    pps.qmatrix_flags.bits.qm_y = bs_fh.quantization_params.qm_y;
    pps.qmatrix_flags.bits.qm_u = bs_fh.quantization_params.qm_u;
    pps.qmatrix_flags.bits.qm_v = bs_fh.quantization_params.qm_v;

#if VA_CHECK_VERSION(1, 16, 0)
    pps.hierarchical_level_plus1 = static_cast<mfxU8>(task.PyramidLevel + 1);
#else
    pps.reserved8bits0 = static_cast<mfxU8>(task.PyramidLevel + 1);
#endif

    pps.skip_frames_reduced_size = static_cast<mfxI32>(task.RepeatedFrameBytes);
}

inline void AddVaMiscHRD(
    const Glob::VideoParam::TRef& par
    , std::list<std::vector<mfxU8>>& buf)
{
    auto& hrd = AddVaMisc<VAEncMiscParameterHRD>(VAEncMiscParameterTypeHRD, buf);

    uint32_t bNeedBufParam =
        par.mfx.RateControlMethod == MFX_RATECONTROL_CBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR;

    hrd.initial_buffer_fullness = bNeedBufParam * InitialDelayInKB(par.mfx) * 8000;
    hrd.buffer_size             = bNeedBufParam * BufferSizeInKB(par.mfx) * 8000;
}

inline void AddVaMiscTemporalLayer(
    const Glob::VideoParam::TRef& /*par*/
    , std::list<std::vector<mfxU8>>& buf)
{
    auto& tempLayer = AddVaMisc<VAEncMiscParameterTemporalLayerStructure>(VAEncMiscParameterTypeTemporalLayerStructure, buf);

    tempLayer.number_of_layers = 0;
}

inline void AddVaMiscFR(
    const Glob::VideoParam::TRef& par
    , std::list<std::vector<mfxU8>>& buf)
{
    auto& fr = AddVaMisc<VAEncMiscParameterFrameRate>(VAEncMiscParameterTypeFrameRate, buf);

    PackMfxFrameRate(par.mfx.FrameInfo.FrameRateExtN, par.mfx.FrameInfo.FrameRateExtD, fr.framerate);
}

inline void AddVaMiscRC(
    const Glob::VideoParam::TRef& par
    , const Glob::FH::TRef& bs_fh
    , const Task::Common::TRef& task
    , std::list<std::vector<mfxU8>>& buf
    , bool bResetBRC = false)
{
    auto& rc = AddVaMisc<VAEncMiscParameterRateControl>(VAEncMiscParameterTypeRateControl, buf);

    mfxU32 bNeedRateParam =
        par.mfx.RateControlMethod == MFX_RATECONTROL_CBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR;

    rc.bits_per_second = bNeedRateParam * MaxKbps(par.mfx) * 1000;

    if(rc.bits_per_second)
        rc.target_percentage = mfxU32(100.0 * (mfxF64)TargetKbps(par.mfx) / (mfxF64)MaxKbps(par.mfx));

    rc.rc_flags.bits.reset = bNeedRateParam && bResetBRC;
    rc.quality_factor = par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ ? par.mfx.ICQQuality : 0;

    const mfxExtCodingOption2* CO2 = ExtBuffer::Get(par);

    if (CO2)
    {
        //MBBRC control
        //Control VA_RC_MB 0: default, 1: enable, 2: disable, other: reserved
        rc.rc_flags.bits.mb_rate_control = IsOn(CO2->MBBRC) +IsOff(CO2->MBBRC) * 2;
    }

    // TCBRC control
#if VA_CHECK_VERSION(1, 10, 0)
    rc.target_frame_size = task.TCBRCTargetFrameSize;
#endif

    const mfxExtCodingOption3* CO3 = ExtBuffer::Get(par);
    if (CO3)
    {
        rc.rc_flags.bits.frame_tolerance_mode =
            IsOn(CO3->LowDelayBRC) ? eFrameSizeTolerance_ExtremelyLow : eFrameSizeTolerance_Normal;
    }

    rc.initial_qp         = bs_fh.quantization_params.base_q_idx;
}

inline void AddVaMiscTU(
    const Glob::VideoParam::TRef& par
    , std::list<std::vector<mfxU8>>& buf)
{
    auto& tu = AddVaMisc<VAEncMiscParameterBufferQualityLevel>(VAEncMiscParameterTypeQualityLevel, buf);
    tu.quality_level = par.mfx.TargetUsage;
}

void VAPacker::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetCallChains
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        MFX_CHECK(!strg.Contains(CC::Key), MFX_ERR_NONE);

        const auto& par = Glob::VideoParam::Get(strg);
        auto& cc = CC::GetOrConstruct(strg);

        cc.InitSPS.Push([&par](
            CallChains::TInitSPS::TExt
            , const StorageR& glob
            , VAEncSequenceParameterBufferAV1& sps)
        {
            const auto& bs_sh = Glob::SH::Get(glob);
            InitSPS(par, bs_sh, sps);
        });

        cc.InitPPS.Push([&par](
            CallChains::TInitPPS::TExt
            , const StorageR& glob
            , VAEncPictureParameterBufferAV1& pps)
        {
            const auto& bs_fh = Glob::FH::Get(glob);
            InitPPS(par, bs_fh, pps);
        });

        cc.UpdatePPS.Push([this](
            CallChains::TUpdatePPS::TExt
            , const StorageR& global
            , const StorageR& s_task
            , VAEncPictureParameterBufferAV1& pps)
        {
            const auto& bs_sh = Glob::SH::Get(global);
            const auto& bs_fh = Task::FH::Get(s_task);
            UpdatePPS(Task::Common::Get(s_task), GetResources(RES_REF), bs_sh, bs_fh, m_tile_groups_task, m_tile_groups_global, pps);
        });

        return MFX_ERR_NONE;
    });
}

void VAPacker::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_Init
    , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        auto& core = Glob::VideoCore::Get(strg);

        const auto& pInfos = Glob::TileGroups::Get(strg);
        InitTileGroupsBuffer(pInfos, m_tile_groups_global);

        const auto& par   = Glob::VideoParam::Get(strg);
        const auto& bs_sh = Glob::SH::Get(strg);
        const auto& bs_fh = Glob::FH::Get(strg);

        auto& cc = CC::GetOrConstruct(strg);
        cc.InitSPS(strg, m_sps);
        cc.InitPPS(strg, m_pps);

        cc.AddPerSeqMiscData[VAEncMiscParameterTypeHRD].Push([this, &par](
            VAPacker::CallChains::TAddMiscData::TExt
            , const StorageR& strg
            , const StorageR& local
            , std::list<std::vector<mfxU8>>& data)
        {
            AddVaMiscHRD(par, data);
            return true;
        });
        cc.AddPerSeqMiscData[VAEncMiscParameterTypeTemporalLayerStructure].Push([this, &par](
            VAPacker::CallChains::TAddMiscData::TExt
            , const StorageR& strg
            , const StorageR& local
            , std::list<std::vector<mfxU8>>& data)
        {
            AddVaMiscTemporalLayer(par, data);
            return true;
        });
        cc.AddPerPicMiscData[VAEncMiscParameterTypeRateControl].Push([this](
            VAPacker::CallChains::TAddMiscData::TExt
            , const StorageR& strg
            , const StorageR& s_task
            , std::list<std::vector<mfxU8>>& data)
        {
            const auto& par   = Glob::VideoParam::Get(strg);
            const auto& bs_fh = Glob::FH::Get(strg);
            const auto& task  = Task::Common::Get(s_task);

            AddVaMiscRC(par, bs_fh, task, data);
            return true;
        });
        cc.AddPerSeqMiscData[VAEncMiscParameterTypeFrameRate].Push([this, &par](
            VAPacker::CallChains::TAddMiscData::TExt
            , const StorageR& strg
            , const StorageR& local
            , std::list<std::vector<mfxU8>>& data)
        {
            AddVaMiscFR(par, data);
            return true;
        });
        cc.AddPerSeqMiscData[VAEncMiscParameterTypeQualityLevel].Push([this, &par](
            VAPacker::CallChains::TAddMiscData::TExt
            , const StorageR& strg
            , const StorageR& local
            , std::list<std::vector<mfxU8>>& data)
        {
            AddVaMiscTU(par, data);
            return true;
        });

        auto& vaInitPar = Tmp::DDI_InitParam::GetOrConstruct(local);
        auto& bsInfo    = Glob::AllocBS::Get(strg);

        SetMaxBs(bsInfo.GetInfo().Width * bsInfo.GetInfo().Height);

        for (auto& AddMisc : cc.AddPerSeqMiscData)
        {
            if (AddMisc.second(strg, local, m_vaPerSeqMiscData))
            {
                auto& misc = m_vaPerSeqMiscData.back();
                vaInitPar.push_back(PackVaMiscPar(misc));
            }
        }

        mfxStatus sts = Register(core, Glob::AllocRec::Get(strg).GetResponse(), RES_REF);
        MFX_CHECK_STS(sts);

        sts = Register(core, bsInfo.GetResponse(), RES_BS);
        MFX_CHECK_STS(sts);

        auto& resources = Glob::DDI_Resources::GetOrConstruct(strg);
        DDIExecParam xPar;

        xPar.Function = MFX_FOURCC_NV12;
        PackResources(xPar.Resource, RES_REF);
        resources.push_back(xPar);

        xPar.Function = MFX_FOURCC_P8;
        PackResources(xPar.Resource, RES_BS);
        resources.push_back(xPar);

        InitFeedback(bsInfo.GetResponse().NumFrameActual);

        GetFeedbackInterface(Glob::DDI_Feedback::GetOrConstruct(strg));

        Glob::DDI_SubmitParam::GetOrConstruct(strg);

        cc.ReadFeedback.Push([this](
            CallChains::TReadFeedback::TExt
            , const StorageR& /*global*/
            , StorageW& s_task
            , const VACodedBufferSegment& fb)
        {
            return ReadFeedback(fb, Task::Common::Get(s_task).BsDataLength);
        });

        return MFX_ERR_NONE;
    });
}

void VAPacker::ResetState(const FeatureBlocks& /*blocks*/, TPushRS Push)
{
    Push(BLK_Reset
        ,[this](StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        const auto& par    = Glob::VideoParam::Get(strg);
        const auto& bs_fh  = Glob::FH::Get(strg);

        const auto& pInfos = Glob::TileGroups::Get(strg);
        InitTileGroupsBuffer(pInfos, m_tile_groups_global);

        auto& cc = CC::GetOrConstruct(strg);
        cc.InitSPS(strg, m_sps);
        cc.InitPPS(strg, m_pps);

        InitSegMap(par, m_segment_map);

        m_segment = {};
        m_segment.segmentMapDataSize = (mfxU32)m_segment_map.size();
        m_segment.pSegmentMap        = m_segment_map.data();

        m_vaPerSeqMiscData.clear();
        cc.AddPerSeqMiscData[VAEncMiscParameterTypeHRD].Push([this, &par](
            VAPacker::CallChains::TAddMiscData::TExt
            , const StorageR& strg
            , const StorageR& local
            , std::list<std::vector<mfxU8>>& data)
        {
            AddVaMiscHRD(par, data);
            return true;
        });
        cc.AddPerSeqMiscData[VAEncMiscParameterTypeTemporalLayerStructure].Push([this, &par](
            VAPacker::CallChains::TAddMiscData::TExt
            , const StorageR& strg
            , const StorageR& local
            , std::list<std::vector<mfxU8>>& data)
        {
            AddVaMiscTemporalLayer(par, data);
            return true;
        });
        cc.AddPerPicMiscData[VAEncMiscParameterTypeRateControl].Push([this](
            VAPacker::CallChains::TAddMiscData::TExt
            , const StorageR& strg
            , const StorageR& s_task
            , std::list<std::vector<mfxU8>>& data)
        {
            const auto& par   = Glob::VideoParam::Get(strg);
            const auto& bs_fh = Glob::FH::Get(strg);
            const auto& task  = Task::Common::Get(s_task);

            AddVaMiscRC(par, bs_fh, task, data, !!(Glob::ResetHint::Get(strg).Flags & RF_BRC_RESET));
            return true;
        });
        cc.AddPerSeqMiscData[VAEncMiscParameterTypeFrameRate].Push([this, &par](
            VAPacker::CallChains::TAddMiscData::TExt
            , const StorageR& strg
            , const StorageR& local
            , std::list<std::vector<mfxU8>>& data)
        {
            AddVaMiscFR(par, data);
            return true;
        });
        cc.AddPerSeqMiscData[VAEncMiscParameterTypeQualityLevel].Push([this, &par](
            VAPacker::CallChains::TAddMiscData::TExt
            , const StorageR& strg
            , const StorageR& local
            , std::list<std::vector<mfxU8>>& data)
        {
            AddVaMiscTU(par, data);
            return true;
        });

        auto& vaInitPar = Tmp::DDI_InitParam::GetOrConstruct(local);
        vaInitPar.clear();

        for (auto& AddMisc : cc.AddPerSeqMiscData)
        {
            if (AddMisc.second(strg, local, m_vaPerSeqMiscData))
            {
                auto& misc = m_vaPerSeqMiscData.back();
                vaInitPar.push_back(PackVaMiscPar(misc));
            }
        }

        return MFX_ERR_NONE;
    });
}

void VAPacker::SubmitTask(const FeatureBlocks& blocks, TPushST Push)
{
    Push(BLK_SubmitTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& task  = Task::Common::Get(s_task);
        auto& ph    = Glob::PackedHeaders::Get(global);
        auto& bs_sh = Glob::SH::Get(global);
        auto& bs_fh = Task::FH::Get(s_task);

        MFX_LOG_INFO("Task[%d] Submitting: %dx%d, frame_type=%d\n", task.StatusReportId, bs_fh.FrameWidth, bs_fh.FrameHeight, bs_fh.frame_type);

        const auto& tileGroupInfos = Task::TileGroups::Get(s_task);
        if(tileGroupInfos.empty())
            m_tile_groups_task.clear();
        else
            InitTileGroupsBuffer(tileGroupInfos, m_tile_groups_task);

        auto& cc = CC::Get(global);
        cc.UpdatePPS(global, s_task, m_pps);

        const auto& seg = Task::Segment::Get(s_task);
        InitSegParam(seg, bs_fh, m_pps);
        InitSegMap(seg, m_segment_map);

        m_segment = {};
        m_segment.segmentMapDataSize = (mfxU32)m_segment_map.size();
        m_segment.pSegmentMap        = m_segment_map.data();

        m_pps.coded_buf = GetResources(RES_BS).at(task.BS.Idx);

        auto& par = Glob::DDI_SubmitParam::Get(global);
        par.clear();

        par.push_back(PackVaBuffer(VAEncSequenceParameterBufferType, m_sps));
        par.push_back(PackVaBuffer(VAEncPictureParameterBufferType, m_pps));

        if (!m_tile_groups_task.empty())
        {
            std::transform(m_tile_groups_task.begin(), m_tile_groups_task.end(), std::back_inserter(par)
                , [&](VAEncTileGroupBufferAV1& t) { return PackVaBuffer(VAEncSliceParameterBufferType, t); });
        }
        else
        {
            std::transform(m_tile_groups_global.begin(), m_tile_groups_global.end(), std::back_inserter(par)
                , [&](VAEncTileGroupBufferAV1& t) { return PackVaBuffer(VAEncSliceParameterBufferType, t); });
        }

        if (m_segment.segmentMapDataSize)
        {
            par.push_back(PackVaBuffer(VAEncMacroblockMapBufferType, m_segment));
        }

        m_vaPerPicMiscData.clear();
        m_vaPackedHeaders.clear();

        AddPackedHeaderIf(!!((task.InsertHeaders & INSERT_IVF_SEQ) || (task.InsertHeaders & INSERT_IVF_FRM))
            , ph.IVF, par, VAEncPackedHeaderAV1_SPS);

        AddPackedHeaderIf(!!(task.InsertHeaders & INSERT_TD)
            , ph.TD, par, VAEncPackedHeaderAV1_SPS);

        AddPackedHeaderIf(!!(task.InsertHeaders & INSERT_SPS)
            , ph.SPS, par, VAEncPackedHeaderAV1_SPS);

        AddPackedHeaderIf(!!(task.InsertHeaders & INSERT_HDR)
            , ph.HDR, par, VAEncPackedHeaderAV1_SPS);

        AddPackedHeaderIf(!!(task.InsertHeaders & INSERT_PPS)
            , ph.PPS, par, VAEncPackedHeaderAV1_PPS);

        for (auto& AddMisc : cc.AddPerPicMiscData)
        {
            if (AddMisc.second(global, s_task, m_vaPerPicMiscData))
            {
                auto& misc = m_vaPerPicMiscData.back();
                par.push_back(PackVaBuffer(VAEncMiscParameterBufferType, misc.data(), (mfxU32)misc.size()));
            }
        }

        SetFeedback(task.StatusReportId, *(VASurfaceID*)task.HDLRaw.first, GetResources(RES_BS).at(task.BS.Idx));

        return MFX_ERR_NONE;
    });
}

void VAPacker::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_QueryTask
        , [](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& fb = Glob::DDI_Feedback::Get(global);
        MFX_CHECK(!fb.bNotReady, MFX_TASK_BUSY);

        auto& task = Task::Common::Get(s_task);

        MFX_CHECK((task.SkipCMD & SKIPCMD_NeedDriverCall), MFX_ERR_NONE);

        auto pFB = fb.Get(task.StatusReportId);
        MFX_CHECK(pFB, MFX_TASK_BUSY);

        auto& rtErr = Glob::RTErr::Get(global);

        auto sts = CC::Get(global).ReadFeedback(global, s_task, *(const VACodedBufferSegment*)pFB);
        SetIf(rtErr, sts < MFX_ERR_NONE, sts);

        fb.Remove(task.StatusReportId);

        return sts;
    });
}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
