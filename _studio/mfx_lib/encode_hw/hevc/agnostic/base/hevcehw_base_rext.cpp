// Copyright (c) 2019-2021 Intel Corporation
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

#include "hevcehw_base_rext.h"
#include "hevcehw_base_legacy.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;

void RExt::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetRecInfo
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        auto& par = Base::Glob::VideoParam::Get(strg);
        mfxExtCodingOption3& CO3 = ExtBuffer::Get(par);

        bool bG12SpecificRec =
            (CO3.TargetBitDepthLuma == 12 || CO3.TargetBitDepthLuma == 10)
             && (CO3.TargetChromaFormatPlus1 == (1 + MFX_CHROMAFORMAT_YUV420)
                 || CO3.TargetChromaFormatPlus1 == (1 + MFX_CHROMAFORMAT_YUV422));

        bG12SpecificRec = 
            bG12SpecificRec 
            || (CO3.TargetBitDepthLuma == 12 && CO3.TargetChromaFormatPlus1 == (1 + MFX_CHROMAFORMAT_YUV444));

        MFX_CHECK(bG12SpecificRec, MFX_ERR_NONE);

        local.Erase(Base::Tmp::RecInfo::Key);

        auto pRI = make_storable<mfxFrameAllocRequest>(mfxFrameAllocRequest{});
        auto& rec = pRI->Info;
        rec = par.mfx.FrameInfo;
        auto itUpdateRecInfo = mUpdateRecInfo.find(CO3.TargetChromaFormatPlus1);
        bool bUndef = (itUpdateRecInfo == mUpdateRecInfo.end());

        if (!bUndef)
            mUpdateRecInfo.at(CO3.TargetChromaFormatPlus1)(rec, pRI->Type, IsOn(par.mfx.LowPower));

        rec.ChromaFormat   = CO3.TargetChromaFormatPlus1 - 1;
        rec.BitDepthLuma   = CO3.TargetBitDepthLuma;
        rec.BitDepthChroma = CO3.TargetBitDepthChroma;

        local.Insert(Base::Tmp::RecInfo::Key, std::move(pRI));

        return MFX_ERR_NONE;
    });
}

