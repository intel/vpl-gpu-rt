// Copyright (c) 2008-2024 Intel Corporation
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

#include "mfx_umc_alloc_wrapper.h"
#include "mfx_common.h"
#include "libmfx_core.h"
#include "mfx_common_int.h"
#include "mfx_common_decode_int.h"
#include <functional>

#if !defined MFX_DEC_VIDEO_POSTPROCESS_DISABLE
// For setting SFC surface
#include "umc_va_video_processing.h"
#endif



#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)
#include "mfx_vpp_jpeg.h"
#endif

mfx_UMC_MemAllocator::mfx_UMC_MemAllocator():m_pCore(NULL)
{
}

mfx_UMC_MemAllocator::~mfx_UMC_MemAllocator()
{
}

UMC::Status mfx_UMC_MemAllocator::InitMem(UMC::MemoryAllocatorParams *, VideoCORE* mfxCore)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    UMC::Status Sts = UMC::UMC_OK;
    if(!mfxCore)
        return UMC::UMC_ERR_NULL_PTR;
    m_pCore = mfxCore;
    return Sts;
}

UMC::Status mfx_UMC_MemAllocator::Close()
{
    UMC::AutomaticUMCMutex guard(m_guard);

    UMC::Status sts = UMC::UMC_OK;
    m_pCore = 0;
    return sts;
}

UMC::Status mfx_UMC_MemAllocator::Alloc(UMC::MemID *pNewMemID, size_t Size, Ipp32u , Ipp32u )
{
    UMC::AutomaticUMCMutex guard(m_guard);

    mfxMemId memId;
    mfxStatus Sts = m_pCore->AllocBuffer((mfxU32)Size, /*MFX_MEMTYPE_PERSISTENT_MEMORY*/ MFX_MEMTYPE_SYSTEM_MEMORY, &memId);
    MFX_CHECK_UMC_STS(Sts);
    *pNewMemID = ((UMC::MemID)memId + 1);
    return UMC::UMC_OK;
}

void* mfx_UMC_MemAllocator::Lock(UMC::MemID MID)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    mfxStatus Sts = MFX_ERR_NONE;

    mfxU8 *ptr;
    Sts = m_pCore->LockBuffer((mfxHDL)(MID - 1), &ptr);
    if (Sts < MFX_ERR_NONE)
        return 0;

    return ptr;
}

UMC::Status mfx_UMC_MemAllocator::Unlock(UMC::MemID MID)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    UMC::Status sts = UMC::UMC_OK;
    m_pCore->UnlockBuffer((mfxHDL)(MID - 1));
    return sts;
}

UMC::Status mfx_UMC_MemAllocator::Free(UMC::MemID MID)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    m_pCore->FreeBuffer((mfxHDL)(MID - 1));
    return UMC::UMC_OK;
}

UMC::Status mfx_UMC_MemAllocator::DeallocateMem(UMC::MemID )
{
    UMC::Status sts = UMC::UMC_OK;
    return sts;
}


////////////////////////////////////////////////////////////////////////////////////////////////
// mfx_UMC_FrameAllocator implementation
////////////////////////////////////////////////////////////////////////////////////////////////
mfx_UMC_FrameAllocator::InternalFrameData::FrameRefInfo::FrameRefInfo()
    : m_referenceCounter(0)
{
}

void mfx_UMC_FrameAllocator::InternalFrameData::FrameRefInfo::Reset()
{
    m_referenceCounter = 0;
}

bool mfx_UMC_FrameAllocator::InternalFrameData::IsValidMID(mfxU32 index) const
{
    if (index >= m_frameData.size())
        return false;

    return true;
}

mfxFrameSurface1 & mfx_UMC_FrameAllocator::InternalFrameData::GetSurface(mfxU32 index)
{
    if (!IsValidMID(index))
        throw std::exception();

    return m_frameData[index].first;
}

UMC::FrameData   & mfx_UMC_FrameAllocator::InternalFrameData::GetFrameData(mfxU32 index)
{
    if (!IsValidMID(index))
        throw std::exception();

    return m_frameData[index].second;
}

void mfx_UMC_FrameAllocator::InternalFrameData::Close()
{
    m_frameData.clear();
    m_frameDataRefs.clear();
}

void mfx_UMC_FrameAllocator::InternalFrameData::ResetFrameData(mfxU32 index)
{
    if (!IsValidMID(index))
        throw std::exception();

    m_frameDataRefs[index].Reset();
    m_frameData[index].second.Reset();
}

void mfx_UMC_FrameAllocator::InternalFrameData::Resize(mfxU32 size)
{
    m_frameData.resize(size);
    m_frameDataRefs.resize(size);
}

mfxU32 mfx_UMC_FrameAllocator::InternalFrameData::IncreaseRef(mfxU32 index)
{
    if (!IsValidMID(index))
        throw std::exception();

    FrameRefInfo * frameRef = &m_frameDataRefs[index];
    frameRef->m_referenceCounter++;
    return frameRef->m_referenceCounter;
}

mfxU32 mfx_UMC_FrameAllocator::InternalFrameData::DecreaseRef(mfxU32 index)
{
    if (!IsValidMID(index))
        throw std::exception();

    FrameRefInfo * frameRef = &m_frameDataRefs[index];
    frameRef->m_referenceCounter--;
    return frameRef->m_referenceCounter;
}

void mfx_UMC_FrameAllocator::InternalFrameData::Reset()
{
    // unlock internal surfaces
    for (mfxU32 i = 0; i < m_frameData.size(); i++)
    {
        m_frameData[i].first.Data.Locked = 0;  // if app ext allocator then should decrease Locked counter same times as locked by medisSDK
        m_frameData[i].second.Reset();
    }

    for (mfxU32 i = 0; i < m_frameDataRefs.size(); i++)
    {
        m_frameDataRefs[i].Reset();
    }
}

mfxU32 mfx_UMC_FrameAllocator::InternalFrameData::GetSize() const
{
    return (mfxU32)m_frameData.size();
}

void mfx_UMC_FrameAllocator::InternalFrameData::AddNewFrame(mfx_UMC_FrameAllocator * alloc, mfxFrameSurface1 *surface, UMC::VideoDataInfo * info)
{
    FrameRefInfo refInfo;
    m_frameDataRefs.push_back(refInfo);

    FrameInfo  frameInfo;
    m_frameData.push_back(frameInfo);

    mfxU32 index = (mfxU32)(m_frameData.size() - 1);;

    memset(&(m_frameData[index].first), 0, sizeof(m_frameData[index].first));
    m_frameData[index].first.Data.MemId = surface->Data.MemId;
    m_frameData[index].first.Info = surface->Info;

    // fill UMC frameData
    UMC::FrameData* frameData = &GetFrameData(index);

    // set correct width & height to planes
    frameData->Init(info, (UMC::FrameMemID)index, alloc);
}


mfx_UMC_FrameAllocator::mfx_UMC_FrameAllocator()
    : m_curIndex(-1)
    , m_IsUseExternalFrames(true)
    , m_sfcVideoPostProcessing(false)
    , m_surface_info()
    , m_pCore(0)
    , m_externalFramesResponse(0)
    , m_isSWDecode(false)
    , m_IOPattern(0)
{
}

mfx_UMC_FrameAllocator::~mfx_UMC_FrameAllocator()
{
    Close();
}

UMC::Status mfx_UMC_FrameAllocator::InitMfx(UMC::FrameAllocatorParams *,
                                            VideoCORE* mfxCore,
                                            const mfxVideoParam *params,
                                            const mfxFrameAllocRequest *request,
                                            mfxFrameAllocResponse *response,
                                            bool isUseExternalFrames,
                                            bool isSWplatform)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    m_isSWDecode = isSWplatform;

    if (!mfxCore || !params)
        return UMC::UMC_ERR_NULL_PTR;

    m_IOPattern = params->IOPattern;

    if (!isUseExternalFrames && (!request || !response))
        return UMC::UMC_ERR_NULL_PTR;

    m_pCore = mfxCore;
    m_IsUseExternalFrames = isUseExternalFrames;

    mfxU32 bit_depth              = BitDepthFromFourcc(params->mfx.FrameInfo.FourCC);

    UMC::ColorFormat color_format;

    switch (params->mfx.FrameInfo.FourCC)
    {
    case MFX_FOURCC_NV12:
        color_format = UMC::NV12;
        break;
    case MFX_FOURCC_P010:
        color_format = UMC::NV12;
        break;
    case MFX_FOURCC_NV16:
        color_format = UMC::NV16;
        break;
    case MFX_FOURCC_P210:
        color_format = UMC::NV16;
        break;
    case MFX_FOURCC_RGB4:
        color_format = UMC::RGB32;
        break;
    case MFX_FOURCC_YV12:
        color_format = UMC::YUV420;
        break;
    case MFX_FOURCC_YUY2:
        color_format = UMC::YUY2;
        break;
    case MFX_FOURCC_UYVY:
        color_format = UMC::UYVY;
        break;
    case MFX_FOURCC_AYUV:
        color_format = UMC::AYUV;
        break;
    case MFX_FOURCC_Y210:
        color_format = UMC::Y210;
        break;
    case MFX_FOURCC_Y410:
        color_format = UMC::Y410;
        break;
    case MFX_FOURCC_P016:
        color_format = UMC::P016;
        break;
    case MFX_FOURCC_Y216:
        color_format = UMC::Y216;
        break;
    case MFX_FOURCC_Y416:
        color_format = UMC::Y416;
        break;
    case MFX_FOURCC_RGBP:
    case MFX_FOURCC_BGRP:
    case MFX_FOURCC_YUV444:
        color_format = UMC::YUV444;
        break;
    case MFX_FOURCC_YUV411:
        color_format = UMC::YUV411;
        break;
    case MFX_FOURCC_YUV400:
        color_format = UMC::GRAY;
        break;
    case MFX_FOURCC_YUV422H:
    case MFX_FOURCC_YUV422V:
        color_format = UMC::YUV422;
        break;
    case MFX_FOURCC_IMC3:
        color_format = UMC::IMC3;
        break;
    default:
        return UMC::UMC_ERR_UNSUPPORTED;
    }

    UMC::Status umcSts = m_info.Init(request->Info.Width, request->Info.Height, color_format, bit_depth);

    m_surface_info = request->Info;

    if (umcSts != UMC::UMC_OK)
        return umcSts;

    if (!m_IsUseExternalFrames || !m_isSWDecode)
    {
        m_frameDataInternal.Resize(response->NumFrameActual);
        m_extSurfaces.resize(response->NumFrameActual);

        for (mfxU32 i = 0; i < response->NumFrameActual; i++)
        {
            mfxFrameSurface1 & surface = m_frameDataInternal.GetSurface(i);
            surface.Data.MemId   = response->mids[i];
            surface.Data.MemType = request->Type;
            surface.Info         = request->Info;

            // fill UMC frameData
            UMC::FrameData& frameData = m_frameDataInternal.GetFrameData(i);

            // set correct width & height to planes
            frameData.Init(&m_info, (UMC::FrameMemID)i, this);
        }
    }
    else
    {
        m_extSurfaces.reserve(response->NumFrameActual);
    }

    return UMC::UMC_OK;
}


