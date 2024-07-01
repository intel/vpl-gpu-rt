// Copyright (c) 2022 Intel Corporation
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

#ifndef __CM_MEM_COPY_INTERFACE_H__
#define __CM_MEM_COPY_INTERFACE_H__
#ifndef MFX_ENABLE_EXT

#include "mfxdefs.h"

typedef void CmDevice;

class CmCopyWrapper
{
public:

    CmCopyWrapper(bool /*cm_buffer_cache*/ = false) {}

    // destructor
    virtual ~CmCopyWrapper(void) {}


    template <typename D3DAbstract>
    CmDevice* GetCmDevice(D3DAbstract *pD3D)
    {
        (void*)pD3D;
        return nullptr;
    };

    CmDevice* GetCmDevice(VADisplay dpy)
    {
        return nullptr;
    };

    static bool CanUseCmCopy(mfxFrameSurface1* pDst, mfxFrameSurface1* pSrc) 
    { 
        (void*)pDst;
        (void*)pSrc;
        return false; 
    }
    
    void CleanUpCache() {}
    void Close() {}

    mfxStatus CopySysToVideo(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc) 
    {
        (void*)pDst;
        (void*)pSrc;
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }
    mfxStatus CopyVideoToSys(mfxFrameSurface1* pDst, mfxFrameSurface1* pSrc)
    {
        (void*)pDst;
        (void*)pSrc;
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }
    mfxStatus CopyVideoToVideo(mfxFrameSurface1* pDst, mfxFrameSurface1* pSrc)
    {
        (void*)pDst;
        (void*)pSrc;
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }
    
    mfxStatus InitializeSwapKernels(eMFXHWType hwtype = MFX_HW_UNKNOWN)
    {
        (void)hwtype;
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    mfxStatus Initialize(mfxU16 hwDeviceId, eMFXHWType hwtype = MFX_HW_UNKNOWN)
    {
        (void)hwtype;
        (void)hwDeviceId;
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }
    
protected:

private:

};

#endif

#endif