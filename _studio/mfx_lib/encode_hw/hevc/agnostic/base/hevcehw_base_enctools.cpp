// Copyright (c) 2020-2021 Intel Corporation
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
#if defined(MFX_ENABLE_ENCTOOLS)

#include "hevcehw_base.h"
#include "hevcehw_base_data.h"
#include "hevcehw_base_enctools.h"
#include "hevcehw_base_task.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;

void HevcEncTools::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_ENCTOOLS_CONFIG].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        auto& buf_src = *(const mfxExtEncToolsConfig*)pSrc;
        auto& buf_dst = *(mfxExtEncToolsConfig*)pDst;

        MFX_COPY_FIELD(AdaptiveI);
        MFX_COPY_FIELD(AdaptiveB);
        MFX_COPY_FIELD(AdaptiveLTR);
        MFX_COPY_FIELD(AdaptivePyramidQuantB);
        MFX_COPY_FIELD(AdaptivePyramidQuantP);
        MFX_COPY_FIELD(AdaptiveQuantMatrices);
        MFX_COPY_FIELD(AdaptiveRefB);
        MFX_COPY_FIELD(AdaptiveRefP);
        MFX_COPY_FIELD(BRC);
        MFX_COPY_FIELD(BRCBufferHints);
        MFX_COPY_FIELD(SceneChange);
    });
    blocks.m_ebCopySupported[MFX_EXTBUFF_CODING_OPTION2].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtCodingOption2*)pSrc;
        auto& buf_dst = *(mfxExtCodingOption2*)pDst;
        MFX_COPY_FIELD(AdaptiveI);
        MFX_COPY_FIELD(AdaptiveB);
        MFX_COPY_FIELD(LookAheadDepth);
    });
    blocks.m_ebCopySupported[MFX_EXTBUFF_CODING_OPTION3].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtCodingOption3*)pSrc;
        auto& buf_dst = *(mfxExtCodingOption3*)pDst;
        MFX_COPY_FIELD(ExtBrcAdaptiveLTR);
    });
}

void HevcEncTools::SetInherited(ParamInheritance& par)
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
        InheritOption(buf_src.AdaptiveLTR, buf_dst.AdaptiveLTR);
        InheritOption(buf_src.AdaptivePyramidQuantB, buf_dst.AdaptivePyramidQuantB);
        InheritOption(buf_src.AdaptivePyramidQuantP, buf_dst.AdaptivePyramidQuantP);
        InheritOption(buf_src.AdaptiveQuantMatrices, buf_dst.AdaptiveQuantMatrices);
        InheritOption(buf_src.AdaptiveRefB, buf_dst.AdaptiveRefB);
        InheritOption(buf_src.AdaptiveRefP, buf_dst.AdaptiveRefP);
        InheritOption(buf_src.BRC, buf_dst.BRC);
        InheritOption(buf_src.BRCBufferHints, buf_dst.BRCBufferHints);
        InheritOption(buf_src.SceneChange, buf_dst.SceneChange);
    });
}

inline bool IsLookAhead(const mfxExtEncToolsConfig &config, bool bGameStreaming)
{
    if (!bGameStreaming)
        return false;
    return
        (IsOn(config.AdaptiveI)
        || IsOn(config.AdaptiveB)
        || IsOn(config.SceneChange)
        || IsOn(config.AdaptivePyramidQuantP)
        || IsOn(config.BRCBufferHints)
        || IsOn(config.AdaptiveQuantMatrices));
}

inline bool IsAdaptiveGOP(const mfxExtEncToolsConfig &config)
{
    return (IsOn(config.AdaptiveI)
        ||  IsOn(config.AdaptiveB));
}

inline bool IsAdaptiveQP(const mfxExtEncToolsConfig &config)
{
    return
        (IsOn(config.AdaptivePyramidQuantP)
        ||   IsOn(config.AdaptivePyramidQuantB));
}

inline bool IsAdaptiveRef(const mfxExtEncToolsConfig &config)
{
    return
        (IsOn(config.AdaptiveRefP)
        || IsOn(config.AdaptiveRefB));
}

inline bool IsAdaptiveLTR(const mfxExtEncToolsConfig &config)
{
    return IsOn(config.AdaptiveLTR);
}

inline bool IsBRC(const mfxExtEncToolsConfig &config)
{
    return
        IsOn(config.BRC);
}

bool HEVCEHW::Base::IsEncToolsOptOn(const mfxExtEncToolsConfig &config, bool bGameStreaming)
{
    return
        (IsAdaptiveGOP(config)
            || IsAdaptiveQP(config)
            || IsAdaptiveRef(config)
            || IsAdaptiveLTR(config)
            || IsBRC(config)
            || IsLookAhead(config,bGameStreaming));
}

bool HEVCEHW::Base::IsLPLAEncToolsOn(const mfxExtEncToolsConfig &config, bool bGameStreaming)
{
    return IsLookAhead(config, bGameStreaming);
}

inline mfxU32 GetNumTempLayers(mfxVideoParam &video)
{
    mfxExtAvcTemporalLayers* tempLayers = ExtBuffer::Get(video);

    if (tempLayers == nullptr)
        return 0;

    mfxU32 numTemporalLayer = 0;
    if (tempLayers)
    {
        for (mfxU32 i = 0; i < 8; i++)
        {
            if (tempLayers->Layer[i].Scale != 0)
                numTemporalLayer++;
        }
    }
    return numTemporalLayer;
}

