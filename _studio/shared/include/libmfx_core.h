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


class mfx_UMC_FrameAllocator;

// Virtual table size for CommonCORE should be considered fixed.
// Otherwise binary compatibility with already released plugins would be broken.

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
                                   mfxFrameAllocResponse *response)                                       override;

    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request,
                                   mfxFrameAllocResponse *response,
                                   mfxFrameSurface1 **pOpaqueSurface,
                                   mfxU32 NumOpaqueSurface)                                               override;

    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr)                                          override;
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr = nullptr)                              override;
    virtual mfxStatus FreeFrames(mfxFrameAllocResponse *response, bool ExtendedSearch = true)             override;

    virtual mfxStatus LockExternalFrame(mfxMemId mid, mfxFrameData *ptr, bool ExtendedSearch = true)      override;
    virtual mfxStatus GetExternalFrameHDL(mfxMemId mid, mfxHDL *handle, bool ExtendedSearch = true)       override;
    virtual mfxStatus UnlockExternalFrame(mfxMemId mid, mfxFrameData *ptr=0, bool ExtendedSearch = true)  override;

    virtual mfxMemId MapIdx(mfxMemId mid)                                                                 override;

    // Get original Surface corresponding to OpaqueSurface
    virtual mfxFrameSurface1* GetNativeSurface(mfxFrameSurface1 *pOpqSurface, bool ExtendedSearch = true) override;
    // Get OpaqueSurface corresponding to Original
    virtual mfxFrameSurface1* GetOpaqSurface(mfxMemId mid, bool ExtendedSearch = true)                    override;

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

    virtual bool CheckOpaqueRequest(mfxFrameAllocRequest *request, mfxFrameSurface1 **pOpaqueSurface, mfxU32 NumOpaqueSurface, bool ExtendedSearch = true) override;

    virtual eMFXVAType GetVAType() const                   override { return MFX_HW_NO; }

    virtual bool SetCoreId(mfxU32 Id)                      override;
    virtual void* QueryCoreInterface(const MFX_GUID &guid) override;

    virtual mfxSession GetSession()                        override { return m_session; }

    virtual void SetWrapper(void* pWrp)                    override;

    virtual mfxU16 GetAutoAsyncDepth()                     override;

    virtual bool IsCompatibleForOpaq()                     override { return true; }

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
    virtual mfxStatus          InternalFreeFrames(mfxFrameAllocResponse *response);
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

    //function checks if surfaces already allocated and mapped and request is consistent. Fill response if surfaces are correct
    virtual bool IsOpaqSurfacesAlreadyMapped(mfxFrameSurface1 **pOpaqueSurface, mfxU32 NumOpaqueSurface, mfxFrameAllocResponse *response, bool ExtendedSearch = true) override;

    typedef struct
    {
        mfxMemId InternalMid;
        bool isDefaultMem;
        mfxU16 memType;

    } MemDesc;

    typedef std::map<mfxMemId, MemDesc> CorrespTbl;
    typedef std::map<mfxMemId, mfxBaseWideFrameAllocator*> AllocQueue;
    typedef std::map<mfxMemId*, mfxMemId*> MemIDMap;

    typedef std::map<mfxFrameSurface1*, mfxFrameSurface1> OpqTbl;
    typedef std::map<mfxMemId, mfxFrameSurface1*> OpqTbl_MemId;
    typedef std::map<mfxFrameData*, mfxFrameSurface1*> OpqTbl_FrameData;
    typedef std::map<mfxFrameAllocResponse*, mfxU32> RefCtrTbl;


    CorrespTbl       m_CTbl;
    AllocQueue       m_AllocatorQueue;
    MemIDMap         m_RespMidQ;
    OpqTbl           m_OpqTbl;
    OpqTbl_MemId     m_OpqTbl_MemId;
    OpqTbl_FrameData m_OpqTbl_FrameData;
    RefCtrTbl        m_RefCtrTbl;

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

    bool                                       m_bIsOpaqMode;

    mfxU32                                     m_CoreId;

    mfx_UMC_FrameAllocator*                    m_pWrp;

    EncodeHWCaps                               m_encode_caps;
    EncodeHWCaps                               m_encode_mbprocrate;

    std::vector<mfxFrameAllocResponse>         m_PlugInMids;

    API_1_19_Adapter                           m_API_1_19;

    mfxU16                                     m_deviceId;

    bool                                       m_enabled20Interface = false;

    CommonCORE & operator = (const CommonCORE &) = delete;
};

mfxStatus CoreDoSWFastCopy(mfxFrameSurface1 & dst, const mfxFrameSurface1 & src, int copyFlag);

// Refactored MSDK 2.0 core

template<class Base>
class deprecate_from_base : public Base
{
public:
    virtual ~deprecate_from_base() {}

