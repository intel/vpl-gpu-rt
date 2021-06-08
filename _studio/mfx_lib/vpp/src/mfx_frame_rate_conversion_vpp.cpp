// Copyright (c) 2008-2020 Intel Corporation
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

#if defined (MFX_ENABLE_VPP)

#include "mfx_vpp_utils.h"
#include "mfx_frame_rate_conversion_vpp.h"

#include "umc_defs.h"

/* ******************************************************************** */
/*                 implementation of VPP filter [FrameRateConversion]   */
/* ******************************************************************** */

mfxStatus MFXVideoVPPFrameRateConversion::Query( mfxExtBuffer* pHint )
{
    if( NULL == pHint )
    {
        return MFX_ERR_NONE;
    }


    mfxExtVPPFrameRateConversion* pParam = (mfxExtVPPFrameRateConversion*)pHint;

    if( MFX_FRCALGM_PRESERVE_TIMESTAMP    == pParam->Algorithm    || 
        MFX_FRCALGM_DISTRIBUTED_TIMESTAMP == pParam->Algorithm ||
        MFX_FRCALGM_FRAME_INTERPOLATION   == pParam->Algorithm)
    {
        return MFX_ERR_NONE;
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }    

} // mfxStatus MFXVideoVPPFrameRateConversion::Query( mfxExtBuffer* pHint )

#endif // MFX_ENABLE_VPP
/* EOF */
