// Copyright (c) 2007-2020 Intel Corporation
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

#include <iostream>

#include "mfx_common.h"


#include "umc_va_linux.h"

#include "libmfx_core_vaapi.h"
#include "mfx_utils.h"
#include "mfx_session.h"
#include "ippi.h"
#include "mfx_common_decode_int.h"
#include "mfx_enc_common.h"

#include "libmfx_core_hw.h"

#include "umc_va_linux_protected.h"

#include "cm_mem_copy.h"

#include <sys/ioctl.h>

#include "va/va.h"
#include <va/va_backend.h>
#include "va/va_drm.h"
#include <unistd.h>
#include <fcntl.h>

typedef struct drm_i915_getparam {
    int param;
    int *value;
} drm_i915_getparam_t;

#define I915_PARAM_CHIPSET_ID   4
#define DRM_I915_GETPARAM       0x06
#define DRM_IOCTL_BASE          'd'
#define DRM_COMMAND_BASE        0x40
#define DRM_IOWR(nr,type)       _IOWR(DRM_IOCTL_BASE,nr,type)
#define DRM_IOCTL_I915_GETPARAM DRM_IOWR(DRM_COMMAND_BASE + DRM_I915_GETPARAM, drm_i915_getparam_t)

#define TMP_DEBUG

using namespace std;
using namespace UMC;

#ifdef _MSVC_LANG
#pragma warning(disable: 4311) // in HWVideoCORE::TraceFrames(): pointer truncation from 'void*' to 'int'
#endif

static
mfx_device_item getDeviceItem(VADisplay pVaDisplay)
{
    /* This is value by default */
    mfx_device_item retDeviceItem = { 0x0000, MFX_HW_UNKNOWN, MFX_GT_UNKNOWN };

    VADisplayContextP pDisplayContext_test = reinterpret_cast<VADisplayContextP>(pVaDisplay);
    VADriverContextP  pDriverContext_test  = pDisplayContext_test->pDriverContext;

    int fd = *(int*)pDriverContext_test->drm_state;

    /* Now as we know real authenticated fd of VAAPI library,
    * we can call ioctl() to kernel mode driver,
    * get device ID and find out platform type
    * */
    int devID = 0;
    drm_i915_getparam_t gp;
    gp.param = I915_PARAM_CHIPSET_ID;
    gp.value = &devID;

    int ret = ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp);

    if (!ret)
    {
        mfxU32 listSize = (sizeof(listLegalDevIDs) / sizeof(mfx_device_item));

        for (mfxU32 i = 0; i < listSize; ++i)
        {
            if (listLegalDevIDs[i].device_id == devID)
            {
                retDeviceItem = listLegalDevIDs[i];
                break;
            }
        }
    }

    return retDeviceItem;
} // eMFXHWType getDeviceItem (VADisplay pVaDisplay)

template <class Base>
VAAPIVideoCORE_T<Base>::VAAPIVideoCORE_T(
    const mfxU32 adapterNum,
    const mfxU32 numThreadsAvailable,
    const mfxSession session)
          : Base(numThreadsAvailable, session)
          , m_Display(0)
          , m_VAConfigHandle((mfxHDL)VA_INVALID_ID)
          , m_VAContextHandle((mfxHDL)VA_INVALID_ID)
          , m_KeepVAState(false)
          , m_adapterNum(adapterNum)
          , m_bUseExtAllocForHWFrames(false)
          , m_HWType(MFX_HW_UNKNOWN)
          , m_GTConfig(MFX_GT_UNKNOWN)
#if !defined(ANDROID)
          , m_bCmCopy(false)
          , m_bCmCopyAllowed(true)
#else
          , m_bCmCopy(false)
          , m_bCmCopyAllowed(false)
#endif
{
} // VAAPIVideoCORE_T<Base>::VAAPIVideoCORE_T(...)

template <class Base>
VAAPIVideoCORE_T<Base>::~VAAPIVideoCORE_T()
{
    Close();
}

template <class Base>
void VAAPIVideoCORE_T<Base>::Close()
{
    m_KeepVAState = false;
    m_pVA.reset();

    if (m_intDRM >= 0)
    {
        if (m_Display)
        {
            vaTerminate(m_Display);
            m_Display = nullptr;
        }
        close(m_intDRM);
        m_intDRM = -1;
    }
}

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::CheckOrInitDisplay()
{
    if (!m_Display)
    {
        std::string path  = std::string("/dev/dri/renderD") + std::to_string(128 + m_adapterNum);
        VADisplay   displ = nullptr;

        int fd = open(path.c_str(), O_RDWR);
        MFX_CHECK(fd >= 0, MFX_ERR_NOT_INITIALIZED);

        mfx::OnExit closeFD([&displ, fd]
            {
                if (displ)
                    vaTerminate(displ);
                close(fd);
            });

        displ = vaGetDisplayDRM(fd);
        MFX_CHECK(displ, MFX_ERR_NOT_INITIALIZED);

        int vamajor = 0, vaminor = 0;
        MFX_CHECK(VA_STATUS_SUCCESS == vaInitialize(displ, &vamajor, &vaminor), MFX_ERR_NOT_INITIALIZED);

        MFX_SAFE_CALL(this->SetHandle(MFX_HANDLE_VA_DISPLAY, displ));

        m_intDRM = fd;
        closeFD = [] {};
    }

    return MFX_ERR_NONE;
}

