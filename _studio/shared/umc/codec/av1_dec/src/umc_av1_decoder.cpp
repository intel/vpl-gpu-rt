// Copyright (c) 2017-2024 Intel Corporation
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
#ifdef MFX_ENABLE_AV1_VIDEO_DECODE

#include "umc_structures.h"
#include "umc_frame_data.h"

#include "umc_av1_decoder.h"
#include "umc_av1_utils.h"
#include "umc_av1_bitstream.h"
#include "umc_av1_frame.h"

#include <algorithm>
#include "mfx_umc_alloc_wrapper.h"

#include "libmfx_core_vaapi.h"

#include "mfx_unified_av1d_logging.h"

namespace UMC_AV1_DECODER
{
    AV1Decoder::AV1Decoder()
        : allocator(nullptr)
        , sequence_header(nullptr)
        , old_seqHdr(nullptr)
        , counter(0)
        , Curr(nullptr)
        , Curr_temp(nullptr)
        , Repeat_show(0)
        , PreFrame_id(0)
        , OldPreFrame_id(0)
        , frame_order(0)
        , in_framerate(0)
        , repeateFrame(UMC::FRAME_MID_INVALID)
        , anchor_frames_count(0)
        , tile_list_idx(0)
        , frames_to_skip(0)
        , saved_clip_info_width(0)
        , saved_clip_info_height(0)
        , clip_info_size_saved(false)
        , sequence_header_ready(false)
        , m_prev_frame_header_exist(false)
        , m_specified_anchor_Idx(0)
        , m_isAnchor(false)
        , m_RecreateSurfaceFlag(false)
        , m_drcFrameWidth(0)
        , m_drcFrameHeight(0)
        , m_pCore(nullptr)
    {
        outputed_frames.clear();
        m_prev_frame_header = {};
    }

    AV1Decoder::~AV1Decoder()
    {
        std::for_each(std::begin(dpb), std::end(dpb),
            std::default_delete<AV1DecoderFrame>()
        );
        outputed_frames.clear();

        // release unique_ptr resource
        sequence_header.reset();
        old_seqHdr.reset();
    }

    inline bool CheckOBUType(AV1_OBU_TYPE type)
    {
        switch (type)
        {
        case OBU_TEMPORAL_DELIMITER:
        case OBU_SEQUENCE_HEADER:
        case OBU_FRAME_HEADER:
        case OBU_REDUNDANT_FRAME_HEADER:
        case OBU_FRAME:
        case OBU_TILE_GROUP:
        case OBU_TILE_LIST:
        case OBU_METADATA:
        case OBU_PADDING:
            return true;
        default:
            return false;
        }
    }

    inline uint32_t MapLevel(uint32_t levelIdx)
    {
        if (levelIdx <= 31)
            return (2 + (levelIdx >> 2)) * 10 + (levelIdx & 3);
        else
            return MFX_LEVEL_UNKNOWN;
    }

    static bool IsNeedSPSInvalidate(SequenceHeader *old_sps, const SequenceHeader *new_sps)
    {
        if (!old_sps || !new_sps)
        {
            return false;
        }
        if (old_sps->max_frame_width == 0 || old_sps->max_frame_height == 0)
        {
            old_sps->max_frame_width = new_sps->max_frame_width;
            old_sps->max_frame_height = new_sps->max_frame_height;
            old_sps->seq_profile = new_sps->seq_profile;
            old_sps->film_grain_param_present = new_sps->film_grain_param_present;
            return false;
        }

        if ((old_sps->max_frame_width != new_sps->max_frame_width) ||
            (old_sps->max_frame_height != new_sps->max_frame_height))
        {
            old_sps->max_frame_width = new_sps->max_frame_width;
            old_sps->max_frame_height = new_sps->max_frame_height;
            return true;
        }

        if (old_sps->seq_profile != new_sps->seq_profile)
        {
            old_sps->seq_profile = new_sps->seq_profile;
            return true;
        }

        if (old_sps->film_grain_param_present != new_sps->film_grain_param_present)
        {
            old_sps->film_grain_param_present = new_sps->film_grain_param_present;
            return true;
        }

        return false;
    }

    static bool IsNeedRecreateSurface(SequenceHeader* old_sps, const SequenceHeader* new_sps)
    {
        if (!old_sps || !new_sps)
        {
            return false;
        }

        if ((old_sps->seq_profile != new_sps->seq_profile) ||
            (old_sps->film_grain_param_present != new_sps->film_grain_param_present))
        {
            return true;
        }
        return false;
    }

