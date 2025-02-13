// Copyright (c) 2013-2024 Intel Corporation
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

#include "umc_h265_bitstream_headers.h"
#include "umc_h265_va_supplier.h"
#include "umc_h265_frame_list.h"

#include "umc_h265_dec_defs.h"

#include "umc_h265_task_broker.h"
#include "umc_structures.h"

#include "umc_h265_debug.h"

#include "mfx_umc_alloc_wrapper.h"
#include "mfx_common_int.h"
#include "mfx_ext_buffers.h"

#include "libmfx_core_vaapi.h"

namespace UMC_HEVC_DECODER
{

VATaskSupplier::VATaskSupplier()
    : m_bufferedFrameNumber(0)
    , m_drcFrameWidth(0)
    , m_drcFrameHeight(0)
{
}

UMC::Status VATaskSupplier::Init(UMC::VideoDecoderParams *pInit)
{
    SetVideoHardwareAccelerator(pInit->pVideoAccelerator);
    m_pMemoryAllocator = pInit->lpMemoryAllocator;

    pInit->numThreads = 1;

    UMC::Status umsRes = TaskSupplier_H265::Init(pInit);
    if (umsRes != UMC::UMC_OK)
        return umsRes;
    m_iThreadNum = 1;

    DXVASupport<VATaskSupplier>::Init();

    if (m_va)
    {
        m_DPBSizeEx = m_iThreadNum + pInit->info.bitrate;
    }

    m_sei_messages = new SEI_Storer_H265();
    m_sei_messages->Init();

    return UMC::UMC_OK;
}

void VATaskSupplier::CreateTaskBroker()
{
    m_pTaskBroker = new TaskBrokerSingleThreadDXVA(this);

    for (uint32_t i = 0; i < m_iThreadNum; i += 1)
    {
        m_pSegmentDecoder[i] = new H265_DXVA_SegmentDecoder(this);
    }
}

mfxStatus VATaskSupplier::ChangeVideoDecodingSpeed(int32_t& /* num */)
{
    MFX_RETURN(MFX_ERR_UNSUPPORTED);
}

void VATaskSupplier::SetBufferedFramesNumber(uint32_t buffered)
{
    m_DPBSizeEx = 1 + buffered;
    m_bufferedFrameNumber = buffered;
}

H265DecoderFrame * VATaskSupplier::GetFrameToDisplayInternal(bool force)
{
    //ViewItem_H265 &view = *GetView();
    //view.maxDecFrameBuffering += m_bufferedFrameNumber;

    H265DecoderFrame * frame = MFXTaskSupplier_H265::GetFrameToDisplayInternal(force);

    //view.maxDecFrameBuffering -= m_bufferedFrameNumber;

    return frame;
}

void VATaskSupplier::Reset()
{
    if (m_pTaskBroker)
        m_pTaskBroker->Reset();

    MFXTaskSupplier_H265::Reset();
}

inline bool isFreeFrame(H265DecoderFrame * pTmp)
{
    return (!pTmp->m_isShortTermRef &&
        !pTmp->m_isLongTermRef
        //((pTmp->m_wasOutputted != 0) || (pTmp->m_Flags.isDisplayable == 0)) &&
        //!pTmp->m_BusyState
        );
}

void VATaskSupplier::CompleteFrame(H265DecoderFrame * pFrame)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);
    if (!pFrame)
        return;

    if (pFrame->GetAU()->GetStatus() > H265DecoderFrameInfo::STATUS_NOT_FILLED)
        return;

    MFXTaskSupplier_H265::CompleteFrame(pFrame);

    if (H265DecoderFrameInfo::STATUS_FILLED != pFrame->GetAU()->GetStatus())
        return;

    StartDecodingFrame(pFrame);
    EndDecodingFrame();
}

void VATaskSupplier::InitFrameCounter(H265DecoderFrame * pFrame, const H265Slice *pSlice)
{
    TaskSupplier_H265::InitFrameCounter(pFrame, pSlice);
}

