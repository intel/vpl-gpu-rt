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


#include "mfx_common.h"

#include <algorithm>
#include <vector>

#include "ippcore.h"

#include "libmfx_allocator_vaapi.h"
#include "mfx_utils.h"
#include "mfx_ext_buffers.h"

/*
    These memory types and corresponding processing functions used not only by VP8
    but for VP9 encoder as well.
*/

enum {
    MFX_FOURCC_VP8_NV12    = MFX_MAKEFOURCC('V','P','8','N'),
    MFX_FOURCC_VP8_MBDATA  = MFX_MAKEFOURCC('V','P','8','M'),
    MFX_FOURCC_VP8_SEGMAP  = MFX_MAKEFOURCC('V','P','8','S'),
};

static inline mfxU32 ConvertVP8FourccToMfxFourcc(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_VP8_NV12:
    case MFX_FOURCC_VP8_MBDATA:
        return MFX_FOURCC_NV12;
    case MFX_FOURCC_VP8_SEGMAP:
        return MFX_FOURCC_P8;

    default:
        return fourcc;
    }
}

static inline mfxU32 ConvertMfxFourccToVAFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
        return VA_FOURCC_NV12;
    case MFX_FOURCC_YUY2:
        return VA_FOURCC_YUY2;
    case MFX_FOURCC_YV12:
        return VA_FOURCC_YV12;
#if defined (MFX_ENABLE_FOURCC_RGB565)
    case MFX_FOURCC_RGB565:
        return VA_FOURCC_RGB565;
#endif
    case MFX_FOURCC_RGB4:
        return VA_FOURCC_ARGB;
    case MFX_FOURCC_BGR4:
        return VA_FOURCC_ABGR;
    case MFX_FOURCC_A2RGB10:
        return VA_FOURCC_ARGB;  // rt format will be VA_RT_FORMAT_RGB32_10BPP
#ifdef MFX_ENABLE_RGBP
    case MFX_FOURCC_RGBP:
        return VA_FOURCC_RGBP;
#endif
    case MFX_FOURCC_BGRP:
        return VA_FOURCC_BGRP;
    case MFX_FOURCC_P8:
        return VA_FOURCC_P208;
    case MFX_FOURCC_UYVY:
        return VA_FOURCC_UYVY;
    case MFX_FOURCC_P010:
        return VA_FOURCC_P010;
    case MFX_FOURCC_AYUV:
        return VA_FOURCC_AYUV;
    case MFX_FOURCC_Y210:
        return VA_FOURCC_Y210;
    case MFX_FOURCC_Y410:
        return VA_FOURCC_Y410;
    case MFX_FOURCC_YUV400:
        return VA_FOURCC_Y800;
    case MFX_FOURCC_YUV411:
        return VA_FOURCC_411P;
    case MFX_FOURCC_YUV444:
        return VA_FOURCC_444P;
    case MFX_FOURCC_P016:
        return VA_FOURCC_P016;
    case MFX_FOURCC_Y216:
        return VA_FOURCC_Y216;
    case MFX_FOURCC_Y416:
        return VA_FOURCC_Y416;
    case MFX_FOURCC_YUV422H:
        return VA_FOURCC_422H;
    case MFX_FOURCC_YUV422V:
        return VA_FOURCC_422V;
    case MFX_FOURCC_I420:
        return VA_FOURCC_I420;
    case MFX_FOURCC_IMC3:
        return VA_FOURCC_IMC3;
    default:
        return 0;
    }
}

