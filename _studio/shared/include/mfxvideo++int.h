// Copyright (c) 2018-2024 Intel Corporation
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

#ifndef __MFXVIDEOPLUSPLUS_INTERNAL_H
#define __MFXVIDEOPLUSPLUS_INTERNAL_H

#include "mfxvideo.h"
#include "mfxstructures-int.h"
#include <mfx_task_threading_policy.h>
#include <mfx_interface.h>
#include "mfxstructurespro.h"
#include "mfxmvc.h"
#include "mfxsvc.h"
#include "mfxjpeg.h"
#include "mfxvp8.h"
#include <memory>
#include <functional>

#include "mfx_utils.h"

#ifdef _MSVC_LANG
#pragma warning(push)
#pragma warning(disable:26812)
#endif

#include "va/va.h"

// Helper struct VaGuidMapper is placed _studio/shared/include/libmfx_core_vaapi.h for use linux/android GUIDs
// Pack VAEntrypoint and VAProfile into GUID data structure
#define DEFINE_GUID_VA(name, profile, entrypoint) \
    static const GUID name = { profile, entrypoint >> 16, entrypoint & 0xffff, {} }

/* H.264/AVC Enc */
DEFINE_GUID_VA(DXVA2_Intel_Encode_AVC ,                      VAProfileH264High,      VAEntrypointEncSlice);
DEFINE_GUID_VA(DXVA2_INTEL_LOWPOWERENCODE_AVC,               VAProfileH264High,      VAEntrypointEncSliceLP);

/* H.264/AVC VLD */
DEFINE_GUID_VA(sDXVA_ModeH264_VLD_Multiview_NoFGT,           VAProfileH264MultiviewHigh,       VAEntrypointVLD);
DEFINE_GUID_VA(sDXVA_ModeH264_VLD_Stereo_NoFGT,              VAProfileH264StereoHigh,          VAEntrypointVLD);
DEFINE_GUID_VA(sDXVA2_ModeH264_VLD_NoFGT,                    VAProfileH264High,                VAEntrypointVLD);
DEFINE_GUID_VA(sDXVA_ModeH264_VLD_Stereo_Progressive_NoFGT,  VAProfileH264StereoHigh,          VAEntrypointVLD);

DEFINE_GUID_VA(DXVA_Intel_Decode_Elementary_Stream_AVC,      VAProfileH264High,                VAEntrypointVLD);

/* H.265 VLD */
DEFINE_GUID_VA(DXVA_ModeHEVC_VLD_Main,                       VAProfileHEVCMain,      VAEntrypointVLD);

/* VP9 */
DEFINE_GUID_VA(DXVA_Intel_ModeVP9_Profile0_VLD,              VAProfileVP9Profile0,   VAEntrypointVLD);
DEFINE_GUID_VA(DXVA_Intel_ModeVP9_Profile1_YUV444_VLD,       VAProfileVP9Profile1,   VAEntrypointVLD);
DEFINE_GUID_VA(DXVA_Intel_ModeVP9_Profile2_10bit_VLD,        VAProfileVP9Profile2,   VAEntrypointVLD);
DEFINE_GUID_VA(DXVA_Intel_ModeVP9_Profile3_YUV444_10bit_VLD, VAProfileVP9Profile3,   VAEntrypointVLD);

/* VP8 */
DEFINE_GUID_VA(sDXVA_Intel_ModeVP8_VLD,                      VAProfileVP8Version0_3, VAEntrypointVLD);
DEFINE_GUID_VA(DXVA2_Intel_Encode_VP8,                       VAProfileVP8Version0_3, VAEntrypointEncSlice);

/* VC1 */
DEFINE_GUID_VA(sDXVA2_Intel_ModeVC1_D_Super,                 VAProfileVC1Advanced,   VAEntrypointVLD);

/* JPEG */
DEFINE_GUID_VA(sDXVA2_Intel_IVB_ModeJPEG_VLD_NoFGT,          VAProfileJPEGBaseline,  VAEntrypointVLD);

/* MPEG2 */
DEFINE_GUID_VA(sDXVA2_ModeMPEG2_VLD,                         VAProfileMPEG2Main,     VAEntrypointVLD);
DEFINE_GUID_VA(DXVA2_Intel_Encode_MPEG2,                     VAProfileMPEG2Main,     VAEntrypointEncSlice);

//vvc
#if defined(MFX_ENABLE_VVC_VIDEO_DECODE)
DEFINE_GUID_VA(DXVA_Intel_ModeVVC_VLD,                       VAProfileVVCMain10,   VAEntrypointVLD);
#endif

