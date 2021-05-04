// Copyright (c) 2013-2020 Intel Corporation
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
#ifdef MFX_ENABLE_H265_VIDEO_DECODE

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA

#include "umc_hevc_ddi.h"
#include "umc_h265_va_packer_dxva.h"

using namespace UMC;

namespace UMC_HEVC_DECODER
{
    class MSPackerDXVA2
        : public PackerDXVA2
    {

    public:

        MSPackerDXVA2(VideoAccelerator * va)
            : PackerDXVA2(va)
        {}

    private:

        void PackPicParams(const H265DecoderFrame *pCurrentFrame, TaskSupplier_H265 * supplier);
        bool PackSliceParams(H265Slice const* pSlice, size_t, bool isLastSlice);
    };

    Packer* CreatePackerMS(VideoAccelerator* va)
    { return new MSPackerDXVA2(va); }

    void MSPackerDXVA2::PackPicParams(const H265DecoderFrame *pCurrentFrame, TaskSupplier_H265 * supplier)
    {
        UMCVACompBuffer *compBuf;
        DXVA_PicParams_HEVC* pPicParam = (DXVA_PicParams_HEVC*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBuf);
        memset(pPicParam, 0, sizeof(DXVA_PicParams_HEVC));

        compBuf->SetDataSize(sizeof(DXVA_PicParams_HEVC));

        H265DecoderFrameInfo const* pSliceInfo = pCurrentFrame->GetAU();
        if (!pSliceInfo)
            throw h265_exception(UMC_ERR_FAILED);

        auto pSlice = pSliceInfo->GetSlice(0);
        if (!pSlice)
            throw h265_exception(UMC_ERR_FAILED);

        H265SliceHeader const* sliceHeader = pSlice->GetSliceHeader();
        H265SeqParamSet const* pSeqParamSet = pSlice->GetSeqParam();
        H265PicParamSet const* pPicParamSet = pSlice->GetPicParam();

        pPicParam->PicWidthInMinCbsY = (USHORT)LengthInMinCb(pSeqParamSet->pic_width_in_luma_samples, pSeqParamSet->log2_min_luma_coding_block_size);
        pPicParam->PicHeightInMinCbsY = (USHORT)LengthInMinCb(pSeqParamSet->pic_height_in_luma_samples, pSeqParamSet->log2_min_luma_coding_block_size);

        pPicParam->chroma_format_idc = pSeqParamSet->chroma_format_idc;
        pPicParam->separate_colour_plane_flag = 0;    // 0 in HEVC spec HM10 by design
        pPicParam->bit_depth_luma_minus8 = (UCHAR)(pSeqParamSet->bit_depth_luma - 8);
        pPicParam->bit_depth_chroma_minus8 = (UCHAR)(pSeqParamSet->bit_depth_chroma - 8);
        pPicParam->log2_max_pic_order_cnt_lsb_minus4 = (UCHAR)(pSeqParamSet->log2_max_pic_order_cnt_lsb - 4);
        pPicParam->NoPicReorderingFlag = pSeqParamSet->sps_max_num_reorder_pics == 0 ? 1 : 0;
        pPicParam->NoBiPredFlag = 0;

        pPicParam->CurrPic.Index7Bits = pCurrentFrame->m_index;

        pPicParam->sps_max_dec_pic_buffering_minus1 = (UCHAR)(pSeqParamSet->sps_max_dec_pic_buffering[sliceHeader->nuh_temporal_id] - 1);
        pPicParam->log2_min_luma_coding_block_size_minus3 = (UCHAR)(pSeqParamSet->log2_min_luma_coding_block_size - 3);
        pPicParam->log2_diff_max_min_transform_block_size = (UCHAR)(pSeqParamSet->log2_max_transform_block_size - pSeqParamSet->log2_min_transform_block_size);
        pPicParam->log2_min_transform_block_size_minus2 = (UCHAR)(pSeqParamSet->log2_min_transform_block_size - 2);
        pPicParam->log2_diff_max_min_luma_coding_block_size = (UCHAR)(pSeqParamSet->log2_max_luma_coding_block_size - pSeqParamSet->log2_min_luma_coding_block_size);
        pPicParam->max_transform_hierarchy_depth_intra = (UCHAR)pSeqParamSet->max_transform_hierarchy_depth_intra;
        pPicParam->max_transform_hierarchy_depth_inter = (UCHAR)pSeqParamSet->max_transform_hierarchy_depth_inter;
        pPicParam->num_short_term_ref_pic_sets = (UCHAR)pSeqParamSet->getRPSList()->getNumberOfReferencePictureSets();
        pPicParam->num_long_term_ref_pics_sps = (UCHAR)pSeqParamSet->num_long_term_ref_pics_sps;
        pPicParam->num_ref_idx_l0_default_active_minus1 = (UCHAR)(pPicParamSet->num_ref_idx_l0_default_active - 1);
        pPicParam->num_ref_idx_l1_default_active_minus1 = (UCHAR)(pPicParamSet->num_ref_idx_l1_default_active - 1);
        pPicParam->init_qp_minus26 = (CHAR)pPicParamSet->init_qp - 26;

        if (!sliceHeader->short_term_ref_pic_set_sps_flag)
        {
            pPicParam->ucNumDeltaPocsOfRefRpsIdx = (UCHAR)pSeqParamSet->getRPSList()->getNumberOfReferencePictureSets();
            pPicParam->wNumBitsForShortTermRPSInSlice = (USHORT)sliceHeader->wNumBitsForShortTermRPSInSlice;
        }

        // dwCodingParamToolFlags
        //
        pPicParam->scaling_list_enabled_flag = pSeqParamSet->scaling_list_enabled_flag ? 1 : 0;
        pPicParam->amp_enabled_flag = pSeqParamSet->amp_enabled_flag ? 1 : 0;
        pPicParam->sample_adaptive_offset_enabled_flag = pSeqParamSet->sample_adaptive_offset_enabled_flag ? 1 : 0;
        pPicParam->pcm_enabled_flag = pSeqParamSet->pcm_enabled_flag ? 1 : 0;
        pPicParam->pcm_sample_bit_depth_luma_minus1 = (UCHAR)(pSeqParamSet->pcm_sample_bit_depth_luma - 1);
        pPicParam->pcm_sample_bit_depth_chroma_minus1 = (UCHAR)(pSeqParamSet->pcm_sample_bit_depth_chroma - 1);
        pPicParam->log2_min_pcm_luma_coding_block_size_minus3 = (UCHAR)(pSeqParamSet->log2_min_pcm_luma_coding_block_size - 3);
        pPicParam->log2_diff_max_min_pcm_luma_coding_block_size = (UCHAR)(pSeqParamSet->log2_max_pcm_luma_coding_block_size - pSeqParamSet->log2_min_pcm_luma_coding_block_size);
        pPicParam->pcm_loop_filter_disabled_flag = pSeqParamSet->pcm_loop_filter_disabled_flag ? 1 : 0;
        pPicParam->long_term_ref_pics_present_flag = pSeqParamSet->long_term_ref_pics_present_flag ? 1 : 0;
        pPicParam->sps_temporal_mvp_enabled_flag = pSeqParamSet->sps_temporal_mvp_enabled_flag ? 1 : 0;
        pPicParam->strong_intra_smoothing_enabled_flag = pSeqParamSet->sps_strong_intra_smoothing_enabled_flag ? 1 : 0;
        pPicParam->dependent_slice_segments_enabled_flag = pPicParamSet->dependent_slice_segments_enabled_flag ? 1 : 0;
        pPicParam->output_flag_present_flag = pPicParamSet->output_flag_present_flag ? 1 : 0;
        pPicParam->num_extra_slice_header_bits = (UCHAR)pPicParamSet->num_extra_slice_header_bits;
        pPicParam->sign_data_hiding_enabled_flag = pPicParamSet->sign_data_hiding_enabled_flag ? 1 : 0;
        pPicParam->cabac_init_present_flag = pPicParamSet->cabac_init_present_flag ? 1 : 0;

        pPicParam->constrained_intra_pred_flag = pPicParamSet->constrained_intra_pred_flag ? 1 : 0;
        pPicParam->transform_skip_enabled_flag = pPicParamSet->transform_skip_enabled_flag ? 1 : 0;
        pPicParam->cu_qp_delta_enabled_flag = pPicParamSet->cu_qp_delta_enabled_flag;
        pPicParam->pps_slice_chroma_qp_offsets_present_flag = pPicParamSet->pps_slice_chroma_qp_offsets_present_flag ? 1 : 0;
        pPicParam->weighted_pred_flag = pPicParamSet->weighted_pred_flag ? 1 : 0;
        pPicParam->weighted_bipred_flag = pPicParamSet->weighted_bipred_flag ? 1 : 0;
        pPicParam->transquant_bypass_enabled_flag = pPicParamSet->transquant_bypass_enabled_flag ? 1 : 0;
        pPicParam->tiles_enabled_flag = pPicParamSet->tiles_enabled_flag;
        pPicParam->entropy_coding_sync_enabled_flag = pPicParamSet->entropy_coding_sync_enabled_flag;
        pPicParam->uniform_spacing_flag = 0;
        pPicParam->loop_filter_across_tiles_enabled_flag = pPicParamSet->loop_filter_across_tiles_enabled_flag;
        pPicParam->pps_loop_filter_across_slices_enabled_flag = pPicParamSet->pps_loop_filter_across_slices_enabled_flag;
        pPicParam->deblocking_filter_override_enabled_flag = pPicParamSet->deblocking_filter_override_enabled_flag ? 1 : 0;
        pPicParam->pps_deblocking_filter_disabled_flag = pPicParamSet->pps_deblocking_filter_disabled_flag ? 1 : 0;
        pPicParam->lists_modification_present_flag = pPicParamSet->lists_modification_present_flag ? 1 : 0;
        pPicParam->slice_segment_header_extension_present_flag = pPicParamSet->slice_segment_header_extension_present_flag;
        pPicParam->IrapPicFlag = (sliceHeader->nal_unit_type >= NAL_UT_CODED_SLICE_BLA_W_LP && sliceHeader->nal_unit_type <= NAL_UT_CODED_SLICE_CRA) ? 1 : 0;
        pPicParam->IdrPicFlag = sliceHeader->IdrPicFlag;
        pPicParam->IntraPicFlag = pSliceInfo->IsIntraAU() ? 1 : 0;

        pPicParam->pps_cb_qp_offset = (CHAR)pPicParamSet->pps_cb_qp_offset;
        pPicParam->pps_cr_qp_offset = (CHAR)pPicParamSet->pps_cr_qp_offset;
        if (pPicParam->tiles_enabled_flag)
        {
            pPicParam->num_tile_columns_minus1 = UCHAR(std::min<uint32_t>(sizeof(pPicParam->column_width_minus1) / sizeof(pPicParam->column_width_minus1[0]), pPicParamSet->num_tile_columns - 1));
            pPicParam->num_tile_rows_minus1    = UCHAR(std::min<uint32_t>(sizeof(pPicParam->row_height_minus1)   / sizeof(pPicParam->row_height_minus1[0]),   pPicParamSet->num_tile_rows    - 1));

            //if (!pPicParamSet->uniform_spacing_flag)
            {
                for (uint32_t i = 0; i <= pPicParam->num_tile_columns_minus1; i++)
                    pPicParam->column_width_minus1[i] = (uint16_t)(pPicParamSet->column_width[i] - 1);

                for (uint32_t i = 0; i <= pPicParam->num_tile_rows_minus1; i++)
                    pPicParam->row_height_minus1[i] = (uint16_t)(pPicParamSet->row_height[i] - 1);
            }
        }

        pPicParam->diff_cu_qp_delta_depth = (UCHAR)(pPicParamSet->diff_cu_qp_delta_depth);
        pPicParam->pps_beta_offset_div2 = (CHAR)(pPicParamSet->pps_beta_offset >> 1);
        pPicParam->pps_tc_offset_div2 = (CHAR)(pPicParamSet->pps_tc_offset >> 1);
        pPicParam->log2_parallel_merge_level_minus2 = (UCHAR)pPicParamSet->log2_parallel_merge_level - 2;
        pPicParam->CurrPicOrderCntVal = sliceHeader->m_poc;


        H265DBPList *dpb = supplier->GetDPBList();
        if (!dpb)
            throw h265_exception(UMC_ERR_FAILED);

        int32_t count = 0;
        ReferencePictureSet *rps = pSlice->getRPS();
        for (H265DecoderFrame* frame = dpb->head(); frame && count < sizeof(pPicParam->RefPicList) / sizeof(pPicParam->RefPicList[0]); frame = frame->future())
        {
            if (frame != pCurrentFrame)
            {
                int refType = frame->isShortTermRef() ? SHORT_TERM_REFERENCE : (frame->isLongTermRef() ? LONG_TERM_REFERENCE : NO_REFERENCE);

                if (refType != NO_REFERENCE)
                {
                    pPicParam->PicOrderCntValList[count] = frame->m_PicOrderCnt;

                    pPicParam->RefPicList[count].Index7Bits = frame->m_index;
                    pPicParam->RefPicList[count].AssociatedFlag = (refType == LONG_TERM_REFERENCE);

                    count++;
                }
            }
        }

        uint32_t index;
        int32_t pocList[3 * 8];
        int32_t numRefPicSetStCurrBefore = 0,
            numRefPicSetStCurrAfter = 0,
            numRefPicSetLtCurr = 0;
        for (index = 0; index < rps->getNumberOfNegativePictures(); index++)
            pocList[numRefPicSetStCurrBefore++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
        for (; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures(); index++)
            pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
        for (; index < rps->getNumberOfNegativePictures() + rps->getNumberOfPositivePictures() + rps->getNumberOfLongtermPictures(); index++)
        {
            int32_t poc = rps->getPOC(index);
            H265DecoderFrame *pFrm = dpb->findLongTermRefPic(pCurrentFrame, poc, pSeqParamSet->log2_max_pic_order_cnt_lsb, !rps->getCheckLTMSBPresent(index));

            if (pFrm)
            {
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = pFrm->PicOrderCnt();
            }
            else
            {
                pocList[numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr++] = pPicParam->CurrPicOrderCntVal + rps->getDeltaPOC(index);
            }
        }

        int32_t cntRefPicSetStCurrBefore = 0,
            cntRefPicSetStCurrAfter = 0,
            cntRefPicSetLtCurr = 0;

        for (int32_t n = 0; n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr; n++)
        {
            if (!rps->getUsed(n))
                continue;

            for (int32_t k = 0; k < count; k++)
            {
                if (pocList[n] == pPicParam->PicOrderCntValList[k])
                {
                    if (n < numRefPicSetStCurrBefore)
                        pPicParam->RefPicSetStCurrBefore[cntRefPicSetStCurrBefore++] = (UCHAR)k;
                    else if (n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter)
                        pPicParam->RefPicSetStCurrAfter[cntRefPicSetStCurrAfter++] = (UCHAR)k;
                    else if (n < numRefPicSetStCurrBefore + numRefPicSetStCurrAfter + numRefPicSetLtCurr)
                        pPicParam->RefPicSetLtCurr[cntRefPicSetLtCurr++] = (UCHAR)k;
                }
            }
        }

        m_refFrameListCacheSize = count;
        for (int n = count; n < sizeof(pPicParam->RefPicList) / sizeof(pPicParam->RefPicList[0]); n++)
        {
            pPicParam->RefPicList[n].bPicEntry = (UCHAR)0xff;
            pPicParam->PicOrderCntValList[n] = -1;
        }

        for (int32_t i = 0; i < 8; i++)
        {
            if (i >= cntRefPicSetStCurrBefore)
                pPicParam->RefPicSetStCurrBefore[i] = 0xff;
            if (i >= cntRefPicSetStCurrAfter)
                pPicParam->RefPicSetStCurrAfter[i] = 0xff;
            if (i >= cntRefPicSetLtCurr)
                pPicParam->RefPicSetLtCurr[i] = 0xff;
        }

        pPicParam->StatusReportFeedbackNumber = m_statusReportFeedbackCounter;
    }

    bool MSPackerDXVA2::PackSliceParams(H265Slice const* pSlice, size_t sliceNum, bool isLastSlice)
    {
        static uint8_t start_code_prefix[] = { 0, 0, 1 };

        uint32_t  rawDataSize = 0;
        const void*  rawDataPtr = 0;

        H265HeadersBitstream const* pBitstream = pSlice->GetBitStream();
        pBitstream->GetOrg((uint32_t**)&rawDataPtr, &rawDataSize);

        UMCVACompBuffer *headVABffr = 0;
        UMCVACompBuffer *dataVABffr = 0;
        DXVA_Slice_HEVC_Short* dxvaSlice = (DXVA_Slice_HEVC_Short*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &headVABffr);
        if (headVABffr->GetBufferSize() - headVABffr->GetDataSize() < sizeof(DXVA_Slice_HEVC_Short))
            return false;

        dxvaSlice += sliceNum;
        uint8_t *dataBffr = (uint8_t *)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &dataVABffr);

        dxvaSlice->BSNALunitDataLocation = dataVABffr->GetDataSize();

        int32_t storedSize = rawDataSize + sizeof(start_code_prefix);
        dxvaSlice->SliceBytesInBuffer = storedSize;
        dxvaSlice->wBadSliceChopping = 0;

        if (storedSize + 127 >= dataVABffr->GetBufferSize() - dataVABffr->GetDataSize())
            return false;

        dataBffr += dataVABffr->GetDataSize();
        MFX_INTERNAL_CPY(dataBffr, start_code_prefix, sizeof(start_code_prefix));
        MFX_INTERNAL_CPY(dataBffr + sizeof(start_code_prefix), rawDataPtr, rawDataSize);

        int32_t fullSize = dataVABffr->GetDataSize() + storedSize;
        if (isLastSlice)
        {
            int32_t alignedSize = mfx::align2_value(fullSize, 128);
            VM_ASSERT(alignedSize < dataVABffr->GetBufferSize());
            memset(dataBffr + storedSize, 0, alignedSize - fullSize);
            fullSize = alignedSize;
        }

        dataVABffr->SetDataSize(fullSize);
        headVABffr->SetDataSize(headVABffr->GetDataSize() + sizeof(DXVA_Slice_HEVC_Short));
        return true;
    }
}

#endif //UMC_VA_DXVA

#endif // UMC_RESTRICTED_CODE_VA
#endif // MFX_ENABLE_H265_VIDEO_DECODE