UMC::Status mfx_UMC_FrameAllocator::Close()
{
    UMC::AutomaticUMCMutex guard(m_guard);

    Reset();
    m_frameDataInternal.Close();
    m_extSurfaces.clear();
    return UMC::UMC_OK;
}

void mfx_UMC_FrameAllocator::SetExternalFramesResponse(mfxFrameAllocResponse *response)
{
    m_externalFramesResponse = 0;

    if (!response || (!m_pCore->IsSupportedDelayAlloc() && !response->NumFrameActual))
        return;

    m_externalFramesResponse = response;
}

UMC::Status mfx_UMC_FrameAllocator::Reset()
{
    UMC::AutomaticUMCMutex guard(m_guard);

    m_curIndex = -1;
    mfxStatus sts = MFX_ERR_NONE;

    m_frameDataInternal.Reset();

    // free external surfaces
    for (mfxU32 i = 0; i < m_extSurfaces.size(); i++)
    {
        if (m_extSurfaces[i].isUsed)
        {
            sts = m_pCore->DecreaseReference(&m_extSurfaces[i].FrameSurface->Data);
            if (sts < MFX_ERR_NONE)
                return UMC::UMC_ERR_FAILED;
            m_extSurfaces[i].isUsed = false;
        }

        m_extSurfaces[i].FrameSurface = 0;
    }

    if (m_IsUseExternalFrames && m_isSWDecode)
    {
        m_extSurfaces.clear();
        m_frameDataInternal.Close();
    }

    return UMC::UMC_OK;
}

UMC::Status mfx_UMC_FrameAllocator::GetFrameHandle(UMC::FrameMemID memId, void * handle)
{
    if (m_pCore->GetFrameHDL(ConvertMemId(memId), (mfxHDL*)handle) != MFX_ERR_NONE)
        return UMC::UMC_ERR_ALLOC;

    return UMC::UMC_OK;
}

static mfxStatus SetSurfaceForSFC(VideoCORE& core, mfxFrameSurface1& surf)
{
#if !defined MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    // Set surface for SFC
    UMC::VideoAccelerator * va = nullptr;

    core.GetVA((mfxHDL*)&va, MFX_MEMTYPE_FROM_DECODE);
    MFX_CHECK_HDL(va);

    auto video_processing_va = va->GetVideoProcessingVA();

    if (video_processing_va && core.GetVAType() == MFX_HW_VAAPI)
    {
        mfxHDLPair surfHDLpair = {};
        MFX_SAFE_CALL(core.GetExternalFrameHDL(surf, surfHDLpair, false));

        video_processing_va->SetOutputSurface(surfHDLpair.first);
    }
#else
    std::ignore = core;
    std::ignore = surf;
#endif

    return MFX_ERR_NONE;
}

UMC::Status mfx_UMC_FrameAllocator::Alloc(UMC::FrameMemID *pNewMemID, const UMC::VideoDataInfo * info, uint32_t a_flags)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    mfxStatus sts = MFX_ERR_NONE;
    if (!pNewMemID)
        return UMC::UMC_ERR_NULL_PTR;

    mfxI32 index = FindFreeSurface();
    if (index == -1)
    {
        *pNewMemID = UMC::FRAME_MID_INVALID;
        return UMC::UMC_ERR_ALLOC;
    }

    *pNewMemID = (UMC::FrameMemID)index;

    mfxFrameInfo &surfInfo = m_frameDataInternal.GetSurface(index).Info;

    IppiSize allocated = { surfInfo.Width, surfInfo.Height};
    IppiSize passed = {static_cast<int>(info->GetWidth()), static_cast<int>(info->GetHeight())};
    UMC::ColorFormat colorFormat = m_info.GetColorFormat();

    switch(colorFormat)
    {
    case UMC::YUV420:
    case UMC::GRAY:
    case UMC::YV12:
    case UMC::YUV422:
    case UMC::NV12:
    case UMC::NV16:
    case UMC::YUY2:
    case UMC::UYVY:
    case UMC::IMC3:
    case UMC::RGB32:
    case UMC::AYUV:
    case UMC::YUV444:
    case UMC::YUV411:
    case UMC::Y210:
    case UMC::Y216:
    case UMC::Y410:
    case UMC::P016:
    case UMC::Y416:
        break;
    default:
        return UMC::UMC_ERR_UNSUPPORTED;
    }

    if (colorFormat == UMC::NV12 && info->GetColorFormat() == UMC::NV12)
    {
        if ((m_info.GetPlaneSampleSize(0) != info->GetPlaneSampleSize(0)) || (m_info.GetPlaneSampleSize(1) != info->GetPlaneSampleSize(1)))
            return UMC::UMC_ERR_UNSUPPORTED;
    }

    if (passed.width > allocated.width ||
        passed.height > allocated.height)
    {
         if (!(a_flags & mfx_UMC_ReallocAllowed))
            return UMC::UMC_ERR_UNSUPPORTED;
    }

    sts = m_pCore->IncreasePureReference(m_frameDataInternal.GetSurface(index).Data.Locked);
    if (sts < MFX_ERR_NONE)
        return UMC::UMC_ERR_FAILED;

    if ((m_IsUseExternalFrames) || (m_sfcVideoPostProcessing))
    {
        if (m_extSurfaces[index].FrameSurface)
        {
            sts = m_pCore->IncreaseReference(&m_extSurfaces[index].FrameSurface->Data);
            if (sts < MFX_ERR_NONE)
                return UMC::UMC_ERR_FAILED;

            m_extSurfaces[m_curIndex].isUsed = true;

            if (m_sfcVideoPostProcessing)
            {
                SetSurfaceForSFC(*m_pCore, *m_extSurfaces[index].FrameSurface);
            }
        }
    }

    m_frameDataInternal.ResetFrameData(index);
    m_curIndex = -1;

    if (passed.width > allocated.width ||
        passed.height > allocated.height)
    {
        if (a_flags & mfx_UMC_ReallocAllowed)
            return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
    }

    return UMC::UMC_OK;
}

const UMC::FrameData* mfx_UMC_FrameAllocator::Lock(UMC::FrameMemID mid)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    mfxU32 index = (mfxU32)mid;
    if (!m_frameDataInternal.IsValidMID(index))
        return 0;

    mfxFrameData *data = 0;

    mfxFrameSurface1 check_surface = {};
    mfxFrameSurface1 &internal_surface = m_frameDataInternal.GetSurface(index);
    check_surface.Info.FourCC = internal_surface.Info.FourCC;

    if (m_IsUseExternalFrames)
    {
        if (internal_surface.Data.MemId != 0)
        {
            data = &internal_surface.Data;
            mfxStatus sts = m_pCore->LockExternalFrame(internal_surface.Data.MemId, data);

            if (sts < MFX_ERR_NONE || !data)
                return 0;

            check_surface.Data = *data;
            check_surface.Data.MemId = 0;
            sts = CheckFrameData(&check_surface);
            if (sts < MFX_ERR_NONE)
                return 0;
        }
        else
        {
            data = &m_extSurfaces[index].FrameSurface->Data;
        }
    }
    else
    {
        if (internal_surface.Data.MemId != 0)
        {
            data = &internal_surface.Data;
            mfxStatus sts = m_pCore->LockFrame(internal_surface.Data.MemId, data);

            if (sts < MFX_ERR_NONE || !data)
                return 0;

            check_surface.Data = *data;
            check_surface.Data.MemId = 0;
            sts = CheckFrameData(&check_surface);
            if (sts < MFX_ERR_NONE)
                return 0;
        }
        else // invalid situation, we always allocate internal frames with MemId
            return 0;
    }

    UMC::FrameData* frameData = &m_frameDataInternal.GetFrameData(index);
    mfxU32 pitch = data->PitchLow + ((mfxU32)data->PitchHigh << 16);

    switch (frameData->GetInfo()->GetColorFormat())
    {
    case UMC::NV16:
    case UMC::NV12:
        frameData->SetPlanePointer(data->Y, 0, pitch);
        frameData->SetPlanePointer(data->U, 1, pitch);
        break;
    case UMC::YUV420:
    case UMC::YUV422:
        frameData->SetPlanePointer(data->Y, 0, pitch);
        frameData->SetPlanePointer(data->U, 1, pitch >> 1);
        frameData->SetPlanePointer(data->V, 2, pitch >> 1);
        break;
    case UMC::IMC3:
        frameData->SetPlanePointer(data->Y, 0, pitch);
        frameData->SetPlanePointer(data->U, 1, pitch);
        frameData->SetPlanePointer(data->V, 2, pitch);
        break;
    case UMC::RGB32:
        {
            frameData->SetPlanePointer(data->B, 0, pitch);
        }
        break;
    case UMC::YUY2:
        {
            frameData->SetPlanePointer(data->Y, 0, pitch);
        }
        break;
    default:
        if (internal_surface.Data.MemId)
        {
            if (m_IsUseExternalFrames)
            {
                m_pCore->UnlockExternalFrame(m_extSurfaces[index].FrameSurface->Data.MemId);
            }
            else
            {
                m_pCore->UnlockFrame(internal_surface.Data.MemId);
            }
        }
        return 0;
    }

    //frameMID->m_locks++;
    return frameData;
}

