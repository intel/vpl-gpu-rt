// Copyright (c) 2006-2020 Intel Corporation
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

#ifndef __UMC_VA_BASE_H__
#define __UMC_VA_BASE_H__

#include <vector>
#include "mfx_common.h"
#include "mfxstructures-int.h"


#ifndef UMC_VA
#   define UMC_VA
#endif

#   ifndef UMC_VA_LINUX
#       define UMC_VA_LINUX          // HW acceleration through Linux VA
#   endif
#   ifndef SYNCHRONIZATION_BY_VA_SYNC_SURFACE
#       ifdef ANDROID
#           define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
#       else
#           define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
#       endif
#   endif


#ifdef  __cplusplus
#include "umc_structures.h"
#include "umc_dynamic_cast.h"

#ifdef _MSVC_LANG
#pragma warning(disable : 4201)
#pragma warning(disable : 26812)
#endif

#ifdef UMC_VA_LINUX
#include <va/va.h>
#include <va/va_dec_vp8.h>
#include <va/va_vpp.h>
#include <va/va_dec_vp9.h>
#include <va/va_dec_hevc.h>
#if defined(MFX_ENABLE_AV1_VIDEO_DECODE)
#include <va/va_dec_av1.h>
#endif

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(p) (void) (p);
#endif
#endif


#ifdef UMC_VA_DXVA
#include <windows.h>
#endif


#ifdef UMC_VA_DXVA
#include <d3d9.h>
#include <dxva2api.h>
#include <dxva.h>
#endif