inline bool IsAdaptiveRefAllowed(mfxVideoParam &video)
{
    mfxExtCodingOption3 *pExtOpt3 = ExtBuffer::Get(video);
    bool adaptiveRef = (video.mfx.TargetUsage != 7);
    if (pExtOpt3)
    {
        adaptiveRef = adaptiveRef && !(pExtOpt3->NumRefActiveP[0] == 1 || (video.mfx.GopRefDist > 1 && pExtOpt3->NumRefActiveBL0[0] == 1));
        adaptiveRef = adaptiveRef && !IsOff(pExtOpt3->ExtBrcAdaptiveLTR);
    }
    return adaptiveRef;
}

inline bool CheckEncToolsCondition(mfxVideoParam &video)
{
    return ((video.mfx.FrameInfo.PicStruct == 0
        || video.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
        && GetNumTempLayers(video) <= 1);
}

inline bool  CheckSWEncCondition(mfxVideoParam &video)
{
    return (
        CheckEncToolsCondition(video)
        && (video.mfx.GopRefDist == 0
            || video.mfx.GopRefDist == 1
            || video.mfx.GopRefDist == 2
            || video.mfx.GopRefDist == 4
            || video.mfx.GopRefDist == 8));
}

inline void SetDefaultOpt(mfxU16 &opt, bool bCond)
{
    SetDefault(opt, bCond ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
}

inline mfxEncTools *GetEncTools(mfxVideoParam &video)
{
    return (mfxEncTools *)mfx::GetExtBuffer(video.ExtParam, video.NumExtParam, MFX_EXTBUFF_ENCTOOLS);
}

inline bool IsEncToolsImplicit(const mfxVideoParam &video)
{
    const mfxExtCodingOption2  *pExtOpt2 = ExtBuffer::Get(video);
    return ((video.mfx.GopRefDist == 2 || video.mfx.GopRefDist == 8) && pExtOpt2 && IsOn(pExtOpt2->ExtBRC) && pExtOpt2->LookAheadDepth > 0);
}

static void SetDefaultConfig(mfxVideoParam &video, mfxExtEncToolsConfig &config)
{
    mfxExtCodingOption2  *pExtOpt2 = ExtBuffer::Get(video);
    mfxExtCodingOption3  *pExtOpt3 = ExtBuffer::Get(video);
    mfxExtEncToolsConfig *pExtConfig = ExtBuffer::Get(video);
    bool bGameStreaming = pExtOpt3 && pExtOpt3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING;

    if (!pExtConfig || !IsEncToolsOptOn(*pExtConfig, bGameStreaming))
    {
        if (IsEncToolsImplicit(video)
            && !(pExtOpt3 && pExtOpt3->ScenarioInfo != MFX_SCENARIO_UNKNOWN))
        {
            config.AdaptiveI             = mfxU16((pExtConfig && IsOff(pExtConfig->AdaptiveI))             ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_UNKNOWN);
            config.AdaptiveB             = mfxU16((pExtConfig && IsOff(pExtConfig->AdaptiveB))             ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_UNKNOWN);
            config.AdaptivePyramidQuantP = mfxU16((pExtConfig && IsOff(pExtConfig->AdaptivePyramidQuantP)) ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_UNKNOWN);
            config.AdaptivePyramidQuantB = mfxU16((pExtConfig && IsOff(pExtConfig->AdaptivePyramidQuantB)) ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_UNKNOWN);
            config.AdaptiveRefP          = mfxU16((pExtConfig && IsOff(pExtConfig->AdaptiveRefP))          ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_UNKNOWN);
            config.AdaptiveRefB          = mfxU16((pExtConfig && IsOff(pExtConfig->AdaptiveRefB))          ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_UNKNOWN);
            config.AdaptiveLTR           = mfxU16((pExtConfig && IsOff(pExtConfig->AdaptiveLTR))           ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_UNKNOWN);
            config.BRCBufferHints        = mfxU16((pExtConfig && IsOff(pExtConfig->BRCBufferHints))        ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_UNKNOWN);
            config.BRC                   = mfxU16((pExtConfig && IsOff(pExtConfig->BRC))                   ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_UNKNOWN);
            config.AdaptiveQuantMatrices = mfxU16((pExtConfig && IsOff(pExtConfig->AdaptiveQuantMatrices)) ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_UNKNOWN);
            config.SceneChange           = mfxU16((pExtConfig && IsOff(pExtConfig->SceneChange))           ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_UNKNOWN);
        }
        else
        {
            config.AdaptiveI = MFX_CODINGOPTION_OFF;
            config.AdaptiveB = MFX_CODINGOPTION_OFF;
            config.AdaptivePyramidQuantP = MFX_CODINGOPTION_OFF;
            config.AdaptivePyramidQuantB = MFX_CODINGOPTION_OFF;
            config.AdaptiveRefP = MFX_CODINGOPTION_OFF;
            config.AdaptiveRefB = MFX_CODINGOPTION_OFF;
            config.AdaptiveLTR = MFX_CODINGOPTION_OFF;
            config.BRCBufferHints = MFX_CODINGOPTION_OFF;
            config.BRC = MFX_CODINGOPTION_OFF;
            config.AdaptiveQuantMatrices = MFX_CODINGOPTION_OFF;
            config.SceneChange = MFX_CODINGOPTION_OFF;
            return;
        }
    }
    else
        config = *pExtConfig;

    if (!bGameStreaming)
    {
        if (CheckSWEncCondition(video))
        {
            bool bAdaptiveI = !(pExtOpt2 && IsOff(pExtOpt2->AdaptiveI)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT);
            SetDefaultOpt(config.AdaptiveI, bAdaptiveI);
            SetDefaultOpt(config.AdaptiveB, !(pExtOpt2 && IsOff(pExtOpt2->AdaptiveB)));
            SetDefaultOpt(config.AdaptivePyramidQuantP, false);
            SetDefaultOpt(config.AdaptivePyramidQuantB, false);

            bool bAdaptiveRef = IsAdaptiveRefAllowed(video);
            SetDefaultOpt(config.AdaptiveRefP, bAdaptiveRef);
            SetDefaultOpt(config.AdaptiveRefB, bAdaptiveRef);
            SetDefaultOpt(config.AdaptiveLTR, bAdaptiveRef);
        }
        SetDefaultOpt(config.BRC, (video.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
            video.mfx.RateControlMethod == MFX_RATECONTROL_VBR));

        bool lplaAssistedBRC = IsOn(config.BRC) && pExtOpt2 && (pExtOpt2->LookAheadDepth > video.mfx.GopRefDist);
        SetDefaultOpt(config.BRCBufferHints, lplaAssistedBRC);
    }
#ifdef MFX_ENABLE_ENCTOOLS_LPLA
    else
    {
        bool bLA = (pExtOpt2 && pExtOpt2->LookAheadDepth > 0 &&
            (video.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
                video.mfx.RateControlMethod == MFX_RATECONTROL_VBR));
        // LPLA assumes reordering for I frames, doesn't make much sense with closed GOP
        bool bAdaptiveI = !(pExtOpt2 && IsOff(pExtOpt2->AdaptiveI)) && !(video.mfx.GopOptFlag & (MFX_GOP_STRICT | MFX_GOP_CLOSED));

       SetDefaultOpt(config.BRCBufferHints, bLA);
       SetDefaultOpt(config.AdaptivePyramidQuantP, bLA);
       SetDefaultOpt(config.AdaptivePyramidQuantB, bLA);
       SetDefaultOpt(config.AdaptiveQuantMatrices, bLA);
       SetDefaultOpt(config.AdaptiveI, bLA && bAdaptiveI);
       SetDefaultOpt(config.AdaptiveB, bLA);
    }
#endif
}

inline mfxU32 CheckFlag(mfxU16 & flag, bool bCond)
{
    return CheckOrZero<mfxU16>(flag
        , mfxU16(MFX_CODINGOPTION_UNKNOWN)
        , mfxU16(MFX_CODINGOPTION_OFF)
        , mfxU16(MFX_CODINGOPTION_ON * (bCond)));
}

static mfxU32 CorrectVideoParams(mfxVideoParam & video, mfxExtEncToolsConfig & supportedConfig)
{
    mfxExtCodingOption2   *pExtOpt2 = ExtBuffer::Get(video);
    mfxExtCodingOption3   *pExtOpt3 = ExtBuffer::Get(video);
    mfxExtBRC             *pBRC = ExtBuffer::Get(video);
    mfxExtEncToolsConfig  *pConfig = ExtBuffer::Get(video);

    bool bIsEncToolsEnabled = CheckEncToolsCondition(video);
    mfxU32 changed = 0;

#ifdef MFX_ENABLE_ENCTOOLS_LPLA
    if (pExtOpt3 && pExtOpt3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING && video.mfx.GopRefDist > 4) {
        changed++;
        video.mfx.GopRefDist = 4;
    }
#endif

    if (pConfig)
    {
        bool bAdaptiveI = !(pExtOpt2 && IsOff(pExtOpt2->AdaptiveI)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT);
#ifdef MFX_ENABLE_ENCTOOLS_LPLA
        if (pExtOpt3 && pExtOpt3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING)
        {
            // LPLA assumes reordering for I frames, doesn't make much sense with closed GOP
            bAdaptiveI = bAdaptiveI && !(video.mfx.GopOptFlag & MFX_GOP_CLOSED);
        }
#endif
        bool bAdaptiveB = !(pExtOpt2 && IsOff(pExtOpt2->AdaptiveB));
        bool bAdaptiveRef = IsAdaptiveRefAllowed(video);

        changed += CheckFlag(pConfig->AdaptiveI, bIsEncToolsEnabled && bAdaptiveI);
        changed += CheckFlag(pConfig->AdaptiveB, bIsEncToolsEnabled && bAdaptiveB);
        changed += CheckFlag(pConfig->AdaptivePyramidQuantB, bIsEncToolsEnabled);
        changed += CheckFlag(pConfig->AdaptivePyramidQuantP, bIsEncToolsEnabled);

        changed += CheckFlag(pConfig->AdaptiveRefP, bIsEncToolsEnabled  && bAdaptiveRef);
        changed += CheckFlag(pConfig->AdaptiveRefB, bIsEncToolsEnabled  && bAdaptiveRef);
        changed += CheckFlag(pConfig->AdaptiveLTR, bIsEncToolsEnabled  && bAdaptiveRef);
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
        changed += CheckFlag(pExtOpt3->ExtBrcAdaptiveLTR, IsOn(supportedConfig.AdaptiveLTR));

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

static mfxStatus InitEncToolsCtrl(
    mfxVideoParam const & par
    , mfxEncToolsCtrl *ctrl
    , bool bMBQPSupport)
{
    MFX_CHECK_NULL_PTR1(ctrl);

    const mfxExtCodingOption  *pCO  = ExtBuffer::Get(par);
    const mfxExtCodingOption2 *pCO2 = ExtBuffer::Get(par);
    const mfxExtCodingOption3 *pCO3 = ExtBuffer::Get(par);

    ctrl->CodecId = par.mfx.CodecId;
    ctrl->CodecProfile = par.mfx.CodecProfile;
    ctrl->CodecLevel = par.mfx.CodecLevel;

    ctrl->AsyncDepth = par.AsyncDepth;

    ctrl->FrameInfo = par.mfx.FrameInfo;
    ctrl->IOPattern = par.IOPattern;
    ctrl->MaxDelayInFrames = pCO2 ? pCO2->LookAheadDepth : 0 ;
    ctrl->MBBRC = (ctrl->CodecId == MFX_CODEC_HEVC && ctrl->MaxDelayInFrames > par.mfx.GopRefDist && bMBQPSupport);

    ctrl->MaxGopSize = par.mfx.GopPicSize;
    ctrl->MaxGopRefDist = par.mfx.GopRefDist;
    ctrl->MaxIDRDist = par.mfx.GopPicSize * (par.mfx.IdrInterval + 1);
    // HEVC Defaults to CRA for IdrInterval == 0
    if (par.mfx.IdrInterval == 0 && ctrl->CodecId == MFX_CODEC_HEVC && par.mfx.GopPicSize != 0) {
        ctrl->MaxIDRDist = par.mfx.GopPicSize * (0xffff / par.mfx.GopPicSize);
    }
    ctrl->BRefType = pCO2 ? pCO2->BRefType : 0;

    ctrl->ScenarioInfo = pCO3 ? pCO3->ScenarioInfo : 0;

    ctrl->GopOptFlag = (mfxU8)par.mfx.GopOptFlag;

    // Rate control info
    mfxU32 mult = par.mfx.BRCParamMultiplier ? par.mfx.BRCParamMultiplier : 1;
    bool   BRC = (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
        par.mfx.RateControlMethod == MFX_RATECONTROL_VBR);

    ctrl->RateControlMethod = par.mfx.RateControlMethod;  //CBR, VBR, CRF,CQP

    if (!BRC)
    {
        ctrl->QPLevel[0] = par.mfx.QPI;
        ctrl->QPLevel[1] = par.mfx.QPP;
        ctrl->QPLevel[2] = par.mfx.QPB;
    }
    else
    {
        ctrl->TargetKbps = par.mfx.TargetKbps * mult;
        ctrl->MaxKbps = par.mfx.MaxKbps * mult;

        if (pCO)
        {
            ctrl->HRDConformance = MFX_BRC_NO_HRD;
            if (!IsOff(pCO->NalHrdConformance) && !IsOff(pCO->VuiNalHrdParameters))
                ctrl->HRDConformance = MFX_BRC_HRD_STRONG;
            else if (IsOn(pCO->NalHrdConformance) && IsOff(pCO->VuiNalHrdParameters))
                ctrl->HRDConformance = MFX_BRC_HRD_WEAK;
        }
        else
            ctrl->HRDConformance = MFX_BRC_HRD_STRONG;

        if (ctrl->HRDConformance)
        {
            ctrl->BufferSizeInKB   = par.mfx.BufferSizeInKB*mult;      //if HRDConformance is ON
            ctrl->InitialDelayInKB = par.mfx.InitialDelayInKB*mult;    //if HRDConformance is ON
        }
        else
        {
            ctrl->ConvergencePeriod = 0;     //if HRDConformance is OFF, 0 - the period is whole stream,
            ctrl->Accuracy = 10;              //if HRDConformance is OFF
        }

        mfxU32 maxFrameSize = pCO2 ? pCO2->MaxFrameSize : 0;
        ctrl->WinBRCMaxAvgKbps = pCO3 ? pCO3->WinBRCMaxAvgKbps*mult : 0;
        ctrl->WinBRCSize = pCO3 ? pCO3->WinBRCSize : 0;
        ctrl->MaxFrameSizeInBytes[0] = (pCO3 && pCO3->MaxFrameSizeI) ? pCO3->MaxFrameSizeI : maxFrameSize;     // MaxFrameSize limitation
        ctrl->MaxFrameSizeInBytes[1] = (pCO3 && pCO3->MaxFrameSizeP) ? pCO3->MaxFrameSizeP : maxFrameSize;
        ctrl->MaxFrameSizeInBytes[2] = maxFrameSize;

        ctrl->MinQPLevel[0] = pCO2 ? pCO2->MinQPI : 0;       //QP range  limitations
        ctrl->MinQPLevel[1] = pCO2 ? pCO2->MinQPP : 0;
        ctrl->MinQPLevel[2] = pCO2 ? pCO2->MinQPB : 0;

        ctrl->MaxQPLevel[0] = pCO2 ? pCO2->MaxQPI : 0;       //QP range limitations
        ctrl->MaxQPLevel[1] = pCO2 ? pCO2->MaxQPP : 0;
        ctrl->MaxQPLevel[2] = pCO2 ? pCO2->MaxQPB : 0;

        ctrl->PanicMode = pCO3 ? pCO3->BRCPanicMode : 0;
    }

    // LaScale here
    ctrl->LaScale = 0;
    ctrl->LaQp = 30;
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
        if (maxDim >= 720) 
        {
            ctrl->LaScale = 2;
            ctrl->LaQp = 26;
        }
    }

    return MFX_ERR_NONE;
}

void HevcEncTools::Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push)
{
    Push(BLK_Check,
        [&blocks](const mfxVideoParam&, mfxVideoParam& par, StorageW&) -> mfxStatus
    {
        mfxExtEncToolsConfig *pConfig = ExtBuffer::Get(par);
        const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
        bool bEncTools = (pConfig) ?
            IsEncToolsOptOn(*pConfig, pCO3 && pCO3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING) :
            false;
        mfxU32 changed = 0;
        MFX_CHECK(bEncTools, MFX_ERR_NONE);

        mfxEncTools *pEncTools = GetEncTools(par);
        bool bCreated = false;

        if (!pEncTools)
        {
            pEncTools = MFXVideoENCODE_CreateEncTools();
            bCreated = !!pEncTools;
        }

        mfxEncToolsCtrl ctrl = {};
        mfxExtEncToolsConfig supportedConfig = {};

        mfxStatus sts = InitEncToolsCtrl(par, &ctrl, false);
        MFX_CHECK_STS(sts);

        pEncTools->GetSupportedConfig(pEncTools->Context, &supportedConfig, &ctrl);

        changed += CorrectVideoParams(par, supportedConfig);
        if (bCreated)
            MFXVideoENCODE_DestroyEncTools(pEncTools);

        MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        return MFX_ERR_NONE;
    });
}

void HevcEncTools::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [](mfxVideoParam& par, StorageW&, StorageRW&)
    {
        mfxExtEncToolsConfig *pConfig = ExtBuffer::Get(par);
        if (pConfig)
            SetDefaultConfig(par, *pConfig);
    });
}

