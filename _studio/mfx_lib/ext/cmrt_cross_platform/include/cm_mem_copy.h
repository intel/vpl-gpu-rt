// Copyright (c) 2011-2022 Intel Corporation
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

#ifndef __CM_MEM_COPY_H__
#define __CM_MEM_COPY_H__

#include "mfxdefs.h"
#include "mfxstructures.h"
#include "ippi.h"
#ifdef MFX_ENABLE_KERNELS
#include "genx_copy_kernel_gen12lp_isa.h"
#endif

#ifdef _MSVC_LANG
#pragma warning(disable: 4505)
#pragma warning(disable: 4100)
#pragma warning(disable: 4201)
#endif


#include <mutex>

#include <algorithm>
#include <set>
#include <map>
#include <vector>

#include "cmrt_cross_platform.h"

typedef mfxI32 cmStatus;

#define BLOCK_PIXEL_WIDTH   (32)
#define BLOCK_HEIGHT        (8)

#define CM_MAX_GPUCOPY_SURFACE_WIDTH_IN_BYTE 65408
#define CM_MAX_GPUCOPY_SURFACE_HEIGHT        4088

#define CM_SUPPORTED_COPY_SIZE(ROI) (ROI.width <= CM_MAX_GPUCOPY_SURFACE_WIDTH_IN_BYTE && ROI.height <= CM_MAX_GPUCOPY_SURFACE_HEIGHT )
#define CM_ALIGNED(PTR) (!((mfxU64(PTR))&0xf))
#define CM_ALIGNED64(PTR) (!((mfxU64(PTR))&0x3f))

static inline bool operator < (const mfxHDLPair & l, const mfxHDLPair & r)
{
    return (l.first == r.first) ? (l.second < r.second) : (l.first < r.first);
};

class CmDevice;
class CmBuffer;
class CmBufferUP;
class CmSurface2D;
class CmEvent;
class CmQueue;
class CmProgram;
class CmKernel;
class SurfaceIndex;
class CmThreadSpace;
class CmTask;
struct IDirect3DSurface9;
struct IDirect3DDeviceManager9;

template <typename T>
class CmSurfBufferBase
{
public:
    CmSurfBufferBase(T* surfbuf, CmDevice* device)
        : surfacebuffer(surfbuf)
        , cm_device(device)
        , use_count(0)
    {}

    CmSurfBufferBase(const CmSurfBufferBase&) = delete;
    CmSurfBufferBase(CmSurfBufferBase&& other) noexcept
    {
        *this = std::move(other);
    }

    CmSurfBufferBase& operator= (const CmSurfBufferBase&) = delete;
    CmSurfBufferBase& operator= (CmSurfBufferBase&& other) noexcept
    {
        if (this != &other)
        {
            surfacebuffer = other.surfacebuffer;
            cm_device     = other.cm_device;
            use_count.exchange(other.use_count, std::memory_order_relaxed);

            other.surfacebuffer = nullptr;
            other.cm_device     = nullptr;
        }
        return *this;
    }

    void AddRef()
    {
        use_count.fetch_add(1u, std::memory_order_relaxed);
    }

    void Release()
    {
        use_count.fetch_sub(1u, std::memory_order_relaxed);
    }

    bool IsFree() const
    {
        return use_count.load(std::memory_order_relaxed) == 0;
    }

    operator T* ()
    {
        return surfacebuffer;
    }

protected:
    T* surfacebuffer = nullptr;
    CmDevice* cm_device = nullptr;
    std::atomic<mfxU32> use_count;
};

class CmSurface2DWrapper : public CmSurfBufferBase<CmSurface2D>
{
public:
    CmSurface2DWrapper(CmSurface2D* surf, CmDevice* device)
        : CmSurfBufferBase(surf, device)
    {}
    CmSurface2DWrapper(const CmSurface2DWrapper&) = delete;
    CmSurface2DWrapper(CmSurface2DWrapper&& other) noexcept
        : CmSurfBufferBase(nullptr, nullptr)
    {
        *this = std::move(other);
    }

    CmSurface2DWrapper& operator= (const CmSurface2DWrapper&) = delete;
    CmSurface2DWrapper& operator= (CmSurface2DWrapper&& other) noexcept
    {
        if (this != &other)
        {
            DestroySurface();

            static_cast<CmSurfBufferBase&>(*this) = std::move(static_cast<CmSurfBufferBase&>(other));
        }
        return *this;
    }

    ~CmSurface2DWrapper()
    {
        DestroySurface();
    }

private:
    void DestroySurface()
    {
        if (cm_device && surfacebuffer)
        {
            assert(use_count == 0);
            std::ignore = MFX_STS_TRACE(cm_device->DestroySurface(surfacebuffer));
        }
    }
};

