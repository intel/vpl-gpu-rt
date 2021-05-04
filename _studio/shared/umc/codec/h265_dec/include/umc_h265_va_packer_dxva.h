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

#ifndef __UMC_H265_VA_PACKER_DXVA_H
#define __UMC_H265_VA_PACKER_DXVA_H

#include "umc_h265_va_packer.h"

#ifndef UMC_RESTRICTED_CODE_VA

#ifdef UMC_VA_DXVA

#include "umc_h265_task_supplier.h"

namespace UMC_HEVC_DECODER
{
    inline int
    LengthInMinCb(int length, int cbsize)
    {
        return length/(1 << cbsize);
    }

    class PackerDXVA2
        : public Packer
    {

    public:

        PackerDXVA2(UMC::VideoAccelerator * va);

        virtual UMC::Status GetStatusReport(void * pStatusReport, size_t size);
        virtual UMC::Status SyncTask(int32_t index, void * error);
        virtual bool IsGPUSyncEventEnable();

        virtual void BeginFrame(H265DecoderFrame*);
        virtual void EndFrame();

        void PackAU(H265DecoderFrame const*, TaskSupplier_H265*);

        using Packer::PackQmatrix;

        virtual void PackSubsets(H265DecoderFrame const*);

    protected:

        void PackQmatrix(H265Slice const*);

    protected:

        uint32_t              m_statusReportFeedbackCounter;
        int                   m_refFrameListCacheSize;
    };
}

#endif // UMC_VA_DXVA

#endif // UMC_RESTRICTED_CODE_VA

#endif /* __UMC_H265_VA_PACKER_DXVA_H */
#endif // MFX_ENABLE_H265_VIDEO_DECODE