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

#ifdef MFX_ENABLE_EXT
#include "cm_mem_copy.h"
#else
#include "cm_mem_copy_stub.h"
#endif

#include <sys/ioctl.h>

#include "va/va.h"
#include <va/va_backend.h>
#include "va/va_drm.h"
#include <unistd.h>
#include <fcntl.h>

#if defined(MFX_ENABLE_PXP)
#include "mfx_pxp_video_accelerator_vaapi.h"
#endif // MFX_ENABLE_PXP

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

using namespace std;
using namespace UMC;

static
mfx_device_item getDeviceItem(VADisplay pVaDisplay)
{
    /* This is value by default */
    mfx_device_item retDeviceItem = { 0x0000, MFX_HW_UNKNOWN, MFX_GT_UNKNOWN };
    int devID = 0;
    int ret = -1;
#if VA_CHECK_VERSION(1, 15, 0)
    VADisplayAttribute attr = {};
    attr.type = VADisplayPCIID;
    auto sts = vaGetDisplayAttributes(pVaDisplay, &attr, 1);
    if (VA_STATUS_SUCCESS == sts &&
        VA_DISPLAY_ATTRIB_GETTABLE == attr.flags)
    {
        devID = attr.value & 0xffff;
        ret = 0;
    }
#else
    VADisplayContextP pDisplayContext_test = reinterpret_cast<VADisplayContextP>(pVaDisplay);
    VADriverContextP  pDriverContext_test  = pDisplayContext_test->pDriverContext;

    int fd = *(int*)pDriverContext_test->drm_state;

    /* Now as we know real authenticated fd of VAAPI library,
    * we can call ioctl() to kernel mode driver,
    * get device ID and find out platform type
    * */
    drm_i915_getparam_t gp;
    gp.param = I915_PARAM_CHIPSET_ID;
    gp.value = &devID;
    ret = ioctl(fd, DRM_IOCTL_I915_GETPARAM, &gp);
#endif
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

class VACopyWrapper
{
public:
    enum eMode
    {
        VACOPY_UNSUPPORTED = -1
        , VACOPY_VIDEO_TO_VIDEO
        , VACOPY_SYSTEM_TO_VIDEO
        , VACOPY_VIDEO_TO_SYSTEM
    };

    enum eEngine
    {
        INVALID = uint32_t(-1)
        , DEFAULT = VA_EXEC_MODE_DEFAULT
        , BLT = VA_EXEC_MODE_POWER_SAVING
        , EU = VA_EXEC_MODE_PERFORMANCE
        , VE = EU + 1
    };

    struct FCCDesc
    {
        uint32_t VAFourcc;
        std::function<void(const mfxFrameSurface1&, VASurfaceAttribExternalBuffers&, bool)> SetBuffers;
        std::function<bool(const mfxFrameSurface1&)> CheckPlanes;
    };

    struct Buffer
    {
        bool bLocked = false;
        std::vector<uint8_t> Buffer;
    };

    class SurfaceWrapper
    {
    public:
        SurfaceWrapper(VADisplay dpy, const mfxFrameSurface1& surf, Buffer* pStagingBuffer, uint32_t copyEngine)
            : m_dpy(dpy)
        {
            if (copyEngine == EU || copyEngine == BLT)
                m_pitchAlign = 16;
            else if(copyEngine == VE)
                m_pitchAlign = 64;

            if (pStagingBuffer)
            {
                m_pBuffer = &pStagingBuffer->Buffer;
                AcquireSurface(surf);
            }
            else
            {
                m_id = *(VASurfaceID*)((mfxHDLPair*)surf.Data.MemId)->first;
            }
        }

        ~SurfaceWrapper()
        {
            if (m_id != VA_INVALID_SURFACE && m_bDestroySurface)
                std::ignore = MFX_STS_TRACE(vaDestroySurfaces(m_dpy, &m_id, 1));
        }

        VASurfaceID GetId() const
        {
            return m_id;
        }

        void CopyUserToStaging()
        {
            if (m_bUseStaging)
                Copy(m_user, m_staging);
        }

        void CopyStagingToUser()
        {
            if (m_bUseStaging)
                Copy(m_staging, m_user);
        }

    protected:
        void AcquireSurface(const mfxFrameSurface1& surf)
        {
            auto& fcc = FccMap.at(surf.Info.FourCC);
            VASurfaceAttrib attrib[3] = {};
            VASurfaceAttribExternalBuffers eb = {};

            attrib[0].flags         = VA_SURFACE_ATTRIB_SETTABLE;
            attrib[0].type          = VASurfaceAttribPixelFormat;
            attrib[0].value.type    = VAGenericValueTypeInteger;
            attrib[0].value.value.i = fcc.VAFourcc;

            attrib[1].flags         = VA_SURFACE_ATTRIB_SETTABLE;
            attrib[1].type          = VASurfaceAttribMemoryType;
            attrib[1].value.type    = VAGenericValueTypeInteger;
            attrib[1].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;

            attrib[2].flags         = VA_SURFACE_ATTRIB_SETTABLE;
            attrib[2].type          = VASurfaceAttribExternalBufferDescriptor;
            attrib[2].value.type    = VAGenericValueTypePointer;
            attrib[2].value.value.p = (void*)&eb;

            eb = SetBuffers(surf, fcc);

            auto vaSts = vaCreateSurfaces(m_dpy, eb.pixel_format, eb.width, eb.height, &m_id, 1, attrib, 3);

            if (vaSts != VA_STATUS_SUCCESS)
                m_id = VA_INVALID_SURFACE;
            else
                m_bDestroySurface = true;
        }

        VASurfaceAttribExternalBuffers SetBuffers(const mfxFrameSurface1& surf, const VACopyWrapper::FCCDesc& fcc)
        {
            auto GetPlane0Size = [](const VASurfaceAttribExternalBuffers& eb)
            {
                return (eb.num_planes > 1 ? eb.offsets[1] : eb.data_size) - eb.offsets[0];
            };
            auto upBase        = uintptr_t(GetFramePointer(surf));
            m_user.pixel_format = fcc.VAFourcc;
            m_user.width        = surf.Info.Width;
            m_user.height       = surf.Info.Height;
            m_user.flags        = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;
            m_user.buffers      = m_buffersUser;
            m_user.buffers[0]   = (upBase - (upBase % BASE_ADDR_ALIGN));
            m_user.offsets[0]   = (upBase - m_user.buffers[0]);

            m_user.pitches[0]   = VACopyWrapper::GetPitch(surf);
            m_user.num_buffers  = 1;

            m_staging = m_user;

            fcc.SetBuffers(surf, m_user, true);

            m_bUseStaging =
                   (!m_bOffsetSupported && m_user.offsets[0])                       // data offset from mem page start
                || (!m_bOffsetSupported && (m_user.data_size % BASE_ADDR_ALIGN))    // data size alignment
                || (m_user.pitches[0] % m_pitchAlign)                               // H-pitch
                || ((GetPlane0Size(m_user) / m_user.pitches[0]) % m_pitchAlign)     // V-pitch
                || !fcc.CheckPlanes(surf);                                          // continuous planes allocation

            if (!m_bUseStaging)
            {
                m_staging = {};
                return m_user;
            }

            m_staging.offsets[0] = 0;
            m_staging.pitches[0] = mfx::align2_value(m_user.pitches[0], m_pitchAlign);

            fcc.SetBuffers(surf, m_staging, false);

            if(m_pBuffer->size() < (m_staging.data_size + BASE_ADDR_ALIGN))
                m_pBuffer->resize(m_staging.data_size + BASE_ADDR_ALIGN);

            m_staging.buffers    = m_buffersStaging;
            m_staging.buffers[0] = uintptr_t(mfx::align2_value(uintptr_t(m_pBuffer->data()), BASE_ADDR_ALIGN));
            m_staging.data_size  = mfx::align2_value(m_staging.data_size, BASE_ADDR_ALIGN);

            return m_staging;
        }

        void Copy(const VASurfaceAttribExternalBuffers& src, VASurfaceAttribExternalBuffers& dst)
        {
            for (uint32_t plane = 0; plane < dst.num_planes; ++plane)
            {
                auto pSrc = (uint8_t*)(src.buffers[0] + src.offsets[plane]);
                auto pDst = (uint8_t*)(dst.buffers[0] + dst.offsets[plane]);
                auto hpitchSrc = src.pitches[plane];
                auto hpitchDst = dst.pitches[plane];
                auto vpitchSrc = (src.num_planes > (plane + 1))
                    ? (src.offsets[plane + 1] - src.offsets[plane]) / hpitchSrc
                    : (src.data_size - src.offsets[plane]) / hpitchSrc;
                auto vpitchDst = (dst.num_planes > (plane + 1))
                    ? (dst.offsets[plane + 1] - dst.offsets[plane]) / hpitchDst
                    : (dst.data_size - dst.offsets[plane]) / hpitchDst;
                auto roiW = std::min(hpitchSrc, hpitchDst);
                auto roiH = std::min(vpitchSrc, vpitchDst);

                for (uint32_t y = 0; y < roiH; ++y)
                {
                    std::copy(pSrc, pSrc + roiW, pDst);
                    pSrc += hpitchSrc;
                    pDst += hpitchDst;
                }
            }
        }

        static const uint32_t BASE_ADDR_ALIGN = 0x1000; // vaCreateSurfaces requires user ptr data to be aligned to memory page
        bool m_bOffsetSupported = false; // is copy engine supports data offset from mem page start
                                         // it looks like is unsupported by vaCreateSurfaces atm
        uint32_t m_pitchAlign = 64; // copy engine requirements to both h-pitch and v-pitch
        VADisplay m_dpy = nullptr;
        VASurfaceID m_id = VA_INVALID_SURFACE;
        VASurfaceAttribExternalBuffers m_user = {}, m_staging = {};
        uintptr_t m_buffersUser[1] = {}, m_buffersStaging[1] = {};
        bool m_bUseStaging = false;
        std::vector<uint8_t>* m_pBuffer = nullptr;
        bool m_bDestroySurface = false;
    };

    static const std::map<mfxU32, FCCDesc> FccMap;
    VADisplay  m_dpy = nullptr;
    uint32_t   m_copyEngine = INVALID;

    VACopyWrapper(VADisplay dpy)
        : m_dpy(dpy)
    {
        m_copyEngine = DEFAULT;
    }

    bool IsSupported() const
    {
        return m_dpy && m_copyEngine != INVALID;
    }

    eMode GetMode(const mfxFrameSurface1& src, const mfxFrameSurface1& dst) const
    {
        bool bSupported =
               IsSupported()
            && dst.Info.FourCC == src.Info.FourCC
            && !dst.Info.Shift == !src.Info.Shift
            && FccMap.count(dst.Info.FourCC)
            ;

        if (bSupported)
        {
            auto pSrc   = GetFramePointer(src);
            auto pDst   = GetFramePointer(dst);
            auto midSrc = src.Data.MemId;
            auto midDst = dst.Data.MemId;

            if (midSrc && midDst)
                return VACOPY_VIDEO_TO_VIDEO;

            if (pSrc && midDst)
                return VACOPY_SYSTEM_TO_VIDEO;

            if (midSrc && pDst)
                return VACOPY_VIDEO_TO_SYSTEM;
        }

        return VACOPY_UNSUPPORTED;
    }

    mfxStatus Copy(const mfxFrameSurface1& src, const mfxFrameSurface1& dst, eEngine forceEngine = DEFAULT)
    {
        uint32_t copyEngine = (forceEngine == DEFAULT) ? m_copyEngine : uint32_t(forceEngine);

        auto copyMode = GetMode(src, dst);
        MFX_CHECK(copyMode != VACOPY_UNSUPPORTED, MFX_ERR_UNSUPPORTED);

        if (   src.Info.Width != dst.Info.Width
            || src.Info.Height != dst.Info.Height)
            copyEngine = BLT;

        auto pSrcBuffer = GetSrcBuffer(copyMode);
        mfx::OnExit releaseSrc([&] { Release(pSrcBuffer); });

        auto pDstBuffer = GetDstBuffer(copyMode);
        mfx::OnExit releaseDst([&] { Release(pDstBuffer); });

        SurfaceWrapper surfSrc(m_dpy, src, pSrcBuffer, copyEngine);
        MFX_CHECK(surfSrc.GetId() != VA_INVALID_SURFACE, MFX_ERR_DEVICE_FAILED);

        SurfaceWrapper surfDst(m_dpy, dst, pDstBuffer, copyEngine);
        MFX_CHECK(surfDst.GetId() != VA_INVALID_SURFACE, MFX_ERR_DEVICE_FAILED);

        surfSrc.CopyUserToStaging();

        VACopyObject objSrc = {}, objDst = {};
        VACopyOption opt = {};

        objSrc.obj_type          = VACopyObjectSurface;
        objSrc.object.surface_id = surfSrc.GetId();

        objDst.obj_type          = VACopyObjectSurface;
        objDst.object.surface_id = surfDst.GetId();

        opt.bits.va_copy_mode = copyEngine;
        opt.bits.va_copy_sync = VA_EXEC_SYNC;

        auto vaCopySts = vaCopy(m_dpy, &objDst, &objSrc, opt);
        MFX_CHECK(vaCopySts == VA_STATUS_SUCCESS, MFX_ERR_DEVICE_FAILED);

        surfDst.CopyStagingToUser();

        return MFX_ERR_NONE;
    }

    static bool IsSupportedByPlatform(eMFXHWType hw_type)
    {
        switch (hw_type)
        {
        case MFX_HW_DG2:
        case MFX_HW_PVC:
        case MFX_HW_MTL:
        case MFX_HW_ARL:
        case MFX_HW_LNL:
        case MFX_HW_BMG:
        case MFX_HW_PTL:
            return true;
        default:
            return false;
        }
    }

    static void SetBuffersNV12(const mfxFrameSurface1& surf, VASurfaceAttribExternalBuffers& eb, bool bUsePtrs)
    {
        eb.offsets[1] =
            bUsePtrs
            ? uint32_t(uintptr_t(surf.Data.UV) - eb.buffers[0])
            : uint32_t(eb.pitches[0] * eb.height);
        eb.pitches[1] = eb.pitches[0];
        eb.num_planes = 2;
        eb.data_size = eb.offsets[1] + (eb.height * eb.pitches[1] / 2);
    }

    static void SetBuffersYV12(const mfxFrameSurface1& surf, VASurfaceAttribExternalBuffers& eb, bool bUsePtrs)
    {
        eb.pitches[1] = eb.pitches[0] / 2;
        eb.pitches[2] = eb.pitches[0] / 2;
        if (bUsePtrs)
        {
            eb.offsets[1] = uint32_t(uintptr_t(surf.Data.V) - eb.buffers[0]);
            eb.offsets[2] = uint32_t(uintptr_t(surf.Data.U) - eb.buffers[0]);
        }
        else
        {
            eb.offsets[1] = eb.offsets[0] + uint32_t(eb.height * eb.pitches[0]);
            eb.offsets[2] = eb.offsets[1] + uint32_t(eb.height * eb.pitches[1] / 2);
        }
        eb.num_planes = 3;
        eb.data_size = eb.offsets[2] + (eb.height * eb.pitches[2] / 2);
    }

    static void SetBuffersOnePlane(const mfxFrameSurface1& surf, VASurfaceAttribExternalBuffers& eb, bool /*bUsePtrs*/)
    {
        eb.num_planes = 1;
        eb.data_size = eb.offsets[0] + (eb.height * eb.pitches[0]);
    }

    template<uint32_t N>
    static void SetBuffersNPlanes444(const mfxFrameSurface1& surf, VASurfaceAttribExternalBuffers& eb, bool bUsePtrs)
    {
        eb.num_planes = N;
        if (bUsePtrs)
        {
            std::list<uintptr_t> ptrs({ uintptr_t(surf.Data.A), uintptr_t(surf.Data.B), uintptr_t(surf.Data.G), uintptr_t(surf.Data.R) });

            ptrs.sort();
            while (ptrs.size() >= N)
                ptrs.pop_front();

            for (uint32_t plane = 1; plane < eb.num_planes; ++plane)
            {
                eb.pitches[plane] = eb.pitches[0];
                eb.offsets[plane] = uint32_t(ptrs.front() - eb.buffers[0]);
                ptrs.pop_front();
            }
        }
        else
        {
            for (uint32_t plane = 1; plane < eb.num_planes; ++plane)
            {
                eb.pitches[plane] = eb.pitches[0];
                eb.offsets[plane] = uint32_t(eb.pitches[0] * eb.height) * plane;
            }
        }

        eb.data_size = eb.offsets[N - 1] + (eb.height * eb.pitches[0]);
    }

    static uint32_t GetPitch(const mfxFrameSurface1& surf)
    {
        return (surf.Data.PitchHigh << 16) + surf.Data.PitchLow;
    }

    static bool CheckPlanesNV12(const mfxFrameSurface1& surf)
    {
        uint32_t  pitch = GetPitch(surf);
        ptrdiff_t offset = surf.Data.UV - (surf.Data.Y + surf.Info.Height * pitch);
        return offset == 0;
    }

    static bool CheckPlanesYV12(const mfxFrameSurface1& surf)
    {
        uint32_t  pitch   = GetPitch(surf);
        ptrdiff_t offsetV = surf.Data.V - (surf.Data.Y + surf.Info.Height * pitch);
        ptrdiff_t offsetU = surf.Data.U - (surf.Data.V + surf.Info.Height * pitch / 4);
        return offsetV == 0 && offsetU == 0;
    }

    static bool CheckPlanes444(const mfxFrameSurface1& surf)
    {
        uint32_t pitch = GetPitch(surf);
        std::list<uintptr_t> ptrs({ uintptr_t(surf.Data.A), uintptr_t(surf.Data.B), uintptr_t(surf.Data.G), uintptr_t(surf.Data.R) });
        ptrs.sort();
        ptrs.remove(0);

        while (ptrs.size() >= 2)
        {
            uintptr_t ptr0 = ptrs.front();
            ptrs.pop_front();

            if (ptrs.front() != (ptr0 + pitch * surf.Info.Height))
                return false;
        }

        return true;
    }

    static bool CheckPlanesOne(const mfxFrameSurface1&)
    {
        return true;
    }

    static bool IsTwoPlanesFormat(mfxU32 fourcc)
    {
        return fourcc == MFX_FOURCC_NV12
            || fourcc == MFX_FOURCC_P010
            || fourcc == MFX_FOURCC_P016;
    }

    static bool IsVaCopySupportSurface(const mfxFrameSurface1& dst_surface, const mfxFrameSurface1& src_surface, eMFXHWType platform)
    {
        if (dst_surface.Info.FourCC != src_surface.Info.FourCC)
        {
            return false;
        }

        if (platform >= MFX_HW_MTL)
        {
            if (dst_surface.Info.FourCC != MFX_FOURCC_NV12 &&
                dst_surface.Info.FourCC != MFX_FOURCC_P010 &&
                dst_surface.Info.FourCC != MFX_FOURCC_P016 &&
                dst_surface.Info.FourCC != MFX_FOURCC_YUY2 &&
                dst_surface.Info.FourCC != MFX_FOURCC_Y210 &&
                dst_surface.Info.FourCC != MFX_FOURCC_Y216 &&
                dst_surface.Info.FourCC != MFX_FOURCC_Y416
                )
            {
                return false;
            }
        }

        auto CheckOneSurface = [](const mfxFrameSurface1& sw_surface)
        {
            // Only start addresses with 4k aligment for UsrPtr surface are supported, refer: https://dri.freedesktop.org/docs/drm/gpu/driver-uapi.html#c.drm_i915_gem_userptr
            if ((size_t)sw_surface.Data.Y % BASE_ADDR_ALIGN)
                return false;

            // Pitch should be 16-aligned
            if (sw_surface.Data.Pitch % 16)
                return false;

            // Pixel data is stored continuously for chroma and Luma for multiplane format.
            if (!IsTwoPlanesFormat(sw_surface.Info.FourCC))
                return true;

            // Two planes format
            size_t luma_size_in_bytes_aligned = (mfxU32)sw_surface.Data.Pitch * mfx::align2_value(sw_surface.Info.Height, 32);
            size_t luma_size_in_bytes         = (mfxU32)sw_surface.Data.Pitch * mfx::align2_value(sw_surface.Info.Height, 1);
            // Assume that frame data is stored in continuous chunk (Chroma right after Luma)
            // use relative offset between UV and Y, not pitch * height
            // Two cases need to be checked:
            // 1. Height 32 aligned, e.g, internal allocator need this path
            // 2. Height not aligned, i.e., app uses specific allocator without alignment.
            // vaCopy can't support the two planes' formats which are not stored continuously.
            // App need copy each plane as 1 plane format if it still keep Luma/ chroma plan using noncontinuous chunk.

            return (sw_surface.Data.Y + luma_size_in_bytes_aligned == sw_surface.Data.UV)
                || (sw_surface.Data.Y + luma_size_in_bytes         == sw_surface.Data.UV);
        };

        // SW to SW copy is not supported
        if (dst_surface.Data.Y && src_surface.Data.Y)
            return false;

        // If dst surface is in SW memory, check it's data layout
        if (dst_surface.Data.Y && !CheckOneSurface(dst_surface))
            return false;

        // If src surface is in SW memory, check it's data layout
        if (src_surface.Data.Y && !CheckOneSurface(src_surface))
            return false;

        return true;
    }

protected:
    static const uint32_t VACOPY_CACHE_SIZE    = 3;
    static const uint32_t VACOPY_CACHE_WAIT_MS = 2000;
    std::mutex m_mtx;
    std::condition_variable m_cv;
    Buffer m_buffer[VACOPY_CACHE_SIZE];

    Buffer* GetBuffer()
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        Buffer* pBuffer = nullptr;
        auto FindFreeBuffer = [&]()
        {
            for (auto& buf : m_buffer)
            {
                if (!buf.bLocked)
                {
                    buf.bLocked = true;
                    pBuffer = &buf;
                    return true;
                }
            }
            return false;
        };

        auto waitTime = std::chrono::milliseconds(VACOPY_CACHE_WAIT_MS);
        MFX_CHECK_WITH_THROW_STS(m_cv.wait_for(lock, waitTime, FindFreeBuffer), MFX_ERR_UNKNOWN);

        return pBuffer;
    }

    Buffer* GetSrcBuffer(eMode mode)
    {
        if (mode == VACOPY_SYSTEM_TO_VIDEO)
            return GetBuffer();
        return nullptr;
    }

    Buffer* GetDstBuffer(eMode mode)
    {
        if (mode == VACOPY_VIDEO_TO_SYSTEM)
            return GetBuffer();
        return nullptr;
    }

    void Release(Buffer* pBuffer)
    {
        if (pBuffer)
        {
            {
                std::unique_lock<std::mutex> lock(m_mtx);
                pBuffer->bLocked = false;
            }
            m_cv.notify_one();
        }
    }
};