/* AV1 */
#if defined(MFX_ENABLE_AV1_VIDEO_DECODE)
DEFINE_GUID_VA(DXVA_Intel_ModeAV1_VLD,                       VAProfileAV1Profile0,   VAEntrypointVLD);
#endif


namespace UMC
{
    class FrameAllocator;
}

// Forward declaration of used classes
struct MFX_ENTRY_POINT;

enum
{
    MFX_COPY_USE_CM             = 0x00000001u
    , MFX_COPY_USE_VACOPY_VE    = 0x00000002u
    , MFX_COPY_USE_VACOPY_BLT   = 0x00000004u
    , MFX_COPY_USE_VACOPY_EU    = 0x00000008u
    , MFX_COPY_USE_VACOPY_ANY   = (MFX_COPY_USE_VACOPY_VE | MFX_COPY_USE_VACOPY_BLT | MFX_COPY_USE_VACOPY_EU)
    , MFX_COPY_USE_ANY          = 0xffffffffu
};

class VideoCORE {
public:

    virtual ~VideoCORE(void) {}

    // imported to external API
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *handle) = 0;
    virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL handle) = 0;
    virtual mfxStatus SetBufferAllocator(mfxBufferAllocator *allocator) = 0;
    virtual mfxStatus SetFrameAllocator(mfxFrameAllocator *allocator) = 0;

    // Internal interface only
    // Utility functions for memory access
    virtual mfxStatus  AllocBuffer(mfxU32 nbytes, mfxU16 type, mfxMemId *mid) = 0;
    virtual mfxStatus  LockBuffer(mfxMemId mid, mfxU8 **ptr) = 0;
    virtual mfxStatus  UnlockBuffer(mfxMemId mid) = 0;
    virtual mfxStatus  FreeBuffer(mfxMemId mid) = 0;

    // Function checks D3D device for I/O D3D surfaces
    // If external allocator exists means that component can obtain device handle
    // If I/O surfaces in system memory  returns MFX_ERR_NONE
    // THIS IS DEPRECATED FUNCTION kept here only for backward compatibility.
    virtual mfxStatus  CheckHandle() = 0;


    virtual mfxStatus  GetFrameHDL(mfxMemId mid, mfxHDL *handle, bool ExtendedSearch = true) = 0;

    virtual mfxStatus  AllocFrames(mfxFrameAllocRequest *request,
                                   mfxFrameAllocResponse *response, bool isNeedCopy = true) = 0;

    virtual mfxStatus  LockFrame(mfxMemId mid, mfxFrameData *ptr) = 0;
    virtual mfxStatus  UnlockFrame(mfxMemId mid, mfxFrameData *ptr=0) = 0;
    virtual mfxStatus  FreeFrames(mfxFrameAllocResponse *response, bool ExtendedSearch = true) = 0;

    virtual mfxStatus  LockExternalFrame(mfxMemId mid, mfxFrameData *ptr, bool ExtendedSearch = true) = 0;
    virtual mfxStatus  GetExternalFrameHDL(mfxMemId mid, mfxHDL *handle, bool ExtendedSearch = true) = 0;
    virtual mfxStatus  UnlockExternalFrame(mfxMemId mid, mfxFrameData *ptr=0, bool ExtendedSearch = true) = 0;

    virtual mfxMemId MapIdx(mfxMemId mid) = 0;

    // Increment Surface lock
    virtual mfxStatus  IncreaseReference(mfxFrameData *ptr, bool ExtendedSearch = true) = 0;
    // Decrement Surface lock
    virtual mfxStatus  DecreaseReference(mfxFrameData *ptr, bool ExtendedSearch = true) = 0;

        // no care about surface, opaq and all round. Just increasing reference
    virtual mfxStatus IncreasePureReference(mfxU16 &) = 0;
    // no care about surface, opaq and all round. Just decreasing reference
    virtual mfxStatus DecreasePureReference(mfxU16 &) = 0;

    // Check HW property
    virtual void  GetVA(mfxHDL* phdl, mfxU16 type) = 0;
    virtual mfxStatus CreateVA(mfxVideoParam * , mfxFrameAllocRequest *, mfxFrameAllocResponse *, UMC::FrameAllocator *) = 0;
    // Get the current working adapter's number
    virtual mfxU32 GetAdapterNumber(void) = 0;

    // Get Video Processing
    virtual void  GetVideoProcessing(mfxHDL* phdl) = 0;
    virtual mfxStatus CreateVideoProcessing(mfxVideoParam *) = 0;

    virtual eMFXPlatform GetPlatformType() = 0;

    // Get the current number of working threads
    virtual mfxU32 GetNumWorkingThreads(void) = 0;
    virtual void INeedMoreThreadsInside(const void *pComponent) = 0;

    // need for correct video accelerator creation
    virtual mfxStatus DoFastCopy(mfxFrameSurface1 *dst, mfxFrameSurface1 *src) = 0;
    virtual mfxStatus DoFastCopyExtended(mfxFrameSurface1 *dst, mfxFrameSurface1 *src, mfxU32 gpuCopyMode = MFX_COPY_USE_ANY) = 0;
    virtual mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *dst, mfxU16 dstMemType, mfxFrameSurface1 *src, mfxU16 srcMemType, mfxU32 gpuCopyMode = MFX_COPY_USE_ANY) = 0;
    // DEPRECATED
    virtual bool IsFastCopyEnabled(void) = 0;

    virtual bool IsExternalFrameAllocator(void) const = 0;

    virtual eMFXHWType   GetHWType() { return MFX_HW_UNKNOWN; };
    virtual mfxU16       GetHWDeviceId() = 0;

    virtual bool         SetCoreId(mfxU32 Id) = 0;
    virtual eMFXVAType   GetVAType() const = 0;
    virtual
    mfxStatus CopyFrame(mfxFrameSurface1 *dst, mfxFrameSurface1 *src) = 0;
    virtual
    mfxStatus CopyBuffer(mfxU8 *dst, mfxU32 dst_size, mfxFrameSurface1 *src) = 0;

    virtual
    mfxStatus CopyFrameEx(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType) = 0;

    virtual mfxStatus IsGuidSupported(const GUID guid, mfxVideoParam *par, bool isEncoder = false) = 0;

    virtual void* QueryCoreInterface(const MFX_GUID &guid) = 0;
    virtual mfxSession GetSession() = 0;

    virtual mfxU16 GetAutoAsyncDepth() = 0;
    virtual bool IsSupportedDelayAlloc() const { return true; }

    mfxStatus GetFrameHDL(mfxFrameSurface1& surf, mfxHDLPair& handle, bool ExtendedSearch = true)
    {
        handle = {};

        if (surf.FrameInterface)
        {
            mfxResourceType rt = mfxResourceType(0);
            mfxStatus sts = surf.FrameInterface->GetNativeHandle ? surf.FrameInterface->GetNativeHandle(&surf, &handle.first, &rt) : MFX_ERR_NULL_PTR;

            if (sts != MFX_ERR_NONE)
                return sts;

            eMFXVAType type = GetVAType();
            bool bValidType =
                   (type == MFX_HW_D3D11 && rt == MFX_RESOURCE_DX11_TEXTURE)
                || (type == MFX_HW_D3D9 && rt == MFX_RESOURCE_DX9_SURFACE)
                || (type == MFX_HW_VAAPI && rt == MFX_RESOURCE_VA_SURFACE);

            return bValidType ? MFX_ERR_NONE : MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        return GetFrameHDL(surf.Data.MemId, &handle.first, ExtendedSearch);
    }

    mfxStatus IncreaseReference(mfxFrameSurface1& surf)
    {
        if (surf.FrameInterface)
        {
            mfxStatus sts = surf.FrameInterface->AddRef ? surf.FrameInterface->AddRef(&surf) : MFX_ERR_NULL_PTR;

            if (sts != MFX_ERR_NONE)
                return sts;
        }
        return IncreaseReference(&surf.Data);
    }

    mfxStatus DecreaseReference(mfxFrameSurface1& surf)
    {
        mfxStatus sts = DecreaseReference(&surf.Data);
        if (sts != MFX_ERR_NONE)
            return sts;

        if (surf.FrameInterface)
        {
            sts = surf.FrameInterface->Release ? surf.FrameInterface->Release(&surf) : MFX_ERR_NULL_PTR;

            if (sts != MFX_ERR_NONE)
                return sts;
        }
        return MFX_ERR_NONE;
    }

    mfxStatus LockFrame(mfxFrameSurface1& surf, mfxU32 flags = 3u /*MFX_MAP_READ_WRITE*/)
    {
        if (surf.FrameInterface)
        {
            return surf.FrameInterface->Map ? surf.FrameInterface->Map(&surf, flags) : MFX_ERR_NULL_PTR;
        }
        return LockFrame(surf.Data.MemId, &surf.Data);
    }

    mfxStatus UnlockFrame(mfxFrameSurface1& surf)
    {
        if (surf.FrameInterface)
        {
            return surf.FrameInterface->Unmap ? surf.FrameInterface->Unmap(&surf) : MFX_ERR_NULL_PTR;
        }
        return UnlockFrame(surf.Data.MemId, &surf.Data);
    }

    mfxStatus GetExternalFrameHDL(mfxFrameSurface1& surf, mfxHDLPair& handle, bool ExtendedSearch = true)
    {
        handle = {};

        if (surf.FrameInterface)
        {
            return GetFrameHDL(surf, handle, ExtendedSearch);
        }
        return GetExternalFrameHDL(surf.Data.MemId, &handle.first, ExtendedSearch);
    }

    mfxStatus LockExternalFrame(mfxFrameSurface1& surf, bool ExtendedSearch = true)
    {
        if (surf.FrameInterface)
        {
            return LockFrame(surf);
        }
        return LockExternalFrame(surf.Data.MemId, &surf.Data, ExtendedSearch);
    }

    mfxStatus UnlockExternalFrame(mfxFrameSurface1& surf, bool ExtendedSearch = true)
    {
        if (surf.FrameInterface)
        {
            return UnlockFrame(surf);
        }
        return UnlockExternalFrame(surf.Data.MemId, &surf.Data, ExtendedSearch);
    }


};


