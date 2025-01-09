// Copyright (c) 2022-2025 Intel Corporation
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

#include <limits>
#include <algorithm>

#include "umc_vvc_frame.h"
#include "umc_frame_data.h"
#include "umc_vvc_dec_defs.h"
#include "umc_vvc_slice_decoding.h"


namespace UMC_VVC_DECODER
{
    VVCDecoderFrameInfo::VVCDecoderFrameInfo(VVCDecoderFrame &Frame)
        : m_IsIDR(0)
        , m_frame(Frame)
        , m_frameBeforeIDR(0)
        , m_isFilled(false)
        , m_Status()
        , m_isIntraAU(0)
        , m_nextAU(0)
        , m_prevAU(0)
        , m_refAU(0)
    {
    }

    VVCDecoderFrameInfo::~VVCDecoderFrameInfo()
    {
        std::for_each(m_sliceQueue.begin(), m_sliceQueue.end(),
            std::default_delete<VVCSlice>()
        );
    }

    void VVCDecoderFrameInfo::Reset()
    {
        m_isFilled = false;

        std::for_each(m_sliceQueue.begin(), m_sliceQueue.end(),
            std::default_delete<VVCSlice>()
        );

        m_sliceQueue.resize(0);
        FreeReferenceFrames();
    }

    void VVCDecoderFrameInfo::AddSlice(VVCSlice *slice)
    {
        if (0 == m_sliceQueue.size()) // on the first slice
        {
        // ...
        }

        m_sliceQueue.push_back(slice);
        const VVCSliceHeader* sliceHeader = slice->GetSliceHeader();
        m_isIntraAU = m_isIntraAU && (sliceHeader->slice_type == I_SLICE);
        m_IsIDR = sliceHeader->IdrPicFlag != 0;
        m_frameBeforeIDR = 0;
    }


    bool VVCDecoderFrameInfo::IsReference() const
    {
        return m_frame.m_isUsedAsReference;
    }

    void VVCDecoderFrameInfo::UpdateReferences(const DPBType &dpb)
    {
        (void)(dpb); //TODO: remove
        //...
    }

    void VVCDecoderFrameInfo::FreeReferenceFrames()
    {
    // ...
    }

    bool VVCDecoderFrameInfo::CheckReferenceFrameError()
    {
        //uint32_t checkedErrorMask = UMC::ERROR_FRAME_MINOR | UMC::ERROR_FRAME_MAJOR | UMC::ERROR_FRAME_REFERENCE_FRAME;

        return false;
    }

    VVCDecoderFrame::VVCDecoderFrame()
        : m_frameOrder(0)
        , m_decOrder(0xffffffff)
        , m_displayOrder(0xffffffff)
        , m_dFrameTime(-1.0)
        , m_isOriginalPTS(false)
        , m_isPostProccesComplete(false)
        , m_horizontalSize(0)
        , m_verticalSize(0)
        , m_aspectWidth(0)
        , m_aspectHeight(0)
        , m_RefPicListResetCount(0)
        , m_pitchChroma(0)
        , m_isFull(false)
        , m_decFlush(false)
        , m_data(new UMC::FrameData{})
        , m_locked(0)
        , m_decodingStarted(false)
        , m_decodingCompleted(false)
        , m_reordered(false)
        , m_outputted(false)
        , m_displayed(false)
        , m_decoded(false)
        , m_isRef(false)
        , m_isSkipped(false)
        , m_error(0)
        , show_as_existing(false)
        , m_isInter(true)
        , m_slicesInfo(*this)
    {
        Reset();
    }

    // Add target frame to the list of reference frames
    void VVCDecoderFrame::AddReferenceFrame(VVCDecoderFrame* frm)
    {
        if (!frm || frm == this)
            return;

        if (std::find(m_references.begin(), m_references.end(), frm) != m_references.end())
            return;

        frm->IncrementReference();
        m_references.push_back(frm);
    }

    // Clear all references to other frames
    void VVCDecoderFrame::FreeReferenceFrames()
    {
        if (m_references.size())
        {
            ReferenceList::iterator iter = m_references.begin();
            ReferenceList::iterator end_iter = m_references.end();

            for (; iter != end_iter; ++iter)
            {
                RefCounter* reference = *iter;
                if (reference->GetRefCounter())
                    reference->DecrementReference();
            }

            m_references.clear();
        }
    }

    VVCDecoderFrame::~VVCDecoderFrame()
    {
        auto fd = GetFrameData();
        fd->m_locked = false;
        Reset();
        Deallocate();
    }