void HevcEncTools::Reset(const FeatureBlocks& /*blocks*/, TPushR Push)
{
    Push(BLK_ResetCheck
        , [](
            const mfxVideoParam& /*par*/
            , StorageRW& global
            , StorageRW&) -> mfxStatus
    {
        auto& init = Glob::RealState::Get(global);
        auto& parOld = Glob::VideoParam::Get(init);
        auto& parNew = Glob::VideoParam::Get(global);

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

void HevcEncTools::ResetState(const FeatureBlocks& /*blocks*/, TPushRS Push)
{
    Push(BLK_Reset
        , [this](
            StorageRW& global
            , StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);

        if (m_pEncTools && m_pEncTools->Reset)
        {
            m_EncToolCtrl = {};
            mfxExtBuffer* ExtParam;
            mfxExtEncoderResetOption rOpt = {};

            if (Glob::ResetHint::Get(global).Flags & RF_IDR_REQUIRED)
            {
                rOpt.Header.BufferId = MFX_EXTBUFF_ENCODER_RESET_OPTION;
                rOpt.Header.BufferSz = sizeof(rOpt);
                rOpt.StartNewSequence = MFX_CODINGOPTION_ON;
                ExtParam = &rOpt.Header;
                m_EncToolCtrl.NumExtParam = 1;
                m_EncToolCtrl.ExtParam = &ExtParam;
            }
            auto& caps = Glob::EncodeCaps::Get(global);
            auto sts = InitEncToolsCtrl(par, &m_EncToolCtrl, caps.MbQpDataSupport);
            MFX_CHECK_STS(sts);

            sts = m_pEncTools->Reset(m_pEncTools->Context, &m_EncToolConfig, &m_EncToolCtrl);
            MFX_CHECK_STS(sts);
        }

        return MFX_ERR_NONE;
    });
}

inline void InitEncToolsCtrlExtDevice(mfxEncToolsCtrlExtDevice & extDevice
                            , mfxHandleType h_type
                            , mfxHDL device_handle)
{
    extDevice.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_DEVICE;
    extDevice.Header.BufferSz = sizeof(mfxEncToolsCtrlExtDevice);
    extDevice.HdlType = h_type;
    extDevice.DeviceHdl = device_handle;
}

inline void InitEncToolsCtrlExtAllocator(mfxEncToolsCtrlExtAllocator & extAllocator
                            , mfxFrameAllocator &allocator)
{
    extAllocator.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_ALLOCATOR;
    extAllocator.Header.BufferSz = sizeof(mfxEncToolsCtrlExtAllocator);
    extAllocator.pAllocator = &allocator;
}

inline mfxHandleType GetHandleType(eMFXVAType vaType)
{
    switch (vaType)
    {
    case MFX_HW_D3D9:  return MFX_HANDLE_D3D9_DEVICE_MANAGER;
    case MFX_HW_VAAPI: return MFX_HANDLE_VA_DISPLAY;
    default:
      break;
    }
    return MFX_HANDLE_D3D11_DEVICE;
}

void HevcEncTools::QueryIOSurf(const FeatureBlocks&, TPushQIS Push)
{
    Push(BLK_QueryIOSurf
        , [](const mfxVideoParam& par, mfxFrameAllocRequest& req, StorageRW& /*strg*/) -> mfxStatus
    {
        const mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);
        const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
        const mfxExtEncToolsConfig  *pConfig = ExtBuffer::Get(par);
        bool bGameStreaming = pCO3 && pCO3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING;
        bool bEncToolsPreEnc = false;
        if (pConfig)
            bEncToolsPreEnc = IsEncToolsOptOn(*pConfig, bGameStreaming);

        if (bEncToolsPreEnc || IsEncToolsImplicit(par))
        {
            mfxU16 nExtraRaw = 8;
            if (pCO2)
                nExtraRaw = (mfxU16)std::max(0, 8 - pCO2->LookAheadDepth); //LA is used in base_legacy
            req.NumFrameMin += nExtraRaw;
            req.NumFrameSuggested += nExtraRaw;
        }

        return MFX_ERR_NONE;
    });
}

mfxStatus HevcEncTools::SubmitPreEncTask(StorageW&  /*global*/, StorageW& s_task)
{
    MFX_CHECK(m_pEncTools && m_pEncTools->Submit, MFX_ERR_NONE);

    mfxEncToolsTaskParam task_par = {};
    auto&      task = Task::Common::Get(s_task);
    std::vector<mfxExtBuffer*> extParams;
    mfxEncToolsFrameToAnalyze extFrameData = {};
    if (task.pSurfIn)
    {
        extFrameData.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_FRAME_TO_ANALYZE;
        extFrameData.Header.BufferSz = sizeof(extFrameData);
        extFrameData.Surface = task.pSurfIn;
        extParams.push_back(&extFrameData.Header);
        task_par.ExtParam = extParams.data();
    }
    task_par.DisplayOrder = task.DisplayOrder;
    task_par.NumExtParam = (mfxU16)extParams.size();

    auto sts = m_pEncTools->Submit(m_pEncTools->Context, &task_par);
    if (sts == MFX_ERR_MORE_DATA) sts = MFX_ERR_NONE;
    return (sts);
}

constexpr mfxU32 ENCTOOLS_QUERY_TIMEOUT = 5000;
mfxStatus HevcEncTools::BRCGetCtrl(StorageW&  , StorageW& s_task,
    mfxEncToolsBRCQuantControl &extQuantCtrl , mfxEncToolsBRCHRDPos  &extHRDPos )
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
        extFrameData.PersistenceMapNZ = task.GopHints.PersistenceMapNZ;
        memcpy(extFrameData.PersistenceMap, task.GopHints.PersistenceMap, sizeof(extFrameData.PersistenceMap));

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

        if (task.GopHints.QPModulaton)
        {
            mfxEncToolsHintPreEncodeGOP  extPreGop = {};
            extPreGop.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_HINT_GOP;
            extPreGop.Header.BufferSz = sizeof(extPreGop);
            extPreGop.QPModulation = (mfxU16)task.GopHints.QPModulaton;
            extPreGop.MiniGopSize = (mfxU16)task.GopHints.MiniGopSize;
            extParams.push_back(&extPreGop.Header);
        }

        task_par.ExtParam = extParams.data();
        task_par.NumExtParam = (mfxU16)extParams.size();

        auto sts = m_pEncTools->Submit(m_pEncTools->Context, &task_par);
        MFX_CHECK_STS(sts);
    }
    {
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

        task_par.NumExtParam = (mfxU16)extParams.size();
        task_par.ExtParam = extParams.data();

        auto sts = m_pEncTools->Query(m_pEncTools->Context, &task_par, ENCTOOLS_QUERY_TIMEOUT);
        MFX_CHECK_STS(sts);
    }
    return MFX_ERR_NONE;
 }

