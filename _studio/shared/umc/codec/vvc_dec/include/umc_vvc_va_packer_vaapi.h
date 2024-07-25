// Copyright (c) 2023 Intel Corporation
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

#ifndef __UMC_VVC_VA_PACKER_H

#include "umc_vvc_decoder_va.h"

#include <va/va_dec_vvc.h>

#define __UMC_VVC_VA_PACKER_H


namespace UMC
{
    class MediaData;
}

namespace UMC_VVC_DECODER
{
    class VVCSlice;
    class Packer;
    class PackerVA
        : public Packer
    {
    public:
        PackerVA(UMC::VideoAccelerator * va);
        void PackAU(VVCDecoderFrame *pFrame, DPBType dpb) override;

        UMC::Status GetStatusReport(void * pStatusReport, size_t size) override;
        UMC::Status SyncTask(int32_t index, void * error) override;
        UMC::Status QueryTaskStatus(int32_t index, void * status, void * error);

        void BeginFrame() override;
        void EndFrame() override;
    private:
        void PackPicParams(VAPictureParameterBufferVVC *pPicParam, VVCSlice* pSlice, VVCDecoderFrame* pFrame, DPBType dpb);
        void PackPicParamsChromaQPTable(VAPictureParameterBufferVVC* pPicParam, const VVCSeqParamSet* pSeqParamSet);
        bool IsAPSAvailable(VVCSlice* pSlice, ApsType aps_type);
        void PackALFParams(VAAlfDataVVC*, VVCSlice*);
        void PackLMCSParams(VALmcsDataVVC* pLMCSParam, VVCSlice* pSlice);
        void PackScalingParams(VAScalingListVVC *pScalingParam, VVCSlice* pSlice);
        void PackTileParams(VVCSlice* pSlice);
        void PackSubpicParams(VVCSlice* pSlice);
        void PackSliceStructParams(VASliceStructVVC* pSliceStruct, VVCSlice* pSlice);
        void PackSliceLong(VAPictureParameterBufferVVC* pPicParam, VASliceParameterBufferVVC* pSliceParam, VVCSlice* pSlice);
        void PackSliceParam(VAPictureParameterBufferVVC* pPicParam, DPBType dpb, VVCDecoderFrameInfo const* pSliceInfo);
    };
} // namespace UMC_VVC_DECODER


#endif /* __UMC_VVC_VA_PACKER_H */
#endif // MFX_ENABLE_VVC_VIDEO_DECODE