template <class Base>
eMFXHWType VAAPIVideoCORE_T<Base>::GetHWType()
{
    std::ignore = MFX_STS_TRACE(this->CheckOrInitDisplay());

    return m_HWType;
}

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::GetHandle(
    mfxHandleType type,
    mfxHDL *handle)
{
    MFX_CHECK_NULL_PTR1(handle);
    UMC::AutomaticUMCMutex guard(this->m_guard);

#if (defined (MFX_ENABLE_CPLIB)) && !defined (MFX_ADAPTIVE_PLAYBACK_DISABLE)
#if (MFX_VERSION >= 1030)
    if (MFX_HANDLE_VA_CONTEXT_ID == (mfxU32)type)
    {
        // not exist handle yet
        MFX_CHECK(m_VAContextHandle != (mfxHDL)VA_INVALID_ID, MFX_ERR_NOT_FOUND);

        *handle = m_VAContextHandle;
        return MFX_ERR_NONE;
    }
    else
#endif
#endif
        return Base::GetHandle(type, handle);

} // mfxStatus VAAPIVideoCORE_T<Base>::GetHandle(mfxHandleType type, mfxHDL *handle)

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::SetHandle(
    mfxHandleType type,
    mfxHDL hdl)
{
    MFX_CHECK_HDL(hdl);

    UMC::AutomaticUMCMutex guard(this->m_guard);
    try
    {
        switch ((mfxU32)type)
        {
#if (defined (MFX_ENABLE_CPLIB)) && !defined (MFX_ADAPTIVE_PLAYBACK_DISABLE)
#if (MFX_VERSION >= 1030)
        case MFX_HANDLE_VA_CONFIG_ID:
            // if device manager already set
            MFX_CHECK(m_VAConfigHandle == (mfxHDL)VA_INVALID_ID, MFX_ERR_UNDEFINED_BEHAVIOR);

            // set external handle
            m_VAConfigHandle = hdl;
            m_KeepVAState = true;
            break;

        case MFX_HANDLE_VA_CONTEXT_ID:
            // if device manager already set
            MFX_CHECK(m_VAContextHandle == (mfxHDL)VA_INVALID_ID, MFX_ERR_UNDEFINED_BEHAVIOR);

            // set external handle
            m_VAContextHandle = hdl;
            m_KeepVAState = true;
            break;
#endif
#endif
        case MFX_HANDLE_VA_DISPLAY:
        {
            // If device manager already set, return error
            MFX_CHECK(!this->m_hdl, MFX_ERR_UNDEFINED_BEHAVIOR);

            this->m_hdl = hdl;
            m_Display   = (VADisplay)this->m_hdl;

            /* As we know right VA handle (pointer),
            * we can get real authenticated fd of VAAPI library(display),
            * and can call ioctl() to kernel mode driver,
            * to get device ID and find out platform type
            */
            const auto devItem = getDeviceItem(m_Display);
            MFX_CHECK(MFX_HW_UNKNOWN != devItem.platform, MFX_ERR_DEVICE_FAILED);

            m_HWType         = devItem.platform;
            m_GTConfig       = devItem.config;
            this->m_deviceId = mfxU16(devItem.device_id);

            const bool disableGpuCopy = false
                ;
            if (disableGpuCopy)
            {
                mfxStatus mfxRes = this->SetCmCopyStatus(false);
                if (MFX_ERR_NONE != mfxRes) {
                    return mfxRes;
                }
            }

            this->m_enabled20Interface = false;
        }
            break;

        default:
            return Base::SetHandle(type, hdl);
        }
        return MFX_ERR_NONE;
    }
    catch (...)
    {
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }
}// mfxStatus VAAPIVideoCORE_T<Base>::SetHandle(mfxHandleType type, mfxHDL handle)

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::TraceFrames(
    mfxFrameAllocRequest* request,
    mfxFrameAllocResponse* response,
    mfxStatus sts)
{
    (void)request;
    (void)response;

    return sts;
}

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::AllocFrames(
    mfxFrameAllocRequest* request,
    mfxFrameAllocResponse* response,
    bool isNeedCopy)
{
    MFX_CHECK_NULL_PTR2(request, response);

    UMC::AutomaticUMCMutex guard(this->m_guard);

    try
    {
        mfxStatus sts = MFX_ERR_NONE;
        mfxFrameAllocRequest temp_request = *request;


        if (!m_bCmCopy && m_bCmCopyAllowed && isNeedCopy && m_Display)
        {
            m_pCmCopy.reset(new CmCopyWrapper);

            if (!m_pCmCopy->GetCmDevice(m_Display))
            {
                m_bCmCopy        = false;
                m_bCmCopyAllowed = false;
                m_pCmCopy.reset();
            }
            else
            {
                sts = m_pCmCopy->Initialize(GetHWType());
                MFX_CHECK_STS(sts);
                m_bCmCopy = true;
            }
        }
        else if (m_bCmCopy)
        {
            if (m_pCmCopy)
                m_pCmCopy->ReleaseCmSurfaces();
            else
                m_bCmCopy = false;
        }


        // use common core for sw surface allocation
        if (request->Type & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            sts = Base::AllocFrames(request, response);
            return TraceFrames(request, response, sts);
        } else
        {
            bool isExtAllocatorCallAllowed = ((request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) &&
                (request->Type & MFX_MEMTYPE_FROM_DECODE)) || // 'fake' Alloc call to retrieve memId's of surfaces already allocated by app
                (request->Type & (MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_FROM_PAK)); // 'fake' Alloc call for FEI ENC/PAC cases to get reconstructed surfaces
            // external allocator
            if (this->m_bSetExtFrameAlloc && isExtAllocatorCallAllowed)
            {
                sts = (*this->m_FrameAllocator.frameAllocator.Alloc)(this->m_FrameAllocator.frameAllocator.pthis, &temp_request, response);

                m_bUseExtAllocForHWFrames = false;
                MFX_CHECK_STS(sts);

                // let's create video accelerator
                // Checking for unsupported mode - external allocator exist but Device handle doesn't set
                MFX_CHECK(m_Display, MFX_ERR_UNSUPPORTED)

                if (response->NumFrameActual < request->NumFrameMin)
                {
                    (*this->m_FrameAllocator.frameAllocator.Free)(this->m_FrameAllocator.frameAllocator.pthis, response);
                    MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
                }

                m_bUseExtAllocForHWFrames = true;
                sts = ProcessRenderTargets(request, response, &this->m_FrameAllocator);
                MFX_CHECK_STS(sts);

                return TraceFrames(request, response, sts);
            }
            else
            {
                // Default Allocator is used for internal memory allocation and all coded buffers allocation
                m_bUseExtAllocForHWFrames = false;
                sts = this->DefaultAllocFrames(request, response);
                MFX_CHECK_STS(sts);

                return TraceFrames(request, response, sts);
            }
        }
    }
    catch(...)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

} // mfxStatus VAAPIVideoCORE_T<Base>::AllocFrames(...)

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::ReallocFrame(mfxFrameSurface1 *surf)
{
    MFX_CHECK_NULL_PTR1(surf);

    mfxMemId memid = surf->Data.MemId;

    if (!(surf->Data.MemType & MFX_MEMTYPE_INTERNAL_FRAME &&
        ((surf->Data.MemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)||
         (surf->Data.MemType & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET))))
        return MFX_ERR_MEMORY_ALLOC;

    mfxFrameAllocator *pFrameAlloc = this->GetAllocatorAndMid(memid);
    if (!pFrameAlloc)
        return MFX_ERR_MEMORY_ALLOC;

    mfxHDL srcHandle;
    if (MFX_ERR_NONE == this->GetFrameHDL(surf->Data.MemId, &srcHandle))
    {
        VASurfaceID *va_surf = (VASurfaceID*)srcHandle;
        return mfxDefaultAllocatorVAAPI::ReallocFrameHW(pFrameAlloc->pthis, surf, va_surf);
    }

    return MFX_ERR_MEMORY_ALLOC;
}

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::DefaultAllocFrames(
    mfxFrameAllocRequest* request,
    mfxFrameAllocResponse* response)
{
    mfxStatus sts = MFX_ERR_NONE;

    if ((request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET)||
        (request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)) // SW - TBD !!!!!!!!!!!!!!
    {
        MFX_SAFE_CALL(this->CheckOrInitDisplay());

        mfxBaseWideFrameAllocator* pAlloc = this->GetAllocatorByReq(request->Type);
        // VPP, ENC, PAK can request frames for several times
        if (pAlloc && (request->Type & MFX_MEMTYPE_FROM_DECODE))
            return MFX_ERR_MEMORY_ALLOC;

        if (!pAlloc)
        {
            m_pcHWAlloc.reset(new mfxDefaultAllocatorVAAPI::mfxWideHWFrameAllocator(request->Type, m_Display));
            pAlloc = m_pcHWAlloc.get();
        }
        // else ???

        pAlloc->frameAllocator.pthis = pAlloc;
        sts = (*pAlloc->frameAllocator.Alloc)(pAlloc->frameAllocator.pthis,request, response);
        MFX_CHECK_STS(sts);
        sts = ProcessRenderTargets(request, response, pAlloc);
        MFX_CHECK_STS(sts);

    }
    else
    {
        return Base::DefaultAllocFrames(request, response);
    }
    ++this->m_NumAllocators;

    return sts;

} // mfxStatus VAAPIVideoCORE_T<Base>::DefaultAllocFrames(...)

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::CreateVA(
    mfxVideoParam* param,
    mfxFrameAllocRequest* request,
    mfxFrameAllocResponse* response,
    UMC::FrameAllocator *allocator)
{
    MFX_CHECK_NULL_PTR3(param, request, response);

    if (!(request->Type & MFX_MEMTYPE_FROM_DECODE) ||
        !(request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET))
        return MFX_ERR_NONE;

    auto const profile = ChooseProfile(param, GetHWType());
    MFX_CHECK(profile != UMC::UNKNOWN, MFX_ERR_UNSUPPORTED);

#ifndef MFX_ADAPTIVE_PLAYBACK_DISABLE
    if (GetExtBuffer(param->ExtParam, param->NumExtParam, MFX_EXTBUFF_DEC_ADAPTIVE_PLAYBACK))
        m_KeepVAState = true;
    else
#endif
        m_KeepVAState = false;

    return CreateVideoAccelerator(param, profile, 0, nullptr, allocator);
} // mfxStatus VAAPIVideoCORE_T<Base>::CreateVA(...)

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::CreateVideoProcessing(mfxVideoParam * param)
{
    (void)param;

#if defined (MFX_ENABLE_VPP)
    if (!m_vpp_hw_resmng.GetDevice()){
        return m_vpp_hw_resmng.CreateDevice(this);
    }

    return MFX_ERR_NONE;
#else
    MFX_RETURN(MFX_ERR_UNSUPPORTED);
#endif
}

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::ProcessRenderTargets(
    mfxFrameAllocRequest* request,
    mfxFrameAllocResponse* response,
    mfxBaseWideFrameAllocator* pAlloc)
{
#if defined(ANDROID)
    if (response->NumFrameActual > 128)
        return MFX_ERR_UNSUPPORTED;
#endif

    this->RegisterMids(response, request->Type, !m_bUseExtAllocForHWFrames, pAlloc);
    m_pcHWAlloc.release();

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoCORE_T<Base>::ProcessRenderTargets(...)

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::GetVAService(
    VADisplay*  pVADisplay)
{
    // check if created already
    MFX_SAFE_CALL(this->CheckOrInitDisplay());

    if (pVADisplay)
    {
        *pVADisplay = m_Display;
    }

    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoCORE_T<Base>::GetVAService(...)

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::SetCmCopyStatus(bool enable)
{
    UMC::AutomaticUMCMutex guard(this->m_guard);

    m_bCmCopyAllowed = enable;

    if (!m_bCmCopyAllowed)
    {
        m_pCmCopy.reset();

        m_bCmCopy = false;
    }

    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoCORE_T<Base>::SetCmCopyStatus(...)

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::CreateVideoAccelerator(
    mfxVideoParam* param,
    int profile,
    int NumOfRenderTarget,
    VASurfaceID* RenderTargets,
    UMC::FrameAllocator *allocator)
{
    MFX_CHECK_NULL_PTR1(param);
    MFX_SAFE_CALL(this->CheckOrInitDisplay());

    UMC::AutomaticUMCMutex guard(this->m_guard);

    UMC::LinuxVideoAcceleratorParams params;
    mfxFrameInfo *pInfo = &(param->mfx.FrameInfo);

    UMC::VideoStreamInfo VideoInfo;
    VideoInfo.clip_info.width  = pInfo->Width;
    VideoInfo.clip_info.height = pInfo->Height;

    // Init Accelerator
    params.m_Display          = m_Display;
    params.m_pConfigId        = (VAConfigID*)&m_VAConfigHandle;
    params.m_pContext         = (VAContextID*)&m_VAContextHandle;
    params.m_pKeepVAState     = &m_KeepVAState;
    params.m_pVideoStreamInfo = &VideoInfo;
    params.m_iNumberSurfaces  = NumOfRenderTarget;
    params.m_allocator        = allocator;
    params.m_surf             = (void **)RenderTargets;

    params.m_protectedVA      = param->Protected;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    /* There are following conditions for post processing via HW fixed function engine:
     * (1): AVC
     * (2): Progressive only
     * (3): Supported on SKL (Core) and APL (Atom) platforms and above
     * (4): Only video memory supported (so, OPAQ memory does not supported!)
     * */
    if ( (GetExtBuffer(param->ExtParam, param->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING)) &&
         (MFX_PICSTRUCT_PROGRESSIVE == param->mfx.FrameInfo.PicStruct) &&
         (MFX_HW_SCL <= GetHWType()) &&
         (param->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
    {
        params.m_needVideoProcessingVA = true;
    }
#endif
        m_pVA.reset(new LinuxVideoAccelerator());

    m_pVA->m_Platform   = UMC::VA_LINUX;
    m_pVA->m_Profile    = (VideoAccelerationProfile)profile;
    m_pVA->m_HWPlatform = m_HWType;

    Status st = m_pVA->Init(&params);
    MFX_CHECK(st == UMC_OK, MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoCORE_T<Base>::CreateVideoAccelerator(...)

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::DoFastCopyWrapper(
    mfxFrameSurface1* pDst,
    mfxU16 dstMemType,
    mfxFrameSurface1* pSrc,
    mfxU16 srcMemType)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VAAPIVideoCORE_T<Base>::DoFastCopyWrapper");
    mfxStatus sts;

    mfxHDLPair srcHandle = {}, dstHandle = {};
    mfxMemId srcMemId, dstMemId;

    mfxFrameSurface1 srcTempSurface, dstTempSurface;

    memset(&srcTempSurface, 0, sizeof(mfxFrameSurface1));
    memset(&dstTempSurface, 0, sizeof(mfxFrameSurface1));

    // save original mem ids
    srcMemId = pSrc->Data.MemId;
    dstMemId = pDst->Data.MemId;

    mfxU8* srcPtr = GetFramePointer(pSrc->Info.FourCC, pSrc->Data);
    mfxU8* dstPtr = GetFramePointer(pDst->Info.FourCC, pDst->Data);

    srcTempSurface.Info = pSrc->Info;
    dstTempSurface.Info = pDst->Info;

    bool isSrcLocked = false;
    bool isDstLocked = false;

    if (srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
    {
        if (srcMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (nullptr == srcPtr)
            {
                sts = this->LockExternalFrame(srcMemId, &srcTempSurface.Data);
                MFX_CHECK_STS(sts);

                isSrcLocked = true;
            }
            else
            {
                srcTempSurface.Data = pSrc->Data;
                srcTempSurface.Data.MemId = 0;
            }
        }
        else if (srcMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = this->GetExternalFrameHDL(srcMemId, (mfxHDL *)&srcHandle);
            MFX_CHECK_STS(sts);

            srcTempSurface.Data.MemId = &srcHandle;
        }
    }
    else if (srcMemType & MFX_MEMTYPE_INTERNAL_FRAME)
    {
        if (srcMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (nullptr == srcPtr)
            {
                sts = this->LockFrame(srcMemId, &srcTempSurface.Data);
                MFX_CHECK_STS(sts);

                isSrcLocked = true;
            }
            else
            {
                srcTempSurface.Data = pSrc->Data;
                srcTempSurface.Data.MemId = 0;
            }
        }
        else if (srcMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = this->GetFrameHDL(srcMemId, (mfxHDL *)&srcHandle);
            MFX_CHECK_STS(sts);

            srcTempSurface.Data.MemId = &srcHandle;
        }
    }

    if (dstMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
    {
        if (dstMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (nullptr == dstPtr)
            {
                sts = this->LockExternalFrame(dstMemId, &dstTempSurface.Data);
                MFX_CHECK_STS(sts);

                isDstLocked = true;
            }
            else
            {
                dstTempSurface.Data = pDst->Data;
                dstTempSurface.Data.MemId = 0;
            }
        }
        else if (dstMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = this->GetExternalFrameHDL(dstMemId, (mfxHDL *)&dstHandle);
            MFX_CHECK_STS(sts);

            dstTempSurface.Data.MemId = &dstHandle;
        }
    }
    else if (dstMemType & MFX_MEMTYPE_INTERNAL_FRAME)
    {
        if (dstMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (nullptr == dstPtr)
            {
                sts = this->LockFrame(dstMemId, &dstTempSurface.Data);
                MFX_CHECK_STS(sts);

                isDstLocked = true;
            }
            else
            {
                dstTempSurface.Data = pDst->Data;
                dstTempSurface.Data.MemId = 0;
            }
        }
        else if (dstMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = this->GetFrameHDL(dstMemId, (mfxHDL *)&dstHandle);
            MFX_CHECK_STS(sts);

            dstTempSurface.Data.MemId = &dstHandle;
        }
    }

    mfxStatus fcSts = DoFastCopyExtended(&dstTempSurface, &srcTempSurface);

    if (MFX_ERR_DEVICE_FAILED == fcSts && 0 != dstTempSurface.Data.Corrupted)
    {
        // complete task even if frame corrupted
        pDst->Data.Corrupted = dstTempSurface.Data.Corrupted;
        fcSts = MFX_ERR_NONE;
    }

    if (true == isSrcLocked)
    {
        if (srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
        {
            sts = this->UnlockExternalFrame(srcMemId, &srcTempSurface.Data);
            MFX_CHECK_STS(fcSts);
            MFX_CHECK_STS(sts);
        }
        else if (srcMemType & MFX_MEMTYPE_INTERNAL_FRAME)
        {
            sts = this->UnlockFrame(srcMemId, &srcTempSurface.Data);
            MFX_CHECK_STS(fcSts);
            MFX_CHECK_STS(sts);
        }
    }

    if (true == isDstLocked)
    {
        if (dstMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
        {
            sts = this->UnlockExternalFrame(dstMemId, &dstTempSurface.Data);
            MFX_CHECK_STS(fcSts);
            MFX_CHECK_STS(sts);
        }
        else if (dstMemType & MFX_MEMTYPE_INTERNAL_FRAME)
        {
            sts = this->UnlockFrame(dstMemId, &dstTempSurface.Data);
            MFX_CHECK_STS(fcSts);
            MFX_CHECK_STS(sts);
        }
    }

    return fcSts;

} // mfxStatus VAAPIVideoCORE_T<Base>::DoFastCopyWrapper(...)

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::DoFastCopyExtended(
    mfxFrameSurface1* pDst,
    mfxFrameSurface1* pSrc)
{
    mfxStatus sts;
    mfxU8* srcPtr;
    mfxU8* dstPtr;

    sts = GetFramePointerChecked(pSrc->Info, pSrc->Data, &srcPtr);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);
    sts = GetFramePointerChecked(pDst->Info, pDst->Data, &dstPtr);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);

    // check that only memId or pointer are passed
    // otherwise don't know which type of memory copying is requested
    if (
        (nullptr != dstPtr && nullptr != pDst->Data.MemId) ||
        (nullptr != srcPtr && nullptr != pSrc->Data.MemId)
        )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    IppiSize roi = {std::min(pSrc->Info.Width, pDst->Info.Width), std::min(pSrc->Info.Height, pDst->Info.Height)};

    // check that region of interest is valid
    if (0 == roi.width || 0 == roi.height)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    bool canUseCMCopy = m_bCmCopy ? CmCopyWrapper::CanUseCmCopy(pDst, pSrc) : false;

    if (NULL != pSrc->Data.MemId && NULL != pDst->Data.MemId)
    {
        if (canUseCMCopy)
        {
            sts = m_pCmCopy->CopyVideoToVideo(pDst, pSrc);
            MFX_CHECK_STS(sts);
        }
        else
        {
            MFX_SAFE_CALL(this->CheckOrInitDisplay());

            VASurfaceID *va_surf_src = (VASurfaceID*)(((mfxHDLPair *)pSrc->Data.MemId)->first);
            VASurfaceID *va_surf_dst = (VASurfaceID*)(((mfxHDLPair *)pDst->Data.MemId)->first);
            MFX_CHECK(va_surf_src != va_surf_dst, MFX_ERR_UNDEFINED_BEHAVIOR);

            VAImage va_img_src = {};
            VAStatus va_sts;

            va_sts = vaDeriveImage(m_Display, *va_surf_src, &va_img_src);
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaPutImage");
                va_sts = vaPutImage(m_Display, *va_surf_dst, va_img_src.image_id,
                                    0, 0, roi.width, roi.height,
                                    0, 0, roi.width, roi.height);
            }
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

            va_sts = vaDestroyImage(m_Display, va_img_src.image_id);
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);
        }
    }
    else if (nullptr != pSrc->Data.MemId && nullptr != dstPtr)
    {
        MFX_SAFE_CALL(this->CheckOrInitDisplay());

        // copy data
        {
            if (canUseCMCopy)
            {
                sts = m_pCmCopy->CopyVideoToSys(pDst, pSrc);
                MFX_CHECK_STS(sts);
            }
            else
            {
                VASurfaceID *va_surface = (VASurfaceID*)(((mfxHDLPair *)pSrc->Data.MemId)->first);
                VAImage va_image;
                VAStatus va_sts;
                void *pBits = NULL;

                va_sts = vaDeriveImage(m_Display, *va_surface, &va_image);
                MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
                    va_sts = vaMapBuffer(m_Display, va_image.buf, (void **) &pBits);
                }
                MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "FastCopy_vid2sys");
                    mfxStatus sts = mfxDefaultAllocatorVAAPI::SetFrameData(va_image, pDst->Info.FourCC, (mfxU8*)pBits, pSrc->Data);
                    MFX_CHECK_STS(sts);

                    mfxMemId saveMemId = pSrc->Data.MemId;
                    pSrc->Data.MemId = 0;

                    sts = CoreDoSWFastCopy(*pDst, *pSrc, COPY_VIDEO_TO_SYS); // sw copy
                    MFX_CHECK_STS(sts);

                    pSrc->Data.MemId = saveMemId;
                    MFX_CHECK_STS(sts);

                }

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
                    va_sts = vaUnmapBuffer(m_Display, va_image.buf);
                }
                MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

                va_sts = vaDestroyImage(m_Display, va_image.image_id);
                MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

            }
        }

    }
    else if (nullptr != srcPtr && nullptr != dstPtr)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "FastCopy_sys2sys");
        // system memories were passed
        // use common way to copy frames
        sts = CoreDoSWFastCopy(*pDst, *pSrc, COPY_SYS_TO_SYS); // sw copy
        MFX_CHECK_STS(sts);
    }
    else if (nullptr != srcPtr && nullptr != pDst->Data.MemId)
    {
        if (canUseCMCopy)
        {
            sts = m_pCmCopy->CopySysToVideo(pDst, pSrc);
            MFX_CHECK_STS(sts);
        }
        else
        {
            VAStatus va_sts = VA_STATUS_SUCCESS;
            VASurfaceID *va_surface = (VASurfaceID*)((mfxHDLPair *)pDst->Data.MemId)->first;
            VAImage va_image;
            void *pBits = NULL;

            MFX_SAFE_CALL(this->CheckOrInitDisplay());

            va_sts = vaDeriveImage(m_Display, *va_surface, &va_image);
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
                va_sts = vaMapBuffer(m_Display, va_image.buf, (void **) &pBits);
            }
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "FastCopy_sys2vid");

                mfxStatus sts = mfxDefaultAllocatorVAAPI::SetFrameData(va_image, pDst->Info.FourCC, (mfxU8*)pBits, pDst->Data);
                MFX_CHECK_STS(sts);

                mfxMemId saveMemId = pDst->Data.MemId;
                pDst->Data.MemId = 0;

                sts = CoreDoSWFastCopy(*pDst, *pSrc, COPY_SYS_TO_VIDEO); // sw copy
                MFX_CHECK_STS(sts);

                pDst->Data.MemId = saveMemId;
                MFX_CHECK_STS(sts);

            }

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
                va_sts = vaUnmapBuffer(m_Display, va_image.buf);
            }
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

            // vaDestroyImage
            va_sts = vaDestroyImage(m_Display, va_image.image_id);
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);
        }
    }
    else
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoCORE_T<Base>::DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)