    void VVCDecoderFrame::Reset()
    {
        m_slicesInfo.Reset();

        ResetRefCounter();

        m_error     = 0;
        m_displayed = false;
        m_outputted = false;
        m_decoded   = false;

        m_decodingStarted   = false;
        m_decodingCompleted = false;

        m_reordered         = false;
        m_data->Close();

        FreeReferenceFrames();

        m_decOrder     = 0xffffffff;
        m_displayOrder = 0xffffffff;
        m_isFull       = false;
        m_isRef        = false;
        m_isSkipped    = false;

        m_dFrameTime = -1.0;
        m_isOriginalPTS = false;
        m_isPostProccesComplete = false;
        m_RefPicListResetCount = 0;

        m_chromaSize.height = 0;
        m_chromaSize.width = 0;
        m_chromaFormat = 0;

        m_FrameType = UMC::NONE_PICTURE;
        m_colorFormat = UMC::YV12;

        m_UID = -1;
        m_index = -1;

        m_horizontalSize = 0;
        m_verticalSize   = 0;

        m_aspectWidth  = 0;
        m_aspectHeight = 0;

        m_PicOrderCnt = 0;
        m_referenced = 0;
        m_pic_output = true;
        m_isShortTermRef = false;
        m_isDisplayable = false;
        m_isInter = true;
        m_decFlush = false;
        m_isURF = false;

        prepared = 0;
        layerId = 0;
        m_displayPictureStruct = {};
        m_pictureStructure = 0;
        m_maxUIDWhenDisplayed = 0;
        m_pitchLuma = 0;
        m_cropLeft = 0;
        m_cropRight = 0;
        m_cropTop = 0;
        m_cropBottom = 0;
        m_cropFlag = 0;
        m_isUsedAsReference = 0;
        m_isLongTermRef = 0;
        m_isDPBErrorFound = 0;
        m_ppsID = 0;

        Deallocate();
        // ...
    }

    void VVCDecoderFrame::Release()
    {
        auto fd = GetFrameData();
        fd->m_locked = false;
        m_slicesInfo.Reset();

        ResetRefCounter();

        m_error = 0;
        m_displayed = false;
        m_outputted = false;
        m_decoded = false;

        m_decodingStarted = false;
        m_decodingCompleted = false;

        m_reordered = false;
        m_data->Close();

        FreeReferenceFrames();

        m_decOrder = 0xffffffff;
        m_displayOrder = 0xffffffff;
        m_isFull = false;
        m_isRef = false;
        m_isSkipped = false;
        prepared = false;

        m_dFrameTime = -1.0;
        m_isOriginalPTS = false;
        m_isPostProccesComplete = false;
        m_RefPicListResetCount = 0;
        m_frameOrder = 0;
        m_maxUIDWhenDisplayed = 0;
        m_chromaSize.height = 0;
        m_chromaSize.width = 0;
        m_chromaFormat = 0;

        m_FrameType = UMC::NONE_PICTURE;
        m_colorFormat = UMC::YV12;

        m_UID = -1;
        m_index = -1;

        m_horizontalSize = 0;
        m_verticalSize = 0;

        m_aspectWidth = 0;
        m_aspectHeight = 0;

        m_PicOrderCnt = 0;
        m_referenced = 0;
        m_pic_output = true;
        m_isShortTermRef = false;
        m_isDisplayable = false;
        m_isInter = true;
        m_decFlush = false;
        Deallocate();
    }

    // Deallocate all memory
    void VVCDecoderFrame::Deallocate() 
    {
        if (m_data->GetFrameMID() != UMC::FRAME_MID_INVALID)
        {
            m_data->Close();
            return;
        }

        m_pYPlane = m_pUPlane = m_pVPlane = m_pUVPlane = NULL;

        m_lumaSize = { 0, 0 };
    }

    int32_t VVCDecoderFrame::GetVVCColorFormat(UMC::ColorFormat color_format)
    {
        int32_t format;
        switch (color_format)
        {
        case UMC::GRAY:
        case UMC::GRAYA:
            format = CHROMA_FORMAT_400;
            break;
        case UMC::YUV422A:
        case UMC::YUV422:
            format = CHROMA_FORMAT_422;
            break;
        case UMC::YUV444:
        case UMC::YUV444A:
            format = CHROMA_FORMAT_444;
            break;
        case UMC::YUV420:
        case UMC::YUV420A:
        case UMC::NV12:
        case UMC::YV12:
        default:
            format = CHROMA_FORMAT_420;
            break;
        }
        return format;
    }

    // Initialize variables to default values
    void VVCDecoderFrame::Init(const UMC::VideoDataInfo* info)
    {
        if (info == nullptr)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);
        if (info->GetNumPlanes() == 0)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        m_colorFormat = info->GetColorFormat();
        m_chromaFormat = GetVVCColorFormat(info->GetColorFormat());
        m_lumaSize = info->GetPlaneInfo(0)->m_ippSize;
        m_pYPlane = 0;
        m_pUPlane = 0;
        m_pVPlane = 0;
        m_pUVPlane = 0;

