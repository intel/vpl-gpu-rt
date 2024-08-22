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

#ifndef _LIBMFX_ALLOCATOR_H_
#define _LIBMFX_ALLOCATOR_H_

#include "mfxvideo.h"
#include "mfx_common_int.h"

// It is only needed for Synchronize
#include "mfx_session.h"

#include "mfxsurfacepool.h"

#include "vm_interlocked.h"

#include <shared_mutex>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <algorithm>

#ifdef MFX_ENABLE_ENCODE_STATS
#include "mfxencodestats.h"
#endif // MFX_ENABLE_ENCODE_STATS

static const size_t BASE_ADDR_ALIGN = 0x1000; // 4k page size alignment
static const size_t BASE_SIZE_ALIGN = 0x1000; // 4k page size alignment

// Internal Allocators
namespace mfxDefaultAllocator
{
    mfxStatus AllocBuffer(mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxMemId *mid);
    mfxStatus LockBuffer(mfxHDL pthis, mfxMemId mid, mfxU8 **ptr);
    mfxStatus UnlockBuffer(mfxHDL pthis, mfxMemId mid);
    mfxStatus FreeBuffer(mfxHDL pthis, mfxMemId mid);

    mfxStatus AllocFrames(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    mfxStatus LockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    mfxStatus GetHDL(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
    mfxStatus UnlockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0);
    mfxStatus FreeFrames(mfxHDL pthis, mfxFrameAllocResponse *response);

    mfxStatus GetSurfaceSizeInBytes(mfxU32 pitch, mfxU32 height, mfxU32 fourCC, mfxU32& nBytes);
    mfxStatus GetNumBytesRequired(const mfxFrameInfo& Info, mfxU32& nbytes, size_t power_of_2_alignment = BASE_SIZE_ALIGN);

    struct BufferStruct
    {
        mfxHDL      allocator;
        mfxU32      id;
        mfxU32      nbytes;
        mfxU16      type;
    };
    struct FrameStruct
    {
        mfxU32          id;
        mfxFrameInfo    info;
    };
}

class mfxWideBufferAllocator
{
public:
    std::vector<mfxDefaultAllocator::BufferStruct*> m_bufHdl;
    mfxWideBufferAllocator(void);
    ~mfxWideBufferAllocator(void);
    mfxBufferAllocator bufferAllocator;
};

class mfxBaseWideFrameAllocator
{
public:
    mfxBaseWideFrameAllocator(mfxU16 type = 0);
    virtual ~mfxBaseWideFrameAllocator();
    mfxFrameAllocator       frameAllocator;
    mfxWideBufferAllocator  wbufferAllocator;
    mfxU32                  NumFrames;
    std::vector<mfxHDL>     m_frameHandles;
    // Type of this allocator
    mfxU16                  type;
};
class mfxWideSWFrameAllocator : public  mfxBaseWideFrameAllocator
{
public:
    mfxWideSWFrameAllocator(mfxU16 type);
    virtual ~mfxWideSWFrameAllocator(void) {};
};

inline void clear_frame_data(mfxFrameData& frame_data) noexcept
{
    frame_data.PitchHigh = frame_data.PitchLow = 0;

    frame_data.U = frame_data.V = frame_data.Y = frame_data.A = nullptr;
}

#if defined (MFX_DEBUG_REFCOUNT)

static class RefcountGlobalRegistry
{
public:
    ~RefcountGlobalRegistry()
    {
        for (auto id : m_object_id)
        {
            printf("\n\nREFCOUNT ERROR: NOT DELETED OBJECT %p\n\n", id);
        }
    }

    void RegisterRefcountObject(void* id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (std::find(std::begin(m_object_id), std::end(m_object_id), id) != std::end(m_object_id))
        {
            printf("\n\nREFCOUNT ERROR: CANNOT RE-REGISTER OBJECT %p\n\n", id);
            return;
        }

        printf("\n\nREFCOUNT NOTIFY: REGISTER OBJECT %p\n\n", id);

        m_object_id.push_back(id);
    }

    void UnregisterRefcountObject(void* id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = std::find(std::begin(m_object_id), std::end(m_object_id), id);
        if (it == std::end(m_object_id))
        {
            printf("\n\nREFCOUNT ERROR: CANNOT DELETE NON-REGISTERED OBJECT %p\n\n", id);
            return;
        }

        printf("\n\nREFCOUNT NOTIFY: UNREGISTER OBJECT %p\n\n", id);

        m_object_id.erase(it);
    }

private:
    std::mutex         m_mutex;
    std::vector<void*> m_object_id;
} g_global_registry;
#endif

class FrameAllocatorWrapper;

class FrameAllocatorBase
{
public:
    FrameAllocatorBase(mfxSession session = nullptr): m_session(session) {}
    virtual ~FrameAllocatorBase() {}
    virtual mfxStatus Alloc(mfxFrameAllocRequest& request, mfxFrameAllocResponse& response)                 = 0;
    virtual mfxStatus Lock(mfxMemId mid, mfxFrameData* frame_data, mfxU32 flags = MFX_MAP_READ_WRITE)       = 0;
    virtual mfxStatus Unlock(mfxMemId mid, mfxFrameData* frame_data)                                        = 0;
    virtual mfxStatus GetHDL(mfxMemId mid, mfxHDL& handle)                                            const = 0;
    virtual mfxStatus Free(mfxFrameAllocResponse& response)                                                 = 0;
    virtual mfxStatus CreateSurface(mfxU16 type, const mfxFrameInfo& info, mfxFrameSurface1* & output_surf,
                                                                          mfxSurfaceHeader* import_surface) = 0;
    virtual mfxStatus ReallocSurface(const mfxFrameInfo& info, mfxMemId id)                                 = 0;
    virtual void      SetDevice(mfxHDL device)                                                              = 0;

    // Currently Synchronize implemented through mfxSyncPoint mechanism
    mfxStatus Synchronize(mfxSyncPoint, mfxU32 /*timeout*/);

    static bool CheckMemoryFlags(mfxU32 flags)
    {
        switch (flags & 0xf)
        {
        case MFX_MAP_READ:
        case MFX_MAP_WRITE:
        case MFX_MAP_READ_WRITE:
            break;
        default:
            return false;
        }

        return ((flags & 0xf0) & ~MFX_MAP_NOWAIT) == 0;
    }

    static std::atomic<std::uint32_t> m_allocator_num;

protected:

    // Surface need access to Remove method from destructor, for allocator state update
    friend class mfxFrameSurfaceBaseInterface;
    virtual void Remove(mfxMemId mid) = 0;

    friend class FrameAllocatorWrapper;
    void SetWrapper(FrameAllocatorWrapper* wrapper)
    {
        // It is assumed that there is only one possible wrapper, so it is ok to reassign without a check here
        m_frame_allocator_wrapper = wrapper;
    }

