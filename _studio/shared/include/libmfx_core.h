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

#ifndef __LIBMFX_CORE_H__
#define __LIBMFX_CORE_H__

#include <map>

#include "umc_mutex.h"
#include "libmfx_allocator.h"
#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "mfx_ext_buffers.h"
#include "fast_copy.h"
#include "libmfx_core_interface.h"

#include <memory>
#include <deque>
#include <chrono>
#include <limits>


class CommonCORE : public VideoCORE
{
public:

    friend class FactoryCORE;

    virtual ~CommonCORE() override { Close(); }

    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *handle)          override;
    virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL handle)           override;

    virtual mfxStatus SetBufferAllocator(mfxBufferAllocator *)               override;
    virtual mfxStatus SetFrameAllocator(mfxFrameAllocator *allocator)        override;

    // Utility functions for memory access
    virtual mfxStatus AllocBuffer(mfxU32 nbytes, mfxU16 type, mfxMemId *mid) override;
    virtual mfxStatus LockBuffer(mfxMemId mid, mfxU8 **ptr)                  override;
    virtual mfxStatus UnlockBuffer(mfxMemId mid)                             override;
    virtual mfxStatus FreeBuffer(mfxMemId mid)                               override;

    // DEPRECATED
    virtual mfxStatus CheckHandle() override { return MFX_ERR_NONE; }

    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle, bool ExtendedSearch = true)               override;

    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request,
                                   mfxFrameAllocResponse *response, bool isNeedCopy = true)               override;

    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr)                                          override;
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr = nullptr)                              override;
    virtual mfxStatus FreeFrames(mfxFrameAllocResponse *response, bool ExtendedSearch = true)             override;

    virtual mfxStatus LockExternalFrame(mfxMemId mid, mfxFrameData *ptr, bool ExtendedSearch = true)      override;
    virtual mfxStatus GetExternalFrameHDL(mfxMemId mid, mfxHDL *handle, bool ExtendedSearch = true)       override;
    virtual mfxStatus UnlockExternalFrame(mfxMemId mid, mfxFrameData *ptr=0, bool ExtendedSearch = true)  override;

    virtual mfxMemId MapIdx(mfxMemId mid)                                                                 override;

    // Increment Surface lock caring about opaq
    virtual mfxStatus IncreaseReference(mfxFrameData *ptr, bool ExtendedSearch = true)                    override;
    // Decrement Surface lock caring about opaq
    virtual mfxStatus DecreaseReference(mfxFrameData *ptr, bool ExtendedSearch = true)                    override;

    // no care about surface, opaq and all round. Just increasing reference
    virtual mfxStatus IncreasePureReference(mfxU16 &Locked)                                               override;
    // no care about surface, opaq and all round. Just decreasing reference
    virtual mfxStatus DecreasePureReference(mfxU16 &Locked)                                               override;

    // Get Video Accelerator.
    virtual void  GetVA(mfxHDL* phdl, mfxU16)                   override { *phdl = nullptr; }
    virtual mfxStatus CreateVA(mfxVideoParam *, mfxFrameAllocRequest *, mfxFrameAllocResponse *, UMC::FrameAllocator *) override { MFX_RETURN(MFX_ERR_UNSUPPORTED); }
    // Get the current working adapter's number
    virtual mfxU32 GetAdapterNumber()                           override { return 0; }
#ifdef _MSVC_LANG
#pragma warning(push)
#pragma warning(disable : 26812)
#endif
    //
    virtual eMFXPlatform GetPlatformType()                      override { return MFX_PLATFORM_SOFTWARE; }
