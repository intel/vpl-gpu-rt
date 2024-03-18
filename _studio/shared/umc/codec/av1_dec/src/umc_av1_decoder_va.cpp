// Copyright (c) 2017-2021 Intel Corporation
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
#include "umc_va_base.h"

#include "umc_av1_decoder_va.h"
#include "umc_av1_utils.h"
#include "umc_av1_frame.h"
#include "umc_av1_bitstream.h"
#include "umc_av1_va_packer.h"

#include "umc_frame_data.h"

#include <algorithm>

#include "mfx_unified_av1d_logging.h"

namespace UMC_AV1_DECODER
{
    AV1DecoderVA::AV1DecoderVA()
        : va(nullptr)
    {}

    UMC::Status AV1DecoderVA::SetParams(UMC::BaseCodecParams* info)
    {
        if (!info)
            return UMC::UMC_ERR_NULL_PTR;

        AV1DecoderParams* dp =
            DynamicCast<AV1DecoderParams, UMC::BaseCodecParams>(info);
        if (!dp)
            return UMC::UMC_ERR_INVALID_PARAMS;

        if (!dp->pVideoAccelerator)
            return UMC::UMC_ERR_NULL_PTR;

        va = dp->pVideoAccelerator;
        packer.reset(Packer::CreatePacker(va));

        uint32_t dpb_size = std::max(params.async_depth + TOTAL_REFS, 8u);

        if (dp->lst_mode)
        {
            dpb_size = MAX_EXTERNAL_REFS;
        }


        SetDPBSize(dpb_size);
        SetRefSize(TOTAL_REFS);
        return UMC::UMC_OK;
    }

    UMC::Status AV1DecoderVA::SubmitTiles(AV1DecoderFrame& frame, bool firstSubmission)
    {
        UMC::Status sts = UMC::UMC_OK;

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "AV1 decode DDISubmitTask");

        assert(va);

        if (firstSubmission)
        {
            // it's first submission for current frame - need to call BeginFrame
            sts = va->BeginFrame(frame.GetMemID(SURFACE_RECON));
            MFX_LTRACE_I(MFX_TRACE_LEVEL_INTERNAL, sts);

            TRACE_EVENT(MFX_TRACE_HOTSPOT_DDI_SUBMIT_TASK, EVENT_TYPE_END, TR_KEY_DDI_API, make_event_data(FrameIndex, frame.GetMemID(), sts));

            if (sts != UMC::UMC_OK)
                return sts;

            assert(packer);
            packer->BeginFrame();

            frame.StartDecoding();
        }

        auto &tileSets = frame.GetTileSets();
        packer->PackAU(tileSets, frame, firstSubmission);

        const bool lastSubmission = (GetNumMissingTiles(frame) == 0);
        if (lastSubmission)
        {
            packer->EndFrame();
            // VA Execute after full frame detected to avoid duplicate submission
            // of the decode buffer when app use small decbufsize.
            sts = va->Execute();
            MFX_LTRACE_I(MFX_TRACE_LEVEL_INTERNAL, sts);
        }
        if (sts != UMC::UMC_OK)
        {
            sts = UMC::UMC_ERR_DEVICE_FAILED;
            return sts;
        }