    FrameAllocatorWrapper*            m_frame_allocator_wrapper = nullptr;
    mfxSession                        m_session;
};

class FrameAllocatorExternal : public FrameAllocatorBase
{
public:
    FrameAllocatorExternal(const mfxFrameAllocator& ext_allocator)
        : allocator(ext_allocator)
    {}

    mfxStatus Alloc(mfxFrameAllocRequest& request, mfxFrameAllocResponse& response) override
    {
        return allocator.Alloc(allocator.pthis, &request, &response);
    }

    mfxStatus Lock(mfxMemId mid, mfxFrameData* frame_data, mfxU32 = MFX_MAP_READ) override
    {
        return allocator.Lock(allocator.pthis, mid, frame_data);
    }

    mfxStatus Unlock(mfxMemId mid, mfxFrameData* frame_data) override
    {
        return allocator.Unlock(allocator.pthis, mid, frame_data);
    }

    mfxStatus GetHDL(mfxMemId mid, mfxHDL& handle) const override
    {
        return allocator.GetHDL(allocator.pthis, mid, &handle);
    }

    mfxStatus Free(mfxFrameAllocResponse& response) override
    {
        return allocator.Free(allocator.pthis, &response);
    }

    mfxStatus CreateSurface(mfxU16, const mfxFrameInfo &, mfxFrameSurface1* &,
                                                            mfxSurfaceHeader*) override { return MFX_ERR_UNSUPPORTED; }
    mfxStatus ReallocSurface(const mfxFrameInfo &, mfxMemId )                  override { return MFX_ERR_UNSUPPORTED; }
    void      SetDevice(mfxHDL )                                               override { return; }
    void      Remove(mfxMemId)                                                 override { return; }

    mfxFrameAllocator* GetExtAllocator() { return &allocator; }

private:
    mfxFrameAllocator allocator;
};

static bool RequiredHWallocator(mfxU16 memtype)
{
    constexpr mfxU16 MFX_MEMTYPE_VIDEO_MASK = MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET;

    return memtype & MFX_MEMTYPE_VIDEO_MASK;
}

class FrameAllocatorWrapper
{
public:
    FrameAllocatorWrapper(bool delayed_allocation)
        : m_delayed_allocation(delayed_allocation)
    {}

    mfxStatus Alloc(mfxFrameAllocRequest& request, mfxFrameAllocResponse& response, bool ext_alloc_hint = false)
    {
        try
        {
            // external allocator
            if (IsExtAllocatorSet() &&
                // Only external frames can go to external allocator
                 ( ((request.Type & MFX_MEMTYPE_EXTERNAL_FRAME)
                // Make 'fake' Alloc call to retrieve memId's of surfaces already allocated by app
                && (request.Type & MFX_MEMTYPE_FROM_DECODE))
                // Some possible forcing of external allocator
                || ext_alloc_hint))
            {

               MFX_SAFE_CALL(allocator_ext->Alloc(request, response));
               // In delay allocate mode, response frame num only need >= 0.
               // Delay allocate mode not work with D3D9, D3D9 will use legacy allocator logical
                if(!m_delayed_allocation && (response.NumFrameActual < request.NumFrameMin))
                {
                    std::ignore = MFX_STS_TRACE(allocator_ext->Free(response));
                    MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
                }

               CacheMids(response, *allocator_ext);
               return MFX_ERR_NONE;
            }
            else
            {
                // Default Allocator is used for internal memory allocation only
                MFX_CHECK(!(request.Type & MFX_MEMTYPE_EXTERNAL_FRAME), MFX_ERR_MEMORY_ALLOC);

                return DefaultAllocFrames(request, response);
            }
        }
        catch (...)
        {
            MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
        }
    }

    mfxStatus Lock(mfxMemId mid, mfxFrameData* frame_data, mfxU32 flags = MFX_MAP_READ_WRITE)
    {
        auto allocator = GetAllocatorByMid(mid);
        MFX_CHECK(allocator, MFX_ERR_LOCK_MEMORY);

        return allocator->Lock(mid, frame_data, flags);
    }

    mfxStatus Unlock(mfxMemId mid, mfxFrameData* frame_data)
    {
        auto allocator = GetAllocatorByMid(mid);
        MFX_CHECK(allocator, MFX_ERR_UNKNOWN);

        return allocator->Unlock(mid, frame_data);
    }

    mfxStatus GetHDL(mfxMemId mid, mfxHDL& handle)
    {
        auto allocator = GetAllocatorByMid(mid);
        MFX_CHECK(allocator, MFX_ERR_UNDEFINED_BEHAVIOR);

        return allocator->GetHDL(mid, handle);
    }

    mfxStatus Free(mfxFrameAllocResponse& response)
    {
        MFX_CHECK_NULL_PTR1(response.mids);

        // We can only Free those surfaces which were allocated inside library, so
        // we shouldn't default to ext allocator
        auto allocator = GetAllocatorByMid(response.mids[0], false);
        MFX_CHECK(allocator, MFX_ERR_UNKNOWN);

        std::unique_lock<std::shared_timed_mutex> lock(m_mutex);

        for (mfxU32 i = 0; i < response.NumFrameActual; ++i)
        {
            m_mid_to_allocator.erase(response.mids[i]);
        }

        lock.unlock();

        return allocator->Free(response);
    }

    mfxStatus CreateSurface(mfxU16 type, const mfxFrameInfo& info, mfxFrameSurface1* & output_surf, mfxSurfaceHeader* import_surface)
    {
        if (RequiredHWallocator(type))
        {
            MFX_CHECK(allocator_hw, MFX_ERR_UNSUPPORTED);
        }

        FrameAllocatorBase* allocator = ((type & MFX_MEMTYPE_SYSTEM_MEMORY) || !allocator_hw) ? allocator_sw.get() : allocator_hw.get();
        MFX_CHECK_HDL(allocator);
        MFX_SAFE_CALL(allocator->CreateSurface(type, info, output_surf, import_surface));

        CacheMid(output_surf->Data.MemId, *allocator);
        // it is required to clean up m_mid_to_allocator when output_surf will be completely deleted
        allocator->SetWrapper(this);

        return MFX_ERR_NONE;
    }

    mfxStatus ReallocSurface(const mfxFrameInfo & info, mfxMemId id)
    {
        // We can't realloc surface from ext allocator, so let's not even try default to it
        auto allocator = GetAllocatorByMid(id, false);
        MFX_CHECK(allocator, MFX_ERR_UNKNOWN);

        return allocator->ReallocSurface(info, id);
    }

    void SetDevice(mfxHDL device)
    {
        if (allocator_hw)
            allocator_hw->SetDevice(device);
    }

    mfxStatus SetFrameAllocator(const mfxFrameAllocator& allocator)
    {
        MFX_CHECK(!IsExtAllocatorSet(), MFX_ERR_UNDEFINED_BEHAVIOR);
        allocator_ext.reset(new FrameAllocatorExternal(allocator));
        return MFX_ERR_NONE;
    }