// Core extension should be obtained using MFXICORE_API_1_19_GUID
class IVideoCore_API_1_19
{
public:
    virtual ~IVideoCore_API_1_19() {}
    virtual mfxStatus QueryPlatform(mfxPlatform* platform) = 0;
};

class VideoENC
{
public:
    virtual
    ~VideoENC(void){}

    virtual
    mfxStatus Init(mfxVideoParam *par) = 0;
    virtual
    mfxStatus Reset(mfxVideoParam *par) = 0;
    virtual
    mfxStatus Close(void) = 0;
    virtual
    mfxTaskThreadingPolicy GetThreadingPolicy(void) {return MFX_TASK_THREADING_DEFAULT;}

    virtual
    mfxStatus GetVideoParam(mfxVideoParam *par) = 0;
    virtual
    mfxStatus GetFrameParam(mfxFrameParam *par) = 0;

};

class VideoPAK
{
public:
    virtual
    ~VideoPAK(void) {}

    virtual
    mfxStatus Init(mfxVideoParam *par) = 0;
    virtual
    mfxStatus Reset(mfxVideoParam *par) = 0;
    virtual
    mfxStatus Close(void) = 0;
    virtual
    mfxTaskThreadingPolicy GetThreadingPolicy(void) {return MFX_TASK_THREADING_DEFAULT;}

