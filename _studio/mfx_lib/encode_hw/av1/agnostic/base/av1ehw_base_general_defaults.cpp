// Copyright (c) 2019-2023 Intel Corporation
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
#include "mfx_enc_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base_general.h"
#include "av1ehw_base_data.h"
#include "av1ehw_base_constraints.h"
#include <numeric>
#include <set>

namespace AV1EHW
{

namespace Base
{

class GetDefault
{
private:
    inline static void TuneCDEFLowQP(uint32_t* strength, int32_t qp)
    {
        if (!(qp < 90))
            assert(false && "Only called if qp < 90");

        strength[0] = 5;
        strength[1] = 41;
        strength[3] = 6;
        strength[5] = 16;
    }

    inline static void TuneCDEFHighQP(
        CdefParams& cdef
        , uint32_t* strength
        , int32_t qp)
    {
        if (!(qp > 140))
            assert(false && "Only called if qp > 140");

        cdef.cdef_bits = 2;
        strength[1] = 63;
        if (qp > 210)
        {
            cdef.cdef_bits = 1;
            strength[0] = 0;
        }
    }

    inline static void TuneCDEFMediumQP(
        const FH& bs_fh
        , CdefParams& cdef
        , uint32_t* strength
        , int32_t qp)
    {
        if (!(qp > 130 && qp <= 140))
            assert(false && "Only called if qp > 130 && qp <= 140");

        cdef.cdef_bits = 2;
        strength[1] = 63;

        if (bs_fh.FrameWidth < 1600 && bs_fh.FrameHeight < 1600)
            strength[3] = 1;
        else
            strength[3] = 32;
    }

public:
    static mfxU16 CodedPicAlignment(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        (void)par;
        return mfxU16(8);
    }

    static mfxU16 CodedPicWidth(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        const auto&  fi          = par.mvp.mfx.FrameInfo;
        const bool   bCropsValid = fi.CropW > 0 && (fi.CropW + fi.CropX <= fi.Width);
        const mfxU16 W           = mfxU16(bCropsValid * (fi.CropW + fi.CropX) + !bCropsValid * fi.Width);

        return mfx::align2_value(W, par.base.GetCodedPicAlignment(par));
    }

    static mfxU16 CodedPicHeight(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        const auto&  fi          = par.mvp.mfx.FrameInfo;
        const bool   bCropsValid = fi.CropH > 0 && (fi.CropH + fi.CropY <= fi.Height);
        const mfxU16 H           = mfxU16(bCropsValid * (fi.CropH + fi.CropY) + !bCropsValid * fi.Height);

        return mfx::align2_value(H, par.base.GetCodedPicAlignment(par));
    }

    static mfxU16 GopPicSize(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        if (par.mvp.mfx.GopPicSize)
        {
            return par.mvp.mfx.GopPicSize;
        }

        return mfxU16(DEFAULT_GOP_PIC_SIZE);
    }

    static mfxU16 GopRefDist(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        if (par.mvp.mfx.GopRefDist)
        {
            return par.mvp.mfx.GopRefDist;
        }

        return std::min<mfxU16>(DEFAULT_GOP_REF_DIST, par.base.GetGopPicSize(par) - 1);
    }

    static mfxU16 TargetUsage(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        if (par.mvp.mfx.TargetUsage)
        {
            return par.mvp.mfx.TargetUsage;
        }

        return DEFAULT_TARGET_USAGE;
    }

    static mfxU16 NumBPyramidLayers(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        mfxU16 refB = (par.base.GetGopRefDist(par) - 1) / 2;
        mfxU16 x    = refB;

        while (x > 2)
        {
            x = (x - 1) / 2;
            refB -= x;
        }

        return refB + 1;
    }

    static mfxU16 NumRefBPyramid(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        mfxU16 NumRefActiveP[8], NumRefActiveBL0[8], NumRefActiveBL1[8];
        mfxU16 NumLayers = par.base.GetNumBPyramidLayers(par);
        mfxU16 NumRefFrame = par.base.GetMinRefForBPyramid(par);
        bool bExternalNRef = par.base.GetNumRefActive(par, &NumRefActiveP, &NumRefActiveBL0, &NumRefActiveBL1);

        SetIf(NumRefFrame, bExternalNRef, [&]() -> mfxU16
        {
            auto maxBL0idx = std::max_element(NumRefActiveBL0, NumRefActiveBL0 + NumLayers) - NumRefActiveBL0;
            auto maxBL1idx = std::max_element(NumRefActiveBL1, NumRefActiveBL1 + NumLayers) - NumRefActiveBL1;
            mfxU16 maxBL0 = mfxU16((NumRefActiveBL0[maxBL0idx] + maxBL0idx + 1) * (maxBL0idx < NumLayers));
            mfxU16 maxBL1 = mfxU16((NumRefActiveBL1[maxBL1idx] + maxBL1idx + 1) * (maxBL1idx < NumLayers));
            return std::max<mfxU16>({ NumRefFrame, NumRefActiveP[0], maxBL0, maxBL1 });
        });

        // Need to have one more DPB buffer slot to enable 3 P refs or 2 BL0 refs in AV1
        if (NumRefActiveP[0] == 3  || *std::max_element(NumRefActiveBL0, NumRefActiveBL0 + NumLayers) == 2)
        {
            NumRefFrame++;
        }

        return NumRefFrame;
    }

    static mfxU16 NumRefNoPyramid(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        mfxU16 NumRefActiveP[8], NumRefActiveBL0[8], NumRefActiveBL1[8];
        par.base.GetNumRefActive(par, &NumRefActiveP, &NumRefActiveBL0, &NumRefActiveBL1);

        const mfxU16 RefActiveP   = *std::max_element(NumRefActiveP,   NumRefActiveP   + mfx::size(NumRefActiveP));
        const mfxU16 RefActiveBL0 = *std::max_element(NumRefActiveBL0, NumRefActiveBL0 + mfx::size(NumRefActiveBL0));
        const mfxU16 RefActiveBL1 = *std::max_element(NumRefActiveBL1, NumRefActiveBL1 + mfx::size(NumRefActiveBL1));

        return mfxU16((par.base.GetGopRefDist(par) > 1) ? std::max<mfxU16>(RefActiveP, RefActiveBL0 + RefActiveBL1) : RefActiveP);
    }

