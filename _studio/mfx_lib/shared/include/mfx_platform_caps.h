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

#include "mfx_common.h"

#ifndef __MFX_PLATFORM_CAPS_H__
#define __MFX_PLATFORM_CAPS_H__

namespace CommonCaps {
    inline bool IsPreSiPlatform(eMFXHWType platform)
    {
        return
            false;
    }

    inline bool HasNativeDX9Support(eMFXHWType platform)
    {
        return platform < MFX_HW_ADL_S;
    }

    inline bool IsVplHW(eMFXHWType hw, mfxU32 deviceId)
    {
        return hw >= MFX_HW_TGL_LP && deviceId != 0x4907;
    }

    inline bool CapsQueryOptimizationSupport(eMFXHWType platform)
    {
        return
            true
            ;
    }
    inline bool IsCmSupported(eMFXHWType platform)
    {
        return platform <= MFX_HW_XE_HP_SDV;
    }
}

#ifdef MFX_ENABLE_H264_VIDEO_ENCODE
namespace H264ECaps {
    inline bool IsVmeSupported(eMFXHWType platform)
    {
        return
            (platform <= MFX_HW_ADL_N);
    }

    inline bool IsHvsSupported(eMFXHWType platform)
    {
        return (platform >= MFX_HW_DG2);
    }

    inline bool IsFrameSizeTolerenceSupported(eMFXHWType platform)
    {
       return
           false;
    }

    inline bool IsAcqmSupported(eMFXHWType platform)
    {
        return (platform >= MFX_HW_DG2);
    }

    inline bool IsAltRefSupported(eMFXHWType platform)
    {
        return (platform >= MFX_HW_DG2);
    }

    inline bool IsRateControlLASupported(eMFXHWType platform)
    {
        return (platform >= MFX_HW_DG2);
    }
}
#endif //MFX_ENABLE_H264_VIDEO_ENCODE

#ifdef MFX_ENABLE_VP9_VIDEO_ENCODE
namespace VP9ECaps {
    inline bool Is32x32BlockSupported(eMFXHWType platform)
    {
        return (platform >= MFX_HW_DG2);
    }
}
#endif

namespace VppCaps
{
    inline bool IsMctfSupported(eMFXHWType platform)
    {
        return (platform >= MFX_HW_TGL_LP && platform < MFX_HW_DG2);
    }

    inline bool IsVideoSignalSupported(eMFXHWType platform)
    {
        return 
        platform >= MFX_HW_DG2;
    }

    inline bool IsSwFieldProcessingSupported(eMFXHWType platform)
    {
        return
        platform < MFX_HW_DG2;
    }

    inline bool IsFieldProcessingSupported(eMFXHWType platform)
    {
        return platform != MFX_HW_DG2;
    }
}

#ifdef MFX_ENABLE_VP8_VIDEO_DECODE
namespace VP8DCaps {
    inline bool IsSupported(eMFXHWType platform)
    {
        return platform >= MFX_HW_ADL_S;
    }
}
#endif // MFX_ENABLE_VP8_VIDEO_DECODE

#ifdef MFX_ENABLE_AV1_VIDEO_DECODE
namespace AV1DCaps {
    inline bool IsPostProcessSupported(eMFXHWType platform)
    {
        return
            false
            ;
    }
}
#endif // MFX_ENABLE_AV1_VIDEO_DECODE

#ifdef MFX_ENABLE_H264_VIDEO_DECODE
namespace H264DCaps {
    // Progressive only for Platforms Before Tgllp.
    inline bool IsOnlyProgressivePicStructSupported(eMFXHWType platform)
    {
        return
            platform != MFX_HW_DG2;
    }
}
#endif // MFX_ENABLE_H264_VIDEO_DECODE
#endif // __MFX_PLATFORM_CAPS_H__
