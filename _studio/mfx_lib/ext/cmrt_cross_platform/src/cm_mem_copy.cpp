// Copyright (c) 2017-2022 Intel Corporation
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

#include "cm_mem_copy.h"
#include "cm_gpu_copy_code.h"

#include "mfx_common_int.h"
#include "mfx_platform_caps.h"

typedef const mfxU8 mfxUC8;

#define ALIGN128(X) (((mfxU32)((X)+127)) & (~ (mfxU32)127))

#define CHECK_CM_STATUS(_sts, _ret)              \
        if (CM_SUCCESS != _sts)                  \
        {                                        \
            MFX_RETURN(_ret);                    \
        }

#define CHECK_CM_STATUS_RET_NULL(_sts)           \
        if (CM_SUCCESS != MFX_STS_TRACE(_sts))   \
        {                                        \
            return NULL;                         \
        }

#define CHECK_CM_NULL_PTR(_ptr, _ret)            \
        if (NULL == _ptr)                        \
        {                                        \
            MFX_RETURN(_ret);                    \
        }

CmCopyWrapper::~CmCopyWrapper()
{
    Close();
}

#define ADDRESS_PAGE_ALIGNMENT_MASK_X64             0xFFFFFFFFFFFFF000ULL
#define ADDRESS_PAGE_ALIGNMENT_MASK_X86             0xFFFFF000
#define INNER_LOOP                   (4)

#define CHECK_CM_HR(HR) \
    if(HR != CM_SUCCESS)\
    {\
        if(pTS)           m_pCmDevice->DestroyThreadSpace(pTS);\
        if(pGPUCopyTask)  m_pCmDevice->DestroyTask(pGPUCopyTask);\
        if(pInternalEvent)m_pCmQueue->DestroyEvent(pInternalEvent);\
        MFX_RETURN(MFX_ERR_DEVICE_FAILED);\
    }

bool CmCopyWrapper::isSinglePlainFormat(mfxU32 format)
{
    switch (format)
    {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_P010:
    case MFX_FOURCC_YV12:
    case MFX_FOURCC_NV16:
    case MFX_FOURCC_P210:
    case MFX_FOURCC_P016:
        return false;
    case MFX_FOURCC_BGR4:
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_P8:
    case MFX_FOURCC_A2RGB10:
    case MFX_FOURCC_ARGB16:
    case MFX_FOURCC_ABGR16:
    case MFX_FOURCC_R16:
    case MFX_FOURCC_AYUV:
    case MFX_FOURCC_AYUV_RGB4:
    case MFX_FOURCC_UYVY:
    case MFX_FOURCC_YUY2:
    case MFX_FOURCC_Y210:
    case MFX_FOURCC_Y410:
    case MFX_FOURCC_Y216:
    case MFX_FOURCC_Y416:
#ifdef MFX_ENABLE_RGBP
    case MFX_FOURCC_RGBP:
#endif

        return true;
    }
    return false;
}

bool CmCopyWrapper::isNV12LikeFormat(mfxU32 format)
{
    switch (format)
    {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_P010:
    case MFX_FOURCC_P016:
        return true;
    }
    return false;
}

int CmCopyWrapper::getSizePerPixel(mfxU32 format)
{
    switch (format)
    {
    case MFX_FOURCC_P8:
        return 1;
    case MFX_FOURCC_R16:
    case MFX_FOURCC_UYVY:
    case MFX_FOURCC_YUY2:
        return 2;
    case MFX_FOURCC_BGR4:
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_A2RGB10:
    case MFX_FOURCC_AYUV:
    case MFX_FOURCC_AYUV_RGB4:
    case MFX_FOURCC_Y410:
    case MFX_FOURCC_Y210:
    case MFX_FOURCC_Y216:
        return 4;
    case MFX_FOURCC_Y416:
    case MFX_FOURCC_ARGB16:
    case MFX_FOURCC_ABGR16:
        return 8;
    }
    return 0;
}
bool CmCopyWrapper::isNeedSwapping(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)
{
    return (pDst->Info.FourCC == MFX_FOURCC_BGR4 && pSrc->Info.FourCC == MFX_FOURCC_RGB4) ||
           (pDst->Info.FourCC == MFX_FOURCC_RGB4 && pSrc->Info.FourCC == MFX_FOURCC_BGR4) ||
           (pDst->Info.FourCC == MFX_FOURCC_ABGR16 && pSrc->Info.FourCC == MFX_FOURCC_ARGB16) ||
           (pDst->Info.FourCC == MFX_FOURCC_ARGB16 && pSrc->Info.FourCC == MFX_FOURCC_ABGR16);
}
bool CmCopyWrapper::isNeedShift(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)
{
    bool shift = pDst->Info.Shift != pSrc->Info.Shift && pDst->Info.FourCC == pSrc->Info.FourCC;
    //no support for shift in single plane formats currently.
    switch (pDst->Info.FourCC)
    {
    case MFX_FOURCC_P010:
    case MFX_FOURCC_Y210:
    case MFX_FOURCC_Y216:
    case MFX_FOURCC_P016:
    case MFX_FOURCC_Y416:
        return shift;
    }
    return false;
}

CmBufferUPWrapper* CmCopyWrapper::CreateUpBuffer(mfxU8 *pDst, mfxU32 memSize, mfxU32 width, mfxU32 height)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    constexpr size_t CM_MAX_UPBUFFER_TABLE_SIZE = 256;

    auto it = m_tableSysRelations.find(std::tie(pDst, width, height));

    if (m_tableSysRelations.end() != it)
    {
        // If caching is switched off, simply recreate each buffer
        if (it->second.IsFree() && !m_use_cm_buffers_cache)
        {
            CmBufferUP* pCmUserBuffer;
            cmStatus cmSts = m_pCmDevice->CreateBufferUP(memSize, pDst, pCmUserBuffer);
            CHECK_CM_STATUS_RET_NULL(cmSts);

            SurfaceIndex* pCmDstIndex;
            cmSts = pCmUserBuffer->GetIndex(pCmDstIndex);
            CHECK_CM_STATUS_RET_NULL(cmSts);

            CmBufferUPWrapper tmp_buf_to_destroy = std::move(it->second);
            it->second = CmBufferUPWrapper(pCmUserBuffer, pCmDstIndex, m_pCmDevice);
        }

        it->second.AddRef();
        return &(it->second);
    }

    if (m_tableSysRelations.size() == CM_MAX_UPBUFFER_TABLE_SIZE)
    {
        // Delete a free buffer
        auto it_buffer_to_delete = std::find_if(std::begin(m_tableSysRelations), std::end(m_tableSysRelations), [](auto& map_node) { return map_node.second.IsFree(); });
        if (it_buffer_to_delete == std::end(m_tableSysRelations))
        {
            std::ignore = MFX_STS_TRACE(MFX_ERR_MEMORY_ALLOC);
            return nullptr;
        }
        m_tableSysRelations.erase(it_buffer_to_delete);
    }

    CmBufferUP *pCmUserBuffer;
    cmStatus cmSts = m_pCmDevice->CreateBufferUP(memSize, pDst, pCmUserBuffer);
    CHECK_CM_STATUS_RET_NULL(cmSts);

    SurfaceIndex* pCmDstIndex;
    cmSts = pCmUserBuffer->GetIndex(pCmDstIndex);
    CHECK_CM_STATUS_RET_NULL(cmSts);

    auto insert_pair = m_tableSysRelations.emplace(std::piecewise_construct,
        std::forward_as_tuple(pDst, width, height),
        std::forward_as_tuple(CmBufferUPWrapper(pCmUserBuffer, pCmDstIndex, m_pCmDevice)));

    insert_pair.first->second.AddRef();

    return &(insert_pair.first->second);

} // CmBufferUP * CmCopyWrapper::CreateUpBuffer(mfxU8 *pDst, mfxU32 memSize)

class CmBufferUPWrapperScopedLock
{
public:
    CmBufferUPWrapperScopedLock() {}
    CmBufferUPWrapperScopedLock(CmBufferUPWrapper *buf)
    {
        buffers.push_back(buf);
    }
    ~CmBufferUPWrapperScopedLock()
    {
        for (auto buf : buffers)
        {
            if (buf)
                buf->Release();
        }
    }
    void Add(CmBufferUPWrapper* buf)
    {
        buffers.push_back(buf);
    }
private:
    std::vector<CmBufferUPWrapper*> buffers;
};

