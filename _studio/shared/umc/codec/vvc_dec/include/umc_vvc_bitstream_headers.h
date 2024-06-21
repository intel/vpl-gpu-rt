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

#pragma once

#include "umc_defs.h"

#ifdef MFX_ENABLE_VVC_VIDEO_DECODE

#ifndef __UMC_VVC_BITSTREAM_HEADERS_H_
#define __UMC_VVC_BITSTREAM_HEADERS_H_

#include "umc_structures.h"
#include "umc_vvc_headers_manager.h"

// Read N bits from 32-bit array
#define VVCGetNBits(current_data, offset, nbits, data) \
{                                              \
    uint32_t x;                                \
                                               \
    assert((nbits) > 0 && (nbits) <= 32);      \
    assert(offset >= 0 && offset <= 31);       \
                                               \
    offset -= (nbits);                         \
                                               \
    if (offset >= 0)                           \
    {                                          \
        x = current_data[0] >> (offset + 1);   \
    }                                          \
    else                                       \
    {                                          \
        offset += 32;                          \
                                               \
        x = current_data[1] >> (offset);       \
        x >>= 1;                               \
        x += current_data[0] << (31 - offset); \
        current_data++;                        \
    }                                          \
                                               \
    assert(offset >= 0 && offset <= 31);       \
                                               \
    (data) = x & bits_data[nbits];             \
}

// Return bitstream position pointers N bits back
#define VVCUngetNBits(current_data, offset, nbits) \
{                                              \
    assert(offset >= 0 && offset <= 31);       \
                                               \
    offset += (nbits);                         \
    if (offset > 31)                           \
    {                                          \
        offset -= 32;                          \
        current_data--;                        \
    }                                          \
                                               \
    assert(offset >= 0 && offset <= 31);       \
}

// Skip N bits in 32-bit array
#define SkipNBits(current_data, offset, nbits) \
{                                              \
    /* check error(s) */                       \
    assert((nbits) > 0 && (nbits) <= 32);      \
    assert(offset >= 0 && offset <= 31);       \
    /* decrease number of available bits */    \
    offset -= (nbits);                         \
    /* normalize bitstream pointer */          \
    if (0 > offset)                            \
    {                                          \
        offset += 32;                          \
        current_data++;                        \
    }                                          \
    /* check error(s) again */                 \
    assert(offset >= 0 && offset <= 31);       \
 }

// Read 1 bit from 32-bit array
#define GetBits1(current_data, offset, data)   \
{                                              \
    data = ((current_data[0] >> (offset)) & 1);\
    offset -= 1;                               \
    if (offset < 0)                            \
    {                                          \
        offset = 31;                           \
        current_data += 1;                     \
    }                                          \
}

// Read N bits from 32-bit array
#define PeakNextBits(current_data, bp, nbits, data) \
{                                              \
    uint32_t x;                                \
                                               \
    assert((nbits) > 0 && (nbits) <= 32);      \
    assert(nbits >= 0 && nbits <= 31);         \
                                               \
    int32_t offset = bp - (nbits);             \
                                               \
    if (offset >= 0)                           \
    {                                          \
        x = current_data[0] >> (offset + 1);   \
    }                                          \
    else                                       \
    {                                          \
        offset += 32;                          \
                                               \
        x = current_data[1] >> (offset);       \
        x >>= 1;                               \
        x += current_data[0] << (31 - offset); \
    }                                          \
                                               \
    assert(offset >= 0 && offset <= 31);       \
                                               \
    (data) = x & bits_data[nbits];             \
}