void RExt::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_SetDefaultsCallChain
        , [this](const mfxVideoParam&, mfxVideoParam&, StorageRW& strg) -> mfxStatus
    {
        auto& defaults = Glob::Defaults::GetOrConstruct(strg);
        auto& bSet = defaults.SetForFeature[GetID()];
        MFX_CHECK(!bSet, MFX_ERR_NONE);

        defaults.CheckFourCC.Push(
            [](Base::Defaults::TCheckAndFix::TExt prev
                , const Base::Defaults::Param& dpar
                , mfxVideoParam& par)
        {
            MFX_CHECK(IsRextFourCC(par.mfx.FrameInfo.FourCC), prev(dpar, par));
            return MFX_ERR_NONE;
        });

        defaults.CheckInputFormatByFourCC.Push(
            [](Base::Defaults::TCheckAndFix::TExt prev
                , const Base::Defaults::Param& dpar
                , mfxVideoParam& par)
        {
            MFX_CHECK(IsRextFourCC(par.mfx.FrameInfo.FourCC), prev(dpar, par));

            auto&  fi      = par.mfx.FrameInfo; 
            mfxU32 invalid = 0;

            if (IsOn(par.mfx.LowPower))
            {
                invalid += Check<mfxU16, 10 >(fi.BitDepthLuma);
                invalid += Check<mfxU16, 10 >(fi.BitDepthChroma);
            }
            else
            {
                invalid += CheckOrZero<mfxU16, 12, 0>(fi.BitDepthLuma);
                invalid += CheckOrZero<mfxU16, 12, 0>(fi.BitDepthChroma);
            }

            invalid += (fi.FourCC == MFX_FOURCC_P016) && CheckOrZero<mfxU16, MFX_CHROMAFORMAT_YUV420>(fi.ChromaFormat);
            invalid += (fi.FourCC == MFX_FOURCC_Y216) && CheckOrZero<mfxU16, MFX_CHROMAFORMAT_YUV422>(fi.ChromaFormat);
            invalid += (fi.FourCC == MFX_FOURCC_Y416) && CheckOrZero<mfxU16, MFX_CHROMAFORMAT_YUV444>(fi.ChromaFormat);

            MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);
            return MFX_ERR_NONE;
        });

        defaults.CheckTargetBitDepth.Push(
            [](Base::Defaults::TCheckAndFix::TExt prev
                , const Base::Defaults::Param& dpar
                , mfxVideoParam& par)
        {
            mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);

            MFX_CHECK(pCO3 && IsRextFourCC(par.mfx.FrameInfo.FourCC), prev(dpar, par));

            mfxU32 invalid = 0;

            if (IsOn(par.mfx.LowPower))
            {
                invalid += CheckOrZero<mfxU16, 10, 0>(pCO3->TargetBitDepthLuma);
                invalid += CheckOrZero<mfxU16, 10, 0>(pCO3->TargetBitDepthChroma);
            }
            else
            {
                invalid += CheckOrZero<mfxU16, 12, 0>(pCO3->TargetBitDepthLuma);
                invalid += CheckOrZero<mfxU16, 12, 0>(pCO3->TargetBitDepthChroma);
            }

            MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);
            return MFX_ERR_NONE;
        });

        defaults.CheckFourCCByTargetFormat.Push(
            [](Base::Defaults::TCheckAndFix::TExt prev
                , const Base::Defaults::Param& dpar
                , mfxVideoParam& par)
        {
            MFX_CHECK(IsRextFourCC(par.mfx.FrameInfo.FourCC), prev(dpar, par));

            auto   tcf = dpar.base.GetTargetChromaFormat(dpar);
            auto&  fi = par.mfx.FrameInfo;
            mfxU32 invalid = 0;

            invalid +=
                (tcf == MFX_CHROMAFORMAT_YUV444 + 1)
                && Check<mfxU32, MFX_FOURCC_Y416>(fi.FourCC);

            invalid +=
                (tcf == MFX_CHROMAFORMAT_YUV422 + 1)
                && Check<mfxU32, MFX_FOURCC_Y216, MFX_FOURCC_Y416>(fi.FourCC);

            MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);
            return MFX_ERR_NONE;
        });

        defaults.CheckProfile.Push(
            [](Base::Defaults::TCheckAndFix::TExt prev
                , const Base::Defaults::Param& dpar
                , mfxVideoParam& par)
        {
            MFX_CHECK(IsRextFourCC(par.mfx.FrameInfo.FourCC), prev(dpar, par));

            bool bInvalid = CheckOrZero<mfxU16, 0, MFX_PROFILE_HEVC_REXT>(par.mfx.CodecProfile);

            MFX_CHECK(!bInvalid, MFX_ERR_UNSUPPORTED);
            return MFX_ERR_NONE;
        });

        defaults.GetMaxChromaByFourCC.Push(
            [](Base::Defaults::TChain<mfxU16>::TExt prev
                , const Base::Defaults::Param& dpar)
        {
            MFX_CHECK(IsRextFourCC(dpar.mvp.mfx.FrameInfo.FourCC), prev(dpar));
            auto fcc = dpar.mvp.mfx.FrameInfo.FourCC;
            return mfxU16(
                  (fcc == MFX_FOURCC_P016) * MFX_CHROMAFORMAT_YUV420
                + (fcc == MFX_FOURCC_Y216) * MFX_CHROMAFORMAT_YUV422
                + (fcc == MFX_FOURCC_Y416) * MFX_CHROMAFORMAT_YUV444);
        });

        defaults.GetMaxBitDepthByFourCC.Push(
            [](Base::Defaults::TChain<mfxU16>::TExt prev
                , const Base::Defaults::Param& dpar)
        {
            MFX_CHECK(IsRextFourCC(dpar.mvp.mfx.FrameInfo.FourCC), prev(dpar));
            return mfxU16(12);
        });

        defaults.RunFastCopyWrapper.Push(
            [](Base::Defaults::TRunFastCopyWrapper::TExt prev
                , mfxFrameSurface1 &surfDst
                , mfxU16 dstMemType
                , mfxFrameSurface1 &surfSrc
                , mfxU16 srcMemType)
            {
                // convert to native shift in core.CopyFrame() if required
                surfDst.Info.Shift |=
                    (mfxU16) (surfDst.Info.FourCC == MFX_FOURCC_P016
                    || surfDst.Info.FourCC == MFX_FOURCC_Y216);

                return prev(
                     surfDst
                    , dstMemType
                    , surfSrc
                    , srcMemType);
            });

        bSet = true;

        return MFX_ERR_NONE;

    });
}

void RExt::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckShift
        , [](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        auto& fi = out.mfx.FrameInfo;
        bool bVideoMem = Base::Legacy::IsInVideoMem(out);

        bool bNeedShift =
            (bVideoMem && !fi.Shift)
            && (   fi.FourCC == MFX_FOURCC_P016
                || fi.FourCC == MFX_FOURCC_Y216
                || fi.FourCC == MFX_FOURCC_Y416);

        SetIf(fi.Shift, bNeedShift, 1);
        MFX_CHECK(!bNeedShift, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        return MFX_ERR_NONE;
    });

    Push(BLK_HardcodeCaps
        , [](const mfxVideoParam& par, mfxVideoParam&, StorageRW& strg) -> mfxStatus
    {
        Base::Glob::EncodeCaps::Get(strg).MaxEncodedBitDepth = IsOn(par.mfx.LowPower) ? 1 : 2;
        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