    static mfxU16 NumRefFrames(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        if (par.mvp.mfx.NumRefFrame)
        {
            return par.mvp.mfx.NumRefFrame;
        }

        mfxU16 numRef = 0;
        if ((par.base.GetBRefType(par) == MFX_B_REF_PYRAMID))
        {
            numRef = par.base.GetNumRefBPyramid(par);
        }
        else
        {
            numRef = par.base.GetNumRefNoPyramid(par);
        }

        mfxU16 numTL = par.base.GetNumTemporalLayers(par);
        numRef       = std::min<mfxU16>(std::max<mfxU16>(numRef, numTL - 1), NUM_REF_FRAMES);

        return numRef;
    }

    static mfxU16 MinRefForBPyramid(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        return par.base.GetNumBPyramidLayers(par) + 1;
    }

    static mfxU16 MinRefForBNoPyramid(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& /*par*/)
    {
        return 2;
    }

    static bool NumRefActive(
        Defaults::TGetNumRefActive::TExt
        , const Defaults::Param& par
        , mfxU16(*pP)[8]
        , mfxU16(*pBL0)[8]
        , mfxU16(*pBL1)[8])
    {
        bool bExternal = false;
        const mfxU16 numRefByTU[3][7] =
        {
            { 3, 3, 3, 2, 2, 1, 1 },
            { 1, 1, 1, 1, 1, 1, 1 },
            { 1, 1, 1, 1, 1, 1, 1 }
        };

        mfxU16 maxP = 0, maxBL0 = 0, maxBL1 = 0;
        std::tie(maxP, maxBL0, maxBL1) = par.base.GetMaxNumRef(par);

        // Get default active ref frame number
        mfxU16 tu = par.mvp.mfx.TargetUsage;
        CheckRangeOrSetDefault<mfxU16>(tu, 1, 7, DEFAULT_TARGET_USAGE);

        mfxU16 defaultP = 0, defaultBL0 = 0, defaultBL1 = 0;
        std::tie(defaultP, defaultBL0, defaultBL1) = std::make_tuple(
            std::min<mfxU16>(numRefByTU[0][tu - 1], maxP)
            , std::min<mfxU16>(numRefByTU[1][tu - 1], maxBL0)
            , std::min<mfxU16>(numRefByTU[2][tu - 1], maxBL1));

        auto SetDefaultNRef =
            [](const mfxU16(*extRef)[8], mfxU16 defaultRef, mfxU16(*NumRefActive)[8])
        {
            bool bExternal = false;
            if (!NumRefActive)
            {
                return bExternal;
            }
            
            if (!extRef)
            {
                std::fill_n(*NumRefActive, 8, defaultRef);
                return bExternal;
            }

            std::transform(
                *extRef
                , std::end(*extRef)
                , *NumRefActive
                , [&](mfxU16 ext)
                {
                    bExternal |= !!ext;
                    defaultRef *= !bExternal;
                    SetDefault(defaultRef, ext);
                    return defaultRef;
                });

            return bExternal;
        };

        const mfxU16(*extRefP)[8] = nullptr;
        const mfxU16(*extRefBL0)[8] = nullptr;
        const mfxU16(*extRefBL1)[8] = nullptr;

        if (const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par.mvp))
        {
            extRefP = &pCO3->NumRefActiveP;
            extRefBL0 = &pCO3->NumRefActiveBL0;
            extRefBL1 = &pCO3->NumRefActiveBL1;
        }

        bExternal |= SetDefaultNRef(extRefP,   defaultP,   pP);
        bExternal |= SetDefaultNRef(extRefBL0, defaultBL0, pBL0);
        bExternal |= SetDefaultNRef(extRefBL1, defaultBL1, pBL1);

        return bExternal;
    }

    static mfxU16 BRefType(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        const mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par.mvp);
        if (pCO2 && pCO2->BRefType)
            return pCO2->BRefType;

        // In the future it might be turned on by default
        const mfxU16 BPyrCand[2] = { mfxU16(MFX_B_REF_OFF), mfxU16(MFX_B_REF_PYRAMID) };
        bool bValid =
            par.base.GetGopRefDist(par) > 3
            && (par.mvp.mfx.NumRefFrame == 0
                || par.base.GetMinRefForBPyramid(par) <= par.mvp.mfx.NumRefFrame);

        return BPyrCand[bValid];
    }

    static mfxU16 PRefType(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param&)
    {
        return MFX_P_REF_SIMPLE;
    }

    static std::tuple<mfxU32, mfxU32> FrameRate(
        Defaults::TChain<std::tuple<mfxU32, mfxU32>>::TExt
        , const Defaults::Param& par)
    {
        const auto& fi = par.mvp.mfx.FrameInfo;
        if (fi.FrameRateExtN && fi.FrameRateExtD)
        {
            return std::make_tuple(fi.FrameRateExtN, fi.FrameRateExtD);
        }

        mfxU32 frN = 30, frD = 1;
        mfxF64 frMax = 30.0;
        if (par.mvp.mfx.CodecLevel)
        {
            const auto res = GetRealResolution(par.mvp);
            frMax = GetMaxFrameRateByLevel(par.mvp.mfx.CodecLevel, std::get<0>(res), std::get<1>(res));
        }

        const bool bFrByLevel = frN > (frMax * frD);
        frN = mfxU32(frN * !bFrByLevel + (frMax * 1001) * bFrByLevel);
        frD = mfxU32(frD * !bFrByLevel + 1001 * bFrByLevel);

        return std::make_tuple(frN, frD);
    }