mfxStatus CmCopyWrapper::EnqueueCopySwapRBGPUtoCPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    (void)option;
    (void)pEvent;

    INT             hr                      = CM_SUCCESS;
    UINT            sizePerPixel            = (format==MFX_FOURCC_ARGB16||format==MFX_FOURCC_ABGR16)? 8: 4;//RGB now
    UINT            stride_in_bytes         = widthStride;
    UINT            stride_in_dwords        = 0;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;
    CmKernel        *m_pCmKernel            = 0;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_dword             = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0;
    UINT            slice_copy_height_row   = 0;
    UINT            sliceCopyBufferUPSize   = 0;
    INT             totalBufferUPSize       = 0;
    UINT            start_x                 = 0;
    UINT            start_y                 = 0;


    MFX_CHECK_NULL_PTR1(pSurface);
    width_byte                      = width * sizePerPixel;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region
    copy_width_byte = std::min      (stride_in_bytes, width_byte);
    copy_height_row = std::min<UINT>(height_stride_in_rows, height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory

    totalBufferUPSize = stride_in_bytes * height_stride_in_rows;

    CmBufferUPWrapperScopedLock buflock;

    while (totalBufferUPSize > 0)
    {
        pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;

        //Calculate  Left Shift offset
        AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
        totalBufferUPSize   += AddedShiftLeftOffset;
        if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH)
        {
            slice_copy_height_row = ((CM_MAX_1D_SURF_WIDTH - AddedShiftLeftOffset)/(stride_in_bytes*(BLOCK_HEIGHT * INNER_LOOP))) * (BLOCK_HEIGHT * INNER_LOOP);
            sliceCopyBufferUPSize = slice_copy_height_row * stride_in_bytes + AddedShiftLeftOffset;
        }
        else
        {
            slice_copy_height_row = copy_height_row;
            sliceCopyBufferUPSize = totalBufferUPSize;
        }

        auto pCmUpBuffer = CreateUpBuffer((mfxU8*)pLinearAddressAligned, sliceCopyBufferUPSize, width, height);
        MFX_CHECK(pCmUpBuffer, MFX_ERR_DEVICE_FAILED);

        buflock.Add(pCmUpBuffer);

        pBufferIndexCM = pCmUpBuffer->GetIndex();
        MFX_CHECK_NULL_PTR1(pBufferIndexCM);

        hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceCopy_readswap_32x32), m_pCmKernel);
        CHECK_CM_HR(hr);
        MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

        hr = pSurface->GetIndex( pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
        threadHeight = ( UINT )ceil( ( double )slice_copy_height_row/BLOCK_HEIGHT/INNER_LOOP );
        threadNum = threadWidth * threadHeight;
        hr = m_pCmKernel->SetThreadCount( threadNum );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pBufferIndexCM );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        width_dword = (UINT)ceil((double)width_byte / 4);
        stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);

        hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &stride_in_dwords );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height_stride_in_rows );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &threadHeight );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 6, sizeof( UINT ), &width_dword );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 7, sizeof( UINT ), &slice_copy_height_row );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 8, sizeof( UINT ), &sizePerPixel );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 9, sizeof( UINT ), &start_x );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 10, sizeof( UINT ), &start_y );
        CHECK_CM_HR(hr);

        hr = m_pCmDevice->CreateTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = pGPUCopyTask->AddKernel( m_pCmKernel );
        CHECK_CM_HR(hr);
        hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyThreadSpace(pTS);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyKernel(m_pCmKernel);
        CHECK_CM_HR(hr);
        pLinearAddress += sliceCopyBufferUPSize - AddedShiftLeftOffset;
        totalBufferUPSize -= sliceCopyBufferUPSize;
        copy_height_row -= slice_copy_height_row;
        start_x = 0;
        start_y += slice_copy_height_row;
        if(totalBufferUPSize > 0)   //Intermediate event, we don't need it
        {
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        else //Last one event, need keep or destroy it
        {
            hr = pInternalEvent->WaitForTaskFinished(m_timeout);
            if(hr == CM_EXCEED_MAX_TIMEOUT)
                return MFX_ERR_GPU_HANG;
            else
                CHECK_CM_HR(hr);
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        CHECK_CM_HR(hr);
    }

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopyGPUtoCPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    (void)option;
    (void)pEvent;

    INT             hr                      = CM_SUCCESS;
    UINT            stride_in_bytes         = widthStride;
    UINT            stride_in_dwords        = 0;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;
    CmKernel        *m_pCmKernel            = 0;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_dword             = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0;
    UINT            slice_copy_height_row   = 0;
    UINT            sliceCopyBufferUPSize   = 0;
    INT             totalBufferUPSize       = 0;
    UINT            start_x                 = 0;
    UINT            start_y                 = 0;


    UINT            sizePerPixel = getSizePerPixel(format);

    if (sizePerPixel == 0)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_CHECK_NULL_PTR1(pSurface);
    width_byte                      = width * sizePerPixel;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region
    copy_width_byte = std::min      (stride_in_bytes, width_byte);
    copy_height_row = std::min<UINT>(height_stride_in_rows, height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory

    totalBufferUPSize = stride_in_bytes * height_stride_in_rows;

    CmBufferUPWrapperScopedLock buflock;

    while (totalBufferUPSize > 0)
    {
            pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;

        //Calculate  Left Shift offset
        AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
        totalBufferUPSize   += AddedShiftLeftOffset;
        if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH)
        {
            slice_copy_height_row = ((CM_MAX_1D_SURF_WIDTH - AddedShiftLeftOffset)/(stride_in_bytes*(BLOCK_HEIGHT * INNER_LOOP))) * (BLOCK_HEIGHT * INNER_LOOP);
            sliceCopyBufferUPSize = slice_copy_height_row * stride_in_bytes + AddedShiftLeftOffset;
        }
        else
        {
            slice_copy_height_row = copy_height_row;
            sliceCopyBufferUPSize = totalBufferUPSize;
        }

        auto pCmUpBuffer = CreateUpBuffer((mfxU8*)pLinearAddressAligned, sliceCopyBufferUPSize, width, height);
        MFX_CHECK(pCmUpBuffer, MFX_ERR_DEVICE_FAILED);

        buflock.Add(pCmUpBuffer);

        pBufferIndexCM = pCmUpBuffer->GetIndex();
        MFX_CHECK_NULL_PTR1(pBufferIndexCM);

        hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceCopy_read_32x32), m_pCmKernel);
        CHECK_CM_HR(hr);
        MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

        hr = pSurface->GetIndex( pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
        threadHeight = ( UINT )ceil( ( double )slice_copy_height_row/BLOCK_HEIGHT/INNER_LOOP );
        threadNum = threadWidth * threadHeight;
        hr = m_pCmKernel->SetThreadCount( threadNum );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pBufferIndexCM );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        width_dword = (UINT)ceil((double)width_byte / 4);
        stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);

        hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &stride_in_dwords );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height_stride_in_rows );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &threadHeight );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 6, sizeof( UINT ), &width_dword );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 7, sizeof( UINT ), &slice_copy_height_row );
        CHECK_CM_HR(hr);
        /*hr = m_pCmKernel->SetKernelArg( 8, sizeof( UINT ), &sizePerPixel );
        CHECK_CM_HR(hr);*/
        hr = m_pCmKernel->SetKernelArg( 8, sizeof( UINT ), &start_x );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 9, sizeof( UINT ), &start_y );
        CHECK_CM_HR(hr);

        hr = m_pCmDevice->CreateTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = pGPUCopyTask->AddKernel( m_pCmKernel );
        CHECK_CM_HR(hr);
        hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyThreadSpace(pTS);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyKernel(m_pCmKernel);
        CHECK_CM_HR(hr);
        pLinearAddress += sliceCopyBufferUPSize - AddedShiftLeftOffset;
        totalBufferUPSize -= sliceCopyBufferUPSize;
        copy_height_row -= slice_copy_height_row;
        start_x = 0;
        start_y += slice_copy_height_row;
        if(totalBufferUPSize > 0)   //Intermediate event, we don't need it
        {
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        else //Last one event, need keep or destroy it
        {
            hr = pInternalEvent->WaitForTaskFinished(m_timeout);
            if(hr == CM_EXCEED_MAX_TIMEOUT)
                return MFX_ERR_GPU_HANG;
            else
                CHECK_CM_HR(hr);
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        CHECK_CM_HR(hr);
    }

    return MFX_ERR_NONE;
}
mfxStatus CmCopyWrapper::EnqueueCopyShiftGPUtoCPU(CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT,
                                    int bitshift,
                                    CmEvent* &)
{
    INT             hr = CM_SUCCESS;
    UINT            stride_in_bytes = widthStride;
    UINT            stride_in_dwords = 0;
    UINT            height_stride_in_rows = heightStride;
    UINT            AddedShiftLeftOffset = 0;
    size_t          pLinearAddress = (size_t)pSysMem;
    size_t          pLinearAddressAligned = 0;
    CmKernel        *m_pCmKernel = 0;
    SurfaceIndex    *pBufferIndexCM = NULL;
    SurfaceIndex    *pSurf2DIndexCM = NULL;
    CmThreadSpace   *pTS = NULL;
    CmTask          *pGPUCopyTask = NULL;
    CmEvent         *pInternalEvent = NULL;

    UINT            threadWidth = 0;
    UINT            threadHeight = 0;
    UINT            threadNum = 0;
    UINT            width_dword = 0;
    UINT            width_byte = 0;
    UINT            copy_width_byte = 0;
    UINT            copy_height_row = 0;
    UINT            slice_copy_height_row = 0;
    UINT            sliceCopyBufferUPSize = 0;
    INT             totalBufferUPSize = 0;
    UINT            start_x = 0;
    UINT            start_y = 0;


    UINT            sizePerPixel = getSizePerPixel(format);

    if (sizePerPixel == 0)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_CHECK_NULL_PTR1(pSurface);
    width_byte = width * sizePerPixel;

    //Align the width regarding stride
    if (stride_in_bytes == 0)
    {
        stride_in_bytes = width_byte;
    }

    if (height_stride_in_rows == 0)
    {
        height_stride_in_rows = height;
    }

    // the actual copy region
    copy_width_byte = std::min      (stride_in_bytes, width_byte);
    copy_height_row = std::min<UINT>(height_stride_in_rows, height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if (stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if ((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory

    totalBufferUPSize = stride_in_bytes * height_stride_in_rows;

    CmBufferUPWrapperScopedLock buflock;

    while (totalBufferUPSize > 0)
    {
        pLinearAddressAligned = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;

        //Calculate  Left Shift offset
        AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
        totalBufferUPSize += AddedShiftLeftOffset;
        if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH)
        {
            slice_copy_height_row = ((CM_MAX_1D_SURF_WIDTH - AddedShiftLeftOffset) / (stride_in_bytes*(BLOCK_HEIGHT * INNER_LOOP))) * (BLOCK_HEIGHT * INNER_LOOP);
            sliceCopyBufferUPSize = slice_copy_height_row * stride_in_bytes + AddedShiftLeftOffset;
        }
        else
        {
            slice_copy_height_row = copy_height_row;
            sliceCopyBufferUPSize = totalBufferUPSize;
        }

        auto pCmUpBuffer = CreateUpBuffer((mfxU8*)pLinearAddressAligned, sliceCopyBufferUPSize, width, height);
        MFX_CHECK(pCmUpBuffer, MFX_ERR_DEVICE_FAILED);

        buflock.Add(pCmUpBuffer);

        pBufferIndexCM = pCmUpBuffer->GetIndex();
        MFX_CHECK_NULL_PTR1(pBufferIndexCM);

        hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceCopy_read_shift_32x32), m_pCmKernel);
        CHECK_CM_HR(hr);
        MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

        hr = pSurface->GetIndex(pSurf2DIndexCM);
        CHECK_CM_HR(hr);
        threadWidth = (UINT)ceil((double)copy_width_byte / BLOCK_PIXEL_WIDTH / 4);
        threadHeight = (UINT)ceil((double)slice_copy_height_row / BLOCK_HEIGHT / INNER_LOOP);
        threadNum = threadWidth * threadHeight;
        hr = m_pCmKernel->SetThreadCount(threadNum);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateThreadSpace(threadWidth, threadHeight, pTS);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg(1, sizeof(SurfaceIndex), pBufferIndexCM);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg(0, sizeof(SurfaceIndex), pSurf2DIndexCM);
        CHECK_CM_HR(hr);
        width_dword = (UINT)ceil((double)width_byte / 4);
        stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);

        hr = m_pCmKernel->SetKernelArg(2, sizeof(UINT), &stride_in_dwords);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg(3, sizeof(UINT), &height_stride_in_rows);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg(4, sizeof(UINT), &AddedShiftLeftOffset);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg(5, sizeof(UINT), &bitshift);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg(6, sizeof(UINT), &threadHeight);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg(7, sizeof(UINT), &width_dword);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg(8, sizeof(UINT), &slice_copy_height_row);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg(9, sizeof(UINT), &start_x);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg(10, sizeof(UINT), &start_y);
        CHECK_CM_HR(hr);

        hr = m_pCmDevice->CreateTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = pGPUCopyTask->AddKernel(m_pCmKernel);
        CHECK_CM_HR(hr);
        hr = m_pCmQueue->Enqueue(pGPUCopyTask, pInternalEvent, pTS);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyThreadSpace(pTS);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyKernel(m_pCmKernel);
        CHECK_CM_HR(hr);
        pLinearAddress += sliceCopyBufferUPSize - AddedShiftLeftOffset;
        totalBufferUPSize -= sliceCopyBufferUPSize;
        copy_height_row -= slice_copy_height_row;
        start_x = 0;
        start_y += slice_copy_height_row;
        if (totalBufferUPSize > 0)   //Intermediate event, we don't need it
        {
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        else //Last one event, need keep or destroy it
        {
            hr = pInternalEvent->WaitForTaskFinished(m_timeout);
            if (hr == CM_EXCEED_MAX_TIMEOUT)
                return MFX_ERR_GPU_HANG;
            else
                CHECK_CM_HR(hr);
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        CHECK_CM_HR(hr);
    }

    return MFX_ERR_NONE;
}
mfxStatus CmCopyWrapper::EnqueueCopySwapRBCPUtoGPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    (void)option;
    (void)pEvent;

    INT             hr                      = CM_SUCCESS;
    UINT            sizePerPixel            = (format==MFX_FOURCC_ARGB16||format==MFX_FOURCC_ABGR16)? 8: 4;//RGB now
    UINT            stride_in_bytes         = widthStride;
    UINT            stride_in_dwords        = 0;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;

    CmKernel        *m_pCmKernel            = 0;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0;
    UINT            slice_copy_height_row   = 0;
    UINT            sliceCopyBufferUPSize   = 0;
    INT             totalBufferUPSize       = 0;
    UINT            start_x                 = 0;
    UINT            start_y                 = 0;

    MFX_CHECK_NULL_PTR1(pSurface);
    width_byte                      = width * sizePerPixel;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region
    copy_width_byte = std::min      (stride_in_bytes, width_byte);
    copy_height_row = std::min<UINT>(height_stride_in_rows, height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory

    totalBufferUPSize = stride_in_bytes * height_stride_in_rows;

    CmBufferUPWrapperScopedLock buflock;

    while (totalBufferUPSize > 0)
    {
            pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;

        //Calculate  Left Shift offset
        AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
        totalBufferUPSize   += AddedShiftLeftOffset;
        if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH)
        {
            slice_copy_height_row = ((CM_MAX_1D_SURF_WIDTH - AddedShiftLeftOffset)/(stride_in_bytes*(BLOCK_HEIGHT * INNER_LOOP))) * (BLOCK_HEIGHT * INNER_LOOP);
            sliceCopyBufferUPSize = slice_copy_height_row * stride_in_bytes + AddedShiftLeftOffset;
        }
        else
        {
            slice_copy_height_row = copy_height_row;
            sliceCopyBufferUPSize = totalBufferUPSize;
        }

        auto pCmUpBuffer = CreateUpBuffer((mfxU8*)pLinearAddressAligned, sliceCopyBufferUPSize, width, height);
        MFX_CHECK(pCmUpBuffer, MFX_ERR_DEVICE_FAILED);

        buflock.Add(pCmUpBuffer);

        pBufferIndexCM = pCmUpBuffer->GetIndex();
        MFX_CHECK_NULL_PTR1(pBufferIndexCM);

        hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceCopy_writeswap_32x32), m_pCmKernel);
        CHECK_CM_HR(hr);
        MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

        hr = pSurface->GetIndex( pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
        threadHeight = ( UINT )ceil( ( double )slice_copy_height_row/BLOCK_HEIGHT/INNER_LOOP );
        threadNum = threadWidth * threadHeight;
        hr = m_pCmKernel->SetThreadCount( threadNum );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
        CHECK_CM_HR(hr);

        m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pBufferIndexCM);
        CHECK_CM_HR(hr);
        m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pSurf2DIndexCM);
        CHECK_CM_HR(hr);


        stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);

        hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &stride_in_dwords );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &slice_copy_height_row );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &threadHeight );
        CHECK_CM_HR(hr);

        //this only works for the kernel surfaceCopy_write_32x32
        hr = m_pCmKernel->SetKernelArg( 6, sizeof( UINT ), &sizePerPixel );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 7, sizeof( UINT ), &start_x );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 8, sizeof( UINT ), &start_y );
        CHECK_CM_HR(hr);

        hr = m_pCmDevice->CreateTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = pGPUCopyTask->AddKernel( m_pCmKernel );
        CHECK_CM_HR(hr);
        hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyThreadSpace(pTS);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyKernel(m_pCmKernel);
        CHECK_CM_HR(hr);
        pLinearAddress += sliceCopyBufferUPSize - AddedShiftLeftOffset;
        totalBufferUPSize -= sliceCopyBufferUPSize;
        copy_height_row -= slice_copy_height_row;
        start_x = 0;
        start_y += slice_copy_height_row;
        if(totalBufferUPSize > 0)   //Intermediate event, we don't need it
        {
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        else //Last one event, need keep or destroy it
        {
            hr = pInternalEvent->WaitForTaskFinished(m_timeout);
            if(hr == CM_EXCEED_MAX_TIMEOUT)
                return MFX_ERR_GPU_HANG;
            else
                CHECK_CM_HR(hr);
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        CHECK_CM_HR(hr);
    }

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopyCPUtoGPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    (void)option;
    (void)pEvent;

    INT             hr                      = CM_SUCCESS;
    UINT            stride_in_bytes         = widthStride;
    UINT            stride_in_dwords        = 0;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;

    CmKernel        *m_pCmKernel            = 0;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0;
    UINT            slice_copy_height_row   = 0;
    UINT            sliceCopyBufferUPSize   = 0;
    INT             totalBufferUPSize       = 0;
    UINT            start_x                 = 0;
    UINT            start_y                 = 0;

    UINT            sizePerPixel = getSizePerPixel(format);

    if (sizePerPixel == 0)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_CHECK_NULL_PTR1(pSurface);
    width_byte                      = width * sizePerPixel;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region
    copy_width_byte = std::min      (stride_in_bytes, width_byte);
    copy_height_row = std::min<UINT>(height_stride_in_rows, height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory

    totalBufferUPSize = stride_in_bytes * height_stride_in_rows;

    CmBufferUPWrapperScopedLock buflock;

    while (totalBufferUPSize > 0)
    {
        pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;

        //Calculate  Left Shift offset
        AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
        totalBufferUPSize   += AddedShiftLeftOffset;
        if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH)
        {
            slice_copy_height_row = ((CM_MAX_1D_SURF_WIDTH - AddedShiftLeftOffset)/(stride_in_bytes*(BLOCK_HEIGHT * INNER_LOOP))) * (BLOCK_HEIGHT * INNER_LOOP);
            sliceCopyBufferUPSize = slice_copy_height_row * stride_in_bytes + AddedShiftLeftOffset;
        }
        else
        {
            slice_copy_height_row = copy_height_row;
            sliceCopyBufferUPSize = totalBufferUPSize;
        }

        auto pCmUpBuffer = CreateUpBuffer((mfxU8*)pLinearAddressAligned, sliceCopyBufferUPSize, width, height);
        MFX_CHECK(pCmUpBuffer, MFX_ERR_DEVICE_FAILED);

        buflock.Add(pCmUpBuffer);

        pBufferIndexCM = pCmUpBuffer->GetIndex();
        MFX_CHECK_NULL_PTR1(pBufferIndexCM);

        hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceCopy_write_32x32), m_pCmKernel);
        CHECK_CM_HR(hr);
        MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

        hr = pSurface->GetIndex( pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
        threadHeight = ( UINT )ceil( ( double )slice_copy_height_row/BLOCK_HEIGHT/INNER_LOOP );
        threadNum = threadWidth * threadHeight;
        hr = m_pCmKernel->SetThreadCount( threadNum );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
        CHECK_CM_HR(hr);

        m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pBufferIndexCM);
        CHECK_CM_HR(hr);
        m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pSurf2DIndexCM);
        CHECK_CM_HR(hr);

        stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);

        hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &stride_in_dwords );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &slice_copy_height_row );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &threadHeight );
        CHECK_CM_HR(hr);

        //this only works for the kernel surfaceCopy_write_32x32
        /*hr = m_pCmKernel->SetKernelArg( 6, sizeof( UINT ), &sizePerPixel );
        CHECK_CM_HR(hr);*/
        hr = m_pCmKernel->SetKernelArg( 6, sizeof( UINT ), &start_x );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 7, sizeof( UINT ), &start_y );
        CHECK_CM_HR(hr);

        hr = m_pCmDevice->CreateTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = pGPUCopyTask->AddKernel( m_pCmKernel );
        CHECK_CM_HR(hr);
        hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyThreadSpace(pTS);
        CHECK_CM_HR(hr);

        hr = m_pCmDevice->DestroyKernel(m_pCmKernel);
        CHECK_CM_HR(hr);
        pLinearAddress += sliceCopyBufferUPSize - AddedShiftLeftOffset;
        totalBufferUPSize -= sliceCopyBufferUPSize;
        copy_height_row -= slice_copy_height_row;
        start_x = 0;
        start_y += slice_copy_height_row;
        if(totalBufferUPSize > 0)   //Intermediate event, we don't need it
        {
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        else //Last one event, need keep or destroy it
        {
            hr = pInternalEvent->WaitForTaskFinished(m_timeout);
            if(hr == CM_EXCEED_MAX_TIMEOUT)
                return MFX_ERR_GPU_HANG;
            else
                CHECK_CM_HR(hr);
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        CHECK_CM_HR(hr);
    }

    return MFX_ERR_NONE;
}
mfxStatus CmCopyWrapper::EnqueueCopyShiftCPUtoGPU(CmSurface2D* pSurface,
    unsigned char* pSysMem,
    int width,
    int height,
    const UINT widthStride,
    const UINT heightStride,
    mfxU32 format,
    int,
    const UINT,
    CmEvent* &)
{
    INT             hr = CM_SUCCESS;
    UINT            stride_in_bytes = widthStride;
    UINT            stride_in_dwords = 0;
    UINT            height_stride_in_rows = heightStride;
    UINT            AddedShiftLeftOffset = 0;
    size_t          pLinearAddress = (size_t)pSysMem;
    size_t          pLinearAddressAligned = 0;

    CmKernel        *m_pCmKernel = 0;
    SurfaceIndex    *pBufferIndexCM = NULL;
    SurfaceIndex    *pSurf2DIndexCM = NULL;
    CmThreadSpace   *pTS = NULL;
    CmTask          *pGPUCopyTask = NULL;
    CmEvent         *pInternalEvent = NULL;

    UINT            threadWidth = 0;
    UINT            threadHeight = 0;
    UINT            threadNum = 0;
    UINT            width_byte = 0;
    UINT            copy_width_byte = 0;
    UINT            copy_height_row = 0;
    UINT            slice_copy_height_row = 0;
    UINT            sliceCopyBufferUPSize = 0;
    INT             totalBufferUPSize = 0;
    UINT            start_x = 0;
    UINT            start_y = 0;

    UINT            sizePerPixel = getSizePerPixel(format);

    if (sizePerPixel == 0)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_CHECK_NULL_PTR1(pSurface);
    width_byte = width * sizePerPixel;

    //Align the width regarding stride
    if (stride_in_bytes == 0)
    {
        stride_in_bytes = width_byte;
    }

    if (height_stride_in_rows == 0)
    {
        height_stride_in_rows = height;
    }

    // the actual copy region
    copy_width_byte = std::min      (stride_in_bytes, width_byte);
    copy_height_row = std::min<UINT>(height_stride_in_rows, height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if (stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if ((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory

    totalBufferUPSize = stride_in_bytes * height_stride_in_rows;

    CmBufferUPWrapperScopedLock buflock;

    while (totalBufferUPSize > 0)
    {
        pLinearAddressAligned = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;

        //Calculate  Left Shift offset
        AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
        totalBufferUPSize += AddedShiftLeftOffset;
        if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH)
        {
            slice_copy_height_row = ((CM_MAX_1D_SURF_WIDTH - AddedShiftLeftOffset) / (stride_in_bytes*(BLOCK_HEIGHT * INNER_LOOP))) * (BLOCK_HEIGHT * INNER_LOOP);
            sliceCopyBufferUPSize = slice_copy_height_row * stride_in_bytes + AddedShiftLeftOffset;
        }
        else
        {
            slice_copy_height_row = copy_height_row;
            sliceCopyBufferUPSize = totalBufferUPSize;
        }

        auto pCmUpBuffer = CreateUpBuffer((mfxU8*)pLinearAddressAligned, sliceCopyBufferUPSize, width, height);
        MFX_CHECK(pCmUpBuffer, MFX_ERR_DEVICE_FAILED);

        buflock.Add(pCmUpBuffer);

        pBufferIndexCM = pCmUpBuffer->GetIndex();
        MFX_CHECK_NULL_PTR1(pBufferIndexCM);

        hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceCopy_write_shift_32x32), m_pCmKernel);
        CHECK_CM_HR(hr);
        MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

        hr = pSurface->GetIndex(pSurf2DIndexCM);
        CHECK_CM_HR(hr);
        threadWidth = (UINT)ceil((double)copy_width_byte / BLOCK_PIXEL_WIDTH / 4);
        threadHeight = (UINT)ceil((double)slice_copy_height_row / BLOCK_HEIGHT / INNER_LOOP);
        threadNum = threadWidth * threadHeight;
        hr = m_pCmKernel->SetThreadCount(threadNum);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateThreadSpace(threadWidth, threadHeight, pTS);
        CHECK_CM_HR(hr);

        m_pCmKernel->SetKernelArg(0, sizeof(SurfaceIndex), pBufferIndexCM);
        CHECK_CM_HR(hr);
        m_pCmKernel->SetKernelArg(1, sizeof(SurfaceIndex), pSurf2DIndexCM);
        CHECK_CM_HR(hr);

        stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);

        hr = m_pCmKernel->SetKernelArg(2, sizeof(UINT), &stride_in_dwords);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg(3, sizeof(UINT), &slice_copy_height_row);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg(4, sizeof(UINT), &AddedShiftLeftOffset);
        CHECK_CM_HR(hr);

        hr = m_pCmKernel->SetKernelArg(5, sizeof( UINT ), &sizePerPixel );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg(6, sizeof(UINT), &threadHeight);
        CHECK_CM_HR(hr);

        hr = m_pCmKernel->SetKernelArg(7, sizeof(UINT), &start_x);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg(8, sizeof(UINT), &start_y);
        CHECK_CM_HR(hr);

        hr = m_pCmDevice->CreateTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = pGPUCopyTask->AddKernel(m_pCmKernel);
        CHECK_CM_HR(hr);
        hr = m_pCmQueue->Enqueue(pGPUCopyTask, pInternalEvent, pTS);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyThreadSpace(pTS);
        CHECK_CM_HR(hr);

        hr = m_pCmDevice->DestroyKernel(m_pCmKernel);
        CHECK_CM_HR(hr);
        pLinearAddress += sliceCopyBufferUPSize - AddedShiftLeftOffset;
        totalBufferUPSize -= sliceCopyBufferUPSize;
        copy_height_row -= slice_copy_height_row;
        start_x = 0;
        start_y += slice_copy_height_row;
        if (totalBufferUPSize > 0)   //Intermediate event, we don't need it
        {
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        else //Last one event, need keep or destroy it
        {
            hr = pInternalEvent->WaitForTaskFinished(m_timeout);
            if (hr == CM_EXCEED_MAX_TIMEOUT)
                return MFX_ERR_GPU_HANG;
            else
                CHECK_CM_HR(hr);
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        CHECK_CM_HR(hr);
    }

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopySwapRBGPUtoGPU(   CmSurface2D* pSurfaceIn,
                                    CmSurface2D* pSurfaceOut,
                                    int width,
                                    int height,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    (void)option;
    (void)pEvent;

    INT             hr                      = CM_SUCCESS;
    UINT            sizePerPixel            = (format==MFX_FOURCC_ARGB16||format==MFX_FOURCC_ABGR16)? 8: 4;//RGB now

    SurfaceIndex    *pSurf2DIndexCM_In  = NULL;
    SurfaceIndex    *pSurf2DIndexCM_Out = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    CmKernel        *m_pCmKernel            = 0;
    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;

    if ( !pSurfaceIn || !pSurfaceOut )
    {
        return MFX_ERR_NULL_PTR;
    }

    hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(SurfaceCopySwap_2DTo2D_32x32), m_pCmKernel);
    CHECK_CM_HR(hr);

    MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

    hr = pSurfaceOut->GetIndex( pSurf2DIndexCM_Out );
    CHECK_CM_HR(hr);
    hr = pSurfaceIn->GetIndex( pSurf2DIndexCM_In );
    CHECK_CM_HR(hr);

    threadWidth = ( UINT )ceil( ( double )width/BLOCK_PIXEL_WIDTH );
    threadHeight = ( UINT )ceil( ( double )height/BLOCK_HEIGHT/INNER_LOOP );
    threadNum = threadWidth * threadHeight;
    hr = m_pCmKernel->SetThreadCount( threadNum );
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
    CHECK_CM_HR(hr);

    m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pSurf2DIndexCM_In);
    m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pSurf2DIndexCM_Out);

    hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &threadHeight );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &sizePerPixel );
    CHECK_CM_HR(hr);

    hr = m_pCmDevice->CreateTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = pGPUCopyTask->AddKernel( m_pCmKernel );
    CHECK_CM_HR(hr);
    hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
    CHECK_CM_HR(hr);

    hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyThreadSpace(pTS);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyKernel(m_pCmKernel);
    CHECK_CM_HR(hr);
    hr = pInternalEvent->WaitForTaskFinished(m_timeout);

    if(hr == CM_EXCEED_MAX_TIMEOUT)
        return MFX_ERR_GPU_HANG;
    else
        CHECK_CM_HR(hr);
    hr = m_pCmQueue->DestroyEvent(pInternalEvent);
    CHECK_CM_HR(hr);

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopyMirrorGPUtoGPU(   CmSurface2D* pSurfaceIn,
                                    CmSurface2D* pSurfaceOut,
                                    int width,
                                    int height,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    (void)format;
    (void)option;
    (void)pEvent;

    INT             hr                      = CM_SUCCESS;
//    UINT            sizePerPixel            = (format==MFX_FOURCC_ARGB16||format==MFX_FOURCC_ABGR16)? 8: 4;//RGB now

    SurfaceIndex    *pSurf2DIndexCM_In  = NULL;
    SurfaceIndex    *pSurf2DIndexCM_Out = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    CmKernel        *m_pCmKernel            = 0;
    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;

    MFX_CHECK_NULL_PTR2(pSurfaceIn, pSurfaceOut);

    hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(SurfaceMirror_2DTo2D_NV12), m_pCmKernel);
    CHECK_CM_HR(hr);

    MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

    hr = pSurfaceOut->GetIndex( pSurf2DIndexCM_Out );
    CHECK_CM_HR(hr);
    hr = pSurfaceIn->GetIndex( pSurf2DIndexCM_In );
    CHECK_CM_HR(hr);

    threadWidth = ( UINT )ceil( ( double )width/BLOCK_PIXEL_WIDTH );
    threadHeight = ( UINT )ceil( ( double )height/BLOCK_HEIGHT );
    threadNum = threadWidth * threadHeight;
    hr = m_pCmKernel->SetThreadCount( threadNum );
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
    CHECK_CM_HR(hr);

    m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pSurf2DIndexCM_In);
    m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pSurf2DIndexCM_Out);

    hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &width );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height );
    CHECK_CM_HR(hr);

    hr = m_pCmDevice->CreateTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = pGPUCopyTask->AddKernel( m_pCmKernel );
    CHECK_CM_HR(hr);
    hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
    CHECK_CM_HR(hr);

    hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyThreadSpace(pTS);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyKernel(m_pCmKernel);
    CHECK_CM_HR(hr);
    hr = pInternalEvent->WaitForTaskFinished(m_timeout);

    if(hr == CM_EXCEED_MAX_TIMEOUT)
        return MFX_ERR_GPU_HANG;
    else
        CHECK_CM_HR(hr);
    hr = m_pCmQueue->DestroyEvent(pInternalEvent);
    CHECK_CM_HR(hr);

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopyMirrorNV12GPUtoCPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    (void)format;
    (void)option;
    (void)pEvent;

    INT             hr                      = CM_SUCCESS;
    UINT            stride_in_bytes         = widthStride;
    UINT            stride_in_dwords        = 0;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;
    CmKernel        *m_pCmKernel            = 0;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_dword             = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0;
    INT             totalBufferUPSize       = 0;

    MFX_CHECK_NULL_PTR1(pSurface);
    width_byte                      = width;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region
    copy_width_byte = std::min      (stride_in_bytes, width_byte);
    copy_height_row = std::min<UINT>(height_stride_in_rows, height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory

    totalBufferUPSize = stride_in_bytes * height_stride_in_rows + stride_in_bytes * height/2;

    pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;

    //Calculate  Left Shift offset
    AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
    totalBufferUPSize   += AddedShiftLeftOffset;
    MFX_CHECK(totalBufferUPSize <= CM_MAX_1D_SURF_WIDTH, MFX_ERR_DEVICE_FAILED);

    auto pCmUpBuffer = CreateUpBuffer((mfxU8*)pLinearAddressAligned, totalBufferUPSize, width, height);
    MFX_CHECK(pCmUpBuffer, MFX_ERR_DEVICE_FAILED);

    CmBufferUPWrapperScopedLock buflock(pCmUpBuffer);

    pBufferIndexCM = pCmUpBuffer->GetIndex();
    MFX_CHECK_NULL_PTR1(pBufferIndexCM);

    hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceMirror_read_NV12), m_pCmKernel);
    CHECK_CM_HR(hr);
    MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

    hr = pSurface->GetIndex( pSurf2DIndexCM );
    CHECK_CM_HR(hr);
    threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
    threadHeight = ( UINT )ceil( ( double )copy_height_row /BLOCK_HEIGHT );
    threadNum = threadWidth * threadHeight;
    hr = m_pCmKernel->SetThreadCount( threadNum );
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pBufferIndexCM );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pSurf2DIndexCM );
    CHECK_CM_HR(hr);
    width_dword = (UINT)ceil((double)width_byte / 4);
    stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);

    hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &stride_in_dwords );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height);
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &width_dword );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 6, sizeof( UINT ), &height_stride_in_rows );
    CHECK_CM_HR(hr);
    //hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &threadHeight );
    //CHECK_CM_HR(hr);
    //hr = m_pCmKernel->SetKernelArg( 7, sizeof( UINT ), &copy_height_row );
    //CHECK_CM_HR(hr);
    /*hr = m_pCmKernel->SetKernelArg( 9, sizeof( UINT ), &start_x );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 10, sizeof( UINT ), &start_y );
    CHECK_CM_HR(hr);*/

    hr = m_pCmDevice->CreateTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = pGPUCopyTask->AddKernel( m_pCmKernel );
    CHECK_CM_HR(hr);
    hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyThreadSpace(pTS);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyKernel(m_pCmKernel);
    CHECK_CM_HR(hr);

    hr = pInternalEvent->WaitForTaskFinished(m_timeout);
    if(hr == CM_EXCEED_MAX_TIMEOUT)
        return MFX_ERR_GPU_HANG;
    else
        CHECK_CM_HR(hr);
    hr = m_pCmQueue->DestroyEvent(pInternalEvent);
    CHECK_CM_HR(hr);

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopyNV12GPUtoCPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    (void)option;
    (void)pEvent;

    INT             hr                      = CM_SUCCESS;
    UINT            stride_in_bytes         = widthStride;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    UINT            byte_per_pixel          = (format==MFX_FOURCC_P010 || format == MFX_FOURCC_P016)?2:1;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;
    CmKernel        *m_pCmKernel            = 0;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_dword             = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0;
    INT             totalBufferUPSize       = 0;

    MFX_CHECK_NULL_PTR1(pSurface);
    width_byte                      = width*byte_per_pixel;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region
    copy_width_byte = std::min      (stride_in_bytes, width_byte);
    copy_height_row = std::min<UINT>(height_stride_in_rows, height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory

    totalBufferUPSize = stride_in_bytes * height_stride_in_rows + stride_in_bytes * height/2;
    if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH || height>4088)//not supported by kernel now, to be fixed in case of customer requirements
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;

    //Calculate  Left Shift offset
    AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
    totalBufferUPSize   += AddedShiftLeftOffset;
    MFX_CHECK(totalBufferUPSize <= CM_MAX_1D_SURF_WIDTH, MFX_ERR_DEVICE_FAILED);

    auto pCmUpBuffer = CreateUpBuffer((mfxU8*)pLinearAddressAligned, totalBufferUPSize, width, height);
    MFX_CHECK(pCmUpBuffer, MFX_ERR_DEVICE_FAILED);

    CmBufferUPWrapperScopedLock buflock(pCmUpBuffer);

    pBufferIndexCM = pCmUpBuffer->GetIndex();
    MFX_CHECK_NULL_PTR1(pBufferIndexCM);

    hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceCopy_read_NV12), m_pCmKernel);
    CHECK_CM_HR(hr);
    MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

    hr = pSurface->GetIndex( pSurf2DIndexCM );
    CHECK_CM_HR(hr);
    threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
    threadHeight = ( UINT )ceil( ( double )copy_height_row/BLOCK_HEIGHT );
    threadNum = threadWidth * threadHeight;
    hr = m_pCmKernel->SetThreadCount( threadNum );
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pBufferIndexCM );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pSurf2DIndexCM );
    CHECK_CM_HR(hr);
    width_dword = (UINT)ceil((double)width_byte / 4);

    hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &width_dword );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height);
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &heightStride );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 6, sizeof( UINT ), &widthStride );
    CHECK_CM_HR(hr);

    hr = m_pCmDevice->CreateTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = pGPUCopyTask->AddKernel( m_pCmKernel );
    CHECK_CM_HR(hr);
    hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyThreadSpace(pTS);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyKernel(m_pCmKernel);
    CHECK_CM_HR(hr);

    hr = pInternalEvent->WaitForTaskFinished(m_timeout);
    if(hr == CM_EXCEED_MAX_TIMEOUT)
        return MFX_ERR_GPU_HANG;
    else
        CHECK_CM_HR(hr);
    hr = m_pCmQueue->DestroyEvent(pInternalEvent);
    CHECK_CM_HR(hr);

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopyMirrorNV12CPUtoGPU(CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    (void)format;
    (void)option;
    (void)pEvent;

    INT             hr                      = CM_SUCCESS;
    UINT            stride_in_bytes         = widthStride;
    UINT            stride_in_dwords        = 0;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;
    CmKernel        *m_pCmKernel            = 0;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_dword             = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0;
    INT             totalBufferUPSize       = 0;

    MFX_CHECK_NULL_PTR1(pSurface);
    width_byte                      = width;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region
    copy_width_byte = std::min      (stride_in_bytes, width_byte);
    copy_height_row = std::min<UINT>(height_stride_in_rows, height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory

    totalBufferUPSize = stride_in_bytes * height_stride_in_rows + stride_in_bytes * height/2;

    pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;

    //Calculate  Left Shift offset
    AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
    totalBufferUPSize   += AddedShiftLeftOffset;
    MFX_CHECK(totalBufferUPSize <= CM_MAX_1D_SURF_WIDTH, MFX_ERR_DEVICE_FAILED);

    auto pCmUpBuffer = CreateUpBuffer((mfxU8*)pLinearAddressAligned, totalBufferUPSize, width, height);
    MFX_CHECK(pCmUpBuffer, MFX_ERR_DEVICE_FAILED);

    CmBufferUPWrapperScopedLock buflock(pCmUpBuffer);

    pBufferIndexCM = pCmUpBuffer->GetIndex();
    MFX_CHECK_NULL_PTR1(pBufferIndexCM);

    hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceMirror_write_NV12), m_pCmKernel);
    CHECK_CM_HR(hr);
    MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

    hr = pSurface->GetIndex( pSurf2DIndexCM );
    CHECK_CM_HR(hr);
    threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
    threadHeight = ( UINT )ceil( ( double )copy_height_row /BLOCK_HEIGHT );
    threadNum = threadWidth * threadHeight;
    hr = m_pCmKernel->SetThreadCount( threadNum );
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pBufferIndexCM );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pSurf2DIndexCM );
    CHECK_CM_HR(hr);
    width_dword = (UINT)ceil((double)width_byte / 4);
    stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);

    hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &stride_in_dwords );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height_stride_in_rows);
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &width_dword );
    CHECK_CM_HR(hr);

    hr = m_pCmDevice->CreateTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = pGPUCopyTask->AddKernel( m_pCmKernel );
    CHECK_CM_HR(hr);
    hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyThreadSpace(pTS);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyKernel(m_pCmKernel);
    CHECK_CM_HR(hr);

    hr = pInternalEvent->WaitForTaskFinished(m_timeout);
    if(hr == CM_EXCEED_MAX_TIMEOUT)
        return MFX_ERR_GPU_HANG;
    else
        CHECK_CM_HR(hr);
    hr = m_pCmQueue->DestroyEvent(pInternalEvent);
    CHECK_CM_HR(hr);

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopyNV12CPUtoGPU(CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    (void)option;
    (void)pEvent;

    INT             hr                      = CM_SUCCESS;
    UINT            stride_in_bytes         = widthStride;
    UINT            height_stride_in_rows   = heightStride;
    UINT            byte_per_pixel          = (format==MFX_FOURCC_P010 || format == MFX_FOURCC_P016)?2:1;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;
    CmKernel        *m_pCmKernel            = 0;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_dword             = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0;
    INT             totalBufferUPSize       = 0;

    MFX_CHECK_NULL_PTR1(pSurface);
    width_byte                      = width*byte_per_pixel;

    //Align the width regarding stride
    if(stride_in_bytes == 0)
    {
         stride_in_bytes = width_byte;
    }

    if(height_stride_in_rows == 0)
    {
         height_stride_in_rows = height;
    }

    // the actual copy region
    copy_width_byte = std::min      (stride_in_bytes, width_byte);
    copy_height_row = std::min<UINT>(height_stride_in_rows, height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory

    totalBufferUPSize = stride_in_bytes * height_stride_in_rows + stride_in_bytes * height/2;
    if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH || height > 4088)//not supported by kernel now, to be fixed in case of customer requirements
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;

    //Calculate  Left Shift offset
    AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
    totalBufferUPSize   += AddedShiftLeftOffset;
    MFX_CHECK(totalBufferUPSize <= CM_MAX_1D_SURF_WIDTH, MFX_ERR_DEVICE_FAILED);

    auto pCmUpBuffer = CreateUpBuffer((mfxU8*)pLinearAddressAligned, totalBufferUPSize, width, height);
    MFX_CHECK(pCmUpBuffer, MFX_ERR_DEVICE_FAILED);

    CmBufferUPWrapperScopedLock buflock(pCmUpBuffer);

    pBufferIndexCM = pCmUpBuffer->GetIndex();
    MFX_CHECK_NULL_PTR1(pBufferIndexCM);

    hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceCopy_write_NV12), m_pCmKernel);
    CHECK_CM_HR(hr);
    MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

    hr = pSurface->GetIndex( pSurf2DIndexCM );
    CHECK_CM_HR(hr);
    threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
    threadHeight = ( UINT )ceil( ( double )copy_height_row /BLOCK_HEIGHT );
    threadNum = threadWidth * threadHeight;
    hr = m_pCmKernel->SetThreadCount( threadNum );
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pBufferIndexCM );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pSurf2DIndexCM );
    CHECK_CM_HR(hr);
    width_dword = (UINT)ceil((double)width_byte / 4);

    hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &width_dword );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height);
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &stride_in_bytes );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 6, sizeof( UINT ), &height_stride_in_rows );
    CHECK_CM_HR(hr);

    hr = m_pCmDevice->CreateTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = pGPUCopyTask->AddKernel( m_pCmKernel );
    CHECK_CM_HR(hr);
    hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyThreadSpace(pTS);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyKernel(m_pCmKernel);
    CHECK_CM_HR(hr);

    hr = pInternalEvent->WaitForTaskFinished(m_timeout);
    if(hr == CM_EXCEED_MAX_TIMEOUT)
        return MFX_ERR_GPU_HANG;
    else
        CHECK_CM_HR(hr);
    hr = m_pCmQueue->DestroyEvent(pInternalEvent);
    CHECK_CM_HR(hr);

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopyShiftP010GPUtoCPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    int bitshift,
                                    CmEvent* & pEvent )
{
    (void)format;
    (void)option;
    (void)pEvent;

    INT             hr                      = CM_SUCCESS;
    UINT            stride_in_bytes         = widthStride;
    UINT            stride_in_dwords        = 0;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;
    CmKernel        *m_pCmKernel            = 0;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;//(CmEvent*)-1;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_dword             = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0;
    INT             totalBufferUPSize       = 0;

    MFX_CHECK_NULL_PTR1(pSurface);
    width_byte                      = width*2;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region
    copy_width_byte = std::min      (stride_in_bytes, width_byte);
    copy_height_row = std::min<UINT>(height_stride_in_rows, height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory
    totalBufferUPSize = stride_in_bytes * height_stride_in_rows + stride_in_bytes * height/2;

    pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;

    //Calculate  Left Shift offset
    AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
    totalBufferUPSize   += AddedShiftLeftOffset;
    MFX_CHECK(totalBufferUPSize <= CM_MAX_1D_SURF_WIDTH, MFX_ERR_DEVICE_FAILED);

    auto pCmUpBuffer = CreateUpBuffer((mfxU8*)pLinearAddressAligned, totalBufferUPSize, width, height);
    MFX_CHECK(pCmUpBuffer, MFX_ERR_DEVICE_FAILED);

    CmBufferUPWrapperScopedLock buflock(pCmUpBuffer);

    pBufferIndexCM = pCmUpBuffer->GetIndex();
    MFX_CHECK_NULL_PTR1(pBufferIndexCM);

    hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceCopy_read_P010_shift), m_pCmKernel);
    CHECK_CM_HR(hr);
    MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

    hr = pSurface->GetIndex( pSurf2DIndexCM );
    CHECK_CM_HR(hr);
    threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
    threadHeight = ( UINT )ceil( ( double )copy_height_row /BLOCK_HEIGHT );
    threadNum = threadWidth * threadHeight;
    hr = m_pCmKernel->SetThreadCount( threadNum );
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pBufferIndexCM );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pSurf2DIndexCM );
    CHECK_CM_HR(hr);
    width_dword = (UINT)ceil((double)width_byte / 4);
    stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);

    hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &width_dword );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height);
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &bitshift );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 6, sizeof( UINT ), &stride_in_dwords );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 7, sizeof( UINT ), &height_stride_in_rows );
    CHECK_CM_HR(hr);

    hr = m_pCmDevice->CreateTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = pGPUCopyTask->AddKernel( m_pCmKernel );
    CHECK_CM_HR(hr);
    hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyThreadSpace(pTS);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyKernel(m_pCmKernel);
    CHECK_CM_HR(hr);

    hr = pInternalEvent->WaitForTaskFinished(m_timeout);
    if(hr == CM_EXCEED_MAX_TIMEOUT)
        return MFX_ERR_GPU_HANG;
    else
        CHECK_CM_HR(hr);
    hr = m_pCmQueue->DestroyEvent(pInternalEvent);
    CHECK_CM_HR(hr);

    return MFX_ERR_NONE;
}
mfxStatus CmCopyWrapper::EnqueueCopyShiftP010CPUtoGPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride,
                                    const UINT heightStride,
                                    mfxU32 format,
                                    const UINT option,
                                    int bitshift,
                                    CmEvent* & pEvent )
{
    (void)format;
    (void)option;
    (void)pEvent;

    INT             hr                      = CM_SUCCESS;
    UINT            stride_in_bytes         = widthStride;
    UINT            stride_in_dwords        = 0;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;

    CmKernel        *m_pCmKernel        = NULL;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0;
    UINT            slice_copy_height_row   = 0;
    UINT            sliceCopyBufferUPSize   = 0;
    INT             totalBufferUPSize       = 0;


    MFX_CHECK_NULL_PTR1(pSurface);
    width_byte                      = width * 2;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region
    copy_width_byte = std::min      (stride_in_bytes, width_byte);
    copy_height_row = std::min<UINT>(height_stride_in_rows, height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory

    totalBufferUPSize = stride_in_bytes * height_stride_in_rows + stride_in_bytes * height/2;

    CmBufferUPWrapperScopedLock buflock;

    while (totalBufferUPSize > 0)
    {
        pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;

        //Calculate  Left Shift offset
        AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
        totalBufferUPSize   += AddedShiftLeftOffset;
        if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH)
        {
            slice_copy_height_row = ((CM_MAX_1D_SURF_WIDTH - AddedShiftLeftOffset)/(stride_in_bytes*(BLOCK_HEIGHT * INNER_LOOP))) * (BLOCK_HEIGHT * INNER_LOOP);
            sliceCopyBufferUPSize = slice_copy_height_row * stride_in_bytes + AddedShiftLeftOffset;
        }
        else
        {
            slice_copy_height_row = copy_height_row;
            sliceCopyBufferUPSize = totalBufferUPSize;
        }

        auto pCmUpBuffer = CreateUpBuffer((mfxU8*)pLinearAddressAligned, sliceCopyBufferUPSize, width, height);
        MFX_CHECK(pCmUpBuffer, MFX_ERR_DEVICE_FAILED);

        buflock.Add(pCmUpBuffer);

        pBufferIndexCM = pCmUpBuffer->GetIndex();
        MFX_CHECK_NULL_PTR1(pBufferIndexCM);

        hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceCopy_write_P010_shift), m_pCmKernel);
        CHECK_CM_HR(hr);
        MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

        hr = pSurface->GetIndex( pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
        threadHeight = ( UINT )ceil( ( double )slice_copy_height_row/BLOCK_HEIGHT );
        threadNum = threadWidth * threadHeight;
        hr = m_pCmKernel->SetThreadCount( threadNum );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
        CHECK_CM_HR(hr);

        m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pBufferIndexCM);
        CHECK_CM_HR(hr);
        m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pSurf2DIndexCM);
        CHECK_CM_HR(hr);
        stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);
        hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &stride_in_dwords );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height_stride_in_rows );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &bitshift );
        CHECK_CM_HR(hr);

        hr = m_pCmDevice->CreateTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = pGPUCopyTask->AddKernel( m_pCmKernel );
        CHECK_CM_HR(hr);
        hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyThreadSpace(pTS);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyKernel(m_pCmKernel);
        CHECK_CM_HR(hr);
        pLinearAddress += sliceCopyBufferUPSize - AddedShiftLeftOffset;
        totalBufferUPSize -= sliceCopyBufferUPSize;
        copy_height_row -= slice_copy_height_row;
        if(totalBufferUPSize > 0)   //Intermediate event, we don't need it
        {
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        else //Last one event, need keep or destroy it
        {
            hr = pInternalEvent->WaitForTaskFinished(m_timeout);
            if(hr == CM_EXCEED_MAX_TIMEOUT)
                return MFX_ERR_GPU_HANG;
            else
                CHECK_CM_HR(hr);
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        CHECK_CM_HR(hr);
    }

    return MFX_ERR_NONE;
}