mfxStatus HevcEncTools::QueryPreEncTask(StorageW&  /*global*/, StorageW& s_task)
{
    MFX_CHECK(m_pEncTools && m_pEncTools->Query, MFX_ERR_NONE);

    auto& task = Task::Common::Get(s_task);

    mfxEncToolsTaskParam task_par = {};
    std::vector<mfxExtBuffer*> extParams;
    mfxEncToolsHintPreEncodeSceneChange preEncodeSChg = {};
    mfxEncToolsHintPreEncodeGOP preEncodeGOP = {};
    mfxEncToolsBRCBufferHint bufHint = {};
    mfxEncToolsHintQuantMatrix cqmHint = {};

    bool isLABRC = (m_EncToolCtrl.ScenarioInfo == MFX_SCENARIO_UNKNOWN &&
                     IsOn(m_EncToolConfig.BRCBufferHints) &&
                     IsOn(m_EncToolConfig.BRC));

    MFX_CHECK(task.DisplayOrder!= (mfxU32)(-1), MFX_ERR_NONE);
    task_par.DisplayOrder = task.DisplayOrder;
    task_par.NumExtParam = 0;

    if(IsOn(m_EncToolConfig.BRC))
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

    task_par.ExtParam = extParams.data();
    task_par.NumExtParam = (mfxU16)extParams.size();
    MFX_CHECK(task_par.NumExtParam, MFX_ERR_NONE);

    auto sts = m_pEncTools->Query(m_pEncTools->Context, &task_par, ENCTOOLS_QUERY_TIMEOUT);
    task.GopHints.MiniGopSize = preEncodeGOP.MiniGopSize;
    task.GopHints.FrameType = preEncodeGOP.FrameType;
    task.GopHints.SceneChange = preEncodeSChg.SceneChangeFlag;
    task.GopHints.PersistenceMapNZ = preEncodeSChg.PersistenceMapNZ;
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
        if (IsOn(m_EncToolConfig.AdaptivePyramidQuantB) || IsOn(m_EncToolConfig.AdaptivePyramidQuantP))
            task.GopHints.QPModulaton = (mfxU8)preEncodeGOP.QPModulation;

        if (isLABRC)
        {
            task.BrcHints.LaAvgEncodedBits = bufHint.AvgEncodedSizeInBits;
            task.BrcHints.LaCurEncodedBits = bufHint.CurEncodedSizeInBits;
            task.BrcHints.LaDistToNextI = bufHint.DistToNextI;
        }
     }

    if (sts == MFX_ERR_MORE_DATA) sts = MFX_ERR_NONE;
    MFX_CHECK_STS(sts);

    return sts;
}

