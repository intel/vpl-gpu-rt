// Copyright (c) 2012-2024 Intel Corporation
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

#ifndef __UMC_AV1_DECODER_H
#define __UMC_AV1_DECODER_H

#include "umc_video_decoder.h"
#include "umc_frame_allocator.h"
#include "umc_av1_frame.h"

#include <mutex>
#include <memory>
#include <vector>
#include <deque>

namespace UMC
{ class FrameAllocator; }

namespace UMC_AV1_DECODER
{
    struct SequenceHeader;
    struct FrameHeader;
    class AV1DecoderFrame;

    class AV1DecoderParams
        : public UMC::VideoDecoderParams
    {
        DYNAMIC_CAST_DECL(AV1DecoderParams, UMC::VideoDecoderParams)

    public:

        AV1DecoderParams()
            : allocator(nullptr)
            , async_depth(0)
            , film_grain(0)
            , io_pattern(0)
            , lst_mode(0)
            , anchors_num(0)
            , anchors_loaded(false)
            , skip_first_frames(0)
            , pre_loaded_anchors(nullptr)
            , color_range(0)
            , color_description_present_flag(0)
            , color_primaries(AOM_CICP_CP_UNSPECIFIED)
            , transfer_characteristics(AOM_CICP_TC_UNSPECIFIED)
            , matrix_coefficients(AOM_CICP_MC_UNSPECIFIED)
            , framerate_n(0)
            , framerate_d(0)
        {}

    public:

        UMC::FrameAllocator* allocator;
        uint32_t             async_depth;
        uint32_t             film_grain;
        uint32_t             io_pattern;
        uint32_t             lst_mode;
        uint32_t             anchors_num;
        bool                 anchors_loaded;
        uint32_t             skip_first_frames;
        mfxFrameSurface1**   pre_loaded_anchors;
        uint32_t             color_range;
        uint32_t             color_description_present_flag;
        uint32_t             color_primaries;
        uint32_t             transfer_characteristics;
        uint32_t             matrix_coefficients;
        uint32_t             framerate_n;
        uint32_t             framerate_d;
    };

    class ReportItem // adopted from HEVC/AVC decoders
    {
    public:
        uint32_t  m_index;
        uint8_t   m_status;

        ReportItem(uint32_t index, uint8_t status)
            : m_index(index)
            , m_status(status)
        {
        }

        bool operator == (const ReportItem & item) const
        {
            return (item.m_index == m_index);
        }

        bool operator != (const ReportItem & item) const
        {
            return (item.m_index != m_index);
        }
    };