inline INT GetTimeout(mfxU16 hwDeviceId, eMFXHWType hw_type)
{
    std::ignore = hw_type;
    std::ignore = hwDeviceId;

    return CM_MAX_TIMEOUT_MS;
}

mfxStatus CmCopyWrapper::Initialize(mfxU16 hwDeviceId, eMFXHWType hwtype)
{
    cmStatus cmSts = CM_SUCCESS;

    if (!m_pCmDevice)
        return MFX_ERR_DEVICE_FAILED;

    m_HWType = hwtype;
    if (m_HWType == MFX_HW_UNKNOWN)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    m_timeout = GetTimeout(hwDeviceId, m_HWType);

    mfxStatus mfxSts = InitializeSwapKernels(m_HWType);
    MFX_CHECK_STS(mfxSts);

    cmSts = m_pCmDevice->CreateQueue(m_pCmQueue);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);
    m_tableCmRelations.clear();

    m_tableSysRelations.clear();

    return MFX_ERR_NONE;

} // mfxStatus CmCopyWrapper::Initialize(void)

mfxStatus CmCopyWrapper::InitializeSwapKernels(eMFXHWType hwtype)
{
    if (m_bSwapKernelsInitialized)
        return MFX_ERR_NONE;

    cmStatus cmSts = CM_SUCCESS;

    if (!m_pCmDevice)
        return MFX_ERR_DEVICE_FAILED;

#ifdef MFX_ENABLE_KERNELS
    switch (hwtype)
    {
    case MFX_HW_TGL_LP:
    case MFX_HW_DG1:
    case MFX_HW_RKL:
    case MFX_HW_ADL_S:
    case MFX_HW_ADL_P:
    case MFX_HW_ADL_N:
        cmSts = m_pCmDevice->LoadProgram((void*)genx_copy_kernel_gen12lp,sizeof(genx_copy_kernel_gen12lp),m_pCmProgram,"nojitter");
        break;
    default:
        cmSts = CM_FAILURE;
        break;
    }
#endif
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    m_bSwapKernelsInitialized = true;

    return MFX_ERR_NONE;

} // mfxStatus CmCopyWrapper::InitializeSwapKernels(eMFXHWType hwtype)