    UMC::Status AV1Decoder::DecodeHeader(UMC::MediaData* in, UMC_AV1_DECODER::AV1DecoderParams& par)
    {
        if (!in)
            return UMC::UMC_ERR_NULL_PTR;

        SequenceHeader sh = {};

        while (in->GetDataSize() >= MINIMAL_DATA_SIZE)
        {
            try
            {
                const auto src = reinterpret_cast<uint8_t*>(in->GetDataPointer());
                AV1Bitstream bs(src, uint32_t(in->GetDataSize()));

                OBUInfo obuInfo;
                bs.ReadOBUInfo(obuInfo);
                if (obuInfo.header.obu_type > OBU_PADDING)
                    return UMC::UMC_ERR_INVALID_PARAMS;

                if (obuInfo.header.obu_type == OBU_SEQUENCE_HEADER)
                {
                    bs.ReadSequenceHeader(sh);

                    in->MoveDataPointer(static_cast<int32_t>(obuInfo.size));

                    if (FillVideoParam(sh, par) == UMC::UMC_OK)
                        return UMC::UMC_OK;
                }

                if(in->MoveDataPointer(static_cast<int32_t>(obuInfo.size)) != UMC::UMC_OK)
                    return UMC::UMC_ERR_NOT_ENOUGH_DATA;

            }
            catch (av1_exception const& e)
            {
                return e.GetStatus();
            }
        }

        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    UMC::Status AV1Decoder::Init(UMC::BaseCodecParams* vp)
    {
        if (!vp)
            return UMC::UMC_ERR_NULL_PTR;

        AV1DecoderParams* dp =
            DynamicCast<AV1DecoderParams, UMC::BaseCodecParams>(vp);
        if (!dp)
            return UMC::UMC_ERR_INVALID_PARAMS;

        if (!dp->allocator)
            return UMC::UMC_ERR_INVALID_PARAMS;
        allocator = dp->allocator;

        if (dp->anchors_loaded)
        {
            // Parameter for clips started with anchor frames (Vicue)
            // and anchor loaded from secondary source
            frames_to_skip = dp->skip_first_frames;
        }

        params = *dp;
        frame_order = 0;
        return SetParams(vp);
    }

    UMC::Status AV1Decoder::Update_drc(SequenceHeader* seq)
    {
        if (!seq)
            return UMC::UMC_ERR_NULL_PTR;

        params.info.clip_info.height = seq->max_frame_height;
        params.info.clip_info.width = seq->max_frame_width;
        params.info.disp_clip_info = params.info.clip_info;
        m_ClipInfo = params.info;
        return UMC::UMC_OK;
    }

    UMC::Status AV1Decoder::GetInfo(UMC::BaseCodecParams* info)
    {
        AV1DecoderParams* vp =
            DynamicCast<AV1DecoderParams, UMC::BaseCodecParams>(info);

        if (!vp)
            return UMC::UMC_ERR_INVALID_PARAMS;

        *vp = params;
        return UMC::UMC_OK;
    }

    DPBType AV1Decoder::DPBUpdate(AV1DecoderFrame * prevFrame)
    {
        assert(prevFrame);

        std::unique_lock<std::mutex> l(guard);
        DPBType updatedFrameDPB;

        DPBType const& prevFrameDPB = prevFrame->frame_dpb;
        if (prevFrameDPB.empty())
            updatedFrameDPB.resize(NUM_REF_FRAMES);
        else
            updatedFrameDPB = prevFrameDPB;

        const FrameHeader& fh = prevFrame->GetFrameHeader();

        prevFrame->DpbUpdated(true);

        if (fh.refresh_frame_flags == 0)
            return updatedFrameDPB;

        for (uint8_t i = 0; i < NUM_REF_FRAMES; i++)
        {
            if ((fh.refresh_frame_flags >> i) & 1)
            {
                if (!prevFrameDPB.empty() && prevFrameDPB[i] && prevFrameDPB[i]->UID != -1)
                    prevFrameDPB[i]->DecrementReference();

                updatedFrameDPB[i] = const_cast<AV1DecoderFrame*>(prevFrame);
                prevFrame->IncrementReference();
            }
        }

        return updatedFrameDPB;
    }

    static void GetTileLocation(
        AV1Bitstream* bs,
        FrameHeader const& fh,
        TileGroupInfo const& tgInfo,
        uint32_t idxInTG,
        size_t OBUSize,
        size_t OBUOffset,
        TileLocation& loc)
    {
        // calculate tile row and column
        const uint32_t idxInFrame = tgInfo.startTileIdx + idxInTG;
        loc.row = idxInFrame / fh.tile_info.TileCols;
        loc.col = idxInFrame - loc.row * fh.tile_info.TileCols;
        loc.anchorFrameIdx = AV1_INVALID_IDX;
        loc.tileIdxInTileList = AV1_INVALID_IDX;

        size_t tileOffsetInTG = bs->BytesDecoded();

        if (idxInTG == tgInfo.numTiles - 1)
            loc.size = OBUSize - tileOffsetInTG;  // tile is last in tile group - no explicit size signaling
        else
        {
            tileOffsetInTG += fh.tile_info.TileSizeBytes;

            // read tile size
            size_t reportedSize = 0;
            size_t actualSize = 0;
            bs->ReadTile(fh.tile_info.TileSizeBytes, reportedSize, actualSize);
            if (actualSize != reportedSize)
            {
                // before parsing tiles we check that tile_group_obu() is complete (bitstream has enough bytes to hold whole OBU)
                // but here we encountered incomplete tile inside this tile_group_obu() which means tile size corruption
                // [maybe] later check for complete tile_group_obu() will be removed, and thus incomplete tile will be possible
                assert("Tile size corruption: Incomplete tile encountered inside complete tile_group_obu()!");
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
            }

            loc.size = reportedSize;
        }

        loc.offset = OBUOffset + tileOffsetInTG;
        loc.tile_location_type = 0;

    }

    static void GetTileListEntry(
        AV1Bitstream& bs,
        TileListInfo &tlInfo,
        uint32_t idxInTL,
        size_t OBUOffset,
        uint32_t shift,
        size_t &OBUSize,
        TileLocation& loc)
    {
        // calculate tile row and column
        const uint32_t idxInFrame = idxInTL;

        loc.shift = shift;

        loc.row = idxInFrame / tlInfo.frameWidthInTiles;
        loc.col = idxInFrame - loc.row * tlInfo.frameWidthInTiles;

        loc.tileIdxInTileList = idxInTL;

        size_t tileOffsetInTL = bs.BytesDecoded();
        bs.ReadTileListEntry(tlInfo, loc);

        OBUSize = loc.size + OBU_TILE_LIST_ENTRY_HEDAER_LENGTH;
        loc.offset = OBUOffset + OBU_TILE_LIST_ENTRY_HEDAER_LENGTH;

        tileOffsetInTL += loc.size;

        size_t actualSize = 0;
        bs.ReadTileListEntryData(loc.size, actualSize);

        if (loc.size != actualSize)
        {
            throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
    }

    inline bool CheckTileGroup(uint32_t prevNumTiles, FrameHeader const& fh, TileGroupInfo const& tgInfo)
    {
        if (prevNumTiles + tgInfo.numTiles > NumTiles(fh.tile_info))
            return false;

        if (tgInfo.numTiles == 0 || tgInfo.startTileIdx > tgInfo.endTileIdx)
            return false;

        return true;
    }

    AV1DecoderFrame* AV1Decoder::StartFrame(FrameHeader const& fh, DPBType & frameDPB, AV1DecoderFrame* pPrevFrame)
    {
        AV1DecoderFrame* pFrame = nullptr;

        if (fh.show_existing_frame)
        {
            std::unique_lock<std::mutex> l(guard);
            pFrame = frameDPB[fh.frame_to_show_map_idx];

            if (pFrame->Repeated())
                return nullptr;

            //Increase referernce here, and will be decreased when
            //CompleteDecodedFrames not show_frame case.
            pFrame->IncrementReference();
            assert(pFrame);
            repeateFrame = pFrame->GetMemID();

            //Add one more Reference, and add it into outputed frame list
            //When QueryFrame finished and update status in outputed frame
            //list, then it will be released in CompleteDecodedFrames.
            pFrame->IncrementReference();
            pFrame->Repeated(true);
            outputed_frames.push_back(pFrame);

            FrameHeader const& refFH = pFrame->GetFrameHeader();

            if (!refFH.showable_frame)
                throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);

            FrameHeader const& Repeat_H = pFrame->GetFrameHeader();
            if (Repeat_H.frame_type == KEY_FRAME)
            {
                for (uint8_t i = 0; i < NUM_REF_FRAMES; i++)
                {
                    if ((Repeat_H.refresh_frame_flags >> i) & 1)
                    {
                        if (!frameDPB.empty() && frameDPB[i] && frameDPB[i]->GetRefCounter())
                           frameDPB[i]->DecrementReference();

                        frameDPB[i] = const_cast<AV1DecoderFrame*>(pFrame);
                        pFrame->IncrementReference();
                    }
                }
            }
            refs_temp = frameDPB;
            if (pPrevFrame)
            {
                DPBType & prevFrameDPB = pPrevFrame->frame_dpb;
                prevFrameDPB = frameDPB;
            }

            return pFrame;
        }
        else
	{
            AV1DecoderFrame *pExFrame = nullptr;
	    if (fh.refresh_frame_flags != 0)
	    {
                for (uint8_t i = 0; i < NUM_REF_FRAMES; i++)
                {
                    if ((fh.refresh_frame_flags >> i) & 1)
                    {
                        pExFrame = frameDPB[i];
		        break;
		    }
	        }
	    }
            pFrame = GetFrameBuffer(fh, pExFrame);
	}

        if (!pFrame)
            return nullptr;

        pFrame->SetSeqHeader(*sequence_header.get());

        if (fh.refresh_frame_flags)
            pFrame->SetRefValid(true);

        pFrame->frame_dpb = frameDPB;
        pFrame->UpdateReferenceList();

        if (!params.film_grain)
            pFrame->DisableFilmGrain();

        return pFrame;
    }

    AV1DecoderFrame* AV1Decoder::StartAnchorFrame(FrameHeader const& fh, DPBType const& frameDPB, uint32_t idx)
    {
        AV1DecoderFrame* pFrame = nullptr;

        pFrame = GetFrameBufferByIdx(fh, idx);

        if (!pFrame)
            return nullptr;

        pFrame->SetSeqHeader(*sequence_header.get());

        if (fh.refresh_frame_flags)
            pFrame->SetRefValid(true);

        pFrame->frame_dpb = frameDPB;
        pFrame->UpdateReferenceList();

        m_anchor_frame_mem_ids[idx] = pFrame->m_index;

        return pFrame;
    }

    static void ReadTileGroup(TileLayout& layout, AV1Bitstream& bs, FrameHeader const& fh, size_t obuOffset, size_t obuSize)
    {
        TileGroupInfo tgInfo = {};
        bs.ReadTileGroupHeader(fh, tgInfo);

        if (!CheckTileGroup(static_cast<uint32_t>(layout.size()), fh, tgInfo))
            throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);

        uint32_t idxInLayout = static_cast<uint32_t>(layout.size());

        layout.resize(layout.size() + tgInfo.numTiles,
            { tgInfo.startTileIdx, tgInfo.endTileIdx });

        for (int idxInTG = 0; idxInLayout < layout.size(); idxInLayout++, idxInTG++)
        {
            TileLocation& loc = layout[idxInLayout];
            GetTileLocation(&bs, fh, tgInfo, idxInTG, obuSize, obuOffset, loc);
        }
    }