namespace UMC_VVC_DECODER
{
    // Bit masks for fast extraction of bits from bitstream
    const uint32_t bits_data[33] =
    {
        (((uint32_t)0x01 << (0)) - 1),
        (((uint32_t)0x01 << (1)) - 1),
        (((uint32_t)0x01 << (2)) - 1),
        (((uint32_t)0x01 << (3)) - 1),
        (((uint32_t)0x01 << (4)) - 1),
        (((uint32_t)0x01 << (5)) - 1),
        (((uint32_t)0x01 << (6)) - 1),
        (((uint32_t)0x01 << (7)) - 1),
        (((uint32_t)0x01 << (8)) - 1),
        (((uint32_t)0x01 << (9)) - 1),
        (((uint32_t)0x01 << (10)) - 1),
        (((uint32_t)0x01 << (11)) - 1),
        (((uint32_t)0x01 << (12)) - 1),
        (((uint32_t)0x01 << (13)) - 1),
        (((uint32_t)0x01 << (14)) - 1),
        (((uint32_t)0x01 << (15)) - 1),
        (((uint32_t)0x01 << (16)) - 1),
        (((uint32_t)0x01 << (17)) - 1),
        (((uint32_t)0x01 << (18)) - 1),
        (((uint32_t)0x01 << (19)) - 1),
        (((uint32_t)0x01 << (20)) - 1),
        (((uint32_t)0x01 << (21)) - 1),
        (((uint32_t)0x01 << (22)) - 1),
        (((uint32_t)0x01 << (23)) - 1),
        (((uint32_t)0x01 << (24)) - 1),
        (((uint32_t)0x01 << (25)) - 1),
        (((uint32_t)0x01 << (26)) - 1),
        (((uint32_t)0x01 << (27)) - 1),
        (((uint32_t)0x01 << (28)) - 1),
        (((uint32_t)0x01 << (29)) - 1),
        (((uint32_t)0x01 << (30)) - 1),
        (((uint32_t)0x01 << (31)) - 1),
        ((uint32_t)0xFFFFFFFF),
    };

    // Scan order tables class
    class VVCScanOrderTables
    {
    public:
        static VVCScanOrderTables& getInstance()
        {
            static VVCScanOrderTables instance;
            return instance;
        }

        static uint32_t GetBLKWidthSize() { return m_blkWidthSize; }
        static uint32_t GetBLKHeightSize() { return m_blkHeightSize; }

        static uint32_t GetBLKWidthIndex(uint32_t blkWidth);
        static uint32_t GetBLKHeightIndex(uint32_t blkHeight);

        ScanElement* GetScanOrder(CoeffScanGroupType scanGType, CoeffScanType scanType, uint32_t blkWidthIndex, uint32_t blkHeightIndex) const;

    private:
        VVCScanOrderTables();
        virtual ~VVCScanOrderTables();

        // Initialize scan order array
        void InitScanOrderTables();
        // Destroy scan order array
        void DestroyScanOrderTables();

        ScanElement* m_scanOrders[SCAN_NUMBER_OF_GROUP_TYPES][SCAN_NUMBER_OF_TYPES][VVC_MAX_CU_SIZE / 2 + 1][VVC_MAX_CU_SIZE / 2 + 1];
        static const uint32_t m_blkWidths[8];
        static const uint32_t m_blkHeights[8];
        static const uint32_t m_blkWidthSize;
        static const uint32_t m_blkHeightSize;

    };

    // Bitstream low level parser/utility class
    class VVCBitstreamUtils
    {
    public:

        VVCBitstreamUtils();
        VVCBitstreamUtils(uint8_t * const pb, const uint32_t maxsize);
        virtual ~VVCBitstreamUtils();

        // Reset the bitstream with new data pointer
        void Reset(uint8_t * const pb, const uint32_t maxsize);
        // Reset the bitstream with new data pointer and bit offset
        void Reset(uint8_t * const pb, int32_t offset, const uint32_t maxsize);

        // Align bitstream position to byte boundary
        inline void AlignPointerRight(void);

        // Read N bits from bitstream array
        inline uint32_t GetBits(uint32_t nbits);

        // Read N bits from bitstream array
        template <uint32_t nbits>
        inline uint32_t GetPredefinedBits();

        // Read variable length coded unsigned element
        uint32_t GetVLCElementU();

        // Read variable length coded signed element
        int32_t GetVLCElementS();

        // Reads one bit from the buffer.
        uint8_t Get1Bit();

        // Check if bitstream position has moved outside the limit
        bool CheckBSLeft();

        // Check whether more data is present
        bool More_RBSP_Data() const;

        // Return number of decoded bytes since last reset
        size_t BytesDecoded() const;

        // Return number of decoded bits since last reset
        size_t BitsDecoded() const;

        // Return number of bytes left in bitstream array
        size_t BytesLeft() const;

        // Return number of bits needed for byte alignment
        unsigned GetNumBitsUntilByteAligned() const;

        // Align bitstream to byte boundary
        void ReadOutTrailingBits();

        // Return current bitstream buffer pointer
        const uint8_t *GetRawDataPtr() const    {
            return (const uint8_t *)m_pbs + (31 - m_bitOffset)/8;
        }

        // Return bitstream array base address and size
        void GetOrg(uint32_t **pbs, uint32_t *size) const;
        // Return current bitstream address and bit offset
        void GetState(uint32_t **pbs, uint32_t *bitOffset);
        // Set current bitstream address and bit offset
        void SetState(uint32_t *pbs, uint32_t bitOffset);