    static mfxU16 BitDepthLuma(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        if (par.mvp.mfx.FrameInfo.BitDepthLuma)
        {
            return par.mvp.mfx.FrameInfo.BitDepthLuma;
        }

        const auto FourCC    = par.mvp.mfx.FrameInfo.FourCC;
        const bool b4CCMax10 =
            FourCC == MFX_FOURCC_P010
            ;

        return mfxU16(8 + 2 * b4CCMax10);
    }

    static mfxU16 TargetBitDepthLuma(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par.mvp);

        if (pCO3 && pCO3->TargetBitDepthLuma)
        {
            return pCO3->TargetBitDepthLuma;
        }

        return par.base.GetBitDepthLuma(par);
    }

    static mfxU16 TargetChromaFormatPlus1(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par.mvp);

        if (pCO3 && pCO3->TargetChromaFormatPlus1)
        {
            return pCO3->TargetChromaFormatPlus1;
        }

        //For RGB4 use illogical default 420 for backward compatibility
        if (par.mvp.mfx.FrameInfo.FourCC == MFX_FOURCC_RGB4
            || par.mvp.mfx.FrameInfo.FourCC == MFX_FOURCC_BGR4)
        {
            return mfxU16(MFX_CHROMAFORMAT_YUV420 + 1);
        }

        return par.mvp.mfx.FrameInfo.ChromaFormat + 1;
    }

    static mfxU32 TargetKbps(
        Defaults::TChain<mfxU32>::TExt
        , const Defaults::Param& defPar)
    {
        const auto& mfx = defPar.mvp.mfx;

        // TargetKbps/ICQFactor are placed in the same union in sturct mfxInfoMFX,
        // While in ICQ, union of TargetKbps/ICQFactor is only for ICQFactor and TargetKbps will be calculated later.
        if (mfx.TargetKbps && mfx.RateControlMethod != MFX_RATECONTROL_ICQ)
        {
            return mfx.TargetKbps * std::max<const mfxU32>(1, mfx.BRCParamMultiplier);
        }

        mfxU32 frN = 0, frD = 0, maxKbps = 0xffffffff;

        SetIf(maxKbps, !!mfx.CodecLevel, [&]() { return GetMaxKbpsByLevel(mfx.CodecLevel, mfx.CodecProfile); });

        mfxU32 rawBits           = General::GetRawBytes(defPar) << 3;
        std::tie(frN, frD)       = defPar.base.GetFrameRate(defPar);

        // Set a large default value for TargetKbps when ICQ enalbed
        // In the future it might be tuned
        if (mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
        {
            return std::min<mfxU32>(maxKbps, static_cast<mfxU32>(static_cast<mfxU64>(rawBits) * frN / frD / 50000));
        }
        return std::min<mfxU32>(maxKbps, static_cast<mfxU32>(static_cast<mfxU64>(rawBits) * frN / frD / 150000));
    }

    static mfxU32 MaxKbps(
        Defaults::TChain<mfxU32>::TExt
        , const Defaults::Param& par)
    {
        const auto& mfx = par.mvp.mfx;

        if (mfx.MaxKbps && mfx.RateControlMethod != MFX_RATECONTROL_CQP && mfx.RateControlMethod != MFX_RATECONTROL_CBR)
        {
            return mfx.MaxKbps * std::max<const mfxU32>(1, mfx.BRCParamMultiplier);
        }

        return par.base.GetTargetKbps(par);
    }

    static mfxU32 BufferSizeInKB(
        Defaults::TChain<mfxU32>::TExt
        , const Defaults::Param& defPar)
    {
        const auto& mfx = defPar.mvp.mfx;
        if (mfx.BufferSizeInKB)
        {
            return mfx.BufferSizeInKB * std::max<const mfxU32>(1, mfx.BRCParamMultiplier);
        }

        bool bUseMaxKbps =
            mfx.RateControlMethod == MFX_RATECONTROL_CBR
            || mfx.RateControlMethod == MFX_RATECONTROL_VBR;
        mfxU32 minSize         = bUseMaxKbps * InitialDelayInKB(mfx);
        mfxU32 defaultSize     = 0;
        auto   GetFromMaxKbps  = [&]() { return defPar.base.GetMaxKbps(defPar) / 4; };
        auto   GetFromRawBytes = [&]()
        {
            const mfxU32 numCacheFrames = defPar.base.GetTemporalUnitCacheSize(defPar);
            return General::GetRawBytes(defPar) / 1000 * numCacheFrames;
        };

        SetIf(defaultSize, bUseMaxKbps, GetFromMaxKbps);
        SetDefault(defaultSize, GetFromRawBytes);

        return std::max<mfxU32>(minSize, defaultSize);
    }

    static std::tuple<mfxU16, mfxU16, mfxU16> MaxNumRef(
        Defaults::TChain<std::tuple<mfxU16, mfxU16, mfxU16>>::TExt
        , const Defaults::Param& par)
    {
        return std::make_tuple(
            par.caps.MaxNum_ReferenceL0_P
            , par.caps.MaxNum_ReferenceL0_B
            , par.caps.MaxNum_ReferenceL1_B);
    }

    static mfxU16 RateControlMethod(
        Defaults::TChain< mfxU16>::TExt
        , const Defaults::Param& par)
    {
        if (par.mvp.mfx.RateControlMethod)
        {
            return par.mvp.mfx.RateControlMethod;
        }
        return mfxU16(MFX_RATECONTROL_CQP);
    }

    static mfxU8 MinQPMFX(
        Defaults::TChain< mfxU16>::TExt
        , const Defaults::Param& /*par*/)
    {
        return mfxU8(AV1_MIN_Q_INDEX);
    }

    static mfxU8 MaxQPMFX(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& /*par*/)
    {
        return mfxU8(AV1_MAX_Q_INDEX);
    }

    static std::tuple<mfxU16, mfxU16, mfxU16> QPMFX(
        Defaults::TChain<std::tuple<mfxU16, mfxU16, mfxU16>>::TExt
        , const Defaults::Param& par)
    {
        bool bCQP = (par.base.GetRateControlMethod(par) == MFX_RATECONTROL_CQP);

        mfxU16 QPI = bCQP * par.mvp.mfx.QPI;
        mfxU16 QPP = bCQP * par.mvp.mfx.QPP;
        mfxU16 QPB = bCQP * par.mvp.mfx.QPB;

        if (bCQP)
        {
            const bool isLossless = (QPI == 0) && (QPP == 0) && (QPB == 0);
            if (!isLossless)
            {
                if (QPI)
                {
                    SetDefault(QPP, QPI);
                    SetDefault(QPB, QPP);
                }
                else if (QPP)
                {
                    SetDefault(QPI, QPP);
                    SetDefault(QPB, QPP);
                }
                else if (QPB)
                {
                    SetDefault(QPP, QPB);
                    SetDefault(QPI, QPP);
                }
            }
        }
        else
        {
            bool bValid = ((QPI) && (QPP) && (QPB));

            if (bValid)
                return std::make_tuple(QPI, QPP, QPB);

            const auto minQP = par.base.GetMinQPMFX(par);
            const auto maxQP = par.base.GetMaxQPMFX(par);
            SetDefault(QPI, std::max<mfxU16>(minQP, (maxQP + 1) / 2));
            SetDefault(QPP, std::min<mfxU16>(QPI + 5, maxQP));
            SetDefault(QPB, std::min<mfxU16>(QPP + 5, maxQP));
        }

        return std::make_tuple(QPI, QPP, QPB);
    }

    static void QPOffset(
        Defaults::TGetQPOffset::TExt
        , const Defaults::Param& par
        , mfxU16& EnableQPOffset
        , mfxI16(&QPOffset)[8])
    {
        if (EnableQPOffset == MFX_CODINGOPTION_UNKNOWN)
        {
            const bool   bCQP       = par.base.GetRateControlMethod(par) == MFX_RATECONTROL_CQP;
            const mfxU16 GopRefDist = par.base.GetGopRefDist(par);
            const bool   bBPyr      = GopRefDist > 1 && par.base.GetBRefType(par) == MFX_B_REF_PYRAMID;

            EnableQPOffset = Bool2CO(bCQP && bBPyr);
            if (IsOn(EnableQPOffset))
            {
                const mfxI16 QPX = std::get<2>(par.base.GetQPMFX(par));
                const mfxI16 minQPOffset = mfxI16(AV1_MIN_Q_INDEX - QPX);
                const mfxI16 maxQPOffset = mfxI16(AV1_MAX_Q_INDEX - QPX);

                mfxI16 i = 0;
                std::generate_n(QPOffset, 8
                    , [&]() { return mfx::clamp<mfxI16>(DEFAULT_BPYR_QP_OFFSETS[i++], minQPOffset, maxQPOffset); });
            }
        }
    }

    static mfxU16 Profile(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        bool bPassThrough = !!par.mvp.mfx.CodecProfile;

        mfxU16 bd = 0, cf = 0;
        SetIf(bd, !bPassThrough, [&]() { return par.base.GetTargetBitDepthLuma(par); });
        SetIf(cf, !bPassThrough, [&]() { return mfxU16(par.base.GetTargetChromaFormatPlus1(par) - 1); });

        bool bRext444 = !bPassThrough && (cf == MFX_CHROMAFORMAT_YUV444);
        bool bMain10  = !bPassThrough && !bRext444 && bd == 10;
        bool bMain    = !bPassThrough && !bRext444 && !bMain10;

        return
            bPassThrough * par.mvp.mfx.CodecProfile
            + bRext444 * MFX_PROFILE_AV1_HIGH
            + bMain10 * MFX_PROFILE_AV1_MAIN
            + bMain * MFX_PROFILE_AV1_MAIN;
    }

    static mfxU16 AsyncDepth(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        if (par.mvp.AsyncDepth)
        {
            return par.mvp.AsyncDepth;
        }

        return 5;
    }

    static mfxU8 NumReorderFrames(
        Defaults::TChain<mfxU8>::TExt
        , const Defaults::Param& par)
    {
        const mfxU8 GopRefDist = mfxU8(par.base.GetGopRefDist(par));
        if (GopRefDist < 2)
            return 0;

        const bool bBPyramid  = (par.base.GetBRefType(par) == MFX_B_REF_PYRAMID);
        if (!bBPyramid)
            return 1;

        mfxU8 BFrameRate = GopRefDist - 1;
        mfxU8 n          = 0;
        while (BFrameRate)
        {
            BFrameRate >>= 1;
            n++;
        }

        return n;
    }

    static bool NonStdReordering(
        Defaults::TChain<mfxU8>::TExt
        , const Defaults::Param& par)
    {
        return
            par.mvp.mfx.EncodedOrder
            && par.mvp.mfx.NumRefFrame > 2;
    }

    static mfxU32 TemporalUnitCacheSize(
        Defaults::TChain<mfxU32>::TExt
        , const Defaults::Param& par)
    {
        mfxU32 numCacheFrames = 1;
        if (HaveRABFrames(par.mvp))
        {
            numCacheFrames = (par.base.GetBRefType(par) != MFX_B_REF_PYRAMID) ? mfxU32(2)
            : mfxU32(par.base.GetNumBPyramidLayers(par)) + 1;
        }

        return numCacheFrames;
    }

    static mfxU16 FrameType(
        Defaults::TGetFrameType::TExt
        , const Defaults::Param& par
        , mfxU32 displayOrder
        , mfxU32 lastKeyFrame
        , mfxGopHints GopHints)
    {
        mfxU32 gopPicSize = par.mvp.mfx.GopPicSize;
        mfxU32 gopRefDist = GopHints.MiniGopSize ? std::min<mfxU32>(GopHints.MiniGopSize, par.mvp.mfx.GopRefDist) : par.mvp.mfx.GopRefDist;
        mfxU32 idrPicDist = gopPicSize * (par.mvp.mfx.IdrInterval);

        //infinite GOP
        SetIf(idrPicDist, gopPicSize == 0xffff, 0xffffffff);
        SetIf(gopPicSize, gopPicSize == 0xffff, 0xffffffff);

        mfxU32 fo = displayOrder - lastKeyFrame;
        bool bIdr = (fo % gopPicSize == 0);
        bool bPRef =
            !bIdr
            && (   (fo % gopPicSize % gopRefDist == 0)
                || ((fo + 1) % gopPicSize == 0));
        bool bB = !(bPRef || bIdr);
        mfxU16 ft = GopHints.FrameType ? GopHints.FrameType :
            bIdr * (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR)
            + bPRef * (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF)
            + bB * (MFX_FRAMETYPE_B);

        return ft;
    }

    static mfxU16 MBBRC(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        const mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par.mvp);
        bool bPassThrough               = pCO2 && pCO2->MBBRC;
        bool bON                        = bPassThrough && IsOn(pCO2->MBBRC);
        bool bOFF                       = bPassThrough && IsOff(pCO2->MBBRC);

        bOFF |= !bPassThrough
            && (   par.base.GetRateControlMethod(par) == MFX_RATECONTROL_CQP
                || ::IsSWBRCMode(par.mvp)
                || IsOn(par.mvp.mfx.LowPower));

        return mfxU16(
            bON * MFX_CODINGOPTION_ON
            + bOFF * MFX_CODINGOPTION_OFF);
    }


    class TemporalLayers
    {
    public:
        TemporalLayers() = delete;
        TemporalLayers(const mfxExtTemporalLayers& tl)
        {
            SetTL(tl);
        }

        ~TemporalLayers() {}

        void SetTL(mfxExtTemporalLayers const & tl)
        {
            m_numTL = 0;
            memset(&m_TL, 0, sizeof(m_TL));
            m_TL[0].Scale = 1;

            for (mfxU8 i = 0; i < tl.NumLayers; i++)
            {
                if (tl.Layers[i].FrameRateScale)
                {
                    m_TL[m_numTL].TId = i;
                    m_TL[m_numTL].Scale = (mfxU8)tl.Layers[i].FrameRateScale;
                    m_numTL++;
                }
            }

            m_numTL = std::max<mfxU8>(m_numTL, 1);
        }

        mfxU8 GetTId(mfxU32 frameOrder) const
        {
            mfxU16 i;

            if (m_numTL < 1 || m_numTL > 8)
                return 0;

            for (i = 0; i < m_numTL && (frameOrder % (m_TL[m_numTL - 1].Scale / m_TL[i].Scale)); i++);

            return (i < m_numTL) ? m_TL[i].TId : 0;
        }

        mfxU8 HighestTId() const
        {
            mfxU8 htid = m_TL[m_numTL - 1].TId;
            return mfxU8(htid + !htid * -1);
        }

    private:
        mfxU8 m_numTL;

        struct
        {
            mfxU8 TId = 0;
            mfxU8 Scale = 0;
        }m_TL[8];
    };

    static mfxU8 GetTId(
        const Defaults::Param& par
        , mfxU32 fo)
    {
        const mfxExtTemporalLayers* pTemporalLayers = ExtBuffer::Get(par.mvp);
        if (!pTemporalLayers)
            return 0;

        return TemporalLayers(*pTemporalLayers).GetTId(fo);
    }

    static mfxU16 NumTemporalLayers(
        Defaults::TChain<mfxU16>::TExt
        , const Defaults::Param& par)
    {
        const mfxExtTemporalLayers* pTemporalLayers = ExtBuffer::Get(par.mvp);
        if (!pTemporalLayers)
        {
            return 1;
        }

        return CountTL(*pTemporalLayers);
    }

    static mfxU8 GetHighestTId(
        const Defaults::Param& par)
    {
        const mfxExtTemporalLayers* pTemporalLayers = ExtBuffer::Get(par.mvp);
        if (!pTemporalLayers)
            return mfxU8(-1);

        return TemporalLayers(*pTemporalLayers).HighestTId();
    }

    static mfxStatus PreReorderInfo(
        Defaults::TGetPreReorderInfo::TExt
        , const Defaults::Param& par
        , FrameBaseInfo& fi
        , const mfxFrameSurface1* pSurfIn
        , const mfxEncodeCtrl*    pCtrl
        , mfxU32 prevKeyFrameOrder
        , mfxU32 frameOrder
        , mfxGopHints GopHints)
    {
        mfxU16 ftype = 0;
        auto SetFrameTypeFromGOP   = [&]() { return Res2Bool(ftype, par.base.GetFrameType(par, frameOrder, prevKeyFrameOrder, GopHints)); };
        auto SetFrameTypeFromCTRL  = [&]() { return Res2Bool(ftype, pCtrl->FrameType); };
        auto ForceIdr              = [&]() { return Res2Bool(ftype, mfxU16(MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR)); };
        auto SetFrameOrderFromSurf = [&]() { frameOrder = pSurfIn->Data.FrameOrder;  return true; };

        bool bFrameInfoValid =
            (par.mvp.mfx.EncodedOrder && SetFrameOrderFromSurf() && SetFrameTypeFromCTRL() )
            || (pCtrl && IsI(pCtrl->FrameType) && ForceIdr())
            || SetFrameTypeFromGOP();
        MFX_CHECK(bFrameInfoValid, MFX_ERR_UNDEFINED_BEHAVIOR);

        fi.DisplayOrderInGOP = !IsI(ftype) * (frameOrder - prevKeyFrameOrder);
        fi.FrameType         = ftype;
        fi.TemporalID        = !IsI(ftype) * GetTId(par, fi.DisplayOrderInGOP);

        bool bForceNonRef = IsRef(ftype) && fi.TemporalID == GetHighestTId(par);
        fi.FrameType &= ~mfxU16(bForceNonRef * MFX_FRAMETYPE_REF);

        if (IsP(ftype))
        {
            const mfxExtCodingOption3& CO3 = ExtBuffer::Get(par.mvp);

            fi.isLDB        = IsOn(CO3.GPB);
        }

        return MFX_ERR_NONE;
    }

    static void LoopFilterLevels(
        Defaults::TGetLoopFilterLevels::TExt
        , const Defaults::Param& par
        , FH& bs_fh)
    {
        int32_t levelY = 0xff;
        int32_t levelUV = 0xff;
        //for BRC cases place holder values are set to have correct PPS-header,
        //because when [0] and [1] are zeroes header is of different size
        if (par.mvp.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        {
            const int32_t qp = bs_fh.quantization_params.base_q_idx;
            levelY = LoopFilterLevelsLuma[qp];
            levelUV = LoopFilterLevelsChroma[qp];
        }

        auto& lp = bs_fh.loop_filter_params;

        lp.loop_filter_level[0] = levelY;
        lp.loop_filter_level[1] = levelY;
        lp.loop_filter_level[2] = levelUV;
        lp.loop_filter_level[3] = levelUV;
    }

    static void CDEF(
        Defaults::TGetCDEF::TExt
        , FH& bs_fh)
    {
        const int32_t qp = bs_fh.quantization_params.base_q_idx;

        uint32_t YStrengths[CDEF_MAX_STRENGTHS];
        YStrengths[0] = 36;
        YStrengths[1] = 50;
        YStrengths[2] = 0;
        YStrengths[3] = 24;
        YStrengths[4] = 8;
        YStrengths[5] = 17;
        YStrengths[6] = 4;
        YStrengths[7] = 9;

        auto& cdef = bs_fh.cdef_params;
        cdef.cdef_bits = 3;

        if (qp < 90)
            TuneCDEFLowQP(YStrengths, qp);
        else if (qp > 140)
            TuneCDEFHighQP(cdef, YStrengths, qp);
        else if (qp > 130)
            TuneCDEFMediumQP(bs_fh, cdef, YStrengths, qp);

        if (bs_fh.FrameWidth < 1600 && bs_fh.FrameHeight < 1600)
            YStrengths[3] = 5;

        for (int i = 0; i < CDEF_MAX_STRENGTHS; i++)
        {
            cdef.cdef_y_pri_strength[i] = YStrengths[i] / CDEF_STRENGTH_DIVISOR;
            cdef.cdef_y_sec_strength[i] = YStrengths[i] % CDEF_STRENGTH_DIVISOR;
            cdef.cdef_uv_pri_strength[i] = YStrengths[i] / CDEF_STRENGTH_DIVISOR;
            cdef.cdef_uv_sec_strength[i] = YStrengths[i] % CDEF_STRENGTH_DIVISOR;
        }

        cdef.cdef_damping = (qp >> 6) + 3;
    }

    static void Push(Defaults& df)
    {
#define PUSH_DEFAULT(X) df.Get##X.Push(X);

        PUSH_DEFAULT(CodedPicWidth);
        PUSH_DEFAULT(CodedPicHeight);
        PUSH_DEFAULT(CodedPicAlignment);
        PUSH_DEFAULT(GopPicSize);
        PUSH_DEFAULT(GopRefDist);
        PUSH_DEFAULT(TargetUsage);
        PUSH_DEFAULT(NumBPyramidLayers);
        PUSH_DEFAULT(NumRefFrames);
        PUSH_DEFAULT(NumRefBPyramid);
        PUSH_DEFAULT(NumRefNoPyramid);
        PUSH_DEFAULT(MinRefForBPyramid);
        PUSH_DEFAULT(MinRefForBNoPyramid);
        PUSH_DEFAULT(NumRefActive);
        PUSH_DEFAULT(BRefType);
        PUSH_DEFAULT(PRefType);
        PUSH_DEFAULT(FrameRate);
        PUSH_DEFAULT(BitDepthLuma);
        PUSH_DEFAULT(TargetBitDepthLuma);
        PUSH_DEFAULT(TargetChromaFormatPlus1);
        PUSH_DEFAULT(TargetKbps);
        PUSH_DEFAULT(MaxKbps);
        PUSH_DEFAULT(BufferSizeInKB);
        PUSH_DEFAULT(MaxNumRef);
        PUSH_DEFAULT(RateControlMethod);
        PUSH_DEFAULT(MinQPMFX);
        PUSH_DEFAULT(MaxQPMFX);
        PUSH_DEFAULT(QPMFX);
        PUSH_DEFAULT(QPOffset);
        PUSH_DEFAULT(Profile);
        PUSH_DEFAULT(AsyncDepth);
        PUSH_DEFAULT(FrameType);
        PUSH_DEFAULT(NumTemporalLayers);
        PUSH_DEFAULT(PreReorderInfo);
        PUSH_DEFAULT(NumReorderFrames);
        PUSH_DEFAULT(NonStdReordering);
        PUSH_DEFAULT(TemporalUnitCacheSize);
        PUSH_DEFAULT(LoopFilterLevels);
        PUSH_DEFAULT(CDEF);
        PUSH_DEFAULT(MBBRC);

#undef PUSH_DEFAULT
    }
};