const std::map<mfxU32, VACopyWrapper::FCCDesc> VACopyWrapper::FccMap =
{
      { MFX_FOURCC_NV12,    { VA_FOURCC_NV12,        SetBuffersNV12,          CheckPlanesNV12 } }
    , { MFX_FOURCC_YV12,    { VA_FOURCC_YV12,        SetBuffersYV12,          CheckPlanesYV12 } }
    , { MFX_FOURCC_P010,    { VA_FOURCC_P010,        SetBuffersNV12,          CheckPlanesNV12 } }
    , { MFX_FOURCC_P016,    { VA_FOURCC_P016,        SetBuffersNV12,          CheckPlanesNV12 } }
    , { MFX_FOURCC_YUY2,    { VA_FOURCC_YUY2,        SetBuffersOnePlane,      CheckPlanesOne } }
    , { MFX_FOURCC_RGB565,  { VA_FOURCC_RGB565,      SetBuffersOnePlane,      CheckPlanesOne } }
    , { MFX_FOURCC_RGBP,    { VA_FOURCC_RGBP,        SetBuffersNPlanes444<3>, CheckPlanes444 } }
    , { MFX_FOURCC_BGRP,    { VA_FOURCC_BGRP,        SetBuffersNPlanes444<3>, CheckPlanes444 } }
    , { MFX_FOURCC_A2RGB10, { VA_FOURCC_A2R10G10B10, SetBuffersOnePlane,      CheckPlanesOne } }
    , { MFX_FOURCC_AYUV,    { VA_FOURCC_AYUV,        SetBuffersOnePlane,      CheckPlanesOne } }
    , { MFX_FOURCC_Y210,    { VA_FOURCC_Y210,        SetBuffersOnePlane,      CheckPlanesOne } }
    , { MFX_FOURCC_Y216,    { VA_FOURCC_Y216,        SetBuffersOnePlane,      CheckPlanesOne } }
    , { MFX_FOURCC_Y410,    { VA_FOURCC_Y410,        SetBuffersOnePlane,      CheckPlanesOne } }
    , { MFX_FOURCC_Y416,    { VA_FOURCC_Y416,        SetBuffersOnePlane,      CheckPlanesOne } }
};

