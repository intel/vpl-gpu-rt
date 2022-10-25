// Copyright (c) 2019-2022 Intel Corporation
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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base_general.h"
#include "av1ehw_base_data.h"
#include "av1ehw_base_constraints.h"
#include "av1ehw_base_task.h"
#include "fast_copy.h"
#include "mfx_common_int.h"
#include <algorithm>
#include <exception>
#include <iterator>
#include <numeric>
#include <set>
#include <iterator>

using namespace AV1EHW::Base;

namespace AV1EHW
{

void General::SetSupported(ParamSupport& blocks)
{
    blocks.m_mvpCopySupported.emplace_back(
        [](const mfxVideoParam* pSrc, mfxVideoParam* pDst) -> void
    {
        const auto& buf_src = *(const mfxVideoParam*)pSrc;
        auto& buf_dst = *(mfxVideoParam*)pDst;

        MFX_COPY_FIELD(IOPattern);
        MFX_COPY_FIELD(Protected);
        MFX_COPY_FIELD(AsyncDepth);
        MFX_COPY_FIELD(mfx.LowPower);
        MFX_COPY_FIELD(mfx.CodecId);
        MFX_COPY_FIELD(mfx.CodecLevel);
        MFX_COPY_FIELD(mfx.CodecProfile);
        MFX_COPY_FIELD(mfx.TargetUsage);
        MFX_COPY_FIELD(mfx.GopPicSize);
        MFX_COPY_FIELD(mfx.GopRefDist);
        MFX_COPY_FIELD(mfx.GopOptFlag);
        MFX_COPY_FIELD(mfx.BRCParamMultiplier);
        MFX_COPY_FIELD(mfx.RateControlMethod);
        MFX_COPY_FIELD(mfx.InitialDelayInKB);
        MFX_COPY_FIELD(mfx.BufferSizeInKB);
        MFX_COPY_FIELD(mfx.TargetKbps);
        MFX_COPY_FIELD(mfx.MaxKbps);
        MFX_COPY_FIELD(mfx.NumRefFrame);
        MFX_COPY_FIELD(mfx.EncodedOrder);
        MFX_COPY_FIELD(mfx.FrameInfo.Shift);
        MFX_COPY_FIELD(mfx.FrameInfo.BitDepthLuma);
        MFX_COPY_FIELD(mfx.FrameInfo.BitDepthChroma);
        MFX_COPY_FIELD(mfx.FrameInfo.FourCC);
        MFX_COPY_FIELD(mfx.FrameInfo.Width);
        MFX_COPY_FIELD(mfx.FrameInfo.Height);
        MFX_COPY_FIELD(mfx.FrameInfo.CropX);
        MFX_COPY_FIELD(mfx.FrameInfo.CropY);
        MFX_COPY_FIELD(mfx.FrameInfo.CropW);
        MFX_COPY_FIELD(mfx.FrameInfo.CropH);
        MFX_COPY_FIELD(mfx.FrameInfo.FrameRateExtN);
        MFX_COPY_FIELD(mfx.FrameInfo.FrameRateExtD);
        MFX_COPY_FIELD(mfx.FrameInfo.AspectRatioW);
        MFX_COPY_FIELD(mfx.FrameInfo.AspectRatioH);
        MFX_COPY_FIELD(mfx.FrameInfo.PicStruct);
        MFX_COPY_FIELD(mfx.FrameInfo.ChromaFormat);
    });


    blocks.m_ebCopySupported[MFX_EXTBUFF_AV1_BITSTREAM_PARAM].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtAV1BitstreamParam*)pSrc;
        auto& buf_dst = *(mfxExtAV1BitstreamParam*)pDst;

        MFX_COPY_FIELD(WriteIVFHeaders);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_AV1_RESOLUTION_PARAM].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtAV1ResolutionParam*)pSrc;
        auto& buf_dst = *(mfxExtAV1ResolutionParam*)pDst;

        MFX_COPY_FIELD(FrameWidth);
        MFX_COPY_FIELD(FrameHeight);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_CODING_OPTION3].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtCodingOption3*)pSrc;
        auto& buf_dst = *(mfxExtCodingOption3*)pDst;

        MFX_COPY_FIELD(EnableQPOffset);
        MFX_COPY_FIELD(GPB);

        MFX_COPY_ARRAY_FIELD(QPOffset);
        MFX_COPY_ARRAY_FIELD(NumRefActiveP);
        MFX_COPY_ARRAY_FIELD(NumRefActiveBL0);
        MFX_COPY_ARRAY_FIELD(NumRefActiveBL1);

        MFX_COPY_FIELD(TargetChromaFormatPlus1);
        MFX_COPY_FIELD(TargetBitDepthLuma);
        MFX_COPY_FIELD(TargetBitDepthChroma);
        MFX_COPY_FIELD(LowDelayBRC);
        MFX_COPY_FIELD(ScenarioInfo);
    });

    // keep it temporally for backward compability
    blocks.m_ebCopySupported[MFX_EXTBUFF_AVC_TEMPORAL_LAYERS].emplace_back(
        [](const mfxExtBuffer* /*pSrc*/, mfxExtBuffer* /*pDst*/) -> void
    {
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_UNIVERSAL_TEMPORAL_LAYERS].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtTemporalLayers*)pSrc;
        auto& buf_dst = *(mfxExtTemporalLayers*)pDst;

        MFX_COPY_FIELD(NumLayers);
        MFX_COPY_FIELD(Layers);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_ENCODER_CAPABILITY].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        auto& buf_src = *(const mfxExtEncoderCapability*)pSrc;
        auto& buf_dst = *(mfxExtEncoderCapability*)pDst;

        MFX_COPY_FIELD(MBPerSec);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_CODING_OPTION].emplace_back(
        [](const mfxExtBuffer* /* pSrc */, mfxExtBuffer* /* pDst */) -> void
    {
        // Teams query this buffer supportness, keeps empty since those fields ignored by AV1
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_VP9_PARAM].emplace_back(
        [](const mfxExtBuffer* /* pSrc */, mfxExtBuffer* /* pDst */) -> void
    {
        // Teams query this buffer supportness, keeps empty since those fields ignored by AV1
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_CODING_OPTION2].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtCodingOption2*)pSrc;
        auto& buf_dst = *(mfxExtCodingOption2*)pDst;

        MFX_COPY_FIELD(BRefType);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_ENCODER_RESET_OPTION].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtEncoderResetOption*)pSrc;
        auto& buf_dst = *(mfxExtEncoderResetOption*)pDst;

        MFX_COPY_FIELD(StartNewSequence);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_UNIVERSAL_REFLIST_CTRL].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtRefListCtrl*)pSrc;
        auto& buf_dst = *(mfxExtRefListCtrl*)pDst;

        for (mfxU32 i = 0; i < 16; i++)
        {
            MFX_COPY_FIELD(RejectedRefList[i].FrameOrder);
            MFX_COPY_FIELD(LongTermRefList[i].FrameOrder);
        }
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_VIDEO_SIGNAL_INFO].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtVideoSignalInfo*)pSrc;
        auto& buf_dst = *(mfxExtVideoSignalInfo*)pDst;

        MFX_COPY_FIELD(VideoFormat);
        MFX_COPY_FIELD(VideoFullRange);
        MFX_COPY_FIELD(ColourDescriptionPresent);
        MFX_COPY_FIELD(ColourPrimaries);
        MFX_COPY_FIELD(TransferCharacteristics);
        MFX_COPY_FIELD(MatrixCoefficients);
    });


    blocks.m_ebCopySupported[MFX_EXTBUFF_ALLOCATION_HINTS].emplace_back(
        [](const mfxExtBuffer*, mfxExtBuffer*) -> void
    {
        /* Just allow this buffer to be present at Init */
    });
}

void General::SetInherited(ParamInheritance& par)
{
    par.m_mvpInheritDefault.emplace_back(
        [](const mfxVideoParam* pSrc, mfxVideoParam* pDst)
    {
        auto& parInit = *pSrc;
        auto& parReset = *pDst;

#define INHERIT_OPT(OPT) InheritOption(parInit.OPT, parReset.OPT)
#define INHERIT_BRC(OPT) { OPT tmp(parReset.mfx); InheritOption(OPT(parInit.mfx), tmp); }

        INHERIT_OPT(AsyncDepth);
        //INHERIT_OPT(mfx.BRCParamMultiplier);
        INHERIT_OPT(mfx.CodecId);
        INHERIT_OPT(mfx.CodecProfile);
        INHERIT_OPT(mfx.CodecLevel);
        INHERIT_OPT(mfx.NumThread);
        INHERIT_OPT(mfx.TargetUsage);
        INHERIT_OPT(mfx.GopPicSize);
        INHERIT_OPT(mfx.GopRefDist);
        INHERIT_OPT(mfx.GopOptFlag);
        INHERIT_OPT(mfx.RateControlMethod);
        INHERIT_OPT(mfx.BufferSizeInKB);
        INHERIT_OPT(mfx.NumRefFrame);

        mfxU16 RC = parInit.mfx.RateControlMethod
            * (parInit.mfx.RateControlMethod == parReset.mfx.RateControlMethod);
        static const std::map<
            mfxU16 , std::function<void(const mfxVideoParam&, mfxVideoParam&)>
        > InheritBrcOpt =
        {
            {
                mfxU16(MFX_RATECONTROL_CBR)
                , [](const mfxVideoParam& parInit, mfxVideoParam& parReset)
                {
                    INHERIT_BRC(InitialDelayInKB);
                    INHERIT_BRC(TargetKbps);
                }
            }
            , {
                mfxU16(MFX_RATECONTROL_VBR)
                , [](const mfxVideoParam& parInit, mfxVideoParam& parReset)
                {
                    INHERIT_BRC(InitialDelayInKB);
                    INHERIT_BRC(TargetKbps);
                    INHERIT_BRC(MaxKbps);
                }
            }
            , {
                mfxU16(MFX_RATECONTROL_CQP)
                , [](const mfxVideoParam& parInit, mfxVideoParam& parReset)
                {
                    INHERIT_OPT(mfx.QPI);
                    INHERIT_OPT(mfx.QPP);
                    INHERIT_OPT(mfx.QPB);
                }
            }
            , {
                mfxU16(MFX_RATECONTROL_ICQ)
                , [](const mfxVideoParam& parInit, mfxVideoParam& parReset)
                {
                    INHERIT_OPT(mfx.ICQQuality);
                }
            }
            , {
                mfxU16(MFX_RATECONTROL_LA_ICQ)
                , [](const mfxVideoParam& parInit, mfxVideoParam& parReset)
                {
                    INHERIT_OPT(mfx.ICQQuality);
                }
            }
            , {
                mfxU16(MFX_RATECONTROL_VCM)
                , [](const mfxVideoParam& parInit, mfxVideoParam& parReset)
                {
                    INHERIT_BRC(InitialDelayInKB);
                    INHERIT_BRC(TargetKbps);
                    INHERIT_BRC(MaxKbps);
                }
            }
            , {
                mfxU16(MFX_RATECONTROL_QVBR)
                , [](const mfxVideoParam& parInit, mfxVideoParam& parReset)
                {
                    INHERIT_BRC(InitialDelayInKB);
                    INHERIT_BRC(TargetKbps);
                    INHERIT_BRC(MaxKbps);
                }
            }
        };
        auto itInheritBrcOpt = InheritBrcOpt.find(RC);

        if (itInheritBrcOpt != InheritBrcOpt.end())
            itInheritBrcOpt->second(parInit, parReset);

        INHERIT_OPT(mfx.FrameInfo.FourCC);
        INHERIT_OPT(mfx.FrameInfo.Width);
        INHERIT_OPT(mfx.FrameInfo.Height);
        INHERIT_OPT(mfx.FrameInfo.CropX);
        INHERIT_OPT(mfx.FrameInfo.CropY);
        INHERIT_OPT(mfx.FrameInfo.CropW);
        INHERIT_OPT(mfx.FrameInfo.CropH);
        INHERIT_OPT(mfx.FrameInfo.FrameRateExtN);
        INHERIT_OPT(mfx.FrameInfo.FrameRateExtD);
        INHERIT_OPT(mfx.FrameInfo.AspectRatioW);
        INHERIT_OPT(mfx.FrameInfo.AspectRatioH);

#undef INHERIT_OPT
#undef INHERIT_BRC
    });

#define INIT_EB(TYPE)\
    if (!pSrc || !pDst) return;\
    auto& ebInit = *(TYPE*)pSrc;\
    auto& ebReset = *(TYPE*)pDst;
#define INHERIT_OPT(OPT) InheritOption(ebInit.OPT, ebReset.OPT);

    par.m_ebInheritDefault[MFX_EXTBUFF_AV1_BITSTREAM_PARAM].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        INIT_EB(mfxExtAV1BitstreamParam);

        INHERIT_OPT(WriteIVFHeaders);
    });

    par.m_ebInheritDefault[MFX_EXTBUFF_CODING_OPTION2].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        INIT_EB(mfxExtCodingOption2);
        INHERIT_OPT(BRefType);
    });

    par.m_ebInheritDefault[MFX_EXTBUFF_CODING_OPTION3].emplace_back(
        [this](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        INIT_EB(mfxExtCodingOption3);

        INHERIT_OPT(GPB);

        for (mfxU32 i = 0; i < 8; i++)
        {
            INHERIT_OPT(NumRefActiveP[i]);
            INHERIT_OPT(NumRefActiveBL0[i]);
            INHERIT_OPT(NumRefActiveBL1[i]);
        }

        INHERIT_OPT(TargetChromaFormatPlus1);
        INHERIT_OPT(TargetBitDepthLuma);
        INHERIT_OPT(TargetBitDepthChroma);
        INHERIT_OPT(LowDelayBRC);
        INHERIT_OPT(ScenarioInfo);
    });
#undef INIT_EB
#undef INHERIT_OPT
}

void General::Query0(const FeatureBlocks& blocks, TPushQ0 Push)
{
    using namespace std::placeholders;
    Push(BLK_Query0, std::bind(&General::CheckQuery0, this, std::cref(blocks), _1));
}

void General::Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push)
{
#if defined(MFX_ENABLE_LOG_UTILITY)
    Push(BLK_SetLogging,
        [this](const mfxVideoParam&, mfxVideoParam& out, StorageRW&) -> mfxStatus
    {

        return MFX_ERR_NONE;
    });
#endif

    Push(BLK_SetDefaultsCallChain,
        [this](const mfxVideoParam&, mfxVideoParam&, StorageRW& strg) -> mfxStatus
    {
        auto& defaults = Glob::Defaults::GetOrConstruct(strg);
        auto& bSet = defaults.SetForFeature[GetID()];
        MFX_CHECK(!bSet, MFX_ERR_NONE);

        PushDefaults(defaults);

        bSet = true;

        m_pQNCDefaults = &defaults;
        m_hw = Glob::VideoCore::Get(strg).GetHWType();

        return MFX_ERR_NONE;
    });

    Push(BLK_PreCheckCodecId,
        [&blocks, this](const mfxVideoParam& in, mfxVideoParam&, StorageRW& /*strg*/) -> mfxStatus
    {
        return m_pQNCDefaults->PreCheckCodecId(in);
    });

    Push(BLK_PreCheckChromaFormat,
        [this](const mfxVideoParam& in, mfxVideoParam&, StorageW&) -> mfxStatus
    {
        return m_pQNCDefaults->PreCheckChromaFormat(in);
    });

    Push(BLK_PreCheckExtBuffers
        , [this, &blocks](const mfxVideoParam& in, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckBuffers(blocks, in, &out);
    });

    Push(BLK_CopyConfigurable
        , [this, &blocks](const mfxVideoParam& in, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CopyConfigurable(blocks, in, out);
    });

    Push(BLK_CheckAndFixLowPower
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW& /*strg*/) -> mfxStatus
    {
        mfxU32 invalid = 0;

        invalid += Check<mfxU16
            , MFX_CODINGOPTION_UNKNOWN
            , MFX_CODINGOPTION_ON
            , MFX_CODINGOPTION_OFF>
            (out.mfx.LowPower);

        if (invalid)
        {
            out.mfx.LowPower = MFX_CODINGOPTION_ON;
            return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        SetIf(out.mfx.LowPower, out.mfx.LowPower == MFX_CODINGOPTION_OFF, MFX_CODINGOPTION_ON);

        return MFX_ERR_NONE;
    });
}

mfxStatus General::MapLevel(mfxVideoParam& par)
{
    MFX_CHECK(par.mfx.CodecLevel, MFX_ERR_NONE);

    mfxU32 changed = 0;

    // Map undefined level to defined level
    changed += MapToDefinedLevel(par.mfx.CodecLevel);

    MFX_CHECK(!changed, MFX_WRN_VIDEO_PARAM_CHANGED);
    return MFX_ERR_NONE;
}

inline mfxStatus CheckPicStruct(mfxVideoParam & par)
{
    mfxU32 invalid = 0;
    invalid += CheckOrZero(par.mfx.FrameInfo.PicStruct, mfxU16(MFX_PICSTRUCT_PROGRESSIVE), mfxU16(0));

    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
}

inline mfxStatus CheckProtected(mfxVideoParam& par)
{
    mfxU32 invalid = 0;
    invalid += CheckOrZero(par.Protected, mfxU16(0));

    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
}

inline mfxStatus CheckEncodedOrder(mfxVideoParam& par)
{
    mfxU32 invalid = 0;
    invalid += CheckOrZero(par.mfx.EncodedOrder, mfxU16(0));

    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
}

