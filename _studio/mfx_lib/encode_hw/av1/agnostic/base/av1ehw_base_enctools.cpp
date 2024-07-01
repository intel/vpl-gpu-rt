// Copyright (c) 2020-2022 Intel Corporation
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
#if defined(MFX_ENABLE_ENCTOOLS)

#include "mfx_platform_caps.h"
#include "av1ehw_base.h"
#include "av1ehw_base_data.h"
#include "av1ehw_base_enctools.h"
#include "av1ehw_base_task.h"

#include "av1ehw_base_packer.h"
#include "av1ehw_base_segmentation.h"

#include "libmfx_core.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;


inline bool IsSegBlockSmallerThanSB(mfxU32 segBlock, mfxU32 sb)
{
    return sb == 128 ||
        (sb == 64 && segBlock < MFX_AV1_SEGMENT_ID_BLOCK_SIZE_64x64);
}

void ApplyHWLimitation_XeHpm(StorageW& global, StorageW& s_task, mfxExtAV1Segmentation& seg_enctools)
{
    auto& fh = Task::FH::Get(s_task);
    mfxExtAV1Segmentation& seg_task = Task::Segment::Get(s_task);

    // Xe_HPM does not support segmentation_temporal_update
    fh.segmentation_params.segmentation_temporal_update = 0;

    // Update segmentation_update_map for small segment block size
    if (!fh.segmentation_params.segmentation_update_map)
    {
        // in Xe_HPM "segmentation_update_map = 0" mode is implemented by forcing segmentation through VDEnc Stream in
        // there are following related restrictions/requirements:
        // 1) segmentation map should be always sent to the driver explicitly (even if there is no map update)
        // 2) "segmentation_update_map = 0" cannot be safely applied for SegmentIdBlockSize smaller than SB size

        const Base::SH& sh = Base::Glob::SH::Get(global);

        seg_task.SegmentIds = seg_enctools.SegmentIds;
        seg_task.NumSegmentIdAlloc = seg_enctools.NumSegmentIdAlloc;

        if (IsSegBlockSmallerThanSB(seg_task.SegmentIdBlockSize, sh.sbSize))
        {
            // force map update if SegmentIdBlockSize is smaller than SB size
            fh.segmentation_params.segmentation_update_map = 1;
        }
    }
}

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


static void CDEF(FH& bs_fh)
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

template<class T> struct ExtBufTypeToId {};
#define BIND_EXTBUF_TYPE_TO_ID(TYPE, ID) template<> struct ExtBufTypeToId<TYPE> { enum { id = ID }; }
BIND_EXTBUF_TYPE_TO_ID (mfxExtAVCRefListCtrl, MFX_EXTBUFF_AVC_REFLIST_CTRL);

static void InitExtBufHeader_AV1(mfxExtRefListCtrl& extBuf)
{
    extBuf = {};
    extBuf.Header.BufferId = ExtBufTypeToId<mfxExtAVCRefListCtrl>::id;
    extBuf.Header.BufferSz = sizeof(mfxExtRefListCtrl);

    for (mfxU32 i = 0; i < 32; i++)
        extBuf.PreferredRefList[i].FrameOrder = mfxU32(MFX_FRAMEORDER_UNKNOWN);
    for (mfxU32 i = 0; i < 16; i++)
        extBuf.RejectedRefList[i].FrameOrder = mfxU32(MFX_FRAMEORDER_UNKNOWN);
    for (mfxU32 i = 0; i < 16; i++)
        extBuf.LongTermRefList[i].FrameOrder = mfxU32(MFX_FRAMEORDER_UNKNOWN);
}

void AV1EncTools::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_ENCTOOLS_CONFIG].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtEncToolsConfig*)pSrc;
        auto& buf_dst = *(mfxExtEncToolsConfig*)pDst;

        MFX_COPY_FIELD(AdaptiveI);
        MFX_COPY_FIELD(AdaptiveB);
        MFX_COPY_FIELD(AdaptiveRefP);
        MFX_COPY_FIELD(AdaptiveRefB);
        MFX_COPY_FIELD(SceneChange);
        MFX_COPY_FIELD(AdaptiveLTR);
        MFX_COPY_FIELD(AdaptivePyramidQuantP);
        MFX_COPY_FIELD(AdaptivePyramidQuantB);
        MFX_COPY_FIELD(AdaptiveQuantMatrices);
        MFX_COPY_FIELD(AdaptiveMBQP);
        MFX_COPY_FIELD(BRCBufferHints);
        MFX_COPY_FIELD(BRC);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_CODING_OPTION2].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtCodingOption2*)pSrc;
        auto& buf_dst = *(mfxExtCodingOption2*)pDst;

        MFX_COPY_FIELD(AdaptiveI);
        MFX_COPY_FIELD(AdaptiveB);
        MFX_COPY_FIELD(LookAheadDepth);
        MFX_COPY_FIELD(ExtBRC);
        MFX_COPY_FIELD(MBBRC);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_CODING_OPTION3].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtCodingOption3*)pSrc;
        auto& buf_dst = *(mfxExtCodingOption3*)pDst;

        MFX_COPY_FIELD(AdaptiveCQM);
        MFX_COPY_FIELD(ScenarioInfo);
        MFX_COPY_FIELD(ContentInfo);
        MFX_COPY_FIELD(ExtBrcAdaptiveLTR);
    });

#if defined (ONEVPL_EXPERIMENTAL)
    blocks.m_ebCopySupported[MFX_EXTBUFF_TUNE_ENCODE_QUALITY].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtTuneEncodeQuality*)pSrc;
        auto& buf_dst = *(mfxExtTuneEncodeQuality*)pDst;
        MFX_COPY_FIELD(TuneQuality);
    });
#endif
}

void AV1EncTools::SetInherited(ParamInheritance& par)
{
    par.m_ebInheritDefault[MFX_EXTBUFF_ENCTOOLS_CONFIG].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        auto& buf_src = *(const mfxExtEncToolsConfig*)pSrc;
        auto& buf_dst = *(mfxExtEncToolsConfig*)pDst;

        InheritOption(buf_src.AdaptiveI, buf_dst.AdaptiveI);
        InheritOption(buf_src.AdaptiveB, buf_dst.AdaptiveB);
        InheritOption(buf_src.AdaptiveRefP, buf_dst.AdaptiveRefP);
        InheritOption(buf_src.AdaptiveRefB, buf_dst.AdaptiveRefB);
        InheritOption(buf_src.SceneChange, buf_dst.SceneChange);
        InheritOption(buf_src.AdaptiveLTR, buf_dst.AdaptiveLTR);
        InheritOption(buf_src.AdaptivePyramidQuantP, buf_dst.AdaptivePyramidQuantP);
        InheritOption(buf_src.AdaptivePyramidQuantB, buf_dst.AdaptivePyramidQuantB);
        InheritOption(buf_src.AdaptiveQuantMatrices, buf_dst.AdaptiveQuantMatrices);
        InheritOption(buf_src.AdaptiveMBQP, buf_dst.AdaptiveMBQP);
        InheritOption(buf_src.BRCBufferHints, buf_dst.BRCBufferHints);
        InheritOption(buf_src.BRC, buf_dst.BRC);
    });
}

static bool isSWLACondition(const mfxVideoParam& video)
{
    const mfxExtCodingOption2* pExtOpt2 = ExtBuffer::Get(video);
    return (pExtOpt2 && 
        (pExtOpt2->LookAheadDepth > video.mfx.GopRefDist));
}

