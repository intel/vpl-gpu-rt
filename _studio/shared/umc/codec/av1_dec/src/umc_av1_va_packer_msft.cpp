// Copyright (c) 2017-2020 Intel Corporation
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



#include "umc_vp9_dec_defs.h"

#ifdef MFX_ENABLE_AV1_VIDEO_DECODE

#ifdef UMC_VA_DXVA

#include <algorithm>
#include "umc_structures.h"
#include "umc_vp9_bitstream.h"
#include "umc_vp9_frame.h"
#include "umc_av1_utils.h"

#include "umc_av1_frame.h"
#include "umc_av1_va_packer_dxva.h"
#include "umc_av1_msft_ddi.h"

#include "umc_va_base.h"

using namespace UMC;

namespace UMC_AV1_DECODER
{
    PackerMSFT::PackerMSFT(VideoAccelerator * va)
        : PackerDXVA(va)
    {}

    void PackerMSFT::PackAU(std::vector<TileSet>& tileSets, AV1DecoderFrame const& info, bool firtsSubmission)
    {
        if (firtsSubmission)
        {
            // it's first submission for current frame - need to fill and submit picture parameters
            UMC::UMCVACompBuffer* compBufPic = NULL;
            DXVA_PicParams_AV1_MSFT *picParam = (DXVA_PicParams_AV1_MSFT*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBufPic);
            if (!picParam || !compBufPic || (compBufPic->GetBufferSize() < sizeof(DXVA_PicParams_AV1_MSFT)))
                throw UMC_VP9_DECODER::vp9_exception(MFX_ERR_MEMORY_ALLOC);

            compBufPic->SetDataSize(sizeof(DXVA_PicParams_AV1_MSFT));
            *picParam = DXVA_PicParams_AV1_MSFT{};

            PackPicParams(*picParam, info);
        }

        UMC::UMCVACompBuffer* compBufBs = nullptr;
        uint8_t* const bistreamData = (uint8_t *)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &compBufBs);
        if (!bistreamData || !compBufBs)
            throw av1_exception(MFX_ERR_MEMORY_ALLOC);

        std::vector<DXVA_Tile_AV1> tileControlParams;

        size_t offsetInBuffer = 0;
        for (auto& tileSet : tileSets)
        {
            const size_t spaceInBuffer = compBufBs->GetBufferSize() - offsetInBuffer;
            TileLayout layout;
            const size_t bytesSubmitted = tileSet.Submit(bistreamData, spaceInBuffer, offsetInBuffer, layout);

            if (bytesSubmitted)
            {
                offsetInBuffer += bytesSubmitted;

                for (auto& loc : layout)
                {
                    tileControlParams.emplace_back();
                    PackTileControlParams(tileControlParams.back(), loc);
                }
            }
        }
        compBufBs->SetDataSize(static_cast<uint32_t>(offsetInBuffer));

        UMCVACompBuffer* compBufTile = nullptr;
        const int32_t tileControlInfoSize = static_cast<int32_t>(sizeof(DXVA_Tile_AV1) * tileControlParams.size());
        DXVA_Tile_AV1 *tileControlParam = (DXVA_Tile_AV1*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &compBufTile);
        if (!tileControlParam || !compBufTile || (compBufTile->GetBufferSize() < tileControlInfoSize))
            throw av1_exception(MFX_ERR_MEMORY_ALLOC);