void CmCopyWrapper::CleanUpCache()
{
    std::lock_guard<std::mutex> guard(m_mutex);

    m_tableCmRelations.clear();
    m_tableSysRelations.clear();
}

void CmCopyWrapper::Close()
{
    CleanUpCache();

    if (!m_pCmDevice)
        return;

    if (m_pCmProgram)
    {
        std::ignore = MFX_STS_TRACE(m_pCmDevice->DestroyProgram(m_pCmProgram));
        m_pCmProgram = nullptr;
    }

    if (m_pThreadSpace)
    {
        std::ignore = MFX_STS_TRACE(m_pCmDevice->DestroyThreadSpace(m_pThreadSpace));
        m_pThreadSpace = nullptr;
    }

    if (m_pCmTask1)
    {
        std::ignore = MFX_STS_TRACE(m_pCmDevice->DestroyTask(m_pCmTask1));
        m_pCmTask1 = nullptr;
    }

    if (m_pCmTask2)
    {
        std::ignore = MFX_STS_TRACE(m_pCmDevice->DestroyTask(m_pCmTask2));
        m_pCmTask2 = nullptr;
    }

    std::ignore = MFX_STS_TRACE(DestroyCmDevice(m_pCmDevice));
    m_pCmDevice = nullptr;
} // mfxStatus CmCopyWrapper::Close()

