// Copyright (c) 2019-2020 Intel Corporation
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

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_va_base.h"


#include "umc_va_linux.h"
#include "umc_h265_va_packer_vaapi.h"
#include "umc_h265_task_supplier.h"
#include "umc_va_video_processing.h"

#include <va/va_dec_hevc.h>

#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
#include "mfx_ext_buffers.h"
#include "vaapi_ext_interface.h"
#endif
#include "mfx_unified_h265d_logging.h"

namespace UMC_HEVC_DECODER
{
    void PackerVAAPI::BeginFrame(H265DecoderFrame* frame)
    {
#if !defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
        (void)frame;
#else
        auto fd = frame->GetFrameData();
        assert(fd);

        auto aux = fd->GetAuxInfo(MFX_EXTBUFF_GPU_HANG);
        if (aux)
        {
            assert(aux->type == MFX_EXTBUFF_GPU_HANG);

            auto ht = reinterpret_cast<mfxExtIntGPUHang*>(aux->ptr);
            assert(ht && "Buffer pointer should be valid here");
            if (!ht)
                throw h265_exception(UMC::UMC_ERR_FAILED);

            //clear trigger to ensure GPU hang fired only once for this frame
            fd->ClearAuxInfo(aux->type);

            UMC::UMCVACompBuffer* buffer = nullptr;
            m_va->GetCompBuffer(VATriggerCodecHangBufferType, &buffer, sizeof(unsigned int));
            if (buffer)
            {
                auto trigger =
                    reinterpret_cast<unsigned int*>(buffer->GetPtr());
                if (!trigger)
                    throw h265_exception(UMC::UMC_ERR_FAILED);

                *trigger = 1;
            }
        }
#endif
    }

    void PackerVAAPI::PackQmatrix(H265Slice const* slice)
    {
        VAIQMatrixBufferHEVC* qmatrix = nullptr;
        GetParamsBuffer(m_va, &qmatrix);

        auto pps = slice->GetPicParam();
        assert(pps);
        auto sps = slice->GetSeqParam();
        assert(sps);

        H265ScalingList const* scalingList = nullptr;

        if (pps->pps_scaling_list_data_present_flag)
        {
            scalingList = slice->GetPicParam()->getScalingList();
        }
        else if (sps->sps_scaling_list_data_present_flag)
        {
            scalingList = slice->GetSeqParam()->getScalingList();
        }
        else
        {
            // build default scaling list in target buffer location
            static bool doInit = true;
            static H265ScalingList sl{};

            if (doInit)
            {
                for(uint32_t sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
                {
                    for(uint32_t listId = 0; listId < g_scalingListNum[sizeId]; listId++)
                    {
                        const int *src = getDefaultScalingList(sizeId, listId);
                              int *dst = sl.getScalingListAddress(sizeId, listId);
                        int count = std::min<int>(MAX_MATRIX_COEF_NUM, (int32_t)g_scalingListSize[sizeId]);
                        ::MFX_INTERNAL_CPY(dst, src, sizeof(int32_t) * count);
                        sl.setScalingListDC(sizeId, listId, SCALING_LIST_DC);
                    }
                }

                doInit = false;
            }

            scalingList = &sl;
        }

        initQMatrix<16>(scalingList, SCALING_LIST_4x4,   qmatrix->ScalingList4x4);    // 4x4
        initQMatrix<64>(scalingList, SCALING_LIST_8x8,   qmatrix->ScalingList8x8);    // 8x8
        initQMatrix<64>(scalingList, SCALING_LIST_16x16, qmatrix->ScalingList16x16);  // 16x16
        initQMatrix(scalingList, SCALING_LIST_32x32, qmatrix->ScalingList32x32);      // 32x32

        for (int sizeId = SCALING_LIST_16x16; sizeId <= SCALING_LIST_32x32; sizeId++)
        {
            for(unsigned listId = 0; listId <  g_scalingListNum[sizeId]; listId++)
            {
                if(sizeId == SCALING_LIST_16x16)
                    qmatrix->ScalingListDC16x16[listId] = (uint8_t)scalingList->getScalingListDC(sizeId, listId);
                else if(sizeId == SCALING_LIST_32x32)
                    qmatrix->ScalingListDC32x32[listId] = (uint8_t)scalingList->getScalingListDC(sizeId, listId);
            }
        }
        TRACE_BUFFER_EVENT(VA_TRACE_API_HEVC_QMATRIXARAMETER_TASK, EVENT_TYPE_INFO, TR_KEY_DECODE_QMATRIX,
            qmatrix, H265DecodeQmatrixParam, Qmatrix_HEVC);
    }
    void PackerVAAPI::PackProcessingInfo(H265DecoderFrameInfo * sliceInfo)
    {
        UMC::VideoProcessingVA *vpVA = m_va->GetVideoProcessingVA();
        if (!vpVA)
            throw h265_exception(UMC::UMC_ERR_FAILED);

        UMC::UMCVACompBuffer *pipelineVABuf;
        auto* pipelineBuf = reinterpret_cast<VAProcPipelineParameterBuffer *>(m_va->GetCompBuffer(VAProcPipelineParameterBufferType, &pipelineVABuf, sizeof(VAProcPipelineParameterBuffer)));
        if (!pipelineBuf)
            throw h265_exception(UMC::UMC_ERR_FAILED);
        pipelineVABuf->SetDataSize(sizeof(VAProcPipelineParameterBuffer));

        MFX_INTERNAL_CPY(pipelineBuf, &vpVA->m_pipelineParams, sizeof(VAProcPipelineParameterBuffer));

        pipelineBuf->surface = m_va->GetSurfaceID(sliceInfo->m_pFrame->m_index); // should filled in packer
        pipelineBuf->additional_outputs = (VASurfaceID*)vpVA->GetCurrentOutputSurface();
        // To keep output aligned, decode downsampling use this fixed combination of chroma sitting type
        pipelineBuf->input_color_properties.chroma_sample_location = VA_CHROMA_SITING_HORIZONTAL_LEFT | VA_CHROMA_SITING_VERTICAL_CENTER;
    }

    void PackerVAAPI::PackAU(H265DecoderFrame const* frame, TaskSupplier_H265 * supplier)
    {
        auto fi = frame->GetAU();
        if (!fi)
            throw h265_exception(UMC::UMC_ERR_FAILED);

        auto count = fi->GetSliceCount();
        if (!count)
            return;

        auto slice = fi->GetSlice(0);
        if (!slice)
            return;

        auto pps = slice->GetPicParam();
        auto sps = slice->GetSeqParam();
        if (!sps || !pps)
            throw h265_exception(UMC::UMC_ERR_FAILED);

        PackPicParams(frame, supplier);
        if (sps->scaling_list_enabled_flag)
        {
            PackQmatrix(slice);
        }

        CreateSliceParamBuffer(count);
        CreateSliceDataBuffer(m_va, fi);

        for (size_t n = 0; n < count; n++)
            PackSliceParams(fi->GetSlice(int32_t(n)), n, n == count - 1);
#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
        if (m_va->GetVideoProcessingVA())
            PackProcessingInfo(fi);
#endif
        auto s = m_va->Execute();
        if (s != UMC::UMC_OK)
            throw h265_exception(s);
    }

} // namespace UMC_HEVC_DECODER

namespace UMC_HEVC_DECODER
{
    Packer* CreatePackerVAAPI(UMC::VideoAccelerator* va)
    {
        return new G12::PackerVAAPI(va);
    }
}


#endif //  UMC_RESTRICTED_CODE_VA

#endif //MFX_ENABLE_H265_VIDEO_DECODE
