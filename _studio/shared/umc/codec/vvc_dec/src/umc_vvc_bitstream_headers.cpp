// Copyright (c) 2022-2023 Intel Corporation
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

#include "umc_vvc_bitstream_headers.h"
#include "umc_vvc_slice_decoding.h"

namespace UMC_VVC_DECODER
{
/**********************************************************************************/
// VVCScanOrderTables class implementation
/**********************************************************************************/

    const uint32_t VVCScanOrderTables::m_blkWidths[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };
    const uint32_t VVCScanOrderTables::m_blkHeights[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };
    const uint32_t VVCScanOrderTables::m_blkWidthSize = 8;
    const uint32_t VVCScanOrderTables::m_blkHeightSize = 8;

    VVCScanOrderTables::VVCScanOrderTables()
    {
        InitScanOrderTables();
    }

    VVCScanOrderTables::~VVCScanOrderTables()
    {
        DestroyScanOrderTables();
    }

    uint32_t VVCScanOrderTables::GetBLKWidthIndex(uint32_t blkWidth)
    {
        for (uint32_t i = 0; i < GetBLKWidthSize(); i++)
        {
            if (blkWidth == m_blkWidths[i])
                return i;
        }

        return 0;
    }

    uint32_t VVCScanOrderTables::GetBLKHeightIndex(uint32_t blkHeight)
    {
        for (uint32_t i = 0; i < GetBLKHeightSize(); i++)
        {
            if (blkHeight == m_blkHeights[i])
                return i;
        }

        return 0;
    }

    inline ScanElement* VVCScanOrderTables::GetScanOrder(CoeffScanGroupType scanGType, CoeffScanType scanType, uint32_t blkWidthIndex, uint32_t blkHeightIndex) const
    {
        return m_scanOrders[scanGType][scanType][blkWidthIndex][blkHeightIndex];
    }

    void VVCScanOrderTables::InitScanOrderTables()
    {
        // SCAN_UNGROUPED
        // SCAN_DIAG
        for (uint32_t blkWidthIdx = 0; blkWidthIdx < VVCScanOrderTables::GetBLKWidthSize(); blkWidthIdx++)
        {
            for (uint32_t blkHeightIdx = 0; blkHeightIdx < VVCScanOrderTables::GetBLKHeightSize(); blkHeightIdx++)
            {
                uint32_t blkWidth = VVCScanOrderTables::m_blkWidths[blkWidthIdx];
                uint32_t blkHeight = VVCScanOrderTables::m_blkHeights[blkHeightIdx];
                uint32_t totalValues = blkWidth * blkHeight;
                ScanElement* scan = new ScanElement[totalValues];
                m_scanOrders[SCAN_UNGROUPED][SCAN_DIAG][blkWidthIdx][blkHeightIdx] = scan;
                ScanElement* scanEnd = scan + totalValues;

                uint32_t x = 0, y = 0;
                scan->idx = 0;
                scan->x = 0;
                scan->y = 0;
                scan++;

                while (scan < scanEnd)
                {
                    if (y == 0 || x == blkWidth - 1)
                    {
                        y += x + 1;
                        x = 0;

                        if (y >= blkHeight)
                        {
                            x += y - (blkHeight - 1);
                            y = blkHeight - 1;
                        }
                    }
                    else
                    {
                        y--;
                        x++;
                    }

                    scan->idx = y * blkWidth + x;
                    scan->x = x;
                    scan->y = y;
                    scan++;
                }
            }
        }

        // SCAN_TRAV_HOR
        for (uint32_t blkWidthIdx = 0; blkWidthIdx < VVCScanOrderTables::GetBLKWidthSize(); blkWidthIdx++)
        {
            for (uint32_t blkHeightIdx = 0; blkHeightIdx < VVCScanOrderTables::GetBLKHeightSize(); blkHeightIdx++)
            {
                uint32_t blkWidth = VVCScanOrderTables::m_blkWidths[blkWidthIdx];
                uint32_t blkHeight = VVCScanOrderTables::m_blkHeights[blkHeightIdx];
                uint32_t totalValues = blkWidth * blkHeight;
                ScanElement* scan = new ScanElement[totalValues];
                m_scanOrders[SCAN_UNGROUPED][SCAN_TRAV_HOR][blkWidthIdx][blkHeightIdx] = scan;

                // TODO:
            }
        }

        // SCAN_TRAV_VER
        for (uint32_t blkWidthIdx = 0; blkWidthIdx < VVCScanOrderTables::GetBLKWidthSize(); blkWidthIdx++)
        {
            for (uint32_t blkHeightIdx = 0; blkHeightIdx < VVCScanOrderTables::GetBLKHeightSize(); blkHeightIdx++)
            {
                uint32_t blkWidth = VVCScanOrderTables::m_blkWidths[blkWidthIdx];
                uint32_t blkHeight = VVCScanOrderTables::m_blkHeights[blkHeightIdx];
                uint32_t totalValues = blkWidth * blkHeight;
                ScanElement* scan = new ScanElement[totalValues];
                m_scanOrders[SCAN_UNGROUPED][SCAN_TRAV_VER][blkWidthIdx][blkHeightIdx] = scan;

                // TODO:
            }
        }

        // SCAN_GROUPED_4x4
        // TODO:
    }

    void VVCScanOrderTables::DestroyScanOrderTables()
    {
        for (uint32_t gIdx = 0; gIdx < SCAN_NUMBER_OF_GROUP_TYPES; gIdx++)
        {
            for (uint32_t sIdx = 0; sIdx < SCAN_NUMBER_OF_TYPES; sIdx++)
            {
                for (uint32_t blkWidthIdx = 0; blkWidthIdx < VVC_MAX_CU_SIZE / 2 + 1; blkWidthIdx++)
                {
                    for (uint32_t blkHeightIdx = 0; blkHeightIdx < VVC_MAX_CU_SIZE / 2 + 1; blkHeightIdx++)
                    {
                        delete[] m_scanOrders[gIdx][sIdx][blkWidthIdx][blkHeightIdx];
                        m_scanOrders[gIdx][sIdx][blkWidthIdx][blkHeightIdx] = nullptr;
                    }
                }
            }
        }
    }

/**********************************************************************************/
// VVCBitstreamUtils class implementation
/**********************************************************************************/

    VVCBitstreamUtils::VVCBitstreamUtils()
    {
        Reset(0, 0);
    }

    VVCBitstreamUtils::VVCBitstreamUtils(uint8_t * const pb, const uint32_t maxsize)
    {
        Reset(pb, maxsize);
    }

    VVCBitstreamUtils::~VVCBitstreamUtils()
    {
    }

    // Reset the bitstream with new data pointer
    void VVCBitstreamUtils::Reset(uint8_t * const pb, const uint32_t maxsize)
    {
        m_pbs       = (uint32_t*)pb;
        m_pbsBase   = (uint32_t*)pb;
        m_bitOffset = 31;
        m_maxBsSize = maxsize;
    }

    // Reset the bitstream with new data pointer and bit offset
    void VVCBitstreamUtils::Reset(uint8_t * const pb, int32_t offset, const uint32_t maxsize)
    {
        m_pbs       = (uint32_t*)pb;
        m_pbsBase   = (uint32_t*)pb;
        m_bitOffset = offset;
        m_maxBsSize = maxsize;
    }

    // Return number of decoded bytes since last reset
    size_t VVCBitstreamUtils::BytesDecoded() const
    {
        return static_cast<size_t>((uint8_t*)m_pbs - (uint8_t*)m_pbsBase) +
            ((31 - m_bitOffset) >> 3);
    }

    // Return number of decoded bits since last reset
    size_t VVCBitstreamUtils::BitsDecoded() const
    {
        return static_cast<size_t>((uint8_t*)m_pbs - (uint8_t*)m_pbsBase) * 8 +
            (31 - m_bitOffset);
    }

    size_t VVCBitstreamUtils::BytesLeft() const
    {
        return (int32_t)m_maxBsSize - (int32_t)BytesDecoded();
    }

    // Return bitstream array base address and size
    void VVCBitstreamUtils::GetOrg(uint32_t** pbs, uint32_t* size) const
    {
        *pbs = m_pbsBase;
        *size = m_maxBsSize;
    }

    // Read N bits from bitstream array
    inline uint32_t VVCBitstreamUtils::GetBits(const uint32_t nbits)
    {
        uint32_t w, n = nbits;

        VVCGetNBits(m_pbs, m_bitOffset, n, w);
        return(w);
    }

    // Read N bits from bitstream array
    template <uint32_t nbits>
    inline uint32_t VVCBitstreamUtils::GetPredefinedBits()
    {
        uint32_t w, n = nbits;

        VVCGetNBits(m_pbs, m_bitOffset, n, w);
        return(w);
    }

    inline bool DecodeExpGolombOne_VVC_1u32s (uint32_t **ppBitStream,
                                                int32_t *pBitOffset,
                                                int32_t *pDst,
                                                int32_t isSigned)
    {
        uint32_t code;
        uint32_t info     = 0;
        int32_t length   = 1;            /* for first bit read above*/
        uint32_t thisChunksLength = 0;
        uint32_t sval;

        /* check error(s) */

        /* Fast check for element = 0 */
        VVCGetNBits((*ppBitStream), (*pBitOffset), 1, code)
        if (code)
        {
            *pDst = 0;
            return true;
        }

        VVCGetNBits((*ppBitStream), (*pBitOffset), 8, code);
        length += 8;

        /* find nonzero byte */
        while (code == 0 && 32 > length)
        {
            VVCGetNBits((*ppBitStream), (*pBitOffset), 8, code);
            length += 8;
        }

        /* find leading '1' */
        while ((code & 0x80) == 0 && 32 > thisChunksLength)
        {
            code <<= 1;
            thisChunksLength++;
        }
        length -= 8 - thisChunksLength;

        VVCUngetNBits((*ppBitStream), (*pBitOffset),8 - (thisChunksLength + 1))

        /* skipping very long codes, let's assume what the code is corrupted */
        if (32 <= length || 32 <= thisChunksLength)
        {
            uint32_t dwords;
            length -= (*pBitOffset + 1);
            dwords = length/32;
            length -= (32*dwords);
            *ppBitStream += (dwords + 1);
            *pBitOffset = 31 - length;
            *pDst = 0;
            return false;
        }

        /* Get info portion of codeword */
        if (length)
        {
            VVCGetNBits((*ppBitStream), (*pBitOffset),length, info)
        }

        sval = ((1 << (length)) + (info) - 1);
        if (isSigned)
        {
            if (sval & 1)
                *pDst = (int32_t) ((sval + 1) >> 1);
            else
                *pDst = -((int32_t) (sval >> 1));
        }
        else
            *pDst = (int32_t) sval;

        return true;
    }

    // Read variable length coded unsigned element
    inline uint32_t VVCBitstreamUtils::GetVLCElementU()
    {
        int32_t sval = 0;

        bool res = DecodeExpGolombOne_VVC_1u32s(&m_pbs, &m_bitOffset, &sval, false);

        if (!res)
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);

        return (uint32_t)sval;
    }

    // Read variable length coded signed element
    inline int32_t VVCBitstreamUtils::GetVLCElementS()
    {
        int32_t sval = 0;

        bool res = DecodeExpGolombOne_VVC_1u32s(&m_pbs, &m_bitOffset, &sval, true);

        if (!res)
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);

        return sval;
    }

    // Read one bit
    inline uint8_t VVCBitstreamUtils::Get1Bit()
    {
        uint32_t w;

        GetBits1(m_pbs, m_bitOffset, w);
        return (uint8_t)w;
    }

    // Return number of bits needed for byte alignment
    inline unsigned VVCBitstreamUtils::GetNumBitsUntilByteAligned() const
    {
        return ((m_bitOffset + 1) % 8);
    }

    // Align bitstream to byte boundary
    inline void VVCBitstreamUtils::ReadOutTrailingBits()
    {
        Get1Bit();

        uint32_t bits = GetNumBitsUntilByteAligned();

        if (bits)
        {
            GetBits(bits);
        }
    }

    // Align bitstream position to byte boundary
    inline void VVCBitstreamUtils::AlignPointerRight(void)
    {
        if ((m_bitOffset & 0x07) != 0x07)
        {
            m_bitOffset = (m_bitOffset | 0x07) - 8;
            if (m_bitOffset == -1)
            {
                m_bitOffset = 31;
                m_pbs++;
            } 
        }
    }

    void VVCBitstreamUtils::GetState(uint32_t** pbs, uint32_t* bitOffset)
    {
        *pbs = m_pbs;
        *bitOffset = m_bitOffset;
    }

    // Set current bitstream address and bit offset
    void VVCBitstreamUtils::SetState(uint32_t* pbs, uint32_t bitOffset)
    {
        m_pbs = pbs;
        m_bitOffset = bitOffset;
    }

    // Check if bitstream position has moved outside the limit
    bool VVCBitstreamUtils::CheckBSLeft()
    {
        size_t bitsDecoded = BitsDecoded();
        return (bitsDecoded > m_maxBsSize * 8);
    }

