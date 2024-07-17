// Copyright (c) 2023 Intel Corporation
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

#include "umc_defs.h"

#ifdef MFX_ENABLE_VVC_VIDEO_DECODE

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_va_base.h"


#include "umc_va_linux.h"
#include "umc_vvc_va_packer_vaapi.h"
#include "umc_va_video_processing.h"

namespace UMC_VVC_DECODER
{
    Packer *Packer::CreatePacker(UMC::VideoAccelerator *va)
    {
        return new PackerVA(va);
    }

    Packer::Packer(UMC::VideoAccelerator *va)
        : m_va(va)
    {
    }

    Packer::~Packer()
    {
    }
    /****************************************************************************************************/
    // vaapi packer implementation
    /****************************************************************************************************/
    PackerVA::PackerVA(UMC::VideoAccelerator *va)
        : Packer(va)
    {
    }
    UMC::Status PackerVA::SyncTask(int32_t index, void * error)
    {
        return m_va->SyncTask(index, error);
    }

    UMC::Status PackerVA::QueryTaskStatus(int32_t index, void * status, void * error)
    {
        return m_va->QueryTaskStatus(index, status, error);
    }
    UMC::Status PackerVA::GetStatusReport(void *pStatusReport, size_t size)
    {
        return UMC::UMC_OK;
    }

    void PackerVA::BeginFrame()
    {
    }

    void PackerVA::EndFrame()
    {
    }