UMC::Status mfx_UMC_FrameAllocator::Unlock(UMC::FrameMemID mid)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    mfxU32 index = (mfxU32)mid;
    if (!m_frameDataInternal.IsValidMID(index))
        return UMC::UMC_ERR_FAILED;

    mfxFrameSurface1 &internal_surface = m_frameDataInternal.GetSurface(index);
    if (internal_surface.Data.MemId)
    {
        mfxStatus sts;
        if (m_IsUseExternalFrames)
            sts = m_pCore->UnlockExternalFrame(m_extSurfaces[index].FrameSurface->Data.MemId);
        else
            sts = m_pCore->UnlockFrame(internal_surface.Data.MemId);

        if (sts < MFX_ERR_NONE)
            return UMC::UMC_ERR_FAILED;
    }

    return UMC::UMC_OK;
}

UMC::Status mfx_UMC_FrameAllocator::IncreaseReference(UMC::FrameMemID mid)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    mfxU32 index = (mfxU32)mid;
    if (!m_frameDataInternal.IsValidMID(index))
        return UMC::UMC_ERR_FAILED;

    m_frameDataInternal.IncreaseRef(index);

    return UMC::UMC_OK;
}

UMC::Status mfx_UMC_FrameAllocator::DecreaseReference(UMC::FrameMemID mid)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    mfxU32 index = (mfxU32)mid;
    if (!m_frameDataInternal.IsValidMID(index))
        return UMC::UMC_ERR_FAILED;

    mfxU32 refCounter = m_frameDataInternal.DecreaseRef(index);
    if (!refCounter)
    {
        return Free(mid);
    }

    return UMC::UMC_OK;
}

UMC::Status mfx_UMC_FrameAllocator::Free(UMC::FrameMemID mid)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 index = (mfxU32)mid;
    if (!m_frameDataInternal.IsValidMID(index))
        return UMC::UMC_ERR_FAILED;

    sts = m_pCore->DecreasePureReference(m_frameDataInternal.GetSurface(index).Data.Locked);
    if (sts < MFX_ERR_NONE)
        return UMC::UMC_ERR_FAILED;

    if ((m_IsUseExternalFrames) || (m_sfcVideoPostProcessing))
    {
        if (m_extSurfaces[index].FrameSurface)
        {
            sts = m_pCore->DecreaseReference(&m_extSurfaces[index].FrameSurface->Data);
            if (sts < MFX_ERR_NONE)
                return UMC::UMC_ERR_FAILED;
        }
        m_extSurfaces[index].isUsed = false;
    }

    return UMC::UMC_OK;
}

mfxStatus mfx_UMC_FrameAllocator::SetCurrentMFXSurface(mfxFrameSurface1 *surf)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    MFX_CHECK_NULL_PTR1(surf);

    if (surf->Data.Locked)
        return MFX_ERR_MORE_SURFACE;

    // check input surface
    if (!(m_sfcVideoPostProcessing && (surf->Info.FourCC != m_surface_info.FourCC)))// if csc is done via sfc, will not do below checks
    {
    if ((surf->Info.BitDepthLuma ? surf->Info.BitDepthLuma : 8) != (m_surface_info.BitDepthLuma ? m_surface_info.BitDepthLuma : 8))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((surf->Info.BitDepthChroma ? surf->Info.BitDepthChroma : 8) != (m_surface_info.BitDepthChroma ? m_surface_info.BitDepthChroma : 8))
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (surf->Info.FourCC == MFX_FOURCC_P010
        || surf->Info.FourCC == MFX_FOURCC_P210
        || surf->Info.FourCC == MFX_FOURCC_Y210
        || surf->Info.FourCC == MFX_FOURCC_P016
        || surf->Info.FourCC == MFX_FOURCC_Y216
        || surf->Info.FourCC == MFX_FOURCC_Y416)
    {
        if (m_isSWDecode)
        {
            if (surf->Info.Shift != 0)
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        else
        {
            if ((m_IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && surf->Info.Shift != 1)
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    if (m_externalFramesResponse && surf->Data.MemId)
    {
        bool isFound = false;
        for (mfxI32 i = 0; i < m_externalFramesResponse->NumFrameActual; i++)
        {
            if (m_pCore->MapIdx(m_externalFramesResponse->mids[i]) == surf->Data.MemId)
            {
                isFound = true;
                break;
            }
        }

        // in delay allocate mode, the m_externalFramesResponse->mids maybe not filled in the MFXInit
        // surface will be added into m_frameDataInternal on the fly
        // Delay allocate mode not work with D3D9, D3D9 will use legacy allocator logical
        if (!isFound)
        {
            MFX_CHECK(m_pCore->IsSupportedDelayAlloc(), MFX_ERR_UNDEFINED_BEHAVIOR);
            for (mfxU32 i = 0; i < m_frameDataInternal.GetSize(); i++)
            {
                auto& internal_surf = m_frameDataInternal.GetSurface(i);
                if (internal_surf.Data.MemId == surf->Data.MemId)
                {
                    isFound = true;
                    break;
                }
            }
        }
        // add the new APP surface on the fly
        if (m_pCore->IsSupportedDelayAlloc() && !isFound)
        {
            m_frameDataInternal.AddNewFrame(this, surf, &m_info);
            m_extSurfaces.push_back(surf_descr(surf, false));
        }
    }

    m_curIndex = -1;

    if ((!m_IsUseExternalFrames) && (!m_sfcVideoPostProcessing))
        m_curIndex = FindFreeSurface();
    else if ((!m_IsUseExternalFrames) && (m_sfcVideoPostProcessing))
    {
        for (mfxU32 i = 0; i < m_extSurfaces.size(); i++)
        {
            if (NULL == m_extSurfaces[i].FrameSurface)
            {
                /* new surface */
                m_curIndex = i;
                m_extSurfaces[m_curIndex].FrameSurface = surf;
                break;
            }

            if ( (NULL != m_extSurfaces[i].FrameSurface) &&
                  (0 == m_extSurfaces[i].FrameSurface->Data.Locked) &&
                  (m_extSurfaces[i].FrameSurface->Data.MemId == surf->Data.MemId) &&
                  (0 == m_frameDataInternal.GetSurface(i).Data.Locked) )
            {
                /* surfaces filled already */
                m_curIndex = i;
                m_extSurfaces[m_curIndex].FrameSurface = surf;
                break;
            }
        }

        // Still not found. It may happen if decoder gets 'surf' surface which on app side belongs to
        // a pool bigger than m_extSurfaces/m_frameDataInternal pools which decoder is aware.
        if (m_curIndex == -1)
        {
            for (mfxU32 i = 0; i < m_extSurfaces.size(); i++)
            {
                // So attemping to find an expired slot in m_extSurfaces
                if (!m_extSurfaces[i].isUsed && (0 == m_frameDataInternal.GetSurface(i).Data.Locked))
                {
                    m_curIndex = i;
                    m_extSurfaces[m_curIndex].FrameSurface = surf;
                    break;
                }
            }
        }
    }
    else
    {
        m_curIndex = FindSurface(surf);

        if (m_curIndex != -1)
        {
            mfxFrameSurface1 &internalSurf = m_frameDataInternal.GetSurface(m_curIndex);
            m_extSurfaces[m_curIndex].FrameSurface = surf;
            if (internalSurf.Data.Locked) // surface was locked yet
            {
                m_curIndex = -1;
            }

            // update info
            internalSurf.Info = surf->Info;
        }
        else
        {
            m_curIndex = AddSurface(surf);
            if (m_curIndex != -1)
                m_extSurfaces[m_curIndex].FrameSurface = surf;
        }
    }

    return MFX_ERR_NONE;
}

mfxI32 mfx_UMC_FrameAllocator::AddSurface(mfxFrameSurface1 *surface)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    mfxI32 index = -1;

    if (!m_IsUseExternalFrames)
        return -1;

    if (surface->Data.MemId && !m_isSWDecode)
    {
        mfxU32 i;
        for (i = 0; i < m_extSurfaces.size(); i++)
        {
            if (surface->Data.MemId == m_pCore->MapIdx(m_frameDataInternal.GetSurface(i).Data.MemId))
            {
                m_extSurfaces[i].FrameSurface = surface;
                index = i;
                break;
            }
        }
    }
    else
    {
        m_extSurfaces.push_back(surf_descr(surface,false));
        index = (mfxI32)(m_extSurfaces.size() - 1);
    }

    switch (surface->Info.FourCC)
    {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_NV16:
    case MFX_FOURCC_YV12:
    case MFX_FOURCC_YUY2:
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_AYUV:
    case MFX_FOURCC_P010:
    case MFX_FOURCC_P210:
    case MFX_FOURCC_Y210:
    case MFX_FOURCC_Y410:
    case MFX_FOURCC_Y216:

        break;
    default:
        return -1;
    }

    if (m_IsUseExternalFrames && m_isSWDecode)
    {
        m_frameDataInternal.AddNewFrame(this, surface, &m_info);
    }

    return index;
}

mfxI32 mfx_UMC_FrameAllocator::FindSurface(mfxFrameSurface1 *surf)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    if (!surf)
        return -1;

    mfxFrameData * data = &surf->Data;

    if (data->MemId && m_IsUseExternalFrames)
    {
        mfxMemId sMemId;
        for (mfxU32 i = 0; i < m_frameDataInternal.GetSize(); i++)
        {
            mfxMemId memId = m_frameDataInternal.GetSurface(i).Data.MemId;
            sMemId = m_pCore->MapIdx(memId);
            if (sMemId == data->MemId)
            {
                return i;
            }
        }
    }

    for (mfxU32 i = 0; i < m_extSurfaces.size(); i++)
    {
        if (m_extSurfaces[i].FrameSurface == surf)
        {
            return i;
        }
    }

    return -1;
}

mfxI32 mfx_UMC_FrameAllocator::FindFreeSurface()
{
    UMC::AutomaticUMCMutex guard(m_guard);

    if ((m_IsUseExternalFrames) || (m_sfcVideoPostProcessing))
    {
        return m_curIndex;
    }

    if (m_curIndex != -1)
        return m_curIndex;

    for (mfxU32 i = 0; i < m_frameDataInternal.GetSize(); i++)
    {
        if (!m_frameDataInternal.GetSurface(i).Data.Locked)
        {
            return i;
        }
    }

    return -1;
}

bool mfx_UMC_FrameAllocator::HasFreeSurface()
{
    return FindFreeSurface() != -1;
}

mfxFrameSurface1 * mfx_UMC_FrameAllocator::GetInternalSurface(UMC::FrameMemID index)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    if (m_IsUseExternalFrames)
    {
        return 0;
    }

    if (index >= 0)
    {
        if (!m_frameDataInternal.IsValidMID((mfxU32)index))
            return 0;
        return &m_frameDataInternal.GetSurface(index);
    }

    return 0;
}

mfxFrameSurface1 * mfx_UMC_FrameAllocator::GetSurfaceByIndex(UMC::FrameMemID index)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    if (index < 0)
        return 0;

    if (!m_frameDataInternal.IsValidMID((mfxU32)index))
        return 0;

    return m_IsUseExternalFrames ? m_extSurfaces[index].FrameSurface : &m_frameDataInternal.GetSurface(index);
}