    static uint32_t ReadTileListHeader(AV1Bitstream& bs, FrameHeader const& fh, TileListInfo &tlInfo)
    {
        bs.ReadTileListHeader(fh, tlInfo);
        return tlInfo.numTiles;
    }

    static uint32_t ReadTileList(TileLayout& layout, AV1Bitstream& bs, FrameHeader const& /*fh*/, size_t /*obuOffset*/, TileListInfo tlInfo, uint32_t shift)
    {
        uint32_t idxInLayout = static_cast<uint32_t>(layout.size());

        layout.resize(layout.size() + tlInfo.numTiles);

        size_t TileListEntryOffset = OBU_TILE_LIST_HEADER_LENGTH;
        for (uint32_t idxInTL = 0; idxInLayout < layout.size() && idxInTL < tlInfo.numTiles; idxInLayout++, idxInTL++)
        {
            size_t obuSize;
            TileLocation& loc = layout[idxInLayout];
            GetTileListEntry(bs, tlInfo, idxInTL, TileListEntryOffset, shift, obuSize, loc);
            TileListEntryOffset += obuSize;
        }

        return tlInfo.numTiles;
    }

    inline bool NextFrameDetected(AV1_OBU_TYPE obuType)
    {
        switch (obuType)
        {
        case OBU_REDUNDANT_FRAME_HEADER:
        case OBU_TILE_GROUP:
        case OBU_TILE_LIST:
        case OBU_PADDING:
        case OBU_SEQUENCE_HEADER:
        case OBU_METADATA:
            return false;
        default:
            return true;
        }
    }

