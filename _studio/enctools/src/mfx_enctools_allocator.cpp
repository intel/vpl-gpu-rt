// Copyright (c) 2008-2021 Intel Corporation
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

#include <assert.h>
#include <algorithm>
#include <functional>
#include "mfx_enctools.h"

MFXFrameAllocator::MFXFrameAllocator()
{
    pthis = this;
    Alloc = Alloc_;
    Lock  = Lock_;
    Free  = Free_;
    Unlock = Unlock_;
    GetHDL = GetHDL_;
}

MFXFrameAllocator::~MFXFrameAllocator()
{
}

mfxStatus MFXFrameAllocator::Alloc_(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.AllocFrames(request, response);
}

mfxStatus MFXFrameAllocator::Lock_(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.LockFrame(mid, ptr);
}

mfxStatus MFXFrameAllocator::Unlock_(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.UnlockFrame(mid, ptr);
}

mfxStatus MFXFrameAllocator::Free_(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.FreeFrames(response);
}

mfxStatus MFXFrameAllocator::GetHDL_(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    if (0 == pthis)
        return MFX_ERR_MEMORY_ALLOC;

    MFXFrameAllocator& self = *(MFXFrameAllocator *)pthis;

    return self.GetFrameHDL(mid, handle);
}

BaseFrameAllocator::BaseFrameAllocator()
{
}

BaseFrameAllocator::~BaseFrameAllocator()
{
}