    virtual mfxMemId MapIdx(mfxMemId mid)                      override { return this->m_enabled20Interface ? mid : Base::MapIdx(mid); }

    // TODO: Clean up code from buffer allocator usage and uncomment following items
    /* Deprecated functionality : buffer allocator */
    /*
    virtual mfxStatus SetBufferAllocator(mfxBufferAllocator * allocator)      override
    {
        return this->m_enabled20Interface ? MFX_ERR_UNSUPPORTED : Base::SetBufferAllocator(allocator);
    }

    virtual mfxStatus AllocBuffer(mfxU32 nbytes, mfxU16 type, mfxMemId *mid)  override
    {
        return this->m_enabled20Interface ? MFX_ERR_UNSUPPORTED : Base::AllocBuffer(nbytes, type, mid);
    }

    virtual mfxStatus LockBuffer(mfxMemId mid, mfxU8 **ptr)                   override
    {
        return this->m_enabled20Interface ? MFX_ERR_UNSUPPORTED : Base::LockBuffer(mid, ptr);
    }

    virtual mfxStatus UnlockBuffer(mfxMemId mid)                              override
    {
        return this->m_enabled20Interface ? MFX_ERR_UNSUPPORTED : Base::UnlockBuffer(mid);
    }

    virtual mfxStatus FreeBuffer(mfxMemId mid)                                override
    {
        return this->m_enabled20Interface ? MFX_ERR_UNSUPPORTED : Base::FreeBuffer(mid);
    }
    */
    /* Deprecated functionality : opaq memory */
    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request,
                                  mfxFrameAllocResponse *response,
                                  mfxFrameSurface1 **pOpaqueSurface,
                                  mfxU32 NumOpaqueSurface)                                                      override
    {
        return this->m_enabled20Interface ? MFX_ERR_UNSUPPORTED : CommonCORE::AllocFrames(request, response, pOpaqueSurface, NumOpaqueSurface);
    }

    virtual mfxFrameSurface1* GetNativeSurface(mfxFrameSurface1 *pOpqSurface, bool ExtendedSearch = true)       override
    {
        return this->m_enabled20Interface ? nullptr : Base::GetNativeSurface(pOpqSurface, ExtendedSearch);
    }

    virtual mfxFrameSurface1* GetOpaqSurface(mfxMemId mid, bool ExtendedSearch = true)                          override
    {
        return this->m_enabled20Interface ? nullptr : Base::GetOpaqSurface(mid, ExtendedSearch);
    }

    virtual bool CheckOpaqueRequest(mfxFrameAllocRequest *request,
                                    mfxFrameSurface1 **pOpaqueSurface,
                                    mfxU32 NumOpaqueSurface,
                                    bool ExtendedSearch = true)                                                 override
    {
        return this->m_enabled20Interface ? false : Base::CheckOpaqueRequest(request, pOpaqueSurface, NumOpaqueSurface, ExtendedSearch);
    }

    virtual bool SetCoreId(mfxU32 Id)                                                                           override
    {
        return this->m_enabled20Interface ? false : Base::SetCoreId(Id);
    }

    virtual bool IsCompatibleForOpaq()                                                                          override
    {
        return this->m_enabled20Interface ? false : Base::IsCompatibleForOpaq();
    }

protected:
    virtual mfxStatus DefaultAllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)        override
    {
        return this->m_enabled20Interface ? MFX_ERR_UNSUPPORTED : Base::DefaultAllocFrames(request, response);
    }

    virtual mfxStatus InternalFreeFrames(mfxFrameAllocResponse *response)                                       override
    {
        return this->m_enabled20Interface ? MFX_ERR_UNSUPPORTED : Base::InternalFreeFrames(response);
    }

    virtual bool IsOpaqSurfacesAlreadyMapped(mfxFrameSurface1     **pOpaqueSurface,
                                             mfxU32                 NumOpaqueSurface,
                                             mfxFrameAllocResponse *response,
                                             bool                   ExtendedSearch = true)                      override
    {
        return this->m_enabled20Interface ? false : Base::IsOpaqSurfacesAlreadyMapped(pOpaqueSurface, NumOpaqueSurface, response, ExtendedSearch);
    }

    deprecate_from_base(const mfxU32 numThreadsAvailable, const mfxSession session = nullptr)
        : Base(numThreadsAvailable, session)
    {}

    deprecate_from_base(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session = nullptr)
        : Base(adapterNum, numThreadsAvailable, session)
    {}
};

// Virtual table size for CommonCORE and CommonCORE20 should be considered fixed.
// Otherwise binary compatibility with already released plugins would be broken.
class CommonCORE20 : public deprecate_from_base<CommonCORE>
{
public:

    friend class FactoryCORE;