class PreCheck
{
public:
    static mfxStatus CodecId(
        Defaults::TPreCheck::TExt
        , const mfxVideoParam& in)
    {
        MFX_CHECK(in.mfx.CodecId == MFX_CODEC_AV1, MFX_ERR_UNSUPPORTED);
        return MFX_ERR_NONE;
    }

    static mfxStatus ChromaFormat(
        Defaults::TPreCheck::TExt
        , const mfxVideoParam& in)
    {
        bool bInvalid = Check<mfxU16
            , MFX_CHROMAFORMAT_YUV420
            , MFX_CHROMAFORMAT_YUV444>
            (in.mfx.FrameInfo.ChromaFormat);

        MFX_CHECK(!bInvalid, MFX_ERR_UNSUPPORTED);

        return MFX_ERR_NONE;
    }

    static void Push(Defaults& df)
    {
#define PUSH_DEFAULT(X) df.PreCheck##X.Push(X);

        PUSH_DEFAULT(CodecId);
        PUSH_DEFAULT(ChromaFormat);

#undef PUSH_DEFAULT
    }
};

class CheckAndFix
{
public:
    static mfxStatus SurfSize(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& dpar
        , mfxVideoParam& par)
    {
        const mfxU16 MaxW = mfxU16(dpar.caps.MaxPicWidth);
        const mfxU16 MaxH = mfxU16(dpar.caps.MaxPicHeight);
        const mfxU16 MinW = MIN_FRAME_WIDTH;
        const mfxU16 MinH = MIN_FRAME_HEIGHT;

        auto&        W    = par.mfx.FrameInfo.Width;
        auto&        H    = par.mfx.FrameInfo.Height;

        MFX_CHECK(W, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(H, MFX_ERR_INVALID_VIDEO_PARAM);

        mfxU32 invalid = 0;
        invalid += CheckRangeOrSetDefault<mfxU16>(W, MinW, MaxW, 0);
        invalid += CheckRangeOrSetDefault<mfxU16>(H, MinH, MaxH, 0);

        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

        return MFX_ERR_NONE;
    }

    static mfxStatus Profile(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& dpar
        , mfxVideoParam& par)
    {
        bool bInvalid = CheckOrZero<mfxU16
            , 0
            , MFX_PROFILE_AV1_MAIN
            , MFX_PROFILE_AV1_HIGH>
            (par.mfx.CodecProfile);

        if (par.mfx.CodecProfile)
        {
            mfxU16 ChromaFormat = dpar.base.GetTargetChromaFormatPlus1(dpar) - 1;
            bInvalid |= (par.mfx.CodecProfile == MFX_PROFILE_AV1_MAIN && ChromaFormat != MFX_CHROMAFORMAT_YUV420)
                || (par.mfx.CodecProfile == MFX_PROFILE_AV1_HIGH && ChromaFormat != MFX_CHROMAFORMAT_YUV444);
        }

        MFX_CHECK(!bInvalid, MFX_ERR_UNSUPPORTED);
        return MFX_ERR_NONE;
    }

    static mfxStatus FourCC(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& dpar
        , mfxVideoParam& par)
    {
        mfxU32 invalid = 0;
        invalid += CheckOrZero<mfxU32
            , MFX_FOURCC_NV12
            , MFX_FOURCC_P010
            , MFX_FOURCC_RGB4
            , MFX_FOURCC_BGR4
            >
            (par.mfx.FrameInfo.FourCC);

        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

        // caps check
        const auto& fourCC = par.mfx.FrameInfo.FourCC;
        const auto& csf = dpar.caps.ChromaSupportFlags;
        const auto& bsf = dpar.caps.BitDepthSupportFlags;
        invalid += (fourCC == MFX_FOURCC_NV12
            && (!csf.fields.i420 || !bsf.fields.eight_bits));
        invalid += (fourCC == MFX_FOURCC_P010
            && (!csf.fields.i420 || !bsf.fields.ten_bits));
        invalid += ((fourCC == MFX_FOURCC_RGB4 || fourCC == MFX_FOURCC_BGR4)
            && (!csf.fields.RGB || !bsf.fields.eight_bits));

        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

        return MFX_ERR_NONE;
    }

    static mfxStatus InputFormatByFourCC(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& /*dpar*/
        , mfxVideoParam& par)
    {
        mfxU32 invalid = 0;
        static const std::map<mfxU32, std::array<mfxU16, 2>> FourCCPar=
        {
            {mfxU32(MFX_FOURCC_NV12),      {mfxU16(MFX_CHROMAFORMAT_YUV420), BITDEPTH_8}}
            , {mfxU32(MFX_FOURCC_P010),    {mfxU16(MFX_CHROMAFORMAT_YUV420), BITDEPTH_10}}
            , {mfxU32(MFX_FOURCC_RGB4),    {mfxU16(MFX_CHROMAFORMAT_YUV444), BITDEPTH_8}}
            , {mfxU32(MFX_FOURCC_BGR4),    {mfxU16(MFX_CHROMAFORMAT_YUV444), BITDEPTH_8}}
        };

        auto itFourCCPar = FourCCPar.find(par.mfx.FrameInfo.FourCC);
        MFX_CHECK(itFourCCPar != FourCCPar.end(), MFX_ERR_UNSUPPORTED);

        invalid += CheckOrZero(par.mfx.FrameInfo.ChromaFormat, itFourCCPar->second[0]);
        invalid += CheckOrZero(par.mfx.FrameInfo.BitDepthLuma, itFourCCPar->second[1], 0);
        invalid += CheckOrZero(par.mfx.FrameInfo.BitDepthChroma, itFourCCPar->second[1], 0);

        if (par.mfx.FrameInfo.BitDepthLuma != par.mfx.FrameInfo.BitDepthChroma)
        {
            par.mfx.FrameInfo.BitDepthLuma   = 0;
            par.mfx.FrameInfo.BitDepthChroma = 0;
            invalid += 1;
        }

        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

        return MFX_ERR_NONE;
    }

    static mfxStatus TargetChromaFormat(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& defPar
        , mfxVideoParam& par)
    {
        mfxU32 invalid = 0;
        mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);

        MFX_CHECK(pCO3, MFX_ERR_NONE);

        if (!pCO3->TargetChromaFormatPlus1)
        {
            pCO3->TargetChromaFormatPlus1 = defPar.base.GetTargetChromaFormatPlus1(defPar);
        }

        //check targetChromaFormat By FourCC and profile
        mfxU16 profile = defPar.base.GetProfile(defPar);
        mfxU32 fourCC = par.mfx.FrameInfo.FourCC;

        static const std::map <mfxU16, std::map<mfxU16, std::vector<mfxU32>>> compatible =
        {
            {mfxU16(MFX_CHROMAFORMAT_YUV420 + 1)
            ,{
                {
                    mfxU16(MFX_PROFILE_AV1_MAIN),
                    {mfxU32(MFX_FOURCC_NV12), mfxU32(MFX_FOURCC_P010), mfxU32(MFX_FOURCC_RGB4), mfxU32(MFX_FOURCC_BGR4),
                        }}
                }
            },
        };

        invalid += !compatible.count(pCO3->TargetChromaFormatPlus1)
            || !compatible.at(pCO3->TargetChromaFormatPlus1).count(profile);

        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

        const auto& supportFourCC = compatible.at(pCO3->TargetChromaFormatPlus1).at(profile);
        invalid += (std::find(supportFourCC.begin(), supportFourCC.end(), fourCC) == supportFourCC.end());

        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

        //check targetChromaFormat by caps
        const auto& csf = defPar.caps.ChromaSupportFlags;
        invalid += (pCO3->TargetChromaFormatPlus1 == (MFX_CHROMAFORMAT_YUV420 + 1) && (!csf.fields.i420));

        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

        return MFX_ERR_NONE;
    }