    bool IsExtAllocatorSet() const
    {
        return !!allocator_ext;
    }

    mfxFrameAllocator* GetExtAllocator()
    {
        if (!IsExtAllocatorSet())
            return nullptr;

        auto ext_alloc = dynamic_cast<FrameAllocatorExternal*>(allocator_ext.get());

        return ext_alloc ? ext_alloc->GetExtAllocator() : nullptr;
    }

    std::unique_ptr<FrameAllocatorBase> allocator_sw;
    std::unique_ptr<FrameAllocatorBase> allocator_hw;
    std::unique_ptr<FrameAllocatorBase> allocator_ext;

private:
    mfxStatus DefaultAllocFrames(mfxFrameAllocRequest& request, mfxFrameAllocResponse& response)
    {
        FrameAllocatorBase* allocator;

        if ((request.Type & MFX_MEMTYPE_SYSTEM_MEMORY) || !allocator_hw)
        {
            allocator = allocator_sw.get();
        }
        else
        {
            allocator = allocator_hw.get();
        }

        mfxStatus sts = allocator->Alloc(request, response);
        MFX_CHECK_STS(sts);

        if (response.NumFrameActual < request.NumFrameMin)
        {
            std::ignore = Free(response);
            MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
        }

        CacheMids(response, *allocator);

        return MFX_ERR_NONE;
    }

    FrameAllocatorBase* GetAllocatorByMid(mfxMemId mid, bool default_to_external = true)
    {
        std::shared_lock<std::shared_timed_mutex> guard(m_mutex);

        if (m_mid_to_allocator.find(mid) == std::end(m_mid_to_allocator))
        {
            // If this surface wasn't allocated through Alloc call (cache miss), it might be external surface passed by user,
            // so it should be handled by external allocator
            return default_to_external ? allocator_ext.get() : nullptr;
        }

        return m_mid_to_allocator[mid];
    }

    void CacheMids(const mfxFrameAllocResponse & response, FrameAllocatorBase& allocator)
    {
        std::lock_guard<std::shared_timed_mutex> guard(m_mutex);
        for (mfxU32 i = 0; i < response.NumFrameActual; ++i)
        {
            m_mid_to_allocator[response.mids[i]] = &allocator;
        }
    }

    void CacheMid(mfxMemId mid, FrameAllocatorBase& allocator)
    {
        std::lock_guard<std::shared_timed_mutex> guard(m_mutex);

        m_mid_to_allocator[mid] = &allocator;
    }

    // To allow access to Remove function
    template <class T, class U> friend class FlexibleFrameAllocator;

    // This function is called when surface is deleted by reducing refcount to zero
    void Remove(mfxMemId mid)
    {
        std::unique_lock<std::shared_timed_mutex> lock(m_mutex);

        m_mid_to_allocator.erase(mid);
    }

    std::shared_timed_mutex                 m_mutex;
    std::map<mfxMemId, FrameAllocatorBase*> m_mid_to_allocator;
    bool                                    m_delayed_allocation;
};

inline mfxU16 AdjustTypeInternal(mfxU16 type)
{
    return (type & ~MFX_MEMTYPE_EXTERNAL_FRAME) | MFX_MEMTYPE_INTERNAL_FRAME;
}
template <class T, class U>
class FlexibleFrameAllocator : public FrameAllocatorBase
{
#define MFX_DETACH_FRAME                                                                         \
    [](T* surface)                                                                               \
    {                                                                                            \
        /* We already removed from allocator's list, no need to update it through destructor */  \
        surface->DetachParentAllocator();                                                        \
                                                                                                 \
        std::ignore = MFX_STS_TRACE(surface->Release());                                         \
    }

    using pT = std::unique_ptr<T, void(*)(T* surface)>;

public:
    FlexibleFrameAllocator(mfxHDL device = nullptr, mfxSession session = nullptr)
        // ids across different allocators (SW / HW in one core and in different cores (for simplicity)) shouldn't overlap
        : FrameAllocatorBase(session)
        // fetch_add returns value prior the increment
        , m_mid_high_part(size_t(m_allocator_num.fetch_add(1, std::memory_order_relaxed) + 1) << m_bits_n_surf)
        , m_mid_low_part_modulo((size_t(1) << m_bits_n_surf) - 1)
        , m_device(device)
        , m_staging_adapter(std::make_shared<U>(device))
    {
    }

    mfxStatus Alloc(mfxFrameAllocRequest& request, mfxFrameAllocResponse& response) override
    {
        response = {};

        if (!request.NumFrameSuggested)
            return MFX_ERR_NONE;

        MFX_CHECK(!(request.Type & MFX_MEMTYPE_EXTERNAL_FRAME), MFX_ERR_UNSUPPORTED);

        mfxU16 type = AdjustTypeInternal(request.Type);

        try
        {
            std::vector<mfxMemId> mids(request.NumFrameSuggested);

            std::list<pT> alloc_list;
            for (mfxU16 i = 0; i < request.NumFrameSuggested; ++i)
            {
                mids[i] = GenerateMid();

                alloc_list.emplace_back(pT(T::Create(request.Info, type, mids[i], m_staging_adapter, m_device, request.AllocId, *this, nullptr), MFX_DETACH_FRAME));
            }

            std::lock_guard<std::shared_timed_mutex> guard(m_mutex);

            m_allocated_pool.splice(m_allocated_pool.end(), alloc_list);

            m_returned_mids.emplace_back(std::move(mids));

            response.AllocId        = request.AllocId;
            response.mids           = m_returned_mids.back().data();
            response.NumFrameActual = request.NumFrameSuggested;
        }
        catch (const std::system_error& ex)
        {
            MFX_CHECK_STS(mfxStatus(ex.code().value()));
        }
        catch (...)
        {
            MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
        }

        return MFX_ERR_NONE;
    }

    mfxStatus Lock(mfxMemId mid, mfxFrameData* frame_data, mfxU32 flags = MFX_MAP_READ) override
    {
        MFX_CHECK_HDL(mid);
        MFX_CHECK(CheckMemoryFlags(flags), MFX_ERR_LOCK_MEMORY);

        std::shared_lock<std::shared_timed_mutex> guard(m_mutex);

        auto it = std::find_if(std::begin(m_allocated_pool), std::end(m_allocated_pool),
            [mid](const pT& surf) { return surf->GetMid() == mid; });

        MFX_CHECK(it != std::end(m_allocated_pool), MFX_ERR_NOT_FOUND);

        MFX_SAFE_CALL((*it)->Lock(flags));

        (*it)->CopyPointers(frame_data);

        return MFX_ERR_NONE;
    }

