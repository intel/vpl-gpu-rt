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

#ifndef _LIBMFX_ALLOCATOR_H_
#define _LIBMFX_ALLOCATOR_H_

#include "mfxvideo.h"
#include "mfx_common_int.h"

// It is only needed for Synchronize
#include "mfx_session.h"

#include "vm_interlocked.h"

#include <shared_mutex>
#include <atomic>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <algorithm>

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
    virtual mfxStatus CreateSurface(mfxU16 type, const mfxFrameInfo& info, mfxFrameSurface1* & output_surf) = 0;
    virtual mfxStatus ReallocSurface(const mfxFrameInfo& info, mfxMemId id)                                 = 0;
    virtual void      SetDevice(mfxHDL device)                                                              = 0;
    virtual void      Remove(mfxMemId mid)                                                                  = 0;

    // this is actually a WA, which should be removed after Synchronize will be implemented through dependency manager
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

    mfxStatus CreateSurface(mfxU16, const mfxFrameInfo &, mfxFrameSurface1* &) override { return MFX_ERR_UNSUPPORTED; }
    mfxStatus ReallocSurface(const mfxFrameInfo &, mfxMemId )                  override { return MFX_ERR_UNSUPPORTED; }
    void      SetDevice(mfxHDL )                                               override { return; }
    void      Remove(mfxMemId )                                                override { return; }

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

               if (response.NumFrameActual < request.NumFrameMin)
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

    mfxStatus CreateSurface(mfxU16 type, const mfxFrameInfo& info, mfxFrameSurface1* & output_surf)
    {
        if (RequiredHWallocator(type))
        {
            MFX_CHECK(allocator_hw, MFX_ERR_UNSUPPORTED);
        }

        FrameAllocatorBase* allocator = ((type & MFX_MEMTYPE_SYSTEM_MEMORY) || !allocator_hw) ? allocator_sw.get() : allocator_hw.get();
        MFX_CHECK_HDL(allocator);
        MFX_SAFE_CALL(allocator->CreateSurface(type, info, output_surf));

        CacheMid(output_surf->Data.MemId, *allocator);

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

    std::shared_timed_mutex                 m_mutex;
    std::map<mfxMemId, FrameAllocatorBase*> m_mid_to_allocator;
};

template <class T, class U>
class FlexibleFrameAllocator : public FrameAllocatorBase
{
public:
    FlexibleFrameAllocator(mfxHDL device = nullptr, mfxSession session = nullptr)
        // ids across different allocators (SW / HW in one core and in different cores (for simplicity)) shouldn't overlap
        : FrameAllocatorBase(session)
        , m_last_created_mid(m_allocator_num << 16)
        , m_device(device)
        , m_staging_adapter(device)
    {
        std::ignore = m_allocator_num.fetch_add(1, std::memory_order_relaxed);
    }

    mfxStatus Alloc(mfxFrameAllocRequest& request, mfxFrameAllocResponse& response) override
    {
        response = {};

        if (!request.NumFrameSuggested)
            return MFX_ERR_NONE;

        MFX_CHECK(!(request.Type & MFX_MEMTYPE_EXTERNAL_FRAME), MFX_ERR_UNSUPPORTED);

        mfxU16 type = T::AdjustType(request.Type);

        try
        {
            std::vector<mfxMemId> mids(request.NumFrameSuggested);

            std::list<T> alloc_list;
            for (mfxU16 i = 0; i < request.NumFrameSuggested; ++i)
            {
                mids[i] = GenerateMid();

                alloc_list.emplace_back(request.Info, type, mids[i], m_staging_adapter, m_device, request.AllocId, *this);
            }

            std::lock_guard<std::shared_timed_mutex> guard(m_mutex);

            m_allocated_pool.splice(m_allocated_pool.end(), alloc_list);

            m_returned_mids.emplace_back(std::move(mids));

            response.AllocId        = request.AllocId;
            response.mids           = m_returned_mids.back().data();
            response.NumFrameActual = request.NumFrameSuggested;
            response.MemType        = type;
        }
        catch (const mfx::mfxStatus_exception& ex)
        {
            MFX_CHECK_STS(ex.sts);
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
            [mid](const T& surf) { return surf.GetMid() == mid; });

        MFX_CHECK(it != std::end(m_allocated_pool), MFX_ERR_NOT_FOUND);

        MFX_SAFE_CALL(it->Lock(flags));

        it->CopyPointers(frame_data);

        return MFX_ERR_NONE;
    }

