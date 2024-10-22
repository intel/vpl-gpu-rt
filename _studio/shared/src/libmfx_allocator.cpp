// Copyright (c) 2007-2024 Intel Corporation
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

#include "mfxvideo++int.h"
#include "libmfx_allocator.h"


#include "ippcore.h"
#include "ipps.h"

#include "mfx_utils.h"
#include "mfx_common.h"

#include <stdlib.h>

#define ALIGN32(X) (((mfxU32)((X)+31)) & (~ (mfxU32)31))
#define ID_BUFFER MFX_MAKEFOURCC('B','U','F','F')
#define ID_FRAME  MFX_MAKEFOURCC('F','R','M','E')

#define ERROR_STATUS(sts) ((sts)<MFX_ERR_NONE)

#define DEFAULT_ALIGNMENT_SIZE 32

// Implementation of Internal allocators
mfxStatus mfxDefaultAllocator::AllocBuffer(mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxHDL *mid)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;
    if(!mid)
        return MFX_ERR_NULL_PTR;
    mfxU32 header_size = ALIGN32(sizeof(BufferStruct));
    mfxU8 *buffer_ptr=(mfxU8 *)malloc(header_size + nbytes + DEFAULT_ALIGNMENT_SIZE);

    if (!buffer_ptr)
        return MFX_ERR_MEMORY_ALLOC;

    memset(buffer_ptr, 0, header_size + nbytes);

    BufferStruct *bs=(BufferStruct *)buffer_ptr;
    bs->allocator = pthis;
    bs->id = ID_BUFFER;
    bs->type = type;
    bs->nbytes = nbytes;

    // save index
    {
        mfxWideBufferAllocator* pBA = (mfxWideBufferAllocator*)pthis;
        pBA->m_bufHdl.push_back(bs);
        *mid = (mfxHDL) pBA->m_bufHdl.size();
    }

    return MFX_ERR_NONE;

}

inline
size_t midToSizeT(mfxHDL mid)
{
    return ((Ipp8u *) mid - (Ipp8u *) 0);

} // size_t midToSizeT(mfxHDL mid)

mfxStatus mfxDefaultAllocator::LockBuffer(mfxHDL pthis, mfxHDL mid, mfxU8 **ptr)
{
    //BufferStruct *bs=(BufferStruct *)mid;
    BufferStruct *bs;
    try
    {
        size_t index = midToSizeT(mid);
        if (!pthis)
            return MFX_ERR_INVALID_HANDLE;

        mfxWideBufferAllocator* pBA = (mfxWideBufferAllocator*)pthis;
        if ((index > pBA->m_bufHdl.size())||
            (index == 0))
            return MFX_ERR_INVALID_HANDLE;
        bs = pBA->m_bufHdl[index - 1];
    }
    catch (...)
    {
        return MFX_ERR_INVALID_HANDLE;
    }

    if (ptr) *ptr = UMC::align_pointer<mfxU8*>((mfxU8 *)bs + ALIGN32(sizeof(BufferStruct)), DEFAULT_ALIGNMENT_SIZE);

    return MFX_ERR_NONE;
}
mfxStatus mfxDefaultAllocator::UnlockBuffer(mfxHDL pthis, mfxHDL mid)
{
    try
    {
        if (!pthis)
            return MFX_ERR_INVALID_HANDLE;

        BufferStruct *bs;
        size_t index = midToSizeT(mid);
        mfxWideBufferAllocator* pBA = (mfxWideBufferAllocator*)pthis;
        if (index > pBA->m_bufHdl.size())
            return MFX_ERR_INVALID_HANDLE;

        bs = pBA->m_bufHdl[index - 1];
        if (bs->id!=ID_BUFFER)
            return MFX_ERR_INVALID_HANDLE;
    }
    catch (...)
    {
        return MFX_ERR_INVALID_HANDLE;
    }

    return MFX_ERR_NONE;
}
mfxStatus mfxDefaultAllocator::FreeBuffer(mfxHDL pthis, mfxMemId mid)
{
    try
    {
        if (!pthis)
            return MFX_ERR_INVALID_HANDLE;
        BufferStruct *bs;
        size_t index = midToSizeT(mid);
        mfxWideBufferAllocator* pBA = (mfxWideBufferAllocator*)pthis;
        if (index > pBA->m_bufHdl.size())
            return MFX_ERR_INVALID_HANDLE;

        bs = pBA->m_bufHdl[index - 1];
        if (bs->id!=ID_BUFFER)
            return MFX_ERR_INVALID_HANDLE;
        if (bs->id!=ID_BUFFER)
            return MFX_ERR_INVALID_HANDLE;
        free(bs);
        return MFX_ERR_NONE;
    }
    catch (...)
    {
        return MFX_ERR_INVALID_HANDLE;
    }
}