CmSurface2DWrapper* CmCopyWrapper::CreateCmSurface2D(mfxHDLPair surfaceIdPair, mfxU32 width, mfxU32 height)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    constexpr size_t CM_MAX_2DSURFACE_TABLE_SIZE = 256;

    auto it = m_tableCmRelations.find(std::tie(surfaceIdPair, width, height));
    if (m_tableCmRelations.end() != it)
    {
        it->second.AddRef();
        return &(it->second);
    }

    cmStatus cmSts;

    // CM has limit on amount of surfaces that can be created
    if (m_tableCmRelations.size() == CM_MAX_2DSURFACE_TABLE_SIZE)
    {
        // Delete the oldest free surface (it will delete actually surface with lowest memId, which is most probably the oldest one)

        auto it_surface_to_delete = std::find_if(std::begin(m_tableCmRelations), std::end(m_tableCmRelations), [](auto& map_node) { return map_node.second.IsFree(); });
        if (it_surface_to_delete == std::end(m_tableCmRelations))
        {
            std::ignore = MFX_STS_TRACE(MFX_ERR_MEMORY_ALLOC);
            return nullptr;
        }

        m_tableCmRelations.erase(it_surface_to_delete);
    }

    CmSurface2D *pCmSurface2D = nullptr;

    cmSts = m_pCmDevice->CreateSurface2D(surfaceIdPair, pCmSurface2D);
    CHECK_CM_STATUS_RET_NULL(cmSts);

    auto insert_pair = m_tableCmRelations.emplace(std::piecewise_construct,
        std::forward_as_tuple(surfaceIdPair, width, height),
        std::forward_as_tuple(CmSurface2DWrapper(pCmSurface2D, m_pCmDevice)));

    insert_pair.first->second.AddRef();
    return &(insert_pair.first->second);
} // CmSurface2D * CmCopyWrapper::CreateCmSurface2D(void *pSrc, mfxU32 width, mfxU32 height)