mfxStatus BaseFrameAllocator::CheckRequestType(mfxFrameAllocRequest *request)
{
    if (0 == request)
        return MFX_ERR_NULL_PTR;

    // check that Media SDK component is specified in request
    if ((request->Type & MEMTYPE_FROM_MASK) != 0)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxStatus BaseFrameAllocator::AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{

    if (0 == request || 0 == response || 0 == request->NumFrameSuggested)
        return MFX_ERR_MEMORY_ALLOC;

    if (MFX_ERR_NONE != CheckRequestType(request))
        return MFX_ERR_UNSUPPORTED;

    mfxStatus sts = MFX_ERR_NONE;

    if ( (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) && (request->Type & MFX_MEMTYPE_FROM_DECODE) )
    {
        // external decoder allocations
        std::list<UniqueResponse>::iterator it =
            std::find_if( m_ExtResponses.begin()
                        , m_ExtResponses.end()
                        , UniqueResponse (*response, request->Info.CropW, request->Info.CropH, 0));

        if (it != m_ExtResponses.end())
        {
            // check if enough frames were allocated
            if (request->NumFrameMin > it->NumFrameActual)
                return MFX_ERR_MEMORY_ALLOC;

            it->m_refCount++;
            // return existing response
            *response = (mfxFrameAllocResponse&)*it;
        }
        else
        {
            sts = AllocImpl(request, response);
            if (sts == MFX_ERR_NONE)
            {
                m_ExtResponses.push_back(UniqueResponse(*response, request->Info.CropW, request->Info.CropH, request->Type & MEMTYPE_FROM_MASK));
            }
        }
    }
    else
    {
        // internal allocations

        // reserve space before allocation to avoid memory leak
        std::list<mfxFrameAllocResponse> tmp(1, mfxFrameAllocResponse(), m_responses.get_allocator());

        sts = AllocImpl(request, response);
        if (sts == MFX_ERR_NONE)
        {
            m_responses.splice(m_responses.end(), tmp);
            m_responses.back() = *response;
        }
    }

    return sts;
}

mfxStatus BaseFrameAllocator::FreeFrames(mfxFrameAllocResponse *response)
{
    if (response == 0)
        return MFX_ERR_INVALID_HANDLE;

    if (response->mids == nullptr || response->NumFrameActual == 0)
        return MFX_ERR_NONE;

    mfxStatus sts = MFX_ERR_NONE;

    // check whether response is an external decoder response
    std::list<UniqueResponse>::iterator i =
        std::find_if( m_ExtResponses.begin(), m_ExtResponses.end(), std::bind1st(IsSame(), *response));

    if (i != m_ExtResponses.end())
    {
        if ((--i->m_refCount) == 0)
        {
            sts = ReleaseResponse(response);
            m_ExtResponses.erase(i);
        }
        return sts;
    }

    // if not found so far, then search in internal responses
    std::list<mfxFrameAllocResponse>::iterator i2 =
        std::find_if(m_responses.begin(), m_responses.end(), std::bind1st(IsSame(), *response));

    if (i2 != m_responses.end())
    {
        sts = ReleaseResponse(response);
        m_responses.erase(i2);
        return sts;
    }

    // not found anywhere, report an error
    return MFX_ERR_INVALID_HANDLE;
}

mfxStatus BaseFrameAllocator::Close()
{
    std::list<UniqueResponse> ::iterator i;
    for (i = m_ExtResponses.begin(); i!= m_ExtResponses.end(); i++)
    {
        ReleaseResponse(&*i);
    }
    m_ExtResponses.clear();

    std::list<mfxFrameAllocResponse> ::iterator i2;
    for (i2 = m_responses.begin(); i2!= m_responses.end(); i2++)
    {
        ReleaseResponse(&*i2);
    }

    return MFX_ERR_NONE;
}



#include <dlfcn.h>
#include <iostream>

namespace MfxLoader
{

    SimpleLoader::SimpleLoader(const char* name)
    {
        dlerror();
        so_handle = dlopen(name, RTLD_GLOBAL | RTLD_NOW);
        if (NULL == so_handle)
        {
            std::cerr << dlerror() << std::endl;
            throw std::runtime_error("Can't load library");
        }
    }

    void* SimpleLoader::GetFunction(const char* name)
    {
        void* fn_ptr = dlsym(so_handle, name);
        if (!fn_ptr)
            throw std::runtime_error("Can't find function");
        return fn_ptr;
    }

    SimpleLoader::~SimpleLoader()
    {
        if (so_handle)
            dlclose(so_handle);
    }

#define SIMPLE_LOADER_STRINGIFY1( x) #x
#define SIMPLE_LOADER_STRINGIFY(x) SIMPLE_LOADER_STRINGIFY1(x)
#define SIMPLE_LOADER_DECORATOR1(fun,suffix) fun ## _ ## suffix
#define SIMPLE_LOADER_DECORATOR(fun,suffix) SIMPLE_LOADER_DECORATOR1(fun,suffix)


    // Following macro applied on vaInitialize will give:  vaInitialize((vaInitialize_type)lib.GetFunction("vaInitialize"))
#define SIMPLE_LOADER_FUNCTION(name) name( (SIMPLE_LOADER_DECORATOR(name, type)) lib.GetFunction(SIMPLE_LOADER_STRINGIFY(name)) )


    VA_Proxy::VA_Proxy()
        : lib("libva.so.2")
        , SIMPLE_LOADER_FUNCTION(vaInitialize)
        , SIMPLE_LOADER_FUNCTION(vaTerminate)
        , SIMPLE_LOADER_FUNCTION(vaCreateSurfaces)
        , SIMPLE_LOADER_FUNCTION(vaDestroySurfaces)
        , SIMPLE_LOADER_FUNCTION(vaCreateBuffer)
        , SIMPLE_LOADER_FUNCTION(vaDestroyBuffer)
        , SIMPLE_LOADER_FUNCTION(vaMapBuffer)
        , SIMPLE_LOADER_FUNCTION(vaUnmapBuffer)
        , SIMPLE_LOADER_FUNCTION(vaDeriveImage)
        , SIMPLE_LOADER_FUNCTION(vaDestroyImage)
        , SIMPLE_LOADER_FUNCTION(vaSyncSurface)
    {
    }

    VA_Proxy::~VA_Proxy()
    {}


#undef SIMPLE_LOADER_FUNCTION

} // MfxLoader


mfxStatus va_to_mfx_status(VAStatus va_res)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    switch (va_res)
    {
    case VA_STATUS_SUCCESS:
        mfxRes = MFX_ERR_NONE;
        break;
    case VA_STATUS_ERROR_ALLOCATION_FAILED:
        mfxRes = MFX_ERR_MEMORY_ALLOC;
        break;
    case VA_STATUS_ERROR_ATTR_NOT_SUPPORTED:
    case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
    case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
    case VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT:
    case VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE:
    case VA_STATUS_ERROR_FLAG_NOT_SUPPORTED:
    case VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED:
        mfxRes = MFX_ERR_UNSUPPORTED;
        break;
    case VA_STATUS_ERROR_INVALID_DISPLAY:
    case VA_STATUS_ERROR_INVALID_CONFIG:
    case VA_STATUS_ERROR_INVALID_CONTEXT:
    case VA_STATUS_ERROR_INVALID_SURFACE:
    case VA_STATUS_ERROR_INVALID_BUFFER:
    case VA_STATUS_ERROR_INVALID_IMAGE:
    case VA_STATUS_ERROR_INVALID_SUBPICTURE:
        mfxRes = MFX_ERR_NOT_INITIALIZED;
        break;
    case VA_STATUS_ERROR_INVALID_PARAMETER:
        mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
    default:
        mfxRes = MFX_ERR_UNKNOWN;
        break;
    }
    return mfxRes;
}