void AV1EncTools::SetDefaultConfig(const mfxVideoParam &video, mfxExtEncToolsConfig &config, bool bMBQPSupport)
{


    const mfxExtCodingOption2  *pExtOpt2 = ExtBuffer::Get(video);
    const mfxExtCodingOption3  *pExtOpt3 = ExtBuffer::Get(video);
    const mfxExtEncToolsConfig *pExtConfig = ExtBuffer::Get(video);
    const mfxExtAV1AuxData *pAuxData = ExtBuffer::Get(video);
    const mfxExtCodingOptionDDI *pExtDdi = ExtBuffer::Get(video);
    bool bGameStreaming = pExtOpt3 && pExtOpt3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING;

    if (!IsEncToolsConfigDefined(pExtConfig))
    {
        if (IsSwEncToolsImplicit(video) || IsHwEncToolsOn(video))
        {
            //ENC Tools are turned ON by specific command line parameter set
            config.AdaptiveI             = MFX_CODINGOPTION_UNKNOWN;
            config.AdaptiveB             = MFX_CODINGOPTION_UNKNOWN;
            config.AdaptivePyramidQuantP = MFX_CODINGOPTION_UNKNOWN;
            config.AdaptivePyramidQuantB = MFX_CODINGOPTION_UNKNOWN;
            config.AdaptiveRefP          = MFX_CODINGOPTION_UNKNOWN;
            config.AdaptiveRefB          = MFX_CODINGOPTION_UNKNOWN;
            config.AdaptiveLTR           = MFX_CODINGOPTION_UNKNOWN;
            config.BRCBufferHints        = MFX_CODINGOPTION_UNKNOWN;
            config.BRC                   = MFX_CODINGOPTION_UNKNOWN;
            config.AdaptiveQuantMatrices = MFX_CODINGOPTION_UNKNOWN;
            config.SceneChange           = MFX_CODINGOPTION_UNKNOWN;
            config.AdaptiveMBQP          = MFX_CODINGOPTION_UNKNOWN;
        }
        else
        {
            //turn Enc Tools OFF
            config.AdaptiveI             = MFX_CODINGOPTION_OFF;
            config.AdaptiveB             = MFX_CODINGOPTION_OFF;
            config.AdaptivePyramidQuantP = MFX_CODINGOPTION_OFF;
            config.AdaptivePyramidQuantB = MFX_CODINGOPTION_OFF;
            config.AdaptiveRefP          = MFX_CODINGOPTION_OFF;
            config.AdaptiveRefB          = MFX_CODINGOPTION_OFF;
            config.AdaptiveLTR           = MFX_CODINGOPTION_OFF;
            config.BRCBufferHints        = MFX_CODINGOPTION_OFF;
            config.BRC                   = MFX_CODINGOPTION_OFF;
            config.AdaptiveQuantMatrices = MFX_CODINGOPTION_OFF;
            config.SceneChange           = MFX_CODINGOPTION_OFF;
            config.AdaptiveMBQP          = MFX_CODINGOPTION_OFF;
            return;
        }
    }
    else if (pExtConfig)
        config = *pExtConfig;

    if (!bGameStreaming)
    {
        if (IsSwEncToolsSupported(video))
        {
            bool bAdaptiveI = !(pExtOpt2 && IsOff(pExtOpt2->AdaptiveI)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT);
            bool bHasB = video.mfx.GopRefDist > 1;
            bool bAdaptiveB = !(pExtOpt2 && IsOff(pExtOpt2->AdaptiveB)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT) && bHasB;
            bool bAdaptiveLTR = !(pExtDdi && pExtDdi->NumActiveRefP < 2) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT) && (pExtOpt3 && IsOn(pExtOpt3->ExtBrcAdaptiveLTR));

            SetDefaultOpt(config.AdaptiveI, bAdaptiveI);
            SetDefaultOpt(config.AdaptiveB, bAdaptiveB);
            SetDefaultOpt(config.AdaptivePyramidQuantP, false);
            SetDefaultOpt(config.AdaptivePyramidQuantB, bHasB);
            SetDefaultOpt(config.AdaptiveLTR, bAdaptiveLTR);
        }
        SetDefaultOpt(config.BRC, (video.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
            video.mfx.RateControlMethod == MFX_RATECONTROL_VBR));

        bool lplaAssistedBRC = IsOn(config.BRC) && isSWLACondition(video);
        SetDefaultOpt(config.BRCBufferHints, lplaAssistedBRC);

        bool bIsSegModeUnknown =  !pAuxData || (pAuxData->SegmentationMode == MFX_CODINGOPTION_UNKNOWN);

        SetDefaultOpt(config.AdaptiveMBQP, bMBQPSupport && lplaAssistedBRC && pExtOpt2 && IsOn(pExtOpt2->MBBRC) && bIsSegModeUnknown);

        //these features are not supported for now, we will enable them in future
        config.AdaptiveRefP          = MFX_CODINGOPTION_OFF;
        config.AdaptiveRefB          = MFX_CODINGOPTION_OFF;
        config.AdaptiveQuantMatrices = MFX_CODINGOPTION_OFF;
    }
#ifdef MFX_ENABLE_ENCTOOLS_LPLA
    else
    {
        bool bLA = (pExtOpt2 && pExtOpt2->LookAheadDepth > 0 &&
            (video.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
                video.mfx.RateControlMethod == MFX_RATECONTROL_VBR));
        bool bAdaptiveI = (pExtOpt2 && IsOn(pExtOpt2->AdaptiveI)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT);
        bool bHasB = video.mfx.GopRefDist > 1;
        bool bAdaptiveB = (pExtOpt2 && IsOn(pExtOpt2->AdaptiveB)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT) && bHasB;
        bool bDisableCQM = pExtOpt3 && IsOff(pExtOpt3->AdaptiveCQM);

        SetDefaultOpt(config.BRCBufferHints, bLA);
        SetDefaultOpt(config.AdaptivePyramidQuantP, bLA);
        SetDefaultOpt(config.AdaptivePyramidQuantB, bLA && bHasB);
        SetDefaultOpt(config.AdaptiveQuantMatrices, bLA);
        SetDefaultOpt(config.AdaptiveI, bLA && bAdaptiveI);
        SetDefaultOpt(config.AdaptiveB, bLA && bAdaptiveB);
        SetDefaultOpt(config.AdaptiveQuantMatrices, bLA && !bDisableCQM);
    }
#endif
}

