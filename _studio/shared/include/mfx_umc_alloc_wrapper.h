// Copyright (c) 2009-2020 Intel Corporation
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

#ifndef _MFX_ALLOC_WRAPPER_H_
#define _MFX_ALLOC_WRAPPER_H_

#include <vector>
#include <memory> // unique_ptr

#include "mfx_common.h"
#include "umc_memory_allocator.h"
#include "umc_frame_allocator.h"
#include "umc_frame_data.h"

#include "mfxvideo++int.h"

#include "libmfx_core.h"

#define MFX_UMC_MAX_ALLOC_SIZE 128

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// mfx_UMC_MemAllocator - buffer allocator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class mfx_UMC_MemAllocator : public UMC::MemoryAllocator
{
    DYNAMIC_CAST_DECL(mfx_UMC_MemAllocator, MemoryAllocator)

public:
    mfx_UMC_MemAllocator(void);
    virtual ~mfx_UMC_MemAllocator(void);

    // Initiates object
    virtual UMC::Status InitMem(UMC::MemoryAllocatorParams *pParams, VideoCORE* mfxCore);

    // Closes object and releases all allocated memory
    virtual UMC::Status Close() override;

    // Allocates or reserves physical memory and return unique ID
    // Sets lock counter to 0
    virtual UMC::Status Alloc(UMC::MemID *pNewMemID, size_t Size, uint32_t Flags, uint32_t Align = 16) override;

    // Lock() provides pointer from ID. If data is not in memory (swapped)
    // prepares (restores) it. Increases lock counter
    virtual void *Lock(UMC::MemID MID) override;

    // Unlock() decreases lock counter
    virtual UMC::Status Unlock(UMC::MemID MID) override;

    // Notifies that the data wont be used anymore. Memory can be free
    virtual UMC::Status Free(UMC::MemID MID) override;

    // Immediately deallocates memory regardless of whether it is in use (locked) or no
    virtual UMC::Status DeallocateMem(UMC::MemID MID) override;

protected:
    VideoCORE* m_pCore;
};