template <class Base>
VAAPIVideoCORE_T<Base>::VAAPIVideoCORE_T(
    const mfxU32 adapterNum,
    const AffinityMaskType& affinityMask,
    const mfxU32 numThreadsAvailable,
    const mfxSession session)
          : Base(numThreadsAvailable, session)
          , m_VAConfigHandle((mfxHDL)VA_INVALID_ID)
          , m_VAContextHandle((mfxHDL)VA_INVALID_ID)
          , m_KeepVAState(false)
          , m_adapterNum(adapterNum)
          , m_affinityMask(affinityMask)
          , m_bUseExtAllocForHWFrames(false)
          , m_HWType(MFX_HW_UNKNOWN)
          , m_GTConfig(MFX_GT_UNKNOWN)
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

    // It's important to close CM device when VA display is alive
    m_pCmCopy.reset();

#if defined (MFX_ENABLE_VPP)
    // Destroy this object when VA display is alive
    m_vpp_hw_resmng.Close();
#endif

    m_p_display_wrapper.reset();
}

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::CheckOrInitDisplay()
{
    if (!m_p_display_wrapper)
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

        if (m_affinityMask.first)
        {
            auto GetSubDeviceIdxPlus1 = [](const AffinityMaskType & affinityMask) -> mfxU8
            {
                mfxU8 byteIdx = 0;
                for (auto val : affinityMask.second)
                {
                    const auto length = affinityMask.first - (8*byteIdx);
                    val &= (1<<length) - 1;
                    if (val)
                        return ffs(val);
                }
                return 0;
            };
            VADisplayAttribValSubDevice subDev = {};
            subDev.bits.current_sub_device = GetSubDeviceIdxPlus1(m_affinityMask) - 1;
            VADisplayAttribute attr = { VADisplayAttribSubDevice, 0, 0, static_cast<int32_t>(subDev.value), VA_DISPLAY_ATTRIB_SETTABLE};
            MFX_CHECK(VA_STATUS_SUCCESS == vaSetDisplayAttributes(displ, &attr, 1), MFX_ERR_NOT_INITIALIZED);
        }

        MFX_SAFE_CALL(this->SetHandle(MFX_HANDLE_VA_DISPLAY, displ));

        m_p_display_wrapper = std::make_shared<VADisplayWrapper>(displ, fd);

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
mfxU16 VAAPIVideoCORE_T<Base>::GetHWDeviceId()
{
    std::ignore = MFX_STS_TRACE(this->CheckOrInitDisplay());

    return this->m_deviceId;
}

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::GetHandle(
    mfxHandleType type,
    mfxHDL *handle)
{
    MFX_CHECK_NULL_PTR1(handle);
    UMC::AutomaticUMCMutex guard(this->m_guard);

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
        switch (type)
        {
        case MFX_HANDLE_VA_DISPLAY:
        {
            // If device manager already set, return error
            MFX_CHECK(!this->m_hdl,         MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(!m_p_display_wrapper, MFX_ERR_UNDEFINED_BEHAVIOR);

            this->m_hdl         = hdl;
            m_p_display_wrapper = std::make_shared<VADisplayWrapper>(reinterpret_cast<VADisplay>(this->m_hdl));

            /* As we know right VA handle (pointer),
            * we can get real authenticated fd of VAAPI library(display),
            * and can call ioctl() to kernel mode driver,
            * to get device ID and find out platform type
            */
            const auto devItem = getDeviceItem(*m_p_display_wrapper);
            MFX_CHECK(MFX_HW_UNKNOWN != devItem.platform, MFX_ERR_DEVICE_FAILED);

            m_HWType         = devItem.platform;
            m_GTConfig       = devItem.config;
            this->m_deviceId = mfxU16(devItem.device_id);

            std::ignore = MFX_STS_TRACE(TryInitializeCm(false));

            if (VACopyWrapper::IsSupportedByPlatform(m_HWType))
            {
                this->m_pVaCopy.reset(new VACopyWrapper(*m_p_display_wrapper));
                if (!this->m_pVaCopy->IsSupported())
                {
                    this->m_pVaCopy.reset();
                }
            }
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
bool VAAPIVideoCORE_T<Base>::IsCmSupported()
{
    return GetHWType() < MFX_HW_DG2;
}

template <class Base>
bool VAAPIVideoCORE_T<Base>::IsCmCopyEnabledByDefault()
{
    // For Linux by default CM copy is ON on RKL
    return IsCmSupported() && GetHWType() != MFX_HW_DG1 && GetHWType() != MFX_HW_TGL_LP && GetHWType() != MFX_HW_ADL_P && GetHWType() != MFX_HW_ADL_S;
}

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::TryInitializeCm(bool force_cm_device_creation)
{
    if (m_pCmCopy)
        return MFX_ERR_NONE;

    // Return immediately if user requested to turn OFF GPU copy.
    // Device creation may be forced, since other components may need the device for non-copy kernels
    if (m_ForcedGpuCopyState == MFX_GPUCOPY_OFF && !force_cm_device_creation)
    {
        return MFX_ERR_NONE;
    }

    MFX_CHECK(IsCmSupported(), MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    // Copy wrapper creation can be forced, since CM device depends on copy wrapper. Other component may require CM device.
    // VPP is calling QueryCoreInterface<CmDevice>, if copy wrapper isn't created, then CM device cannot be obtained.
    // If copy force state is default and CM copy disabled by default, set copy force state as off.
    if (m_ForcedGpuCopyState == MFX_GPUCOPY_DEFAULT && !IsCmCopyEnabledByDefault())
    {
        m_ForcedGpuCopyState = MFX_GPUCOPY_OFF;
    }

#ifdef ONEVPL_EXPERIMENTAL
    bool use_cm_buffer_cache = m_ForcedGpuCopyState == MFX_GPUCOPY_FAST;
#else
    bool use_cm_buffer_cache = false;
#endif

    std::unique_ptr<CmCopyWrapper> tmp_cm(new CmCopyWrapper(use_cm_buffer_cache));

    MFX_CHECK_NULL_PTR1(tmp_cm->GetCmDevice(*m_p_display_wrapper));

    MFX_SAFE_CALL(tmp_cm->Initialize(GetHWDeviceId(), GetHWType()));

    m_pCmCopy = std::move(tmp_cm);

    return MFX_ERR_NONE;
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

        // use common core for sw surface allocation
        if (request->Type & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            sts = Base::AllocFrames(request, response);
            MFX_RETURN(sts);
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
                MFX_CHECK(m_p_display_wrapper, MFX_ERR_UNSUPPORTED)

                if (response->NumFrameActual < request->NumFrameMin)
                {
                    (*this->m_FrameAllocator.frameAllocator.Free)(this->m_FrameAllocator.frameAllocator.pthis, response);
                    MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
                }

                m_bUseExtAllocForHWFrames = true;
                sts = ProcessRenderTargets(request, response, &this->m_FrameAllocator);
                MFX_CHECK_STS(sts);

                return MFX_ERR_NONE;
            }
            else
            {
                // Default Allocator is used for internal memory allocation and all coded buffers allocation
                m_bUseExtAllocForHWFrames = false;
                sts = this->DefaultAllocFrames(request, response);
                MFX_CHECK_STS(sts);

                return MFX_ERR_NONE;
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
            m_pcHWAlloc.reset(new mfxDefaultAllocatorVAAPI::mfxWideHWFrameAllocator(request->Type, *m_p_display_wrapper));
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
    if (mfx::GetExtBuffer(param->ExtParam, param->NumExtParam, MFX_EXTBUFF_DEC_ADAPTIVE_PLAYBACK))
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
    m_pcHWAlloc.release(); // pointer is managed by m_AllocatorQueue

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
        *pVADisplay = *m_p_display_wrapper;
    }

    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoCORE_T<Base>::GetVAService(...)

template <class Base>
void VAAPIVideoCORE_T<Base>::SetCmCopyMode(mfxU16 cm_copy_mode)
{
    UMC::AutomaticUMCMutex guard(this->m_guard);

    m_ForcedGpuCopyState = cm_copy_mode;
} // void VAAPIVideoCORE_T<Base>::SetCmCopyMode(...)

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
    params.m_Display          = *m_p_display_wrapper;
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
     * */
    if ( (mfx::GetExtBuffer(param->ExtParam, param->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING)) &&
         (MFX_PICSTRUCT_PROGRESSIVE == param->mfx.FrameInfo.PicStruct) &&
         (param->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
    {
        params.m_needVideoProcessingVA = true;
    }
#endif
#if defined(MFX_ENABLE_PXP)
    if ( this->m_pPXPCtxHdl )
    {
        m_pVA.reset(new PXPLinuxVideoAccelerator());
        params.m_pPXPCtxHdl = this->m_pPXPCtxHdl;
    }
    else
#endif // MFX_ENABLE_PXP
        m_pVA.reset(new LinuxVideoAccelerator());

    m_pVA->m_Platform   = UMC::VA_LINUX;
    m_pVA->m_Profile    = (VideoAccelerationProfile)profile;
    m_pVA->m_HWPlatform = m_HWType;
    m_pVA->m_HWDeviceID = this->m_deviceId;

    Status st = m_pVA->Init(&params);
    MFX_CHECK(st == UMC_OK, MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoCORE_T<Base>::CreateVideoAccelerator(...)

template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::DoFastCopyWrapper(
    mfxFrameSurface1* pDst,
    mfxU16 dstMemType,
    mfxFrameSurface1* pSrc,
    mfxU16 srcMemType,
    mfxU32 gpuCopyMode)
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

    mfxStatus fcSts = DoFastCopyExtended(&dstTempSurface, &srcTempSurface, gpuCopyMode);

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
    mfxFrameSurface1* pSrc,
    mfxU32 gpuCopyMode)
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

    // Check if requested copy backend is CM and CM is capable to perform copy
    bool canUseCMCopy = (gpuCopyMode & MFX_COPY_USE_CM) && m_pCmCopy && (m_ForcedGpuCopyState != MFX_GPUCOPY_OFF) && CmCopyWrapper::CanUseCmCopy(pDst, pSrc);

    if (NULL != pSrc->Data.MemId && NULL != pDst->Data.MemId)
    {
        if (canUseCMCopy)
        {
            // If CM copy failed, fallback to VA copy
            MFX_RETURN_IF_ERR_NONE(m_pCmCopy->CopyVideoToVideo(pDst, pSrc));
            // Remove CM adapter in case of failed copy
            this->SetCmCopyMode(MFX_GPUCOPY_OFF);
        }

        VASurfaceID *va_surf_src = (VASurfaceID*)(((mfxHDLPair *)pSrc->Data.MemId)->first);
        VASurfaceID *va_surf_dst = (VASurfaceID*)(((mfxHDLPair *)pDst->Data.MemId)->first);
        MFX_CHECK(va_surf_src != va_surf_dst, MFX_ERR_UNDEFINED_BEHAVIOR);

        VAImage va_img_src = {};
        VAStatus va_sts;
        {
            PERF_UTILITY_AUTO("vaDeriveImage", PERF_LEVEL_DDI);
            va_sts = vaDeriveImage(*m_p_display_wrapper, *va_surf_src, &va_img_src);
        }
        MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaPutImage");
            PERF_UTILITY_AUTO("vaPutImage", PERF_LEVEL_DDI);
            va_sts = vaPutImage(*m_p_display_wrapper, *va_surf_dst, va_img_src.image_id,
                                0, 0, roi.width, roi.height,
                                0, 0, roi.width, roi.height);
        }
        MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

        {
            PERF_UTILITY_AUTO("vaDestroyImage", PERF_LEVEL_DDI);
            va_sts = vaDestroyImage(*m_p_display_wrapper, va_img_src.image_id);
        }
        MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);
    }
    else if (nullptr != pSrc->Data.MemId && nullptr != dstPtr)
    {
        // copy data
        {
            if (canUseCMCopy)
            {
                // If CM copy failed, fallback to VA copy
                MFX_RETURN_IF_ERR_NONE(m_pCmCopy->CopyVideoToSys(pDst, pSrc));
                // Remove CM adapter in case of failed copy
                this->SetCmCopyMode(MFX_GPUCOPY_OFF);
            }

            VASurfaceID *va_surface = (VASurfaceID*)(((mfxHDLPair *)pSrc->Data.MemId)->first);
            VAImage va_image;
            VAStatus va_sts;
            void *pBits = NULL;

            va_sts = vaDeriveImage(*m_p_display_wrapper, *va_surface, &va_image);
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
                PERF_UTILITY_AUTO("vaMapBuffer", PERF_LEVEL_DDI);
                va_sts = vaMapBuffer(*m_p_display_wrapper, va_image.buf, (void **) &pBits);
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
                PERF_UTILITY_AUTO("vaUnmapBuffer", PERF_LEVEL_DDI);
                va_sts = vaUnmapBuffer(*m_p_display_wrapper, va_image.buf);
            }
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);
            {
                PERF_UTILITY_AUTO("vaDestroyImage", PERF_LEVEL_DDI);
                va_sts = vaDestroyImage(*m_p_display_wrapper, va_image.image_id);
            }
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);
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
            // If CM copy failed, fallback to VA copy
            MFX_RETURN_IF_ERR_NONE(m_pCmCopy->CopySysToVideo(pDst, pSrc));
            // Remove CM adapter in case of failed copy
            this->SetCmCopyMode(MFX_GPUCOPY_OFF);
        }

        VAStatus va_sts = VA_STATUS_SUCCESS;
        VASurfaceID *va_surface = (VASurfaceID*)((mfxHDLPair *)pDst->Data.MemId)->first;
        VAImage va_image;
        void *pBits = NULL;

        va_sts = vaDeriveImage(*m_p_display_wrapper, *va_surface, &va_image);
        MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
            PERF_UTILITY_AUTO("vaMapBuffer", PERF_LEVEL_DDI);
            va_sts = vaMapBuffer(*m_p_display_wrapper, va_image.buf, (void **) &pBits);
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
            PERF_UTILITY_AUTO("vaUnmapBuffer", PERF_LEVEL_DDI);
            va_sts = vaUnmapBuffer(*m_p_display_wrapper, va_image.buf);
        }
        MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);

        // vaDestroyImage
        {
            PERF_UTILITY_AUTO("vaDestroyImage", PERF_LEVEL_DDI);
            va_sts = vaDestroyImage(*m_p_display_wrapper, va_image.image_id);
        }
        MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);
    }
    else
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoCORE_T<Base>::DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc, bool gpuCopyMode)