mfxU32 AV1EncTools::CorrectVideoParams(mfxVideoParam & video, mfxExtEncToolsConfig & supportedConfig)
{


    mfxExtCodingOption2   *pExtOpt2 = ExtBuffer::Get(video);
    mfxExtCodingOption3   *pExtOpt3 = ExtBuffer::Get(video);
    mfxExtBRC             *pBRC = ExtBuffer::Get(video);
    mfxExtEncToolsConfig  *pConfig = ExtBuffer::Get(video);
    mfxExtCodingOptionDDI *pExtDdi = ExtBuffer::Get(video);

    mfxU32 changed = 0;
    bool bIsEncToolsEnabled = false;

    if (pConfig)
    {
        bIsEncToolsEnabled = IsEncToolsOn(video);

        bool bAdaptiveI = !(pExtOpt2 && IsOff(pExtOpt2->AdaptiveI)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT);
        bool bHasB = video.mfx.GopRefDist > 1;
        bool bAdaptiveB = !(pExtOpt2 && IsOff(pExtOpt2->AdaptiveB)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT) && bHasB;
        bool bAdaptiveRef = false;
        bool bAdaptiveLTR = !(pExtDdi && pExtDdi->NumActiveRefP < 2) && !(pExtOpt3 && IsOff(pExtOpt3->ExtBrcAdaptiveLTR)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT);        

        changed += CheckFlag(pConfig->AdaptiveI, bIsEncToolsEnabled && bAdaptiveI);
        changed += CheckFlag(pConfig->AdaptiveB, bIsEncToolsEnabled && bAdaptiveB);
        changed += CheckFlag(pConfig->AdaptivePyramidQuantB, bIsEncToolsEnabled && bHasB);
        changed += CheckFlag(pConfig->AdaptivePyramidQuantP, bIsEncToolsEnabled);

        changed += CheckFlag(pConfig->AdaptiveRefP, bIsEncToolsEnabled  && bAdaptiveRef);
        changed += CheckFlag(pConfig->AdaptiveRefB, bIsEncToolsEnabled  && bAdaptiveRef);
        changed += CheckFlag(pConfig->AdaptiveLTR, bIsEncToolsEnabled  && bAdaptiveLTR);
        changed += CheckFlag(pConfig->SceneChange, bIsEncToolsEnabled);
        changed += CheckFlag(pConfig->BRCBufferHints, bIsEncToolsEnabled);
        changed += CheckFlag(pConfig->AdaptiveQuantMatrices, bIsEncToolsEnabled);
        changed += CheckFlag(pConfig->BRC, bIsEncToolsEnabled);

        changed += CheckFlag(pConfig->AdaptiveI, IsOn(supportedConfig.AdaptiveI));
        changed += CheckFlag(pConfig->AdaptiveB, IsOn(supportedConfig.AdaptiveB));
        changed += CheckFlag(pConfig->AdaptivePyramidQuantB, IsOn(supportedConfig.AdaptivePyramidQuantB));
        changed += CheckFlag(pConfig->AdaptivePyramidQuantP, IsOn(supportedConfig.AdaptivePyramidQuantP));
        changed += CheckFlag(pConfig->AdaptiveRefP, IsOn(supportedConfig.AdaptiveRefP));
        changed += CheckFlag(pConfig->AdaptiveRefB, IsOn(supportedConfig.AdaptiveRefB));
        changed += CheckFlag(pConfig->AdaptiveLTR, IsOn(supportedConfig.AdaptiveLTR));
        changed += CheckFlag(pConfig->SceneChange, IsOn(supportedConfig.SceneChange));
        changed += CheckFlag(pConfig->BRCBufferHints, IsOn(supportedConfig.BRCBufferHints));
        changed += CheckFlag(pConfig->AdaptiveQuantMatrices, IsOn(supportedConfig.AdaptiveQuantMatrices));
        changed += CheckFlag(pConfig->BRC, IsOn(supportedConfig.BRC));
    }
    if (pExtOpt2)
    {
        changed += CheckFlag(pExtOpt2->AdaptiveI, IsOn(supportedConfig.AdaptiveI));
        changed += CheckFlag(pExtOpt2->AdaptiveB, IsOn(supportedConfig.AdaptiveB));
        changed += CheckFlag(pExtOpt2->ExtBRC, IsOn(supportedConfig.BRC));
    }
    if (pExtOpt3)
    {
        changed += CheckFlag(pExtOpt3->ExtBrcAdaptiveLTR, IsOn(supportedConfig.AdaptiveLTR));
        changed += CheckFlag(pExtOpt3->AdaptiveCQM, IsOn(supportedConfig.AdaptiveQuantMatrices));
    }

    //ExtBRC isn't compatible with EncTools
    if (pBRC && bIsEncToolsEnabled &&
        (pBRC->pthis ||
         pBRC->Init ||
         pBRC->Close ||
         pBRC->Update ||
         pBRC->GetFrameCtrl ||
         pBRC->Reset ||
         pBRC->GetFrameCtrl))
    {
        pBRC->pthis = nullptr;
        pBRC->Init = nullptr;
        pBRC->Close = nullptr;
        pBRC->Update = nullptr;
        pBRC->GetFrameCtrl = nullptr;
        pBRC->Reset = nullptr;
        changed++;
    }

    return changed;
}

mfxStatus AV1EncTools::InitEncToolsCtrl(
    mfxVideoParam const & par
    , mfxEncToolsCtrl *ctrl)
{
    mfxStatus sts = AV1EncToolsCommon::InitEncToolsCtrl(par, ctrl);
    MFX_CHECK_STS(sts);

    if (ctrl->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING)
    {
        mfxU16 crW = par.mfx.FrameInfo.CropW ? par.mfx.FrameInfo.CropW : par.mfx.FrameInfo.Width;
        if (crW >= 720) ctrl->LaScale = 2;
    }
    else
    {
        mfxU16 crH = par.mfx.FrameInfo.CropH ? par.mfx.FrameInfo.CropH : par.mfx.FrameInfo.Height;
        mfxU16 crW = par.mfx.FrameInfo.CropW ? par.mfx.FrameInfo.CropW : par.mfx.FrameInfo.Width;
        mfxU16 maxDim = std::max(crH, crW);
        mfxU16 minDim = std::min(crH, crW);
        constexpr mfxU16 LaScale = 2;
        if (maxDim >= 720 &&
            minDim >= (128 << LaScale)) //encoder limitation, 128 and up is fine
        {
            ctrl->LaScale = LaScale;
        }
        ctrl->LaQp = 26;
    }

    return MFX_ERR_NONE;
}

void AV1EncTools::Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push)
{
    // This "Push" sets CO2.MBBRC to ON if it is not specified in cmdline
    Push(BLK_SetDefaultsCallChain,
        [this](const mfxVideoParam&, mfxVideoParam& par, StorageRW& strg) -> mfxStatus
        {
            MFX_CHECK(IsFeatureEnabled(par), MFX_ERR_NONE);

            auto& defaults = Glob::Defaults::GetOrConstruct(strg);
            auto& bSet = defaults.SetForFeature[GetID()];
            MFX_CHECK(!bSet, MFX_ERR_NONE);
            defaults.GetMBBRC.Push(
                [this](Defaults::TChain<mfxU16>::TExt prev
                    , const Defaults::Param& par)
                {
                    bool bEncTools = IsSwEncToolsOn(par.mvp);
                    const mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par.mvp);
                    if (bEncTools && isSWLACondition(par.mvp))
                    {
                        const mfxExtEncToolsConfig* pConfig = ExtBuffer::Get(par.mvp);
                        bool bDisableMBQP = pConfig && (IsOff(pConfig->AdaptiveMBQP) || IsOff(pConfig->BRC));
                        return (mfxU16)((pCO2 && pCO2->MBBRC) ?
                            pCO2->MBBRC :
                            bDisableMBQP ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON);
                    }

                    return mfxU16(prev(par));

                });
            return MFX_ERR_NONE;
        });

    AV1EncToolsCommon::Query1NoCaps(blocks, Push);
}

