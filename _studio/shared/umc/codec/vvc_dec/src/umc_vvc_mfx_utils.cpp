// Copyright (c) 2022-2025 Intel Corporation
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

#include "umc_defs.h"
#ifdef MFX_ENABLE_VVC_VIDEO_DECODE

#include "umc_structures.h"
#include "umc_vvc_mfx_utils.h"


#define CHECK_UNSUPPORTED(EXPR) if (EXPR) return MFX_ERR_UNSUPPORTED;

namespace UMC_VVC_DECODER
{
    namespace MFX_Utility{

    inline
    mfxU32 CalculateFourcc(mfxU16 codecProfile, mfxFrameInfo const* frameInfo)
    {
        //map profile + chroma fmt + bit depth => fcc
        if (codecProfile != 1 && codecProfile != 65)
            return 0;

        mfxU16 bit_depth =
            std::max(frameInfo->BitDepthLuma, frameInfo->BitDepthChroma);

        //map chroma fmt & bit depth onto fourcc (NOTE: we currently don't support bit depth above 10 bit)
        mfxU32 const map[][4] =
        {
            /* 8 bit */      /* 10 bit */
        {               0,               0,               0, 0 }, //400
        { MFX_FOURCC_NV12, MFX_FOURCC_P010, MFX_FOURCC_P016, 0 }, //420
        { MFX_FOURCC_YUY2, MFX_FOURCC_Y210, MFX_FOURCC_Y216, 0 }, //422
        { MFX_FOURCC_AYUV, MFX_FOURCC_Y410, MFX_FOURCC_Y416, 0 }  //444
        };

        // Unsupported chroma format, should be validated before
        MFX_CHECK_WITH_ASSERT(
            (frameInfo->ChromaFormat == MFX_CHROMAFORMAT_YUV400 ||
             frameInfo->ChromaFormat == MFX_CHROMAFORMAT_YUV420 ||
             frameInfo->ChromaFormat == MFX_CHROMAFORMAT_YUV422 ||
             frameInfo->ChromaFormat == MFX_CHROMAFORMAT_YUV444), 0);

        //align luma depth up to 2 (8-10-12 ...)
        bit_depth = (bit_depth + 2 - 1) & ~(2 - 1);
        // Luma depth should be aligned up to 2
        MFX_CHECK_WITH_ASSERT(!(bit_depth & 1), 0);

        // Unsupported bit depth, should be validated before
        MFX_CHECK_WITH_ASSERT(
            (bit_depth == 8 ||
             bit_depth == 10 ||
             bit_depth == 12), 0);

        mfxU16 const bit_depth_idx = (bit_depth - 8) / 2;

        return map[frameInfo->ChromaFormat][bit_depth_idx];
    }