mfxStatus HevcEncTools::BRCUpdate(StorageW&  , StorageW& s_task, mfxEncToolsBRCStatus & brcStatus)
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
        extEncRes.QpY = (mfxU16)task.QpY;
        extEncRes.NumRecodesDone = task.NumRecode;

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

void HevcEncTools::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
     Push(BLK_Init
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto&                       par = Glob::VideoParam::Get(strg);

        mfxExtEncToolsConfig* pConfig = ExtBuffer::Get(par);
        const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
        bool bEncTools = (pConfig) ?
            IsEncToolsOptOn(*pConfig, pCO3 && pCO3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING) :
            false;
        MFX_CHECK(bEncTools, MFX_ERR_NONE);
        MFX_CHECK(!m_pEncTools, MFX_ERR_NONE);

        mfxEncTools*                encTools = GetEncTools(par);
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
        auto& caps = Glob::EncodeCaps::Get(strg);
        auto sts = InitEncToolsCtrl(par, &m_EncToolCtrl, caps.MbQpDataSupport);
        MFX_CHECK_STS(sts);

        m_bEncToolsInner = false;
        if (!(encTools && encTools->Context))
        {
            encTools = MFXVideoENCODE_CreateEncTools();
            m_bEncToolsInner = !!encTools;
        }
        if (encTools)
        {
            mfxExtEncToolsConfig supportedConfig = {};

            encTools->GetSupportedConfig(encTools->Context, &supportedConfig, &m_EncToolCtrl);

            if (CorrectVideoParams(par, supportedConfig))
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

            SetDefaultConfig(par, m_EncToolConfig);

            sts = encTools->Init(encTools->Context, &m_EncToolConfig, &m_EncToolCtrl);
            MFX_CHECK_STS(sts);

            sts = encTools->GetActiveConfig(encTools->Context, &m_EncToolConfig);
            MFX_CHECK_STS(sts);

            encTools->GetDelayInFrames(encTools->Context, &m_EncToolConfig, &m_EncToolCtrl, &m_maxDelay);

            auto& taskMgrIface = TaskManager::TMInterface::Get(strg);
            auto& tm = taskMgrIface.Manager;

            S_ET_SUBMIT = tm.AddStage(tm.S_NEW);
            S_ET_QUERY = tm.AddStage(S_ET_SUBMIT);

            m_pEncTools = encTools;
        }

        m_destroy = [this]()
        {
            if (m_bEncToolsInner)
                MFXVideoENCODE_DestroyEncTools(m_pEncTools);
            m_bEncToolsInner = false;
        };

        return MFX_ERR_NONE;
    });


    // Add S_ET_SUBMIT and S_ET_QUERY stages for EncTools
    Push(BLK_AddTask
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        MFX_CHECK(S_ET_SUBMIT != mfxU16(-1) && S_ET_QUERY != mfxU16(-1), MFX_ERR_NONE);
        auto& taskMgrIface = TaskManager::TMInterface::Get(global);
        auto& tm = taskMgrIface.Manager;

        auto  ETSubmit = [&](
            TaskManager::ExtTMInterface::TAsyncStage::TExt
            , StorageW& global
            , StorageW& /*s_task*/) -> mfxStatus
        {
            std::unique_lock<std::mutex> closeGuard(tm.m_closeMtx);

            if (StorageW* pTask = tm.GetTask(tm.Stage(S_ET_SUBMIT)))
            {
                SubmitPreEncTask(global, *pTask);
                tm.MoveTaskForward(tm.Stage(S_ET_SUBMIT), tm.FixedTask(*pTask));
            }

            return MFX_ERR_NONE;
        };

        auto  ETQuery = [&](
            TaskManager::ExtTMInterface::TAsyncStage::TExt
            , StorageW& /*global*/
            , StorageW& s_task) -> mfxStatus
        {
            std::unique_lock<std::mutex> closeGuard(tm.m_closeMtx);
            bool       bFlush = !tm.IsInputTask(s_task);

            // Delay For LookAhead Depth
            MFX_CHECK(tm.m_stages.at(tm.Stage(S_ET_QUERY)).size() >= std::max(m_maxDelay,1U)  || bFlush,MFX_ERR_NONE);

            StorageW* pTask = tm.GetTask(tm.Stage(S_ET_QUERY));
            MFX_CHECK(pTask, MFX_ERR_NONE);
            auto sts = QueryPreEncTask(global, *pTask);
            MFX_CHECK_STS(sts);

            tm.MoveTaskForward(tm.Stage(S_ET_QUERY), tm.FixedTask(*pTask));

            return MFX_ERR_NONE;
        };

        taskMgrIface.AsyncStages[tm.Stage(S_ET_SUBMIT)].Push(ETSubmit);
        taskMgrIface.AsyncStages[tm.Stage(S_ET_QUERY)].Push(ETQuery);

        // Extend Num of tasks and size of buffer.
        taskMgrIface.ResourceExtra += (mfxU16)m_maxDelay;

        return MFX_ERR_NONE;
    });
    Push(BLK_UpdateTask
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        MFX_CHECK(m_pEncTools, MFX_ERR_NONE);
        auto& taskMgrIface = TaskManager::TMInterface::Get(global);

        auto  UpdateTask = [&](
            TaskManager::ExtTMInterface::TUpdateTask::TExt
            , StorageW* dstTask) -> mfxStatus
        {

            if (dstTask)
            {
                auto& dst_task = Task::Common::Get(*dstTask);
                if (LpLaStatus.size() > 0)
                {
                    dst_task.LplaStatus = *(LpLaStatus.begin());
                    LpLaStatus.pop_front();
                }
            }
            return MFX_ERR_NONE;
        };
        taskMgrIface.UpdateTask.Push(UpdateTask);

        return MFX_ERR_NONE;
    });
}