#ifdef _MSVC_LANG
#pragma warning(pop)
#endif

    // Get Video Processing
    virtual void  GetVideoProcessing(mfxHDL* phdl)              override { *phdl = 0; }
    virtual mfxStatus CreateVideoProcessing(mfxVideoParam *)    override { MFX_RETURN(MFX_ERR_UNSUPPORTED); }

    // Get the current number of working threads
    virtual mfxU32 GetNumWorkingThreads()                       override { return m_numThreadsAvailable; }
    virtual void INeedMoreThreadsInside(const void *pComponent) override;

    virtual mfxStatus DoFastCopy(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)         override;
    virtual mfxStatus DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc) override;

    virtual mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType) override;

    // DEPRECATED
    virtual bool IsFastCopyEnabled()              override { return true; }

    virtual bool IsExternalFrameAllocator() const override;
    virtual eMFXHWType GetHWType()                override { return MFX_HW_UNKNOWN; }

    virtual mfxStatus CopyFrame(mfxFrameSurface1 *dst, mfxFrameSurface1 *src) override;

    virtual mfxStatus CopyBuffer(mfxU8 *, mfxU32, mfxFrameSurface1 *)         override { MFX_RETURN(MFX_ERR_UNKNOWN); }

    virtual mfxStatus CopyFrameEx(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType) override
    {
        return DoFastCopyWrapper(pDst, dstMemType, pSrc, srcMemType);
    }

    virtual mfxStatus IsGuidSupported(const GUID, mfxVideoParam *, bool)      override { return MFX_ERR_NONE; }

    virtual eMFXVAType GetVAType() const                   override { return MFX_HW_NO; }

    virtual bool SetCoreId(mfxU32 Id)                      override;
    virtual void* QueryCoreInterface(const MFX_GUID &guid) override;

    virtual mfxSession GetSession()                        override { return m_session; }

    virtual mfxU16 GetAutoAsyncDepth()                     override;

    // keep frame response structure describing plug-in memory surfaces
    void AddPluginAllocResponse(mfxFrameAllocResponse& response);

    // get response which corresponds required conditions: same mids and number
    mfxFrameAllocResponse* GetPluginAllocResponse(mfxFrameAllocResponse& temp_response);

    // non-virtual QueryPlatform, as we should not change vtable
    mfxStatus QueryPlatform(mfxPlatform* platform);

protected:

    CommonCORE(const mfxU32 numThreadsAvailable, const mfxSession session = nullptr);

    class API_1_19_Adapter : public IVideoCore_API_1_19
    {
    public:
        API_1_19_Adapter(CommonCORE * core) : m_core(core) {}
        virtual mfxStatus QueryPlatform(mfxPlatform* platform);

    private:
        CommonCORE *m_core;
    };


    virtual mfxStatus          DefaultAllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    mfxFrameAllocator*         GetAllocatorAndMid(mfxMemId& mid);
    mfxBaseWideFrameAllocator* GetAllocatorByReq(mfxU16 type) const;
    virtual void               Close();
    mfxStatus                  FreeMidArray(mfxFrameAllocator* pAlloc, mfxFrameAllocResponse *response);
    mfxStatus                  RegisterMids(mfxFrameAllocResponse *response, mfxU16 memType, bool IsDefaultAlloc, mfxBaseWideFrameAllocator* pAlloc = 0);
    mfxStatus                  CheckTimingLog();

    bool                       GetUniqID(mfxMemId& mId);
    bool IsEqual (const mfxFrameAllocResponse &resp1, const mfxFrameAllocResponse &resp2) const
    {
        if (resp1.NumFrameActual != resp2.NumFrameActual)
            return false;

        for (mfxU32 i=0; i < resp1.NumFrameActual; i++)
        {
            if (resp1.mids[i] != resp2.mids[i])
                return false;
        }
        return true;
    };

    typedef struct
    {
        mfxMemId InternalMid;
        bool isDefaultMem;
        mfxU16 memType;

    } MemDesc;

    typedef std::map<mfxMemId, MemDesc> CorrespTbl;
    typedef std::map<mfxMemId, mfxBaseWideFrameAllocator*> AllocQueue;
    typedef std::map<mfxMemId*, mfxMemId*> MemIDMap;

    CorrespTbl       m_CTbl;
    AllocQueue       m_AllocatorQueue;
    MemIDMap         m_RespMidQ;

    // Number of available threads
    const
    mfxU32                                     m_numThreadsAvailable;
    // Handler to the owning session
    const
    mfxSession                                 m_session;

    // Common I/F
    mfxWideBufferAllocator                     m_bufferAllocator;
    mfxBaseWideFrameAllocator                  m_FrameAllocator;

    mfxU32                                     m_NumAllocators;
    mfxHDL                                     m_hdl;

    mfxHDL                                     m_DXVA2DecodeHandle;

    mfxHDL                                     m_D3DDecodeHandle;
    mfxHDL                                     m_D3DEncodeHandle;
    mfxHDL                                     m_D3DVPPHandle;

    bool                                       m_bSetExtBufAlloc;
    bool                                       m_bSetExtFrameAlloc;

    std::unique_ptr<mfxMemId[]>                m_pMemId;
    std::unique_ptr<mfxBaseWideFrameAllocator> m_pcAlloc;

    std::unique_ptr<FastCopy>                  m_pFastCopy;
    bool                                       m_bUseExtManager;
    UMC::Mutex                                 m_guard;

    mfxU32                                     m_CoreId;

    EncodeHWCaps                               m_encode_caps;
    EncodeHWCaps                               m_encode_mbprocrate;


    std::vector<mfxFrameAllocResponse>         m_PlugInMids;

    API_1_19_Adapter                           m_API_1_19;

    mfxU16                                     m_deviceId;

    CommonCORE & operator = (const CommonCORE &) = delete;
};