        if ((m_chromaFormat > 0) && (info->GetNumPlanes() >= 2))
        {
            m_chromaSize = info->GetPlaneInfo(1)->m_ippSize;
        }
        else
        {
            m_chromaSize = { 0, 0 };
        }
    }

    void VVCDecoderFrame::Allocate(UMC::FrameData const *fd, const UMC::VideoDataInfo* info)
    {
        if (!fd)
            throw vvc_exception(UMC::UMC_ERR_NULL_PTR);

        if (info == nullptr || fd == nullptr || info->GetNumPlanes() == 0)
        {
            Deallocate();
            return;
        }

        *m_data = *fd;

        if (fd->GetPlaneMemoryInfo(0)->m_planePtr)
            m_data->m_locked = true;

        m_colorFormat = info->GetColorFormat();

        m_chromaFormat = GetVVCColorFormat(info->GetColorFormat());
        m_pitchLuma = (int32_t)m_data->GetPlaneMemoryInfo(0)->m_pitch / info->GetPlaneSampleSize(0);

        m_pYPlane = (PlanePtrY)m_data->GetPlaneMemoryInfo(0)->m_planePtr;
        if ((m_chromaFormat > 0 || GetVVCColorFormat(fd->GetInfo()->GetColorFormat()) > 0) &&
            (info->GetNumPlanes() >= 2))
        {
            if (m_chromaFormat == 0)
                info = fd->GetInfo();

            if (info->GetPlaneInfo(1) == 0)
                throw vvc_exception(UMC::UMC_ERR_NULL_PTR);
            m_pitchChroma = (int32_t)m_data->GetPlaneMemoryInfo(1)->m_pitch / info->GetPlaneSampleSize(1);

            if (m_data->GetInfo()->GetNumPlanes() == 2)
            {
                m_pUVPlane = (PlanePtrUV)m_data->GetPlaneMemoryInfo(1)->m_planePtr;
                m_pUPlane = 0;
                m_pVPlane = 0;
            }
            else
            {
                assert(m_data->GetInfo()->GetNumPlanes() == 3);
                m_pUPlane = (PlanePtrUV)m_data->GetPlaneMemoryInfo(1)->m_planePtr;
                m_pVPlane = (PlanePtrUV)m_data->GetPlaneMemoryInfo(2)->m_planePtr;
                m_pUVPlane = 0;
            }
        }
        else
        {
            m_chromaSize = { 0, 0 };
            m_pitchChroma = 0;
            m_pUPlane = 0;
            m_pVPlane = 0;
        }
    }

    void VVCDecoderFrame::FreeResources()
    {
        FreeReferenceFrames();

        if (IsDecoded())
        {
            m_slicesInfo.Reset();
        }
    }

    // Free resources if possible
    void VVCDecoderFrame::Free()
    {
        if (IsDisplayed() && IsOutputted())
        {
            Reset();
        }
    }

    void VVCDecoderFrame::CompleteDecoding()
    {
        UpdateErrorWithRefFrameStatus();
        m_decodingCompleted = true;
    }

    void VVCDecoderFrame::OnDecodingCompleted()
    {
        UpdateErrorWithRefFrameStatus();
        m_decoded = true;
        FreeResources();
    }

    // Mark frame as short term reference frame
    void VVCDecoderFrame::SetisShortTermRef(bool isRef)
    {
        if (isRef)
        {
            // for DPB update
            if (!isShortTermRef() && !isLongTermRef() && !isUnavailableRefFrame())
            {
                IncrementReference();
            }

            m_isShortTermRef = true;
        }
        else
        {
            bool wasRef = isShortTermRef() != 0;

            m_isShortTermRef = false;

            // for DPB update
            if (wasRef && !isShortTermRef() && !isLongTermRef())
            {
                DecrementReference();
            }
        }
    }

    // Mark frame as long term reference frame
    void VVCDecoderFrame::SetisLongTermRef(bool isRef)
    {
        if (isRef)
        {
            if (!isShortTermRef() && !isLongTermRef() && !isUnavailableRefFrame())
            {
                IncrementReference();
            }
            m_isLongTermRef = true;
        }
        else
        {
            bool wasRef = isLongTermRef() != 0;

            m_isLongTermRef = false;

            if (wasRef && !isShortTermRef() && !isLongTermRef())
            {
                DecrementReference();
            }
        }
    }

    void VVCDecoderFrame::UpdateErrorWithRefFrameStatus()
    {
        if (m_slicesInfo.CheckReferenceFrameError())
        {
            AddError(UMC::ERROR_FRAME_REFERENCE_FRAME);
        }
    }

    void VVCDecoderFrame::SetError(VVCDecoderFrame& frame, uint8_t status)
    {
        switch (status)
        {
        case 0:
            break;
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

    void VVCDecoderFrame::AddSlice(VVCSlice* pSlice)
    {
        uint32_t iSliceNumber = (uint32_t)m_slicesInfo.GetSliceCount() + 1;

        pSlice->SetSliceNumber(iSliceNumber);
        pSlice->SetCurrentFrame(this);
        m_slicesInfo.AddSlice(pSlice);
    }
}

#endif //MFX_ENABLE_VVC_VIDEO_DECODE
