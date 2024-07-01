// Copyright (c) 2022-2023 Intel Corporation
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

#ifndef __UMC_VVC_FRAME_H__
#define __UMC_VVC_FRAME_H__

#include <memory>

#include "umc_frame_data.h"
#include "mfx_common_decode_int.h"
#include "umc_vvc_dec_defs.h"

namespace UMC_VVC_DECODER
{
    class VVCSlice;
    class VVCDecoderFrame;

    class VVCDecoderFrameInfo
    {
    public:
        enum FillnessStatus
        {
            STATUS_NONE,
            STATUS_NOT_FILLED,
            STATUS_FILLED,
            STATUS_COMPLETED,
            STATUS_STARTED
        };
        VVCDecoderFrameInfo(VVCDecoderFrame &);
        ~VVCDecoderFrameInfo();

        // Reset frame structure before reusing frame
        void Reset();

        // Add a new slice into a set of frame slices
        void AddSlice(VVCSlice* slice);

        size_t GetSliceCount() const
        { return m_sliceQueue.size(); }

        VVCSlice* GetSlice(size_t num) const
        {
            return num < m_sliceQueue.size() ? m_sliceQueue[num] : nullptr;
        }

        // Set forward and backward reference frames
        void UpdateReferences(const DPBType &);
        // Clear all references to other frames
        void FreeReferenceFrames();

        bool CheckReferenceFrameError();

        bool IsCompleted() const
        {
            if (GetStatus() == VVCDecoderFrameInfo::STATUS_COMPLETED)
                return true;

            return false;
        }

        void SetFilled()
        { m_isFilled = true; }

        bool IsFilled () const
        { return m_isFilled; }

        FillnessStatus GetStatus() const
        {
            return m_Status;
        }
        void SetStatus(FillnessStatus status)
        {
            m_Status = status;
        }

        VVCDecoderFrameInfo* GetNextAU() const { return m_nextAU; }
        VVCDecoderFrameInfo* GetPrevAU() const { return m_prevAU; }
        VVCDecoderFrameInfo* GetRefAU() const { return m_refAU; }

        void SetNextAU(VVCDecoderFrameInfo* au) { m_nextAU = au; }
        void SetPrevAU(VVCDecoderFrameInfo* au) { m_prevAU = au; }
        void SetRefAU(VVCDecoderFrameInfo* au) { m_refAU = au; }
        bool IsReference() const;

        bool                                  m_IsIDR;
        VVCDecoderFrame                       &m_frame;
        uint32_t                              m_frameBeforeIDR;

    private:
        bool                                  m_isFilled;
        std::vector<VVCSlice*>                m_sliceQueue;
        FillnessStatus                        m_Status;
        bool                                  m_isIntraAU;

        VVCDecoderFrameInfo                   *m_nextAU;
        VVCDecoderFrameInfo                   *m_prevAU;
        VVCDecoderFrameInfo                   *m_refAU;
    };

    class VVCDecoderFrame : public RefCounter
    {
    public:

        VVCDecoderFrame();
        ~VVCDecoderFrame();

        void Reset();
        void Release();
        void Init(const UMC::VideoDataInfo* info);
        void Allocate(UMC::FrameData const* fd, const UMC::VideoDataInfo* info);
        void Deallocate();

        bool Empty() const
        { return !(m_data->m_locked); }
        bool IsDecoded() const
        { return m_decoded; }

        UMC::FrameData* GetFrameData()
        { return m_data.get(); }
        UMC::FrameData const* GetFrameData() const
        { return m_data.get(); }

        // Wrapper for getting 'surface Index' FrameMID
        UMC::FrameMemID GetMemID() const
        {
            return m_data->GetFrameMID();
        }

        int32_t GetVVCColorFormat(UMC::ColorFormat color_format);

        int32_t GetError() const
        { return m_error; }

        void AddError(int32_t e)
        { m_error |= e; }

        void SetError(VVCDecoderFrame& frame, uint8_t status);

        void SetErrorFlagged(int32_t errorType)
        {
            m_error |= errorType;
        }