mfxStatus mfxDefaultAllocator::GetSurfaceSizeInBytes(mfxU32 pitch, mfxU32 height, mfxU32 fourCC, mfxU32& nBytes)
{
    switch (fourCC)
    {
    case MFX_FOURCC_YV12:
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_P010:
    case MFX_FOURCC_P016:
    case MFX_FOURCC_YUV411:
    case MFX_FOURCC_IMC3:
        nBytes = pitch * height + (pitch >> 1) * (height >> 1) + (pitch >> 1) * (height >> 1);
        break;
    case MFX_FOURCC_P210:
    case MFX_FOURCC_YUY2:
    case MFX_FOURCC_YUV422H:
    case MFX_FOURCC_YUV422V:
    case MFX_FOURCC_UYVY:
        nBytes = pitch * height + (pitch >> 1) * height + (pitch >> 1) * height;
        break;
    case MFX_FOURCC_YUV444:
    case MFX_FOURCC_RGB3:
    case MFX_FOURCC_RGBP:
    case MFX_FOURCC_BGRP:
        nBytes = pitch * height + pitch * height + pitch * height;
        break;
    case MFX_FOURCC_RGB565:
        nBytes = 2 * pitch * height;
        break;
    case MFX_FOURCC_BGR4:
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_AYUV:
    case MFX_FOURCC_A2RGB10:
        nBytes = pitch * height + pitch * height + pitch * height + pitch * height;
        break;
    case MFX_FOURCC_Y410:
    case MFX_FOURCC_Y416:
    case MFX_FOURCC_P8:
    case MFX_FOURCC_Y210:
    case MFX_FOURCC_Y216:
    case MFX_FOURCC_P8_TEXTURE:
    case MFX_FOURCC_YUV400:
    case MFX_FOURCC_R16:
    case MFX_FOURCC_ARGB16:
    case MFX_FOURCC_ABGR16:
        nBytes = pitch * height;
        break;
    case MFX_FOURCC_ABGR16F:
        nBytes = (pitch * height + pitch * height + pitch * height + pitch * height) * 2;
        break;
    default:
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
        break;
    }
    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocator::GetNumBytesRequired(const mfxFrameInfo & Info, mfxU32& nbytes, size_t power_of_2_alignment)
{
    mfxU32 Pitch = mfx::align2_value(Info.Width, 32), Height2 = mfx::align2_value(Info.Height, 32);

    MFX_CHECK(Pitch,   MFX_ERR_MEMORY_ALLOC);
    MFX_CHECK(Height2, MFX_ERR_MEMORY_ALLOC);

    // Decoders and Encoders use YV12 and NV12 only
    switch (Info.FourCC) 
    {
    case MFX_FOURCC_P010:
    case MFX_FOURCC_P016:
    case MFX_FOURCC_P210:
        Pitch = mfx::align2_value(Info.Width * 2, 32);
        break;
    case MFX_FOURCC_Y410:
    case MFX_FOURCC_Y210:
    case MFX_FOURCC_Y216:
        Pitch = mfx::align2_value(Info.Width * 4, 32);
        break;
    case MFX_FOURCC_Y416:
        Pitch = mfx::align2_value(Info.Width * 8, 32);
        break;
    default:
        break;
    }

    MFX_SAFE_CALL(mfxDefaultAllocator::GetSurfaceSizeInBytes(Pitch, Height2, Info.FourCC, nbytes));
    // Allocate SW memmory with page aligned size
    nbytes = mfx::align2_value(nbytes, power_of_2_alignment);

    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocator::AllocFrames(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxU32 numAllocated, maxNumFrames;
    mfxWideSWFrameAllocator *pSelf = (mfxWideSWFrameAllocator*)pthis;

    // frames were allocated
    // return existent frames
    if (pSelf->NumFrames)
    {
        if (request->NumFrameSuggested > pSelf->NumFrames)
            return MFX_ERR_MEMORY_ALLOC;
        else
        {
            response->mids = &pSelf->m_frameHandles[0];
            return MFX_ERR_NONE;
        }
    }

    mfxU32 nbytes = 0;
    mfxStatus sts = mfxDefaultAllocator::GetNumBytesRequired(request->Info, nbytes);
    MFX_CHECK_STS(sts);

    // allocate frames in cycle
    maxNumFrames = request->NumFrameSuggested;
    pSelf->m_frameHandles.resize(request->NumFrameSuggested);
    for (numAllocated = 0; numAllocated < maxNumFrames; numAllocated += 1)
    {
        sts = (pSelf->wbufferAllocator.bufferAllocator.Alloc)(pSelf->wbufferAllocator.bufferAllocator.pthis, nbytes + ALIGN32(sizeof(FrameStruct)), request->Type, &pSelf->m_frameHandles[numAllocated]);
        if (ERROR_STATUS(sts)) break;

        FrameStruct *fs;
        sts = (pSelf->wbufferAllocator.bufferAllocator.Lock)(pSelf->wbufferAllocator.bufferAllocator.pthis, pSelf->m_frameHandles[numAllocated], (mfxU8 **)&fs);
        if (ERROR_STATUS(sts)) break;
        fs->id = ID_FRAME;
        fs->info = request->Info;
        (pSelf->wbufferAllocator.bufferAllocator.Unlock)(pSelf->wbufferAllocator.bufferAllocator.pthis, pSelf->m_frameHandles[numAllocated]);
    }
    response->mids = &pSelf->m_frameHandles[0];
    response->NumFrameActual = (mfxU16) numAllocated;

    // check the number of allocated frames
    if (numAllocated < request->NumFrameMin)
    {
        FreeFrames(pSelf, response);
        return MFX_ERR_MEMORY_ALLOC;
    }
    pSelf->NumFrames = maxNumFrames;

    return MFX_ERR_NONE;
}

static inline std::pair<mfxU16, mfxU16> pitch_from_width(mfxU32 width, mfxU32 multiplier)
{
    mfxU32 pitch = multiplier * mfx::align2_value(width, 32u);

    return { mfxU16(pitch >> 16), mfxU16(pitch & 0xffff) };
}

static inline size_t pitch_from_frame_data(const mfxFrameData& frame_data)
{

    return (size_t(frame_data.PitchHigh) << 16) + frame_data.PitchLow;
}

static inline mfxStatus SetPointers(mfxFrameData& frame_data, const mfxFrameInfo & info, mfxU8* bytes)
{
    clear_frame_data(frame_data);

    mfxU32 Height2 = mfx::align2_value(info.Height, 32u);
    switch (info.FourCC)
    {
    case MFX_FOURCC_NV12:
    {
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 1u);
        size_t pitch = pitch_from_frame_data(frame_data);

        frame_data.Y = bytes;
        frame_data.U = frame_data.Y + pitch * Height2;
        frame_data.V = frame_data.U + 1;
    }
        break;
    case MFX_FOURCC_P010:
    case MFX_FOURCC_P016:
    {
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 2u);
        size_t pitch = pitch_from_frame_data(frame_data);

        frame_data.Y = bytes;
        frame_data.U = frame_data.Y + pitch * Height2;
        frame_data.V = frame_data.U + 2;
    }
        break;
    case MFX_FOURCC_P210:
    {
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 2u);
        size_t pitch = pitch_from_frame_data(frame_data);

        frame_data.Y = bytes;
        frame_data.U = frame_data.Y + pitch * Height2;
        frame_data.V = frame_data.U + 2;
    }
        break;
    case MFX_FOURCC_YV12:
    {
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 1u);
        size_t pitch = pitch_from_frame_data(frame_data);

        frame_data.Y = bytes;
        frame_data.V = frame_data.Y + pitch * Height2;
        frame_data.U = frame_data.V + (pitch >> 1) * (Height2 >> 1);
    }
        break;
    case MFX_FOURCC_YUY2:
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 2u);
        frame_data.Y = bytes;
        frame_data.U = frame_data.Y + 1;
        frame_data.V = frame_data.Y + 3;
        break;
    case MFX_FOURCC_UYVY:
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 2u);
        frame_data.U = bytes;
        frame_data.Y = frame_data.Y + 1;
        frame_data.V = frame_data.Y + 2;
        break;
