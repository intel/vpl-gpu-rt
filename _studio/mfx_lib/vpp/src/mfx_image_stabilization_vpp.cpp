// Copyright (c) 2012-2019 Intel Corporation
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

#if defined(MFX_ENABLE_VPP) && defined(MFX_ENABLE_IMAGE_STABILIZATION_VPP)
#if defined(__GNUC__)
#if defined(__INTEL_COMPILER)
    #pragma warning (disable:1478)
#else
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif

#include "mfx_image_stabilization_vpp.h"
#include "mfx_vpp_utils.h"

#include "umc_defs.h"

/* ******************************************************************** */
/*                 implementation of Interface functions [Img Stab]     */
/* ******************************************************************** */

mfxStatus MFXVideoVPPImgStab::Query( mfxExtBuffer* pHint )
{
    if( NULL == pHint )
    {
        return MFX_ERR_NONE;
    }

    mfxExtVPPImageStab* pParam = (mfxExtVPPImageStab*)pHint;

    if( MFX_IMAGESTAB_MODE_UPSCALE == pParam->Mode || 
        MFX_IMAGESTAB_MODE_BOXING  == pParam->Mode)
    {
        return MFX_ERR_NONE;
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    } 

} // static mfxStatus MFXVideoVPPDenoise::Query( mfxExtBuffer* pHint )

#endif // MFX_ENABLE_VPP
/* EOF */
