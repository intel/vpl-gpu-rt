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

#include "umc_structures.h"
#include "umc_va_base.h"
#include "umc_vvc_decoder_va.h"
#include "mfx_umc_alloc_wrapper.h"

namespace UMC_VVC_DECODER
{
    VVCDecoderVA::VVCDecoderVA()
        : m_va(nullptr)
    {}

    UMC::Status VVCDecoderVA::SetParams(UMC::BaseCodecParams* info)
    {
        auto dp = dynamic_cast<VVCDecoderParams*>(info);
        MFX_CHECK(dp, UMC::UMC_ERR_INVALID_PARAMS);
        MFX_CHECK(dp->pVideoAccelerator, UMC::UMC_ERR_NULL_PTR);

        m_va = dp->pVideoAccelerator;
        m_packer.reset(Packer::CreatePacker(m_va));

        return UMC::UMC_OK;
    }

    // Submit frame to driver
    UMC::Status VVCDecoderVA::Submit(VVCDecoderFrame *pFrame)
    {
        UMC::Status sts = 0;
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VVC decode DDISubmitTask");

        TRACE_EVENT(MFX_TRACE_HOTSPOT_DDI_SUBMIT_TASK, EVENT_TYPE_INFO, 0, make_event_data(this));
        sts = m_va->BeginFrame(pFrame->GetMemID());
        MFX_LTRACE_I(MFX_TRACE_LEVEL_INTERNAL, sts);
        MFX_CHECK(UMC::UMC_OK == sts, sts);

        m_packer->PackAU(pFrame, m_dpb);

        sts = m_va->Execute();
        MFX_LTRACE_I(MFX_TRACE_LEVEL_INTERNAL, sts);
        MFX_CHECK(UMC::UMC_OK == sts, sts);

        sts = m_va->EndFrame();
        MFX_LTRACE_I(MFX_TRACE_LEVEL_INTERNAL, sts);
        MFX_CHECK(UMC::UMC_OK == sts, sts);

        return sts;
    }

    // Is decoding still in process
    inline bool InProgress(VVCDecoderFrame const& frame)
    {
        return frame.IsDecodingStarted() && !frame.IsDecodingCompleted();
    }

    // Allocate frame internals
    void VVCDecoderVA::AllocateFrameData(UMC::VideoDataInfo *pInfo, UMC::FrameMemID id, VVCDecoderFrame *pFrame)
    {
        UMC::FrameData fd;
        fd.Init(pInfo, id, m_allocator);

        auto frame_source = dynamic_cast<SurfaceSource*>(m_allocator);
        if (frame_source)
        {
            mfxFrameSurface1* surface =
                frame_source->GetSurfaceByIndex(id);
            if (!surface)
                throw vvc_exception(UMC::UMC_ERR_ALLOC);
        }
        pFrame->Allocate(&fd, pInfo);
        pFrame->m_index = GetFreeIndex(id);
    }

    int32_t VVCDecoderVA::GetFreeIndex (UMC::FrameMemID id) const
    {
        int32_t index = id;

        return index;
    }

    // Finish frame decoding
    void VVCDecoder::CompleteFrame(VVCDecoderFrame* frame)
    {
        if (!frame || m_decodingQueue.empty() || !frame->IsFullFrame())
            return;

        VVCDecoderFrameInfo* frameInfo = frame->GetAU();
        if (frameInfo->GetStatus() > VVCDecoderFrameInfo::STATUS_NOT_FILLED)
            return;

        if (!IsFrameCompleted(frame) || frame->IsDecodingCompleted())
            return;

        if (frame == m_decodingQueue.front())
        {
            RemoveAU(frame->GetAU());
            m_decodingQueue.pop_front();
        }
        else
        {
            RemoveAU(frame->GetAU());
            m_decodingQueue.remove(frame);
        }

        frame->CompleteDecoding();
    }