#if defined (MFX_ENABLE_FOURCC_RGB565)
    case MFX_FOURCC_RGB565:
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 2u);
        frame_data.B = bytes;
        frame_data.G = frame_data.B;
        frame_data.R = frame_data.B;
        break;
#endif
    case MFX_FOURCC_RGB3:
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 3u);
        frame_data.B = bytes;
        frame_data.G = frame_data.B + 1;
        frame_data.R = frame_data.B + 2;
        break;
#ifdef MFX_ENABLE_RGBP
    case MFX_FOURCC_RGBP:
    {
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 1u);
        size_t pitch = pitch_from_frame_data(frame_data);

        frame_data.R = bytes;
        frame_data.G = frame_data.R + pitch * Height2;
        frame_data.B = frame_data.R + 2 * pitch * Height2;
    }
        break;
#endif
    case MFX_FOURCC_BGRP:
    {
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 1u);
        size_t pitch = pitch_from_frame_data(frame_data);

        frame_data.B = bytes;
        frame_data.G = frame_data.B + pitch * Height2;
        frame_data.R = frame_data.B + 2 * pitch * Height2;
    }
        break;
    case MFX_FOURCC_RGB4:
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 4u);
        frame_data.B = bytes;
        frame_data.G = frame_data.B + 1;
        frame_data.R = frame_data.B + 2;
        frame_data.A = frame_data.B + 3;
        break;
    case MFX_FOURCC_BGR4:
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 4u);
        frame_data.R = bytes;
        frame_data.G = frame_data.R + 1;
        frame_data.B = frame_data.R + 2;
        frame_data.A = frame_data.R + 3;
        break;
    case MFX_FOURCC_A2RGB10:
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 4u);
        frame_data.R = frame_data.G = frame_data.B = frame_data.A = bytes;
        break;
    case MFX_FOURCC_P8:
        // Linear data buffer, so pitch is the size of buffer
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, Height2);
        frame_data.Y = bytes;
        break;
    case MFX_FOURCC_P8_TEXTURE:
        // 2-D data buffer, so pitch is width of picture
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 1u);
        frame_data.Y = bytes;
        break;
    case MFX_FOURCC_AYUV:
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 4u);
        frame_data.V = bytes;
        frame_data.U = frame_data.V + 1;
        frame_data.Y = frame_data.V + 2;
        frame_data.A = frame_data.V + 3;
        break;
    case MFX_FOURCC_Y210:
    case MFX_FOURCC_Y216:
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 4u);
        frame_data.Y16 = (mfxU16*)bytes;
        frame_data.U16 = frame_data.Y16 + 1;
        frame_data.V16 = frame_data.Y16 + 3;
        break;

    case MFX_FOURCC_Y410:
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 4u);
        frame_data.Y410 = (mfxY410*)bytes;
        break;

    case MFX_FOURCC_Y416:
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 8u);
        frame_data.U16 = (mfxU16*)bytes;
        frame_data.Y16 = frame_data.U16 + 1;
        frame_data.V16 = frame_data.Y16 + 1;
        frame_data.A   = (mfxU8 *)(frame_data.V16 + 1);
        break;
    case MFX_FOURCC_ABGR16F:
        std::tie(frame_data.PitchHigh, frame_data.PitchLow) = pitch_from_width(info.Width, 8u);
        frame_data.R = frame_data.G = frame_data.B = frame_data.A = bytes;
        break;

    default:
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    return MFX_ERR_NONE;
}