UMC::Status VATaskSupplier::AllocateFrameData(H265DecoderFrame * pFrame, mfxSize dimensions, const H265SeqParamSet* pSeqParamSet, const H265PicParamSet *)
{
    UMC::ColorFormat chroma_format_idc = pFrame->GetColorFormat();
    UMC::VideoDataInfo info;
    int32_t bit_depth = pSeqParamSet->need16bitOutput ? 10 : 8;
    info.Init(dimensions.width, dimensions.height, chroma_format_idc, bit_depth);

    UMC::FrameMemID frmMID;
    UMC::Status sts = m_pFrameAllocator->Alloc(&frmMID, &info, mfx_UMC_ReallocAllowed);

    if (sts == UMC::UMC_ERR_ALLOC)
        return UMC::UMC_ERR_ALLOC;

    UMC::FrameData frmData;
    frmData.Init(&info, frmMID, m_pFrameAllocator);

    auto frame_source = dynamic_cast<SurfaceSource*>(m_pFrameAllocator);
    if (sts != UMC::UMC_OK)
    {
        throw h265_exception(UMC::UMC_ERR_ALLOC);
    }

    if (frame_source)
    {
        mfxFrameSurface1* surface =
            frame_source->GetSurfaceByIndex(frmMID);
        if (!surface)
            throw h265_exception(UMC::UMC_ERR_ALLOC);

        if (m_drcFrameWidth && m_drcFrameHeight && (m_drcFrameWidth != surface->Info.Width|| m_drcFrameHeight != surface->Info.Height))
        {
            surface->Info.Width = mfx::align2_value(m_drcFrameWidth, 16);
            surface->Info.Height = mfx::align2_value(m_drcFrameHeight, 16);
            VAAPIVideoCORE_VPL* vaapi_core_vpl = reinterpret_cast<VAAPIVideoCORE_VPL*>(m_pCore->QueryCoreInterface(MFXIVAAPIVideoCORE_VPL_GUID));
            MFX_CHECK_NULL_PTR1(vaapi_core_vpl);
            vaapi_core_vpl->ReallocFrame(surface);
        }

#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
        mfxExtBuffer* extbuf =
            GetExtendedBuffer(surface->Data.ExtParam, surface->Data.NumExtParam, MFX_EXTBUFF_GPU_HANG);
        if (extbuf)
            frmData.SetAuxInfo(extbuf, extbuf->BufferSz, extbuf->BufferId);
#endif
    }

    pFrame->allocate(&frmData, &info);
    pFrame->m_index = frmMID;

    return UMC::UMC_OK;
}

H265Slice * VATaskSupplier::DecodeSliceHeader(UMC::MediaDataEx *nalUnit)
{
    size_t dataSize = nalUnit->GetDataSize();
    nalUnit->SetDataSize(std::min<size_t>(
        DEFAULT_MAX_ENETRY_POINT_NUM * 4 + DEFAULT_MAX_PREVENTION_BYTES +
        DEAFULT_MAX_SLICE_HEADER_SIZE, dataSize));

    H265Slice * slice = TaskSupplier_H265::DecodeSliceHeader(nalUnit);

    nalUnit->SetDataSize(dataSize);

    if (!slice)
        return 0;

    if (nalUnit->GetFlags() & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME)
    {
        slice->m_source.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);
        MFX_INTERNAL_CPY(slice->m_source.GetPointer(), nalUnit->GetDataPointer(), (uint32_t)nalUnit->GetDataSize());
        memset(slice->m_source.GetPointer() + nalUnit->GetDataSize(), DEFAULT_NU_TAIL_VALUE, DEFAULT_NU_TAIL_SIZE);
        slice->m_source.SetDataSize(nalUnit->GetDataSize());
        slice->m_source.SetTime(nalUnit->GetTime());
    }
    else
    {
        slice->m_source.SetData(nalUnit);
    }

    uint32_t* pbs;
    uint32_t bitOffset;

    slice->GetBitStream()->GetState(&pbs, &bitOffset);

    size_t bytes = slice->GetBitStream()->BytesDecodedRoundOff();

    slice->GetBitStream()->Reset(slice->m_source.GetPointer(), bitOffset,
        (uint32_t)slice->m_source.GetDataSize());
    slice->GetBitStream()->SetState((uint32_t*)(slice->m_source.GetPointer() + bytes), bitOffset);


    return slice;
}

} // namespace UMC_HEVC_DECODER

#endif // MFX_ENABLE_H265_VIDEO_DECODE