void AV1EncTools::Reset(const FeatureBlocks& /*blocks*/, TPushR Push)
{
    Push(BLK_ResetCheck
        , [this](
            const mfxVideoParam& /*par*/
            , StorageRW& global
            , StorageRW&) -> mfxStatus
    {
        MFX_CHECK(m_pEncTools, MFX_ERR_NONE);

        auto& init = Glob::RealState::Get(global);
        auto& parOld = Glob::VideoParam::Get(init);
        auto& parNew = Glob::VideoParam::Get(global);

        MFX_CHECK(IsFeatureEnabled(parNew), MFX_ERR_NONE);

        mfxExtEncToolsConfig *pConfigOld = ExtBuffer::Get(parOld);
        mfxExtEncToolsConfig *pConfigNew = ExtBuffer::Get(parNew);

        if (pConfigOld && pConfigNew)
        {
            MFX_CHECK(
                pConfigOld->AdaptiveB == pConfigNew->AdaptiveB
                && pConfigOld->AdaptiveI == pConfigNew->AdaptiveI
                && pConfigOld->AdaptiveRefP == pConfigNew->AdaptiveRefP
                && pConfigOld->AdaptiveRefB == pConfigNew->AdaptiveRefB
                && pConfigOld->AdaptiveLTR == pConfigNew->AdaptiveLTR
                && pConfigOld->AdaptivePyramidQuantP == pConfigNew->AdaptivePyramidQuantP
                && pConfigOld->AdaptivePyramidQuantB == pConfigNew->AdaptivePyramidQuantB
                && pConfigOld->BRC == pConfigNew->BRC
                && pConfigOld->BRCBufferHints == pConfigNew->BRCBufferHints
                && pConfigOld->AdaptiveQuantMatrices == pConfigNew->AdaptiveQuantMatrices
                , MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        }

        return MFX_ERR_NONE;
    });
}

mfxStatus AV1EncTools::SubmitPreEncTask(StorageW& global, StorageW& s_task)
{
    auto sts = AV1EncToolsCommon::SubmitPreEncTask(global, s_task);
    auto& task = Task::Common::Get(s_task);
    if(m_saliencyMapSupported && task.pSurfIn)
    {
        if(m_saliencyMapSize == 0)
        {
            const mfxU32 blockSize = 8;
            m_saliencyMapSize = task.pSurfIn->Info.Width * task.pSurfIn->Info.Height / (blockSize * blockSize);
        }

        task.saliencyMap.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_HINT_SALIENCY_MAP;
        task.saliencyMap.Header.BufferSz = sizeof(mfxEncToolsHintSaliencyMap);
        task.saliencyMap.AllocatedSize = (mfxU32)m_saliencyMapSize;

        //task should be POD, so we can't use unique_ptr here
        task.saliencyMap.SaliencyMap = new mfxF32[m_saliencyMapSize];

        std::vector<mfxExtBuffer*> extQueryParams;
        extQueryParams.push_back(&task.saliencyMap.Header);

        mfxEncToolsTaskParam param{};
        param.ExtParam = extQueryParams.data();
        param.NumExtParam = (mfxU16)extQueryParams.size();
        param.DisplayOrder = task.DisplayOrder;

        sts = m_pEncTools->Query(m_pEncTools->Context, &param, 0 /*timeout*/);
        if (sts == MFX_ERR_NOT_ENOUGH_BUFFER)
        {
            m_saliencyMapSize = task.saliencyMap.Width * task.saliencyMap.Height;
            task.saliencyMap.AllocatedSize = (mfxU32)m_saliencyMapSize;
            delete[] task.saliencyMap.SaliencyMap;
            task.saliencyMap.SaliencyMap = new mfxF32[m_saliencyMapSize];
            sts = m_pEncTools->Query(m_pEncTools->Context, &param, 0 /*timeout*/);
        }
        MFX_CHECK_STS(sts);
    }
    return (sts);
}

mfxStatus AV1EncTools::BRCGetCtrl(StorageW& global, StorageW& s_task,
    mfxEncToolsBRCQuantControl &extQuantCtrl, mfxEncToolsBRCHRDPos &extHRDPos)
{
    MFX_CHECK(IsOn (m_EncToolConfig.BRC) && m_pEncTools && m_pEncTools->Submit, MFX_ERR_NONE);

    mfxEncToolsTaskParam task_par = {};
    auto&      task = Task::Common::Get(s_task);
    std::vector<mfxExtBuffer*> extParams;
    mfxEncToolsBRCFrameParams  extFrameData = {};
    mfxEncToolsBRCBufferHint extBRCHints = {};
    task_par.DisplayOrder = task.DisplayOrder;

    {
        // input params
        extFrameData.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_BRC_FRAME_PARAM;
        extFrameData.Header.BufferSz = sizeof(extFrameData);
        extFrameData.EncodeOrder = task.EncodedOrder;
        extFrameData.FrameType = task.FrameType;
        extFrameData.PyramidLayer = (mfxU16) task.PyramidLevel;
        extFrameData.SceneChange = task.GopHints.SceneChange;
        extFrameData.SpatialComplexity = task.GopHints.SpatialComplexity;
        extFrameData.PersistenceMapNZ = task.GopHints.PersistenceMapNZ;
        if (extFrameData.PersistenceMapNZ)
            memcpy(extFrameData.PersistenceMap, task.GopHints.PersistenceMap, sizeof(extFrameData.PersistenceMap));

        if (task.InternalListCtrlPresent && task.InternalListCtrl.LongTermRefList[0].FrameOrder == task.DisplayOrder) {
            extFrameData.LongTerm = 1;
        }

        extParams.push_back(&extFrameData.Header);

        if (task.BrcHints.LaAvgEncodedBits)
        {
            extBRCHints.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_BRC_BUFFER_HINT;
            extBRCHints.Header.BufferSz = sizeof(extBRCHints);
            extBRCHints.AvgEncodedSizeInBits = task.BrcHints.LaAvgEncodedBits;
            extBRCHints.CurEncodedSizeInBits = task.BrcHints.LaCurEncodedBits;
            extBRCHints.DistToNextI = task.BrcHints.LaDistToNextI;
            extParams.push_back(&extBRCHints.Header);
        }

        if (task.GopHints.QPModulaton || task.GopHints.MiniGopSize)
        {
            mfxEncToolsHintPreEncodeGOP  extPreGop = {};
            extPreGop.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_HINT_GOP;
            extPreGop.Header.BufferSz = sizeof(extPreGop);
            extPreGop.QPModulation = (mfxU16)task.GopHints.QPModulaton;
            extPreGop.QPDeltaExplicitModulation = (mfxI8)task.GopHints.QPDeltaExplicitModulation;
            extPreGop.MiniGopSize = (mfxU16)task.GopHints.MiniGopSize;
            extParams.push_back(&extPreGop.Header);
        }

        task_par.ExtParam = extParams.data();
        task_par.NumExtParam = (mfxU16)extParams.size();

        auto sts = m_pEncTools->Submit(m_pEncTools->Context, &task_par);
        MFX_CHECK_STS(sts);
    }
    {
        mfxEncToolsHintQPMap   qpMapHint = {};
        auto& par = Glob::VideoParam::Get(global);

        extParams.clear();

        // output params
        extQuantCtrl = {};
        extQuantCtrl.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_BRC_QUANT_CONTROL;
        extQuantCtrl.Header.BufferSz = sizeof(extQuantCtrl);
        extParams.push_back(&extQuantCtrl.Header);

        extHRDPos = {};
        extHRDPos.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_BRC_HRD_POS;
        extHRDPos.Header.BufferSz = sizeof(extHRDPos);
        extParams.push_back(&extHRDPos.Header);

        mfxExtAV1Segmentation seg_enctools = {};
        const auto& caps = Glob::EncodeCaps::Get(global);
        if (IsOn(m_EncToolConfig.AdaptiveMBQP)) {

            // Allocate memory for segmentation, if not allocated yet
            if (m_pSegmentQPMap == nullptr)
                AllocSegmentationData(par.mfx.FrameInfo.Width, par.mfx.FrameInfo.Height, caps.MinSegIdBlockSizeAccepted);

            // Get QP map
            qpMapHint.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_HINT_QPMAP;
            qpMapHint.Header.BufferSz = sizeof(qpMapHint);
            qpMapHint.ExtQpMap.BlockSize = m_SegmentationInfo.SegmentIdBlockSize;
            qpMapHint.ExtQpMap.QP = m_pSegmentQPMap;
            qpMapHint.ExtQpMap.Mode = MFX_MBQP_MODE_QP_VALUE;
            qpMapHint.QpMapPitch = (mfxU16)((par.mfx.FrameInfo.Width + m_SegmentationInfo.SegmentIdBlockSize - 1) / m_SegmentationInfo.SegmentIdBlockSize);
            qpMapHint.ExtQpMap.NumQPAlloc = m_SegmentationInfo.NumSegmentIdAlloc;
            extParams.push_back(&qpMapHint.Header);

            // Get segmentIDs and AltQIndexes
            seg_enctools.Header.BufferId = MFX_EXTBUFF_AV1_SEGMENTATION;
            seg_enctools.Header.BufferSz = sizeof(seg_enctools);
            seg_enctools.SegmentIdBlockSize = m_SegmentationInfo.SegmentIdBlockSize;
            seg_enctools.NumSegments = m_SegmentationInfo.NumSegments;
            seg_enctools.NumSegmentIdAlloc = m_SegmentationInfo.NumSegmentIdAlloc;
            seg_enctools.SegmentIds = m_pSegmentIDMap;
            extParams.push_back(&seg_enctools.Header);
        }

        task_par.NumExtParam = (mfxU16)extParams.size();
        task_par.ExtParam = extParams.data();

        auto sts = m_pEncTools->Query(m_pEncTools->Context, &task_par, ENCTOOLS_QUERY_TIMEOUT);
        MFX_CHECK_STS(sts);

        task.bCUQPMap = qpMapHint.QpMapFilled ? true : false;

        mfxExtAV1Segmentation& seg_task = Task::Segment::Get(s_task);

        // sanity check:  segmentId must be in [0,6]
        if (IsOn(m_EncToolConfig.AdaptiveMBQP)) {
            if (task.bCUQPMap) {
                mfxU8 max_id = *(std::max_element(seg_enctools.SegmentIds, seg_enctools.SegmentIds + seg_enctools.NumSegmentIdAlloc ));
                MFX_CHECK(max_id <= 6, MFX_ERR_UNDEFINED_BEHAVIOR);

                // Enctools does not modify segment 7.  So copy segment 7
                seg_enctools.Segment[7].FeatureEnabled = seg_task.Segment[7].FeatureEnabled;
                seg_enctools.NumSegments = 8;

                // Xe-Hpm limitation
                if (AV1ECaps::IsSegmentationHWLimitationNeeded(Glob::VideoCore::Get(global).GetHWType()))
                    EnableFeature(seg_enctools.Segment[AV1_MAX_NUM_OF_SEGMENTS -1].FeatureEnabled, Base::SEG_LVL_SKIP);

            } else {
                // If this frame does not use PAQ, set below flags for segmentation module to disable segmentation.
                seg_enctools.NumSegmentIdAlloc = 0;
                seg_enctools.NumSegments = 0;                
            }

            // These function calls are needed even if PAQ is not used for this frame.
            auto& dpb = Glob::SegDpb::Get(global);
            auto& fh = Task::FH::Get(s_task);
            const mfxExtAV1AuxData& auxData = ExtBuffer::Get(par);
            const auto& numRefFrame         = Glob::VideoParam::Get(global).mfx.NumRefFrame;
            const auto& refreshFrameFlags   = Task::Common::Get(s_task).RefreshFrameFlags;

            MFX_CHECK_STS(Base::Segmentation::UpdateSegmentBuffers(seg_task, &seg_enctools));
            MFX_CHECK_STS(Segmentation::PostUpdateSegmentParam(seg_task, fh, dpb, auxData, numRefFrame, refreshFrameFlags));

            const auto& defchain           = Glob::Defaults::Get(global);
            const Defaults::Param& defPar  = Defaults::Param(par, caps, defchain);
            MFX_CHECK_STS(Segmentation::CheckAndFixSegmentBuffers(&(Task::Segment::Get(s_task)), defPar));

            //HW limitation
            if (task.bCUQPMap) {
                if (AV1ECaps::IsSegmentationHWLimitationNeeded(Glob::VideoCore::Get(global).GetHWType()))
                    ApplyHWLimitation_XeHpm(global, s_task, seg_enctools);
            }
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus AV1EncTools::QueryPreEncTask(StorageW&  /*global*/, StorageW& s_task)
{
    MFX_CHECK(m_pEncTools && m_pEncTools->Query, MFX_ERR_NONE);

    auto& task = Task::Common::Get(s_task);

    mfxEncToolsTaskParam task_par = {};
    std::vector<mfxExtBuffer*> extParams;
    mfxEncToolsHintPreEncodeSceneChange preEncodeSChg = {};
    mfxEncToolsHintPreEncodeGOP preEncodeGOP = {};
    mfxEncToolsBRCBufferHint bufHint = {};
    mfxEncToolsHintQuantMatrix cqmHint = {};
    mfxEncToolsHintQPMap qpMapHint = {};
    mfxEncToolsHintPreEncodeARefFrames aRef = {};

    bool isLABRC = (m_EncToolCtrl.ScenarioInfo == MFX_SCENARIO_UNKNOWN &&
                     IsOn(m_EncToolConfig.BRCBufferHints) &&
                     IsOn(m_EncToolConfig.BRC));

    MFX_CHECK(task.DisplayOrder!= (mfxU32)(-1), MFX_ERR_NONE);
    task_par.DisplayOrder = task.DisplayOrder;
    task_par.NumExtParam = 0;

    if (IsOn(m_EncToolConfig.BRC))
    {
        preEncodeSChg.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_HINT_SCENE_CHANGE;
        preEncodeSChg.Header.BufferSz = sizeof(preEncodeSChg);
        extParams.push_back(&preEncodeSChg.Header);
    }

    if (IsOn(m_EncToolConfig.AdaptiveI) ||
        IsOn(m_EncToolConfig.AdaptiveB) ||
        IsOn(m_EncToolConfig.AdaptivePyramidQuantP) ||
        IsOn(m_EncToolConfig.AdaptivePyramidQuantB))
    {
        preEncodeGOP.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_HINT_GOP;
        preEncodeGOP.Header.BufferSz = sizeof(preEncodeGOP);
        extParams.push_back(&preEncodeGOP.Header);
    }

#if defined MFX_ENABLE_ENCTOOLS_LPLA
    if (IsOn(m_EncToolConfig.AdaptiveQuantMatrices))
    {
        cqmHint.MatrixType = CQM_HINT_USE_FLAT_MATRIX;
        cqmHint.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_HINT_MATRIX;
        cqmHint.Header.BufferSz = sizeof(cqmHint);
        extParams.push_back(&cqmHint.Header);
    }
#endif

    if (IsOn(m_EncToolConfig.BRCBufferHints))
    {
        bufHint.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_BRC_BUFFER_HINT;
        bufHint.Header.BufferSz = sizeof(bufHint);
        bufHint.OutputMode = mfxU16(isLABRC ? MFX_BUFFERHINT_OUTPUT_DISPORDER : MFX_BUFFERHINT_OUTPUT_ENCORDER);
        extParams.push_back(&bufHint.Header);
    }

    if (IsOn(m_EncToolConfig.AdaptiveLTR))
    {
        aRef.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_HINT_AREF;
        aRef.Header.BufferSz = sizeof(aRef);
        extParams.push_back(&aRef.Header);
    }

    task_par.ExtParam = extParams.data();
    task_par.NumExtParam = (mfxU16)extParams.size();
    MFX_CHECK(task_par.NumExtParam, MFX_ERR_NONE);

    auto sts = m_pEncTools->Query(m_pEncTools->Context, &task_par, ENCTOOLS_QUERY_TIMEOUT);
    if (sts == MFX_ERR_MORE_DATA) sts = MFX_ERR_NONE;
    MFX_CHECK_STS(sts);

    task.GopHints.MiniGopSize = preEncodeGOP.MiniGopSize;
    task.GopHints.FrameType = preEncodeGOP.FrameType;
    task.GopHints.SceneChange = preEncodeSChg.SceneChangeFlag;
    task.GopHints.SpatialComplexity = preEncodeSChg.SpatialComplexity;
    task.GopHints.PersistenceMapNZ = preEncodeSChg.PersistenceMapNZ;
    if (preEncodeSChg.PersistenceMapNZ)
        memcpy(task.GopHints.PersistenceMap, preEncodeSChg.PersistenceMap, sizeof(task.GopHints.PersistenceMap));

#if defined(MFX_ENABLE_ENCTOOLS_LPLA)
    if (IsOn(m_EncToolConfig.BRCBufferHints) && !isLABRC)
    {
        //Low power look ahead with HW BRC
        mfxLplastatus laStatus = {};
        laStatus.QpModulation = (mfxU8)preEncodeGOP.QPModulation;
        laStatus.MiniGopSize = (mfxU8)preEncodeGOP.MiniGopSize;


        switch (cqmHint.MatrixType)
        {
        case MFX_QUANT_MATRIX_WEAK:
            laStatus.CqmHint = CQM_HINT_USE_CUST_MATRIX1;
            break;
        case MFX_QUANT_MATRIX_MEDIUM:
            laStatus.CqmHint = CQM_HINT_USE_CUST_MATRIX2;
            break;
        case MFX_QUANT_MATRIX_STRONG:
            laStatus.CqmHint = CQM_HINT_USE_CUST_MATRIX3;
            break;
        case MFX_QUANT_MATRIX_EXTREME:
            laStatus.CqmHint = CQM_HINT_USE_CUST_MATRIX4;
            break;
        case MFX_QUANT_MATRIX_FLAT:
        default:
            laStatus.CqmHint = CQM_HINT_USE_FLAT_MATRIX;
        }

        laStatus.TargetFrameSize = bufHint.OptimalFrameSizeInBytes;
        LpLaStatus.push_back(laStatus); //lpLaStatus is got in encoded order
    }
    else
#endif
    {
        if (IsOn(m_EncToolConfig.AdaptivePyramidQuantB) || IsOn(m_EncToolConfig.AdaptivePyramidQuantP)) {
            task.GopHints.QPModulaton = (mfxU8)preEncodeGOP.QPModulation;
            //task.GopHints.QPModulaton = MFX_QP_MODULATION_EXPLICIT;
            //task.GopHints.QPDeltaExplicitModulation = (mfxI8)preEncodeGOP.QPDeltaExplicitModulation;
        }

        if (isLABRC)
        {
            task.BrcHints.LaAvgEncodedBits = bufHint.AvgEncodedSizeInBits;
            task.BrcHints.LaCurEncodedBits = bufHint.CurEncodedSizeInBits;
            task.BrcHints.LaDistToNextI = bufHint.DistToNextI;
        }


        if (IsOn(m_EncToolConfig.AdaptiveLTR))
        {
            InitExtBufHeader_AV1(task.InternalListCtrl);
            task.InternalListCtrlPresent = true;
            if (aRef.CurrFrameType == MFX_REF_FRAME_TYPE_LTR)
            {
                task.InternalListCtrl.LongTermRefList[0].FrameOrder = task.DisplayOrder;
                task.InternalListCtrl.LongTermRefList[0].PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            }

            for (mfxU32 i = (aRef.CurrFrameType == MFX_REF_FRAME_TYPE_LTR) ? 1 : 0; i < std::min(aRef.LongTermRefListSize, (mfxU16)16); i++)
            {
                task.InternalListCtrl.LongTermRefList[i].FrameOrder = aRef.LongTermRefList[i];
                task.InternalListCtrl.LongTermRefList[i].PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            }

            for (mfxU32 i = 0; i < std::min(aRef.PreferredRefListSize, (mfxU16)32); i++)
            {
                task.InternalListCtrl.PreferredRefList[i].FrameOrder = aRef.PreferredRefList[i];
                task.InternalListCtrl.PreferredRefList[i].PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            }

            for (mfxU32 i = 0; i < std::min(aRef.RejectedRefListSize, (mfxU16)16); i++)
            {
                task.InternalListCtrl.RejectedRefList[i].FrameOrder = aRef.RejectedRefList[i];
                task.InternalListCtrl.RejectedRefList[i].PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            }
        }
     }
    return sts;
}

mfxStatus AV1EncTools::BRCUpdate(StorageW&  , StorageW& s_task, mfxEncToolsBRCStatus & brcStatus)
{
    MFX_CHECK(IsOn(m_EncToolConfig.BRC) && m_pEncTools && m_pEncTools->Query, MFX_ERR_NONE);

    auto& task = Task::Common::Get(s_task);
    mfxEncToolsTaskParam task_par = {};
    std::vector<mfxExtBuffer*> extParams;

    MFX_CHECK(task.DisplayOrder != (mfxU32)(-1), MFX_ERR_NONE);
    task_par.DisplayOrder = task.DisplayOrder;

    {
        mfxEncToolsBRCEncodeResult extEncRes;
        extEncRes.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_BRC_ENCODE_RESULT;
        extEncRes.Header.BufferSz = sizeof(extEncRes);
        extEncRes.CodedFrameSize = task.MinFrameSize > task.BsDataLength ? task.MinFrameSize : task.BsDataLength;
        extEncRes.NumRecodesDone = task.NumRecode;
        extEncRes.QpY = task.QpY;

        extParams.push_back(&extEncRes.Header);
        task_par.NumExtParam = (mfxU16)extParams.size();
        task_par.ExtParam = extParams.data();
        auto sts = m_pEncTools->Submit(m_pEncTools->Context, &task_par);
        MFX_CHECK_STS(sts);
    }
    {
        extParams.clear();
        brcStatus = {};
        brcStatus.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_BRC_STATUS;
        brcStatus.Header.BufferSz = sizeof(brcStatus);

        extParams.push_back(&brcStatus.Header);
        task_par.NumExtParam = (mfxU16)extParams.size();
        task_par.ExtParam = extParams.data();
        auto sts = m_pEncTools->Query(m_pEncTools->Context, &task_par, ENCTOOLS_QUERY_TIMEOUT);
        MFX_CHECK_STS(sts)
    }
    return MFX_ERR_NONE;
}

void AV1EncTools::AllocSegmentationData(mfxU16 frame_width, mfxU16 frame_height, mfxU8 blockSize)
{
    mfxU16 numSegBlksInWidth = (mfxU16)((frame_width + blockSize - 1) / blockSize);
    mfxU16 numSegBlksIHeight = (mfxU16)((frame_height + blockSize - 1) / blockSize);

    m_SegmentationInfo.NumSegmentIdAlloc = numSegBlksInWidth * numSegBlksIHeight;
    m_SegmentationInfo.SegmentIdBlockSize = blockSize;
    m_SegmentationInfo.NumSegments = 8;   //enctools needs all 8 segments

    m_pSegmentQPMap = new mfxU8[m_SegmentationInfo.NumSegmentIdAlloc];
    m_pSegmentIDMap = new mfxU8[m_SegmentationInfo.NumSegmentIdAlloc];
}

void AV1EncTools::ReleaseSegmentationData(void)
{
    if (m_pSegmentQPMap) {
        delete [] m_pSegmentQPMap;
        m_pSegmentQPMap = nullptr;
    }

    if (m_pSegmentIDMap) {
        delete [] m_pSegmentIDMap;
        m_pSegmentIDMap = nullptr;
    }

}

void AV1EncTools::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
     Push(BLK_Init
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(strg);
        MFX_CHECK(IsFeatureEnabled(par), MFX_ERR_NONE);

        auto& caps = Glob::EncodeCaps::Get(strg);
        bool bEncTools = IsEncToolsOn(par);
        MFX_CHECK(bEncTools, MFX_ERR_NONE);
        MFX_CHECK(!m_pEncTools, MFX_ERR_NONE);

        m_pEncTools = GetEncTools(par);
        mfxEncToolsCtrlExtDevice    extBufDevice = {};
        mfxEncToolsCtrlExtAllocator extBufAlloc = {};
        mfxExtBuffer* ExtParam[2] = {};

        memset(&m_EncToolCtrl, 0, sizeof(mfxEncToolsCtrl));
        auto vaType = Glob::VideoCore::Get(strg).GetVAType();

        if (vaType!= MFX_HW_NO)
        {
            mfxFrameAllocator *pFrameAlloc = nullptr;
            mfxHDL device_handle = {};
            mfxHandleType h_type = GetHandleType(vaType);
            auto sts = Glob::VideoCore::Get(strg).GetHandle(h_type, &device_handle);
            MFX_CHECK_STS(sts);
            pFrameAlloc = (mfxFrameAllocator*)Glob::VideoCore::Get(strg).QueryCoreInterface(MFXIEXTERNALLOC_GUID);//(&frameAlloc);

            InitEncToolsCtrlExtDevice(extBufDevice, h_type, device_handle);
            if (pFrameAlloc)
                InitEncToolsCtrlExtAllocator(extBufAlloc, *pFrameAlloc);

            ExtParam[0] = &extBufDevice.Header;
            ExtParam[1] = &extBufAlloc.Header;
            m_EncToolCtrl.ExtParam = ExtParam;
            m_EncToolCtrl.NumExtParam = 2;
        }

        auto sts = InitEncToolsCtrl(par, &m_EncToolCtrl);
        MFX_CHECK_STS(sts);

        m_bEncToolsInner = false;
        if (!(m_pEncTools && m_pEncTools->Context))
        {
            m_pEncTools = MFXVideoENCODE_CreateEncTools(par);
            m_bEncToolsInner = !!m_pEncTools;
        }

        m_destroy = [this]()
        {
            if (m_bEncToolsInner)
                MFXVideoENCODE_DestroyEncTools(m_pEncTools);
            m_bEncToolsInner = false;

            ReleaseSegmentationData();
        };

        if (m_pEncTools)
        {
            mfxExtEncToolsConfig supportedConfig = {};

            m_pEncTools->GetSupportedConfig(m_pEncTools->Context, &supportedConfig, &m_EncToolCtrl);

            if (CorrectVideoParams(par, supportedConfig))
                MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            SetDefaultConfig(par, m_EncToolConfig, caps.ForcedSegmentationSupport);

#if defined (ONEVPL_EXPERIMENTAL)
            const mfxExtTuneEncodeQuality& tuneVQ = ExtBuffer::Get(par);
            m_enablePercEncPrefilter = 
                tuneVQ.TuneQuality & MFX_ENCODE_TUNE_VMAF ||
                tuneVQ.TuneQuality & MFX_ENCODE_TUNE_PERCEPTUAL;
#endif

            if(m_enablePercEncPrefilter)
            {
                m_EncToolConfig.SaliencyMapHint = MFX_CODINGOPTION_ON;
                m_saliencyMapSupported = false;
            }

            sts = m_pEncTools->Init(m_pEncTools->Context, &m_EncToolConfig, &m_EncToolCtrl);
            if(m_enablePercEncPrefilter && sts == MFX_ERR_UNSUPPORTED)
            {
                m_EncToolConfig.SaliencyMapHint = MFX_CODINGOPTION_OFF;
                sts = m_pEncTools->Init(m_pEncTools->Context, &m_EncToolConfig, &m_EncToolCtrl);
            }
            MFX_CHECK_STS(sts);

            m_saliencyMapSupported = (m_EncToolConfig.SaliencyMapHint == MFX_CODINGOPTION_ON);

            sts = m_pEncTools->GetActiveConfig(m_pEncTools->Context, &m_EncToolConfig);
            MFX_CHECK_STS(sts);

            m_pEncTools->GetDelayInFrames(m_pEncTools->Context, &m_EncToolConfig, &m_EncToolCtrl, &m_maxDelay);

            auto& tm = Glob::TaskManager::Get(strg).m_tm;

            S_ET_SUBMIT = tm.AddStage(S_NEW);
            S_ET_QUERY = tm.AddStage(S_ET_SUBMIT);

            if(m_enablePercEncPrefilter)
            {
                mfxU16 frameType = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET;

                CommonCORE_VPL* core = dynamic_cast <CommonCORE_VPL*>(&Glob::VideoCore::Get(strg));
                MFX_CHECK(core, MFX_ERR_UNKNOWN);

                m_filteredFrameCache.reset(SurfaceCache::Create(*core, frameType, par.mfx.FrameInfo));
            }
        }

        return MFX_ERR_NONE;
    });


    // Add S_ET_SUBMIT and S_ET_QUERY stages for EncTools
    Push(BLK_AddTask
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(IsFeatureEnabled(par), MFX_ERR_NONE);

        MFX_CHECK(S_ET_SUBMIT != mfxU16(-1) && S_ET_QUERY != mfxU16(-1), MFX_ERR_NONE);
        auto& tm = Glob::TaskManager::Get(global).m_tm;

        auto  ETSubmit = [&](
            MfxEncodeHW::TaskManager::TAsyncStage::TExt
            , StorageW& global
            , StorageW& /*s_task*/) -> mfxStatus
        {
            if (tm.m_nRecodeTasks)
            {
                return MFX_ERR_NONE;
            }

            if (StorageW* pTask = tm.GetTask(tm.Stage(S_ET_SUBMIT)))
            {
                SubmitPreEncTask(global, *pTask);
                tm.MoveTaskForward(tm.Stage(S_ET_SUBMIT), tm.FixedTask(*pTask));
            }

            return MFX_ERR_NONE;
        };

        auto  ETQuery = [&](
            MfxEncodeHW::TaskManager::TAsyncStage::TExt
            , StorageW& /*global*/
            , StorageW& s_task) -> mfxStatus
        {
            if (tm.m_nRecodeTasks)
            {
                return MFX_ERR_NONE;
            }

            bool bFlush = !tm.IsInputTask(s_task);
            // Delay For LookAhead Depth
            MFX_CHECK(tm.m_stages.at(tm.Stage(S_ET_QUERY)).size() >= std::max(m_maxDelay,1U)  || bFlush,MFX_ERR_NONE);

            StorageW* pTask = tm.GetTask(tm.Stage(S_ET_QUERY));
            MFX_CHECK(pTask, MFX_ERR_NONE);
            auto sts = QueryPreEncTask(global, *pTask);
            MFX_CHECK_STS(sts);

            tm.MoveTaskForward(tm.Stage(S_ET_QUERY), tm.FixedTask(*pTask));
            return MFX_ERR_NONE;
        };

        tm.m_AsyncStages[tm.Stage(S_ET_SUBMIT)].Push(ETSubmit);
        tm.m_AsyncStages[tm.Stage(S_ET_QUERY)].Push(ETQuery);

        // Extend Num of tasks and size of buffer.
        tm.m_ResourceExtra += (mfxU16)m_maxDelay;

        return MFX_ERR_NONE;
    });

    Push(BLK_UpdateTask
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        MFX_CHECK(m_pEncTools, MFX_ERR_NONE);
        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(IsFeatureEnabled(par), MFX_ERR_NONE);

        auto& tm = Glob::TaskManager::Get(global).m_tm;

        auto  UpdateTask = [&](
            MfxEncodeHW::TaskManager::TUpdateTask::TExt
            , StorageW&  global
            , StorageW* dstTask) -> mfxStatus
        {
            global; dstTask;
#if defined MFX_ENABLE_ENCTOOLS_LPLA
            if (dstTask)
            {
                auto& dst_task = Task::Common::Get(*dstTask);
                if (LpLaStatus.size() > 0)
                {
                    auto& par = Glob::VideoParam::Get(global);
                    const mfxExtCodingOption3& CO3 = ExtBuffer::Get(par);
                    auto& fh = Task::FH::Get(*dstTask);

                    dst_task.LplaStatus = *(LpLaStatus.begin());

                    if (IsOn(CO3.AdaptiveCQM))
                    {
                        fh.quantization_params.using_qmatrix = IS_CUST_MATRIX(dst_task.LplaStatus.CqmHint) ? 1 : 0;

                        if (fh.quantization_params.using_qmatrix)
                        {
                            // best mapping scheme for PSNR metric
                            const mfxU8 qMatrixYIdx[CQM_HINT_NUM_CUST_MATRIX] = { 10, 6,  3,  0 };
                            mfxU8 idx = dst_task.LplaStatus.CqmHint >= CQM_HINT_USE_CUST_MATRIX1 ?
                                dst_task.LplaStatus.CqmHint - CQM_HINT_USE_CUST_MATRIX1 : 0;

                            fh.quantization_params.qm_y = qMatrixYIdx[idx];
                            fh.quantization_params.qm_u = std::min(fh.quantization_params.qm_y + 8, 15U);
                            fh.quantization_params.qm_v = fh.quantization_params.qm_u;
                        }
                    }
                    LpLaStatus.pop_front();
                }
            }
#endif
            return MFX_ERR_NONE;
        };

        tm.UpdateTask.Push(UpdateTask);

        return MFX_ERR_NONE;
    });

    Push(BLK_SetCallChains
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
        {
            auto& par = Glob::VideoParam::Get(global);
            MFX_CHECK(IsFeatureEnabled(par), MFX_ERR_NONE);

 #if defined(MFX_ENABLE_ENCTOOLS_LPLA)
#endif
            return MFX_ERR_NONE;
        });
}

