// Copyright (c) 2011-2024 Intel Corporation
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


#ifndef _LIBMFX_ALLOCATOR_VAAPI_H_
#define _LIBMFX_ALLOCATOR_VAAPI_H_

#include <va/va.h>
#include <unistd.h>

#include "mfxvideo++int.h"
#include "libmfx_allocator.h"


// VAAPI Allocator internal Mem ID
struct vaapiMemIdInt
{
    VASurfaceID* m_surface;
    VAImage      m_image;
    unsigned int m_fourcc;
};

// Internal Allocators 
namespace mfxDefaultAllocatorVAAPI
{
    mfxStatus AllocFramesHW(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    mfxStatus LockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    mfxStatus GetHDLHW(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
    mfxStatus UnlockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr = nullptr);
    mfxStatus FreeFramesHW(mfxHDL pthis, mfxFrameAllocResponse *response);
    mfxStatus ReallocFrameHW(mfxHDL pthis, mfxFrameSurface1 *surf, VASurfaceID *va_surf);

    mfxStatus SetFrameData(const VAImage &va_image, mfxU32 mfx_fourcc, mfxU8* p_buffer, mfxFrameData& frame_data);

    class mfxWideHWFrameAllocator : public  mfxBaseWideFrameAllocator
    {
    public:
        mfxWideHWFrameAllocator(mfxU16 type, mfxHDL handle);
        virtual ~mfxWideHWFrameAllocator(void){};

        VADisplay* m_pVADisplay;

        mfxU32 m_DecId;

        std::vector<VASurfaceID>   m_allocatedSurfaces;
        std::vector<vaapiMemIdInt> m_allocatedMids;
    };

} //  namespace mfxDefaultAllocatorVAAPI

class SurfaceScopedLock
{
public:

    SurfaceScopedLock(VADisplay disp, VASurfaceID& surface_id)
        : m_display(disp)
        , m_surface_id(surface_id)
    {}

    ~SurfaceScopedLock()
    {
        if (m_mapped)        std::ignore = MFX_STS_TRACE(Unmap());
        if (m_image_created) std::ignore = MFX_STS_TRACE(DestroyImage());
    }

    mfxStatus DeriveImage()
    {
        MFX_CHECK(!m_image_created, MFX_ERR_UNDEFINED_BEHAVIOR);
        {
            PERF_UTILITY_AUTO("vaDeriveImage", PERF_LEVEL_DDI);
            VAStatus va_sts = vaDeriveImage(m_display, m_surface_id, &m_image);
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);
        }

        m_image_created = true;

        return MFX_ERR_NONE;
    }

    // add map type when vaMapBuffer will support it
    mfxStatus Map(mfxU8* & ptr/*, MAP_TYPE*/)
    {
        ptr = nullptr;

        MFX_CHECK(m_image_created, MFX_ERR_LOCK_MEMORY);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
            PERF_UTILITY_AUTO("vaMapBuffer", PERF_LEVEL_DDI);
            VAStatus va_sts = vaMapBuffer(m_display, m_image.buf, (void **)&ptr);
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);
        }

        m_mapped = true;

        return MFX_ERR_NONE;
    }

    mfxStatus Unmap()
    {
        MFX_CHECK(m_image_created, MFX_ERR_NOT_INITIALIZED);
        MFX_CHECK(m_mapped, MFX_ERR_UNDEFINED_BEHAVIOR);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
            PERF_UTILITY_AUTO("vaUnmapBuffer", PERF_LEVEL_DDI);
            VAStatus va_sts = vaUnmapBuffer(m_display, m_image.buf);
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);
        }

        m_mapped = false;

        return MFX_ERR_NONE;
    }

    mfxStatus DestroyImage()
    {
        MFX_CHECK(m_image_created, MFX_ERR_NOT_INITIALIZED);
        MFX_CHECK(!m_mapped, MFX_ERR_UNKNOWN);
        {
            PERF_UTILITY_AUTO("vaUnmapBuffer", PERF_LEVEL_DDI);
            VAStatus va_sts = vaDestroyImage(m_display, m_image.image_id);
            MFX_CHECK(VA_STATUS_SUCCESS == va_sts, MFX_ERR_DEVICE_FAILED);
        }

        m_image_created = false;

        return MFX_ERR_NONE;
    }

    VAImage           m_image = {};

