// Copyright (c) 2022-2024 Intel Corporation
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

#include "umc_vvc_slice_decoding.h"

#include <algorithm>
#include <numeric>

namespace UMC_VVC_DECODER
{
    VVCSlice::VVCSlice()
        : m_tileByteLocation(0)
        , m_picParamSet(0)
        , m_seqParamSet(0)
    {
        m_picHeader = {};
        m_adaptParamSet.clear();
        Reset();
    }

    VVCSlice::~VVCSlice()
    {
        m_currentFrame = 0;
        delete[] m_tileByteLocation;
        m_tileByteLocation = 0;

        if (m_seqParamSet)
        {
            if (m_seqParamSet)
                ((VVCSeqParamSet*)m_seqParamSet)->DecrementReference();
            if (m_picParamSet)
                ((VVCPicParamSet*)m_picParamSet)->DecrementReference();
            m_seqParamSet = 0;
            m_picParamSet = 0;
        }

        for (uint32_t i = 0; i < m_rectSlice.size(); i++)
        {
            delete m_rectSlice[i];
        }
        m_rectSlice.clear();
    }

    void VVCSlice::Reset()
    {
        m_currentFrame = 0;
        m_NumEmuPrevnBytesInSliceHdr = 0;
        memset(m_rpl, 0, sizeof(VVCReferencePictureList) * 2);

        if (m_seqParamSet) 
        {
            if (m_seqParamSet)
                ((VVCSeqParamSet*)m_seqParamSet)->DecrementReference();
            if (m_picParamSet)
                ((VVCPicParamSet*)m_picParamSet)->DecrementReference();
            m_seqParamSet = 0;
            m_picParamSet = 0;
        }
        m_adaptParamSet.clear();

        m_sliceHeader.nuh_temporal_id = 0;
        m_sliceHeader.deblocking_filter_disable_flag = false;
        m_sliceHeader.num_entry_point_offsets = 0;
        m_sliceHeader.sh_sign_data_hiding_used_flag = false;
        m_sliceMap.slice_id = 0;
        m_sliceMap.num_ctu_in_slice = 0;
        m_sliceMap.num_tiles_in_slice = 0;
        m_sliceMap.ctu_addr_in_slice.clear();

        m_tileCount = 0;
        delete[] m_tileByteLocation;
        m_tileByteLocation = 0;
    }

    void VVCSlice::Release()
    {
        Reset();
    }

    int32_t VVCSlice::RetrievePPSNumber(VVCPicHeader* picHeader)
    {
        if (!m_source.GetDataSize())
            return -1;

        m_sliceHeader = {};
        m_bitStream.Reset((uint8_t*)m_source.GetPointer(), (uint32_t)m_source.GetDataSize());

        UMC::Status umcRes = UMC::UMC_OK;

        try
        {
            umcRes = m_bitStream.GetNALUnitType(m_sliceHeader.nal_unit_type,
                m_sliceHeader.nuh_temporal_id, m_sliceHeader.nuh_layer_id);
            if (UMC::UMC_OK != umcRes)
                return false;

            // decode first part of slice header
            m_bitStream.GetSlicePicParamSetNumber(&m_sliceHeader, picHeader);
            if (UMC::UMC_OK != umcRes)
                return -1;
        }
        catch (...)
        {
            return -1;
        }

        return m_sliceHeader.slice_pic_parameter_set_id;
    }

    void VVCSlice::SetSliceNumber(uint32_t iSliceNumber)
    {
        m_iNumber = iSliceNumber;
    }

    void VVCSlice::SetPPS(const VVCPicParamSet* pps)
    {
        m_picParamSet = pps;
        if (m_picParamSet)
            m_picParamSet->IncrementReference();
    }

    void VVCSlice::SetSPS(const VVCSeqParamSet* sps)
    {
        m_seqParamSet = sps;
        if (m_seqParamSet)
            m_seqParamSet->IncrementReference();
    }

    void VVCSlice::SetAPS(HeaderSet<VVCAPS> *aps)
    {
        auto apsNum = aps->GetHeaderNum();
        for (uint32_t i = 0; i < apsNum; i++)
        { 
            VVCAPS* curAPS = aps->GetHeader(i);
            if (!curAPS) 
            {
                continue;
            }
            else if (curAPS->m_fresh) 
            {
                m_adaptParamSet.push_back(*curAPS);
                curAPS->Reset();
                curAPS->m_fresh = false;
            }
        }
    }

    void VVCSlice::MarkAPS() 
    {
        for (size_t i = 0; i < m_adaptParamSet.size(); i++)
        {
            m_adaptParamSet[i].m_fresh = false;
        }
    }

    // Decode slice header and intialize slice structure with parsed values
    bool VVCSlice::Reset(ParameterSetManager* m_currHeader, PocDecoding* pocDecoding, const int prevPicPOC)
    {
        m_bitStream.Reset((uint8_t*)m_source.GetPointer(), (uint32_t)m_source.GetDataSize());

        // decode slice header
        if (m_source.GetDataSize() && false == DecodeSliceHeader(m_currHeader, pocDecoding, prevPicPOC))
            return false;

        m_sliceHeader.m_HeaderBitstreamOffset = (uint32_t)m_bitStream.BytesDecoded();

        m_currentFrame = nullptr;
        return true;
    }