    // Initialize mfxVideoParam structure based on decoded bitstream header values
    UMC::Status FillVideoParam(const VVCVideoParamSet* pVps, const VVCSeqParamSet *pSps, const VVCPicParamSet *pPps, mfxVideoParam *par)
    {
        if (!pSps)
            return UMC::UMC_ERR_FAILED;
        if (!pPps)
            return UMC::UMC_ERR_FAILED;

        par->mfx.CodecId = MFX_CODEC_VVC;

        par->mfx.FrameInfo.Width = (mfxU16)(pSps->sps_pic_width_max_in_luma_samples);
        par->mfx.FrameInfo.Height = (mfxU16)(pSps->sps_pic_height_max_in_luma_samples);

        par->mfx.FrameInfo.Width = mfx::align2_value(par->mfx.FrameInfo.Width, VVC_MAX_FRAME_PIXEL_ALIGNED);
        par->mfx.FrameInfo.Height = mfx::align2_value(par->mfx.FrameInfo.Height, VVC_MAX_FRAME_PIXEL_ALIGNED);

        par->mfx.FrameInfo.BitDepthLuma = (mfxU16)(pSps->sps_bitdepth_minus8 + 8);
        par->mfx.FrameInfo.BitDepthChroma = (mfxU16)(pSps->sps_bitdepth_minus8 + 8);
        par->mfx.FrameInfo.Shift = 0;

        int unitX = 1;
        int unitY = 1;
        if (getWinUnit(pSps->sps_chroma_format_idc, unitX, unitY) != UMC::UMC_OK)
            return UMC::UMC_ERR_FAILED;

        par->mfx.FrameInfo.CropX = (mfxU16)(pPps->pps_conf_win_left_offset * unitX);
        par->mfx.FrameInfo.CropY = (mfxU16)(pPps->pps_conf_win_top_offset * unitY);
        par->mfx.FrameInfo.CropH = (mfxU16)(pPps->pps_pic_height_in_luma_samples - (pPps->pps_conf_win_top_offset + pPps->pps_conf_win_bottom_offset) * unitY);
        par->mfx.FrameInfo.CropW = (mfxU16)(pPps->pps_pic_width_in_luma_samples - (pPps->pps_conf_win_left_offset + pPps->pps_conf_win_right_offset) * unitX);

        par->mfx.FrameInfo.PicStruct = static_cast<mfxU16>(pSps->sps_field_seq_flag ? MFX_PICSTRUCT_FIELD_SINGLE : MFX_PICSTRUCT_PROGRESSIVE);
        par->mfx.FrameInfo.ChromaFormat = (mfxU16)(pSps->sps_chroma_format_idc);

        par->mfx.FrameInfo.AspectRatioW = 1; // There is a known gap in VVC VPL, which does not implement to parse VUI messages,so AspectRatioW and AspectRatioH are set 1 by default.
        par->mfx.FrameInfo.AspectRatioH = 1;

        par->mfx.FrameInfo.FrameRateExtN = (mfxU32)(pSps->general_timing_hrd_parameters.time_scale);
        par->mfx.FrameInfo.FrameRateExtD = (mfxU32)(pSps->general_timing_hrd_parameters.num_units_in_tick);

        if (pVps && pVps->vps_num_layers_in_ols.size())
        {
            if (pVps->vps_num_layers_in_ols[pVps->vps_target_ols_idx] == 1) // general_timing_hrd_parameters and ols_timing_hrd_parameters in the SPS are selected
            {
                par->mfx.FrameInfo.FrameRateExtN = (mfxU32)(pSps->general_timing_hrd_parameters.time_scale);
                par->mfx.FrameInfo.FrameRateExtD = (mfxU32)(pSps->general_timing_hrd_parameters.num_units_in_tick);
            }
            else // general_timing_hrd_parameters and ols_timing_hrd_parameters in the VPS are selected
            {
                par->mfx.FrameInfo.FrameRateExtN = (mfxU32)(pVps->general_timing_hrd_parameters.time_scale);
                par->mfx.FrameInfo.FrameRateExtD = (mfxU32)(pVps->general_timing_hrd_parameters.num_units_in_tick);
            }
        }

        par->mfx.CodecProfile = (mfxU16)pSps->profile_tier_level.general_profile_idc;
        par->mfx.CodecLevel = (mfxU16)pSps->profile_tier_level.general_level_idc;
        if (pSps->profile_tier_level.general_profile_idc == (VVC_STILL_PICTURE | VVC_MAIN_10))
        {
            par->mfx.MaxDecFrameBuffering = 1;
        }
        else if (pSps->sps_ptl_dpb_hrd_params_present_flag)
        {
            par->mfx.MaxDecFrameBuffering = (mfxU16)pSps->dpb_parameter.dpb_max_dec_pic_buffering_minus1[pSps->sps_max_sublayers_minus1] + 1;
        }
        else
        {
            par->mfx.MaxDecFrameBuffering = 0;
        }

        par->mfx.FrameInfo.FourCC = CalculateFourcc(par->mfx.CodecProfile, &par->mfx.FrameInfo);

        par->mfx.DecodedOrder = 0;

        return UMC::UMC_OK;
    }