    virtual mfxStatus SetFrameAllocator(mfxFrameAllocator *allocator)                             override;

    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle, bool = true)                      override;

    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response) override;

    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr)                                  override;
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr = nullptr)                      override;
    virtual mfxStatus FreeFrames(mfxFrameAllocResponse *response, bool = true)                    override;

    virtual mfxStatus LockExternalFrame(mfxMemId mid, mfxFrameData *ptr, bool = true) override;
    virtual mfxStatus GetExternalFrameHDL(mfxMemId mid, mfxHDL *handle, bool = true) override;
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

    // TODO: check if we allowed to break vtable for MSDK2.0
    virtual mfxStatus CreateSurface(mfxU16 type, const mfxFrameInfo& info, mfxFrameSurface1* &surf);

protected:

    CommonCORE20(const mfxU32 numThreadsAvailable, const mfxSession session = nullptr);

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
    mfxFrameSurface1_scoped_lock(mfxFrameSurface1* surf = nullptr, CommonCORE20* core = nullptr)
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
    CommonCORE20*      core      = nullptr;
    mfxMemId           mid       = nullptr;
    SurfaceLockType    lock_type = SurfaceLockType::LOCK_NONE;
};

class SurfaceCache
{
public:
    SurfaceCache(CommonCORE20& core, mfxU16 type, const mfxFrameInfo& frame_info)
        : m_core(core)
        , m_type((type & ~MFX_MEMTYPE_EXTERNAL_FRAME) | MFX_MEMTYPE_INTERNAL_FRAME)
        , m_frame_info(frame_info)
    {}
    virtual ~SurfaceCache() {}

    virtual mfxFrameSurface1* GetSurface(bool emulate_zero_refcount_base = false)
    {
        std::unique_lock<std::shared_timed_mutex> lock(m_mutex);

        // Export from cache
        auto it = find_if(std::begin(m_cached_surfaces), std::end(m_cached_surfaces), [](SurfaceHolder& surface) { return !surface.m_exported; });

        if (it != std::end(m_cached_surfaces))
        {
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

        // Get the new one from allocator
        lock.unlock();

        mfxFrameSurface1* surf = nullptr;
        // Surfaces returned by CreateSurface already have RefCounter == 1
        mfxStatus sts = m_core.CreateSurface(m_type, m_frame_info, surf);
        if (MFX_STS_TRACE(sts) != MFX_ERR_NONE)
        {
            return nullptr;
        }

        lock.lock();
        m_cached_surfaces.emplace_back(*surf);
        m_cached_surfaces.back().m_exported = true;

        if (emulate_zero_refcount_base)
        {
            m_cached_surfaces.back().FrameInterface->AddRef = &skip_one_addref;
        }

        return &m_cached_surfaces.back();
    }

    mfxFrameSurface1* FindSurface(const mfxMemId memid)
    {
        std::shared_lock<std::shared_timed_mutex> lock(m_mutex);

        auto it = find_if(std::begin(m_cached_surfaces), std::end(m_cached_surfaces), [memid](SurfaceHolder& surface) { return surface.Data.MemId == memid; });

        return it != std::end(m_cached_surfaces) ? &(*it) : nullptr;
    }

private:

    static mfxStatus skip_one_addref(mfxFrameSurface1* surface)
    {
        MFX_CHECK_NULL_PTR1(surface);
        MFX_CHECK_HDL(surface->FrameInterface);

        // Return back original AddRef function
        surface->FrameInterface->AddRef = &mfxFrameSurfaceInterfaceImpl::AddRef_impl;

        return MFX_ERR_NONE;
    }

    class SurfaceHolder : public mfxFrameSurface1
    {
    public:
        bool m_exported = false;
#ifndef NDEBUG
        bool m_was_released = false;
#endif

        SurfaceHolder(mfxFrameSurface1& surf)
            : mfxFrameSurface1(surf)
            , m_surface_interface(*FrameInterface)
            , original_release(m_surface_interface.Release)
            , m_locked_count(surf.Data.Locked)
        {
            m_surface_interface.Release = proxy_release;

            std::ignore = vm_interlocked_inc16((volatile Ipp16u*)&m_locked_count);
        }

        ~SurfaceHolder()
        {
            std::ignore = vm_interlocked_dec16((volatile Ipp16u*)&m_locked_count);

            m_surface_interface.Release = original_release;

            MFX_STS_TRACE(m_surface_interface.Release(this));
        }

    private:

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

            if (ref_counter > 1)
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

    std::shared_timed_mutex  m_mutex;

    CommonCORE20&            m_core;
    mfxU16                   m_type;
    mfxFrameInfo             m_frame_info;

    std::list<SurfaceHolder> m_cached_surfaces;
};

inline bool Supports20FeatureSet(VideoCORE& core)
{
    bool* core20_interface = reinterpret_cast<bool*>(core.QueryCoreInterface(MFXICORE_API_2_0_GUID));

    return core20_interface && *core20_interface;
}

#endif