        // Returns access unit: frame
        VVCDecoderFrameInfo * GetAU()
        { return &m_slicesInfo; }

        // Returns access unit: frame
        const VVCDecoderFrameInfo * GetAU() const
        { return &m_slicesInfo; }

        bool        IsDisplayable() const { return m_isDisplayable != 0; }

        void        SetisDisplayable(bool isDisplayable)
        {
            m_isDisplayable = isDisplayable ? 1 : 0;
        }

        bool IsDisplayed() const
        { return m_displayed; }
        void SetDisplayed()
        { m_displayed = true; }

        bool IsOutputted() const
        { return m_outputted; }
        void SetOutputted()
        { m_outputted = true; }

        bool isDisposable() const
        {
            return (!m_isShortTermRef &&
                    !m_isLongTermRef &&
                    ((m_outputted != 0 && m_displayed != 0) || (m_isDisplayable == 0)) &&
                    !GetRefCounter());
        }

        bool isInDisplayingStage() const
        {
            return (m_isDisplayable && m_outputted && !m_displayed);
        }

        bool isAlmostDisposable() const
        {
            return (!m_isShortTermRef &&
                    !m_isLongTermRef &&
                    ((m_outputted != 0) || (m_isDisplayable == 0)) &&
                    !GetRefCounter());
        }

        bool ShowAsExisting() const
        {
            return show_as_existing;
        }
        void ShowAsExisting(bool show)
        {
            show_as_existing = show;
        }

        VVCDecoderFrameInfo GetSliceInfo()
        {
            return m_slicesInfo;
        }

        bool IsDecodingStarted() const
        { return m_decodingStarted; }
        void StartDecoding()
        { m_decodingStarted = true; }

        bool IsDecodingCompleted() const
        { return m_decodingCompleted; }

        // Flag frame as completely decoded
        void CompleteDecoding();

        const VVCReferencePictureList* GetRefPicList(int32_t sliceNumber, int32_t list);

        // Check reference frames for error status and flag current frame if error is found
        void UpdateErrorWithRefFrameStatus();
        bool CheckReferenceFrameError();

        bool IsReadyToBeOutputted() const
        { return m_reordered; }
        void SetReadyToBeOutputted()
        { m_reordered = true; }

        bool IsFullFrame() const
        { return m_isFull;}
        void SetFullFrame(bool IsFull)
        { m_isFull = IsFull; }

        typedef std::list<RefCounter*>  ReferenceList;
        ReferenceList m_references;

        // Add target frame to the list of reference frames
        void AddReferenceFrame(VVCDecoderFrame* frm);

        // Clear all references to other frames
        void FreeReferenceFrames();

        // Clean up data after decoding is done
        void FreeResources();

        // Sets reference frames
        void UpdateReferenceList(const DPBType &dpb);

        // Delete unneeded references and set flags after decoding is done
        void OnDecodingCompleted();

        bool isShortTermRef() const
        {
            return m_isShortTermRef;
        }

        // Mark frame as short term reference frame
        void SetisShortTermRef(bool isRef);

        int32_t PicOrderCnt() const
        {
            return m_PicOrderCnt;
        }
        void setPicOrderCnt(int32_t PicOrderCnt)
        {
            m_PicOrderCnt = PicOrderCnt;
        }

        bool isLongTermRef() const
        {
            return m_isLongTermRef;
        }

        // Mark frame as long term reference frame
        void SetisLongTermRef(bool isRef);

        void IncreaseRefPicListResetCount()
        {
            m_RefPicListResetCount++;
        }

        void InitRefPicListResetCount()
        {
            m_RefPicListResetCount = 0;
        }

        int32_t RefPicListResetCount() const
        {
            return m_RefPicListResetCount;
        }

        bool IsRef() const
        { return m_isRef; }
        // Mark frame as a reference frame
        void SetAsRef(bool ref)
        {
            if (ref)
            {
                if (!IsRef())
                {
                    IncrementReference();
                }
                m_isRef = true;
            }
            else
            {
                bool wasRef = IsRef();
                m_isRef = false;

                if (wasRef)
                {
                    DecrementReference();
                }
            }
        }