        if (lastSubmission) // it's last submission for current frame - need to call EndFrame
        {
            TRACE_EVENT(MFX_TRACE_HOTSPOT_DDI_ENDFRAME_TASK, EVENT_TYPE_START, TR_KEY_DDI_API, make_event_data(FrameIndex, sts));

            sts = va->EndFrame();
            MFX_LTRACE_I(MFX_TRACE_LEVEL_INTERNAL, sts);

            TRACE_EVENT(MFX_TRACE_HOTSPOT_DDI_ENDFRAME_TASK, EVENT_TYPE_END, TR_KEY_DDI_API, make_event_data(FrameIndex, sts));
        }
        return sts;
    }

    UMC::Status AV1DecoderVA::SubmitTileList(AV1DecoderFrame& frame)
    {
        assert(va);
        UMC::Status sts = UMC::UMC_OK;

        sts = va->BeginFrame(frame.GetMemID(SURFACE_RECON));
        if (sts != UMC::UMC_OK)
            return sts;

        assert(packer);
        packer->BeginFrame();
        frame.StartDecoding();

        auto &tileSets = frame.GetTileSets();
        packer->PackAU(tileSets, frame, true);

        packer->EndFrame();

        sts = va->Execute();
        if (sts != UMC::UMC_OK)
            return UMC::UMC_ERR_DEVICE_FAILED;

        sts = va->EndFrame();
        if (sts != UMC::UMC_OK)
            return sts;

        return sts;
    }

    UMC::Status AV1DecoderVA::RegisterAnchorFrame(uint32_t id)
    {
        UMC::Status sts = UMC::UMC_OK;

        assert(packer);
        packer->RegisterAnchor(id);

        return sts;
    }

    void AV1DecoderVA::AllocateFrameData(UMC::VideoDataInfo const& info, UMC::FrameMemID id, AV1DecoderFrame& frame)
    {
        assert(id != UMC::FRAME_MID_INVALID);

        UMC::FrameData fd;
        fd.Init(&info, id, allocator);

        frame.AllocateAndLock(&fd);
        frame.m_index = GetFreeIndex(id);
    }

    int32_t AV1DecoderVA::GetFreeIndex (UMC::FrameMemID id) const
    {
        int32_t index = id;

        return index;
    }

    inline bool InProgress(AV1DecoderFrame const& frame)
    {
        return frame.DecodingStarted() && !frame.DecodingCompleted();
    }


    bool AV1DecoderVA::QueryFrames()
    {
        std::unique_lock<std::mutex> auto_guard(guard);
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);
        // form frame queue in decoded order
        DPBType decode_queue;
        for (DPBType::iterator frm = dpb.begin(); frm != dpb.end(); frm++)
            if (InProgress(**frm))
                decode_queue.push_back(*frm);

        std::sort(decode_queue.begin(), decode_queue.end(),
            [](AV1DecoderFrame const* f1, AV1DecoderFrame const* f2) {return f1->UID < f2->UID; });

        // below logic around "wasCompleted" was adopted from AVC/HEVC decoders
        bool wasCompleted = false;
        UMC::Status sts = UMC::UMC_OK;
        // iterate through frames submitted to the driver in decoded order
        for (DPBType::iterator frm = decode_queue.begin(); frm != decode_queue.end(); frm++)
        {
            wasCompleted = false;
            AV1DecoderFrame& frame = **frm;
            uint32_t index = 0;
            if (va->IsGPUSyncEventEnable())
            {
                auto_guard.unlock();
                {
                    sts = packer->SyncTask(frame.GetMemID(), NULL);
                }
                auto_guard.lock();
            }
            VAStatus surfErr = VA_STATUS_SUCCESS;
            index = frame.GetMemID();
            auto_guard.unlock();
            UMC::Status sts =  packer->SyncTask(index, &surfErr);
            auto_guard.lock();

            frame.CompleteDecoding();
            wasCompleted = true;

            if (sts < UMC::UMC_OK)
            {
                // [Global] Add GPU hang reporting
            }
            else if (sts == UMC::UMC_OK)
            {
                switch (surfErr)
                {
                    case MFX_CORRUPTION_MAJOR:
                        frame.AddError(UMC::ERROR_FRAME_MAJOR);
                        break;

                    case MFX_CORRUPTION_MINOR:
                        frame.AddError(UMC::ERROR_FRAME_MINOR);
                        break;
                }
            }

            TRACE_EVENT(MFX_TRACE_API_AV1_SYNCINFO_TASK, EVENT_TYPE_INFO, TR_KEY_DECODE_BASIC_INFO, make_event_data(
                frame.m_index, (uint32_t)frame.Outputted(), (uint32_t)frame.DecodingCompleted(), (uint32_t)frame.Displayed(), sts));
        }

        return wasCompleted;
    }
}

#endif //MFX_ENABLE_AV1_VIDEO_DECODE