    mfxStatus Unlock(mfxMemId mid, mfxFrameData* frame_data) override
    {
        MFX_CHECK_HDL(mid);

        std::shared_lock<std::shared_timed_mutex> guard(m_mutex);

        auto it = std::find_if(std::begin(m_allocated_pool), std::end(m_allocated_pool),
            [mid](const pT& surf) { return surf->GetMid() == mid; });

        MFX_CHECK(it != std::end(m_allocated_pool), MFX_ERR_NOT_FOUND);

        MFX_SAFE_CALL((*it)->Unlock());

        (*it)->CopyPointers(frame_data);

        return MFX_ERR_NONE;
    }

    mfxStatus GetHDL(mfxMemId mid, mfxHDL& handle) const override
    {
        MFX_CHECK_HDL(mid);

        std::shared_lock<std::shared_timed_mutex> guard(m_mutex);

        auto it = std::find_if(std::begin(m_allocated_pool), std::end(m_allocated_pool),
            [mid](const pT& surf) { return surf->GetMid() == mid; });

        MFX_CHECK(it != std::end(m_allocated_pool), MFX_ERR_INVALID_HANDLE);

        return (*it)->GetHDL(handle);
    }

    mfxStatus Free(mfxFrameAllocResponse& response) override
    {
        std::list<pT> frames_to_erase;

        std::lock_guard<std::shared_timed_mutex> guard(m_mutex);

        // Clear mids
        auto it_mids = std::find_if(std::begin(m_returned_mids), std::end(m_returned_mids),
            [this, response](const std::vector<mfxMemId> & mids)
                {
                    return mids.size() == response.NumFrameActual
                    && std::equal(response.mids, response.mids + response.NumFrameActual, std::begin(mids),
                        [this](mfxMemId l, mfxMemId r) { return l == r || l == ALREADY_REMOVED_MID || r == ALREADY_REMOVED_MID; });
                });

        MFX_CHECK(it_mids != std::end(m_returned_mids),       MFX_ERR_NOT_FOUND);
        // Partial freeing is not allowed
        MFX_CHECK(it_mids->size() == response.NumFrameActual, MFX_ERR_INVALID_HANDLE);

        mfxStatus sts = MFX_ERR_NONE;

        for (mfxU32 i = 0; i < response.NumFrameActual; ++i)
        {
            // Clear exported pool (if surface is there, MSDK lib might be doing something wrong)
            auto mid = response.mids[i];

            // This mid was already deleted by calling Release (object is deleted when it's refcounter reaches zero)
            if (mid == ALREADY_REMOVED_MID) continue;

            auto it_alloc = std::find_if(std::begin(m_allocated_pool), std::end(m_allocated_pool), [mid](const pT& surf) { return surf->GetMid() == mid; });

            if (it_alloc != std::end(m_allocated_pool))
            {
                frames_to_erase.splice(frames_to_erase.end(), m_allocated_pool, it_alloc);
            }
            else
            {
                sts = MFX_ERR_NOT_FOUND;
            }
        }

        m_returned_mids.erase(it_mids);

        return sts;
    }

    mfxStatus CreateSurface(mfxU16 type, const mfxFrameInfo& info, mfxFrameSurface1* & output_surf, mfxSurfaceHeader* import_surface) override
    {
        MFX_CHECK(!(type & MFX_MEMTYPE_EXTERNAL_FRAME), MFX_ERR_UNSUPPORTED);

        try
        {
            std::list<pT> alloc_list;

            alloc_list.emplace_back(pT(T::Create(info, T::AdjustType(type), GenerateMid(), m_staging_adapter, m_device, 0u, *this, import_surface), MFX_DETACH_FRAME));

            std::lock_guard<std::shared_timed_mutex> guard(m_mutex);

            m_allocated_pool.splice(m_allocated_pool.end(), alloc_list);

            // Fill mfxFrameSurface1 object and return to user
            output_surf = &(m_allocated_pool.back()->m_exported_surface);
        }
        catch (const std::system_error& ex)
        {
            MFX_CHECK_STS(mfxStatus(ex.code().value()));
        }
        catch (...)
        {
            MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
        }

        return MFX_ERR_NONE;
    }

    mfxStatus ReallocSurface(const mfxFrameInfo & info, mfxMemId mid) override
    {
        MFX_CHECK_HDL(mid);

        std::shared_lock<std::shared_timed_mutex> guard(m_mutex);

        auto it = std::find_if(std::begin(m_allocated_pool), std::end(m_allocated_pool),
            [mid](const pT& surf) { return surf->GetMid() == mid; });
        MFX_CHECK(it != std::end(m_allocated_pool), MFX_ERR_NOT_FOUND);

        // Will not reallocate surface which is locked by someone
        MFX_CHECK(!(*it)->Locked(),                 MFX_ERR_LOCK_MEMORY);

        MFX_CHECK((*it)->ReallocAllowed(info),      MFX_ERR_INVALID_VIDEO_PARAM);

        return (*it)->Realloc(info);
    }

    void SetDevice(mfxHDL device) override
    {
        m_device = device;

        m_staging_adapter->SetDevice(device);
    }

protected:
    void Remove(mfxMemId mid) override
    {
        std::lock_guard<std::shared_timed_mutex> guard(m_mutex);

        auto it = std::find_if(std::begin(m_allocated_pool), std::end(m_allocated_pool),
            [mid](const pT& surf) { return surf->GetMid() == mid; });

        if (it == std::end(m_allocated_pool))
        {
            std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_FOUND);
            return;
        }

        // Surface is being deleted after decreasing refcount to zero, no need decrease refcount in destructor of holder
        it->release();

        // Remove surface from mid <-> allocator binding table
        if (m_frame_allocator_wrapper)
            m_frame_allocator_wrapper->Remove(mid);

        m_allocated_pool.erase(it);

        std::ignore = std::find_if(std::begin(m_returned_mids), std::end(m_returned_mids),
            [this, mid](std::vector<mfxMemId>& v_mid)
            {
                auto it = std::find(std::begin(v_mid), std::end(v_mid), mid);

                if (it == std::end(v_mid)) return false;

                *it = ALREADY_REMOVED_MID;

                return true;
            });
    }

private:
    const size_t                           m_bits_n_surf      = 16; // One session can't have more than 2^16 surfaces simultaneously
    const size_t                           m_mid_high_part;
    const size_t                           m_mid_low_part_modulo;
    size_t                                 m_mid_low_part     = 0;
    mfxHDL                                 m_device           = nullptr;

    mutable std::shared_timed_mutex        m_mutex;

    // Do not change order of m_staging_adapter and m_allocated_pool (surfaces destruction has side effect on staging adapter)
    std::shared_ptr<U>                     m_staging_adapter;

    std::list<pT>                          m_allocated_pool;  // Pool of allocated surfaces

    std::list<std::vector<mfxMemId>>       m_returned_mids;   // Storage of memory for mids returned to MSDK lib

