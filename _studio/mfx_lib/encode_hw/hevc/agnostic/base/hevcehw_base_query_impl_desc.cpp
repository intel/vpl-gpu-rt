// Copyright (c) 2020-2025 Intel Corporation
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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_base_query_impl_desc.h"
#include "hevcehw_base_legacy.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;

void QueryImplDesc::QueryImplsDescription(const FeatureBlocks& blocks, TPushQID Push)
{
    auto Query = [&blocks](
        VideoCORE& core
        , mfxEncoderDescription::encoder& caps
        , mfx::PODArraysHolder& ah
        , StorageRW&) -> mfxStatus
    {
        const mfxU32 FourCC[] =
        {
            MFX_FOURCC_NV12
            , MFX_FOURCC_YV12
            , MFX_FOURCC_NV16
            , MFX_FOURCC_YUY2
            , MFX_FOURCC_RGB565
            , MFX_FOURCC_RGBP
            , MFX_FOURCC_RGB4
            , MFX_FOURCC_P010
            , MFX_FOURCC_P016
            , MFX_FOURCC_P210
            , MFX_FOURCC_BGR4
            , MFX_FOURCC_A2RGB10
            , MFX_FOURCC_ARGB16
            , MFX_FOURCC_ABGR16
            , MFX_FOURCC_AYUV
            , MFX_FOURCC_AYUV_RGB4
            , MFX_FOURCC_UYVY
            , MFX_FOURCC_Y210
            , MFX_FOURCC_Y410
            , MFX_FOURCC_Y216
            , MFX_FOURCC_Y416
            , MFX_FOURCC_NV21
            , MFX_FOURCC_IYUV
            , MFX_FOURCC_I010
        };
#ifdef ONEVPL_EXPERIMENTAL
        const mfxU16 ChromaFormat[] =
        {
            MFX_CHROMAFORMAT_YUV420
            , MFX_CHROMAFORMAT_YUV422
            , MFX_CHROMAFORMAT_YUV444
        };
        const mfxU16 RateControl[] =
        {
            MFX_RATECONTROL_CBR
            , MFX_RATECONTROL_VBR
            , MFX_RATECONTROL_CQP
            , MFX_RATECONTROL_ICQ
            , MFX_RATECONTROL_VCM
            , MFX_RATECONTROL_QVBR
        };
        const mfxU32 ExtendedBuffer[] =
        {
            MFX_EXTBUFF_CODING_OPTION
            , MFX_EXTBUFF_CODING_OPTION2
            , MFX_EXTBUFF_CODING_OPTION3
            , MFX_EXTBUFF_HEVC_TILES
            , MFX_EXTBUFF_HEVC_PARAM
            , MFX_EXTBUFF_HEVC_REGION
            , MFX_EXTBUFF_HEVC_REFLIST_CTRL
            , MFX_EXTBUFF_HEVC_REFLISTS
            , MFX_EXTBUFF_HEVC_TEMPORAL_LAYERS
            , MFX_EXTBUFF_ENCODER_ROI
            , MFX_EXTBUFF_MBQP
            , MFX_EXTBUFF_DIRTY_RECTANGLES
            , MFX_EXTBUFF_PRED_WEIGHT_TABLE
        };
#endif
        struct Config
        {
            mfxU16 Profile;
            mfxU32 FourCC;
            mfxU16 BitDepth;
            mfxU16 Shift;
            mfxU16 ChromaFormat;
            mfxU16 LowPower;
        };

        const Config cfgs[] =
        {
              {MFX_PROFILE_HEVC_MAIN  , MFX_FOURCC_NV12,  8, 0, MFX_CHROMAFORMAT_YUV420, MFX_CODINGOPTION_UNKNOWN}
            , {MFX_PROFILE_HEVC_MAIN  , MFX_FOURCC_NV12,  8, 0, MFX_CHROMAFORMAT_YUV420, MFX_CODINGOPTION_ON}
            , {MFX_PROFILE_HEVC_MAIN10, MFX_FOURCC_P010, 10, 1, MFX_CHROMAFORMAT_YUV420, MFX_CODINGOPTION_UNKNOWN}
            , {MFX_PROFILE_HEVC_MAIN10, MFX_FOURCC_P010, 10, 1, MFX_CHROMAFORMAT_YUV420, MFX_CODINGOPTION_ON}
            , {MFX_PROFILE_HEVC_MAINSP, MFX_FOURCC_NV12,  8, 0, MFX_CHROMAFORMAT_YUV420, MFX_CODINGOPTION_UNKNOWN}
            , {MFX_PROFILE_HEVC_MAINSP, MFX_FOURCC_NV12,  8, 0, MFX_CHROMAFORMAT_YUV420, MFX_CODINGOPTION_ON}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_YUY2,  8, 0, MFX_CHROMAFORMAT_YUV422, MFX_CODINGOPTION_UNKNOWN}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_AYUV,  8, 0, MFX_CHROMAFORMAT_YUV444, MFX_CODINGOPTION_UNKNOWN}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_P010, 10, 1, MFX_CHROMAFORMAT_YUV420, MFX_CODINGOPTION_UNKNOWN}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_Y210, 10, 1, MFX_CHROMAFORMAT_YUV422, MFX_CODINGOPTION_UNKNOWN}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_Y410, 10, 0, MFX_CHROMAFORMAT_YUV444, MFX_CODINGOPTION_UNKNOWN}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_P016, 12, 1, MFX_CHROMAFORMAT_YUV420, MFX_CODINGOPTION_UNKNOWN}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_Y216, 12, 1, MFX_CHROMAFORMAT_YUV422, MFX_CODINGOPTION_UNKNOWN}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_Y416, 12, 1, MFX_CHROMAFORMAT_YUV444, MFX_CODINGOPTION_UNKNOWN}
            , {MFX_PROFILE_HEVC_SCC   , MFX_FOURCC_NV12,  8, 0, MFX_CHROMAFORMAT_YUV420, MFX_CODINGOPTION_UNKNOWN}
            , {MFX_PROFILE_HEVC_SCC   , MFX_FOURCC_P010, 10, 1, MFX_CHROMAFORMAT_YUV420, MFX_CODINGOPTION_UNKNOWN}
            , {MFX_PROFILE_HEVC_SCC   , MFX_FOURCC_AYUV,  8, 0, MFX_CHROMAFORMAT_YUV444, MFX_CODINGOPTION_UNKNOWN}
            , {MFX_PROFILE_HEVC_SCC   , MFX_FOURCC_Y410, 10, 0, MFX_CHROMAFORMAT_YUV444, MFX_CODINGOPTION_UNKNOWN}
        };

        auto& queryNC = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(blocks);
        auto& queryWC = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);