private:
    bool              m_image_created = false;
    bool              m_mapped        = false;

    VADisplay         m_display;
    VASurfaceID&      m_surface_id;
};

class VADisplayWrapper : public std::enable_shared_from_this<VADisplayWrapper>
{
public:
    VADisplayWrapper(VADisplay dpy, int fdDRM = -1)
        : m_display(dpy)
        , m_fdDRM(fdDRM)
    {}

    ~VADisplayWrapper()
    {
        if (m_fdDRM != -1)
        {
            std::ignore = MFX_STS_TRACE(vaTerminate(m_display));
            std::ignore = MFX_STS_TRACE(close(m_fdDRM));
        }
    }

    VADisplayWrapper(const VADisplayWrapper&)  = delete;
    VADisplayWrapper(VADisplayWrapper&& other) = default;

    VADisplayWrapper& operator= (const VADisplayWrapper&)  = delete;
    VADisplayWrapper& operator= (VADisplayWrapper&& other) = default;

    operator VADisplay() const { return m_display; }

private:
    VADisplay m_display;
    int       m_fdDRM = -1;
};

class vaapi_resource_wrapper
{
public:
    vaapi_resource_wrapper(VADisplayWrapper& display)
        : m_resource_id(VA_INVALID_ID), m_pVADisplay(display.shared_from_this())
    {}

    virtual mfxStatus Lock(mfxFrameData& frame_data, mfxU32 flags) = 0;
    virtual mfxStatus Unlock()                                     = 0;
    virtual ~vaapi_resource_wrapper() {};

    virtual mfxStatus Export(const mfxSurfaceHeader& /*export_header*/, mfxSurfaceBase*& /*exported_surface*/,
        mfxFrameSurfaceInterfaceImpl* /*p_base_surface*/)
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    VAGenericID*      GetHandle() { return &m_resource_id; }
    VADisplayWrapper& GetDevice() { return *m_pVADisplay;  }

protected:
    VAGenericID                       m_resource_id;
    std::shared_ptr<VADisplayWrapper> m_pVADisplay;
};

class vaapi_buffer_wrapper : public vaapi_resource_wrapper
{
public:
    vaapi_buffer_wrapper(const mfxFrameInfo &info, VADisplayWrapper& display, mfxU32 context);
    ~vaapi_buffer_wrapper();
    virtual mfxStatus Lock(mfxFrameData& frame_data, mfxU32 flags) override;
    virtual mfxStatus Unlock()                                     override;

private:
    bool   m_bIsSegmap;

    mfxU32 m_pitch;
};

class vaapi_surface_wrapper : public vaapi_resource_wrapper
{
public:
    vaapi_surface_wrapper(const mfxFrameInfo &info, mfxU16 type, VADisplayWrapper& display, mfxSurfaceHeader* import_surface);
    ~vaapi_surface_wrapper();
    virtual mfxStatus Lock(mfxFrameData& frame_data, mfxU32 flags) override;
    virtual mfxStatus Unlock()                                     override;

    virtual mfxStatus Export(const mfxSurfaceHeader& export_header, mfxSurfaceBase*& exported_surface, mfxFrameSurfaceInterfaceImpl* p_base_surface) override;
    std::shared_ptr<ImportExportHelper>& GetCreateHelper()
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        if (!m_import_export_helper)
            m_import_export_helper.reset(new ImportExportHelper());

        return m_import_export_helper;
    }

private:
    std::pair<mfxStatus, bool> TryImportSurface     (const mfxFrameInfo& info, mfxSurfaceHeader* import_surface);
    std::pair<mfxStatus, bool> TryImportSurfaceVAAPI(const mfxFrameInfo& info, mfxSurfaceVAAPI&  import_surface);

    mfxStatus CopyImportSurface     (const mfxFrameInfo& info, mfxSurfaceHeader* import_surface);
    mfxStatus CopyImportSurfaceVAAPI(const mfxFrameInfo& info, mfxSurfaceVAAPI&  import_surface);


    SurfaceScopedLock m_surface_lock;
    // If m_imported == true we will not delete vaapi surface in destructor
    bool                                m_imported = false;
    mfxU16                              m_type;
    mfxU32                              m_fourcc;
    std::mutex                          m_mutex;
    std::shared_ptr<ImportExportHelper> m_import_export_helper;

};