        // Set current decoding position
        void SetDecodedBytes(size_t);

        size_t GetAllBitsCount()
        {
            return m_maxBsSize;
        }

        size_t BytesDecodedRoundOff()
        {
            return static_cast<size_t>((uint8_t*)m_pbs - (uint8_t*)m_pbsBase);
        }

        inline uint32_t CeilLog2(uint32_t x) { uint32_t l = 0; while (x > (1U << l)) l++; return l; }
        inline uint32_t FloorLog2(uint32_t x) { uint32_t l = 0; while (x >= (1U << (l + 1))) l++; return l; }

    protected:

        uint32_t *m_pbs;                             // pointer to the current position of the buffer.
        int32_t   m_bitOffset;                       // the bit position (0 to 31) in the dword pointed by m_pbs.
        uint32_t *m_pbsBase;                         // pointer to the first byte of the buffer.
        uint32_t  m_maxBsSize;                       // maximum buffer size in bytes.
    };

    class VVCSlice;

    // Bitstream headers parsing class
    class VVCHeadersBitstream : public VVCBitstreamUtils
    {
    public:

        VVCHeadersBitstream();
        VVCHeadersBitstream(uint8_t *const pb, const uint32_t maxsize);

        // Read and return NAL unit type and NAL storage idc.
        // Bitstream position is expected to be at the start of a NAL unit.
        UMC::Status GetNALUnitType(NalUnitType &nal_unit_type, uint32_t &nuh_temporal_id, uint32_t &nuh_layer_id);

        // Read optional access unit delimiter from bitstream.
        UMC::Status GetAUDelimiter(uint32_t &PicCodType);

        // Parse SEI message
        int32_t ParseSEI(std::shared_ptr<VVCSeqParamSet> &sps, int32_t current_sps, VVCSEIPayLoad *spl);

        // Parse picHeader PPS ID to slice header
        UMC::Status GetSlicePicParamSetNumber(VVCSliceHeader* sliceHdr, VVCPicHeader* picHeader);

        // Parse slice header
        UMC::Status GetSliceHeader(VVCSlice* slice, VVCPicHeader* picHeader, ParameterSetManager* m_currHeader, 
                                   PocDecoding* pocDecoding, const int prevPicPOC);

        // Get and parse APS header
        UMC::Status GetAdaptionParamSet(VVCAPS* aps);

        // Parse adaptive loop filter data 
        void ParseAlfAps(VVCAPS* aps);

        // Parse luma mapping with chroma scaling data 
        void ParseLMCS(VVCAPS* aps);
        
        // Parse scaling list data block
        void ParseScalingList(ScalingList* scalingList, VVCAPS* aps);

        // Reserved for future header extensions
        bool MoreRbspData();

        // Get VPS header
        UMC::Status GetVideoParamSet(VVCVideoParamSet *vps);

        // Get SPS header
        UMC::Status GetSequenceParamSet(VVCSeqParamSet *sps);

        // Parse PPS header
        UMC::Status GetPictureParamSet(VVCPicParamSet *pPps);

        // Parse picture header
        UMC::Status GetPictureHeader(VVCPicHeader *ph, ParameterSetManager* m_currHeader, bool readRbspTrailingBits);

        UMC::Status GetOperatingPointInformation(VVCOPI *opi);

        UMC::Status GetWPPTileInfo(VVCSliceHeader *hdr, const VVCPicParamSet *pps, const VVCSeqParamSet *sps);

        // ...

        void ParseShortTermRefPicSet(const VVCSeqParamSet* sps, ReferencePictureSet* pRPS, uint32_t idx);

    protected:

