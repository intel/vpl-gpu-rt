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

#ifndef __UMC_VVC_SLICE_DECODING_H
#define __UMC_VVC_SLICE_DECODING_H

#include "umc_media_data.h"
#include "umc_vvc_dec_defs.h"
#include "umc_vvc_bitstream_headers.h"
#include "umc_vvc_headers_manager.h"
#include "umc_vvc_frame.h"

namespace UMC_VVC_DECODER
{
    class VVCDecoderFrame;

    // Slice descriptor class
    class VVCSlice : public RefCounter
    {
    public:

        VVCSlice();
        virtual ~VVCSlice(void);

        virtual void                    Reset();                                            // Reset slice structure to default values
        virtual void                    Release();                                          // Release resources
        virtual bool                    Reset(ParameterSetManager* m_currHeader, PocDecoding* pocDecoding, const int prevPicPOC);    // Decode slice header and initializ slice structure with parsed values
        bool                            DecodeSliceHeader(ParameterSetManager* m_currHeader, PocDecoding* pocDecoding, const int prevPicPOC);    // Decoder slice header and calculate POC
        VVCHeadersBitstream             *GetBitStream() { return &m_bitStream; }             // Get bit stream object
        VVCHeadersBitstream const       *GetBitStream() const { return &m_bitStream; }
        VVCPicHeader const              *GetPictureHeader() const { return &m_picHeader; }
        VVCPicHeader                    *GetPictureHeader() { return &m_picHeader; }
        void                            SetPH(VVCPicHeader ph) { m_picHeader = ph; }
        VVCPicParamSet const            *GetPPS() const {return m_picParamSet;}
        void                            SetPPS(const VVCPicParamSet* pps);
        VVCSeqParamSet const            *GetSPS(void) {return m_seqParamSet;}
        void                            SetSPS(const VVCSeqParamSet* sps);
        void                            setSeqParamSet(VVCSeqParamSet* sps) { m_seqParamSet = sps; }
        VVCVideoParamSet const          *GetVPS() const { return m_vidParamSet; }
        void                            SetVPS(VVCVideoParamSet* vps) { m_vidParamSet = vps; }
        VVCAPS const                    &GetAPS(uint8_t i) const { return m_adaptParamSet[i]; }
        void                            SetAPS(HeaderSet<VVCAPS> *aps);
        uint32_t                        GetAPSSize() { return (uint32_t)m_adaptParamSet.size(); }
        void                            MarkAPS();
        void                            SetSliceNumber(int32_t iSliceNumber);                 // Set current slice number
        int32_t                         RetrievePPSNumber(VVCPicHeader* picHeader); // Parse beginning of slice header to get PPS ID
        void                            SetSliceNumber(uint32_t iSliceNumber);                 // Set current slice number
        const VVCSliceHeader            *GetSliceHeader() const {return &m_sliceHeader;}      // Get pointer to slice header
        VVCSliceHeader                  *GetSliceHeader() {return &m_sliceHeader;}
        VVCDecoderFrame                 *GetCurrentFrame(void) const {return m_currentFrame;} // Get current destination frame
        void                            SetCurrentFrame(VVCDecoderFrame * pFrame) {m_currentFrame = pFrame;}
        int32_t                         GetSliceNum(void) const {return m_iNumber;}

        void                            InheritFromPicHeader(const VVCPicHeader *picHeader, const VVCPicParamSet *pps, const VVCSeqParamSet *sps);
        int32_t                         GetFirstMB() const {return m_iFirstMB;}              // Get first macroblock
        int32_t                         GetMaxMB(void) const {return m_iMaxMB;}              // Get maximum of macroblock
        void                            SetMaxMB(int32_t x) {m_iMaxMB = x;}
        bool                            IsError() const {return m_bError;}
        bool                            GetRapPicFlag() const;
        void                            InitializeContexts();                                // Initialize CABAC context depending on slice type
        void                            SetDefaultClpRng(const VVCSeqParamSet& sps);
        void                            setCtuAddrInSlice(uint32_t i) { return m_sliceMap.ctu_addr_in_slice.push_back(i); }
        uint32_t                        GetFirstCtuRsAddrInSlice() const { return m_sliceMap.ctu_addr_in_slice[0]; }
        void                            setSliceID(uint32_t id) { m_sliceMap.slice_id = id; }
        uint32_t                        getSliceID() { return m_sliceMap.slice_id; }
        void                            SetNumSubstream(const VVCSeqParamSet* sps, const VVCPicParamSet* pps);
        void                            ResetNumberOfSubstream() { m_numSubstream = 0; }
        void                            SetNumEntryPoints(const VVCSeqParamSet* sps, const VVCPicParamSet* pps);
        uint32_t                        GetNumEntryPoints() const { return m_numEntryPoints; }
        void                            setSliceMap(SliceMap map) { m_sliceMap = map; }
        SliceMap*                       getSliceMap() { return &m_sliceMap; }
        int                             GetNumRpsCurrTempList() const;            // Returns number of used references in RPS
        void                            SetRefPOCListSliceHeader();
        void                            CopyFromBaseSlice(const VVCSlice *slice, bool cpyFlag); // For dependent slice copy data from another slice
        uint32_t                        GetTileColumnWidth(uint32_t col) const;
        uint32_t                        GetTileRowHeight(uint32_t row) const;
        uint32_t                        GetTileXIdx() const;
        uint32_t                        GetTileYIdx() const;
        VVCDecoderFrame*                GetRefPic(DPBType dpb, int32_t picPOC, uint32_t layerId);
        VVCDecoderFrame*                GetLongTermRefPic(DPBType dpb, int32_t picPOC, uint32_t bitsForPOC, bool pocHasMsb, uint32_t layerId);
        VVCDecoderFrame*                GetLongTermRefPicCandidate(DPBType dpb,
                                                                   int32_t picPOC, uint32_t bitsForPOC,
                                                                   bool isUseMask, uint32_t layerId) const;
        UMC::Status                     ConstructReferenceList(DPBType dpb/*, VVCDecoderFrame* curr_ref*/);
        UMC::Status                     UpdateRPLMarking(DPBType pDPB);
        int                             CheckAllRefPicsAvail(DPBType dpb, uint8_t rplIdx, int* refPicIndex);
        bool                            checkPOCInfo(DPBType& dpb);        // check the frame info
        void                            ResetUnusedFrames(DPBType dpb);
        uint32_t                        GetMaxSurfNum();
        void                            checkCRA(DPBType dpb, const int pocCRA);

