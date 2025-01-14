// Copyright (c) 2007-2025 Intel Corporation
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
#include "libmfx_core_hw.h"
#include "mfx_common_decode_int.h"
#include "mfx_enc_common.h"
#include "mfxpcp.h"
#include "mfx_ext_buffers.h"

#include "umc_va_base.h"

using namespace UMC;

mfxU32 ChooseProfile(mfxVideoParam const* param, eMFXHWType)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "ChooseProfile");

    mfxU32 profile = UMC::VA_VLD;

    // video accelerator is needed for decoders only
    switch (param->mfx.CodecId)
    {
    case MFX_CODEC_VC1:
        profile |= VA_VC1;
        break;

    case MFX_CODEC_MPEG2:
        profile |= VA_MPEG2;
        break;

    case MFX_CODEC_AVC:
        {
        profile |= VA_H264;


#ifdef MFX_ENABLE_SVC_VIDEO_DECODE
        if (IsSVCProfile(profile_idc))
        {
            profile |= (profile_idc == MFX_PROFILE_AVC_SCALABLE_HIGH) ? VA_PROFILE_SVC_HIGH : VA_PROFILE_SVC_BASELINE;
        }
#endif
        }
        break;

    case MFX_CODEC_JPEG:
        profile |= VA_JPEG;
        break;

    case MFX_CODEC_VP8:
        profile |= VA_VP8;
        break;

    case MFX_CODEC_VP9:
        profile |= VA_VP9;
        switch (param->mfx.FrameInfo.FourCC)
        {
        case MFX_FOURCC_P010:
            profile |= VA_PROFILE_10;
            break;
        case MFX_FOURCC_AYUV:
            profile |= VA_PROFILE_444;
            break;
        case MFX_FOURCC_Y410:
            profile |= VA_PROFILE_10 | VA_PROFILE_444;
            break;
        case MFX_FOURCC_P016:
            profile |= VA_PROFILE_12;
            break;
        case MFX_FOURCC_Y416:
            profile |= VA_PROFILE_12 | VA_PROFILE_444;
            break;
        }
        break;

#if defined(MFX_ENABLE_AV1_VIDEO_CODEC)
    case MFX_CODEC_AV1:
        profile |= VA_AV1;
        switch (param->mfx.FrameInfo.FourCC)
        {
        case MFX_FOURCC_P010:
            profile |= VA_PROFILE_10;
            break;
        }
        break;
#endif

    case MFX_CODEC_HEVC:
        profile |= VA_H265;
        switch (param->mfx.FrameInfo.FourCC)
        {
            case MFX_FOURCC_P010:
                profile |= VA_PROFILE_10;
                break;
            case MFX_FOURCC_YUY2:
                profile |= VA_PROFILE_422;
                break;
            case MFX_FOURCC_AYUV:
                profile |= VA_PROFILE_444;
                break;
            case MFX_FOURCC_Y210:
                profile |= VA_PROFILE_10 | VA_PROFILE_422;
                break;
            case MFX_FOURCC_Y410:
                profile |= VA_PROFILE_10 | VA_PROFILE_444;
                break;
            case MFX_FOURCC_P016:
                profile |= VA_PROFILE_12;
                break;
            case MFX_FOURCC_Y216:
                profile |= VA_PROFILE_12 | VA_PROFILE_422;
                break;
            case MFX_FOURCC_Y416:
                profile |= VA_PROFILE_12 | VA_PROFILE_444;
                break;
        }

        {
            mfxU32 const profile_idc = ExtractProfile(param->mfx.CodecProfile);
            if (profile_idc == MFX_PROFILE_HEVC_SCC)
                profile |= VA_PROFILE_SCC;

            if (profile_idc == MFX_PROFILE_HEVC_REXT)
                profile |= VA_PROFILE_REXT;
        }
        break;

#if defined(MFX_ENABLE_VVC_VIDEO_DECODE)
    case MFX_CODEC_VVC:
        profile |= VA_VVC;
        switch (param->mfx.FrameInfo.FourCC)
        {
        case MFX_FOURCC_P010:
            profile |= VA_PROFILE_10;
            break;
        }
        break;
#endif
    default:
        return 0;
    }

#ifdef MFX_EXTBUFF_FORCE_PRIVATE_DDI_ENABLE
    {
        mfxExtBuffer* extbuf =
             GetExtendedBuffer(param->ExtParam, param->NumExtParam, MFX_EXTBUFF_FORCE_PRIVATE_DDI);
         if (extbuf)
            profile |= VA_PRIVATE_DDI_MODE;
    }
#endif

    return profile;
}


/* EOF */