        // Skip frame API
        void SetSkipped(bool skipped)
        {
            m_isSkipped = skipped;
        }

        bool IsSkipped() const
        {
            return m_isSkipped;
        }

        bool isInter()
        {
            return m_isInter;
        }
        void setAsInter(bool inter)
        {
            m_isInter = inter;
        }

        bool isUnavailableRefFrame()
        {
            return m_isURF;
        }
        void setAsUnavailableRef(bool urf)
        {
            m_isURF = urf;
        }
        bool isReferenced()
        {
            return m_referenced;
        }
        void setAsReferenced(bool referenced)
        {
            m_referenced = referenced;
        }

        void AddSlice(VVCSlice* pSlice);

    public:
        int32_t                           m_PicOrderCnt;    // Display order picture count mod MAX_PIC_ORDER_CNT.
        int32_t                           m_frameOrder;
        uint32_t                          m_decOrder;
        uint32_t                          m_displayOrder;
        bool                              prepared;
        uint32_t                          layerId;

        virtual void Free();

        DisplayPictureStruct              m_displayPictureStruct;
        int32_t                           m_pictureStructure;
        double                            m_dFrameTime;
        bool                              m_isOriginalPTS;
        bool                              m_isPostProccesComplete;
        int32_t                           m_maxUIDWhenDisplayed;

        uint32_t                          m_horizontalSize;
        uint32_t                          m_verticalSize;
        uint32_t                          m_aspectWidth;
        uint32_t                          m_aspectHeight;

        UMC::FrameMemID                   m_index;
        int32_t                           m_UID;

        int32_t                           m_RefPicListResetCount;
        mfxSize                           m_lumaSize;
        mfxSize                           m_chromaSize;
        int32_t                           m_pitchLuma;
        int32_t                           m_pitchChroma;
        int32_t                           m_cropLeft;
        int32_t                           m_cropRight;
        int32_t                           m_cropTop;
        int32_t                           m_cropBottom;
        int32_t                           m_cropFlag;
        int32_t                           m_chromaFormat;
        bool                              m_isUsedAsReference;
        UMC::FrameType                    m_FrameType;
        UMC::ColorFormat                  m_colorFormat;

        PlanePtrY                         m_pYPlane;

        PlanePtrUV                        m_pUVPlane;  // for NV12 support

        PlanePtrUV                        m_pUPlane;
        PlanePtrUV                        m_pVPlane;

        bool                              m_pic_output;
        bool                              m_flushed;
        bool                              m_isShortTermRef;
        bool                              m_isLongTermRef;
        uint32_t                          m_isDPBErrorFound;
        bool                              m_isFull;
        bool                              m_decFlush;
        uint32_t                          m_ppsID;

    private:

        std::unique_ptr<UMC::FrameData>   m_data;
        uint16_t                          m_locked;

        bool                              m_decodingStarted;
        bool                              m_decodingCompleted;
        bool                              m_reordered; // Can frame already be returned to application or not
        bool                              m_outputted; // set in [application thread] when frame is mapped to respective output mfxFrameSurface
        bool                              m_isDisplayable;
        bool                              m_displayed; // set in [scheduler thread] when frame decoding is finished and
                                                       // respective mfxFrameSurface prepared for output to application
        bool                              m_decoded;   // set in [application thread] to signal that frame is completed and respective reference counter decremented
                                                       // after it frame still may remain in [VVCDecoder::dpb], but only as reference

        bool                              m_isRef;     // Is reference frame
        bool                              m_isSkipped; // Is frame skipped
        int32_t                           m_error;     // error flags
        bool                              show_as_existing;
        bool                              m_referenced;
        bool                              m_isInter;
        bool                              m_isURF;     // flag to indicate if it is an unavailable reference frame

        VVCDecoderFrameInfo               m_slicesInfo;
    };
}

#endif // __UMC_VVC_FRAME_H__
#endif // MFX_ENABLE_VVC_VIDEO_DECODE