struct mfxFrameSurface1_hw_vaapi : public RWAcessSurface
{
    static mfxFrameSurface1_hw_vaapi* Create(const mfxFrameInfo& info, mfxU16 type, mfxMemId mid, std::shared_ptr<staging_adapter_stub>& stg_adapter, mfxHDL display, mfxU32 context, FrameAllocatorBase& allocator,
        mfxSurfaceHeader* import_surface)
    {
        auto surface = new mfxFrameSurface1_hw_vaapi(info, type, mid, stg_adapter, display, context, allocator, import_surface);
        surface->AddRef();
        return surface;
    }

    ~mfxFrameSurface1_hw_vaapi()
    {
        // Unmap surface if it is still mapped
        while (Locked())
        {
            if (MFX_FAILED(Unlock()))
                break;
        }
    }

    mfxStatus Lock(mfxU32 flags)                               override;
    mfxStatus Unlock()                                         override;
    std::pair<mfxHDL, mfxResourceType> GetNativeHandle() const override;
    std::pair<mfxHDL, mfxHandleType>   GetDeviceHandle() const override;

    mfxStatus CreateExportSurface(const mfxSurfaceHeader& export_header, mfxSurfaceBase*& exported_surface) override
    {
        //mfxFrameSurfaceInterfaceImpl
        switch (export_header.SurfaceType)
        {
        case MFX_SURFACE_TYPE_VAAPI:
            MFX_CHECK_HDL(m_resource_wrapper);

            MFX_RETURN(m_resource_wrapper->Export(export_header, exported_surface, this));
        default:
            MFX_RETURN(MFX_ERR_UNSUPPORTED);
        }
    }

    mfxStatus GetHDL(mfxHDL& handle) const;
    mfxStatus Realloc(const mfxFrameInfo & info);

    static mfxU16 AdjustType(mfxU16 type) { return mfxFrameSurface1_sw::AdjustType(type); }

private:
    mfxFrameSurface1_hw_vaapi(const mfxFrameInfo& info, mfxU16 type, mfxMemId mid, std::shared_ptr<staging_adapter_stub>& stg_adapter, mfxHDL display, mfxU32 context, FrameAllocatorBase& allocator,
                              mfxSurfaceHeader* import_surface);

    mutable std::shared_timed_mutex         m_hdl_mutex;

    mfxU16                                  m_type;
    VAContextID                             m_context;
    std::unique_ptr<vaapi_resource_wrapper> m_resource_wrapper;
};

using FlexibleFrameAllocatorHW_VAAPI = FlexibleFrameAllocator<mfxFrameSurface1_hw_vaapi, staging_adapter_stub>;

class mfxSurfaceVAAPIImpl
    : public mfxSurfaceImpl<mfxSurfaceVAAPI>
{
public:

    static mfxSurfaceVAAPIImpl* Create(const mfxSurfaceHeader& export_header, mfxFrameSurfaceInterfaceImpl* p_base_surface, std::shared_ptr<VADisplayWrapper>& display, VASurfaceID surface_id)
    {
        auto surface = new mfxSurfaceVAAPIImpl(export_header, p_base_surface, display, surface_id);
        surface->AddRef();
        return surface;
    }

    ~mfxSurfaceVAAPIImpl();

private:
    mfxSurfaceVAAPIImpl(const mfxSurfaceHeader& export_header, mfxFrameSurfaceInterfaceImpl* p_base_surface, std::shared_ptr<VADisplayWrapper>& display, VASurfaceID surface_id);

    VASurfaceID                       m_surface_id = VA_INVALID_ID;
    std::shared_ptr<VADisplayWrapper> m_pVADisplay;

};

#endif // LIBMFX_ALLOCATOR_VAAPI_H_
/* EOF */