        std::copy(tileControlParams.begin(), tileControlParams.end(), tileControlParam);
        compBufTile->SetDataSize(tileControlInfoSize);
    }


    void PackerMSFT::PackPicParams(DXVA_PicParams_AV1_MSFT& picParam, AV1DecoderFrame const& frame)
    {
        SequenceHeader const& sh = frame.GetSeqHeader();

        FrameHeader const& info =
            frame.GetFrameHeader();

        picParam.width = (USHORT)frame.GetUpscaledWidth();
        picParam.height = (USHORT)frame.GetFrameHeight();
        picParam.max_width = (USHORT)frame.GetUpscaledWidth();
        picParam.max_height = (USHORT)frame.GetFrameHeight();
        picParam.CurrPicTextureIndex = (UCHAR)frame.GetMemID(SURFACE_RECON);
        picParam.superres_denom = (UCHAR)info.SuperresDenom;
        picParam.BitDepth = (UCHAR)sh.color_config.BitDepth;
        picParam.profile = (UCHAR)sh.seq_profile;

        // Tiles:

        picParam.tiles.cols = (UCHAR)info.tile_info.TileCols;
        picParam.tiles.rows = (UCHAR)info.tile_info.TileRows;
        for (uint32_t i = 0; i < picParam.tiles.cols; i++)
        {
            picParam.tiles.widths[i] =
                (USHORT)(info.tile_info.SbColStarts[i + 1] - info.tile_info.SbColStarts[i]);
        }

        for (int i = 0; i < picParam.tiles.rows; i++)
        {
            picParam.tiles.heights[i] =
                (USHORT)(info.tile_info.SbRowStarts[i + 1] - info.tile_info.SbRowStarts[i]);
        }

        picParam.tiles.context_update_id = (USHORT)info.tile_info.context_update_tile_id;

        // Coding Tools

        picParam.coding.use_128x128_superblock = (sh.sbSize == BLOCK_128X128) ? 1 : 0;
        picParam.coding.intra_edge_filter = sh.enable_intra_edge_filter;
        picParam.coding.interintra_compound = sh.enable_interintra_compound;
        picParam.coding.masked_compound = sh.enable_masked_compound;
        picParam.coding.warped_motion = info.allow_warped_motion;
        picParam.coding.dual_filter = sh.enable_dual_filter;
        picParam.coding.jnt_comp = sh.enable_jnt_comp;
        picParam.coding.screen_content_tools = info.allow_screen_content_tools;
        picParam.coding.integer_mv = info.force_integer_mv;
        picParam.coding.cdef = sh.enable_cdef;
        picParam.coding.restoration = sh.enable_restoration;
        picParam.coding.film_grain = sh.film_grain_param_present;
        picParam.coding.intrabc = info.allow_intrabc;
        picParam.coding.high_precision_mv = info.allow_high_precision_mv;
        picParam.coding.switchable_motion_mode = info.is_motion_mode_switchable;
        picParam.coding.filter_intra = sh.enable_filter_intra;
        picParam.coding.disable_frame_end_update_cdf = info.disable_frame_end_update_cdf;
        picParam.coding.disable_cdf_update = info.disable_cdf_update;
        picParam.coding.reference_mode = (info.reference_mode == 0) ? 0 : 1;
        picParam.coding.skip_mode = info.skip_mode_present;
        picParam.coding.reduced_tx_set = info.reduced_tx_set;

        picParam.coding.superres = (info.SuperresDenom == SCALE_NUMERATOR) ? 0 : 1;
        picParam.coding.tx_mode = info.TxMode;
        picParam.coding.use_ref_frame_mvs = info.use_ref_frame_mvs;
        picParam.coding.enable_ref_frame_mvs = 0;
        picParam.coding.reference_frame_update = 0;

        // Format & Picture Info flags

        picParam.format.frame_type = info.frame_type;
        picParam.format.show_frame = info.show_frame;
        picParam.format.showable_frame = info.showable_frame;
        picParam.format.subsampling_x = sh.color_config.subsampling_x;
        picParam.format.subsampling_y = sh.color_config.subsampling_y;
        picParam.format.mono_chrome = sh.color_config.mono_chrome;

        // References
        picParam.primary_ref_frame = (UCHAR)info.primary_ref_frame;
        picParam.order_hint = (UCHAR)info.order_hint;
        picParam.order_hint_bits = (UCHAR)sh.order_hint_bits_minus1 + 1;


        for (uint8_t ref = 0; ref < NUM_REF_FRAMES; ++ref)
        {
            picParam.ref_frame_map_texture_index[ref] = (UCHAR)frame.frame_dpb[ref]->GetMemID(SURFACE_RECON);

        }
        for (uint8_t ref_idx = 0; ref_idx < INTER_REFS; ref_idx++)
        {
            uint8_t idxInDPB = (uint8_t)info.ref_frame_idx[ref_idx];

            picParam.frame_refs[ref_idx].Index = idxInDPB;
        }
        
        // fill global motion params
        for (uint8_t i = 0; i < INTER_REFS; i++)
        {
            picParam.frame_refs[i].wmtype = static_cast<UCHAR>(info.global_motion_params[i + 1].wmtype);
            picParam.frame_refs[i].wminvalid = static_cast<UCHAR>(info.global_motion_params[i + 1].invalid);
            for (uint8_t j = 0; j < 6; j++)
            {
                picParam.frame_refs[i].wmmat[j] = info.global_motion_params[i + 1].wmmat[j];
                // TODO: [Rev0.5] implement processing of alpha, beta, gamma, delta.
            }
        }

        // Loop filter parameters

        picParam.loop_filter.filter_level[0] = (UCHAR)info.loop_filter_params.loop_filter_level[0];
        picParam.loop_filter.filter_level[1] = (UCHAR)info.loop_filter_params.loop_filter_level[1];
        picParam.loop_filter.filter_level_u = (UCHAR)info.loop_filter_params.loop_filter_level[2];

        picParam.loop_filter.filter_level_v = (UCHAR)info.loop_filter_params.loop_filter_level[3];
        picParam.loop_filter.sharpness_level = (UCHAR)info.loop_filter_params.loop_filter_sharpness;

        picParam.loop_filter.mode_ref_delta_enabled = info.loop_filter_params.loop_filter_delta_enabled;
        picParam.loop_filter.mode_ref_delta_update = info.loop_filter_params.loop_filter_delta_update;
        picParam.loop_filter.delta_lf_multi = info.delta_lf_multi;
        picParam.loop_filter.delta_lf_present = info.delta_lf_present;

        for (uint8_t i = 0; i < TOTAL_REFS; i++)
        {
            picParam.loop_filter.ref_deltas[i] = info.loop_filter_params.loop_filter_ref_deltas[i];
        }
        for (uint8_t i = 0; i < UMC_VP9_DECODER::MAX_MODE_LF_DELTAS; i++)
        {
            picParam.loop_filter.mode_deltas[i] = info.loop_filter_params.loop_filter_mode_deltas[i];
        }
        //picParam.loop_filter.delta_lf_res = CeilLog2(info.delta_lf_res);
        picParam.loop_filter.delta_lf_res = (UCHAR)info.delta_lf_res;

        picParam.loop_filter.log2_restoration_unit_size[0] = (USHORT)(log(256) / log(2) - 2 + info.lr_params.lr_unit_shift);
        uint32_t uv_shift;
        uv_shift = info.lr_params.lr_uv_shift;
        if (sh.color_config.subsampling_x && sh.color_config.subsampling_y)
        {
        }
        else {
            uv_shift = 0;
        }
        picParam.loop_filter.log2_restoration_unit_size[1] = (USHORT)(picParam.loop_filter.log2_restoration_unit_size[0] - info.lr_params.lr_uv_shift);
        picParam.loop_filter.log2_restoration_unit_size[2] = (USHORT)(picParam.loop_filter.log2_restoration_unit_size[0] - info.lr_params.lr_uv_shift);

        picParam.loop_filter.frame_restoration_type[0] = (UCHAR)info.lr_params.lr_type[0];
        picParam.loop_filter.frame_restoration_type[1] = (UCHAR)info.lr_params.lr_type[1];
        picParam.loop_filter.frame_restoration_type[2] = (UCHAR)info.lr_params.lr_type[2];

        // Quantization

        picParam.quantization.delta_q_present = info.delta_q_present;
        //picParam.quantization.delta_q_res = CeilLog2(info.delta_q_res);
        picParam.quantization.delta_q_res = CeilLog2(info.delta_q_res);

        picParam.quantization.base_qindex = (UCHAR)info.quantization_params.base_q_idx;
        picParam.quantization.y_dc_delta_q = (CHAR)info.quantization_params.DeltaQYDc;
        picParam.quantization.u_dc_delta_q = (CHAR)info.quantization_params.DeltaQUDc;
        picParam.quantization.v_dc_delta_q = (CHAR)info.quantization_params.DeltaQVDc;
        picParam.quantization.u_ac_delta_q = (CHAR)info.quantization_params.DeltaQUAc;
        picParam.quantization.v_ac_delta_q = (CHAR)info.quantization_params.DeltaQVAc;

        if (info.quantization_params.using_qmatrix == 0)
        {
            picParam.quantization.qm_y = 0xFF;
            picParam.quantization.qm_u = 0xFF;
            picParam.quantization.qm_v = 0xFF;

        }
        else {
            picParam.quantization.qm_y = (UCHAR)info.quantization_params.qm_y;
            picParam.quantization.qm_u = (UCHAR)info.quantization_params.qm_u;
            picParam.quantization.qm_v = (UCHAR)info.quantization_params.qm_v;
        }
        picParam.quantization.Reserved16Bits = 0;

        // Cdef parameters

        picParam.cdef.damping = (UCHAR)(info.cdef_params.cdef_damping - 3);
        picParam.cdef.bits = (UCHAR)info.cdef_params.cdef_bits;

        for (uint8_t i = 0; i < CDEF_MAX_STRENGTHS; i++)
        {
#if UMC_AV1_DECODER_REV >= 8500
            picParam.cdef.y_strengths[i].primary = info.cdef_params.cdef_y_pri_strength[i];
            picParam.cdef.y_strengths[i].secondary = info.cdef_params.cdef_y_sec_strength[i];

            //picParam.cdef.y_strengths[i].combined = (UCHAR)((info.cdef_params.cdef_y_pri_strength[i] << 2) + info.cdef_params.cdef_y_sec_strength[i]);

            picParam.cdef.uv_strengths[i].primary = info.cdef_params.cdef_uv_pri_strength[i];
            picParam.cdef.uv_strengths[i].secondary = info.cdef_params.cdef_uv_sec_strength[i];

            //picParam.cdef.uv_strengths[i].combined = (UCHAR)((info.cdef_params.cdef_uv_pri_strength[i] << 2) + info.cdef_params.cdef_uv_sec_strength[i]);
#else
            picParam.cdef_y_strengths[i] = (UCHAR)info.cdef_params.cdef_y_strength[i];
            picParam.cdef_uv_strengths[i] = (UCHAR)info.cdef_params.cdef_uv_strength[i];
#endif
        }

        picParam.interp_filter = (UCHAR)info.interpolation_filter;

        // Segmentation
        picParam.segmentation.enabled = info.segmentation_params.segmentation_enabled;
        picParam.segmentation.update_map = info.segmentation_params.segmentation_update_map;
        picParam.segmentation.update_data = info.segmentation_params.segmentation_update_data;
        picParam.segmentation.temporal_update = info.segmentation_params.segmentation_temporal_update;
        //picParam.segmentation.Reserved24Bits = 0;
        picParam.segmentation.update_map = info.segmentation_params.segmentation_update_map;
        picParam.segmentation.update_map = info.segmentation_params.segmentation_update_map;

        if (picParam.segmentation.enabled)
        {
            for (uint8_t i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; i++)
            {
                picParam.segmentation.feature_mask[i].mask = (UCHAR)info.segmentation_params.FeatureMask[i];
                for (uint8_t j = 0; j < SEG_LVL_MAX; j++)
                    picParam.segmentation.feature_data[i][j] = (SHORT)info.segmentation_params.FeatureData[i][j];
            }
        }

        //film grain
        picParam.film_grain.matrix_coeff_is_identity = (sh.color_config.matrix_coefficients == AOM_CICP_MC_IDENTITY) ? 1 : 0;
        picParam.film_grain.apply_grain = info.film_grain_params.apply_grain;
        picParam.film_grain.scaling_shift_minus8 = info.film_grain_params.grain_scaling - 8;
        picParam.film_grain.chroma_scaling_from_luma = info.film_grain_params.chroma_scaling_from_luma;
        picParam.film_grain.ar_coeff_lag = info.film_grain_params.ar_coeff_lag;
        picParam.film_grain.ar_coeff_shift_minus6 = info.film_grain_params.ar_coeff_shift - 6;
        picParam.film_grain.grain_scale_shift = info.film_grain_params.grain_scale_shift;
        picParam.film_grain.overlap_flag = info.film_grain_params.overlap_flag;
        picParam.film_grain.clip_to_restricted_range = info.film_grain_params.clip_to_restricted_range;
        picParam.film_grain.grain_seed = (USHORT)info.film_grain_params.grain_seed;

        for (uint8_t i = 0; i < MAX_POINTS_IN_SCALING_FUNCTION_LUMA; i++)
        {
            picParam.film_grain.scaling_points_y[i][0] = (UCHAR)info.film_grain_params.point_y_value[i];
            picParam.film_grain.scaling_points_y[i][1] = (UCHAR)info.film_grain_params.point_y_scaling[i];
        }

        picParam.film_grain.num_y_points = (UCHAR)info.film_grain_params.num_y_points;
        picParam.film_grain.num_cb_points = (UCHAR)info.film_grain_params.num_cb_points;
        picParam.film_grain.num_cr_points = (UCHAR)info.film_grain_params.num_cr_points;


        for (uint8_t i = 0; i < MAX_POINTS_IN_SCALING_FUNCTION_CHROMA; i++)
        {
            picParam.film_grain.scaling_points_cb[i][0] = (UCHAR)info.film_grain_params.point_cb_value[i];
            picParam.film_grain.scaling_points_cb[i][1] = (UCHAR)info.film_grain_params.point_cb_scaling[i];
            picParam.film_grain.scaling_points_cr[i][0] = (UCHAR)info.film_grain_params.point_cr_value[i];
            picParam.film_grain.scaling_points_cr[i][1] = (UCHAR)info.film_grain_params.point_cr_scaling[i];
        }

        for (uint8_t i = 0; i < MAX_AUTOREG_COEFFS_LUMA; i++)
            picParam.film_grain.ar_coeffs_y[i] = (CHAR)info.film_grain_params.ar_coeffs_y[i];

        for (uint8_t i = 0; i < MAX_AUTOREG_COEFFS_CHROMA; i++)
        {
            picParam.film_grain.ar_coeffs_cb[i] = (CHAR)info.film_grain_params.ar_coeffs_cb[i];
            picParam.film_grain.ar_coeffs_cr[i] = (CHAR)info.film_grain_params.ar_coeffs_cr[i];
        }

        picParam.film_grain.cb_mult = (UCHAR)info.film_grain_params.cb_mult;
        picParam.film_grain.cb_luma_mult = (UCHAR)info.film_grain_params.cb_luma_mult;
        picParam.film_grain.cb_offset = (USHORT)info.film_grain_params.cb_offset;
        picParam.film_grain.cr_mult = (UCHAR)info.film_grain_params.cr_mult;
        picParam.film_grain.cr_luma_mult = (UCHAR)info.film_grain_params.cr_luma_mult;
        picParam.film_grain.cr_offset = (USHORT)info.film_grain_params.cr_offset;

        picParam.StatusReportFeedbackNumber = ++m_report_counter;
    }

    void PackerMSFT::PackTileControlParams(DXVA_Tile_AV1& tileControlParam, TileLocation const& loc)
    {
        tileControlParam.dataOffset = (UINT)loc.offset;
        tileControlParam.dataSize = (UINT)loc.size;
        //tileControlParam.wBadBSBufferChopping = 0;
        tileControlParam.row = (USHORT)loc.row;
        tileControlParam.column = (USHORT)loc.col;
#if AV1D_DDI_VERSION >= 26
        //tileControlParam.TileIndex = 0; // valid for large_scale_tile only
#endif
        //tileControlParam.StartTileIdx = (USHORT)loc.startIdx;
        //tileControlParam.EndTileIdx = (USHORT)loc.endIdx;
#if AV1D_DDI_VERSION >= 31
        tileControlParam.anchor_frame = 255;
#else
        tileControlParam.anchor_frame_idx.wPicEntry = 0;
#endif
        //tileControlParam.BSTilePayloadSizeInBytes = (UINT)loc.size;
    }

} // namespace UMC_AV1_DECODER

#endif // UMC_VA_DXVA

#endif // MFX_ENABLE_AV1_VIDEO_DECODE