    static mfxStatus TargetBitDepth(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& defPar
        , mfxVideoParam& par)
    {
        mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
        MFX_CHECK(pCO3, MFX_ERR_NONE);

        bool b8bit  = (pCO3->TargetBitDepthLuma == BITDEPTH_8);
        bool b10bit = (pCO3->TargetBitDepthLuma == BITDEPTH_10);

        mfxU32 invalid = (pCO3->TargetBitDepthLuma > 0) && (!(b8bit || b10bit));
        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

        //check TargetBitDepth by caps
        invalid += (b8bit && !defPar.caps.BitDepthSupportFlags.fields.eight_bits);
        invalid += (b10bit && !defPar.caps.BitDepthSupportFlags.fields.ten_bits);
        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

        if (pCO3->TargetBitDepthLuma != pCO3->TargetBitDepthChroma)
        {
            pCO3->TargetBitDepthLuma   = 0;
            pCO3->TargetBitDepthChroma = 0;
            invalid += 1;
        }

        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

        return MFX_ERR_NONE;
    }

    static mfxStatus FourCCByTargetFormat(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& dpar
        , mfxVideoParam& par)
    {
        auto tbdl = dpar.base.GetTargetBitDepthLuma(dpar);
        auto tcf = dpar.base.GetTargetChromaFormatPlus1(dpar);

        static const std::map<mfxU16, std::set<mfxU32>> Compatible[2] =
        {
            //8
            {
                {
                    mfxU16(1 + MFX_CHROMAFORMAT_YUV420)
                    , {MFX_FOURCC_NV12, MFX_FOURCC_P010
                    , MFX_FOURCC_RGB4, MFX_FOURCC_BGR4
                    }
                }
            },
            //10
            {
                {
                    mfxU16(1 + MFX_CHROMAFORMAT_YUV420)
                    , {MFX_FOURCC_P010
                    }
                }
            },
        };

        bool bUndefinedTargetFormat =
            (tbdl != 8 && tbdl != 10)
            || !Compatible[tbdl == 10].count(tcf)
            || !Compatible[tbdl == 10].at(tcf).count(par.mfx.FrameInfo.FourCC);

        par.mfx.FrameInfo.FourCC *= !bUndefinedTargetFormat;

        MFX_CHECK(!bUndefinedTargetFormat, MFX_ERR_UNSUPPORTED);

        return MFX_ERR_NONE;
    }