class CmBufferUPWrapper : public CmSurfBufferBase<CmBufferUP>
{
public:
    CmBufferUPWrapper(CmBufferUP* buf, SurfaceIndex* pindx, CmDevice* device)
        : CmSurfBufferBase(buf, device)
        , pIndex(pindx)
    {}
    CmBufferUPWrapper(const CmBufferUPWrapper&) = delete;
    CmBufferUPWrapper(CmBufferUPWrapper&& other) noexcept
        : CmSurfBufferBase(nullptr, nullptr)
    {
        *this = std::move(other);
    }

    CmBufferUPWrapper& operator= (const CmBufferUPWrapper&) = delete;
    CmBufferUPWrapper& operator= (CmBufferUPWrapper&& other) noexcept
    {
        if (this != &other)
        {
            DestroyBuffer();
            pIndex = other.pIndex;

            static_cast<CmSurfBufferBase&>(*this) = std::move(static_cast<CmSurfBufferBase&>(other));

            other.pIndex = nullptr;
        }
        return *this;
    }

    ~CmBufferUPWrapper()
    {
        DestroyBuffer();
    }

    SurfaceIndex* GetIndex()
    {
        return pIndex;
    }

private:
    void DestroyBuffer()
    {
        if (cm_device && surfacebuffer)
        {
            std::ignore = MFX_STS_TRACE(cm_device->DestroyBufferUP(surfacebuffer));
        }
    }

    SurfaceIndex* pIndex = nullptr;
};

class CmCopyWrapper
{
public:

    CmCopyWrapper(bool cm_buffer_cache = false)
        : m_use_cm_buffers_cache(cm_buffer_cache)
    {}

    // destructor
    virtual ~CmCopyWrapper(void);

    template <typename D3DAbstract>
    CmDevice* GetCmDevice(D3DAbstract *pD3D)
    {
        cmStatus cmSts = CM_SUCCESS;
        mfxU32 version = 0;

        if (m_pCmDevice)
            return m_pCmDevice;

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CreateCmDevice");
        cmSts = ::CreateCmDevice(m_pCmDevice, version, pD3D, CM_DEVICE_CREATE_OPTION_SCRATCH_SPACE_DISABLE);
        if (cmSts != CM_SUCCESS)
            return NULL;

        if (CM_1_0 > version)
        {
            return NULL;
        }

        return m_pCmDevice;
    };

    CmDevice* GetCmDevice(VADisplay dpy)
    {
        cmStatus cmSts = CM_SUCCESS;
        mfxU32 version;

        if (m_pCmDevice)
            return m_pCmDevice;

        cmSts = ::CreateCmDevice(m_pCmDevice, version, dpy, CM_DEVICE_CREATE_OPTION_SCRATCH_SPACE_DISABLE);
        if (cmSts != CM_SUCCESS)
            return NULL;

        if (CM_1_0 > version)
        {
            return NULL;
        }
        return m_pCmDevice;
    };

    // initialize available functionality
    mfxStatus Initialize(mfxU16 hwDeviceId, eMFXHWType hwtype = MFX_HW_UNKNOWN);
    mfxStatus InitializeSwapKernels(eMFXHWType hwtype = MFX_HW_UNKNOWN);

    // release all resources
    void Close();

    // check input parameters
    mfxStatus IsCmCopySupported(mfxFrameSurface1 *pSurface, IppiSize roi);

    static bool CanUseCmCopy(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);
    static bool CheckSurfaceContinuouslyAllocated(const mfxFrameSurface1 &surf);
    static bool isSinglePlainFormat(mfxU32 format);
    static bool isNeedSwapping(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);
    static bool isNeedShift(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);
    static bool isNV12LikeFormat(mfxU32 format);
    static int  getSizePerPixel(mfxU32 format);
    mfxStatus CopyVideoToVideo(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);
    mfxStatus CopySysToVideo(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);
    mfxStatus CopyVideoToSys(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);

    mfxStatus CopyVideoToSystemMemoryAPI(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, mfxHDLPair src, mfxU32 srcPitch, IppiSize roi);
    mfxStatus CopyVideoToSystemMemory(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, mfxHDLPair src, mfxU32 srcPitch, IppiSize roi, mfxU32 format);

    mfxStatus CopySystemToVideoMemoryAPI(mfxHDLPair dst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi);
    mfxStatus CopySystemToVideoMemory(mfxHDLPair dst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi, mfxU32 format);
    mfxStatus CopyVideoToVideoMemoryAPI(mfxHDLPair dst, mfxHDLPair src, IppiSize roi);