void mfx_UMC_FrameAllocator::SetSfcPostProcessingFlag(bool flagToSet)
{
    m_sfcVideoPostProcessing = flagToSet;
}

mfxFrameSurface1 * mfx_UMC_FrameAllocator::GetSurface(UMC::FrameMemID index, mfxFrameSurface1 *surface, const mfxVideoParam * videoPar)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    if (!surface || !videoPar || 0 > index)
        return 0;

    if ((m_IsUseExternalFrames) || (m_sfcVideoPostProcessing))
    {
        if ((Ipp32u)index >= m_extSurfaces.size())
            return 0;
        return m_extSurfaces[index].FrameSurface;
    }
    else
    {
        mfxStatus sts = m_pCore->IncreaseReference(&surface->Data);
        if (sts < MFX_ERR_NONE)
            return 0;

        m_extSurfaces[index].FrameSurface = surface;
    }

    return surface;
}

mfxStatus mfx_UMC_FrameAllocator::PrepareToOutput(mfxFrameSurface1 *surface_work, UMC::FrameMemID index, const mfxVideoParam *, mfxU32 gpuCopyMode)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    mfxStatus sts;
    mfxU16 dstMemType = (MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET);

    UMC::FrameData* frame = &m_frameDataInternal.GetFrameData(index);

    if (m_IsUseExternalFrames)
        return MFX_ERR_NONE;

    mfxFrameSurface1 surface;

    memset(&surface, 0, sizeof(mfxFrameSurface1));

    surface.Info = m_surface_info;
    surface.Info.Width  = (mfxU16)frame->GetInfo()->GetWidth();
    surface.Info.Height = (mfxU16)frame->GetInfo()->GetHeight();

    switch (frame->GetInfo()->GetColorFormat())
    {
    case UMC::NV12:
        surface.Data.Y = frame->GetPlaneMemoryInfo(0)->m_planePtr;
        surface.Data.UV = frame->GetPlaneMemoryInfo(1)->m_planePtr;
        surface.Data.PitchHigh = (mfxU16)(frame->GetPlaneMemoryInfo(0)->m_pitch / (1 << 16));
        surface.Data.PitchLow  = (mfxU16)(frame->GetPlaneMemoryInfo(0)->m_pitch % (1 << 16));
        break;

    case UMC::YUV420:
        surface.Data.Y = frame->GetPlaneMemoryInfo(0)->m_planePtr;
        surface.Data.U = frame->GetPlaneMemoryInfo(1)->m_planePtr;
        surface.Data.V = frame->GetPlaneMemoryInfo(2)->m_planePtr;
        surface.Data.PitchHigh = (mfxU16)(frame->GetPlaneMemoryInfo(0)->m_pitch / (1 << 16));
        surface.Data.PitchLow  = (mfxU16)(frame->GetPlaneMemoryInfo(0)->m_pitch % (1 << 16));
        break;

    case UMC::IMC3:
        surface.Data.Y = frame->GetPlaneMemoryInfo(0)->m_planePtr;
        surface.Data.U = frame->GetPlaneMemoryInfo(1)->m_planePtr;
        surface.Data.V = frame->GetPlaneMemoryInfo(2)->m_planePtr;
        surface.Data.PitchHigh = (mfxU16)(frame->GetPlaneMemoryInfo(0)->m_pitch / (1 << 16));
        surface.Data.PitchLow  = (mfxU16)(frame->GetPlaneMemoryInfo(0)->m_pitch % (1 << 16));
        break;

    case UMC::RGB32:
        surface.Data.B = frame->GetPlaneMemoryInfo(0)->m_planePtr;
        surface.Data.G = surface.Data.B + 1;
        surface.Data.R = surface.Data.B + 2;
        surface.Data.A = surface.Data.B + 3;
        surface.Data.PitchHigh = (mfxU16)(frame->GetPlaneMemoryInfo(0)->m_pitch / (1 << 16));
        surface.Data.PitchLow  = (mfxU16)(frame->GetPlaneMemoryInfo(0)->m_pitch % (1 << 16));
        break;

    case UMC::YUY2:
        surface.Data.Y = frame->GetPlaneMemoryInfo(0)->m_planePtr;
        surface.Data.U = surface.Data.Y + 1;
        surface.Data.V = surface.Data.Y + 3;
        surface.Data.PitchHigh = (mfxU16)(frame->GetPlaneMemoryInfo(0)->m_pitch / (1 << 16));
        surface.Data.PitchLow  = (mfxU16)(frame->GetPlaneMemoryInfo(0)->m_pitch % (1 << 16));
        break;

    default:
        return MFX_ERR_UNSUPPORTED;
    }
    surface.Info.FourCC = surface_work->Info.FourCC;
    surface.Info.Shift = m_IsUseExternalFrames ? m_extSurfaces[index].FrameSurface->Info.Shift : m_frameDataInternal.GetSurface(index).Info.Shift;

    //Performance issue. We need to unlock mutex to let decoding thread run async.
    guard.Unlock();
    sts = m_pCore->DoFastCopyWrapper(surface_work,
                                     dstMemType,
                                     &surface,
                                     MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                                     gpuCopyMode);
    guard.Lock();

    MFX_CHECK_STS(sts);

    if (!m_IsUseExternalFrames)
    {
        mfxStatus temp_sts = m_pCore->DecreaseReference(&surface_work->Data);

        if (temp_sts < MFX_ERR_NONE && sts >= MFX_ERR_NONE)
        {
            sts = temp_sts;
        }

        m_extSurfaces[index].FrameSurface = 0;
    }

    return sts;
}

