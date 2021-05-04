// Copyright (c) 2013-2019 Intel Corporation
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

#include <algorithm>
#include "umc_structures.h"

#include "umc_h265_segment_decoder_dxva.h"
#include "umc_h265_frame_list.h"

#include "umc_h265_slice_decoding.h"
#include "umc_h265_task_supplier.h"
#include "umc_h265_frame_info.h"

#include "umc_h265_va_packer.h"

#include "mfx_common.h" //  for trace routines

#include "vm_time.h"


namespace UMC_HEVC_DECODER
{
H265_DXVA_SegmentDecoderCommon::H265_DXVA_SegmentDecoderCommon(TaskSupplier_H265 * pTaskSupplier)
    : H265SegmentDecoderBase(pTaskSupplier->GetTaskBroker())
    , m_va(0)
    , m_pTaskSupplier(pTaskSupplier)
{
}

void H265_DXVA_SegmentDecoderCommon::SetVideoAccelerator(UMC::VideoAccelerator *va)
{
    VM_ASSERT(va);
    m_va = (UMC::VideoAccelerator*)va;
}

H265_DXVA_SegmentDecoder::H265_DXVA_SegmentDecoder(TaskSupplier_H265 * pTaskSupplier)
    : H265_DXVA_SegmentDecoderCommon(pTaskSupplier)
    , m_CurrentSliceID(0)
{
}

H265_DXVA_SegmentDecoder::~H265_DXVA_SegmentDecoder()
{
}

UMC::Status H265_DXVA_SegmentDecoder::Init(int32_t iNumber)
{
    return H265SegmentDecoderBase::Init(iNumber);
}

void H265_DXVA_SegmentDecoder::PackAllHeaders(H265DecoderFrame * pFrame)
{
    if (!m_Packer.get())
    {
        m_Packer.reset(Packer::CreatePacker(m_va));
        VM_ASSERT(m_Packer.get());
    }

    m_Packer->BeginFrame(pFrame);
    m_Packer->PackAU(pFrame, m_pTaskSupplier);
    m_Packer->EndFrame();
}

UMC::Status H265_DXVA_SegmentDecoder::ProcessSegment(void)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "H265_DXVA_SegmentDecoder::ProcessSegment");
    try
    {
        if (!m_pTaskBroker->GetNextTask(0))
            return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }
    catch (h265_exception const& e)
    {
        return e.GetStatus();
    }

    return UMC::UMC_OK;
}


TaskBrokerSingleThreadDXVA::TaskBrokerSingleThreadDXVA(TaskSupplier_H265 * pTaskSupplier)
    : TaskBroker_H265(pTaskSupplier)
    , m_lastCounter(0)
{
    m_counterFrequency = vm_time_get_frequency();
}

bool TaskBrokerSingleThreadDXVA::PrepareFrame(H265DecoderFrame * pFrame)
{
    if (!pFrame || pFrame->prepared)
    {
        return true;
    }

    if (!pFrame->prepared &&
        (pFrame->GetAU()->GetStatus() == H265DecoderFrameInfo::STATUS_FILLED || pFrame->GetAU()->GetStatus() == H265DecoderFrameInfo::STATUS_STARTED))
    {
        pFrame->prepared = true;
    }

    return true;
}

void TaskBrokerSingleThreadDXVA::Reset()
{
    m_lastCounter = 0;
    TaskBroker_H265::Reset();
    m_reports.clear();
}

void TaskBrokerSingleThreadDXVA::Start()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    TaskBroker_H265::Start();
    m_completedQueue.clear();
}

enum
{
    NUMBER_OF_STATUS = 512,
};

bool TaskBrokerSingleThreadDXVA::GetNextTaskInternal(H265Task *)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "TaskBrokerSingleThreadDXVA::GetNextTaskInternal");
    UMC::AutomaticUMCMutex guard(m_mGuard);

    // check error(s)
    if (m_IsShouldQuit)
    {
        return false;
    }

    H265_DXVA_SegmentDecoder * dxva_sd = static_cast<H265_DXVA_SegmentDecoder *>(m_pTaskSupplier->m_pSegmentDecoder[0]);

    if (!dxva_sd->GetPacker())
        return false;

#if defined(UMC_VA_LINUX)
#if !defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)
    #error unsupported sync. type
#else
    UMC::Status sts = UMC::UMC_OK;
    VAStatus surfErr = VA_STATUS_SUCCESS;
    int32_t index;

    for (H265DecoderFrameInfo * au = m_FirstAU; au; au = au->GetNextAU())
    {
        index = au->m_pFrame->GetFrameMID();

        m_mGuard.Unlock();
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Dec vaSyncSurface");
            sts = dxva_sd->GetPacker()->SyncTask(index, &surfErr);
        }
        m_mGuard.Lock();

        //we should complete frame even we got an error
        //this allows to return the error from [RunDecoding]
        au->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED);
        CompleteFrame(au->m_pFrame);

        if (sts < UMC::UMC_OK)
        {
            if (sts != UMC::UMC_ERR_GPU_HANG)
                sts = UMC::UMC_ERR_DEVICE_FAILED;

            au->m_pFrame->SetError(sts);
        }
        else if (sts == UMC::UMC_OK)
            switch (surfErr)
            {
                case MFX_CORRUPTION_MAJOR:
                    au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                    break;

                case MFX_CORRUPTION_MINOR:
                    au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MINOR);
                    break;
            }

        if (sts != UMC::UMC_OK)
            throw h265_exception(sts);
    }

    SwitchCurrentAU();