#ifndef GUID_TYPE_DEFINED
typedef struct {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID;
#define GUID_TYPE_DEFINED
#endif

#define UMC_VA_AV1_MSFT

namespace UMC
{

#define VA_COMBINATIONS(CODEC) \
    CODEC##_VLD     = VA_##CODEC | VA_VLD

enum VideoAccelerationProfile
{
    UNKNOWN         = 0,

    // Codecs
    VA_CODEC        = 0x00ff,
    VA_MPEG2        = 0x0001,
    VA_H264         = 0x0003,
    VA_VC1          = 0x0004,
    VA_JPEG         = 0x0005,
    VA_VP8          = 0x0006,
    VA_H265         = 0x0007,
    VA_VP9          = 0x0008,
    VA_AV1          = 0x0009,

    // Entry points
    VA_ENTRY_POINT  = 0xfff00,
    VA_VLD          = 0x00400,

    VA_PROFILE                  = 0xff000,
	VA_PROFILE_MVC              = 0x04000,
    VA_PROFILE_MVC_MV           = 0x05000,
    VA_PROFILE_MVC_STEREO       = 0x06000,
    VA_PROFILE_MVC_STEREO_PROG  = 0x07000,

    VA_PROFILE_422              = 0x0a000,
    VA_PROFILE_444              = 0x0b000,
    VA_PROFILE_10               = 0x10000,
    VA_PROFILE_REXT             = 0x20000,
    VA_PROFILE_12               = 0x40000,
    VA_PROFILE_SCC              = 0x80000,

    // configurations
    VA_CONFIGURATION            = 0x0ff00000,
    VA_LONG_SLICE_MODE          = 0x00100000,
    VA_SHORT_SLICE_MODE         = 0x00200000,
    VA_ANY_SLICE_MODE           = 0x00300000,

    MPEG2_VLD       = VA_MPEG2 | VA_VLD,
    H264_VLD        = VA_H264 | VA_VLD,
    H265_VLD        = VA_H265 | VA_VLD,
    VC1_VLD         = VA_VC1 | VA_VLD,
    JPEG_VLD        = VA_JPEG | VA_VLD,
    VP8_VLD         = VA_VP8 | VA_VLD,
    HEVC_VLD        = VA_H265 | VA_VLD,
    VP9_VLD         = VA_VP9 | VA_VLD,
    AV1_VLD         = VA_AV1 | VA_VLD,
    AV1_10_VLD      = VA_AV1 | VA_VLD | VA_PROFILE_10,

    H265_VLD_REXT               = VA_H265 | VA_VLD | VA_PROFILE_REXT,
    H265_10_VLD_REXT            = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_10,
    H265_10_VLD                 = VA_H265 | VA_VLD | VA_PROFILE_10,
    H265_VLD_422                = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_422,
    H265_VLD_444                = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_444,
    H265_10_VLD_422             = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_10 | VA_PROFILE_422,
    H265_10_VLD_444             = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_10 | VA_PROFILE_444,


    H265_12_VLD_420             = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_12,
    H265_12_VLD_422             = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_12 | VA_PROFILE_422,
    H265_12_VLD_444             = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_12 | VA_PROFILE_444,

    H265_VLD_SCC                = VA_H265 | VA_VLD | VA_PROFILE_SCC,
    H265_VLD_444_SCC            = VA_H265 | VA_VLD | VA_PROFILE_SCC  | VA_PROFILE_444,
    H265_10_VLD_SCC             = VA_H265 | VA_VLD | VA_PROFILE_SCC  | VA_PROFILE_10,
    H265_10_VLD_444_SCC         = VA_H265 | VA_VLD | VA_PROFILE_SCC  | VA_PROFILE_10 | VA_PROFILE_444,

    VP9_10_VLD                  = VA_VP9 | VA_VLD | VA_PROFILE_10,
    VP9_VLD_422                 = VA_VP9 | VA_VLD | VA_PROFILE_422,
    VP9_VLD_444                 = VA_VP9 | VA_VLD | VA_PROFILE_444,
    VP9_10_VLD_422              = VA_VP9 | VA_VLD | VA_PROFILE_10 | VA_PROFILE_422,
    VP9_10_VLD_444              = VA_VP9 | VA_VLD | VA_PROFILE_10 | VA_PROFILE_444,
    VP9_12_VLD_420              = VA_VP9 | VA_VLD | VA_PROFILE_12,
    VP9_12_VLD_444              = VA_VP9 | VA_VLD | VA_PROFILE_12 | VA_PROFILE_444,

};

#define MAX_BUFFER_TYPES    32
enum VideoAccelerationPlatform
{
    VA_UNKNOWN_PLATFORM = 0,

    VA_PLATFORM  = 0x0f0000,
    VA_DXVA2     = 0x020000,
    VA_LINUX     = 0x030000,
};

class UMCVACompBuffer;
class ProtectedVA;
class VideoProcessingVA;

enum eUMC_VA_Status
{
    UMC_ERR_DEVICE_FAILED         = -2000,
    UMC_ERR_DEVICE_LOST           = -2001,
    UMC_ERR_FRAME_LOCKED          = -2002
};

///////////////////////////////////////////////////////////////////////////////////
class FrameAllocator;
class VideoAcceleratorParams
{
    DYNAMIC_CAST_DECL_BASE(VideoAcceleratorParams);

public:

    VideoAcceleratorParams()
        : m_pVideoStreamInfo(nullptr)
        , m_iNumberSurfaces(0)
        , m_protectedVA(0)
        , m_needVideoProcessingVA(false)
        , m_allocator(nullptr)
        , m_surf(nullptr)
    {}

    virtual ~VideoAcceleratorParams(){}

    VideoStreamInfo *m_pVideoStreamInfo;
    int32_t          m_iNumberSurfaces;
    int32_t          m_protectedVA;
    bool             m_needVideoProcessingVA;

    FrameAllocator  *m_allocator;

    // if extended surfaces exist
    void*           *m_surf;
};


class VideoAccelerator
{
    DYNAMIC_CAST_DECL_BASE(VideoAccelerator);

public:
    VideoAccelerator() :
        m_Profile(UNKNOWN),
        m_Platform(VA_UNKNOWN_PLATFORM),
        m_HWPlatform(MFX_HW_UNKNOWN),
        m_protectedVA(nullptr),
#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
        m_videoProcessingVA(0),
#endif
        m_allocator(0),
        m_bH264ShortSlice(false),
        m_bH264MVCSupport(false),
        m_isUseStatuReport(true),
        m_H265ScalingListScanOrder(1)
    {
    }

    virtual ~VideoAccelerator()
    {
        Close();
    }

    virtual Status Init(VideoAcceleratorParams* pInfo) = 0; // Initilize and allocate all resources
    virtual Status Close();
    virtual Status Reset();

    virtual Status BeginFrame   (int32_t  index) = 0; // Begin decoding for specified index

    // Begin decoding for specified index, keep in mind fieldId to sync correctly on task in next SyncTask call.
    // By default just calls BeginFrame(index). must be overridden by child class
    virtual Status BeginFrame(int32_t  index, uint32_t fieldId )
    {
        // by default just calls BeginFrame(index)
        (void)fieldId;
        return BeginFrame(index);
    }
    virtual void*  GetCompBuffer(int32_t            buffer_type,
                                 UMCVACompBuffer** buf   = NULL,
                                 int32_t            size  = -1,
                                 int32_t            index = -1) = 0; // request buffer

    virtual Status Execute      () = 0;                  // execute decoding
    virtual Status ExecuteExtensionBuffer(void * buffer) = 0;
    virtual Status ExecuteStatusReportBuffer(void * buffer, int32_t size) = 0;
    virtual Status SyncTask(int32_t index, void * error = NULL) = 0;
    virtual Status QueryTaskStatus(int32_t index, void * status, void * error) = 0;
    virtual Status ReleaseBuffer(int32_t type) = 0;      // release buffer
    virtual Status EndFrame     (void * handle = 0) = 0; // end frame

    virtual Status UnwrapBuffer(mfxMemId /*bufferId*/) { return MFX_ERR_NONE; };

    virtual bool IsIntelCustomGUID() const = 0;
    /* TODO: is used on Linux only? On Linux there are isues with signed/unsigned return value. */
    virtual int32_t GetSurfaceID(int32_t idx) const { return idx; }

    virtual ProtectedVA * GetProtectedVA() {return m_protectedVA;}

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    virtual VideoProcessingVA * GetVideoProcessingVA() {return m_videoProcessingVA;}
#endif

    bool IsGPUSyncEventEnable() const
    {
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE
        VideoAccelerationProfile codec = (VideoAccelerationProfile)(m_Profile & VA_CODEC);
        VideoAccelerationProfile profile = (VideoAccelerationProfile)(m_Profile & VA_PROFILE);
        bool isHybrid = ((codec == VA_H265 && profile == VA_PROFILE_10) || codec == VA_VP9) && (m_HWPlatform < MFX_HW_APL);
        bool isEnabled = false;
        switch (codec)
        {
        case VA_MPEG2:
#if defined (MFX_ENABLE_HW_BLOCKING_TASK_SYNC_MPEG2D)
            isEnabled = true;
#endif
            break;
        case VA_H264:
#if defined (MFX_ENABLE_HW_BLOCKING_TASK_SYNC_H264D)
            isEnabled = true;
#endif
            break;
        case VA_VC1:
#if defined (MFX_ENABLE_HW_BLOCKING_TASK_SYNC_VC1D)
            isEnabled = true;
#endif
            break;
        case VA_JPEG:
#if defined (MFX_ENABLE_HW_BLOCKING_TASK_SYNC_JPEGD)
            isEnabled = true;
#endif
            break;
        case VA_VP8:
#if defined (MFX_ENABLE_HW_BLOCKING_TASK_SYNC_VP8D)
            isEnabled = true;
#endif
            break;
        case VA_H265:
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC_H265D)
            isEnabled = true;
#endif
            break;
        case VA_VP9:
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC_VP9D)
            isEnabled = true;
#endif
            break;
#if defined(MFX_ENABLE_AV1_VIDEO_DECODE)
        case VA_AV1:
#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC_AV1D)
            isEnabled = true;
#endif
            break;
#endif
        default:
            break;
        }

        return !isHybrid && isEnabled;
#else
        return false;
#endif
    }

    bool IsLongSliceControl() const { return (!m_bH264ShortSlice); };
    bool IsMVCSupport() const {return m_bH264MVCSupport; };
    bool IsUseStatusReport() const { return m_isUseStatuReport; }
    void SetStatusReportUsing(bool isUseStatuReport) { m_isUseStatuReport = isUseStatuReport; }

    int32_t ScalingListScanOrder() const
    { return m_H265ScalingListScanOrder; }

    virtual void GetVideoDecoder(void **handle) = 0;

    /* Contains private data for the [ExecuteExtension] method */
    struct ExtensionData
    {
        std::pair<void*, size_t> input;      //Pointer & size (in bytes) to private input data passed to the driver.
        std::pair<void*, size_t> output;     //Pointer & size (in bytes) to private output data reveived from the driver.
        std::vector<void*>       resources;  //Array of resource pointers passed to the driver
    };

    /* Executes an extended <function> operation on the current frame. */
    virtual Status ExecuteExtension(int function, ExtensionData const&) = 0;

    VideoAccelerationProfile    m_Profile;          // entry point
    VideoAccelerationPlatform   m_Platform;         // DXVA, LinuxVA, etc
    eMFXHWType                  m_HWPlatform;

protected:

    ProtectedVA       *  m_protectedVA;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    VideoProcessingVA *  m_videoProcessingVA;
#endif

    FrameAllocator    *  m_allocator;

    bool                 m_bH264ShortSlice;
    bool                 m_bH264MVCSupport;
    bool                 m_isUseStatuReport;
    int32_t              m_H265ScalingListScanOrder; //0 - up-right, 1 - raster . Default is 1 (raster).
};