SurfaceSource::SurfaceSource(VideoCORE* core, const mfxVideoParam& video_param, eMFXPlatform platform, mfxFrameAllocRequest& request, mfxFrameAllocRequest& request_internal, mfxFrameAllocResponse& response, mfxFrameAllocResponse& response_alien, bool needVppJPEG)
    : m_core(core)
    , m_response(response)
    , m_response_alien(response_alien)
{
    MFX_CHECK_WITH_THROW_STS(m_core, MFX_ERR_NULL_PTR);

    m_response = {};

    // Since DECODE uses internal allocation at init step (when we can't actually understand whether user will use
    // VPL interface or not) we are forcing 1.x interface in case if ext allocator set

    bool vpl_interface = SupportsVPLFeatureSet(*m_core);

    m_redirect_to_vpl_path = vpl_interface && !m_core->IsExternalFrameAllocator();

    if (m_redirect_to_vpl_path)
    {
        auto dec_postprocessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(video_param.ExtParam, video_param.NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);

        mfxFrameInfo output_info = needVppJPEG ? request_internal.Info : request.Info;

        mfxU16 request_type = request.Type;
        mfxFrameInfo request_info = request.Info;

        if (dec_postprocessing)
        {
            output_info.FourCC       = dec_postprocessing->Out.FourCC;
            output_info.ChromaFormat = dec_postprocessing->Out.ChromaFormat;
            output_info.Width        = dec_postprocessing->Out.Width;
            output_info.Height       = dec_postprocessing->Out.Height;
            output_info.CropX        = dec_postprocessing->Out.CropX;
            output_info.CropY        = dec_postprocessing->Out.CropY;
            output_info.CropW        = dec_postprocessing->Out.CropW;
            output_info.CropH        = dec_postprocessing->Out.CropH;
        }

        mfxU16 output_type = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;
        output_type |= (video_param.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) ? MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_DXVA2_DECODER_TARGET;

        auto base_core_vpl = dynamic_cast<CommonCORE_VPL*>(m_core);
        MFX_CHECK_WITH_THROW_STS(base_core_vpl, MFX_ERR_UNSUPPORTED);

        if ((request.Type & MFX_MEMTYPE_INTERNAL_FRAME) || needVppJPEG)
        {
            request = request_internal;
        }
        std::unique_ptr<SurfaceCache> scoped_cache_ptr(SurfaceCache::Create(*base_core_vpl, request.Type, request.Info));

        m_vpl_cache_decoder_surfaces.reset(new surface_cache_controller<SurfaceCache>(scoped_cache_ptr.get(), ComponentType::DECODE));

        scoped_cache_ptr.release();

        m_sw_fallback_sys_mem = (MFX_PLATFORM_SOFTWARE == platform) && (video_param.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

        m_need_to_copy_before_output =
            // SW / HW config mismatch between decoder impl and requested IOPattern
            ((MFX_PLATFORM_SOFTWARE == platform) ? (video_param.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (video_param.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
            || needVppJPEG;

        m_allocate_internal = m_need_to_copy_before_output
            // SFC requested on Linux (Windows uses SFC surfaces as decoders work surface directly)
            || (dec_postprocessing && m_core->GetVAType() == MFX_HW_VAAPI);

        if (!m_allocate_internal)
        {
            // We simply use the same surfaces
            m_vpl_cache_output_surfaces = m_vpl_cache_decoder_surfaces;
        }
        else
        {
            scoped_cache_ptr.reset(SurfaceCache::Create(*base_core_vpl, needVppJPEG ? request_type : output_type, needVppJPEG ? request_info : output_info));

            m_vpl_cache_output_surfaces.reset(new surface_cache_controller<SurfaceCache>(scoped_cache_ptr.get(), ComponentType::DECODE));

            scoped_cache_ptr.release();
        }

        mfxSession session = m_core->GetSession();
        MFX_CHECK_WITH_THROW_STS(session, MFX_ERR_INVALID_HANDLE);

        mfxStatus sts = m_vpl_cache_output_surfaces->SetupCache(session, video_param);
        MFX_CHECK_WITH_THROW_STS(sts == MFX_ERR_NONE, sts);

        mfxU32 bit_depth              = BitDepthFromFourcc(video_param.mfx.FrameInfo.FourCC);

        UMC::ColorFormat color_format;

        switch (video_param.mfx.FrameInfo.FourCC)
        {
        case MFX_FOURCC_NV12:
            color_format = UMC::NV12;
            break;
        case MFX_FOURCC_P010:
            color_format = UMC::NV12;
            break;
        case MFX_FOURCC_NV16:
            color_format = UMC::NV16;
            break;
        case MFX_FOURCC_P210:
            color_format = UMC::NV16;
            break;
        case MFX_FOURCC_RGB4:
            color_format = UMC::RGB32;
            break;
        case MFX_FOURCC_YV12:
            color_format = UMC::YUV420;
            break;
        case MFX_FOURCC_YUY2:
            color_format = UMC::YUY2;
            break;
        case MFX_FOURCC_AYUV:
            color_format = UMC::AYUV;
            break;
        case MFX_FOURCC_Y210:
            color_format = UMC::Y210;
            break;
        case MFX_FOURCC_Y410:
            color_format = UMC::Y410;
            break;
        case MFX_FOURCC_P016:
            color_format = UMC::P016;
            break;
        case MFX_FOURCC_Y216:
            color_format = UMC::Y216;
            break;
        case MFX_FOURCC_Y416:
            color_format = UMC::Y416;
            break;
        case MFX_FOURCC_RGBP:
        case MFX_FOURCC_BGRP:
        case MFX_FOURCC_YUV444:
            color_format = UMC::YUV444;
            break;
        case MFX_FOURCC_YUV411:
            color_format = UMC::YUV411;
            break;
        case MFX_FOURCC_YUV400:
            color_format = UMC::GRAY;
            break;
        case MFX_FOURCC_YUV422H:
        case MFX_FOURCC_YUV422V:
            color_format = UMC::YUV422;
            break;
        default:
            MFX_CHECK_WITH_THROW_STS(false, MFX_ERR_UNSUPPORTED);
        }

        UMC::Status umcSts = m_video_data_info.Init(request.Info.Width, request.Info.Height, color_format, bit_depth);
        MFX_CHECK_WITH_THROW_STS(ConvertStatusUmc2Mfx(umcSts) == MFX_ERR_NONE, MFX_ERR_UNSUPPORTED);
    }
    else
    {
        CreateUMCAllocator(video_param, platform, needVppJPEG);

        MFX_CHECK_WITH_THROW_STS(m_umc_allocator_adapter.get(), MFX_ERR_INVALID_HANDLE);

        bool useInternal = request.Type & MFX_MEMTYPE_INTERNAL_FRAME;
        mfxStatus mfxSts = MFX_ERR_NONE;

        if (platform != MFX_PLATFORM_SOFTWARE && !useInternal)
        {
            request.AllocId = video_param.AllocId;
            mfxSts = m_core->AllocFrames(&request, &m_response, false);
        }

        MFX_CHECK_WITH_THROW_STS(mfxSts >= MFX_ERR_NONE, mfxSts);

        useInternal |= needVppJPEG;

        // allocates internal surfaces:
        if (useInternal)
        {
            request = request_internal;
            bool useSystem = needVppJPEG ? video_param.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY : true;
            mfxSts = m_core->AllocFrames(&request_internal, &m_response, useSystem);

            MFX_CHECK_WITH_THROW_STS(mfxSts >= MFX_ERR_NONE, mfxSts);

            UMC::Status umcSts = m_umc_allocator_adapter->InitMfx(0, m_core, &video_param, &request, &m_response, !useInternal, platform == MFX_PLATFORM_SOFTWARE);
            MFX_CHECK_WITH_THROW_STS(umcSts == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);
        }
        else
        {
            UMC::Status umcSts = m_umc_allocator_adapter->InitMfx(0, m_core, &video_param, &request, &m_response, !useInternal, platform == MFX_PLATFORM_SOFTWARE);
            MFX_CHECK_WITH_THROW_STS(umcSts == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);

            m_umc_allocator_adapter->SetExternalFramesResponse(&m_response);
        }

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
        if ((mfxExtDecVideoProcessing *)GetExtendedBuffer(video_param.ExtParam, video_param.NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING))
        {
            MFX_CHECK_WITH_THROW_STS(useInternal || MFX_HW_D3D11 == m_core->GetVAType() || MFX_HW_VAAPI == m_core->GetVAType(), MFX_ERR_UNSUPPORTED);
            m_umc_allocator_adapter->SetSfcPostProcessingFlag(true);
        }
#endif
    }

}

void SurfaceSource::CreateUMCAllocator(const mfxVideoParam & video_param, eMFXPlatform platform, bool needVppJPEG)
{
    (void) needVppJPEG;

    if (MFX_PLATFORM_SOFTWARE == platform)
    {
        MFX_CHECK_WITH_THROW_STS(false, MFX_ERR_UNSUPPORTED);
    }
    else
    {
        switch (video_param.mfx.CodecId)
        {
        case MFX_CODEC_VC1:
#if defined(MFX_ENABLE_VC1_VIDEO_DECODE)
            if (MFX_ERR_NONE == m_core->IsGuidSupported(sDXVA2_Intel_ModeVC1_D_Super, const_cast<mfxVideoParam*>(&video_param)))
                m_umc_allocator_adapter.reset(new mfx_UMC_FrameAllocator_D3D());
#endif // #if defined(MFX_ENABLE_VC1_VIDEO_DECODE)
            break;
        case MFX_CODEC_JPEG:
#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)
            if (!needVppJPEG)
                m_umc_allocator_adapter.reset(new mfx_UMC_FrameAllocator_D3D());
#if defined (MFX_ENABLE_VPP)
            else
                m_umc_allocator_adapter.reset(new mfx_UMC_FrameAllocator_D3D_Converter());
#endif
#endif // defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)
            break;
        default:
            m_umc_allocator_adapter.reset(new mfx_UMC_FrameAllocator_D3D());
        }
    }
}

SurfaceSource::~SurfaceSource()
{
    std::ignore = MFX_STS_TRACE(ConvertStatusUmc2Mfx(Close()));
}

void SurfaceSource::ReleaseCurrentWorkSurface()
{
    // Release previously set surface if it wasn't taken by decoder
    if (m_current_work_surface && m_allocate_internal)
    {
        if (!m_need_to_copy_before_output)
        {
            RemoveCorrespondence(*m_current_work_surface);
        }

        std::ignore = MFX_STS_TRACE(ReleaseSurface(*m_current_work_surface));
    }
}

// Closes object and releases all allocated memory
UMC::Status SurfaceSource::Close()
{
    if (m_redirect_to_vpl_path != !!m_vpl_cache_decoder_surfaces
    || !m_redirect_to_vpl_path != !!m_umc_allocator_adapter)
    {
        // Was already Closed, do nothing
        return UMC::UMC_OK;
    }

    if (m_redirect_to_vpl_path)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ReleaseCurrentWorkSurface();

        m_vpl_cache_decoder_surfaces.reset();
        m_vpl_cache_output_surfaces.reset();

        m_mfx2umc_memid.clear();
        m_umc2mfx_memid.clear();
        m_umc2framedata.clear();
        m_work_output_surface_map.clear();
        m_output_work_surface_map.clear();
        m_sw_fallback_surfaces.clear();

        return UMC::UMC_OK;
    }
    else
    {
        UMC::Status sts = m_umc_allocator_adapter->Close();

        if (m_response.NumFrameActual)
            std::ignore = MFX_STS_TRACE(m_core->FreeFrames(&m_response));

        if (m_response_alien.NumFrameActual)
            std::ignore = MFX_STS_TRACE(m_core->FreeFrames(&m_response_alien));

        m_umc_allocator_adapter.reset();

        return sts;
    }
}

UMC::Status SurfaceSource::Reset()
{
    MFX_CHECK(m_redirect_to_vpl_path == !!m_vpl_cache_decoder_surfaces, UMC::UMC_ERR_NOT_INITIALIZED);
    MFX_CHECK(!m_redirect_to_vpl_path == !!m_umc_allocator_adapter, UMC::UMC_ERR_NOT_INITIALIZED);

    if (m_redirect_to_vpl_path)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ReleaseCurrentWorkSurface();

        m_mfx2umc_memid.clear();
        m_umc2mfx_memid.clear();
        m_umc2framedata.clear();
        m_work_output_surface_map.clear();
        m_output_work_surface_map.clear();
        m_sw_fallback_surfaces.clear();

        m_current_work_surface = nullptr;

        return UMC::UMC_OK;
    }
    else
    {
        return m_umc_allocator_adapter->Reset();
    }
}

void SurfaceSource::CreateBinding(const mfxFrameSurface1 & surf)
{
    if (m_mfx2umc_memid.find(surf.Data.MemId) != std::end(m_mfx2umc_memid))
        RemoveBinding(surf);

    UMC::FrameMemID mid_to_insert = 0;

    for (; m_umc2mfx_memid.find(mid_to_insert) != std::end(m_umc2mfx_memid); ++mid_to_insert) {}

    m_mfx2umc_memid.insert({ surf.Data.MemId, mid_to_insert });
    m_umc2mfx_memid.insert({ mid_to_insert,  surf.Data.MemId });
}

void SurfaceSource::RemoveBinding(const mfxFrameSurface1 & surf)
{
    auto it = m_mfx2umc_memid.find(surf.Data.MemId);
    if (it == std::end(m_mfx2umc_memid))
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_FOUND);
        return;
    }

    m_umc2mfx_memid.erase(it->second);
    m_umc2framedata.erase(it->second);
    m_mfx2umc_memid.erase(it);
}