#endif
#elif defined(UMC_VA_DXVA)
    bool wasCompleted = false;
    UMC::Status sts = UMC::UMC_OK;

    for (H265DecoderFrameInfo * au = m_FirstAU; au; au = au->GetNextAU())
    {
        if (dxva_sd->GetPacker()->IsGPUSyncEventEnable())
        {
            int32_t index = au->m_pFrame->GetFrameMID();
            m_mGuard.Unlock();
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Dec  DXVA SyncSurface");
                sts = dxva_sd->GetPacker()->SyncTask(index, NULL);
            }
            m_mGuard.Lock();
        }

        for (uint32_t i = 0; i < m_reports.size(); i++)
        {
            if (m_reports[i].m_index == (uint32_t)au->m_pFrame->m_index)
            {
                switch (m_reports[i].m_status)
                {
                case 1:
                    au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MINOR);
                    break;
                case 2:
                    au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                    break;
                case 3:
                    au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                    break;
                case 4:
                    au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                    break;
                }

                au->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED);
                CompleteFrame(au->m_pFrame);
                wasCompleted = true;

                m_reports.erase(m_reports.begin() + i);
                break;
            }
        }

        if (!wasCompleted)
        {
            DXVA_Status_HEVC pStatusReport[NUMBER_OF_STATUS] = {};
            dxva_sd->GetPacker()->GetStatusReport(&pStatusReport[0], sizeof(pStatusReport));

            for (uint32_t i = 0; i < NUMBER_OF_STATUS; i++)
            {
                if (!pStatusReport[i].StatusReportFeedbackNumber)
                    continue;

                bool wasFound = false;
                if (au && pStatusReport[i].CurrPic.Index7Bits == au->m_pFrame->m_index)
                {
                    switch (pStatusReport[i].bStatus)
                    {
                    case 1:
                        au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MINOR);
                        break;
                    case 2:
                        au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                        break;
                    case 3:
                        au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                        break;
                    case 4:
                        au->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
                        break;
                    }

                    au->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED);
                    CompleteFrame(au->m_pFrame);
                    wasFound = true;
                    wasCompleted = true;
                }

                if (!wasFound)
                {
                    if (std::find(m_reports.begin(), m_reports.end(), ReportItem(pStatusReport[i].CurrPic.Index7Bits, 0/*field*/, 0)) == m_reports.end())
                    {
                        m_reports.push_back(ReportItem(pStatusReport[i].CurrPic.Index7Bits, 0/*field*/, pStatusReport[i].bStatus));
                        wasCompleted = true;
                    }
                }
            }
        }
        //check exit from waiting status.
        if (sts != UMC::UMC_OK && sts != UMC::UMC_ERR_TIMEOUT && dxva_sd->GetPacker()->IsGPUSyncEventEnable())
        {
            // SyncTask failed for some reason
            au->m_pFrame->SetError(UMC::UMC_ERR_DEVICE_FAILED);
            au->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED);
            CompleteFrame(au->m_pFrame);
        }
    }
    SwitchCurrentAU();

    if (!wasCompleted && m_FirstAU)
    {
        unsigned long long currentCounter = (unsigned long long) vm_time_get_tick();

        if (m_lastCounter == 0)
            m_lastCounter = currentCounter;

        unsigned long long diff = (currentCounter - m_lastCounter);
        if (diff >= m_counterFrequency ||
            (sts == UMC::UMC_ERR_TIMEOUT && dxva_sd->GetPacker()->IsGPUSyncEventEnable()))
        {
            Report::iterator iter = std::find(m_reports.begin(), m_reports.end(), ReportItem(m_FirstAU->m_pFrame->m_index, false, 0));
            if (iter != m_reports.end())
            {
                m_reports.erase(iter);
            }

            m_FirstAU->m_pFrame->SetErrorFlagged(UMC::ERROR_FRAME_MAJOR);
            m_FirstAU->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED);
            CompleteFrame(m_FirstAU->m_pFrame);

            SwitchCurrentAU();
            m_lastCounter = 0;
        }
    }
    else
    {
        m_lastCounter = 0;
    }

#endif

    return false;
}

void TaskBrokerSingleThreadDXVA::AwakeThreads()
{
}

} // namespace UMC_HEVC_DECODER

#endif // MFX_ENABLE_H265_VIDEO_DECODE