static void FillSurfaceAttrs(std::vector<VASurfaceAttrib> &attrib, unsigned int &format, const mfxU32 fourcc, const mfxU32 va_fourcc, const mfxU32 memType)
{
    attrib.clear();
    attrib.reserve(2);

    attrib.resize(attrib.size()+1);
    attrib[0].type            = VASurfaceAttribPixelFormat;
    attrib[0].flags           = VA_SURFACE_ATTRIB_SETTABLE;
    attrib[0].value.type      = VAGenericValueTypeInteger;
    attrib[0].value.value.i   = va_fourcc;

    switch (fourcc)
    {
        case MFX_FOURCC_VP8_NV12:
            // special configuration for NV12 surf allocation for VP8 hybrid encoder is required
            attrib.resize(attrib.size()+1);
            attrib[1].type            = VASurfaceAttribUsageHint;
            attrib[1].flags           = VA_SURFACE_ATTRIB_SETTABLE;
            attrib[1].value.type      = VAGenericValueTypeInteger;
            attrib[1].value.value.i   = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER;
            format                    = va_fourcc;
            break;
        case MFX_FOURCC_VP8_MBDATA:
            // special configuration for MB data surf allocation for VP8 hybrid encoder is required
            attrib[0].value.value.i  = VA_FOURCC_P208;
            format                   = VA_FOURCC_P208;
            break;
        case MFX_FOURCC_NV12:
            format = VA_RT_FORMAT_YUV420;
            break;
        case MFX_FOURCC_UYVY:
        case MFX_FOURCC_YUY2:
            format = VA_RT_FORMAT_YUV422;
            break;
        case MFX_FOURCC_A2RGB10:
            format = VA_RT_FORMAT_RGB32_10BPP;
            break;
        case MFX_FOURCC_YUV400:
            format = VA_RT_FORMAT_YUV400;
            //  Enable this hint as required for creating YUV400 surface for JPEG.
            if ((memType & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
                    && (memType & MFX_MEMTYPE_FROM_DECODE))
            {
                attrib.resize(attrib.size()+1);
                attrib[1].type            = VASurfaceAttribUsageHint;
                attrib[1].flags           = VA_SURFACE_ATTRIB_SETTABLE;
                attrib[1].value.type      = VAGenericValueTypeInteger;
                attrib[1].value.value.i   = VA_SURFACE_ATTRIB_USAGE_HINT_DECODER;
            }
            break;
        case MFX_FOURCC_YUV411:
            format = VA_RT_FORMAT_YUV411;
            break;
        case MFX_FOURCC_YUV422H:
        case MFX_FOURCC_YUV422V:
            format = VA_RT_FORMAT_YUV422;
            break;
        case MFX_FOURCC_YUV444:
            format = VA_RT_FORMAT_YUV444;
            break;
        // only Media_Format_RGBP/Media_Format_BGRP and Media_Format_A8R8G8B8 will use this HINT in driver
        case MFX_FOURCC_RGBP:
            format = VA_RT_FORMAT_RGBP;
            //  Enable this hint as required for creating RGBP surface for JPEG.
            if ((memType & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
                    && (memType & MFX_MEMTYPE_FROM_DECODE))
            {
                attrib.resize(attrib.size()+1);
                attrib[1].type            = VASurfaceAttribUsageHint;
                attrib[1].flags           = VA_SURFACE_ATTRIB_SETTABLE;
                attrib[1].value.type      = VAGenericValueTypeInteger;
                attrib[1].value.value.i   = VA_SURFACE_ATTRIB_USAGE_HINT_DECODER;
            }
            break;
        case MFX_FOURCC_BGRP:
            format = VA_RT_FORMAT_RGBP;
            break;
        case MFX_FOURCC_RGB4:
        case MFX_FOURCC_BGR4:
            format = VA_RT_FORMAT_RGB32;
            //  Enable this hint as required for creating RGB32 surface for MJPEG.
            if ((memType & MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET)
                    && (memType & MFX_MEMTYPE_FROM_ENCODE))
            {
                //  Input Attribute Usage Hint
                attrib.resize(attrib.size()+1);
                attrib[1].flags           = VA_SURFACE_ATTRIB_SETTABLE;
                attrib[1].type            = VASurfaceAttribUsageHint;
                attrib[1].value.type      = VAGenericValueTypeInteger;
                attrib[1].value.value.i   = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER;
            }
            break;
        default:
            format = va_fourcc;
            break;
    }
}

static inline bool isFourCCSupported(mfxU32 va_fourcc)
{
    switch (va_fourcc)
    {
        case VA_FOURCC_NV12:
        case VA_FOURCC_YV12:
        case VA_FOURCC_YUY2:
        case VA_FOURCC_ARGB:
        case VA_FOURCC_ABGR:
#ifdef MFX_ENABLE_RGBP
        case VA_FOURCC_RGBP:
#endif
        case VA_FOURCC_BGRP:
        case VA_FOURCC_UYVY:
        case VA_FOURCC_P208:
        case VA_FOURCC_P010:
        case VA_FOURCC_AYUV:
#if defined (MFX_ENABLE_FOURCC_RGB565)
        case VA_FOURCC_RGB565:
#endif
        case VA_FOURCC_Y210:
        case VA_FOURCC_Y410:
        case VA_FOURCC_Y800:
        case VA_FOURCC_P016:
        case VA_FOURCC_Y216:
        case VA_FOURCC_Y416:
        case VA_FOURCC_411P:
        case VA_FOURCC_444P:
        case VA_FOURCC_422H:
        case VA_FOURCC_422V:
        case VA_FOURCC_I420:
            return true;
        default:
            return false;
    }
}

static mfxStatus ReallocImpl(VADisplay* va_disp, vaapiMemIdInt *vaapi_mid, mfxFrameSurface1 *surf)
{
    MFX_CHECK_NULL_PTR3(va_disp, vaapi_mid, surf);
    MFX_CHECK_NULL_PTR1(vaapi_mid->m_surface);

    // VP8 hybrid driver has weird requirements for allocation of surfaces/buffers for VP8 encoding
    // to comply with them additional logic is required to support regular and VP8 hybrid allocation paths
    mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(surf->Info.FourCC);
    unsigned int va_fourcc = ConvertMfxFourccToVAFormat(mfx_fourcc);

    MFX_CHECK(isFourCCSupported(va_fourcc), MFX_ERR_UNSUPPORTED);

    VAStatus va_res = VA_STATUS_SUCCESS;
    if (MFX_FOURCC_P8 == vaapi_mid->m_fourcc)
    {
        mfxStatus sts = CheckAndDestroyVAbuffer(va_disp, *vaapi_mid->m_surface);
        MFX_CHECK(sts == MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
    }
    else
    {
        va_res = vaDestroySurfaces(va_disp, vaapi_mid->m_surface, 1);
        MFX_CHECK(va_res == VA_STATUS_SUCCESS, MFX_ERR_MEMORY_ALLOC);
        *vaapi_mid->m_surface = VA_INVALID_ID;
    }


    std::vector<VASurfaceAttrib> attrib;
    unsigned int format;
    FillSurfaceAttrs(attrib, format, surf->Info.FourCC, va_fourcc, 0);

    va_res = vaCreateSurfaces(va_disp,
                            format,
                            surf->Info.Width, surf->Info.Height,
                            vaapi_mid->m_surface,
                            1,
                            attrib.data(), attrib.size());
    MFX_CHECK(va_res == VA_STATUS_SUCCESS, MFX_ERR_MEMORY_ALLOC);

    // Update fourcc of reallocated element. VAid was updated automatically
    vaapi_mid->m_fourcc = surf->Info.FourCC;

    return MFX_ERR_NONE;
}

// aka AllocImpl(surface)
mfxStatus mfxDefaultAllocatorVAAPI::ReallocFrameHW(mfxHDL pthis, mfxFrameSurface1 *surf, VASurfaceID *va_surf)
{
    MFX_CHECK_NULL_PTR3(pthis, surf, va_surf);

    mfxWideHWFrameAllocator *self = reinterpret_cast<mfxWideHWFrameAllocator*>(pthis);

    auto it = std::find_if(std::begin(self->m_frameHandles), std::end(self->m_frameHandles),
                            [va_surf](mfxHDL hndl){ return hndl && *(reinterpret_cast<vaapiMemIdInt *>(hndl))->m_surface == *va_surf; });

    MFX_CHECK(it != std::end(self->m_frameHandles), MFX_ERR_MEMORY_ALLOC);

    return ReallocImpl(self->m_pVADisplay, reinterpret_cast<vaapiMemIdInt *>(*it), surf);
}

mfxStatus
mfxDefaultAllocatorVAAPI::AllocFramesHW(
    mfxHDL                  pthis,
    mfxFrameAllocRequest*   request,
    mfxFrameAllocResponse*  response)
{
    MFX_CHECK_NULL_PTR2(request, response);
    MFX_CHECK(pthis, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(request->NumFrameSuggested, MFX_ERR_MEMORY_ALLOC);

    *response = {};

    // VP8/VP9 driver has weird requirements for allocation of surfaces/buffers for VP8/VP9 encoding
    // to comply with them additional logic is required to support regular and VP8/VP9 allocation paths
    mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(request->Info.FourCC);
    unsigned int va_fourcc = ConvertMfxFourccToVAFormat(mfx_fourcc);
    MFX_CHECK(isFourCCSupported(va_fourcc), MFX_ERR_UNSUPPORTED);


    auto self = reinterpret_cast<mfxWideHWFrameAllocator*>(pthis);

    // Enough frames were allocated previously. Return existing frames
    if (self->NumFrames)
    {
        MFX_CHECK(request->NumFrameSuggested <= self->NumFrames, MFX_ERR_MEMORY_ALLOC);

        response->mids           = self->m_frameHandles.data();
        response->NumFrameActual = request->NumFrameSuggested;
        response->AllocId        = request->AllocId;

        return MFX_ERR_NONE;
    }

    // Use temporary storage for preliminary operations. If some of them fail, current state of allocator remain unchanged.
    // When allocation will be finished, just move content of these vectors to internal allocator storage
    std::vector<VASurfaceID>   allocated_surfaces(request->NumFrameSuggested, VA_INVALID_ID);
    std::vector<vaapiMemIdInt> allocated_mids(request->NumFrameSuggested);

    VAStatus  va_res  = VA_STATUS_SUCCESS;
    mfxStatus mfx_res = MFX_ERR_NONE;

    if( VA_FOURCC_P208 != va_fourcc)
    {
        unsigned int format;
        std::vector<VASurfaceAttrib> attrib;
        FillSurfaceAttrs(attrib, format, request->Info.FourCC, va_fourcc, request->Type);

        va_res = vaCreateSurfaces(self->m_pVADisplay,
                            format,
                            request->Info.Width, request->Info.Height,
                            allocated_surfaces.data(),
                            allocated_surfaces.size(),
                            attrib.data(),
                            attrib.size());

        MFX_CHECK(va_res == VA_STATUS_SUCCESS, MFX_ERR_DEVICE_FAILED);
    }
    else
    {
        mfxU32 codedbuf_size, codedbuf_num;
        VABufferType codedbuf_type;

        if (request->Info.FourCC == MFX_FOURCC_VP8_SEGMAP)
        {
            codedbuf_size = request->Info.Width;
            codedbuf_num  = request->Info.Height;
            codedbuf_type = VAEncMacroblockMapBufferType;
        }
        else
        {
            int aligned_width  = mfx::align2_value(request->Info.Width,  32);
            int aligned_height = mfx::align2_value(request->Info.Height, 32);
            codedbuf_size = static_cast<mfxU32>((aligned_width * aligned_height) * 400LL / (16 * 16));

            codedbuf_num  = 1;
            codedbuf_type = VAEncCodedBufferType;
        }

        for (VABufferID& coded_buf : allocated_surfaces)
        {
            va_res = vaCreateBuffer(self->m_pVADisplay,
                      VAContextID(request->AllocId),
                      codedbuf_type,
                      codedbuf_size,
                      codedbuf_num,
                      nullptr,
                      &coded_buf);

            if (va_res != VA_STATUS_SUCCESS)
            {
                // Need to clean up already allocated buffers
                mfx_res = MFX_ERR_DEVICE_FAILED;
                break;
            }
        }
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        // Clean up existing state
        self->NumFrames = 0;
        self->m_frameHandles.clear();
        self->m_frameHandles.reserve(request->NumFrameSuggested);

        self->m_allocatedSurfaces = std::move(allocated_surfaces);

        // Push new frames
        for (mfxU32 i = 0; i < request->NumFrameSuggested; ++i)
        {
            allocated_mids[i].m_surface = &self->m_allocatedSurfaces[i];
            allocated_mids[i].m_fourcc  = request->Info.FourCC;

            self->m_frameHandles.push_back(&allocated_mids[i]);
        }
        response->mids           = self->m_frameHandles.data();
        response->NumFrameActual = request->NumFrameSuggested;
        response->AllocId        = request->AllocId;

        self->NumFrames = self->m_frameHandles.size();

        // Save new frames in internal state
        self->m_allocatedMids     = std::move(allocated_mids);
    }
    else
    {
        // Some of vaCreateBuffer calls failed
        for (VABufferID& coded_buf : allocated_surfaces)
        {
            mfxStatus sts = CheckAndDestroyVAbuffer(self->m_pVADisplay, coded_buf);
            MFX_CHECK_STS(sts);
        }
    }

    return mfx_res;
}

mfxStatus mfxDefaultAllocatorVAAPI::FreeFramesHW(
    mfxHDL                  pthis,
    mfxFrameAllocResponse*  response)
{
    MFX_CHECK(pthis, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK_NULL_PTR1(response);

    if (response->mids)
    {
        auto vaapi_mids = reinterpret_cast<vaapiMemIdInt*>(response->mids[0]);
        MFX_CHECK_NULL_PTR1(vaapi_mids);
        MFX_CHECK_NULL_PTR1(vaapi_mids->m_surface);

        auto self = reinterpret_cast<mfxWideHWFrameAllocator*>(pthis);
        // Make sure that we are asked to clean memory which was allocated by current allocator
        MFX_CHECK(self->m_allocatedSurfaces.data() == vaapi_mids->m_surface, MFX_ERR_UNDEFINED_BEHAVIOR);

        if (ConvertVP8FourccToMfxFourcc(vaapi_mids->m_fourcc) == MFX_FOURCC_P8)
        {
            for (VABufferID& coded_buf : self->m_allocatedSurfaces)
            {
                mfxStatus sts = CheckAndDestroyVAbuffer(self->m_pVADisplay, coded_buf);
                MFX_CHECK_STS(sts);
            }
        }
        else
        {
            // Not Buffered memory
            VAStatus va_sts = vaDestroySurfaces(self->m_pVADisplay, vaapi_mids->m_surface, response->NumFrameActual);
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);
        }
        response->mids = nullptr;

        // Reset internal state
        self->NumFrames = 0;
        self->m_frameHandles.clear();
        self->m_allocatedSurfaces.clear();
        self->m_allocatedMids.clear();
    }
    response->NumFrameActual = 0;

    return MFX_ERR_NONE;
}

static inline mfxU32 SupportedVAfourccToMFXfourcc(mfxU32 va_fourcc)
{
    switch (va_fourcc)
    {
    case VA_FOURCC_RGB565:
        return MFX_FOURCC_RGB565;
    case VA_FOURCC_ARGB:
        return MFX_FOURCC_RGB4;
    case VA_FOURCC_A2R10G10B10:
        return MFX_FOURCC_A2RGB10;
    case VA_FOURCC_ABGR:
        return MFX_FOURCC_BGR4;
    case VA_FOURCC_P208:
        return MFX_FOURCC_NV12;
    case MFX_FOURCC_VP8_SEGMAP:
        return MFX_FOURCC_P8;
    case VA_FOURCC_I420:
        return MFX_FOURCC_I420;
    case VA_FOURCC_P012:
        return VA_FOURCC_P016;
    case VA_FOURCC_Y212:
        return VA_FOURCC_Y216;
    case VA_FOURCC_Y412:
        return VA_FOURCC_Y416;
    default:
        return va_fourcc;
    }
}

mfxStatus mfxDefaultAllocatorVAAPI::SetFrameData(const VAImage &va_image, mfxU32 mfx_fourcc, mfxU8* p_buffer, mfxFrameData& frame_data)
{
    MFX_CHECK_NULL_PTR1(p_buffer);
    MFX_CHECK(mfx_fourcc == SupportedVAfourccToMFXfourcc(va_image.format.fourcc), MFX_ERR_LOCK_MEMORY);

    clear_frame_data(frame_data);

    switch (va_image.format.fourcc)
    {
    case VA_FOURCC_NV12:
        frame_data.Y = p_buffer + va_image.offsets[0];
        frame_data.U = p_buffer + va_image.offsets[1];
        frame_data.V = frame_data.U + 1;
        break;

    case VA_FOURCC_YV12:
        frame_data.Y = p_buffer + va_image.offsets[0];
        frame_data.V = p_buffer + va_image.offsets[1];
        frame_data.U = p_buffer + va_image.offsets[2];
        break;

    case VA_FOURCC_I420:
        frame_data.Y = p_buffer + va_image.offsets[0];
        frame_data.U = p_buffer + va_image.offsets[1];
        frame_data.V = p_buffer + va_image.offsets[2];
        break;

    case VA_FOURCC_YUY2:
        frame_data.Y = p_buffer + va_image.offsets[0];
        frame_data.U = frame_data.Y + 1;
        frame_data.V = frame_data.Y + 3;
        break;

    case VA_FOURCC_UYVY:
        frame_data.U = p_buffer + va_image.offsets[0];
        frame_data.Y = frame_data.U + 1;
        frame_data.V = frame_data.U + 2;
        break;

#if defined (MFX_ENABLE_FOURCC_RGB565)
    case VA_FOURCC_RGB565:
        frame_data.B = p_buffer + va_image.offsets[0];
        frame_data.G = frame_data.B;
        frame_data.R = frame_data.B;
        break;

#endif
    case VA_FOURCC_ARGB:
        frame_data.B = p_buffer + va_image.offsets[0];
        frame_data.G = frame_data.B + 1;
        frame_data.R = frame_data.B + 2;
        frame_data.A = frame_data.B + 3;
        break;
#ifndef ANDROID
    case VA_FOURCC_A2R10G10B10:
        frame_data.B = p_buffer + va_image.offsets[0];
        frame_data.G = frame_data.B;
        frame_data.R = frame_data.B;
        frame_data.A = frame_data.B;
        break;
#endif
#ifdef MFX_ENABLE_RGBP
    case VA_FOURCC_RGBP:
        frame_data.R = p_buffer + va_image.offsets[0];
        frame_data.G = p_buffer + va_image.offsets[1];
        frame_data.B = p_buffer + va_image.offsets[2];
        break;
#endif
    case VA_FOURCC_BGRP:
        frame_data.B = p_buffer + va_image.offsets[0];
        frame_data.G = p_buffer + va_image.offsets[1];
        frame_data.R = p_buffer + va_image.offsets[2];
        break;
    case VA_FOURCC_ABGR:
        frame_data.R = p_buffer + va_image.offsets[0];
        frame_data.G = frame_data.R + 1;
        frame_data.B = frame_data.R + 2;
        frame_data.A = frame_data.R + 3;
        break;

    case MFX_FOURCC_YUV400:
    case VA_FOURCC_P208:
        frame_data.Y = p_buffer + va_image.offsets[0];
        break;

    case VA_FOURCC_P010:
    case VA_FOURCC_P012:
    case VA_FOURCC_P016:
        frame_data.Y = p_buffer + va_image.offsets[0];
        frame_data.U = p_buffer + va_image.offsets[1];
        frame_data.V = frame_data.U + sizeof(mfxU16);
        break;

    case VA_FOURCC_AYUV:
        frame_data.V = p_buffer + va_image.offsets[0];
        frame_data.U = frame_data.V + 1;
        frame_data.Y = frame_data.V + 2;
        frame_data.A = frame_data.V + 3;
        break;

    case VA_FOURCC_Y210:
    case VA_FOURCC_Y212:
    case VA_FOURCC_Y216:
        frame_data.Y16 = (mfxU16 *) (p_buffer + va_image.offsets[0]);
        frame_data.U16 = frame_data.Y16 + 1;
        frame_data.V16 = frame_data.Y16 + 3;
        break;

    case VA_FOURCC_Y410:
        frame_data.Y = frame_data.U = frame_data.V = frame_data.A = 0;
        frame_data.Y410 = (mfxY410*)(p_buffer + va_image.offsets[0]);
        break;

    case VA_FOURCC_Y412:
    case VA_FOURCC_Y416:
        frame_data.U16 = (mfxU16 *) (p_buffer + va_image.offsets[0]);
        frame_data.Y16 = frame_data.U16 + 1;
        frame_data.V16 = frame_data.Y16 + 1;
        frame_data.A   = (mfxU8 *)(frame_data.V16 + 1);
        break;

    default:
        MFX_RETURN(MFX_ERR_LOCK_MEMORY);
    }

    frame_data.PitchHigh = mfxU16(va_image.pitches[0] >> 16);
    frame_data.PitchLow  = mfxU16(va_image.pitches[0] & 0xffff);

    return MFX_ERR_NONE;
}

mfxStatus
mfxDefaultAllocatorVAAPI::LockFrameHW(
    mfxHDL         pthis,
    mfxMemId       mid,
    mfxFrameData*  ptr)
{
    MFX_CHECK(pthis, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(mid,   MFX_ERR_INVALID_HANDLE);
    MFX_CHECK_NULL_PTR1(ptr);

    auto vaapi_mids = reinterpret_cast<vaapiMemIdInt*>(mid);
    MFX_CHECK(vaapi_mids->m_surface, MFX_ERR_INVALID_HANDLE);

    auto self = reinterpret_cast<mfxWideHWFrameAllocator*>(pthis);

    VAStatus va_res = VA_STATUS_SUCCESS;

    mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(vaapi_mids->m_fourcc);
    if (MFX_FOURCC_P8 == mfx_fourcc)   // bitstream processing
    {
        if (vaapi_mids->m_fourcc == MFX_FOURCC_VP8_SEGMAP)
        {
            mfxU8* p_buffer = nullptr;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
                PERF_UTILITY_AUTO("vaMapBuffer", PERF_LEVEL_DDI);
                va_res = vaMapBuffer(self->m_pVADisplay, *(vaapi_mids->m_surface), (void **)(&p_buffer));
                MFX_CHECK(va_res == VA_STATUS_SUCCESS, MFX_ERR_DEVICE_FAILED);
            }

            ptr->Y = p_buffer;
        }
        else
        {
            VACodedBufferSegment *coded_buffer_segment;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
                PERF_UTILITY_AUTO("vaMapBuffer", PERF_LEVEL_DDI);
                va_res =  vaMapBuffer(self->m_pVADisplay, *(vaapi_mids->m_surface), (void **)(&coded_buffer_segment));
                MFX_CHECK(va_res == VA_STATUS_SUCCESS, MFX_ERR_DEVICE_FAILED);
            }

            ptr->Y = reinterpret_cast<mfxU8*>(coded_buffer_segment->buf);
        }
    }
    else
    {
        {
            PERF_UTILITY_AUTO("vaDeriveImage", PERF_LEVEL_DDI);
            va_res = vaDeriveImage(self->m_pVADisplay, *(vaapi_mids->m_surface), &(vaapi_mids->m_image));
            MFX_CHECK(va_res == VA_STATUS_SUCCESS, MFX_ERR_DEVICE_FAILED);
        }

        mfxU8* p_buffer = nullptr;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
            PERF_UTILITY_AUTO("vaMapBuffer", PERF_LEVEL_DDI);
            va_res = vaMapBuffer(self->m_pVADisplay, vaapi_mids->m_image.buf, (void **) &p_buffer);
            MFX_CHECK(va_res == VA_STATUS_SUCCESS, MFX_ERR_DEVICE_FAILED);
        }

        mfxStatus mfx_res = SetFrameData(vaapi_mids->m_image, mfx_fourcc, p_buffer, *ptr);
        MFX_CHECK_STS(mfx_res);
    }

    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocatorVAAPI::UnlockFrameHW(
    mfxHDL         pthis,
    mfxMemId       mid,
    mfxFrameData*  ptr)
{
    MFX_CHECK(pthis, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(mid,   MFX_ERR_INVALID_HANDLE);

    auto vaapi_mids = reinterpret_cast<vaapiMemIdInt*>(mid);
    MFX_CHECK(vaapi_mids->m_surface, MFX_ERR_INVALID_HANDLE);

    auto self = reinterpret_cast<mfxWideHWFrameAllocator*>(pthis);

    VAStatus va_res = VA_STATUS_SUCCESS;

    mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(vaapi_mids->m_fourcc);
    if (MFX_FOURCC_P8 == mfx_fourcc)   // bitstream processing
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        PERF_UTILITY_AUTO("vaUnmapBuffer", PERF_LEVEL_DDI);
        va_res = vaUnmapBuffer(self->m_pVADisplay, *(vaapi_mids->m_surface));
        MFX_CHECK(va_res == VA_STATUS_SUCCESS, MFX_ERR_DEVICE_FAILED);
    }
    else  // Image processing
    {
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
            PERF_UTILITY_AUTO("vaUnmapBuffer", PERF_LEVEL_DDI);
            va_res = vaUnmapBuffer(self->m_pVADisplay, vaapi_mids->m_image.buf);
            MFX_CHECK(va_res == VA_STATUS_SUCCESS, MFX_ERR_DEVICE_FAILED);
        }
        {
            PERF_UTILITY_AUTO("vaDestroyImage", PERF_LEVEL_DDI);
            va_res = vaDestroyImage(self->m_pVADisplay, vaapi_mids->m_image.image_id);
            MFX_CHECK(va_res == VA_STATUS_SUCCESS, MFX_ERR_DEVICE_FAILED);
        }

        if (ptr)
        {
            ptr->PitchLow  = 0;
            ptr->PitchHigh = 0;
            ptr->Y     = nullptr;
            ptr->U     = nullptr;
            ptr->V     = nullptr;
            ptr->A     = nullptr;
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus
mfxDefaultAllocatorVAAPI::GetHDLHW(
    mfxHDL    pthis,
    mfxMemId  mid,
    mfxHDL*   handle)
{
    MFX_CHECK(pthis, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(mid,   MFX_ERR_INVALID_HANDLE);

    auto vaapi_mids = reinterpret_cast<vaapiMemIdInt*>(mid);
    MFX_CHECK(vaapi_mids->m_surface, MFX_ERR_INVALID_HANDLE);

    *handle = vaapi_mids->m_surface; //VASurfaceID* <-> mfxHDL
    return MFX_ERR_NONE;
}

mfxDefaultAllocatorVAAPI::mfxWideHWFrameAllocator::mfxWideHWFrameAllocator(
    mfxU16  type,
    mfxHDL  handle)
    : mfxBaseWideFrameAllocator(type)
    , m_pVADisplay(reinterpret_cast<VADisplay *>(handle))
    , m_DecId(0)
{
    frameAllocator.Alloc  = &mfxDefaultAllocatorVAAPI::AllocFramesHW;
    frameAllocator.Lock   = &mfxDefaultAllocatorVAAPI::LockFrameHW;
    frameAllocator.GetHDL = &mfxDefaultAllocatorVAAPI::GetHDLHW;
    frameAllocator.Unlock = &mfxDefaultAllocatorVAAPI::UnlockFrameHW;
    frameAllocator.Free   = &mfxDefaultAllocatorVAAPI::FreeFramesHW;
}

vaapi_buffer_wrapper::vaapi_buffer_wrapper(const mfxFrameInfo &info, VADisplayWrapper& display, mfxU32 context)
    : vaapi_resource_wrapper(display)
    , m_bIsSegmap(info.FourCC == MFX_FOURCC_VP8_SEGMAP)
{
    mfxU32 codedbuf_size, codedbuf_num;
    VABufferType codedbuf_type;

    if (m_bIsSegmap)
    {
        codedbuf_size = info.Width;
        codedbuf_num  = info.Height;
        codedbuf_type = VAEncMacroblockMapBufferType;
    }
    else
    {
        mfxI32 aligned_width  = mfx::align2_value(info.Width,  32);
        mfxI32 aligned_height = mfx::align2_value(info.Height, 32);
        codedbuf_size = static_cast<mfxU32>((aligned_width * aligned_height) * 400LL / (16 * 16));

        codedbuf_num  = 1;
        codedbuf_type = VAEncCodedBufferType;
    }

    m_pitch = codedbuf_size;

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaCreateBuffer");
        PERF_UTILITY_AUTO("vaCreateBuffer", PERF_LEVEL_DDI);
        VAStatus va_res = vaCreateBuffer(*m_pVADisplay,
            context,
            codedbuf_type,
            codedbuf_size,
            codedbuf_num,
            nullptr,
            &m_resource_id);

        MFX_CHECK_WITH_THROW_STS(MFX_STS_TRACE(va_res) == VA_STATUS_SUCCESS, MFX_ERR_MEMORY_ALLOC);
    }
}

vaapi_buffer_wrapper::~vaapi_buffer_wrapper()
{
    std::ignore = MFX_STS_TRACE(vaDestroyBuffer(*m_pVADisplay, m_resource_id));
}

mfxStatus vaapi_buffer_wrapper::Lock(mfxFrameData& frame_data, mfxU32 flags)
{
    MFX_CHECK(FrameAllocatorBase::CheckMemoryFlags(flags), MFX_ERR_LOCK_MEMORY);

    clear_frame_data(frame_data);

    if (m_bIsSegmap)
    {
        mfxU8* p_buffer;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
            PERF_UTILITY_AUTO("vaMapBuffer", PERF_LEVEL_DDI);
            VAStatus va_res = vaMapBuffer(*m_pVADisplay, m_resource_id, (void **)(&p_buffer));
            MFX_CHECK(va_res == VA_STATUS_SUCCESS, MFX_ERR_LOCK_MEMORY);
        }

        frame_data.Y = p_buffer;
    }
    else
    {
        VACodedBufferSegment *coded_buffer_segment;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
            PERF_UTILITY_AUTO("vaMapBuffer", PERF_LEVEL_DDI);
            VAStatus va_res = vaMapBuffer(*m_pVADisplay, m_resource_id, (void **)(&coded_buffer_segment));
            MFX_CHECK(va_res == VA_STATUS_SUCCESS, MFX_ERR_LOCK_MEMORY);
        }

        frame_data.Y = reinterpret_cast<mfxU8*>(coded_buffer_segment->buf);
    }

    frame_data.PitchLow  = mfxU16(m_pitch & 0xffff);
    frame_data.PitchHigh = mfxU16(m_pitch >> 16);

    return MFX_ERR_NONE;
}

mfxStatus vaapi_buffer_wrapper::Unlock()
{
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        PERF_UTILITY_AUTO("vaUnmapBuffer", PERF_LEVEL_DDI);
        VAStatus va_res = vaUnmapBuffer(*m_pVADisplay, m_resource_id);
        MFX_CHECK(va_res == VA_STATUS_SUCCESS, MFX_ERR_DEVICE_FAILED);
    }

    return MFX_ERR_NONE;
}

vaapi_surface_wrapper::vaapi_surface_wrapper(const mfxFrameInfo &info, mfxU16 type, VADisplayWrapper& display, mfxSurfaceHeader* import_surface)
    : vaapi_resource_wrapper(display)
    , m_surface_lock(*m_pVADisplay, m_resource_id)
    , m_imported(false)
    , m_type(type)
    , m_fourcc(info.FourCC)
{
    bool should_copy = false;

    if (import_surface)
    {
        // First try no-copy import
        mfxStatus sts;
        std::tie(sts, should_copy) = TryImportSurface(info, import_surface);
        MFX_CHECK_WITH_THROW_STS(sts == MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);

        if (!should_copy)
            return;
    }

    mfxU32 format;
    std::vector<VASurfaceAttrib> attrib;

    mfxU32 va_fourcc = ConvertMfxFourccToVAFormat(ConvertVP8FourccToMfxFourcc(m_fourcc));
    FillSurfaceAttrs(attrib, format, m_fourcc, va_fourcc, m_type);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaCreateSurfaces");

        PERF_UTILITY_AUTO("vaCreateSurfaces", PERF_LEVEL_DDI);
        VAStatus va_res = vaCreateSurfaces(*m_pVADisplay,
            format,
            info.Width, info.Height,
            &m_resource_id,
            1,
            attrib.data(),
            attrib.size());

        MFX_CHECK_WITH_THROW_STS(MFX_STS_TRACE(va_res) == VA_STATUS_SUCCESS, MFX_ERR_MEMORY_ALLOC);
    }

    // We've just created VA surface so should destroy it in destructor
    m_imported = false;

    if (should_copy)
    {
        // We only get here if MFX_SURFACE_FLAG_IMPORT_COPY flag was set. Also in case of failed import if MFX_SURFACE_FLAG_IMPORT_SHARED was set as well
        mfxStatus sts = CopyImportSurface(info, import_surface);
        MFX_CHECK_WITH_THROW_STS(sts == MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
    }
}

// Returns overall status and copy flag (if true import failed, but need to try to copy import surface)
std::pair<mfxStatus, bool> vaapi_surface_wrapper::TryImportSurface(const mfxFrameInfo& info, mfxSurfaceHeader* import_surface)
{
    if (!import_surface)
        return { MFX_ERR_NONE, false };

    switch (import_surface->SurfaceType)
    {
    case MFX_SURFACE_TYPE_VAAPI:
        return TryImportSurfaceVAAPI(info, *(reinterpret_cast<mfxSurfaceVAAPI*>(import_surface)));


    default:
        return { MFX_STS_TRACE(MFX_ERR_UNSUPPORTED), false };
    }
}

// Returns overall status and copy flag (if true import failed, but need to try to copy import surface)
std::pair<mfxStatus, bool> vaapi_surface_wrapper::TryImportSurfaceVAAPI(const mfxFrameInfo& info, mfxSurfaceVAAPI& import_surface)
{
    if (!check_import_flags(import_surface.SurfaceInterface.Header.SurfaceFlags))
        return { MFX_STS_TRACE(MFX_ERR_INVALID_VIDEO_PARAM), false };

    if (import_surface.SurfaceInterface.Header.StructSize != sizeof(mfxSurfaceVAAPI))
        return { MFX_STS_TRACE(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM), false };

    if (!import_surface.vaDisplay)
        return { MFX_STS_TRACE(MFX_ERR_INVALID_HANDLE), false };

    if (import_surface.vaSurfaceID == VA_INVALID_ID)
        return { MFX_STS_TRACE(MFX_ERR_INVALID_HANDLE), false };

    // Check compatibility of passed surface
    bool can_import = (import_surface.SurfaceInterface.Header.SurfaceFlags & MFX_SURFACE_FLAG_IMPORT_SHARED)
        || (import_surface.SurfaceInterface.Header.SurfaceFlags == MFX_SURFACE_FLAG_DEFAULT);

    bool can_copy = (import_surface.SurfaceInterface.Header.SurfaceFlags & MFX_SURFACE_FLAG_IMPORT_COPY);

    // We can't import surface from another display
    can_import = can_import && import_surface.vaDisplay == (VADisplay)(*m_pVADisplay);
    // If copies between displays are supported, the followinf restriction can be removed
    can_copy   = can_copy   && import_surface.vaDisplay == (VADisplay)(*m_pVADisplay);

    SurfaceScopedLock src_surface_lock(import_surface.vaDisplay, import_surface.vaSurfaceID);
    mfxStatus sts = src_surface_lock.DeriveImage();
    if (sts != MFX_ERR_NONE)
        return { MFX_STS_TRACE(sts), false };

    // We can import surface if it's not smaller than size passed on component init
    can_import = can_import && src_surface_lock.m_image.width >= info.Width && src_surface_lock.m_image.height >= info.Height;

    // We can copy surface if it's not bigger than size passed on component init
    can_copy = can_copy && src_surface_lock.m_image.width <= info.Width && src_surface_lock.m_image.height <= info.Height;

    can_import = can_import && src_surface_lock.m_image.format.fourcc == ConvertMfxFourccToVAFormat(info.FourCC);
    can_copy   = can_copy   && src_surface_lock.m_image.format.fourcc == ConvertMfxFourccToVAFormat(info.FourCC);

    if (can_import)
    {
        m_resource_id = import_surface.vaSurfaceID;
        m_imported = true;

        import_surface.SurfaceInterface.Header.SurfaceFlags = MFX_SURFACE_FLAG_IMPORT_SHARED;

        return { MFX_ERR_NONE, false };
    }

    // If we get here, we cannot import this surface without a copy
    if (!can_copy)
        return { MFX_STS_TRACE(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM), false };

    // Can't import, but can copy
    return { MFX_ERR_NONE, true };
}



mfxStatus vaapi_surface_wrapper::CopyImportSurface(const mfxFrameInfo& info, mfxSurfaceHeader* import_surface)
{
    MFX_CHECK_NULL_PTR1(import_surface);

    switch (import_surface->SurfaceType)
    {
    case MFX_SURFACE_TYPE_VAAPI:
        MFX_RETURN(CopyImportSurfaceVAAPI(info, *(reinterpret_cast<mfxSurfaceVAAPI*>(import_surface))));


    default:
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }
}

mfxStatus vaapi_surface_wrapper::CopyImportSurfaceVAAPI(const mfxFrameInfo& info, mfxSurfaceVAAPI& import_surface)
{
    SurfaceScopedLock src_surface_lock(import_surface.vaDisplay, import_surface.vaSurfaceID);
    MFX_SAFE_CALL(src_surface_lock.DeriveImage());

    VAStatus va_sts = vaPutImage(*m_pVADisplay, m_resource_id, src_surface_lock.m_image.image_id,
        0, 0, src_surface_lock.m_image.width, src_surface_lock.m_image.height,
        0, 0, src_surface_lock.m_image.width, src_surface_lock.m_image.height);
    MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

    import_surface.SurfaceInterface.Header.SurfaceFlags = MFX_SURFACE_FLAG_IMPORT_COPY;

    return MFX_ERR_NONE;
}



vaapi_surface_wrapper::~vaapi_surface_wrapper()
{
    if (!m_imported)
    {
        std::ignore = MFX_STS_TRACE(vaDestroySurfaces(*m_pVADisplay, &m_resource_id, 1));
    }
}

mfxStatus vaapi_surface_wrapper::Lock(mfxFrameData& frame_data, mfxU32 flags)
{
    MFX_CHECK(FrameAllocatorBase::CheckMemoryFlags(flags), MFX_ERR_LOCK_MEMORY);

    mfxStatus sts = m_surface_lock.DeriveImage();
    MFX_CHECK_STS(sts);

    mfxU8* p_buffer = nullptr;
    sts = m_surface_lock.Map(p_buffer);
    MFX_CHECK_STS(sts);

    return mfxDefaultAllocatorVAAPI::SetFrameData(m_surface_lock.m_image, ConvertVP8FourccToMfxFourcc(m_fourcc), p_buffer, frame_data);
}

mfxStatus vaapi_surface_wrapper::Unlock()
{
    mfxStatus sts = m_surface_lock.Unmap();
    MFX_CHECK_STS(sts);

    sts = m_surface_lock.DestroyImage();
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus vaapi_surface_wrapper::Export(const mfxSurfaceHeader& export_header, mfxSurfaceBase*& exported_surface, mfxFrameSurfaceInterfaceImpl* p_base_surface)
{

    switch (export_header.SurfaceType)
    {
    case MFX_SURFACE_TYPE_VAAPI:
        exported_surface = mfxSurfaceVAAPIImpl::Create(export_header, p_base_surface, m_pVADisplay, m_resource_id);
        break;


    default:
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    return MFX_ERR_NONE;
}

mfxFrameSurface1_hw_vaapi::mfxFrameSurface1_hw_vaapi(const mfxFrameInfo & info, mfxU16 type, mfxMemId mid, std::shared_ptr<staging_adapter_stub>&, mfxHDL display, mfxU32 context, FrameAllocatorBase& allocator,
                                                     mfxSurfaceHeader* import_surface)
    : RWAcessSurface(info, type, mid, allocator)
    , m_type(type)
    , m_context(VAContextID(context))
{
    mfxU32 vp8_fourcc = ConvertVP8FourccToMfxFourcc(info.FourCC);

    MFX_CHECK_WITH_THROW_STS(isFourCCSupported(ConvertMfxFourccToVAFormat(vp8_fourcc)), MFX_ERR_UNSUPPORTED);
    MFX_CHECK_WITH_THROW_STS(!(m_type & MFX_MEMTYPE_SYSTEM_MEMORY),                     MFX_ERR_UNSUPPORTED);

    auto p_va_display_wrapper = reinterpret_cast<VADisplayWrapper*>(display);
    MFX_CHECK_WITH_THROW_STS(p_va_display_wrapper, MFX_ERR_INVALID_HANDLE);

    if (vp8_fourcc == MFX_FOURCC_P8)
    {
        // Import of plain buffers is not supported
        MFX_CHECK_WITH_THROW_STS(!import_surface, MFX_ERR_UNSUPPORTED);
        m_resource_wrapper.reset(new vaapi_buffer_wrapper(info, *p_va_display_wrapper, m_context));
    }
    else
    {
        m_resource_wrapper.reset(new vaapi_surface_wrapper(info, m_type, *p_va_display_wrapper, import_surface));
    }
}

mfxStatus mfxFrameSurface1_hw_vaapi::Lock(mfxU32 flags)
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
        sts = m_resource_wrapper->Lock(m_internal_surface.Data, flags);
        MFX_CHECK_STS(sts);
    }

    // No error, remove guard without decreasing locked counter
    scoped_lock.release();

    return MFX_ERR_NONE;
}

mfxStatus mfxFrameSurface1_hw_vaapi::Unlock()
{
    std::unique_lock<std::mutex> guard(m_mutex);

    MFX_SAFE_CALL(UnlockRW());

    if (NumReaders() == 0) // So it was 1 before UnlockRW
    {
        MFX_SAFE_CALL(m_resource_wrapper->Unlock());

        clear_frame_data(m_internal_surface.Data);
    }

    return MFX_ERR_NONE;
}

std::pair<mfxHDL, mfxResourceType> mfxFrameSurface1_hw_vaapi::GetNativeHandle() const
{
    std::shared_lock<std::shared_timed_mutex> guard(m_hdl_mutex);

    return { reinterpret_cast<mfxHDL>(m_resource_wrapper->GetHandle()), MFX_RESOURCE_VA_SURFACE };
}

std::pair<mfxHDL, mfxHandleType> mfxFrameSurface1_hw_vaapi::GetDeviceHandle() const
{
    return { reinterpret_cast<mfxHDL>((VADisplay)(m_resource_wrapper->GetDevice())), MFX_HANDLE_VA_DISPLAY };
}

mfxStatus mfxFrameSurface1_hw_vaapi::GetHDL(mfxHDL& handle) const
{
    std::shared_lock<std::shared_timed_mutex> guard(m_hdl_mutex);

    handle = m_resource_wrapper->GetHandle();

    return MFX_ERR_NONE;
}

mfxStatus mfxFrameSurface1_hw_vaapi::Realloc(const mfxFrameInfo & info)
{
    std::lock_guard<std::mutex>              guard(m_mutex);
    std::lock_guard<std::shared_timed_mutex> guard_hdl(m_hdl_mutex);

    MFX_CHECK(!Locked(), MFX_ERR_LOCK_MEMORY);

    m_internal_surface.Info = info;

    if (info.FourCC == MFX_FOURCC_P8)
    {
        m_resource_wrapper.reset(new vaapi_buffer_wrapper(info, m_resource_wrapper->GetDevice(), m_context));
    }
    else
    {
        m_resource_wrapper.reset(new vaapi_surface_wrapper(info, m_type, m_resource_wrapper->GetDevice(), nullptr));
    }

    return MFX_ERR_NONE;
}

template<>
void FlexibleFrameAllocatorHW_VAAPI::SetDevice(mfxHDL device)
{
    if (m_session)
    {
        mfxHDL hdl = nullptr;

        mfxStatus sts = m_session->m_pCORE->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);

        MFX_CHECK_WITH_THROW_STS(sts == MFX_ERR_NONE, sts);

        auto p_va_display = reinterpret_cast<VADisplayWrapper*>(device);
        MFX_CHECK_WITH_THROW_STS(p_va_display, MFX_ERR_INVALID_HANDLE);

        MFX_CHECK_WITH_THROW_STS(hdl == *p_va_display, MFX_ERR_INVALID_HANDLE);
    }

    m_device = device;

    m_staging_adapter->SetDevice(device);
}

mfxSurfaceVAAPIImpl::mfxSurfaceVAAPIImpl(const mfxSurfaceHeader& export_header, mfxFrameSurfaceInterfaceImpl* p_base_surface, std::shared_ptr<VADisplayWrapper>& display, VASurfaceID surface_id)
    : mfxSurfaceImpl<mfxSurfaceVAAPI>(export_header, p_base_surface)
    , m_pVADisplay(display)
{
    MFX_CHECK_WITH_THROW_STS(check_export_flags(export_header.SurfaceFlags), MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK_WITH_THROW_STS(m_pVADisplay,                                   MFX_ERR_INVALID_HANDLE);
    MFX_CHECK_WITH_THROW_STS(surface_id != VA_INVALID_ID,                    MFX_ERR_INVALID_HANDLE);

    bool default_set = export_header.SurfaceFlags == MFX_SURFACE_FLAG_DEFAULT;
    if ((export_header.SurfaceFlags & MFX_SURFACE_FLAG_EXPORT_SHARED) || default_set)
    {
        m_surface_id = surface_id;
        mfxSurfaceVAAPI::vaDisplay   = (VADisplay)(*m_pVADisplay);
        mfxSurfaceVAAPI::vaSurfaceID = m_surface_id;

        // Hold original surface to keep VASurfaceID
        GetParentSurface()->AddRef();

        SetResultedExportType(MFX_SURFACE_FLAG_EXPORT_SHARED);
        return;
    }

    if (export_header.SurfaceFlags & MFX_SURFACE_FLAG_EXPORT_COPY)
    {
        SurfaceScopedLock orig_surface_lock(*m_pVADisplay, surface_id);
        mfxStatus sts = orig_surface_lock.DeriveImage();
        MFX_CHECK_WITH_THROW_STS(sts == MFX_ERR_NONE, sts);

        mfxU32 format;
        std::vector<VASurfaceAttrib> attrib;
        FillSurfaceAttrs(attrib, format, orig_surface_lock.m_image.format.fourcc, orig_surface_lock.m_image.format.fourcc, GetParentSurface()->m_exported_surface.Data.MemType);

        VAStatus va_sts = vaCreateSurfaces(*m_pVADisplay,
            format,
            orig_surface_lock.m_image.width, orig_surface_lock.m_image.height,
            &m_surface_id,
            1,
            attrib.data(), attrib.size());
        MFX_CHECK_WITH_THROW_STS(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

        va_sts = vaPutImage(*m_pVADisplay, m_surface_id, orig_surface_lock.m_image.image_id,
            0, 0, orig_surface_lock.m_image.width, orig_surface_lock.m_image.height,
            0, 0, orig_surface_lock.m_image.width, orig_surface_lock.m_image.height);
        MFX_CHECK_WITH_THROW_STS(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

        mfxSurfaceVAAPI::vaDisplay   = (VADisplay)(*m_pVADisplay);
        mfxSurfaceVAAPI::vaSurfaceID = m_surface_id;

        SetResultedExportType(MFX_SURFACE_FLAG_EXPORT_COPY);
        return;
    }
}

mfxSurfaceVAAPIImpl::~mfxSurfaceVAAPIImpl()
{
    if (mfxSurfaceInterface::Header.SurfaceFlags & MFX_SURFACE_FLAG_EXPORT_COPY)
    {
        std::ignore = MFX_STS_TRACE(vaDestroySurfaces(*m_pVADisplay, &m_surface_id, 1));
    }
    // In reality excessive check, with correct refmanagement original surface should be alive
    else if (GetParentSurface())
    {
        // Release original surface, VASurfaceID can be destroyed now
        GetParentSurface()->Release();
    }
}

/* EOF */