    const mfxMemId ALREADY_REMOVED_MID = mfxMemId(std::numeric_limits<size_t>::max());

    // This method always called without m_mutex being locked
    mfxMemId GenerateMid()
    {
        std::lock_guard<std::shared_timed_mutex> guard(m_mutex);

        // Check that pool is not already full
        MFX_CHECK_WITH_THROW_STS(m_allocated_pool.size() <= (m_mid_low_part_modulo + 1), MFX_ERR_MEMORY_ALLOC);

        mfxMemId new_memid;
        // There is only m_mid_low_part_modulo + 1 possible mids within one allocator
        for (size_t i = 0; i < m_mid_low_part_modulo + 1; ++i)
        {
            new_memid = mfxMemId(m_mid_high_part | ((++m_mid_low_part) & m_mid_low_part_modulo));

            if (
                // Check if current mid is already in pool
                std::find_if(std::begin(m_allocated_pool), std::end(m_allocated_pool),
                    [new_memid](const pT& surf) { return surf->GetMid() == new_memid; }) == std::end(m_allocated_pool)
                )
                return new_memid;
        }

        // Couldn't find suitable mid
        MFX_CHECK_WITH_THROW_STS(false, MFX_ERR_MEMORY_ALLOC);
    }

#undef MFX_DETACH_FRAME
};

class mfxFrameSurfaceBaseInterface
    : public mfxRefCountableImpl<mfxFrameSurfaceInterface, mfxFrameSurface1>
{
public:
    mfxFrameSurfaceBaseInterface(mfxMemId mid, FrameAllocatorBase& allocator)
        : m_allocator(&allocator)
        , m_mid(mid)
    {}

    virtual mfxStatus                          Lock(mfxU32 flags)      = 0;
    virtual mfxStatus                          Unlock()                = 0;
    virtual std::pair<mfxHDL, mfxResourceType> GetNativeHandle() const = 0;
    virtual std::pair<mfxHDL, mfxHandleType>   GetDeviceHandle() const = 0;
    virtual mfxStatus Export(const mfxSurfaceHeader& export_header,
                                  mfxSurfaceHeader** exported_surface) = 0;

    mfxMemId GetMid() const { return m_mid; }

    mfxStatus Synchronize(mfxU32 timeout)
    {
        // If allocator is detached, no need to sychronize surface. It is already synchronized
        return m_allocator ? m_allocator->Synchronize(m_sp, timeout) : MFX_ERR_NONE;
    }

    void SetSyncPoint(mfxSyncPoint Sync)
    {
        m_sp = Sync;
    }

    void DetachParentAllocator()
    {
        m_allocator = nullptr;
    }

    mfxSurfacePoolInterface* QueryParentPool()
    {
        if (!m_parent_pool)
            return nullptr;

        if (MFX_STS_TRACE(m_parent_pool->AddRef(m_parent_pool)) != MFX_ERR_NONE)
            return nullptr;

        return m_parent_pool;
    }

    void SetParentPool(mfxSurfacePoolInterface* pool)
    {
        m_parent_pool = pool;
    }

protected:

    void Close() override
    {
        if (m_allocator)
            m_allocator->Remove(m_mid);
    }

private:
    FrameAllocatorBase*      m_allocator;
    mfxMemId                 m_mid;
    mfxSyncPoint             m_sp          = nullptr;
    mfxSurfacePoolInterface* m_parent_pool = nullptr;
};

template <>
struct mfxRefCountableInstance<mfxFrameSurface1>
{
    static mfxRefCountable* Get(mfxFrameSurface1* object)
    { return reinterpret_cast<mfxFrameSurfaceBaseInterface*>(object->FrameInterface->Context); }
};

inline void copy_frame_surface_pixel_pointers(mfxFrameData& buf_dst, const mfxFrameData& buf_src)
{
    MFX_COPY_FIELD_NO_LOG(PitchLow);
    MFX_COPY_FIELD_NO_LOG(PitchHigh);
    MFX_COPY_FIELD_NO_LOG(Y);
    MFX_COPY_FIELD_NO_LOG(U);
    MFX_COPY_FIELD_NO_LOG(V);
    MFX_COPY_FIELD_NO_LOG(A);
}

class mfxFrameSurfaceInterfaceImpl;

class mfxSurfaceBase
    : public mfxRefCountableImpl<mfxSurfaceInterface>
{
public:
    mfxSurfaceBase(const mfxSurfaceHeader& export_header, mfxFrameSurfaceInterfaceImpl* p_base_surface)
        : m_p_base_surface(p_base_surface)
    {
        MFX_CHECK_WITH_THROW_STS(CheckExportFlags(export_header.SurfaceFlags), MFX_ERR_INVALID_VIDEO_PARAM);

        // Surface interface level
        Context         = this;
        Version.Version = MFX_SURFACEINTERFACE_VERSION;
        Header          = export_header;

        mfxSurfaceInterface::Synchronize = &mfxSurfaceBase::Synchronize_impl;
    }

    static mfxStatus Synchronize_impl(mfxSurfaceInterface* ext_surface, mfxU32 timeout)
    {
        MFX_CHECK_NULL_PTR1(ext_surface);
        MFX_CHECK_HDL(ext_surface->Context);

        return
            reinterpret_cast<mfxSurfaceBase*>(ext_surface->Context)->Synchronize(timeout);
    }

    mfxStatus Synchronize(mfxU32 timeout);

    void DetachBaseSurface()
    {
        m_p_base_surface = nullptr;
    }

    mfxFrameSurfaceInterfaceImpl* GetParentSurface()
    {
        return m_p_base_surface;
    }

    // Here we return memory which will be transferred outside, it will be copy of internal fields,
    // so we will be on safe side if user accidently memset this memory, so it protects internal state from accidental change
    virtual mfxSurfaceHeader* GetExport() = 0;

private:

    static bool CheckExportFlags(mfxU32 export_flags)
    {
        return (export_flags == MFX_SURFACE_FLAG_DEFAULT) || (export_flags & (MFX_SURFACE_FLAG_EXPORT_SHARED | MFX_SURFACE_FLAG_EXPORT_COPY));
    }

    void Close() override;

    mfxFrameSurfaceInterfaceImpl* m_p_base_surface = nullptr;

};