/*********************************************************************************/
// VVCHeadersBitstream class implementation
/*********************************************************************************/

    VVCHeadersBitstream::VVCHeadersBitstream()
    : VVCBitstreamUtils()
    {
    }

    VVCHeadersBitstream::VVCHeadersBitstream(uint8_t * const pb, const uint32_t maxsize)
        : VVCBitstreamUtils(pb, maxsize)
    {
    }

    UMC::Status VVCHeadersBitstream::GetNALUnitType(NalUnitType &nal_unit_type, uint32_t &nuh_temporal_id, uint32_t &nuh_layer_id)
    {
        uint32_t forbidden_zero_bit = Get1Bit();
        if (forbidden_zero_bit)
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);

        uint32_t nuh_reserved_zero_bit = Get1Bit();
        (void)nuh_reserved_zero_bit;

        nuh_layer_id = GetBits(6);

        if (nuh_layer_id > 55) // nuh_layer_id shall be in the range of 0 to 55, inclusive
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);

        nal_unit_type = (NalUnitType)GetBits(5);

        uint32_t const nuh_temporal_id_plus1 = GetBits(3);
        if (!nuh_temporal_id_plus1)
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);

        nuh_temporal_id = nuh_temporal_id_plus1 - 1;

        return UMC::UMC_OK;
    }

    void VVCHeadersBitstream::xInitTiles(VVCPicParamSet* pPps)
    {
        uint32_t colIdx = 0;
        uint32_t rowIdx = 0;
        uint32_t ctuX = 0;
        uint32_t ctuY = 0;
        uint32_t remainingWidthInCtu = pPps->pps_pic_width_in_ctu;

        for (colIdx = 0; colIdx <= pPps->pps_num_exp_tile_columns_minus1; colIdx++)
        {
            if (pPps->pps_tile_column_width[colIdx] > remainingWidthInCtu)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            remainingWidthInCtu -= pPps->pps_tile_column_width[colIdx];
        }
        uint32_t uniformTileColWidth = pPps->pps_tile_column_width[colIdx - 1];
        while (remainingWidthInCtu > 0)
        {
            uniformTileColWidth = std::min(remainingWidthInCtu, uniformTileColWidth);
            pPps->pps_tile_column_width.push_back(uniformTileColWidth);
            remainingWidthInCtu -= uniformTileColWidth;
            colIdx++;
        }
        pPps->pps_num_tile_cols = colIdx;
        uint32_t remainingHeightInCtu = pPps->pps_pic_height_in_ctu;
        for (rowIdx = 0; rowIdx <= pPps->pps_num_exp_tile_rows_minus1; rowIdx++)
        {
            if (pPps->pps_tile_row_height[rowIdx] > remainingHeightInCtu)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            remainingHeightInCtu -= pPps->pps_tile_row_height[rowIdx];
        }
        uint32_t uniformTileRowHeight = pPps->pps_tile_row_height[rowIdx - 1];
        while (remainingHeightInCtu > 0)
        {
            uniformTileRowHeight = std::min(remainingHeightInCtu, uniformTileRowHeight);
            pPps->pps_tile_row_height.push_back(uniformTileRowHeight);
            remainingHeightInCtu -= uniformTileRowHeight;
            rowIdx++;
        }
        pPps->pps_num_tile_rows = rowIdx;
        pPps->pps_tile_col_bd.push_back(0);
        for (colIdx = 0; colIdx < pPps->pps_num_tile_cols; colIdx++)
        {
            pPps->pps_tile_col_bd.push_back(pPps->pps_tile_col_bd[colIdx] + pPps->pps_tile_column_width[colIdx]);
        }
        pPps->pps_tile_row_bd.push_back(0);
        for (rowIdx = 0; rowIdx < pPps->pps_num_tile_rows; rowIdx++)
        {
            pPps->pps_tile_row_bd.push_back(pPps->pps_tile_row_bd[rowIdx] + pPps->pps_tile_row_height[rowIdx]);
        }
        colIdx = 0;
        for (ctuX = 0; ctuX <= pPps->pps_pic_width_in_ctu; ctuX++)
        {
            if (ctuX == pPps->pps_tile_col_bd[colIdx + 1])
            {
                colIdx++;
            }
            pPps->pps_ctu_to_tile_col.push_back(colIdx);
        }
        rowIdx = 0;
        for (ctuY = 0; ctuY <= pPps->pps_pic_height_in_ctu; ctuY++)
        {
            if (ctuY == pPps->pps_tile_row_bd[rowIdx + 1])
            {
                rowIdx++;
            }
            pPps->pps_ctu_to_tile_row.push_back(rowIdx);
        }
    }

    UMC::Status VVCHeadersBitstream::GetPictureParamSet(VVCPicParamSet *pPps)
    {
        if (!pPps)
        {
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);
        }
        pPps->Reset();
        UMC::Status sts = UMC::UMC_OK;
        pPps->pps_subpic_id_len = VVC_MAX_PPS_SUBPIC_ID_LEN;
        pPps->pps_pic_parameter_set_id = GetBits(6);
        if (pPps->pps_pic_parameter_set_id > VVC_MAX_PIC_PARAMETER_SET_ID)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }

        pPps->pps_seq_parameter_set_id = GetBits(4);
        if (pPps->pps_seq_parameter_set_id > VVC_MAX_SEQ_PARAMETER_SET_ID)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pPps->pps_mixed_nalu_types_in_pic_flag = Get1Bit();
        pPps->pps_pic_width_in_luma_samples = GetVLCElementU();
        pPps->pps_pic_height_in_luma_samples = GetVLCElementU();
        pPps->pps_conformance_window_flag = Get1Bit();
        if (pPps->pps_conformance_window_flag)
        {
            pPps->pps_conf_win_left_offset = GetVLCElementU();
            pPps->pps_conf_win_right_offset = GetVLCElementU();
            pPps->pps_conf_win_top_offset = GetVLCElementU();
            pPps->pps_conf_win_bottom_offset = GetVLCElementU();
        }
        pPps->pps_scaling_window_explicit_signalling_flag = Get1Bit();
        if (pPps->pps_scaling_window_explicit_signalling_flag)
        {
            pPps->pps_scaling_win_left_offset = GetVLCElementS();
            pPps->pps_scaling_win_right_offset = GetVLCElementS();
            pPps->pps_scaling_win_top_offset = GetVLCElementS();
            pPps->pps_scaling_win_bottom_offset = GetVLCElementS();
        }
        else
        {
            pPps->pps_scaling_win_left_offset = pPps->pps_conf_win_left_offset;
            pPps->pps_scaling_win_right_offset = pPps->pps_conf_win_right_offset;
            pPps->pps_scaling_win_top_offset = pPps->pps_conf_win_top_offset;
            pPps->pps_scaling_win_bottom_offset = pPps->pps_conf_win_bottom_offset;
        }
        pPps->pps_output_flag_present_flag = Get1Bit();
        pPps->pps_no_pic_partition_flag = Get1Bit();
        pPps->pps_subpic_id_mapping_present_flag = Get1Bit();
        pPps->pps_num_subpics = 1;
        pPps->pps_loop_filter_across_tiles_enabled_flag = true;
        if (pPps->pps_subpic_id_mapping_present_flag)
        {
            if (!pPps->pps_no_pic_partition_flag)
            {
                pPps->pps_num_subpics = GetVLCElementU() + 1;
            }
            else
            {
                pPps->pps_num_subpics = 1;
            }
            pPps->pps_subpic_id.resize(pPps->pps_num_subpics);
            if (pPps->pps_num_subpics > VVC_MAX_NUM_SUB_PICS)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            pPps->pps_subpic_id_len = GetVLCElementU() + 1;
            if (pPps->pps_subpic_id_len > VVC_MAX_PPS_SUBPIC_ID_LEN)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            if ((uint32_t)(1 << (pPps->pps_subpic_id_len)) < pPps->pps_num_subpics)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            for (uint32_t i = 0; i < pPps->pps_num_subpics; i++)
            {
                pPps->pps_subpic_id[i] = GetBits(pPps->pps_subpic_id_len);
            }
        }
        for (uint32_t i = 0; i < pPps->pps_num_slices_in_pic_minus1 + 1; i++)
        {
            pPps->pps_rect_slices.push_back(new VVCRectSlice());
        }
        pPps->pps_tile_column_width.resize(pPps->pps_num_exp_tile_columns_minus1 + 1);
        pPps->pps_tile_row_height.resize(pPps->pps_num_exp_tile_rows_minus1 + 1);
        if (!pPps->pps_no_pic_partition_flag)
        {
            xResetTileSliceInfo(pPps);
            pPps->pps_log2_ctu_size = GetBits(2) + 5;
            pPps->pps_ctu_size = 1 << pPps->pps_log2_ctu_size;
            pPps->pps_pic_width_in_ctu = (pPps->pps_pic_width_in_luma_samples + pPps->pps_ctu_size - 1) /
                                         pPps->pps_ctu_size;
            pPps->pps_pic_height_in_ctu = (pPps->pps_pic_height_in_luma_samples + pPps->pps_ctu_size - 1) /
                                          pPps->pps_ctu_size;
            if (pPps->pps_log2_ctu_size > VVC_MAX_PPS_LOG2_CTU_SIZE)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            pPps->pps_num_exp_tile_columns_minus1 = GetVLCElementU();
            pPps->pps_num_exp_tile_rows_minus1 = GetVLCElementU();
            if (pPps->pps_num_exp_tile_columns_minus1 + 1 > VVC_MAX_TILE_COLS)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            uint32_t code = 0;
            for (uint32_t i = 0; i <= pPps->pps_num_exp_tile_columns_minus1; i++)
            {
                code = GetVLCElementU();
                pPps->pps_tile_column_width.push_back(code + 1);
                if (code > pPps->pps_pic_width_in_ctu - 1)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
            }
            for (uint32_t i = 0; i <= pPps->pps_num_exp_tile_rows_minus1; i++)
            {
                code = GetVLCElementU();
                pPps->pps_tile_row_height.push_back(code + 1);
                if (code > pPps->pps_pic_height_in_ctu - 1)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
            }
            xInitTiles(pPps);
            pPps->pps_num_tiles = pPps->pps_num_tile_cols * pPps->pps_num_tile_rows;
            if (pPps->pps_num_tiles > 1)
            {
                pPps->pps_loop_filter_across_tiles_enabled_flag = Get1Bit();
                pPps->pps_rect_slice_flag = Get1Bit();
            }
            else
            {
                pPps->pps_loop_filter_across_tiles_enabled_flag = false;
                pPps->pps_rect_slice_flag = true;
            }
            if (pPps->pps_rect_slice_flag)
            {
                pPps->pps_single_slice_per_subpic_flag = Get1Bit();
            }
            else
            {
                pPps->pps_single_slice_per_subpic_flag = false;
            }
            if (pPps->pps_rect_slice_flag && !pPps->pps_single_slice_per_subpic_flag)
            {
                uint32_t tileIdx = 0;

                pPps->pps_num_slices_in_pic_minus1 = GetVLCElementU();
                if (pPps->pps_num_slices_in_pic_minus1 + 1 > VVC_MAX_SLICES)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                if (pPps->pps_num_slices_in_pic_minus1 > 1)
                {
                    pPps->pps_tile_idx_delta_present_flag = Get1Bit();
                }
                else
                {
                    pPps->pps_tile_idx_delta_present_flag = false;
                }
                for (uint32_t i = 0; i < pPps->pps_num_slices_in_pic_minus1 + 1; i++)
                {
                    pPps->pps_rect_slices.push_back(new VVCRectSlice());
                }
                for (uint32_t i = 0; i < pPps->pps_num_slices_in_pic_minus1; i++)
                {
                    pPps->pps_rect_slices[i]->pps_tile_idx = tileIdx;

                    if ((tileIdx % pPps->pps_num_tile_cols) != pPps->pps_num_tile_cols - 1)
                    {
                        pPps->pps_rect_slices[i]->pps_slice_width_in_tiles_minus1 = GetVLCElementU();
                    }
                    else
                    {
                        pPps->pps_rect_slices[i]->pps_slice_width_in_tiles_minus1 = 0;
                    }
                    if ((tileIdx / pPps->pps_num_tile_cols) != pPps->pps_num_tile_rows - 1 &&
                        (pPps->pps_tile_idx_delta_present_flag || tileIdx % pPps->pps_num_tile_cols == 0))
                    {
                        pPps->pps_rect_slices[i]->pps_slice_height_in_tiles_minus1 = GetVLCElementU();
                    }
                    else
                    {
                        if ((tileIdx / pPps->pps_num_tile_cols) == pPps->pps_num_tile_rows - 1)
                        {
                            pPps->pps_rect_slices[i]->pps_slice_height_in_tiles_minus1 = 0;
                        }
                        else
                        {
                            pPps->pps_rect_slices[i]->pps_slice_height_in_tiles_minus1 = pPps->pps_rect_slices[i - 1]->pps_slice_height_in_tiles_minus1;
                        }
                    }

                    if (pPps->pps_rect_slices[i]->pps_slice_width_in_tiles_minus1 == 0 &&
                        pPps->pps_rect_slices[i]->pps_slice_height_in_tiles_minus1 == 0)
                    {
                        if (pPps->pps_tile_row_height[tileIdx / pPps->pps_num_tile_cols] > 1)
                        {
                            code = GetVLCElementU();
                            if (code == 0)
                            {
                                pPps->pps_rect_slices[i]->pps_num_slices_in_tile = 1;
                                pPps->pps_rect_slices[i]->pps_slice_height_in_ctu_minus1 = pPps->pps_tile_row_height[tileIdx / pPps->pps_num_tile_cols] - 1;
                            }
                            else
                            {
                                uint32_t numExpSliceInTile = code;
                                uint32_t remTileRowHeight = pPps->pps_tile_row_height[tileIdx / pPps->pps_num_tile_cols];
                                uint32_t j = 0;
                                uint32_t pps_exp_slice_height_in_ctus_minus1 = 0;
                                for (; j < numExpSliceInTile; j++)
                                {
                                    pps_exp_slice_height_in_ctus_minus1 = GetVLCElementU();
                                    pPps->pps_rect_slices[i + j]->pps_slice_height_in_ctu_minus1 = pps_exp_slice_height_in_ctus_minus1;
                                    remTileRowHeight -= (pps_exp_slice_height_in_ctus_minus1 + 1);
                                }
                                uint32_t uniformSliceHeight = pps_exp_slice_height_in_ctus_minus1 + 1;

                                while (remTileRowHeight >= uniformSliceHeight)
                                {
                                    pPps->pps_rect_slices[i + j]->pps_slice_height_in_ctu_minus1 = uniformSliceHeight - 1;
                                    pPps->pps_rect_slices[i + j]->pps_derived_exp_slice_height_flag = true;
                                    remTileRowHeight -= uniformSliceHeight;
                                    j++;
                                }
                                if (remTileRowHeight > 0)
                                {
                                    pPps->pps_rect_slices[i + j]->pps_slice_height_in_ctu_minus1 = remTileRowHeight - 1;
                                    pPps->pps_rect_slices[i + j]->pps_derived_exp_slice_height_flag = true;
                                    j++;
                                }
                                for (uint32_t k = 0; k < j; k++)
                                {
                                    pPps->pps_rect_slices[i + k]->pps_num_slices_in_tile = j;
                                    pPps->pps_rect_slices[i + k]->pps_slice_width_in_tiles_minus1 = 0;
                                    pPps->pps_rect_slices[i + k]->pps_slice_height_in_tiles_minus1 = 0;
                                    pPps->pps_rect_slices[i + k]->pps_tile_idx = tileIdx;
                                }
                                i += (j - 1);
                            }
                        }
                        else
                        {
                            pPps->pps_rect_slices[i]->pps_num_slices_in_tile = 1;
                            pPps->pps_rect_slices[i]->pps_slice_height_in_ctu_minus1 = pPps->pps_tile_row_height[tileIdx / pPps->pps_num_tile_cols] - 1;
                        }
                    }

                    if (i < pPps->pps_num_slices_in_pic_minus1)
                    {
                        if (pPps->pps_tile_idx_delta_present_flag)
                        {
                            int32_t tileIdxDelta = GetVLCElementS();
                            tileIdx += tileIdxDelta;
                            if (tileIdx >= pPps->pps_num_tiles)
                            {
                                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                            }
                        }
                        else
                        {
                            tileIdx += (pPps->pps_rect_slices[i]->pps_slice_width_in_tiles_minus1 + 1);
                            if (tileIdx % pPps->pps_num_tile_cols == 0)
                            {
                                tileIdx += (pPps->pps_rect_slices[i]->pps_slice_height_in_tiles_minus1) * pPps->pps_num_tile_cols;
                            }
                        }
                    }
                }
                pPps->pps_rect_slices[pPps->pps_num_slices_in_pic_minus1]->pps_tile_idx = tileIdx;
            }
            else
            {
                for (uint32_t i = 0; i < pPps->pps_num_slices_in_pic_minus1 + 1; i++)
                {
                    pPps->pps_rect_slices.push_back(new VVCRectSlice());
                }
            }
            if (!pPps->pps_rect_slice_flag || pPps->pps_single_slice_per_subpic_flag || pPps->pps_num_slices_in_pic_minus1 > 0)
            {
                pPps->pps_loop_filter_across_slices_enabled_flag = Get1Bit();
            }
            else
            {
                pPps->pps_loop_filter_across_slices_enabled_flag = false;
            }
        }
        else
        {
            pPps->pps_single_slice_per_subpic_flag = true;
        }
        pPps->pps_cabac_init_present_flag = Get1Bit();
        pPps->pps_num_ref_idx_default_active_minus1[0] = GetVLCElementU();
        if (pPps->pps_num_ref_idx_default_active_minus1[0] + 1 > VVC_MAX_NUM_REF_IDX)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pPps->pps_num_ref_idx_default_active_minus1[1] = GetVLCElementU();
        if (pPps->pps_num_ref_idx_default_active_minus1[1] + 1 > VVC_MAX_NUM_REF_IDX)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pPps->pps_rpl1_idx_present_flag = Get1Bit();
        pPps->pps_weighted_pred_flag = Get1Bit();
        pPps->pps_weighted_bipred_flag = Get1Bit();
        pPps->pps_ref_wraparound_enabled_flag = Get1Bit();
        if (pPps->pps_ref_wraparound_enabled_flag)
        {
            pPps->pps_pic_width_minus_wraparound_offset = GetVLCElementU();
        }
        else
        {
            pPps->pps_pic_width_minus_wraparound_offset = 0;
        }
        pPps->pps_init_qp_minus26 = GetVLCElementS();
        pPps->pps_cu_qp_delta_enabled_flag = Get1Bit();
        pPps->pps_chroma_tool_offsets_present_flag = Get1Bit();
        if (pPps->pps_chroma_tool_offsets_present_flag)
        {
            pPps->pps_cb_qp_offset = GetVLCElementS();
            if (pPps->pps_cb_qp_offset < VVC_CHROMA_QP_OFFSET_LOWER_BOUND ||
                pPps->pps_cb_qp_offset > VVC_CHROMA_QP_OFFSET_UPPER_BOUND)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            pPps->pps_cr_qp_offset = GetVLCElementS();
            if (pPps->pps_cr_qp_offset < VVC_CHROMA_QP_OFFSET_LOWER_BOUND ||
                pPps->pps_cr_qp_offset > VVC_CHROMA_QP_OFFSET_UPPER_BOUND)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            pPps->pps_joint_cbcr_qp_offset_present_flag = Get1Bit();
            if (pPps->pps_joint_cbcr_qp_offset_present_flag)
            {
                pPps->pps_joint_cbcr_qp_offset_value = GetVLCElementS();
            }
            else
            {
                pPps->pps_joint_cbcr_qp_offset_value = 0;
            }
            if (pPps->pps_joint_cbcr_qp_offset_value < VVC_CHROMA_QP_OFFSET_LOWER_BOUND ||
                pPps->pps_joint_cbcr_qp_offset_value > VVC_CHROMA_QP_OFFSET_UPPER_BOUND)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            pPps->pps_slice_chroma_qp_offsets_present_flag = Get1Bit();
            pPps->pps_cu_chroma_qp_offset_list_enabled_flag = Get1Bit();
            if (!pPps->pps_cu_chroma_qp_offset_list_enabled_flag)
            {
                pPps->pps_chroma_qp_offset_list_len = 1;
            }
            else
            {
                pPps->pps_chroma_qp_offset_list_len = GetVLCElementU() + 1;
                if (pPps->pps_chroma_qp_offset_list_len > VVC_MAX_QP_OFFSET_LIST_SIZE)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                for (uint32_t i = 0; i < pPps->pps_chroma_qp_offset_list_len; i++)
                {
                    pPps->pps_cb_qp_offset_list[i] = GetVLCElementS();
                    if (pPps->pps_cb_qp_offset_list[i] < VVC_CHROMA_QP_OFFSET_LOWER_BOUND ||
                        pPps->pps_cb_qp_offset_list[i] > VVC_CHROMA_QP_OFFSET_UPPER_BOUND)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                    pPps->pps_cr_qp_offset_list[i] = GetVLCElementS();
                    if (pPps->pps_cr_qp_offset_list[i] < VVC_CHROMA_QP_OFFSET_LOWER_BOUND ||
                        pPps->pps_cr_qp_offset_list[i] > VVC_CHROMA_QP_OFFSET_UPPER_BOUND)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                    if (pPps->pps_joint_cbcr_qp_offset_present_flag)
                    {
                        pPps->pps_joint_cbcr_qp_offset_list[i] = GetVLCElementS();
                    }
                    else
                    {
                        pPps->pps_joint_cbcr_qp_offset_list[i] = 0;
                    }
                    if (pPps->pps_joint_cbcr_qp_offset_list[i] < VVC_CHROMA_QP_OFFSET_LOWER_BOUND ||
                        pPps->pps_joint_cbcr_qp_offset_list[i] > VVC_CHROMA_QP_OFFSET_UPPER_BOUND)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                }
            }
        }
        else
        {
            pPps->pps_cb_qp_offset = 0;
            pPps->pps_cr_qp_offset = 0;
            pPps->pps_joint_cbcr_qp_offset_present_flag = false;
            pPps->pps_slice_chroma_qp_offsets_present_flag = false;
            pPps->pps_chroma_qp_offset_list_len = 1;
            pPps->pps_cu_chroma_qp_offset_list_enabled_flag = 0;
        }
        pPps->pps_deblocking_filter_control_present_flag = Get1Bit();
        if (pPps->pps_deblocking_filter_control_present_flag)
        {
            pPps->pps_deblocking_filter_override_enabled_flag = Get1Bit();
            pPps->pps_deblocking_filter_disabled_flag = Get1Bit();
            if (!pPps->pps_no_pic_partition_flag && pPps->pps_deblocking_filter_override_enabled_flag)
            {
                pPps->pps_dbf_info_in_ph_flag = Get1Bit();
            }
            else
            {
                pPps->pps_dbf_info_in_ph_flag = false;
            }
            if (!pPps->pps_deblocking_filter_disabled_flag)
            {
                pPps->pps_luma_beta_offset_div2 = GetVLCElementS();
                if (pPps->pps_luma_beta_offset_div2 < VVC_DEBLOCKING_OFFSET_LOWER_BOUND ||
                    pPps->pps_luma_beta_offset_div2 > VVC_DEBLOCKING_OFFSET_UPPER_BOUND)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                pPps->pps_luma_tc_offset_div2 = GetVLCElementS();
                if (pPps->pps_luma_tc_offset_div2 < VVC_DEBLOCKING_OFFSET_LOWER_BOUND ||
                    pPps->pps_luma_tc_offset_div2 > VVC_DEBLOCKING_OFFSET_UPPER_BOUND)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                if (pPps->pps_chroma_tool_offsets_present_flag)
                {
                    pPps->pps_cb_beta_offset_div2 = GetVLCElementS();
                    if (pPps->pps_cb_beta_offset_div2 < VVC_DEBLOCKING_OFFSET_LOWER_BOUND ||
                        pPps->pps_cb_beta_offset_div2 > VVC_DEBLOCKING_OFFSET_UPPER_BOUND)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                    pPps->pps_cb_tc_offset_div2 = GetVLCElementS();
                    if (pPps->pps_cb_tc_offset_div2 < VVC_DEBLOCKING_OFFSET_LOWER_BOUND ||
                        pPps->pps_cb_tc_offset_div2 > VVC_DEBLOCKING_OFFSET_UPPER_BOUND)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                    pPps->pps_cr_beta_offset_div2 = GetVLCElementS();
                    if (pPps->pps_cr_beta_offset_div2 < VVC_DEBLOCKING_OFFSET_LOWER_BOUND ||
                        pPps->pps_cr_beta_offset_div2 > VVC_DEBLOCKING_OFFSET_UPPER_BOUND)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                    pPps->pps_cr_tc_offset_div2 = GetVLCElementS();
                    if (pPps->pps_cr_tc_offset_div2 < VVC_DEBLOCKING_OFFSET_LOWER_BOUND ||
                        pPps->pps_cr_tc_offset_div2 > VVC_DEBLOCKING_OFFSET_UPPER_BOUND)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                }
                else
                {
                    pPps->pps_cb_beta_offset_div2 = pPps->pps_luma_beta_offset_div2;
                    pPps->pps_cb_tc_offset_div2 = pPps->pps_luma_tc_offset_div2;
                    pPps->pps_cr_beta_offset_div2 = pPps->pps_luma_beta_offset_div2;
                    pPps->pps_cr_tc_offset_div2 = pPps->pps_luma_tc_offset_div2;
                }
            }
        }
        else
        {
            pPps->pps_deblocking_filter_override_enabled_flag = false;
            pPps->pps_dbf_info_in_ph_flag = false;
        }
        if (!pPps->pps_no_pic_partition_flag)
        {
            pPps->pps_rpl_info_in_ph_flag = Get1Bit();
            pPps->pps_sao_info_in_ph_flag = Get1Bit();
            pPps->pps_alf_info_in_ph_flag = Get1Bit();
            if ((pPps->pps_weighted_pred_flag || pPps->pps_weighted_bipred_flag) &&
                pPps->pps_rpl_info_in_ph_flag)
            {
                pPps->pps_wp_info_in_ph_flag = Get1Bit();
            }
            else
            {
                pPps->pps_wp_info_in_ph_flag = false;
            }
            pPps->pps_qp_delta_info_in_ph_flag = Get1Bit();
        }
        else
        {
            pPps->pps_rpl_info_in_ph_flag = false;
            pPps->pps_sao_info_in_ph_flag = false;
            pPps->pps_alf_info_in_ph_flag = false;
            pPps->pps_wp_info_in_ph_flag = false;
            pPps->pps_qp_delta_info_in_ph_flag = false;
        }
        pPps->pps_picture_header_extension_present_flag = Get1Bit();
        pPps->pps_slice_header_extension_present_flag = Get1Bit();
        pPps->pps_extension_flag = Get1Bit();
        if (pPps->pps_extension_flag)
        {
            while (MoreRbspData())
            {
                pPps->pps_extension_data_flag = Get1Bit();
            }
        }
        ReadOutTrailingBits();
        return sts;
    }

    void VVCHeadersBitstream::xInitializeVps(VVCVideoParamSet *pVps)
    {
        pVps->vps_video_parameter_set_id = 0;
        pVps->vps_max_layers_minus1 = 0;
        pVps->vps_max_sublayers_minus1 = 6;
        pVps->vps_default_ptl_dpb_hrd_max_tid_flag = true;
        pVps->vps_all_independent_layers_flag = true;
        pVps->vps_each_layer_is_an_ols_flag = true;
        pVps->vps_ols_mode_idc = 0;
        pVps->vps_num_ptls_minus1 = 0;
        pVps->vps_extension_flag = false;
        pVps->vps_timing_hrd_params_present_flag = false;
        pVps->vps_sublayer_cpb_params_present_flag = false;
        pVps->vps_num_ols_timing_hrd_params_minus1 = 0;
        pVps->vps_total_num_olss = 1;
        pVps->vps_num_multi_layered_olss = 0;
        pVps->vps_sublayer_dpb_params_present_flag = false;
        pVps->vps_target_ols_idx = 0;
    }

    void VVCHeadersBitstream::xInitSubPic(const VVCSeqParamSet* sps, VVCPicParamSet* pps) 
    {
        if (pps->pps_subpic_id_mapping_present_flag) 
        {
            if (pps->pps_num_subpics != (sps->sps_num_subpics_minus1 + 1)) 
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
        }
        else 
        {
            pps->pps_num_subpics = sps->sps_num_subpics_minus1 + 1;
        }
        if (pps->pps_num_subpics > VVC_MAX_NUM_SUB_PICS) 
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }

        if (pps->pps_sub_pics.size() < pps->pps_num_subpics)
        {
            for (auto i = pps->pps_sub_pics.size(); i < pps->pps_num_subpics; i++)
            {
                pps->pps_sub_pics.push_back(new SubPic);
            }
        }

        bool pps_ctu = (pps->pps_ctu_size == 0) || (pps->pps_pic_width_in_ctu == 0) || (pps->pps_pic_height_in_ctu == 0);
        if (pps_ctu)
        {
            pps->pps_ctu_size = sps->sps_ctu_size;
            pps->pps_pic_width_in_ctu = (pps->pps_pic_width_in_luma_samples + pps->pps_ctu_size - 1) / pps->pps_ctu_size;
            pps->pps_pic_height_in_ctu = (pps->pps_pic_height_in_luma_samples + pps->pps_ctu_size - 1) / pps->pps_ctu_size;
        }
        for (uint32_t i = 0; i < pps->pps_num_subpics; i++) 
        {
            if (sps->sps_subpic_id_mapping_explicitly_signalled_flag) 
            {
                if (pps->pps_subpic_id_mapping_present_flag) 
                {
                    pps->pps_sub_pics[i]->sub_pic_id = pps->pps_subpic_id[i];
                }
                else 
                {
                    pps->pps_sub_pics[i]->sub_pic_id = sps->sps_subpic_id[i];
                }
            }
            else 
            {
                pps->pps_sub_pics[i]->sub_pic_id = i;
            }
            pps->pps_sub_pics[i]->sub_pic_ctu_top_leftx = sps->sps_subpic_ctu_top_left_x[i];
            pps->pps_sub_pics[i]->sub_pic_ctu_top_lefty = sps->sps_subpic_ctu_top_left_y[i];
            pps->pps_sub_pics[i]->sub_pic_width = sps->sps_subpic_width_minus1[i] + 1;
            pps->pps_sub_pics[i]->sub_pic_height = sps->sps_subpic_height_minus1[i] + 1;
            pps->pps_sub_pics[i]->first_ctu_in_subpic = sps->sps_subpic_ctu_top_left_y[i] * pps->pps_pic_width_in_ctu + sps->sps_subpic_ctu_top_left_x[i];
            pps->pps_sub_pics[i]->last_ctu_in_subpic = (sps->sps_subpic_ctu_top_left_y[i] + sps->sps_subpic_height_minus1[i]) * pps->pps_pic_width_in_ctu + sps->sps_subpic_ctu_top_left_x[i] + sps->sps_subpic_width_minus1[i];
            pps->pps_sub_pics[i]->sub_pic_left = sps->sps_subpic_ctu_top_left_x[i] * pps->pps_ctu_size;
            pps->pps_sub_pics[i]->sub_pic_right = ((pps->pps_pic_width_in_luma_samples - 1) < ((sps->sps_subpic_ctu_top_left_x[i] + sps->sps_subpic_width_minus1[i] + 1) * pps->pps_ctu_size - 1)) ? 
                                                    (pps->pps_pic_width_in_luma_samples - 1) : ((sps->sps_subpic_ctu_top_left_x[i] + sps->sps_subpic_width_minus1[i] + 1) * pps->pps_ctu_size - 1);
            pps->pps_sub_pics[i]->subpic_width_in_luma_sample = pps->pps_sub_pics[i]->sub_pic_right - pps->pps_sub_pics[i]->sub_pic_left + 1;
            pps->pps_sub_pics[i]->sub_pic_top = sps->sps_subpic_ctu_top_left_y[i] * pps->pps_ctu_size;
            uint32_t bottom = (pps->pps_pic_height_in_luma_samples - 1) < ((sps->sps_subpic_ctu_top_left_y[i] + sps->sps_subpic_height_minus1[i] + 1) * pps->pps_ctu_size - 1) ?
                                (pps->pps_pic_height_in_luma_samples - 1) : ((sps->sps_subpic_ctu_top_left_y[i] + sps->sps_subpic_height_minus1[i] + 1) * pps->pps_ctu_size - 1);
            pps->pps_sub_pics[i]->subpic_height_in_luma_sample = bottom - pps->pps_sub_pics[i]->sub_pic_top + 1;
            pps->pps_sub_pics[i]->sub_pic_bottom = bottom;
            pps->pps_sub_pics[i]->ctu_addr_in_subpic.clear();

            if (pps->pps_num_slices_in_pic == 1) 
            {
                if (pps->pps_num_subpics != 1) 
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                if (pps->pps_pic_width_in_ctu <= 0 || pps->pps_pic_height_in_ctu <= 0) 
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                for (uint32_t ctbY = 0; ctbY < pps->pps_pic_height_in_ctu; ctbY++) 
                {
                    for (uint32_t ctbX = 0; ctbX < pps->pps_pic_width_in_ctu; ctbX++) 
                    {
                        pps->pps_sub_pics[i]->ctu_addr_in_subpic.push_back(ctbY * pps->pps_pic_width_in_ctu + ctbX);
                    }
                }
                pps->pps_sub_pics[i]->num_slices_in_subPic = 1;
            }
            else 
            {
                int32_t numSlicesInSubPic = 0;
                int32_t idxLastSliceInSubpic = -1;
                uint32_t idxFirstSliceAfterSubpic = pps->pps_num_slices_in_pic;
                for (uint32_t j = 0; j < pps->pps_num_slices_in_pic; j++)
                {
                    uint32_t ctu = pps->pps_slice_map[j]->ctu_addr_in_slice[0];
                    uint32_t ctu_x = ctu % pps->pps_pic_width_in_ctu;
                    uint32_t ctu_y = ctu / pps->pps_pic_width_in_ctu;
                    if (ctu_x >= sps->sps_subpic_ctu_top_left_x[i] &&
                        ctu_x < (sps->sps_subpic_ctu_top_left_x[i] + sps->sps_subpic_width_minus1[i] + 1) &&
                        ctu_y >= sps->sps_subpic_ctu_top_left_y[i] &&
                        ctu_y < (sps->sps_subpic_ctu_top_left_y[i] + sps->sps_subpic_height_minus1[i] + 1)) 
                    {
                        for (uint32_t ctuAddr : pps->pps_slice_map[j]->ctu_addr_in_slice)
                        {
                            pps->pps_sub_pics[i]->ctu_addr_in_subpic.push_back(ctuAddr);
                        }
                        numSlicesInSubPic++;
                        idxLastSliceInSubpic = j;
                    }
                    else if ((idxFirstSliceAfterSubpic == pps->pps_num_slices_in_pic) && idxLastSliceInSubpic != -1)
                    {
                        idxFirstSliceAfterSubpic = j;
                    }
                }
                pps->pps_sub_pics[i]->num_slices_in_subPic = numSlicesInSubPic;
            }
        }
    }

    void VVCHeadersBitstream::xParseGeneralConstraintsInfo(VVCConstraintInfo *general_constraints_info)
    {
        general_constraints_info->gci_present_flag = Get1Bit();
        if (general_constraints_info->gci_present_flag)
        {
            // general
            general_constraints_info->gci_intra_only_constraint_flag = Get1Bit();
            general_constraints_info->gci_all_layers_independent_constraint_flag = Get1Bit();
            general_constraints_info->gci_one_au_only_constraint_flag = Get1Bit();
            // picture format
            general_constraints_info->gci_sixteen_minus_max_bitdepth_constraint_idc = GetBits(4);
            if (general_constraints_info->gci_sixteen_minus_max_bitdepth_constraint_idc > 8)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            general_constraints_info->gci_three_minus_max_chroma_format_constraint_idc = GetBits(2);
            // NAL unit type related
            general_constraints_info->gci_no_mixed_nalu_types_in_pic_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_trail_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_stsa_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_rasl_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_radl_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_idr_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_cra_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_gdr_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_aps_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_idr_rpl_constraint_flag = Get1Bit();
            // tile, slice, subpicture partitioning
            general_constraints_info->gci_one_tile_per_pic_constraint_flag = Get1Bit();
            general_constraints_info->gci_pic_header_in_slice_header_constraint_flag = Get1Bit();
            general_constraints_info->gci_one_slice_per_pic_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_rectangular_slice_constraint_flag = Get1Bit();
            general_constraints_info->gci_one_slice_per_subpic_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_subpic_info_constraint_flag = Get1Bit();
            // CTU and block partitioning
            general_constraints_info->gci_three_minus_max_log2_ctu_size_constraint_idc = GetBits(2);
            general_constraints_info->gci_no_partition_constraints_override_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_mtt_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_qtbtt_dual_tree_intra_constraint_flag = Get1Bit();
            // intra
            general_constraints_info->gci_no_palette_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_ibc_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_isp_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_mrl_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_mip_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_cclm_constraint_flag = Get1Bit();
            // inter
            general_constraints_info->gci_no_ref_pic_resampling_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_res_change_in_clvs_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_weighted_prediction_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_ref_wraparound_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_temporal_mvp_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_sbtmvp_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_amvr_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_bdof_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_smvd_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_dmvr_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_mmvd_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_affine_motion_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_prof_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_bcw_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_ciip_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_gpm_constraint_flag = Get1Bit();
            // transform, quantization, residual
            general_constraints_info->gci_no_luma_transform_size_64_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_transform_skip_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_bdpcm_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_mts_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_lfnst_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_joint_cbcr_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_sbt_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_act_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_explicit_scaling_list_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_dep_quant_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_sign_data_hiding_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_cu_qp_delta_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_chroma_qp_offset_constraint_flag = Get1Bit();
            // loop filter
            general_constraints_info->gci_no_sao_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_alf_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_ccalf_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_lmcs_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_ladf_constraint_flag = Get1Bit();
            general_constraints_info->gci_no_virtual_boundaries_constraint_flag = Get1Bit();
            general_constraints_info->gci_num_reserved_bits = GetBits(8);
            for (uint32_t i = 0; i < general_constraints_info->gci_num_reserved_bits; i++)
            {
                Get1Bit();
            }
        }
        uint32_t numBitsUntilByteAligned = GetNumBitsUntilByteAligned();
        if (numBitsUntilByteAligned > 0)
        {
            GetBits(numBitsUntilByteAligned);
        }
    }

    void VVCHeadersBitstream::xParseProfileTierLevel(VVCProfileTierLevel *profileTierLevel,
                                                     bool profileTierPresentFlag,
                                                     uint32_t maxNumSubLayersMinus1)
    {
        if (!profileTierLevel)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        if (profileTierPresentFlag)
        {
            profileTierLevel->general_profile_idc = GetBits(7);
            profileTierLevel->general_tier_flag = Get1Bit();
        }
        profileTierLevel->general_level_idc = GetBits(8);
        profileTierLevel->ptl_frame_only_constraint_flag = Get1Bit();
        profileTierLevel->ptl_multilayer_enabled_flag = Get1Bit();
        bool profileFlag = profileTierLevel->general_profile_idc == Profile::VVC_MAIN_10 ||
            profileTierLevel->general_profile_idc == Profile::VVC_MAIN_10_444 ||
            profileTierLevel->general_profile_idc == Profile::VVC_MAIN_10_STILL_PICTURE ||
            profileTierLevel->general_profile_idc == Profile::VVC_MAIN_10_444_STILL_PICTURE;
        if (profileFlag && profileTierLevel->ptl_multilayer_enabled_flag)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        if (profileTierPresentFlag)
        {
            xParseGeneralConstraintsInfo(&profileTierLevel->general_constraints_info);
        }
        for (int32_t i = maxNumSubLayersMinus1 - 1; i >= 0; i--)
        {
            profileTierLevel->ptl_sublayer_level_present_flag[i] = Get1Bit();
        }
        uint32_t numBitsUntilByteAligned = GetNumBitsUntilByteAligned();
        if (numBitsUntilByteAligned > 0)
        {
            GetBits(numBitsUntilByteAligned);
        }
        profileTierLevel->sublayer_level_idc[maxNumSubLayersMinus1] = profileTierLevel->general_level_idc;
        for (int32_t i = maxNumSubLayersMinus1 - 1; i >= 0; i--)
        {
            if (profileTierLevel->ptl_sublayer_level_present_flag[i])
            {
                profileTierLevel->sublayer_level_idc[i] = GetBits(8);
            }
            else
            {
                profileTierLevel->sublayer_level_idc[i] = profileTierLevel->sublayer_level_idc[i + 1];
            }
        }
        if (profileTierPresentFlag)
        {
            profileTierLevel->ptl_num_sub_profiles = GetBits(8);
            profileTierLevel->general_sub_profile_idc.resize(profileTierLevel->ptl_num_sub_profiles);
            for (uint32_t i = 0; i < profileTierLevel->ptl_num_sub_profiles; i++)
            {
                profileTierLevel->general_sub_profile_idc.push_back(GetBits(32));
            }
        }
    }

    void VVCHeadersBitstream::xDeriveOutputLayerSets(VVCVideoParamSet *pVps)
    {
        if (pVps->vps_each_layer_is_an_ols_flag || pVps->vps_ols_mode_idc < 2)
        {
            pVps->vps_total_num_olss = pVps->vps_max_layers_minus1 + 1;
        }
        else if (pVps->vps_ols_mode_idc == 2)
        {
            pVps->vps_total_num_olss = pVps->vps_num_output_layer_sets_minus2 + 2;
        }
        pVps->vps_ols_dpb_pic_width.resize(pVps->vps_total_num_olss);
        pVps->vps_ols_dpb_pic_height.resize(pVps->vps_total_num_olss);
        pVps->vps_ols_dpb_chroma_format.resize(pVps->vps_total_num_olss);
        pVps->vps_ols_dpb_bitdepth_minus8.resize(pVps->vps_total_num_olss);
        pVps->vps_ols_dpb_params_idx.resize(pVps->vps_total_num_olss);

        uint32_t maxLayers = pVps->vps_max_layers_minus1 + 1;
        pVps->vps_num_output_layers_in_ols.resize(pVps->vps_total_num_olss);
        pVps->vps_num_layers_in_ols.resize(pVps->vps_total_num_olss);
        pVps->vps_output_layer_id_in_ols.resize(pVps->vps_total_num_olss,
            std::vector<uint32_t>(maxLayers, 0));
        pVps->vps_num_sub_layers_in_layer_in_ols.resize(pVps->vps_total_num_olss,
            std::vector<uint32_t>(maxLayers, 0));
        pVps->vps_layer_id_in_ols.resize(pVps->vps_total_num_olss,
            std::vector<uint32_t>(maxLayers, 0));

        std::vector<uint32_t> numRefLayers(maxLayers);
        std::vector<std::vector<uint32_t>> outputLayerIdx(pVps->vps_total_num_olss,
            std::vector<uint32_t>(maxLayers, 0));
        std::vector<std::vector<uint32_t>> layerIncludedInOlsFlag(pVps->vps_total_num_olss,
            std::vector<uint32_t>(maxLayers, 0));
        std::vector<std::vector<uint32_t>> dependencyFlag(maxLayers,
            std::vector<uint32_t>(maxLayers, 0));
        std::vector<std::vector<uint32_t>> refLayerIdx(maxLayers,
            std::vector<uint32_t>(maxLayers, 0));
        std::vector<uint32_t> layerUsedAsRefLayerFlag(maxLayers, 0);
        std::vector<uint32_t> layerUsedAsOutputLayerFlag(maxLayers, 0);

        for (uint32_t i = 0; i < maxLayers; i++)
        {
            uint32_t r = 0;

            for (uint32_t j = 0; j < maxLayers; j++)
            {
                dependencyFlag[i][j] = pVps->vps_direct_ref_layer_flag[i][j];

                for (uint32_t k = 0; k < i; k++)
                {
                    if (pVps->vps_direct_ref_layer_flag[i][k] && dependencyFlag[k][j])
                    {
                        dependencyFlag[i][j] = 1;
                    }
                }
                if (pVps->vps_direct_ref_layer_flag[i][j])
                {
                    layerUsedAsRefLayerFlag[j] = 1;
                }

                if (dependencyFlag[i][j])
                {
                    refLayerIdx[i][r++] = j;
                }
            }

            numRefLayers[i] = r;
        }

        pVps->vps_num_output_layers_in_ols[0] = 1;
        pVps->vps_output_layer_id_in_ols[0][0] = pVps->vps_layer_id[0];
        pVps->vps_num_sub_layers_in_layer_in_ols[0][0] = pVps->vps_ptl_max_tid[pVps->vps_ols_ptl_idx[0]] + 1;
        layerUsedAsOutputLayerFlag[0] = 1;
        for (uint32_t i = 1; i < maxLayers; i++)
        {
            if (pVps->vps_each_layer_is_an_ols_flag || pVps->vps_ols_mode_idc < 2)
            {
                layerUsedAsOutputLayerFlag[i] = 1;
            }
            else
            {
                layerUsedAsOutputLayerFlag[i] = 0;
            }
        }
        for (uint32_t i = 1; i < pVps->vps_total_num_olss; i++)
        {
            if (pVps->vps_each_layer_is_an_ols_flag || pVps->vps_ols_mode_idc == 0)
            {
                pVps->vps_num_output_layers_in_ols[i] = 1;
                pVps->vps_output_layer_id_in_ols[i][0] = pVps->vps_layer_id[i];
                if (pVps->vps_each_layer_is_an_ols_flag)
                {
                    pVps->vps_num_sub_layers_in_layer_in_ols[i][0] = pVps->vps_ptl_max_tid[pVps->vps_ols_ptl_idx[i]] + 1;
                }
                else
                {
                    pVps->vps_num_sub_layers_in_layer_in_ols[i][i] = pVps->vps_ptl_max_tid[pVps->vps_ols_ptl_idx[i]] + 1;
                    for (int32_t k = i - 1; k >= 0; k--)
                    {
                        pVps->vps_num_sub_layers_in_layer_in_ols[i][k] = 0;
                        for (uint32_t m = k + 1; m <= i; m++)
                        {
                            uint32_t maxSublayerNeeded = std::min((uint32_t)pVps->vps_num_sub_layers_in_layer_in_ols[i][m], pVps->vps_max_tid_il_ref_pics_plus1[m][k]);
                            if (pVps->vps_direct_ref_layer_flag[m][k] &&
                                pVps->vps_num_sub_layers_in_layer_in_ols[i][k] < maxSublayerNeeded)
                            {
                                pVps->vps_num_sub_layers_in_layer_in_ols[i][k] = maxSublayerNeeded;
                            }
                        }
                    }
                }
            }
            else if (pVps->vps_ols_mode_idc == 1)
            {
                pVps->vps_num_output_layers_in_ols[i] = i + 1;

                for (uint32_t j = 0; j < pVps->vps_num_output_layers_in_ols[i]; j++)
                {
                    pVps->vps_output_layer_id_in_ols[i][j] = pVps->vps_layer_id[j];
                    pVps->vps_num_sub_layers_in_layer_in_ols[i][j] = pVps->vps_ptl_max_tid[pVps->vps_ols_ptl_idx[i]] + 1;
                }
            }
            else if (pVps->vps_ols_mode_idc == 2)
            {
                uint32_t j = 0;
                uint32_t highestIncludedLayer = 0;
                for (j = 0; j < maxLayers; j++)
                {
                    pVps->vps_num_sub_layers_in_layer_in_ols[i][j] = 0;
                }
                j = 0;
                for (uint32_t k = 0; k < maxLayers; k++)
                {
                    if (pVps->vps_ols_output_layer_flag[i][k])
                    {
                        layerIncludedInOlsFlag[i][k] = 1;
                        highestIncludedLayer = k;
                        layerUsedAsOutputLayerFlag[k] = 1;
                        outputLayerIdx[i][j] = k;
                        pVps->vps_output_layer_id_in_ols[i][j++] = pVps->vps_layer_id[k];
                        pVps->vps_num_sub_layers_in_layer_in_ols[i][k] = pVps->vps_ptl_max_tid[pVps->vps_ols_ptl_idx[i]] + 1;
                    }
                }
                pVps->vps_num_output_layers_in_ols[i] = j;

                for (j = 0; j < pVps->vps_num_output_layers_in_ols[i]; j++)
                {
                    uint32_t idx = outputLayerIdx[i][j];
                    for (uint32_t k = 0; k < numRefLayers[idx]; k++)
                    {
                        layerIncludedInOlsFlag[i][refLayerIdx[idx][k]] = 1;
                    }
                }
                for (int32_t k = highestIncludedLayer - 1; k >= 0; k--)
                {
                    if (layerIncludedInOlsFlag[i][k] && !pVps->vps_ols_output_layer_flag[i][k])
                    {
                        for (uint32_t m = k + 1; m <= highestIncludedLayer; m++)
                        {
                            uint32_t maxSublayerNeeded = std::min((uint32_t)pVps->vps_num_sub_layers_in_layer_in_ols[i][m], pVps->vps_max_tid_il_ref_pics_plus1[m][k]);
                            if (pVps->vps_direct_ref_layer_flag[m][k] &&
                                layerIncludedInOlsFlag[i][m] &&
                                pVps->vps_num_sub_layers_in_layer_in_ols[i][k] < maxSublayerNeeded)
                            {
                                pVps->vps_num_sub_layers_in_layer_in_ols[i][k] = maxSublayerNeeded;
                            }
                        }
                    }
                }
            }
        }
        for (uint32_t i = 0; i < maxLayers; i++)
        {
            if (layerUsedAsRefLayerFlag[i] == 0 && layerUsedAsOutputLayerFlag[i] == 0)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
        }

        pVps->vps_num_layers_in_ols[0] = 1;
        pVps->vps_layer_id_in_ols[0][0] = pVps->vps_layer_id[0];
        pVps->vps_num_multi_layered_olss = 0;
        for (uint32_t i = 1; i < pVps->vps_total_num_olss; i++)
        {
            if (pVps->vps_each_layer_is_an_ols_flag)
            {
                pVps->vps_num_layers_in_ols[i] = 1;
                pVps->vps_layer_id_in_ols[i][0] = pVps->vps_layer_id[i];
            }
            else if (pVps->vps_ols_mode_idc == 0 || pVps->vps_ols_mode_idc == 1)
            {
                pVps->vps_num_layers_in_ols[i] = i + 1;
                for (uint32_t j = 0; j < pVps->vps_num_layers_in_ols[i]; j++)
                {
                    pVps->vps_layer_id_in_ols[i][j] = pVps->vps_layer_id[j];
                }
            }
            else if (pVps->vps_ols_mode_idc == 2)
            {
                uint32_t j = 0;
                for (uint32_t k = 0; k < pVps->vps_max_layers_minus1 + 1; k++)
                {
                    if (layerIncludedInOlsFlag[i][k])
                    {
                        pVps->vps_layer_id_in_ols[i][j++] = pVps->vps_layer_id[k];
                    }
                }
                pVps->vps_num_layers_in_ols[i] = j;
            }
            if (pVps->vps_num_layers_in_ols[i] > 1)
            {
                pVps->vps_num_multi_layered_olss++;
            }
        }
    }

    void VVCHeadersBitstream::xDeriveTargetOutputLayerSet(VVCVideoParamSet *pVps, int32_t targetOlsIdx)
    {
        pVps->vps_target_ols_idx = targetOlsIdx < 0 ? pVps->vps_max_layers_minus1: targetOlsIdx;
        pVps->vps_target_output_layer_id_set.clear();
        pVps->vps_target_layer_id_set.clear();

        for (uint32_t i = 0; i < pVps->vps_num_output_layers_in_ols[pVps->vps_target_ols_idx]; i++)
        {
            pVps->vps_target_output_layer_id_set.push_back(pVps->vps_output_layer_id_in_ols[pVps->vps_target_ols_idx][i]);
        }

        for (uint32_t i = 0; i < pVps->vps_num_layers_in_ols[pVps->vps_target_ols_idx]; i++)
        {
            pVps->vps_target_layer_id_set.push_back(pVps->vps_layer_id_in_ols[pVps->vps_target_ols_idx][i]);
        }
    }

    int32_t VVCHeadersBitstream::xDeriveTargetOLSIdx(VVCVideoParamSet* pVps)
    {
        int32_t lowestIdx = 0;

        for (uint32_t idx = 1; idx < pVps->vps_total_num_olss; idx++)
        {
            if ((pVps->vps_num_layers_in_ols[lowestIdx] == pVps->vps_num_layers_in_ols[idx]
                && pVps->vps_num_output_layers_in_ols[lowestIdx] < pVps->vps_num_output_layers_in_ols[idx])
                || pVps->vps_num_layers_in_ols[lowestIdx] < pVps->vps_num_layers_in_ols[idx])
            {
                lowestIdx = idx;
            }
        }

        return lowestIdx;
    }

    void VVCHeadersBitstream::xParseDpbParameters(VVCDpbParameter *dpb_parameter, uint32_t MaxSubLayersMinus1, bool subLayerInfoFlag)
    {
        for (uint32_t i = (subLayerInfoFlag ? 0 : MaxSubLayersMinus1); i <= MaxSubLayersMinus1; i++)
        {
            dpb_parameter->dpb_max_dec_pic_buffering_minus1[i] = GetVLCElementU();
            dpb_parameter->dpb_max_num_reorder_pics[i] = GetVLCElementU();
            dpb_parameter->dpb_max_latency_increase_plus1[i] = GetVLCElementU();
        }
        if (!subLayerInfoFlag)
        {
            for (uint32_t i = 0; i < MaxSubLayersMinus1; ++i)
            {
                dpb_parameter->dpb_max_dec_pic_buffering_minus1[i] = dpb_parameter->dpb_max_dec_pic_buffering_minus1[MaxSubLayersMinus1];
                dpb_parameter->dpb_max_num_reorder_pics[i] = dpb_parameter->dpb_max_num_reorder_pics[MaxSubLayersMinus1];
                dpb_parameter->dpb_max_latency_increase_plus1[i] = dpb_parameter->dpb_max_latency_increase_plus1[MaxSubLayersMinus1];
            }
        }
    }

    void VVCHeadersBitstream::xParseGeneralTimingHrdParameters(VVCGeneralTimingHrdParams *general_timing_hrd_parameters)
    {
        general_timing_hrd_parameters->num_units_in_tick = GetBits(32);
        general_timing_hrd_parameters->time_scale = GetBits(32);
        general_timing_hrd_parameters->general_nal_hrd_params_present_flag = Get1Bit();
        general_timing_hrd_parameters->general_vcl_hrd_params_present_flag = Get1Bit();
        if (general_timing_hrd_parameters->general_nal_hrd_params_present_flag ||
            general_timing_hrd_parameters->general_vcl_hrd_params_present_flag)
        {
            general_timing_hrd_parameters->general_same_pic_timing_in_all_ols_flag = Get1Bit();
            general_timing_hrd_parameters->general_du_hrd_params_present_flag = Get1Bit();
            if (general_timing_hrd_parameters->general_du_hrd_params_present_flag)
            {
                general_timing_hrd_parameters->tick_divisor_minus2 = GetBits(8);
            }
            general_timing_hrd_parameters->bit_rate_scale = GetBits(4);
            general_timing_hrd_parameters->cpb_size_scale = GetBits(4);
            if (general_timing_hrd_parameters->general_du_hrd_params_present_flag)
            {
                general_timing_hrd_parameters->cpb_size_du_scale = GetBits(4);
            }
            general_timing_hrd_parameters->hrd_cpb_cnt_minus1 = GetVLCElementU();
            if (general_timing_hrd_parameters->hrd_cpb_cnt_minus1 > 31)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
        }
    }

    void VVCHeadersBitstream::xParseOlsTimingHrdParameters(VVCGeneralTimingHrdParams *general_timing_hrd_parameters,
                                                           VVCOlsTimingHrdParams *ols_timing_hrd_parameters,
                                                           uint32_t firstSubLayer,
                                                           uint32_t MaxSubLayersVal)
    {
        for (uint32_t i = firstSubLayer; i <= MaxSubLayersVal; i++)
        {
            ols_timing_hrd_parameters->fixed_pic_rate_general_flag[i] = Get1Bit();
            if (!ols_timing_hrd_parameters->fixed_pic_rate_general_flag[i])
            {
                ols_timing_hrd_parameters->fixed_pic_rate_within_cvs_flag[i] = Get1Bit();
            }
            else
            {
                ols_timing_hrd_parameters->fixed_pic_rate_within_cvs_flag[i] = true;
            }
            ols_timing_hrd_parameters->low_delay_hrd_flag[i] = false;
            if (ols_timing_hrd_parameters->fixed_pic_rate_within_cvs_flag[i])
            {
                ols_timing_hrd_parameters->elemental_duration_in_tc_minus1[i] = GetVLCElementU();
            }
            else if ((general_timing_hrd_parameters->general_nal_hrd_params_present_flag ||
                      general_timing_hrd_parameters->general_vcl_hrd_params_present_flag) &&
                      general_timing_hrd_parameters->hrd_cpb_cnt_minus1 == 0)
            {
                ols_timing_hrd_parameters->low_delay_hrd_flag[i] = Get1Bit();
            }
            for (uint32_t nalOrVcl = 0; nalOrVcl < 2; nalOrVcl++)
            {
                if (((nalOrVcl == 0) && (general_timing_hrd_parameters->general_nal_hrd_params_present_flag)) ||
                    ((nalOrVcl == 1) && (general_timing_hrd_parameters->general_vcl_hrd_params_present_flag)))
                {
                    for (uint32_t j = 0; j <= general_timing_hrd_parameters->hrd_cpb_cnt_minus1; j++)
                    {
                        ols_timing_hrd_parameters->bit_rate_value_minus1[i][j][nalOrVcl] = GetVLCElementU();
                        ols_timing_hrd_parameters->cpb_size_value_minus1[i][j][nalOrVcl] = GetVLCElementU();
                        if (general_timing_hrd_parameters->general_du_hrd_params_present_flag)
                        {
                            ols_timing_hrd_parameters->cpb_size_du_value_minus1[i][j][nalOrVcl] = GetVLCElementU();
                            ols_timing_hrd_parameters->bit_rate_du_value_minus1[i][j][nalOrVcl] = GetVLCElementU();
                        }
                        ols_timing_hrd_parameters->cbr_flag[i][j][nalOrVcl] = Get1Bit();
                    }
                }
            }
        }
        for (uint32_t i = 0; i < firstSubLayer; i++)
        {
            ols_timing_hrd_parameters->fixed_pic_rate_general_flag[i] = ols_timing_hrd_parameters->fixed_pic_rate_general_flag[MaxSubLayersVal];
            ols_timing_hrd_parameters->fixed_pic_rate_within_cvs_flag[i] = ols_timing_hrd_parameters->fixed_pic_rate_within_cvs_flag[MaxSubLayersVal];
            ols_timing_hrd_parameters->elemental_duration_in_tc_minus1[i] = ols_timing_hrd_parameters->elemental_duration_in_tc_minus1[MaxSubLayersVal];
            ols_timing_hrd_parameters->low_delay_hrd_flag[i] = ols_timing_hrd_parameters->low_delay_hrd_flag[MaxSubLayersVal];
            for (uint32_t nalOrVcl = 0; nalOrVcl < 2; nalOrVcl++)
            {
                if (((nalOrVcl == 0) && (general_timing_hrd_parameters->general_nal_hrd_params_present_flag)) ||
                    ((nalOrVcl == 1) && (general_timing_hrd_parameters->general_vcl_hrd_params_present_flag)))
                {
                    for (uint32_t j = 0; j <= general_timing_hrd_parameters->hrd_cpb_cnt_minus1; j++)
                    {
                        ols_timing_hrd_parameters->bit_rate_value_minus1[i][j][nalOrVcl] = ols_timing_hrd_parameters->bit_rate_value_minus1[MaxSubLayersVal][j][nalOrVcl];
                        ols_timing_hrd_parameters->cpb_size_value_minus1[i][j][nalOrVcl] = ols_timing_hrd_parameters->cpb_size_value_minus1[MaxSubLayersVal][j][nalOrVcl];
                        if (general_timing_hrd_parameters->general_du_hrd_params_present_flag)
                        {
                            ols_timing_hrd_parameters->cpb_size_du_value_minus1[i][j][nalOrVcl] = ols_timing_hrd_parameters->cpb_size_du_value_minus1[MaxSubLayersVal][j][nalOrVcl];
                            ols_timing_hrd_parameters->bit_rate_du_value_minus1[i][j][nalOrVcl] = ols_timing_hrd_parameters->bit_rate_du_value_minus1[MaxSubLayersVal][j][nalOrVcl];
                        }
                        ols_timing_hrd_parameters->cbr_flag[i][j][nalOrVcl] = ols_timing_hrd_parameters->cbr_flag[MaxSubLayersVal][j][nalOrVcl];
                    }
                }
            }
        }
    }

    UMC::Status VVCHeadersBitstream::GetVideoParamSet(VVCVideoParamSet *pVps)
    {
        if (!pVps)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        UMC::Status sts = UMC::UMC_OK;
        xInitializeVps(pVps);
        pVps->vps_video_parameter_set_id = GetBits(4);

        if (pVps->vps_video_parameter_set_id == 0)
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);

        pVps->vps_max_layers_minus1 = GetBits(6);

        if (pVps->vps_max_layers_minus1 + 1 > VVC_MAX_VPS_LAYERS)
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);

        if (pVps->vps_max_layers_minus1 == 0)
        {
            pVps->vps_each_layer_is_an_ols_flag = 1;
        }

        pVps->vps_max_sublayers_minus1 = GetBits(3);

        if (pVps->vps_max_sublayers_minus1 + 1 > VVC_MAX_VPS_SUBLAYERS)
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);

        if (pVps->vps_max_layers_minus1 > 0 && pVps->vps_max_sublayers_minus1 > 0)
        {
            pVps->vps_default_ptl_dpb_hrd_max_tid_flag = Get1Bit();
        }
        else
        {
            pVps->vps_default_ptl_dpb_hrd_max_tid_flag = 1;
        }

        if (pVps->vps_max_layers_minus1 > 0)
        {
            pVps->vps_all_independent_layers_flag = Get1Bit();
            if (!pVps->vps_all_independent_layers_flag)
            {
                pVps->vps_each_layer_is_an_ols_flag = 0;
            }
        }
        else
        {
            pVps->vps_all_independent_layers_flag = 1;
        }

        for (uint32_t i = 0; i <= pVps->vps_max_layers_minus1; i++)
        {
            pVps->vps_layer_id[i] = GetBits(6);
            if (i > 0 && !pVps->vps_all_independent_layers_flag)
            {
                pVps->vps_independent_layer_flag[i] = Get1Bit();
                if (!pVps->vps_independent_layer_flag[i])
                {
                    pVps->vps_max_tid_ref_present_flag[i] = Get1Bit();
                    uint32_t numVpsDirectRefLayerFlag = 0;
                    for (uint32_t j = 0, k = 0; j < i; j++)
                    {
                        pVps->vps_direct_ref_layer_flag[i][j] = Get1Bit();
                        if (pVps->vps_direct_ref_layer_flag[i][j])
                        {
                            pVps->m_directRefLayerIdx[i][k++] = j;
                            numVpsDirectRefLayerFlag++;
                        }
                        if (pVps->vps_max_tid_ref_present_flag[i] && pVps->vps_direct_ref_layer_flag[i][j])
                        {
                            pVps->vps_max_tid_il_ref_pics_plus1[i][j] = GetBits(3);
                        }
                        else
                        {
                            pVps->vps_max_tid_il_ref_pics_plus1[i][j] = VVC_MAX_VPS_SUBLAYERS;
                        }
                    }
                    if (numVpsDirectRefLayerFlag == 0)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                }
            }
            else
            {
                pVps->vps_independent_layer_flag[i] = 1;
            }
        }

        if (pVps->vps_max_layers_minus1 > 0)
        {
            if (pVps->vps_all_independent_layers_flag)
            {
                pVps->vps_each_layer_is_an_ols_flag = Get1Bit();
                if (!pVps->vps_each_layer_is_an_ols_flag)
                {
                    pVps->vps_ols_mode_idc = 2;
                }
            }
            else
            {
                pVps->vps_each_layer_is_an_ols_flag = 0;
            }
            if (!pVps->vps_each_layer_is_an_ols_flag)
            {
                if (!pVps->vps_all_independent_layers_flag)
                {
                    pVps->vps_ols_mode_idc = GetBits(2);
                    if (pVps->vps_ols_mode_idc > VVC_MAX_VPS_OLS_MODE_IDC)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                }
                if (pVps->vps_ols_mode_idc == 2)
                {
                    pVps->vps_num_output_layer_sets_minus2 = GetBits(8);
                    for (uint32_t i = 1; i <= pVps->vps_num_output_layer_sets_minus2 + 1; i++)
                    {
                        for (uint32_t j = 0; j <= pVps->vps_max_layers_minus1; j++)
                        {
                            pVps->vps_ols_output_layer_flag[i][j] = Get1Bit();
                        }
                    }
                }
            }
            pVps->vps_num_ptls_minus1 = GetBits(8);
        }
        else
        {
            pVps->vps_each_layer_is_an_ols_flag = 1;
            pVps->vps_num_ptls_minus1 = 0;
        }

        xDeriveOutputLayerSets(pVps);

        if (pVps->vps_num_ptls_minus1 >= pVps->vps_total_num_olss)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }

        std::vector<bool> isPTLReferred((uint64_t)pVps->vps_num_ptls_minus1 + 1, false);

        for (uint32_t i = 0; i <= pVps->vps_num_ptls_minus1; i++)
        {
            if (i > 0)
            {
                pVps->vps_pt_present_flag[i] = Get1Bit();
            }
            else
            {
                pVps->vps_pt_present_flag[0] = 1;
            }
            if (!pVps->vps_default_ptl_dpb_hrd_max_tid_flag)
            {
                pVps->vps_ptl_max_tid[i] = GetBits(3);
            }
            else
            {
                pVps->vps_ptl_max_tid[i] = pVps->vps_max_sublayers_minus1;
            }
        }

        uint32_t numBitsUntilByteAligned = GetNumBitsUntilByteAligned();

        if (numBitsUntilByteAligned > 0)
        {
            GetBits(numBitsUntilByteAligned);
        }

        pVps->profile_tier_level.resize((uint64_t)pVps->vps_num_ptls_minus1 + 1);

        for (uint32_t i = 0; i <= pVps->vps_num_ptls_minus1; i++)
        {
            xParseProfileTierLevel(&pVps->profile_tier_level[i], pVps->vps_pt_present_flag[i], pVps->vps_ptl_max_tid[i]);
        }

        for (uint32_t i = 0; i < pVps->vps_total_num_olss; i++)
        {
            if (pVps->vps_num_ptls_minus1 > 0 && pVps->vps_num_ptls_minus1 + 1 != pVps->vps_total_num_olss)
            {
                pVps->vps_ols_ptl_idx[i] = GetBits(8);
            }
            else if (pVps->vps_num_ptls_minus1 + 1 == pVps->vps_total_num_olss)
            {
                pVps->vps_ols_ptl_idx[i] = i;
            }
            else
            {
                pVps->vps_ols_ptl_idx[i] = 0;
            }
            isPTLReferred[pVps->vps_ols_ptl_idx[i]] = true;
        }

        for (uint32_t i = 0; i < pVps->vps_num_ptls_minus1 + 1; i++)
        {
            if (!isPTLReferred[i])
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
        }

        if (!pVps->vps_each_layer_is_an_ols_flag)
        {
            pVps->vps_num_dpb_params_minus1 = GetVLCElementU();
            if (pVps->vps_num_dpb_params_minus1 + 1 > pVps->vps_num_multi_layered_olss)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            std::vector<bool> isDPBParamReferred((uint64_t)pVps->vps_num_dpb_params_minus1 + 1, false);

            if (pVps->vps_max_sublayers_minus1 > 0)
            {
                pVps->vps_sublayer_dpb_params_present_flag = Get1Bit();
            }
            pVps->dpb_parameters.resize((uint64_t)pVps->vps_num_dpb_params_minus1 + 1);

            for (uint32_t i = 0; i <= pVps->vps_num_dpb_params_minus1; i++)
            {
                if (!pVps->vps_default_ptl_dpb_hrd_max_tid_flag)
                {
                    uint32_t uiCode = GetBits(3);
                    pVps->vps_dpb_max_tid.push_back(uiCode);
                    if (uiCode > pVps->vps_max_sublayers_minus1)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                }
                else
                {
                    pVps->vps_dpb_max_tid.push_back(pVps->vps_max_sublayers_minus1);
                }

                VVCDpbParameter dpb_parameter_temp;
                xParseDpbParameters(&dpb_parameter_temp, pVps->vps_dpb_max_tid[i], pVps->vps_sublayer_dpb_params_present_flag);
                pVps->dpb_parameters.push_back(dpb_parameter_temp);
            }

            uint32_t j = 0;

            for (uint32_t i = 0; i < pVps->vps_total_num_olss; i++)
            {
                if (pVps->vps_num_layers_in_ols[i] > 1)
                {
                    pVps->vps_ols_dpb_pic_width[i] = GetVLCElementU();
                    pVps->vps_ols_dpb_pic_height[i] = GetVLCElementU();
                    pVps->vps_ols_dpb_chroma_format[i] = GetBits(2);
                    pVps->vps_ols_dpb_bitdepth_minus8[i] = GetVLCElementU();

                    if ((pVps->vps_num_dpb_params_minus1 > 0) && (pVps->vps_num_dpb_params_minus1 + 1 != pVps->vps_num_multi_layered_olss))
                    {
                        pVps->vps_ols_dpb_params_idx[i] = GetVLCElementU();
                    }
                    else if (pVps->vps_num_dpb_params_minus1 == 0)
                    {
                        pVps->vps_ols_dpb_params_idx[i] = 0;
                    }
                    else
                    {
                        pVps->vps_ols_dpb_params_idx[i] = j;
                    }

                    j += 1;
                    isDPBParamReferred[pVps->vps_ols_dpb_params_idx[i]] = true;
                }
            }

            for (uint32_t i = 0; i < pVps->vps_num_dpb_params_minus1 + 1; i++)
            {
                if (!isDPBParamReferred[i])
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
            }

            pVps->vps_timing_hrd_params_present_flag = Get1Bit();

            if (pVps->vps_timing_hrd_params_present_flag)
            {
                xParseGeneralTimingHrdParameters(&pVps->general_timing_hrd_parameters);
                if (pVps->vps_max_sublayers_minus1 > 0)
                {
                    pVps->vps_sublayer_cpb_params_present_flag = Get1Bit();
                }
                else
                {
                    pVps->vps_sublayer_cpb_params_present_flag = 0;
                }

                pVps->vps_num_ols_timing_hrd_params_minus1 = GetVLCElementU();

                if (pVps->vps_num_ols_timing_hrd_params_minus1 >= pVps->vps_num_multi_layered_olss)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }

                std::vector<bool> isHRDParamReferred((uint64_t)pVps->vps_num_ols_timing_hrd_params_minus1 + 1, false);
                pVps->ols_timing_hrd_parameters.resize(pVps->vps_num_ols_timing_hrd_params_minus1);

                for (uint32_t i = 0; i <= pVps->vps_num_ols_timing_hrd_params_minus1; i++)
                {
                    if (!pVps->vps_default_ptl_dpb_hrd_max_tid_flag)
                    {
                        pVps->vps_hrd_max_tid[i] = GetBits(3);
                        if (pVps->vps_hrd_max_tid[i] > pVps->vps_max_sublayers_minus1)
                        {
                            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                        }
                    }
                    else
                    {
                        pVps->vps_hrd_max_tid[i] = pVps->vps_max_sublayers_minus1;
                    }
                    uint32_t firstSubLayer = pVps->vps_sublayer_cpb_params_present_flag ? 0 : pVps->vps_hrd_max_tid[i];
                    VVCOlsTimingHrdParams ols_timing_hrd_parameters;
                    xParseOlsTimingHrdParameters(&pVps->general_timing_hrd_parameters,
                                                 &ols_timing_hrd_parameters,
                                                 firstSubLayer,
                                                 pVps->vps_hrd_max_tid[i]);
                    pVps->ols_timing_hrd_parameters.push_back(ols_timing_hrd_parameters);
                }

                for (uint32_t i = pVps->vps_num_ols_timing_hrd_params_minus1 + 1; i < pVps->vps_total_num_olss; i++)
                {
                    pVps->vps_hrd_max_tid[i] = pVps->vps_max_sublayers_minus1;
                }

                for (uint32_t i = 0; i < pVps->vps_num_multi_layered_olss; i++)
                {
                    if (pVps->vps_num_ols_timing_hrd_params_minus1 > 0 &&
                        pVps->vps_num_ols_timing_hrd_params_minus1 + 1 != pVps->vps_num_multi_layered_olss)
                    {
                        pVps->vps_ols_timing_hrd_idx[i] = GetVLCElementU();
                        if (pVps->vps_ols_timing_hrd_idx[i] > pVps->vps_num_ols_timing_hrd_params_minus1)
                        {
                            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                        }
                    }
                    else if (pVps->vps_num_ols_timing_hrd_params_minus1 == 0)
                    {
                        pVps->vps_ols_timing_hrd_idx[i] = 0;
                    }
                    else
                    {
                        pVps->vps_ols_timing_hrd_idx[i] = i;
                    }
                    isHRDParamReferred[pVps->vps_ols_timing_hrd_idx[i]] = true;
                }

                for (uint32_t i = 0; i <= pVps->vps_num_ols_timing_hrd_params_minus1; i++)
                {
                    if (!isHRDParamReferred[i])
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                }
            }
            else
            {
                for (uint32_t i = 0; i < pVps->vps_total_num_olss; i++)
                {
                    pVps->vps_hrd_max_tid[i] = pVps->vps_max_sublayers_minus1;
                }
            }
        }

        pVps->vps_extension_flag = Get1Bit();

        if (pVps->vps_extension_flag)
        {
            while (MoreRbspData())
            {
                pVps->vps_extension_data_flag = Get1Bit();
            }
        }
        ReadOutTrailingBits();

        pVps->vps_target_ols_idx = xDeriveTargetOLSIdx(pVps);

        return sts;
    }


    UMC::Status VVCHeadersBitstream::GetAdaptionParamSet(VVCAPS* aps)
    {
        if (!aps)
        {
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);
        }

        UMC::Status sts = UMC::UMC_OK;

        aps->aps_params_type = (ApsType)GetBits(3);
        aps->aps_adaptation_parameter_set_id = GetBits(5);
        aps->aps_chroma_present_flag = Get1Bit();
        if (aps->aps_params_type == ALF_APS)
        {
            if (aps->aps_adaptation_parameter_set_id > VVC_APS_RANGE)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            ParseAlfAps(aps);
        }
        else if (aps->aps_params_type == LMCS_APS)
        {
            if (aps->aps_adaptation_parameter_set_id > VVC_APS_LMCS_RANGE)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            ParseLMCS(aps);
        }
        else if (aps->aps_params_type == SCALING_LIST_APS)
        {
            if (aps->aps_adaptation_parameter_set_id > VVC_APS_RANGE)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            ScalingList& info = aps->scalingListInfo;
            ParseScalingList(&info, aps);
        }
        aps->aps_extension_flag = Get1Bit();
        if (aps->aps_extension_flag)
        {
            while (MoreRbspData())
            {
                aps->aps_extension_data_flag = Get1Bit();
            }
        }
        ReadOutTrailingBits();
        return sts;
    }

    void VVCHeadersBitstream::ParseAlfAps(VVCAPS* aps)
    {
        aps->alf_luma_filter_signal_flag = Get1Bit();
        if (aps->aps_chroma_present_flag)
        {
            aps->alf_chroma_filter_signal_flag = Get1Bit();
            aps->alf_cc_cb_filter_signal_flag = Get1Bit();
            aps->alf_cc_cr_filter_signal_flag = Get1Bit();
        }
        if (!aps->alf_luma_filter_signal_flag && !aps->alf_chroma_filter_signal_flag &&
            !aps->alf_cc_cb_filter_signal_flag && !aps->alf_cc_cr_filter_signal_flag)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        aps->alf_luma_coeff_delta_idx.resize(VVC_MAX_NUM_ALF_CLASSES);
        if (aps->alf_luma_filter_signal_flag)
        {
            aps->alf_luma_clip_flag = Get1Bit();
            aps->alf_luma_num_filters_signalled_minus1 = GetVLCElementU();
            uint32_t numLumaFilters = aps->alf_luma_num_filters_signalled_minus1 + 1;
            if (aps->alf_luma_num_filters_signalled_minus1 > 0)
            {
                uint32_t CeilLog2Luma = CeilLog2(numLumaFilters);
                for (uint32_t filtIdx = 0; filtIdx < VVC_MAX_NUM_ALF_CLASSES; filtIdx++)
                {
                    aps->alf_luma_coeff_delta_idx[filtIdx] = GetBits(CeilLog2Luma);
                }
            }
            alfFilter(aps, false, 0);
        }
        if (aps->alf_chroma_filter_signal_flag)
        {
            aps->alf_chroma_clip_flag = Get1Bit();
            aps->alf_chroma_num_alt_filters_minus1 = GetVLCElementU();
            for (uint32_t altIdx = 0; altIdx < aps->alf_chroma_num_alt_filters_minus1 + 1; altIdx++)
            {
                alfFilter(aps, true, altIdx);
            }
        }
        for (uint32_t ccIdx = 0; ccIdx < VVC_MAX_CC_ID_COEFF; ccIdx++)
        {
            bool alf_cc_filter_signal_flag = (ccIdx ? aps->alf_cc_cr_filter_signal_flag : aps->alf_cc_cb_filter_signal_flag);
            if (alf_cc_filter_signal_flag) 
            {
                if (ccIdx)
                {
                    aps->alf_cc_cr_filters_signalled_minus1 = GetVLCElementU();
                    aps->alf_cc_filters_signalled_minus1 = aps->alf_cc_cr_filters_signalled_minus1;
                }
                else
                {
                    aps->alf_cc_cb_filters_signalled_minus1 = GetVLCElementU();
                    aps->alf_cc_filters_signalled_minus1 = aps->alf_cc_cb_filters_signalled_minus1;
                }
                for (uint32_t k = 0; k < aps->alf_cc_filters_signalled_minus1 + 1; k++)
                {
                    aps->alf_cc_filter_Idx_enabled[ccIdx][k] = true;
                    uint32_t* coeff = aps->alf_cc_coeff[ccIdx][k];
                    for (uint32_t j = 0; j < VVC_MAX_CC_ALF_COEFF; j++)
                    {
                        coeff[j] = 0;
                        if (ccIdx)
                        {
                            aps->alf_cc_cr_mapped_coeff_abs[k][j] = GetBits(3);
                            coeff[j] = 1 << (aps->alf_cc_cr_mapped_coeff_abs[k][j] - 1);
                            if (aps->alf_cc_cr_mapped_coeff_abs[k][j])
                            {
                                aps->alf_cc_cr_coeff_sign[k][j] = GetBits(1);
                                coeff[j] *= 1 - 2 * aps->alf_cc_cr_coeff_sign[k][j];
                            }
                            aps->CcAlfApsCoeffCr[k][j] = coeff[j];
                        }
                        else
                        {
                            aps->alf_cc_cb_mapped_coeff_abs[k][j] = GetBits(3);
                            coeff[j] = 1 << (aps->alf_cc_cb_mapped_coeff_abs[k][j] - 1);
                            if (aps->alf_cc_cb_mapped_coeff_abs[k][j])
                            {
                                aps->alf_cc_cb_coeff_sign[k][j] = GetBits(1);
                                coeff[j] *= 1 - 2 * aps->alf_cc_cb_coeff_sign[k][j];
                            }
                            aps->CcAlfApsCoeffCb[k][j] = coeff[j];
                        }
                    }
                }
                for (int filterIdx = aps->alf_cc_filters_signalled_minus1 + 1; filterIdx < VVC_MAX_CC_ALF_FILTERS; filterIdx++) 
                {
                    aps->alf_cc_filter_Idx_enabled[ccIdx][filterIdx] = false;
                }
            }
        }
    }

    //protected member Function
    void VVCHeadersBitstream::alfFilter(VVCAPS* aps, const bool isChroma, const int altIdx)
    {
        uint32_t alf_num_filters_minus1 = isChroma ? 1 : (aps->alf_luma_num_filters_signalled_minus1 + 1);
        uint32_t numCoeff = isChroma ? 6 : 12;
        for (uint32_t Idx = 0; Idx < alf_num_filters_minus1; Idx++)
        {
            for (uint32_t j = 0; j < numCoeff; j++)
            {
                if (isChroma)
                {
                    aps->alf_chroma_coeff_abs[altIdx][j] = GetVLCElementU();
                    if (aps->alf_chroma_coeff_abs[altIdx][j] > 0)
                    {
                        aps->alf_chroma_coeff_sign[altIdx][j] = Get1Bit();
                    }
                }
                else
                {
                    aps->alf_luma_coeff_abs[Idx][j] = GetVLCElementU();
                    if (aps->alf_luma_coeff_abs[Idx][j] > 0)
                    {
                        aps->alf_luma_coeff_sign[Idx][j] = Get1Bit();
                    }
                    if (aps->alf_luma_coeff_sign[Idx][j] > VVC_ALF_LUMA_COEFF_UPPER_BOUND || aps->alf_luma_coeff_sign[Idx][j] < VVC_ALF_LUMA_COEFF_LOWER_BOUND)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                }
            }
        }
        // Clipping values coding
        for (uint32_t Idx = 0; Idx < alf_num_filters_minus1; Idx++)
        {
            for (uint32_t j = 0; j < numCoeff; j++)
            {
                bool alf_clip_flag = isChroma ? aps->alf_chroma_clip_flag : aps->alf_luma_clip_flag;
                if (alf_clip_flag)
                {
                    (isChroma ? aps->alf_chroma_clip_idx[altIdx][j] : aps->alf_luma_clip_idx[Idx][j]) = GetBits(2);
                }
            }
        }
    }

    void VVCHeadersBitstream::ParseLMCS(VVCAPS* aps)
    {
        aps->lmcs_min_bin_idx = GetVLCElementU();
        aps->lmcs_delta_max_bin_idx = GetVLCElementU();
        aps->lmcs_delta_cw_prec_minus1 = GetVLCElementU();
        uint32_t maxNbitsDeltaCW = aps->lmcs_delta_cw_prec_minus1 + 1;
        uint32_t LmcsMaxBinIdx = 16 - 1 - aps->lmcs_delta_max_bin_idx;
        for (uint32_t i = aps->lmcs_min_bin_idx; i <= LmcsMaxBinIdx; i++)
        {
            aps->lmcs_delta_abs_cw[i] = GetBits(maxNbitsDeltaCW);
            int absCW = aps->lmcs_delta_abs_cw[i];
            if (absCW > 0)
            {
                aps->lmcs_delta_sign_cw_flag[i] = Get1Bit();
            }
        }
        if (aps->aps_chroma_present_flag)
        {
            aps->lmcs_delta_abs_crs = GetBits(3);
            if (aps->lmcs_delta_abs_crs > 0)
            {
                aps->lmcs_delta_sign_crs_flag = Get1Bit();
            }
        }
    }

    void VVCHeadersBitstream::decodeScalingList(ScalingList* scalingList,
                                                uint32_t scalingListId,
                                                uint32_t matrixSize,
                                                bool isPred)
    {
        int32_t nextCoef = (isPred) ? 0 : SCALING_LIST_START_VALUE;
        ScanElement* scan = VVCScanOrderTables::getInstance().GetScanOrder(SCAN_UNGROUPED, SCAN_DIAG,
            VVCScanOrderTables::GetBLKWidthIndex(matrixSize), VVCScanOrderTables::GetBLKHeightIndex(matrixSize));
        int* dst = &scalingList->scaling_list_coef[scalingListId][0];
        uint32_t PredListId = scalingList->ref_matrix_id[scalingListId];
        if (isPred && PredListId > scalingListId)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        uint32_t sizeId = (scalingListId < SCALING_LIST_1D_START_8x8) ? 2 : 3;
        bool scaling_list_TS_default_addr = (sizeId == SCALING_LIST_1x1 || sizeId == SCALING_LIST_2x2 || sizeId == SCALING_LIST_4x4);
        const int* scaling_list_default_addr = scaling_list_TS_default_addr ? quantTSDefault4x4 : quantInterDefault8x8;
        const int* srcPred = (isPred)? ((scalingListId == PredListId) ? scaling_list_default_addr : &scalingList->scaling_list_coef[PredListId][0]) : nullptr;
        if (isPred && scalingListId == PredListId)
        {
            scalingList->scaling_list_DC[PredListId] = SCALING_LIST_DC;
        }
        int32_t predCoef = 0;

        if (scalingListId >= SCALING_LIST_1D_START_16x16)
        {
            int32_t scaling_list_dc_coef_minus8 = GetVLCElementS();
            nextCoef += scaling_list_dc_coef_minus8;
            if (isPred)
            {
                predCoef = (PredListId >= SCALING_LIST_1D_START_16x16) ? scalingList->scaling_list_DC[PredListId] : srcPred[0];
            }
            scalingList->scaling_list_DC[scalingListId] =  (nextCoef + predCoef + 256) & 255;
        }

        for (uint32_t i = 0; i < matrixSize * matrixSize; i++)
        {
            if (scalingListId >= SCALING_LIST_1D_START_64x64 && scan[i].x >= 4 && scan[i].y >= 4)
            {
                dst[scan[i].idx] = 0;
                continue;
            }
            int32_t scaling_list_delta_coef = GetVLCElementS();
            nextCoef += scaling_list_delta_coef;
            predCoef = isPred ? srcPred[scan[i].idx] : 0;
            dst[scan[i].idx] = (nextCoef + predCoef + 256) & 255;
        }
    }

    void VVCHeadersBitstream::updateScalingList(ScalingList* scalingList,
                                                uint32_t matrixSize,
                                                uint32_t scalingListId) 
    {
        ScanElement* scan = VVCScanOrderTables::getInstance().GetScanOrder(SCAN_UNGROUPED, SCAN_DIAG,
            VVCScanOrderTables::GetBLKWidthIndex(matrixSize), VVCScanOrderTables::GetBLKHeightIndex(matrixSize));
        int* src = &scalingList->scaling_list_coef[scalingListId][0];
        uint32_t coefNum = matrixSize * matrixSize;
        if (scalingListId >= SCALING_LIST_1D_START_16x16) 
        {
            scalingList->ScalingMatrixDCRec[scalingListId - 14] = scalingList->scaling_list_DC[scalingListId];
        }
        for (uint8_t i = 0; i < coefNum; i++) 
        {
            if (scalingListId < SCALING_LIST_1D_START_4x4)
            {
                scalingList->ScalingMatrixRec2x2[scalingListId][scan[i].x][scan[i].y] = src[scan[i].idx];
            }
            else if (scalingListId < SCALING_LIST_1D_START_8x8)
            {
                scalingList->ScalingMatrixRec4x4[scalingListId - SCALING_LIST_1D_START_4x4][scan[i].x][scan[i].y] = src[scan[i].idx];
            }
            else
            {
                scalingList->ScalingMatrixRec8x8[scalingListId - SCALING_LIST_1D_START_8x8][scan[i].x][scan[i].y] = src[scan[i].idx];
            }
        }
    }

    void VVCHeadersBitstream::ParseScalingList(ScalingList* scalingList, VVCAPS* aps)
    {
        bool aps_chromaPrsentFlag = aps->aps_chroma_present_flag;
        bool scalingListCopyModeFlag;

        scalingList->chroma_scaling_list_present_flag = aps_chromaPrsentFlag;
        for (uint8_t scalingListId = 0; scalingListId < VVC_MAX_SCALING_LIST_ID; scalingListId++) 
        {
            uint32_t matrixSize = (scalingListId < SCALING_LIST_1D_START_4x4) ? 2 : (scalingListId < SCALING_LIST_1D_START_8x8) ? 4 : 8;
            scalingList->scaling_list_coef[scalingListId].resize(matrixSize * matrixSize);
        }

        for (uint8_t scalingListId = 0; scalingListId < VVC_MAX_SCALING_LIST_ID; scalingListId++)
        {
            uint32_t matrixSize = (scalingListId < SCALING_LIST_1D_START_4x4) ? 2 : (scalingListId < SCALING_LIST_1D_START_8x8) ? 4 : 8;
            bool isLumaScalingList = scalingListId % MAX_NUM_COMPONENT == SCALING_LIST_1D_START_4x4 || scalingListId == SCALING_LIST_1D_START_64x64 + 1;
            if (aps_chromaPrsentFlag || isLumaScalingList)
            {
                uint8_t scaling_list_copy_mode_flag = Get1Bit();
                scalingListCopyModeFlag = (scaling_list_copy_mode_flag) ? true : false;
                scalingList->scaling_list_pred_mode_flag_is_copy[scalingListId] = scalingListCopyModeFlag;
                if (!scalingListCopyModeFlag)
                {
                    uint8_t scaling_list_pred_mode_flag = Get1Bit();
                    scalingList->scaling_list_pred_mode_flag[scalingListId] = scaling_list_pred_mode_flag;
                }
                if ((scalingListCopyModeFlag || scalingList->scaling_list_pred_mode_flag[scalingListId])
                    && scalingListId != SCALING_LIST_1D_START_2x2 && scalingListId != SCALING_LIST_1D_START_4x4
                    && scalingListId != SCALING_LIST_1D_START_8x8)
                {
                    uint32_t scaling_list_pred_matrix_id_delta = GetVLCElementU();
                    scalingList->ref_matrix_id[scalingListId] = (uint32_t)((int)(scalingListId)-(scaling_list_pred_matrix_id_delta));
                }
                else if (scalingListCopyModeFlag || scalingList->scaling_list_pred_mode_flag[scalingListId])
                {
                    scalingList->ref_matrix_id[scalingListId] = (uint32_t)((int)(scalingListId));
                }
                if (scalingListCopyModeFlag)
                {
                    if (scalingListId >= SCALING_LIST_1D_START_16x16)
                    {
                        scalingList->scaling_list_DC[scalingListId] = ((scalingListId == scalingList->ref_matrix_id[scalingListId]) ? 16
                                : (scalingList->ref_matrix_id[scalingListId] < SCALING_LIST_1D_START_16x16)
                                ? scalingList->scaling_list_coef[scalingList->ref_matrix_id[scalingListId]][0]
                                : scalingList->scaling_list_DC[scalingList->ref_matrix_id[scalingListId]]);
                    }
                    uint32_t refListId = scalingList->ref_matrix_id[scalingListId];
                    uint32_t refSizeId = (refListId < SCALING_LIST_1D_START_8x8) ? 2 : 3;
                    bool scaling_list_ref_TS_default_addr = (refSizeId == SCALING_LIST_1x1 || refSizeId == SCALING_LIST_2x2 || refSizeId == SCALING_LIST_4x4);
                    const int* scaling_list_ref_addr = scaling_list_ref_TS_default_addr ? quantTSDefault4x4 : quantInterDefault8x8;
                    if (scaling_list_ref_addr == nullptr)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                    MFX_INTERNAL_CPY(&scalingList->scaling_list_coef[scalingListId][0],
                        ((scalingListId == scalingList->ref_matrix_id[scalingListId])
                            ? scaling_list_ref_addr : &scalingList->scaling_list_coef[refListId][0]),
                        sizeof(int) * matrixSize * matrixSize);
                }
                else
                {
                    decodeScalingList(scalingList, scalingListId, matrixSize, scalingList->scaling_list_pred_mode_flag[scalingListId]);
                }
            }
            else
            {
                scalingListCopyModeFlag = true;
                scalingList->scaling_list_pred_mode_flag_is_copy[scalingListId] = scalingListCopyModeFlag;
                scalingList->ref_matrix_id[scalingListId] = (uint32_t)((int)(scalingListId));
                if (scalingListId >= SCALING_LIST_1D_START_16x16)
                {
                    scalingList->scaling_list_DC[scalingListId]= 16;
                }
                uint32_t refListId = scalingList->ref_matrix_id[scalingListId];
                uint32_t refSizeId = (refListId < SCALING_LIST_1D_START_8x8) ? 2 : 3;
                bool scaling_list_ref_TS_default_addr = (refSizeId == SCALING_LIST_1x1 || refSizeId == SCALING_LIST_2x2 || refSizeId == SCALING_LIST_4x4);
                const int* scaling_list_ref_addr = scaling_list_ref_TS_default_addr ? quantTSDefault4x4 : quantInterDefault8x8;
                if (scaling_list_ref_addr == nullptr) 
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                MFX_INTERNAL_CPY(&scalingList->scaling_list_coef[scalingListId][0],
                    ((scalingListId == scalingList->ref_matrix_id[scalingListId]) 
                   ? scaling_list_ref_addr : &scalingList->scaling_list_coef[refListId][0]),
                    sizeof(int) * matrixSize * matrixSize);
            }
            updateScalingList(scalingList, matrixSize, scalingListId);
        }
    }

    void VVCHeadersBitstream::xParseVUI(VVCSeqParamSet *pSps, VVCVUI *pVUI)
    {
        uint64_t numBitsDecodedStart = BitsDecoded();
        uint32_t vuiPayloadSize = (pSps->sps_vui_payload_size_minus1 + 1) * 8;
        uint32_t numBitUsed = 0;
        pVUI->vui_progressive_source_flag = Get1Bit();
        pVUI->vui_interlaced_source_flag = Get1Bit();
        pVUI->vui_non_packed_constraint_flag = Get1Bit();
        pVUI->vui_non_projected_constraint_flag = Get1Bit();
        pVUI->vui_aspect_ratio_info_present_flag = Get1Bit();
        numBitUsed += 5;
        if (pVUI->vui_aspect_ratio_info_present_flag)
        {
            pVUI->vui_aspect_ratio_constant_flag = Get1Bit();
            pVUI->vui_aspect_ratio_idc = GetBits(8);
            numBitUsed += 9;
            if (pVUI->vui_aspect_ratio_idc == 255)
            {
                pVUI->vui_sar_width = GetBits(16);
                pVUI->vui_sar_height = GetBits(16);
                numBitUsed += 32;
            }
        }
        pVUI->vui_overscan_info_present_flag = Get1Bit();
        numBitUsed++;
        if (pVUI->vui_overscan_info_present_flag)
        {
            pVUI->vui_overscan_appropriate_flag = Get1Bit();
            numBitUsed++;
        }
        pVUI->vui_colour_description_present_flag = Get1Bit();
        numBitUsed++;
        if (pVUI->vui_colour_description_present_flag)
        {
            pVUI->vui_colour_primaries = GetBits(8);
            pVUI->vui_transfer_characteristics = GetBits(8);
            pVUI->vui_matrix_coeffs = GetBits(8);
            pVUI->vui_full_range_flag = Get1Bit();
            numBitUsed += 25;
        }
        pVUI->vui_chroma_loc_info_present_flag = Get1Bit();
        numBitUsed++;
        if (pVUI->vui_chroma_loc_info_present_flag)
        {
            if (pVUI->vui_progressive_source_flag && !pVUI->vui_interlaced_source_flag)
            {
                pVUI->vui_chroma_sample_loc_type = GetVLCElementU();
            }
            else
            {
                pVUI->vui_chroma_sample_loc_type_top_field = GetVLCElementU();
                pVUI->vui_chroma_sample_loc_type_bottom_field = GetVLCElementU();
            }
        }
        uint64_t numBitsDecodedEnd = BitsDecoded();
        int64_t payloadBitsRem = vuiPayloadSize - (numBitsDecodedEnd - numBitsDecodedStart);
        for (int64_t i = 0; i < payloadBitsRem; i++)
        {
            Get1Bit();
        }
    }

    void VVCHeadersBitstream::xSetRefPicIdentifier(VVCReferencePictureList *pRPL,
                                                   uint32_t idx,
                                                   uint32_t identifier,
                                                   bool isLongterm,
                                                   bool isInterLayerRefPic,
                                                   uint32_t interLayerIdx)
    {
        pRPL->ref_pic_identifier[idx] = identifier;
        pRPL->is_long_term_ref_pic[idx] = isLongterm;
        pRPL->delta_poc_msb_present_flag[idx] = false;
        pRPL->delta_poc_msb_cycle_lt[idx] = 0;
        pRPL->is_inter_layer_ref_pic[idx] = isInterLayerRefPic;
        pRPL->inter_layer_ref_pic_idx[idx] = interLayerIdx;
    }

    void VVCHeadersBitstream::xDestroyTileSliceInfo(VVCPicParamSet* pps)
    {
        for (uint32_t i = 0; i < pps->pps_rect_slices.size(); i++)
        {
            delete pps->pps_rect_slices[i];
            pps->pps_rect_slices[i] = nullptr;
        }
        for (uint32_t j = 0; j < pps->pps_slice_map.size(); j++)
        {
            delete pps->pps_slice_map[j];
            pps->pps_slice_map[j] = nullptr;
        }
        for (uint32_t k = 0; k < pps->pps_sub_pics.size(); k++)
        {
            delete pps->pps_sub_pics[k];
            pps->pps_sub_pics[k] = nullptr;
        }
    }

    void VVCHeadersBitstream::xResetTileSliceInfo(VVCPicParamSet* pps)
    {
        pps->pps_num_exp_tile_columns_minus1 = 0;
        pps->pps_num_exp_tile_rows_minus1 = 0;
        pps->pps_num_tile_cols = 0;
        pps->pps_num_tile_rows = 0;
        pps->pps_num_slices_in_pic_minus1 = 0;
        pps->pps_tile_column_width.clear();
        pps->pps_tile_row_height.clear();
        pps->pps_tile_col_bd.clear();
        pps->pps_tile_row_bd.clear();
        pps->pps_ctu_to_tile_col.clear();
        pps->pps_ctu_to_tile_row.clear();
        pps->pps_ctu_to_subpic_idx.clear();
        xDestroyTileSliceInfo(pps);
        pps->pps_rect_slices.clear();
        pps->pps_slice_map.clear();
        pps->pps_sub_pics.clear();
    }

    void VVCHeadersBitstream::xInitRectSliceMap(VVCPicParamSet* pps, const VVCSeqParamSet* sps)
    {
        if (sps) 
        {
            pps->pps_ctu_to_subpic_idx.resize(pps->pps_pic_width_in_ctu * pps->pps_pic_height_in_ctu);
            if (sps->sps_num_subpics_minus1 > 0) 
            {
                for (uint32_t i = 0; i <= sps->sps_num_subpics_minus1; i++) 
                {
                    for (uint32_t y = sps->sps_subpic_ctu_top_left_y[i]; y < sps->sps_subpic_ctu_top_left_y[i] + sps->sps_subpic_height_minus1[i] + 1; y++) 
                    {
                        for (uint32_t x = sps->sps_subpic_ctu_top_left_x[i]; x < sps->sps_subpic_ctu_top_left_x[i] + sps->sps_subpic_width_minus1[i] + 1; x++) 
                        {
                            pps->pps_ctu_to_subpic_idx[x + y * pps->pps_pic_width_in_ctu] = i;
                        }
                    }
                }
            }
            else 
            {
                for (uint32_t i = 0; i < pps->pps_pic_width_in_ctu * pps->pps_pic_height_in_ctu; i++)
                {
                    pps->pps_ctu_to_subpic_idx[i] = 0;
                }
            }
        }

        uint32_t ctuY;
        uint32_t tileX, tileY;
        if (pps->pps_single_slice_per_subpic_flag)
        {
            if(sps)
                pps->pps_num_slices_in_pic = sps->sps_num_subpics_minus1 + 1;

            for (uint32_t i = 0; i < pps->pps_num_slices_in_pic; i++)
            {
                pps->pps_slice_map.push_back(new SliceMap);
            }
            if (pps->pps_num_slices_in_pic > 1)
            {
                std::vector<uint32_t> subpicWidthInTiles;
                std::vector<uint32_t> subpicHeightInTiles;
                std::vector<uint32_t> subpicHeightLessThanOneTileFlag;
                subpicWidthInTiles.resize(pps->pps_num_slices_in_pic);
                subpicHeightInTiles.resize(pps->pps_num_slices_in_pic);
                subpicHeightLessThanOneTileFlag.resize(pps->pps_num_slices_in_pic);
                for (uint32_t i = 0; i < pps->pps_num_slices_in_pic; i++)
                {
                    uint32_t leftX = sps->sps_subpic_ctu_top_left_x[i];
                    uint32_t rightX = leftX + sps->sps_subpic_width_minus1[i];
                    subpicWidthInTiles[i] = pps->pps_ctu_to_tile_col[rightX] + 1 - pps->pps_ctu_to_tile_col[leftX];

                    uint32_t topY = sps->sps_subpic_ctu_top_left_y[i];
                    uint32_t bottomY = topY + sps->sps_subpic_height_minus1[i];
                    subpicHeightInTiles[i] = pps->pps_ctu_to_tile_row[bottomY] + 1 - pps->pps_ctu_to_tile_row[topY];

                    if (subpicHeightInTiles[i] == 1 &&
                        ((sps->sps_subpic_height_minus1[i] + 1) < pps->pps_tile_row_height[pps->pps_ctu_to_tile_row[topY]]))
                    {
                        subpicHeightLessThanOneTileFlag[i] = 1;
                    }
                    else
                    {
                        subpicHeightLessThanOneTileFlag[i] = 0;
                    }
                }

                for (uint32_t i = 0; i < pps->pps_num_slices_in_pic; i++)
                {
                    pps->pps_slice_map[i]->ctu_addr_in_slice.clear();
                    pps->pps_slice_map[i]->num_ctu_in_slice = 0;
                    pps->pps_slice_map[i]->num_tiles_in_slice = 0;
                    pps->pps_slice_map[i]->slice_id = 0;
                    if (subpicHeightLessThanOneTileFlag[i])
                    {
                        for (uint32_t ctbY = sps->sps_subpic_ctu_top_left_y[i]; ctbY < sps->sps_subpic_ctu_top_left_y[i] + sps->sps_subpic_height_minus1[i] + 1; ctbY++)
                        {
                            for (uint32_t ctbX = sps->sps_subpic_ctu_top_left_x[i]; ctbX < sps->sps_subpic_ctu_top_left_x[i] + sps->sps_subpic_width_minus1[i] + 1; ctbX++)
                            {
                                pps->pps_slice_map[i]->ctu_addr_in_slice.push_back(ctbY * pps->pps_pic_width_in_ctu + ctbX);
                                pps->pps_slice_map[i]->num_ctu_in_slice++;
                            }
                        }
                    }
                    else
                    {
                        tileX = pps->pps_ctu_to_tile_col[sps->sps_subpic_ctu_top_left_x[i]];
                        tileY = pps->pps_ctu_to_tile_row[sps->sps_subpic_ctu_top_left_y[i]];
                        for (uint32_t j = 0; j < subpicHeightInTiles[i]; j++)
                        {
                            for (uint32_t k = 0; k < subpicWidthInTiles[i]; k++)
                            {
                                for (uint32_t ctbY = pps->pps_tile_row_bd[tileY + j]; ctbY < pps->pps_tile_row_bd[tileY + j + 1]; ctbY++)
                                {
                                    for (uint32_t ctbX = pps->pps_tile_col_bd[tileX + k]; ctbX < pps->pps_tile_col_bd[tileX + k + 1]; ctbX++)
                                    {
                                        pps->pps_slice_map[i]->ctu_addr_in_slice.push_back(ctbY * pps->pps_pic_width_in_ctu + ctbX);
                                        pps->pps_slice_map[i]->num_ctu_in_slice++;
                                    }
                                }
                            }
                        }
                    }
                }
                subpicWidthInTiles.clear();
                subpicHeightInTiles.clear();
                subpicHeightLessThanOneTileFlag.clear();
            }
            else 
            {
                pps->pps_slice_map[0]->num_ctu_in_slice = 0;
                pps->pps_slice_map[0]->num_tiles_in_slice = 0;
                pps->pps_slice_map[0]->slice_id = 0;
                pps->pps_slice_map[0]->ctu_addr_in_slice.clear();
                for (uint32_t tiley = 0; tiley < pps->pps_num_tile_rows; tiley++) 
                {
                    for (uint32_t tilex = 0; tilex < pps->pps_num_tile_cols; tilex++) 
                    {
                        for (uint32_t ctbY = pps->pps_tile_row_bd[tiley]; ctbY < pps->pps_tile_row_bd[tiley + 1]; ctbY++)
                        {
                            for (uint32_t ctbX = pps->pps_tile_col_bd[tilex]; ctbX < pps->pps_tile_col_bd[tilex + 1]; ctbX++)
                            {
                                pps->pps_slice_map[0]->ctu_addr_in_slice.push_back(ctbY * pps->pps_pic_width_in_ctu + ctbX);
                                pps->pps_slice_map[0]->num_ctu_in_slice++;
                            }
                        }
                    }
                }
                pps->pps_slice_map[0]->slice_id = 0;
            }
        }
        else 
        {
            pps->pps_num_slices_in_pic = pps->pps_num_slices_in_pic_minus1 + 1;
            for (uint32_t i = 0; i < pps->pps_num_slices_in_pic; i++)
            {
                pps->pps_slice_map.push_back(new SliceMap);
            }
            for (uint32_t i = 0; i < pps->pps_num_slices_in_pic; i++)
            {
                pps->pps_slice_map[i]->ctu_addr_in_slice.clear();
                pps->pps_slice_map[i]->num_ctu_in_slice = 0;
                pps->pps_slice_map[i]->num_tiles_in_slice = 0;
                pps->pps_slice_map[i]->slice_id = 0;

                tileX = pps->pps_rect_slices[i]->pps_tile_idx % pps->pps_num_tile_cols;
                tileY = pps->pps_rect_slices[i]->pps_tile_idx / pps->pps_num_tile_cols;

                if (i == pps->pps_num_slices_in_pic - 1)
                {
                    pps->pps_rect_slices[i]->pps_slice_width_in_tiles_minus1 = pps->pps_num_tile_cols - tileX - 1;
                    pps->pps_rect_slices[i]->pps_slice_height_in_tiles_minus1 = pps->pps_num_tile_rows - tileY - 1;
                    pps->pps_rect_slices[i]->pps_num_slices_in_tile = 1;
                }
                pps->pps_slice_map[i]->slice_id = i;
                if (pps->pps_rect_slices[i]->pps_slice_width_in_tiles_minus1 > 0 || pps->pps_rect_slices[i]->pps_slice_height_in_tiles_minus1 > 0)
                {
                    for (uint32_t j = 0; j <= pps->pps_rect_slices[i]->pps_slice_height_in_tiles_minus1; j++)
                    {
                        for (uint32_t k = 0; k <= pps->pps_rect_slices[i]->pps_slice_width_in_tiles_minus1; k++)
                        {
                            for (uint32_t ctbY = pps->pps_tile_row_bd[tileY + j]; ctbY < pps->pps_tile_row_bd[tileY + j + 1]; ctbY++)
                            {
                                for (uint32_t ctbX = pps->pps_tile_col_bd[tileX + k]; ctbX < pps->pps_tile_col_bd[tileX + k + 1]; ctbX++)
                                {
                                    pps->pps_slice_map[i]->ctu_addr_in_slice.push_back(ctbY * pps->pps_pic_width_in_ctu + ctbX);
                                    pps->pps_slice_map[i]->num_ctu_in_slice++;
                                }
                            }
                        }
                    }
                }
                else 
                {
                    uint32_t numSlicesInTile = pps->pps_rect_slices[i]->pps_num_slices_in_tile;
                    ctuY = pps->pps_tile_row_bd[tileY];
                    for (uint32_t j = 0; j < numSlicesInTile - 1; j++) 
                    {
                        for (uint32_t ctbY = ctuY; ctbY < (ctuY + (pps->pps_rect_slices[i]->pps_slice_height_in_ctu_minus1 + 1)); ctbY++)
                        {
                            for (uint32_t ctbX = pps->pps_tile_col_bd[tileX]; ctbX < pps->pps_tile_col_bd[tileX + 1]; ctbX++)
                            {
                                pps->pps_slice_map[i]->ctu_addr_in_slice.push_back(ctbY * pps->pps_pic_width_in_ctu + ctbX);
                                pps->pps_slice_map[i]->num_ctu_in_slice++;
                            }
                        }
                        ctuY += (pps->pps_rect_slices[i]->pps_slice_height_in_ctu_minus1 + 1);
                        i++;
                        pps->pps_slice_map[i]->ctu_addr_in_slice.clear();
                        pps->pps_slice_map[i]->num_ctu_in_slice = 0;
                        pps->pps_slice_map[i]->num_tiles_in_slice = 0;
                        pps->pps_slice_map[i]->slice_id = i;
                    }
                    pps->pps_rect_slices[i]->pps_slice_height_in_ctu_minus1 = pps->pps_tile_row_bd[tileY + 1] - ctuY - 1;
                    for (uint32_t ctbY = ctuY; ctbY < pps->pps_tile_row_bd[tileY + 1]; ctbY++)
                    {
                        for (uint32_t ctbX = pps->pps_tile_col_bd[tileX]; ctbX < pps->pps_tile_col_bd[tileX + 1]; ctbX++)
                        {
                            pps->pps_slice_map[i]->ctu_addr_in_slice.push_back(ctbY * pps->pps_pic_width_in_ctu + ctbX);
                            pps->pps_slice_map[i]->num_ctu_in_slice++;
                        }
                    }
                }
            }
        }
    }

    void VVCHeadersBitstream::xParseRefPicListStruct(const VVCSeqParamSet *pSps, VVCReferencePictureList *pRPL, int32_t rplIdx)
    {
        pRPL->num_ref_entries = GetVLCElementU();
        uint32_t numStrp = 0;
        uint32_t numLtrp = 0;
        uint32_t numIlrp = 0;

        if (pSps->sps_long_term_ref_pics_flag && pRPL->num_ref_entries > 0 && rplIdx != -1)
        {
            pRPL->ltrp_in_header_flag = Get1Bit();
        }
        else if (pSps->sps_long_term_ref_pics_flag)
        {
            pRPL->ltrp_in_header_flag = true;
        }

        bool isLongTerm;
        int prevDelta = VVC_MAX_INT;
        int deltaValue = 0;
        bool firstSTRP = true;
        uint32_t absDeltaPocSt = 0;

        pRPL->inter_layer_present_flag = pSps->sps_inter_layer_prediction_enabled_flag;
        for (uint32_t i = 0; i < pRPL->num_ref_entries; i++)
        {
            pRPL->inter_layer_ref_pic_flag[i] = false;
            if (pRPL->inter_layer_present_flag)
            {
                pRPL->inter_layer_ref_pic_flag[i] = Get1Bit();
                if (pRPL->inter_layer_ref_pic_flag[i])
                {
                    pRPL->ilrp_idx[i] = GetVLCElementU();
                    xSetRefPicIdentifier(pRPL, i, 0, true, true, pRPL->ilrp_idx[i]);
                    numIlrp++;
                }
            }
            if (!pRPL->inter_layer_ref_pic_flag[i])
            {
                isLongTerm = false;
                if (pSps->sps_long_term_ref_pics_flag)
                {
                    pRPL->st_ref_pic_flag[i] = Get1Bit();
                    isLongTerm = !pRPL->st_ref_pic_flag[i];
                }
                else
                {
                    isLongTerm = false;
                }
                if (!isLongTerm)
                {
                    pRPL->abs_delta_poc_st[i] = GetVLCElementU();
                    absDeltaPocSt = pRPL->abs_delta_poc_st[i];
                    if ((!pSps->sps_weighted_pred_flag && !pSps->sps_weighted_bipred_flag) || (i == 0))
                    {
                        absDeltaPocSt++;
                    }
                    int readValue = absDeltaPocSt;
                    if (readValue > 0)
                    {
                        pRPL->strp_entry_sign_flag[i] = Get1Bit();
                        if (pRPL->strp_entry_sign_flag[i])
                        {
                            readValue = -readValue;
                        }
                    }
                    if (firstSTRP)
                    {
                        firstSTRP = false;
                        deltaValue = readValue;
                        prevDelta = readValue;
                    }
                    else
                    {
                        deltaValue = prevDelta + readValue;
                        prevDelta = deltaValue;
                    }
                    xSetRefPicIdentifier(pRPL, i, deltaValue, isLongTerm, false, 0);
                    numStrp++;
                }
                else
                {
                    bool flag = false;
                    uint32_t poc_lsb_lt = 0;
                    if (!pRPL->ltrp_in_header_flag)
                    {
                        flag = true;
                        pRPL->poc_lsb_lt[i] = GetBits(pSps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
                        poc_lsb_lt = pRPL->poc_lsb_lt[i];
                    }
                    xSetRefPicIdentifier(pRPL, i, poc_lsb_lt, isLongTerm, false, 0);
                    numLtrp++;
                }
            }
        }
        pRPL->number_of_short_term_pictures = numStrp;
        pRPL->number_of_long_term_pictures = numLtrp;
        pRPL->number_of_inter_layer_pictures = numIlrp;
    }

    void VVCHeadersBitstream::xCopyRefPicListStruct(VVCSeqParamSet *pSps,
                                                    VVCReferencePictureList *pSrcRPL,
                                                    VVCReferencePictureList *pDestRPL)
    {
        pDestRPL->number_of_short_term_pictures = pSrcRPL->number_of_short_term_pictures;
        pDestRPL->number_of_inter_layer_pictures = pSps->sps_inter_layer_prediction_enabled_flag ? 
                                                   pSrcRPL->number_of_inter_layer_pictures : 0;
        pDestRPL->num_ref_entries = pSrcRPL->num_ref_entries;

        if (pSps->sps_long_term_ref_pics_flag)
        {
            pDestRPL->ltrp_in_header_flag = pSrcRPL->ltrp_in_header_flag;
            pDestRPL->number_of_long_term_pictures = pSrcRPL->number_of_long_term_pictures;
        }
        else
        {
            pDestRPL->number_of_long_term_pictures = 0;
        }
        uint32_t numRefPic = pDestRPL->number_of_short_term_pictures + pDestRPL->number_of_long_term_pictures;
        for (uint32_t i = 0; i < numRefPic; i++)
        {
            xSetRefPicIdentifier(pDestRPL, i, pSrcRPL->ref_pic_identifier[i], pSrcRPL->is_long_term_ref_pic[i],
                                 pSrcRPL->is_inter_layer_ref_pic[i], pSrcRPL->inter_layer_ref_pic_idx[i]);

            pDestRPL->inter_layer_ref_pic_flag[i] = pSrcRPL->inter_layer_ref_pic_flag[i];
            pDestRPL->ilrp_idx[i]                 = pSrcRPL->ilrp_idx[i];
            pDestRPL->st_ref_pic_flag[i]          = pSrcRPL->st_ref_pic_flag[i];
            pDestRPL->abs_delta_poc_st[i]         = pSrcRPL->abs_delta_poc_st[i];
            pDestRPL->strp_entry_sign_flag[i]     = pSrcRPL->strp_entry_sign_flag[i];
            pDestRPL->DeltaPocValSt[i]            = pSrcRPL->DeltaPocValSt[i];
            pDestRPL->poc_lsb_lt[i]               = pSrcRPL->poc_lsb_lt[i];
        }
    }

    UMC::Status VVCHeadersBitstream::GetSequenceParamSet(VVCSeqParamSet *pSps)
    {
        if (!pSps)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        UMC::Status sts = UMC::UMC_OK;

        pSps->sps_scaling_matrix_for_lfnst_disabled_flag = true;
        pSps->sps_seq_parameter_set_id = GetBits(4);
        pSps->sps_video_parameter_set_id = GetBits(4);
        pSps->sps_max_sublayers_minus1 = GetBits(3);
        if (pSps->sps_max_sublayers_minus1 + 1 > VVC_MAX_VPS_SUBLAYERS)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pSps->sps_chroma_format_idc = GetBits(2);
        pSps->sps_log2_ctu_size_minus5 = GetBits(2);
        uint32_t ctbLog2SizeY = pSps->sps_log2_ctu_size_minus5 + 5;
        pSps->sps_ctu_size = 1 << ctbLog2SizeY;
        if (pSps->sps_log2_ctu_size_minus5 > 2)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pSps->sps_max_cu_width = pSps->sps_ctu_size;
        pSps->sps_max_cu_height = pSps->sps_ctu_size;
        pSps->sps_ptl_dpb_hrd_params_present_flag = Get1Bit();
        if (pSps->sps_video_parameter_set_id == 0 && !pSps->sps_ptl_dpb_hrd_params_present_flag)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        if (pSps->sps_ptl_dpb_hrd_params_present_flag)
        {
            xParseProfileTierLevel(&pSps->profile_tier_level, true, pSps->sps_max_sublayers_minus1);
        }
        pSps->sps_gdr_enabled_flag = Get1Bit();
        if (pSps->profile_tier_level.general_constraints_info.gci_no_gdr_constraint_flag &&
            pSps->sps_gdr_enabled_flag)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pSps->sps_ref_pic_resampling_enabled_flag = Get1Bit();
        if (pSps->profile_tier_level.general_constraints_info.gci_no_ref_pic_resampling_constraint_flag &&
            pSps->sps_ref_pic_resampling_enabled_flag)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        if (pSps->sps_ref_pic_resampling_enabled_flag)
        {
            pSps->sps_res_change_in_clvs_allowed_flag = Get1Bit();
        }
        else
        {
            pSps->sps_res_change_in_clvs_allowed_flag = false;
        }
        if (pSps->profile_tier_level.general_constraints_info.gci_no_res_change_in_clvs_constraint_flag &&
            pSps->sps_res_change_in_clvs_allowed_flag)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pSps->sps_pic_width_max_in_luma_samples = GetVLCElementU();
        pSps->sps_pic_height_max_in_luma_samples = GetVLCElementU();
        pSps->sps_conformance_window_flag = Get1Bit();
        if (pSps->sps_conformance_window_flag)
        {
            pSps->sps_conf_win_left_offset = GetVLCElementU();
            pSps->sps_conf_win_right_offset = GetVLCElementU();
            pSps->sps_conf_win_top_offset = GetVLCElementU();
            pSps->sps_conf_win_bottom_offset = GetVLCElementU();
        }
        pSps->sps_subpic_info_present_flag = Get1Bit();
        if (pSps->profile_tier_level.general_constraints_info.gci_no_subpic_info_constraint_flag &&
            pSps->sps_subpic_info_present_flag)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        if (pSps->sps_subpic_info_present_flag)
        {
            pSps->sps_num_subpics_minus1 = GetVLCElementU();
            uint32_t sps_num_subpics = pSps->sps_num_subpics_minus1 + 1;
            if (sps_num_subpics >= VVC_MAX_NUM_SUB_PICS)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            pSps->sps_subpic_ctu_top_left_x.resize(sps_num_subpics);
            pSps->sps_subpic_ctu_top_left_y.resize(sps_num_subpics);
            pSps->sps_subpic_width_minus1.resize(sps_num_subpics);
            pSps->sps_subpic_height_minus1.resize(sps_num_subpics);
            pSps->sps_subpic_treated_as_pic_flag.resize(sps_num_subpics);
            pSps->sps_loop_filter_across_subpic_enabled_flag.resize(sps_num_subpics);
            pSps->sps_subpic_id.resize(sps_num_subpics);
            if (pSps->sps_num_subpics_minus1 >
                ((pSps->sps_pic_width_max_in_luma_samples - 1) / pSps->sps_ctu_size + 1) *
                ((pSps->sps_pic_height_max_in_luma_samples - 1) / pSps->sps_ctu_size + 1))
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            if (pSps->sps_num_subpics_minus1 == 0)
            {
                pSps->sps_subpic_ctu_top_left_x[0] = 0;
                pSps->sps_subpic_ctu_top_left_y[0] = 0;
                pSps->sps_subpic_width_minus1[0] = (pSps->sps_pic_width_max_in_luma_samples + pSps->sps_ctu_size - 1) / pSps->sps_ctu_size;
                pSps->sps_subpic_height_minus1[0] = (pSps->sps_pic_height_max_in_luma_samples + pSps->sps_ctu_size - 1) / pSps->sps_ctu_size;
                pSps->sps_independent_subpics_flag = true;
                pSps->sps_subpic_same_size_flag = false;
                pSps->sps_subpic_treated_as_pic_flag[0] = true;
                pSps->sps_loop_filter_across_subpic_enabled_flag[0] = false;
            }
            else
            {
                pSps->sps_independent_subpics_flag = Get1Bit();
                pSps->sps_subpic_same_size_flag = Get1Bit();
                uint32_t tmpWidthVal = (pSps->sps_pic_width_max_in_luma_samples + pSps->sps_ctu_size - 1) / pSps->sps_ctu_size;
                uint32_t tmpHeightVal = (pSps->sps_pic_height_max_in_luma_samples + pSps->sps_ctu_size - 1) / pSps->sps_ctu_size;
                uint32_t numSubpicCols = 1;
                uint32_t ceilLog2Width = CeilLog2(tmpWidthVal);
                uint32_t ceilLog2Height = CeilLog2(tmpHeightVal);
                for (uint32_t i = 0; i <= pSps->sps_num_subpics_minus1; i++)
                {
                    if (!pSps->sps_subpic_same_size_flag || i == 0)
                    {
                        if ((i > 0) && (pSps->sps_pic_width_max_in_luma_samples > pSps->sps_ctu_size))
                        {
                            pSps->sps_subpic_ctu_top_left_x[i] = GetBits(ceilLog2Width);
                        }
                        else
                        {
                            pSps->sps_subpic_ctu_top_left_x[i] = 0;
                        }
                        if ((i > 0) && (pSps->sps_pic_height_max_in_luma_samples > pSps->sps_ctu_size))
                        {
                            pSps->sps_subpic_ctu_top_left_y[i] = GetBits(ceilLog2Height);
                        }
                        else
                        {
                            pSps->sps_subpic_ctu_top_left_y[i] = 0;
                        }
                        if (i <pSps->sps_num_subpics_minus1 && pSps->sps_pic_width_max_in_luma_samples > pSps->sps_ctu_size)
                        {
                            pSps->sps_subpic_width_minus1[i] = GetBits(ceilLog2Width);
                        }
                        else
                        {
                            pSps->sps_subpic_width_minus1[i] = tmpWidthVal - pSps->sps_subpic_ctu_top_left_x[i] - 1;
                        }
                        if (i <pSps->sps_num_subpics_minus1 && pSps->sps_pic_height_max_in_luma_samples > pSps->sps_ctu_size)
                        {
                            pSps->sps_subpic_height_minus1[i] = GetBits(ceilLog2Height);
                        }
                        else
                        {
                            pSps->sps_subpic_height_minus1[i] = tmpHeightVal - pSps->sps_subpic_ctu_top_left_y[i] - 1;
                        }
                        if (pSps->sps_subpic_same_size_flag)
                        {
                            numSubpicCols = tmpWidthVal / (pSps->sps_subpic_width_minus1[0] + 1);
                            if (tmpWidthVal % (pSps->sps_subpic_width_minus1[0] + 1) != 0)
                            {
                                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                            }
                            if (tmpHeightVal % (pSps->sps_subpic_height_minus1[0] + 1) != 0)
                            {
                                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                            }
                            if ((numSubpicCols * tmpHeightVal / (pSps->sps_subpic_height_minus1[0] + 1)) != (pSps->sps_num_subpics_minus1 + 1))
                            {
                                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                            }
                        }
                    }
                    else
                    {
                        pSps->sps_subpic_ctu_top_left_x[i] = (i % numSubpicCols) * (pSps->sps_subpic_width_minus1[0] + 1);
                        pSps->sps_subpic_ctu_top_left_y[i] = (i / numSubpicCols) * (pSps->sps_subpic_height_minus1[0] + 1);
                        pSps->sps_subpic_width_minus1[i] = pSps->sps_subpic_width_minus1[0];
                        pSps->sps_subpic_height_minus1[i] = pSps->sps_subpic_height_minus1[0];
                    }
                    if (!pSps->sps_independent_subpics_flag)
                    {
                        pSps->sps_subpic_treated_as_pic_flag[i] = Get1Bit();
                        pSps->sps_loop_filter_across_subpic_enabled_flag[i] = Get1Bit();
                    }
                    else
                    {
                        pSps->sps_subpic_treated_as_pic_flag[i] = true;
                        pSps->sps_loop_filter_across_subpic_enabled_flag[i] = false;
                    }
                }
            }
            pSps->sps_subpic_id_len_minus1 = GetVLCElementU();
            if (pSps->sps_subpic_id_len_minus1 > VVC_MAX_SUBPIC_ID_LEN)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            uint32_t tmpNumSubPics = 1 << (pSps->sps_subpic_id_len_minus1 + 1);
            if (tmpNumSubPics < (pSps->sps_num_subpics_minus1 + 1))
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            pSps->sps_subpic_id_mapping_explicitly_signalled_flag = Get1Bit();
            if (pSps->sps_subpic_id_mapping_explicitly_signalled_flag)
            {
                pSps->sps_subpic_id_mapping_present_flag = Get1Bit();
                if (pSps->sps_subpic_id_mapping_present_flag)
                {
                    for (uint32_t i = 0; i <= pSps->sps_num_subpics_minus1; i++)
                    {
                        pSps->sps_subpic_id[i] = GetBits(pSps->sps_subpic_id_len_minus1 + 1);
                    }
                }
            }
        }
        else
        {
            pSps->sps_subpic_ctu_top_left_x.resize(1);
            pSps->sps_subpic_ctu_top_left_y.resize(1);
            pSps->sps_subpic_width_minus1.resize(1);
            pSps->sps_subpic_height_minus1.resize(1);
            pSps->sps_subpic_treated_as_pic_flag.resize(1);
            pSps->sps_loop_filter_across_subpic_enabled_flag.resize(1);
            pSps->sps_subpic_id.resize(1);
            pSps->sps_subpic_id_mapping_explicitly_signalled_flag = false;
            pSps->sps_num_subpics_minus1 = 0;
            pSps->sps_subpic_ctu_top_left_x[0] = 0;
            pSps->sps_subpic_ctu_top_left_y[0] = 0;
            pSps->sps_subpic_width_minus1[0] = (pSps->sps_pic_width_max_in_luma_samples + pSps->sps_ctu_size - 1) / pSps->sps_ctu_size;
            pSps->sps_subpic_height_minus1[0] = (pSps->sps_pic_height_max_in_luma_samples + pSps->sps_ctu_size - 1) / pSps->sps_ctu_size;
        }
        if (!pSps->sps_subpic_id_mapping_explicitly_signalled_flag ||
            !pSps->sps_subpic_id_mapping_present_flag)
        {
            for (uint32_t i = 0; i <= pSps->sps_num_subpics_minus1; i++)
            {
                pSps->sps_subpic_id[i] = i;
            }
        }
        pSps->sps_bitdepth_minus8 = GetVLCElementU();
        pSps->sps_qp_bd_offset[0] = 6 * pSps->sps_bitdepth_minus8;
        pSps->sps_qp_bd_offset[1] = 6 * pSps->sps_bitdepth_minus8;
        pSps->sps_entropy_coding_sync_enabled_flag = Get1Bit();
        pSps->sps_entry_point_offsets_present_flag = Get1Bit();
        pSps->sps_log2_max_pic_order_cnt_lsb_minus4 = GetBits(4);
        if (pSps->sps_log2_max_pic_order_cnt_lsb_minus4 > 12)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pSps->sps_poc_msb_cycle_flag = Get1Bit();
        if (pSps->sps_poc_msb_cycle_flag)
        {
            pSps->sps_poc_msb_cycle_len_minus1 = GetVLCElementU();
            if (pSps->sps_poc_msb_cycle_len_minus1 > (27 - pSps->sps_log2_max_pic_order_cnt_lsb_minus4))
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
        }
        pSps->sps_num_extra_ph_bytes = GetBits(2);
        uint32_t spsNumExtraPhBitPresentFlag = 8 * pSps->sps_num_extra_ph_bytes;
        pSps->sps_extra_ph_bit_present_flag.resize(spsNumExtraPhBitPresentFlag);
        for (uint32_t i = 0; i < spsNumExtraPhBitPresentFlag; i++)
        {
            pSps->sps_extra_ph_bit_present_flag[i] = Get1Bit();
        }
        pSps->sps_num_extra_sh_bytes = GetBits(2);
        uint32_t spsNumExtraSHBitPresentFlag = 8 * pSps->sps_num_extra_sh_bytes;
        pSps->sps_extra_sh_bit_present_flag.resize(spsNumExtraSHBitPresentFlag);
        for (uint32_t i = 0; i < spsNumExtraSHBitPresentFlag; i++)
        {
            pSps->sps_extra_sh_bit_present_flag[i] = Get1Bit();
        }
        if (pSps->sps_ptl_dpb_hrd_params_present_flag)
        {
            if (pSps->sps_max_sublayers_minus1 > 0)
            {
                pSps->sps_sublayer_dpb_params_flag = Get1Bit();
            }
            xParseDpbParameters(&pSps->dpb_parameter, pSps->sps_max_sublayers_minus1, pSps->sps_sublayer_dpb_params_flag);
        }
        pSps->sps_log2_min_luma_coding_block_size_minus2 = GetVLCElementU();
        uint32_t log2MinCUSize = pSps->sps_log2_min_luma_coding_block_size_minus2 + 2;
        if (log2MinCUSize > ctbLog2SizeY)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        if (log2MinCUSize > (uint32_t)std::min(6, (int)ctbLog2SizeY))
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        uint32_t maxMinCUSize = (uint32_t)std::max(8, 1 << log2MinCUSize);
        if ((pSps->sps_pic_width_max_in_luma_samples % maxMinCUSize) != 0)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        if ((pSps->sps_pic_height_max_in_luma_samples % maxMinCUSize) != 0)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pSps->sps_partition_constraints_override_enabled_flag = Get1Bit();
        pSps->sps_log2_diff_min_qt_min_cb_intra_slice_luma = GetVLCElementU();
        uint32_t minQtLog2SizeIntraY = pSps->sps_log2_diff_min_qt_min_cb_intra_slice_luma + log2MinCUSize;
        pSps->sps_min_qt[0] = 1 << minQtLog2SizeIntraY;
        if (pSps->sps_min_qt[0] > 64)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        if (pSps->sps_min_qt[0] > (uint32_t)(1 << ctbLog2SizeY))
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pSps->sps_max_mtt_hierarchy_depth_intra_slice_luma = GetVLCElementU();
        pSps->sps_max_mtt_hierarchy_depth[0] = pSps->sps_max_mtt_hierarchy_depth_intra_slice_luma;
        if (pSps->sps_max_mtt_hierarchy_depth_intra_slice_luma > 2 * (ctbLog2SizeY - log2MinCUSize))
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pSps->sps_max_bt_size[0] = pSps->sps_min_qt[0];
        pSps->sps_max_tt_size[0] = pSps->sps_min_qt[0];
        if (pSps->sps_max_mtt_hierarchy_depth_intra_slice_luma != 0)
        {
            pSps->sps_log2_diff_max_bt_min_qt_intra_slice_luma = GetVLCElementU();
            pSps->sps_max_bt_size[0] = pSps->sps_max_bt_size[0] << pSps->sps_log2_diff_max_bt_min_qt_intra_slice_luma;
            if (pSps->sps_log2_diff_max_bt_min_qt_intra_slice_luma > ctbLog2SizeY - minQtLog2SizeIntraY)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            pSps->sps_log2_diff_max_tt_min_qt_intra_slice_luma = GetVLCElementU();
            pSps->sps_max_tt_size[0] = pSps->sps_max_tt_size[0] << pSps->sps_log2_diff_max_tt_min_qt_intra_slice_luma;
            if (pSps->sps_log2_diff_max_tt_min_qt_intra_slice_luma > ctbLog2SizeY - minQtLog2SizeIntraY)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            if (pSps->sps_max_tt_size[0] > 64)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
        }
        if (pSps->sps_chroma_format_idc != 0)
        {
            pSps->sps_qtbtt_dual_tree_intra_flag = Get1Bit();
        }
        else
        {
            pSps->sps_qtbtt_dual_tree_intra_flag = false;
        }
        if (pSps->sps_qtbtt_dual_tree_intra_flag)
        {
            pSps->sps_log2_diff_min_qt_min_cb_intra_slice_chroma = GetVLCElementU();
            pSps->sps_min_qt[2] = 1 << (pSps->sps_log2_diff_min_qt_min_cb_intra_slice_chroma + log2MinCUSize);
            pSps->sps_max_mtt_hierarchy_depth_intra_slice_chroma = GetVLCElementU();
            pSps->sps_max_mtt_hierarchy_depth[2] = pSps->sps_max_mtt_hierarchy_depth_intra_slice_chroma;
            if (pSps->sps_max_mtt_hierarchy_depth_intra_slice_chroma > 2 * (ctbLog2SizeY - log2MinCUSize))
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            pSps->sps_max_bt_size[2] = pSps->sps_min_qt[2];
            pSps->sps_max_tt_size[2] = pSps->sps_min_qt[2];
            if (pSps->sps_max_mtt_hierarchy_depth_intra_slice_chroma != 0)
            {
                pSps->sps_log2_diff_max_bt_min_qt_intra_slice_chroma = GetVLCElementU();
                pSps->sps_max_bt_size[2] = pSps->sps_max_bt_size[2] << pSps->sps_log2_diff_max_bt_min_qt_intra_slice_chroma;
                if (pSps->sps_max_bt_size[2] > 64)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                pSps->sps_log2_diff_max_tt_min_qt_intra_slice_chroma = GetVLCElementU();
                pSps->sps_max_tt_size[2] = pSps->sps_max_tt_size[2] << pSps->sps_log2_diff_max_tt_min_qt_intra_slice_chroma;
                if (pSps->sps_max_tt_size[2] > 64)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
            }
        }
        pSps->sps_log2_diff_min_qt_min_cb_inter_slice = GetVLCElementU();
        uint32_t minQtLog2SizeInterY = pSps->sps_log2_diff_min_qt_min_cb_inter_slice + log2MinCUSize;
        pSps->sps_min_qt[1] = 1 << minQtLog2SizeInterY;
        pSps->sps_max_mtt_hierarchy_depth_inter_slice = GetVLCElementU();
        pSps->sps_max_mtt_hierarchy_depth[1] = pSps->sps_max_mtt_hierarchy_depth_inter_slice;
        if (pSps->sps_max_mtt_hierarchy_depth_inter_slice > 2 * (ctbLog2SizeY - log2MinCUSize))
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pSps->sps_max_bt_size[1] = pSps->sps_min_qt[1];
        pSps->sps_max_tt_size[1] = pSps->sps_min_qt[1];
        if (pSps->sps_max_mtt_hierarchy_depth_inter_slice != 0)
        {
            pSps->sps_log2_diff_max_bt_min_qt_inter_slice = GetVLCElementU();
            pSps->sps_max_bt_size[1] = pSps->sps_max_bt_size[1] << pSps->sps_log2_diff_max_bt_min_qt_inter_slice;
            if (pSps->sps_log2_diff_max_bt_min_qt_inter_slice > ctbLog2SizeY - minQtLog2SizeInterY)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            pSps->sps_log2_diff_max_tt_min_qt_inter_slice = GetVLCElementU();
            pSps->sps_max_tt_size[1] = pSps->sps_max_tt_size[1] << pSps->sps_log2_diff_max_tt_min_qt_inter_slice;
            if (pSps->sps_log2_diff_max_tt_min_qt_inter_slice > ctbLog2SizeY - minQtLog2SizeInterY)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            if (pSps->sps_max_tt_size[1] > 64)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
        }
        if (pSps->sps_ctu_size > 32)
        {
            pSps->sps_max_luma_transform_size_64_flag = Get1Bit();
            pSps->sps_log2_max_tb_size = (pSps->sps_max_luma_transform_size_64_flag ? 1 : 0) + 5;
        }
        else
        {
            pSps->sps_log2_max_tb_size = 5;
        }
        pSps->sps_transform_skip_enabled_flag = Get1Bit();
        if (pSps->sps_transform_skip_enabled_flag)
        {
            pSps->sps_log2_transform_skip_max_size_minus2 = GetVLCElementU();
            pSps->sps_bdpcm_enabled_flag = Get1Bit();
        }
        pSps->sps_mts_enabled_flag = Get1Bit();
        if (pSps->sps_mts_enabled_flag)
        {
            pSps->sps_explicit_mts_intra_enabled_flag = Get1Bit();
            pSps->sps_explicit_mts_inter_enabled_flag = Get1Bit();
        }
        pSps->sps_lfnst_enabled_flag = Get1Bit();
        if (pSps->sps_chroma_format_idc != 0)
        {
            pSps->sps_joint_cbcr_enabled_flag = Get1Bit();
            pSps->sps_same_qp_table_for_chroma_flag = Get1Bit();
            pSps->sps_num_qp_tables = pSps->sps_same_qp_table_for_chroma_flag ? 1 :
                (pSps->sps_joint_cbcr_enabled_flag ? 3 : 2);
            for (uint32_t i = 0; i < pSps->sps_num_qp_tables; i++)
            {
                pSps->sps_qp_table_start_minus26[i] = GetVLCElementS();
                if (pSps->sps_qp_table_start_minus26[i] < -26 - pSps->sps_qp_bd_offset[0] ||
                    pSps->sps_qp_table_start_minus26[i] > 36)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                pSps->sps_num_points_in_qp_table_minus1[i] = GetVLCElementU();
                if (pSps->sps_num_points_in_qp_table_minus1[i] > (36 - pSps->sps_qp_table_start_minus26[i]))
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                uint32_t spsNumPointsInQpTable = pSps->sps_num_points_in_qp_table_minus1[i] + 1;
                pSps->sps_delta_qp_in_val_minus1[i].resize(spsNumPointsInQpTable);
                pSps->sps_delta_qp_diff_val[i].resize(spsNumPointsInQpTable);
                for (int32_t j = 0; j <= pSps->sps_num_points_in_qp_table_minus1[i]; j++)
                {
                    pSps->sps_delta_qp_in_val_minus1[i][j] = GetVLCElementU();
                    pSps->sps_delta_qp_diff_val[i][j] = GetVLCElementU();
                }
            }
        }
        pSps->sps_sao_enabled_flag = Get1Bit();
        pSps->sps_alf_enabled_flag = Get1Bit();
        if (pSps->sps_alf_enabled_flag && pSps->sps_chroma_format_idc != 0)
        {
            pSps->sps_ccalf_enabled_flag = Get1Bit();
        }
        else
        {
            pSps->sps_ccalf_enabled_flag = false;
        }
        pSps->sps_lmcs_enabled_flag = Get1Bit();
        pSps->sps_weighted_pred_flag = Get1Bit();
        pSps->sps_weighted_bipred_flag = Get1Bit();
        pSps->sps_long_term_ref_pics_flag = Get1Bit();
        if (pSps->sps_video_parameter_set_id > 0)
        {
            pSps->sps_inter_layer_prediction_enabled_flag = Get1Bit();
        }
        else
        {
            pSps->sps_inter_layer_prediction_enabled_flag = false;
        }
        pSps->sps_idr_rpl_present_flag = Get1Bit();
        if (pSps->profile_tier_level.general_constraints_info.gci_no_idr_rpl_constraint_flag &&
            pSps->sps_idr_rpl_present_flag)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pSps->sps_rpl1_same_as_rpl0_flag = Get1Bit();
        uint32_t numRPLSyntax = pSps->sps_rpl1_same_as_rpl0_flag ? 1 : 2;
        for (uint32_t i = 0; i < numRPLSyntax; i++)
        {
            pSps->sps_num_ref_pic_lists[i] = GetVLCElementU();
            pSps->sps_ref_pic_lists[i].resize(pSps->sps_num_ref_pic_lists[i]);
            for (uint32_t j = 0; j < pSps->sps_num_ref_pic_lists[i]; j++)
            {
                xParseRefPicListStruct(pSps, &pSps->sps_ref_pic_lists[i][j], j);
                pSps->sps_ref_pic_lists[i][j].rpl_idx = j;
            }
        }
        if (pSps->sps_rpl1_same_as_rpl0_flag)
        {
            pSps->sps_num_ref_pic_lists[1] = pSps->sps_num_ref_pic_lists[0];
            pSps->sps_ref_pic_lists[1].resize(pSps->sps_num_ref_pic_lists[1]);
            for (uint32_t i = 0; i < pSps->sps_num_ref_pic_lists[1]; i++)
            {
                xCopyRefPicListStruct(pSps, &pSps->sps_ref_pic_lists[0][i], &pSps->sps_ref_pic_lists[1][i]);
                pSps->sps_ref_pic_lists[1][i].rpl_idx = i;
            }
        }
        pSps->sps_ref_wraparound_enabled_flag = Get1Bit();
        pSps->sps_temporal_mvp_enabled_flag = Get1Bit();
        if (pSps->sps_temporal_mvp_enabled_flag)
        {
            pSps->sps_sbtmvp_enabled_flag = Get1Bit();
        }
        else
        {
            pSps->sps_sbtmvp_enabled_flag = false;
        }
        pSps->sps_amvr_enabled_flag = Get1Bit();
        pSps->sps_bdof_enabled_flag = Get1Bit();
        if (pSps->sps_bdof_enabled_flag)
        {
            pSps->sps_bdof_control_present_in_ph_flag = Get1Bit();
        }
        else
        {
            pSps->sps_bdof_control_present_in_ph_flag = false;
        }
        pSps->sps_smvd_enabled_flag = Get1Bit();
        pSps->sps_dmvr_enabled_flag = Get1Bit();
        if (pSps->sps_dmvr_enabled_flag)
        {
            pSps->sps_dmvr_control_present_in_ph_flag = Get1Bit();
        }
        else
        {
            pSps->sps_dmvr_control_present_in_ph_flag = false;
        }
        pSps->sps_mmvd_enabled_flag = Get1Bit();
        if (pSps->sps_mmvd_enabled_flag)
        {
            pSps->sps_mmvd_fullpel_only_enabled_flag = Get1Bit();
        }
        else
        {
            pSps->sps_mmvd_fullpel_only_enabled_flag = false;
        }
        pSps->sps_six_minus_max_num_merge_cand = GetVLCElementU();
        if (pSps->sps_six_minus_max_num_merge_cand >= VVC_MRG_MAX_NUM_CANDS)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pSps->sps_sbt_enabled_flag = Get1Bit();
        pSps->sps_affine_enabled_flag = Get1Bit();
        if (pSps->sps_affine_enabled_flag)
        {
            pSps->sps_five_minus_max_num_subblock_merge_cand = GetVLCElementU();
            if (pSps->sps_five_minus_max_num_subblock_merge_cand > (uint32_t)(5 - (pSps->sps_sbtmvp_enabled_flag ? 1 : 0)))
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            if (pSps->sps_five_minus_max_num_subblock_merge_cand > VVC_AFFINE_MRG_MAX_NUM_CANDS)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            pSps->sps_6param_affine_enabled_flag = Get1Bit();
            if (pSps->sps_amvr_enabled_flag)
            {
                pSps->sps_affine_amvr_enabled_flag = Get1Bit();
            }
            pSps->sps_affine_prof_enabled_flag = Get1Bit();
            if (pSps->sps_affine_prof_enabled_flag)
            {
                pSps->sps_prof_control_present_in_ph_flag = Get1Bit();
            }
            else
            {
                pSps->sps_prof_control_present_in_ph_flag = false;
            }
        }
        pSps->sps_bcw_enabled_flag = Get1Bit();
        pSps->sps_ciip_enabled_flag = Get1Bit();
        uint32_t maxNumMergeCand = 6 - pSps->sps_six_minus_max_num_merge_cand;
        if (maxNumMergeCand >= 2)
        {
            pSps->sps_gpm_enabled_flag = Get1Bit();
            if (pSps->sps_gpm_enabled_flag)
            {
                if (maxNumMergeCand >= 3)
                {
                    pSps->sps_max_num_merge_cand_minus_max_num_gpm_cand = GetVLCElementU();
                    if (maxNumMergeCand - 2 < pSps->sps_max_num_merge_cand_minus_max_num_gpm_cand)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                    pSps->sps_max_num_geo_cand = maxNumMergeCand - pSps->sps_max_num_merge_cand_minus_max_num_gpm_cand;
                }
                else
                {
                    pSps->sps_max_num_geo_cand = 2;
                    pSps->sps_max_num_merge_cand_minus_max_num_gpm_cand = maxNumMergeCand - 2;
                }
            }
        }
        else
        {
            pSps->sps_gpm_enabled_flag = false;
            pSps->sps_max_num_geo_cand = 0;
            pSps->sps_max_num_merge_cand_minus_max_num_gpm_cand = maxNumMergeCand;
        }
        pSps->sps_max_num_merge_cand_minus_max_num_gpm_cand = maxNumMergeCand - pSps->sps_max_num_geo_cand;
        pSps->sps_log2_parallel_merge_level_minus2 = GetVLCElementU();
        if (pSps->sps_log2_parallel_merge_level_minus2 + 2 > ctbLog2SizeY)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pSps->sps_isp_enabled_flag = Get1Bit();
        pSps->sps_mrl_enabled_flag = Get1Bit();
        pSps->sps_mip_enabled_flag = Get1Bit();
        if (pSps->sps_chroma_format_idc != 0)
        {
            pSps->sps_cclm_enabled_flag = Get1Bit();
        }
        else
        {
            pSps->sps_cclm_enabled_flag = false;
        }
        if (pSps->sps_chroma_format_idc == 1)
        {
            pSps->sps_chroma_horizontal_collocated_flag = Get1Bit();
            pSps->sps_chroma_vertical_collocated_flag = Get1Bit();
        }
        else
        {
            pSps->sps_chroma_horizontal_collocated_flag = true;
            pSps->sps_chroma_vertical_collocated_flag = true;
        }
        pSps->sps_palette_enabled_flag = Get1Bit();
        if (pSps->sps_chroma_format_idc == 3 && !pSps->sps_max_luma_transform_size_64_flag)
        {
            pSps->sps_act_enabled_flag = Get1Bit();
        }
        else
        {
            pSps->sps_act_enabled_flag = false;
        }
        if (pSps->sps_transform_skip_enabled_flag || pSps->sps_palette_enabled_flag)
        {
            pSps->sps_min_qp_prime_ts = GetVLCElementU();
            if (pSps->sps_min_qp_prime_ts > 8)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
        }
        pSps->sps_ibc_enabled_flag = Get1Bit();
        if (pSps->sps_ibc_enabled_flag)
        {
            pSps->sps_six_minus_max_num_ibc_merge_cand = GetVLCElementU();
            if (pSps->sps_six_minus_max_num_ibc_merge_cand >= VVC_IBC_MRG_MAX_NUM_CANDS)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
        }
        else
        {
            pSps->sps_six_minus_max_num_ibc_merge_cand = 6;
        }
        pSps->sps_ladf_enabled_flag = Get1Bit();
        if (pSps->sps_ladf_enabled_flag)
        {
            pSps->sps_num_ladf_intervals_minus2 = GetBits(2);
            pSps->sps_ladf_lowest_interval_qp_offset = GetVLCElementS();
            for (uint32_t i = 0; i < pSps->sps_num_ladf_intervals_minus2 + 1; i++)
            {
                pSps->sps_ladf_qp_offset[i] = GetVLCElementS();
                pSps->sps_ladf_delta_threshold_minus1[i] = GetVLCElementU();
            }
        }
        pSps->sps_explicit_scaling_list_enabled_flag = Get1Bit();
        pSps->sps_scaling_matrix_designated_colour_space_flag = true;
        if (pSps->profile_tier_level.general_constraints_info.gci_no_explicit_scaling_list_constraint_flag &&
            pSps->sps_explicit_scaling_list_enabled_flag)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        if (pSps->sps_lfnst_enabled_flag && pSps->sps_explicit_scaling_list_enabled_flag)
        {
            pSps->sps_scaling_matrix_for_lfnst_disabled_flag = Get1Bit();
        }
        if (pSps->sps_act_enabled_flag && pSps->sps_explicit_scaling_list_enabled_flag)
        {
            pSps->sps_scaling_matrix_for_alternative_colour_space_disabled_flag = Get1Bit();
        }
        if (pSps->sps_scaling_matrix_for_alternative_colour_space_disabled_flag)
        {
            pSps->sps_scaling_matrix_designated_colour_space_flag = Get1Bit();
        }
        pSps->sps_dep_quant_enabled_flag = Get1Bit();
        pSps->sps_sign_data_hiding_enabled_flag = Get1Bit();
        pSps->sps_virtual_boundaries_enabled_flag = Get1Bit();
        if (pSps->profile_tier_level.general_constraints_info.gci_no_virtual_boundaries_constraint_flag &&
            pSps->sps_virtual_boundaries_enabled_flag)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        if (pSps->sps_virtual_boundaries_enabled_flag)
        {
            pSps->sps_virtual_boundaries_present_flag = Get1Bit();
            if (pSps->sps_virtual_boundaries_present_flag)
            {
                pSps->sps_num_ver_virtual_boundaries = GetVLCElementU();
                if (pSps->sps_pic_width_max_in_luma_samples <= 8 && pSps->sps_num_ver_virtual_boundaries > 0)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                if (pSps->sps_num_ver_virtual_boundaries > 3)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                uint32_t ceilMaxPicWidthInBlockMinus2 = ((pSps->sps_pic_width_max_in_luma_samples + 7) >> 3) - 2;
                for (uint32_t i = 0; i < pSps->sps_num_ver_virtual_boundaries; i++)
                {
                    pSps->sps_virtual_boundary_pos_x_minus1[i] = GetVLCElementU();
                    if (pSps->sps_virtual_boundary_pos_x_minus1[i] > ceilMaxPicWidthInBlockMinus2)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                }
                pSps->sps_num_hor_virtual_boundaries = GetVLCElementU();
                if (pSps->sps_pic_height_max_in_luma_samples <= 8 && pSps->sps_num_hor_virtual_boundaries > 0)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                if (pSps->sps_num_hor_virtual_boundaries > 3)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                uint32_t ceilMaxPicHeightInBlockMinus2 = ((pSps->sps_pic_height_max_in_luma_samples + 7) >> 3) - 2;
                for (uint32_t i = 0; i < pSps->sps_num_hor_virtual_boundaries; i++)
                {
                    pSps->sps_virtual_boundary_pos_y_minus1[i] = GetVLCElementU();
                    if (pSps->sps_virtual_boundary_pos_y_minus1[i] > ceilMaxPicHeightInBlockMinus2)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                }
            }
            else
            {
                pSps->sps_num_ver_virtual_boundaries = 0;
                pSps->sps_num_hor_virtual_boundaries = 0;
            }
        }
        else
        {
            pSps->sps_virtual_boundaries_present_flag = false;
        }
        if (pSps->sps_ptl_dpb_hrd_params_present_flag)
        {
            pSps->sps_timing_hrd_params_present_flag = Get1Bit();
            if (pSps->sps_timing_hrd_params_present_flag)
            {
                xParseGeneralTimingHrdParameters(&pSps->general_timing_hrd_parameters);
                if (pSps->sps_max_sublayers_minus1 > 0)
                {
                    pSps->sps_sublayer_cpb_params_present_flag = Get1Bit();
                }
                else
                {
                    pSps->sps_sublayer_cpb_params_present_flag = false;
                }
                uint32_t firstSubLayer = pSps->sps_sublayer_cpb_params_present_flag ? 0 : pSps->sps_max_sublayers_minus1;
                xParseOlsTimingHrdParameters(&pSps->general_timing_hrd_parameters,
                                             &pSps->ols_timing_hrd_parameters,
                                             firstSubLayer,
                                             pSps->sps_max_sublayers_minus1);
            }
        }
        pSps->sps_field_seq_flag = Get1Bit();
        if (pSps->profile_tier_level.ptl_frame_only_constraint_flag && pSps->sps_field_seq_flag)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        pSps->sps_vui_parameters_present_flag = Get1Bit();
        if (pSps->sps_vui_parameters_present_flag)
        {
            pSps->sps_vui_payload_size_minus1 = GetVLCElementU();
            uint32_t numBitsUntilByteAligned = GetNumBitsUntilByteAligned();
            if (numBitsUntilByteAligned > 0)
            {
                GetBits(numBitsUntilByteAligned);
            }
            xParseVUI(pSps, &pSps->vui);
        }
        pSps->sps_extension_flag = Get1Bit();
        if (pSps->sps_extension_flag)
        {
            bool sps_extension_flags[NUM_SPS_EXTENSION_FLAGS];
            for (int i = 0; i < NUM_SPS_EXTENSION_FLAGS; i++)
            {
                sps_extension_flags[i] = Get1Bit();
            }
            bool skip_trailing_extension_bits = false;
            for (int i = 0; i < NUM_SPS_EXTENSION_FLAGS; i++) 
            {
                if (sps_extension_flags[i])
                {
                    switch (SPSExtensionFlagIndex(i))
                    {
                    case SPS_EXT__REXT:
                        if (skip_trailing_extension_bits)
                        {
                            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                        }
                        pSps->extended_precision_processing_flag = Get1Bit();
                        if (pSps->sps_transform_skip_enabled_flag)
                        {
                            pSps->sps_ts_residual_coding_rice_present_in_sh_flag = Get1Bit();
                        }
                        pSps->rrc_rice_extension_flag = Get1Bit();
                        pSps->persistent_rice_adaptation_enabled_flag = Get1Bit();
                        pSps->reverse_last_position_enabled_flag = Get1Bit();
                        break;
                    default:
                        skip_trailing_extension_bits = true;
                        break;
                    }
                }
            }
            if (skip_trailing_extension_bits)
            {
                while (MoreRbspData())
                {
                    pSps->sps_extension_data_flag = Get1Bit();
                }
            }
        }
        ReadOutTrailingBits();
        return sts;
    }

    // Parse beginning of slice header to get PPS ID
    UMC::Status VVCHeadersBitstream::GetSlicePicParamSetNumber(VVCSliceHeader* sliceHdr,
                                                               VVCPicHeader* picHeader) 
    {
        if (!sliceHdr || !picHeader)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        sliceHdr->slice_pic_parameter_set_id = picHeader->ph_pic_parameter_set_id;
        if (sliceHdr->slice_pic_parameter_set_id > 63) 
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        return UMC::UMC_OK;
    }

    // Parse full slice header
    UMC::Status VVCHeadersBitstream::GetSliceHeader(VVCSlice* slice,
                                                    VVCPicHeader* picHeader,
                                                    ParameterSetManager* m_currHeader,
                                                    PocDecoding* pocDecoding,
                                                    const int prevPicPOC)
    {
        if (!slice || !picHeader)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }

        UMC::Status sts = UMC::UMC_OK;
        VVCSliceHeader* sliceHdr = slice->GetSliceHeader();
        sliceHdr->sh_picture_header_in_slice_header_flag = Get1Bit();
        bool phInsh_flag = sliceHdr->sh_picture_header_in_slice_header_flag;

        if (phInsh_flag)
        {
            GetPictureHeader(picHeader, m_currHeader, false);
            sliceHdr->slice_pic_parameter_set_id = picHeader->ph_pic_parameter_set_id;
            m_currHeader->m_phParams = *picHeader;
        }
        slice->SetPH(m_currHeader->m_phParams);
        int32_t pps_pid = m_currHeader->m_phParams.ph_pic_parameter_set_id;
        if (pps_pid < 0)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        slice->SetPPS(m_currHeader->m_picParams.GetHeader(pps_pid));
        slice->SetSPS(m_currHeader->m_seqParams.GetHeader(slice->GetPPS()->pps_seq_parameter_set_id));
        int32_t vid_parameter_set_id = slice->GetSPS()->sps_video_parameter_set_id;
        slice->SetVPS(m_currHeader->m_videoParams.GetHeader(vid_parameter_set_id));

        const VVCPicParamSet* pps = slice->GetPPS();
        const VVCSeqParamSet* sps = slice->GetSPS();

        if (!pps || !sps)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        if (sps->profile_tier_level.general_constraints_info.gci_pic_header_in_slice_header_constraint_flag)
        {
            if (!phInsh_flag)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
        }
        if ((phInsh_flag && (pps->pps_rpl_info_in_ph_flag || pps->pps_dbf_info_in_ph_flag
            || pps->pps_sao_info_in_ph_flag || pps->pps_alf_info_in_ph_flag
            || pps->pps_wp_info_in_ph_flag || pps->pps_qp_delta_info_in_ph_flag
            || sps->sps_subpic_info_present_flag))
            || (sps->sps_subpic_info_present_flag && !sps->sps_subpic_info_present_flag && sps->sps_virtual_boundaries_enabled_flag))
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }

        for (uint32_t i = 0; i < pps->pps_rect_slices.size(); i++)
        {
            slice->m_rectSlice.push_back(new VVCRectSlice());
            *slice->m_rectSlice[i] = *m_currHeader->m_picParams.GetHeader(pps_pid)->pps_rect_slices[i];
        }

        uint32_t chFmt = sps->sps_chroma_format_idc;
        uint32_t numValidComp = (chFmt == CHROMA_FORMAT_400) ? 1 : MAX_NUM_COMPONENT;
        sliceHdr->IdrPicFlag = ((pps->pps_mixed_nalu_types_in_pic_flag == 0) &&
                                (sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL ||
                                sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_N_LP ||
                                sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_CRA)) ? 1 : 0;

        // picture order count
        int32_t PicOrderCntMsb;
        sliceHdr->slice_pic_order_cnt_lsb = picHeader->ph_pic_order_cnt_lsb;
        int32_t slice_pic_order_cnt_lsb = sliceHdr->slice_pic_order_cnt_lsb;
        int32_t MaxPicOrderCntLsb = 1 << (sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
        sliceHdr->IDRflag = (sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL || sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_N_LP);
        if (sliceHdr->IDRflag)
        {
            if (picHeader->ph_poc_msb_cycle_present_flag)
            {
                PicOrderCntMsb = picHeader->ph_poc_msb_cycle_val * MaxPicOrderCntLsb;
            }
            else
            {
                PicOrderCntMsb = 0;
            }
        }
        else
        {
            int32_t prevPicOrderCntLsb = pocDecoding->prevPocPicOrderCntLsb & (MaxPicOrderCntLsb - 1);
            int32_t prevPicOrderCntMsb = pocDecoding->prevPocPicOrderCntLsb - prevPicOrderCntLsb;
            if (picHeader->ph_poc_msb_cycle_present_flag)
            {
                PicOrderCntMsb = picHeader->ph_poc_msb_cycle_val * MaxPicOrderCntLsb;
            }
            else
            {
                if ((slice_pic_order_cnt_lsb < prevPicOrderCntLsb) && ((prevPicOrderCntLsb - slice_pic_order_cnt_lsb) >= (MaxPicOrderCntLsb / 2)))
                {
                    PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
                }
                else if ((slice_pic_order_cnt_lsb > prevPicOrderCntLsb) && ((slice_pic_order_cnt_lsb - prevPicOrderCntLsb) > (MaxPicOrderCntLsb / 2)))
                {
                    PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
                }
                else
                {
                    PicOrderCntMsb = prevPicOrderCntMsb;
                }
            }
        }
        int m_poc = PicOrderCntMsb + slice_pic_order_cnt_lsb;
        sliceHdr->m_poc = m_poc;
        if(sliceHdr->nuh_layer_id == 0 && (sliceHdr->nal_unit_type != NAL_UNIT_CODED_SLICE_RASL) && (sliceHdr->nal_unit_type != NAL_UNIT_CODED_SLICE_RADL) && !picHeader->ph_non_ref_pic_flag)
            pocDecoding->prevPocPicOrderCntLsb = m_poc;

        if (sps->sps_subpic_info_present_flag)
        {
            uint32_t bitsSubPicId;
            bitsSubPicId = sps->sps_subpic_id_len_minus1 + 1;
            sliceHdr->sh_subpic_id = GetBits(bitsSubPicId);
            sliceHdr->slice_subPic_id = sliceHdr->sh_subpic_id;
        }
        else
        {
            sliceHdr->sh_subpic_id = 0;
            sliceHdr->slice_subPic_id = sliceHdr->sh_subpic_id;          
        }

        // raster scan slices
        uint32_t sliceAddr = 0;
        if (!pps->pps_rect_slice_flag)
        {
            // slice address is the raster scan tile index of first tile in slice
            if (pps->pps_num_tiles > 1)
            {
                int bitsSliceAddress = CeilLog2(pps->pps_num_tiles);
                sliceHdr->sh_slice_address = GetBits(bitsSliceAddress);            
            }
            else
            {
                sliceHdr->sh_slice_address = 0;
            }
            sliceAddr = sliceHdr->sh_slice_address;
        }
        // rectangular slices
        else
        {
            uint32_t sliceSubPicId = sliceHdr->slice_subPic_id;
            uint32_t currSubPicIdx = 0;
            for (uint32_t i = 0; i < pps->pps_num_subpics; i++)
            {
                if (pps->pps_sub_pics[i]->sub_pic_id == sliceSubPicId)
                {
                    currSubPicIdx = i;
                    break;
                }
                currSubPicIdx = 0;
            }
            SubPic* currSubPic = pps->pps_sub_pics[currSubPicIdx];
            if (currSubPic->num_slices_in_subPic > 1)
            {
                int bitsSliceAddress = CeilLog2(currSubPic->num_slices_in_subPic); // check currSubPics & pps_num_slices_in_pic
                sliceHdr->sh_slice_address = GetBits(bitsSliceAddress);
                if (sliceAddr >= currSubPic->num_slices_in_subPic)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
            }
            else
            {
                sliceHdr->sh_slice_address = 0;
            }
            sliceAddr = sliceHdr->sh_slice_address;
            uint32_t picLevelSliceIdx = sliceAddr;
            for (uint16_t subpic = 0; subpic < currSubPicIdx; subpic++)
            {
                picLevelSliceIdx += pps->pps_sub_pics[subpic]->num_slices_in_subPic;
            }
            if (picLevelSliceIdx > pps->pps_num_slices_in_pic)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            slice->setSliceMap(*pps->pps_slice_map[picLevelSliceIdx]);
            slice->setSliceID(picLevelSliceIdx);
        }

        std::vector<bool> shExtraBitsPresent = sps->sps_extra_sh_bit_present_flag;
        for (uint8_t i = 0; i < sps->sps_num_extra_sh_bytes * 8; i++)
        {
            if (shExtraBitsPresent[i])
            {
                sliceHdr->sh_extra_bit.push_back(Get1Bit());
            }
        }
        if (!pps->pps_rect_slice_flag)
        {
            uint32_t numTilesInSlice = 1;
            if (pps->pps_num_tiles > 1)
            {
                if ((int)pps->pps_num_tiles - (int)sliceAddr > 1)
                {
                    sliceHdr->sh_num_tiles_in_slice_minus1 = GetVLCElementU();                    
                }
                else
                {
                    sliceHdr->sh_num_tiles_in_slice_minus1 = 0;
                      
                }
		        numTilesInSlice = sliceHdr->sh_num_tiles_in_slice_minus1 + 1;
                if (!pps->pps_rect_slice_flag && sps->profile_tier_level.general_constraints_info.gci_one_slice_per_pic_constraint_flag)
                {
                    if (pps->pps_num_tiles != numTilesInSlice)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                }
            }
            else
            {
                sliceHdr->sh_num_tiles_in_slice_minus1 = 0;
            }
            if (sliceAddr >= pps->pps_num_tiles)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            slice->initSliceMap();
            slice->setSliceID(sliceAddr);

            for (uint32_t tileIdx = sliceAddr; tileIdx < sliceAddr + numTilesInSlice; tileIdx++)
            {
                uint32_t tileX = tileIdx % pps->pps_num_tile_cols;
                uint32_t tileY = tileIdx / pps->pps_num_tile_cols;
                if (tileY >= pps->pps_num_tile_rows)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                if (pps->pps_tile_col_bd[tileX] >= pps->pps_tile_col_bd[tileX + 1] || pps->pps_tile_row_bd[tileY] >= pps->pps_tile_row_bd[tileY + 1])
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                for (uint32_t ctbY = pps->pps_tile_row_bd[tileY]; ctbY < pps->pps_tile_row_bd[tileY + 1]; ctbY++)
                {
                    for (uint32_t ctbX = pps->pps_tile_col_bd[tileX]; ctbX < pps->pps_tile_col_bd[tileX + 1]; ctbX++)
                    {
                        slice->setCtuAddrInSlice(ctbY * pps->pps_pic_width_in_ctu + ctbX);
                        slice->getSliceMap()->num_ctu_in_slice++;
                    }
                }
            }
        }
        else
        {
            sliceHdr->sh_num_tiles_in_slice_minus1 = 0;
        }

        if (picHeader->ph_inter_slice_allowed_flag)
        {
            sliceHdr->slice_type = (SliceType)GetVLCElementU();
            const VVCVideoParamSet* vps = slice->GetVPS();
            if (vps)
            {
                bool isIRAP = (sliceHdr->nal_unit_type >= NAL_UNIT_CODED_SLICE_IDR_W_RADL) && (sliceHdr->nal_unit_type <= NAL_UNIT_CODED_SLICE_CRA);
                bool vpsFlag = (vps->vps_independent_layer_flag[vps->vps_layer_id[sliceHdr->nuh_layer_id]] == 1);
                if (isIRAP && (!vps->vps_video_parameter_set_id || sliceHdr->m_poc != prevPicPOC || vpsFlag) && sliceHdr->slice_type != 2)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
            }
        }
        else
        {
            sliceHdr->slice_type = I_SLICE;
        }
        if (!picHeader->ph_intra_slice_allowed_flag && sliceHdr->slice_type == I_SLICE)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
        
        if (sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_CRA
            || sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_N_LP
            || sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL
            || sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_GDR)
        {
            sliceHdr->sh_no_output_of_prior_pics_flag = Get1Bit();
        }

        // inherit values from picture header
        // set default values in case slice overrides are disabled
        slice->InheritFromPicHeader(picHeader, pps, sps);

        bool alf_info_flag = sps->sps_alf_enabled_flag && (pps->pps_alf_info_in_ph_flag == 0);
        if (alf_info_flag)
        {
            sliceHdr->alf_enabled_flag[COMPONENT_Y] = Get1Bit();
            int alfCbEnabledFlag = 0;
            int alfCrEnabledFlag = 0;

            if (sliceHdr->alf_enabled_flag[COMPONENT_Y])
            {
                sliceHdr->num_alf_aps_ids_luma = GetBits(3);
                sliceHdr->alf_aps_ids_luma.resize(sliceHdr->num_alf_aps_ids_luma);
                for (int i = 0; i < sliceHdr->num_alf_aps_ids_luma; i++)
                {
                    sliceHdr->alf_aps_ids_luma[i] = static_cast<uint8_t>(GetBits(3));
                }
                if (chFmt)
                {
                    alfCbEnabledFlag = Get1Bit();
                    alfCrEnabledFlag = Get1Bit();
                }
                else
                {
                    alfCbEnabledFlag = 0;
                    alfCrEnabledFlag = 0;
                }
                if (alfCbEnabledFlag || alfCrEnabledFlag)
                {
                    sliceHdr->alf_aps_id_chroma = GetBits(3);
                }
            }
            else
            {
                sliceHdr->num_alf_aps_ids_luma = 0;
            }
            sliceHdr->alf_enabled_flag[COMPONENT_Cb] = alfCbEnabledFlag;
            sliceHdr->alf_enabled_flag[COMPONENT_Cr] = alfCrEnabledFlag;

            if (sps->sps_ccalf_enabled_flag && sliceHdr->alf_enabled_flag[COMPONENT_Y])
            {
                sliceHdr->alf_cc_cb_enabled_flag = Get1Bit();
                sliceHdr->alf_cc_cb_aps_id = (uint32_t)(-1);
                if (sliceHdr->alf_cc_cb_enabled_flag == 1)
                {
                    sliceHdr->alf_cc_cb_aps_id = GetBits(3);
                }
                sliceHdr->alf_cc_cr_enabled_flag = Get1Bit();
                sliceHdr->alf_cc_cr_aps_id = (uint32_t)(-1);
                if (sliceHdr->alf_cc_cr_enabled_flag == 1)
                {
                    sliceHdr->alf_cc_cr_aps_id = GetBits(3);
                }
            }
            else
            {
                sliceHdr->alf_cc_cb_aps_id = (uint32_t)(-1);
                sliceHdr->alf_cc_cr_aps_id = (uint32_t)(-1);
            }
        }
        else
        {
            sliceHdr->alf_enabled_flag[COMPONENT_Y] = 0;
            sliceHdr->alf_enabled_flag[COMPONENT_Cb] = 0;
            sliceHdr->alf_enabled_flag[COMPONENT_Cr] = 0;        
            sliceHdr->num_alf_aps_ids_luma = 0;
            sliceHdr->alf_aps_id_chroma = 0;
            sliceHdr->alf_cc_cb_enabled_flag = 0;
            sliceHdr->alf_cc_cb_aps_id = (uint32_t)(-1);
            sliceHdr->alf_cc_cr_enabled_flag = 0;
            sliceHdr->alf_cc_cr_aps_id = (uint32_t)(-1);
        }
        if (picHeader->ph_lmcs_enabled_flag && !sliceHdr->sh_picture_header_in_slice_header_flag)
        {
            sliceHdr->sh_lmcs_used_flag = Get1Bit();
        }
        else
        {
            sliceHdr->sh_lmcs_used_flag = (sliceHdr->sh_picture_header_in_slice_header_flag) ? picHeader->ph_lmcs_enabled_flag : false;
        }
        if (picHeader->ph_explicit_scaling_list_enabled_flag && !sliceHdr->sh_picture_header_in_slice_header_flag)
        {
            sliceHdr->sh_explicit_scaling_list_used_flag = Get1Bit();
        }
        else
        {
            sliceHdr->sh_explicit_scaling_list_used_flag = (sliceHdr->sh_picture_header_in_slice_header_flag) ? picHeader->ph_explicit_scaling_list_enabled_flag : false;
        }

        if (pps->pps_rpl_info_in_ph_flag)
        {
            sliceHdr->sh_rpl[0] = picHeader->rpl[0];
            sliceHdr->sh_rpl[1] = picHeader->rpl[1];
        }
        else if (sliceHdr->IDRflag && !sps->sps_idr_rpl_present_flag)
        {
            VVCReferencePictureList* rpl0 = &sliceHdr->sh_rpl[0];
            *rpl0 = VVCReferencePictureList();
            VVCReferencePictureList* rpl1 = &sliceHdr->sh_rpl[1];
            *rpl1 = VVCReferencePictureList();
        }
        else
        {
            // Read L0 related syntax elements
            bool rplSpsFlag0 = 0;

            if (sps->sps_num_ref_pic_lists[0] > 0)
            {
                sliceHdr->ref_pic_list_sps_flag[0] = Get1Bit();
            }
            else
            {
                sliceHdr->ref_pic_list_sps_flag[0] = 0;
            }

            rplSpsFlag0 = sliceHdr->ref_pic_list_sps_flag[0];

            VVCReferencePictureList* rpl[VVC_RPL_NUM];
            rpl[0] = &sliceHdr->sh_rpl[0];

            if (!sliceHdr->ref_pic_list_sps_flag[0])
            {
                (*rpl[0]) = VVCReferencePictureList();
                xParseRefPicListStruct(sps, rpl[0], -1);
                sliceHdr->sh_rpl_idx[0] = -1;
            }
            else
            {
                if (sps->sps_num_ref_pic_lists[0] > 1)
                {
                    int numBits = CeilLog2(sps->sps_num_ref_pic_lists[0]);
                    sliceHdr->sh_rpl_idx[0] = GetBits(numBits);
                    *rpl[0] = sps->sps_ref_pic_lists[0][sliceHdr->sh_rpl_idx[0]];
                }
                else
                {
                    sliceHdr->sh_rpl_idx[0] = 0;
                    *rpl[0] = sps->sps_ref_pic_lists[0][0];
                }
            }

            // Deal POC Msb cycle signalling for LTRP
            for (uint32_t i = 0; i < rpl[0]->number_of_long_term_pictures + rpl[0]->number_of_short_term_pictures; i++)
            {
                rpl[0]->delta_poc_msb_present_flag[i] = false;
                rpl[0]->delta_poc_msb_cycle_lt[i] = 0;
                sliceHdr->sh_rpl[0].delta_poc_msb_present_flag[i] = false;
                sliceHdr->sh_rpl[0].delta_poc_msb_cycle_lt[i] = 0;
            }
            if (rpl[0]->number_of_long_term_pictures)
            {
                for (uint32_t i = 0; i < rpl[0]->number_of_long_term_pictures + rpl[0]->number_of_short_term_pictures; i++)
                {
                    if (rpl[0]->is_long_term_ref_pic[i])
                    {
                        if (rpl[0]->ltrp_in_header_flag)
                        {
                            sliceHdr->slice_poc_lsb_lt[i] = GetBits(sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
                            sliceHdr->sh_rpl[0].poc_lsb_lt[i] = sliceHdr->slice_poc_lsb_lt[i];
                            xSetRefPicIdentifier(rpl[0], i, rpl[0]->poc_lsb_lt[i], true, false, 0);
                        }
                        sliceHdr->sh_rpl[0].delta_poc_msb_present_flag[i] = Get1Bit();
                        rpl[0]->delta_poc_msb_present_flag[i] = sliceHdr->sh_rpl[0].delta_poc_msb_present_flag[i];
                        bool deltaPocMsbPreFlag = rpl[0]->delta_poc_msb_present_flag[i];
                        if (deltaPocMsbPreFlag)
                        {
                            sliceHdr->slice_delta_poc_msb_cycle_lt[i] = GetVLCElementU();
                            sliceHdr->sh_rpl[0].delta_poc_msb_cycle_lt[i] = sliceHdr->slice_delta_poc_msb_cycle_lt[i];
                            int32_t deltaPocMsbCycleLt = sliceHdr->slice_delta_poc_msb_cycle_lt[i];
                            if (i != 0)
                            {
                                deltaPocMsbCycleLt += rpl[0]->delta_poc_msb_cycle_lt[i - 1];
                            }
                            rpl[0]->delta_poc_msb_cycle_lt[i] = deltaPocMsbCycleLt;
                        }
                        else if (i != 0)
                        {
                            rpl[0]->delta_poc_msb_cycle_lt[i] = rpl[0]->delta_poc_msb_cycle_lt[i - 1];
                            sliceHdr->sh_rpl[0].delta_poc_msb_cycle_lt[i] = sliceHdr->sh_rpl[0].delta_poc_msb_cycle_lt[i - 1];
                        }
                        else
                        {
                            rpl[0]->delta_poc_msb_cycle_lt[i] = 0;
                            sliceHdr->sh_rpl[0].delta_poc_msb_cycle_lt[i] = 0;
                        }
                    }
                    else if (i != 0)
                    {
                        rpl[0]->delta_poc_msb_cycle_lt[i] = rpl[0]->delta_poc_msb_cycle_lt[i - 1];
                        sliceHdr->sh_rpl[0].delta_poc_msb_cycle_lt[i] = sliceHdr->sh_rpl[0].delta_poc_msb_cycle_lt[i - 1];
                    }
                    else
                    {
                        rpl[0]->delta_poc_msb_cycle_lt[i] = 0;
                        sliceHdr->sh_rpl[0].delta_poc_msb_cycle_lt[i] = 0;
                    }
                }
            }

            // Read L1 related syntax elements
            if (sps->sps_num_ref_pic_lists[1] > 0 && pps->pps_rpl1_idx_present_flag)
            {
                sliceHdr->ref_pic_list_sps_flag[1] = Get1Bit();
                sliceHdr->sh_rpl[1].rpl_sps_flag = sliceHdr->ref_pic_list_sps_flag[1];
            }
            else if (sps->sps_num_ref_pic_lists[1] == 0)
            {
                sliceHdr->sh_rpl[1].rpl_sps_flag = 0;
            }
            else
            {
                sliceHdr->sh_rpl[1].rpl_sps_flag = rplSpsFlag0;
            }

            rpl[1] = &sliceHdr->sh_rpl[1];
            if (sliceHdr->sh_rpl[1].rpl_sps_flag)
            {
                if (sps->sps_num_ref_pic_lists[1] > 1 && pps->pps_rpl1_idx_present_flag)
                {
                    int numBits = CeilLog2(sps->sps_num_ref_pic_lists[1]);
                    sliceHdr->sh_rpl_idx[1] = GetBits(numBits);
                    *rpl[1] = sps->sps_ref_pic_lists[1][sliceHdr->sh_rpl_idx[1]];
                }
                else if (sps->sps_num_ref_pic_lists[1] == 1)
                {
                    sliceHdr->sh_rpl_idx[1] = 0;
                    *rpl[1] = sps->sps_ref_pic_lists[1][0];
                }
                else
                {
                    assert(sliceHdr->sh_rpl_idx[0] != -1);
                    sliceHdr->sh_rpl_idx[1] = sliceHdr->sh_rpl_idx[0];
                    *rpl[1] = sps->sps_ref_pic_lists[1][sliceHdr->sh_rpl_idx[0]];
                }
            }
            else
            {
                *rpl[1] = VVCReferencePictureList();
                xParseRefPicListStruct(sps, rpl[1], -1);
                sliceHdr->sh_rpl_idx[1] = -1;
            }

            // Deal POC Msb cycle signalling for LTRP
            for (uint32_t i = 0; i < rpl[1]->number_of_long_term_pictures + rpl[1]->number_of_short_term_pictures; i++)
            {
                rpl[1]->delta_poc_msb_present_flag[i] = false;
                rpl[1]->delta_poc_msb_cycle_lt[i] = 0;
                sliceHdr->sh_rpl[1].delta_poc_msb_present_flag[i] = false;
                sliceHdr->sh_rpl[1].delta_poc_msb_cycle_lt[i] = 0;
            }
            if (rpl[1]->number_of_long_term_pictures)
            {
                for (uint32_t i = 0; i < rpl[1]->number_of_long_term_pictures + rpl[1]->number_of_short_term_pictures; i++)
                {
                    if (rpl[1]->is_long_term_ref_pic[i])
                    {
                        if (rpl[1]->ltrp_in_header_flag)
                        {
                            sliceHdr->slice_poc_lsb_lt[i] = GetBits(sps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
                            sliceHdr->sh_rpl[1].poc_lsb_lt[i] = sliceHdr->slice_poc_lsb_lt[i];
                            xSetRefPicIdentifier(rpl[1], i, rpl[1]->poc_lsb_lt[i], true, false, 0);
                        }
                        sliceHdr->sh_rpl[1].delta_poc_msb_present_flag[i] = Get1Bit();
                        rpl[1]->delta_poc_msb_present_flag[i] = sliceHdr->sh_rpl[1].delta_poc_msb_present_flag[i];
                        bool deltaPocMsbPreFlag = rpl[1]->delta_poc_msb_present_flag[i];
                        if (deltaPocMsbPreFlag)
                        {
                            sliceHdr->slice_delta_poc_msb_cycle_lt[i] = GetVLCElementU();
                            sliceHdr->sh_rpl[1].delta_poc_msb_cycle_lt[i] = sliceHdr->slice_delta_poc_msb_cycle_lt[i];
                            int32_t deltaPocMsbCycleLt = sliceHdr->slice_delta_poc_msb_cycle_lt[i];
                            if (i != 0)
                            {
                                deltaPocMsbCycleLt += rpl[1]->delta_poc_msb_cycle_lt[i - 1];
                            }
                            rpl[1]->delta_poc_msb_cycle_lt[i] = deltaPocMsbCycleLt;
                        }
                        else if (i != 0)
                        {
                            rpl[1]->delta_poc_msb_cycle_lt[i] = rpl[1]->delta_poc_msb_cycle_lt[i - 1];
                            sliceHdr->sh_rpl[1].delta_poc_msb_cycle_lt[i] = sliceHdr->sh_rpl[1].delta_poc_msb_cycle_lt[i - 1];
                        }
                        else
                        {
                            rpl[1]->delta_poc_msb_cycle_lt[i] = 0;
                            sliceHdr->sh_rpl[1].delta_poc_msb_cycle_lt[i] = 0;
                        }
                    }
                    else if (i != 0)
                    {
                        rpl[1]->delta_poc_msb_cycle_lt[i] = rpl[1]->delta_poc_msb_cycle_lt[i - 1];
                        sliceHdr->sh_rpl[1].delta_poc_msb_cycle_lt[i] = sliceHdr->sh_rpl[1].delta_poc_msb_cycle_lt[i - 1];
                    }
                    else
                    {
                        rpl[1]->delta_poc_msb_cycle_lt[i] = 0;
                        sliceHdr->sh_rpl[1].delta_poc_msb_cycle_lt[i] = 0;
                    }
                }
            }
        }

        if (!pps->pps_rpl_info_in_ph_flag && sliceHdr->IDRflag && !sps->sps_idr_rpl_present_flag)
        {
            sliceHdr->num_ref_Idx[REF_PIC_LIST_0] = 0;
            sliceHdr->num_ref_Idx[REF_PIC_LIST_1] = 0;
        }
        if ((sliceHdr->slice_type != I_SLICE && sliceHdr->sh_rpl[0].num_ref_entries > 1)
            || (sliceHdr->slice_type == B_SLICE && sliceHdr->sh_rpl[1].num_ref_entries > 1))
        {
            sliceHdr->sh_num_ref_idx_active_override_flag = Get1Bit();
            if (sliceHdr->sh_num_ref_idx_active_override_flag)
            {
                if (sliceHdr->sh_rpl[0].num_ref_entries > 1)
                {
                    sliceHdr->sh_num_ref_idx_active_minus1[0] = GetVLCElementU();
                }
                else
                {
                    sliceHdr->sh_num_ref_idx_active_minus1[0] = 0;
                }
                sliceHdr->num_ref_Idx[REF_PIC_LIST_0] = sliceHdr->sh_num_ref_idx_active_minus1[0] + 1;
                if (sliceHdr->slice_type == B_SLICE)
                {
                    if (sliceHdr->sh_rpl[1].num_ref_entries > 1)
                    {
                        sliceHdr->sh_num_ref_idx_active_minus1[1] = GetVLCElementU();
                    }
                    else
                    {
                        sliceHdr->sh_num_ref_idx_active_minus1[1] = 0;
                    }
                    sliceHdr->num_ref_Idx[REF_PIC_LIST_1] = sliceHdr->sh_num_ref_idx_active_minus1[1] + 1;
                }
                else
                {
                    sliceHdr->num_ref_Idx[REF_PIC_LIST_1] = 0;
                }
            }
            else
            {
                if (sliceHdr->sh_rpl[0].num_ref_entries > pps->pps_num_ref_idx_default_active_minus1[0])
                {
                    sliceHdr->num_ref_Idx[REF_PIC_LIST_0] = pps->pps_num_ref_idx_default_active_minus1[0] + 1;
                }
                else
                {
                    sliceHdr->num_ref_Idx[REF_PIC_LIST_0] = sliceHdr->sh_rpl[0].num_ref_entries;
                }

                if (sliceHdr->slice_type == B_SLICE)
                {
                    if (sliceHdr->sh_rpl[1].num_ref_entries > pps->pps_num_ref_idx_default_active_minus1[1])
                    {
                        sliceHdr->num_ref_Idx[REF_PIC_LIST_1] = pps->pps_num_ref_idx_default_active_minus1[1] + 1;
                    }
                    else
                    {
                        sliceHdr->num_ref_Idx[REF_PIC_LIST_1] = sliceHdr->sh_rpl[1].num_ref_entries;
                    }
                }
                else
                {
                    sliceHdr->num_ref_Idx[REF_PIC_LIST_1] = 0;
                }
            }
        }
        else
        {
            sliceHdr->num_ref_Idx[REF_PIC_LIST_0] = (sliceHdr->slice_type == I_SLICE) ? 0 : 1;
            sliceHdr->num_ref_Idx[REF_PIC_LIST_1] = (sliceHdr->slice_type == B_SLICE) ? 1 : 0;
        }
        sliceHdr->sh_num_ref_idx_active_minus1[REF_PIC_LIST_0] = (uint32_t)std::max(0, sliceHdr->num_ref_Idx[REF_PIC_LIST_0] - 1);
        sliceHdr->sh_num_ref_idx_active_minus1[REF_PIC_LIST_1] = (uint32_t)std::max(0, sliceHdr->num_ref_Idx[REF_PIC_LIST_1] - 1);

        if (sliceHdr->slice_type == P_SLICE || sliceHdr->slice_type == B_SLICE)
        {
            if (sliceHdr->num_ref_Idx[REF_PIC_LIST_0] == 0)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            if (sliceHdr->slice_type == B_SLICE && sliceHdr->num_ref_Idx[1] == 0)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
        }

        sliceHdr->cabac_init_flag = false;
        if (pps->pps_cabac_init_present_flag && sliceHdr->slice_type != I_SLICE)
        {
            sliceHdr->cabac_init_flag = Get1Bit();
            sliceHdr->enc_CABA_table_idx = (sliceHdr->slice_type == B_SLICE) ? (sliceHdr->cabac_init_flag ? P_SLICE : B_SLICE)
                : (sliceHdr->cabac_init_flag ? B_SLICE : P_SLICE);
        }

        if (picHeader->ph_temporal_mvp_enabled_flag)
        {
            if (sliceHdr->slice_type == P_SLICE)
            {
                sliceHdr->sh_collocated_from_l0_flag = true;
            }
            else if (!pps->pps_rpl_info_in_ph_flag && sliceHdr->slice_type == B_SLICE)
            {
                sliceHdr->sh_collocated_from_l0_flag = Get1Bit();
            }
            else
            {
                sliceHdr->sh_collocated_from_l0_flag = picHeader->ph_collocated_from_l0_flag;
            }

            if (!pps->pps_rpl_info_in_ph_flag)
            {
                if (sliceHdr->slice_type != I_SLICE
                    && ((sliceHdr->sh_collocated_from_l0_flag && sliceHdr->num_ref_Idx[REF_PIC_LIST_0] > 1)
                        || (!sliceHdr->sh_collocated_from_l0_flag && sliceHdr->num_ref_Idx[REF_PIC_LIST_1] > 1)))
                {
                    sliceHdr->sh_collocated_ref_idx = GetVLCElementU();
                }
                else
                {
                    sliceHdr->sh_collocated_ref_idx = 0;
                }
            }
            else
            {
                sliceHdr->sh_collocated_ref_idx = picHeader->ph_collocated_ref_idx;
            }
        }
        else
        {
            sliceHdr->sh_collocated_from_l0_flag = true;
            sliceHdr->sh_collocated_ref_idx = 0;
        }

        if ((pps->pps_weighted_pred_flag && sliceHdr->slice_type == P_SLICE)
            || (pps->pps_weighted_bipred_flag && sliceHdr->slice_type == B_SLICE))
        {
            if (pps->pps_wp_info_in_ph_flag)
            {
                if ((sliceHdr->num_ref_Idx[REF_PIC_LIST_0] > picHeader->ph_num_lx_weights[REF_PIC_LIST_0])
                    || (sliceHdr->num_ref_Idx[REF_PIC_LIST_1] > picHeader->ph_num_lx_weights[REF_PIC_LIST_1]))
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                MFX_INTERNAL_CPY(&sliceHdr->weightPredTable, &picHeader->weightPredTable, sizeof(VVCParsedWP));
            }
            else
            {
                xParsePredWeightTable(sps, sliceHdr);
            }
        }
        else
        {
            memset(&sliceHdr->weightPredTable, 0, sizeof(VVCParsedWP));
        }
    
        int QPDelta = 0;
        if (pps->pps_qp_delta_info_in_ph_flag) 
        {
            QPDelta = picHeader->ph_qp_delta;
        }
        else 
        {
            sliceHdr->sh_qp_delta = GetVLCElementS();
            QPDelta = sliceHdr->sh_qp_delta;
        }
        sliceHdr->sliceQP = pps->pps_init_qp_minus26 + 26 + QPDelta;
        if (sliceHdr->sliceQP<-sps->sps_qp_bd_offset[CHANNEL_TYPE_LUMA] || sliceHdr->sliceQP>VVC_MAX_QP)
        {
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }

        if (pps->pps_slice_chroma_qp_offsets_present_flag) 
        {
            if (numValidComp > COMPONENT_Cb) 
            {
                sliceHdr->sh_cb_qp_offset = GetVLCElementS();
                sliceHdr->sh_chroma_qp_delta[COMPONENT_Cb] = sliceHdr->sh_cb_qp_offset;
                if ((sliceHdr->sh_chroma_qp_delta[COMPONENT_Cb] > 12)
                    || (sliceHdr->sh_chroma_qp_delta[COMPONENT_Cb] < -12)
                    || ((pps->pps_cb_qp_offset + sliceHdr->sh_chroma_qp_delta[COMPONENT_Cb]) > 12)
                    || ((pps->pps_cb_qp_offset + sliceHdr->sh_chroma_qp_delta[COMPONENT_Cb]) < -12))
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
            }
            else
            {
                sliceHdr->sh_cb_qp_offset = 0;
                sliceHdr->sh_chroma_qp_delta[COMPONENT_Cb] = sliceHdr->sh_cb_qp_offset;
            }
            if (numValidComp > COMPONENT_Cr)
            {
                sliceHdr->sh_cr_qp_offset = GetVLCElementS();
                sliceHdr->sh_chroma_qp_delta[COMPONENT_Cr] = sliceHdr->sh_cr_qp_offset;
                if ((sliceHdr->sh_chroma_qp_delta[COMPONENT_Cr] > 12)
                    || (sliceHdr->sh_chroma_qp_delta[COMPONENT_Cr] < -12)
                    || ((pps->pps_cr_qp_offset + sliceHdr->sh_chroma_qp_delta[COMPONENT_Cr]) > 12)
                    || ((pps->pps_cr_qp_offset + sliceHdr->sh_chroma_qp_delta[COMPONENT_Cr]) < -12))
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                if (sps->sps_joint_cbcr_enabled_flag) 
                {
                    sliceHdr->sh_joint_cbcr_qp_offset = GetVLCElementS();
                    sliceHdr->sh_chroma_qp_delta[JOINT_CbCr] = sliceHdr->sh_joint_cbcr_qp_offset;
                    if ((sliceHdr->sh_chroma_qp_delta[JOINT_CbCr] > 12)
                        || (sliceHdr->sh_chroma_qp_delta[JOINT_CbCr] < -12)
                        || ((pps->pps_joint_cbcr_qp_offset_value + sliceHdr->sh_chroma_qp_delta[JOINT_CbCr]) > 12)
                        || ((pps->pps_joint_cbcr_qp_offset_value + sliceHdr->sh_chroma_qp_delta[JOINT_CbCr]) < -12))
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                }
                else
                {
                    sliceHdr->sh_joint_cbcr_qp_offset = 0;
                }
            }
            else
            {
                sliceHdr->sh_cb_qp_offset = 0;      
                sliceHdr->sh_cr_qp_offset = 0;
                sliceHdr->sh_joint_cbcr_qp_offset = 0;
            }
	        sliceHdr->sh_chroma_qp_delta[COMPONENT_Cr] = sliceHdr->sh_cr_qp_offset;
            sliceHdr->sh_chroma_qp_delta[JOINT_CbCr] = sliceHdr->sh_joint_cbcr_qp_offset;
            sliceHdr->sh_chroma_qp_delta[COMPONENT_Cb] = sliceHdr->sh_cb_qp_offset;
        }
        else
        {
                sliceHdr->sh_cr_qp_offset = 0;
                sliceHdr->sh_cb_qp_offset = 0;
                sliceHdr->sh_joint_cbcr_qp_offset = 0;
                sliceHdr->sh_chroma_qp_delta[COMPONENT_Cr] = sliceHdr->sh_cr_qp_offset;
                sliceHdr->sh_chroma_qp_delta[JOINT_CbCr] = sliceHdr->sh_joint_cbcr_qp_offset;                           
                sliceHdr->sh_chroma_qp_delta[COMPONENT_Cb] = sliceHdr->sh_cb_qp_offset;
        }

        if (pps->pps_cu_chroma_qp_offset_list_enabled_flag) 
        {
            sliceHdr->sh_cu_chroma_qp_offset_enabled_flag = Get1Bit();
        }
        else
        {
            sliceHdr->sh_cu_chroma_qp_offset_enabled_flag = 0;
        }

        if (sps->sps_sao_enabled_flag && !pps->pps_sao_info_in_ph_flag) 
        {
            sliceHdr->sh_sao_luma_used_flag = Get1Bit();
            if (chFmt)
            {
                sliceHdr->sh_sao_chroma_used_flag = Get1Bit();
            }
        }

        if (pps->pps_deblocking_filter_control_present_flag) 
        {
            if (pps->pps_deblocking_filter_override_enabled_flag && !pps->pps_dbf_info_in_ph_flag) 
            {
                sliceHdr->sh_deblocking_params_present_flag = Get1Bit();
            }
            else
            {
                sliceHdr->sh_deblocking_params_present_flag = false;
            }
            if (sliceHdr->sh_deblocking_params_present_flag)
            {
                if (!pps->pps_deblocking_filter_disabled_flag) 
                {
                    sliceHdr->deblocking_filter_disable_flag = Get1Bit();
                }
                else
                {
                    sliceHdr->deblocking_filter_disable_flag = false;
                }
                if (!sliceHdr->deblocking_filter_disable_flag) 
                {
                    sliceHdr->sh_luma_beta_offset_div2 = GetVLCElementS();
                    sliceHdr->sh_luma_tc_offset_div2 = GetVLCElementS();
                    if ((sliceHdr->sh_luma_beta_offset_div2 < -12)
                        || (sliceHdr->sh_luma_beta_offset_div2 > 12)
                        || (sliceHdr->sh_luma_tc_offset_div2 < -12)
                        || (sliceHdr->sh_luma_tc_offset_div2 > 12)) 
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                    if (pps->pps_chroma_tool_offsets_present_flag) 
                    {
                        sliceHdr->sh_cb_beta_offset_div2 = GetVLCElementS();
                        sliceHdr->sh_cb_tc_offset_div2 = GetVLCElementS();
                        sliceHdr->sh_cr_beta_offset_div2 = GetVLCElementS();
                        sliceHdr->sh_cr_tc_offset_div2 = GetVLCElementS();
                        if ((sliceHdr->sh_cb_beta_offset_div2 < -12)
                            || (sliceHdr->sh_cb_beta_offset_div2 > 12)
                            || (sliceHdr->sh_cb_tc_offset_div2 < -12)
                            || (sliceHdr->sh_cb_tc_offset_div2 > 12)
                            || (sliceHdr->sh_cr_beta_offset_div2 < -12)
                            || (sliceHdr->sh_cr_beta_offset_div2 > 12)
                            || (sliceHdr->sh_cr_tc_offset_div2 < -12)
                            || (sliceHdr->sh_cr_tc_offset_div2 > 12))
                        {
                            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                        }
                    }
                    else 
                    {
                        sliceHdr->sh_cb_beta_offset_div2 = sliceHdr->sh_luma_beta_offset_div2;
                        sliceHdr->sh_cb_tc_offset_div2 = sliceHdr->sh_luma_tc_offset_div2;
                        sliceHdr->sh_cr_beta_offset_div2 = sliceHdr->sh_luma_beta_offset_div2;
                        sliceHdr->sh_cr_tc_offset_div2 = sliceHdr->sh_luma_tc_offset_div2;
                    }
                }
            }
            else 
            {
                sliceHdr->deblocking_filter_disable_flag = picHeader->ph_deblocking_filter_disabled_flag;
                sliceHdr->sh_luma_beta_offset_div2 = picHeader->ph_luma_beta_offset_div2;
                sliceHdr->sh_luma_tc_offset_div2 = picHeader->ph_luma_tc_offset_div2;
                sliceHdr->sh_cb_beta_offset_div2 = picHeader->ph_cb_beta_offset_div2;
                sliceHdr->sh_cb_tc_offset_div2 = picHeader->ph_cb_tc_offset_div2;
                sliceHdr->sh_cr_beta_offset_div2 = picHeader->ph_cr_beta_offset_div2;
                sliceHdr->sh_cr_tc_offset_div2 = picHeader->ph_cr_tc_offset_div2;
            }
        }
        else 
        {
            sliceHdr->deblocking_filter_disable_flag = false;
            sliceHdr->sh_luma_beta_offset_div2 = 0;
            sliceHdr->sh_luma_tc_offset_div2 = 0;
            sliceHdr->sh_cb_beta_offset_div2 = 0;
            sliceHdr->sh_cb_tc_offset_div2 = 0;
            sliceHdr->sh_cr_beta_offset_div2 = 0;
            sliceHdr->sh_cr_tc_offset_div2 = 0;
        }

        // dependent quantization
        if (sps->sps_dep_quant_enabled_flag) 
        {
            sliceHdr->sh_dep_quant_used_flag = Get1Bit();
        }
        else
        {
            sliceHdr->sh_dep_quant_used_flag = false;
        }
        
        // sign data hiding
        if (sps->sps_sign_data_hiding_enabled_flag && !sliceHdr->sh_dep_quant_used_flag) 
        {
            sliceHdr->sh_sign_data_hiding_used_flag = Get1Bit();
        }
        else
        {
            sliceHdr->sh_sign_data_hiding_used_flag = false;
        }

        // signal TS residual coding disabled flag
        if (sps->sps_transform_skip_enabled_flag && !sliceHdr->sh_dep_quant_used_flag && !sliceHdr->sh_sign_data_hiding_used_flag) 
        {
            sliceHdr->sh_ts_residual_coding_disabled_flag = Get1Bit();
        }
        else
        {
            sliceHdr->sh_ts_residual_coding_disabled_flag = 0;
        }

        if (!sliceHdr->sh_ts_residual_coding_disabled_flag && sps->sps_ts_residual_coding_rice_present_in_sh_flag) 
        {
            sliceHdr->sh_ts_residual_coding_rice_idx_minus1 = GetBits(3);
        }
        if (sps->reverse_last_position_enabled_flag) 
        {
            sliceHdr->sh_reverse_last_sig_coeff_flag = Get1Bit();
        }
        
        if (slice->GetFirstCtuRsAddrInSlice() == 0) 
        {
            slice->SetDefaultClpRng(*sps);
        }
        if (pps->pps_slice_header_extension_present_flag)
        {
            uint32_t slice_header_extension_length = GetVLCElementU();

            if (slice_header_extension_length > 256)
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);

            for (uint32_t i = 0; i < slice_header_extension_length; i++)
            {
                GetBits(8); // slice_header_extension_data_byte
            }
        }
        slice->ResetNumberOfSubstream();
        slice->SetNumSubstream(sps, pps);
        slice->SetNumEntryPoints(sps, pps);
        sliceHdr->NumEntryPoints = slice->GetNumEntryPoints();

        if (slice->GetNumEntryPoints())
        {
            uint32_t offsetLenMinus1 = GetVLCElementU();
            if (offsetLenMinus1 > 31)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }

            std::vector<uint32_t> entryPointOffsets;
            entryPointOffsets.resize(slice->GetNumEntryPoints());
            for (uint32_t idx = 0; idx < slice->GetNumEntryPoints(); idx++)
            {
                entryPointOffsets[idx] = GetBits(offsetLenMinus1 + 1) + 1;
            }

            slice->allocateTileLocation(sliceHdr->num_entry_point_offsets + 1);

            unsigned prevPos = 0;
            slice->m_tileByteLocation[0] = 0;
            for (uint32_t idx = 1; idx < slice->getTileLocationCount(); idx++)
            {
                slice->m_tileByteLocation[idx] = prevPos + entryPointOffsets[idx - 1];
                prevPos += entryPointOffsets[idx - 1];
            }
        }
        else
        {
            sliceHdr->num_entry_point_offsets = 0;

            slice->allocateTileLocation(1);
            slice->m_tileByteLocation[0] = 0;
        }

        ReadOutTrailingBits();

        if (CheckBSLeft())
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);

        return sts;
    }

    UMC::Status VVCHeadersBitstream::GetPictureHeader(VVCPicHeader *picHeader, ParameterSetManager* m_currHeader, bool readRbspTrailingBits)
    {
        if (!picHeader)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        UMC::Status sts = UMC::UMC_OK;

        picHeader->ph_gdr_or_irap_pic_flag = Get1Bit();
        picHeader->ph_non_ref_pic_flag = Get1Bit();

        if (picHeader->ph_gdr_or_irap_pic_flag)
        {
            picHeader->ph_gdr_pic_flag = Get1Bit();
        }
        else
        {
            picHeader->ph_gdr_pic_flag = false;
        }

        picHeader->ph_inter_slice_allowed_flag = Get1Bit();

        if (picHeader->ph_inter_slice_allowed_flag)
        {
            picHeader->ph_intra_slice_allowed_flag = Get1Bit();
        }
        else
        {
            picHeader->ph_intra_slice_allowed_flag = true;
        }

        if (picHeader->ph_inter_slice_allowed_flag == 0 && picHeader->ph_intra_slice_allowed_flag == 0)
        {
            // Invalid picture without intra or inter slice
            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
        }

        picHeader->ph_pic_parameter_set_id = GetVLCElementU();
        VVCPicParamSet* pPps = m_currHeader->m_picParams.GetHeader(picHeader->ph_pic_parameter_set_id);
        if (!pPps)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        VVCSeqParamSet* pSps = m_currHeader->m_seqParams.GetHeader(pPps->pps_seq_parameter_set_id);

        if (!pSps)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        picHeader->ph_pic_order_cnt_lsb = GetBits(pSps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);

        if (picHeader->ph_gdr_pic_flag)
        {
            picHeader->ph_recovery_poc_cnt = GetVLCElementU();
        }
        else 
        {
            picHeader->ph_recovery_poc_cnt = -1;
        }

        if ((!picHeader->ph_gdr_or_irap_pic_flag && picHeader->ph_gdr_pic_flag)
            && (!picHeader->ph_gdr_pic_flag && picHeader->ph_recovery_poc_cnt)) 
        {
            if (pSps->profile_tier_level.general_profile_idc == VVC_MAIN_12_INTRA 
                || pSps->profile_tier_level.general_profile_idc == VVC_MAIN_12_444_INTRA 
                || pSps->profile_tier_level.general_profile_idc == VVC_MAIN_16_444_INTRA)
            {
                throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
        }

        std::vector<bool> phExtraBitsPresent = pSps->sps_extra_ph_bit_present_flag;
        picHeader->ph_extra_bit.resize(pSps->sps_num_extra_ph_bytes * 8);
        for (uint32_t i = 0; i < pSps->sps_num_extra_ph_bytes * 8; i++)
        {
            if (phExtraBitsPresent[i])
            {
                picHeader->ph_extra_bit[i] = Get1Bit();
            }
            else
            {
                picHeader->ph_extra_bit[i] = 0;
            }
        }

        if (pSps->sps_poc_msb_cycle_flag)
        {
            picHeader->ph_poc_msb_cycle_present_flag = Get1Bit();
            if (picHeader->ph_poc_msb_cycle_present_flag)
            {
                picHeader->ph_poc_msb_cycle_val = GetBits(pSps->sps_poc_msb_cycle_len_minus1 + 1);
            }
        }
        else
        {
            picHeader->ph_poc_msb_cycle_present_flag = 0;
            picHeader->ph_poc_msb_cycle_val = 0;
        }

        picHeader->ph_alf_cc_cb_enabled_flag = false;
        picHeader->ph_alf_cc_cr_enabled_flag = false;
        if (pSps->sps_alf_enabled_flag)
        {
            if (pPps->pps_alf_info_in_ph_flag)
            {
                bool alfCbEnabledFlag = false;
                bool alfCrEnabledFlag = false;
                picHeader->ph_alf_enabled_flag = Get1Bit();
                if (picHeader->ph_alf_enabled_flag)
                {
                    picHeader->ph_num_alf_aps_ids_luma = GetBits(3);
                    picHeader->ph_alf_aps_id_luma.resize(picHeader->ph_num_alf_aps_ids_luma);
                    for (uint32_t i = 0; i < picHeader->ph_num_alf_aps_ids_luma; i++)
                    {
                        picHeader->ph_alf_aps_id_luma[i] = static_cast<uint8_t>(GetBits(3));
                    }
                    if (pSps->sps_chroma_format_idc)
                    {
                        alfCbEnabledFlag = Get1Bit();
                        alfCrEnabledFlag = Get1Bit();
                    }
                    else
                    {
                        alfCbEnabledFlag = false;
                        alfCrEnabledFlag = false;
                    }
                    if (alfCbEnabledFlag || alfCrEnabledFlag)
                    {
                        picHeader->ph_alf_aps_id_chroma = GetBits(3);
                    }
                    else
                    {
                        picHeader->ph_alf_aps_id_chroma = 0;
                    }
                    if (pSps->sps_ccalf_enabled_flag)
                    {
                        picHeader->ph_alf_cc_cb_enabled_flag = Get1Bit();
                        picHeader->ph_alf_cc_cb_aps_id = -1;
                        if (picHeader->ph_alf_cc_cb_enabled_flag)
                        {
                            picHeader->ph_alf_cc_cb_aps_id = GetBits(3);
                        }

                        picHeader->ph_alf_cc_cr_enabled_flag = Get1Bit();
                        picHeader->ph_alf_cc_cr_aps_id = -1;
                        if (picHeader->ph_alf_cc_cr_enabled_flag)
                        {
                            picHeader->ph_alf_cc_cr_aps_id = GetBits(3);
                        }
                    }
                    else
                    {
                        picHeader->ph_alf_cc_cb_enabled_flag = 0;
                        picHeader->ph_alf_cc_cb_aps_id = -1;
                        picHeader->ph_alf_cc_cr_enabled_flag = 0;
                    }
                }
                else
                {
                    picHeader->ph_num_alf_aps_ids_luma = 0;
                    alfCbEnabledFlag = false;
                    alfCrEnabledFlag = false;
                    picHeader->ph_alf_aps_id_chroma = 0;
                    picHeader->ph_alf_cc_cb_enabled_flag = 0;
                    picHeader->ph_alf_cc_cr_enabled_flag = 0;
                    picHeader->ph_alf_cc_cr_aps_id = -1;
                }
                picHeader->ph_alf_cb_enabled_flag = alfCbEnabledFlag;
                picHeader->ph_alf_cr_enabled_flag = alfCrEnabledFlag;
            }
            else
            {
                picHeader->ph_alf_enabled_flag = false;
                picHeader->ph_alf_cb_enabled_flag = false;
                picHeader->ph_alf_cr_enabled_flag = false;
                picHeader->ph_num_alf_aps_ids_luma = 0;
                picHeader->ph_alf_aps_id_chroma = 0;
                picHeader->ph_alf_cc_cb_enabled_flag = 0;
                picHeader->ph_alf_cc_cr_enabled_flag = 0;
                picHeader->ph_alf_cc_cr_aps_id = -1;
            }
        }
        else
        {
            picHeader->ph_alf_enabled_flag = false;
            picHeader->ph_alf_cb_enabled_flag = false;
            picHeader->ph_alf_cr_enabled_flag = false;
            picHeader->ph_num_alf_aps_ids_luma = 0;
            picHeader->ph_alf_aps_id_chroma = 0;
            picHeader->ph_alf_cc_cb_enabled_flag = 0;
            picHeader->ph_alf_cc_cr_enabled_flag = 0;
            picHeader->ph_alf_cc_cr_aps_id = -1;
        }

        if (pSps->sps_lmcs_enabled_flag)
        {
            picHeader->ph_lmcs_enabled_flag = Get1Bit();
            if (picHeader->ph_lmcs_enabled_flag)
            {
                picHeader->ph_lmcs_aps_id = GetBits(2);
                if (pSps->sps_chroma_format_idc)
                {
                    picHeader->ph_chroma_residual_scale_flag = Get1Bit();
                }
                else
                {
                    picHeader->ph_chroma_residual_scale_flag = false;
                }
            }
            else
            {
                picHeader->ph_lmcs_aps_id = 0;
                picHeader->ph_chroma_residual_scale_flag = false;
            }
        }
        else
        {
            picHeader->ph_lmcs_enabled_flag = false;
            picHeader->ph_chroma_residual_scale_flag = false;
            picHeader->ph_lmcs_aps_id = 0;
        }
        picHeader->ph_scaling_list_aps_id = false;
        if (pSps->sps_explicit_scaling_list_enabled_flag)
        {
            picHeader->ph_explicit_scaling_list_enabled_flag = Get1Bit();
            if (picHeader->ph_explicit_scaling_list_enabled_flag)
            {
                picHeader->ph_scaling_list_aps_id = GetBits(3);
            }
        }
        else
        {
            picHeader->ph_explicit_scaling_list_enabled_flag = false;
        }

        // initialize tile/slice info for no partitioning case
        if (pPps->pps_no_pic_partition_flag) 
        {
            xResetTileSliceInfo(pPps);
            pPps->pps_log2_ctu_size = CeilLog2(pSps->sps_ctu_size);
            pPps->pps_ctu_size = 1 << pPps->pps_log2_ctu_size;
            pPps->pps_pic_width_in_ctu = (pPps->pps_pic_width_in_luma_samples + pPps->pps_ctu_size - 1) / pPps->pps_ctu_size;
            pPps->pps_pic_height_in_ctu = (pPps->pps_pic_height_in_luma_samples + pPps->pps_ctu_size - 1) / pPps->pps_ctu_size;
            pPps->pps_num_exp_tile_columns_minus1 = 0;
            pPps->pps_num_exp_tile_rows_minus1 = 0;
            pPps->pps_tile_column_width.push_back(pPps->pps_pic_width_in_ctu);
            pPps->pps_tile_row_height.push_back(pPps->pps_pic_height_in_ctu);
            xInitTiles(pPps);
            pPps->pps_rect_slice_flag = 1;
            pPps->pps_num_slices_in_pic_minus1 = 0;
            for (uint32_t i = 0; i < pPps->pps_num_slices_in_pic_minus1 + 1; i++)
            {
                pPps->pps_rect_slices.push_back(new VVCRectSlice());
            }
            pPps->pps_tile_idx_delta_present_flag = 0;
            pPps->pps_rect_slices[0]->pps_tile_idx = 0;
            xInitRectSliceMap(pPps,pSps);
        }
        else
        {
            if (pPps->pps_rect_slice_flag)
            {
                xInitRectSliceMap(pPps, pSps);
            }
        }

        xInitSubPic(pSps, pPps);

        if (pSps->sps_virtual_boundaries_enabled_flag && !pSps->sps_virtual_boundaries_present_flag)
        {
            picHeader->ph_virtual_boundaries_present_flag = Get1Bit();
            if (picHeader->ph_virtual_boundaries_present_flag)
            {
                picHeader->ph_num_ver_virtual_boundaries = GetVLCElementU();
                if (pPps->pps_pic_width_in_luma_samples <= 8 && picHeader->ph_num_ver_virtual_boundaries != 0)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                else if (pPps->pps_pic_width_in_luma_samples > 8 && picHeader->ph_num_ver_virtual_boundaries > 3)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }

                for (uint32_t i = 0; i < picHeader->ph_num_ver_virtual_boundaries; i++)
                {
                    picHeader->ph_virtual_boundary_pos_x_minus1[i] = GetVLCElementU();
                    uint32_t ceilMaxPicWidthInBlockMinus2 = ((pPps->pps_pic_width_in_luma_samples + 7) >> 3) - 2;
                    if (picHeader->ph_virtual_boundary_pos_x_minus1[i] > ceilMaxPicWidthInBlockMinus2)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                }
                picHeader->ph_num_hor_virtual_boundaries = GetVLCElementU();
                for (uint32_t i = 0; i < picHeader->ph_num_hor_virtual_boundaries; i++)
                {
                    picHeader->ph_virtual_boundary_pos_y_minus1[i] = GetVLCElementU();
                }
            }
            else
            {
                picHeader->ph_num_ver_virtual_boundaries = 0;
                picHeader->ph_num_hor_virtual_boundaries = 0;
            }
        }
        else
        {
            picHeader->ph_virtual_boundaries_present_flag = pSps->sps_virtual_boundaries_present_flag;
            if (picHeader->ph_virtual_boundaries_present_flag)
            {
                picHeader->ph_num_ver_virtual_boundaries = pSps->sps_num_ver_virtual_boundaries;
                picHeader->ph_num_hor_virtual_boundaries = pSps->sps_num_hor_virtual_boundaries;
                for (uint32_t i = 0; i < 3; i++)
                {
                    picHeader->ph_virtual_boundary_pos_x_minus1[i] = pSps->sps_virtual_boundary_pos_x_minus1[i];
                    picHeader->ph_virtual_boundary_pos_y_minus1[i] = pSps->sps_virtual_boundary_pos_y_minus1[i];
                }
            }
            else
            {
                picHeader->ph_num_ver_virtual_boundaries = 0;
                picHeader->ph_num_hor_virtual_boundaries = 0;               
            }
        }

        if (pPps->pps_output_flag_present_flag && !picHeader->ph_non_ref_pic_flag)
        {
            picHeader->ph_pic_output_flag = Get1Bit();
        }
        else
        {
            picHeader->ph_pic_output_flag = true;
        }

        std::fill_n(picHeader->rpl_idx, VVC_MAX_NUM_REF_PIC_LISTS , -1);

        if (pPps->pps_rpl_info_in_ph_flag)
        {
            xParseRefPicLists(picHeader, pSps, pPps, -1);
        }

        if (pSps->sps_partition_constraints_override_enabled_flag)
        {
            picHeader->ph_partition_constraints_override_flag = Get1Bit();
        }
        else
        {
            picHeader->ph_partition_constraints_override_flag = false;
        }

        uint32_t minQT[3] = { 0, 0, 0 };
        uint32_t maxBTD[3] = { 0, 0, 0 };
        uint32_t maxBTSize[3] = { 0, 0, 0 };
        uint32_t maxTTSize[3] = { 0, 0, 0 };
        uint32_t ctbLog2SizeY = FloorLog2(pSps->sps_ctu_size);

        if (picHeader->ph_intra_slice_allowed_flag)
        {
            if (picHeader->ph_partition_constraints_override_flag)
            {
                picHeader->ph_log2_diff_min_qt_min_cb_intra_slice_luma = GetVLCElementU();
                picHeader->ph_max_mtt_hierarchy_depth_intra_slice_luma = GetVLCElementU();

                uint32_t minQtLog2SizeIntraY = picHeader->ph_log2_diff_min_qt_min_cb_intra_slice_luma +
                                               pSps->sps_log2_min_luma_coding_block_size_minus2;
                minQT[0] = 1 << minQtLog2SizeIntraY;
                maxBTD[0] = picHeader->ph_max_mtt_hierarchy_depth_intra_slice_luma;

                if (minQT[0] > VVC_MIN_CTB_SIZE)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }

                maxTTSize[0] = maxBTSize[0] = minQT[0];
                if (picHeader->ph_max_mtt_hierarchy_depth_intra_slice_luma != 0)
                {
                    picHeader->ph_log2_diff_max_bt_min_qt_intra_slice_luma = GetVLCElementU();
                    maxBTSize[0] <<= picHeader->ph_log2_diff_max_bt_min_qt_intra_slice_luma;
                    if (picHeader->ph_log2_diff_max_bt_min_qt_intra_slice_luma > ctbLog2SizeY - minQtLog2SizeIntraY)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                    picHeader->ph_log2_diff_max_tt_min_qt_intra_slice_luma = GetVLCElementU();
                    maxTTSize[0] <<= picHeader->ph_log2_diff_max_tt_min_qt_intra_slice_luma;
                    if (maxTTSize[0] > VVC_MIN_CTB_SIZE)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                }

                if (pSps->sps_qtbtt_dual_tree_intra_flag)
                {
                    picHeader->ph_log2_diff_min_qt_min_cb_intra_slice_chroma = GetVLCElementU();
                    minQT[2] = 1 << (picHeader->ph_log2_diff_min_qt_min_cb_intra_slice_chroma +
                               pSps->sps_log2_min_luma_coding_block_size_minus2);
                    if (minQT[2] > VVC_MIN_CTB_SIZE)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }

                    picHeader->ph_max_mtt_hierarchy_depth_intra_slice_chroma = GetVLCElementU();
                    maxBTD[2] =picHeader->ph_max_mtt_hierarchy_depth_intra_slice_chroma;
                    maxTTSize[2] = maxBTSize[2] = minQT[2];

                    if (picHeader->ph_max_mtt_hierarchy_depth_intra_slice_chroma != 0)
                    {
                        picHeader->ph_log2_diff_max_bt_min_qt_intra_slice_chroma = GetVLCElementU();
                        maxBTSize[2] <<= picHeader->ph_log2_diff_max_bt_min_qt_intra_slice_chroma;
                        picHeader->ph_log2_diff_max_tt_min_qt_intra_slice_chroma = GetVLCElementU();
                        maxTTSize[2] <<= picHeader->ph_log2_diff_max_tt_min_qt_intra_slice_chroma;
                        if (maxBTSize[2] > VVC_MIN_CTB_SIZE || maxTTSize[2] > VVC_MIN_CTB_SIZE)
                        {
                            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                        }
                    }
                }
            }
            else
            {
                picHeader->ph_log2_diff_min_qt_min_cb_intra_slice_luma = 0;
                picHeader->ph_max_mtt_hierarchy_depth_intra_slice_luma = 0;
                picHeader->ph_log2_diff_max_bt_min_qt_intra_slice_luma = 0;
                picHeader->ph_log2_diff_max_tt_min_qt_intra_slice_luma = 0;
                picHeader->ph_log2_diff_min_qt_min_cb_intra_slice_chroma = 0;
                picHeader->ph_max_mtt_hierarchy_depth_intra_slice_chroma = 0;
                picHeader->ph_log2_diff_max_bt_min_qt_intra_slice_chroma = 0;
                picHeader->ph_log2_diff_max_tt_min_qt_intra_slice_chroma  = 0;
            }
            if (pPps->pps_cu_qp_delta_enabled_flag)
            {
                picHeader->ph_cu_qp_delta_subdiv_intra_slice = GetVLCElementU();
            }
            else
            {
                picHeader->ph_cu_qp_delta_subdiv_intra_slice = 0;
            }
            if (pPps->pps_cu_chroma_qp_offset_list_enabled_flag)
            {
                picHeader->ph_cu_chroma_qp_offset_subdiv_intra_slice = GetVLCElementU();
            }
            else
            {
                picHeader->ph_cu_chroma_qp_offset_subdiv_intra_slice = 0;
            }
        }
        picHeader->ph_mvd_l1_zero_flag = false;

        if (picHeader->ph_inter_slice_allowed_flag)
        {
            if (picHeader->ph_partition_constraints_override_flag)
            {
                picHeader->ph_log2_diff_min_qt_min_cb_inter_slice = GetVLCElementU();
                uint32_t minQtLog2SizeInterY = picHeader->ph_log2_diff_min_qt_min_cb_inter_slice +
                                               pSps->sps_log2_min_luma_coding_block_size_minus2;
                minQT[1] = 1 << minQtLog2SizeInterY;
                if (minQT[1] > VVC_MIN_CTB_SIZE)
                {
                    throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
                picHeader->ph_max_mtt_hierarchy_depth_inter_slice = GetVLCElementU();
                if (picHeader->ph_max_mtt_hierarchy_depth_inter_slice != 0)
                {
                    picHeader->ph_log2_diff_max_bt_min_qt_inter_slice = GetVLCElementU();
                    picHeader->ph_log2_diff_max_tt_min_qt_inter_slice = GetVLCElementU();
                }
            }
            else
            {
                picHeader->ph_log2_diff_min_qt_min_cb_inter_slice = 0;
                picHeader->ph_max_mtt_hierarchy_depth_inter_slice = 0;
                picHeader->ph_log2_diff_max_bt_min_qt_inter_slice = 0;
                picHeader->ph_log2_diff_max_tt_min_qt_inter_slice = 0;
            }
            if (pPps->pps_cu_qp_delta_enabled_flag)
            {
                picHeader->ph_cu_qp_delta_subdiv_inter_slice = GetVLCElementU();
            }
            else
            {
                picHeader->ph_cu_qp_delta_subdiv_inter_slice = 0;
            }
            if (pPps->pps_cu_chroma_qp_offset_list_enabled_flag)
            {
                picHeader->ph_cu_chroma_qp_offset_subdiv_inter_slice = GetVLCElementU();
            }
            else
            {
                picHeader->ph_cu_chroma_qp_offset_subdiv_inter_slice = 0;
            }

            if (pSps->sps_temporal_mvp_enabled_flag)
            {
                picHeader->ph_temporal_mvp_enabled_flag = Get1Bit();
            }
            else
            {
                picHeader->ph_temporal_mvp_enabled_flag = false;
            }

            if (picHeader->ph_temporal_mvp_enabled_flag && pPps->pps_rpl_info_in_ph_flag)
            {
                if (picHeader->rpl[1].num_ref_entries > 0)
                {
                    picHeader->ph_collocated_from_l0_flag = Get1Bit();
                }
                else
                {
                    picHeader->ph_collocated_from_l0_flag = true;
                }
                if ((picHeader->ph_collocated_from_l0_flag &&
                    picHeader->rpl[0].num_ref_entries > 1) ||
                    (!picHeader->ph_collocated_from_l0_flag &&
                        picHeader->rpl[1].num_ref_entries > 1))
                {
                    picHeader->ph_collocated_ref_idx = GetVLCElementU();
                }
                else
                {
                    picHeader->ph_collocated_ref_idx = 0;
                }
            }
            else
            {
                picHeader->ph_collocated_from_l0_flag = false;
                picHeader->ph_collocated_ref_idx  = 0;
            }

            if (pSps->sps_mmvd_fullpel_only_enabled_flag)
            {
                picHeader->ph_mmvd_fullpel_only_flag = Get1Bit();
            }
            else
            {
                picHeader->ph_mmvd_fullpel_only_flag = false;
            }

            if (!pPps->pps_rpl_info_in_ph_flag || picHeader->rpl[1].num_ref_entries > 0)
            {
                picHeader->ph_mvd_l1_zero_flag = Get1Bit();
            }
            else
            {
                picHeader->ph_mvd_l1_zero_flag = true;
            }

            if (pSps->sps_bdof_control_present_in_ph_flag &&
                (!pPps->pps_rpl_info_in_ph_flag || picHeader->rpl[1].num_ref_entries > 0))
            {
                picHeader->ph_bdof_disabled_flag = Get1Bit();
            }
            else
            {
                if (!pSps->sps_bdof_control_present_in_ph_flag)
                {
                    picHeader->ph_bdof_disabled_flag = !pSps->sps_bdof_enabled_flag;
                }
                else
                {
                    picHeader->ph_bdof_disabled_flag = true;
                }
            }

            if (pSps->sps_dmvr_control_present_in_ph_flag &&
                (!pPps->pps_rpl_info_in_ph_flag || picHeader->rpl[1].num_ref_entries > 0))
            {
                picHeader->ph_dmvr_disabled_flag = Get1Bit();
            }
            else
            {
                if (!pSps->sps_dmvr_control_present_in_ph_flag)
                {
                    picHeader->ph_dmvr_disabled_flag = !pSps->sps_dmvr_enabled_flag;
                }
                else
                {
                    picHeader->ph_dmvr_disabled_flag = true;
                }
            }

            if (pSps->sps_prof_control_present_in_ph_flag)
            {
                picHeader->ph_prof_disabled_flag = Get1Bit();
            }
            else
            {
                picHeader->ph_prof_disabled_flag = true;
            }

            if ((pPps->pps_weighted_pred_flag || pPps->pps_weighted_bipred_flag) &&
                pPps->pps_wp_info_in_ph_flag)
            {
                xParsePredWeightTable(picHeader, pSps, pPps);
            }
        }
        else
        {
            picHeader->ph_log2_diff_min_qt_min_cb_inter_slice = 0;
            picHeader->ph_max_mtt_hierarchy_depth_inter_slice = 0;
            picHeader->ph_log2_diff_max_bt_min_qt_inter_slice = 0;
            picHeader->ph_log2_diff_max_tt_min_qt_inter_slice = 0;
            picHeader->ph_cu_qp_delta_subdiv_inter_slice = 0;
            picHeader->ph_cu_chroma_qp_offset_subdiv_inter_slice = 0;
            picHeader->ph_temporal_mvp_enabled_flag = false;
            picHeader->ph_collocated_ref_idx = 0;
            picHeader->ph_collocated_from_l0_flag = true;
            picHeader->ph_mmvd_fullpel_only_flag = false;
            picHeader->ph_mvd_l1_zero_flag = false;
            picHeader->ph_bdof_disabled_flag = false;
            picHeader->ph_dmvr_disabled_flag = false;
            picHeader->ph_prof_disabled_flag = false;
        }

        if (pPps->pps_qp_delta_info_in_ph_flag)
        {
            picHeader->ph_qp_delta = GetVLCElementS();
        }
        else
        {
            picHeader->ph_qp_delta = 0;
        }

        if (pSps->sps_joint_cbcr_enabled_flag)
        {
            picHeader->ph_joint_cbcr_sign_flag = Get1Bit();
        }
        else
        {
            picHeader->ph_joint_cbcr_sign_flag = false;
        }

        if (pSps->sps_sao_enabled_flag)
        {
            if (pPps->pps_sao_info_in_ph_flag)
            {
                picHeader->ph_sao_luma_enabled_flag = Get1Bit();

                if (pSps->sps_chroma_format_idc != 0)
                {
                    picHeader->ph_sao_chroma_enabled_flag = Get1Bit();
                }
                else
                {
                    picHeader->ph_sao_chroma_enabled_flag = pSps->sps_chroma_format_idc != 0;
                }
            }
            else
            {
                picHeader->ph_sao_luma_enabled_flag = true;
                picHeader->ph_sao_chroma_enabled_flag = pSps->sps_chroma_format_idc != 0;
            }
        }
        else
        {
            picHeader->ph_sao_luma_enabled_flag = false;
            picHeader->ph_sao_chroma_enabled_flag = false;
        }

        if (pPps->pps_deblocking_filter_control_present_flag)
        {
            if (pPps->pps_dbf_info_in_ph_flag)
            {
                picHeader->ph_deblocking_params_present_flag = Get1Bit();
            }
            else
            {
                picHeader->ph_deblocking_params_present_flag = false;
            }

            if (picHeader->ph_deblocking_params_present_flag)
            {
                if (!pPps->pps_deblocking_filter_disabled_flag)
                {
                    picHeader->ph_deblocking_filter_disabled_flag = Get1Bit();
                }
                else
                {
                    picHeader->ph_deblocking_filter_disabled_flag = false;
                }
                if (!picHeader->ph_deblocking_filter_disabled_flag)
                {
                    picHeader->ph_luma_beta_offset_div2 = GetVLCElementS();
                    if (picHeader->ph_luma_beta_offset_div2 > VVC_DEBLOCKING_OFFSET_UPPER_BOUND ||
                        picHeader->ph_luma_beta_offset_div2 < VVC_DEBLOCKING_OFFSET_LOWER_BOUND)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                    picHeader->ph_luma_tc_offset_div2 = GetVLCElementS();
                    if (picHeader->ph_luma_tc_offset_div2 > VVC_DEBLOCKING_OFFSET_UPPER_BOUND ||
                        picHeader->ph_luma_tc_offset_div2 < VVC_DEBLOCKING_OFFSET_LOWER_BOUND)
                    {
                        throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                    }
                    if (pPps->pps_chroma_tool_offsets_present_flag)
                    {
                        picHeader->ph_cb_beta_offset_div2 = GetVLCElementS();
                        if (picHeader->ph_cb_beta_offset_div2 > VVC_DEBLOCKING_OFFSET_UPPER_BOUND ||
                            picHeader->ph_cb_beta_offset_div2 < VVC_DEBLOCKING_OFFSET_LOWER_BOUND)
                        {
                            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                        }
                        picHeader->ph_cb_tc_offset_div2 = GetVLCElementS();
                        if (picHeader->ph_cb_tc_offset_div2 > VVC_DEBLOCKING_OFFSET_UPPER_BOUND ||
                            picHeader->ph_cb_tc_offset_div2 < VVC_DEBLOCKING_OFFSET_LOWER_BOUND)
                        {
                            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                        }
                        picHeader->ph_cr_beta_offset_div2 = GetVLCElementS();
                        if (picHeader->ph_cr_beta_offset_div2 > VVC_DEBLOCKING_OFFSET_UPPER_BOUND ||
                            picHeader->ph_cr_beta_offset_div2 < VVC_DEBLOCKING_OFFSET_LOWER_BOUND)
                        {
                            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                        }
                        picHeader->ph_cr_tc_offset_div2 = GetVLCElementS();
                        if (picHeader->ph_cr_tc_offset_div2 > VVC_DEBLOCKING_OFFSET_UPPER_BOUND ||
                            picHeader->ph_cr_tc_offset_div2 < VVC_DEBLOCKING_OFFSET_LOWER_BOUND)
                        {
                            throw vvc_exception(UMC::UMC_ERR_INVALID_STREAM);
                        }
                    }
                    else
                    {
                        picHeader->ph_cb_beta_offset_div2 = picHeader->ph_luma_beta_offset_div2;
                        picHeader->ph_cb_tc_offset_div2 = picHeader->ph_luma_tc_offset_div2;
                        picHeader->ph_cr_beta_offset_div2 = picHeader->ph_luma_beta_offset_div2;
                        picHeader->ph_cr_tc_offset_div2 = picHeader->ph_luma_tc_offset_div2;
                    }
                }
                else
                {
                    picHeader->ph_luma_beta_offset_div2 = pPps->pps_luma_beta_offset_div2;
                    picHeader->ph_luma_tc_offset_div2 = pPps->pps_luma_tc_offset_div2;
                    picHeader->ph_cb_beta_offset_div2 = pPps->pps_cb_beta_offset_div2;
                    picHeader->ph_cb_tc_offset_div2 = pPps->pps_cb_tc_offset_div2;
                    picHeader->ph_cr_beta_offset_div2 = pPps->pps_cr_beta_offset_div2;
                    picHeader->ph_cr_tc_offset_div2 = pPps->pps_cr_tc_offset_div2;
                }
            }
            else
            {
                picHeader->ph_deblocking_filter_disabled_flag = pPps->pps_deblocking_filter_disabled_flag;
                picHeader->ph_luma_beta_offset_div2 = pPps->pps_luma_beta_offset_div2;
                picHeader->ph_luma_tc_offset_div2 = pPps->pps_luma_tc_offset_div2;
                picHeader->ph_cb_beta_offset_div2 = pPps->pps_cb_beta_offset_div2;
                picHeader->ph_cb_tc_offset_div2 = pPps->pps_cb_tc_offset_div2;
                picHeader->ph_cr_beta_offset_div2 = pPps->pps_cr_beta_offset_div2;
                picHeader->ph_cr_tc_offset_div2 = pPps->pps_cr_tc_offset_div2;
            }
        }
        else
        {
            picHeader->ph_deblocking_filter_disabled_flag = false;
            picHeader->ph_luma_beta_offset_div2 = 0;
            picHeader->ph_luma_tc_offset_div2 = 0;
            picHeader->ph_cb_beta_offset_div2 = 0;
            picHeader->ph_cb_tc_offset_div2 = 0;
            picHeader->ph_cr_beta_offset_div2 = 0;
            picHeader->ph_cr_tc_offset_div2 = 0;
        }

        if (pPps->pps_picture_header_extension_present_flag)
        {
            picHeader->ph_extension_length = GetVLCElementU();
            picHeader->ph_extension_data_byte.resize(picHeader->ph_extension_length);
            for (uint32_t i = 0; i < picHeader->ph_extension_length; i++)
            {
                picHeader->ph_extension_data_byte[i] = GetBits(8);
            }
        }
        else
        {
            picHeader->ph_extension_length = 0;
        }

        if (readRbspTrailingBits)
        {
            ReadOutTrailingBits();
        }

        return sts;
    }

    UMC::Status VVCHeadersBitstream::GetOperatingPointInformation(VVCOPI *opi)
    {
        if (!opi)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        UMC::Status sts = UMC::UMC_OK;
        opi->opi_ols_info_present_flag = Get1Bit();
        opi->opi_htid_info_present_flag = Get1Bit();
        if (opi->opi_ols_info_present_flag)
        {
            opi->opi_ols_idx = GetVLCElementU();
        }
        else
        {
            opi->opi_ols_idx = 0;
        }

        if (opi->opi_htid_info_present_flag)
        {
            opi->opi_htid_plus1 = GetBits(3);
        }
        else
        {
            opi->opi_htid_plus1 = 0;
        }

        opi->opi_extension_flag = Get1Bit();
        if (opi->opi_extension_flag)
        {
            while (MoreRbspData())
            {
                opi->opi_extension_data_flag = Get1Bit();
            }
        }
        else
        {
            opi->opi_extension_data_flag = 0;
        }
        ReadOutTrailingBits();
        return sts;
    }

    void VVCHeadersBitstream::xParseRefPicLists(VVCPicHeader *picHeader, const VVCSeqParamSet *pSps, const VVCPicParamSet *pPps, int32_t rplIdx)
    {
        bool rplSpsFlag0 = 0;
        // List0 and List1
        for (uint32_t listIdx = 0; listIdx < 2; listIdx++)
        {
            bool rps_sps_flag;
            if (pSps->sps_num_ref_pic_lists[listIdx] > 0 && (listIdx == 0 ||
                (listIdx == 1 && pPps->pps_rpl1_idx_present_flag)))
            {
                picHeader->rpl_sps_flag[listIdx] = Get1Bit();
                rps_sps_flag = picHeader->rpl_sps_flag[listIdx];
            }
            else if (pSps->sps_num_ref_pic_lists[listIdx] == 0)
            {
                rps_sps_flag = 0;
            }
            else
            {
                rps_sps_flag = rplSpsFlag0;
            }
            picHeader->rpl_sps_flag[listIdx] = rps_sps_flag;

            if (listIdx == 0)
            {
                rplSpsFlag0 = rps_sps_flag;
            }
            picHeader->ph_num_ref_entries_rpl_larger_than0[listIdx] = rps_sps_flag;

            // explicit RPL in picture header
            auto const refPicList = &picHeader->rpl[listIdx];
            if (!rps_sps_flag)
            {
                (*refPicList) = VVCReferencePictureList();
                xParseRefPicListStruct(pSps, refPicList, rplIdx);
            }
            // use list from SPS
            else
            {
                picHeader->rpl_idx[listIdx] = 0;
                if (pSps->sps_num_ref_pic_lists[listIdx] > 1 && (listIdx == 0 ||
                    (listIdx == 1 && pPps->pps_rpl1_idx_present_flag)))
                {
                    int numBits = CeilLog2(pSps->sps_num_ref_pic_lists[listIdx]);
                    picHeader->rpl_idx[listIdx] = GetBits(numBits);
                    *refPicList = pSps->sps_ref_pic_lists[listIdx][picHeader->rpl_idx[listIdx]];
                }
                else if (pSps->sps_num_ref_pic_lists[listIdx] == 1)
                {
                    picHeader->rpl_idx[listIdx] = 0;
                    *refPicList = pSps->sps_ref_pic_lists[listIdx][picHeader->rpl_idx[0]];
                }
                else
                {
                    picHeader->rpl_idx[listIdx] = picHeader->rpl_idx[0];
                    *refPicList = pSps->sps_ref_pic_lists[listIdx][picHeader->rpl_idx[listIdx]];
                }
            }
            // POC MSB cycle signalling for LTRP
            for (uint32_t i = 0; i < refPicList->number_of_long_term_pictures + refPicList->number_of_short_term_pictures; i++)
            {
                picHeader->rpl[listIdx].delta_poc_msb_present_flag[i] = false;
                picHeader->rpl[listIdx].delta_poc_msb_cycle_lt[i] = 0;
            }
            if (refPicList->number_of_long_term_pictures)
            {
                for (uint32_t i = 0; i < refPicList->number_of_long_term_pictures + refPicList->number_of_short_term_pictures; i++)
                {
                    if (refPicList->is_long_term_ref_pic[i])
                    {
                        if (refPicList->ltrp_in_header_flag)
                        {
                            picHeader->rpl[listIdx].poc_lsb_lt[i] = GetBits(pSps->sps_log2_max_pic_order_cnt_lsb_minus4 + 4);
                            xSetRefPicIdentifier(refPicList, i, picHeader->rpl[listIdx].poc_lsb_lt[i], true, false, 0);
                        }
                        picHeader->rpl[listIdx].delta_poc_msb_present_flag[i] = Get1Bit();
                        if (picHeader->rpl[listIdx].delta_poc_msb_present_flag[i])
                        {
                            picHeader->rpl[listIdx].delta_poc_msb_cycle_lt[i] = GetVLCElementU();
                            if (i != 0)
                            {
                                picHeader->rpl[listIdx].delta_poc_msb_cycle_lt[i] += refPicList->delta_poc_msb_cycle_lt[i - 1];
                            }
                        }
                        else if (i != 0)
                        {
                            picHeader->rpl[listIdx].delta_poc_msb_cycle_lt[i] = picHeader->rpl[listIdx].delta_poc_msb_cycle_lt[i - 1];
                        }
                        else
                        {
                            picHeader->rpl[listIdx].delta_poc_msb_cycle_lt[i] = 0;
                        }
                    }
                    else if (i != 0)
                    {
                        picHeader->rpl[listIdx].delta_poc_msb_cycle_lt[i] = picHeader->rpl[listIdx].delta_poc_msb_cycle_lt[i - 1];
                    }
                    else
                    {
                        picHeader->rpl[listIdx].delta_poc_msb_cycle_lt[i] = 0;
                    }
                }
            }
        }
    }

    void VVCHeadersBitstream::xParsePredWeightTable(const VVCSeqParamSet* sps, VVCSliceHeader* sliceHdr)
    {
        uint32_t log2WeightDenomLuma = GetVLCElementU();
        uint32_t NumRefIdxL0 = sliceHdr->num_ref_Idx[REF_PIC_LIST_0];
        uint32_t NumRefIdxL1 = sliceHdr->num_ref_Idx[REF_PIC_LIST_1];
        int32_t deltaDenom = 0;

        if (sps->sps_chroma_format_idc != 0)
        {
            deltaDenom = GetVLCElementS();
        }
        VVCParsedWP* wp = &sliceHdr->weightPredTable;   
        for(uint32_t i = 0; i < 15; i++)
        {
            wp->luma_weight_l0_flag[i] = 0;
            wp->chroma_weight_l0_flag[i] = 0;
            wp->delta_luma_weight_l0[i] = 0;
            wp->luma_offset_l0[i] = 0;
            wp->luma_weight_l1_flag[i] = 0;
            wp->chroma_weight_l1_flag[i] = 0;
            wp->delta_luma_weight_l1[i] = 0;
            wp->luma_offset_l1[i] = 0;
            for(uint32_t j = 0; j <2; j++)
            {
                wp->delta_chroma_weight_l0[i][j] = 0;
                wp->delta_chroma_offset_l0[i][j] = 0;
                wp->delta_chroma_weight_l1[i][j] = 0;
                wp->delta_chroma_offset_l1[i][j] = 0;
            }
        }
        memset(wp,0,sizeof(VVCParsedWP));
        wp->luma_log2_weight_denom = log2WeightDenomLuma;
        wp->delta_chroma_log2_weight_denom = deltaDenom;

        wp->num_l0_weights = NumRefIdxL0;

        for (uint32_t refIdx = 0; refIdx < NumRefIdxL0; refIdx++)
        {
            wp->luma_weight_l0_flag[refIdx] = Get1Bit();
        }
        if (sps->sps_chroma_format_idc != 0)
        {
            for (uint32_t i = 0; i < NumRefIdxL0; i++)
            {
                wp->chroma_weight_l0_flag[i] = Get1Bit();
            }
        }

        for (uint32_t i = 0; i < NumRefIdxL0; i++)
        {
            wp->delta_luma_weight_l0[i] = wp->luma_weight_l0_flag[i] ? GetVLCElementS() : 0;
            wp->luma_offset_l0[i] = wp->luma_weight_l0_flag[i] ? GetVLCElementS() : 0;

            for (uint32_t j = 0; j < 2; j++)
            {
                wp->delta_chroma_weight_l0[i][j] = wp->chroma_weight_l0_flag[i] ? GetVLCElementS() : 0;
                wp->delta_chroma_offset_l0[i][j] = wp->chroma_weight_l0_flag[i] ? GetVLCElementS() : 0;
            }
        }

        wp->num_l1_weights = NumRefIdxL1;

        for (uint32_t i = 0; i < NumRefIdxL1; i++)
        {
            wp->luma_weight_l1_flag[i] = Get1Bit();
        }

        if (sps->sps_chroma_format_idc != 0)
        {
            for (uint32_t i = 0; i < NumRefIdxL1; i++)
            {
                wp->chroma_weight_l1_flag[i] = Get1Bit();
            }
        }

        for (uint32_t i = 0; i < NumRefIdxL1; i++)
        {
            wp->delta_luma_weight_l1[i] = wp->luma_weight_l1_flag[i] ? GetVLCElementS() : 0;
            wp->luma_offset_l1[i] = wp->luma_weight_l1_flag[i] ? GetVLCElementS() : 0;

            for (uint32_t j = 0; j < 2; j++)
            {
                wp->delta_chroma_weight_l1[i][j] = wp->chroma_weight_l1_flag[i] ? GetVLCElementS() : 0;
                wp->delta_chroma_offset_l1[i][j] = wp->chroma_weight_l1_flag[i] ? GetVLCElementS() : 0;
            }
        }
    }

    void VVCHeadersBitstream::xParsePredWeightTable(VVCPicHeader *picHeader, const VVCSeqParamSet *pSps, const VVCPicParamSet *pPps)
    {
        uint32_t log2WeightDenomLuma = GetVLCElementU();
        uint32_t NumL0Weights = 0;
        int32_t deltaDenom = 0;
        uint32_t NumL1Weights = 0;

        if (pSps->sps_chroma_format_idc != 0)
        {
            deltaDenom = GetVLCElementS();
        }

        NumL0Weights = GetVLCElementU();

        if (pPps->pps_wp_info_in_ph_flag)
        {
            picHeader->ph_num_lx_weights[REF_PIC_LIST_0] = NumL0Weights;
        }
        else
        {
            picHeader->ph_num_lx_weights[REF_PIC_LIST_0] = 0;
        }

        VVCParsedWP* wp = &picHeader->weightPredTable;
        for(uint32_t i = 0; i < 15; i++)
        {
            wp->luma_weight_l0_flag[i] = 0;
            wp->chroma_weight_l0_flag[i] = 0;
            wp->delta_luma_weight_l0[i] = 0;
            wp->luma_offset_l0[i] = 0;
            wp->luma_weight_l1_flag[i] = 0;
            wp->chroma_weight_l1_flag[i] = 0;
            wp->delta_luma_weight_l1[i] = 0;
            wp->luma_offset_l1[i] = 0;
            for(uint32_t j = 0; j <2; j++)
            {
                wp->delta_chroma_weight_l0[i][j] = 0;
                wp->delta_chroma_offset_l0[i][j] = 0;
                wp->delta_chroma_weight_l1[i][j] = 0;
                wp->delta_chroma_offset_l1[i][j] = 0;
            }
        }
        memset(wp,0,sizeof(VVCParsedWP));
        wp->luma_log2_weight_denom = log2WeightDenomLuma;
        wp->delta_chroma_log2_weight_denom = deltaDenom;

        wp->num_l0_weights = NumL0Weights;

        for (uint32_t refIdx = 0; refIdx < NumL0Weights; refIdx++)
        {
            wp->luma_weight_l0_flag[refIdx] = Get1Bit();
        }
        if (pSps->sps_chroma_format_idc != 0)
        {
            for (uint32_t i = 0; i < NumL0Weights; i++)
            {
                wp->chroma_weight_l0_flag[i] = Get1Bit();
            }
        }

        for (uint32_t i = 0; i < NumL0Weights; i++)
        {
            wp->delta_luma_weight_l0[i] = wp->luma_weight_l0_flag[i] ? GetVLCElementS() : 0;
            wp->luma_offset_l0[i] = wp->luma_weight_l0_flag[i] ? GetVLCElementS() : 0;

            for (uint32_t j = 0; j < 2; j++)
            {
                wp->delta_chroma_weight_l0[i][j] = wp->chroma_weight_l0_flag[i] ? GetVLCElementS() : 0;
                wp->delta_chroma_offset_l0[i][j] = wp->chroma_weight_l0_flag[i] ? GetVLCElementS() : 0;
            }
        }

        if (pPps->pps_weighted_bipred_flag &&
            pPps->pps_wp_info_in_ph_flag &&
            picHeader->rpl[1].num_ref_entries > 0)
        {
            NumL1Weights = GetVLCElementU();
            picHeader->ph_num_lx_weights[REF_PIC_LIST_1] = NumL1Weights;
        }
        else
        {
            NumL1Weights = 0;
            picHeader->ph_num_lx_weights[REF_PIC_LIST_1] = 0;
        }

        wp->num_l1_weights = NumL1Weights;

        for (uint32_t i = 0; i < NumL1Weights; i++)
        {
            wp->luma_weight_l1_flag[i] = Get1Bit();
        }

        if (pSps->sps_chroma_format_idc != 0)
        {
            for (uint32_t i = 0; i < NumL1Weights; i++)
            {
                wp->chroma_weight_l1_flag[i] = Get1Bit();
            }
        }

        for (uint32_t i = 0; i < NumL1Weights; i++)
        {
            wp->delta_luma_weight_l1[i] = wp->luma_weight_l1_flag[i] ? GetVLCElementS() : 0;
            wp->luma_offset_l1[i] = wp->luma_weight_l1_flag[i] ? GetVLCElementS() : 0;

            for (uint32_t j = 0; j < 2; j++)
            {
                wp->delta_chroma_weight_l1[i][j] = wp->chroma_weight_l1_flag[i] ? GetVLCElementS() : 0;
                wp->delta_chroma_offset_l1[i][j] = wp->chroma_weight_l1_flag[i] ? GetVLCElementS() : 0;
            }
        }
    }

    bool VVCHeadersBitstream::MoreRbspData()
    {
        return false;
    }

    // ...

}

#endif // MFX_ENABLE_VVC_VIDEO_DECODE