mfxStatus mfxDefaultAllocator::LockFrame(mfxHDL pthis, mfxHDL mid, mfxFrameData *ptr)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideSWFrameAllocator *pSelf = (mfxWideSWFrameAllocator*)pthis;

    // The default LockFrame is to simulate using LockBuffer.
    FrameStruct *fs;
    mfxStatus sts = (pSelf->wbufferAllocator.bufferAllocator.Lock)(pSelf->wbufferAllocator.bufferAllocator.pthis, mid,(mfxU8 **)&fs);
    if (ERROR_STATUS(sts)) return sts;
    if (fs->id!=ID_FRAME)
    {
        (pSelf->wbufferAllocator.bufferAllocator.Unlock)(pSelf->wbufferAllocator.bufferAllocator.pthis, mid);
        return MFX_ERR_INVALID_HANDLE;
    }

    //ptr->MemId = mid; !!!!!!!!!!!!!!!!!!!!!!!!!

    mfxU8 *sptr = (mfxU8 *)fs+mfx::align2_value(sizeof(FrameStruct), 32);
    return SetPointers(*ptr, fs->info, sptr);
}
mfxStatus mfxDefaultAllocator::GetHDL(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    *handle = mid;
    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocator::UnlockFrame(mfxHDL pthis, mfxHDL mid, mfxFrameData *ptr)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideSWFrameAllocator *pSelf = (mfxWideSWFrameAllocator*)pthis;

    // The default UnlockFrame behavior is to simulate using UnlockBuffer
    mfxStatus sts = (pSelf->wbufferAllocator.bufferAllocator.Unlock)(pSelf->wbufferAllocator.bufferAllocator.pthis, mid);
    if (ERROR_STATUS(sts)) return sts;
    if (ptr) {
        ptr->PitchHigh=0;
        ptr->PitchLow=0;
        ptr->U=ptr->V=ptr->Y=nullptr;
        ptr->A=ptr->R=ptr->G=ptr->B=nullptr;
    }
    return sts;

} // mfxStatus SWVideoCORE::UnlockFrame(mfxHDL mid, mfxFrameData *ptr)
mfxStatus mfxDefaultAllocator::FreeFrames(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideSWFrameAllocator *pSelf = (mfxWideSWFrameAllocator*)pthis;
    mfxU32 i;
    // free all allocated frames in cycle
    for (i = 0; i < response->NumFrameActual; i += 1)
    {
        if (response->mids[i])
        {
            (pSelf->wbufferAllocator.bufferAllocator.Free)(pSelf->wbufferAllocator.bufferAllocator.pthis, response->mids[i]);
        }
    }

    pSelf->m_frameHandles.clear();

    return MFX_ERR_NONE;

} // mfxStatus SWVideoCORE::FreeFrames(void)