    inline bool GotFullFrame(AV1DecoderFrame const* curr_frame, FrameHeader const& fh, TileLayout const& layout)
    {
        const unsigned numMissingTiles = curr_frame ? GetNumMissingTiles(*curr_frame) : NumTiles(fh.tile_info);
        if (layout.size() == numMissingTiles) // current tile group_obu() contains all missing tiles
            return true;
        else
            return false;
    }

    inline bool HaveTilesToSubmit(AV1DecoderFrame const* curr_frame, TileLayout const& layout)
    {
        if (!layout.empty() ||
            (curr_frame && BytesReadyForSubmission(curr_frame->GetTileSets())))
            return true;

        return false;
    }

    inline bool AllocComplete(AV1DecoderFrame const& frame)
    {
        return frame.GetFrameData(SURFACE_DISPLAY) &&
            frame.GetFrameData(SURFACE_RECON);
    }

    inline bool FrameInProgress(AV1DecoderFrame const& frame)
    {
        // frame preparation to decoding is in progress
        // i.e. SDK decoder still getting tiles of the frame from application (has both arrived and missing tiles)
        //      or it got all tiles and waits for output surface to start decoding
        if (GetNumArrivedTiles(frame) == 0 || frame.UID == -1)
            return false;

        return GetNumMissingTiles(frame) || !AllocComplete(frame);
    }

    inline void AV1Decoder::CalcFrameTime(AV1DecoderFrame* frame)
    {
        if (!frame)
            return;

        frame->SetFrameTime(frame_order * in_framerate);
        frame->SetFrameOrder(frame_order);
        FrameHeader strFrameHeader = frame->GetFrameHeader();
        if (strFrameHeader.show_frame || strFrameHeader.show_existing_frame)//add for fix -DecoderdOrder hang issue
            frame_order++;
    }