    virtual
    mfxStatus GetVideoParam(mfxVideoParam *par) = 0;
    virtual
    mfxStatus GetFrameParam(mfxFrameParam *par) = 0;

};

// mfxEncodeInternalParams
typedef enum
{

    MFX_IFLAG_ADD_HEADER = 1,  // MPEG2: add SeqHeader before this frame
    MFX_IFLAG_ADD_EOS = 2,     // MPEG2: add EOS after this frame
    MFX_IFLAG_BWD_ONLY = 4,    // MPEG2: only backward prediction for this frame
    MFX_IFLAG_FWD_ONLY = 8     // MPEG2: only forward prediction for this frame

} MFX_ENCODE_INTERNAL_FLAGS;


typedef struct _mfxEncodeInternalParams : public mfxEncodeCtrl
{
    mfxU32              FrameOrder;
    mfxU32              InternalFlags; //MFX_ENCODE_INTERNAL_FLAGS
    mfxFrameSurface1    *surface;
} mfxEncodeInternalParams;

class SurfaceCache;

class VideoENCODE
{
public:
    virtual
    ~VideoENCODE(void) {}

    virtual
    mfxStatus Init(mfxVideoParam *par) = 0;
    virtual
    mfxStatus Reset(mfxVideoParam *par) = 0;
    virtual
    mfxStatus Close(void) = 0;
    virtual
    mfxTaskThreadingPolicy GetThreadingPolicy(void) {return MFX_TASK_THREADING_DEFAULT;}