// On linux/android specific function!
// correct work since libva 1.2 (libva 2.2.1.pre1)
// function checks profile and entrypoint and video resolution support
template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::IsGuidSupported(const GUID guid,
                                         mfxVideoParam *par, bool /* isEncoder */)
{
    MFX_CHECK(par, MFX_WRN_PARTIAL_ACCELERATION);
    MFX_CHECK(!IsMVCProfile(par->mfx.CodecProfile), MFX_WRN_PARTIAL_ACCELERATION);

    MFX_SAFE_CALL(this->CheckOrInitDisplay());

#if VA_CHECK_VERSION(1, 2, 0)
    VaGuidMapper mapper(guid);
    VAProfile req_profile         = mapper.m_profile;
    VAEntrypoint req_entrypoint   = mapper.m_entrypoint;
    mfxI32 va_max_num_entrypoints = vaMaxNumEntrypoints(m_Display);
    mfxI32 va_max_num_profiles    = vaMaxNumProfiles(m_Display);
    MFX_CHECK_COND(va_max_num_entrypoints && va_max_num_profiles);

    //driver always support VAProfileNone
    if (req_profile != VAProfileNone)
    {
        vector <VAProfile> va_profiles (va_max_num_profiles, VAProfileNone);

        //ask driver about profile support
        VAStatus va_sts = vaQueryConfigProfiles(m_Display,
                            va_profiles.data(), &va_max_num_profiles);
        MFX_CHECK(va_sts == VA_STATUS_SUCCESS, MFX_ERR_UNSUPPORTED);

        //check profile support
        auto it_profile = find(va_profiles.begin(), va_profiles.end(), req_profile);
        MFX_CHECK(it_profile != va_profiles.end(), MFX_ERR_UNSUPPORTED);
    }

    vector <VAEntrypoint> va_entrypoints (va_max_num_entrypoints, static_cast<VAEntrypoint> (0));

    //ask driver about entrypoint support
    VAStatus va_sts = vaQueryConfigEntrypoints(m_Display, req_profile,
                    va_entrypoints.data(), &va_max_num_entrypoints);
    MFX_CHECK(va_sts == VA_STATUS_SUCCESS, MFX_ERR_UNSUPPORTED);

    //check entrypoint support
    auto it_entrypoint = find(va_entrypoints.begin(), va_entrypoints.end(), req_entrypoint);
    MFX_CHECK(it_entrypoint != va_entrypoints.end(), MFX_ERR_UNSUPPORTED);

    VAConfigAttrib attr[] = {{VAConfigAttribMaxPictureWidth,  0},
                             {VAConfigAttribMaxPictureHeight, 0}};

    //ask driver about support
    va_sts = vaGetConfigAttributes(m_Display, req_profile,
                                   req_entrypoint,
                                   attr, sizeof(attr)/sizeof(*attr));

    MFX_CHECK(va_sts == VA_STATUS_SUCCESS, MFX_ERR_UNSUPPORTED);

    //check video resolution
    MFX_CHECK(attr[0].value != VA_ATTRIB_NOT_SUPPORTED, MFX_ERR_UNSUPPORTED);
    MFX_CHECK(attr[1].value != VA_ATTRIB_NOT_SUPPORTED, MFX_ERR_UNSUPPORTED);
    MFX_CHECK_COND(attr[0].value && attr[1].value);
    MFX_CHECK(attr[0].value >= par->mfx.FrameInfo.Width, MFX_ERR_UNSUPPORTED);
    MFX_CHECK(attr[1].value >= par->mfx.FrameInfo.Height, MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
#else
    (void)guid;

    switch (par->mfx.CodecId)
    {
    case MFX_CODEC_VC1:
    case MFX_CODEC_AVC:
    case MFX_CODEC_VP9:
        break;
    case MFX_CODEC_HEVC:
        MFX_CHECK(m_HWType >= MFX_HW_HSW, MFX_WRN_PARTIAL_ACCELERATION);
        MFX_CHECK(par->mfx.FrameInfo.Width <= 8192 && par->mfx.FrameInfo.Height <= 8192, MFX_WRN_PARTIAL_ACCELERATION);
        break;
    case MFX_CODEC_MPEG2: //MPEG2 decoder doesn't support resolution bigger than 2K
        MFX_CHECK(par->mfx.FrameInfo.Width <= 2048 && par->mfx.FrameInfo.Height <= 2048, MFX_WRN_PARTIAL_ACCELERATION);
        break;
    case MFX_CODEC_JPEG:
        MFX_CHECK(par->mfx.FrameInfo.Width <= 8192 && par->mfx.FrameInfo.Height <= 8192, MFX_WRN_PARTIAL_ACCELERATION);
        break;
    case MFX_CODEC_VP8:
        MFX_CHECK(m_HWType >= MFX_HW_BDW, MFX_ERR_UNSUPPORTED);
        break;
    default:
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    MFX_CHECK(MFX_CODEC_JPEG == par->mfx.CodecId || MFX_CODEC_HEVC == par->mfx.CodecId ||
        (par->mfx.FrameInfo.Width <= 4096 && par->mfx.FrameInfo.Height <= 4096),
        MFX_WRN_PARTIAL_ACCELERATION
    );

    return MFX_ERR_NONE;
#endif
}

template <class Base>
void* VAAPIVideoCORE_T<Base>::QueryCoreInterface(const MFX_GUID &guid)
{
    if (MFXICOREVAAPI_GUID == guid)
    {
        return (void*) m_pAdapter.get();
    }

    if (MFXICORE_GT_CONFIG_GUID == guid)
    {
        return (void*)&m_GTConfig;
    }

    if (MFXIHWCAPS_GUID == guid)
    {
        return (void*) &this->m_encode_caps;
    }
    if (MFXICORECM_GUID == guid)
    {
        CmDevice* pCmDevice = nullptr;
        if (!m_bCmCopy)
        {
            UMC::AutomaticUMCMutex guard(this->m_guard);

            m_pCmCopy.reset(new CmCopyWrapper);
            pCmDevice = m_pCmCopy->GetCmDevice(m_Display);

            if (!pCmDevice)
                return nullptr;

            if (MFX_ERR_NONE != m_pCmCopy->Initialize(GetHWType()))
                return nullptr;

            m_bCmCopy = true;
        }
        else
        {
            pCmDevice =  m_pCmCopy->GetCmDevice(m_Display);
        }
        return (void*)pCmDevice;
    }

    if (MFXICORECMCOPYWRAPPER_GUID == guid)
    {
        if (!m_pCmCopy)
        {
            UMC::AutomaticUMCMutex guard(this->m_guard);

            m_pCmCopy.reset(new CmCopyWrapper);
            if (!m_pCmCopy->GetCmDevice(m_Display))
            {
                m_bCmCopy        = false;
                m_bCmCopyAllowed = false;

                m_pCmCopy.reset();
                return nullptr;
            }

            if (MFX_ERR_NONE != m_pCmCopy->Initialize(GetHWType()))
                return nullptr;

            m_bCmCopy = true;
        }
        return (void*)m_pCmCopy.get();
    }

    if (MFXICMEnabledCore_GUID == guid)
    {
        if (!m_pCmAdapter)
        {
            UMC::AutomaticUMCMutex guard(this->m_guard);

            m_pCmAdapter.reset(new CMEnabledCoreAdapter(this));
        }
        return (void*)m_pCmAdapter.get();
    }

    if (MFXIHWMBPROCRATE_GUID == guid)
    {
        return (void*) &this->m_encode_mbprocrate;
    }

    return Base::QueryCoreInterface(guid);
} // void* VAAPIVideoCORE_T<Base>::QueryCoreInterface(const MFX_GUID &guid)

bool IsHwMvcEncSupported()
{
    return false;
}

VAAPIVideoCORE20::VAAPIVideoCORE20(
    const mfxU32 adapterNum,
    const mfxU32 numThreadsAvailable,
    const mfxSession session)
    : VAAPIVideoCORE20_base(adapterNum, numThreadsAvailable, session)
{
    m_frame_allocator_wrapper.allocator_hw.reset(new FlexibleFrameAllocatorHW_VAAPI(nullptr, m_session));

    m_enabled20Interface = false;
}

mfxStatus
VAAPIVideoCORE20::SetHandle(
    mfxHandleType type,
    mfxHDL hdl)
{
    MFX_SAFE_CALL(VAAPIVideoCORE20_base::SetHandle(type, hdl));

    if (m_enabled20Interface && type == MFX_HANDLE_VA_DISPLAY)
    {
        // Pass display to allocator
        m_frame_allocator_wrapper.SetDevice(m_Display);
    }

    return MFX_ERR_NONE;
}

VAAPIVideoCORE20::~VAAPIVideoCORE20()
{}

mfxStatus VAAPIVideoCORE20::AllocFrames(
    mfxFrameAllocRequest* request,
    mfxFrameAllocResponse* response,
    bool isNeedCopy)
{
    if (!m_enabled20Interface)
        return VAAPIVideoCORE_T<CommonCORE20>::AllocFrames(request, response, isNeedCopy);

    MFX_CHECK_NULL_PTR2(request, response);

    MFX_CHECK(!(request->Type & 0x0004), MFX_ERR_UNSUPPORTED); // 0x0004 means MFX_MEMTYPE_OPAQUE_FRAME

    UMC::AutomaticUMCMutex guard(this->m_guard);

    try
    {
        mfxStatus sts = MFX_ERR_NONE;

        if (!m_bCmCopy && m_bCmCopyAllowed && isNeedCopy && m_Display)
        {
            m_pCmCopy.reset(new CmCopyWrapper);

            if (!m_pCmCopy->GetCmDevice(m_Display))
            {
                m_bCmCopy = false;
                m_bCmCopyAllowed = false;
                m_pCmCopy.reset();
            }
            else
            {
                sts = m_pCmCopy->Initialize(GetHWType());
                MFX_CHECK_STS(sts);
                m_bCmCopy = true;
            }
        }
        else if (m_bCmCopy)
        {
            if (m_pCmCopy)
                m_pCmCopy->ReleaseCmSurfaces();
            else
                m_bCmCopy = false;
        }

        sts = m_frame_allocator_wrapper.Alloc(*request, *response, request->Type & (MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_FROM_PAK));

#if defined(ANDROID)
        MFX_CHECK(response->NumFrameActual <= 128, MFX_ERR_UNSUPPORTED);
#endif
        return TraceFrames(request, response, sts);
    }
    catch (...)
    {
        MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
    }
} // mfxStatus VAAPIVideoCORE20::AllocFrames(...)


mfxStatus VAAPIVideoCORE20::ReallocFrame(mfxFrameSurface1 *surf)
{
    if (!m_enabled20Interface)
        return VAAPIVideoCORE_T<CommonCORE20>::ReallocFrame(surf);

    MFX_CHECK_NULL_PTR1(surf);

    return m_frame_allocator_wrapper.ReallocSurface(surf->Info, surf->Data.MemId);
}

mfxStatus
VAAPIVideoCORE20::DoFastCopyWrapper(
    mfxFrameSurface1* pDst,
    mfxU16 dstMemType,
    mfxFrameSurface1* pSrc,
    mfxU16 srcMemType)
{
    if (!m_enabled20Interface)
        return VAAPIVideoCORE_T<CommonCORE20>::DoFastCopyWrapper(pDst, dstMemType, pSrc, srcMemType);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VAAPIVideoCORE20::DoFastCopyWrapper");

    MFX_CHECK_NULL_PTR2(pSrc, pDst);

    // TODO: uncomment underlying checks after additional validation
    //MFX_CHECK(!pSrc->Data.MemType || MFX_MEMTYPE_BASE(pSrc->Data.MemType) == MFX_MEMTYPE_BASE(srcMemType), MFX_ERR_UNSUPPORTED);
    //MFX_CHECK(!pDst->Data.MemType || MFX_MEMTYPE_BASE(pDst->Data.MemType) == MFX_MEMTYPE_BASE(dstMemType), MFX_ERR_UNSUPPORTED);

    mfxFrameSurface1 srcTempSurface = *pSrc, dstTempSurface = *pDst;
    srcTempSurface.Data.MemType = srcMemType;
    dstTempSurface.Data.MemType = dstMemType;

    mfxFrameSurface1_scoped_lock src_surf_lock(&srcTempSurface, this), dst_surf_lock(&dstTempSurface, this);
    mfxHDLPair handle_pair_src, handle_pair_dst;

    mfxStatus sts;
    if (srcTempSurface.Data.MemType & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
    {
        clear_frame_data(srcTempSurface.Data);
        sts = SwitchMemidInSurface(srcTempSurface, handle_pair_src);
        MFX_CHECK_STS(sts);
    }
    else
    {
        sts = src_surf_lock.lock(MFX_MAP_READ, SurfaceLockType::LOCK_GENERAL);
        MFX_CHECK_STS(sts);
        srcTempSurface.Data.MemId = 0;
    }

    if (dstTempSurface.Data.MemType & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET)
    {
        clear_frame_data(dstTempSurface.Data);
        sts = SwitchMemidInSurface(dstTempSurface, handle_pair_dst);
        MFX_CHECK_STS(sts);
    }
    else
    {
        sts = dst_surf_lock.lock(MFX_MAP_WRITE, SurfaceLockType::LOCK_GENERAL);
        MFX_CHECK_STS(sts);
        dstTempSurface.Data.MemId = 0;
    }

    sts = DoFastCopyExtended(&dstTempSurface, &srcTempSurface);
    MFX_CHECK_STS(sts);

    sts = src_surf_lock.unlock();
    MFX_CHECK_STS(sts);

    return dst_surf_lock.unlock();
}

mfxStatus
VAAPIVideoCORE20::DoFastCopyExtended(
    mfxFrameSurface1* pDst,
    mfxFrameSurface1* pSrc)
{
    if (!m_enabled20Interface)
        return VAAPIVideoCORE_T<CommonCORE20>::DoFastCopyExtended(pDst, pSrc);

    MFX_CHECK_NULL_PTR2(pDst, pSrc);

    mfxU8 *srcPtr, *dstPtr;

    mfxStatus sts = GetFramePointerChecked(pSrc->Info, pSrc->Data, &srcPtr);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);
    sts = GetFramePointerChecked(pDst->Info, pDst->Data, &dstPtr);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);

    // check that only memId or pointer are passed
    // otherwise don't know which type of memory copying is requested
    MFX_CHECK(!dstPtr || !pDst->Data.MemId, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(!srcPtr || !pSrc->Data.MemId, MFX_ERR_UNDEFINED_BEHAVIOR);

    IppiSize roi = { min(pSrc->Info.Width, pDst->Info.Width), min(pSrc->Info.Height, pDst->Info.Height) };

    // check that region of interest is valid
    MFX_CHECK(roi.width && roi.height, MFX_ERR_UNDEFINED_BEHAVIOR);

    bool canUseCMCopy = m_bCmCopy && CmCopyWrapper::CanUseCmCopy(pDst, pSrc);

    if (NULL != pSrc->Data.MemId && NULL != pDst->Data.MemId)
    {
        if (canUseCMCopy)
        {
            return m_pCmCopy->CopyVideoToVideo(pDst, pSrc);
        }

        MFX_SAFE_CALL(this->CheckOrInitDisplay());

        VASurfaceID *va_surf_src = (VASurfaceID*)(((mfxHDLPair *)pSrc->Data.MemId)->first);
        VASurfaceID *va_surf_dst = (VASurfaceID*)(((mfxHDLPair *)pDst->Data.MemId)->first);
        MFX_CHECK(va_surf_src != va_surf_dst, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK_HDL(va_surf_src);
        MFX_CHECK_HDL(va_surf_dst);

        SurfaceScopedLock src_lock(m_Display, *va_surf_src);
        sts = src_lock.DeriveImage();
        MFX_CHECK_STS(sts);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaPutImage");
            VAStatus va_sts = vaPutImage(m_Display, *va_surf_dst, src_lock.m_image.image_id,
                0, 0, roi.width, roi.height,
                0, 0, roi.width, roi.height);
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);
        }

        return src_lock.DestroyImage();
    }

    if (NULL != pSrc->Data.MemId && NULL != dstPtr)
    {
        MFX_SAFE_CALL(this->CheckOrInitDisplay());

        if (canUseCMCopy)
        {
            return m_pCmCopy->CopyVideoToSys(pDst, pSrc);
        }

        VASurfaceID *va_surface = (VASurfaceID*)(((mfxHDLPair *)pSrc->Data.MemId)->first);
        MFX_CHECK_HDL(va_surface);

        SurfaceScopedLock src_lock(m_Display, *va_surface);
        sts = src_lock.DeriveImage();
        MFX_CHECK_STS(sts);

        mfxU8* pBits;
        sts = src_lock.Map(pBits);
        MFX_CHECK_STS(sts);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "FastCopy_vid2sys");
            mfxStatus sts = mfxDefaultAllocatorVAAPI::SetFrameData(src_lock.m_image, pDst->Info.FourCC, pBits, pSrc->Data);
            MFX_CHECK_STS(sts);

            mfxMemId saveMemId = pSrc->Data.MemId;
            pSrc->Data.MemId = 0;

            sts = CoreDoSWFastCopy(*pDst, *pSrc, COPY_VIDEO_TO_SYS); // sw copy
            MFX_CHECK_STS(sts);

            pSrc->Data.MemId = saveMemId;
            MFX_CHECK_STS(sts);
        }

        sts = src_lock.Unmap();
        MFX_CHECK_STS(sts);

        return src_lock.DestroyImage();
    }

    if (NULL != srcPtr && NULL != dstPtr)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "FastCopy_sys2sys");
        // system memories were passed
        // use common way to copy frames
        return CoreDoSWFastCopy(*pDst, *pSrc, COPY_SYS_TO_SYS); // sw copy
    }

    if (NULL != srcPtr && NULL != pDst->Data.MemId)
    {
        if (canUseCMCopy)
        {
            return m_pCmCopy->CopySysToVideo(pDst, pSrc);
        }

        MFX_SAFE_CALL(this->CheckOrInitDisplay());

        VASurfaceID *va_surface = (VASurfaceID*)(((mfxHDLPair *)pDst->Data.MemId)->first);
        MFX_CHECK_HDL(va_surface);

        SurfaceScopedLock dst_lock(m_Display, *va_surface);
        sts = dst_lock.DeriveImage();
        MFX_CHECK_STS(sts);

        mfxU8* pBits;
        sts = dst_lock.Map(pBits);
        MFX_CHECK_STS(sts);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "FastCopy_sys2vid");

            sts = mfxDefaultAllocatorVAAPI::SetFrameData(dst_lock.m_image, pDst->Info.FourCC, (mfxU8*)pBits, pDst->Data);
            MFX_CHECK_STS(sts);

            mfxMemId saveMemId = pDst->Data.MemId;
            pDst->Data.MemId = 0;

            sts = CoreDoSWFastCopy(*pDst, *pSrc, COPY_SYS_TO_VIDEO); // sw copy
            MFX_CHECK_STS(sts);

            pDst->Data.MemId = saveMemId;
        }

        sts = dst_lock.Unmap();
        MFX_CHECK_STS(sts);

        return dst_lock.DestroyImage();
    }

    MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
} // mfxStatus VAAPIVideoCORE20::DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)

mfxStatus VAAPIVideoCORE20::CreateSurface(mfxU16 type, const mfxFrameInfo& info, mfxFrameSurface1*& surf)
{
    MFX_CHECK(m_enabled20Interface, MFX_ERR_UNSUPPORTED);

    MFX_SAFE_CALL(CheckOrInitDisplay());
    m_frame_allocator_wrapper.SetDevice(m_Display);

    return m_frame_allocator_wrapper.CreateSurface(type, info, surf);
}

template class VAAPIVideoCORE_T<CommonCORE  >;
template class VAAPIVideoCORE_T<CommonCORE20>;

/* EOF */