        int32_t                         m_tileCount;
        uint32_t*                       m_tileByteLocation;
        uint32_t                        m_temporal_id;
        int32_t                         m_NumEmuPrevnBytesInSliceHdr;
        bool                            aps_params_type[VVC_NUM_APS_TYPE_LEN];
        uint32_t                        aps_num[VVC_NUM_APS_TYPE_LEN];
        uint32_t                        m_reorderPicNum;
        uint32_t                        m_maxSurfNum;
        int                             m_pocCRA[VVC_MAX_VPS_LAYERS];

        uint32_t getTileLocationCount() const { return m_tileCount; }
        void allocateTileLocation(int32_t val)
        {
            if (m_tileCount < val)
                delete[] m_tileByteLocation;

            m_tileCount = val;
            m_tileByteLocation = new uint32_t[val];
        }

        void initSliceMap()
        {
            m_sliceMap.slice_id = 0;
            m_sliceMap.num_ctu_in_slice  = 0;
            m_sliceMap.num_tiles_in_slice = 0;
            m_sliceMap.ctu_addr_in_slice.clear();
        }

        MemoryPiece                     m_source;                          // slice bitstream data
        VVCSliceHeader                  m_sliceHeader;                     // slice header
        std::vector<VVCRectSlice*>      m_rectSlice;

    private:
        VVCDecoderFrame                 *m_currentFrame;
        int32_t                         m_numEmuPrevnBytesInSliceHdr;      // number of emulation prevention bytes in slice header
        uint32_t                        m_iNumber;                         // current slice number
        int32_t                         m_iFirstMB;                        // first MB number in slice
        int32_t                         m_iMaxMB;                          // last unavailable  MB number in slice
        bool                            m_bError;                          // if there is an error in decoding
        VVCHeadersBitstream             m_bitStream;
        const VVCPicParamSet            *m_picParamSet;
        const VVCSeqParamSet            *m_seqParamSet;
        const VVCVideoParamSet          *m_vidParamSet;
        std::vector<VVCAPS>             m_adaptParamSet;
        VVCPicHeader                    m_picHeader;
        VVCReferencePictureList         m_rpl[VVC_MAX_NUM_REF_PIC_LISTS];       // RPL for L0 when present in slice header
        int                             m_rplIdx[VVC_MAX_NUM_REF_PIC_LISTS];    // index of used RPL in the SPS or -1 for local RPL in the slice header
        uint32_t                        m_numSubstream;
        uint32_t                        m_numEntryPoints;
        NalUnitType                     m_nal_unit_type;

        ClpRngs                         m_clpRngs;
        SliceMap                        m_sliceMap;
    };

    // Check whether two slices are from the same picture. VVC spec
    inline bool SlicesInSamePic(const VVCSlice *pSliceOne, VVCSlice *pSliceTwo)
    {
        if (!pSliceOne)
            return true;

        const VVCSliceHeader *pOne = pSliceOne->GetSliceHeader();
        const VVCSliceHeader *pTwo = pSliceTwo->GetSliceHeader();

        if (pOne->m_poc != pTwo->m_poc || pTwo->sh_picture_header_in_slice_header_flag)
        {
            return false;
        }

        return true;
    }

} // namespace UMC_VVC_DECODER

#endif // __UMC_VVC_SLICE_DECODING_H
#endif // MFX_ENABLE_VVC_VIDEO_DECODE
