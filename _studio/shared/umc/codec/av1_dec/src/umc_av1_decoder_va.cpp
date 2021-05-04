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

#include "umc_defs.h"
#ifdef MFX_ENABLE_AV1_VIDEO_DECODE

#include "umc_structures.h"
#include "umc_va_base.h"

#include "umc_av1_decoder_va.h"
#include "umc_av1_utils.h"
#include "umc_av1_frame.h"
#include "umc_av1_bitstream.h"
#include "umc_av1_va_packer.h"
#include "umc_av1_msft_ddi.h"

#include "umc_frame_data.h"

#include <algorithm>

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

        uint32_t const dpb_size =
            params.async_depth + TOTAL_REFS + 2;
        SetDPBSize(dpb_size);
        SetRefSize(TOTAL_REFS);
        return UMC::UMC_OK;
    }

    UMC::Status AV1DecoderVA::SubmitTiles(AV1DecoderFrame& frame, bool firstSubmission)
    {
        VM_ASSERT(va);
        UMC::Status sts = UMC::UMC_OK;

        if (firstSubmission)
        {
            // it's first submission for current frame - need to call BeginFrame
#ifdef UMC_VA_LINUX
            sts = va->BeginFrame(frame.GetMemID(SURFACE_RECON));
#else
            sts = va->BeginFrame(frame.GetMemID());
#endif
            if (sts != UMC::UMC_OK)
                return sts;

            VM_ASSERT(packer);
            packer->BeginFrame();

            frame.StartDecoding();
        }

        auto &tileSets = frame.GetTileSets();
        packer->PackAU(tileSets, frame, firstSubmission);

        const bool lastSubmission = (GetNumMissingTiles(frame) == 0);
        if (lastSubmission)
            packer->EndFrame();

        sts = va->Execute();
        if (sts != UMC::UMC_OK)
            sts = UMC::UMC_ERR_DEVICE_FAILED;

        if (lastSubmission) // it's last submission for current frame - need to call EndFrame
            sts = va->EndFrame();

        return sts;
    }

    void AV1DecoderVA::AllocateFrameData(UMC::VideoDataInfo const& info, UMC::FrameMemID id, AV1DecoderFrame& frame)
    {
        VM_ASSERT(id != UMC::FRAME_MID_INVALID);

        UMC::FrameData fd;
        fd.Init(&info, id, allocator);

        frame.AllocateAndLock(&fd);
    }

    inline bool InProgress(AV1DecoderFrame const& frame)
    {
        return frame.DecodingStarted() && !frame.DecodingCompleted();
    }

#ifndef UMC_VA_LINUX
    inline void SetError(AV1DecoderFrame& frame, uint8_t status)
    {
        switch (status)
        {
        case 1:
            frame.AddError(UMC::ERROR_FRAME_MINOR);
            break;
        case 2:
        case 3:
        case 4:
        default:
            frame.AddError(UMC::ERROR_FRAME_MAJOR);
            break;
        }
    }

    const uint32_t NUMBER_OF_STATUS = 32;