enum {
    MFX_FOURCC_VP8_NV12 = MFX_MAKEFOURCC('V', 'P', '8', 'N'),
    MFX_FOURCC_VP8_MBDATA = MFX_MAKEFOURCC('V', 'P', '8', 'M'),
    MFX_FOURCC_VP8_SEGMAP = MFX_MAKEFOURCC('V', 'P', '8', 'S'),
};

unsigned int ConvertMfxFourccToVAFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
        return VA_FOURCC_NV12;
    case MFX_FOURCC_YUY2:
        return VA_FOURCC_YUY2;
    case MFX_FOURCC_YV12:
        return VA_FOURCC_YV12;
    case MFX_FOURCC_RGB4:
        return VA_FOURCC_ARGB;
    case MFX_FOURCC_A2RGB10:
        return VA_FOURCC_ARGB;  // rt format will be VA_RT_FORMAT_RGB32_10BPP
    case MFX_FOURCC_BGR4:
        return VA_FOURCC_ABGR;
    case MFX_FOURCC_P8:
        return VA_FOURCC_P208;
    case MFX_FOURCC_P010:
        return VA_FOURCC_P010;
    case MFX_FOURCC_Y210:
        return VA_FOURCC_Y210;
    case MFX_FOURCC_Y410:
        return VA_FOURCC_Y410;
    case MFX_FOURCC_P016:
        return VA_FOURCC_P016;
    case MFX_FOURCC_Y216:
        return VA_FOURCC_Y216;
    case MFX_FOURCC_Y416:
        return VA_FOURCC_Y416;
    case MFX_FOURCC_AYUV:
        return VA_FOURCC_AYUV;
    case MFX_FOURCC_RGB565:
        return VA_FOURCC_RGB565;
    case MFX_FOURCC_RGBP:
        return VA_RT_FORMAT_RGBP;
    default:
        assert(!"unsupported fourcc");
        return 0;
    }
}

unsigned int ConvertVP8FourccToMfxFourcc(mfxU32 fourcc)
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

vaapiFrameAllocator::vaapiFrameAllocator()
    : m_dpy(0),
    m_libva(new MfxLoader::VA_Proxy),
    m_bAdaptivePlayback(false),
    m_Width(0),
    m_Height(0)
{
}

vaapiFrameAllocator::~vaapiFrameAllocator()
{
    Close();
    delete m_libva;
}

mfxStatus vaapiFrameAllocator::Init(mfxAllocatorParams* pParams)
{
    vaapiAllocatorParams* p_vaapiParams = dynamic_cast<vaapiAllocatorParams*>(pParams);

    if ((NULL == p_vaapiParams) || (NULL == p_vaapiParams->m_dpy))
        return MFX_ERR_NOT_INITIALIZED;

    m_dpy = p_vaapiParams->m_dpy;
    m_bAdaptivePlayback = p_vaapiParams->bAdaptivePlayback;
    return MFX_ERR_NONE;
}

mfxStatus vaapiFrameAllocator::CheckRequestType(mfxFrameAllocRequest* request)
{
    mfxStatus sts = BaseFrameAllocator::CheckRequestType(request);
    if (MFX_ERR_NONE != sts)
        return sts;

    if ((request->Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)) != 0)
        return MFX_ERR_NONE;
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxStatus vaapiFrameAllocator::Close()
{
    return BaseFrameAllocator::Close();
}

