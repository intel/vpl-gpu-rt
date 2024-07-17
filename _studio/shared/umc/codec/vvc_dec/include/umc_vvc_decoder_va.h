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

#include "umc_vvc_decoder.h"

#ifndef __UMC_VVC_DECODER_VA_H
#define __UMC_VVC_DECODER_VA_H

namespace UMC_VVC_DECODER
{
    class Packer
    {

    public:

        Packer(UMC::VideoAccelerator *va);
        virtual ~Packer();

        virtual UMC::Status GetStatusReport(void *pStatusReport, size_t size) = 0;
        virtual UMC::Status SyncTask(int32_t index, void *error) = 0;

        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;

        virtual void PackAU(VVCDecoderFrame *pFrame, DPBType dpb) = 0;

        static Packer* CreatePacker(UMC::VideoAccelerator *va);

    protected:

        UMC::VideoAccelerator *m_va;
    };

    class VVCDecoderVA
        : public VVCDecoder
    {
        std::unique_ptr<Packer>    m_packer;

    public:
        VVCDecoderVA();

        UMC::Status SetParams(UMC::BaseCodecParams*) override;
        // Check for frame completeness and get decoding errors
        bool QueryFrames() override;
        UMC::Status ProcessSegment(void);
        VVCDecoderFrameInfo* FindAU();
        void InitAUs();
        void RemoveAU(VVCDecoderFrameInfo* toRemove);
        void ThreadAwake();
        mfxStatus RunThread(mfxU32 threadNumber);
        UMC::VideoAccelerator* GetVA() { return m_va; }

    private:

        void AllocateFrameData(UMC::VideoDataInfo *pInfo, UMC::FrameMemID, VVCDecoderFrame *pFrame) override;

        int32_t GetFreeIndex(UMC::FrameMemID) const;
        // Pass picture to driver
        virtual UMC::Status Submit(VVCDecoderFrame *pFrame) override;

    protected:
        class ReportItem
        {
        public:
            uint32_t  m_index;
            uint32_t  m_field;
            uint8_t   m_status;

            ReportItem(uint32_t index, uint8_t status)
                : m_index(index)
                , m_field(0)
                , m_status(status)
            {
            }

            bool operator == (const ReportItem & item)
            {
                return (item.m_index == m_index);
            }

            bool operator != (const ReportItem & item)
            {
                return (item.m_index != m_index);
            }
        };

        typedef std::vector<ReportItem> Report;
        Report m_reports;
        mfx::ResettableTimerMs  timer;
        UMC::VideoAccelerator* m_va;
    };
}

#endif // __UMC_VVC_DECODER_VA_H
#endif // MFX_ENABLE_VVC_VIDEO_DECODE