    virtual
    mfxStatus GetVideoParam(mfxVideoParam *par) = 0;
    virtual
    mfxStatus GetFrameParam(mfxFrameParam *par) = 0;
    virtual
    mfxStatus GetEncodeStat(mfxEncodeStat *stat) = 0;
    virtual
    mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl,
                               mfxFrameSurface1 *surface,
                               mfxBitstream *bs,
                               mfxFrameSurface1 **reordered_surface,
                               mfxEncodeInternalParams *pInternalParams,
                               MFX_ENTRY_POINT *pEntryPoint)
    {
        (void)pEntryPoint;

        return EncodeFrameCheck(ctrl, surface, bs, reordered_surface, pInternalParams);
    }
    virtual
    mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl,
                               mfxFrameSurface1 *surface,
                               mfxBitstream *bs,
                               mfxFrameSurface1 **reordered_surface,
                               mfxEncodeInternalParams *pInternalParams,
                               MFX_ENTRY_POINT pEntryPoints[],
                               mfxU32 &numEntryPoints)
    {
        mfxStatus mfxRes;

        // call the overloaded version
        mfxRes = EncodeFrameCheck(ctrl, surface, bs, reordered_surface, pInternalParams, pEntryPoints);
        numEntryPoints = 1;
        return mfxRes;
    }
    virtual
    mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams) = 0;
    virtual
    mfxStatus EncodeFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *surface, mfxBitstream *bs) = 0;
    virtual
    mfxStatus CancelFrame(mfxEncodeCtrl *ctrl, mfxEncodeInternalParams *pInternalParams, mfxFrameSurface1 *surface, mfxBitstream *bs) = 0;

    mfxStatus ResetCache(mfxVideoParam* par)
    {
        MFX_CHECK_NULL_PTR1(par);

        if (m_pSurfaceCache)
            MFX_SAFE_CALL(m_pSurfaceCache->ResetCache(*par));

        return MFX_ERR_NONE;
    }

    virtual
    mfxU16 GetMemType(mfxVideoParam par)
    {
        return mfxU16(par.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ? MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET);
    }

    virtual mfxStatus GetSurface(mfxFrameSurface1** output_surf, mfxSurfaceHeader* import_surface) = 0;

    std::unique_ptr<surface_cache_controller<SurfaceCache>, std::function<void(surface_cache_controller<SurfaceCache>*)>> m_pSurfaceCache;
    static bool m_singleEncodeMode;
};

#define MFX_PROPAGATE_GetSurface_VideoENCODE_Definition \
mfxStatus GetSurface(mfxFrameSurface1** output_surf, mfxSurfaceHeader* import_surface) override

#define MFX_PROPAGATE_GetSurface_VideoENCODE_Impl(ClassName)                                          \
mfxStatus ClassName::GetSurface(mfxFrameSurface1** output_surf, mfxSurfaceHeader* import_surface)     \
{                                                                                                     \
    MFX_CHECK_NULL_PTR1(output_surf);                                                                 \
    MFX_CHECK(m_pSurfaceCache, MFX_ERR_NOT_INITIALIZED);                                              \
                                                                                                      \
    try                                                                                               \
    {                                                                                                 \
        MFX_RETURN(m_pSurfaceCache->GetSurface(*output_surf, import_surface));                        \
    }                                                                                                 \
    catch (...)                                                                                       \
    {                                                                                                 \
        MFX_RETURN(MFX_ERR_MEMORY_ALLOC);                                                             \
    }                                                                                                 \
}

class VideoDECODE
{
public:
    virtual
    ~VideoDECODE(void) {}

    virtual
    mfxStatus Init(mfxVideoParam *par) = 0;
    virtual
    mfxStatus Reset(mfxVideoParam *par) = 0;
    virtual
    mfxStatus Close(void) = 0;
    virtual
    mfxTaskThreadingPolicy GetThreadingPolicy(void) {return MFX_TASK_THREADING_DEFAULT;}

    virtual
    mfxStatus GetVideoParam(mfxVideoParam *par) = 0;
    virtual
    mfxStatus GetDecodeStat(mfxDecodeStat *stat) = 0;
    virtual
    mfxStatus DecodeFrameCheck(mfxBitstream *bs,
                               mfxFrameSurface1 *surface_work,
                               mfxFrameSurface1 **surface_out,
                               MFX_ENTRY_POINT *pEntryPoint) = 0;
    virtual mfxStatus SetSkipMode(mfxSkipMode mode) {
        (void)mode;

        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload) = 0;

    virtual mfxStatus         GetSurface(mfxFrameSurface1* & surface, mfxSurfaceHeader* /*import_surface*/) { surface = nullptr; return MFX_ERR_UNSUPPORTED; }
    virtual mfxFrameSurface1* GetInternalSurface(mfxFrameSurface1 * /*surface*/) { return nullptr; }

