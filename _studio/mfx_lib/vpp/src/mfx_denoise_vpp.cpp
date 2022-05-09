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

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#if defined(__GNUC__)
#if defined(__INTEL_COMPILER)
    #pragma warning (disable:1478)
#else
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif

#include "umc_defs.h"

#include "mfx_vpp_utils.h"
#include "mfx_denoise_vpp.h"


/* ******************************************************************** */
/*                 implementation of VPP filter [Denoise]               */
/* ******************************************************************** */
// this range are used by Query to correct application request
#define PAR_NRF_STRENGTH_MIN                (0)
#define PAR_NRF_STRENGTH_MAX                100 // real value is 63
#define PAR_NRF_STRENGTH_DEFAULT            PAR_NRF_STRENGTH_MIN

mfxStatus MFXVideoVPPDenoise::Query( mfxExtBuffer* pHint )
{
    if( NULL == pHint )
    {
        return MFX_ERR_UNSUPPORTED;
    }

    mfxStatus sts = MFX_ERR_NONE;

    mfxU32 bufferId = pHint->BufferId;
    if (MFX_EXTBUFF_VPP_DENOISE == bufferId)
    {
        mfxExtVPPDenoise* bufDN = reinterpret_cast<mfxExtVPPDenoise*>(pHint);
        MFX_CHECK_NULL_PTR1(bufDN);
        if (bufDN->DenoiseFactor > PAR_NRF_STRENGTH_MAX)
        {
            bufDN->DenoiseFactor = PAR_NRF_STRENGTH_MAX;
            sts = MFX_STS_TRACE(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        }
    }
    else if (MFX_EXTBUFF_VPP_DENOISE2 == bufferId)
    {
        mfxExtVPPDenoise2* bufDN = reinterpret_cast<mfxExtVPPDenoise2*>(pHint);
        MFX_CHECK_NULL_PTR1(bufDN);
        mfxDenoiseMode mode = bufDN->Mode;
        switch (mode)
        {
            case mfxDenoiseMode::MFX_DENOISE_MODE_INTEL_HVS_PRE_MANUAL:
            case mfxDenoiseMode::MFX_DENOISE_MODE_INTEL_HVS_POST_MANUAL:
            {
                if (bufDN->Strength > PAR_NRF_STRENGTH_MAX)
                {
                    bufDN->Strength = PAR_NRF_STRENGTH_MAX;
                    sts = MFX_STS_TRACE(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
                }
                break;
            }
            default:
                break;
        }
    }

    return sts;

} // static mfxStatus MFXVideoVPPDenoise::Query( mfxExtBuffer* pHint )

#endif // MFX_ENABLE_VPP

/* EOF */