class mfxFrameSurfaceInterfaceImpl : public mfxFrameSurfaceBaseInterface
{
public:
    mfxFrameSurfaceInterfaceImpl(const mfxFrameInfo& info, mfxU16 type, mfxMemId mid, FrameAllocatorBase& allocator)
        : mfxFrameSurfaceBaseInterface(mid, allocator)
    {
        // Surface interface level
        Context         = static_cast<mfxFrameSurfaceBaseInterface*>(this);
        Version.Version = MFX_FRAMESURFACEINTERFACE_VERSION;

        mfxFrameSurfaceInterface::Map             = &mfxFrameSurfaceInterfaceImpl::Map_impl;
        mfxFrameSurfaceInterface::Unmap           = &mfxFrameSurfaceInterfaceImpl::Unmap_impl;
        mfxFrameSurfaceInterface::GetNativeHandle = &mfxFrameSurfaceInterfaceImpl::GetNativeHandle_impl;
        mfxFrameSurfaceInterface::GetDeviceHandle = &mfxFrameSurfaceInterfaceImpl::GetDeviceHandle_impl;
        mfxFrameSurfaceInterface::Synchronize     = &mfxFrameSurfaceInterfaceImpl::Synchronize_impl;
        mfxFrameSurfaceInterface::QueryInterface  = &mfxFrameSurfaceInterfaceImpl::QueryInterface_impl;
        mfxFrameSurfaceInterface::Export          = &mfxFrameSurfaceInterfaceImpl::Export_impl;

        // Surface representation
        m_internal_surface.Version.Version = MFX_FRAMESURFACE1_VERSION;

        m_internal_surface.Info           = info;
        m_internal_surface.Data.MemId     = mid;
        m_internal_surface.Data.MemType   = type;
        m_internal_surface.FrameInterface = static_cast<mfxFrameSurfaceInterface*>(this);

        if (!m_internal_surface.Info.BitDepthLuma)
            m_internal_surface.Info.BitDepthLuma = BitDepthFromFourcc(m_internal_surface.Info.FourCC);

        if (!m_internal_surface.Info.BitDepthChroma)
            m_internal_surface.Info.BitDepthChroma = m_internal_surface.Info.BitDepthLuma ? m_internal_surface.Info.BitDepthLuma : BitDepthFromFourcc(m_internal_surface.Info.FourCC);

        if (!m_internal_surface.Info.ChromaFormat)
            m_internal_surface.Info.ChromaFormat = ChromaFormatFromFourcc(m_internal_surface.Info.FourCC);

        m_exported_surface = m_internal_surface;
    }

    static mfxStatus Map_impl(mfxFrameSurface1* surface, mfxU32 flags)
    {
        MFX_CHECK_NULL_PTR1(surface);
        MFX_CHECK_HDL(surface->FrameInterface);
        MFX_CHECK_HDL(surface->FrameInterface->Context);

        MFX_SAFE_CALL(reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->Lock(flags));

        // User may try to Map surface using copy of mfxFrameSurface1 object, so we have to copy internally set pointers
        reinterpret_cast<mfxFrameSurfaceInterfaceImpl*>(surface->FrameInterface->Context)->CopyPointers(&surface->Data);

        return MFX_ERR_NONE;
    }

    static mfxStatus Unmap_impl(mfxFrameSurface1* surface)
    {
        MFX_CHECK_NULL_PTR1(surface);
        MFX_CHECK_HDL(surface->FrameInterface);
        MFX_CHECK_HDL(surface->FrameInterface->Context);

        MFX_SAFE_CALL(reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->Unlock());

        // User may try to Map surface using copy of mfxFrameSurface1 object, so we have to copy internally set pointers
        // (in Unmap case it means that data pointers will be zeroed if surface becomes unmapped)
        reinterpret_cast<mfxFrameSurfaceInterfaceImpl*>(surface->FrameInterface->Context)->CopyPointers(&surface->Data);

        return MFX_ERR_NONE;
    }

    static mfxStatus GetNativeHandle_impl(mfxFrameSurface1* surface, mfxHDL* resource, mfxResourceType* resource_type)
    {
        MFX_CHECK_NULL_PTR3(surface, resource, resource_type);
        MFX_CHECK_HDL(surface->FrameInterface);
        MFX_CHECK_HDL(surface->FrameInterface->Context);

        std::tie(*resource, *resource_type) = reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->GetNativeHandle();
        MFX_CHECK(*resource, MFX_ERR_UNSUPPORTED);

        return MFX_ERR_NONE;
    }

    static mfxStatus GetDeviceHandle_impl(mfxFrameSurface1* surface, mfxHDL* device_handle, mfxHandleType* device_type)
    {
        MFX_CHECK_NULL_PTR3(surface, device_handle, device_type);
        MFX_CHECK_HDL(surface->FrameInterface);
        MFX_CHECK_HDL(surface->FrameInterface->Context);

        std::tie(*device_handle, *device_type) = reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->GetDeviceHandle();
        MFX_CHECK(*device_handle, MFX_ERR_UNSUPPORTED);

        return MFX_ERR_NONE;
    }

    static mfxStatus Synchronize_impl(mfxFrameSurface1* surface, mfxU32 timeout)
    {
        MFX_CHECK_NULL_PTR1(surface);
        MFX_CHECK_HDL(surface->FrameInterface);
        MFX_CHECK_HDL(surface->FrameInterface->Context);

        return
            reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->Synchronize(timeout);
    }

    static mfxStatus QueryInterface_impl(mfxFrameSurface1* surface, mfxGUID guid, mfxHDL* iface)
    {
        MFX_CHECK_NULL_PTR2(surface, iface);
        MFX_CHECK_HDL(surface->FrameInterface);
        MFX_CHECK_HDL(surface->FrameInterface->Context);

        if (guid == MFX_GUID_SURFACE_POOL)
        {
            *iface = reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->QueryParentPool();

            MFX_CHECK(*iface, MFX_ERR_NOT_INITIALIZED);

            return MFX_ERR_NONE;
        }

        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    static mfxStatus Export_impl(mfxFrameSurface1* surface, mfxSurfaceHeader export_header, mfxSurfaceHeader** exported_surface)
    {
        MFX_CHECK_NULL_PTR1(surface);
        MFX_CHECK_HDL(surface->FrameInterface);
        MFX_CHECK_HDL(surface->FrameInterface->Context);

        // This is virtual function call, so it will be dispatched to Export function defined in current child class
        return reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->Export(export_header, exported_surface);
    }

    virtual mfxStatus CreateExportSurface(const mfxSurfaceHeader& /*export_header*/, mfxSurfaceBase*& /*exported_surface*/)
    {
        MFX_RETURN(MFX_ERR_NOT_IMPLEMENTED);
    }

    bool ReallocAllowed(const mfxFrameInfo& frame_info) const
    {
        mfxU16 bitdepth_luma   = frame_info.BitDepthLuma   ? frame_info.BitDepthLuma   : BitDepthFromFourcc(frame_info.FourCC);
        mfxU16 bitdepth_chroma = frame_info.BitDepthChroma ? frame_info.BitDepthChroma : BitDepthFromFourcc(frame_info.FourCC);
        mfxU16 chroma_format   = frame_info.ChromaFormat   ? frame_info.ChromaFormat   : ChromaFormatFromFourcc(frame_info.FourCC);

        bool realloc_allowed = frame_info.FourCC == m_internal_surface.Info.FourCC
                            && bitdepth_luma     == m_internal_surface.Info.BitDepthLuma
                            && bitdepth_chroma   == m_internal_surface.Info.BitDepthChroma
                            && frame_info.Shift  == m_internal_surface.Info.Shift
                            && chroma_format     == m_internal_surface.Info.ChromaFormat;

        return realloc_allowed;
    }

    void CopyPointers(mfxFrameData* frame_data) const
    {
        if (!frame_data)
            return;

        copy_frame_surface_pixel_pointers(*frame_data, m_internal_surface.Data);
    }

    void DetachExported(mfxSurfaceBase* exp_surface)
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        auto it = std::find_if(std::begin(m_converted_surfaces_for_export), std::end(m_converted_surfaces_for_export),
            [exp_surface](const pExpSurf& p_exported_surf)
            {
                return p_exported_surf.get() == exp_surface;
            });

        if (it == std::end(m_converted_surfaces_for_export))
        {
            std::ignore = MFX_STS_TRACE(MFX_WRN_OUT_OF_RANGE);
            return;
        }

        // This function is called from exported surface destructor, so no need to decrease reference here, object is already being deleted
        it->release();

        m_converted_surfaces_for_export.erase(it);
    }