#endif

    bool AV1DecoderVA::QueryFrames()
    {
        std::unique_lock<std::mutex> auto_guard(guard);

        // form frame queue in decoded order
        DPBType decode_queue;
        for (DPBType::iterator frm = dpb.begin(); frm != dpb.end(); frm++)
            if (InProgress(**frm))
                decode_queue.push_back(*frm);

        std::sort(decode_queue.begin(), decode_queue.end(),
            [](AV1DecoderFrame const* f1, AV1DecoderFrame const* f2) {return f1->UID < f2->UID; });

        // below logic around "wasCompleted" was adopted from AVC/HEVC decoders
        bool wasCompleted = false;

        // iterate through frames submitted to the driver in decoded order
        for (DPBType::iterator frm = decode_queue.begin(); frm != decode_queue.end(); frm++)
        {
            AV1DecoderFrame& frame = **frm;
            uint32_t index = 0;
#ifdef UMC_VA_LINUX
            VAStatus surfErr = VA_STATUS_SUCCESS;
            index = frame.GetMemID();
            auto_guard.unlock();
            UMC::Status sts =  packer->SyncTask(index, &surfErr);
            auto_guard.lock();

            frame.CompleteDecoding();
            wasCompleted = true;

            if (sts < UMC::UMC_OK)
            {
                // TODO: [Global] Add GPU hang reporting
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
#else
            // check previously cached reports
            for (uint32_t i = 0; i < reports.size(); i++)
            {
                if (reports[i].m_index == static_cast<uint32_t>(frame.GetMemID())) // report for the frame was found in previuously cached reports
                {
                    SetError(frame, reports[i].m_status);
                    frame.CompleteDecoding();
                    wasCompleted = true;
                    reports.erase(reports.begin() + i);
                    break;
                }
            }

#ifdef UMC_VA_AV1_MSFT
            if (!wasCompleted) // nothing from "decode_queue" completed yet - need to get new status reports from the driver
            {
                DXVA_Status_AV1 pStatusReport[NUMBER_OF_STATUS];

                std::fill_n(pStatusReport, NUMBER_OF_STATUS, DXVA_Status_AV1{});
                // get new frame status reports from the driver
                packer->GetStatusReport(&pStatusReport[0], sizeof(DXVA_Status_AV1)*NUMBER_OF_STATUS);

                // iterate through new status reports
                for (uint32_t i = 0; i < NUMBER_OF_STATUS; i++)
                {
                    if (!pStatusReport[i].StatusReportFeedbackNumber)
                        continue;

                    bool wasFound = false;
                    index = pStatusReport[i].CurrPic.Index;
                    if (index == static_cast<uint32_t>(frame.GetMemID())) // report for the frame was found in new reports
                    {
                        SetError(frame, pStatusReport[i].bStatus);
                        frame.CompleteDecoding();
                        wasFound = true;
                        wasCompleted = true;
                    }

                    if (!wasFound) // new reports don't contain report for current frame
                    {
                        if (std::find(reports.begin(), reports.end(), ReportItem(index, 0)) == reports.end()) // discard new reports which duplicate previously cached reports
                        {
                            // push unique new reports to status report cache
                            reports.push_back(ReportItem(index, pStatusReport[i].bStatus));
                            // if got at least one unique report - stop getting more status reports from the driver
                            wasCompleted = true;
                        }
                    }
                }
            }
#else //intel DDI
            if (!wasCompleted) // nothing from "decode_queue" completed yet - need to get new status reports from the driver
            {
                DXVA_Intel_Status_AV1 pStatusReport[NUMBER_OF_STATUS];
                std::fill_n(pStatusReport, NUMBER_OF_STATUS, DXVA_Intel_Status_AV1{});
                // get new frame status reports from the driver
                packer->GetStatusReport(&pStatusReport[0], sizeof(DXVA_Intel_Status_AV1)*NUMBER_OF_STATUS);

                // iterate through new status reports
                for (uint32_t i = 0; i < NUMBER_OF_STATUS; i++)
                {
                    if (!pStatusReport[i].StatusReportFeedbackNumber)
                        continue;

                    bool wasFound = false;
#if AV1D_DDI_VERSION >= 31
                    index = pStatusReport[i].current_picture.bPicEntry;
#else
                    index = pStatusReport[i].current_picture.wPicEntry;
#endif
                    if (index == static_cast<uint32_t>(frame.GetMemID())) // report for the frame was found in new reports
                    {
                        SetError(frame, pStatusReport[i].bStatus);
                        frame.CompleteDecoding();
                        wasFound = true;
                        wasCompleted = true;
                    }

                    if (!wasFound) // new reports don't contain report for current frame
                    {
                        if (std::find(reports.begin(), reports.end(), ReportItem(index, 0)) == reports.end()) // discard new reports which duplicate previously cached reports
                        {
                            // push unique new reports to status report cache
                            reports.push_back(ReportItem(index, pStatusReport[i].bStatus));
                            // if got at least one unique report - stop getting more status reports from the driver
                            wasCompleted = true;
                        }
                    }
                }
            }
#endif

#if UMC_AV1_DECODER_REV <= 8500
            // so far driver doesn't support status reporting for AV1 decoder on Windows
            // TODO: check if status reporting is still not supported
            // workaround by marking frame as decoding_completed and setting wasCompleted to true
            frame.CompleteDecoding();
            wasCompleted = true;
#endif

#endif
        }

        return wasCompleted;
    }
}

#endif //MFX_ENABLE_AV1_VIDEO_DECODE