// On linux/android specific function!
// correct work since libva 1.2 (libva 2.2.1.pre1)
// function checks profile and entrypoint and video resolution support
template <class Base>
mfxStatus VAAPIVideoCORE_T<Base>::IsGuidSupported(const GUID guid,
                                         mfxVideoParam *par, bool /* isEncoder */)
{
    MFX_CHECK(par, MFX_ERR_UNSUPPORTED);
    MFX_CHECK(!IsMVCProfile(par->mfx.CodecProfile), MFX_ERR_UNSUPPORTED);

    MFX_SAFE_CALL(this->CheckOrInitDisplay());

#if VA_CHECK_VERSION(1, 2, 0)
    VaGuidMapper mapper(guid);
    VAProfile req_profile         = mapper.m_profile;
    VAEntrypoint req_entrypoint   = mapper.m_entrypoint;
    mfxI32 va_max_num_entrypoints = vaMaxNumEntrypoints(*m_p_display_wrapper);
    mfxI32 va_max_num_profiles    = vaMaxNumProfiles(*m_p_display_wrapper);
    MFX_CHECK_COND(va_max_num_entrypoints && va_max_num_profiles);

    //driver always support VAProfileNone
    if (req_profile != VAProfileNone)
    {
        vector <VAProfile> va_profiles (va_max_num_profiles, VAProfileNone);

        //ask driver about profile support
        VAStatus va_sts = vaQueryConfigProfiles(*m_p_display_wrapper,
                            va_profiles.data(), &va_max_num_profiles);
        MFX_CHECK(va_sts == VA_STATUS_SUCCESS, MFX_ERR_UNSUPPORTED);

        //check profile support
        auto it_profile = find(va_profiles.begin(), va_profiles.end(), req_profile);
        MFX_CHECK(it_profile != va_profiles.end(), MFX_ERR_UNSUPPORTED);
    }

    vector <VAEntrypoint> va_entrypoints (va_max_num_entrypoints, static_cast<VAEntrypoint> (0));

    //ask driver about entrypoint support
    VAStatus va_sts = vaQueryConfigEntrypoints(*m_p_display_wrapper, req_profile,
                    va_entrypoints.data(), &va_max_num_entrypoints);
    MFX_CHECK(va_sts == VA_STATUS_SUCCESS, MFX_ERR_UNSUPPORTED);

    //check entrypoint support
    auto it_entrypoint = find(va_entrypoints.begin(), va_entrypoints.end(), req_entrypoint);
    MFX_CHECK(it_entrypoint != va_entrypoints.end(), MFX_ERR_UNSUPPORTED);

    VAConfigAttrib attr[] = {{VAConfigAttribMaxPictureWidth,  0},
                             {VAConfigAttribMaxPictureHeight, 0}};

    //ask driver about support
    va_sts = vaGetConfigAttributes(*m_p_display_wrapper, req_profile,
                                   req_entrypoint,
                                   attr, sizeof(attr)/sizeof(*attr));

    MFX_CHECK(va_sts == VA_STATUS_SUCCESS, MFX_ERR_UNSUPPORTED);

    //check video resolution
    MFX_CHECK(attr[0].value != VA_ATTRIB_NOT_SUPPORTED, MFX_ERR_UNSUPPORTED);
    MFX_CHECK(attr[1].value != VA_ATTRIB_NOT_SUPPORTED, MFX_ERR_UNSUPPORTED);
    MFX_CHECK_COND(attr[0].value && attr[1].value);
    MFX_CHECK(attr[0].value >= par->mfx.FrameInfo.Width, MFX_ERR_UNSUPPORTED);
    MFX_CHECK(attr[1].value >= par->mfx.FrameInfo.Height, MFX_ERR_UNSUPPORTED);

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    //check VD-SFC support
    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    if (videoProcessing)
    {
        VAConfigAttrib attrDecProcess = {VAConfigAttribDecProcessing, 0};

        //ask driver about support
        va_sts = vaGetConfigAttributes(*m_p_display_wrapper, req_profile, req_entrypoint, &attrDecProcess, 1);

        MFX_CHECK(va_sts == VA_STATUS_SUCCESS, MFX_ERR_UNSUPPORTED);
        MFX_CHECK(attrDecProcess.value == VA_DEC_PROCESSING , MFX_ERR_UNSUPPORTED);
    }
#endif

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
        MFX_CHECK(par->mfx.FrameInfo.Width <= 8192 && par->mfx.FrameInfo.Height <= 8192, MFX_ERR_UNSUPPORTED);
        break;
    case MFX_CODEC_MPEG2: //MPEG2 decoder doesn't support resolution bigger than 2K
        MFX_CHECK(par->mfx.FrameInfo.Width <= 2048 && par->mfx.FrameInfo.Height <= 2048, MFX_ERR_UNSUPPORTED);
        break;
    case MFX_CODEC_JPEG:
        MFX_CHECK(par->mfx.FrameInfo.Width <= 8192 && par->mfx.FrameInfo.Height <= 8192, MFX_WRN_PARTIAL_ACCELERATION);
        break;
    case MFX_CODEC_VP8:
        break;
    case MFX_CODEC_VVC:
        break; 
    default:
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    MFX_CHECK(MFX_CODEC_JPEG == par->mfx.CodecId || MFX_CODEC_HEVC == par->mfx.CodecId ||
        (par->mfx.FrameInfo.Width <= 4096 && par->mfx.FrameInfo.Height <= 4096),
        MFX_ERR_UNSUPPORTED
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
        if (!m_pCmCopy)
        {
            UMC::AutomaticUMCMutex guard(this->m_guard);
            MFX_CHECK_STS_RET_NULL(TryInitializeCm(true));
        }

        return m_pCmCopy ? (void*)m_pCmCopy->GetCmDevice(*m_p_display_wrapper) : nullptr;
    }

    if (MFXICORECMCOPYWRAPPER_GUID == guid)
    {
        if (!m_pCmCopy)
        {
            UMC::AutomaticUMCMutex guard(this->m_guard);
            MFX_CHECK_STS_RET_NULL(TryInitializeCm(false));
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

VAAPIVideoCORE_VPL::VAAPIVideoCORE_VPL(
    const mfxU32 adapterNum,
    const AffinityMaskType & affinityMask,
    const mfxU32 numThreadsAvailable,
    const mfxSession session)
    : VAAPIVideoCORE_VPL_base(adapterNum, affinityMask, numThreadsAvailable, session)
{
    m_frame_allocator_wrapper.allocator_hw.reset(new FlexibleFrameAllocatorHW_VAAPI(nullptr, m_session));
}

mfxStatus
VAAPIVideoCORE_VPL::SetHandle(
    mfxHandleType type,
    mfxHDL hdl)
{
    MFX_SAFE_CALL(VAAPIVideoCORE_VPL_base::SetHandle(type, hdl));

    if (type == MFX_HANDLE_VA_DISPLAY)
    {
        // Pass display to allocator
        m_frame_allocator_wrapper.SetDevice(m_p_display_wrapper.get());
    }

    return MFX_ERR_NONE;
}

VAAPIVideoCORE_VPL::~VAAPIVideoCORE_VPL()
{
    // Need to clean up allocated surfaces before display is closed in destructor of base class (VAAPIVideoCORE_T)
    m_frame_allocator_wrapper.allocator_hw.reset();
}

void* VAAPIVideoCORE_VPL::QueryCoreInterface(const MFX_GUID& guid)
{
    if (MFXIVAAPIVideoCORE_VPL_GUID == guid)
    {
        return (void*)this;
    }

    return VAAPIVideoCORE_VPL_base::QueryCoreInterface(guid);
}

mfxStatus VAAPIVideoCORE_VPL::AllocFrames(
    mfxFrameAllocRequest* request,
    mfxFrameAllocResponse* response,
    bool isNeedCopy)
{
    MFX_CHECK_NULL_PTR2(request, response);

    MFX_CHECK(!(request->Type & 0x0004), MFX_ERR_UNSUPPORTED); // 0x0004 means MFX_MEMTYPE_OPAQUE_FRAME

    UMC::AutomaticUMCMutex guard(this->m_guard);

    try
    {
        MFX_SAFE_CALL(CheckOrInitDisplay());
        m_frame_allocator_wrapper.SetDevice(m_p_display_wrapper.get());

        mfxStatus sts = m_frame_allocator_wrapper.Alloc(*request, *response, request->Type & (MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_FROM_PAK));

#if defined(ANDROID)
        MFX_CHECK(response->NumFrameActual <= 128, MFX_ERR_UNSUPPORTED);
#endif
        MFX_RETURN(sts);
    }
    catch (...)
    {
        MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
    }
} // mfxStatus VAAPIVideoCORE_VPL::AllocFrames(...)


mfxStatus VAAPIVideoCORE_VPL::ReallocFrame(mfxFrameSurface1 *surf)
{
    MFX_CHECK_NULL_PTR1(surf);

    return m_frame_allocator_wrapper.ReallocSurface(surf->Info, surf->Data.MemId);
}

mfxStatus
VAAPIVideoCORE_VPL::DoFastCopyWrapper(
    mfxFrameSurface1* pDst,
    mfxU16 dstMemType,
    mfxFrameSurface1* pSrc,
    mfxU16 srcMemType,
    mfxU32 gpuCopyMode)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VAAPIVideoCORE_VPL::DoFastCopyWrapper");

    MFX_CHECK_NULL_PTR2(pSrc, pDst);

    // uncomment underlying checks after additional validation
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

    sts = DoFastCopyExtended(&dstTempSurface, &srcTempSurface, gpuCopyMode);
    MFX_CHECK_STS(sts);

    sts = src_surf_lock.unlock();
    MFX_CHECK_STS(sts);

    return dst_surf_lock.unlock();
}

mfxStatus
VAAPIVideoCORE_VPL::DoFastCopyExtended(
    mfxFrameSurface1* pDst,
    mfxFrameSurface1* pSrc,
    mfxU32 gpuCopyMode)
{
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

    // Check if requested copy backend is CM and CM is capable to perform copy
    bool canUseCMCopy = (gpuCopyMode & MFX_COPY_USE_CM) && m_pCmCopy && (m_ForcedGpuCopyState != MFX_GPUCOPY_OFF) && CmCopyWrapper::CanUseCmCopy(pDst, pSrc);

    if (m_pVaCopy && (VACopyWrapper::IsVaCopySupportSurface(*pDst, *pSrc, m_HWType)) && (gpuCopyMode & MFX_COPY_USE_VACOPY_ANY) && (m_ForcedGpuCopyState != MFX_GPUCOPY_OFF))
    {
        auto vacopyMode = VACopyWrapper::DEFAULT;

        if (m_HWType == MFX_HW_DG2)
        {
            vacopyMode = VACopyWrapper::BLT;
        }

        auto vaCopySts = m_pVaCopy->Copy(*pSrc, *pDst, vacopyMode);
        MFX_RETURN_IF_ERR_NONE(vaCopySts);

        if (vaCopySts == MFX_ERR_DEVICE_FAILED)
        {
            UMC::AutomaticUMCMutex guard(this->m_guard);
            m_pVaCopy.reset(); //once failed, don't try to use it anymore
        }
    }

    if (NULL != pSrc->Data.MemId && NULL != pDst->Data.MemId)
    {
        // At First try to copy with CM
        if (canUseCMCopy)
        {
            // If CM copy failed, fallback to VA copy
            MFX_RETURN_IF_ERR_NONE(m_pCmCopy->CopyVideoToVideo(pDst, pSrc));
            // Remove CM adapter in case of failed copy
            this->SetCmCopyMode(MFX_GPUCOPY_OFF);
        }
        // Fallback to VA copy in case of failed CM copy

        VASurfaceID *va_surf_src = (VASurfaceID*)(((mfxHDLPair *)pSrc->Data.MemId)->first);
        VASurfaceID *va_surf_dst = (VASurfaceID*)(((mfxHDLPair *)pDst->Data.MemId)->first);
        MFX_CHECK(va_surf_src != va_surf_dst, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK_HDL(va_surf_src);
        MFX_CHECK_HDL(va_surf_dst);

        SurfaceScopedLock src_lock(*m_p_display_wrapper, *va_surf_src);
        sts = src_lock.DeriveImage();
        MFX_CHECK_STS(sts);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaPutImage");
            PERF_UTILITY_AUTO("vaPutImage", PERF_LEVEL_DDI);
            VAStatus va_sts = vaPutImage(*m_p_display_wrapper, *va_surf_dst, src_lock.m_image.image_id,
                0, 0, roi.width, roi.height,
                0, 0, roi.width, roi.height);
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);
        }

        return src_lock.DestroyImage();
    }

    if (NULL != pSrc->Data.MemId && NULL != dstPtr)
    {
        // At First try to copy with CM
        if (canUseCMCopy)
        {
            // If CM copy failed, fallback to VA copy
            MFX_RETURN_IF_ERR_NONE(m_pCmCopy->CopyVideoToSys(pDst, pSrc));
            // Remove CM adapter in case of failed copy
            this->SetCmCopyMode(MFX_GPUCOPY_OFF);
        }
        // Fallback to SW copy in case of failed CM copy

        VASurfaceID *va_surface = (VASurfaceID*)(((mfxHDLPair *)pSrc->Data.MemId)->first);
        MFX_CHECK_HDL(va_surface);

        SurfaceScopedLock src_lock(*m_p_display_wrapper, *va_surface);
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
        // At First try to copy with CM
        if (canUseCMCopy)
        {
            // If CM copy failed, fallback to VA copy
            MFX_RETURN_IF_ERR_NONE(m_pCmCopy->CopySysToVideo(pDst, pSrc));
            // Remove CM adapter in case of failed copy
            this->SetCmCopyMode(MFX_GPUCOPY_OFF);
        }
        // Fallback to SW copy in case of failed CM copy

        VASurfaceID *va_surface = (VASurfaceID*)(((mfxHDLPair *)pDst->Data.MemId)->first);
        MFX_CHECK_HDL(va_surface);

        SurfaceScopedLock dst_lock(*m_p_display_wrapper, *va_surface);
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
} // mfxStatus VAAPIVideoCORE_VPL::DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)

mfxStatus VAAPIVideoCORE_VPL::CreateSurface(mfxU16 type, const mfxFrameInfo& info, mfxFrameSurface1*& surf, mfxSurfaceHeader* import_surface)
{
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        MFX_SAFE_CALL(CheckOrInitDisplay());
        m_frame_allocator_wrapper.SetDevice(m_p_display_wrapper.get());
    }

    return m_frame_allocator_wrapper.CreateSurface(type, info, surf, import_surface);
}

template class VAAPIVideoCORE_T<CommonCORE_VPL>;

/* EOF */
