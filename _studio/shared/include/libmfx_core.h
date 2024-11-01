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


using AffinityMaskType = std::pair<mfxU32/*size*/, std::vector<mfxU8>/*mask*/>;

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

    virtual mfxStatus DoFastCopy(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)                      override;
    virtual mfxStatus DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc, mfxU32 = MFX_COPY_USE_ANY) override;

    virtual mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType, mfxU32 = MFX_COPY_USE_ANY) override;

    // DEPRECATED
    virtual bool IsFastCopyEnabled()              override { return true; }

    virtual bool IsExternalFrameAllocator() const override;
    virtual eMFXHWType GetHWType()                override { return MFX_HW_UNKNOWN; }

    virtual mfxU16    GetHWDeviceId()             override { return 0; }

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

#if defined(MFX_ENABLE_PXP)
    mfxHDL                                     m_pPXPCtxHdl;
#endif // MFX_ENABLE_PXP

    mfxU16                                     m_deviceId;

    class mfxMemoryInterfaceWrapper : public mfxMemoryInterface
    {
    public:
        mfxMemoryInterfaceWrapper(mfxSession session)
        {
            Context            = session;
            Version.Version    = MFX_MEMORYINTERFACE_VERSION;
            ImportFrameSurface = ImportFrameSurface_impl;
        }

        static mfxStatus ImportFrameSurface_impl(mfxMemoryInterface* memory_interface, mfxSurfaceComponent surf_component, mfxSurfaceHeader* ext_surface, mfxFrameSurface1** imported_surface)
        {
            MFX_CHECK_NULL_PTR2(memory_interface, ext_surface);

            auto session = reinterpret_cast<mfxSession>(memory_interface->Context);

            switch (surf_component)
            {
            case MFX_SURFACE_COMPONENT_ENCODE:
                MFX_CHECK(session->m_pENCODE, MFX_ERR_NOT_INITIALIZED);

                MFX_RETURN(session->m_pENCODE->GetSurface(imported_surface, ext_surface));

            case MFX_SURFACE_COMPONENT_DECODE:
                MFX_CHECK(session->m_pDECODE, MFX_ERR_NOT_INITIALIZED);
                MFX_CHECK_NULL_PTR1(imported_surface);

                MFX_RETURN(session->m_pDECODE->GetSurface(*imported_surface, ext_surface));

            case MFX_SURFACE_COMPONENT_VPP_INPUT:
                MFX_CHECK(session->m_pVPP, MFX_ERR_NOT_INITIALIZED);

                MFX_RETURN(session->m_pVPP->GetSurfaceFromIn(imported_surface, ext_surface));

            case MFX_SURFACE_COMPONENT_VPP_OUTPUT:
                MFX_CHECK(session->m_pVPP, MFX_ERR_NOT_INITIALIZED);

                MFX_RETURN(session->m_pVPP->GetSurfaceFromOut(imported_surface, ext_surface));

            default:
                MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
            }
        }
    } m_memory_interface;

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

    deprecate_from_base(const mfxU32 adapterNum, const AffinityMaskType& affinityMask, const mfxU32 numThreadsAvailable, const mfxSession session = nullptr)
        : Base(adapterNum, affinityMask, numThreadsAvailable, session)
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

    virtual mfxStatus DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc, mfxU32 = MFX_COPY_USE_ANY)                                      override;

    virtual mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType, mfxU32 = MFX_COPY_USE_ANY) override;

    virtual bool IsExternalFrameAllocator() const                             override;

    virtual mfxStatus CopyFrame(mfxFrameSurface1 *dst, mfxFrameSurface1 *src) override;

    virtual void* QueryCoreInterface(const MFX_GUID &guid)                    override;

    virtual mfxStatus CreateSurface(mfxU16 type, const mfxFrameInfo& info, mfxFrameSurface1* &surf, mfxSurfaceHeader* import_surface);

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

template <>
struct mfxRefCountableInstance<mfxSurfacePoolInterface>
{
    static mfxRefCountable* Get(mfxSurfacePoolInterface* object)
    { return reinterpret_cast<mfxRefCountable*>(object->Context); }
};