    mfxStatus Unlock(mfxMemId mid, mfxFrameData* frame_data) override
    {
        MFX_CHECK_HDL(mid);

        std::shared_lock<std::shared_timed_mutex> guard(m_mutex);

        auto it = std::find_if(std::begin(m_allocated_pool), std::end(m_allocated_pool),
            [mid](const T& surf) { return surf.GetMid() == mid; });

        MFX_CHECK(it != std::end(m_allocated_pool), MFX_ERR_NOT_FOUND);

        MFX_SAFE_CALL(it->Unlock());

        it->CopyPointers(frame_data);

        return MFX_ERR_NONE;
    }

    mfxStatus GetHDL(mfxMemId mid, mfxHDL& handle) const override
    {
        MFX_CHECK_HDL(mid);

        std::shared_lock<std::shared_timed_mutex> guard(m_mutex);

        auto it = std::find_if(std::begin(m_allocated_pool), std::end(m_allocated_pool),
            [mid](const T& surf) { return surf.GetMid() == mid; });

        MFX_CHECK(it != std::end(m_allocated_pool), MFX_ERR_INVALID_HANDLE);

        return it->GetHDL(handle);
    }

    mfxStatus Free(mfxFrameAllocResponse& response) override
    {
        std::list<T> frames_to_erase;

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

            auto it_alloc = std::find_if(std::begin(m_allocated_pool), std::end(m_allocated_pool), [mid](const T& surf) { return surf.GetMid() == mid; });

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

    mfxStatus CreateSurface(mfxU16 type, const mfxFrameInfo& info, mfxFrameSurface1* & output_surf) override
    {
        MFX_CHECK(!(type & MFX_MEMTYPE_EXTERNAL_FRAME), MFX_ERR_UNSUPPORTED);

        try
        {
            std::list<T> alloc_list;

            alloc_list.emplace_back(info, T::AdjustType(type), GenerateMid(), m_staging_adapter, m_device, 0u, *this);

            std::lock_guard<std::shared_timed_mutex> guard(m_mutex);

            m_allocated_pool.splice(m_allocated_pool.end(), alloc_list);

            // Fill mfxFrameSurface1 object and return to user
            output_surf = &m_allocated_pool.back().m_exported_surface;

            return output_surf->FrameInterface->AddRef(output_surf);
        }
        catch (const mfx::mfxStatus_exception& ex)
        {
            MFX_CHECK_STS(ex.sts);
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
            [mid](const T& surf) { return surf.GetMid() == mid; });
        MFX_CHECK(it != std::end(m_allocated_pool), MFX_ERR_NOT_FOUND);

        // Will not reallocate surface which is locked by someone
        MFX_CHECK(!it->Locked(),                    MFX_ERR_LOCK_MEMORY);

        MFX_CHECK(it->ReallocAllowed(info),         MFX_ERR_INVALID_VIDEO_PARAM);

        return it->Realloc(info);
    }

    void Remove(mfxMemId mid) override
    {
        std::lock_guard<std::shared_timed_mutex> guard(m_mutex);

        auto it = std::find_if(std::begin(m_allocated_pool), std::end(m_allocated_pool),
            [mid](const T& surf) { return surf.GetMid() == mid; });

        if (it == std::end(m_allocated_pool))
        {
            std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_FOUND);
            return;
        }

        m_allocated_pool.erase(it);

        std::ignore = std::find_if(std::begin(m_returned_mids), std::end(m_returned_mids),
            [this, mid] (std::vector<mfxMemId> & v_mid)
            {
                auto it = std::find(std::begin(v_mid), std::end(v_mid), mid);

                if (it == std::end(v_mid)) return false;

                *it = ALREADY_REMOVED_MID;

                return true;
            });
    }


    void SetDevice(mfxHDL device) override
    {
        m_device = device;

        m_staging_adapter.SetDevice(device);
    }

private:
    std::atomic<size_t>                    m_last_created_mid = { 0 };
    mfxHDL                                 m_device           = nullptr;

    mutable std::shared_timed_mutex        m_mutex;

    // Do not change order of m_staging_adapter and m_allocated_pool (surfaces destruction has side effect on staging adapter)
    U                                      m_staging_adapter;

    std::list<T>                           m_allocated_pool;  // Pool of allocated surfaces

    std::list<std::vector<mfxMemId>>       m_returned_mids;   // Storage of memory for mids returned to MSDK lib

    const mfxMemId ALREADY_REMOVED_MID = mfxMemId(std::numeric_limits<size_t>::max());

    mfxMemId GenerateMid()
    {
        return mfxMemId(m_last_created_mid.fetch_add(1, std::memory_order_relaxed) + 1);
    }
};

