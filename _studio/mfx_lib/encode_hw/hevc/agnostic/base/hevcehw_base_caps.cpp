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

#include "hevcehw_base_caps.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;

void Caps::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    using Base::Glob;
    using Base::Defaults;

    Push(BLK_SetLowPower,
        [](const mfxVideoParam&, mfxVideoParam& par, StorageRW&) -> mfxStatus
    {
        mfxU32 invalid = 0;

        invalid += Check<mfxU16
            , MFX_CODINGOPTION_UNKNOWN
            , MFX_CODINGOPTION_ON
            , MFX_CODINGOPTION_OFF>
            (par.mfx.LowPower);

        if (invalid)
        {
            par.mfx.LowPower = MFX_CODINGOPTION_ON;
            return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        SetIf(par.mfx.LowPower, par.mfx.LowPower == MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON);

        return MFX_ERR_NONE;
    });

    Push(BLK_SetDefaultsCallChain,
        [this](const mfxVideoParam&, mfxVideoParam&, StorageRW& strg) -> mfxStatus
    {
        auto& defaults = Glob::Defaults::GetOrConstruct(strg);
        auto& bSet = defaults.SetForFeature[GetID()];
        MFX_CHECK(!bSet, MFX_ERR_NONE);

        defaults.GetLowPower.Push([](
            Defaults::TGetHWDefault<mfxU16>::TExt /*prev*/
            , const mfxVideoParam& /*par*/
            , eMFXHWType /*hw*/)
        {
            return mfxU16(MFX_CODINGOPTION_ON);
        });

        defaults.GetMaxNumRef.Push([](
            Base::Defaults::TChain<std::tuple<mfxU16, mfxU16, mfxU16>>::TExt
            , const Base::Defaults::Param& dpar)
        {
            //HEVC VDENC Maximum supported number
            const mfxU16 nRef[3] = { 3, 2, 1 };

            mfxU16 numRefFrame = dpar.mvp.mfx.NumRefFrame + !dpar.mvp.mfx.NumRefFrame * 16;

            return std::make_tuple(
                std::min<mfxU16>(nRef[0], std::min<mfxU16>(dpar.caps.MaxNum_Reference0, numRefFrame))
                , std::min<mfxU16>(nRef[1], std::min<mfxU16>(dpar.caps.MaxNum_Reference0, numRefFrame))
                , std::min<mfxU16>(nRef[2], std::min<mfxU16>(dpar.caps.MaxNum_Reference1, numRefFrame)));
        });

        defaults.GetNumRefActive.Push([](
            Base::Defaults::TGetNumRefActive::TExt
            , const Base::Defaults::Param& dpar
            , mfxU16(*pP)[8]
            , mfxU16(*pBL0)[8]
            , mfxU16(*pBL1)[8])
        {
            bool bExternal = false;
            mfxU16 defaultP = 0, defaultBL0 = 0, defaultBL1 = 0;

            const mfxU16 nRef[3][7] =
            {
                // HEVC VDENC default reference number with TU
                { 3, 3, 2, 2, 2, 1, 1 },
                { 2, 2, 1, 1, 1, 1, 1 },
                { 1, 1, 1, 1, 1, 1, 1 }

            };
            mfxU16 tu = dpar.mvp.mfx.TargetUsage;

            CheckRangeOrSetDefault<mfxU16>(tu, 1, 7, 4);
            --tu;

            mfxU16 numRefFrame = dpar.mvp.mfx.NumRefFrame + !dpar.mvp.mfx.NumRefFrame * 16;

            // Get default active frame number
            std::tie(defaultP, defaultBL0, defaultBL1) = std::make_tuple(
                std::min<mfxU16>(nRef[0][tu], std::min<mfxU16>(dpar.caps.MaxNum_Reference0, numRefFrame))
                , std::min<mfxU16>(nRef[1][tu], std::min<mfxU16>(dpar.caps.MaxNum_Reference0, numRefFrame))
                , std::min<mfxU16>(nRef[2][tu], std::min<mfxU16>(dpar.caps.MaxNum_Reference1, numRefFrame)));

            auto SetDefaultNRef =
                [](const mfxU16(*extRef)[8], mfxU16 defaultRef, mfxU16(*NumRefActive)[8])
            {
                bool bExternal = false;
                bool bDone = false;

                bDone |= !NumRefActive;
                bDone |= !bDone && !extRef && std::fill_n(*NumRefActive, 8, defaultRef);
                bDone |= !bDone && std::transform(
                    *extRef
                    , std::end(*extRef)
                    , *NumRefActive
                    , [&](mfxU16 ext)
                    {
                        bExternal |= SetIf(defaultRef, !!ext, ext);
                        return defaultRef;
                    });

                return bExternal;
            };

            const mfxU16(*extRefP)[8]   = nullptr;
            const mfxU16(*extRefBL0)[8] = nullptr;
            const mfxU16(*extRefBL1)[8] = nullptr;

            if (const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(dpar.mvp))
            {
                extRefP   = &pCO3->NumRefActiveP;
                extRefBL0 = &pCO3->NumRefActiveBL0;
                extRefBL1 = &pCO3->NumRefActiveBL1;
            }

            bExternal |= SetDefaultNRef(extRefP, defaultP, pP);
            bExternal |= SetDefaultNRef(extRefBL0, defaultBL0, pBL0);
            bExternal |= SetDefaultNRef(extRefBL1, defaultBL1, pBL1);

            return bExternal;
        });

        defaults.CheckSlices.Push([](
            Base::Defaults::TCheckAndFix::TExt prev
            , const Defaults::Param& dpar
            , mfxVideoParam& par)
        {
            auto sts = prev(dpar, par);
            MFX_CHECK_STS(sts);

            mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);
            bool bCheckNMB = pCO2 && pCO2->NumMbPerSlice;

            if (bCheckNMB)
            {
                // It is not supported when LCU number in slice is not row aligned.
                mfxU16 W = dpar.base.GetCodedPicWidth(dpar);
                mfxU16 LCUSize = dpar.base.GetLCUSize(dpar);
                mfxU32 nLCUsInWidth = mfx::CeilDiv(W, LCUSize);
                MFX_CHECK(!(pCO2->NumMbPerSlice % nLCUsInWidth), MFX_ERR_UNSUPPORTED);
            }

            return MFX_ERR_NONE;
        });

        defaults.GetSPS.Push([](
            Defaults::TGetSPS::TExt prev
            , const Defaults::Param& defPar
            , const Base::VPS& vps
            , Base::SPS& sps)
        {
            auto sts = prev(defPar, vps, sps);

            if (sps.temporal_mvp_enabled_flag)
            {
                sps.temporal_mvp_enabled_flag = (defPar.mvp.mfx.TargetUsage < 6);
            }

            return sts;
        });

        defaults.GetGopRefDist.Push([](
            Base::Defaults::TChain<mfxU16>::TExt
            , const Defaults::Param& par)
        {
            if (par.mvp.mfx.GopRefDist)
            {
                return par.mvp.mfx.GopRefDist;
            }
            auto GopPicSize = par.base.GetGopPicSize(par);
            const mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par.mvp);
            bool bNoB =
                (pCO2 && pCO2->IntRefType)
                || par.base.GetNumTemporalLayers(par) > 1
                || par.caps.SliceIPOnly
                || GopPicSize < 3
                || par.mvp.mfx.NumRefFrame == 1;

            if (bNoB)
            {
                return mfxU16(1);
            }

            return std::min<mfxU16>(GopPicSize - 1, 8);
        });

        defaults.GetTileSlices.Push([](
            Base::Defaults::TGetTileSlices::TExt prev
            , const Defaults::Param& dpar
            , std::vector<SliceInfo>& slices
            , mfxU32 SliceStructure
            , mfxU32 nCol
            , mfxU32 nRow
            , mfxU32 nSlice)
        {
            const mfxExtCodingOption2* pCO2 = ExtBuffer::Get(dpar.mvp);
            if (SliceStructure != ROWSLICE || (pCO2 && pCO2->NumMbPerSlice != 0))
            {
                prev(dpar, slices, SliceStructure, nCol, nRow, nSlice);
            }
            else
            {
                nSlice = std::max<mfxU32>(nSlice * (SliceStructure != 0), 1);
                nSlice = std::min<mfxU32>(nSlice, nRow);
                mfxU32 nLCU          = nCol * nRow;
                mfxU32 segAddr       = 0;
                mfxU32 nSlicePrev    = (mfxU32)slices.size();
                mfxU32 nRowsPerSlice = mfx::CeilDiv(nRow, nSlice);
                mfxU32 nSliceIdx     = nSlicePrev;

                if (nSlicePrev)
                {
                    segAddr = slices.back().SegmentAddress + slices.back().NumLCU;
                }
                slices.resize(nSlicePrev + nSlice);

                SliceInfo zeroSI = {};
                auto      slBegin = std::next(slices.begin(), nSlicePrev);
                auto      slEnd = slices.end();

                std::fill(slBegin, slEnd, zeroSI);

                std::for_each(
                    slBegin
                    , slEnd
                    , [&](SliceInfo& si)
                {
                    si.NumLCU += nRowsPerSlice * nCol;
                    si.SegmentAddress = segAddr;
                    segAddr += si.NumLCU;
                    nSliceIdx ++;
                    if (nSliceIdx == (nRow % nSlice)) nRowsPerSlice -= 1;
                });

                slices.back().NumLCU = slBegin->SegmentAddress + nLCU - slices.back().SegmentAddress;
            }
            return (mfxU16)nSlice;
        });

        bSet = true;

        return MFX_ERR_NONE;
    });
}

void Caps::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_HardcodeCaps
        , [this](const mfxVideoParam&, mfxVideoParam& par, StorageRW& strg) -> mfxStatus
    {
        auto& caps = HEVCEHW::Base::Glob::EncodeCaps::Get(strg);
        caps.SliceIPOnly = (par.mfx.CodecProfile == MFX_PROFILE_HEVC_SCC);
        caps.msdk.PSliceSupport = true;

        SetSpecificCaps(caps);

        return MFX_ERR_NONE;
    });
}

void Caps::GetVideoParam(const FeatureBlocks& /*blocks*/, TPushGVP Push)
{
    Push(BLK_FixParam
        , [](mfxVideoParam& par, StorageR& /*global*/) -> mfxStatus
    {
        par.mfx.LowPower = MFX_CODINGOPTION_ON;

        return MFX_ERR_NONE;
    });
}

void Caps::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [](mfxVideoParam& par, StorageW&, StorageRW&)
    {
        SetDefault(par.mfx.LowPower, MFX_CODINGOPTION_ON);
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