mfxStatus CoreDoSWFastCopy(mfxFrameSurface1 & dst, const mfxFrameSurface1 & src, int copyFlag);

// Refactored MSDK 2.0 core

template<class Base>
class deprecate_from_base : public Base
{
public:
    virtual ~deprecate_from_base() {}

    virtual mfxMemId MapIdx(mfxMemId mid)                      override { return mid; }

    // Clean up code from buffer allocator usage and uncomment following items
    /* Deprecated functionality : buffer allocator */
    /*
    virtual mfxStatus SetBufferAllocator(mfxBufferAllocator *) override
    {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus AllocBuffer(mfxU32, mfxU16, mfxMemId *)  override
    {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus LockBuffer(mfxMemId, mfxU8 **)           override
    {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus UnlockBuffer(mfxMemId)                   override
    {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus FreeBuffer(mfxMemId)                     override
    {
        return MFX_ERR_UNSUPPORTED;
    }
    */

    virtual bool SetCoreId(mfxU32)                                                        override
    {
        return false;
    }

protected:
    virtual mfxStatus DefaultAllocFrames(mfxFrameAllocRequest *, mfxFrameAllocResponse *) override
    {
        return MFX_ERR_UNSUPPORTED;
    }

    deprecate_from_base(const mfxU32 numThreadsAvailable, const mfxSession session = nullptr)
        : Base(numThreadsAvailable, session)
    {}

    deprecate_from_base(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session = nullptr)
        : Base(adapterNum, numThreadsAvailable, session)
    {}
};

class CommonCORE_VPL : public deprecate_from_base<CommonCORE>
{
public:

    friend class FactoryCORE;

    virtual mfxStatus SetFrameAllocator(mfxFrameAllocator *allocator)                             override;

    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle, bool = true)                      override;

    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool isNeedCopy = true) override;

    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr)                                  override;
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr = nullptr)                      override;
    virtual mfxStatus FreeFrames(mfxFrameAllocResponse *response, bool = true)                    override;

    virtual mfxStatus LockExternalFrame(mfxMemId mid, mfxFrameData *ptr, bool = true)             override;
    virtual mfxStatus GetExternalFrameHDL(mfxMemId mid, mfxHDL *handle, bool = true)              override;
    virtual mfxStatus UnlockExternalFrame(mfxMemId mid, mfxFrameData *ptr = nullptr, bool = true) override;

    std::pair<mfxStatus, bool> Lock(mfxFrameSurface1& surf, mfxU32 flags);
    std::pair<mfxStatus, bool> LockExternal(mfxFrameSurface1& surf, mfxU32 flags);
    std::pair<mfxStatus, bool> LockInternal(mfxFrameSurface1& surf, mfxU32 flags);
    mfxStatus Unlock(mfxFrameSurface1& surf);
    mfxStatus UnlockExternal(mfxFrameSurface1& surf);
    mfxStatus UnlockInternal(mfxFrameSurface1& surf);
    mfxStatus SwitchMemidInSurface(mfxFrameSurface1 & surf, mfxHDLPair& handle_pair);
    mfxStatus DeriveMemoryType(const mfxFrameSurface1& surf, mfxU16& derived_memtype);

    virtual mfxStatus DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)                                      override;

    virtual mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType) override;

    virtual bool IsExternalFrameAllocator() const                             override;

    virtual mfxStatus CopyFrame(mfxFrameSurface1 *dst, mfxFrameSurface1 *src) override;

    virtual void* QueryCoreInterface(const MFX_GUID &guid)                    override;

    virtual mfxStatus CreateSurface(mfxU16 type, const mfxFrameInfo& info, mfxFrameSurface1* &surf);