    void VVCSlice::InheritFromPicHeader(const VVCPicHeader* picHeader, const VVCPicParamSet* pps, const VVCSeqParamSet* sps)
    {
        if (pps->pps_rpl_info_in_ph_flag) 
        {
            for (uint8_t i = 0; i < VVC_MAX_NUM_REF_PIC_LISTS; i++) 
            {
                m_rplIdx[i] = picHeader->rpl_idx[i];
                m_sliceHeader.sh_rpl[i] = picHeader->rpl[i];
                if (m_rplIdx[i] != -1) 
                {
                    m_sliceHeader.sh_rpl[i] = sps->sps_ref_pic_lists[i][m_rplIdx[i]];
                }
            }
        }

        m_sliceHeader.deblocking_filter_disable_flag = picHeader->ph_deblocking_filter_disabled_flag;
        m_sliceHeader.sh_luma_beta_offset_div2 = picHeader->ph_luma_beta_offset_div2;
        m_sliceHeader.sh_luma_tc_offset_div2 = picHeader->ph_luma_tc_offset_div2;
        if (pps->pps_chroma_tool_offsets_present_flag) 
        {
            m_sliceHeader.sh_cb_beta_offset_div2 = picHeader->ph_cb_beta_offset_div2;
            m_sliceHeader.sh_cb_tc_offset_div2 = picHeader->ph_cb_tc_offset_div2;
            m_sliceHeader.sh_cr_beta_offset_div2 = picHeader->ph_cr_beta_offset_div2;
            m_sliceHeader.sh_cr_tc_offset_div2 = picHeader->ph_cr_tc_offset_div2;
        }
        else 
        {
            m_sliceHeader.sh_cb_beta_offset_div2 = m_sliceHeader.sh_luma_beta_offset_div2;
            m_sliceHeader.sh_cb_tc_offset_div2 = m_sliceHeader.sh_luma_tc_offset_div2;
            m_sliceHeader.sh_cr_beta_offset_div2 = m_sliceHeader.sh_luma_beta_offset_div2;
            m_sliceHeader.sh_cr_tc_offset_div2 = m_sliceHeader.sh_luma_tc_offset_div2;
        }
        m_sliceHeader.sh_sao_chroma_used_flag = picHeader->ph_sao_chroma_enabled_flag;
        m_sliceHeader.sh_sao_luma_used_flag = picHeader->ph_sao_luma_enabled_flag;

        m_sliceHeader.alf_enabled_flag[COMPONENT_Y] = picHeader->ph_alf_enabled_flag;
        m_sliceHeader.alf_enabled_flag[COMPONENT_Cb] = picHeader->ph_alf_cb_enabled_flag;
        m_sliceHeader.alf_enabled_flag[COMPONENT_Cr] = picHeader->ph_alf_cr_enabled_flag;
        m_sliceHeader.num_alf_aps_ids_luma = picHeader->ph_num_alf_aps_ids_luma;
        m_sliceHeader.alf_aps_ids_luma = picHeader->ph_alf_aps_id_luma;
        m_sliceHeader.alf_aps_id_chroma = picHeader->ph_alf_aps_id_chroma;
        m_sliceHeader.alf_cc_cb_enabled_flag = picHeader->ph_alf_cc_cb_enabled_flag;
        m_sliceHeader.alf_cc_cr_enabled_flag = picHeader->ph_alf_cc_cr_enabled_flag;
        m_sliceHeader.alf_cc_cb_aps_id = picHeader->ph_alf_cc_cb_aps_id;
        m_sliceHeader.alf_cc_cr_aps_id = picHeader->ph_alf_cc_cr_aps_id;
        m_sliceHeader.alf_cc_enabled_flag[COMPONENT_Cb - 1] = picHeader->ph_alf_cc_cb_enabled_flag;
        m_sliceHeader.alf_cc_enabled_flag[COMPONENT_Cr - 1] = picHeader->ph_alf_cc_cr_enabled_flag;
    }

    // Decoder slice header and calculate POC
    bool VVCSlice::DecodeSliceHeader(ParameterSetManager* m_currHeader, PocDecoding* pocDecoding, const int prevPicPOC)
    {
        UMC::Status umcRes = UMC::UMC_OK;

        // Locals for additional slice data to be read into, the data
        // was read and saved from the first slice header of the picture,
        // it is not supposed to change within the picture, so can be
        // discarded when read again here.
        try
        {
            umcRes = m_bitStream.GetNALUnitType(m_sliceHeader.nal_unit_type,
                                                m_sliceHeader.nuh_temporal_id,
                                                m_sliceHeader.nuh_layer_id);
            if (UMC::UMC_OK != umcRes)
                return false;

            umcRes = m_bitStream.GetSliceHeader(this, &m_picHeader, m_currHeader, pocDecoding, prevPicPOC);

       }
        catch(...)
        {
            return false;
        }

        return (UMC::UMC_OK == umcRes);
    }

    // Get tile column CTB width
    uint32_t VVCSlice::GetTileColumnWidth(uint32_t col) const
    {
        uint32_t tileColumnWidth = 0;

        (void)(col); //TODO: remove
        //...

        return tileColumnWidth;
    }

    // Get tile row CTB height
    uint32_t VVCSlice::GetTileRowHeight(uint32_t row) const
    {
        (void)(row); //TODO: remove
        uint32_t tileRowHeight = 0;
        // ...
        return tileRowHeight;
    }

    uint32_t VVCSlice::GetTileXIdx() const
    {
        uint32_t i = 0;
        // ...
        return i;
    }

    uint32_t VVCSlice::GetTileYIdx() const
    {
        uint32_t i = 0;
        // ...
        return i;
    }

    // Returns number of used references in RPS
    int VVCSlice::GetNumRpsCurrTempList() const
    {
        int numRpsCurrTempList = 0;

        // ... 
        return numRpsCurrTempList;
    }

    void VVCSlice::SetRefPOCListSliceHeader()
    {
    }

    // Searches DPB for a short term reference frame with specified POC
    VVCDecoderFrame* VVCSlice::GetRefPic(DPBType dpb, int32_t picPOC, uint32_t layerId)
    {
        VVCDecoderFrame* pCurr = nullptr;

        for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
        {
            pCurr = *it;
            if (pCurr->PicOrderCnt() == picPOC && pCurr->layerId == layerId)
                break;
        }

        return pCurr;
    }

    VVCDecoderFrame* VVCSlice::GetLongTermRefPic(DPBType dpb, int32_t picPOC, uint32_t bitsForPOC,
                                                 bool pocHasMsb, uint32_t layerId)
    {
        VVCDecoderFrame* refPic = nullptr;
        uint32_t pocCycle = 1 << bitsForPOC;
        uint32_t refPoc = pocHasMsb ? picPOC : (picPOC & (pocCycle - 1));

        VVCDecoderFrame* currPic = nullptr;
        for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
        {
            currPic = *it;
            if (currPic->PicOrderCnt() != m_sliceHeader.m_poc && currPic->isReferenced() && currPic->layerId == layerId)
            {
                uint32_t currPicPoc = pocHasMsb ? currPic->PicOrderCnt() : (currPic->PicOrderCnt() & (pocCycle - 1));
                if (refPoc == currPicPoc)
                {
                    if (currPic->isLongTermRef())
                    {
                        refPic = currPic;
                    }
                }
            }
        }
        return refPic;
    }


    // Searches DPB for a long term reference frame with specified POC
    VVCDecoderFrame* VVCSlice::GetLongTermRefPicCandidate(DPBType dpb, int32_t picPOC, uint32_t bitsForPOC,
                                                          bool pocHasMsb, uint32_t layerId) const
    {
        VVCDecoderFrame* refPic = nullptr;
        uint32_t pocCycle = 1 << bitsForPOC;
        uint32_t refPoc = pocHasMsb ? picPOC : (picPOC & (pocCycle - 1));

        VVCDecoderFrame* currPic = nullptr;
        for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
        {
            currPic = *it;
            if (currPic->PicOrderCnt() != m_sliceHeader.m_poc && currPic->isReferenced() && currPic->layerId == layerId)
            {
                uint32_t currPicPoc = pocHasMsb ? currPic->PicOrderCnt() : (currPic->PicOrderCnt() & (pocCycle - 1));
                if (refPoc == currPicPoc)
                {
                    refPic = currPic;
                }
            }
        }
        return refPic;
    } // findLongTermRefPic