    class AV1Decoder
        : public UMC::VideoDecoder
    {

    public:

        AV1Decoder();
        ~AV1Decoder();

    public:

        static UMC::Status DecodeHeader(UMC::MediaData*, UMC_AV1_DECODER::AV1DecoderParams&);

        /* UMC::BaseCodec interface */
        UMC::Status Init(UMC::BaseCodecParams*) override;
        UMC::Status Update_drc(SequenceHeader* seq);
        UMC::Status GetFrame(UMC::MediaData* in, UMC::MediaData* out) override;

        virtual UMC::Status Reset() override
        { return UMC::UMC_ERR_NOT_IMPLEMENTED; }
        UMC::Status GetInfo(UMC::BaseCodecParams*) override;

    public:

        /* UMC::VideoDecoder interface */
        virtual UMC::Status ResetSkipCount() override
        { return UMC::UMC_ERR_NOT_IMPLEMENTED; }
        virtual UMC::Status SkipVideoFrame(int32_t) override
        { return UMC::UMC_ERR_NOT_IMPLEMENTED; }
        virtual uint32_t GetNumOfSkippedFrames() override
        { return 0; }
        bool is_seq_header_ready() const
        {
            return sequence_header_ready;
        }
        void set_seq_header_ready()
        {
            sequence_header_ready = true;
        }
        void clean_seq_header_ready()
        {
            sequence_header_ready = false;
        }

    public:

        AV1DecoderFrame* FindFrameByMemID(UMC::FrameMemID);
        AV1DecoderFrame* GetFrameToDisplay();
        AV1DecoderFrame* FindFrameByUID(int64_t uid);
        AV1DecoderFrame* DecodeFrameID(UMC::FrameMemID);
        AV1DecoderFrame* FindFrameInProgress();
        AV1DecoderFrame* GetCurrFrame()
        { return Curr; }
        UMC::FrameMemID  GetRepeatedFrame(){return repeateFrame;}
        void SetInFrameRate(mfxF64 rate)
        { in_framerate = rate; }
        void SetAnchorIdx(UMC::FrameMemID idx)
        { m_specified_anchor_Idx = idx;}
        void SetAsAnchor(bool isAnchorFrame)
        { m_isAnchor = isAnchorFrame;}

        virtual bool QueryFrames() = 0;

        void SetVideoCore(VideoCORE* pCore)
        {
            m_pCore = pCore;
        }

        void Flush();

    protected:

        static UMC::Status FillVideoParam(SequenceHeader const&, UMC_AV1_DECODER::AV1DecoderParams&);

        virtual void SetDPBSize(uint32_t);
        virtual void SetRefSize(uint32_t);
        virtual AV1DecoderFrame* GetFreeFrame(AV1DecoderFrame*);
        virtual AV1DecoderFrame* GetFrameBuffer(FrameHeader const&, AV1DecoderFrame*);
        virtual void AddFrameData(AV1DecoderFrame&);
        virtual void AddFrameDataByIdx(AV1DecoderFrame& frame, uint32_t idx);

        virtual void AllocateFrameData(UMC::VideoDataInfo const&, UMC::FrameMemID, AV1DecoderFrame&) = 0;
        virtual void CompleteDecodedFrames(FrameHeader const&, AV1DecoderFrame*, AV1DecoderFrame*);
        virtual UMC::Status SubmitTiles(AV1DecoderFrame&, bool) = 0;

        virtual UMC::Status SubmitTileList(AV1DecoderFrame&) = 0;
        virtual UMC::Status RegisterAnchorFrame(uint32_t id) = 0;

    private:

        template <typename F>
        AV1DecoderFrame* FindFrame(F pred);
        AV1DecoderFrame* StartFrame(FrameHeader const&, DPBType &, AV1DecoderFrame*);

        void CalcFrameTime(AV1DecoderFrame*);

        AV1DecoderFrame* GetFrameBufferByIdx(FrameHeader const& fh, UMC::FrameMemID id);
        AV1DecoderFrame* StartAnchorFrame(FrameHeader const& fh, DPBType const& frameDPB, uint32_t idx);
        DPBType DPBUpdate(AV1DecoderFrame * prevFrame);


    protected:

        std::mutex                      guard;

        UMC::FrameAllocator*            allocator;

        std::unique_ptr<SequenceHeader> sequence_header;
        std::unique_ptr<SequenceHeader> old_seqHdr;

        DPBType                         dpb;     // store of decoded frames

        uint32_t                        counter;
        AV1DecoderParams                params;
        std::vector<AV1DecoderFrame*>   outputed_frames; // tore frames need to be output
        AV1DecoderFrame*                Curr; // store current frame for Poutput
        AV1DecoderFrame*                Curr_temp; // store current frame insist double updateDPB
        uint32_t                        Repeat_show; // show if current frame is repeated frame
        uint32_t                        PreFrame_id;//id of previous frame
        uint32_t                        OldPreFrame_id;//old id of previous frame. When decode LST clip, need this for parsing twice
        DPBType                         refs_temp; // previous updated frameDPB
        mfxU16                          frame_order;
        mfxF64                          in_framerate;
        UMC::FrameMemID                 repeateFrame;//frame to be repeated

        uint32_t                        anchor_frames_count;
        uint32_t                        tile_list_idx;
        uint32_t                        frames_to_skip;
        uint32_t                        saved_clip_info_width;
        uint32_t                        saved_clip_info_height;
        bool                            clip_info_size_saved;
        bool                            sequence_header_ready;

        FrameHeader                     m_prev_frame_header;
        bool                            m_prev_frame_header_exist;
        UMC::FrameMemID                 m_anchor_frame_mem_ids[MAX_ANCHOR_SIZE];
        UMC::FrameMemID                 m_specified_anchor_Idx; // anchor frame index specified by application
        bool                            m_isAnchor; // check if current frame is a anchor frame
        bool                            m_RecreateSurfaceFlag;
        uint16_t                        m_drcFrameWidth;
        uint16_t                        m_drcFrameHeight;
        VideoCORE*                      m_pCore;
    };
}

#endif // __UMC_AV1_DECODER_H
#endif // MFX_ENABLE_AV1_VIDEO_DECODE