enum mfx_UMC_FrameAllocator_Flags {
    mfx_UMC_ReallocAllowed = 1,
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// mfx_UMC_FrameAllocator
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class mfx_UMC_FrameAllocator : public UMC::FrameAllocator
{
    DYNAMIC_CAST_DECL(mfx_UMC_FrameAllocator, UMC::FrameAllocator)

public:
    mfx_UMC_FrameAllocator(void);
    virtual ~mfx_UMC_FrameAllocator(void);

    // Initiates object
    virtual UMC::Status InitMfx(UMC::FrameAllocatorParams *pParams,
                                VideoCORE* mfxCore,
                                const mfxVideoParam *params,
                                const mfxFrameAllocRequest *request,
                                mfxFrameAllocResponse *response,
                                bool isUseExternalFrames,
                                bool isSWplatform);

    // Closes object and releases all allocated memory
    virtual UMC::Status Close() override;

    // Allocates or reserves physical memory and returns unique ID
    // Sets lock counter to 0
    virtual UMC::Status Alloc(UMC::FrameMemID *pNewMemID, const UMC::VideoDataInfo * info, uint32_t flags) override;

    virtual UMC::Status GetFrameHandle(UMC::FrameMemID memId, void * handle) override;

    // Lock() provides pointer from ID. If data is not in memory (swapped)
    // prepares (restores) it. Increases lock counter
    virtual const UMC::FrameData* Lock(UMC::FrameMemID mid) override;

    // Unlock() decreases lock counter
    virtual UMC::Status Unlock(UMC::FrameMemID mid) override;

    // Notifies that the data won't be used anymore. Memory can be freed.
    virtual UMC::Status IncreaseReference(UMC::FrameMemID mid) override;

    // Notifies that the data won't be used anymore. Memory can be freed.
    virtual UMC::Status DecreaseReference(UMC::FrameMemID mid) override;

    virtual UMC::Status Reset() override;

    virtual mfxStatus SetCurrentMFXSurface(mfxFrameSurface1 *srf);

    virtual mfxFrameSurface1*  GetSurface(UMC::FrameMemID index, mfxFrameSurface1 *surface_work, const mfxVideoParam * videoPar);
    virtual mfxStatus          PrepareToOutput(mfxFrameSurface1 *surface_work, UMC::FrameMemID index, const mfxVideoParam * videoPar, mfxU32 gpuCopyMode = MFX_COPY_USE_ANY);
    mfxI32 FindSurface(mfxFrameSurface1 *surf);
    bool   HasFreeSurface();

    void SetSfcPostProcessingFlag(bool flagToSet);

    void SetExternalFramesResponse(mfxFrameAllocResponse *response);
    mfxFrameSurface1 * GetInternalSurface(UMC::FrameMemID index);
    mfxFrameSurface1 * GetSurfaceByIndex(UMC::FrameMemID index);

    mfxMemId ConvertMemId(UMC::FrameMemID index)
    {
        return m_frameDataInternal.GetSurface(index).Data.MemId;
    };

protected:
    struct  surf_descr
    {
        surf_descr(mfxFrameSurface1* FrameSurface, bool isUsed):FrameSurface(FrameSurface),
                                                                isUsed(isUsed)
        {
        };
        surf_descr():FrameSurface(0),
                     isUsed(false)
        {
        };
        mfxFrameSurface1* FrameSurface;
        bool              isUsed;
    };

    virtual UMC::Status Free(UMC::FrameMemID mid);

    virtual mfxI32 AddSurface(mfxFrameSurface1 *surface);

    class InternalFrameData
    {
        class FrameRefInfo
        {
        public:
            FrameRefInfo();
            void Reset();

            mfxU32 m_referenceCounter;
        };

        typedef std::pair<mfxFrameSurface1, UMC::FrameData> FrameInfo;

    public:

        mfxFrameSurface1 & GetSurface(mfxU32 index);
        UMC::FrameData   & GetFrameData(mfxU32 index);
        void ResetFrameData(mfxU32 index);
        mfxU32 IncreaseRef(mfxU32 index);
        mfxU32 DecreaseRef(mfxU32 index);

        bool IsValidMID(mfxU32 index) const;

        void AddNewFrame(mfx_UMC_FrameAllocator * alloc, mfxFrameSurface1 *surface, UMC::VideoDataInfo * info);

        mfxU32 GetSize() const;

        void Close();
        void Reset();

        void Resize(mfxU32 size);

    private:
        std::vector<FrameInfo>  m_frameData;
        std::vector<FrameRefInfo>  m_frameDataRefs;
    };

    InternalFrameData m_frameDataInternal;

    std::vector<surf_descr> m_extSurfaces;

    mfxI32        m_curIndex;

    bool m_IsUseExternalFrames;
    bool m_sfcVideoPostProcessing;

    mfxFrameInfo m_surface_info;  // for copying

    UMC::VideoDataInfo m_info;

    VideoCORE* m_pCore;

    mfxFrameAllocResponse *m_externalFramesResponse;

    bool       m_isSWDecode;
    mfxU16     m_IOPattern;

private:
    mfxI32 FindFreeSurface();
};

class SurfaceSource : public UMC::FrameAllocator
{
public:
    SurfaceSource(VideoCORE* core, const mfxVideoParam & video_param, eMFXPlatform platform, mfxFrameAllocRequest& request, mfxFrameAllocRequest& request_internal, mfxFrameAllocResponse& response, mfxFrameAllocResponse& response_alien, bool needVppJPEG = false);

    ~SurfaceSource();

    virtual UMC::Status Close() override;

    virtual UMC::Status Reset() override;

    virtual UMC::Status Alloc(UMC::FrameMemID *pNewMemID, const UMC::VideoDataInfo * info, uint32_t Flags) override;

    virtual UMC::Status GetFrameHandle(UMC::FrameMemID MID, void * handle) override;

    virtual const UMC::FrameData* Lock(UMC::FrameMemID MID) override;

    virtual UMC::Status Unlock(UMC::FrameMemID MID) override;