mfxStatus vaapiFrameAllocator::AllocImpl(mfxFrameAllocRequest* request, mfxFrameAllocResponse* response)
{
    mfxStatus mfx_res = MFX_ERR_NONE;
    VAStatus  va_res = VA_STATUS_SUCCESS;
    unsigned int va_fourcc = 0;
    VASurfaceID* surfaces = NULL;
    vaapiMemId* vaapi_mids = NULL, * vaapi_mid = NULL;
    mfxMemId* mids = NULL;
    mfxU32 fourcc = request->Info.FourCC;
    mfxU16 surfaces_num = request->NumFrameSuggested, numAllocated = 0, i = 0;
    bool bCreateSrfSucceeded = false;

    memset(response, 0, sizeof(mfxFrameAllocResponse));

    // VP8 hybrid driver has weird requirements for allocation of surfaces/buffers for VP8 encoding
    // to comply with them additional logic is required to support regular and VP8 hybrid allocation pathes
    mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(fourcc);
    va_fourcc = ConvertMfxFourccToVAFormat(mfx_fourcc);
    if (!va_fourcc || ((VA_FOURCC_NV12 != va_fourcc) &&
        (VA_FOURCC_YV12 != va_fourcc) &&
        (VA_FOURCC_YUY2 != va_fourcc) &&
        (VA_FOURCC_ARGB != va_fourcc) &&
        (VA_FOURCC_ABGR != va_fourcc) &&
        (VA_FOURCC_P208 != va_fourcc) &&
        (VA_FOURCC_P010 != va_fourcc) &&
        (VA_FOURCC_YUY2 != va_fourcc) &&
        (VA_FOURCC_Y210 != va_fourcc) &&
        (VA_FOURCC_Y410 != va_fourcc) &&
        (VA_FOURCC_RGB565 != va_fourcc) &&
        (VA_RT_FORMAT_RGBP != va_fourcc) &&
        (VA_FOURCC_P016 != va_fourcc) &&
        (VA_FOURCC_Y216 != va_fourcc) &&
        (VA_FOURCC_Y416 != va_fourcc) &&
        (VA_FOURCC_AYUV != va_fourcc)))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    if (!surfaces_num)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        surfaces = (VASurfaceID*)calloc(surfaces_num, sizeof(VASurfaceID));
        vaapi_mids = (vaapiMemId*)calloc(surfaces_num, sizeof(vaapiMemId));
        mids = (mfxMemId*)calloc(surfaces_num, sizeof(mfxMemId));
        if ((NULL == surfaces) || (NULL == vaapi_mids) || (NULL == mids)) mfx_res = MFX_ERR_MEMORY_ALLOC;
    }

    m_Width = request->Info.Width;
    m_Height = request->Info.Height;

    if (MFX_ERR_NONE == mfx_res)
    {
        if (m_bAdaptivePlayback)
        {
            for (i = 0; i < surfaces_num; ++i)
            {
                vaapi_mid = &(vaapi_mids[i]);
                vaapi_mid->m_fourcc = fourcc;
                surfaces[i] = (VASurfaceID)VA_INVALID_ID;
                vaapi_mid->m_surface = &surfaces[i];
                mids[i] = vaapi_mid;
            }
            response->mids = mids;
            response->NumFrameActual = surfaces_num;
            return MFX_ERR_NONE;
        }

        if (VA_FOURCC_P208 != va_fourcc)
        {
            unsigned int format;
            VASurfaceAttrib attrib[2];
            VASurfaceAttrib* pAttrib = &attrib[0];
            int attrCnt = 0;

            attrib[attrCnt].type = VASurfaceAttribPixelFormat;
            attrib[attrCnt].flags = VA_SURFACE_ATTRIB_SETTABLE;
            attrib[attrCnt].value.type = VAGenericValueTypeInteger;
            attrib[attrCnt++].value.value.i = (va_fourcc == VA_RT_FORMAT_RGBP ? VA_FOURCC_RGBP : va_fourcc);
            format = va_fourcc;

            if ((fourcc == MFX_FOURCC_VP8_NV12) ||
                ((MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET & request->Type)
                    && ((fourcc == MFX_FOURCC_RGB4) || (fourcc == MFX_FOURCC_BGR4))))
            {
                /*
                 *  special configuration for NV12 surf allocation for VP8 hybrid encoder and
                 *  RGB32 for JPEG is required
                 */
                attrib[attrCnt].type = (VASurfaceAttribType)VASurfaceAttribUsageHint;
                attrib[attrCnt].flags = VA_SURFACE_ATTRIB_SETTABLE;
                attrib[attrCnt].value.type = VAGenericValueTypeInteger;
                attrib[attrCnt++].value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER;
            }
            else if (fourcc == MFX_FOURCC_VP8_MBDATA)
            {
                // special configuration for MB data surf allocation for VP8 hybrid encoder is required
                attrib[0].value.value.i = VA_FOURCC_P208;
                format = VA_FOURCC_P208;
            }
            else if (va_fourcc == VA_FOURCC_NV12)
            {
                format = VA_RT_FORMAT_YUV420;
            }
            else if (fourcc == MFX_FOURCC_A2RGB10)
            {
                format = VA_RT_FORMAT_RGB32_10BPP;
            }
#if VA_CHECK_VERSION(1,2,0)
            else if (va_fourcc == VA_FOURCC_P010)
            {
                format = VA_RT_FORMAT_YUV420_10;
            }
#endif

            va_res = m_libva->vaCreateSurfaces(m_dpy,
                format,
                request->Info.Width, request->Info.Height,
                surfaces,
                surfaces_num,
                pAttrib, pAttrib ? attrCnt : 0);
            mfx_res = va_to_mfx_status(va_res);
            bCreateSrfSucceeded = (MFX_ERR_NONE == mfx_res);
        }
        else
        {
            VAContextID context_id = request->AllocId;
            int codedbuf_size, codedbuf_num;

            VABufferType codedbuf_type;
            if (fourcc == MFX_FOURCC_VP8_SEGMAP)
            {
                codedbuf_size = request->Info.Width;
                codedbuf_num = request->Info.Height;
                codedbuf_type = VAEncMacroblockMapBufferType;
            }
            else
            {
                int width32 = 32 * ((request->Info.Width + 31) >> 5);
                int height32 = 32 * ((request->Info.Height + 31) >> 5);
                codedbuf_size = static_cast<int>((width32 * height32) * 400LL / (16 * 16));
                codedbuf_num = 1;
                codedbuf_type = VAEncCodedBufferType;
            }

            for (numAllocated = 0; numAllocated < surfaces_num; numAllocated++)
            {
                VABufferID coded_buf;

                va_res = m_libva->vaCreateBuffer(m_dpy,
                    context_id,
                    codedbuf_type,
                    codedbuf_size,
                    codedbuf_num,
                    NULL,
                    &coded_buf);
                mfx_res = va_to_mfx_status(va_res);
                if (MFX_ERR_NONE != mfx_res) break;
                surfaces[numAllocated] = coded_buf;
            }
        }

    }
    if (MFX_ERR_NONE == mfx_res)
    {
        for (i = 0; i < surfaces_num; ++i)
        {
            vaapi_mid = &(vaapi_mids[i]);
            vaapi_mid->m_fourcc = fourcc;
            vaapi_mid->m_surface = &(surfaces[i]);
            mids[i] = vaapi_mid;
        }
    }
    if (MFX_ERR_NONE == mfx_res)
    {
        response->mids = mids;
        response->NumFrameActual = surfaces_num;
    }
    else // i.e. MFX_ERR_NONE != mfx_res
    {
        response->mids = NULL;
        response->NumFrameActual = 0;
        if (VA_FOURCC_P208 != va_fourcc
            || fourcc == MFX_FOURCC_VP8_MBDATA)
        {
            if (bCreateSrfSucceeded)
                m_libva->vaDestroySurfaces(m_dpy, surfaces, surfaces_num);
        }
        else
        {
            for (i = 0; i < numAllocated; i++)
                m_libva->vaDestroyBuffer(m_dpy, surfaces[i]);
        }
        if (mids)
        {
            free(mids);
            mids = NULL;
        }
        if (vaapi_mids) { free(vaapi_mids); vaapi_mids = NULL; }
        if (surfaces) { free(surfaces); surfaces = NULL; }
    }
    return mfx_res;
}