class mfxRefCountableBase
{
public:
    mfxRefCountableBase()
        : m_ref_count(0)
    {}

    virtual ~mfxRefCountableBase()
    {
        if (m_ref_count.load(std::memory_order_relaxed) != 0)
        {
            std::ignore = MFX_STS_TRACE(MFX_ERR_UNKNOWN);
        }
    }

    mfxU32 GetRefCounter() const
    {
        return m_ref_count.load(std::memory_order_relaxed);
    }

    void AddRef()
    {
        std::ignore = m_ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    mfxStatus Release()
    {
        MFX_CHECK(m_ref_count.load(std::memory_order_relaxed), MFX_ERR_UNDEFINED_BEHAVIOR);

        // fetch_sub return value immediately preceding
        if (m_ref_count.fetch_sub(1, std::memory_order_relaxed) - 1 == 0)
        {
            Close();
        }

        return MFX_ERR_NONE;
    }

protected:
    virtual void Close() { return; };

private:
    std::atomic<uint32_t> m_ref_count;
};

class mfxFrameSurfaceBaseInterface : public mfxRefCountableBase
{
public:
    mfxFrameSurfaceBaseInterface(mfxMemId mid, FrameAllocatorBase& allocator)
        : mfxRefCountableBase()
        , m_allocator(allocator)
        , m_mid(mid)
    {}

    virtual mfxStatus                          Lock(mfxU32 flags)      = 0;
    virtual mfxStatus                          Unlock()                = 0;
    virtual std::pair<mfxHDL, mfxResourceType> GetNativeHandle() const = 0;
    virtual std::pair<mfxHDL, mfxHandleType>   GetDeviceHandle() const = 0;

    mfxMemId GetMid() const { return m_mid; }

    // this is actually a WA, which should be removed after Synchronize will be implemented through dependency manager
    mfxStatus Synchronize(mfxU32 timeout)
    {
        return
            m_allocator.Synchronize(m_sp, timeout);
    }