    mfxStatus ResetCache(mfxVideoParam*) { return MFX_ERR_NONE; }
};

class VideoVPP
{
public:
    virtual
    ~VideoVPP(void) {}

    virtual
    mfxStatus Init(mfxVideoParam *par) = 0;
    virtual
    mfxStatus Reset(mfxVideoParam *par) = 0;
    virtual
    mfxStatus Close(void) = 0;
    virtual
    mfxTaskThreadingPolicy GetThreadingPolicy(void) {return MFX_TASK_THREADING_DEFAULT;}

    virtual
    mfxStatus GetVideoParam(mfxVideoParam *par) = 0;
    virtual
    mfxStatus GetVPPStat(mfxVPPStat *stat) = 0;
    virtual
    mfxStatus VppFrameCheck(mfxFrameSurface1 *in,
                            mfxFrameSurface1 *out,
                            mfxExtVppAuxData *aux,
                            MFX_ENTRY_POINT *pEntryPoint)
    {
        (void)pEntryPoint;
        (void)aux;

        return VppFrameCheck(in, out);
    }

    virtual
    mfxStatus VppFrameCheck(mfxFrameSurface1 *in,
                            mfxFrameSurface1 *out,
                            mfxExtVppAuxData *aux,
                            MFX_ENTRY_POINT pEntryPoints[],
                            mfxU32 &numEntryPoints)
    {
        mfxStatus mfxRes;

        // call the overloaded version
        mfxRes = VppFrameCheck(in, out, aux, pEntryPoints);
        numEntryPoints = 1;

        return mfxRes;
    }

    virtual
    mfxStatus VppFrameCheck(mfxFrameSurface1 *in, mfxFrameSurface1 *out) = 0;
    virtual
    mfxStatus RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux) = 0;

    mfxStatus ResetCache(mfxVideoParam* par)
    {
        MFX_CHECK_NULL_PTR1(par);

        if (m_pSurfaceCacheIn)
            MFX_SAFE_CALL(m_pSurfaceCacheIn->ResetCache(*par));

        if (m_pSurfaceCacheOut)
            MFX_SAFE_CALL(m_pSurfaceCacheOut->ResetCache(*par));

        return MFX_ERR_NONE;
    }

    mfxStatus GetSurfaceFromIn(mfxFrameSurface1** output_surf, mfxSurfaceHeader* import_surface)
    {
        return GetSurface(m_pSurfaceCacheIn, output_surf, import_surface);
    }

    mfxStatus GetSurfaceFromOut(mfxFrameSurface1** output_surf, mfxSurfaceHeader* import_surface)
    {
        return GetSurface(m_pSurfaceCacheOut, output_surf, import_surface);
    }

    using cache_type = std::unique_ptr<surface_cache_controller<SurfaceCache>, std::function<void(surface_cache_controller<SurfaceCache>*)>>;
    cache_type m_pSurfaceCacheIn;
    cache_type m_pSurfaceCacheOut;

    virtual mfxStatus GetSurface(cache_type& cache, mfxFrameSurface1** output_surf, mfxSurfaceHeader* import_surface) = 0;

};

#define MFX_PROPAGATE_GetSurface_VideoVPP_Definition \
mfxStatus GetSurface(cache_type& cache, mfxFrameSurface1** output_surf, mfxSurfaceHeader* import_surface) override

#define MFX_PROPAGATE_GetSurface_VideoVPP_Impl(ClassName)                                                               \
mfxStatus ClassName::GetSurface(cache_type& cache, mfxFrameSurface1** output_surf, mfxSurfaceHeader* import_surface)    \
{                                                                                                                       \
    MFX_CHECK_NULL_PTR1(output_surf);                                                                                   \
    MFX_CHECK(cache, MFX_ERR_NOT_INITIALIZED);                                                                          \
                                                                                                                        \
    try                                                                                                                 \
    {                                                                                                                   \
        MFX_RETURN(cache->GetSurface(*output_surf, import_surface));                                                    \
    }                                                                                                                   \
    catch (...)                                                                                                         \
    {                                                                                                                   \
        MFX_RETURN(MFX_ERR_MEMORY_ALLOC);                                                                               \
    }                                                                                                                   \
}

#ifdef _MSVC_LANG
#pragma warning(pop)
#endif

#endif // __MFXVIDEOPLUSPLUS_INTERNAL_H