mfxStatus vaapiFrameAllocator::ReleaseResponse(mfxFrameAllocResponse* response)
{
    vaapiMemId* vaapi_mids = NULL;
    VASurfaceID* surfaces = NULL;
    mfxU32 i = 0;
    bool isBitstreamMemory = false;

    if (!response) return MFX_ERR_NULL_PTR;

    if (response->mids)
    {
        vaapi_mids = (vaapiMemId*)(response->mids[0]);
        mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(vaapi_mids->m_fourcc);
        isBitstreamMemory = (MFX_FOURCC_P8 == mfx_fourcc) ? true : false;
        surfaces = vaapi_mids->m_surface;
        for (i = 0; i < response->NumFrameActual; ++i)
        {
            if (MFX_FOURCC_P8 == vaapi_mids[i].m_fourcc) m_libva->vaDestroyBuffer(m_dpy, surfaces[i]);
            else if (vaapi_mids[i].m_sys_buffer) free(vaapi_mids[i].m_sys_buffer);
        }
        free(vaapi_mids);
        free(response->mids);
        response->mids = NULL;

        if (!isBitstreamMemory) m_libva->vaDestroySurfaces(m_dpy, surfaces, response->NumFrameActual);
        free(surfaces);
    }
    response->NumFrameActual = 0;
    return MFX_ERR_NONE;
}