protected:

    CommonCORE_VPL(const mfxU32 numThreadsAvailable, const mfxSession session = nullptr);

    FrameAllocatorWrapper m_frame_allocator_wrapper;
};

enum class SurfaceLockType
{
    LOCK_NONE    = 0,
    LOCK_GENERAL = 1,
    LOCK_EXTERNAL,
    LOCK_INTERNAL
};

// Potentially can be put to std::lock_guard and etc
class mfxFrameSurface1_scoped_lock
{
public:
    mfxFrameSurface1_scoped_lock(mfxFrameSurface1* surf = nullptr, CommonCORE_VPL* core = nullptr)
        : surf(surf)
        , core(core)
        , mid(surf ? surf->Data.MemId : nullptr)
    {}

    mfxStatus lock(mfxU32 flags = MFX_MAP_READ_WRITE, SurfaceLockType lock = SurfaceLockType::LOCK_GENERAL)
    {
        MFX_CHECK_HDL(core);
        MFX_CHECK_NULL_PTR1(surf);

        mfxStatus sts;
        bool was_locked = false;

        switch (lock)
        {
        case SurfaceLockType::LOCK_NONE:
            sts = MFX_ERR_NONE;
            break;
        case SurfaceLockType::LOCK_GENERAL:
            std::tie(sts, was_locked) = core->Lock(*surf, flags);
            break;
        case SurfaceLockType::LOCK_EXTERNAL:
            std::tie(sts, was_locked) = core->LockExternal(*surf, flags);
            break;
        case SurfaceLockType::LOCK_INTERNAL:
            std::tie(sts, was_locked) = core->LockInternal(*surf, flags);
            break;
        default:
            MFX_RETURN(MFX_ERR_UNKNOWN);
        }

        MFX_CHECK_STS(sts);

        lock_type = was_locked ? lock : SurfaceLockType::LOCK_NONE;
        return MFX_ERR_NONE;
    }

    mfxStatus unlock()
    {
        MFX_CHECK_HDL(core);
        MFX_CHECK_NULL_PTR1(surf);

        // In some cases MSDK zeroes memid before copying. Following assignment guarantees correct unlock in such case
        surf->Data.MemId = mid;

        switch (lock_type)
        {
        case SurfaceLockType::LOCK_NONE:
            return MFX_ERR_NONE;

        case SurfaceLockType::LOCK_GENERAL:
            MFX_SAFE_CALL(core->Unlock(*surf));
            break;
        case SurfaceLockType::LOCK_EXTERNAL:
            MFX_SAFE_CALL(core->UnlockExternal(*surf));
            break;
        case SurfaceLockType::LOCK_INTERNAL:
            MFX_SAFE_CALL(core->UnlockInternal(*surf));
            break;
        default:
            MFX_RETURN(MFX_ERR_UNKNOWN);
        }

        // Do not unlock in destructor
        lock_type = SurfaceLockType::LOCK_NONE;
        return MFX_ERR_NONE;
    }

    ~mfxFrameSurface1_scoped_lock()
    {
        std::ignore = MFX_STS_TRACE(unlock());
    }

private:
    mfxFrameSurface1*  surf      = nullptr;
    CommonCORE_VPL*    core      = nullptr;
    mfxMemId           mid       = nullptr;
    SurfaceLockType    lock_type = SurfaceLockType::LOCK_NONE;
};

typedef struct SurfaceCacheRefCountable
{
    mfxHDL Context;
    mfxStatus (MFX_CDECL *AddRef)(struct SurfaceCacheRefCountable*);
    mfxStatus (MFX_CDECL *Release)(struct SurfaceCacheRefCountable*);
    mfxStatus (MFX_CDECL *GetRefCounter)(struct SurfaceCacheRefCountable*, mfxU32* /*counter*/);
} SurfaceCacheRefCountable;