    // Initialize mfxVideoParam structure based on decoded bitstream header values
    UMC::Status UpdateVideoParam(const VVCSeqParamSet* pSps, const VVCPicParamSet* pPps, mfxVideoParam* par)
    {
        if (!pPps)
            return UMC::UMC_ERR_FAILED;

        par->mfx.CodecId = MFX_CODEC_VVC;

        par->mfx.FrameInfo.Width = (mfxU16)(pPps->pps_pic_width_in_luma_samples);
        par->mfx.FrameInfo.Height = (mfxU16)(pPps->pps_pic_height_in_luma_samples);

        int unitX = 1;
        int unitY = 1;
        if (getWinUnit(pSps->sps_chroma_format_idc, unitX, unitY) != UMC::UMC_OK)
            return UMC::UMC_ERR_FAILED;

        par->mfx.FrameInfo.CropH = (mfxU16)(pPps->pps_pic_height_in_luma_samples - (pPps->pps_conf_win_top_offset + pPps->pps_conf_win_bottom_offset) * unitY);
        par->mfx.FrameInfo.CropW = (mfxU16)(pPps->pps_pic_width_in_luma_samples - (pPps->pps_conf_win_left_offset + pPps->pps_conf_win_right_offset) * unitX);

        par->mfx.FrameInfo.BitDepthLuma = (mfxU16)(pSps->sps_bitdepth_minus8 + 8);
        par->mfx.FrameInfo.BitDepthChroma = (mfxU16)(pSps->sps_bitdepth_minus8 + 8);
        return UMC::UMC_OK;
    }


    inline  mfxU16 MatchProfile(mfxU32)
    {
        return MFX_PROFILE_VVC_MAIN10;
    }
    inline
    bool CheckGUID(VideoCORE* core, eMFXHWType type, mfxVideoParam const* param)
    {
        mfxVideoParam vp = *param;
        mfxU16 profile = vp.mfx.CodecProfile & 0xFF;
        if (profile == MFX_PROFILE_UNKNOWN)
        {
            profile = MatchProfile(vp.mfx.FrameInfo.FourCC);
            vp.mfx.CodecProfile = profile;
        }
       if (core->IsGuidSupported(DXVA_Intel_ModeVVC_VLD, &vp) != MFX_ERR_NONE)
            return false;

        //Linux doesn't check GUID, just [mfxVideoParam]
        switch (profile)
        {
            case MFX_PROFILE_VVC_MAIN10:
            case MFX_PROFILE_VVC_MAIN10_STILL_PICTURE:
                return true;
            default:
                return false;
        }
        return false;
    }

    eMFXPlatform GetPlatform_VVC(VideoCORE * core, mfxVideoParam * par)
    {
        if (!par)
        {
            return MFX_PLATFORM_SOFTWARE;
        }

        eMFXPlatform platform = core->GetPlatformType();
        eMFXHWType typeHW = core->GetHWType();

        if (!VVCDCaps::IsPlatformSupported(typeHW))
        {
            platform = MFX_PLATFORM_SOFTWARE;
        }

        if (platform != MFX_PLATFORM_SOFTWARE && !CheckGUID(core, typeHW, par))
        {
            platform = MFX_PLATFORM_SOFTWARE;
        }

        return platform;
    }