mfxWideBufferAllocator::mfxWideBufferAllocator()
{
    memset(bufferAllocator.reserved, 0, sizeof(bufferAllocator.reserved));

    bufferAllocator.Alloc = &mfxDefaultAllocator::AllocBuffer;
    bufferAllocator.Lock =  &mfxDefaultAllocator::LockBuffer;
    bufferAllocator.Unlock = &mfxDefaultAllocator::UnlockBuffer;
    bufferAllocator.Free = &mfxDefaultAllocator::FreeBuffer;

    bufferAllocator.pthis = 0;
}

mfxWideBufferAllocator::~mfxWideBufferAllocator()
{
    memset((void*)&bufferAllocator, 0, sizeof(mfxBufferAllocator));
}
mfxBaseWideFrameAllocator::mfxBaseWideFrameAllocator(mfxU16 type)
    : NumFrames(0)
    , type(type)
{
    memset((void*)&frameAllocator, 0, sizeof(frameAllocator));

}
mfxBaseWideFrameAllocator::~mfxBaseWideFrameAllocator()
{
    memset((void*)&frameAllocator, 0, sizeof(mfxFrameAllocator));
}
mfxWideSWFrameAllocator::mfxWideSWFrameAllocator(mfxU16 type):mfxBaseWideFrameAllocator(type)
{
    frameAllocator.Alloc = &mfxDefaultAllocator::AllocFrames;
    frameAllocator.Lock = &mfxDefaultAllocator::LockFrame;
    frameAllocator.GetHDL = &mfxDefaultAllocator::GetHDL;
    frameAllocator.Unlock = &mfxDefaultAllocator::UnlockFrame;
    frameAllocator.Free = &mfxDefaultAllocator::FreeFrames;
}