    static mfxStatus NumRefActive(
        Defaults::TCheckAndFix::TExt
        , const Defaults::Param& defPar
        , mfxVideoParam& par)
    {
        mfxU32 changed = 0;
        mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);

        MFX_CHECK(pCO3, MFX_ERR_NONE);

        mfxU16 maxDPB = par.mfx.NumRefFrame + 1;
        SetIf(maxDPB, !par.mfx.NumRefFrame, NUM_REF_FRAMES + 1);

        mfxU16 maxRef[3] = {0, 0, 0};
        std::tie(maxRef[0], maxRef[1], maxRef[2]) = defPar.base.GetMaxNumRef(defPar);
        for (mfxU16 i = 0; i < 3; i++)
        {
            maxRef[i] = std::min<mfxU16>(maxRef[i], maxDPB - 1);
        }

        for (mfxU16 i = 0; i < 8; i++)
        {
            changed += CheckMaxOrClip(pCO3->NumRefActiveP  [i], maxRef[0]);
            changed += CheckMaxOrClip(pCO3->NumRefActiveBL0[i], maxRef[1]);
            changed += CheckMaxOrClip(pCO3->NumRefActiveBL1[i], maxRef[2]);
        }

        MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        return MFX_ERR_NONE;
    }

    static void Push(Defaults& df)
    {
#define PUSH_DEFAULT(X) df.Check##X.Push(X);

        PUSH_DEFAULT(SurfSize);
        PUSH_DEFAULT(Profile);
        PUSH_DEFAULT(FourCC);
        PUSH_DEFAULT(InputFormatByFourCC);
        PUSH_DEFAULT(TargetChromaFormat);
        PUSH_DEFAULT(TargetBitDepth);
        PUSH_DEFAULT(FourCCByTargetFormat);
        PUSH_DEFAULT(NumRefActive);
#undef PUSH_DEFAULT
    }

};

void General::PushDefaults(Defaults& df)
{
    GetDefault::Push(df);
    PreCheck::Push(df);
    CheckAndFix::Push(df);
}

}

}
#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