void General::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckFormat
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW& strg) -> mfxStatus
    {
        m_pQWCDefaults.reset(
            new Defaults::Param(
                out
                , Glob::EncodeCaps::Get(strg)
                , Glob::Defaults::Get(strg)));

        auto sts = m_pQWCDefaults->base.CheckFourCC(*m_pQWCDefaults, out);
        MFX_CHECK_STS(sts);
        sts = m_pQWCDefaults->base.CheckInputFormatByFourCC(*m_pQWCDefaults, out);
        MFX_CHECK_STS(sts);
        sts = m_pQWCDefaults->base.CheckTargetChromaFormat(*m_pQWCDefaults, out);
        MFX_CHECK_STS(sts);
        sts = m_pQWCDefaults->base.CheckTargetBitDepth(*m_pQWCDefaults, out);
        MFX_CHECK_STS(sts);
        return m_pQWCDefaults->base.CheckFourCCByTargetFormat(*m_pQWCDefaults, out);
    });

    Push(BLK_CheckLevel
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        mfxStatus stsMap = MapLevel(out);
        mfxStatus stsCheckValid = m_pQWCDefaults->base.CheckLevel(*m_pQWCDefaults, out);
        // invalid level error code should override mapped level error code
        MFX_CHECK_STS(stsCheckValid);
        MFX_CHECK_STS(stsMap);
        return MFX_ERR_NONE;

    });

    Push(BLK_CheckPicStruct
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckPicStruct(out);
    });

    Push(BLK_CheckSurfSize
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return m_pQWCDefaults->base.CheckSurfSize(*m_pQWCDefaults, out);
    });

    Push(BLK_CheckCodedPicSize
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckCodedPicSize(out, *m_pQWCDefaults);
    });

    Push(BLK_CheckTU
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckTU(out, m_pQWCDefaults->caps);
    });

    Push(BLK_CheckDeltaQ
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckDeltaQ(out);
    });

    Push(BLK_CheckFrameOBU
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW& strg) -> mfxStatus
    {
        const auto& caps = Glob::EncodeCaps::Get(strg);
        return CheckFrameOBU(out, caps);
    });

    Push(BLK_CheckOrderHint
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW& strg) -> mfxStatus
    {
        const auto& caps = Glob::EncodeCaps::Get(strg);
        return CheckOrderHint(out, caps);
    });

    Push(BLK_CheckOrderHintBits
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckOrderHintBits(out);
    });

    Push(BLK_CheckCDEF
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW& strg) -> mfxStatus
    {
        const auto& caps = Glob::EncodeCaps::Get(strg);
        return CheckCDEF(out, caps);
    });

    Push(BLK_CheckTemporalLayers
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckTemporalLayers(out);
    });

    Push(BLK_CheckStillPicture
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckStillPicture(out);
    });

    Push(BLK_CheckGopRefDist
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckGopRefDist(out, *m_pQWCDefaults);
    });

    Push(BLK_CheckGPB
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckGPB(out);
    });

    Push(BLK_CheckColorConfig
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckColorConfig(out);
    });

    Push(BLK_CheckNumRefFrame
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckNumRefFrame(out, *m_pQWCDefaults);
    });

    Push(BLK_CheckIOPattern
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckIOPattern(out);
    });

    Push(BLK_CheckProtected
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckProtected(out);
    });

    Push(BLK_CheckRateControl
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckRateControl(out, *m_pQWCDefaults);
    });

    Push(BLK_CheckCrops
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckCrops(out, *m_pQWCDefaults);
    });

    Push(BLK_CheckShift
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckShift(out);
    });

    Push(BLK_CheckFrameRate
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckFrameRate(out);
    });

    Push(BLK_CheckProfile
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return m_pQWCDefaults->base.CheckProfile(*m_pQWCDefaults, out);
    });

    Push(BLK_CheckEncodedOrder
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckEncodedOrder(out);
    });

    Push(BLK_CheckLevelConstraints
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        return CheckLevelConstraints(out, *m_pQWCDefaults);
    });

    Push(BLK_CheckTCBRC
        ,[this](const mfxVideoParam&, mfxVideoParam& out, StorageW& strg) -> mfxStatus
    {
        const auto& caps = Glob::EncodeCaps::Get(strg);
        return CheckTCBRC(out, caps);
    });
}

static mfxStatus RunQuery1NoCapsQueue(const FeatureBlocks& blocks, const mfxVideoParam& in, StorageRW& strg)
{
    mfxStatus sts = MFX_ERR_NONE;

    auto pPar = make_storable<ExtBuffer::Param<mfxVideoParam>>(in);
    auto& par = *pPar;
    const auto& query = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(blocks);

    sts = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, query, in, par, strg);
    MFX_CHECK(sts != MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);

    strg.Insert(Glob::VideoParam::Key, std::move(pPar));

    return sts;
}

static mfxStatus RunSetDefaultsQueue(const FeatureBlocks& blocks, StorageRW& strg, StorageRW& local)
{
    auto& par = Glob::VideoParam::Get(strg);
    Glob::EncodeCaps::GetOrConstruct(strg);
    auto& qSD = FeatureBlocks::BQ<FeatureBlocks::BQ_SetDefaults>::Get(blocks);

    return RunBlocks(IgnoreSts, qSD, par, strg, local);
};

static mfxStatus RunQuery1WithCapsQueue(const FeatureBlocks& blocks, const mfxVideoParam& in, StorageRW& strg)
{
    mfxStatus sts = MFX_ERR_NONE;

    auto& par = Glob::VideoParam::Get(strg);
    auto& queryWC = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);

    sts = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, queryWC, in, par, strg);
    MFX_CHECK(sts != MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);

    return sts;
};

static mfxStatus SetUnalignedDefaults(mfxVideoParam& par)
{
    /*
    * VPL and encoder default values are different
    * And writing IVF headers is enabled in encoder when mfxExtAV1BitstreamParam is attached and its value is ON or zero.
    * But writing IVF headers is disabled by default in encoder when mfxExtAV1BitstreamParam is not attached.
    */
    mfxExtAV1BitstreamParam* pBsPar = ExtBuffer::Get(par);
    if (pBsPar != nullptr)
    {
        SetDefault(pBsPar->WriteIVFHeaders, MFX_CODINGOPTION_ON);
    }

    return MFX_ERR_NONE;
}

void General::QueryIOSurf(const FeatureBlocks& blocks, TPushQIS Push)
{
    Push(BLK_Query1NoCaps
        , [this, &blocks](const mfxVideoParam& in, mfxFrameAllocRequest&, StorageRW& strg) -> mfxStatus
    {
        mfxStatus sts = RunQuery1NoCapsQueue(blocks, in, strg);
        MFX_CHECK(sts >= MFX_ERR_NONE, sts);

        return MFX_ERR_NONE;
    });

    Push(BLK_SetUnalignedDefaults
        , [this, &blocks](const mfxVideoParam&, mfxFrameAllocRequest&, StorageRW& strg) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(strg);
        return SetUnalignedDefaults(par);
    });

    Push(BLK_SetDefaults
        , [this, &blocks](const mfxVideoParam&, mfxFrameAllocRequest&, StorageRW& strg) -> mfxStatus
    {
        StorageRW local;

        return RunSetDefaultsQueue(blocks, strg, local);
    });

    Push(BLK_Query1WithCaps
        , [this, &blocks](const mfxVideoParam& in, mfxFrameAllocRequest&, StorageRW& strg) -> mfxStatus
    {
        mfxStatus sts = RunQuery1WithCapsQueue(blocks, in, strg);
        MFX_CHECK(sts >= MFX_ERR_NONE, sts);

        return MFX_ERR_NONE;
    });

    Push(BLK_SetFrameAllocRequest
        , [this, &blocks](const mfxVideoParam&, mfxFrameAllocRequest& req, StorageRW& strg) -> mfxStatus
    {
        ExtBuffer::Param<mfxVideoParam>& par = Glob::VideoParam::Get(strg);
        auto fourCC = par.mfx.FrameInfo.FourCC;

        req.Info = par.mfx.FrameInfo;
        SetDefault(req.Info.Shift, (fourCC == MFX_FOURCC_P010 || fourCC == MFX_FOURCC_Y210));

        bool bSYS = par.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        bool bVID = par.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY;

        req.Type =
            bSYS * (MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME)
            + bVID * (MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME);
        MFX_CHECK(req.Type, MFX_ERR_INVALID_VIDEO_PARAM);

        req.NumFrameMin = GetMaxRaw(par);
        req.NumFrameSuggested = req.NumFrameMin;

        return MFX_ERR_NONE;
    });
}

void General::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [this](mfxVideoParam& par, StorageW& strg, StorageRW&)
    {
        auto& core = Glob::VideoCore::Get(strg);
        auto& caps = Glob::EncodeCaps::Get(strg);
        auto& defchain = Glob::Defaults::Get(strg);
        SetDefaults(par, Defaults::Param(par, caps, defchain), core.IsExternalFrameAllocator());
    });
}

void General::InitExternal(const FeatureBlocks& blocks, TPushIE Push)
{
    Push(BLK_Query1NoCaps
        , [this, &blocks](const mfxVideoParam& in, StorageRW& strg, StorageRW&) -> mfxStatus
    {
        return RunQuery1NoCapsQueue(blocks, in, strg);
    });

    Push(BLK_SetUnalignedDefaults
        , [this, &blocks](const mfxVideoParam&, StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(strg);
        return SetUnalignedDefaults(par);
    });

    Push(BLK_AttachMissingBuffers
        , [this, &blocks](const mfxVideoParam&, StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(strg);
        for (auto& eb : blocks.m_ebCopySupported)
            par.NewEB(eb.first, false);

        par.NewEB(MFX_EXTBUFF_AV1_AUXDATA, false);

        return MFX_ERR_NONE;
    });

    Push(BLK_SetDefaults
        , [this, &blocks](const mfxVideoParam&, StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        return RunSetDefaultsQueue(blocks, strg, local);
    });

    Push(BLK_Query1WithCaps
        , [this, &blocks](const mfxVideoParam& in, StorageRW& strg, StorageRW&) -> mfxStatus
    {
        return RunQuery1WithCapsQueue(blocks, in, strg);
    });
}

void General::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetReorder
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        using namespace std::placeholders;
        auto& par = Glob::VideoParam::Get(strg);

        auto pReorderer = make_storable<Reorderer>();
        pReorderer->Push(
            [&](Reorderer::TExt, TTaskIt begin, TTaskIt end, bool bFlush)
        {
            return ReorderWrap(par, begin, end, bFlush);
        });
        pReorderer->BufferSize = par.mfx.GopRefDist > 1 ? par.mfx.GopRefDist - 1 : 0;
        //pReorderer->DPB = &m_prevTask.DPB.After;

        strg.Insert(Glob::Reorder::Key, std::move(pReorderer));

        return MFX_ERR_NONE;
    });

    Push(BLK_SetRepeat
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        strg.Insert(Glob::FramesToShowInfo::Key, make_storable<TFramesToShowInfo>());
        strg.Insert(Glob::RepeatFrameSizeInfo::Key, make_storable<TRepeatFrameSizeInfo>());

        return MFX_ERR_NONE;
    });

    Push(BLK_SetSH
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        if (!strg.Contains(Glob::SH::Key))
        {
            auto pSH = make_storable<SH>();

            SetSH(
                Glob::VideoParam::Get(strg)
                , Glob::VideoCore::Get(strg).GetHWType()
                , Glob::EncodeCaps::Get(strg)
                , *pSH);

            strg.Insert(Glob::SH::Key, std::move(pSH));
        }

        return MFX_ERR_NONE;
    });

    Push(BLK_SetFH
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        if (!strg.Contains(Glob::FH::Key))
        {
            std::unique_ptr<MakeStorable<FH>> pFH(new MakeStorable<FH>);
            SetFH(
                Glob::VideoParam::Get(strg)
                , Glob::VideoCore::Get(strg).GetHWType()
                , Glob::SH::Get(strg)
                , *pFH);
            strg.Insert(Glob::FH::Key, std::move(pFH));
        }

        return MFX_ERR_NONE;
    });

    Push(BLK_SetRecInfo
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(strg);
        mfxFrameAllocRequest rec = {}, raw = {};

        const mfxExtCodingOption3& CO3 = ExtBuffer::Get(par);
        if (GetRecInfo(par, CO3, Glob::VideoCore::Get(strg).GetHWType(), rec.Info))
        {
            auto& recInfo = Tmp::RecInfo::GetOrConstruct(local, rec);
            SetDefault(recInfo.NumFrameMin, GetMaxRec(strg, par));
        }

        raw.Info = par.mfx.FrameInfo;
        auto& rawInfo = Tmp::RawInfo::GetOrConstruct(local, raw);
        SetDefault(rawInfo.NumFrameMin, GetMaxRaw(par));
        SetDefault(rawInfo.Type
            , mfxU16(MFX_MEMTYPE_FROM_ENCODE
                | MFX_MEMTYPE_DXVA2_DECODER_TARGET
                | MFX_MEMTYPE_INTERNAL_FRAME));

        return MFX_ERR_NONE;
    });
}

void General::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_AllocRaw
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        mfxStatus sts = MFX_ERR_NONE;
        auto& par = Glob::VideoParam::Get(strg);
        auto& rawInfo = Tmp::RawInfo::Get(local);
        auto AllocRaw = [&](mfxU16 NumFrameMin)
        {
            std::unique_ptr<IAllocation> pAlloc(Tmp::MakeAlloc::Get(local)(Glob::VideoCore::Get(strg)));
            mfxFrameAllocRequest req = rawInfo;
            req.NumFrameMin = NumFrameMin;

            sts = pAlloc->Alloc(req, true);
            MFX_CHECK_STS(sts);

            strg.Insert(Glob::AllocRaw::Key, std::move(pAlloc));

            return MFX_ERR_NONE;
        };

        if (par.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
        {
            sts = AllocRaw(rawInfo.NumFrameMin);
            MFX_CHECK_STS(sts);
        }

        return sts;
    });

    Push(BLK_AllocRec
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        mfxStatus sts = MFX_ERR_NONE;
        auto& par = Glob::VideoParam::Get(strg);
        std::unique_ptr<IAllocation> pAlloc(Tmp::MakeAlloc::Get(local)(Glob::VideoCore::Get(strg)));

        MFX_CHECK(local.Contains(Tmp::RecInfo::Key), MFX_ERR_UNDEFINED_BEHAVIOR);
        auto& req = Tmp::RecInfo::Get(local);

        SetDefault(req.NumFrameMin, GetMaxRec(strg, par));
        SetDefault(req.Type
            , mfxU16(MFX_MEMTYPE_FROM_ENCODE
            | MFX_MEMTYPE_DXVA2_DECODER_TARGET
            | MFX_MEMTYPE_INTERNAL_FRAME
            | MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET));

        sts = pAlloc->Alloc(req, false);
        MFX_CHECK_STS(sts);

        strg.Insert(Glob::AllocRec::Key, std::move(pAlloc));

        return sts;
    });

    Push(BLK_AllocBS
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        mfxStatus sts = MFX_ERR_NONE;
        auto& par = Glob::VideoParam::Get(strg);
        std::unique_ptr<IAllocation> pAlloc(Tmp::MakeAlloc::Get(local)(Glob::VideoCore::Get(strg)));

        MFX_CHECK(local.Contains(Tmp::BSAllocInfo::Key), MFX_ERR_UNDEFINED_BEHAVIOR);
        auto& req = Tmp::BSAllocInfo::Get(local);

        SetDefault(req.NumFrameMin, GetMaxBS(strg, par));
        SetDefault(req.Type
            , mfxU16(MFX_MEMTYPE_FROM_ENCODE
            | MFX_MEMTYPE_DXVA2_DECODER_TARGET
            | MFX_MEMTYPE_INTERNAL_FRAME));

        mfxU32 minBS = GetMinBsSize(par, ExtBuffer::Get(par), ExtBuffer::Get(par));

        if (mfxU32(req.Info.Width * req.Info.Height) < minBS)
        {
            MFX_CHECK(req.Info.Width != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
            req.Info.Height = (mfxU16)mfx::CeilDiv<mfxU32>(minBS, req.Info.Width);
        }

        sts = pAlloc->Alloc(req, false);
        MFX_CHECK_STS(sts);

        strg.Insert(Glob::AllocBS::Key, std::move(pAlloc));

        return sts;
    });
}