///////////////////////////////////////////////////////////////////////////////////

class UMCVACompBuffer //: public MediaData
{
public:
    UMCVACompBuffer()
    {
        type = -1;
        BufferSize = 0;
        DataSize = 0;
        ptr = NULL;
        PVPState = NULL;

        FirstMb = -1;
        NumOfMB = -1;
        FirstSlice = -1;
        memset(PVPStateBuf, 0, sizeof(PVPStateBuf));
    }
    virtual ~UMCVACompBuffer(){}

    // Set
    virtual Status SetBufferPointer(uint8_t *_ptr, size_t bytes)
    {
        ptr = _ptr;
        BufferSize = (int32_t)bytes;
        return UMC_OK;
    }
    virtual void SetDataSize(int32_t size);
    virtual void SetNumOfItem(int32_t num);
    virtual Status SetPVPState(void *buf, uint32_t size);

    // Get
    int32_t  GetType()       const { return type; }
    void*    GetPtr()        const { return ptr; }
    int32_t  GetBufferSize() const { return BufferSize; }
    int32_t  GetDataSize()   const { return DataSize; }
    void*    GetPVPStatePtr()const { return PVPState; }
    int32_t  GetPVPStateSize()const { return (NULL == PVPState ? 0 : sizeof(PVPStateBuf)); }

    // public fields
    int32_t      FirstMb;
    int32_t      NumOfMB;
    int32_t      FirstSlice;
    int32_t      type;

protected:
    uint8_t       PVPStateBuf[16];
    void*       ptr;
    void*       PVPState;
    int32_t      BufferSize;
    int32_t      DataSize;
};

} // namespace UMC

#endif // __cplusplus

#ifdef _MSVC_LANG
#pragma warning(default : 4201)
#endif

#endif // __UMC_VA_BASE_H__