void AV1EncTools::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_GetFrameCtrl
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        MFX_CHECK(IsOn(m_EncToolConfig.BRC) && m_pEncTools && m_pEncTools->Submit, MFX_ERR_NONE);

        mfxEncToolsBRCQuantControl quantCtrl = {};
        mfxEncToolsBRCHRDPos  HRDPos = {};

        // call enctools
        BRCGetCtrl(global, s_task, quantCtrl, HRDPos);

        // Report BRC results to encoder
        auto&      task = Task::Common::Get(s_task);
        task.QpY = (mfxU8)quantCtrl.QpY;

        auto&      fh = Task::FH::Get(s_task);
        fh.quantization_params.base_q_idx = task.QpY;

        // Set deblocking filter levels
        fh.loop_filter_params.loop_filter_level[0] = LoopFilterLevelsLuma[task.QpY];
        fh.loop_filter_params.loop_filter_level[1] = LoopFilterLevelsLuma[task.QpY];
        fh.loop_filter_params.loop_filter_level[2] = LoopFilterLevelsChroma[task.QpY];
        fh.loop_filter_params.loop_filter_level[3] = LoopFilterLevelsChroma[task.QpY];

        // Set CDEF filter
        CDEF(fh);

        //filter input surface if nesessary
        if(m_enablePercEncPrefilter && task.pSurfIn){

            mfxStatus sts = MFX_ERR_NONE;

            //sanity check
            if((!task.bRecode && task.pFilteredSurface) || task.DisplayOrder == mfxU32(-1)){
                MFX_RETURN(MFX_ERR_UNKNOWN);
            }

            if(!task.pFilteredSurface){
                sts = m_filteredFrameCache -> GetSurface(task.pFilteredSurface);
                MFX_CHECK_STS(sts);
            }

            //do filtering, task.QpY is valid
            if(task.pFilteredSurface){
                mfxResourceType resType = {};
                sts = task.pFilteredSurface -> FrameInterface->GetNativeHandle(task.pFilteredSurface, &task.HDLRaw.first, &resType);
                MFX_CHECK_STS(sts);

                mfxEncToolsPrefilterParam extPrefilterParam{};
                extPrefilterParam.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_PREFILTER_PARAM;
                extPrefilterParam.Header.BufferSz = sizeof(mfxEncToolsPrefilterParam);
                extPrefilterParam.InSurface = task.pSurfIn;
                extPrefilterParam.OutSurface = task.pFilteredSurface;
                // AV1 dc_q_index to HEVC QP
                const std::array<mfxU8,256> AV1_DC_Q_IDX_2_HEVC_QP =
                {
                    0, 4, 4, 5, 6, 7, 8, 8, 8, 9, 9, 10,11,11,11,11,
                    12,12,13,13,14,14,14,14,15,15,15,16,16,16,16,16,
                    17,17,17,17,17,17,17,18,18,18,18,19,19,19,19,19,
                    20,20,20,20,20,20,20,20,21,21,21,21,21,21,21,22,
                    22,22,22,22,22,22,22,22,22,23,23,23,23,23,23,23,
                    23,23,23,23,23,24,24,24,24,24,24,24,24,24,24,24,
                    25,25,25,25,25,25,26,26,26,26,26,26,26,26,27,27,
                    27,27,27,27,27,27,27,28,28,28,28,28,28,28,29,29,
                    29,29,29,29,29,29,29,30,30,30,30,30,30,30,31,31,
                    31,31,31,31,31,31,32,32,32,32,32,32,32,32,33,33,
                    33,33,33,33,33,33,34,34,34,34,34,34,34,34,35,35,
                    35,35,35,35,35,35,35,36,36,36,36,36,36,36,36,37,
                    37,37,37,37,37,37,37,38,38,38,38,38,38,38,38,38,
                    39,39,39,39,39,39,39,39,40,40,40,40,40,40,40,41,
                    41,41,41,41,41,42,42,42,42,42,43,43,43,43,43,44,
                    44,44,44,45,45,45,45,46,46,46,46,47,47,47,48,48
                };
                extPrefilterParam.QpY = AV1_DC_Q_IDX_2_HEVC_QP.at(task.QpY);

                std::vector<mfxExtBuffer*> extParams;
                extParams.push_back(&extPrefilterParam.Header);

                if(m_saliencyMapSupported)
                {
                    extParams.push_back(&task.saliencyMap.Header);
                }

                mfxEncToolsTaskParam param{};
                param.ExtParam = extParams.data();
                param.NumExtParam = (mfxU16)extParams.size();
                param.DisplayOrder = task.DisplayOrder;
                sts = m_pEncTools->Submit(m_pEncTools->Context, &param);
                MFX_CHECK_STS(sts);
            }
        }

        return MFX_ERR_NONE;
    });
}