    mfxStatus CopySwapVideoToSystemMemory(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, mfxHDLPair src, mfxU32 srcPitch, IppiSize roi, mfxU32 format);
    mfxStatus CopySwapSystemToVideoMemory(mfxHDLPair dst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi, mfxU32 format);
    mfxStatus CopyShiftSystemToVideoMemory(mfxHDLPair dst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi, mfxU32 bitshift, mfxU32 format);
    mfxStatus CopyShiftVideoToSystemMemory(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, mfxHDLPair src, mfxU32 srcPitch, IppiSize roi, mfxU32 bitshift, mfxU32 format);
    mfxStatus CopyMirrorVideoToSystemMemory(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, mfxHDLPair src, mfxU32 srcPitch, IppiSize roi, mfxU32 format);
    mfxStatus CopyMirrorSystemToVideoMemory(mfxHDLPair dst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi, mfxU32 format);
    mfxStatus CopySwapVideoToVideoMemory(mfxHDLPair dst, mfxHDLPair src, IppiSize roi, mfxU32 format);
    mfxStatus CopyMirrorVideoToVideoMemory(mfxHDLPair dst, mfxHDLPair src, IppiSize roi, mfxU32 format);

    void CleanUpCache();
    mfxStatus EnqueueCopyNV12GPUtoCPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent );
    mfxStatus EnqueueCopySwapRBGPUtoCPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent );
    mfxStatus EnqueueCopyGPUtoCPU(   CmSurface2D* pSurface,
                                unsigned char* pSysMem,
                                int width,
                                int height,
                                const UINT widthStride,
                                const UINT heightStride,
                                mfxU32 format,
                                const UINT option,
                                CmEvent* & pEvent );

    mfxStatus EnqueueCopySwapRBCPUtoGPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent );
    mfxStatus EnqueueCopyCPUtoGPU(   CmSurface2D* pSurface,
                                unsigned char* pSysMem,
                                int width,
                                int height,
                                const UINT widthStride,
                                const UINT heightStride,
                                mfxU32 format,
                                const UINT option,
                                CmEvent* & pEvent );
    mfxStatus EnqueueCopySwapRBGPUtoGPU(   CmSurface2D* pSurfaceIn,
                                    CmSurface2D* pSurfaceOut,
                                    int width,
                                    int height,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent );
    mfxStatus EnqueueCopyMirrorGPUtoGPU(   CmSurface2D* pSurfaceIn,
                                    CmSurface2D* pSurfaceOut,
                                    int width,
                                    int height,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent );
    mfxStatus EnqueueCopyMirrorNV12GPUtoCPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent );
    mfxStatus EnqueueCopyMirrorNV12CPUtoGPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent );
    mfxStatus EnqueueCopyShiftP010GPUtoCPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    int bitshift,
                                    CmEvent* & pEvent );
    mfxStatus EnqueueCopyShiftGPUtoCPU(CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    int bitshift,
                                    CmEvent* & pEvent);
    mfxStatus EnqueueCopyShiftP010CPUtoGPU( CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    int bitshift,
                                    CmEvent* & pEvent );
    mfxStatus EnqueueCopyShiftCPUtoGPU(CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    int bitshift,
                                    const UINT option,
                                    CmEvent* & pEvent);
    mfxStatus EnqueueCopyNV12CPUtoGPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent );
protected:

    eMFXHWType     m_HWType        = MFX_HW_UNKNOWN;
    CmDevice      *m_pCmDevice     = nullptr;
    CmProgram     *m_pCmProgram    = nullptr;
    INT            m_timeout       = 0;

    CmThreadSpace *m_pThreadSpace  = nullptr;

    CmQueue       *m_pCmQueue      = nullptr;
    CmTask        *m_pCmTask1      = nullptr;
    CmTask        *m_pCmTask2      = nullptr;

    CmSurface2D   *m_pCmSurface2D  = nullptr;
    CmBufferUP    *m_pCmUserBuffer = nullptr;

    SurfaceIndex  *m_pCmSrcIndex   = nullptr;
    SurfaceIndex  *m_pCmDstIndex   = nullptr;

    // Cache for CM surfaces (associated with frames in video memory)
    std::map<std::tuple<mfxHDLPair, mfxU32, mfxU32>, CmSurface2DWrapper> m_tableCmRelations;
    // Cache for CM Buffers (associated with frames in system memory)
    bool                                                                 m_use_cm_buffers_cache = false;
    std::map<std::tuple<mfxU8 *,    mfxU32, mfxU32>, CmBufferUPWrapper>  m_tableSysRelations;

    std::mutex m_mutex;

    CmSurface2DWrapper* CreateCmSurface2D(mfxHDLPair surfaceIdPair, mfxU32 width, mfxU32 height);

    CmBufferUPWrapper* CreateUpBuffer(mfxU8 *pDst, mfxU32 memSize, mfxU32 width, mfxU32 height);

private:
    bool m_bSwapKernelsInitialized = false;
};

#endif // __CM_MEM_COPY_H__