    bool PackerVA::IsAPSAvailable(VVCSlice *pSlice, ApsType aps_type)
    {
        if (pSlice == nullptr)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);
        for (uint8_t id = 0; id < pSlice->GetAPSSize(); id++) 
        {
            VVCAPS const& aps = pSlice->GetAPS(id);
            if (aps.aps_params_type == aps_type)
            {
                return true;
            }
        }
        return false;
    }
    void PackerVA::PackALFParams(VAAlfDataVVC *pAlfParam, VVCSlice *pSlice)
    {
        if (pAlfParam == nullptr || pSlice == nullptr)
            throw vvc_exception(UMC::UMC_ERR_FAILED);

        for (uint8_t id = 0; id < pSlice->GetAPSSize(); id++)
        {
            VVCAPS const& aps = pSlice->GetAPS(id);
            if (aps.aps_params_type == ALF_APS && aps.m_fresh)
            {
                pAlfParam->aps_adaptation_parameter_set_id = (uint8_t)(aps.aps_adaptation_parameter_set_id);
                pAlfParam->alf_luma_num_filters_signalled_minus1 = (uint8_t)(aps.alf_luma_num_filters_signalled_minus1);
                for (uint32_t i = 0; i < 25; i++)
                {
                    pAlfParam->alf_luma_coeff_delta_idx[i] = (uint8_t)(aps.alf_luma_coeff_delta_idx[i]);
                }
                for (uint32_t i = 0; i < aps.alf_luma_num_filters_signalled_minus1 + 1; i++)
                {
                    for (uint32_t j = 0; j < 12; j++)
                    {
                        pAlfParam->filtCoeff[i][j] = (int8_t)(aps.alf_luma_coeff_abs[i][j] * (1 - 2 * aps.alf_luma_coeff_sign[i][j]));
                        pAlfParam->alf_luma_clip_idx[i][j] = (uint8_t)(aps.alf_luma_clip_idx[i][j]);
                    }
                }
                pAlfParam->alf_chroma_num_alt_filters_minus1 = (uint8_t)(aps.alf_chroma_num_alt_filters_minus1);
                for (uint32_t i = 0; i < aps.alf_chroma_num_alt_filters_minus1 + 1; i++)
                {
                    for (uint32_t j = 0; j < 6; j++)
                    {
                        pAlfParam->AlfCoeffC[i][j] = (int8_t)(aps.alf_chroma_coeff_abs[i][j] * (1 - 2 * aps.alf_chroma_coeff_sign[i][j]));
                        pAlfParam->alf_chroma_clip_idx[i][j] = (uint8_t)(aps.alf_chroma_clip_idx[i][j]);
                    }
                }
                pAlfParam->alf_cc_cb_filters_signalled_minus1 = (uint8_t)(aps.alf_cc_cb_filters_signalled_minus1);
                for (uint32_t i = 0; i < 4; i++)
                {
                    for (uint32_t j = 0; j < 7; j++)
                    {
                        pAlfParam->CcAlfApsCoeffCb[i][j] = (uint8_t)(aps.CcAlfApsCoeffCb[i][j]);
                    }
                }
                pAlfParam->alf_cc_cr_filters_signalled_minus1 = (uint8_t)(aps.alf_cc_cr_filters_signalled_minus1);
                for (uint32_t i = 0; i < 4; i++)
                {
                    for (uint32_t j = 0; j < 7; j++)
                    {
                        pAlfParam->CcAlfApsCoeffCr[i][j] = (uint8_t)(aps.CcAlfApsCoeffCr[i][j]);
                    }
                }
                pAlfParam->alf_flags.bits.alf_luma_filter_signal_flag = aps.alf_luma_filter_signal_flag;
                pAlfParam->alf_flags.bits.alf_chroma_filter_signal_flag = aps.alf_chroma_filter_signal_flag;
                pAlfParam->alf_flags.bits.alf_cc_cb_filter_signal_flag = aps.alf_cc_cb_filter_signal_flag;
                pAlfParam->alf_flags.bits.alf_cc_cr_filter_signal_flag = aps.alf_cc_cr_filter_signal_flag;
                pAlfParam->alf_flags.bits.alf_luma_clip_flag = aps.alf_luma_clip_flag;
                pAlfParam->alf_flags.bits.alf_chroma_clip_flag = aps.alf_chroma_clip_flag;
                pAlfParam++;
            }
        }
    }

    void PackerVA::PackLMCSParams(VALmcsDataVVC *pLMCSParam, VVCSlice *pSlice)
    {
        if (pLMCSParam == nullptr || pSlice == nullptr)
            throw vvc_exception(UMC::UMC_ERR_FAILED);
        for (uint8_t id = 0; id < pSlice->GetAPSSize(); id++)
        {
            VVCAPS const& aps = pSlice->GetAPS(id);   
            if (aps.aps_params_type == LMCS_APS && aps.m_fresh)
            {
                pLMCSParam->aps_adaptation_parameter_set_id = (uint8_t)(aps.aps_adaptation_parameter_set_id);
                pLMCSParam->lmcs_min_bin_idx = (uint8_t)(aps.lmcs_min_bin_idx);
                pLMCSParam->lmcs_delta_max_bin_idx = (uint8_t)(aps.lmcs_delta_max_bin_idx);
                for (uint32_t i = 0; i < 16; i++)
                {
                    pLMCSParam->lmcsDeltaCW[i] = (int16_t)((1 - 2 * aps.lmcs_delta_sign_cw_flag[i]) * aps.lmcs_delta_abs_cw[i]);
                }
                pLMCSParam->lmcsDeltaCrs = (int8_t)((1 - 2 * aps.lmcs_delta_sign_crs_flag) * aps.lmcs_delta_abs_crs);
                pLMCSParam++;
            }
        }
    }
    void PackerVA::PackScalingParams(VAScalingListVVC *pScalingParam, VVCSlice *pSlice)
    {
        if (pScalingParam == nullptr || pSlice == nullptr)
            throw vvc_exception(UMC::UMC_ERR_FAILED);
        for (uint8_t id = 0; id < pSlice->GetAPSSize(); id++)
        {
            VVCAPS const& aps = pSlice->GetAPS(id);
            if (aps.aps_params_type == SCALING_LIST_APS && aps.m_fresh)
            {
                const ScalingList& pScalingList = aps.scalingListInfo;
                pScalingParam->aps_adaptation_parameter_set_id = (uint8_t)(aps.aps_adaptation_parameter_set_id);
                for (uint8_t i = 0; i < SCALING_LIST_1D_START_16x16; i++) 
                {
                    pScalingParam->ScalingMatrixDCRec[i] = (uint8_t)pScalingList.ScalingMatrixDCRec[i];
                }
                for (uint8_t i = 0; i < SCALING_LIST_1D_START_4x4; i++) 
                {
                    for (uint8_t j = 0; j < 2; j++)
                    {
                        for (uint8_t k = 0; k < 2; k++)
                        {
                            pScalingParam->ScalingMatrixRec2x2[i][j][k] = (uint8_t)pScalingList.ScalingMatrixRec2x2[i][j][k];
                        }
                    }
                }
                for (uint8_t i = 0; i < SCALING_LIST_1D_START_8x8 - SCALING_LIST_1D_START_4x4; i++)
                {
                    for (uint8_t j = 0; j < 4; j++)
                    {
                        for (uint8_t k = 0; k < 4; k++)
                        {
                            pScalingParam->ScalingMatrixRec4x4[i][j][k] = (uint8_t)pScalingList.ScalingMatrixRec4x4[i][j][k];
                        }
                    }
                }
                for (uint8_t i = 0; i < VVC_MAX_SCALING_LIST_ID - SCALING_LIST_1D_START_8x8; i++)
                {
                    for (uint8_t j = 0; j < 8; j++)
                    {
                        for (uint8_t k = 0; k < 8; k++)
                        {
                            pScalingParam->ScalingMatrixRec8x8[i][j][k] = (uint8_t)pScalingList.ScalingMatrixRec8x8[i][j][k];
                        }
                    }
                }
                pScalingParam ++;
            }
        }
    }
    void PackerVA::PackTileParams(VVCSlice *pSlice)
    {
        if (!pSlice)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);
        VVCPicParamSet const* pPicParamSet = pSlice->GetPPS();
        if(!pPicParamSet)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);
        uint32_t tile_count = pPicParamSet->pps_num_exp_tile_columns_minus1 + pPicParamSet->pps_num_exp_tile_rows_minus1 + 2;
        UMC::UMCVACompBuffer* compBufTile = nullptr;
        auto pTileParam = reinterpret_cast<uint16_t*>(m_va->GetCompBuffer(VATileBufferType, &compBufTile,sizeof(uint16_t) * tile_count));
        if (!pTileParam || !compBufTile || (static_cast<size_t>(compBufTile->GetBufferSize()) < sizeof(uint16_t)))
            throw vvc_exception(MFX_ERR_MEMORY_ALLOC);

        memset(pTileParam, 0, sizeof(uint16_t) * tile_count);
        for (uint32_t i = 0; i < tile_count; i++) 
        {
            if (i < pPicParamSet->pps_num_exp_tile_columns_minus1 + 1) 
            {
                *pTileParam = (uint16_t)pPicParamSet->pps_tile_column_width[i] - 1;  //check pps_tile_column_width_minus1
            }
            else 
            {
                *pTileParam = (uint16_t)pPicParamSet->pps_tile_row_height[i - pPicParamSet->pps_num_exp_tile_columns_minus1 - 1] - 1;
            }
            pTileParam++;
        }
    }
    void PackerVA::PackSubpicParams(VVCSlice *pSlice)
    {
        if (!pSlice)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);
        VVCSeqParamSet const* pSeqParamSet = pSlice->GetSPS();
        VVCPicParamSet const* pPicParamSet = pSlice->GetPPS();
        if(!pSeqParamSet || !pPicParamSet)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        if (pSeqParamSet->sps_subpic_info_present_flag && pSeqParamSet->sps_num_subpics_minus1)
        {
            uint32_t subpic_count = pSeqParamSet->sps_num_subpics_minus1 + 1;
            UMC::UMCVACompBuffer* compBufSubpic = nullptr;
            auto pSubpicParam = reinterpret_cast<VASubPicVVC*>(m_va->GetCompBuffer(VASubPicBufferType, &compBufSubpic,sizeof(VASubPicVVC) * subpic_count));
            if (!pSubpicParam || !compBufSubpic || (static_cast<size_t>(compBufSubpic->GetBufferSize()) < sizeof(VASubPicVVC)))
                throw vvc_exception(MFX_ERR_MEMORY_ALLOC);
            memset(pSubpicParam, 0, sizeof(VASubPicVVC) * subpic_count);

            for (uint16_t i = 0; i < subpic_count; i++)
            {
                pSubpicParam->sps_subpic_ctu_top_left_x = (uint16_t)pSeqParamSet->sps_subpic_ctu_top_left_x[i];
                pSubpicParam->sps_subpic_ctu_top_left_y = (uint16_t)pSeqParamSet->sps_subpic_ctu_top_left_y[i];
                pSubpicParam->sps_subpic_width_minus1   = (uint16_t)pSeqParamSet->sps_subpic_width_minus1[i];
                pSubpicParam->sps_subpic_height_minus1  = (uint16_t)pSeqParamSet->sps_subpic_height_minus1[i];
                if (pSeqParamSet->sps_subpic_id_mapping_explicitly_signalled_flag)
                {
                    pSubpicParam->SubpicIdVal = (uint16_t)(pPicParamSet->pps_subpic_id_mapping_present_flag ? pPicParamSet->pps_subpic_id[i] : pSeqParamSet->sps_subpic_id[i]);
                }
                else
                {
                    pSubpicParam->SubpicIdVal = i;
                }
                pSubpicParam->subpic_flags.bits.sps_subpic_treated_as_pic_flag             = (uint16_t)pSeqParamSet->sps_subpic_treated_as_pic_flag[i];
                pSubpicParam->subpic_flags.bits.sps_loop_filter_across_subpic_enabled_flag = (uint16_t)pSeqParamSet->sps_loop_filter_across_subpic_enabled_flag[i];
                pSubpicParam++;
            }
        }
    }
    void PackerVA::PackSliceStructParams(VASliceStructVVC *pSliceStruct, VVCSlice *pSlice)
    {
        if (pSliceStruct == nullptr || pSlice == nullptr)
            throw vvc_exception(UMC::UMC_ERR_FAILED);
        VVCPicParamSet const* pPicParamSet = pSlice->GetPPS();
        if(pPicParamSet == nullptr)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        for (uint32_t i = 0; i < (pPicParamSet->pps_num_slices_in_pic_minus1 + 1); i++) 
        {
            if (pSlice->m_rectSlice[i] && !pSlice->m_rectSlice[i]->pps_derived_exp_slice_height_flag)
            {
                pSliceStruct->SliceTopLeftTileIdx = (uint16_t)pSlice->m_rectSlice[i]->pps_tile_idx;
                pSliceStruct->pps_slice_width_in_tiles_minus1 = (uint16_t)pSlice->m_rectSlice[i]->pps_slice_width_in_tiles_minus1;
                pSliceStruct->pps_slice_height_in_tiles_minus1 = (uint16_t)pSlice->m_rectSlice[i]->pps_slice_height_in_tiles_minus1;
                pSliceStruct->pps_exp_slice_height_in_ctus_minus1 = (uint16_t)pSlice->m_rectSlice[i]->pps_slice_height_in_ctu_minus1;
                pSliceStruct++;
            }
        }
    }

    void PackerVA::PackSliceLong(VAPictureParameterBufferVVC *pPicParam, VASliceParameterBufferVVC *pSliceParam, VVCSlice *pSlice)
    {
       if (!pPicParam || !pSliceParam || !pSlice)
            throw vvc_exception(UMC::UMC_ERR_FAILED);
        VVCSliceHeader const* pSliceHdr = pSlice->GetSliceHeader();
        VVCPicParamSet const* pPicParamSet = pSlice->GetPPS();
        VVCSeqParamSet const* pSeqParamSet = pSlice->GetSPS();
        VVCPicHeader const* pPicHeader = pSlice->GetPictureHeader();
        if(!pSliceHdr || !pPicParamSet || !pSeqParamSet || !pPicHeader)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        pSliceParam->slice_data_byte_offset = (uint32_t)(pSlice->GetBitStream()->BytesDecoded() + prefix_size) + pSlice->m_NumEmuPrevnBytesInSliceHdr;

        pSliceParam->sh_subpic_id = (uint16_t)(pSliceHdr->sh_subpic_id);
        pSliceParam->sh_slice_address = (uint16_t)(pSliceHdr->sh_slice_address);
        pSliceParam->sh_num_tiles_in_slice_minus1 = (uint16_t)(pSliceHdr->sh_num_tiles_in_slice_minus1);
        pSliceParam->sh_slice_type = (uint8_t)(pSliceHdr->slice_type);

        if (pSeqParamSet->sps_alf_enabled_flag)
        {
            if (pPicParamSet->pps_alf_info_in_ph_flag)
            {
                pSliceParam->sh_num_alf_aps_ids_luma = (uint8_t)(pPicHeader->ph_num_alf_aps_ids_luma);
                std::copy_n(std::begin(pPicHeader->ph_alf_aps_id_luma), std::min((size_t)7, pPicHeader->ph_alf_aps_id_luma.size()),
                    std::begin(pSliceParam->sh_alf_aps_id_luma));
                pSliceParam->sh_alf_aps_id_chroma = (uint8_t)(pPicHeader->ph_alf_aps_id_chroma);
                pSliceParam->sh_alf_cc_cb_aps_id = (uint8_t)(pPicHeader->ph_alf_cc_cb_aps_id);
                pSliceParam->sh_alf_cc_cr_aps_id = (uint8_t)(pPicHeader->ph_alf_cc_cr_aps_id);
            }
            else
            {
                pSliceParam->sh_num_alf_aps_ids_luma = (uint8_t)(pSliceHdr->num_alf_aps_ids_luma);
                std::copy_n(std::begin(pSliceHdr->alf_aps_ids_luma), std::min((size_t)7, pSliceHdr->alf_aps_ids_luma.size()),
                    std::begin(pSliceParam->sh_alf_aps_id_luma));
                pSliceParam->sh_alf_aps_id_chroma = (uint8_t)(pSliceHdr->alf_aps_id_chroma);
                pSliceParam->sh_alf_cc_cb_aps_id = (uint8_t)(pSliceHdr->alf_cc_cb_aps_id);
                pSliceParam->sh_alf_cc_cr_aps_id = (uint8_t)(pSliceHdr->alf_cc_cr_aps_id);
            }
        }
        for (uint8_t i = 0; i < 2; i++)
        {
            if (pSliceHdr->sh_rpl[i].num_ref_entries == 0)
                pSliceParam->NumRefIdxActive[i] = 0;
            else
                pSliceParam->NumRefIdxActive[i] = (uint8_t)(pSliceHdr->sh_num_ref_idx_active_minus1[i] + 1);
        }
        pSliceParam->sh_collocated_ref_idx = (uint8_t)(pSliceHdr->sh_collocated_ref_idx);
        if (pPicParamSet->pps_qp_delta_info_in_ph_flag)
        {
            pSliceParam->SliceQpY = (int8_t)(26 + pPicParamSet->pps_init_qp_minus26 + pPicHeader->ph_qp_delta);
        }
        else
        {
            pSliceParam->SliceQpY = (int8_t)(26 + pPicParamSet->pps_init_qp_minus26 + pSliceHdr->sh_qp_delta);
        }
        pSliceParam->sh_cb_qp_offset = (int8_t)(pSliceHdr->sh_cb_qp_offset);
        pSliceParam->sh_cr_qp_offset = (int8_t)(pSliceHdr->sh_cr_qp_offset);
        pSliceParam->sh_joint_cbcr_qp_offset = (int8_t)(pSliceHdr->sh_joint_cbcr_qp_offset);
        pSliceParam->sh_luma_beta_offset_div2 = (int8_t)(pSliceHdr->sh_luma_beta_offset_div2);
        pSliceParam->sh_luma_tc_offset_div2 = (int8_t)(pSliceHdr->sh_luma_tc_offset_div2);
        pSliceParam->sh_cb_beta_offset_div2 = (int8_t)(pSliceHdr->sh_cb_beta_offset_div2);
        pSliceParam->sh_cb_tc_offset_div2 = (int8_t)(pSliceHdr->sh_cb_tc_offset_div2);
        pSliceParam->sh_cr_beta_offset_div2 = (int8_t)(pSliceHdr->sh_cr_beta_offset_div2);
        pSliceParam->sh_cr_tc_offset_div2 = (int8_t)(pSliceHdr->sh_cr_tc_offset_div2);

        for (uint8_t i = 0; i < 2; i++)
        {
            for (uint8_t j = 0; j < 15; j++)
            {
                pSliceParam->RefPicList[i][j] = 0x7f;
            }
        }
        const VVCReferencePictureList* sh_rpls[NUM_REF_PIC_LIST_01];
        for (uint32_t listIdx = 0; listIdx < NUM_REF_PIC_LIST_01; listIdx++)
        {

            sh_rpls[listIdx] = &pSliceHdr->sh_rpl[listIdx];
            for (uint32_t i = 0; i < sh_rpls[listIdx]->num_ref_entries; i++)
            {
                bool refFrameFound = false;
                for (int32_t refFrameIdx = 0; refFrameIdx < VVC_MAX_NUM_REF - 1; refFrameIdx++)
                {
                    if (pPicParam->ReferenceFrames[refFrameIdx].pic_order_cnt == sh_rpls[listIdx]->POC[i])
                    {
                        pSliceParam->RefPicList[listIdx][i] = refFrameIdx;
                        refFrameFound = true;
                        break;
                    }
                }
                if(refFrameFound == false)
                {
                    for (int32_t idx = 0; idx < pSliceHdr->num_ref_Idx[listIdx]; idx++)
                    {
                        if (pPicParam->ReferenceFrames[idx].picture_id == 0x7f)
                            break;
                        uint32_t max_surf_num = pSlice->GetMaxSurfNum();
                        uint32_t refFramePOCidx = pPicParam->ReferenceFrames[idx].picture_id - max_surf_num;   // should add sync depth
                        if (pPicParam->ReferenceFrames[refFramePOCidx].picture_id == static_cast<uint32_t>(sh_rpls[listIdx]->POC[i]))
                        {
                            pSliceParam->RefPicList[listIdx][i] = idx;
                            break;
                        }
                    }
                }

            }
        }

        const VVCParsedWP& weightPredTable = pSliceHdr->weightPredTable;
        pSliceParam->WPInfo.luma_log2_weight_denom = (uint8_t)(weightPredTable.luma_log2_weight_denom);
        pSliceParam->WPInfo.delta_chroma_log2_weight_denom = (int8_t)(weightPredTable.delta_chroma_log2_weight_denom);
        pSliceParam->WPInfo.num_l0_weights = (uint8_t)(weightPredTable.num_l0_weights);
        pSliceParam->WPInfo.num_l1_weights = (uint8_t)(weightPredTable.num_l1_weights);
        for(int i = 0; i < 15; i ++)
        {
            pSliceParam->WPInfo.luma_weight_l0_flag[i] = (uint8_t)weightPredTable.luma_weight_l0_flag[i];
            pSliceParam->WPInfo.chroma_weight_l0_flag[i] = (uint8_t)weightPredTable.chroma_weight_l0_flag[i];
        }
        for (uint32_t i = 0; i < 15 && i < weightPredTable.num_l0_weights; i++)
        {
            pSliceParam->WPInfo.delta_luma_weight_l0[i] = (int8_t)weightPredTable.delta_luma_weight_l0[i];
            pSliceParam->WPInfo.luma_offset_l0[i] = (int8_t)weightPredTable.luma_offset_l0[i];
            for (uint32_t j = 0; j < 2; j++)
            {
                pSliceParam->WPInfo.delta_chroma_weight_l0[i][j] = (int8_t)weightPredTable.delta_chroma_weight_l0[i][j];
                pSliceParam->WPInfo.delta_chroma_offset_l0[i][j] = (int16_t)weightPredTable.delta_chroma_offset_l0[i][j];
            }
        }
        for(int i = 0; i < 15; i ++)
        {
            pSliceParam->WPInfo.luma_weight_l1_flag[i] = (uint8_t)weightPredTable.luma_weight_l1_flag[i];
            pSliceParam->WPInfo.chroma_weight_l1_flag[i] = (uint8_t)weightPredTable.chroma_weight_l1_flag[i];
        }        

        for (uint32_t i = 0; i < 15 && i < weightPredTable.num_l1_weights; i++)
        {
            pSliceParam->WPInfo.delta_luma_weight_l1[i] = (int8_t)weightPredTable.delta_luma_weight_l1[i];
            pSliceParam->WPInfo.luma_offset_l1[i] = (int8_t)weightPredTable.luma_offset_l1[i];
            for (uint32_t j = 0; j < 2; j++)
            {
                pSliceParam->WPInfo.delta_chroma_weight_l1[i][j] = (int8_t)weightPredTable.delta_chroma_weight_l1[i][j];
                pSliceParam->WPInfo.delta_chroma_offset_l1[i][j] = (int16_t)weightPredTable.delta_chroma_offset_l1[i][j];
            }
        }
        pSliceParam->sh_flags.bits.sh_alf_enabled_flag = (uint32_t)(pSliceHdr->alf_enabled_flag[COMPONENT_Y]);
        pSliceParam->sh_flags.bits.sh_alf_cb_enabled_flag = (uint32_t)(pSliceHdr->alf_enabled_flag[COMPONENT_Cb]);
        pSliceParam->sh_flags.bits.sh_alf_cr_enabled_flag = (uint32_t)(pSliceHdr->alf_enabled_flag[COMPONENT_Cr]);
        pSliceParam->sh_flags.bits.sh_alf_cc_cb_enabled_flag = (uint32_t)(pSliceHdr->alf_cc_cb_enabled_flag);
        pSliceParam->sh_flags.bits.sh_alf_cc_cr_enabled_flag = (uint32_t)(pSliceHdr->alf_cc_cr_enabled_flag);
        pSliceParam->sh_flags.bits.sh_lmcs_used_flag = (uint32_t)(pSliceHdr->sh_lmcs_used_flag);
        pSliceParam->sh_flags.bits.sh_explicit_scaling_list_used_flag = (uint32_t)(pSliceHdr->sh_explicit_scaling_list_used_flag);
        pSliceParam->sh_flags.bits.sh_cabac_init_flag = (uint32_t)(pSliceHdr->cabac_init_flag);
        pSliceParam->sh_flags.bits.sh_collocated_from_l0_flag = (uint32_t)(pSliceHdr->sh_collocated_from_l0_flag);
        pSliceParam->sh_flags.bits.sh_cu_chroma_qp_offset_enabled_flag = (uint32_t)(pSliceHdr->sh_cu_chroma_qp_offset_enabled_flag);
        if (pPicParamSet->pps_sao_info_in_ph_flag)
        {
            pSliceParam->sh_flags.bits.sh_sao_luma_used_flag = (uint32_t)(pPicHeader->ph_sao_luma_enabled_flag);
            pSliceParam->sh_flags.bits.sh_sao_chroma_used_flag = (uint32_t)(pPicHeader->ph_sao_chroma_enabled_flag);
        }
        else
        {
            pSliceParam->sh_flags.bits.sh_sao_luma_used_flag = (uint32_t)(pSliceHdr->sh_sao_luma_used_flag);
            pSliceParam->sh_flags.bits.sh_sao_chroma_used_flag = (uint32_t)(pSliceHdr->sh_sao_chroma_used_flag);
        }
        pSliceParam->sh_flags.bits.sh_deblocking_filter_disabled_flag = (uint32_t)(pSliceHdr->deblocking_filter_disable_flag);
        pSliceParam->sh_flags.bits.sh_dep_quant_used_flag = (uint32_t)(pSliceHdr->sh_dep_quant_used_flag);
        pSliceParam->sh_flags.bits.sh_sign_data_hiding_used_flag = (uint32_t)(pSliceHdr->sh_sign_data_hiding_used_flag);
        pSliceParam->sh_flags.bits.sh_ts_residual_coding_disabled_flag = (uint32_t)(pSliceHdr->sh_ts_residual_coding_disabled_flag);

    }
    void PackerVA::PackSliceParam(VAPictureParameterBufferVVC *pPicParam, DPBType dpb, VVCDecoderFrameInfo const* pSliceInfo)
    {
        if (!pPicParam || !pSliceInfo)
            throw vvc_exception(UMC::UMC_ERR_FAILED);
        auto slice_num = pSliceInfo->GetSliceCount();

        UMC::UMCVACompBuffer* compBufferSlice = nullptr;
        VASliceParameterBufferVVC* vvcSliceParamsLongBase = reinterpret_cast<VASliceParameterBufferVVC*>(m_va->GetCompBuffer(VASliceParameterBufferType, &compBufferSlice,sizeof(VASliceParameterBufferVVC) * slice_num));
        if (!vvcSliceParamsLongBase || !compBufferSlice || (static_cast<size_t>(compBufferSlice->GetBufferSize()) < sizeof(VASliceParameterBufferVVC) * slice_num))
            throw vvc_exception(MFX_ERR_MEMORY_ALLOC);

        size_t sliceDataBufferSize = 0;
        int32_t alignedSize = 0;
        for(size_t i = 0; i < slice_num; i++)
        {
            VVCSlice *pSlice = pSliceInfo->GetSlice(i);
            if(!pSlice)
                throw vvc_exception(UMC::UMC_ERR_NULL_PTR);
            auto bs = pSlice->GetBitStream();
            assert(bs);

            uint32_t size = 0;
            uint32_t* ptr = 0;
            bs->GetOrg(&ptr, &size);

            sliceDataBufferSize += size + 3;      
            //if the last slice, need to align 
            if(i == (slice_num -1))
                alignedSize = mfx::align2_value(sliceDataBufferSize, 128);
        }

        UMC::UMCVACompBuffer* compBufferSliceData = nullptr;
        uint8_t* sliceDataBuffer = (uint8_t*)(m_va->GetCompBuffer(VASliceDataBufferType, &compBufferSliceData, alignedSize));
        if (!sliceDataBuffer || !compBufferSliceData)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        static uint8_t constexpr start_code[] = { 0, 0, 1 };
        uint8_t* pSliceDataBase = sliceDataBuffer;
        uint32_t size = 0;
        uint32_t* src_ptr = 0;
        uint32_t size_before = 0;
        for(size_t i = 0; i < slice_num; i++)
        {
            vvcSliceParamsLongBase->slice_data_offset = size_before;
            VVCSlice *pSlice = pSliceInfo->GetSlice(i);
            if(!pSlice)
                throw vvc_exception(UMC::UMC_ERR_NULL_PTR);   
            auto bs = pSlice->GetBitStream();
            assert(bs);
            bs->GetOrg(&src_ptr, &size);
            auto src = reinterpret_cast<uint8_t const*>(src_ptr);
            //copy a start code 
            std::copy(start_code, start_code + sizeof(start_code), pSliceDataBase);
            pSliceDataBase += sizeof(start_code);
            //copy bs data
            std::copy(src, src + size, pSliceDataBase);
            pSliceDataBase += size;    

            size += sizeof(start_code);
            size_before  += size;
            if(i == (slice_num - 1))
            {
                memset((uint8_t*)pSliceDataBase, 0, alignedSize - sliceDataBufferSize);
                pSliceDataBase += (alignedSize - sliceDataBufferSize);
            }
            //pack sliec params
            vvcSliceParamsLongBase->slice_data_size = size;
            PackSliceLong(pPicParam, vvcSliceParamsLongBase, pSlice);
            vvcSliceParamsLongBase ++;  
        }
    }

    void PackerVA::PackPicParamsChromaQPTable(VAPictureParameterBufferVVC *pPicParam, const VVCSeqParamSet *pSeqParamSet)
    {
        if(!pPicParam || !pSeqParamSet)
            throw vvc_exception(UMC::UMC_ERR_FAILED);
        for (uint32_t i = 0; i < pSeqParamSet->sps_num_qp_tables; i++)
        {
            int32_t qpBdOffsetC = pSeqParamSet->sps_qp_bd_offset[0];
            int32_t numPtsInCQPTableMinus1 = pSeqParamSet->sps_num_points_in_qp_table_minus1[i];
            std::vector<int8_t> qpInVal(numPtsInCQPTableMinus1 + 2);
            std::vector<int8_t> qpOutVal(numPtsInCQPTableMinus1 + 2);

            qpInVal[0] = (int8_t)pSeqParamSet->sps_qp_table_start_minus26[i] + 26;
            qpOutVal[0] = qpInVal[0];
            for (int32_t j = 0; j <= numPtsInCQPTableMinus1; j++)
            {
                qpInVal[j + 1] = qpInVal[j] + (int8_t)pSeqParamSet->sps_delta_qp_in_val_minus1[i][j] + 1;
                qpOutVal[j + 1] = qpOutVal[j] + (int8_t)(pSeqParamSet->sps_delta_qp_in_val_minus1[i][j] ^ pSeqParamSet->sps_delta_qp_diff_val[i][j]);
            }
            int32_t qpOff = qpBdOffsetC > 0 ? qpBdOffsetC : 0;
            pPicParam->ChromaQpTable[i][qpInVal[0] + qpOff] = qpOutVal[0];
            for (int32_t k = qpInVal[0] - 1; k >= -qpBdOffsetC; k--)
            {
                pPicParam->ChromaQpTable[i][k + qpOff] = (int8_t)Clip3(-qpBdOffsetC, VVC_MAX_QP, pPicParam->ChromaQpTable[i][k + qpOff + 1] - 1);
            }
            for (int32_t j = 0; j <= numPtsInCQPTableMinus1; j++)
            {
                uint32_t sh = (pSeqParamSet->sps_delta_qp_in_val_minus1[i][j] + 1) >> 1;
                for (int32_t k = qpInVal[j] + 1, m = 1; k <= qpInVal[j + 1]; k++, m++)
                {
                    pPicParam->ChromaQpTable[i][k + qpOff] = pPicParam->ChromaQpTable[i][qpInVal[j] + qpOff] +
                        (int8_t)(((qpOutVal[j + 1] - qpOutVal[j]) * m + sh) /
                        (pSeqParamSet->sps_delta_qp_in_val_minus1[i][j] + 1));
                }
            }
            for (int32_t k = qpInVal[numPtsInCQPTableMinus1 + 1] + 1; k <= VVC_MAX_QP; k++)
            {
                pPicParam->ChromaQpTable[i][k + qpOff] = (int8_t)Clip3(-qpBdOffsetC, VVC_MAX_QP, pPicParam->ChromaQpTable[i][k - 1 + qpOff] + 1);
            }
        }        
    }
    
    void PackerVA::PackPicParams(VAPictureParameterBufferVVC *pPicParam, VVCSlice *pSlice, VVCDecoderFrame *pFrame, DPBType dpb)
    {
        if (!pPicParam || !pSlice || !pFrame)
        {
            throw vvc_exception(UMC::UMC_ERR_FAILED);
        }
        VVCSeqParamSet const *pSeqParamSet = pSlice->GetSPS();
        VVCPicParamSet const *pPicParamSet = pSlice->GetPPS();
        VVCPicHeader const *pPicHeader = pSlice->GetPictureHeader();
        VVCSliceHeader const *pSlcHeader = pSlice->GetSliceHeader();
        if(!pSeqParamSet || !pPicParamSet || !pPicHeader || !pSlcHeader)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);
        // set current frame
        pFrame->m_ppsID = pPicParamSet->pps_pic_parameter_set_id;
        pPicParam->CurrPic.picture_id = m_va->GetSurfaceID(pFrame->GetMemID());
        pPicParam->CurrPic.pic_order_cnt = pFrame->m_PicOrderCnt;
        pPicParam->CurrPic.flags = 0;
        for (uint8_t i = 0; i < VVC_MAX_NUM_REF -1; i++)
        {
            pPicParam->ReferenceFrames[i].picture_id = 0x7f;
            pPicParam->ReferenceFrames[i].flags = 0;
        }

        // set reference frames
        uint32_t refidx = 0;
        bool unavailable_frame = true;
        VVCDecoderFrame* currPic = nullptr;
        for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
        {
            currPic = *it;
            int const refType = currPic->isLongTermRef() ? VA_PICTURE_VVC_LONG_TERM_REFERENCE : (currPic->isUnavailableRefFrame() ? VA_PICTURE_VVC_UNAVAILABLE_REFERENCE : 0);

            if (currPic == pFrame)
                continue;
            if (currPic->isReferenced())
            {
                pPicParam->ReferenceFrames[refidx].pic_order_cnt = currPic->PicOrderCnt();
                pPicParam->ReferenceFrames[refidx].picture_id = currPic->isUnavailableRefFrame() ? currPic->m_index : m_va->GetSurfaceID(currPic->GetMemID());
                pPicParam->ReferenceFrames[refidx].flags = refType;
                refidx++;
            }
        }
        
        pPicParam->pps_pic_width_in_luma_samples  = (uint16_t)pPicParamSet->pps_pic_width_in_luma_samples;
        pPicParam->pps_pic_height_in_luma_samples = (uint16_t)pPicParamSet->pps_pic_height_in_luma_samples;
        // slice level params
        pPicParam->sps_num_subpics_minus1 = (uint16_t)pSeqParamSet->sps_num_subpics_minus1;
        pPicParam->sps_chroma_format_idc = (uint8_t)pSeqParamSet->sps_chroma_format_idc;
        pPicParam->sps_bitdepth_minus8 = (uint8_t)pSeqParamSet->sps_bitdepth_minus8;
        pPicParam->sps_log2_ctu_size_minus5 = (uint8_t)pSeqParamSet->sps_log2_ctu_size_minus5;
        pPicParam->sps_log2_min_luma_coding_block_size_minus2 = (uint8_t)pSeqParamSet->sps_log2_min_luma_coding_block_size_minus2;
        pPicParam->sps_log2_transform_skip_max_size_minus2 = (uint8_t)pSeqParamSet->sps_log2_transform_skip_max_size_minus2;

        // chromaQpTable
        PackPicParamsChromaQPTable(pPicParam, pSeqParamSet);

        pPicParam->sps_six_minus_max_num_merge_cand              = (uint8_t)pSeqParamSet->sps_six_minus_max_num_merge_cand;
        pPicParam->sps_five_minus_max_num_subblock_merge_cand    = (uint8_t)pSeqParamSet->sps_five_minus_max_num_subblock_merge_cand;
        pPicParam->sps_max_num_merge_cand_minus_max_num_gpm_cand = (uint8_t)pSeqParamSet->sps_max_num_merge_cand_minus_max_num_gpm_cand;
        pPicParam->sps_log2_parallel_merge_level_minus2          = (uint8_t)pSeqParamSet->sps_log2_parallel_merge_level_minus2;
        pPicParam->sps_min_qp_prime_ts                           = (uint8_t)pSeqParamSet->sps_min_qp_prime_ts;
        pPicParam->sps_six_minus_max_num_ibc_merge_cand          = (uint8_t)pSeqParamSet->sps_six_minus_max_num_ibc_merge_cand;
        pPicParam->sps_num_ladf_intervals_minus2                 = (uint8_t)pSeqParamSet->sps_num_ladf_intervals_minus2;
        pPicParam->sps_ladf_lowest_interval_qp_offset            = (int8_t)pSeqParamSet->sps_ladf_lowest_interval_qp_offset;

        for (uint32_t i = 0; i < 4; i++)
        {
            pPicParam->sps_ladf_qp_offset[i]              = (int8_t)pSeqParamSet->sps_ladf_qp_offset[i];
            pPicParam->sps_ladf_delta_threshold_minus1[i] = (uint16_t)pSeqParamSet->sps_ladf_delta_threshold_minus1[i];
        }
    
        pPicParam->sps_flags.bits.sps_subpic_info_present_flag = (uint64_t)pSeqParamSet->sps_subpic_info_present_flag;
        pPicParam->sps_flags.bits.sps_independent_subpics_flag = (uint64_t)pSeqParamSet->sps_independent_subpics_flag;
        pPicParam->sps_flags.bits.sps_subpic_same_size_flag = (uint64_t)pSeqParamSet->sps_subpic_same_size_flag;
        pPicParam->sps_flags.bits.sps_entropy_coding_sync_enabled_flag = (uint64_t)pSeqParamSet->sps_entropy_coding_sync_enabled_flag;
        pPicParam->sps_flags.bits.sps_qtbtt_dual_tree_intra_flag = (uint64_t)pSeqParamSet->sps_qtbtt_dual_tree_intra_flag;
        pPicParam->sps_flags.bits.sps_max_luma_transform_size_64_flag = (uint64_t)pSeqParamSet->sps_max_luma_transform_size_64_flag;
        pPicParam->sps_flags.bits.sps_transform_skip_enabled_flag = (uint64_t)pSeqParamSet->sps_transform_skip_enabled_flag;
        pPicParam->sps_flags.bits.sps_bdpcm_enabled_flag = (uint64_t)pSeqParamSet->sps_bdpcm_enabled_flag;
        pPicParam->sps_flags.bits.sps_mts_enabled_flag = (uint64_t)pSeqParamSet->sps_mts_enabled_flag;
        pPicParam->sps_flags.bits.sps_explicit_mts_intra_enabled_flag = (uint64_t)pSeqParamSet->sps_explicit_mts_intra_enabled_flag;
        pPicParam->sps_flags.bits.sps_explicit_mts_inter_enabled_flag = (uint64_t)pSeqParamSet->sps_explicit_mts_inter_enabled_flag;
        pPicParam->sps_flags.bits.sps_lfnst_enabled_flag = (uint64_t)pSeqParamSet->sps_lfnst_enabled_flag;
        pPicParam->sps_flags.bits.sps_joint_cbcr_enabled_flag = (uint64_t)pSeqParamSet->sps_joint_cbcr_enabled_flag;
        pPicParam->sps_flags.bits.sps_same_qp_table_for_chroma_flag = (uint64_t)pSeqParamSet->sps_same_qp_table_for_chroma_flag;
        pPicParam->sps_flags.bits.sps_sao_enabled_flag = (uint64_t)pSeqParamSet->sps_sao_enabled_flag;
        pPicParam->sps_flags.bits.sps_alf_enabled_flag = (uint64_t)pSeqParamSet->sps_alf_enabled_flag;
        pPicParam->sps_flags.bits.sps_ccalf_enabled_flag = (uint64_t)pSeqParamSet->sps_ccalf_enabled_flag;
        pPicParam->sps_flags.bits.sps_lmcs_enabled_flag = (uint64_t)pSeqParamSet->sps_lmcs_enabled_flag;

        // spsFlagss1
        pPicParam->sps_flags.bits.sps_sbtmvp_enabled_flag = (uint64_t)pSeqParamSet->sps_sbtmvp_enabled_flag;
        pPicParam->sps_flags.bits.sps_amvr_enabled_flag = (uint64_t)pSeqParamSet->sps_amvr_enabled_flag;
        pPicParam->sps_flags.bits.sps_smvd_enabled_flag = (uint64_t)pSeqParamSet->sps_smvd_enabled_flag;
        pPicParam->sps_flags.bits.sps_mmvd_enabled_flag = (uint64_t)pSeqParamSet->sps_mmvd_enabled_flag;
        pPicParam->sps_flags.bits.sps_sbt_enabled_flag = (uint64_t)pSeqParamSet->sps_sbt_enabled_flag;
        pPicParam->sps_flags.bits.sps_affine_enabled_flag = (uint64_t)pSeqParamSet->sps_affine_enabled_flag;
        pPicParam->sps_flags.bits.sps_6param_affine_enabled_flag = (uint64_t)pSeqParamSet->sps_6param_affine_enabled_flag;
        pPicParam->sps_flags.bits.sps_affine_amvr_enabled_flag = (uint64_t)pSeqParamSet->sps_affine_amvr_enabled_flag;
        pPicParam->sps_flags.bits.sps_affine_prof_enabled_flag = (uint64_t)pSeqParamSet->sps_affine_prof_enabled_flag;
        pPicParam->sps_flags.bits.sps_bcw_enabled_flag = (uint64_t)pSeqParamSet->sps_bcw_enabled_flag;
        pPicParam->sps_flags.bits.sps_ciip_enabled_flag = (uint64_t)pSeqParamSet->sps_ciip_enabled_flag;
        pPicParam->sps_flags.bits.sps_gpm_enabled_flag = (uint64_t)pSeqParamSet->sps_gpm_enabled_flag;
        pPicParam->sps_flags.bits.sps_isp_enabled_flag = (uint64_t)pSeqParamSet->sps_isp_enabled_flag;
        pPicParam->sps_flags.bits.sps_mrl_enabled_flag = (uint64_t)pSeqParamSet->sps_mrl_enabled_flag;
        pPicParam->sps_flags.bits.sps_mip_enabled_flag = (uint64_t)pSeqParamSet->sps_mip_enabled_flag;
        pPicParam->sps_flags.bits.sps_cclm_enabled_flag = (uint64_t)pSeqParamSet->sps_cclm_enabled_flag;
        pPicParam->sps_flags.bits.sps_chroma_horizontal_collocated_flag = (uint64_t)pSeqParamSet->sps_chroma_horizontal_collocated_flag;
        pPicParam->sps_flags.bits.sps_chroma_vertical_collocated_flag = (uint64_t)pSeqParamSet->sps_chroma_vertical_collocated_flag;

        // spsFlagss2
        pPicParam->sps_flags.bits.sps_palette_enabled_flag = (uint64_t)pSeqParamSet->sps_palette_enabled_flag;
        pPicParam->sps_flags.bits.sps_act_enabled_flag = (uint64_t)pSeqParamSet->sps_act_enabled_flag;
        pPicParam->sps_flags.bits.sps_ibc_enabled_flag = (uint64_t)pSeqParamSet->sps_ibc_enabled_flag;
        pPicParam->sps_flags.bits.sps_ladf_enabled_flag = (uint64_t)pSeqParamSet->sps_ladf_enabled_flag;
        pPicParam->sps_flags.bits.sps_explicit_scaling_list_enabled_flag = (uint64_t)pSeqParamSet->sps_explicit_scaling_list_enabled_flag;
        pPicParam->sps_flags.bits.sps_scaling_matrix_for_lfnst_disabled_flag = (uint64_t)pSeqParamSet->sps_scaling_matrix_for_lfnst_disabled_flag;
        pPicParam->sps_flags.bits.sps_scaling_matrix_for_alternative_colour_space_disabled_flag = (uint64_t)pSeqParamSet->sps_scaling_matrix_for_alternative_colour_space_disabled_flag;
        pPicParam->sps_flags.bits.sps_scaling_matrix_designated_colour_space_flag = (uint64_t)pSeqParamSet->sps_scaling_matrix_designated_colour_space_flag;
        pPicParam->sps_flags.bits.sps_virtual_boundaries_enabled_flag = (uint64_t)pSeqParamSet->sps_virtual_boundaries_enabled_flag;
        pPicParam->sps_flags.bits.sps_virtual_boundaries_present_flag = (uint64_t)pSeqParamSet->sps_virtual_boundaries_present_flag;
        
        // picure-level params
        pPicParam->NumVerVirtualBoundaries        = 0;
        pPicParam->NumHorVirtualBoundaries        = 0;
        if (pSeqParamSet->sps_virtual_boundaries_enabled_flag)
        {
            if (pSeqParamSet->sps_virtual_boundaries_present_flag)
            {
                pPicParam->NumVerVirtualBoundaries = (uint8_t)pSeqParamSet->sps_num_ver_virtual_boundaries;
                pPicParam->NumHorVirtualBoundaries = (uint8_t)pSeqParamSet->sps_num_hor_virtual_boundaries;
            }
            else
            {
                pPicParam->NumVerVirtualBoundaries = (uint8_t)pPicHeader->ph_num_ver_virtual_boundaries;
                pPicParam->NumHorVirtualBoundaries = (uint8_t)pPicHeader->ph_num_hor_virtual_boundaries;
            }
        }

        //change to for loop later.
        for (uint32_t i = 0; i < pPicParam->NumVerVirtualBoundaries; i++)
        {
            if (pSeqParamSet->sps_virtual_boundaries_present_flag)
            {
                pPicParam->VirtualBoundaryPosX[i] = (uint16_t)((pSeqParamSet->sps_virtual_boundary_pos_x_minus1[i] + 1) * 8);
            }
            else
            {
                pPicParam->VirtualBoundaryPosX[i] = (uint16_t)((pPicHeader->ph_virtual_boundary_pos_x_minus1[i] + 1) * 8);
            }
        }
        for (uint32_t i = 0; i < pPicParam->NumHorVirtualBoundaries; i++)
        {
            if (pSeqParamSet->sps_virtual_boundaries_present_flag)
            {
                pPicParam->VirtualBoundaryPosY[i] = (uint16_t)((pSeqParamSet->sps_virtual_boundary_pos_y_minus1[i] + 1) * 8);
            }
            else
            {
                pPicParam->VirtualBoundaryPosY[i] = (uint16_t)((pPicHeader->ph_virtual_boundary_pos_y_minus1[i] + 1) * 8);
            }
        }
        pPicParam->pps_scaling_win_left_offset              = (int32_t)pPicParamSet->pps_scaling_win_left_offset;
        pPicParam->pps_scaling_win_right_offset             = (int32_t)pPicParamSet->pps_scaling_win_right_offset;
        pPicParam->pps_scaling_win_top_offset               = (int32_t)pPicParamSet->pps_scaling_win_top_offset;
        pPicParam->pps_scaling_win_bottom_offset            = (int32_t)pPicParamSet->pps_scaling_win_bottom_offset;
        pPicParam->pps_num_exp_tile_columns_minus1          = (int8_t)pPicParamSet->pps_num_exp_tile_columns_minus1;
        pPicParam->pps_num_exp_tile_rows_minus1             = (uint16_t)pPicParamSet->pps_num_exp_tile_rows_minus1;
        pPicParam->pps_num_slices_in_pic_minus1             = (uint16_t)pPicParamSet->pps_num_slices_in_pic_minus1;
        pPicParam->pps_pic_width_minus_wraparound_offset    = (uint16_t)pPicParamSet->pps_pic_width_minus_wraparound_offset;
        pPicParam->pps_cb_qp_offset                         = (int8_t)pPicParamSet->pps_cb_qp_offset;
        pPicParam->pps_cr_qp_offset                         = (int8_t)pPicParamSet->pps_cr_qp_offset;
        pPicParam->pps_joint_cbcr_qp_offset_value           = (int8_t)pPicParamSet->pps_joint_cbcr_qp_offset_value;
        pPicParam->pps_chroma_qp_offset_list_len_minus1     = (uint8_t)(pPicParamSet->pps_chroma_qp_offset_list_len - 1);;

        //change to for loop later.
        for (uint32_t i = 0; i < 6; i++)
        {
            pPicParam->pps_cb_qp_offset_list[i]         = (int8_t)pPicParamSet->pps_cb_qp_offset_list[i];
            pPicParam->pps_cr_qp_offset_list[i]         = (int8_t)pPicParamSet->pps_cr_qp_offset_list[i];
            pPicParam->pps_joint_cbcr_qp_offset_list[i] = (int8_t)pPicParamSet->pps_joint_cbcr_qp_offset_list[i];
        }
        pPicParam->pps_flags.bits.pps_loop_filter_across_tiles_enabled_flag = (uint32_t)pPicParamSet->pps_loop_filter_across_tiles_enabled_flag;
        pPicParam->pps_flags.bits.pps_rect_slice_flag = (uint32_t)pPicParamSet->pps_rect_slice_flag;
        pPicParam->pps_flags.bits.pps_single_slice_per_subpic_flag = (uint32_t)pPicParamSet->pps_single_slice_per_subpic_flag;
        pPicParam->pps_flags.bits.pps_loop_filter_across_slices_enabled_flag = (uint32_t)pPicParamSet->pps_loop_filter_across_slices_enabled_flag;
        pPicParam->pps_flags.bits.pps_weighted_pred_flag = (uint32_t)pPicParamSet->pps_weighted_pred_flag;
        pPicParam->pps_flags.bits.pps_weighted_bipred_flag = (uint32_t)pPicParamSet->pps_weighted_bipred_flag;
        pPicParam->pps_flags.bits.pps_ref_wraparound_enabled_flag = (uint32_t)pPicParamSet->pps_ref_wraparound_enabled_flag;
        pPicParam->pps_flags.bits.pps_cu_qp_delta_enabled_flag = (uint32_t)pPicParamSet->pps_cu_qp_delta_enabled_flag;
        pPicParam->pps_flags.bits.pps_cu_chroma_qp_offset_list_enabled_flag = (uint32_t)pPicParamSet->pps_cu_chroma_qp_offset_list_enabled_flag;
        pPicParam->pps_flags.bits.pps_deblocking_filter_override_enabled_flag = (uint32_t)pPicParamSet->pps_deblocking_filter_override_enabled_flag;
        pPicParam->pps_flags.bits.pps_deblocking_filter_disabled_flag = (uint32_t)pPicParamSet->pps_deblocking_filter_disabled_flag;
        pPicParam->pps_flags.bits.pps_dbf_info_in_ph_flag = (uint32_t)pPicParamSet->pps_dbf_info_in_ph_flag;
        pPicParam->pps_flags.bits.pps_sao_info_in_ph_flag = (uint32_t)pPicParamSet->pps_sao_info_in_ph_flag;
        pPicParam->pps_flags.bits.pps_alf_info_in_ph_flag = (uint32_t)pPicParamSet->pps_alf_info_in_ph_flag;

        pPicParam->ph_lmcs_aps_id         = (uint8_t)pPicHeader->ph_lmcs_aps_id;
        pPicParam->ph_scaling_list_aps_id = (uint8_t)pPicHeader->ph_scaling_list_aps_id;

        if (!pSeqParamSet->sps_partition_constraints_override_enabled_flag ||
            !pPicHeader->ph_partition_constraints_override_flag)
        {
            pPicParam->ph_log2_diff_min_qt_min_cb_intra_slice_luma = (uint8_t)pSeqParamSet->sps_log2_diff_min_qt_min_cb_intra_slice_luma;
            pPicParam->ph_max_mtt_hierarchy_depth_intra_slice_luma = (uint8_t)pSeqParamSet->sps_max_mtt_hierarchy_depth_intra_slice_luma;
            pPicParam->ph_log2_diff_max_bt_min_qt_intra_slice_luma = (uint8_t)pSeqParamSet->sps_log2_diff_max_bt_min_qt_intra_slice_luma;
            pPicParam->ph_log2_diff_max_tt_min_qt_intra_slice_luma = (uint8_t)pSeqParamSet->sps_log2_diff_max_tt_min_qt_intra_slice_luma;
            pPicParam->ph_log2_diff_min_qt_min_cb_intra_slice_chroma = (uint8_t)pSeqParamSet->sps_log2_diff_min_qt_min_cb_intra_slice_chroma;
            pPicParam->ph_max_mtt_hierarchy_depth_intra_slice_chroma = (uint8_t)pSeqParamSet->sps_max_mtt_hierarchy_depth_intra_slice_chroma;
            pPicParam->ph_log2_diff_max_bt_min_qt_intra_slice_chroma = (uint8_t)pSeqParamSet->sps_log2_diff_max_bt_min_qt_intra_slice_chroma;
            pPicParam->ph_log2_diff_max_tt_min_qt_intra_slice_chroma = (uint8_t)pSeqParamSet->sps_log2_diff_max_tt_min_qt_intra_slice_chroma;
            pPicParam->ph_log2_diff_min_qt_min_cb_inter_slice = (uint8_t)pSeqParamSet->sps_log2_diff_min_qt_min_cb_inter_slice;
            pPicParam->ph_max_mtt_hierarchy_depth_inter_slice = (uint8_t)pSeqParamSet->sps_max_mtt_hierarchy_depth_inter_slice;
            pPicParam->ph_log2_diff_max_bt_min_qt_inter_slice = (uint8_t)pSeqParamSet->sps_log2_diff_max_bt_min_qt_inter_slice;
            pPicParam->ph_log2_diff_max_tt_min_qt_inter_slice = (uint8_t)pSeqParamSet->sps_log2_diff_max_tt_min_qt_inter_slice;
        }
        else
        {
            pPicParam->ph_log2_diff_min_qt_min_cb_intra_slice_luma = (uint8_t)pPicHeader->ph_log2_diff_min_qt_min_cb_intra_slice_luma;
            pPicParam->ph_max_mtt_hierarchy_depth_intra_slice_luma = (uint8_t)pPicHeader->ph_max_mtt_hierarchy_depth_intra_slice_luma;
            pPicParam->ph_log2_diff_max_bt_min_qt_intra_slice_luma = (uint8_t)pPicHeader->ph_log2_diff_max_bt_min_qt_intra_slice_luma;
            pPicParam->ph_log2_diff_max_tt_min_qt_intra_slice_luma = (uint8_t)pPicHeader->ph_log2_diff_max_tt_min_qt_intra_slice_luma;
            pPicParam->ph_log2_diff_min_qt_min_cb_intra_slice_chroma = (uint8_t)pPicHeader->ph_log2_diff_min_qt_min_cb_intra_slice_chroma;
            pPicParam->ph_max_mtt_hierarchy_depth_intra_slice_chroma = (uint8_t)pPicHeader->ph_max_mtt_hierarchy_depth_intra_slice_chroma;
            pPicParam->ph_log2_diff_max_bt_min_qt_intra_slice_chroma = (uint8_t)pPicHeader->ph_log2_diff_max_bt_min_qt_intra_slice_chroma;
            pPicParam->ph_log2_diff_max_tt_min_qt_intra_slice_chroma = (uint8_t)pPicHeader->ph_log2_diff_max_tt_min_qt_intra_slice_chroma;
            pPicParam->ph_log2_diff_min_qt_min_cb_inter_slice = (uint8_t)pPicHeader->ph_log2_diff_min_qt_min_cb_inter_slice;
            pPicParam->ph_max_mtt_hierarchy_depth_inter_slice = (uint8_t)pPicHeader->ph_max_mtt_hierarchy_depth_inter_slice;
            pPicParam->ph_log2_diff_max_bt_min_qt_inter_slice = (uint8_t)pPicHeader->ph_log2_diff_max_bt_min_qt_inter_slice;
            pPicParam->ph_log2_diff_max_tt_min_qt_inter_slice = (uint8_t)pPicHeader->ph_log2_diff_max_tt_min_qt_inter_slice;
        }
        pPicParam->ph_cu_qp_delta_subdiv_intra_slice = (uint8_t)pPicHeader->ph_cu_qp_delta_subdiv_intra_slice;
        pPicParam->ph_cu_chroma_qp_offset_subdiv_intra_slice = (uint8_t)pPicHeader->ph_cu_chroma_qp_offset_subdiv_intra_slice;
        pPicParam->ph_cu_qp_delta_subdiv_inter_slice = (uint8_t)pPicHeader->ph_cu_qp_delta_subdiv_inter_slice;
        pPicParam->ph_cu_chroma_qp_offset_subdiv_inter_slice = (uint8_t)pPicHeader->ph_cu_chroma_qp_offset_subdiv_inter_slice;

        pPicParam->ph_flags.bits.ph_non_ref_pic_flag = (uint32_t)pPicHeader->ph_non_ref_pic_flag;
        pPicParam->ph_flags.bits.ph_alf_enabled_flag = (uint32_t)pPicHeader->ph_alf_enabled_flag;
        pPicParam->ph_flags.bits.ph_alf_cb_enabled_flag = (uint32_t)pPicHeader->ph_alf_cb_enabled_flag;
        pPicParam->ph_flags.bits.ph_alf_cr_enabled_flag = (uint32_t)pPicHeader->ph_alf_cr_enabled_flag;
        pPicParam->ph_flags.bits.ph_alf_cc_cb_enabled_flag = (uint32_t)pPicHeader->ph_alf_cc_cb_enabled_flag;
        pPicParam->ph_flags.bits.ph_alf_cc_cr_enabled_flag = (uint32_t)pPicHeader->ph_alf_cc_cr_enabled_flag;
        pPicParam->ph_flags.bits.ph_lmcs_enabled_flag = (uint32_t)pPicHeader->ph_lmcs_enabled_flag;
        pPicParam->ph_flags.bits.ph_chroma_residual_scale_flag = (uint32_t)pPicHeader->ph_chroma_residual_scale_flag;
        pPicParam->ph_flags.bits.ph_explicit_scaling_list_enabled_flag = (uint32_t)pPicHeader->ph_explicit_scaling_list_enabled_flag;
        pPicParam->ph_flags.bits.ph_virtual_boundaries_present_flag = (uint32_t)pPicHeader->ph_virtual_boundaries_present_flag;        
        pPicParam->ph_flags.bits.ph_temporal_mvp_enabled_flag = (uint32_t)pPicHeader->ph_temporal_mvp_enabled_flag;
        pPicParam->ph_flags.bits.ph_mmvd_fullpel_only_flag = (uint32_t)pPicHeader->ph_mmvd_fullpel_only_flag;
        pPicParam->ph_flags.bits.ph_mvd_l1_zero_flag = (uint32_t)pPicHeader->ph_mvd_l1_zero_flag;
        pPicParam->ph_flags.bits.ph_bdof_disabled_flag = (uint32_t)pPicHeader->ph_bdof_disabled_flag;
        pPicParam->ph_flags.bits.ph_dmvr_disabled_flag = (uint32_t)pPicHeader->ph_dmvr_disabled_flag;
        if (pSeqParamSet->sps_prof_control_present_in_ph_flag)
        {
            pPicParam->ph_flags.bits.ph_prof_disabled_flag = (uint32_t)pPicHeader->ph_prof_disabled_flag;
        }
        else  if(pSeqParamSet->sps_affine_prof_enabled_flag)
        {
            pPicParam->ph_flags.bits.ph_prof_disabled_flag = 0;
        }
        else
        {
            pPicParam->ph_flags.bits.ph_prof_disabled_flag = 1;
        }
        
        pPicParam->ph_flags.bits.ph_joint_cbcr_sign_flag = (uint32_t)pPicHeader->ph_joint_cbcr_sign_flag;
        pPicParam->ph_flags.bits.ph_sao_luma_enabled_flag = (uint32_t)pPicHeader->ph_sao_luma_enabled_flag;
        pPicParam->ph_flags.bits.ph_sao_chroma_enabled_flag = (uint32_t)pPicHeader->ph_sao_chroma_enabled_flag;
        pPicParam->ph_flags.bits.ph_deblocking_filter_disabled_flag = (uint32_t)pPicHeader->ph_deblocking_filter_disabled_flag;
        pPicParam->PicMiscFlags.fields.IntraPicFlag = (uint32_t)(pPicParamSet->pps_mixed_nalu_types_in_pic_flag == 0
            && ((pSlcHeader->nal_unit_type >= NAL_UNIT_CODED_SLICE_IDR_W_RADL) && (pSlcHeader->nal_unit_type <= NAL_UNIT_CODED_SLICE_CRA)));
        pFrame->setAsReferenced(true);
    }
    void PackerVA::PackAU(VVCDecoderFrame *pFrame, DPBType dpb)
    {
        if (!pFrame)
            throw vvc_exception(UMC::UMC_ERR_FAILED);

        VVCDecoderFrameInfo const* pSliceInfo = pFrame->GetAU();
        if (!pSliceInfo)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        auto pFirstSlice = pSliceInfo->GetSlice(0);
        if (!pFirstSlice)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        // Pic params buffer packing
        UMC::UMCVACompBuffer* compBufPic = nullptr;
        auto picParam = reinterpret_cast<VAPictureParameterBufferVVC*>(m_va->GetCompBuffer(VAPictureParameterBufferType, &compBufPic,sizeof(VAPictureParameterBufferVVC)));
        if (!picParam || !compBufPic || (static_cast<size_t>(compBufPic->GetBufferSize()) < sizeof(VAPictureParameterBufferVVC)))
            throw vvc_exception(MFX_ERR_MEMORY_ALLOC);

        memset(picParam, 0, sizeof(VAPictureParameterBufferVVC));
        PackPicParams(picParam, pFirstSlice, pFrame, dpb);

        // ALF/LMCS/Scalinglist buffer packing
        if (IsAPSAvailable(pFirstSlice, ALF_APS))
        {
            uint32_t ALF_num = 0;
            for (uint8_t id = 0; id < pFirstSlice->GetAPSSize(); id++)
            {
                VVCAPS const& aps = pFirstSlice->GetAPS(id);
                if (aps.aps_params_type == ALF_APS && aps.m_fresh)
                    ALF_num ++;
            }
            UMC::UMCVACompBuffer* compBufAlf = nullptr;
            auto alfParam = reinterpret_cast<VAAlfDataVVC*>(m_va->GetCompBuffer(VAAlfBufferType, &compBufAlf,sizeof(VAAlfDataVVC)*ALF_num));
            if (!alfParam || !compBufAlf || (static_cast<size_t>(compBufAlf->GetBufferSize()) < sizeof(VAAlfDataVVC)*ALF_num))
                throw vvc_exception(MFX_ERR_MEMORY_ALLOC);
            memset(alfParam, 0, sizeof(VAAlfDataVVC)*ALF_num);
            pFirstSlice->aps_num[ALF_APS] = ALF_num;     // return the actual No.
            PackALFParams(alfParam, pFirstSlice);
        }

        if (IsAPSAvailable(pFirstSlice, LMCS_APS))
        {
            uint32_t LMCS_num = 0;
            for (uint8_t id = 0; id < pFirstSlice->GetAPSSize(); id++)
            {
                VVCAPS const& aps = pFirstSlice->GetAPS(id);
                if (aps.aps_params_type == LMCS_APS && aps.m_fresh)
                    LMCS_num++;
            }
            // LMCS buffer packing
            UMC::UMCVACompBuffer* compBufLmcs = nullptr;
            auto lmcsParam = reinterpret_cast<VALmcsDataVVC*>(m_va->GetCompBuffer(VALmcsBufferType, &compBufLmcs,sizeof(VALmcsDataVVC)*LMCS_num));
            if (!lmcsParam || !compBufLmcs || (static_cast<size_t>(compBufLmcs->GetBufferSize()) < sizeof(VALmcsDataVVC)))
            {
                throw vvc_exception(MFX_ERR_MEMORY_ALLOC);
            }
            pFirstSlice->aps_num[LMCS_APS] = LMCS_num;
            PackLMCSParams(lmcsParam, pFirstSlice);
        }

        if (IsAPSAvailable(pFirstSlice, SCALING_LIST_APS)) 
        {
            uint32_t scalinglist_num = 0;
            for (uint8_t id = 0; id < pFirstSlice->GetAPSSize(); id++)
            {
                VVCAPS const& aps = pFirstSlice->GetAPS(id);
                if (aps.aps_params_type == SCALING_LIST_APS && aps.m_fresh)
                    scalinglist_num++;
            }
            // ScalingList buffer packing
            UMC::UMCVACompBuffer* compBufSL = nullptr;
            auto scalingParam = reinterpret_cast<VAScalingListVVC*>(m_va->GetCompBuffer(VAIQMatrixBufferType, &compBufSL,sizeof(VAScalingListVVC)*scalinglist_num));
            if (!scalingParam || !compBufSL || (static_cast<size_t>(compBufSL->GetBufferSize()) < sizeof(VAScalingListVVC)))
            {
                throw vvc_exception(MFX_ERR_MEMORY_ALLOC);
            }
            pFirstSlice->aps_num[SCALING_LIST_APS] = scalinglist_num;
            PackScalingParams(scalingParam, pFirstSlice);
        }
        
        // Struct params buffer packing
        uint32_t num_slices_in_pic = 0;
        for (uint32_t i = 0; i < (pFirstSlice->GetPPS()->pps_num_slices_in_pic_minus1 + 1); i++) 
        {
            if (pFirstSlice->m_rectSlice[i] && !pFirstSlice->m_rectSlice[i]->pps_derived_exp_slice_height_flag)
                num_slices_in_pic++;
        }
        UMC::UMCVACompBuffer* compBufSlice = nullptr;
        auto sliceStruct = reinterpret_cast<VASliceStructVVC*>(m_va->GetCompBuffer(VASliceStructBufferType, &compBufSlice, sizeof(VASliceStructVVC) * num_slices_in_pic));
        PackSliceStructParams(sliceStruct, pFirstSlice);

        // slice control buffer packing
        PackSliceParam(picParam,dpb,pSliceInfo);

        // Mark APS after sending buffer pack
        pFirstSlice->MarkAPS();

        // Subpic params buffer packing
        PackSubpicParams(pFirstSlice);

        // Tile params buffer packing
        PackTileParams(pFirstSlice);
    } 
} // namespace UMC_VVC_DECODER


#endif //  UMC_RESTRICTED_CODE_VA

#endif // MFX_ENABLE_VVC_VIDEO_DECODE