#ifdef ONEVPL_EXPERIMENTAL
        const auto& blockCheckBRC = FeatureBlocks::Get(queryWC, { FEATURE_LEGACY, Legacy::BLK_CheckBRC });
#endif

        caps.CodecID                 = MFX_CODEC_HEVC;
        caps.MaxcodecLevel           = (MFX_TIER_HEVC_HIGH | MFX_LEVEL_HEVC_62);
        caps.BiDirectionalPrediction = 0;

        struct TmpCaps
        {
            mfxRange32U Width  = {0xffffffff, 0, 0xffffffff};
            mfxRange32U Height = {0xffffffff, 0, 0xffffffff};
            std::list<mfxU32> FourCC;
#ifdef ONEVPL_EXPERIMENTAL
            std::list<mfxU16> TargetChromaSubsamplings;
            mfxU16 TargetMaxBitDepth = 0;
#endif
        };
        std::map<mfxU32, TmpCaps> tmpCaps;

#ifdef ONEVPL_EXPERIMENTAL
        std::list<mfxU16> supportedRC;
#endif

        for (auto& cfg : cfgs)
        {
            StorageRW tmpStorage;
            tmpStorage.Insert(Glob::VideoCore::Key, new StorableRef<VideoCORE>(core));
            ExtBuffer::Param<mfxVideoParam> in, out;

            in.mfx.CodecId = out.mfx.CodecId = caps.CodecID;
            in.IOPattern                     = MFX_IOPATTERN_IN_VIDEO_MEMORY;
            in.mfx.CodecProfile              = cfg.Profile;
            in.mfx.FrameInfo.Width           = 1920;
            in.mfx.FrameInfo.Height          = 1088;
            in.mfx.FrameInfo.FourCC          = cfg.FourCC;
            in.mfx.FrameInfo.ChromaFormat    = cfg.ChromaFormat;
            in.mfx.FrameInfo.BitDepthLuma    = cfg.BitDepth;
            in.mfx.FrameInfo.Shift           = cfg.Shift;
            in.mfx.LowPower                  = cfg.LowPower;

            auto pCO3 = (mfxExtCodingOption3*)in.NewEB(MFX_EXTBUFF_CODING_OPTION3);
            auto pCO3_out = (mfxExtCodingOption3*)out.NewEB(MFX_EXTBUFF_CODING_OPTION3);

            MFX_CHECK(pCO3, MFX_ERR_UNKNOWN);
            pCO3->TargetBitDepthLuma      = cfg.BitDepth;
            pCO3->TargetChromaFormatPlus1 = cfg.ChromaFormat + 1;

            if (MFX_ERR_NONE != RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, queryNC, in, out, tmpStorage))
                continue;

#ifdef ONEVPL_EXPERIMENTAL
            if (out.mfx.LowPower != MFX_CODINGOPTION_ON)
                continue;
#endif

            if (MFX_ERR_NONE != RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, queryWC, in, out, tmpStorage))
                continue;

#ifdef ONEVPL_EXPERIMENTAL
            auto IsSupportedRateControl = [&](mfxU16 rc)
            {
                out.mfx.RateControlMethod = rc;
                return blockCheckBRC->Call(in, out, tmpStorage) == MFX_ERR_NONE;
            };

            for (mfxU16 rc : RateControl)
            {
                if (std::find(supportedRC.begin(), supportedRC.end(), rc) != supportedRC.end())
                    continue;
                if (IsSupportedRateControl(rc))
                    supportedRC.push_back(rc);
            }
