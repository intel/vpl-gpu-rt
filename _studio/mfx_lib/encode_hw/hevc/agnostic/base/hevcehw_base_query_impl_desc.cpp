// Copyright (c) 2020 Intel Corporation
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
        struct Config
        {
            mfxU16 Profile;
            mfxU32 FourCC;
            mfxU16 BitDepth;
            mfxU16 Shift;
            mfxU16 CromaFormat;
        };
        const Config cfgs[] =
        {
              {MFX_PROFILE_HEVC_MAIN  , MFX_FOURCC_NV12,  8, 0, MFX_CHROMAFORMAT_YUV420}
            , {MFX_PROFILE_HEVC_MAIN10, MFX_FOURCC_P010, 10, 1, MFX_CHROMAFORMAT_YUV420}
            , {MFX_PROFILE_HEVC_MAINSP, MFX_FOURCC_NV12,  8, 0, MFX_CHROMAFORMAT_YUV420}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_YUY2,  8, 0, MFX_CHROMAFORMAT_YUV422}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_AYUV,  8, 0, MFX_CHROMAFORMAT_YUV444}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_P010, 10, 1, MFX_CHROMAFORMAT_YUV420}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_Y210, 10, 1, MFX_CHROMAFORMAT_YUV422}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_Y410, 10, 0, MFX_CHROMAFORMAT_YUV444}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_P016, 12, 1, MFX_CHROMAFORMAT_YUV420}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_Y216, 12, 1, MFX_CHROMAFORMAT_YUV422}
            , {MFX_PROFILE_HEVC_REXT  , MFX_FOURCC_Y416, 12, 1, MFX_CHROMAFORMAT_YUV444}
            , {MFX_PROFILE_HEVC_SCC   , MFX_FOURCC_NV12,  8, 0, MFX_CHROMAFORMAT_YUV420}
            , {MFX_PROFILE_HEVC_SCC   , MFX_FOURCC_P010, 10, 1, MFX_CHROMAFORMAT_YUV420}
            , {MFX_PROFILE_HEVC_SCC   , MFX_FOURCC_AYUV,  8, 0, MFX_CHROMAFORMAT_YUV444}
            , {MFX_PROFILE_HEVC_SCC   , MFX_FOURCC_Y410, 10, 0, MFX_CHROMAFORMAT_YUV444}
        };

        auto& queryNC = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(blocks);
        auto& queryWC = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);

        caps.CodecID                 = MFX_CODEC_HEVC;
        caps.MaxcodecLevel           = (MFX_TIER_HEVC_HIGH | MFX_LEVEL_HEVC_62);
        caps.BiDirectionalPrediction = 0;

        struct TmpCaps
        {
            mfxRange32U Width  = {0xffffffff, 0, 0xffffffff};
            mfxRange32U Height = {0xffffffff, 0, 0xffffffff};
            std::list<mfxU32> FourCC;
        };
        std::map<mfxU32, TmpCaps> tmpCaps;

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
            in.mfx.FrameInfo.ChromaFormat    = cfg.CromaFormat;
            in.mfx.FrameInfo.BitDepthLuma    = cfg.BitDepth;
            in.mfx.FrameInfo.Shift           = cfg.Shift;

            auto pCO3 = (mfxExtCodingOption3*)in.NewEB(MFX_EXTBUFF_CODING_OPTION3);
            out.NewEB(MFX_EXTBUFF_CODING_OPTION3);

            MFX_CHECK(pCO3, MFX_ERR_UNKNOWN);
            pCO3->TargetBitDepthLuma      = cfg.BitDepth;
            pCO3->TargetChromaFormatPlus1 = cfg.CromaFormat + 1;

            if (MFX_ERR_NONE != RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, queryNC, in, out, tmpStorage))
                continue;
            if (MFX_ERR_NONE != RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, queryWC, in, out, tmpStorage))
                continue;

            auto& currCaps = tmpCaps[cfg.Profile];
            Defaults::Param defs(
                out
                , Glob::EncodeCaps::Get(tmpStorage)
                , core.GetHWType()
                , Glob::Defaults::Get(tmpStorage));
            auto IsUnsupportedFourCC = [&](mfxU32 fcc)
            {
                out.mfx.FrameInfo.FourCC = fcc;
                return defs.base.CheckFourCC(defs, out) != MFX_ERR_NONE;
            };
            mfxU32 step = defs.base.GetCodedPicAlignment(defs);

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
            }

            ah.PushBack(profileCaps.MemDesc);
            profileCaps.MemDesc[1] = profileCaps.MemDesc[0];
            profileCaps.MemDesc[0].MemHandleType = MFX_RESOURCE_SYSTEM_SURFACE;
            profileCaps.MemDesc[1].MemHandleType = core.GetVAType() == MFX_HW_VAAPI ? MFX_RESOURCE_VA_SURFACE : MFX_RESOURCE_DX11_TEXTURE;

            profileCaps.NumMemTypes = 2;

            ++caps.NumProfiles;
        }

        return MFX_ERR_NONE;
    };

    Push(BLK_Query, Query);
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