void AV1EncTools::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_Update
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(IsFeatureEnabled(par), MFX_ERR_NONE);

        MFX_CHECK(IsOn(m_EncToolConfig.BRC) && m_pEncTools && m_pEncTools->Query, MFX_ERR_NONE);

        auto& task = Task::Common::Get(s_task);
        MFX_CHECK(!task.bBRCUpdated && task.BsDataLength, MFX_ERR_NONE);

        mfxEncToolsBRCStatus brcSts = {};
        auto sts = BRCUpdate(global, s_task, brcSts);
        MFX_CHECK_STS(sts);
        task.bSkip = false;

        switch (brcSts.FrameStatus.BRCStatus)
        {
        case MFX_BRC_OK:
            break;
        case MFX_BRC_PANIC_SMALL_FRAME:
            task.MinFrameSize = brcSts.FrameStatus.MinFrameSize;
            task.NumRecode++;

            sts = BRCUpdate(global, s_task, brcSts);
            MFX_CHECK_STS(sts);
            MFX_CHECK(brcSts.FrameStatus.BRCStatus == MFX_BRC_OK, MFX_ERR_UNDEFINED_BEHAVIOR);
            break;
        case MFX_BRC_PANIC_BIG_FRAME:
            task.bSkip = true;
        case MFX_BRC_BIG_FRAME:
        case MFX_BRC_SMALL_FRAME:
            task.bRecode = true;
            break;
        default:
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        task.bForceSync |= task.bSkip;
        task.bBRCUpdated = !task.bRecode;

        return MFX_ERR_NONE;
    });
}

void AV1EncTools::FreeTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_Discard
        , [this](StorageW& /*global*/, StorageW& s_task)->mfxStatus
    {
        MFX_CHECK(m_pEncTools && m_pEncTools->Discard, MFX_ERR_NONE);

        auto& task = Task::Common::Get(s_task);

        if(task.pFilteredSurface)
        {
            mfxStatus sts = task.pFilteredSurface->FrameInterface->Release(task.pFilteredSurface);
            task.pFilteredSurface = nullptr;
            std::ignore = MFX_STS_TRACE(sts);
        }

        return m_pEncTools->Discard(m_pEncTools->Context, task.DisplayOrder);
    });
}

bool AV1EncTools::IsFeatureEnabled(const mfxVideoParam& par)
{
    return true;
}

#endif //defined(MFX_ENABLE_ENCTOOLS)
#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)