mfxFrameSurface1* SurfaceSource::GetDecoderSurface(UMC::FrameMemID index)
{
    if (index < 0)
    {
        return nullptr;
    }

    auto it = m_umc2mfx_memid.find(index);
    if (it == std::end(m_umc2mfx_memid))
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_INVALID_HANDLE);
        return nullptr;
    }

    mfxFrameSurface1* surf = (*m_vpl_cache_decoder_surfaces)->FindSurface(it->second);
    if (m_sw_fallback_sys_mem)
    {
        if (index >= (UMC::FrameMemID)m_sw_fallback_surfaces.size())
        {
            std::ignore = MFX_STS_TRACE(MFX_ERR_INVALID_HANDLE);
            return nullptr;
        }
        surf = m_sw_fallback_surfaces[index];
    }

    return surf;
}

UMC::Status SurfaceSource::CheckForRealloc(const UMC::VideoDataInfo & info, const mfxFrameSurface1& surf, bool realloc_allowed) const
{
    bool realloc_required = info.GetWidth() > surf.Info.Width || info.GetHeight() > surf.Info.Height;

    MFX_CHECK(!realloc_required, realloc_allowed ? UMC::UMC_ERR_NOT_ENOUGH_BUFFER : UMC::UMC_ERR_UNSUPPORTED);

    return UMC::UMC_OK;
}

static inline mfxMemId GetIdentifier(const mfxFrameSurface1& surf)
{
    return surf.Data.MemId ? surf.Data.MemId : mfxMemId(&surf);
}

bool SurfaceSource::CreateCorrespondence(mfxFrameSurface1& surface_work, mfxFrameSurface1& surface_out)
{
    if (m_work_output_surface_map.find(surface_work.Data.MemId) != std::end(m_work_output_surface_map))
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_UNKNOWN);
        return false;
    }

    if (m_output_work_surface_map.find(GetIdentifier(surface_out)) != std::end(m_output_work_surface_map))
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_UNKNOWN);
        return false;
    }

    // AddRef and ++Data.Locked
    if (MFX_STS_TRACE(m_core->IncreaseReference(surface_out)) != MFX_ERR_NONE)
        return false;

    m_work_output_surface_map.insert({ surface_work.Data.MemId,    &surface_out  });
    m_output_work_surface_map.insert({ GetIdentifier(surface_out), &surface_work });

    return true;
}

void SurfaceSource::RemoveCorrespondence(mfxFrameSurface1& surface_work)
{
    if (!m_allocate_internal)
        return;

    auto it_wo = m_work_output_surface_map.find(surface_work.Data.MemId);
    if (it_wo == std::end(m_work_output_surface_map))
    {
        return;
    }

    mfxFrameSurface1* output_surface = it_wo->second;
    if (!output_surface)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NULL_PTR);
        return;
    }

    m_work_output_surface_map.erase(it_wo);
    m_output_work_surface_map.erase(GetIdentifier(*output_surface));

    // Release and --Data.Locked
    std::ignore = MFX_STS_TRACE(m_core->DecreaseReference(*output_surface));
}


UMC::Status SurfaceSource::Alloc(UMC::FrameMemID *pNewMemID, const UMC::VideoDataInfo * info, uint32_t Flags)
{
    MFX_CHECK(m_redirect_to_vpl_path == !!m_vpl_cache_decoder_surfaces, UMC::UMC_ERR_NOT_INITIALIZED);
    MFX_CHECK(!m_redirect_to_vpl_path == !!m_umc_allocator_adapter, UMC::UMC_ERR_NOT_INITIALIZED);

    if (m_redirect_to_vpl_path)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        MFX_CHECK(pNewMemID, UMC::UMC_ERR_NULL_PTR);

        mfxStatus sts;

        if (m_current_work_surface)
        {
            // MSDK 2.0 memory model 2, returning work surface
            auto it = m_mfx2umc_memid.find(m_current_work_surface->Data.MemId);
            MFX_CHECK(it != std::end(m_mfx2umc_memid), UMC::UMC_ERR_FAILED);

            *pNewMemID = it->second;

            if (m_allocate_internal && !m_need_to_copy_before_output)
            {
                // SFC on Linux
                auto it_wo = m_work_output_surface_map.find(m_current_work_surface->Data.MemId);
                MFX_CHECK(it_wo != std::end(m_work_output_surface_map), UMC::UMC_ERR_FAILED);

                sts = SetSurfaceForSFC(*m_core, *(it_wo->second));
                MFX_CHECK(sts == MFX_ERR_NONE, UMC::UMC_ERR_FAILED);
            }

            const mfxFrameSurface1 & tmp_surf = *m_current_work_surface;

            // Drop current state, then next alloc will result in ERR_MORE_SURFACE
            m_current_work_surface = nullptr;

            return CheckForRealloc(*info, tmp_surf, Flags & mfx_UMC_ReallocAllowed);
        }

        // In MSDK 2.0 memory model 2 we don't allocate missing frames on the fly
        if (m_memory_model2)
        {
            *pNewMemID = UMC::FRAME_MID_INVALID;
            return UMC::UMC_ERR_ALLOC;
        }

        // MSDK 2.0 memory model 3, allocating and returning new work surface

        using namespace std::chrono;

        auto cache_timeout = (*m_vpl_cache_decoder_surfaces)->GetTimeout();

        // Start timer if first entry to Alloc and timeout option passed on Init
        if (!m_timer.IsRunnig() && cache_timeout != 0ms)
        {
            m_timer.Reset(cache_timeout);
        }

        // Check if timer already expired
        if (m_timer.IsRunnig())
        {
            if (m_timer.Expired())
            {
                *pNewMemID = UMC::FRAME_MID_INVALID;
                MFX_RETURN(UMC::UMC_ERR_ALLOC);
            }

            cache_timeout = m_timer.Left();
        }

        mfxFrameSurface1* surf = nullptr;
        // If timer wasn't set (i.e. cache hints buffer wasn't attached) cache timeout below would be zero (i.e. no waiting for free surface)
        sts = (*m_vpl_cache_decoder_surfaces)->GetSurface(surf, cache_timeout, true);

        MFX_CHECK(sts == MFX_ERR_NONE,        UMC::UMC_ERR_ALLOC);

        if (m_sw_fallback_sys_mem)
        {
            /*
            In case of sw fallback and sys mem we can have no ext allocator.
            In this case we can't manage surface through surface cache.
            Instead we will manage them through m_sw_fallback_surfaces.
            */
            m_sw_fallback_surfaces.push_back(surf);
        }

        if (m_allocate_internal && !m_need_to_copy_before_output)
        {
            // SFC on Linux

            mfxFrameSurface1* output_surface = nullptr;
            sts = (*m_vpl_cache_output_surfaces)->GetSurface(output_surface, true);
            MFX_CHECK(sts == MFX_ERR_NONE,        UMC::UMC_ERR_ALLOC);

            // RAII lock to drop refcount in case of error
            surface_refcount_scoped_lock output_surf_scoped_lock(output_surface);

            MFX_CHECK(CreateCorrespondence(*surf, *output_surface), UMC::UMC_ERR_FAILED);

            sts = SetSurfaceForSFC(*m_core, *output_surface);
            MFX_CHECK(sts == MFX_ERR_NONE, UMC::UMC_ERR_FAILED);

            output_surf_scoped_lock.release();
        }

        CreateBinding(*surf);

        *pNewMemID = m_mfx2umc_memid[surf->Data.MemId];

        return CheckForRealloc(*info, *surf, Flags & mfx_UMC_ReallocAllowed);
    }
    else
    {
        return m_umc_allocator_adapter->Alloc(pNewMemID, info, Flags);
    }
}

UMC::Status SurfaceSource::GetFrameHandle(UMC::FrameMemID MID, void * handle)
{
    MFX_CHECK(m_redirect_to_vpl_path == !!m_vpl_cache_decoder_surfaces, UMC::UMC_ERR_NOT_INITIALIZED);
    MFX_CHECK(!m_redirect_to_vpl_path == !!m_umc_allocator_adapter, UMC::UMC_ERR_NOT_INITIALIZED);

    if (m_redirect_to_vpl_path)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        auto it = m_umc2mfx_memid.find(MID);
        MFX_CHECK(it != std::end(m_umc2mfx_memid), MFX_ERR_INVALID_HANDLE);
        return ConvertStatusMfx2Umc(MFX_STS_TRACE(m_core->GetFrameHDL(it->second, reinterpret_cast<mfxHDL*>(handle), false)));
    }
    else
    {
        return m_umc_allocator_adapter->GetFrameHandle(MID, handle);
    }
}