    // Will be returned to user, to protect original fields of mfxFrameSurface1 from zeroing on user side
    mfxFrameSurface1   m_exported_surface = {};

protected:

#define MFX_RELEASE_EXPORTED                                   \
    [](mfxSurfaceBase* surface)                                \
    {                                                          \
        /* Surface is being deleted in destructor of container,
           so no need to recursevely update it's content */    \
        surface->DetachBaseSurface();                          \
                                                               \
        std::ignore = MFX_STS_TRACE(surface->Release());       \
    }

    using pExpSurf = std::unique_ptr<mfxSurfaceBase, void(*)(mfxSurfaceBase* surface)>;

    mfxStatus Export(const mfxSurfaceHeader& export_header, mfxSurfaceHeader** exported_surface) override
    {

        MFX_CHECK_NULL_PTR1(exported_surface);

        std::lock_guard<std::mutex> guard(m_mutex);

        /*
        if ((export_header.SurfaceFlags == MFX_SURFACE_FLAG_DEFAULT) || (export_header.SurfaceFlags & MFX_SURFACE_FLAG_EXPORT_SHARED))
        {
            // First check if we already exported current surface to this type
            auto it = std::find_if(std::begin(m_converted_surfaces_for_export), std::end(m_converted_surfaces_for_export),
                [&export_header](const pExpSurf& p_exported_surf)
                {
                        // Check that surface is of same type
                    return (export_header.SurfaceType == p_exported_surf->Header.SurfaceType)
                        // and export flags are compatible:
                        // only shared (no-copy) export is allowed for reexport (returning same object to user)
                        && (p_exported_surf->Header.SurfaceFlags & MFX_SURFACE_FLAG_EXPORT_SHARED);
                });

            if (it != std::end(m_converted_surfaces_for_export))
            {
                p_exported_surf->AddRef();
                *exported_surface = (*it)->GetExport();
                return MFX_ERR_NONE;
            }
        }

        // Need to create new export surface
        */

        // Here we go into overload of Export in derived class, here actual exported surface is created
        mfxSurfaceBase* tmp_exported_surface = nullptr;
        MFX_SAFE_CALL(this->CreateExportSurface(export_header, tmp_exported_surface));

        MFX_CHECK_NULL_PTR1(tmp_exported_surface);

        m_converted_surfaces_for_export.emplace_back(pExpSurf(tmp_exported_surface, MFX_RELEASE_EXPORTED));

        *exported_surface = tmp_exported_surface->GetExport();

        return MFX_ERR_NONE;
    }

    mfxFrameSurface1    m_internal_surface = {};

    std::list<pExpSurf> m_converted_surfaces_for_export;

    mutable std::mutex  m_mutex;

#undef MFX_RELEASE_EXPORTED
};

class mfxSurfaceArrayImpl;
template <>
struct mfxRefCountableInstance<mfxSurfaceArray>
{
    static mfxRefCountable* Get(mfxSurfaceArray* object)
    { return reinterpret_cast<mfxRefCountable*>(object->Context); }
};

class mfxSurfaceArrayImpl : public mfxRefCountableImpl<mfxSurfaceArray>
{
public:
    static mfxSurfaceArrayImpl* Create()
    {
        mfxSurfaceArrayImpl* surfArr = new mfxSurfaceArrayImpl();
        surfArr->AddRef();
        return surfArr;
    }

    void AddSurface(mfxFrameSurface1* surface)
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        m_surfaces.push_back(surface);

        if (surface)
            std::ignore = MFX_STS_TRACE(AddRefSurface(*surface));

        Surfaces    =         m_surfaces.data();
        NumSurfaces = (mfxU32)m_surfaces.size();
    }

protected:
    void Close() override
    {
        for (auto surface : m_surfaces)
        {
            if (surface)
                std::ignore = MFX_STS_TRACE(ReleaseSurface(*surface));
        }
    }

private:
    mfxSurfaceArrayImpl()
    {
        Context = static_cast<mfxRefCountable*>(this);
        Version.Version = MFX_SURFACEARRAY_VERSION;
    }

    std::vector<mfxFrameSurface1*> m_surfaces;
    std::mutex m_mutex;
};

#ifdef MFX_ENABLE_ENCODE_STATS
class mfxEncodeStatsContainerImpl : public mfxRefCountableImpl<mfxEncodeStatsContainer>
{
public:
    mfxStatus AllocFrameStatsBuf()
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        if (this->EncodeFrameStats == nullptr)
        {
            this->EncodeFrameStats = new mfxEncodeFrameStats{};

            MFX_CHECK(this->EncodeFrameStats, MFX_ERR_MEMORY_ALLOC);
        }
        else
        {
            *this->EncodeFrameStats = {};
        }

        return MFX_ERR_NONE;
    }

    mfxStatus AllocBlkStatsBuf(mfxU32 numBlk)
    {
        MFX_CHECK(numBlk, MFX_ERR_INVALID_VIDEO_PARAM);

        std::lock_guard<std::mutex> guard(m_mutex);

        if (this->EncodeBlkStats == nullptr)
        {
            this->EncodeBlkStats = new mfxEncodeBlkStats{};
            MFX_CHECK(this->EncodeBlkStats, MFX_ERR_MEMORY_ALLOC);
        }

        MFX_CHECK_STS(AllocBlkStatsArray(numBlk));

        return MFX_ERR_NONE;
    }