class SurfaceCache
    : public mfxRefCountableImpl<mfxSurfacePoolInterface>
{
public:
    static SurfaceCache* Create(CommonCORE_VPL& core, mfxU16 type, const mfxFrameInfo& frame_info)
    {
        auto cache = new SurfaceCache(core, type, frame_info);
        cache->AddRef();

        return cache;
    }

    static mfxStatus SetNumSurfaces_impl(mfxSurfacePoolInterface *pool_interface, mfxU32 num_surfaces)
    {
        MFX_CHECK_NULL_PTR1(pool_interface);
        MFX_CHECK_HDL(pool_interface->Context);

        auto cache = reinterpret_cast<SurfaceCache*>(pool_interface->Context);

        return cache->UpdateLimits(num_surfaces);
    }

    static mfxStatus RevokeSurfaces_impl(mfxSurfacePoolInterface *pool_interface, mfxU32 num_surfaces)
    {
        MFX_CHECK_NULL_PTR1(pool_interface);
        MFX_CHECK_HDL(pool_interface->Context);

        auto cache = reinterpret_cast<SurfaceCache*>(pool_interface->Context);

        return cache->RevokeSurfaces(num_surfaces);
    }

    static mfxStatus GetAllocationPolicy_impl(mfxSurfacePoolInterface *pool_interface, mfxPoolAllocationPolicy *policy)
    {
        MFX_CHECK_NULL_PTR2(pool_interface, policy);
        MFX_CHECK_HDL(pool_interface->Context);

        auto cache = reinterpret_cast<SurfaceCache*>(pool_interface->Context);

        *policy = cache->ReportPolicy();

        return MFX_ERR_NONE;
    }

    static mfxStatus GetMaximumPoolSize_impl(mfxSurfacePoolInterface *pool_interface, mfxU32 *size)
    {
        MFX_CHECK_NULL_PTR2(pool_interface, size);
        MFX_CHECK_HDL(pool_interface->Context);

        auto cache = reinterpret_cast<SurfaceCache*>(pool_interface->Context);

        *size = cache->ReportMaxSize();

        return MFX_ERR_NONE;
    }

    static mfxStatus GetCurrentPoolSize_impl(mfxSurfacePoolInterface *pool_interface, mfxU32 *size)
    {
        MFX_CHECK_NULL_PTR2(pool_interface, size);
        MFX_CHECK_HDL(pool_interface->Context);

        auto cache = reinterpret_cast<SurfaceCache*>(pool_interface->Context);

        *size = cache->ReportCurrentSize();

        return MFX_ERR_NONE;
    }

    std::chrono::milliseconds GetTimeout() const
    {
        return m_time_to_wait;
    }

    mfxStatus GetSurface(mfxFrameSurface1*& output_surface, bool emulate_zero_refcount_base = false, mfxSurfaceHeader* import_surface = nullptr)
    {
        return GetSurface(output_surface, m_time_to_wait, emulate_zero_refcount_base, import_surface);
    }

    mfxStatus GetSurface(mfxFrameSurface1* & output_surface, std::chrono::milliseconds current_time_to_wait, bool emulate_zero_refcount_base = false, mfxSurfaceHeader* import_surface = nullptr)
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

        if (!import_surface)
        {
            // Try to export existing surface from cache first

            output_surface = FreeSurfaceLookup(emulate_zero_refcount_base);
            if (output_surface)
            {
                return MFX_ERR_NONE;
            }

            if (m_cached_surfaces.size() + m_num_pending_insertion >= m_limit)
            {
                using namespace std::chrono;

                MFX_CHECK(current_time_to_wait != 0ms, MFX_WRN_ALLOC_TIMEOUT_EXPIRED);

                // Cannot allocate (no free slots) surface, but we can wait
                bool wait_succeeded = m_cv_wait_free_surface.wait_for(lock, current_time_to_wait,
                    [&output_surface, emulate_zero_refcount_base, this]()
                {
                    output_surface = FreeSurfaceLookup(emulate_zero_refcount_base);

                    return output_surface != nullptr;
                });

                MFX_CHECK(wait_succeeded, MFX_WRN_ALLOC_TIMEOUT_EXPIRED);

                return MFX_ERR_NONE;
            }
        }
        // Check if there is a free slot for insertion
        else if (m_cached_surfaces.size() + m_num_pending_insertion >= m_limit)
        {
            // We try to reallocate one of the existing free surfaces if cache limit reached, but user asks to import surface
            auto it = std::find_if(std::begin(m_cached_surfaces), std::end(m_cached_surfaces), [](const SurfaceHolder& surface) { return !surface.m_in_use; });

            if (it == std::end(m_cached_surfaces))
            {
                using namespace std::chrono;

                MFX_CHECK(current_time_to_wait != 0ms, MFX_WRN_ALLOC_TIMEOUT_EXPIRED);

                // Cannot allocate (no free slots) surface, but we can wait
                bool wait_succeeded = m_cv_wait_free_surface.wait_for(lock, current_time_to_wait,
                    [&it, this]()
                    {
                        it = std::find_if(std::begin(m_cached_surfaces), std::end(m_cached_surfaces), [](const SurfaceHolder& surface) { return !surface.m_in_use; });

                        return it != std::end(m_cached_surfaces);
                    });

                MFX_CHECK(wait_succeeded, MFX_WRN_ALLOC_TIMEOUT_EXPIRED);

                m_cached_surfaces.erase(it);
            }
        }

        // Get the new one from allocator
        ++m_num_pending_insertion;
        lock.unlock();

        mfxFrameSurface1* surf = nullptr;
        // Surfaces returned by CreateSurface already have RefCounter == 1
        mfxStatus sts = m_core.CreateSurface(m_type, m_frame_info, surf, import_surface);
        MFX_CHECK_STS(sts);

        lock.lock();
        m_cached_surfaces.emplace_back(*surf, *this);
        --m_num_pending_insertion;
        m_cached_surfaces.back().m_in_use = true;
        // We can relax this in future if actually copy happened during import
        m_cached_surfaces.back().m_created_from_external_handle = !!import_surface;

        if (emulate_zero_refcount_base)
        {
            m_cached_surfaces.back().FrameInterface->AddRef = &skip_one_addref;
        }

        output_surface = &m_cached_surfaces.back();

        TRACE_EVENT(MFX_TRACE_API_GETSURFACE_TASK, EVENT_TYPE_INFO, TR_KEY_INTERNAl, make_event_data(m_cached_surfaces.size()));

        return MFX_ERR_NONE;
    }

    mfxFrameSurface1* FindSurface(const mfxMemId memid)
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        auto it = std::find_if(std::begin(m_cached_surfaces), std::end(m_cached_surfaces), [memid](const SurfaceHolder& surface) { return surface.Data.MemId == memid; });

        return it != std::end(m_cached_surfaces) ? &(*it) : nullptr;
    }

    mfxStatus SetupPolicy(const mfxExtAllocationHints& hints_buffer)
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        m_policy = hints_buffer.AllocationPolicy;

        // Setup upper limit on surface amount
        if (m_policy == MFX_ALLOCATION_LIMITED)
        {
            m_limit = size_t(hints_buffer.NumberToPreAllocate) + hints_buffer.DeltaToAllocateOnTheFly;
        }
        else if (m_policy != MFX_ALLOCATION_UNLIMITED)
        {
            m_limit = 0;
        }

        // Preallocate surfaces if requested
        if ((m_policy == MFX_ALLOCATION_LIMITED || m_policy == MFX_ALLOCATION_UNLIMITED) && hints_buffer.NumberToPreAllocate)
        {
            std::list<SurfaceHolder> preallocated_surfaces;
            mfxFrameSurface1* surf;

            for (mfxU32 i = 0; i < hints_buffer.NumberToPreAllocate; ++i)
            {
                MFX_SAFE_CALL(m_core.CreateSurface(m_type, m_frame_info, surf, nullptr));
                preallocated_surfaces.emplace_back(*surf, *this);
            }

            m_cached_surfaces = std::move(preallocated_surfaces);
        }

        m_time_to_wait = std::chrono::milliseconds(hints_buffer.Wait);

        return MFX_ERR_NONE;
    }

    mfxStatus UpdateLimits(mfxU32 num_surfaces_requested_by_component)
    {
        MFX_CHECK(m_policy != MFX_ALLOCATION_LIMITED && m_policy != MFX_ALLOCATION_UNLIMITED, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        std::lock_guard<std::mutex> guard(m_mutex);

        switch (m_policy)
        {
        case MFX_ALLOCATION_OPTIMAL:
            m_limit += num_surfaces_requested_by_component;
            break;

        default:
            MFX_RETURN(MFX_ERR_UNKNOWN);
        }

        m_requests.push_back(num_surfaces_requested_by_component);

        return MFX_ERR_NONE;
    }

    mfxStatus RevokeSurfaces(mfxU32 num_surfaces_requested_by_component)
    {
        MFX_CHECK(m_policy != MFX_ALLOCATION_LIMITED && m_policy != MFX_ALLOCATION_UNLIMITED, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        std::unique_lock<std::mutex> lock(m_mutex);

        auto it_to_del = std::find(std::begin(m_requests), std::end(m_requests), num_surfaces_requested_by_component);
        MFX_CHECK(it_to_del != std::end(m_requests), MFX_WRN_OUT_OF_RANGE);

        m_requests.erase(it_to_del);

        size_t num_to_revoke = 0;

        switch (m_policy)
        {
        case MFX_ALLOCATION_OPTIMAL:

            m_limit -= num_surfaces_requested_by_component;

            if (m_cached_surfaces.size() <= m_limit)
                return MFX_ERR_NONE;

            num_to_revoke = m_cached_surfaces.size() - m_limit;

            break;

        default:
            MFX_RETURN(MFX_ERR_UNKNOWN);
        }

        // Decommit surfaces
        if (num_to_revoke)
        {
            m_num_to_revoke += num_to_revoke;

            DecomitSurfaces(lock);
        }

        return MFX_ERR_NONE;
    }

    void DecomitSurfaces(std::unique_lock<std::mutex>& outer_lock)
    {
        // Actual delete will happen after mutex unlock
        std::list<SurfaceHolder> surfaces_to_decommit;

        std::unique_lock<std::mutex> lock(std::move(outer_lock));

        auto should_remove_predicate =
            [](const SurfaceHolder& surface_holder)
        {
            // Check if surface is still owned by somebody
            return !surface_holder.m_in_use;
        };

        // splice_if
        for (auto it = std::begin(m_cached_surfaces); it != std::end(m_cached_surfaces) && m_num_to_revoke;)
        {
            auto it_to_transfer = it++;
            if (should_remove_predicate(*it_to_transfer))
            {
                surfaces_to_decommit.splice(std::end(surfaces_to_decommit), m_cached_surfaces, it_to_transfer);
                --m_num_to_revoke;
            }
        }
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

    mfxPoolAllocationPolicy ReportPolicy() const
    {
        // Right now policy is not changing during runtime, so this function don't have mutex protection
        return m_policy;
    }

    mfxStatus MarkSurfaceFree(mfxMemId mid)
    {
        // Actual delete will happen after mutex unlock
        std::list<SurfaceHolder> surface_to_delete;

        std::unique_lock<std::mutex> lock(m_mutex);

        auto p_holder = std::find_if(std::begin(m_cached_surfaces), std::end(m_cached_surfaces), [mid](const SurfaceHolder& surface) { return surface.Data.MemId == mid; });
        MFX_CHECK(p_holder != std::end(m_cached_surfaces), MFX_ERR_NOT_FOUND);

        // Mark as free
        p_holder->m_in_use = false;

#ifndef NDEBUG
        assert(!p_holder->m_was_released);
        p_holder->m_was_released = true;
#endif
        // For imported surfaces we delete it immidiately, without returning to cache (since we don't control lifetime of HW handle)
        if (p_holder->m_created_from_external_handle)
        {
            surface_to_delete.splice(std::end(surface_to_delete), m_cached_surfaces, p_holder);

            return MFX_ERR_NONE;
        }

        // Remove surfaces from pool if required or notify waiters about free surface
        if (!m_num_to_revoke)
        {
            // If no surfaces to decommit, notify some waiter
            lock.unlock();
            m_cv_wait_free_surface.notify_one();
            return MFX_ERR_NONE;
        }

        // Decommit current surface
        surface_to_delete.splice(std::end(surface_to_delete), m_cached_surfaces, p_holder);
        --m_num_to_revoke;

        return MFX_ERR_NONE;
    }

private:

    SurfaceCache(CommonCORE_VPL& core, mfxU16 type, const mfxFrameInfo& frame_info)
        : m_core(core)
        , m_type((type & ~MFX_MEMTYPE_EXTERNAL_FRAME) | MFX_MEMTYPE_INTERNAL_FRAME)
        , m_frame_info(frame_info)
    {
        Context = this;

        mfxSurfacePoolInterface::SetNumSurfaces      = &SurfaceCache::SetNumSurfaces_impl;
        mfxSurfacePoolInterface::RevokeSurfaces      = &SurfaceCache::RevokeSurfaces_impl;
        mfxSurfacePoolInterface::GetAllocationPolicy = &SurfaceCache::GetAllocationPolicy_impl;
        mfxSurfacePoolInterface::GetMaximumPoolSize  = &SurfaceCache::GetMaximumPoolSize_impl;
        mfxSurfacePoolInterface::GetCurrentPoolSize  = &SurfaceCache::GetCurrentPoolSize_impl;
    }

    mfxFrameSurface1* FreeSurfaceLookup(bool emulate_zero_refcount_base = false)
    {
        // This function is called only from thread safe context, so no mutex acquiring here

        auto it = std::find_if(std::begin(m_cached_surfaces), std::end(m_cached_surfaces), [](const SurfaceHolder& surface) { return !surface.m_in_use; });

        if (it == std::end(m_cached_surfaces))
            return nullptr;

        it->m_in_use = true;
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
        // Right now in usage
        bool m_in_use                       = false;
        // Current surface was Imported (i.e. created from user-provided handle)
        bool m_created_from_external_handle = false;
#ifndef NDEBUG
        bool m_was_released = false;
#endif

        SurfaceHolder(mfxFrameSurface1& surf, SurfaceCache& cache)
            : mfxFrameSurface1(surf)
            , m_cache(cache)
            , m_surface_interface(*FrameInterface)
            , original_release(m_surface_interface.Release)
        {
            FrameInterface->Release = m_surface_interface.Release = proxy_release;

            // Connect surface with it's pool (cache instance)
            reinterpret_cast<mfxFrameSurfaceBaseInterface*>(m_surface_interface.Context)->SetParentPool(&m_cache);
        }

        ~SurfaceHolder()
        {
            // Untie surface from pool
            reinterpret_cast<mfxFrameSurfaceBaseInterface*>(m_surface_interface.Context)->SetParentPool(nullptr);

            m_surface_interface.Release = original_release;

            *FrameInterface = m_surface_interface;

            std::ignore = MFX_STS_TRACE(m_surface_interface.Release(this));
        }

    private:

        // To decommit surfaces on release
        SurfaceCache & m_cache;

        // Store it to protect against user zeroing this pointer
        mfxFrameSurfaceInterface m_surface_interface;

        mfxStatus(MFX_CDECL *original_release)(mfxFrameSurface1* surface);

        static mfxStatus proxy_release(mfxFrameSurface1* surface)
        {
            MFX_CHECK_NULL_PTR1(surface);
            MFX_CHECK_HDL(surface->FrameInterface);
            MFX_CHECK_HDL(surface->FrameInterface->Context);

            static std::mutex proxy_release_mutex;
            std::lock_guard<std::mutex> lock(proxy_release_mutex);

            mfxU32 ref_counter = reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->GetRefCounter();

            if (ref_counter > 1)
            {
                // Bypass to original release function
                return reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->Release();
            }

            // Return back to pool, don't touch ref counter
            auto pool = reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->QueryParentPool();
            MFX_CHECK_HDL(pool);
            auto cache = reinterpret_cast<SurfaceCache*>(pool->Context);
            MFX_CHECK_HDL(cache);
            mfx::OnExit release_cache([cache]() { cache->Release(); });

            return cache->MarkSurfaceFree(surface->Data.MemId);
        }
    };

    mutable std::mutex        m_mutex;
    std::condition_variable   m_cv_wait_free_surface;

    std::chrono::milliseconds m_time_to_wait = std::chrono::milliseconds(0);

    CommonCORE_VPL&           m_core;
    mfxU16                    m_type;
    mfxFrameInfo              m_frame_info;

    mfxPoolAllocationPolicy   m_policy = MFX_ALLOCATION_UNLIMITED;
    // Default is MFX_ALLOCATION_UNLIMITED
    size_t                    m_limit                 = std::numeric_limits<size_t>::max();
    // Counter of surfaces being constructed
    size_t                    m_num_pending_insertion = 0;
    size_t                    m_num_to_revoke         = 0;

    std::list<SurfaceHolder>  m_cached_surfaces;
    std::list<mfxU32>         m_requests;
};

inline bool SupportsVPLFeatureSet(VideoCORE& core)
{
    return !!core.QueryCoreInterface(MFXICommonCORE_VPL_GUID);
}

inline bool IsD3D9Simulation(VideoCORE& core)
{
    bool* cored3d9on11_interface = reinterpret_cast<bool*>(core.QueryCoreInterface(MFXI_IS_CORED3D9ON11_GUID));
    return cored3d9on11_interface && *cored3d9on11_interface;
}

#endif