const UMC::FrameData* SurfaceSource::Lock(UMC::FrameMemID MID)
{
    if (m_redirect_to_vpl_path != !!m_vpl_cache_decoder_surfaces)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
        return nullptr;
    }
    if (!m_redirect_to_vpl_path != !!m_umc_allocator_adapter)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
        return nullptr;
    }

    if (m_redirect_to_vpl_path)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        mfxFrameSurface1* surf = GetDecoderSurface(MID);

        if (!surf)
        {
            std::ignore = MFX_STS_TRACE(MFX_ERR_INVALID_HANDLE);
            return nullptr;
        }

        auto base_core_vpl = dynamic_cast<CommonCORE_VPL*>(m_core);
        if (!base_core_vpl)
        {
            std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
            return nullptr;
        }

        auto sts_was_locked_pair = base_core_vpl->Lock(*surf, MFX_MAP_READ_WRITE);

        if (MFX_STS_TRACE(sts_was_locked_pair.first) != MFX_ERR_NONE)
        {
            return nullptr;
        }

        auto it_framedata = m_umc2framedata.find(MID);
        if (it_framedata == std::end(m_umc2framedata))
        {
            UMC::FrameData umc_frame_data;

            umc_frame_data.Init(&m_video_data_info, MID, this);

            std::tie(it_framedata, std::ignore) = m_umc2framedata.insert({ MID, umc_frame_data });
        }

        UMC::FrameData& umc_frame_data = it_framedata->second;

        mfxU32 pitch = surf->Data.PitchLow + ((mfxU32)surf->Data.PitchHigh << 16);

        switch (umc_frame_data.GetInfo()->GetColorFormat())
        {
        case UMC::NV16:
        case UMC::NV12:
            umc_frame_data.SetPlanePointer(surf->Data.Y, 0, pitch);
            umc_frame_data.SetPlanePointer(surf->Data.U, 1, pitch);
            break;
        case UMC::YUV420:
        case UMC::YUV422:
            umc_frame_data.SetPlanePointer(surf->Data.Y, 0, pitch);
            umc_frame_data.SetPlanePointer(surf->Data.U, 1, pitch >> 1);
            umc_frame_data.SetPlanePointer(surf->Data.V, 2, pitch >> 1);
            break;
        case UMC::IMC3:
            umc_frame_data.SetPlanePointer(surf->Data.Y, 0, pitch);
            umc_frame_data.SetPlanePointer(surf->Data.U, 1, pitch);
            umc_frame_data.SetPlanePointer(surf->Data.V, 2, pitch);
            break;
        case UMC::RGB32:
        {
            umc_frame_data.SetPlanePointer(surf->Data.B, 0, pitch);
        }
        break;
        case UMC::YUY2:
        {
            umc_frame_data.SetPlanePointer(surf->Data.Y, 0, pitch);
        }
        break;
        default:

            std::ignore = MFX_STS_TRACE(base_core_vpl->Unlock(*surf));
            return nullptr;
        }

        return &umc_frame_data;
    }
    else
    {
        return m_umc_allocator_adapter->Lock(MID);
    }
}

UMC::Status SurfaceSource::Unlock(UMC::FrameMemID MID)
{
    MFX_CHECK(m_redirect_to_vpl_path == !!m_vpl_cache_decoder_surfaces, UMC::UMC_ERR_NOT_INITIALIZED);
    MFX_CHECK(!m_redirect_to_vpl_path == !!m_umc_allocator_adapter, UMC::UMC_ERR_NOT_INITIALIZED);

    if (m_redirect_to_vpl_path)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        mfxFrameSurface1* surf = GetDecoderSurface(MID);
        MFX_CHECK(surf, UMC::UMC_ERR_NULL_PTR);

        auto base_core_vpl = dynamic_cast<CommonCORE_VPL*>(m_core);
        MFX_CHECK(base_core_vpl, UMC::UMC_ERR_NULL_PTR);

        return ConvertStatusMfx2Umc(MFX_STS_TRACE(base_core_vpl->Unlock(*surf)));
    }
    else
    {
        return m_umc_allocator_adapter->Unlock(MID);
    }
}

UMC::Status SurfaceSource::IncreaseReference(UMC::FrameMemID MID)
{
    MFX_CHECK(m_redirect_to_vpl_path == !!m_vpl_cache_decoder_surfaces, UMC::UMC_ERR_NOT_INITIALIZED);
    MFX_CHECK(!m_redirect_to_vpl_path == !!m_umc_allocator_adapter, UMC::UMC_ERR_NOT_INITIALIZED);

    if (m_redirect_to_vpl_path)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        mfxFrameSurface1* surf = GetDecoderSurface(MID);
        MFX_CHECK(surf, UMC::UMC_ERR_NULL_PTR);

        return ConvertStatusMfx2Umc(MFX_STS_TRACE(AddRefSurface(*surf, true)));
    }
    else
    {
        return m_umc_allocator_adapter->IncreaseReference(MID);
    }
}

UMC::Status SurfaceSource::DecreaseReference(UMC::FrameMemID MID)
{
    MFX_CHECK(m_redirect_to_vpl_path == !!m_vpl_cache_decoder_surfaces, UMC::UMC_ERR_NOT_INITIALIZED);
    MFX_CHECK(!m_redirect_to_vpl_path == !!m_umc_allocator_adapter, UMC::UMC_ERR_NOT_INITIALIZED);

    if (m_redirect_to_vpl_path)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        mfxFrameSurface1* surf = GetDecoderSurface(MID);
        MFX_CHECK(surf, UMC::UMC_ERR_NULL_PTR);

        if (surf->FrameInterface && surf->FrameInterface->GetRefCounter)
        {
            mfxU32 counter = 0;
            MFX_CHECK(surf->FrameInterface->GetRefCounter(surf, &counter) == MFX_ERR_NONE, UMC::UMC_ERR_FAILED);

            if (counter == 1)
                RemoveCorrespondence(*surf);
        }

        return ConvertStatusMfx2Umc(MFX_STS_TRACE(ReleaseSurface(*surf, true)));
    }
    else
    {
        return m_umc_allocator_adapter->DecreaseReference(MID);
    }
}

mfxI32 SurfaceSource::FindSurface(mfxFrameSurface1 *surf)
{
    if (m_redirect_to_vpl_path != !!m_vpl_cache_decoder_surfaces)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
        return -1;
    }
    if (!m_redirect_to_vpl_path != !!m_umc_allocator_adapter)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
        return -1;
    }

    if (m_redirect_to_vpl_path)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        if (!surf)
        {
            std::ignore = MFX_STS_TRACE(MFX_ERR_NULL_PTR);
            return -1;
        }

        auto it = m_mfx2umc_memid.find(GetIdentifier(*surf));
        if (it == std::end(m_mfx2umc_memid))
        {
            // First try to find corresponding decoder work surface (case of internal allocation)
            auto it_ws = m_output_work_surface_map.find(GetIdentifier(*surf));

            if (it_ws == std::end(m_output_work_surface_map) || (it = m_mfx2umc_memid.find(it_ws->second->Data.MemId)) == std::end(m_mfx2umc_memid))
            {
                std::ignore = MFX_STS_TRACE(MFX_ERR_INVALID_HANDLE);
                return -1;
            }
        }

        return it->second;
    }
    else
    {
        return m_umc_allocator_adapter->FindSurface(surf);
    }
}

mfxFrameSurface1* SurfaceSource::GetInternalSurface(mfxFrameSurface1* sfc_surf) {

    auto decSurfIt = m_output_work_surface_map.find(sfc_surf->Data.MemId);
    if (decSurfIt == std::end(m_output_work_surface_map))
    {
        return nullptr;
    }

    return decSurfIt->second;
}

mfxStatus SurfaceSource::SetCurrentMFXSurface(mfxFrameSurface1 *surf)
{
    MFX_CHECK(m_redirect_to_vpl_path == !!m_vpl_cache_decoder_surfaces, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(!m_redirect_to_vpl_path == !!m_umc_allocator_adapter, MFX_ERR_NOT_INITIALIZED);

    if (m_redirect_to_vpl_path)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        // Stop the timer on new DecodeFrameAsync call
        m_timer.Stop();

        m_memory_model2 = surf != nullptr;

        if (!surf)
            return MFX_ERR_NONE;

        MFX_CHECK(surf->Data.Locked == 0, MFX_ERR_MORE_SURFACE);

        MFX_SAFE_CALL(m_vpl_cache_output_surfaces->Update(*surf));

        // Memory model 2, non-null work surface passed

        // If we try to set the same surface twice in a row, do nothing
        auto it = m_output_work_surface_map.find(GetIdentifier(*surf));

        if (m_current_work_surface && (m_current_work_surface->Data.MemId == surf->Data.MemId || (it != std::end(m_output_work_surface_map) && it->second->Data.MemId == m_current_work_surface->Data.MemId)))
            return MFX_ERR_NONE;

        if (m_allocate_internal)
        {
            // Create internal surface
            mfxFrameSurface1* internal_surf = nullptr;
            MFX_SAFE_CALL((*m_vpl_cache_decoder_surfaces)->GetSurface(internal_surf, true));
            MFX_CHECK_NULL_PTR1(internal_surf);

            // RAII lock to drop refcount in case of error
            surface_refcount_scoped_lock internal_surf_scoped_lock(internal_surf);

            if (!m_need_to_copy_before_output)
            {
                // SFC Linux
                MFX_CHECK(CreateCorrespondence(*internal_surf, *surf), MFX_ERR_UNKNOWN);
            }

            // Drop RAII lock and proceed with internal surface
            surf = internal_surf_scoped_lock.release();
        }

        CreateBinding(*surf);

        if (m_sw_fallback_sys_mem)
        {
            /*
            In case of sw fallback and sys mem we can have no ext allocator.
            In this case we can't manage surface through surface cache.
            Instead we will manage them through m_sw_fallback_surfaces.
            */
            m_sw_fallback_surfaces.push_back(surf);
        }

        ReleaseCurrentWorkSurface();

        m_current_work_surface = surf;

        return MFX_ERR_NONE;
    }
    else
    {
        return m_umc_allocator_adapter->SetCurrentMFXSurface(surf);
    }
}

