// Copyright (c) 2008-2021 Intel Corporation
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

#ifndef __MFX_PXP_VIDEO_ACCELERATOR_VAAPI_H
#define __MFX_PXP_VIDEO_ACCELERATOR_VAAPI_H

#include "mfx_common.h"

#if defined(MFX_ENABLE_PXP)

#include "mfxpxp.h"
#include "umc_va_linux.h"

class PXPLinuxVideoAccelerator : public UMC::LinuxVideoAccelerator
{
public:
    // constructor
    PXPLinuxVideoAccelerator();
    // destructor
    virtual ~PXPLinuxVideoAccelerator();

    // VideoAccelerator methods
    virtual UMC::Status Init         (UMC::VideoAcceleratorParams* pInfo) override;
    virtual UMC::Status Execute      () override;
    virtual UMC::Status SetAttributes(VAProfile va_profile, UMC::LinuxVideoAcceleratorParams* pParams, VAConfigAttrib *attribute, int32_t *attribsNumber) override;

protected:
    mfxPXPCtxHDL m_PXPCtxHdl;
};

#endif // MFX_ENABLE_PXP
#endif // __MFX_PXP_VIDEO_ACCELERATOR_VAAPI_H