class SurfaceCache;
template <>
struct mfxRefCountableInstance<SurfaceCacheRefCountable>
{
    static mfxRefCountable* Get(SurfaceCacheRefCountable* object)
    { return reinterpret_cast<mfxRefCountable*>(object); }
};

class SurfaceCache
    : public mfxRefCountableImpl<SurfaceCacheRefCountable>
{
public:
    static SurfaceCache* Create(CommonCORE_VPL& core, mfxU16 type, const mfxFrameInfo& frame_info)
    {
        auto cache = new SurfaceCache(core, type, frame_info);
        cache->AddRef();

        return cache;
    }

    std::chrono::milliseconds GetTimeout() const
    {
        return m_time_to_wait;
    }

    mfxStatus GetSurface(mfxFrameSurface1*& output_surface, bool emulate_zero_refcount_base = false)
    {
        return GetSurface(output_surface, m_time_to_wait, emulate_zero_refcount_base);
    }

    mfxStatus GetSurface(mfxFrameSurface1* & output_surface, std::chrono::milliseconds current_time_to_wait, bool emulate_zero_refcount_base = false)
    {
        /*
            Note: emulate_zero_refcount_base flag is required for some corner cases in decoders. More precisely
            it is required to comply with mfx_UMC_FrameAllocator adapter design: It has it's own refcounting
            logic (We repeat it's AddRef / Release in our "real" refcounting logic of surface), but implementation
            in mfx_UMC_FrameAllocator assumes that surface refcount started from zero while surface arrive,
            but that is not true for VPL memory, our new surfaces arrive with refcount equal to 1. So this trick
            is just allows us to emulate that zero refcount base, and leave mfx_UMC_FrameAllocator code as is.
        */
        std::unique_lock<std::mutex> lock(m_mutex);

        // Try to export existing surface from cache first

        output_surface = FreeSurfaceLookup(emulate_zero_refcount_base);
        if (output_surface)
        {
            return MFX_ERR_NONE;
        }

        if (m_cached_surfaces.size() >= m_limit)
        {
            using namespace std::chrono;

            MFX_CHECK(current_time_to_wait != 0ms, MFX_WRN_DEVICE_BUSY);

            // Cannot allocate surface, but we can wait
            bool wait_succeeded = m_cv_wait_free_surface.wait_for(lock, current_time_to_wait,
                [&output_surface, emulate_zero_refcount_base, this]()
            {
                output_surface = FreeSurfaceLookup(emulate_zero_refcount_base);

                return output_surface != nullptr;
            });

            MFX_CHECK(wait_succeeded, MFX_WRN_DEVICE_BUSY);

            return MFX_ERR_NONE;
        }

        // Get the new one from allocator
        lock.unlock();

        mfxFrameSurface1* surf = nullptr;
        // Surfaces returned by CreateSurface already have RefCounter == 1
        mfxStatus sts = m_core.CreateSurface(m_type, m_frame_info, surf);
        MFX_CHECK_STS(sts);

        lock.lock();
        m_cached_surfaces.emplace_back(*surf, *this);
        m_cached_surfaces.back().m_exported = true;

        if (emulate_zero_refcount_base)
        {
            m_cached_surfaces.back().FrameInterface->AddRef = &skip_one_addref;
        }

        output_surface = &m_cached_surfaces.back();

        return MFX_ERR_NONE;
    }

    mfxFrameSurface1* FindSurface(const mfxMemId memid)
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        auto it = find_if(std::begin(m_cached_surfaces), std::end(m_cached_surfaces), [memid](SurfaceHolder& surface) { return surface.Data.MemId == memid; });

        return it != std::end(m_cached_surfaces) ? &(*it) : nullptr;
    }


    mfxU32 ReportCurrentSize() const
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        return mfxU32(m_cached_surfaces.size());
    }

    mfxU32 ReportMaxSize() const
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        return mfxU32(m_limit);
    }