    virtual UMC::Status IncreaseReference(UMC::FrameMemID MID) override;

    virtual UMC::Status DecreaseReference(UMC::FrameMemID MID) override;

    // Performs mapping mfxMemId <-> UMC::FrameMimID
    mfxI32 FindSurface(mfxFrameSurface1 *surf);

    // Adding new surface to cache
    mfxStatus SetCurrentMFXSurface(mfxFrameSurface1 *surf);

    mfxFrameSurface1 * GetSurface(UMC::FrameMemID index, mfxFrameSurface1 *surface, const mfxVideoParam * videoPar);
    mfxFrameSurface1 * GetInternalSurface(UMC::FrameMemID index);

    mfxStatus GetSurface(mfxFrameSurface1* & surf, mfxSurfaceHeader* import_surface);
    mfxFrameSurface1 * GetInternalSurface(mfxFrameSurface1 *sfc_surf);

    mfxFrameSurface1 * GetSurfaceByIndex(UMC::FrameMemID index);

    mfxStatus PrepareToOutput(mfxFrameSurface1 *surface_work, UMC::FrameMemID index, const mfxVideoParam * videoPar, mfxU32 gpuCopyMode = MFX_COPY_USE_ANY);

    bool HasFreeSurface();

    bool GetSurfaceType();

    void SetFreeSurfaceAllowedFlag(bool flag);

protected:
    VideoCORE*                                    m_core;

    // Decoder works with these surfaces
    //shared_surface_cache_controller
    std::shared_ptr<surface_cache_controller<SurfaceCache> > m_vpl_cache_decoder_surfaces;
    // These surfaces are outputted to user (different with previous pool
    // in case of IO pattern and decoder impl mismatch or SFC on Linux)
    std::shared_ptr<surface_cache_controller<SurfaceCache> > m_vpl_cache_output_surfaces;

    bool                                          m_redirect_to_vpl_path = false;

    std::unique_ptr<mfx_UMC_FrameAllocator>       m_umc_allocator_adapter;

    // Parameters required for proper conversion of mfx <-> UMC params
    std::map<mfxMemId, UMC::FrameMemID>           m_mfx2umc_memid;
    std::map<UMC::FrameMemID, mfxMemId>           m_umc2mfx_memid;

private:

    void CreateUMCAllocator(const mfxVideoParam & video_param, eMFXPlatform platform, bool needVppJPEG);

    bool CreateCorrespondence(mfxFrameSurface1& surface_work, mfxFrameSurface1& surface_out);
    void RemoveCorrespondence(mfxFrameSurface1& surface_work);

    void CreateBinding(const mfxFrameSurface1 & surf);
    void RemoveBinding(const mfxFrameSurface1 & surf);
    mfxFrameSurface1* GetDecoderSurface(UMC::FrameMemID index);

    UMC::Status CheckForRealloc(const UMC::VideoDataInfo & info, const mfxFrameSurface1& surf, bool realloc_allowed) const;

    void ReleaseCurrentWorkSurface();

    // Parameters required for proper conversion of mfx <-> UMC params

    std::map<UMC::FrameMemID, UMC::FrameData> m_umc2framedata;

    UMC::VideoDataInfo                        m_video_data_info;

    // Case when platform of decoder (SW / HW) and type of output surfaces (SW / HW) mismatch
    // or if SFC on linux used
    bool                                      m_allocate_internal          = false;
    // If m_allocate_internal and not SFC on linux, then we need to copy before output
    bool                                      m_need_to_copy_before_output = false;
    // If sys mem with sw fallback then ext alloc can be off so surface cache will be empty
    bool                                      m_sw_fallback_sys_mem        = false;
    // Mapping of work surfaces <-> output surfaces (if they differ)
    std::map<mfxMemId, mfxFrameSurface1*>     m_work_output_surface_map;
    std::map<mfxMemId, mfxFrameSurface1*>     m_output_work_surface_map;
    // For surface handling in case of sys mem with sw fallback and no ext allocator
    std::vector<mfxFrameSurface1*>            m_sw_fallback_surfaces;