#endif

            auto& currCaps = tmpCaps[cfg.Profile];
            Defaults::Param defs(
                out
                , Glob::EncodeCaps::Get(tmpStorage)
                , core.GetHWType()
                , Glob::Defaults::Get(tmpStorage));

            mfxU32 step = defs.base.GetCodedPicAlignment(defs);

#ifdef ONEVPL_EXPERIMENTAL
            auto IsUnsupportedChromaFormat = [&](mfxU16 chroma_format)
            {
                pCO3_out->TargetChromaFormatPlus1 = chroma_format + 1;
                return defs.base.CheckTargetChromaFormat(defs, out) != MFX_ERR_NONE;
            };

            std::list<mfxU16> supportedChromaFormat(std::begin(ChromaFormat), std::end(ChromaFormat));
            supportedChromaFormat.remove_if(IsUnsupportedChromaFormat);
            currCaps.TargetChromaSubsamplings.splice(currCaps.TargetChromaSubsamplings.end(), supportedChromaFormat);

            mfxU16 bitDepth = defs.base.GetMaxBitDepth(defs);

            CheckMinOrClip(currCaps.TargetMaxBitDepth, bitDepth);
#endif

            auto IsUnsupportedFourCC = [&](mfxU32 fcc)
            {
                out.mfx.FrameInfo.FourCC = fcc;
                return defs.base.CheckFourCC(defs, out) != MFX_ERR_NONE;
            };

            std::list<mfxU32> supportedFourCC(std::begin(FourCC), std::end(FourCC));
            supportedFourCC.remove_if(IsUnsupportedFourCC);
            currCaps.FourCC.splice(currCaps.FourCC.end(), supportedFourCC);

            CheckMaxOrClip(currCaps.Width.Step, step);
            CheckMinOrClip(currCaps.Width.Max, defs.caps.MaxPicWidth);
            currCaps.Width.Min = currCaps.Width.Step;

            CheckMaxOrClip(currCaps.Height.Step, step);
            CheckMinOrClip(currCaps.Height.Max, defs.caps.MaxPicHeight);
            currCaps.Height.Min = currCaps.Height.Step;

            caps.BiDirectionalPrediction |= !defs.caps.SliceIPOnly;
        }
        
        for (auto& intCaps : tmpCaps)
        {
            auto& profileCaps = ah.PushBack(caps.Profiles);

            profileCaps.Profile = intCaps.first;

            {
                auto& memCaps = ah.PushBack(profileCaps.MemDesc);

                memCaps.Width = intCaps.second.Width;
                memCaps.Height = intCaps.second.Height;

                intCaps.second.FourCC.sort();
                intCaps.second.FourCC.unique();

                for (mfxU32 fcc : intCaps.second.FourCC)
                {
                    ah.PushBack(memCaps.ColorFormats) = fcc;
                    ++memCaps.NumColorFormats;
                }

#ifdef ONEVPL_EXPERIMENTAL
                intCaps.second.TargetChromaSubsamplings.sort();
                intCaps.second.TargetChromaSubsamplings.unique();

                auto& memCapsExt = ah.PushBack(memCaps.MemExtDesc);
                memCapsExt.Version.Version = MFX_STRUCT_VERSION(1, 0);

                for (mfxU16 chroma_format : intCaps.second.TargetChromaSubsamplings)
                {
                    ah.PushBack(memCapsExt.TargetChromaSubsamplings) = chroma_format;
                    ++memCapsExt.NumTargetChromaSubsamplings;
                }

                memCapsExt.TargetMaxBitDepth = intCaps.second.TargetMaxBitDepth;
#endif
            }

            ah.PushBack(profileCaps.MemDesc);
            profileCaps.MemDesc[1] = profileCaps.MemDesc[0];
            profileCaps.MemDesc[0].MemHandleType = MFX_RESOURCE_SYSTEM_SURFACE;
            profileCaps.MemDesc[1].MemHandleType = core.GetVAType() == MFX_HW_VAAPI ? MFX_RESOURCE_VA_SURFACE : MFX_RESOURCE_DX11_TEXTURE;

            profileCaps.NumMemTypes = 2;

            ++caps.NumProfiles;
        }

#ifdef ONEVPL_EXPERIMENTAL
        auto& extDesc = ah.PushBack(caps.EncExtDesc);
        extDesc.Version.Version = MFX_STRUCT_VERSION(1, 0);

        supportedRC.sort();

        for (mfxU16 rc : supportedRC)
        {
            ah.PushBack(extDesc.RateControlMethods) = rc;
            ++extDesc.NumRateControlMethods;
        }

        for (mfxU32 ext_buf_id : ExtendedBuffer)
        {
            ah.PushBack(extDesc.ExtBufferIDs) = ext_buf_id;
            ++extDesc.NumExtBufferIDs;
        }
#endif

        return MFX_ERR_NONE;
    };

    Push(BLK_Query, Query);
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