mfxStatus vaapiFrameAllocator::LockFrame(mfxMemId mid, mfxFrameData* ptr)
{
    mfxStatus mfx_res = MFX_ERR_NONE;
    VAStatus  va_res = VA_STATUS_SUCCESS;
    vaapiMemId* vaapi_mid = (vaapiMemId*)mid;

    if (!vaapi_mid || !(vaapi_mid->m_surface)) return MFX_ERR_INVALID_HANDLE;

    mfxU8* pBuffer = 0;
    mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(vaapi_mid->m_fourcc);

    if ((VASurfaceID)VA_INVALID_ID == *(vaapi_mid->m_surface))
    {
        if (VA_FOURCC_P208 != vaapi_mid->m_fourcc)
        {
            unsigned int format;
            VASurfaceAttrib attrib[2];
            VASurfaceAttrib* pAttrib = &attrib[0];
            int attrCnt = 0;

            attrib[attrCnt].type = VASurfaceAttribPixelFormat;
            attrib[attrCnt].flags = VA_SURFACE_ATTRIB_SETTABLE;
            attrib[attrCnt].value.type = VAGenericValueTypeInteger;
            attrib[attrCnt++].value.value.i = (vaapi_mid->m_fourcc == VA_RT_FORMAT_RGBP ? VA_FOURCC_RGBP : vaapi_mid->m_fourcc);
            format = vaapi_mid->m_fourcc;

            if (mfx_fourcc == MFX_FOURCC_VP8_NV12)
            {
                // special configuration for NV12 surf allocation for VP8 hybrid encoder is required
                attrib[attrCnt].type = (VASurfaceAttribType)VASurfaceAttribUsageHint;
                attrib[attrCnt].flags = VA_SURFACE_ATTRIB_SETTABLE;
                attrib[attrCnt].value.type = VAGenericValueTypeInteger;
                attrib[attrCnt++].value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_ENCODER;
            }
            else if (mfx_fourcc == MFX_FOURCC_VP8_MBDATA)
            {
                // special configuration for MB data surf allocation for VP8 hybrid encoder is required
                attrib[0].value.value.i = VA_FOURCC_P208;
                format = VA_FOURCC_P208;
            }
            else if (vaapi_mid->m_fourcc == VA_FOURCC_NV12)
            {
                format = VA_RT_FORMAT_YUV420;
            }

            va_res = m_libva->vaCreateSurfaces(m_dpy,
                format,
                m_Width, m_Height,
                vaapi_mid->m_surface,
                1,
                pAttrib, pAttrib ? attrCnt : 0);
            mfx_res = va_to_mfx_status(va_res);
        }
        else
        {
            int codedbuf_size, codedbuf_num;

            VABufferType codedbuf_type;
            if (mfx_fourcc == MFX_FOURCC_VP8_SEGMAP)
            {
                codedbuf_size = m_Width;
                codedbuf_num = m_Height;
                codedbuf_type = VAEncMacroblockMapBufferType;
            }
            else
            {
                int width32 = 32 * ((m_Width + 31) >> 5);
                int height32 = 32 * ((m_Height + 31) >> 5);
                codedbuf_size = static_cast<int>((width32 * height32) * 400LL / (16 * 16));
                codedbuf_num = 1;
                codedbuf_type = VAEncCodedBufferType;
            }
            VABufferID coded_buf;

            va_res = m_libva->vaCreateBuffer(m_dpy,
                VA_INVALID_ID,
                codedbuf_type,
                codedbuf_size,
                codedbuf_num,
                NULL,
                &coded_buf);
            mfx_res = va_to_mfx_status(va_res);
            //vaapi_mid->m_surface = coded_buf;
        }
        return MFX_ERR_NONE;
    }

    if (MFX_FOURCC_P8 == mfx_fourcc)   // bitstream processing
    {
        VACodedBufferSegment* coded_buffer_segment;
        if (vaapi_mid->m_fourcc == MFX_FOURCC_VP8_SEGMAP)
            va_res = m_libva->vaMapBuffer(m_dpy, *(vaapi_mid->m_surface), (void**)(&pBuffer));
        else
            va_res = m_libva->vaMapBuffer(m_dpy, *(vaapi_mid->m_surface), (void**)(&coded_buffer_segment));
        mfx_res = va_to_mfx_status(va_res);
        if (MFX_ERR_NONE == mfx_res)
        {
            if (vaapi_mid->m_fourcc == MFX_FOURCC_VP8_SEGMAP)
                ptr->Y = pBuffer;
            else
                ptr->Y = (mfxU8*)coded_buffer_segment->buf;

        }
    }
    else   // Image processing
    {
        va_res = m_libva->vaDeriveImage(m_dpy, *(vaapi_mid->m_surface), &(vaapi_mid->m_image));
        mfx_res = va_to_mfx_status(va_res);

        if (MFX_ERR_NONE == mfx_res)
        {
            va_res = m_libva->vaMapBuffer(m_dpy, vaapi_mid->m_image.buf, (void**)&pBuffer);
            mfx_res = va_to_mfx_status(va_res);
        }
        if (MFX_ERR_NONE == mfx_res)
        {
            switch (vaapi_mid->m_image.format.fourcc)
            {
            case VA_FOURCC_NV12:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->U = pBuffer + vaapi_mid->m_image.offsets[1];
                    ptr->V = ptr->U + 1;
                }
                break;
            case VA_FOURCC_YV12:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->V = pBuffer + vaapi_mid->m_image.offsets[1];
                    ptr->U = pBuffer + vaapi_mid->m_image.offsets[2];
                }
                break;
            case VA_FOURCC_YUY2:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->U = ptr->Y + 1;
                    ptr->V = ptr->Y + 3;
                }
                break;
            case VA_FOURCC_ARGB:
                if (mfx_fourcc == MFX_FOURCC_RGB4)
                {
                    ptr->B = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = ptr->B + 1;
                    ptr->R = ptr->B + 2;
                    ptr->A = ptr->B + 3;
                }
                else return MFX_ERR_LOCK_MEMORY;
                break;
#ifndef ANDROID
            case VA_FOURCC_A2R10G10B10:
                if (mfx_fourcc == MFX_FOURCC_A2RGB10)
                {
                    ptr->B = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = ptr->B;
                    ptr->R = ptr->B;
                    ptr->A = ptr->B;
                }
                else return MFX_ERR_LOCK_MEMORY;
                break;
#endif
            case VA_FOURCC_ABGR:
                if (mfx_fourcc == MFX_FOURCC_BGR4)
                {
                    ptr->R = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = ptr->R + 1;
                    ptr->B = ptr->R + 2;
                    ptr->A = ptr->R + 3;
                }
                else return MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_RGB565:
                if (mfx_fourcc == MFX_FOURCC_RGB565)
                {
                    ptr->B = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = ptr->B;
                    ptr->R = ptr->B;
                }
                else return MFX_ERR_LOCK_MEMORY;
                break;
            case MFX_FOURCC_RGBP:
                if (mfx_fourcc == MFX_FOURCC_RGBP)
                {
                    ptr->R = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->G = pBuffer + vaapi_mid->m_image.offsets[1];
                    ptr->B = pBuffer + vaapi_mid->m_image.offsets[2];
                }
            case VA_FOURCC_P208:
                if (mfx_fourcc == MFX_FOURCC_NV12)
                {
                    ptr->Y = pBuffer + vaapi_mid->m_image.offsets[0];
                }
                else return MFX_ERR_LOCK_MEMORY;
                break;
            case VA_FOURCC_P010:
            case VA_FOURCC_P016:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y16 = (mfxU16*)(pBuffer + vaapi_mid->m_image.offsets[0]);
                    ptr->U16 = (mfxU16*)(pBuffer + vaapi_mid->m_image.offsets[1]);
                    ptr->V16 = ptr->U16 + 1;
                }
                break;
            case VA_FOURCC_AYUV:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->V = pBuffer + vaapi_mid->m_image.offsets[0];
                    ptr->U = ptr->V + 1;
                    ptr->Y = ptr->V + 2;
                    ptr->A = ptr->V + 3;
                }
                break;
            case VA_FOURCC_Y210:
            case VA_FOURCC_Y216:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y16 = (mfxU16*)(pBuffer + vaapi_mid->m_image.offsets[0]);
                    ptr->U16 = ptr->Y16 + 1;
                    ptr->V16 = ptr->Y16 + 3;
                }
                break;
            case VA_FOURCC_Y410:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->Y410 = (mfxY410*)(pBuffer + vaapi_mid->m_image.offsets[0]);
                    ptr->Y = 0;
                    ptr->V = 0;
                    ptr->A = 0;
                }
                break;
            case VA_FOURCC_Y416:
                if (mfx_fourcc != vaapi_mid->m_image.format.fourcc) return MFX_ERR_LOCK_MEMORY;

                {
                    ptr->U16 = (mfxU16*)(pBuffer + vaapi_mid->m_image.offsets[0]);
                    ptr->Y16 = ptr->U16 + 1;
                    ptr->V16 = ptr->Y16 + 1;
                    ptr->A = (mfxU8*)(ptr->V16 + 1);
                }
                break;
            default:
                return MFX_ERR_LOCK_MEMORY;
            }
        }

        ptr->PitchHigh = (mfxU16)(vaapi_mid->m_image.pitches[0] / (1 << 16));
        ptr->PitchLow = (mfxU16)(vaapi_mid->m_image.pitches[0] % (1 << 16));
    }
    return mfx_res;
}