    /*
    Several skip frame surfaces may be related to one work surface.
    Map new output surface on decoder's surface used for last not skipped frame.
    */
    std::map<mfxFrameSurface1*, mfxFrameSurface1*> m_output_work_surface_skip_frames;

    // VPL memory model 2 (GetSurfaceForDecode + DecodeFrameAsync)
    bool                                      m_memory_model2        = false;
    mfxFrameSurface1*                         m_current_work_surface = nullptr;

    mfx::ResettableTimerMs                    m_timer;

    // Parameters for proper work with mfx_UMC_FrameAllocator (i.e. support of MSDK 1.x path)
    mfxFrameAllocResponse&                    m_response;
    mfxFrameAllocResponse&                    m_response_alien;
};

class mfx_UMC_FrameAllocator_D3D : public mfx_UMC_FrameAllocator
{
public:
    virtual mfxStatus PrepareToOutput(mfxFrameSurface1 *surface_work, UMC::FrameMemID index, const mfxVideoParam * videoPar, mfxU32 gpuCopyMode = MFX_COPY_USE_ANY);
};

#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)
class VideoVppJpeg;

struct JPEG_Info
{
    int32_t colorFormat = 0;
    size_t  UOffset     = 0;
    size_t  VOffset     = 0;
};

class SurfaceSourceJPEG : public SurfaceSource
{
public:
    SurfaceSourceJPEG(VideoCORE* core, const mfxVideoParam & video_param, eMFXPlatform platform, mfxFrameAllocRequest& request, mfxFrameAllocRequest& request_internal,
        mfxFrameAllocResponse& response, mfxFrameAllocResponse& response_alien);

    // suppose that Close() calls Reset(), so override only Reset()
    virtual UMC::Status Reset() override;

    void SetJPEGInfo(JPEG_Info * jpegInfo);

    mfxStatus StartPreparingToOutput(mfxFrameSurface1 *surface_work, UMC::FrameData* in, const mfxVideoParam * par, mfxU16 *taskId);
    mfxStatus CheckPreparingToOutput(mfxFrameSurface1 *surface_work, UMC::FrameData* in, const mfxVideoParam * par, mfxU16 taskId);

private:
    JPEG_Info  m_jpegInfo;

    std::unique_ptr<VideoVppJpeg> m_pCc;

    mfxStatus InitVideoVppJpeg(const mfxVideoParam *params);
    mfxStatus FindSurfaceByMemId(const UMC::FrameData* in, const mfxHDLPair &hdlPair, mfxFrameSurface1 &out_surface);
};

class mfx_UMC_FrameAllocator_D3D_Converter : public mfx_UMC_FrameAllocator_D3D
{
public:
    virtual UMC::Status InitMfx(UMC::FrameAllocatorParams *pParams, 
                                VideoCORE* mfxCore, 
                                const mfxVideoParam *params, 
                                const mfxFrameAllocRequest *request, 
                                mfxFrameAllocResponse *response, 
                                bool isUseExternalFrames,
                                bool isSWplatform) override;

    // suppose that Close() calls Reset(), so override only Reset()
    virtual UMC::Status Reset() override;

    void SetJPEGInfo(JPEG_Info * jpegInfo);

    mfxStatus StartPreparingToOutput(mfxFrameSurface1 *surface_work, UMC::FrameData* in, const mfxVideoParam * par, mfxU16 *taskId);
    mfxStatus CheckPreparingToOutput(mfxFrameSurface1 *surface_work, UMC::FrameData* in, const mfxVideoParam * par, mfxU16 taskId);

private:
    JPEG_Info  m_jpegInfo;
    std::unique_ptr<VideoVppJpeg> m_pCc;

    mfxStatus InitVideoVppJpeg(const mfxVideoParam *params);
    mfxStatus FindSurfaceByMemId(const UMC::FrameData* in, const mfxHDLPair &hdlPair, mfxFrameSurface1 &out_surface);

    friend class SurfaceSourceJPEG;
};

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE) && defined (MFX_VA_WIN)

#endif //_MFX_ALLOC_WRAPPER_H_