void General::Reset(const FeatureBlocks& blocks, TPushR Push)
{
    Push(BLK_ResetInit
        , [this, &blocks](
            const mfxVideoParam& par
            , StorageRW& global
            , StorageRW& local) -> mfxStatus
    {
        mfxStatus wrn = MFX_ERR_NONE;
        auto& init = Glob::RealState::Get(global);
        auto pParNew = make_storable<ExtBuffer::Param<mfxVideoParam>>(par);
        ExtBuffer::Param<mfxVideoParam>& parNew = *pParNew;
        auto& parOld = Glob::VideoParam::Get(init);

        global.Insert(Glob::ResetHint::Key, make_storable<ResetHint>(ResetHint{}));
        auto& hint = Glob::ResetHint::Get(global);

        const mfxExtEncoderResetOption* pResetOpt = ExtBuffer::Get(par);
        hint.Flags = RF_IDR_REQUIRED * (pResetOpt && IsOn(pResetOpt->StartNewSequence));

        std::for_each(std::begin(blocks.m_ebCopySupported)
            , std::end(blocks.m_ebCopySupported)
            , [&](decltype(*std::begin(blocks.m_ebCopySupported)) eb) { parNew.NewEB(eb.first, false); });

        parNew.NewEB(MFX_EXTBUFF_AV1_AUXDATA, false);

        std::for_each(std::begin(blocks.m_mvpInheritDefault)
            , std::end(blocks.m_mvpInheritDefault)
            , [&](decltype(*std::begin(blocks.m_mvpInheritDefault)) inherit) { inherit(&parOld, &parNew); });

        std::for_each(std::begin(blocks.m_ebInheritDefault)
            , std::end(blocks.m_ebInheritDefault)
            , [&](decltype(*std::begin(blocks.m_ebInheritDefault)) eb)
        {
            auto pEbNew = ExtBuffer::Get(parNew, eb.first);
            auto pEbOld = ExtBuffer::Get(parOld, eb.first);

            MFX_CHECK(pEbNew && pEbOld, MFX_ERR_NONE);

            std::for_each(std::begin(eb.second)
                , std::end(eb.second)
                , [&](decltype(*std::begin(eb.second)) inherit) { inherit(parOld, pEbOld, parNew, pEbNew); });

            return MFX_ERR_NONE;
        });

        auto& qInitExternal = FeatureBlocks::BQ<FeatureBlocks::BQ_InitExternal>::Get(blocks);
        auto sts = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, qInitExternal, parNew, global, local);
        MFX_CHECK(sts >= MFX_ERR_NONE, sts);
        wrn = sts;

        auto& qInitInternal = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
        sts = RunBlocks(CheckGE<mfxStatus, MFX_ERR_NONE>, qInitInternal, global, local);
        MFX_CHECK(sts >= MFX_ERR_NONE, sts);

        return GetWorstSts(sts, wrn);
    });

    Push(BLK_ResetCheck
        , [this, &blocks](
            const mfxVideoParam& par
            , StorageRW& global
            , StorageRW& local) -> mfxStatus
    {
        auto& init = Glob::RealState::Get(global);
        auto& parOld = Glob::VideoParam::Get(init);
        auto& parNew = Glob::VideoParam::Get(global);
        auto& hint = Glob::ResetHint::Get(global);
        auto defOld = GetRTDefaults(init);
        auto defNew = GetRTDefaults(global);

        MFX_CHECK(parOld.AsyncDepth                 == parNew.AsyncDepth,                   MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(parOld.mfx.GopRefDist             >= parNew.mfx.GopRefDist,               MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(parOld.mfx.NumRefFrame            >= parNew.mfx.NumRefFrame,              MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(parOld.mfx.RateControlMethod      == parNew.mfx.RateControlMethod,        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(parOld.mfx.FrameInfo.ChromaFormat == parNew.mfx.FrameInfo.ChromaFormat,   MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(parOld.IOPattern                  == parNew.IOPattern,                    MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        MFX_CHECK(local.Contains(Tmp::RecInfo::Key), MFX_ERR_UNDEFINED_BEHAVIOR);
        auto  recOld = Glob::AllocRec::Get(init).GetInfo();
        auto& recNew = Tmp::RecInfo::Get(local).Info;
        MFX_CHECK(recOld.Width  >= recNew.Width,  MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(recOld.Height >= recNew.Height, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(recOld.FourCC == recNew.FourCC, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        MFX_CHECK(
            !(   parOld.mfx.RateControlMethod == MFX_RATECONTROL_CBR
              || parOld.mfx.RateControlMethod == MFX_RATECONTROL_VBR
              || parOld.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
              || ((mfxU32)InitialDelayInKB(parOld.mfx) == (mfxU32)InitialDelayInKB(parNew.mfx)
              && (mfxU32)BufferSizeInKB(parOld.mfx) == (mfxU32)BufferSizeInKB(parNew.mfx))
            , MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        auto& sh = Glob::SH::Get(global);
        bool isSpsChanged = false;
        isSpsChanged = (parOld.mfx.FrameInfo.FrameRateExtN != parNew.mfx.FrameInfo.FrameRateExtN && sh.timing_info_present_flag == 1)
            || (parOld.mfx.FrameInfo.FrameRateExtD != parNew.mfx.FrameInfo.FrameRateExtD && sh.timing_info_present_flag == 1)
            || parOld.mfx.FrameInfo.Height != parNew.mfx.FrameInfo.Height
            || parOld.mfx.FrameInfo.Width != parNew.mfx.FrameInfo.Width
            || parOld.mfx.CodecLevel != parNew.mfx.CodecLevel;

        hint.Flags |= RF_SPS_CHANGED * isSpsChanged;

        bool isIdrRequired = false;
        const auto numTlOld = defOld.base.GetNumTemporalLayers(defOld);
        const auto numTlNew = defNew.base.GetNumTemporalLayers(defNew);

        isIdrRequired =
               (hint.Flags & RF_SPS_CHANGED)
            || (hint.Flags & RF_IDR_REQUIRED)
            || (numTlOld != numTlNew)
            || parOld.mfx.GopPicSize != parNew.mfx.GopPicSize;

        hint.Flags |= RF_IDR_REQUIRED * isIdrRequired;

        const mfxExtEncoderResetOption* pResetOpt = ExtBuffer::Get(par);
        MFX_CHECK(!isIdrRequired || !(pResetOpt && IsOff(pResetOpt->StartNewSequence))
            , MFX_ERR_INVALID_VIDEO_PARAM); // Reset can't change parameters w/o IDR. Report an error

        const mfxExtCodingOption3& CO3 = ExtBuffer::Get(parNew);
        bool brcReset =
            (      parOld.mfx.RateControlMethod == MFX_RATECONTROL_CBR
                || parOld.mfx.RateControlMethod == MFX_RATECONTROL_VBR
                || parOld.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
            && (   ((mfxU32)TargetKbps(parOld.mfx) != (mfxU32)TargetKbps(parNew.mfx) && !IsOn(CO3.LowDelayBRC))
                || (mfxU32)BufferSizeInKB(parOld.mfx) != (mfxU32)BufferSizeInKB(parNew.mfx)
                || (mfxU32)InitialDelayInKB(parOld.mfx) != (mfxU32)InitialDelayInKB(parNew.mfx)
                || parOld.mfx.FrameInfo.FrameRateExtN != parNew.mfx.FrameInfo.FrameRateExtN
                || parOld.mfx.FrameInfo.FrameRateExtD != parNew.mfx.FrameInfo.FrameRateExtD);

        brcReset |=
            (      parOld.mfx.RateControlMethod == MFX_RATECONTROL_VBR
                || parOld.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
            && ((mfxU32)MaxKbps(parOld.mfx) != (mfxU32)MaxKbps(parNew.mfx));

        hint.Flags |= RF_BRC_RESET * (brcReset || isIdrRequired);

        return MFX_ERR_NONE;
    });
}

void General::ResetState(const FeatureBlocks& blocks, TPushRS Push)
{
    Push(BLK_ResetState
        , [this, &blocks](
            StorageRW& global
            , StorageRW&) -> mfxStatus
    {
        auto& real = Glob::RealState::Get(global);
        auto& parInt = Glob::VideoParam::Get(real);
        auto& parNew = Glob::VideoParam::Get(global);
        auto& hint = Glob::ResetHint::Get(global);

        CopyConfigurable(blocks, parNew, parInt);
        Glob::SH::Get(real) = Glob::SH::Get(global);
        Glob::FH::Get(real) = Glob::FH::Get(global);

        MFX_CHECK(hint.Flags & RF_IDR_REQUIRED, MFX_ERR_NONE);

        // Need to call ResetState before UnlockAll surfaces, as DBP releaser will try to unlock surface in ResetState
        ResetState();

        return MFX_ERR_NONE;
    });
}

void General::FrameSubmit(const FeatureBlocks& blocks, TPushFS Push)
{
    Push(BLK_CheckSurf
        , [this, &blocks](
            const mfxEncodeCtrl* /*pCtrl*/
            , const mfxFrameSurface1* pSurf
            , mfxBitstream& /*bs*/
            , StorageW& global
            , StorageRW& /*local*/) -> mfxStatus
    {
        MFX_CHECK(pSurf, MFX_ERR_NONE);

        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(LumaIsNull(pSurf) == (pSurf->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(pSurf->Info.Width >= par.mfx.FrameInfo.Width, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(pSurf->Info.Height >= par.mfx.FrameInfo.Height, MFX_ERR_INVALID_VIDEO_PARAM);
        
        return MFX_ERR_NONE;
    });

    Push(BLK_CheckBS
        , [this, &blocks](
            const mfxEncodeCtrl* /*pCtrl*/
            , const mfxFrameSurface1* /*pSurf*/
            , mfxBitstream& bs
            , StorageW& global
            , StorageRW& local) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        BsDataInfo bsData = {};

        bsData.Data       = bs.Data;
        bsData.DataLength = bs.DataLength;
        bsData.DataOffset = bs.DataOffset;
        bsData.MaxLength  = bs.MaxLength;

        if (local.Contains(Tmp::BsDataInfo::Key))
            bsData = Tmp::BsDataInfo::Get(local);

        MFX_CHECK(bsData.DataOffset <= bsData.MaxLength, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(bsData.DataOffset + bsData.DataLength + BufferSizeInKB(par.mfx) * 1000u <= bsData.MaxLength, MFX_ERR_NOT_ENOUGH_BUFFER);
        MFX_CHECK_NULL_PTR1(bsData.Data);

        return MFX_ERR_NONE;
    });
}

void General::AllocTask(const FeatureBlocks& blocks, TPushAT Push)
{
    Push(BLK_AllocTask
        , [this, &blocks](
            StorageR& /*global*/
            , StorageRW& task) -> mfxStatus
    {
        task.Insert(Task::Common::Key, new Task::Common::TRef);
        task.Insert(Task::FH::Key, new MakeStorable<Task::FH::TRef>);
        return MFX_ERR_NONE;
    });
}

static bool CheckRefListCtrl(mfxExtRefListCtrl* refListCtrl)
{
    mfxU32 changed = 0;
    changed += CheckOrZero<mfxU16>(refListCtrl->ApplyLongTermIdx, 0);
    changed += CheckOrZero<mfxU16>(refListCtrl->NumRefIdxL0Active, 0);
    changed += CheckOrZero<mfxU16>(refListCtrl->NumRefIdxL1Active, 0);
    for (auto& preferred : refListCtrl->PreferredRefList)
    {
        if (preferred.FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN))
        {
            preferred.FrameOrder = mfxU32(MFX_FRAMEORDER_UNKNOWN);
            changed++;
        }
    }
    return changed;
}

void General::InitTask(const FeatureBlocks& blocks, TPushIT Push)
{
    Push(BLK_InitTask
        , [this, &blocks](
            mfxEncodeCtrl* pCtrl
            , mfxFrameSurface1* pSurf
            , mfxBitstream* pBs
            , StorageW& global
            , StorageW& task) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        auto& core = Glob::VideoCore::Get(global);
        auto& tpar = Task::Common::Get(task);

        auto stage = tpar.stage;
        tpar = TaskCommonPar();
        tpar.stage = stage;
        tpar.pBsOut = pBs;

        MFX_CHECK(pSurf, MFX_ERR_NONE);

        tpar.DisplayOrder = m_frameOrder;
        ++m_frameOrder;

        tpar.pSurfIn = pSurf;

        bool changed = 0;
        if (pCtrl)
        {
            tpar.ctrl = *pCtrl;
            if (mfxExtRefListCtrl* refListCtrl = ExtBuffer::Get(tpar.ctrl))
                changed = CheckRefListCtrl(refListCtrl);
        }
        tpar.pSurfReal = tpar.pSurfIn;

        core.IncreaseReference(*tpar.pSurfIn);

        tpar.DPB.resize(par.mfx.NumRefFrame);

        return changed ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
    });
}

void General::PreReorderTask(const FeatureBlocks& blocks, TPushPreRT Push)
{
    Push(BLK_PrepareTask
        , [this, &blocks](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);
        auto  dflts = GetRTDefaults(global);

        auto sts = dflts.base.GetPreReorderInfo(
            dflts, task, task.pSurfIn, &task.ctrl, m_lastKeyFrame, task.DisplayOrder, task.GopHints);
        MFX_CHECK_STS(sts);

        SetIf(m_lastKeyFrame, IsI(task.FrameType), task.DisplayOrder);

        return MFX_ERR_NONE;
    });
}

inline bool IsLossless(FH& fh)
{
    return (fh.quantization_params.base_q_idx == 0 && fh.quantization_params.DeltaQYDc == 0 && fh.quantization_params.DeltaQUAc == 0
        && fh.quantization_params.DeltaQUDc == 0 && fh.quantization_params.DeltaQVAc == 0 && fh.quantization_params.DeltaQVDc == 0);
}

inline void SetTaskFramesToShow(TaskCommonPar& task, TFramesToShowInfo& info)
{
    if (IsHiddenFrame(task))
    {
        info.insert(task.DisplayOrder);
    }

    const mfxU32 nextDisplayOrder = task.DisplayOrder + 1;
    if (info.find(nextDisplayOrder) == info.end())
        return;

    for (mfxU8 refIdx = 0; refIdx < task.DPB.size(); refIdx++)
    {
        if (task.RefreshFrameFlags[refIdx] == 1)
            continue;

        auto& refFrm = task.DPB.at(refIdx);
        if (refFrm->DisplayOrder != nextDisplayOrder)
            continue;

        RepeatedFrameInfo repfrm;
        repfrm.FrameToShowMapIdx = refIdx;
        repfrm.DisplayOrder      = refFrm->DisplayOrder;
        task.FramesToShow.push_back(std::move(repfrm));

        info.erase(nextDisplayOrder);
        return;
    }
}

inline void SetTaskRepeatedFramesSize(TaskCommonPar& task, TRepeatFrameSizeInfo& info)
{
    const mfxU32 prevEncodedOrder = task.EncodedOrder - 1;
    if (info.find(prevEncodedOrder) == info.end())
        return;

    if (info[prevEncodedOrder] != 0)
    {
        task.RepeatedFrameBytes = mfxU8(info[prevEncodedOrder]);
        info.erase(prevEncodedOrder);
    }
}

void General::PostReorderTask(const FeatureBlocks& blocks, TPushPostRT Push)
{
    Push(BLK_ConfigureTask
        , [this, &blocks](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);

        if (global.Contains(Glob::AllocRaw::Key))
        {
            task.Raw = Glob::AllocRaw::Get(global).Acquire();
            MFX_CHECK(task.Raw.Mid, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        auto& recPool = Glob::AllocRec::Get(global);
        task.Rec = recPool.Acquire();
        task.BS = Glob::AllocBS::Get(global).Acquire();
        MFX_CHECK(task.BS.Idx != IDX_INVALID, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(task.Rec.Idx != IDX_INVALID, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(task.Rec.Mid && task.BS.Mid, MFX_ERR_UNDEFINED_BEHAVIOR);

        auto& fh = Glob::FH::Get(global);
        auto  def = GetRTDefaults(global);
        ConfigureTask(task, def, recPool);

        auto& framesToShowInfo = Glob::FramesToShowInfo::Get(global);
        SetTaskFramesToShow(task, framesToShowInfo);

        auto& repeatFrameSizeInfo = Glob::RepeatFrameSizeInfo::Get(global);
        SetTaskRepeatedFramesSize(task, repeatFrameSizeInfo);

        auto& sh = Glob::SH::Get(global);
        auto sts = GetCurrentFrameHeader(task, def, sh, fh, Task::FH::Get(s_task));
        MFX_CHECK_STS(sts);

        return sts;
    });
}

void General::SubmitTask(const FeatureBlocks& blocks, TPushST Push)
{
    Push(BLK_GetRawHDL
        , [this, &blocks](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        auto& core = Glob::VideoCore::Get(global);
        auto& par = Glob::VideoParam::Get(global);
        auto& task = Task::Common::Get(s_task);

        bool bInternalFrame =
            par.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY
            || task.bSkip;

        MFX_CHECK(!bInternalFrame, core.GetFrameHDL(task.Raw.Mid, &task.HDLRaw.first));

        MFX_CHECK(par.IOPattern != MFX_IOPATTERN_IN_VIDEO_MEMORY
            , core.GetExternalFrameHDL(*task.pSurfReal, task.HDLRaw));

        return core.GetFrameHDL(task.pSurfReal->Data.MemId, &task.HDLRaw.first);
    });

    Push(BLK_CopySysToRaw
        , [this, &blocks](
            StorageW& global
            , StorageW& s_task)->mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        auto& task = Task::Common::Get(s_task);

        MFX_CHECK(
            !(task.bSkip
            || par.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY)
            , MFX_ERR_NONE);

        auto& core = Glob::VideoCore::Get(global);

        mfxFrameSurface1 surfSrc = MakeSurface(par.mfx.FrameInfo, *task.pSurfReal);
        mfxFrameSurface1 surfDst = MakeSurface(par.mfx.FrameInfo, task.Raw.Mid);

        surfDst.Info.Shift =
            surfDst.Info.FourCC == MFX_FOURCC_P010
            || surfDst.Info.FourCC == MFX_FOURCC_Y210; // convert to native shift in core.CopyFrame() if required

        return core.DoFastCopyWrapper(
            &surfDst
            , MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_ENCODE
            , &surfSrc
            , MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY);
    });
}

void General::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_CopyBS
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);
        if (!task.pBsData)
        {
            auto& bs              = *task.pBsOut;
            task.pBsData          = bs.Data + bs.DataOffset + bs.DataLength;
            task.pBsDataLength    = &bs.DataLength;
            task.BsBytesAvailable = bs.MaxLength - bs.DataOffset - bs.DataLength;
        }

        mfxStatus sts             = MFX_ERR_NONE;
        auto&     taskMgrIface    = TaskManager::TMInterface::Get(global);
        auto&     tm              = taskMgrIface.m_Manager;
        bool      bNeedCacheFrame = task.BsDataLength > 0 && (IsHiddenFrame(task) || task.DisplayOrder != m_temporalUnitOrder);
        if (bNeedCacheFrame)
        {
            FrameLocker codedFrame(Glob::VideoCore::Get(global), task.BS.Mid);
            MFX_CHECK(codedFrame.Y, MFX_ERR_LOCK_MEMORY);
            MfxEncodeHW::CachedBitstream cachedBs(task.BsDataLength);
            sts = FastCopy::Copy(
                *cachedBs.Data.get()
                , task.BsDataLength
                , codedFrame.Y
                , codedFrame.Pitch
                , { int(task.BsDataLength), 1 }
            , COPY_VIDEO_TO_SYS);

            cachedBs.isHiden = IsHiddenFrame(task);
            cachedBs.DisplayOrder = task.DisplayOrder;
            tm.PushBitstream(m_temporalUnitOrder, std::move(cachedBs));
            // Dont output hidden frame immediately
            task.BsDataLength = 0;
        }

        bool bNeedOutputFrame = (task.DisplayOrder == m_temporalUnitOrder)
            && ((!IsHiddenFrame(task) && task.BsDataLength > 0) || tm.IsCacheReady(m_temporalUnitOrder));
        if (bNeedOutputFrame)
        {
            mfxU32 cacheSize = tm.PeekCachedSize(m_temporalUnitOrder);
            MFX_CHECK(task.BsBytesAvailable >= task.BsDataLength + cacheSize, MFX_ERR_NOT_ENOUGH_BUFFER);

            mfxU32 offset = 0;
            if (cacheSize > 0)
            {
                auto& bss = tm.GetBitstreams(m_temporalUnitOrder);
                for (auto& bs : bss)
                {
                    std::copy_n(*bs.Data.get(), bs.BsDataLength, task.pBsData + offset);
                    offset += bs.BsDataLength;
                }
                tm.ClearBitstreams(m_temporalUnitOrder);
            }

            if (task.BsDataLength)
            {
                FrameLocker codedFrame(Glob::VideoCore::Get(global), task.BS.Mid);
                MFX_CHECK(codedFrame.Y, MFX_ERR_LOCK_MEMORY);
                sts = FastCopy::Copy(
                    task.pBsData + offset
                    , task.BsDataLength
                    , codedFrame.Y
                    , codedFrame.Pitch
                    , { int(task.BsDataLength), 1 }
                , COPY_VIDEO_TO_SYS);
            }
            task.BsDataLength += cacheSize;

            task.pBsOut->TimeStamp = task.pSurfIn ? task.pSurfIn->Data.TimeStamp : 0;
            task.BsBytesAvailable -= task.BsDataLength;
            *task.pBsDataLength   += task.BsDataLength;
        }

        MFX_CHECK_STS(sts);

        if (task.BsDataLength == 0)
        {
            task.pBsData = nullptr;
            task.pBsDataLength = nullptr;
            task.BsBytesAvailable = 0;

            task.SkipCMD &= ~SKIPCMD_NeedDriverCall;
            return MFX_TASK_WORKING;
        }
        else
        {
            ++m_temporalUnitOrder;
        }

        return MFX_ERR_NONE;
    });

    Push(BLK_UpdateBsInfo
        , [this](StorageW& /*global*/, StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);
        MFX_CHECK(task.BsDataLength > 0, MFX_ERR_NONE);

        auto& bs           = *task.pBsOut;
        bs.TimeStamp       = task.pSurfIn ? task.pSurfIn->Data.TimeStamp : 0;
        bs.DecodeTimeStamp = bs.TimeStamp;

        bs.PicStruct = task.pSurfIn ? task.pSurfIn->Info.PicStruct : 0;
        bs.FrameType = task.FrameType;
        bs.FrameType &= ~(task.isLDB * MFX_FRAMETYPE_B);
        bs.FrameType |= task.isLDB * MFX_FRAMETYPE_P;

        return MFX_ERR_NONE;
    });
}

inline bool ReleaseResource(IAllocation& a, Resource& r)
{
    if (r.Mid)
    {
        a.Release(r.Idx);
        r = Resource();
        return true;
    }

    return r.Idx == IDX_INVALID;
}

void General::FreeTask(const FeatureBlocks& /*blocks*/, TPushFT Push)
{
    Push(BLK_FreeTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);
        auto& core = Glob::VideoCore::Get(global);

        ThrowAssert(
            !ReleaseResource(Glob::AllocBS::Get(global), task.BS)
            , "task.BS resource is invalid");
        ThrowAssert(
            global.Contains(Glob::AllocRaw::Key)
            && !ReleaseResource(Glob::AllocRaw::Get(global), task.Raw)
            , "task.Raw resource is invalid");

        SetIf(task.pSurfIn, task.pSurfIn && !core.DecreaseReference(*task.pSurfIn), nullptr);
        ThrowAssert(!!task.pSurfIn, "failed in core.DecreaseReference");

        // In the future the logic might be changed to release recon based on refresh_frame_flag
        auto& atrRec = Glob::AllocRec::Get(global);
        ThrowAssert(
            !IsRef(task.FrameType)
            && !ReleaseResource(atrRec, task.Rec)
            , "task.Rec resource is invalid");

        task.DPB.clear();

        return MFX_ERR_NONE;
    });
}

void General::GetVideoParam(const FeatureBlocks& blocks, TPushGVP Push)
{
    Push(BLK_CopyConfigurable
        , [this, &blocks](mfxVideoParam& out, StorageR& global) -> mfxStatus
    {
        return CopyConfigurable(blocks, Glob::VideoParam::Get(global), out);
    });

    Push(BLK_FixParam
        , [this, &blocks](mfxVideoParam& out, StorageR& global) -> mfxStatus
    {
        out.mfx.LowPower = MFX_CODINGOPTION_ON;

        const bool isRAB = out.mfx.GopPicSize > 2 && out.mfx.GopRefDist > 1;
        if (isRAB)
        {
            auto defPar = GetRTDefaults(global);
            const mfxU32 numCacheFrames = (defPar.base.GetBRefType(defPar) != MFX_B_REF_PYRAMID) ? mfxU32(2)
                : mfxU32(defPar.base.GetNumBPyramidLayers(defPar)) + 1;
            BufferSizeInKB(out.mfx) = BufferSizeInKB(out.mfx) * numCacheFrames;
        }

        return MFX_ERR_NONE;
    });
}

void General::Close(const FeatureBlocks& blocks, TPushCLS Push)
{
    Push(BLK_Close
        , [this, &blocks](
            StorageW& /*global*/) -> mfxStatus
        {
            m_prevTask.DPB.clear();
            return MFX_ERR_NONE;
        });
}

using DpbIndexes = std::vector<mfxU8>;

static void RemoveRejected(
    const DpbType& dpb
    , DpbIndexes* dpbIndexes)
{
    dpbIndexes->erase(
        std::remove_if(
            dpbIndexes->begin()
            , dpbIndexes->end()
            , [&dpb](mfxU8 idx) { return dpb[idx]->isRejected; })
        , dpbIndexes->end());
}

static void FillSortedFwdBwd(
    const TaskCommonPar& task
    , DpbIndexes* fwd
    , DpbIndexes* bwd)
{
    if (!fwd && !bwd)
        return;

    using DisplayOrderToDPBIndex = std::map<mfxI32, mfxU8>;
    using Ref = DisplayOrderToDPBIndex::const_reference;
    auto GetIdx = [](Ref ref) {return ref.second; };
    auto IsBwd = [=](Ref ref) {return ref.first > task.DisplayOrderInGOP; };

    DisplayOrderToDPBIndex uniqueRefs;
    for (mfxU8 refIdx = 0; refIdx < task.DPB.size(); refIdx++)
    {
        auto& refFrm = task.DPB.at(refIdx);
        if (refFrm && refFrm->TemporalID <= task.TemporalID)
            uniqueRefs.insert({ refFrm->DisplayOrderInGOP, refIdx });
    }
    uniqueRefs.erase(task.DisplayOrderInGOP);

    auto firstBwd = find_if(uniqueRefs.begin(), uniqueRefs.end(), IsBwd);

    if (fwd)
    {
        std::transform(uniqueRefs.begin(), firstBwd, std::back_inserter(*fwd), GetIdx);
        RemoveRejected(task.DPB, fwd);
        // if all fwd references are rejected
        // use ref closest rejected to the current frame
        if (fwd->empty() && firstBwd != uniqueRefs.begin()) {
            auto lastFwd = std::prev(firstBwd);
            fwd->push_back(lastFwd->second);
        }
    }

    if (bwd)
    {
        std::transform(firstBwd, uniqueRefs.end(), std::back_inserter(*bwd), GetIdx);
        RemoveRejected(task.DPB, bwd);
        // if all bwd references are rejected
        // use ref closest rejected to the current frame
        if (bwd->empty() && firstBwd != uniqueRefs.end())
            bwd->push_back(firstBwd->second);
    }
}

namespace RefListRules
{
    template <typename Iter>
    using Rules = std::list<std::pair<size_t, Iter>>;

    template<typename Iter>
    class SafeIncrement
    {
        const Iter curr;
        const Iter end;
    public:
        SafeIncrement(const Iter& _curr, const Iter& _end) : curr(_curr), end(_end)
        {
            // the class is not intended to work with reference types
            assert(!std::is_reference<Iter>::value);
        };

        Iter operator+(size_t inc) const
        {
            const size_t remaining = std::distance(curr, end);
            const size_t safeInc = std::min(inc, remaining);
            Iter iter = curr;
            std::advance(iter, safeInc);
            return iter;
        }

        Iter operator()() const { return curr; }
    };

    template<typename Iter>
    static void CleanRules(Rules<Iter>& rules, const Iter toRemove, mfxU8 maxRefs)
    {
        auto NeedRemove = [&toRemove](const typename Rules<Iter>::value_type& rule) { return rule.second == toRemove; };
        rules.remove_if(NeedRemove);

        if (rules.size() > maxRefs)
            rules.resize(maxRefs);
    }

    template<typename Iter>
    static void ApplyRules(const Rules<Iter>& rules, RefListType& refList)
    {
        std::array<mfxU8, NUM_REF_FRAMES> usedDpbSlots = {};

        auto ApplyRule = [&](const typename Rules<Iter>::value_type& rule)
        {
            const mfxU8 dpbIdx = *rule.second;
            if (!usedDpbSlots.at(dpbIdx))
            {
                refList.at(rule.first - LAST_FRAME) = dpbIdx;
                usedDpbSlots.at(dpbIdx) = 1;
            }
        };

        for_each(rules.begin(), rules.end(), ApplyRule);
    }
}

static void FillFwdPart(const DpbIndexes& fwd, mfxU8 maxFwdRefs, RefListType& refList, bool isLdbFrame= false)
{
    // logic below is same as in original GetRefList implementation (for compatibility reasons)
    // In the future might consider improvement of this logic
    // e.g make it closer to logic from "7.8. Set frame refs process"
    using Iter = DpbIndexes::const_reverse_iterator;
    const RefListRules::SafeIncrement<Iter> closestRef{ fwd.crbegin() , fwd.crend() };
    RefListRules::Rules<Iter> constructionRules = {};

    if (isLdbFrame)
    {
        constructionRules = {
            {LAST_FRAME,   closestRef()},
            {LAST2_FRAME,  closestRef + 1},
            {LAST3_FRAME,  closestRef + 2},
        };
    }
    else
    {
        constructionRules = {
            {LAST_FRAME,   closestRef()},
            {GOLDEN_FRAME, closestRef + 1},
            {ALTREF_FRAME, closestRef + 2}
        };
    }

    const mfxU8 maxRefs = std::min(maxFwdRefs, static_cast<mfxU8>(fwd.size()));
    RefListRules::CleanRules(constructionRules, fwd.crend(), maxRefs);
    RefListRules::ApplyRules(constructionRules, refList);
}

static void FillRefListP(const DpbIndexes& fwd, mfxU8 maxFwdRefs, RefListType& refList)
{
    assert(!fwd.empty());

    FillFwdPart(fwd, maxFwdRefs, refList);
}

static void FillBwdPart(const DpbIndexes& bwd, mfxU8 maxBwdrefs, RefListType& refList)
{
    using Iter = DpbIndexes::const_iterator;

    RefListRules::Rules<Iter> constructionRules = {
        {BWDREF_FRAME,  bwd.cbegin()}
    };

    const mfxU8 maxRefs = std::min(maxBwdrefs, static_cast<mfxU8>(bwd.size()));
    RefListRules::CleanRules(constructionRules, bwd.cend(), maxRefs);
    RefListRules::ApplyRules(constructionRules, refList);
}

static void FillRefListRAB(
    const DpbIndexes& fwd
    , mfxU8 maxFwdRefs
    , const DpbIndexes& bwd
    , mfxU8 maxBwdRefs
    , RefListType& refList)
{
    assert(!fwd.empty());
    assert(!bwd.empty());

    FillFwdPart(fwd, maxFwdRefs, refList);
    FillBwdPart(bwd, maxBwdRefs, refList);
}

static void FillRefListLDB(const DpbIndexes& fwd, mfxU8 maxFwdRefs, RefListType& refList)
{
    assert(!fwd.empty());

    FillFwdPart(fwd, maxFwdRefs, refList, true);

    refList.at(BWDREF_FRAME - LAST_FRAME) = refList.at(LAST_FRAME - LAST_FRAME);
    refList.at(ALTREF2_FRAME - LAST_FRAME) = refList.at(LAST2_FRAME - LAST_FRAME);
    refList.at(ALTREF_FRAME - LAST_FRAME) = refList.at(LAST3_FRAME - LAST_FRAME);
}

inline std::tuple<mfxU8, mfxU8> GetMaxRefs(
    const TaskCommonPar& task
    , const mfxVideoParam& par)
{
    const mfxExtCodingOption3& CO3 = ExtBuffer::Get(par);

    const mfxU8 maxFwdRefs = static_cast<mfxU8>(IsB(task.FrameType) ?
        CO3.NumRefActiveBL0[0] : CO3.NumRefActiveP[0]);
    const mfxU8 maxBwdRefs = static_cast<mfxU8>(CO3.NumRefActiveBL1[0]);

    return std::make_tuple(maxFwdRefs, maxBwdRefs);
}

inline void SetTaskQp(
    TaskCommonPar& task
    , const mfxVideoParam& par)
{
    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        task.QpY = 128;
        return;
    }

    if (IsB(task.FrameType))
    {
        task.QpY = static_cast<mfxU8>(par.mfx.QPB);

        const mfxExtCodingOption2& CO2 = ExtBuffer::Get(par);
        const mfxExtCodingOption3& CO3 = ExtBuffer::Get(par);
        const bool bUseQPOffset        = IsOn(CO3.EnableQPOffset) && CO2.BRefType == MFX_B_REF_PYRAMID;
        if (bUseQPOffset)
        {
            task.QpY = static_cast<mfxU8>(mfx::clamp<mfxI32>(
                CO3.QPOffset[mfx::clamp<mfxI32>(task.PyramidLevel - 1, 0, 7)] + task.QpY
                , AV1_MIN_Q_INDEX, AV1_MAX_Q_INDEX));
        }
    }
    else if (IsP(task.FrameType))
        task.QpY = static_cast<mfxU8>(par.mfx.QPP);
    else
    {
        assert(IsI(task.FrameType));
        task.QpY = static_cast<mfxU8>(par.mfx.QPI);
    }

    SetIf(task.QpY, !!task.ctrl.QP, static_cast<mfxU8>(task.ctrl.QP));
}

inline void SetTaskBRCParams(
    TaskCommonPar& task
    , const mfxVideoParam& par)
{
    if(par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        return;

    const mfxExtAV1AuxData& auxPar = ExtBuffer::Get(par);
    task.MinBaseQIndex             = auxPar.QP.MinBaseQIndex;
    task.MaxBaseQIndex             = auxPar.QP.MaxBaseQIndex;
}

inline void SetTaskEncodeOrders(
    TaskCommonPar& task
    , const TaskCommonPar& prevTask)
{
    task.EncodedOrder = prevTask.EncodedOrder + 1;

    if (IsI(task.FrameType))
    {
        task.EncodedOrderInGOP = 0;
        task.RefOrderInGOP     = 0;
    }
    else
    {
        task.EncodedOrderInGOP = prevTask.EncodedOrderInGOP + 1;
        task.RefOrderInGOP = IsRef(task.FrameType) ?
            prevTask.RefOrderInGOP + 1 :
            prevTask.RefOrderInGOP;
    }
}

// task - [in/out] Current task object, task.DPB may be modified
// Return - N/A
inline void MarkLTR(TaskCommonPar& task)
{
    const mfxExtRefListCtrl* refListCtrl = ExtBuffer::Get(task.ctrl);
    if (!refListCtrl)
        return;

    // Count how many unique STR left in DPB so far
    // We need to keep at least 1 STR
    decltype(task.DPB) tmpDPB(task.DPB);
    std::ignore = std::unique(tmpDPB.begin(), tmpDPB.end());
    auto numberOfUniqueSTRs = std::count_if(
        tmpDPB.begin()
        , tmpDPB.end()
        , [](const DpbType::value_type& f) { return f && !f->isLTR; });

    const auto& ltrList = refListCtrl->LongTermRefList;
    for (mfxI32 i = 0; i < 16 && numberOfUniqueSTRs > 1; i++)
    {
        const mfxU32 ltrFrameOrder = ltrList[i].FrameOrder;
        if (ltrFrameOrder == static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN))
            continue;

        auto frameToBecomeLTR = std::find_if(
            task.DPB.begin()
            , task.DPB.end()
            , [ltrFrameOrder](const DpbType::value_type& f) { return f && f->DisplayOrder == ltrFrameOrder; });

        if (frameToBecomeLTR != task.DPB.end()
            && !(*frameToBecomeLTR)->isLTR
            && !(*frameToBecomeLTR)->isRejected)
        {
            (*frameToBecomeLTR)->isLTR = true;
            numberOfUniqueSTRs--;
        }
    }
}

// task - [in/out] Current task object, task.DPB may be modified
// Return - N/A
inline void MarkRejected(TaskCommonPar& task)
{
    const mfxExtRefListCtrl* refListCtrl = ExtBuffer::Get(task.ctrl);
    if (!refListCtrl)
        return;

    for (const auto& rejected : refListCtrl->RejectedRefList)
    {
        const mfxU32 rejectedFrameOrder = rejected.FrameOrder;
        if (rejectedFrameOrder == static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN))
            continue;

        // Frame with certain FrameOrder may be included into DPB muliple times and
        // if it is rejected we need to find all links to it from DPB.
        // Reference frames are refreshed in the end of encoding
        // so, here we only can mark rejected ref frames for further removal
        for (auto &f : task.DPB)
            if (f && f->DisplayOrder == rejectedFrameOrder)
                f->isRejected = true;
    }
}

inline void InitTaskDPB(
    TaskCommonPar& task,
    TaskCommonPar& prevTask)
{
    assert(task.DPB.size() >= prevTask.DPB.size());
    std::move(prevTask.DPB.begin(), prevTask.DPB.end(), task.DPB.begin());

    MarkRejected(task);
}

// task - [in/out] Current task object, RefList field will be set in place
// par - [in] mfxVideoParam
// Return - N/A
inline void SetTaskRefList(
    TaskCommonPar& task
    , const mfxVideoParam& par)
{
    auto& refList = task.RefList;
    std::fill_n(refList.begin(), REFS_PER_FRAME, IDX_INVALID);

    if (IsI(task.FrameType))
        return;

    DpbIndexes fwd;
    DpbIndexes bwd;
    FillSortedFwdBwd(task, &fwd, &bwd);

    mfxU8 maxFwdRefs = 0;
    mfxU8 maxBwdRefs = 0;
    std::tie(maxFwdRefs, maxBwdRefs) = GetMaxRefs(task, par);

    if (IsP(task.FrameType))
    {
        if (task.isLDB)
            FillRefListLDB(fwd, maxFwdRefs, refList);
        else
            FillRefListP(fwd, maxFwdRefs, refList);
    }
    else
        FillRefListRAB(fwd, maxFwdRefs, bwd, maxBwdRefs, refList);
}

template <typename DPBIter>
DPBIter FindOldestSTR(DPBIter dpbBegin, DPBIter dpbEnd, mfxU8 tid)
{
    DPBIter oldestSTR = dpbEnd;
    for (auto it = dpbBegin; it != dpbEnd; ++it)
    {
        if ((*it) && !(*it)->isLTR && (*it)->TemporalID >= tid)
        {
            if (oldestSTR == dpbEnd)
                oldestSTR = it;
            else if ((*oldestSTR)->DisplayOrder > (*it)->DisplayOrder)
                oldestSTR = it;
        }
    }
    return oldestSTR;
}

// task - [in/out] Current task object, RefreshFrameFlags field will be set in place
// Return - N/A
inline void SetTaskDPBRefresh(
    TaskCommonPar& task
    , const mfxVideoParam&)
{
    auto& refreshRefFrames = task.RefreshFrameFlags;

    if (IsI(task.FrameType))
        std::fill(refreshRefFrames.begin(), refreshRefFrames.end(), static_cast<mfxU8>(1));
    else if (IsRef(task.FrameType))
    {
        // At first find all rejected LTRs to refresh them with current frame
        mfxU8 refreshed = 0;
        for (size_t i = 0; i < task.DPB.size(); i++)
            if (task.DPB[i]->isRejected)
                refreshed = refreshRefFrames.at(i) = 1;

        if (!refreshed)
        {
            auto dpbBegin = task.DPB.begin();
            auto dpbEnd = task.DPB.end();
            auto slotToRefresh = dpbEnd;

            // If no LTR was refreshed, then find duplicate reference frame
            // Some frames can be included multiple times into DPB
            for (auto it = dpbBegin + 1; it < dpbEnd && slotToRefresh == dpbEnd; ++it)
                if (std::find(dpbBegin, it, *it) != it)
                    slotToRefresh = it;

            // If no duplicates, then find the oldest STR
            // For temporal scalability frame must not overwrite frames from lower layers
            if (slotToRefresh == dpbEnd)
                slotToRefresh = FindOldestSTR(dpbBegin, dpbEnd, task.TemporalID);

            // If failed, just do not refresh any reference frame.
            // This should not happend since we maintain at least 1 STR in DPB
            if (slotToRefresh != dpbEnd)
                refreshRefFrames.at(slotToRefresh - dpbBegin) = 1;
        }
    }
}

class DpbFrameReleaser
{
    IAllocation& pool;
public:
    DpbFrameReleaser(IAllocation& _pool) : pool(_pool) {};
    void operator()(DpbFrame* pFrm)
    {
        ReleaseResource(pool, pFrm->Rec);
        delete pFrm;
    }
};

inline void SetTaskIVFHeaderInsert(
    TaskCommonPar& task
    , const TaskCommonPar& prevTask
    , bool& insertIVFSeq)
{
    if (IsHiddenFrame(prevTask))
        return;

    if (insertIVFSeq)
    {
        task.InsertHeaders |= INSERT_IVF_SEQ;
        insertIVFSeq        = false;
    }

    task.InsertHeaders |= INSERT_IVF_FRM;
}

inline void SetTaskTDHeaderInsert(
    TaskCommonPar& task
    , const TaskCommonPar& prevTask
    , const mfxVideoParam& par)
{
    if (IsHiddenFrame(prevTask))
        return;

    const mfxExtAV1AuxData& auxPar = ExtBuffer::Get(par);
    if (IsOn(auxPar.InsertTemporalDelimiter))
    {
        task.InsertHeaders |= INSERT_TD;
        return;
    }

    const mfxExtTemporalLayers& TL  = ExtBuffer::Get(par);
    const mfxU16 operPointCntMinus1 = CountTL(TL) - 1;
    if (operPointCntMinus1)
    {
        task.InsertHeaders |= INSERT_TD;
    }
}

inline void SetTaskInsertHeaders(
    TaskCommonPar& task
    , const TaskCommonPar& prevTask
    , const mfxVideoParam& par
    , bool& insertIVFSeq)
{
    const mfxExtAV1BitstreamParam& bsPar = ExtBuffer::Get(par);
    if (IsOn(bsPar.WriteIVFHeaders))
        SetTaskIVFHeaderInsert(task, prevTask, insertIVFSeq);

    SetTaskTDHeaderInsert(task, prevTask, par);

    if (IsI(task.FrameType))
        task.InsertHeaders |= INSERT_SPS;

    task.InsertHeaders |= INSERT_PPS;

    const mfxExtAV1AuxData& auxPar = ExtBuffer::Get(par);
    if (IsOn(auxPar.PackOBUFrame))
        task.InsertHeaders |= INSERT_FRM_OBU;
}

inline void SetTaskTCBRC(
    TaskCommonPar& task
    , const mfxVideoParam& par)
{
    ThrowAssert(par.mfx.FrameInfo.FrameRateExtD == 0, "FrameRateExtD = 0");

    mfxU32 avgFrameSizeInBytes = GetAvgFrameSizeInBytes(par);
    task.TCBRCTargetFrameSize  = avgFrameSizeInBytes;
}

void General::ConfigureTask(
    TaskCommonPar& task
    , const Defaults::Param& dflts
    , IAllocation& recPool)
{
    task.StatusReportId = std::max<mfxU32>(1, m_prevTask.StatusReportId + 1);

    const auto& par = dflts.mvp;
    SetTaskQp(task, par);
    SetTaskBRCParams(task, par);
    SetTaskEncodeOrders(task, m_prevTask);

    InitTaskDPB(task, m_prevTask);

    SetTaskRefList(task, par);
    SetTaskDPBRefresh(task, par);
    SetTaskInsertHeaders(task, m_prevTask, par, m_insertIVFSeq);

    const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
    if (pCO3 && IsOn(pCO3->LowDelayBRC) && task.TCBRCTargetFrameSize == 0)
    {
        SetTaskTCBRC(task, par);
    }

    m_prevTask = task;

    UpdateDPB(m_prevTask.DPB, reinterpret_cast<DpbFrame&>(task), task.RefreshFrameFlags, DpbFrameReleaser(recPool));
    MarkLTR(m_prevTask);
}

static bool HaveL1(DpbType const & dpb, mfxI32 displayOrderInGOP)
{
    return std::any_of(dpb.begin(), dpb.end(),
        [displayOrderInGOP](DpbType::const_reference frm)
        {
            if (frm)
                return frm->DisplayOrderInGOP > displayOrderInGOP;
            else
                return false;
        });
}

static mfxU32 GetEncodingOrder(
    mfxU32 displayOrder
    , mfxU32 begin
    , mfxU32 end
    , mfxU32 &level
    , mfxU32 before
    , bool & ref)
{
    assert(displayOrder >= begin);
    assert(displayOrder <  end);

    ref = (end - begin > 1);

    mfxU32 pivot = (begin + end) / 2;
    if (displayOrder == pivot)
        return level + before;

    level++;
    if (displayOrder < pivot)
        return GetEncodingOrder(displayOrder, begin, pivot, level, before, ref);
    else
        return GetEncodingOrder(displayOrder, pivot + 1, end, level, before + pivot - begin, ref);
}

static mfxU32 GetBiFrameLocation(mfxU32 i, mfxU32 num, bool &ref, mfxU32 &level)
{
    ref = false;
    level = 1;
    return GetEncodingOrder(i, 0, num, level, 0, ref);
}

template<class T>
static T BPyrReorder(T begin, T end)
{
    typedef typename std::iterator_traits<T>::reference TRef;

    mfxU32 num = mfxU32(std::distance(begin, end));
    bool bSetOrder = num && (*begin)->BPyramidOrder == mfxU32(MFX_FRAMEORDER_UNKNOWN);

    if (bSetOrder)
    {
        mfxU32 i = 0;
        std::for_each(begin, end, [&](TRef bref)
        {
            bool bRef = false;
            bref->BPyramidOrder = GetBiFrameLocation(i++, num, bRef, bref->PyramidLevel);
            bref->FrameType |= mfxU16(MFX_FRAMETYPE_REF * bRef);
        });
    }

    return std::min_element(begin, end
        , [](TRef a, TRef b) { return a->BPyramidOrder < b->BPyramidOrder; });
}

template<class T>
static T Reorder(
    ExtBuffer::Param<mfxVideoParam> const & par
    , DpbType const & dpb
    , T begin
    , T end
    , bool flush)
{
    typedef typename std::iterator_traits<T>::reference TRef;

    const mfxExtCodingOption2& CO2        = ExtBuffer::Get(par);
    const bool                 isBPyramid = (CO2.BRefType == MFX_B_REF_PYRAMID);

    T top        = begin;
    T reorderOut = top;
    std::list<T> brefs;
    auto IsB  = [](TRef f) { return AV1EHW::IsB(f.FrameType); };
    auto NoL1 = [&](T& f) { return !HaveL1(dpb, f->DisplayOrderInGOP); };

    std::generate_n(
        std::back_inserter(brefs)
        , std::distance(begin, std::find_if_not(begin, end, IsB))
        , [&]() { return top++; });

    brefs.remove_if(NoL1);

    if (!brefs.empty())
    {
        if (!isBPyramid)
        {
            const auto B0POC = brefs.front()->DisplayOrderInGOP;
            auto       it    = brefs.begin();
            while (it != brefs.end())
            {
                if (IsRef((*it)->FrameType) && ((*it)->DisplayOrderInGOP - B0POC < 2))
                    break;
                ++it;
            }

            if (it == brefs.end())
                it = brefs.begin();

            reorderOut = *it;
        }
        else
        {
            reorderOut = *BPyrReorder(brefs.begin(), brefs.end());
        }
    }
    else
    {
        // optimize end of GOP or end of sequence
        const bool bForcePRef = flush && top == end && begin != end;
        if (bForcePRef)
        {
            --top;
            top->FrameType = mfxU16(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);
        }
        reorderOut = top;
    }

    return reorderOut;
}

TTaskIt General::ReorderWrap(
    ExtBuffer::Param<mfxVideoParam> const & par
    , TTaskIt begin
    , TTaskIt end
    , bool flush)
{
    typedef TaskItWrap<FrameBaseInfo, Task::Common::Key> TItWrap;
    return Reorder(par, m_prevTask.DPB, TItWrap(begin), TItWrap(end), flush).it;
}

mfxU32 General::GetMinBsSize(
    const mfxVideoParam & par
    , const mfxExtAV1ResolutionParam& rsPar
    , const mfxExtCodingOption3& CO3)
{
    mfxU32 size = rsPar.FrameWidth * rsPar.FrameHeight;

    SetDefault(size, par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height);

    bool b10bit = (CO3.TargetBitDepthLuma == 10);
    bool b422   = (CO3.TargetChromaFormatPlus1 == (MFX_CHROMAFORMAT_YUV422 + 1));
    bool b444   = (CO3.TargetChromaFormatPlus1 == (MFX_CHROMAFORMAT_YUV444 + 1));

    mfxF64 k = 2.0
        + (b10bit * 0.3)
        + (b422   * 0.5)
        + (b444   * 1.5);

    size = mfxU32(k * size);

    return size;
}

bool General::GetRecInfo(
    const mfxVideoParam& par
    , const mfxExtCodingOption3& CO3
    , eMFXHWType hw
    , mfxFrameInfo& rec)
{
    static const std::map<mfxU16, std::function<void(mfxFrameInfo&, eMFXHWType)>> ModRec[2] =
    {
        { //8b
            {
                mfxU16(1 + MFX_CHROMAFORMAT_YUV420)
                , [](mfxFrameInfo& rec, eMFXHWType)
                {
                    rec.FourCC = MFX_FOURCC_NV12;
                }
            }
        }
        , { //10b
            {
                mfxU16(1 + MFX_CHROMAFORMAT_YUV420)
                , [](mfxFrameInfo& rec, eMFXHWType)
                {
                    //P010
                    rec.FourCC = MFX_FOURCC_NV12;
                    rec.Width = mfx::align2_value(rec.Width, 32) * 2; //This is require by HW and MMC, which is same as TGL.
                }
            }
        }
    };
    rec = par.mfx.FrameInfo;

    auto& rModRec  = ModRec[CO3.TargetBitDepthLuma == 10];
    auto  itModRec = rModRec.find(CO3.TargetChromaFormatPlus1);
    bool bUndef =
        (CO3.TargetBitDepthLuma != 8 && CO3.TargetBitDepthLuma != 10)
        || (itModRec == rModRec.end());

    if (bUndef)
    {
        assert(!"undefined target format");
        return false;
    }

    itModRec->second(rec, hw);

    rec.ChromaFormat   = CO3.TargetChromaFormatPlus1 - 1;
    rec.BitDepthLuma   = CO3.TargetBitDepthLuma;
    rec.BitDepthChroma = CO3.TargetBitDepthChroma;

    return true;
}

inline mfxU16 MapMfxProfileToSpec(mfxU16 profile)
{
    switch (profile)
    {
    case MFX_PROFILE_AV1_MAIN:
        return 0;
    case MFX_PROFILE_AV1_HIGH:
        return 1;
    case MFX_PROFILE_AV1_PRO:
        return 2;
    default:
        return 0;
    }
}

inline mfxU16 MapMfxLevelToSpec(mfxU16 codecLevel)
{
    return ((((codecLevel / 10) - 2) << 2) + (codecLevel % 10));
}

void General::SetSH(
    const ExtBuffer::Param<mfxVideoParam>& par
    , eMFXHWType /*hw*/
    , const EncodeCapsAv1& caps
    , SH& sh)
{
    sh = {};

    const mfxExtAV1AuxData& auxPar = ExtBuffer::Get(par);
    sh.seq_profile   = MapMfxProfileToSpec(par.mfx.CodecProfile);
    sh.still_picture = IsOn(auxPar.StillPictureMode);

    const int maxFrameResolutionBits = 15;
    sh.frame_width_bits       = maxFrameResolutionBits;
    sh.frame_height_bits      = maxFrameResolutionBits;
    sh.sbSize                 = SB_SIZE;
    sh.enable_order_hint      = CO2Flag(auxPar.EnableOrderHint);
    sh.order_hint_bits_minus1 = auxPar.OrderHintBits - 1;
    sh.enable_cdef            = CO2Flag(auxPar.EnableCdef);
    sh.enable_restoration     = CO2Flag(auxPar.EnableRestoration);

    // Below fields will directly use setting from caps.
    sh.enable_dual_filter         = caps.AV1ToolSupportFlags.fields.enable_dual_filter;
    sh.enable_filter_intra        = caps.AV1ToolSupportFlags.fields.enable_filter_intra;
    sh.enable_interintra_compound = caps.AV1ToolSupportFlags.fields.enable_interintra_compound;
    sh.enable_intra_edge_filter   = caps.AV1ToolSupportFlags.fields.enable_intra_edge_filter;
    sh.enable_jnt_comp            = caps.AV1ToolSupportFlags.fields.enable_jnt_comp;
    sh.enable_masked_compound     = caps.AV1ToolSupportFlags.fields.enable_masked_compound;

    const mfxExtCodingOption3& CO3      = ExtBuffer::Get(par);
    sh.color_config.BitDepth            = CO3.TargetBitDepthLuma;
    sh.color_config.color_range         = 1; // full swing representation
    sh.color_config.separate_uv_delta_q = 1; // NB: currently driver not work if it's '0'
    sh.color_config.subsampling_x       = 1; // YUV 4:2:0
    sh.color_config.subsampling_y       = 1; // YUV 4:2:0

    const mfxExtVideoSignalInfo& VSI               = ExtBuffer::Get(par);
    sh.color_config.color_range                    = VSI.VideoFullRange;
    sh.color_config.color_description_present_flag = VSI.ColourDescriptionPresent;
    sh.color_config.color_primaries                = VSI.ColourPrimaries;
    sh.color_config.transfer_characteristics       = VSI.TransferCharacteristics;
    sh.color_config.matrix_coefficients            = VSI.MatrixCoefficients;

    const mfxExtTemporalLayers& TL  = ExtBuffer::Get(par);
    sh.operating_points_cnt_minus_1 = CountTL(TL) - 1;
    sh.seq_level_idx[0]             = MapMfxLevelToSpec(par.mfx.CodecLevel);
    if (par.mfx.CodecLevel >= MFX_LEVEL_AV1_4
        && MaxKbps(par.mfx) > GetMaxKbpsByLevel(par.mfx.CodecLevel, par.mfx.CodecProfile))
    {
        sh.seq_tier[0] = 1;
    }
    else
    {
        sh.seq_tier[0] = 0;
    }

    if (sh.operating_points_cnt_minus_1)
    {
        // (1) Only temporal scalability is supported
        // (2) Set operating_point_idc[] in the same way as reference AOM
        // to decode by aomdec bitstreams w/ temporal scalability reset enabled
        // It means that the i=0 point corresponds to the
        // highest quality operating point(all layers), and subsequent
        // operarting points (i > 0) are lower quality corresponding to
        // skip decoding enhancement layers
        for (mfxU8 i = 0; i <= sh.operating_points_cnt_minus_1; i++)
        {
            sh.operating_point_idc[i] = (1u << 8) | ~(~0u << (sh.operating_points_cnt_minus_1 + 1 - i));
            sh.seq_level_idx[i]       = sh.seq_level_idx[0];
            sh.seq_tier[i]            = sh.seq_tier[0];
        }
    }
}

inline INTERP_FILTER MapMfxInterpFilter(mfxU16 filter)
{
    switch (filter)
    {
    case MFX_AV1_INTERP_EIGHTTAP_SMOOTH:
        return EIGHTTAP_SMOOTH;
    case MFX_AV1_INTERP_EIGHTTAP_SHARP:
        return EIGHTTAP_SHARP;
    case MFX_AV1_INTERP_BILINEAR:
        return BILINEAR;
    case MFX_AV1_INTERP_SWITCHABLE:
        return SWITCHABLE;
    case MFX_AV1_INTERP_EIGHTTAP:
    default:
        return EIGHTTAP_REGULAR;
    }
}

void General::SetFH(
    const ExtBuffer::Param<mfxVideoParam>& par
    , eMFXHWType /*hw*/
    , const SH& sh
    , FH& fh)
{
    // this functions sets "static" parameters which can be changed via Reset
    fh = {};

    const mfxExtAV1ResolutionParam& rsPar  = ExtBuffer::Get(par);
    const mfxExtAV1AuxData&         auxPar = ExtBuffer::Get(par);

    fh.FrameWidth                    = GetActualEncodeWidth(rsPar.FrameWidth, &auxPar);
    fh.FrameHeight                   = rsPar.FrameHeight;
    fh.UpscaledWidth                 = rsPar.FrameWidth;
    fh.error_resilient_mode          = CO2Flag(auxPar.ErrorResilientMode);
    fh.disable_cdf_update            = CO2Flag(auxPar.DisableCdfUpdate);
    fh.interpolation_filter          = MapMfxInterpFilter(auxPar.InterpFilter);
    fh.RenderWidth                   = rsPar.FrameWidth;
    fh.RenderHeight                  = rsPar.FrameHeight;
    fh.disable_frame_end_update_cdf  = CO2Flag(auxPar.DisableFrameEndUpdateCdf);
    fh.allow_high_precision_mv       = 0;
    fh.skip_mode_present             = 0;

    fh.quantization_params.DeltaQYDc = auxPar.QP.YDcDeltaQ;
    fh.quantization_params.DeltaQUDc = auxPar.QP.UDcDeltaQ;
    fh.quantization_params.DeltaQUAc = auxPar.QP.UAcDeltaQ;
    fh.quantization_params.DeltaQVDc = auxPar.QP.VDcDeltaQ;
    fh.quantization_params.DeltaQVAc = auxPar.QP.VAcDeltaQ;

    // Loop Filter params
    if (IsOn(auxPar.EnableLoopFilter))
    {
        fh.loop_filter_params.loop_filter_sharpness     = auxPar.LoopFilterSharpness;
        fh.loop_filter_params.loop_filter_delta_enabled = auxPar.LoopFilter.ModeRefDeltaEnabled;
        fh.loop_filter_params.loop_filter_delta_update  = auxPar.LoopFilter.ModeRefDeltaUpdate;

        std::copy_n(auxPar.LoopFilter.RefDeltas, TOTAL_REFS_PER_FRAME, fh.loop_filter_params.loop_filter_ref_deltas);
        std::copy_n(auxPar.LoopFilter.ModeDeltas, MAX_MODE_LF_DELTAS, fh.loop_filter_params.loop_filter_mode_deltas);
    }

    if (sh.enable_restoration)
    {
        for (mfxU8 i = 0; i < MAX_MB_PLANE; i++)
        {
            fh.lr_params.lr_type[i] = RESTORE_WIENER;
        }
        fh.lr_params.lr_unit_shift = 0;
        fh.lr_params.lr_unit_extra_shift = 0;
        if (sh.color_config.subsampling_x && sh.color_config.subsampling_y)
        {
            fh.lr_params.lr_uv_shift = 1;
        }
        else
        {
            fh.lr_params.lr_uv_shift = 0;
        }
    }

    fh.TxMode = TX_MODE_SELECT;
    fh.reduced_tx_set = 1;
    fh.delta_lf_present = 1;
    fh.delta_lf_multi = 1;

    fh.quantization_params.using_qmatrix = 0;
    fh.quantization_params.qm_y = 15;
    fh.quantization_params.qm_u = 15;
    fh.quantization_params.qm_v = 15;
}

void SetDefaultFormat(
    mfxVideoParam& par
    , const Defaults::Param& defPar
    , mfxExtCodingOption3* pCO3)
{
    auto& fi = par.mfx.FrameInfo;

    assert(fi.FourCC);

    SetDefault(fi.BitDepthLuma, [&]() { return defPar.base.GetBitDepthLuma(defPar); });
    SetDefault(fi.BitDepthChroma, fi.BitDepthLuma);

    if (pCO3)
    {
        pCO3->TargetChromaFormatPlus1 = defPar.base.GetTargetChromaFormatPlus1(defPar);
        pCO3->TargetBitDepthLuma = defPar.base.GetTargetBitDepthLuma(defPar);
        SetDefault(pCO3->TargetBitDepthChroma, pCO3->TargetBitDepthLuma);
    }
}

void SetDefaultSize(
    mfxVideoParam & par
    , const Defaults::Param& defPar
    , mfxExtAV1ResolutionParam* pRsPar)
{
    auto& fi = par.mfx.FrameInfo;

    SetDefault(fi.CropW, fi.Width);
    SetDefault(fi.CropH, fi.Height);

    if (pRsPar != nullptr)
    {
        SetDefaultFrameInfo(pRsPar->FrameWidth, pRsPar->FrameHeight, fi);
    }

    SetDefault(fi.AspectRatioW, mfxU16(1));
    SetDefault(fi.AspectRatioH, mfxU16(1));

    std::tie(fi.FrameRateExtN, fi.FrameRateExtD) = defPar.base.GetFrameRate(defPar);
}

void SetDefaultGOP(
    mfxVideoParam& par
    , const Defaults::Param& defPar
    , mfxExtCodingOption2* pCO2
    , mfxExtCodingOption3* pCO3)
{
    par.mfx.GopPicSize = defPar.base.GetGopPicSize(defPar);
    par.mfx.GopRefDist = defPar.base.GetGopRefDist(defPar);

    SetDefault(par.mfx.NumRefFrame, defPar.base.GetNumRefFrames(defPar));

    if (pCO2 != nullptr)
    {
        SetIf(pCO2->BRefType, !pCO2->BRefType, [&]() { return defPar.base.GetBRefType(defPar); });
    }

    if (pCO3 != nullptr)
    {
        SetIf(pCO3->PRefType, !pCO3->PRefType, [&]() { return defPar.base.GetPRefType(defPar); });
        
        // change default to LDB when RAB
        if (par.mfx.GopPicSize > 2 && par.mfx.GopRefDist > 1)
            SetDefault<mfxU16>(pCO3->GPB, MFX_CODINGOPTION_ON);
        else
            SetDefault<mfxU16>(pCO3->GPB, MFX_CODINGOPTION_OFF);

        defPar.base.GetNumRefActive(
            defPar
            , &pCO3->NumRefActiveP
            , &pCO3->NumRefActiveBL0
            , &pCO3->NumRefActiveBL1);
    }
}

void SetDefaultBRC(
    mfxVideoParam& par
    , const Defaults::Param& defPar
    , mfxExtCodingOption2* pCO2
    , mfxExtCodingOption3* pCO3)
{
    SetDefault(par.mfx.BRCParamMultiplier, 1);

    par.mfx.RateControlMethod = defPar.base.GetRateControlMethod(defPar);
    BufferSizeInKB(par.mfx)   = defPar.base.GetBufferSizeInKB(defPar);

    if(pCO2)
        pCO2->MBBRC = defPar.base.GetMBBRC(defPar);

    bool bSetQP = par.mfx.RateControlMethod == MFX_RATECONTROL_CQP
        && !(par.mfx.QPI && par.mfx.QPP && par.mfx.QPB);
    bool bSetRCPar = (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR);
    bool bSetICQ  = (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ);

    if (bSetQP)
    {
        std::tie(par.mfx.QPI, par.mfx.QPP, par.mfx.QPB) = defPar.base.GetQPMFX(defPar);
    }

    if (bSetRCPar)
    {
        MaxKbps(par.mfx) = defPar.base.GetMaxKbps(defPar);
        SetDefault<mfxU16>(par.mfx.InitialDelayInKB, par.mfx.BufferSizeInKB / 2);
    }
    SetDefault(par.mfx.BRCParamMultiplier, 1);

    if (bSetICQ)
    {
        SetDefault<mfxU16>(par.mfx.ICQQuality, 26);
    }

    mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
    if (pAuxPar && par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        SetDefault(pAuxPar->QP.MinBaseQIndex, AV1_MIN_Q_INDEX);
        SetDefault(pAuxPar->QP.MaxBaseQIndex, AV1_MAX_Q_INDEX);
    }

    if (pCO3)
    {
        defPar.base.GetQPOffset(defPar, pCO3->EnableQPOffset, pCO3->QPOffset);
        SetDefault(pCO3->LowDelayBRC, MFX_CODINGOPTION_OFF);
    }

    if(pCO3 && IsOn(pCO3->LowDelayBRC))
    {
        SetDefault<mfxU16>(pCO3->ScenarioInfo, MFX_SCENARIO_REMOTE_GAMING);
    }
}

inline void SetDefaultOrderHint(mfxExtAV1AuxData* par)
{
    if (!par)
        return;

    SetDefault(par->EnableOrderHint, MFX_CODINGOPTION_ON);
    if (IsOn(par->EnableOrderHint))
    {
        SetDefault(par->OrderHintBits, 8);
    }
}

void General::SetDefaults(
    mfxVideoParam& par
    , const Defaults::Param& defPar
    , bool bExternalFrameAllocator)
{
    auto GetDefaultLevel = [&]()
    {
        return GetMinLevel(defPar, MFX_LEVEL_AV1_2);
    };

    SetDefault(par.mfx.LowPower, MFX_CODINGOPTION_ON);
    SetDefault(par.AsyncDepth, defPar.base.GetAsyncDepth(defPar));

    const mfxU16 IOPByAlctr[2] = { MFX_IOPATTERN_IN_SYSTEM_MEMORY, MFX_IOPATTERN_IN_VIDEO_MEMORY };
    SetDefault(par.IOPattern, IOPByAlctr[!!bExternalFrameAllocator]);
    SetDefault(par.mfx.TargetUsage, DEFAULT_TARGET_USAGE);
    SetDefault(par.mfx.NumThread, 1);

    mfxExtAV1ResolutionParam* pRsPar = ExtBuffer::Get(par);
    mfxExtCodingOption2*      pCO2   = ExtBuffer::Get(par);
    mfxExtCodingOption3*      pCO3   = ExtBuffer::Get(par);
    SetDefaultSize(par, defPar, pRsPar);
    SetDefaultGOP(par, defPar, pCO2, pCO3);
    SetDefaultBRC(par, defPar, pCO2, pCO3);

    SetDefault(par.mfx.CodecProfile, defPar.base.GetProfile(defPar));
    SetDefault(par.mfx.CodecLevel, GetDefaultLevel);

    mfxExtAV1BitstreamParam* pBsPar = ExtBuffer::Get(par);
    if (pBsPar != nullptr)
    {
        SetDefault(pBsPar->WriteIVFHeaders, MFX_CODINGOPTION_OFF);
    }

    SetDefault(par.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE);

    SetDefaultFormat(par, defPar, pCO3);

    mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
    if (pAuxPar)
    {
        SetDefault(pAuxPar->StillPictureMode, MFX_CODINGOPTION_OFF);
        SetDefault(pAuxPar->UseAnnexB, MFX_CODINGOPTION_OFF);
        SetDefault(pAuxPar->PackOBUFrame, MFX_CODINGOPTION_ON);
        SetDefault(pAuxPar->InsertTemporalDelimiter, MFX_CODINGOPTION_ON);
        SetDefault(pAuxPar->EnableCdef, MFX_CODINGOPTION_ON);
        SetDefault(pAuxPar->EnableRestoration, MFX_CODINGOPTION_OFF);
        SetDefault(pAuxPar->EnableLoopFilter, MFX_CODINGOPTION_ON);
        SetDefault(pAuxPar->InterpFilter, MFX_AV1_INTERP_EIGHTTAP);
        SetDefault(pAuxPar->DisableCdfUpdate, MFX_CODINGOPTION_OFF);
        SetDefault(pAuxPar->DisableFrameEndUpdateCdf, MFX_CODINGOPTION_OFF);
        SetDefault(pAuxPar->LoopFilter.ModeRefDeltaEnabled, MFX_CODINGOPTION_OFF);
        SetDefault(pAuxPar->LoopFilter.ModeRefDeltaUpdate, MFX_CODINGOPTION_OFF);
        SetDefault(pAuxPar->DisplayFormatSwizzle, MFX_CODINGOPTION_OFF);
        SetDefault(pAuxPar->ErrorResilientMode, MFX_CODINGOPTION_OFF);
    }

    SetDefaultOrderHint(pAuxPar);

    mfxExtTemporalLayers* pTemporalLayers = ExtBuffer::Get(par);
    if (pTemporalLayers && pTemporalLayers->NumLayers && pTemporalLayers->Layers)
    {
        SetDefault(pTemporalLayers->Layers[0].FrameRateScale, mfxU16(1));
    }

}

mfxStatus General::CheckNumRefFrame(
    mfxVideoParam & par
    , const Defaults::Param& defPar)
{
    MFX_CHECK(par.mfx.NumRefFrame, MFX_ERR_NONE);

    mfxU32 changed = 0;

    changed += CheckMaxOrClip(par.mfx.NumRefFrame, NUM_REF_FRAMES);

    if (defPar.base.GetBRefType(defPar) == MFX_B_REF_PYRAMID)
    {
        mfxU32 minNumRefFrame = defPar.base.GetNumRefBPyramid(defPar);
        changed += CheckMinOrClip(par.mfx.NumRefFrame, minNumRefFrame);
    }

    changed += SetIf(
        par.mfx.NumRefFrame
        , (par.mfx.GopRefDist > 1
            && par.mfx.NumRefFrame == 1
            && !defPar.base.GetNonStdReordering(defPar))
        , defPar.base.GetMinRefForBNoPyramid(defPar));

    mfxU16 NumTL = defPar.base.GetNumTemporalLayers(defPar);
    changed += SetIf(
        par.mfx.NumRefFrame
        , (NumTL > 1
            && par.mfx.NumRefFrame < NumTL - 1)
        , NumTL - 1);

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    return MFX_ERR_NONE;
}

mfxStatus General::CheckFrameRate(mfxVideoParam & par)
{
    auto& fi = par.mfx.FrameInfo;

    if (fi.FrameRateExtN && fi.FrameRateExtD) // FR <= 300
    {
        if (fi.FrameRateExtN > mfxU32(300 * fi.FrameRateExtD))
        {
            fi.FrameRateExtN = fi.FrameRateExtD = 0;
            MFX_RETURN(MFX_ERR_UNSUPPORTED);
        }
    }

    if ((fi.FrameRateExtN == 0) != (fi.FrameRateExtD == 0))
    {
        fi.FrameRateExtN = 0;
        fi.FrameRateExtD = 0;
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    return MFX_ERR_NONE;
}

bool General::IsInVideoMem(const mfxVideoParam & par)
{
    if (par.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY)
        return true;

    return false;
}

mfxStatus General::CheckShift(mfxVideoParam & par)
{
    auto& fi = par.mfx.FrameInfo;
    bool bVideoMem = IsInVideoMem(par);

    if (bVideoMem && !fi.Shift)
    {
        if (fi.FourCC == MFX_FOURCC_P010 || fi.FourCC == MFX_FOURCC_P210)
        {
            fi.Shift = 1;
            return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus General::CheckCrops(
    mfxVideoParam & par
    , const Defaults::Param& /*defPar*/)
{
    auto& fi = par.mfx.FrameInfo;

    mfxU32 invalid = 0;
    invalid += CheckOrZero<mfxU16>(fi.CropX, 0);
    invalid += CheckOrZero<mfxU16>(fi.CropY, 0);
    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    mfxU32 changed = 0;
    changed += CheckMaxOrClip(fi.CropW, fi.Width);
    changed += CheckMaxOrClip(fi.CropH, fi.Height);
    const mfxExtAV1ResolutionParam* pRsPar = ExtBuffer::Get(par);
    if (pRsPar)
    {
        changed += pRsPar->FrameWidth > 0 && CheckMaxOrClip(fi.CropW, pRsPar->FrameWidth);
        changed += pRsPar->FrameHeight > 0 && CheckMaxOrClip(fi.CropH, pRsPar->FrameHeight);
    }

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

mfxU32 CheckBufferSizeInKB(mfxVideoParam& par, const Defaults::Param& defPar)
{
    mfxU32 changed = 0;
    if (!par.mfx.BufferSizeInKB)
        return changed;

    mfxU32     minSizeInKB   = 0;
    const bool bCqpOrIcq     = par.mfx.RateControlMethod == MFX_RATECONTROL_CQP
        || par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ;
    if (bCqpOrIcq)
    {
        minSizeInKB = General::GetRawBytes(defPar) / 1000;
    }
    else
    {
        mfxU32 frN          = 0;
        mfxU32 frD          = 0;
        std::tie(frN, frD)  = defPar.base.GetFrameRate(defPar);
        mfxU32 avgFrameSize = Ceil((mfxF64)TargetKbps(par.mfx) * frD / frN / 8);
        minSizeInKB         = avgFrameSize * 2 + 1;
    }

    if (BufferSizeInKB(par.mfx) < minSizeInKB)
    {
        BufferSizeInKB(par.mfx) = minSizeInKB;
        changed++;
    }

    return changed;
}

inline mfxU32 CheckAndFixQP(mfxU16& qp, const mfxU16 minQP, const mfxU16 maxQP)
{
    mfxU32 changed = 0;

    if (qp)
    {
        changed += CheckMinOrClip<mfxU16>(qp, minQP);
        changed += CheckMaxOrClip<mfxU16>(qp, maxQP);
    }

    return changed;
}

mfxStatus General::CheckRateControl(
    mfxVideoParam& par
    , const Defaults::Param& defPar)
{
    mfxU32 changed = 0;
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        par.mfx.Accuracy = 0;
        par.mfx.Convergence = 0;
        changed++;
    }

    auto RateControlMethod = defPar.base.GetRateControlMethod(defPar);
    bool bSupported = !CheckOrZero<mfxU16>(RateControlMethod
        , !!defPar.caps.msdk.CBRSupport * MFX_RATECONTROL_CBR
        , !!defPar.caps.msdk.VBRSupport * MFX_RATECONTROL_VBR
        , !!defPar.caps.msdk.CQPSupport * MFX_RATECONTROL_CQP
        , !!defPar.caps.msdk.ICQSupport * MFX_RATECONTROL_ICQ
        );
    MFX_CHECK(bSupported, MFX_ERR_UNSUPPORTED);

    changed += ((RateControlMethod == MFX_RATECONTROL_VBR)
        && par.mfx.MaxKbps != 0
        && par.mfx.TargetKbps != 0)
        && CheckMinOrClip(par.mfx.MaxKbps, par.mfx.TargetKbps);

    changed += CheckBufferSizeInKB(par, defPar);

    const bool bCQP  = RateControlMethod == MFX_RATECONTROL_CQP;
    const auto minQP = defPar.base.GetMinQPMFX(defPar);
    const auto maxQP = defPar.base.GetMaxQPMFX(defPar);
    if (bCQP)
    {
        changed += CheckAndFixQP(par.mfx.QPI, minQP, maxQP);
        changed += CheckAndFixQP(par.mfx.QPP, minQP, maxQP);
        changed += CheckAndFixQP(par.mfx.QPB, minQP, maxQP);
    }

    if (RateControlMethod == MFX_RATECONTROL_ICQ)
    {
        changed += CheckAndFixQP(par.mfx.ICQQuality, 1, 51);
    }

    mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);
    if (pCO2)
    {
        changed += CheckOrZero<mfxU8>(pCO2->MinQPI, 0, minQP);
        changed += CheckOrZero<mfxU8>(pCO2->MaxQPI, 0, maxQP);
        changed += CheckOrZero<mfxU8>(pCO2->MinQPP, 0, minQP);
        changed += CheckOrZero<mfxU8>(pCO2->MaxQPP, 0, maxQP);
        changed += CheckOrZero<mfxU8>(pCO2->MinQPB, 0, minQP);
        changed += CheckOrZero<mfxU8>(pCO2->MaxQPB, 0, maxQP);
    }

    mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
    if (pCO3)
    {
        const mfxU16 GopRefDist     = defPar.base.GetGopRefDist(defPar);
        const bool   bBPyr          = GopRefDist > 1 && defPar.base.GetBRefType(defPar) == MFX_B_REF_PYRAMID;
        const bool   bQpOffsetValid = bCQP && bBPyr;

        changed += CheckOrZero<mfxU16>(pCO3->EnableQPOffset
            , mfxU16(MFX_CODINGOPTION_UNKNOWN)
            , mfxU16(MFX_CODINGOPTION_OFF)
            , mfxU16(MFX_CODINGOPTION_ON * !!bQpOffsetValid));

        mfxI16 minQPOffset = 0;
        mfxI16 maxQPOffset = 0;
        if (IsOn(pCO3->EnableQPOffset))
        {
            const mfxI16 QPX = std::get<2>(defPar.base.GetQPMFX(defPar));
            minQPOffset      = mfxI16(minQP - QPX);
            maxQPOffset      = mfxI16(maxQP - QPX);
        }

        auto CheckQPOffset = [&](mfxI16& QPO)
        {
            return CheckMinOrClip(QPO, minQPOffset) + CheckMaxOrClip(QPO, maxQPOffset);
        };

        changed += !!std::count_if(std::begin(pCO3->QPOffset), std::end(pCO3->QPOffset), CheckQPOffset);
    }

    mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
    if (pAuxPar)
    {
        if (bCQP)
        {
            changed += CheckOrZero<mfxU8>(pAuxPar->QP.MinBaseQIndex, 0);
            changed += CheckOrZero<mfxU8>(pAuxPar->QP.MaxBaseQIndex, 0);
        }
        else
        {
            if (pAuxPar->QP.MinBaseQIndex || pAuxPar->QP.MaxBaseQIndex)
            {
                changed += CheckRangeOrClip(pAuxPar->QP.MinBaseQIndex, minQP, maxQP);
                changed += CheckRangeOrClip(pAuxPar->QP.MaxBaseQIndex, minQP, maxQP);
                changed += CheckMaxOrClip(pAuxPar->QP.MinBaseQIndex, pAuxPar->QP.MaxBaseQIndex);
            }
        }
    }

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

mfxU32 General::GetRawBytes(const Defaults::Param& defPar)
{
    const auto& par = defPar.mvp;
    mfxU64 width = 0, height = 0;
    std::tie(width, height)  = GetRealResolution(par);

    mfxU64 size       = width * height;
    const auto format = defPar.base.GetTargetChromaFormatPlus1(defPar) - 1;
    auto depth        = defPar.base.GetTargetBitDepthLuma(defPar);
    if (format == MFX_CHROMAFORMAT_YUV420)
        size = size * 3 / 2;
    else if (format == MFX_CHROMAFORMAT_YUV422)
        size *= 2;
    else if (format == MFX_CHROMAFORMAT_YUV444)
        size *= 3;

    if (depth < 8)
    {
        MFX_LOG_WARN("TargetBitDepthLuma %d should be greater than 8!", depth);
        depth = 8;
    }

    if (depth != 8)
        size = mfx::CeilDiv(size * depth, mfxU64(8));

    return static_cast<mfxU32>(size);
}
mfxStatus General::CheckIOPattern(mfxVideoParam & par)
{
    mfxU32 invalid = 0;

    invalid += Check<mfxU16
        , MFX_IOPATTERN_IN_VIDEO_MEMORY
        , MFX_IOPATTERN_IN_SYSTEM_MEMORY
        , 0>
        (par.IOPattern);

    if (invalid)
    {
        par.IOPattern = 0;
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    return MFX_ERR_NONE;
}

mfxStatus General::CheckTemporalLayers(mfxVideoParam & par)
{
    mfxExtTemporalLayers* pTemporalLayers = ExtBuffer::Get(par);
    MFX_CHECK(pTemporalLayers && pTemporalLayers->NumLayers != 0, MFX_ERR_NONE);

    mfxU32 invalid = 0;
    invalid += SetIf(pTemporalLayers->NumLayers, pTemporalLayers->Layers == nullptr, 0);
    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    invalid += CheckOrZero<mfxU16>(pTemporalLayers->Layers[0].FrameRateScale, 0, 1);
    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    mfxU16 nTL = pTemporalLayers->NumLayers;
    // Strip tailing zeros, except for the first
    while (nTL > 1 && pTemporalLayers->Layers[nTL - 1].FrameRateScale == 0)
    {
        nTL--;
    }

    mfxU32 changed = 0;
    changed += CheckMaxOrClip(pTemporalLayers->NumLayers, nTL);

    // FrameScale 0 not allowd in the middle except for the first and trailing layers, the latter have been stripped
    mfxU16 scalePrev = std::max(mfxU16(1), pTemporalLayers->Layers[0].FrameRateScale);
    for (mfxU16 idx = 1; idx < pTemporalLayers->NumLayers; ++idx)
    {
        mfxU16 scaleCurr = pTemporalLayers->Layers[idx].FrameRateScale;
        MFX_CHECK(scaleCurr != 0, MFX_ERR_INVALID_VIDEO_PARAM);

        MFX_CHECK(!CheckMinOrZero(scaleCurr, scalePrev + 1), MFX_ERR_UNSUPPORTED);
        MFX_CHECK(!CheckOrZero(scaleCurr, mfxU16(scaleCurr - (scaleCurr % scalePrev))), MFX_ERR_UNSUPPORTED);

        scalePrev = scaleCurr;
    }

    changed += CheckOrZero<mfxU16>(par.mfx.GopRefDist, 0, 1, par.mfx.GopRefDist * (nTL <= 1));

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    return MFX_ERR_NONE;
}

mfxStatus General::CheckStillPicture(mfxVideoParam & par)
{
    mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
    MFX_CHECK(pAuxPar && IsOn(pAuxPar->StillPictureMode), MFX_ERR_NONE);

    mfxU32 changed = 0;
    if (par.mfx.GopPicSize != 1)
    {
        par.mfx.GopPicSize = 1;
        changed += 1;
    }

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

mfxStatus General::CheckGopRefDist(mfxVideoParam & par, const Defaults::Param& defPar)
{
    MFX_CHECK(par.mfx.GopRefDist, MFX_ERR_NONE);

    const mfxU16 GopPicSize = defPar.base.GetGopPicSize(defPar);
    const mfxU16 maxRefDist = std::max<mfxU16>(1, GopPicSize - 1);
    MFX_CHECK(!CheckMaxOrClip(par.mfx.GopRefDist, maxRefDist), MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

mfxStatus General::CheckGPB(mfxVideoParam & par)
{
    mfxU32 changed = 0;
    mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);

    if (pCO3)
    {
        changed += CheckOrZero<mfxU16>(
            pCO3->GPB
            , mfxU16(MFX_CODINGOPTION_ON)
            , mfxU16(MFX_CODINGOPTION_OFF)
            , mfxU16(MFX_CODINGOPTION_UNKNOWN));
    }

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    return MFX_ERR_NONE;
}

mfxStatus General::CheckTU(mfxVideoParam & par, const ENCODE_CAPS_AV1& /* caps */)
{
    auto& tu = par.mfx.TargetUsage;

    if (CheckMaxOrZero(tu, 7u))
        MFX_RETURN(MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;

}

mfxStatus General::CheckDeltaQ(mfxVideoParam& par)
{
    mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
    MFX_CHECK(pAuxPar, MFX_ERR_NONE);

    mfxU32 changed = 0;

    changed += CheckMinOrClip(pAuxPar->QP.YDcDeltaQ, -15);
    changed += CheckMaxOrClip(pAuxPar->QP.YDcDeltaQ, 15);
    changed += CheckMinOrClip(pAuxPar->QP.UDcDeltaQ, -63);
    changed += CheckMaxOrClip(pAuxPar->QP.UDcDeltaQ, 63);
    changed += CheckMinOrClip(pAuxPar->QP.UAcDeltaQ, -63);
    changed += CheckMaxOrClip(pAuxPar->QP.UAcDeltaQ, 63);
    changed += CheckMinOrClip(pAuxPar->QP.VDcDeltaQ, -63);
    changed += CheckMaxOrClip(pAuxPar->QP.VDcDeltaQ, 63);
    changed += CheckMinOrClip(pAuxPar->QP.VAcDeltaQ, -63);
    changed += CheckMaxOrClip(pAuxPar->QP.VAcDeltaQ, 63);
    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

mfxStatus General::CheckFrameOBU(mfxVideoParam& par,  const ENCODE_CAPS_AV1& caps)
{
    mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
    MFX_CHECK(pAuxPar, MFX_ERR_NONE);
    mfxU32 invalid = 0;

    if (IsOn(pAuxPar->PackOBUFrame) && caps.FrameOBUSupport == false)
    {
        pAuxPar->PackOBUFrame = MFX_CODINGOPTION_OFF;
        invalid = 1;
    }

    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    mfxExtAV1TileParam* pTilePar = ExtBuffer::Get(par);
    MFX_CHECK(pTilePar, MFX_ERR_NONE);

    mfxU32 changed = 0;
    if (pTilePar->NumTileGroups > 1 && IsOn(pAuxPar->PackOBUFrame))
    {
        pAuxPar->PackOBUFrame = MFX_CODINGOPTION_OFF;
        changed = 1;
    }

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

mfxStatus General::CheckOrderHint(mfxVideoParam& par,  const ENCODE_CAPS_AV1& caps)
{
    mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
    MFX_CHECK(pAuxPar, MFX_ERR_NONE);
    mfxU32 invalid = 0;

    if (IsOn(pAuxPar->EnableOrderHint) && caps.AV1ToolSupportFlags.fields.enable_order_hint == false)
    {
        pAuxPar->EnableOrderHint = MFX_CODINGOPTION_OFF;
        invalid = 1;
    }

    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
}

mfxStatus General::CheckOrderHintBits(mfxVideoParam& par)
{
    mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
    MFX_CHECK(pAuxPar, MFX_ERR_NONE);

    //OrderHintBits valid range is 1-8, 0 is default value and will be reset in SetDefaults
    mfxU32 changed = 0;
    changed += CheckMaxOrClip(pAuxPar->OrderHintBits, 8);
    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

inline void CheckCDEFStrength(mfxExtAV1AuxData& auxPar, mfxU32& invalid, mfxU32& changed, const mfxU32 CDEFChannelStrengthSupport)
{
    for (mfxU8 i = 0; i < CDEF_MAX_STRENGTHS; i++)
    {
        changed += CheckMaxOrClip(auxPar.Cdef.CdefYStrengths[i], 63);
        changed += CheckMaxOrClip(auxPar.Cdef.CdefUVStrengths[i], 63);
    }

    if (CDEFChannelStrengthSupport)
        return;

    for (mfxU8 i = 0; i < CDEF_MAX_STRENGTHS; i++)
    {
        if (auxPar.Cdef.CdefUVStrengths[i] != auxPar.Cdef.CdefYStrengths[i])
        {
            auxPar.Cdef.CdefUVStrengths[i] = auxPar.Cdef.CdefYStrengths[i] = 0;
            invalid += 1;
        }
    }
}

mfxStatus General::CheckCDEF(mfxVideoParam& par,  const ENCODE_CAPS_AV1& caps)
{
    mfxExtAV1AuxData* const pAuxPar = ExtBuffer::Get(par);

    mfxU32 invalid = 0;
    if (pAuxPar && IsOn(pAuxPar->EnableCdef) && !caps.AV1ToolSupportFlags.fields.enable_cdef)
    {
        pAuxPar->EnableCdef = MFX_CODINGOPTION_OFF;
        invalid += 1;
    }

    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    mfxU32 changed = 0;
    if (pAuxPar)
    {
        changed += CheckMaxOrClip(pAuxPar->Cdef.CdefDampingMinus3, 3);
        changed += CheckMaxOrClip(pAuxPar->Cdef.CdefBits, 3);

        CheckCDEFStrength(*pAuxPar, invalid, changed, caps.CDEFChannelStrengthSupport);
    }

    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

inline void CheckExtParam(
    const ParamSupport& sprt
    , mfxVideoParam& par
    , std::vector<mfxU8>& onesBuf)
{
    if (!par.ExtParam)
        return;

    for (mfxU32 i = 0; i < par.NumExtParam; i++)
    {
        if (!par.ExtParam[i])
            continue;

        mfxExtBuffer header = *par.ExtParam[i];

        memset(par.ExtParam[i], 0, header.BufferSz);
        *par.ExtParam[i] = header;

        auto it = sprt.m_ebCopySupported.find(header.BufferId);

        if (it != sprt.m_ebCopySupported.end())
        {
            if (onesBuf.size() < header.BufferSz)
                onesBuf.insert(onesBuf.end(), header.BufferSz - mfxU32(onesBuf.size()), 1);

            auto pSrc = (mfxExtBuffer*)onesBuf.data();

            if (pSrc != nullptr)
            {
                *pSrc = header;
                for (auto& copy : it->second)
                    copy(pSrc, par.ExtParam[i]);
            }
        }
    }
}

mfxStatus General::CheckColorConfig(mfxVideoParam& par)
{
    mfxU32 changed = 0;

    mfxExtVideoSignalInfo* pVSI = ExtBuffer::Get(par);
    if (pVSI)
    {
        changed += CheckRangeOrSetDefault<mfxU16>(pVSI->VideoFormat, 0, 8, 5);
        changed += CheckRangeOrSetDefault<mfxU16>(pVSI->ColourPrimaries, 0, 255, 2);
        changed += CheckRangeOrSetDefault<mfxU16>(pVSI->TransferCharacteristics, 0, 255, 2);
        changed += CheckRangeOrSetDefault<mfxU16>(pVSI->MatrixCoefficients, 0, 255, 2);
        changed += CheckOrZero<mfxU16, 0, 1>(pVSI->VideoFullRange);
        changed += CheckOrZero<mfxU16, 0, 1>(pVSI->ColourDescriptionPresent);
    }

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    return MFX_ERR_NONE;
}

void General::CheckQuery0(const ParamSupport& sprt, mfxVideoParam& par)
{
    std::vector<mfxU8> onesBuf(sizeof(par), 1);

    auto ExtParam    = par.ExtParam;
    auto NumExtParam = par.NumExtParam;

    par = mfxVideoParam{};

    for (auto& copy : sprt.m_mvpCopySupported)
        copy((mfxVideoParam*)onesBuf.data(), &par);

    par.ExtParam    = ExtParam;
    par.NumExtParam = NumExtParam;

    CheckExtParam(sprt, par, onesBuf);
}

mfxStatus General::CheckBuffers(const ParamSupport& sprt, const mfxVideoParam& in, const mfxVideoParam* out)
{
    MFX_CHECK(!(!in.NumExtParam && (!out || !out->NumExtParam)), MFX_ERR_NONE);
    MFX_CHECK(in.ExtParam, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(!(out && (!out->ExtParam || out->NumExtParam != in.NumExtParam))
        , MFX_ERR_UNDEFINED_BEHAVIOR);

    std::map<mfxU32, mfxU32> detected[2];
    mfxU32 dId = 0;

    for (auto pPar : { &in, out })
    {
        if (!pPar)
            continue;

        for (mfxU32 i = 0; i < pPar->NumExtParam; i++)
        {
            MFX_CHECK_NULL_PTR1(pPar->ExtParam[i]);

            auto id = pPar->ExtParam[i]->BufferId;

            MFX_CHECK(sprt.m_ebCopySupported.find(id) != sprt.m_ebCopySupported.end(), MFX_ERR_UNSUPPORTED);
            MFX_CHECK(!(detected[dId][id]++), MFX_ERR_UNDEFINED_BEHAVIOR);
        }
        dId++;
    }

    MFX_CHECK(!(out && detected[0] != detected[1]), MFX_ERR_UNDEFINED_BEHAVIOR);

    return MFX_ERR_NONE;
}

mfxStatus General::CopyConfigurable(const ParamSupport& sprt, const mfxVideoParam& in, mfxVideoParam& out)
{
    using TFnCopyMVP = std::function<void(const mfxVideoParam*, mfxVideoParam*)>;
    using TFnCopyEB  = std::function<void(const mfxExtBuffer*, mfxExtBuffer*)>;

    auto CopyMVP = [&](const mfxVideoParam& src, mfxVideoParam& dst)
    {
        std::for_each(sprt.m_mvpCopySupported.begin(), sprt.m_mvpCopySupported.end()
            , [&](const TFnCopyMVP& copy)
        {
            copy(&src, &dst);
        });
    };

    auto CopyEB = [](const std::list<TFnCopyEB>& copyList, const mfxExtBuffer* pIn, mfxExtBuffer* pOut)
    {
        std::for_each(copyList.begin(), copyList.end()
            , [&](const TFnCopyEB& copy)
        {
            copy(pIn, pOut);
        });
    };

    mfxVideoParam tmpMVP = {};
    CopyMVP(in, tmpMVP);

    tmpMVP.NumExtParam = out.NumExtParam;
    tmpMVP.ExtParam = out.ExtParam;

    out = tmpMVP;

    std::list<mfxExtBuffer*> outBufs(out.ExtParam, out.ExtParam + out.NumExtParam);
    outBufs.sort();
    outBufs.remove(nullptr);

    std::for_each(outBufs.begin(), outBufs.end()
        , [&](mfxExtBuffer* pEbOut)
    {
        std::vector<mfxU8> ebTmp(pEbOut->BufferSz, mfxU8(0));
        auto               pEbIn      = ExtBuffer::Get(in, pEbOut->BufferId);
        auto               copyIt     = sprt.m_ebCopySupported.find(pEbOut->BufferId);
        auto               copyPtrsIt = sprt.m_ebCopyPtrs.find(pEbOut->BufferId);
        mfxExtBuffer*      pEbTmp     = (mfxExtBuffer*)ebTmp.data();
        bool               bCopyPar   = pEbIn && copyIt != sprt.m_ebCopySupported.end();
        bool               bCopyPtr   = copyPtrsIt != sprt.m_ebCopyPtrs.end();

        *pEbTmp = *pEbOut;

        if (bCopyPtr)
        {
            CopyEB(copyPtrsIt->second, pEbOut, pEbTmp);
        }

        if (bCopyPar)
        {
            CopyEB(copyIt->second, pEbIn, pEbTmp);
        }

        std::copy_n(ebTmp.data(), ebTmp.size(), (mfxU8*)pEbOut);
    });

    return MFX_ERR_NONE;
}

mfxStatus General::CheckCodedPicSize(
    mfxVideoParam& par
    , const Defaults::Param& /*defPar*/)
{
    mfxExtAV1ResolutionParam* pRsPar = ExtBuffer::Get(par);
    MFX_CHECK(pRsPar, MFX_ERR_NONE);

    MFX_CHECK(!CheckMaxOrZero(pRsPar->FrameWidth, par.mfx.FrameInfo.Width), MFX_ERR_UNSUPPORTED);
    MFX_CHECK(!CheckMaxOrZero(pRsPar->FrameHeight, par.mfx.FrameInfo.Height), MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
}

bool Base::MapToDefinedLevel(mfxU16& level)
{
    auto levelIt = LevelsRemap.find(level);
    if(levelIt != LevelsRemap.end())
    {
        level = levelIt->second;
        return true;
    }
    else
    {
        return false;
    }
}

mfxStatus General::CheckLevelConstraints(
    mfxVideoParam& par
    , const Defaults::Param& defPar)
{
    MFX_CHECK(par.mfx.CodecLevel, MFX_ERR_NONE);

    const mfxU16 rc = defPar.base.GetRateControlMethod(defPar);
    mfxU32 maxKbps  = 0;
    SetIf(maxKbps, rc != MFX_RATECONTROL_CQP && rc != MFX_RATECONTROL_ICQ, [&]() { return defPar.base.GetMaxKbps(defPar); });

    const auto                frND    = defPar.base.GetFrameRate(defPar);
    const auto                res     = GetRealResolution(par);
    const mfxExtAV1TileParam* pTiles  = ExtBuffer::Get(par);
    const mfxExtAV1AuxData*   pAuxPar = ExtBuffer::Get(par);
    const auto                tiles   = GetNumTiles(pTiles, pAuxPar);

    const mfxU16 minLevel = GetMinLevel(
        std::get<0>(frND)
        , std::get<1>(frND)
        , std::get<0>(res)
        ,std::get<1>(res)
        , std::get<0>(tiles)
        , std::get<1>(tiles)
        , maxKbps
        , par.mfx.CodecProfile
        , par.mfx.CodecLevel);

    mfxU32 changed = 0;
    changed += CheckMinOrClip(par.mfx.CodecLevel, minLevel);

    // Resolution can't be corrected, return error if Level restriction can't met
    const mfxU32 maxHSize = GetMaxHSizeByLevel(par.mfx.CodecLevel);
    const mfxU32 maxVSize = GetMaxVSizeByLevel(par.mfx.CodecLevel);
    if (par.mfx.FrameInfo.Width > maxHSize || par.mfx.FrameInfo.Height > maxVSize)
    {
        par.mfx.FrameInfo.Width  = 0;
        par.mfx.FrameInfo.Height = 0;
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    mfxExtAV1ResolutionParam* pRsPar  = ExtBuffer::Get(par);
    if (pRsPar != nullptr
        && (pRsPar->FrameWidth > maxHSize || pRsPar->FrameHeight > maxVSize))
    {
        pRsPar->FrameWidth  = 0;
        pRsPar->FrameHeight = 0;
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    const mfxF64 maxFrameRate = GetMaxFrameRateByLevel(par.mfx.CodecLevel, std::get<0>(res), std::get<1>(res));
    if (par.mfx.FrameInfo.FrameRateExtN > maxFrameRate * par.mfx.FrameInfo.FrameRateExtD)
    {
        // Not clip frame rate, only return warning if it exceeds maximum value
        changed++;
    }

    if (maxKbps > 0)
    {
        mfxU32 maxKbpsByLevel = 0;
        if (par.mfx.CodecLevel >= MFX_LEVEL_AV1_4)
        {
            maxKbpsByLevel = GetMaxKbpsByLevel(par.mfx.CodecLevel, par.mfx.CodecProfile, 1);
        }
        else
        {
            maxKbpsByLevel = GetMaxKbpsByLevel(par.mfx.CodecLevel, par.mfx.CodecProfile);
        }

        if (maxKbps > maxKbpsByLevel)
        {
            MaxKbps(par.mfx) = maxKbpsByLevel;
            changed++;
        }
    }

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    return MFX_ERR_NONE;
}

mfxStatus General::CheckTCBRC(mfxVideoParam& par, const ENCODE_CAPS_AV1& caps)
{
    mfxExtCodingOption3* CO3 = ExtBuffer::Get(par);
    MFX_CHECK(CO3 && IsOn(CO3->LowDelayBRC), MFX_ERR_NONE);

    bool isVBR = par.mfx.RateControlMethod  ==  MFX_RATECONTROL_VBR;
    mfxU32 changed = 0;
    changed += SetIf(CO3->LowDelayBRC, !(caps.SupportedRateControlMethods.fields.TCBRCSupport && isVBR), MFX_CODINGOPTION_OFF);

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    return MFX_ERR_NONE;
}

inline FRAME_TYPE MapMfxFrameTypeToSpec(mfxU16 ft)
{
    if (IsB(ft) || IsP(ft))
        return INTER_FRAME;
    else
    {
        assert(IsI(ft));
        return KEY_FRAME;
    }
}

inline bool IsCDEFEmpty(const mfxExtAV1AuxData& auxPar)
{
    bool empty = true;

    empty &= auxPar.Cdef.CdefDampingMinus3 == 0;
    empty &= auxPar.Cdef.CdefBits == 0;

    for (mfxU8 i = 0; i < CDEF_MAX_STRENGTHS; i++)
    {
        empty &= auxPar.Cdef.CdefYStrengths[i] == 0;
        empty &= auxPar.Cdef.CdefUVStrengths[i] == 0;
    }

    return empty;
}

inline void SetCDEFByAuxData(
    const mfxExtAV1AuxData& auxPar
    , FH& bs_fh)
{
    auto& cdef = bs_fh.cdef_params;
    cdef.cdef_damping = auxPar.Cdef.CdefDampingMinus3 + 3;
    cdef.cdef_bits    = auxPar.Cdef.CdefBits;

    for (mfxU8 i = 0; i < CDEF_MAX_STRENGTHS; i++)
    {
        cdef.cdef_y_pri_strength[i]  = auxPar.Cdef.CdefYStrengths[i] / CDEF_STRENGTH_DIVISOR;
        cdef.cdef_y_sec_strength[i]  = auxPar.Cdef.CdefYStrengths[i] % CDEF_STRENGTH_DIVISOR;
        cdef.cdef_uv_pri_strength[i] = auxPar.Cdef.CdefUVStrengths[i] / CDEF_STRENGTH_DIVISOR;
        cdef.cdef_uv_sec_strength[i] = auxPar.Cdef.CdefUVStrengths[i] % CDEF_STRENGTH_DIVISOR;
    }
}

inline void SetCDEF(
    const Defaults::Param& dflts
    , const SH& sh
    , FH& fh)
{
    if (!sh.enable_cdef)
        return;

    const mfxExtAV1AuxData& auxPar = ExtBuffer::Get(dflts.mvp);
    if (IsCDEFEmpty(auxPar))
    {
        //Get default CDEF settings
        dflts.base.GetCDEF(fh);
    }
    else
    {
        //Set CDEF through command option
        SetCDEFByAuxData(auxPar, fh);
    }
}

inline bool IsLoopFilterLevelsEmpty(const mfxExtAV1AuxData& auxPar)
{
    bool empty = true;

    empty &= auxPar.LoopFilter.LFLevelYVert == 0;
    empty &= auxPar.LoopFilter.LFLevelYHorz == 0;
    empty &= auxPar.LoopFilter.LFLevelU == 0;
    empty &= auxPar.LoopFilter.LFLevelV == 0;

    return empty;
}

inline void SetLoopFilterLevelsByAuxData(
    const mfxExtAV1AuxData& auxPar
    , FH& bs_fh)
{
    bs_fh.loop_filter_params.loop_filter_level[0] = auxPar.LoopFilter.LFLevelYVert;
    bs_fh.loop_filter_params.loop_filter_level[1] = auxPar.LoopFilter.LFLevelYHorz;
    bs_fh.loop_filter_params.loop_filter_level[2] = auxPar.LoopFilter.LFLevelU;
    bs_fh.loop_filter_params.loop_filter_level[3] = auxPar.LoopFilter.LFLevelV;
}

inline void SetLoopFilterLevels(
    const Defaults::Param& dflts
    , FH& fh)
{
    const mfxExtAV1AuxData& auxPar = ExtBuffer::Get(dflts.mvp);
    if (!IsOn(auxPar.EnableLoopFilter))
        return;

    if (IsLoopFilterLevelsEmpty(auxPar))
    {
        //Get default LoopFilter settings
        dflts.base.GetLoopFilterLevels(dflts, fh);
    }
    else
    {
        //Set LoopFilter through command option
        SetLoopFilterLevelsByAuxData(auxPar, fh);
    }
}

inline void SetRefFrameFlags(
    const TaskCommonPar& task
    , FH& fh
    , const bool frameIsIntra)
{
    if (frameIsIntra)
        fh.primary_ref_frame = PRIMARY_REF_NONE;

    const int allFrames = (1 << NUM_REF_FRAMES) - 1;
    if (fh.frame_type == SWITCH_FRAME ||
        (fh.frame_type == KEY_FRAME && fh.show_frame))
        fh.refresh_frame_flags = allFrames;
    else
    {
        for (mfxU8 i = 0; i < NUM_REF_FRAMES; i++)
            fh.refresh_frame_flags |= task.RefreshFrameFlags[i] << i;
    }
}

inline void SetRefFrameIndex(
    const TaskCommonPar& task
    , FH& fh
    , const bool frameIsIntra)
{
    if (frameIsIntra)
        return;

    const auto& refList = task.RefList;
    mfxU8 defaultRef = refList[LAST_FRAME - LAST_FRAME];

    auto SetRef = [&defaultRef](mfxU8 ref) -> int32_t
    { return IsValid(ref) ? ref : defaultRef;};

    const mfxU8 bwdStartIdx = BWDREF_FRAME - LAST_FRAME;
    auto bwdStartIt = refList.begin() + bwdStartIdx;

    std::transform(refList.begin(), bwdStartIt, fh.ref_frame_idx, SetRef);

    auto validBwd = IsP(task.FrameType) ? refList.end() : std::find_if(bwdStartIt, refList.end(), IsValid);
    if (validBwd != refList.end())
        defaultRef = *validBwd;

    std::transform(bwdStartIt, refList.end(), fh.ref_frame_idx + bwdStartIdx, SetRef);
}

inline mfxU32 GetReferenceMode(const TaskCommonPar& task)
{
    return IsB(task.FrameType) || task.isLDB ? 1 : 0;
}

inline int av1_get_relative_dist(const SH& sh, const uint32_t a, const uint32_t b)
{
    // the logic is from AV1 spec 5.9.3

    if (!sh.enable_order_hint)
        return 0;

    const uint32_t OrderHintBits = sh.order_hint_bits_minus1 + 1;

    int32_t diff = a - b;
    uint32_t m = 1 << (OrderHintBits - 1);
    diff = (diff & (m - 1)) - (diff & m);
    return diff;
}

inline void GetFwdBwdIdx( const DpbType& dpb, const SH& sh, const FH& fh, const int orderHint, int& forwardIdx, int& forwardHint, int& backwardIdx)
{
    int i = 0;
    int refHint = 0;
    int backwardHint = -1;
    for (i = 0; i < REFS_PER_FRAME; i++)
    {
        auto& refFrm = dpb.at(fh.ref_frame_idx[i]);
        refHint = refFrm->DisplayOrderInGOP;
        if (av1_get_relative_dist(sh, refHint, orderHint) < 0)
        {
            if (forwardIdx < 0 ||
                av1_get_relative_dist(sh, refHint, forwardHint) > 0)
            {
                forwardIdx = i;
                forwardHint = refHint;
            }
        }
        else if (av1_get_relative_dist(sh, refHint, orderHint) > 0)
        {
            if (backwardIdx < 0 ||
                av1_get_relative_dist(sh, refHint, backwardHint) < 0)
            {
                backwardIdx = i;
                backwardHint = refHint;
            }
        }
    }
}

inline void SetSkipModeAllowed(const SH& sh, FH& fh, const DpbType& dpb)
{
    int forwardIdx = -1;
    int forwardHint = -1;
    int backwardIdx = -1;
    GetFwdBwdIdx(dpb, sh, fh, fh.order_hint, forwardIdx, forwardHint, backwardIdx);
    if (forwardIdx < 0)
    {
        fh.skipModeAllowed = 0;
    }
    else if (backwardIdx >= 0)
    {
        fh.skipModeAllowed = 1;
    }
    else
    {
        int secondForwardHint = -1;
        int secondForwardIdx = -1;
        GetFwdBwdIdx(dpb, sh, fh, forwardHint, secondForwardIdx, secondForwardHint, backwardIdx);
        if (secondForwardIdx < 0)
        {
            fh.skipModeAllowed = 0;
        }
        else
        {
            fh.skipModeAllowed = 1;
        }
    }
}

inline void SetSkipModeParams(const SH& sh, FH& fh, const bool frameIsIntra, const DpbType& dpb)
{
    // the logic is from AV1 spec 5.9.22

    fh.skipModeAllowed = 1;

    if (frameIsIntra || !fh.reference_select || !sh.enable_order_hint)
        fh.skipModeAllowed = 0;
    else
    {
        SetSkipModeAllowed(sh, fh, dpb);
    }
}

mfxStatus General::GetCurrentFrameHeader(
    const TaskCommonPar& task
    , const Defaults::Param& dflts
    , const SH& sh
    , const FH& fh
    , FH & currFH) const
{
    currFH = fh;

    currFH.frame_type     = MapMfxFrameTypeToSpec(task.FrameType);
    currFH.show_frame     = IsHiddenFrame(task) ? 0 : 1;
    if (currFH.show_frame)
    {
        currFH.showable_frame = currFH.frame_type != KEY_FRAME;
    }
    else
    {
         currFH.showable_frame = 1; // for now, all hiden frame will be show in later.
    }

    if (currFH.frame_type == SWITCH_FRAME ||
        (currFH.frame_type == KEY_FRAME && currFH.show_frame))
    {
        currFH.error_resilient_mode = 1;
    }

    currFH.order_hint = task.DisplayOrderInGOP;

    const bool frameIsIntra = FrameIsIntra(currFH);
    SetRefFrameFlags(task, currFH, frameIsIntra);
    SetRefFrameIndex(task, currFH, frameIsIntra);

    currFH.quantization_params.base_q_idx = task.QpY;

    if (IsLossless(currFH))
    {
        currFH.CodedLossless = 1;
        DisableCDEF(currFH);
        DisableLoopFilter(currFH);
    }
    else
    {
        SetCDEF(dflts, sh, currFH);
        SetLoopFilterLevels(dflts, currFH);
    }

    currFH.TxMode = (fh.CodedLossless) ? ONLY_4X4 : TX_MODE_SELECT;

    currFH.reference_select = GetReferenceMode(task);

    SetSkipModeParams(sh, currFH, frameIsIntra, task.DPB);

    return MFX_ERR_NONE;
}

}

#endif