    void SetSyncPoint(mfxSyncPoint Sync)
    {
        m_sp = Sync;
    }

protected:
    void Close() override
    {
        m_allocator.Remove(m_mid);
    }

private:
    FrameAllocatorBase&   m_allocator;
    mfxMemId              m_mid;
    mfxSyncPoint          m_sp        = nullptr;
};

inline void copy_frame_surface_pixel_pointers(mfxFrameData& buf_dst, const mfxFrameData& buf_src)
{
    MFX_COPY_FIELD(PitchLow);
    MFX_COPY_FIELD(PitchHigh);
    MFX_COPY_FIELD(Y);
    MFX_COPY_FIELD(U);
    MFX_COPY_FIELD(V);
    MFX_COPY_FIELD(A);
}

class mfxFrameSurfaceInterfaceImpl : public mfxFrameSurfaceInterface, public mfxFrameSurfaceBaseInterface
{
public:
    mfxFrameSurfaceInterfaceImpl(const mfxFrameInfo & info, mfxU16 type, mfxMemId mid, FrameAllocatorBase& allocator)
        : mfxFrameSurfaceInterface()
        , mfxFrameSurfaceBaseInterface(mid, allocator)
    {
        // Surface interface level
        Context = static_cast<mfxFrameSurfaceBaseInterface*>(this);
        Version.Version = MFX_FRAMESURFACEINTERFACE_VERSION;

        mfxFrameSurfaceInterface::AddRef          = &mfxFrameSurfaceInterfaceImpl::AddRef_impl;
        mfxFrameSurfaceInterface::Release         = &mfxFrameSurfaceInterfaceImpl::Release_impl;
        mfxFrameSurfaceInterface::Map             = &mfxFrameSurfaceInterfaceImpl::Map_impl;
        mfxFrameSurfaceInterface::Unmap           = &mfxFrameSurfaceInterfaceImpl::Unmap_impl;
        mfxFrameSurfaceInterface::GetNativeHandle = &mfxFrameSurfaceInterfaceImpl::GetNativeHandle_impl;
        mfxFrameSurfaceInterface::GetDeviceHandle = &mfxFrameSurfaceInterfaceImpl::GetDeviceHandle_impl;
        mfxFrameSurfaceInterface::GetRefCounter   = &mfxFrameSurfaceInterfaceImpl::GetRefCounter_impl;
        mfxFrameSurfaceInterface::Synchronize     = &mfxFrameSurfaceInterfaceImpl::Synchronize_impl;

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

    static mfxStatus AddRef_impl(mfxFrameSurface1* surface)
    {
        MFX_CHECK_NULL_PTR1(surface);
        MFX_CHECK_HDL(surface->FrameInterface);
        MFX_CHECK_HDL(surface->FrameInterface->Context);

        reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->AddRef();

        return MFX_ERR_NONE;
    }

    static mfxStatus Release_impl(mfxFrameSurface1* surface)
    {
        MFX_CHECK_NULL_PTR1(surface);
        MFX_CHECK_HDL(surface->FrameInterface);
        MFX_CHECK_HDL(surface->FrameInterface->Context);

        return reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->Release();
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

    static mfxStatus GetRefCounter_impl(mfxFrameSurface1* surface, mfxU32* counter)
    {
        MFX_CHECK_NULL_PTR2(surface, counter);
        MFX_CHECK_HDL(surface->FrameInterface);
        MFX_CHECK_HDL(surface->FrameInterface->Context);

        *counter = reinterpret_cast<mfxFrameSurfaceBaseInterface*>(surface->FrameInterface->Context)->GetRefCounter();

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

    // Will be returned to user, to protect original fields of mfxFrameSurface1 from zeroing on user side
    mfxFrameSurface1   m_exported_surface = {};

protected:
    mfxFrameSurface1   m_internal_surface = {};

    mutable std::mutex m_mutex;
};

class mfxSurfaceArrayImpl : public mfxSurfaceArray, public mfxRefCountableBase
{
public:
    static mfxSurfaceArrayImpl* CreateSurfaceArray()
    {
        mfxSurfaceArrayImpl* surfArr = new mfxSurfaceArrayImpl();
        ((mfxSurfaceArray*)surfArr)->AddRef(surfArr);
        return surfArr;
    }

    static mfxStatus AddRef_impl(mfxSurfaceArray* surfArray)
    {
        MFX_CHECK_NULL_PTR1(surfArray);
        MFX_CHECK_HDL(surfArray->Context);

        reinterpret_cast<mfxRefCountableBase*>(surfArray->Context)->AddRef();

        return MFX_ERR_NONE;
    }

    static mfxStatus Release_impl(mfxSurfaceArray* surfArray)
    {
        MFX_CHECK_NULL_PTR1(surfArray);
        MFX_CHECK_HDL(surfArray->Context);

        return reinterpret_cast<mfxRefCountableBase*>(surfArray->Context)->Release();
    }

    static mfxStatus GetRefCounter_impl(mfxSurfaceArray* surfArray, mfxU32* counter)
    {
        MFX_CHECK_NULL_PTR2(surfArray, counter);
        MFX_CHECK_HDL(surfArray->Context);

        *counter = reinterpret_cast<mfxRefCountableBase*>(surfArray->Context)->GetRefCounter();

        return MFX_ERR_NONE;
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

        delete this;
    }

private:
    mfxSurfaceArrayImpl()
        : mfxSurfaceArray()
        , mfxRefCountableBase()
    {
        Context = static_cast<mfxRefCountableBase*>(this);
        Version.Version = MFX_SURFACEARRAY_VERSION;

        mfxSurfaceArray::AddRef        = &mfxSurfaceArrayImpl::AddRef_impl;
        mfxSurfaceArray::Release       = &mfxSurfaceArrayImpl::Release_impl;
        mfxSurfaceArray::GetRefCounter = &mfxSurfaceArrayImpl::GetRefCounter_impl;
    }

    std::vector<mfxFrameSurface1*> m_surfaces;
    std::mutex m_mutex;
};

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

struct mfxFrameSurface1_sw : public RWAcessSurface
{
    mfxFrameSurface1_sw(const mfxFrameInfo & info, mfxU16 type, mfxMemId mid, mfxHDL staging_adapter, mfxHDL device, mfxU32 context, FrameAllocatorBase& allocator);

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

    mfxStatus GetHDL(mfxHDL& handle) const
    {
        handle = reinterpret_cast<mfxHDL>(GetMid());
        return MFX_ERR_NONE;
    }

    mfxStatus Realloc(const mfxFrameInfo & info);

    static mfxU16 AdjustType(mfxU16 type)
    {
        return (type & ~MFX_MEMTYPE_EXTERNAL_FRAME) | MFX_MEMTYPE_INTERNAL_FRAME;
    }

private:
    std::unique_ptr<mfxU8[]> m_data;
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

using FlexibleFrameAllocatorSW = FlexibleFrameAllocator<mfxFrameSurface1_sw, staging_adapter_stub>;


#endif