    mfxStatus CheckFrameInfo(mfxFrameInfo & in, mfxFrameInfo & out)
    {
        mfxStatus sts = MFX_ERR_NONE;

        if (!((in.Width % 16 == 0) && (in.Width <= 16834)))
        {
            out.Width = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (!((in.Height % 16 == 0) && (in.Height <= 16834)))
        {
            out.Height = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (!(in.CropX <= out.Width))
        {
            out.CropX = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (!(in.CropY <= out.Height))
        {
            out.CropY = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (!(out.CropX + in.CropW <= out.Width))
        {
            out.CropW = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (!(out.CropY + in.CropH <= out.Height))
        {
            out.CropH = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (!(in.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ||
            in.PicStruct == MFX_PICSTRUCT_UNKNOWN))
        {
            out.PicStruct = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (!(in.ChromaFormat == MFX_CHROMAFORMAT_YUV420 || in.ChromaFormat == 0))
        {
            out.ChromaFormat = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        // ...

        return sts;
    }

    bool CheckVideoParam_VVC(mfxVideoParam *in)
    {
        if (!in)
        {
            return false;
        }

        if (in->mfx.CodecId != MFX_CODEC_VVC)
        {
            return false;
        }

        if (in->mfx.CodecProfile != MFX_PROFILE_VVC_MAIN10 &&
            in->mfx.CodecProfile != MFX_PROFILE_VVC_MAIN10_STILL_PICTURE)
        {
            return false;
        }

        if (in->mfx.FrameInfo.Width > VVC_MAX_FRAME_WIDTH ||
            in->mfx.FrameInfo.Height > VVC_MAX_FRAME_HEIGHT)
        {
            return false;
        }

        if (in->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12 &&
            in->mfx.FrameInfo.FourCC != MFX_FOURCC_P010)
        {
            return false;
        }

        if ((in->mfx.FrameInfo.AspectRatioW || in->mfx.FrameInfo.AspectRatioH) &&
            !(in->mfx.FrameInfo.AspectRatioW && in->mfx.FrameInfo.AspectRatioH))
        {
            return false;
        }

        if (in->mfx.FrameInfo.FourCC == MFX_FOURCC_P010)
        {
            if (in->mfx.FrameInfo.Shift > 1)
            {
                return false;
            }
        }
        else
        {
            if (in->mfx.FrameInfo.Shift)
            {
                return false;
            }
        }

        switch (in->mfx.FrameInfo.PicStruct)
        {
        case MFX_PICSTRUCT_PROGRESSIVE:
        case MFX_PICSTRUCT_FIELD_SINGLE:
            break;
        default:
            return false;
        }

        if (in->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 &&
            in->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV422 &&
            in->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
        {
            return false;
        }

        if (!(in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) &&
            !(in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        {
            return false;
        }

        if ((in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) &&
            (in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        {
            return false;
        }

        return true;
    }

    inline
    bool CheckChromaFormat(mfxU16 profile, mfxU16 format)
    {
        MFX_CHECK_WITH_ASSERT(profile == MFX_PROFILE_VVC_MAIN10 || profile == MFX_PROFILE_VVC_MAIN10_STILL_PICTURE, 0);

        if (format > MFX_CHROMAFORMAT_YUV444)
            return false;

        struct supported_t
        {
            mfxU16 profile;
            mfxI8  chroma[4];
        } static const supported[] =
        {
            { MFX_PROFILE_VVC_MAIN10,               { -1, MFX_CHROMAFORMAT_YUV420, MFX_CHROMAFORMAT_YUV422, MFX_CHROMAFORMAT_YUV444 } },
            { MFX_PROFILE_VVC_MAIN10_STILL_PICTURE, { -1, MFX_CHROMAFORMAT_YUV420, MFX_CHROMAFORMAT_YUV422, MFX_CHROMAFORMAT_YUV444 } },
        };

        supported_t const
            * first = supported,
            * last = first + sizeof(supported) / sizeof(supported[0]);
        for (; first != last; ++first)
            if (first->profile == profile)
                break;

        return first != last && (*first).chroma[format] != -1;
    }

    inline
    bool CheckBitDepth(mfxU16 profile, mfxU16 bit_depth)
    {
        MFX_CHECK_WITH_ASSERT(profile == MFX_PROFILE_VVC_MAIN10 || profile == MFX_PROFILE_VVC_MAIN10_STILL_PICTURE, 0);

        struct minmax_t
        {
            mfxU16 profile;
            mfxU8  lo, hi;
        } static const minmax[] =
        {
            { MFX_PROFILE_VVC_MAIN10,                8, 10 },
            { MFX_PROFILE_VVC_MAIN10_STILL_PICTURE,  8, 10 },
        };

        minmax_t const
            * first = minmax,
            * last = first + sizeof(minmax) / sizeof(minmax[0]);
        for (; first != last; ++first)
            if (first->profile == profile)
                break;

        return
            first != last &&
            !(bit_depth < first->lo) &&
            !(bit_depth > first->hi)
            ;
    }

    mfxStatus Query_VVC(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out)
    {
        MFX_CHECK_NULL_PTR1(out);
        mfxStatus  sts = MFX_ERR_NONE;

        if (in == out)
        {
            mfxVideoParam in1;
            MFX_INTERNAL_CPY(&in1, in, sizeof(mfxVideoParam));
            return MFX_Utility::Query_VVC(core, &in1, out);
        }

        memset(&out->mfx, 0, sizeof(mfxInfoMFX));

        if (in)
        {
            if (in->mfx.CodecId == MFX_CODEC_VVC)
            {
                out->mfx.CodecId = in->mfx.CodecId;
            }
            else
            {
                sts = MFX_ERR_UNSUPPORTED;
            }

            mfxU16 profile = MFX_PROFILE_VVC_MAIN10;
            if (in->mfx.CodecProfile == MFX_PROFILE_VVC_MAIN10 ||
                in->mfx.CodecProfile == MFX_PROFILE_VVC_MAIN10_STILL_PICTURE)
            {
                out->mfx.CodecProfile = in->mfx.CodecProfile;
            }
            else
            {
                sts = MFX_ERR_UNSUPPORTED;
            }
            if (out->mfx.CodecProfile != MFX_PROFILE_UNKNOWN)
            {
                profile = out->mfx.CodecProfile;
            }

            mfxU32 const level = ExtractProfile(in->mfx.CodecLevel);
            switch (level)
            {
            case MFX_LEVEL_UNKNOWN:
            case MFX_LEVEL_VVC_1:
            case MFX_LEVEL_VVC_2:
            case MFX_LEVEL_VVC_21:
            case MFX_LEVEL_VVC_3:
            case MFX_LEVEL_VVC_31:
            case MFX_LEVEL_VVC_4:
            case MFX_LEVEL_VVC_41:
            case MFX_LEVEL_VVC_5:
            case MFX_LEVEL_VVC_51:
            case MFX_LEVEL_VVC_52:
            case MFX_LEVEL_VVC_6:
            case MFX_LEVEL_VVC_61:
            case MFX_LEVEL_VVC_62:
            case MFX_LEVEL_VVC_63:
            case MFX_LEVEL_VVC_155:
                out->mfx.CodecLevel = in->mfx.CodecLevel;
                break;
            default:
                //sts = MFX_ERR_UNSUPPORTED;
                out->mfx.CodecLevel = 102;
                break;
            }

            if (in->mfx.NumThread < VVC_MAX_THREAD_NUM)
            {
                out->mfx.NumThread = in->mfx.NumThread;
            }
            else
            {
                sts = MFX_ERR_UNSUPPORTED;
            }

            out->AsyncDepth = in->AsyncDepth;

            out->mfx.MaxDecFrameBuffering = in->mfx.MaxDecFrameBuffering;

            out->mfx.DecodedOrder = in->mfx.DecodedOrder;
            if (in->mfx.DecodedOrder > VVC_MAX_UNSUPPORTED_FRAME_ORDER)
            {
                sts = MFX_ERR_UNSUPPORTED;
                out->mfx.DecodedOrder = 0;
            }

            if (in->mfx.ExtendedPicStruct)
            {
                if (in->mfx.ExtendedPicStruct == 1)
                {
                    out->mfx.ExtendedPicStruct = in->mfx.ExtendedPicStruct;
                }
                else
                {
                    sts = MFX_ERR_UNSUPPORTED;
                }
            }

            if (in->IOPattern)
            {
                if (in->IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY ||
                    in->IOPattern == MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
                {
                    out->IOPattern = in->IOPattern;
                }
                else
                {
                    sts = MFX_ERR_UNSUPPORTED;
                }
            }

            if (in->mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV400 ||
                CheckChromaFormat(profile, in->mfx.FrameInfo.ChromaFormat))
            {
                out->mfx.FrameInfo.ChromaFormat = in->mfx.FrameInfo.ChromaFormat;
            }
            else
            {
                sts = MFX_ERR_UNSUPPORTED;
            }

            if (in->mfx.FrameInfo.Width % VVC_MAX_FRAME_PIXEL_ALIGNED == 0 &&
                in->mfx.FrameInfo.Width <= VVC_MAX_FRAME_WIDTH)
            {
                out->mfx.FrameInfo.Width = in->mfx.FrameInfo.Width;
            }
            else
            {
                out->mfx.FrameInfo.Width = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            if (in->mfx.FrameInfo.Height % VVC_MAX_FRAME_PIXEL_ALIGNED == 0 &&
                in->mfx.FrameInfo.Height <= VVC_MAX_FRAME_HEIGHT)
            {
                out->mfx.FrameInfo.Height = in->mfx.FrameInfo.Height;
            }
            else
            {
                out->mfx.FrameInfo.Height = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            if ((in->mfx.FrameInfo.Width || in->mfx.FrameInfo.Height) &&
                !(in->mfx.FrameInfo.Width && in->mfx.FrameInfo.Height))
            {
                out->mfx.FrameInfo.Width = 0;
                out->mfx.FrameInfo.Height = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            if (!in->mfx.FrameInfo.Width || (
                in->mfx.FrameInfo.CropX <= in->mfx.FrameInfo.Width &&
                in->mfx.FrameInfo.CropY <= in->mfx.FrameInfo.Height &&
                in->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW <= in->mfx.FrameInfo.Width &&
                in->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH <= in->mfx.FrameInfo.Height))
            {
                out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX;
                out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY;
                out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;
                out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;
            }
            else {
                out->mfx.FrameInfo.CropX = 0;
                out->mfx.FrameInfo.CropY = 0;
                out->mfx.FrameInfo.CropW = 0;
                out->mfx.FrameInfo.CropH = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            out->mfx.FrameInfo.FrameRateExtN = in->mfx.FrameInfo.FrameRateExtN;
            out->mfx.FrameInfo.FrameRateExtD = in->mfx.FrameInfo.FrameRateExtD;

            if ((in->mfx.FrameInfo.FrameRateExtN || in->mfx.FrameInfo.FrameRateExtD) &&
                !(in->mfx.FrameInfo.FrameRateExtN && in->mfx.FrameInfo.FrameRateExtD))
            {
                out->mfx.FrameInfo.FrameRateExtN = 0;
                out->mfx.FrameInfo.FrameRateExtD = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            out->mfx.FrameInfo.AspectRatioW = in->mfx.FrameInfo.AspectRatioW;
            out->mfx.FrameInfo.AspectRatioH = in->mfx.FrameInfo.AspectRatioH;

            if ((in->mfx.FrameInfo.AspectRatioW || in->mfx.FrameInfo.AspectRatioH) &&
                !(in->mfx.FrameInfo.AspectRatioW && in->mfx.FrameInfo.AspectRatioH))
            {
                out->mfx.FrameInfo.AspectRatioW = 0;
                out->mfx.FrameInfo.AspectRatioH = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            out->mfx.FrameInfo.BitDepthLuma = in->mfx.FrameInfo.BitDepthLuma;
            if (in->mfx.FrameInfo.BitDepthLuma && !CheckBitDepth(profile, in->mfx.FrameInfo.BitDepthLuma))
            {
                out->mfx.FrameInfo.BitDepthLuma = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            out->mfx.FrameInfo.BitDepthChroma = in->mfx.FrameInfo.BitDepthChroma;
            if (in->mfx.FrameInfo.BitDepthChroma && !CheckBitDepth(profile, in->mfx.FrameInfo.BitDepthChroma))
            {
                out->mfx.FrameInfo.BitDepthChroma = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            out->mfx.FrameInfo.FourCC = in->mfx.FrameInfo.FourCC;

            out->mfx.FrameInfo.Shift = in->mfx.FrameInfo.Shift;

            switch (in->mfx.FrameInfo.PicStruct)
            {
            case MFX_PICSTRUCT_UNKNOWN:
            case MFX_PICSTRUCT_PROGRESSIVE:
            case MFX_PICSTRUCT_FIELD_SINGLE:
                out->mfx.FrameInfo.PicStruct = in->mfx.FrameInfo.PicStruct;
                break;
            default:
                sts = MFX_ERR_UNSUPPORTED;
                break;
            }
        }
        else
        {
            out->mfx.CodecId = MFX_CODEC_VVC;
            out->mfx.CodecProfile = 1;
            out->mfx.CodecLevel = 1;
            out->mfx.NumThread = 1;
            out->mfx.DecodedOrder = 0;
            out->mfx.ExtendedPicStruct = 1;
            out->AsyncDepth = 1;

            // mfxFrameInfo
            out->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            out->mfx.FrameInfo.Width = 16;
            out->mfx.FrameInfo.Height = 16;

            out->mfx.FrameInfo.FrameRateExtN = 1;
            out->mfx.FrameInfo.FrameRateExtD = 1;

            out->mfx.FrameInfo.AspectRatioW = 1;
            out->mfx.FrameInfo.AspectRatioH = 1;

            out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

            out->mfx.FrameInfo.BitDepthLuma = 8;
            out->mfx.FrameInfo.BitDepthChroma = 8;
            out->mfx.FrameInfo.Shift = 0;

            out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

            if (core->GetHWType() == MFX_HW_UNKNOWN)
            {
                out->IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
            }
            else
            {
                out->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
            }
        }

        return sts;
    }

    void GetMfxFrameRate(uint8_t frame_rate_value, mfxU32 & frameRateN, mfxU32 & frameRateD)
    {
        //TODO, update code refering to VVC spec
        switch (frame_rate_value)
        {
            case 0:  frameRateN = 30;    frameRateD = 1;    break;
            case 1:  frameRateN = 24000; frameRateD = 1001; break;
            case 2:  frameRateN = 24;    frameRateD = 1;    break;
            case 3:  frameRateN = 25;    frameRateD = 1;    break;
            case 4:  frameRateN = 30000; frameRateD = 1001; break;
            case 5:  frameRateN = 30;    frameRateD = 1;    break;
            case 6:  frameRateN = 50;    frameRateD = 1;    break;
            case 7:  frameRateN = 60000; frameRateD = 1001; break;
            case 8:  frameRateN = 60;    frameRateD = 1;    break;
            default: frameRateN = 30;    frameRateD = 1;
        }
        return;
    }

    mfxU8 GetMfxCodecProfile(uint8_t profile)
    {
        switch (profile)
        {
            case 1:
                return MFX_PROFILE_MPEG2_MAIN;
            default:
                return MFX_PROFILE_UNKNOWN;
        }
    }

    mfxU8 GetMfxCodecLevel(uint8_t level)
    {
        switch (level)
        {
            case 1:
                return MFX_PROFILE_UNKNOWN;
            default:
                return MFX_LEVEL_UNKNOWN;
        }
    }

    UMC::Status getWinUnit(int chromaIdc, int& unitX, int& unitY)
    {
        const int winUnitX[] = { 1,2,2,1 };
        const int WinUnitY[] = { 1,2,1,1 };

        if (chromaIdc < CHROMA_FORMAT_400 || chromaIdc > CHROMA_FORMAT_444)
            return UMC::UMC_ERR_FAILED;

        unitX = winUnitX[chromaIdc];
        unitY = WinUnitY[chromaIdc];
        return UMC::UMC_OK;
    }

    }
}

#endif //MFX_ENABLE_VVC_VIDEO_DECODE