    // Update access units list finishing completed frames
    void VVCDecoder::SwitchCurrentAU()
    {
        if (!m_FirstAU || !m_FirstAU->IsCompleted())
            return;

        while (m_FirstAU && m_FirstAU->IsCompleted())
        {
            VVCDecoderFrameInfo* completed = m_FirstAU;
            m_FirstAU = m_FirstAU->GetNextAU();
            CompleteFrame(&completed->m_frame);
        }

        InitAUs();
    }

    // Try to find an access unit which to decode next
    void VVCDecoder::InitAUs()
    {
        VVCDecoderFrameInfo* pPrev;
        VVCDecoderFrameInfo* refAU;

        if (!m_FirstAU)
        {
            m_FirstAU = FindAU();
            if (!m_FirstAU)
                return;

            if (!PrepareFrame(&m_FirstAU->m_frame))
            {
                m_FirstAU = 0;
                return;
            }

            m_FirstAU->SetStatus(VVCDecoderFrameInfo::STATUS_STARTED);

            pPrev = m_FirstAU;
            m_FirstAU->SetPrevAU(0);
            m_FirstAU->SetNextAU(0);
            m_FirstAU->SetRefAU(0);

            refAU = 0;
            if (m_FirstAU->IsReference())
            {
                refAU = m_FirstAU;
            }
        }
        else
        {
            pPrev = m_FirstAU;
            refAU = 0;

            pPrev->SetPrevAU(0);
            pPrev->SetRefAU(0);

            if (m_FirstAU->IsReference())
            {
                refAU = pPrev;
            }

            while (pPrev->GetNextAU())
            {
                pPrev = pPrev->GetNextAU();

                if (!refAU)
                    pPrev->SetRefAU(0);

                if (pPrev->IsReference())
                {
                    refAU = pPrev;
                }

                if (pPrev == m_FirstAU)
                    break;
            };
        }

        VVCDecoderFrameInfo* pTemp = FindAU();
        for (; pTemp; )
        {
            if (!PrepareFrame(&pTemp->m_frame))
            {
                pPrev->SetNextAU(0);
                break;
            }

            pTemp->SetStatus(VVCDecoderFrameInfo::STATUS_STARTED);
            pTemp->SetNextAU(0);
            pTemp->SetPrevAU(pPrev);

            pTemp->SetRefAU(refAU);

            if (pTemp->IsReference())
            {
                refAU = pTemp;
            }

            if (pTemp != pPrev)
            {
                pPrev->SetNextAU(pTemp);
                pPrev = pTemp;
            }
            pTemp = FindAU();
        }
    }

    // Find an access unit which has all slices found
    VVCDecoderFrameInfo* VVCDecoder::FindAU()
    {
        FrameQueue::iterator iter = m_decodingQueue.begin();
        FrameQueue::iterator end_iter = m_decodingQueue.end();

        for (; iter != end_iter; ++iter)
        {
            VVCDecoderFrame* frame = *iter;

            VVCDecoderFrameInfo* frameInfo = frame->GetAU();

            if (frameInfo->GetSliceCount())
            {
                if (frameInfo->GetStatus() == VVCDecoderFrameInfo::STATUS_FILLED)
                    return frameInfo;
            }
        }

        return 0;
    }

    // Remove access unit from the linked list of frames
    void VVCDecoder::RemoveAU(VVCDecoderFrameInfo* toRemove)
    {
        VVCDecoderFrameInfo* temp = m_FirstAU;

        if (!temp)
            return;

        VVCDecoderFrameInfo* reference = 0;

        for (; temp; temp = temp->GetNextAU())
        {
            if (temp == toRemove)
                break;

            if (temp->IsReference())
                reference = temp;
        }

        if (!temp)
            return;

        if (temp->GetPrevAU())
            temp->GetPrevAU()->SetNextAU(temp->GetNextAU());

        if (temp->GetNextAU())
            temp->GetNextAU()->SetPrevAU(temp->GetPrevAU());

        VVCDecoderFrameInfo* nextAU = temp->GetNextAU();

        temp->SetPrevAU(0);
        temp->SetNextAU(0);

        if (temp == m_FirstAU)
            m_FirstAU = nextAU;

        if (nextAU)
        {
            temp = nextAU;

            for (; temp; temp = temp->GetNextAU())
            {
                if (temp->GetRefAU())
                    temp->SetRefAU(reference);

                if (temp->IsReference())
                    break;
            }
        }
    }