mfxStatus vaapiFrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData* ptr)
{
    vaapiMemId* vaapi_mid = (vaapiMemId*)mid;

    if (!vaapi_mid || !(vaapi_mid->m_surface)) return MFX_ERR_INVALID_HANDLE;

    mfxU32 mfx_fourcc = ConvertVP8FourccToMfxFourcc(vaapi_mid->m_fourcc);

    if (MFX_FOURCC_P8 == mfx_fourcc)   // bitstream processing
    {
        m_libva->vaUnmapBuffer(m_dpy, *(vaapi_mid->m_surface));
    }
    else  // Image processing
    {
        m_libva->vaUnmapBuffer(m_dpy, vaapi_mid->m_image.buf);
        m_libva->vaDestroyImage(m_dpy, vaapi_mid->m_image.image_id);

        if (NULL != ptr)
        {
            ptr->PitchLow = 0;
            ptr->PitchHigh = 0;
            ptr->Y = NULL;
            ptr->U = NULL;
            ptr->V = NULL;
            ptr->A = NULL;
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus vaapiFrameAllocator::GetFrameHDL(mfxMemId mid, mfxHDL* handle)
{
    vaapiMemId* vaapi_mid = (vaapiMemId*)mid;

    if (!handle || !vaapi_mid || !(vaapi_mid->m_surface)) return MFX_ERR_INVALID_HANDLE;

    *handle = vaapi_mid->m_surface; //VASurfaceID* <-> mfxHDL
    return MFX_ERR_NONE;
}