    int VVCSlice::CheckAllRefPicsAvail(DPBType dpb, uint8_t rplIdx, int *refPicIndex)
    {
        bool isAvail = false;
        int32_t notPresentPoc = 0;

        // Assume that all pic in the DPB will be flushed anyway so no need to check
        if (m_sliceHeader.nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL || m_sliceHeader.nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_N_LP)
        {
            return 0;
        }

        int32_t numActiveRefPics = m_sliceHeader.num_ref_Idx[rplIdx];
        VVCReferencePictureList* rpl = &m_sliceHeader.sh_rpl[rplIdx];

        // Check long term pics
        for (int32_t ii = 0; rpl->number_of_long_term_pictures && ii < numActiveRefPics; ii++)
        {
            if (!rpl->is_long_term_ref_pic[ii] || rpl->is_inter_layer_ref_pic[ii])
                continue;

            notPresentPoc = rpl->ref_pic_identifier[ii];
            isAvail = 0;
            for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
            {
                VVCDecoderFrame* pCurr = *it;
                int32_t pocCycle = 1 << (m_seqParamSet->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
                int32_t curPoc = pCurr->m_PicOrderCnt;
                int32_t refPoc = rpl->ref_pic_identifier[ii] & (pocCycle - 1);
                if (rpl->delta_poc_msb_present_flag[ii])
                {
                    refPoc += m_sliceHeader.m_poc - rpl->delta_poc_msb_cycle_lt[ii] * pocCycle - (m_sliceHeader.m_poc & (pocCycle - 1));
                }
                else
                {
                    curPoc = curPoc & (pocCycle - 1);
                }
                if (pCurr->isLongTermRef() && curPoc == refPoc && pCurr->isReferenced())
                {
                    isAvail = 1;
                    break;
                }
            }
            // if there was no such long-term check the short terms
            if (!isAvail)
            {
                for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
                {
                    VVCDecoderFrame* pCurr = *it;
                    int32_t pocCycle = 1 << (m_seqParamSet->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
                    int32_t curPoc = pCurr->m_PicOrderCnt;
                    int32_t refPoc = rpl->ref_pic_identifier[ii] & (pocCycle - 1);
                    if (rpl->delta_poc_msb_present_flag[ii])
                    {
                        refPoc += m_sliceHeader.m_poc - rpl->delta_poc_msb_cycle_lt[ii] * pocCycle - (m_sliceHeader.m_poc & (pocCycle - 1));
                    }
                    else
                    {
                        curPoc = curPoc & (pocCycle - 1);
                    }
                    if (!pCurr->isLongTermRef() && curPoc == refPoc && pCurr->isReferenced())
                    {
                        isAvail = true;
                        pCurr->SetisLongTermRef(true);
                        break;
                    }
                }
            }

            if (!isAvail)
            {
                *refPicIndex = ii;
                return notPresentPoc;
            }
        }

        isAvail = false;
        // check short term ref pics
        for (int32_t ii = 0; ii < numActiveRefPics; ii++)
        {
            if (rpl->is_long_term_ref_pic[ii])
                continue;

            notPresentPoc = m_sliceHeader.m_poc + rpl->ref_pic_identifier[ii];
            isAvail = false;
            for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
            {
                VVCDecoderFrame* pCurr = *it;
                if (!pCurr->isLongTermRef() && pCurr->m_PicOrderCnt == m_sliceHeader.m_poc + rpl->ref_pic_identifier[ii] && pCurr->isReferenced())
                {
                    isAvail = true;
                    break;
                }
            }

            //report that a picture is lost if it is in the Reference Picture List but not in the DPB
            if (isAvail == 0 && rpl->number_of_short_term_pictures > 0)
            {
                *refPicIndex = ii;
                return notPresentPoc;
            }

        }

        return 0;
    }

    UMC::Status VVCSlice::UpdateRPLMarking(DPBType dpb)
    {
        UMC::Status umcRes = UMC::UMC_OK;
        VVCDecoderFrame* pTmp = nullptr;

        if (m_sliceHeader.IDRflag && m_picParamSet->pps_mixed_nalu_types_in_pic_flag == 0)
        {
            // mark all reference pictures as unused
            for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
            {
                VVCDecoderFrame* pCurr = *it;
                pCurr->GetAU()->m_frameBeforeIDR = 1;
                if (pCurr->isShortTermRef() || pCurr->isLongTermRef())
                {
                    pCurr->SetisShortTermRef(false);
                    pCurr->SetisLongTermRef(false);
                    pCurr->setAsReferenced(false);
                }
            }
        }
        else 
        {
            VVCReferencePictureList* RPL0 = &m_sliceHeader.sh_rpl[REF_PIC_LIST_0];
            VVCReferencePictureList* RPL1 = &m_sliceHeader.sh_rpl[REF_PIC_LIST_1];

            // mark long-term reference pictures in List0
            for (uint32_t i = 0; i < RPL0->number_of_short_term_pictures + RPL0->number_of_long_term_pictures + RPL0->number_of_inter_layer_pictures; i++)
            {
                if (!RPL0->is_long_term_ref_pic[i] || RPL0->is_inter_layer_ref_pic[i])
                    continue;

                bool isAvailable = 0;
                for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
                {
                    pTmp = *it;
                    if (pTmp->isDisposable() || !pTmp->isReferenced())
                        continue;

                    if (!pTmp->RefPicListResetCount())
                    {
                        int32_t pocCycle = 1 << (m_seqParamSet->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
                        int curPoc = pTmp->m_PicOrderCnt;
                        int32_t refPoc = RPL0->ref_pic_identifier[i] & (pocCycle - 1);
                        if (RPL0->delta_poc_msb_present_flag[i])
                        {
                            refPoc += m_sliceHeader.m_poc - RPL0->delta_poc_msb_cycle_lt[i] * pocCycle - (m_sliceHeader.m_poc & (pocCycle - 1));
                        }
                        else
                        {
                            curPoc = curPoc & (pocCycle - 1);
                        }
                        if (pTmp->isLongTermRef() && curPoc == refPoc && pTmp->isReferenced())
                        {
                            isAvailable = true;
                            break;
                        }
                    }
                }
                // if there was no such long-term check the short terms
                if (!isAvailable)
                {
                    for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
                    {
                        pTmp = *it;
                        if (pTmp->isDisposable() || !pTmp->isReferenced())
                            continue;

                        if (!pTmp->RefPicListResetCount())
                        {
                            int32_t pocCycle = 1 << (m_seqParamSet->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
                            int curPoc = pTmp->m_PicOrderCnt;
                            int32_t refPoc = RPL0->ref_pic_identifier[i] & (pocCycle - 1);
                            if (RPL0->delta_poc_msb_present_flag[i])
                            {
                                refPoc += m_sliceHeader.m_poc - RPL0->delta_poc_msb_cycle_lt[i] * pocCycle - (m_sliceHeader.m_poc & (pocCycle - 1));
                            }
                            else
                            {
                                curPoc = curPoc & (pocCycle - 1);
                            }
                            if (!pTmp->isLongTermRef() && curPoc == refPoc && pTmp->isReferenced())
                            {
                                isAvailable = true;
                                pTmp->SetisLongTermRef(true);
                                pTmp->SetisShortTermRef(false);
                                break;
                            }
                        }
                    }
                }
            }
            // mark long-term reference pictures in List1
            for (uint32_t i = 0; i < RPL1->number_of_short_term_pictures + RPL1->number_of_long_term_pictures + RPL1->number_of_inter_layer_pictures; i++)
            {
                if (!RPL1->is_long_term_ref_pic[i] || RPL1->is_inter_layer_ref_pic[i])
                    continue;

                bool isAvailable = 0;
                for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
                {
                    pTmp = *it;
                    if (pTmp->isDisposable() || !pTmp->isReferenced())
                        continue;

                    if (!pTmp->RefPicListResetCount())
                    {
                        int32_t pocCycle = 1 << (m_seqParamSet->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
                        int curPoc = pTmp->m_PicOrderCnt;
                        int32_t refPoc = RPL1->ref_pic_identifier[i] & (pocCycle - 1);
                        if (RPL1->delta_poc_msb_present_flag[i])
                        {
                            refPoc += m_sliceHeader.m_poc - RPL1->delta_poc_msb_cycle_lt[i] * pocCycle - (m_sliceHeader.m_poc & (pocCycle - 1));
                        }
                        else
                        {
                            curPoc = curPoc & (pocCycle - 1);
                        }
                        if (pTmp->isLongTermRef() && curPoc == refPoc && pTmp->isReferenced())
                        {
                            isAvailable = true;
                            break;
                        }
                    }
                }
                // if there was no such long-term check the short terms
                if (!isAvailable)
                {
                    for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
                    {
                        pTmp = *it;
                        if (pTmp->isDisposable() || !pTmp->isReferenced())
                            continue;

                        if (!pTmp->RefPicListResetCount())
                        {
                            int32_t pocCycle = 1 << (m_seqParamSet->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
                            int curPoc = pTmp->m_PicOrderCnt;
                            int32_t refPoc = RPL1->ref_pic_identifier[i] & (pocCycle - 1);
                            if (RPL1->delta_poc_msb_present_flag[i])
                            {
                                refPoc += m_sliceHeader.m_poc - RPL1->delta_poc_msb_cycle_lt[i] * pocCycle - (m_sliceHeader.m_poc & (pocCycle - 1));
                            }
                            else
                            {
                                curPoc = curPoc & (pocCycle - 1);
                            }
                            if (!pTmp->isLongTermRef() && curPoc == refPoc && pTmp->isReferenced())
                            {
                                isAvailable = true;
                                pTmp->SetisLongTermRef(true);
                                pTmp->SetisShortTermRef(false);
                                break;
                            }
                        }
                    }
                }
            }

            // loop through all pictures in the reference picture buffer
            for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
            {
                pTmp = *it;

                if (pTmp->isDisposable() || !pTmp->isReferenced())
                {
                    continue;
                }

                bool isReference = false;
                // loop through all pictures in the Reference Picture Set
                // to see if the picture should be kept as reference picture
                bool isNeedToCheck = (m_nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_N_LP ||
                                      m_nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL)
                                     && m_picParamSet->pps_mixed_nalu_types_in_pic_flag ? false : true;
                for (uint32_t i = 0; isNeedToCheck && !isReference && i < RPL0->number_of_short_term_pictures + RPL0->number_of_long_term_pictures + RPL0->number_of_inter_layer_pictures; i++)
                {
                    if (RPL0->is_inter_layer_ref_pic[i])
                    {
                        if (pTmp->PicOrderCnt() == m_sliceHeader.m_poc)
                        {
                            isReference = true;
                            pTmp->SetisLongTermRef(true);
                            pTmp->SetisShortTermRef(false);
                        }
                    }
                    else if (pTmp->layerId == m_sliceHeader.nuh_layer_id)
                    {
                        if (!RPL0->is_long_term_ref_pic[i]) 
                        {
                            if (pTmp->PicOrderCnt() == m_sliceHeader.m_poc + RPL0->ref_pic_identifier[i])
                            {
                                isReference = true;
                                pTmp->SetisLongTermRef(false);
                                pTmp->SetisShortTermRef(true);
                            }
                        }
                        else 
                        {
                            int32_t pocCycle = 1 << (m_seqParamSet->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
                            int curPoc = pTmp->m_PicOrderCnt;
                            int32_t refPoc = RPL0->ref_pic_identifier[i] & (pocCycle - 1);
                            if (RPL0->delta_poc_msb_present_flag[i])
                            {
                                refPoc += m_sliceHeader.m_poc - RPL0->delta_poc_msb_cycle_lt[i] * pocCycle - (m_sliceHeader.m_poc & (pocCycle - 1));
                            }
                            else
                            {
                                curPoc = curPoc & (pocCycle - 1);
                            }
                            if(pTmp->isLongTermRef() && curPoc == refPoc)
                            {
                                isReference = true;
                                pTmp->SetisLongTermRef(true);
                                pTmp->SetisShortTermRef(false);
                            }
                        }
                    }
                }

                for (uint32_t i = 0; isNeedToCheck && !isReference && i < RPL1->number_of_short_term_pictures + RPL1->number_of_long_term_pictures + RPL1->number_of_inter_layer_pictures; i++)
                {
                    if (RPL1->is_inter_layer_ref_pic[i])
                    {
                        if (pTmp->PicOrderCnt() == m_sliceHeader.m_poc)
                        {
                            isReference = true;
                            pTmp->SetisLongTermRef(true);
                            pTmp->SetisShortTermRef(false);
                        }
                    }
                    else if (pTmp->layerId == m_sliceHeader.nuh_layer_id)
                    {
                        if (!RPL1->is_long_term_ref_pic[i])
                        {
                            if (pTmp->PicOrderCnt() == m_sliceHeader.m_poc + RPL1->ref_pic_identifier[i])
                            {
                                isReference = true;
                                pTmp->SetisLongTermRef(false);
                                pTmp->SetisShortTermRef(true);
                            }
                        }
                        else
                        {
                            int32_t pocCycle = 1 << (m_seqParamSet->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
                            int curPoc = pTmp->m_PicOrderCnt;
                            int32_t refPoc = RPL1->ref_pic_identifier[i] & (pocCycle - 1);
                            if (RPL1->delta_poc_msb_present_flag[i])
                            {
                                refPoc += m_sliceHeader.m_poc - RPL1->delta_poc_msb_cycle_lt[i] * pocCycle - (m_sliceHeader.m_poc & (pocCycle - 1));
                            }
                            else
                            {
                                curPoc = curPoc & (pocCycle - 1);
                            }
                            if (pTmp->isLongTermRef() && curPoc == refPoc)
                            {
                                isReference = true;
                                pTmp->SetisLongTermRef(true);
                                pTmp->SetisShortTermRef(false);
                            }
                        }
                    }
                }

                // mark the picture as "unused for reference" if it is not in
                // the Reference Picture List
                if (pTmp->layerId == m_sliceHeader.nuh_layer_id && pTmp->PicOrderCnt() != m_sliceHeader.m_poc && isReference == 0)
                {
                    pTmp->setAsReferenced(false);
                    pTmp->SetisShortTermRef(false);
                    pTmp->SetisLongTermRef(false);
                }
            }
        }
        return umcRes;
    }

    uint32_t VVCSlice::GetMaxSurfNum()
    {
        if (!m_vidParamSet || m_vidParamSet->vps_num_layers_in_ols[0] == 1)
        {
            if (m_seqParamSet->profile_tier_level.general_level_idc == 65)
            {
                m_reorderPicNum = 0;
                m_maxSurfNum = 1;
            }
            else
            {
                m_reorderPicNum = m_seqParamSet->dpb_parameter.dpb_max_num_reorder_pics[m_seqParamSet->sps_max_sublayers_minus1];
                m_maxSurfNum = m_seqParamSet->dpb_parameter.dpb_max_dec_pic_buffering_minus1[m_seqParamSet->sps_max_sublayers_minus1] + 1 + m_reorderPicNum;
                if (m_maxSurfNum <= 1)
                {
                    m_maxSurfNum = 5;
                }
            }
        }
        else
        {
            if (m_vidParamSet->profile_tier_level[0].general_profile_idc == 65)
            {
                m_reorderPicNum = 0;
                m_maxSurfNum = 1;
            }
            else
            {
                m_reorderPicNum = m_vidParamSet->dpb_parameters[0].dpb_max_num_reorder_pics[m_seqParamSet->sps_max_sublayers_minus1];
                m_maxSurfNum = m_vidParamSet->dpb_parameters[0].dpb_max_dec_pic_buffering_minus1[m_seqParamSet->sps_max_sublayers_minus1] + 1 + m_reorderPicNum;
            }
        }
        return m_maxSurfNum;
    }

    void VVCSlice::ResetUnusedFrames(DPBType dpb)
    {
        VVCDecoderFrame* pTmp = nullptr;
        for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
        {
            pTmp = *it;
            if (!pTmp->isShortTermRef() && !pTmp->isLongTermRef() && pTmp->GetRefCounter() == 0
                && ((pTmp->IsOutputted() && pTmp->IsDisplayed() && pTmp->IsDisplayable())
                || pTmp->isUnavailableRefFrame() || !pTmp->m_pic_output))
            {
                pTmp->Reset();
            }
            // check frames with same POC and set prev one as unused
            else if (pTmp->PicOrderCnt() == m_sliceHeader.m_poc)
            {
                pTmp->SetisShortTermRef(false);
                pTmp->SetisLongTermRef(false);
                pTmp->setAsReferenced(false);
            }
        }
    }

    // For dependent slice copy data from another slice
    void VVCSlice::CopyFromBaseSlice(const VVCSlice * s, bool cpyFlag)
    {
        if (!s)
            return;

        assert(s);
        m_iNumber = s->m_iNumber;

        const VVCSliceHeader* slice = s->GetSliceHeader();

        m_sliceHeader.IdrPicFlag = slice->IdrPicFlag;
        m_sliceHeader.IDRflag = slice->IDRflag;
        m_sliceHeader.m_poc = slice->m_poc;
        m_sliceHeader.nal_unit_type = slice->nal_unit_type;
        m_sliceHeader.sliceQP = slice->sliceQP;
        m_sliceHeader.sh_cu_chroma_qp_offset_enabled_flag = slice->sh_cu_chroma_qp_offset_enabled_flag;
        m_sliceHeader.deblocking_filter_disable_flag = slice->deblocking_filter_disable_flag;
        m_sliceHeader.sh_deblocking_params_present_flag = slice->sh_deblocking_params_present_flag;
        m_sliceHeader.sh_luma_beta_offset_div2 = slice->sh_luma_beta_offset_div2;
        m_sliceHeader.sh_luma_tc_offset_div2 = slice->sh_luma_tc_offset_div2;
        m_sliceHeader.sh_cb_beta_offset_div2 = slice->sh_cb_beta_offset_div2;
        m_sliceHeader.sh_cb_tc_offset_div2 = slice->sh_cb_tc_offset_div2;
        m_sliceHeader.sh_cr_beta_offset_div2 = slice->sh_cr_beta_offset_div2;
        m_sliceHeader.sh_cr_tc_offset_div2 = slice->sh_cr_tc_offset_div2;
        m_sliceHeader.sh_dep_quant_used_flag = slice->sh_dep_quant_used_flag;
        m_sliceHeader.sh_sign_data_hiding_used_flag = slice->sh_sign_data_hiding_used_flag;
        m_sliceHeader.sh_ts_residual_coding_disabled_flag = slice->sh_ts_residual_coding_disabled_flag;
        m_sliceHeader.sh_ts_residual_coding_rice_idx_minus1 = slice->sh_ts_residual_coding_rice_idx_minus1;

        //TODO: m_riceBit/m_cnt_right_bottom
        m_sliceHeader.sh_reverse_last_sig_coeff_flag = slice->sh_reverse_last_sig_coeff_flag;

        for (uint8_t i = 0; i < NUM_REF_PIC_LIST_01; i++)
        {
            m_sliceHeader.num_ref_Idx[i] = slice->num_ref_Idx[i];
        }

        m_sliceHeader.m_checkLDC = slice->m_checkLDC;
        m_sliceHeader.NoBackwardPredFlag = slice->NoBackwardPredFlag;
        m_sliceHeader.slice_type = slice->slice_type;
        m_sliceHeader.sh_qp_delta = slice->sh_qp_delta;
        m_sliceHeader.sh_cb_qp_offset = slice->sh_cb_qp_offset;
        m_sliceHeader.sh_cr_qp_offset = slice->sh_cr_qp_offset;
        for (int32_t comp = 0; comp < MAX_NUM_COMPONENT; comp++)
        {
            m_sliceHeader.sh_chroma_qp_delta[comp] = slice->sh_chroma_qp_delta[comp];
        }
        m_sliceHeader.sh_chroma_qp_delta[JOINT_CbCr] = slice->sh_chroma_qp_delta[JOINT_CbCr];

        if (cpyFlag)
        {
            for (uint8_t i = 0; i < VVC_MAX_NUM_REF_PIC_LISTS; i++)
            {
                m_sliceHeader.sh_rpl[i] = slice->sh_rpl[i];
            }
        }
        m_picHeader = s->m_picHeader;
        m_sliceHeader.sh_collocated_from_l0_flag = slice->sh_collocated_from_l0_flag;
        m_sliceHeader.sh_collocated_ref_idx = slice->sh_collocated_ref_idx;
        m_sliceHeader.nuh_temporal_id = slice->nuh_temporal_id;
        m_sliceHeader.sh_lmcs_used_flag = slice->sh_lmcs_used_flag;
        m_sliceHeader.sh_explicit_scaling_list_used_flag = slice->sh_explicit_scaling_list_used_flag;

        m_sliceHeader.sh_sao_chroma_used_flag = slice->sh_sao_chroma_used_flag;
        m_sliceHeader.sh_sao_chroma_used_flag = slice->sh_sao_chroma_used_flag;
        m_sliceMap = s->m_sliceMap;

        m_sliceHeader.cabac_init_flag = slice->cabac_init_flag;
        MFX_INTERNAL_CPY(m_sliceHeader.alf_enabled_flag, slice->alf_enabled_flag, sizeof(slice->alf_enabled_flag));
        m_sliceHeader.num_alf_aps_ids_luma = slice->num_alf_aps_ids_luma;
        m_sliceHeader.alf_aps_ids_luma = slice->alf_aps_ids_luma;
        m_sliceHeader.alf_aps_id_chroma = slice->alf_aps_id_chroma;

        if (cpyFlag)
        {
            m_sliceHeader.enc_CABA_table_idx = slice->enc_CABA_table_idx;
        }
        m_sliceHeader.alf_cc_cb_enabled_flag = slice->alf_cc_cb_enabled_flag;
        m_sliceHeader.alf_cc_cr_enabled_flag = slice->alf_cc_cr_enabled_flag;
        m_sliceHeader.alf_cc_cb_aps_id = slice->alf_cc_cb_aps_id;
        m_sliceHeader.alf_cc_cr_aps_id = slice->alf_cc_cr_aps_id;
    }

    // Build reference lists from slice reference pic set. VVC spec 8.3.2 Construct Reference Picture List
    UMC::Status VVCSlice::ConstructReferenceList(DPBType dpb/*, VVCDecoderFrame* curr_ref*/)
    {
        UMC::Status s = UMC::UMC_OK;

        if (m_currentFrame == nullptr)
            return UMC::UMC_ERR_FAILED;

        // TODO: merge pRefPicList/pSHRefPicList to one general refPicList
        VVCReferencePictureList* pRefPicList0 = &m_rpl[REF_PIC_LIST_0];
        VVCReferencePictureList* pRefPicList1 = &m_rpl[REF_PIC_LIST_1];
        VVCReferencePictureList* pSHRefPicList0 = &m_sliceHeader.sh_rpl[REF_PIC_LIST_0];
        VVCReferencePictureList* pSHRefPicList1 = &m_sliceHeader.sh_rpl[REF_PIC_LIST_1];

        VVCDecoderFrame* pFrm = nullptr;

        pRefPicList0->num_ref_entries = pSHRefPicList0->num_ref_entries;
        pSHRefPicList0->num_ref_entries = m_sliceHeader.num_ref_Idx[REF_PIC_LIST_0];
        pRefPicList1->num_ref_entries = pSHRefPicList1->num_ref_entries;
        pSHRefPicList1->num_ref_entries = m_sliceHeader.num_ref_Idx[REF_PIC_LIST_1];

        if (m_sliceHeader.slice_type == I_SLICE)
            return s;

        // construct L0
        const int32_t layerIdx = (m_vidParamSet == nullptr ? 0 : m_vidParamSet->vps_layer_id[m_sliceHeader.nuh_layer_id]);
        for (uint8_t ii = 0; ii < pRefPicList0->num_ref_entries; ii++)
        {
            if (m_sliceHeader.sh_rpl[REF_PIC_LIST_0].is_inter_layer_ref_pic[ii])
            {
                int32_t refLayerId = (m_vidParamSet == nullptr ? 0 : m_vidParamSet->vps_layer_id[m_vidParamSet->m_directRefLayerIdx[layerIdx][m_sliceHeader.sh_rpl[REF_PIC_LIST_0].inter_layer_ref_pic_idx[ii]]]);
                pFrm = GetRefPic(dpb, m_sliceHeader.m_poc, refLayerId);

                if (pFrm)
                    pFrm->SetisLongTermRef(true);
                if (!pFrm)
                {
                    /* Reporting about missed reference */
                    m_currentFrame->SetErrorFlagged(UMC::ERROR_FRAME_REFERENCE_FRAME);
                    /* And because frame can not be decoded properly set flag "ERROR_FRAME_MAJOR" too*/
                    m_currentFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                }
            }
            else if (!m_sliceHeader.sh_rpl[REF_PIC_LIST_0].is_long_term_ref_pic[ii])
            {
                pFrm = GetRefPic(dpb, m_sliceHeader.m_poc + m_sliceHeader.sh_rpl[REF_PIC_LIST_0].ref_pic_identifier[ii], m_sliceHeader.nuh_layer_id);

                if (pFrm)
                    pFrm->SetisLongTermRef(false);
                if (!pFrm)
                {
                    /* Reporting about missed reference */
                    m_currentFrame->SetErrorFlagged(UMC::ERROR_FRAME_REFERENCE_FRAME);
                    /* And because frame can not be decoded properly set flag "ERROR_FRAME_MAJOR" too*/
                    m_currentFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                }
            }
            else
            {
                uint32_t pocBits = m_seqParamSet->sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
                int32_t pocMask = (1 << pocBits) - 1;
                int32_t ltrpPoc = m_sliceHeader.sh_rpl[REF_PIC_LIST_0].ref_pic_identifier[ii] & pocMask;
                if (m_sliceHeader.sh_rpl[REF_PIC_LIST_0].delta_poc_msb_present_flag[ii])
                {
                    ltrpPoc += m_sliceHeader.m_poc - m_sliceHeader.sh_rpl[REF_PIC_LIST_0].delta_poc_msb_cycle_lt[ii] * (pocMask + 1) - (m_sliceHeader.m_poc & pocMask);
                }
                pFrm = GetLongTermRefPicCandidate(dpb, ltrpPoc, pocBits, m_sliceHeader.sh_rpl[REF_PIC_LIST_0].delta_poc_msb_present_flag[ii], m_sliceHeader.nuh_layer_id);

                if(pFrm)
                    pFrm->SetisLongTermRef(true);
                if (!pFrm)
                {
                    /* Reporting about missed reference */
                    m_currentFrame->SetErrorFlagged(UMC::ERROR_FRAME_REFERENCE_FRAME);
                    /* And because frame can not be decoded properly set flag "ERROR_FRAME_MAJOR" too*/
                    m_currentFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                }
            }
            if (ii < pSHRefPicList0->num_ref_entries && pFrm)
            {
                pRefPicList0->refFrame = pFrm;
                pRefPicList0->isLongReference[ii] = pFrm->isLongTermRef();
                pRefPicList0->POC[ii] = pFrm->PicOrderCnt();
                pSHRefPicList0->refFrame = pFrm;
                pSHRefPicList0->isLongReference[ii] = pFrm->isLongTermRef();
                pSHRefPicList0->POC[ii] = pFrm->PicOrderCnt();
            }
        }

        if (m_sliceHeader.slice_type == P_SLICE)
            return s;

        // construct L1
        for (uint8_t ii = 0; ii < pRefPicList1->num_ref_entries; ii++)
        {
            if (m_sliceHeader.sh_rpl[REF_PIC_LIST_1].is_inter_layer_ref_pic[ii])
            {
                int32_t refLayerId = (m_vidParamSet == nullptr ? 0 : m_vidParamSet->vps_layer_id[m_vidParamSet->m_directRefLayerIdx[layerIdx][m_sliceHeader.sh_rpl[REF_PIC_LIST_1].inter_layer_ref_pic_idx[ii]]]);
                pFrm = GetRefPic(dpb, m_sliceHeader.m_poc, refLayerId);

                if (pFrm)
                    pFrm->SetisLongTermRef(true);
                if (!pFrm)
                {
                    /* Reporting about missed reference */
                    m_currentFrame->SetErrorFlagged(UMC::ERROR_FRAME_REFERENCE_FRAME);
                    /* And because frame can not be decoded properly set flag "ERROR_FRAME_MAJOR" too*/
                    m_currentFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                }
            }
            else if (!m_sliceHeader.sh_rpl[REF_PIC_LIST_1].is_long_term_ref_pic[ii])
            {
                pFrm = GetRefPic(dpb, m_sliceHeader.m_poc + m_sliceHeader.sh_rpl[REF_PIC_LIST_1].ref_pic_identifier[ii], m_sliceHeader.nuh_layer_id);

                if (pFrm)
                    pFrm->SetisLongTermRef(false);
                if (!pFrm)
                {
                    /* Reporting about missed reference */
                    m_currentFrame->SetErrorFlagged(UMC::ERROR_FRAME_REFERENCE_FRAME);
                    /* And because frame can not be decoded properly set flag "ERROR_FRAME_MAJOR" too*/
                    m_currentFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                }
            }
            else
            {
                uint32_t pocBits = m_seqParamSet->sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
                int32_t pocMask = (1 << pocBits) - 1;
                int32_t ltrpPoc = m_sliceHeader.sh_rpl[REF_PIC_LIST_1].ref_pic_identifier[ii] & pocMask;
                if (m_sliceHeader.sh_rpl[REF_PIC_LIST_1].delta_poc_msb_present_flag[ii])
                {
                    ltrpPoc += m_sliceHeader.m_poc - m_sliceHeader.sh_rpl[REF_PIC_LIST_1].delta_poc_msb_cycle_lt[ii] * (pocMask + 1) - (m_sliceHeader.m_poc & pocMask);
                }
                pFrm = GetLongTermRefPicCandidate(dpb, ltrpPoc, pocBits, m_sliceHeader.sh_rpl[REF_PIC_LIST_1].delta_poc_msb_present_flag[ii], m_sliceHeader.nuh_layer_id);

                if (pFrm)
                    pFrm->SetisLongTermRef(true);
                if (!pFrm)
                {
                    /* Reporting about missed reference */
                    m_currentFrame->SetErrorFlagged(UMC::ERROR_FRAME_REFERENCE_FRAME);
                    /* And because frame can not be decoded properly set flag "ERROR_FRAME_MAJOR" too*/
                    m_currentFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                }
            }
            if (ii < pSHRefPicList1->num_ref_entries && pFrm)
            {
                pRefPicList1->refFrame = pFrm;
                pRefPicList1->isLongReference[ii] = pFrm->isLongTermRef();
                pRefPicList1->POC[ii] = pFrm->PicOrderCnt();
                pSHRefPicList1->refFrame = pFrm;
                pSHRefPicList1->isLongReference[ii] = pFrm->isLongTermRef();
                pSHRefPicList1->POC[ii] = pFrm->PicOrderCnt();
            }
        }
        return s;
    }

    void VVCSlice::checkCRA(DPBType dpb, const int pocCRA)
    {
        if (GetRapPicFlag() && !m_picParamSet->pps_mixed_nalu_types_in_pic_flag)
        {
            return;
        }

        if (m_sliceHeader.m_poc > pocCRA)
        {
            uint32_t numRefPic = m_rpl[REF_PIC_LIST_0].number_of_short_term_pictures + m_rpl[REF_PIC_LIST_0].number_of_long_term_pictures;
            for (uint32_t i = 0; i < numRefPic; i++)
            {
                if (!m_rpl[REF_PIC_LIST_0].inter_layer_ref_pic_flag[i])
                {
                    uint32_t pocBits = m_seqParamSet->sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
                    int32_t pocMask = (1 << pocBits) - 1;
                    int32_t ltrpPoc = m_sliceHeader.sh_rpl[REF_PIC_LIST_0].ref_pic_identifier[i] & pocMask;
                    if (m_sliceHeader.sh_rpl[REF_PIC_LIST_0].delta_poc_msb_present_flag[i])
                    {
                        ltrpPoc += m_sliceHeader.m_poc - m_sliceHeader.sh_rpl[REF_PIC_LIST_0].delta_poc_msb_cycle_lt[i] * (pocMask + 1) - (m_sliceHeader.m_poc & pocMask);
                    }
                    const VVCDecoderFrame *ltrp = GetLongTermRefPic(dpb, ltrpPoc, pocBits, m_sliceHeader.sh_rpl[REF_PIC_LIST_0].delta_poc_msb_present_flag[i], m_sliceHeader.nuh_layer_id);
                    if (!ltrp || ltrp->PicOrderCnt() < pocCRA)
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
            }
            numRefPic = m_rpl[REF_PIC_LIST_1].number_of_short_term_pictures + m_rpl[REF_PIC_LIST_1].number_of_long_term_pictures;
            for (uint32_t i = 0; i < numRefPic; i++)
            {
                if (!m_rpl[REF_PIC_LIST_1].inter_layer_ref_pic_flag[i])
                {
                    uint32_t pocBits = m_seqParamSet->sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
                    int32_t pocMask = (1 << pocBits) - 1;
                    int32_t ltrpPoc = m_sliceHeader.sh_rpl[REF_PIC_LIST_1].ref_pic_identifier[i] & pocMask;
                    if (m_sliceHeader.sh_rpl[REF_PIC_LIST_1].delta_poc_msb_present_flag[i])
                    {
                        ltrpPoc += m_sliceHeader.m_poc - m_sliceHeader.sh_rpl[REF_PIC_LIST_1].delta_poc_msb_cycle_lt[i] * (pocMask + 1) - (m_sliceHeader.m_poc & pocMask);
                    }
                    const VVCDecoderFrame* ltrp = GetLongTermRefPic(dpb, ltrpPoc, pocBits, m_sliceHeader.sh_rpl[REF_PIC_LIST_1].delta_poc_msb_present_flag[i], m_sliceHeader.nuh_layer_id);
                    if (!ltrp || ltrp->PicOrderCnt() < pocCRA)
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
            }
        }
    }

    bool VVCSlice::GetRapPicFlag() const
    {
        return GetSliceHeader()->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL
            || GetSliceHeader()->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_N_LP;
    }

    void VVCSlice::SetDefaultClpRng(const VVCSeqParamSet& sps)
    {
        m_clpRngs.comp[COMPONENT_Y].min = m_clpRngs.comp[COMPONENT_Cb].min = m_clpRngs.comp[COMPONENT_Cr].min = 0;
        m_clpRngs.comp[COMPONENT_Y].max = (1 << (sps.sps_bitdepth_minus8 + 8)) - 1;
        m_clpRngs.comp[COMPONENT_Y].bd = sps.sps_bitdepth_minus8 + 8;
        m_clpRngs.comp[COMPONENT_Y].n = 0;
        m_clpRngs.comp[COMPONENT_Cb].max = m_clpRngs.comp[COMPONENT_Cr].max = (1 << (sps.sps_bitdepth_minus8 + 8)) - 1;
        m_clpRngs.comp[COMPONENT_Cb].bd = m_clpRngs.comp[COMPONENT_Cr].bd = (sps.sps_bitdepth_minus8 + 8);
        m_clpRngs.comp[COMPONENT_Cb].n = m_clpRngs.comp[COMPONENT_Cr].n = 0;
        m_clpRngs.used = m_clpRngs.chroma = false;
    }

    void VVCSlice::SetNumSubstream(const VVCSeqParamSet* sps, const VVCPicParamSet* pps)
    {
        uint32_t ctuAddr, ctuX, ctuY;
        m_numSubstream = 0;

        // count the number of CTUs that align with either the start of a tile, or with an entropy coding sync point
        // ignore the first CTU since it doesn't count as an entry point
        for (uint32_t i = 1; i < m_sliceMap.num_ctu_in_slice; i++)
        {
            ctuAddr = m_sliceMap.ctu_addr_in_slice[i];
            ctuX = (ctuAddr % pps->pps_pic_width_in_ctu);
            ctuY = (ctuAddr / pps->pps_pic_width_in_ctu);

            if (pps->pps_ctu_to_tile_col[ctuX] >= pps->pps_tile_col_bd.size()
                || (pps->pps_ctu_to_tile_row[ctuY] >= pps->pps_tile_row_bd.size())) 
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }

            if (ctuX == pps->pps_tile_col_bd[pps->pps_ctu_to_tile_col[ctuX]]
                && (ctuY == pps->pps_tile_row_bd[pps->pps_ctu_to_tile_row[ctuY]]
                    || sps->sps_entropy_coding_sync_enabled_flag)) 
            {
                m_numSubstream++;
            }
        }
    }

    void VVCSlice::SetNumEntryPoints(const VVCSeqParamSet* sps, const VVCPicParamSet* pps)
    {
        uint32_t ctuAddr, ctuX, ctuY;
        uint32_t prevCtuAddr, prevCtuX, prevCtuY;
        m_numEntryPoints = 0;

        if (!sps->sps_entry_point_offsets_present_flag)
        {
            return;
        }

        // count the number of CTUs that align with either the start of a tile, or with an entropy coding sync point
        // ignore the first CTU since it doesn't count as an entry point
        for (uint32_t i = 1; i < m_sliceMap.num_ctu_in_slice; i++)
        {
            ctuAddr = m_sliceMap.ctu_addr_in_slice[i];
            ctuX = (ctuAddr % pps->pps_pic_width_in_ctu);
            ctuY = (ctuAddr / pps->pps_pic_width_in_ctu);
            prevCtuAddr = m_sliceMap.ctu_addr_in_slice[i - 1];
            prevCtuX = (prevCtuAddr % pps->pps_pic_width_in_ctu);
            prevCtuY = (prevCtuAddr / pps->pps_pic_width_in_ctu);

            if (pps->pps_ctu_to_tile_col[ctuX] >= pps->pps_tile_col_bd.size()
                || (pps->pps_ctu_to_tile_row[ctuY] >= pps->pps_tile_row_bd.size()))
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }

            if ((pps->pps_tile_col_bd[pps->pps_ctu_to_tile_col[ctuX]] != pps->pps_tile_col_bd[pps->pps_ctu_to_tile_col[prevCtuX]])
                || (pps->pps_tile_row_bd[pps->pps_ctu_to_tile_row[ctuY]] != pps->pps_tile_row_bd[pps->pps_ctu_to_tile_row[prevCtuY]])
                || (ctuY != prevCtuY && sps->sps_entropy_coding_sync_enabled_flag))
            {
                m_numEntryPoints++;
            }
        }
    }


    // RPS data structure constructor
    // ...

} // namespace UMC_VVC_DECODER
#endif // MFX_ENABLE_VVC_VIDEO_DECODE