protected:
    template <typename T>
    static void Delete(T*& p)
    {
        if (p)
        {
            delete p;
            p = nullptr;
        }
    }

    template <typename T>
    static void DeleteArray(T*& p)
    {
        if (p)
        {
            delete[] p;
            p = nullptr;
        }
    }

    template <typename T>
    static mfxStatus AllocBlkStatsBuf(mfxU32 numBlkIn, mfxU32& numBlkOut, T*& buf)
    {
        if (numBlkIn <= numBlkOut && buf)
        {
            for (mfxU32 i = 0; i < numBlkOut; i++)
            {
                buf[i] = {};
            }

            return MFX_ERR_NONE;
        }

        if (numBlkOut)
        {
            DeleteArray(buf);
        }

        buf = new T[numBlkIn]{};
        MFX_CHECK(buf, MFX_ERR_MEMORY_ALLOC);
        numBlkOut = numBlkIn;

        return MFX_ERR_NONE;
    }

    virtual mfxStatus AllocBlkStatsArray(mfxU32 numBlk) = 0;

    virtual void DetroyBlkStatsArray() = 0;

    void Close() override
    {
        if (this->EncodeFrameStats)
        {
            Delete(this->EncodeFrameStats);
        }

        if (this->EncodeBlkStats)
        {
            DetroyBlkStatsArray();
            Delete(this->EncodeBlkStats);
        }
    }

protected:
    mfxEncodeStatsContainerImpl()
    {
        Version.Version      = MFX_ENCODESTATSCONTAINER_VERSION;
        RefInterface.Context = static_cast<mfxRefCountable*>(this);
    }

    std::mutex m_mutex;
};
#endif // MFX_ENABLE_ENCODE_STATS

class RWAcessSurface : public mfxFrameSurfaceInterfaceImpl
{
public:
    RWAcessSurface(const mfxFrameInfo & info, mfxU16 type, mfxMemId mid, FrameAllocatorBase& allocator)
        : mfxFrameSurfaceInterfaceImpl(info, type, mid, allocator)
    {}

    mfxStatus LockRW(std::unique_lock<std::mutex>& guard, bool write, bool nowait);
    mfxStatus UnlockRW();

    // Functions below should be called from thread-safe context
    mfxU32 NumReaders() const { return m_read_locks; }
    bool   Locked()     const { return m_write_lock || m_read_locks != 0; }

private:

    // This class provides shared access to read (with possible wait if write lock was acquired and MFX_MAP_NOWAIT wasn't set)
    // and exclusive write access with immediate return if read lock was acquired

    std::condition_variable m_wait_before_read;
    mfxU32                  m_read_locks = 0u;
    bool                    m_write_lock = false;
};


template <>
struct mfxRefCountableInstance<mfxSurfaceInterface>
{
    static mfxRefCountable* Get(mfxSurfaceInterface* object)
    {
        return reinterpret_cast<mfxRefCountable*>(object->Context);
    }
};

template<class SurfaceType>
class mfxSurfaceImpl : public mfxSurfaceBase, public SurfaceType
{
public:
    mfxSurfaceImpl(const mfxSurfaceHeader& export_header, mfxFrameSurfaceInterfaceImpl* p_base_surface)
        : mfxSurfaceBase(export_header, p_base_surface)
        , SurfaceType()
    {
        mfxSurfaceInterface::Header.StructSize = sizeof(SurfaceType);
        SurfaceType::SurfaceInterface = *(static_cast<mfxSurfaceInterface*>(this));
    }

    mfxSurfaceHeader* GetExport() override
    {
        m_surface_for_export = *(static_cast<SurfaceType*>(this));

        return &m_surface_for_export.SurfaceInterface.Header;
    }

protected:
    void SetResultedExportType(mfxU32 export_type)
    {
        mfxSurfaceInterface::Header.SurfaceFlags = SurfaceType::SurfaceInterface.Header.SurfaceFlags = export_type;
    }

    SurfaceType m_surface_for_export = {};

};

// This stub used for allocators which don't need staging surfaces
class staging_adapter_stub
{
public:
    staging_adapter_stub(mfxHDL = nullptr)
    {}

    operator mfxHDL() const { return nullptr; }

    void SetDevice(mfxHDL)
    {}
};

struct mfxFrameSurface1_sw : public RWAcessSurface
{
    static mfxFrameSurface1_sw* Create(const mfxFrameInfo& info, mfxU16 type, mfxMemId mid, std::shared_ptr<staging_adapter_stub>& staging_adapter, mfxHDL device, mfxU32 context, FrameAllocatorBase& allocator,
        mfxSurfaceHeader* import_surface)
    {
        // Import of SW surfaces is not supported right now
        MFX_CHECK_WITH_THROW_STS(!import_surface, MFX_ERR_UNSUPPORTED);

        auto surface = new mfxFrameSurface1_sw(info, type, mid, staging_adapter, device, context, allocator);
        surface->AddRef();
        return surface;
    }

    ~mfxFrameSurface1_sw()
    {
        // Unmap surface if it is still mapped
        while (Locked())
        {
            if (MFX_FAILED(Unlock()))
                break;
        }
    }

    mfxStatus                          Lock(mfxU32 flags)      override;
    mfxStatus                          Unlock()                override;
    std::pair<mfxHDL, mfxResourceType> GetNativeHandle() const override { return { nullptr, mfxResourceType(0) }; }
    std::pair<mfxHDL, mfxHandleType>   GetDeviceHandle() const override { return { nullptr, mfxHandleType(0)   }; }
    mfxStatus Export(const mfxSurfaceHeader&, mfxSurfaceHeader**) override
    {
        MFX_RETURN(MFX_ERR_NOT_IMPLEMENTED);
    }

    mfxStatus GetHDL(mfxHDL& handle) const
    {
        handle = reinterpret_cast<mfxHDL>(GetMid());
        return MFX_ERR_NONE;
    }

    mfxStatus Realloc(const mfxFrameInfo & info);

    static mfxU16 AdjustType(mfxU16 type)
    {
        return AdjustTypeInternal(type);
    }

protected:
    mfxFrameSurface1_sw(const mfxFrameInfo& info, mfxU16 type, mfxMemId mid, std::shared_ptr<staging_adapter_stub>& staging_adapter, mfxHDL device, mfxU32 context, FrameAllocatorBase& allocator);

    std::unique_ptr<mfxU8, void(*)(void*)> m_data;
};

using FlexibleFrameAllocatorSW = FlexibleFrameAllocator<mfxFrameSurface1_sw, staging_adapter_stub>;

const size_t MFX_MAX_NUM_COLOR_PLANES = 4;

using uniq_ptr_mfx_shared_lib_holder = std::unique_ptr<mfx::mfx_shared_lib_holder>;

class ImportExportHelper : private std::map<mfxSurfaceType, uniq_ptr_mfx_shared_lib_holder>
{
public:

    mfx::mfx_shared_lib_holder* GetHelper(mfxSurfaceType shared_library_type);

    // Write a specialization for desired convertion in dedicated source code file
    template <mfxSurfaceType SharedLibType>
    static uniq_ptr_mfx_shared_lib_holder LoadAndInit();

private:
    std::mutex m_mutex;

};

#endif