    // Wakes up working threads to start working on new tasks
    void VVCDecoder::ThreadAwake()
    {
        std::unique_lock<std::mutex> l(m_guard);

        FrameQueue::iterator iter = m_decodingQueue.begin();

        for (; iter != m_decodingQueue.end(); ++iter)
        {
            VVCDecoderFrame* frame = *iter;

            if (IsFrameCompleted(frame))
            {
                CompleteFrame(frame);
                iter = m_decodingQueue.begin();
                if (iter == m_decodingQueue.end()) // avoid ++iter operation
                    break;
            }
        }

        InitAUs();
    }

    UMC::Status VVCDecoder::RunDecoding()
    {
        UMC::Status umcRes = CompleteDecodedFrames(0);
        if (umcRes != UMC::UMC_OK)
            return umcRes;

        VVCDecoderFrame* pFrame = nullptr;

        for (DPBType::iterator it = m_dpb.begin(); it != m_dpb.end(); it++)
        {
            pFrame = *it;
            if (!pFrame->IsDecodingCompleted())
            {
                break;
            }
        }

        ThreadAwake();

        if (!pFrame)
            return UMC::UMC_OK;

        return UMC::UMC_OK;
    }

    // Check for frame completeness and get decoding errors
    bool VVCDecoderVA::QueryFrames()
    {
        std::unique_lock<std::mutex> l(m_guard);
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);

        DPBType decode_queue;
        for (DPBType::iterator frm = m_dpb.begin(); frm != m_dpb.end(); frm++)
            if (InProgress(**frm))
                decode_queue.push_back(*frm);
        // below logic around "wasCompleted" was adopted from AVC/HEVC decoders
        bool wasCompleted = false;
        UMC::Status sts = UMC::UMC_OK;
        for (DPBType::iterator frm = decode_queue.begin(); frm != decode_queue.end(); frm++)
        {
            wasCompleted = false;
            VVCDecoderFrame& frame = **frm;
            if (!frame.IsDecodingStarted())
                continue;
            uint32_t index = 0;
            if (m_va->IsGPUSyncEventEnable() && frame.m_pic_output && frame.IsDisplayable())
            {
                l.unlock();
                {
                    sts = m_packer->SyncTask(frame.GetMemID(), NULL);
                }
                l.lock();
            }
            VAStatus surfErr = VA_STATUS_SUCCESS;
            l.unlock();
            UMC::Status sts = m_packer->SyncTask(frame.GetMemID(), &surfErr);
            l.lock();

            frame.CompleteDecoding();
            wasCompleted = true;

            if (sts < UMC::UMC_OK)
            {
                if (sts != UMC::UMC_ERR_GPU_HANG)
                    sts = UMC::UMC_ERR_DEVICE_FAILED;

                frame.SetError(frame,sts);
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
            // check previously cached reports
            for (uint32_t i = 0; i < m_reports.size(); i++)
            {
                if (m_reports[i].m_index == static_cast<uint32_t>(frame.m_index)) // report for the frame was found in previuously cached reports
                {
                    frame.SetError(frame, m_reports[i].m_status);
                    frame.CompleteDecoding();
                    wasCompleted = true;
                    m_reports.erase(m_reports.begin() + i);
                    break;
                }
            }
        }
        return wasCompleted;
    }
}

#endif //MFX_ENABLE_VVC_VIDEO_DECODE