    UMC::Status AV1Decoder::GetFrame(UMC::MediaData* in, UMC::MediaData*)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);
        if (!in)
            return UMC::UMC_ERR_NULL_PTR;

        FrameHeader fh = {};

        AV1DecoderFrame* pPrevFrame = FindFrameByUID(counter - 1);
        AV1DecoderFrame* pFrameInProgress = FindFrameInProgress();
        DPBType updated_refs;
        UMC::MediaData tmper = *in;
        repeateFrame = UMC::FRAME_MID_INVALID;

        if ((tmper.GetDataSize() >= MINIMAL_DATA_SIZE) && pPrevFrame && !pFrameInProgress)
        {
            if (!Repeat_show)
            {
                if (!pPrevFrame->DpbUpdated())
                {
                    updated_refs = DPBUpdate(pPrevFrame);
                    refs_temp = updated_refs;
                }
                else
                {
                    updated_refs = refs_temp;
                }

            }
            else
            {
                DPBType const& prevFrameDPB = pPrevFrame->frame_dpb;
                updated_refs = prevFrameDPB;
                Repeat_show = 0;
            }
            Curr_temp = Curr;
        }
        if (!pPrevFrame)
        {
            updated_refs = refs_temp;
            Curr_temp = Curr;
            Repeat_show = 0;
        }

        if ((!updated_refs.empty())&& (updated_refs[0] != nullptr))
        {
            TRACE_BUFFER_EVENT(MFX_TRACE_API_AV1_DPBPARAMETER_TASK, EVENT_TYPE_INFO, TR_KEY_DECODE_DPB_INFO,
                updated_refs, AV1DecodeDpbInfo, DPBINFO_AV1D);
        }

        bool gotFullFrame = false;
        bool repeatedFrame = false;
        bool skipFrame = false;
        bool anchor_decode = params.lst_mode && m_isAnchor;
        bool firstSubmission = false;
 
        AV1DecoderFrame* pCurrFrame = nullptr;

        if (pFrameInProgress && GetNumMissingTiles(*pFrameInProgress) == 0)
        {
            /* this code is executed if and only if whole frame (all tiles) was already got from applicaiton during previous calls of DecodeFrameAsync
               but there were no sufficient surfaces to start decoding (e.g. to apply film_grain)
               in this case reading from bitstream must be skipped, and code should proceed to frame submission to the driver */

            assert(!AllocComplete(*pFrameInProgress));
            pCurrFrame = pFrameInProgress;
            gotFullFrame = true;
            clean_seq_header_ready();
        }
        else
        {
            /*do bitstream parsing*/

            bool gotFrameHeader = false;
            uint32_t OBUOffset = 0;
            TileLayout layout;

            UMC::MediaData tmp = *in; // use local copy of [in] for OBU header parsing to not move data pointer in original [in] prematurely
            OldPreFrame_id = PreFrame_id;

            while (tmp.GetDataSize() >= MINIMAL_DATA_SIZE && gotFullFrame == false && repeatedFrame == false)
            {
                const auto src = reinterpret_cast<uint8_t*>(tmp.GetDataPointer());
                AV1Bitstream bs(src, uint32_t(tmp.GetDataSize()));

                OBUInfo obuInfo;
                TileListInfo tlInfo = {};
                uint32_t lst_shift;
                bs.ReadOBUInfo(obuInfo);
                const AV1_OBU_TYPE obuType = obuInfo.header.obu_type;
                auto frame_source = dynamic_cast<SurfaceSource*>(allocator);

                if (obuInfo.header.obu_type > OBU_PADDING)
                    return UMC::UMC_ERR_INVALID_PARAMS;

                if (tmp.GetDataSize() < obuInfo.size) // not enough data left in the buffer to hold full OBU unit
                    break;

                if (pFrameInProgress && NextFrameDetected(obuType))
                {
                    assert(!"Current frame was interrupted unexpectedly!");
                    throw av1_exception(UMC::UMC_ERR_INVALID_STREAM);
                    // [robust] add support for cases when series of tile_group_obu() interrupted by other OBU type before end of frame was reached
                }

                switch (obuType)
                {
                case OBU_SEQUENCE_HEADER:
                {   if (!sequence_header.get())
                        sequence_header.reset(new SequenceHeader);
                    *sequence_header = SequenceHeader{};
                    bs.ReadSequenceHeader(*sequence_header);

                    if (!old_seqHdr)
                    {
                        old_seqHdr.reset(new SequenceHeader());
                        Update_drc(sequence_header.get());
                    }

                    // check if sequence header has been changed
                    if (IsNeedSPSInvalidate(old_seqHdr.get(), sequence_header.get()))
                    {
                        // According to Spec Section 7.5 the contents of sequence_header_obu must be 
                        // bit-identical each time the sequence header
                        // appears except for the contents of operating_parameters_info
                        if (is_seq_header_ready())
                        {
                            MFX_LTRACE_MSG(MFX_TRACE_LEVEL_CRITICAL_INFO, "Multi sequence_header_obu not bit-identical!");
                            return UMC::UMC_ERR_INVALID_PARAMS;
                        }

                        Update_drc(sequence_header.get());
                        m_RecreateSurfaceFlag = IsNeedRecreateSurface(old_seqHdr.get(), sequence_header.get());

                        if ((frame_source && !frame_source->GetSurfaceType()) || (frame_source->GetSurfaceType() && m_RecreateSurfaceFlag))
                        {
                            // new resolution required
                            return UMC::UMC_NTF_NEW_RESOLUTION;
                        }
                    }

                    set_seq_header_ready();
                    break;
                }
                case OBU_TILE_LIST:
                    // a ext-tile sequence composition: a sequence header OBU, followed by
                    // a number of OBUs that together constitue one or more coded anchors frames, followed by
                    // a frame header OBU, followed by
                    // a set of one or more tile list OBUs
                    // so for multi tile lists case, need get saved frame header since have only one single fh
                    // before tile list OBUs.
                    if (tile_list_idx > 0 && m_prev_frame_header_exist)
                    {
                        fh = m_prev_frame_header;
                    }

                    // different tile lists could have different recon frame size, so need save original clip size,
                    // then use it as a condition for surfaces reallocation of next tile lists.
                    if (!clip_info_size_saved)
                    {
                        saved_clip_info_width = params.info.clip_info.width;
                        saved_clip_info_height= params.info.clip_info.height;
                        clip_info_size_saved = true;
                    }

                    lst_shift = (uint32_t)bs.BytesDecoded();
                    ReadTileListHeader(bs, fh, tlInfo);

                    if ((params.info.clip_info.width != (int32_t)(tlInfo.frameWidthInTiles * fh.tile_info.TileWidth * MI_SIZE) ||
                        params.info.clip_info.height != (int32_t)(tlInfo.frameHeightInTiles * fh.tile_info.TileHeight * MI_SIZE)))
                    {
                        params.info.clip_info.width = (int32_t)(tlInfo.frameWidthInTiles * fh.tile_info.TileWidth * MI_SIZE);
                        params.info.clip_info.height = (int32_t)(tlInfo.frameHeightInTiles * fh.tile_info.TileHeight * MI_SIZE);
                        params.info.disp_clip_info = params.info.clip_info;
                        PreFrame_id = OldPreFrame_id;

                        m_RecreateSurfaceFlag = IsNeedRecreateSurface(old_seqHdr.get(), sequence_header.get());
                        if ((frame_source && !frame_source->GetSurfaceType()) || (frame_source->GetSurfaceType() && m_RecreateSurfaceFlag))
                        {
                            // new resolution required
                            return UMC::UMC_NTF_NEW_RESOLUTION;
                        }
                    }

                    fh.output_frame_width_in_tiles  = tlInfo.frameWidthInTiles;
                    fh.output_frame_height_in_tiles = tlInfo.frameHeightInTiles;
                    fh.tile_count_in_list += ReadTileList(layout, bs, fh, OBUOffset, tlInfo, lst_shift);

                    if (params.lst_mode)
                    {
                        fh.large_scale_tile = true;
                    }

                    in->MoveDataPointer(OBUOffset); // do not submit frame header in data buffer
                    OBUOffset = 0;
                    gotFullFrame = true; // tile list is a complete frame
                    clean_seq_header_ready();
                    tile_list_idx++;
                    break;
                case OBU_FRAME_HEADER:
                case OBU_REDUNDANT_FRAME_HEADER:
                case OBU_FRAME:
                    if (!sequence_header.get())
                        break; // bypass frame header if there is no active seq header
                    if (!gotFrameHeader && !updated_refs.empty())
                    {
                        // we read only first entry of uncompressed header in the frame
                        // each subsequent copy of uncompressed header (i.e. OBU_REDUNDANT_FRAME_HEADER) must be exact copy of first entry by AV1 spec
                        // [robust] maybe need to add check that OBU_REDUNDANT_FRAME_HEADER contains copy of OBU_FRAME_HEADER

                        OldPreFrame_id = PreFrame_id;
                        bs.ReadUncompressedHeader(fh, *sequence_header, updated_refs, obuInfo.header, PreFrame_id);
                        gotFrameHeader = true;
                        m_prev_frame_header_exist = true;
                        m_prev_frame_header = fh;
                    }

                    // got frame in large scale tile mode
                    if (obuType == OBU_FRAME && params.lst_mode)
                    {
                        if (frames_to_skip)
                        {
                            skipFrame = true;
                            gotFrameHeader = false;
                            gotFullFrame = true;
                            frames_to_skip--;
                            fh.is_anchor = 1;
                            clean_seq_header_ready();
                        }
                        else if (anchor_decode)
                        {
                            gotFullFrame = true;
                            fh.is_anchor = 1;
                            clean_seq_header_ready();
                        }
                        else
                        {
                            fh.is_anchor = 0;
                        }
                    }

                    if (obuType != OBU_FRAME)
                    {
                        if (fh.show_existing_frame)
                        {
                            repeatedFrame = true;
                            gotFullFrame = true;
                            clean_seq_header_ready();
                        }
                        break;
                    }
                    bs.ReadByteAlignment();

                    // There are no following tile group
                    if (params.lst_mode && !anchor_decode)
                        break;
                case OBU_TILE_GROUP:
                {
                    FrameHeader const* pFH = nullptr;
                    if (pFrameInProgress)
                        pFH = &(pFrameInProgress->GetFrameHeader());
                    else if (gotFrameHeader)
                        pFH = &fh;

                    if (pFH) // bypass tile group if there is no respective frame header
                    {
                        ReadTileGroup(layout, bs, *pFH, OBUOffset, obuInfo.size);
                        gotFullFrame = GotFullFrame(pFrameInProgress, *pFH, layout);
                        if(gotFullFrame)
                        {
                            clean_seq_header_ready();
                        }
                        break;
                    }
                }
                case OBU_METADATA:
                    bs.ReadMetaData(fh);
                    break;
                default:
                    break;
                }

                OBUOffset += static_cast<uint32_t>(obuInfo.size);
                tmp.MoveDataPointer(static_cast<int32_t>(obuInfo.size));
            }

            // For small decbufsize, cur bitstream may not contain any tile, then deocder will read more data and do header parse again.
            // So we use OldPreFrame_id to update PreFrame_id to avoid the Frame_id check fail in ReadUncompressedHeader.
            if (layout.empty() && !gotFullFrame)
                PreFrame_id = OldPreFrame_id;
            else if (gotFullFrame)
                OldPreFrame_id = PreFrame_id;

            if (!params.lst_mode || !anchor_decode)
            {
                if (!HaveTilesToSubmit(pFrameInProgress, layout) && !repeatedFrame)
                {
                    if (params.lst_mode && !anchor_decode)
                    {
                        PreFrame_id = OldPreFrame_id;
                    }
                    return UMC::UMC_ERR_NOT_ENOUGH_DATA;
                }
            }

            if (!sequence_header.get())
                return UMC::UMC_ERR_NOT_ENOUGH_DATA;

            if (anchor_decode)
            {
                // For anchor frame decoding in ext-tile mode, no need to update DPB since they could be treated as
                // reference surfaces of tile list, but will be not in the reference list if update DPB per frame.
                // Max anchor_frames_count <= 128 per Spec, so no out-of-bound risk if set DPB size = 128.
                pCurrFrame = StartAnchorFrame(fh, updated_refs, anchor_frames_count);
 
                if (!pCurrFrame)
                    throw av1_exception(UMC::UMC_ERR_NOT_INITIALIZED);

                CompleteDecodedFrames(fh, pCurrFrame, pPrevFrame);
                pCurrFrame->SetSkipFlag(true);
            }
            else
            {
                if (!updated_refs.empty())
                {
                    pCurrFrame = pFrameInProgress ?
                        pFrameInProgress : StartFrame(fh, updated_refs, pPrevFrame);
                }
 
                CompleteDecodedFrames(fh, pCurrFrame, pPrevFrame);
                CalcFrameTime(pCurrFrame);

                if (!pCurrFrame)
                    return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

                if (params.lst_mode)
                {
                    // Update frame width and height since it may be different with orinial input
                    FrameHeader &lst_fh = pCurrFrame->GetFrameHeader();
                    lst_fh.RenderWidth = params.info.clip_info.width;
                    lst_fh.RenderHeight = params.info.clip_info.height;
                }
            }

            if (!layout.empty())
                pCurrFrame->AddTileSet(in, layout);

            if (!layout.empty() || repeatedFrame || anchor_decode)
                in->MoveDataPointer(OBUOffset);

            if (repeatedFrame)
            {
                pCurrFrame->ShowAsExisting(true);
                if (anchor_decode && !updated_refs.empty())
                {
                    m_anchor_frame_mem_ids[anchor_frames_count] = updated_refs[pCurrFrame->GetFrameHeader().frame_to_show_map_idx]->m_index;
                    RegisterAnchorFrame(anchor_frames_count++);
                }

                return UMC::UMC_OK;
            }

            if (skipFrame)
            {
                in->MoveDataPointer(OBUOffset);
 
                pCurrFrame->StartDecoding();
                pCurrFrame->CompleteDecoding();
                pCurrFrame->SetSkipFlag(true);

                if (anchor_decode)
                    RegisterAnchorFrame(anchor_frames_count++);

                return UMC::UMC_ERR_NOT_ENOUGH_DATA;
            }
        }

        if (anchor_decode && skipFrame)
        {
            if (!AllocComplete(*pCurrFrame))
            {
                AddFrameDataByIdx(*pCurrFrame, anchor_frames_count);

                if (!AllocComplete(*pCurrFrame))
                    return UMC::UMC_OK;

                firstSubmission = true;
            }
        }
        else
        {
            if (!AllocComplete(*pCurrFrame))
            {
                AddFrameData(*pCurrFrame);

                // for lst mode, different tile lists could have different output buffer size
                // and only have 1 frame header OUB before tile list OBU in exe-tile sequence,
                // so need save clip_info_width/height for new surface re-allocation
                if (params.lst_mode && !anchor_decode)
                {
                     params.info.clip_info.width  = saved_clip_info_width;
                     params.info.clip_info.height = saved_clip_info_height;
                }

                if (!AllocComplete(*pCurrFrame))
                    return UMC::UMC_OK;

                firstSubmission = true;
            }

            if (anchor_decode)
                m_anchor_frame_mem_ids[m_specified_anchor_Idx] = pCurrFrame->m_index;
        }

        UMC::Status umcRes = UMC::UMC_OK;

        if (!params.lst_mode)
        {
            umcRes = SubmitTiles(*pCurrFrame, firstSubmission);
        }
        else if (anchor_decode)
        {
            umcRes = SubmitTiles(*pCurrFrame, firstSubmission);
            RegisterAnchorFrame(anchor_frames_count++);
            QueryFrames();

            return UMC::UMC_ERR_NOT_ENOUGH_DATA;
        }
        else
        {
            pCurrFrame->SetAnchorMap(m_anchor_frame_mem_ids);
            umcRes = SubmitTileList(*pCurrFrame);
        }

        if (umcRes != UMC::UMC_OK)
            return umcRes;

        if (!gotFullFrame)
            return UMC::UMC_ERR_NOT_ENOUGH_DATA;

        return UMC::UMC_OK;
    }

    AV1DecoderFrame* AV1Decoder::FindFrameByMemID(UMC::FrameMemID id)
    {
        return FindFrame(
            [id](AV1DecoderFrame const* f)
            { return f->GetMemID() == id; }
        );
    }

    AV1DecoderFrame* AV1Decoder::DecodeFrameID(UMC::FrameMemID id)
    {
        std::unique_lock<std::mutex> l(guard);
        auto find_it = std::find_if(outputed_frames.begin(), outputed_frames.end(),
            [id](AV1DecoderFrame const* f) { return f->GetMemID() == id; });
        return
            find_it != std::end(outputed_frames) ? (*find_it) : nullptr;
    }

    AV1DecoderFrame* AV1Decoder::GetFrameToDisplay()
    {
        return FindFrame(
            [](AV1DecoderFrame const* f)
            {
                FrameHeader const& h = f->GetFrameHeader();
                bool regularShowFrame = h.show_frame && !h.is_anchor && !FrameInProgress(*f) && !f->Outputted();
                return regularShowFrame || f->ShowAsExisting();
            }
        );
    }

    AV1DecoderFrame* AV1Decoder::FindFrameByUID(int64_t uid)
    {
        return FindFrame(
            [uid](AV1DecoderFrame const* f)
        { return f->UID == uid; }
        );
    }

    AV1DecoderFrame* AV1Decoder::FindFrameInProgress()
    {
        return FindFrame(
            [](AV1DecoderFrame const* f)
        { return FrameInProgress(*f); }
        );
    }

    UMC::Status AV1Decoder::FillVideoParam(SequenceHeader const& sh, UMC_AV1_DECODER::AV1DecoderParams& par)
    {
        par.info.stream_type = UMC::AV1_VIDEO;
        par.info.profile = sh.seq_profile;
        par.info.level = MapLevel(sh.seq_level_idx[0]);

        par.info.clip_info = { int32_t(sh.max_frame_width), int32_t(sh.max_frame_height) };
        par.info.disp_clip_info = par.info.clip_info;

        if (!sh.color_config.subsampling_x && !sh.color_config.subsampling_y)
            par.info.color_format = UMC::YUV444;
        else if (sh.color_config.subsampling_x && !sh.color_config.subsampling_y)
            par.info.color_format = UMC::YUY2;
        else if (sh.color_config.subsampling_x && sh.color_config.subsampling_y)
            par.info.color_format = UMC::NV12;

        if (sh.color_config.BitDepth == 8 && par.info.color_format == UMC::YUV444)
            par.info.color_format = UMC::AYUV;
        if (sh.color_config.BitDepth == 10)
        {
            switch (par.info.color_format)
            {
            case UMC::NV12:   par.info.color_format = UMC::P010; break;
            case UMC::YUY2:   par.info.color_format = UMC::Y210; break;
            case UMC::YUV444: par.info.color_format = UMC::Y410; break;

            default:
                assert(!"Unknown subsampling");
                return UMC::UMC_ERR_UNSUPPORTED;
            }
        }
        else if (sh.color_config.BitDepth == 12)
        {
            switch (par.info.color_format)
            {
            case UMC::NV12:   par.info.color_format = UMC::P016; break;
            case UMC::YUY2:   par.info.color_format = UMC::Y216; break;
            case UMC::YUV444: par.info.color_format = UMC::Y416; break;

            default:
                assert(!"Unknown subsampling");
                return UMC::UMC_ERR_UNSUPPORTED;
            }
        }

        par.info.interlace_type = UMC::PROGRESSIVE;
        par.info.aspect_ratio_width = par.info.aspect_ratio_height = 1;

        par.lFlags = 0;

        par.film_grain = sh.film_grain_param_present;

        return UMC::UMC_OK;
    }

    void AV1Decoder::SetDPBSize(uint32_t size)
    {
        assert(size > 0);
        assert(size <= MAX_EXTERNAL_REFS);

        dpb.resize(size);
        std::generate(std::begin(dpb), std::end(dpb),
            [] { return new AV1DecoderFrame{}; }
        );
    }

    void AV1Decoder::SetRefSize(uint32_t size)
    {
        assert(size > 0);
        assert(size <= MAX_EXTERNAL_REFS);
        
        refs_temp.resize(size);
    }

    void AV1Decoder::CompleteDecodedFrames(FrameHeader const& fh, AV1DecoderFrame* pCurrFrame, AV1DecoderFrame*)
    {
        std::unique_lock<std::mutex> l(guard);
        if (Curr)
        {
            FrameHeader const& FH_OutTemp = Curr->GetFrameHeader();
            if (Repeat_show || FH_OutTemp.show_frame)
            {
                bool bAdded = false;
                for(std::vector<AV1DecoderFrame*>::iterator iter=outputed_frames.begin(); iter!=outputed_frames.end(); iter++)
                {
                    AV1DecoderFrame* temp = *iter;
                    if (Curr->UID == temp->UID)
                    {
                        bAdded = true;
                        break;
                    }
                }
                if (!bAdded)
                    outputed_frames.push_back(Curr);
            }
            else
            {
                // For no display case, decrease reference here which is increased
                // in pFrame->IncrementReference() in show_existing_frame case.
                if(pCurrFrame)
                {
                    if (Curr->UID == -1)
                        Curr = nullptr;
                    else if(Curr != pCurrFrame)
                        Curr->DecrementReference();
                }
            }
        }

        for(std::vector<AV1DecoderFrame*>::iterator iter=outputed_frames.begin(); iter!=outputed_frames.end(); )
        {
            AV1DecoderFrame* temp = *iter;
            if(temp->Outputted() && temp->Displayed() && !temp->Decoded() && !temp->Repeated() && temp->DpbUpdated())
            {
                temp->DecrementReference();
                iter = outputed_frames.erase(iter);
            }
            else
                iter++;
        }

        // When no available buffer, don't update Curr buffer to avoid update DPB duplicated.
        if(pCurrFrame!= NULL)
            Curr = pCurrFrame;

        if (fh.show_existing_frame)
        {
            Repeat_show = 1;
        }
        else
        {
            Repeat_show = 0;
        }
    }

    void AV1Decoder::Flush()
    {
        std::unique_lock<std::mutex> l(guard);
        for(std::vector<AV1DecoderFrame*>::iterator iter=outputed_frames.begin(); iter!=outputed_frames.end(); )
        {
            AV1DecoderFrame* temp = *iter;
            if(temp->Outputted() && temp->Displayed() && !temp->Decoded() && !temp->Repeated() && temp->DpbUpdated())
            {
                temp->DecrementReference();
                iter = outputed_frames.erase(iter);
            }
            else
                iter++;
        }
    }

    AV1DecoderFrame* AV1Decoder::GetFreeFrame(AV1DecoderFrame* excepted)
    {
        std::unique_lock<std::mutex> l(guard);

        auto i = std::find_if(std::begin(dpb), std::end(dpb),
            [excepted](AV1DecoderFrame const* frame)
            { return frame->Empty() && frame != excepted; }
        );

        AV1DecoderFrame* frame =
            i != std::end(dpb) ? *i : nullptr;

        if (frame)
            frame->UID = counter++;

        return frame;
    }

    AV1DecoderFrame* AV1Decoder::GetFrameBuffer(FrameHeader const& fh, AV1DecoderFrame* excepted)
    {
        AV1DecoderFrame* frame = GetFreeFrame(excepted);
        if (!frame)
        {
           return nullptr;
        }

        frame->Reset(&fh);

        // increase ref counter when we get empty frame from DPB
        frame->IncrementReference();

        return frame;
    }

    AV1DecoderFrame* AV1Decoder::GetFrameBufferByIdx(FrameHeader const& fh, UMC::FrameMemID id)
    {
        AV1DecoderFrame* frame = dpb[id];

        if (!frame)
            return nullptr;

        frame->UID = counter++;
        frame->Reset(&fh);
        frame->IncrementReference();
        return frame;
    }

    void AV1Decoder::AddFrameData(AV1DecoderFrame& frame)
    {
        FrameHeader const& fh = frame.GetFrameHeader();

        if (fh.show_existing_frame)
            throw av1_exception(UMC::UMC_ERR_NOT_IMPLEMENTED);

        if (!allocator)
            throw av1_exception(UMC::UMC_ERR_NOT_INITIALIZED);

        UMC::VideoDataInfo info{};
        UMC::Status sts = info.Init(params.info.clip_info.width, params.info.clip_info.height, params.info.color_format, 0);
        if (sts != UMC::UMC_OK)
            throw av1_exception(sts);

        UMC::FrameMemID id;
        sts = allocator->Alloc(&id, &info, mfx_UMC_ReallocAllowed);

        auto frame_source = dynamic_cast<SurfaceSource*>(allocator);
        if (sts != UMC::UMC_OK)
        {
            if (sts == UMC::UMC_ERR_NOT_ENOUGH_BUFFER && frame_source && frame_source->GetSurfaceType() && !m_RecreateSurfaceFlag)
            {
                m_drcFrameWidth = (uint16_t)params.info.clip_info.width;
                m_drcFrameHeight = (uint16_t)params.info.clip_info.height;
            }
            else
            {
                throw av1_exception(UMC::UMC_ERR_ALLOC);
            }
        }

        if (frame_source)
        {
            mfxFrameSurface1* surface = frame_source->GetSurfaceByIndex(id);
            if (!surface)
                throw av1_exception(UMC::UMC_ERR_ALLOC);

            if (m_drcFrameWidth > surface->Info.Width || m_drcFrameHeight > surface->Info.Height)
            {
                surface->Info.Width = mfx::align2_value(m_drcFrameWidth, 16);
                surface->Info.Height = mfx::align2_value(m_drcFrameHeight, 16);
                VAAPIVideoCORE_VPL* vaapi_core_vpl = reinterpret_cast<VAAPIVideoCORE_VPL*>(m_pCore->QueryCoreInterface(MFXIVAAPIVideoCORE_VPL_GUID));
                if (!vaapi_core_vpl)
                    throw av1_exception(UMC::UMC_ERR_NULL_PTR);
                vaapi_core_vpl->ReallocFrame(surface);
            }
        }

        AllocateFrameData(info, id, frame);
        if (frame.m_index < 0)
        {
            throw av1_exception(UMC::UMC_ERR_ALLOC);
        }
    }

    void AV1Decoder::AddFrameDataByIdx(AV1DecoderFrame& frame, uint32_t id)
    {
        if (!allocator)
            throw av1_exception(UMC::UMC_ERR_NOT_INITIALIZED);

        UMC::VideoDataInfo info{};
        UMC::Status sts = info.Init(params.info.clip_info.width, params.info.clip_info.height, params.info.color_format, 0);
        if (sts != UMC::UMC_OK)
            throw av1_exception(sts);

        AllocateFrameData(info, id, frame);
    }

    template <typename F>
    AV1DecoderFrame* AV1Decoder::FindFrame(F pred)
    {
        std::unique_lock<std::mutex> l(guard);

        auto i = std::find_if(std::begin(dpb), std::end(dpb),pred);
        return
            i != std::end(dpb) ? (*i) : nullptr;
    }
}


#endif //MFX_ENABLE_AV1_VIDEO_DECODE