private:

    SurfaceCache(CommonCORE_VPL& core, mfxU16 type, const mfxFrameInfo& frame_info)
        : m_core(core)
        , m_type((type & ~MFX_MEMTYPE_EXTERNAL_FRAME) | MFX_MEMTYPE_INTERNAL_FRAME)
        , m_frame_info(frame_info)
    {
        std::ignore = m_num_to_revoke;
    }

    mfxFrameSurface1* FreeSurfaceLookup(bool emulate_zero_refcount_base = false)
    {
        // This function is called only from thread safe context, so no mutex acquiring here

        auto it = find_if(std::begin(m_cached_surfaces), std::end(m_cached_surfaces), [](SurfaceHolder& surface) { return !surface.m_exported; });

        if (it == std::end(m_cached_surfaces))
            return nullptr;

        it->m_exported = true;
        if (emulate_zero_refcount_base)
        {
            it->FrameInterface->AddRef = &skip_one_addref;
        }
#ifndef NDEBUG
        it->m_was_released = false;
#endif
        return &(*it);
    }

    static mfxStatus skip_one_addref(mfxFrameSurface1* surface)
    {
        MFX_CHECK_NULL_PTR1(surface);
        MFX_CHECK_HDL(surface->FrameInterface);

        // Return back original AddRef function
        surface->FrameInterface->AddRef = mfxFrameSurfaceBaseInterface::_AddRef;

        return MFX_ERR_NONE;
    }

    class SurfaceHolder : public mfxFrameSurface1
    {
    public:
        bool m_exported = false;
#ifndef NDEBUG
        bool m_was_released = false;
#endif

        SurfaceHolder(mfxFrameSurface1& surf, SurfaceCache& cache)
            : mfxFrameSurface1(surf)
            , m_cache(cache)
            , m_surface_interface(*FrameInterface)
            , original_release(m_surface_interface.Release)
            , m_locked_count(surf.Data.Locked)
        {
            m_surface_interface.Release = proxy_release;

            std::ignore = vm_interlocked_inc16((volatile Ipp16u*)&m_locked_count);

            std::ignore = m_cache;
        }

        ~SurfaceHolder()
        {

            std::ignore = vm_interlocked_dec16((volatile Ipp16u*)&m_locked_count);

            m_surface_interface.Release = original_release;

            MFX_STS_TRACE(m_surface_interface.Release(this));
        }


    private:

        // To decommit surfaces on release
        SurfaceCache & m_cache;

        // Store it to protect against user zeroing this pointer
        mfxFrameSurfaceInterface & m_surface_interface;

        mfxStatus(MFX_CDECL *original_release)(mfxFrameSurface1* surface);

        // To mimic legacy behavior
        mfxU16 & m_locked_count;

        static mfxStatus proxy_release(mfxFrameSurface1* surface)
        {
            MFX_CHECK_NULL_PTR1(surface);
            MFX_CHECK_HDL(surface->FrameInterface);
            MFX_CHECK_HDL(surface->FrameInterface->Context);

            static std::mutex proxy_release_mutex;
            std::lock_guard<std::mutex> lock(proxy_release_mutex);

            mfxU32 ref_counter = reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->GetRefCounter();

            if (ref_counter > 2)
            {
                // Bypass to original release function
                return reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->Release();
            }

            // Return back to pool, don't touch ref counter
            reinterpret_cast<SurfaceHolder*>(surface)->m_exported = false;

#ifndef NDEBUG
            assert(!reinterpret_cast<SurfaceHolder*>(surface)->m_was_released);
            reinterpret_cast<SurfaceHolder*>(surface)->m_was_released = true;
#endif

            return MFX_ERR_NONE;
        }
    };

    mutable std::mutex        m_mutex;
    std::condition_variable   m_cv_wait_free_surface;

    std::chrono::milliseconds m_time_to_wait = std::chrono::milliseconds(0);

    CommonCORE_VPL&           m_core;
    mfxU16                    m_type;
    mfxFrameInfo              m_frame_info;

    size_t                    m_limit         = std::numeric_limits<size_t>::max();
    size_t                    m_num_to_revoke = 0;

    std::list<SurfaceHolder>  m_cached_surfaces;
    std::list<mfxU32>         m_requests;
};

inline bool SupportsVPLFeatureSet(VideoCORE& core)
{
    return !!dynamic_cast<CommonCORE_VPL*>(&core);
}

inline bool IsD3D9Simulation(VideoCORE& core)
{
    bool* cored3d9on11_interface = reinterpret_cast<bool*>(core.QueryCoreInterface(MFXI_IS_CORED3D9ON11_GUID));
    return cored3d9on11_interface && *cored3d9on11_interface;
}

#endif