void HevcEncTools::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_GetFrameCtrl
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        MFX_CHECK(IsOn(m_EncToolConfig.BRC) && m_pEncTools && m_pEncTools->Submit, MFX_ERR_NONE);

        auto&      par = Glob::VideoParam::Get(global);
        auto&      task = Task::Common::Get(s_task);
        auto&      sh = Task::SSH::Get(s_task);
        auto&      sps = Glob::SPS::Get(global);
        auto&      pps = Glob::PPS::Get(global);

        bool bNegativeQpAllowed = !IsOn(par.mfx.LowPower);

        mfxI32 minQP = (-6 * sps.bit_depth_luma_minus8) * bNegativeQpAllowed;
        mfxI32 maxQP = 51;

        mfxEncToolsBRCQuantControl quantCtrl = {};
        mfxEncToolsBRCHRDPos  HRDPos = {};

        BRCGetCtrl(global, s_task, quantCtrl, HRDPos);

        SetDefault(HRDPos.InitialCpbRemovalDelay, task.initial_cpb_removal_delay);
        SetDefault(HRDPos.InitialCpbRemovalDelayOffset, task.initial_cpb_removal_offset);

        task.initial_cpb_removal_delay = HRDPos.InitialCpbRemovalDelay;
        task.initial_cpb_removal_offset = HRDPos.InitialCpbRemovalDelayOffset;

        task.QpY = mfxI8(mfx::clamp((mfxI32)quantCtrl.QpY + (-6 * sps.bit_depth_luma_minus8) * bNegativeQpAllowed, minQP, maxQP));
        sh.slice_qp_delta = mfxI8(task.QpY - (pps.init_qp_minus26 + 26));

        sh.temporal_mvp_enabled_flag &= !(par.AsyncDepth > 1 && task.NumRecode); // WA

        task.etQpMapNZ = quantCtrl.QpMapNZ;
        task.etQpMap = quantCtrl.ExtQpMap;

        // Internal MAP from BRC
        if (!task.bCUQPMap && m_EncToolCtrl.MBBRC)
        {
            task.bCUQPMap = (task.etQpMapNZ > 0);
        }

        return MFX_ERR_NONE;
    });
}

void HevcEncTools::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_Update
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        MFX_CHECK(IsOn(m_EncToolConfig.BRC) && m_pEncTools && m_pEncTools->Query, MFX_ERR_NONE);

        auto& task = Task::Common::Get(s_task);
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

        return MFX_ERR_NONE;
    });
}

void HevcEncTools::FreeTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_Discard
        , [this](StorageW& /*global*/, StorageW& s_task)->mfxStatus
    {
        MFX_CHECK(m_pEncTools && m_pEncTools->Discard, MFX_ERR_NONE);

        auto& task = Task::Common::Get(s_task);

        return m_pEncTools->Discard(m_pEncTools->Context, task.DisplayOrder);
    });
}

void HevcEncTools::Close(const FeatureBlocks& /*blocks*/, TPushCLS Push)
{
    Push(BLK_Close
        , [this](StorageW& /*global*/)
    {
        if (m_pEncTools && m_pEncTools->Close)
            m_pEncTools->Close(m_pEncTools->Context);
    });
}

#endif //defined(MFX_ENABLE_ENCTOOLS)
#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