        // Parse video usability information block in SPS
        void xParseVUI(VVCSeqParamSet *sps);
        // Parse weighted prediction table in slice header
        void xParsePredWeightTable(const VVCSeqParamSet *sps, VVCSliceHeader * sliceHdr);
        // Parse scaling list data block
        void xDecodeScalingList(ScalingList *scalingList, unsigned sizeId, unsigned listId);
        // Parse HRD information in VPS or in VUI block of SPS
        void xParseHrdParameters(VVCHRD *hrd, uint8_t commonInfPresentFlag, uint32_t vps_max_sub_layers);
        // Parse ALF filter
        void alfFilter(VVCAPS *aps, const bool isChroma, const int altIdx);
        // Parse Scaling List data
        void decodeScalingList(ScalingList* scalingList,
                               uint32_t scalingListId,
                               uint32_t matrixSize,
                               bool isPredictor);
        void updateScalingList(ScalingList* scalingList,
                               uint32_t matrixSize,
                               uint32_t scalingListId);
        // Destroy slice map and rect slice in pps
        void xDestroyTileSliceInfo(VVCPicParamSet* pps);
        // Reset tile and slice parameters and lists
        void xResetTileSliceInfo(VVCPicParamSet* pps);
        // Initialize mapping between rectangular slices and CTUs
        void initSliceMap(SliceMap* sliceMap);
        void xInitRectSliceMap(VVCPicParamSet* pps, const VVCSeqParamSet* sps);
        // Initialize tile row/column sizes and boundaries
        void xInitTiles(VVCPicParamSet* pPps);
        //initialize mapping between subpicture and CTUs
        void xInitSubPic(const VVCSeqParamSet* sps, VVCPicParamSet* pps);
        // Initialize vps struct with default value
        void xInitializeVps(VVCVideoParamSet *pVps);
        // Parse general constraints info in profile tier level parsing
        void xParseGeneralConstraintsInfo(VVCConstraintInfo *general_constraints_info);
        // Parse profile tier layers header in VPS or SPS
        void xParseProfileTierLevel(VVCProfileTierLevel *profileTierLevel, bool profileTierPresentFlag, uint32_t maxNumSubLayersMinus1);
        // Parse dpb parameters in VPS or SPS
        void xParseDpbParameters(VVCDpbParameter *dpb_parameter, uint32_t MaxSubLayersMinus1, bool subLayerInfoFlag);
        // Parse general timing hrd parameters
        void xParseGeneralTimingHrdParameters(VVCGeneralTimingHrdParams *general_timing_hrd_parameters);
        // Parse ols timing hrd parameters
        void xParseOlsTimingHrdParameters(VVCGeneralTimingHrdParams *general_timing_hrd_parameters,
                                          VVCOlsTimingHrdParams *ols_timing_hrd_parameters,
                                          uint32_t firstSubLayer,
                                          uint32_t vps_hrd_max_tid);
        void xDeriveOutputLayerSets(VVCVideoParamSet *pVps);
        int32_t xDeriveTargetOLSIdx(VVCVideoParamSet* pVps);
        void xDeriveTargetOutputLayerSet(VVCVideoParamSet* pVps, int32_t targetOlsIdx);

        // Parse ref pic list struct
        void xParseRefPicListStruct(const VVCSeqParamSet *pSps, VVCReferencePictureList *pRPL, int32_t rplIdx);
        // Set reference picture list
        void xSetRefPicIdentifier(VVCReferencePictureList *pRPL,
                                  uint32_t idx,
                                  uint32_t identifier,
                                  bool isLongterm,
                                  bool isInterLayerRefPic,
                                  uint32_t interLayerIdx);
        // Copy reference picture list
        void xCopyRefPicListStruct(VVCSeqParamSet *pSps,
                                   VVCReferencePictureList *pSrcRPL,
                                   VVCReferencePictureList *pDestRPL);
        // Parse VUI struct
        void xParseVUI(VVCSeqParamSet *pSps, VVCVUI *pVUI);
        // Parse ref pic list in picture Header
        //void xParseRefPicLists(VVCPicHeader *picHeader, VVCSliceHeader* slcHeader, const VVCSeqParamSet *pSps, const VVCPicParamSet *pPps);
        void xParseRefPicLists(VVCPicHeader* picHeader, const VVCSeqParamSet* pSps, const VVCPicParamSet* pPps, int32_t rplIdx);
        // Parse explicit wp tables
        void xParsePredWeightTable(VVCPicHeader *picHeader, const VVCSeqParamSet *pSps, const VVCPicParamSet *pPps);

        // Decoding SEI message functions
        int32_t sei_message(const VVCSeqParamSet & sps,int32_t current_sps, VVCSEIPayLoad *spl);
        // Parse SEI payload data
        int32_t sei_payload(const VVCSeqParamSet & sps, int32_t current_sps, VVCSEIPayLoad *spl);
        // Parse pic timing SEI data
        int32_t pic_timing(const VVCSeqParamSet & sps, int32_t current_sps, VVCSEIPayLoad *spl);
        // ...
    };
}

#endif // __UMC_VVC_BITSTREAM_HEADERS_H_

#endif // MFX_ENABLE_VVC_VIDEO_DECODE