std::atomic<uint32_t> FrameAllocatorBase::m_allocator_num(0u);

mfxStatus FrameAllocatorBase::Synchronize(mfxSyncPoint sp, mfxU32 timeout)
{
    if (sp == nullptr)
        return MFX_ERR_NONE;

    MFX_CHECK(m_session != nullptr, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_HDL(m_session->m_pScheduler);

    mfxStatus sts = MFXVideoCORE_SyncOperation(m_session, sp, timeout);

    // We ignore nullptr sts because in this case mfxFrameSurface1 has been already synchronized
    MFX_RETURN(sts != MFX_ERR_NULL_PTR ? sts : MFX_ERR_NONE);
}

mfxStatus RWAcessSurface::LockRW(std::unique_lock<std::mutex>& guard, bool write, bool nowait)
{
    if (write)
    {
        // Try to lock for write, if not succeeded - return
        MFX_CHECK(!Locked(), MFX_ERR_LOCK_MEMORY);

        m_write_lock = true;
    }
    else
    {
        if (m_write_lock)
        {
            // If there exists lock for write - and nowait is set, return error; otherwise wait untill writing finished
            MFX_CHECK(!nowait, MFX_ERR_RESOURCE_MAPPED);

            m_wait_before_read.wait(guard, [this] { return !m_write_lock; });
        }
        else if (!m_read_locks && !nowait)
        {
            guard.unlock();
            MFX_SAFE_CALL(mfxFrameSurfaceBaseInterface::Synchronize(MFX_INFINITE));
            guard.lock();
        }

        ++m_read_locks;
    }

    return MFX_ERR_NONE;
}

mfxStatus RWAcessSurface::UnlockRW()
{
    if (m_write_lock)
    {
        m_write_lock = false;

        m_wait_before_read.notify_all();
    }
    else
    {
        // Check if this surface really was locked
        MFX_CHECK(m_read_locks, MFX_ERR_UNDEFINED_BEHAVIOR);
        --m_read_locks;
    }

    return MFX_ERR_NONE;
}

void mfxSurfaceBase::Close()
{
    if (m_p_base_surface)
        m_p_base_surface->DetachExported(this);
}

mfxStatus mfxSurfaceBase::Synchronize(mfxU32 timeout)
{
    if (!m_p_base_surface)
        return MFX_ERR_NONE;

    return m_p_base_surface->Synchronize(timeout);
}

mfxFrameSurface1_sw::mfxFrameSurface1_sw(const mfxFrameInfo & info, mfxU16 type, mfxMemId mid, std::shared_ptr<staging_adapter_stub>&, mfxHDL, mfxU32, FrameAllocatorBase& allocator)
    : RWAcessSurface(info, type, mid, allocator)
    , m_data(nullptr, free)
{
    MFX_CHECK_WITH_THROW_STS(m_internal_surface.Data.MemType & MFX_MEMTYPE_SYSTEM_MEMORY, MFX_ERR_UNSUPPORTED);

    mfxU32 nbytes = 0;
    mfxStatus sts = mfxDefaultAllocator::GetNumBytesRequired(info, nbytes, BASE_SIZE_ALIGN);
    MFX_CHECK_WITH_THROW_STS(sts == MFX_ERR_NONE, sts);

    m_data.reset(reinterpret_cast<mfxU8*>(aligned_alloc(BASE_ADDR_ALIGN, nbytes)));

    MFX_CHECK_WITH_THROW_STS(m_data, MFX_ERR_MEMORY_ALLOC);
}

mfxStatus mfxFrameSurface1_sw::Lock(mfxU32 flags)
{
    MFX_CHECK(FrameAllocatorBase::CheckMemoryFlags(flags), MFX_ERR_LOCK_MEMORY);

    std::unique_lock<std::mutex> guard(m_mutex);

    mfxStatus sts = LockRW(guard, flags & MFX_MAP_WRITE, flags & MFX_MAP_NOWAIT);
    MFX_CHECK_STS(sts);

    auto Unlock = [](RWAcessSurface* s) { s->UnlockRW(); };

    // Scope guard to decrease locked count if real lock fails
    std::unique_ptr<RWAcessSurface, decltype(Unlock)> scoped_lock(this, Unlock);

    if (NumReaders() < 2)
    {
        // First reader or unique writer has just acquired resource
        sts = SetPointers(m_internal_surface.Data, m_internal_surface.Info, m_data.get());
        MFX_CHECK_STS(sts);
    }

    // No error, remove guard without decreasing locked counter
    scoped_lock.release();

    return MFX_ERR_NONE;
}

mfxStatus mfxFrameSurface1_sw::Unlock()
{
    std::unique_lock<std::mutex> guard(m_mutex);

    MFX_SAFE_CALL(UnlockRW());

    if (NumReaders() == 0) // So it was 1 before UnlockRW
    {
        clear_frame_data(m_internal_surface.Data);
    }

    return MFX_ERR_NONE;
}

mfxStatus mfxFrameSurface1_sw::Realloc(const mfxFrameInfo & info)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    MFX_CHECK(!Locked(), MFX_ERR_LOCK_MEMORY);

    mfxU32 nbytes = 0;
    MFX_SAFE_CALL(mfxDefaultAllocator::GetNumBytesRequired(info, nbytes, BASE_SIZE_ALIGN));

    m_data.reset(reinterpret_cast<mfxU8*>(aligned_alloc(BASE_ADDR_ALIGN, nbytes)));

    MFX_CHECK(m_data, MFX_ERR_MEMORY_ALLOC);

    m_internal_surface.Info = info;

    return SetPointers(m_internal_surface.Data, m_internal_surface.Info, m_data.get());
}

mfx::mfx_shared_lib_holder* ImportExportHelper::GetHelper(mfxSurfaceType shared_library_type)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Check if requested library was already loaded
    auto it = find(shared_library_type);
    if (it != end())
        return (it->second).get();

    lock.unlock();
    mfx::mfx_shared_lib_holder* ret = nullptr;
    uniq_ptr_mfx_shared_lib_holder loaded_lib;


    if (loaded_lib)
    {
        lock.lock();
        ret = loaded_lib.get();
        operator[](shared_library_type) = std::move(loaded_lib);
    }

    return ret;
}