mfxStatus CmCopyWrapper::IsCmCopySupported(mfxFrameSurface1 *pSurface, IppiSize roi)
{
    if ((roi.width & 15) || (roi.height & 7))
    {
        return MFX_ERR_UNSUPPORTED;
    }

    if(pSurface->Info.FourCC != MFX_FOURCC_NV12)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    if(pSurface->Data.UV <= pSurface->Data.Y || size_t(pSurface->Data.UV - pSurface->Data.Y) != size_t(pSurface->Data.Pitch) * pSurface->Info.Height)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;

} // mfxStatus CmCopyWrapper::IsCmCopySupported(mfxFrameSurface1 *pSurface, IppiSize roi)

class CmSurface2DWrapperScopedLock
{
public:
    CmSurface2DWrapperScopedLock(CmSurface2DWrapper& surf)
        : surface(surf)
    {}

    ~CmSurface2DWrapperScopedLock()
    {
        surface.Release();
    }

private:
    CmSurface2DWrapper& surface;
};

mfxStatus CmCopyWrapper::CopySystemToVideoMemoryAPI(mfxHDLPair dst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi)
{
    (void)dstPitch;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::CopySystemToVideoMemoryAPI");
    cmStatus cmSts = 0;

    CmEvent* e = NULL;
    //CM_STATUS sts;
    mfxStatus status = MFX_ERR_NONE;

    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

     // create or find already associated cm surface 2d
    auto pCmSurface2D = CreateCmSurface2D(dst, width, height);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock(*pCmSurface2D);

    cmSts = m_pCmQueue->EnqueueCopyCPUToGPUFullStride(*pCmSurface2D, pSrc, srcPitch, srcUVOffset, CM_FASTCOPY_OPTION_NONBLOCKING, e);

    if (CM_SUCCESS == cmSts)
    {
        cmSts = e->WaitForTaskFinished(m_timeout);
        if(cmSts == CM_EXCEED_MAX_TIMEOUT)
        {
            status = MFX_ERR_GPU_HANG;
        }
    }
    else
    {
        status = MFX_ERR_DEVICE_FAILED;
    }
    m_pCmQueue->DestroyEvent(e);

    return status;
}

mfxStatus CmCopyWrapper::CopySystemToVideoMemory(mfxHDLPair dst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi, mfxU32 format)
{
    (void)dstPitch;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::CopySystemToVideoMemory");
    cmStatus cmSts = 0;

    CmEvent* e = CM_NO_EVENT;
    //CM_STATUS sts;
    mfxStatus status = MFX_ERR_NONE;

    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

     // create or find already associated cm surface 2d
    auto pCmSurface2D = CreateCmSurface2D(dst, width, height);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock(*pCmSurface2D);

    if(isSinglePlainFormat(format))
        status = EnqueueCopyCPUtoGPU(*pCmSurface2D, pSrc, roi.width, roi.height, srcPitch, srcUVOffset, format, CM_FASTCOPY_OPTION_BLOCKING, e);
    else
        status = EnqueueCopyNV12CPUtoGPU(*pCmSurface2D, pSrc, roi.width, roi.height, srcPitch, srcUVOffset, format, CM_FASTCOPY_OPTION_BLOCKING, e);

    if (status == MFX_ERR_GPU_HANG || status == MFX_ERR_NONE)
    {
        return status;
    }
    else
    {
        cmSts = m_pCmQueue->EnqueueCopyCPUToGPUFullStride(*pCmSurface2D, pSrc, srcPitch, srcUVOffset, CM_FASTCOPY_OPTION_BLOCKING, e);

        if (CM_SUCCESS == cmSts)
        {
            status = MFX_ERR_NONE;
        }
        else if(cmSts == CM_EXCEED_MAX_TIMEOUT)
        {
            status = MFX_ERR_GPU_HANG;
        }
        else
        {
            status = MFX_ERR_DEVICE_FAILED;
        }
    }

    return status;
}