mfxFrameSurface1 * SurfaceSource::GetSurface(UMC::FrameMemID index, mfxFrameSurface1 *surface, const mfxVideoParam * videoPar)
{
    if (m_redirect_to_vpl_path != !!m_vpl_cache_decoder_surfaces)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
        return nullptr;
    }
    if (!m_redirect_to_vpl_path != !!m_umc_allocator_adapter)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
        return nullptr;
    }

    if (m_redirect_to_vpl_path)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        mfxFrameSurface1* work_surf = GetDecoderSurface(index);
        if (!work_surf)
        {
            std::ignore = MFX_STS_TRACE(MFX_ERR_NULL_PTR);
            return nullptr;
        }

        if (!m_allocate_internal)
        {
            // HW memory

            std::ignore = MFX_STS_TRACE(AddRefSurface(*work_surf, true));

            return work_surf;
        }

        auto it_wo = m_work_output_surface_map.find(work_surf->Data.MemId);

        if (!m_need_to_copy_before_output)
        {
            // SFC Linux

            if (it_wo == std::end(m_work_output_surface_map))
            {
                std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_FOUND);
                return nullptr;
            }

            std::ignore = MFX_STS_TRACE(AddRefSurface(*(it_wo->second), true));

            return it_wo->second;
        }

        // SW surfaces
        if (!surface)
        {
            // Model 3: null work_surface passed by user
            // Allocate SW output surface here
            mfxStatus sts = (*m_vpl_cache_output_surfaces)->GetSurface(surface, true);
            if (MFX_STS_TRACE(sts) != MFX_ERR_NONE)
            {
                return nullptr;
            }

            if (!surface)
            {
                std::ignore = MFX_STS_TRACE(MFX_ERR_NULL_PTR);
                return nullptr;
            }
        }

        std::ignore = MFX_STS_TRACE(AddRefSurface(*surface, true));

        if (it_wo == std::end(m_work_output_surface_map))
        {
            // Create mapping between HW <-> SW surfaces
            if (!CreateCorrespondence(*work_surf, *surface))
            {
                std::ignore = MFX_STS_TRACE(MFX_ERR_UNKNOWN);
                return nullptr;
            }
        }
        else
        {
            // Both decoder's and output surfaces already in use
            if (m_output_work_surface_skip_frames.find(surface) != std::end(m_output_work_surface_skip_frames) || it_wo->second == surface)
            {
                std::ignore = MFX_STS_TRACE(MFX_ERR_UNKNOWN);
                return nullptr;
            }

            /*
            Decoder's surface already in use but output surface is new one.
            Current frame is skip frame and use previous work surface but new user surface
            */
            if (m_core->IncreaseReference(*surface) != MFX_ERR_NONE)
            {
                std::ignore = MFX_STS_TRACE(MFX_ERR_UNKNOWN);
                return nullptr;
            }

            /*
            Output surface mapped on work surface wich is already in m_work_output_surface_map.
            It is possible in case of skip frame.
            So, can't use m_work_output_surface_map and use m_output_work_surface_skip_frames instead
            */
            m_output_work_surface_skip_frames.insert({ surface, work_surf });
        }

        return surface;

    }
    else
    {
        return m_umc_allocator_adapter->GetSurface(index, surface, videoPar);
    }
}

mfxFrameSurface1 * SurfaceSource::GetInternalSurface(UMC::FrameMemID index)
{
    if (m_redirect_to_vpl_path != !!m_vpl_cache_decoder_surfaces)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
        return nullptr;
    }
    if (!m_redirect_to_vpl_path != !!m_umc_allocator_adapter)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
        return nullptr;
    }

    if (m_redirect_to_vpl_path)
    {
        if (!m_allocate_internal)
            return nullptr;

        return GetSurfaceByIndex(index);
    }
    else
    {
        return m_umc_allocator_adapter->GetInternalSurface(index);
    }
}

mfxStatus SurfaceSource::GetSurface(mfxFrameSurface1* & surface, mfxSurfaceHeader* import_surface)
{
    MFX_CHECK(m_redirect_to_vpl_path,      MFX_ERR_UNSUPPORTED);
    MFX_CHECK(m_vpl_cache_output_surfaces, MFX_ERR_NOT_INITIALIZED);

    return (*m_vpl_cache_output_surfaces)->GetSurface(surface, false, import_surface);
}

mfxFrameSurface1 * SurfaceSource::GetSurfaceByIndex(UMC::FrameMemID index)
{
    if (m_redirect_to_vpl_path != !!m_vpl_cache_decoder_surfaces)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
        return nullptr;
    }
    if (!m_redirect_to_vpl_path != !!m_umc_allocator_adapter)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
        return nullptr;
    }

    if (m_redirect_to_vpl_path)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        mfxFrameSurface1* surf = GetDecoderSurface(index);
        return surf;
    }
    else
    {
        return m_umc_allocator_adapter->GetSurfaceByIndex(index);
    }
}

mfxStatus SurfaceSource::PrepareToOutput(mfxFrameSurface1 *surface_out, UMC::FrameMemID index, const mfxVideoParam * videoPar, mfxU32 gpuCopyMode)
{
    MFX_CHECK(m_redirect_to_vpl_path == !!m_vpl_cache_decoder_surfaces, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(!m_redirect_to_vpl_path == !!m_umc_allocator_adapter, MFX_ERR_NOT_INITIALIZED);

    if (m_redirect_to_vpl_path)
    {
        MFX_CHECK_NULL_PTR1(surface_out);

        UMC::AutomaticUMCMutex guard(m_guard);

        if (m_allocate_internal && m_need_to_copy_before_output)
        {
            std::unique_ptr<mfxFrameSurface1, std::function<void(mfxFrameSurface1*)>> srcSurface;

            auto it = m_output_work_surface_map.find(GetIdentifier(*surface_out));
            if (it != std::end(m_output_work_surface_map))
            {
                srcSurface = std::unique_ptr<mfxFrameSurface1, std::function<void(mfxFrameSurface1*)>>(it->second, 
                    [this](mfxFrameSurface1* surf) {RemoveCorrespondence(*surf);});
            }
            else // One decoder's surface used for several output surfaces (skip frame case)
            {
                auto itSkipFrame = m_output_work_surface_skip_frames.find(surface_out);
                MFX_CHECK(itSkipFrame != std::end(m_output_work_surface_skip_frames), MFX_ERR_NOT_FOUND);

                srcSurface = std::unique_ptr<mfxFrameSurface1, std::function<void(mfxFrameSurface1*)>>(itSkipFrame->second,
                    [&surface_out, this](mfxFrameSurface1*) 
                {
                    // Need to decrease output surface ref count after m_core->DoFastCopyWrapper
                    std::ignore = MFX_STS_TRACE(m_core->DecreaseReference(*surface_out));
                });
                m_output_work_surface_skip_frames.erase(itSkipFrame);
            }

            MFX_SAFE_CALL(m_core->DoFastCopyWrapper(surface_out,
                // When this is user provided SW memory surface it might not have correct type set
                surface_out->Data.MemType ? surface_out->Data.MemType : MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                srcSurface.get(),
                srcSurface->Data.MemType,
                gpuCopyMode
            ));
        }

        return MFX_ERR_NONE;
    }
    else
    {
        return m_umc_allocator_adapter->PrepareToOutput(surface_out, index, videoPar, gpuCopyMode);
    }
}

bool SurfaceSource::HasFreeSurface()
{
    if (m_redirect_to_vpl_path != !!m_vpl_cache_decoder_surfaces)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
        return false;
    }
    if (!m_redirect_to_vpl_path != !!m_umc_allocator_adapter)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
        return false;
    }

    if (m_redirect_to_vpl_path)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        return (m_memory_model2 && !m_need_to_copy_before_output) ? (m_current_work_surface != nullptr) : true;
    }
    else
    {
        return m_umc_allocator_adapter->HasFreeSurface();
    }
}

bool SurfaceSource::GetSurfaceType()
{
    return m_redirect_to_vpl_path;
}

void SurfaceSource::SetFreeSurfaceAllowedFlag(bool flag)
{
    if (m_redirect_to_vpl_path != !!m_vpl_cache_decoder_surfaces)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
    }
    if (!m_redirect_to_vpl_path != !!m_umc_allocator_adapter)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
        return;
    }

    if (m_redirect_to_vpl_path)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
    }
    else
    {
        m_umc_allocator_adapter->SetSfcPostProcessingFlag(flag);
    }
}

// D3D functionality
// we should copy to external SW surface
mfxStatus   mfx_UMC_FrameAllocator_D3D::PrepareToOutput(mfxFrameSurface1 *surface_work, UMC::FrameMemID index, const mfxVideoParam *, mfxU32 gpuCopyMode)
{
    UMC::AutomaticUMCMutex guard(m_guard);

    mfxStatus sts = MFX_ERR_NONE;
    mfxMemId memInternal = m_frameDataInternal.GetSurface(index).Data.MemId;
    mfxMemId memId = m_pCore->MapIdx(memInternal);

    if ((surface_work->Data.MemId)&&
        (surface_work->Data.MemId == memId))
    {
        mfxHDLPair surfHDLExt = {};
        mfxHDLPair surfHDLInt = {};

        MFX_CHECK_STS(m_pCore->GetExternalFrameHDL(surface_work->Data.MemId, &surfHDLExt.first, false));
        MFX_CHECK_STS(m_pCore->GetFrameHDL(memId, &surfHDLInt.first, false));

        if (surfHDLExt.first == surfHDLInt.first && surfHDLExt.second == surfHDLInt.second) // The same frame. No need to do anything
            return MFX_ERR_NONE;
    }

    if (!m_sfcVideoPostProcessing)
    {
        mfxFrameSurface1 & internalSurf = m_frameDataInternal.GetSurface(index);
        mfxFrameSurface1 surface = MakeSurface(internalSurf.Info, internalSurf.Data.MemId);
        mfxU16 outMemType = static_cast<mfxU16>((m_IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY ? MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_DXVA2_DECODER_TARGET) |
                                                                                MFX_MEMTYPE_EXTERNAL_FRAME);
        //Performance issue. We need to unlock mutex to let decoding thread run async.
        guard.Unlock();
        sts = m_pCore->DoFastCopyWrapper(surface_work,
                                            outMemType,
                                            &surface,
                                            MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET,
                                            gpuCopyMode
                                            );
        guard.Lock();
        MFX_CHECK_STS(sts);
    }

    if (!m_IsUseExternalFrames)
    {
        if (!m_sfcVideoPostProcessing)
        {
            m_pCore->DecreaseReference(&surface_work->Data);
            m_extSurfaces[index].FrameSurface = 0;
        }
    }

    return sts;
}