mfxStatus CmCopyWrapper::CopySwapSystemToVideoMemory(mfxHDLPair dst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi, mfxU32 format)
{
    (void)dstPitch;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::ARGBSwapSystemToVideo");
    CmEvent* e = CM_NO_EVENT;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

     // create or find already associated cm surface 2d
    auto pCmSurface2D = CreateCmSurface2D(dst, width, height);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock(*pCmSurface2D);

    mfxStatus sts = EnqueueCopySwapRBCPUtoGPU(*pCmSurface2D, pSrc,roi.width,roi.height, srcPitch, srcUVOffset,format, CM_FASTCOPY_OPTION_BLOCKING, e);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::CopyShiftSystemToVideoMemory(mfxHDLPair dst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi, mfxU32 bitshift, mfxU32 format)
{
    (void)dstPitch;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::ShiftSystemToVideo");
    CmEvent* e = CM_NO_EVENT;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;
    mfxStatus sts = MFX_ERR_NONE;

     // create or find already associated cm surface 2d
    auto pCmSurface2D = CreateCmSurface2D(dst, width, height);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock(*pCmSurface2D);

    if (isSinglePlainFormat(format))
        sts = EnqueueCopyShiftCPUtoGPU(*pCmSurface2D, pSrc, roi.width, roi.height, srcPitch, srcUVOffset, format, CM_FASTCOPY_OPTION_BLOCKING, bitshift, e);
    else
        sts = EnqueueCopyShiftP010CPUtoGPU(*pCmSurface2D, pSrc, roi.width, roi.height, srcPitch, srcUVOffset, 0, CM_FASTCOPY_OPTION_BLOCKING, bitshift, e);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::CopyShiftVideoToSystemMemory(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, mfxHDLPair src, mfxU32 srcPitch, IppiSize roi, mfxU32 bitshift, mfxU32 format)
{
    (void)srcPitch;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::ShiftVideoToSystem");
    CmEvent* e = CM_NO_EVENT;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;
    mfxStatus sts = MFX_ERR_NONE;

    // create or find already associated cm surface 2d
    auto pCmSurface2D = CreateCmSurface2D(src, width, height);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock(*pCmSurface2D);

    if(isSinglePlainFormat(format))
        sts = EnqueueCopyShiftGPUtoCPU(*pCmSurface2D, pDst, roi.width, roi.height, dstPitch, dstUVOffset, format, CM_FASTCOPY_OPTION_BLOCKING, bitshift, e);
    else
        sts = EnqueueCopyShiftP010GPUtoCPU(*pCmSurface2D, pDst, roi.width, roi.height, dstPitch, dstUVOffset, 0, CM_FASTCOPY_OPTION_BLOCKING, bitshift, e);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::CopyVideoToSystemMemoryAPI(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, mfxHDLPair src, mfxU32 srcPitch, IppiSize roi)
{
    (void)srcPitch;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::CopyVideoToSystemMemoryAPI");
    cmStatus cmSts = 0;
    CmEvent* e = NULL;
    mfxStatus status = MFX_ERR_NONE;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

    auto pCmSurface2D = CreateCmSurface2D(src, width, height);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    cmSts = m_pCmQueue->EnqueueCopyGPUToCPUFullStride(*pCmSurface2D, pDst, dstPitch, dstUVOffset, CM_FASTCOPY_OPTION_NONBLOCKING, e);

    if (CM_SUCCESS == cmSts)
    {
        cmSts = e->WaitForTaskFinished(m_timeout);
        if (cmSts == CM_EXCEED_MAX_TIMEOUT)
        {
            status = MFX_ERR_GPU_HANG;
        }
    }
    else
    {
        status = MFX_ERR_DEVICE_FAILED;
    }
    m_pCmQueue->DestroyEvent(e);

    return status;
}

mfxStatus CmCopyWrapper::CopyVideoToSystemMemory(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, mfxHDLPair src, mfxU32 srcPitch, IppiSize roi, mfxU32 format)
{
    (void)srcPitch;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::CopyVideoToSystemMemory");
    cmStatus cmSts = 0;
    CmEvent* e = CM_NO_EVENT;
    mfxStatus status = MFX_ERR_NONE;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

    auto pCmSurface2D = CreateCmSurface2D(src, width, height);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock(*pCmSurface2D);

    if(isSinglePlainFormat(format))
        status = EnqueueCopyGPUtoCPU(*pCmSurface2D, pDst, roi.width, roi.height, dstPitch, dstUVOffset, format, CM_FASTCOPY_OPTION_BLOCKING, e);
    else
        status = EnqueueCopyNV12GPUtoCPU(*pCmSurface2D, pDst, roi.width, roi.height, dstPitch, dstUVOffset, format, CM_FASTCOPY_OPTION_BLOCKING, e);

    if (status == MFX_ERR_GPU_HANG || status == MFX_ERR_NONE)
    {
        return status;
    }
    else
    {
        cmSts = m_pCmQueue->EnqueueCopyGPUToCPUFullStride(*pCmSurface2D, pDst, dstPitch, dstUVOffset, CM_FASTCOPY_OPTION_BLOCKING, e);

        if (CM_SUCCESS == cmSts)
        {
            status = MFX_ERR_NONE;
        }
        else if(cmSts == CM_EXCEED_MAX_TIMEOUT)
        {
            status = MFX_ERR_GPU_HANG;
        }
        else
        {
            status = MFX_ERR_DEVICE_FAILED;
        }
    }

    return status;
}

mfxStatus CmCopyWrapper::CopySwapVideoToSystemMemory(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, mfxHDLPair src, mfxU32 srcPitch, IppiSize roi, mfxU32 format)
{
    (void)srcPitch;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::ARGBSwapVideoToSystem");
    CmEvent* e = CM_NO_EVENT;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

    // create or find already associated cm surface 2d
    auto pCmSurface2D = CreateCmSurface2D(src, width, height);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock(*pCmSurface2D);

    mfxStatus sts = EnqueueCopySwapRBGPUtoCPU(*pCmSurface2D, pDst, roi.width, roi.height, dstPitch, dstUVOffset, format, CM_FASTCOPY_OPTION_BLOCKING, e);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::CopyMirrorVideoToSystemMemory(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, mfxHDLPair src, mfxU32 srcPitch, IppiSize roi, mfxU32 format)
{
    (void)srcPitch;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::MirrorVideoToSystem");
    CmEvent* e = CM_NO_EVENT;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

    // create or find already associated cm surface 2d
    auto pCmSurface2D = CreateCmSurface2D(src, width, height);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock(*pCmSurface2D);

    mfxStatus sts = EnqueueCopyMirrorNV12GPUtoCPU(*pCmSurface2D, pDst, roi.width, roi.height, dstPitch, dstUVOffset, format, CM_FASTCOPY_OPTION_BLOCKING, e);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::CopyMirrorSystemToVideoMemory(mfxHDLPair dst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi, mfxU32 format)
{
    (void)dstPitch;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::MirrorSystemToVideo");
    CmEvent* e = CM_NO_EVENT;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

    // create or find already associated cm surface 2d
    auto pCmSurface2D = CreateCmSurface2D(dst, width, height);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock(*pCmSurface2D);

    mfxStatus sts = EnqueueCopyMirrorNV12CPUtoGPU(*pCmSurface2D, pSrc, roi.width, roi.height, srcPitch, srcUVOffset, format, CM_FASTCOPY_OPTION_BLOCKING, e);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::CopyVideoToVideoMemoryAPI(mfxHDLPair dst, mfxHDLPair src, IppiSize roi)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::CopyVideoToVideoMemoryAPI");
    cmStatus cmSts = 0;

    CmEvent* e = NULL;
    mfxStatus status = MFX_ERR_NONE;

    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

    // create or find already associated cm surface 2d
    auto pDstCmSurface2D = CreateCmSurface2D(dst, width, height);
    CHECK_CM_NULL_PTR(pDstCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock_dst(*pDstCmSurface2D);

    auto pSrcCmSurface2D = CreateCmSurface2D(src, width, height);
    CHECK_CM_NULL_PTR(pSrcCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock_src(*pSrcCmSurface2D);

    cmSts = m_pCmQueue->EnqueueCopyGPUToGPU(*pDstCmSurface2D, *pSrcCmSurface2D, CM_FASTCOPY_OPTION_NONBLOCKING, e);

    if (CM_SUCCESS == cmSts)
    {
        cmSts = e->WaitForTaskFinished(m_timeout);
        if (cmSts == CM_EXCEED_MAX_TIMEOUT)
        {
            status = MFX_ERR_GPU_HANG;
        }
    }
    else
    {
        status = MFX_ERR_DEVICE_FAILED;
    }
    m_pCmQueue->DestroyEvent(e);

    return status;
}

mfxStatus CmCopyWrapper::CopySwapVideoToVideoMemory(mfxHDLPair dst, mfxHDLPair src, IppiSize roi, mfxU32 format)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::ARGBSwapVideoToVideo");
    CmEvent* e = NULL;

    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

    // create or find already associated cm surface 2d
    auto pDstCmSurface2D = CreateCmSurface2D(dst, width, height);
    CHECK_CM_NULL_PTR(pDstCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock_dst(*pDstCmSurface2D);

    auto pSrcCmSurface2D = CreateCmSurface2D(src, width, height);
    CHECK_CM_NULL_PTR(pSrcCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock_src(*pSrcCmSurface2D);

    mfxStatus sts = EnqueueCopySwapRBGPUtoGPU(*pSrcCmSurface2D, *pDstCmSurface2D, width, height, format, CM_FASTCOPY_OPTION_BLOCKING, e);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::CopyMirrorVideoToVideoMemory(mfxHDLPair dst, mfxHDLPair src, IppiSize roi, mfxU32 format)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::MirrorVideoToVideo");
    CmEvent* e = NULL;

    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

    // create or find already associated cm surface 2d
    auto pDstCmSurface2D = CreateCmSurface2D(dst, width, height);
    CHECK_CM_NULL_PTR(pDstCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock_dst(*pDstCmSurface2D);

    auto pSrcCmSurface2D = CreateCmSurface2D(src, width, height);
    CHECK_CM_NULL_PTR(pSrcCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2DWrapperScopedLock lock_src(*pSrcCmSurface2D);

    mfxStatus sts = EnqueueCopyMirrorGPUtoGPU(*pSrcCmSurface2D, *pDstCmSurface2D, width, height, format, CM_FASTCOPY_OPTION_BLOCKING, e);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

#define CM_RGB_MAX_GPUCOPY_SURFACE_HEIGHT        4088

#define CM_RGB_SUPPORTED_COPY_SIZE(ROI) (ROI.width <= CM_RGB_MAX_GPUCOPY_SURFACE_HEIGHT && ROI.height <= CM_RGB_MAX_GPUCOPY_SURFACE_HEIGHT )

bool CmCopyWrapper::CheckSurfaceContinuouslyAllocated(const mfxFrameSurface1 &surf)
{
    mfxU32 stride_in_bytes = surf.Data.PitchLow + ((mfxU32)surf.Data.PitchHigh << 16);

    switch (surf.Info.FourCC)
    {
    // Packed formats like YUY2, UYVY, AYUV, Y416, Y210, Y410, A2RGB10, RGB565, RGB3 
    // does not need to be handled here - they got only one plane with all the luma and chroma data interleaved

    // Handling semi-planar formats
    case MFX_FOURCC_P010:
    case MFX_FOURCC_P016:
    case MFX_FOURCC_P210:
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_NV16:
        {
            size_t luma_size_in_bytes = stride_in_bytes * mfx::align2_value(surf.Info.Height, 32);
            return surf.Data.Y + luma_size_in_bytes == surf.Data.UV;
            break;
        }

    // Handling planar formats
    case MFX_FOURCC_YV12:
        {
            size_t luma_size_in_bytes = stride_in_bytes * mfx::align2_value(surf.Info.Height, 32);
            size_t chroma_size_in_bytes = luma_size_in_bytes / 4; //stride of the V plane is half the stride of the Y plane; and the V plane contains half as many lines as the Y plane
            return surf.Data.Y + luma_size_in_bytes == surf.Data.V && surf.Data.V + chroma_size_in_bytes == surf.Data.U;
            break;
        }

    // Handling RGB-like formats
    case MFX_FOURCC_RGBP:
        {
            size_t channel_size_in_bytes = stride_in_bytes * mfx::align2_value(surf.Info.Height, 32);
            return surf.Data.R + channel_size_in_bytes == surf.Data.G && surf.Data.G + channel_size_in_bytes == surf.Data.B;
            break;
        }
    }
    return true;
}

bool CmCopyWrapper::CanUseCmCopy(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)
{
    IppiSize roi = {std::min(pSrc->Info.Width, pDst->Info.Width), std::min(pSrc->Info.Height, pDst->Info.Height)};

    mfxU8* srcPtr = GetFramePointer(pSrc->Info.FourCC, pSrc->Data);
    mfxU8* dstPtr = GetFramePointer(pDst->Info.FourCC, pDst->Data);

    if (NULL != pSrc->Data.MemId && NULL != pDst->Data.MemId)
    {
        if (pDst->Info.FourCC != MFX_FOURCC_YV12 && CM_SUPPORTED_COPY_SIZE(roi))
        {
            return true;
        }
    }
    else if (NULL != pSrc->Data.MemId && NULL != dstPtr)
    {
        mfxU32 dstPitch = pDst->Data.PitchLow + ((mfxU32)pDst->Data.PitchHigh << 16);
        if (!CM_ALIGNED(dstPitch))
            return false;

        mfxI64 verticalPitch = (mfxI64)(pDst->Data.UV - pDst->Data.Y);
        verticalPitch = (verticalPitch % dstPitch)? 0 : verticalPitch / dstPitch;
        if (isNV12LikeFormat(pDst->Info.FourCC) && isNV12LikeFormat(pSrc->Info.FourCC) && CM_ALIGNED(pDst->Data.Y) && CM_ALIGNED(pDst->Data.UV) && CM_SUPPORTED_COPY_SIZE(roi) && verticalPitch >= pDst->Info.Height && verticalPitch <= 16384)
        {
            return CheckSurfaceContinuouslyAllocated(*pDst);
        }
        else if(isSinglePlainFormat(pDst->Info.FourCC) && isSinglePlainFormat(pSrc->Info.FourCC) && pSrc->Info.Shift == pDst->Info.Shift && CM_ALIGNED(dstPtr) && CM_SUPPORTED_COPY_SIZE(roi))
        {
            return true;
        }
        else
            return false;
    }
    else if (NULL != srcPtr && NULL != dstPtr)
    {
        return false;
    }
    else if (NULL != srcPtr && NULL != pDst->Data.MemId)
    {
        mfxU32 srcPitch = pSrc->Data.PitchLow + ((mfxU32)pSrc->Data.PitchHigh << 16);
        if (!CM_ALIGNED(srcPitch))
            return false;

        mfxI64 verticalPitch = (mfxI64)(pSrc->Data.UV - pSrc->Data.Y);
        verticalPitch = (verticalPitch % srcPitch)? 0 : verticalPitch / srcPitch;

        if (isNV12LikeFormat(pDst->Info.FourCC) && isNV12LikeFormat(pSrc->Info.FourCC) && CM_ALIGNED(pSrc->Data.Y) && CM_ALIGNED(pSrc->Data.UV) && CM_SUPPORTED_COPY_SIZE(roi) && verticalPitch >= pSrc->Info.Height && verticalPitch <= 16384)
        {
            return true;
        }
        else if(isSinglePlainFormat(pDst->Info.FourCC) && isSinglePlainFormat(pSrc->Info.FourCC) && pSrc->Info.Shift == pDst->Info.Shift && CM_ALIGNED(srcPtr) && CM_SUPPORTED_COPY_SIZE(roi))
        {
            return true;
        }
    }

    return false;
}

mfxStatus CmCopyWrapper::CopyVideoToVideo(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)
{
    MFX_CHECK_NULL_PTR2(pDst, pSrc);

    IppiSize roi = {std::min(pSrc->Info.Width, pDst->Info.Width), std::min(pSrc->Info.Height, pDst->Info.Height)};

    // check that region of interest is valid
    MFX_CHECK(roi.width != 0 && roi.height != 0 && m_HWType != MFX_HW_UNKNOWN, MFX_ERR_UNDEFINED_BEHAVIOR);

    MFX_CHECK(pSrc->Data.MemId && pDst->Data.MemId, MFX_ERR_UNDEFINED_BEHAVIOR);

    if(isNeedSwapping(pSrc,pDst))
        return CopySwapVideoToVideoMemory(*reinterpret_cast<mfxHDLPair*>(pDst->Data.MemId), *reinterpret_cast<mfxHDLPair*>(pSrc->Data.MemId), roi, pDst->Info.FourCC);
    else
        return CopyVideoToVideoMemoryAPI(*reinterpret_cast<mfxHDLPair*>(pDst->Data.MemId), *reinterpret_cast<mfxHDLPair*>(pSrc->Data.MemId), roi);
}

mfxStatus CmCopyWrapper::CopyVideoToSys(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)
{
    MFX_CHECK_NULL_PTR2(pDst, pSrc);

    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_AUTO_TRACE("CmCopyWrapper::CopyVideoToSys");
    TRACE_EVENT(MFX_TRACE_HOTSPOT_CM_COPY, EVENT_TYPE_INFO, 0, make_event_data(MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY));

    IppiSize roi = {std::min(pSrc->Info.Width, pDst->Info.Width), std::min(pSrc->Info.Height, pDst->Info.Height)};

    // check that region of interest is valid
    if (roi.width == 0 || roi.height == 0 || m_HWType == MFX_HW_UNKNOWN)
    {
        mfxRes = MFX_ERR_UNDEFINED_BEHAVIOR;
        MFX_RETURN(mfxRes);
    }

    mfxU32 dstPitch = pDst->Data.PitchLow + ((mfxU32)pDst->Data.PitchHigh << 16);

    mfxU8* dstPtr = GetFramePointer(pDst->Info.FourCC, pDst->Data);

    if (!pSrc->Data.MemId || !dstPtr)
    {
        mfxRes = MFX_ERR_UNDEFINED_BEHAVIOR;
        MFX_RETURN(mfxRes);
    }

    if (!CM_ALIGNED(pDst->Data.Pitch))
    {
        mfxRes = MFX_ERR_UNDEFINED_BEHAVIOR;
        MFX_RETURN(mfxRes);
    }

    mfxI64 verticalPitch = (mfxI64)(pDst->Data.UV - pDst->Data.Y);
    verticalPitch = (verticalPitch % dstPitch)? 0 : verticalPitch / dstPitch;

#ifdef MFX_ENABLE_RGBP
    if (pDst->Info.FourCC == MFX_FOURCC_RGBP)
    {
        verticalPitch = (mfxI64)(pDst->Data.G - pDst->Data.R);
        verticalPitch = (verticalPitch % dstPitch)? 0 : verticalPitch / dstPitch;
    }
#endif

    if (isNeedShift(pSrc, pDst) && CM_ALIGNED(dstPtr) && CM_SUPPORTED_COPY_SIZE(roi) && verticalPitch >= pDst->Info.Height && verticalPitch <= 16384)
    {
        mfxRes = CopyShiftVideoToSystemMemory(dstPtr, pDst->Data.Pitch,(mfxU32)verticalPitch, *reinterpret_cast<mfxHDLPair*>(pSrc->Data.MemId), 0, roi, 16-pDst->Info.BitDepthLuma, pDst->Info.FourCC);
        MFX_RETURN(mfxRes);
    }

    if (isNV12LikeFormat(pDst->Info.FourCC) && CM_ALIGNED(dstPtr) && CM_SUPPORTED_COPY_SIZE(roi) && verticalPitch >= pDst->Info.Height && verticalPitch <= 16384)
    {
        mfxRes = CopyVideoToSystemMemory(dstPtr, pDst->Data.Pitch, (mfxU32)verticalPitch, *reinterpret_cast<mfxHDLPair*>(pSrc->Data.MemId), pDst->Info.Height, roi, pDst->Info.FourCC);
        MFX_RETURN(mfxRes);
    }

    if (isNeedSwapping(pSrc, pDst) && CM_ALIGNED(dstPtr) && CM_SUPPORTED_COPY_SIZE(roi))
    {
        mfxRes = CopySwapVideoToSystemMemory(dstPtr, pDst->Data.Pitch, (mfxU32)pSrc->Info.Height, *reinterpret_cast<mfxHDLPair*>(pSrc->Data.MemId), 0, roi, pDst->Info.FourCC);
        MFX_RETURN(mfxRes);
    }

    if (isSinglePlainFormat(pDst->Info.FourCC) && isSinglePlainFormat(pSrc->Info.FourCC) && pSrc->Info.FourCC == pDst->Info.FourCC && pSrc->Info.Shift == pDst->Info.Shift && CM_ALIGNED(dstPtr) && CM_SUPPORTED_COPY_SIZE(roi))
    {
        mfxRes = CopyVideoToSystemMemory(dstPtr, pDst->Data.Pitch, (mfxU32)verticalPitch, *reinterpret_cast<mfxHDLPair*>(pSrc->Data.MemId), pDst->Info.Height, roi, pDst->Info.FourCC);
        MFX_RETURN(mfxRes);
    }

    mfxRes = MFX_ERR_UNDEFINED_BEHAVIOR;
    MFX_RETURN(mfxRes);
}

mfxStatus CmCopyWrapper::CopySysToVideo(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)
{
    MFX_CHECK_NULL_PTR2(pDst, pSrc);

    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_AUTO_TRACE("CmCopyWrapper::CopySysToVideo");
    TRACE_EVENT(MFX_TRACE_HOTSPOT_CM_COPY, EVENT_TYPE_INFO, 0, make_event_data(MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY));

    IppiSize roi = {std::min(pSrc->Info.Width, pDst->Info.Width), std::min(pSrc->Info.Height, pDst->Info.Height)};

    mfxU8* srcPtr = GetFramePointer(pSrc->Info.FourCC, pSrc->Data);

    // check that region of interest is valid
    if (roi.width == 0 || roi.height == 0 || m_HWType == MFX_HW_UNKNOWN)
    {
        mfxRes = MFX_ERR_UNDEFINED_BEHAVIOR;
        MFX_RETURN(mfxRes);
    }

    if (!srcPtr || !pDst->Data.MemId)
    {
        mfxRes = MFX_ERR_UNDEFINED_BEHAVIOR;
        MFX_RETURN(mfxRes);
    }

    if (!CM_ALIGNED(pSrc->Data.Pitch))
    {
        mfxRes = MFX_ERR_UNDEFINED_BEHAVIOR;
        MFX_RETURN(mfxRes);
    }

    // source are placed in system memory, destination is in video memory
    // use common way to copy frames from system to video, most faster
    mfxI64 verticalPitch = (mfxI64)(pSrc->Data.UV - pSrc->Data.Y);
    verticalPitch = (verticalPitch % pSrc->Data.Pitch)? 0 : verticalPitch / pSrc->Data.Pitch;

    if (isNeedShift(pSrc, pDst) && CM_ALIGNED(srcPtr) && CM_SUPPORTED_COPY_SIZE(roi) && verticalPitch >= pSrc->Info.Height && verticalPitch <= 16384)
    {
        mfxRes = CopyShiftSystemToVideoMemory(*reinterpret_cast<mfxHDLPair*>(pDst->Data.MemId), 0, pSrc->Data.Y, pSrc->Data.Pitch,(mfxU32)verticalPitch, roi, 16 - pSrc->Info.BitDepthLuma, pDst->Info.FourCC);
        MFX_RETURN(mfxRes);
    }

    if (isNV12LikeFormat(pSrc->Info.FourCC) && CM_ALIGNED(srcPtr) && CM_SUPPORTED_COPY_SIZE(roi) && verticalPitch >= pSrc->Info.Height && verticalPitch <= 16384)
    {
        mfxRes = CopySystemToVideoMemory(*reinterpret_cast<mfxHDLPair*>(pDst->Data.MemId), 0, pSrc->Data.Y, pSrc->Data.Pitch, (mfxU32)verticalPitch, roi, pDst->Info.FourCC);
        MFX_RETURN(mfxRes);
    }

    if (isNeedSwapping(pSrc, pDst) && CM_ALIGNED(srcPtr) && CM_SUPPORTED_COPY_SIZE(roi))
    {
        mfxRes = CopySwapSystemToVideoMemory(*reinterpret_cast<mfxHDLPair*>(pDst->Data.MemId), 0, srcPtr, pSrc->Data.Pitch, (mfxU32)pSrc->Info.Height, roi, pDst->Info.FourCC);
        MFX_RETURN(mfxRes);
    }

    if (isSinglePlainFormat(pDst->Info.FourCC) && isSinglePlainFormat(pSrc->Info.FourCC) && pSrc->Info.FourCC == pDst->Info.FourCC && pSrc->Info.Shift == pDst->Info.Shift && CM_ALIGNED(srcPtr) && CM_SUPPORTED_COPY_SIZE(roi))
    {
        mfxRes = CopySystemToVideoMemory(*reinterpret_cast<mfxHDLPair*>(pDst->Data.MemId), 0, srcPtr, pSrc->Data.Pitch, (mfxU32)pSrc->Info.Height, roi, pDst->Info.FourCC);
        MFX_RETURN(mfxRes);
    }

    mfxRes = MFX_ERR_UNDEFINED_BEHAVIOR;
    MFX_RETURN(mfxRes);
}
