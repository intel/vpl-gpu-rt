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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)
#if defined(MFX_ENABLE_ENCTOOLS)

#include "hevcehw_base.h"
#include "hevcehw_base_data.h"
#include "hevcehw_base_enctools.h"
#include "hevcehw_base_task.h"
#include "hevcehw_base_legacy.h"
#include <climits>

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

bool HevcEncTools::isFeatureEnabled(const mfxVideoParam& par)
{
    return true;
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


inline bool IsEncToolsOptSet(const mfxExtEncToolsConfig& config)
{
    return
        (config.AdaptiveB | config.AdaptiveI | config.AdaptiveLTR | config.AdaptivePyramidQuantB
            | config.AdaptivePyramidQuantP | config.AdaptiveQuantMatrices | config.AdaptiveRefB
            | config.AdaptiveRefP | config.BRC | config.BRCBufferHints | config.SceneChange);
}

int HEVCEHW::Base::EncToolsDeblockingBetaOffset()
{
    return 4;  // Currently hard coded to best tested value when using enctools
}
int HEVCEHW::Base::EncToolsDeblockingAlphaTcOffset()
{
    return 2;  // Currently hard coded to best tested value when using enctools
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

bool HevcEncTools::IsEncToolsConfigOn(const mfxExtEncToolsConfig &config, bool bGameStreaming)
{
    return IsEncToolsOptOn(config,bGameStreaming);
}

bool HEVCEHW::Base::IsLPLAEncToolsOn(const mfxExtEncToolsConfig &config, bool bGameStreaming)
{
    return IsLookAhead(config, bGameStreaming);
}

inline bool IsAdaptiveRefAllowed(const mfxVideoParam &video)
{
    const mfxExtCodingOption3 *pExtOpt3 = ExtBuffer::Get(video);
    bool adaptiveRef = (video.mfx.TargetUsage != 7) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT);
    if (pExtOpt3)
    {
        adaptiveRef = adaptiveRef && !(pExtOpt3->NumRefActiveP[0] == 1 || (video.mfx.GopRefDist > 1 && pExtOpt3->NumRefActiveBL0[0] == 1));
        adaptiveRef = adaptiveRef && !IsOff(pExtOpt3->ExtBrcAdaptiveLTR);
    }
    return adaptiveRef;
}

bool HEVCEHW::Base::IsHwEncToolsOn(const mfxVideoParam& video)
{

    const mfxExtCodingOption3* pExtOpt3 = ExtBuffer::Get(video);
    const mfxExtCodingOption2* pExtOpt2 = ExtBuffer::Get(video);
    const mfxExtEncToolsConfig* pExtConfig = ExtBuffer::Get(video);
    bool bGameStreaming = pExtOpt3 && pExtOpt3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING;
    bool bLA = false;
    if ((bGameStreaming && pExtOpt2 && pExtOpt2->LookAheadDepth > 0) || (pExtConfig && IsLookAhead(*pExtConfig, bGameStreaming)))
        bLA = true;
    return  bLA ;
}

inline bool IsSwEncToolsImplicit(const mfxVideoParam &video)
{
    const mfxExtCodingOption2  *pExtOpt2 = ExtBuffer::Get(video);
    if (pExtOpt2 && pExtOpt2->LookAheadDepth > 0)
    {
        const mfxExtCodingOption3* pExtOpt3 = ExtBuffer::Get(video);
        if(
            (video.mfx.GopRefDist == 2 || video.mfx.GopRefDist == 4 || video.mfx.GopRefDist == 8 || video.mfx.GopRefDist == 16)
            && IsOn(pExtOpt2->ExtBRC)
            && !(pExtOpt3 && pExtOpt3->ScenarioInfo != MFX_SCENARIO_UNKNOWN)
        )
        {
            return true;
        }
    }
    return false;
}

bool HEVCEHW::Base::IsSwEncToolsOn(const mfxVideoParam& video){

    if(IsHwEncToolsOn(video))
    {
        return false;
    }

    const mfxExtEncToolsConfig *pConfig = ExtBuffer::Get(video);
    if(pConfig)
    {
        mfxExtEncToolsConfig config = {};
        HevcEncTools et(FEATURE_ENCTOOLS);
        et.SetDefaultConfig(video, config, true);

        return
              IsOn(config.AdaptiveI) || IsOn(config.AdaptiveB)
            || IsOn(config.AdaptiveRefP) || IsOn(config.AdaptiveRefB)
            || IsOn(config.SceneChange)
            || IsOn(config.AdaptiveLTR)
            || IsOn(config.AdaptivePyramidQuantP) || IsOn(config.AdaptivePyramidQuantB)
            || IsOn(config.AdaptiveQuantMatrices)
            || IsOn(config.AdaptiveMBQP)
            || IsOn(config.BRCBufferHints)
            || IsOn(config.BRC);
    }
    else
    {
        return IsSwEncToolsImplicit(video);
    }
}

inline bool  CheckSWEncCondition(const mfxVideoParam &video)
{
    return (
        CheckEncToolsCondition(video)
        && (video.mfx.GopRefDist == 0
            || video.mfx.GopRefDist == 1
            || video.mfx.GopRefDist == 2
            || video.mfx.GopRefDist == 4
            || video.mfx.GopRefDist == 8
            || video.mfx.GopRefDist == 16));
}

bool HevcEncTools::IsEncToolsImplicit(const mfxVideoParam &video)
{
    const mfxExtCodingOption2  *pExtOpt2 = ExtBuffer::Get(video);
    bool etOn = false;
    if (pExtOpt2 && pExtOpt2->LookAheadDepth > 0)
    {
        const mfxExtCodingOption3* pExtOpt3 = ExtBuffer::Get(video);
        etOn = (IsGameStreaming(video) && IsOn(video.mfx.LowPower)); // LPLA
        etOn = etOn ||
            ((video.mfx.GopRefDist == 2 || video.mfx.GopRefDist == 4 || video.mfx.GopRefDist == 8 || video.mfx.GopRefDist == 16) && IsOn(pExtOpt2->ExtBRC) // SW EncTools
                && !(pExtOpt3 && pExtOpt3->ScenarioInfo != MFX_SCENARIO_UNKNOWN));
    }
    return etOn;
}

bool HEVCEHW::Base::IsSwEncToolsSpsACQM(const mfxVideoParam &video)
{
    bool bETSPSadaptQM = false;
    const mfxExtCodingOption3* pExtOpt3 = ExtBuffer::Get(video);
    if (IsSwEncToolsOn(video) && pExtOpt3)
    {
        const mfxExtEncToolsConfig *pConfig = ExtBuffer::Get(video);
        if(pConfig)
        {
            bETSPSadaptQM = IsOn(pConfig->AdaptiveQuantMatrices) && pExtOpt3->ContentInfo == MFX_CONTENT_NOISY_VIDEO;
        }
        else
        {
            bETSPSadaptQM = !IsOff(pExtOpt3->AdaptiveCQM) && pExtOpt3->ContentInfo == MFX_CONTENT_NOISY_VIDEO;
        }
    }
    return bETSPSadaptQM;
}

bool HEVCEHW::Base::IsSwEncToolsPpsACQM(const mfxVideoParam &video)
{
    bool bETPPSadaptQM = false;
    const mfxExtCodingOption3* pExtOpt3 = ExtBuffer::Get(video);
    const mfxExtCodingOption *pExtOpt = ExtBuffer::Get(video);
    if (IsSwEncToolsOn(video) && pExtOpt3 && pExtOpt)
    {
        const mfxExtEncToolsConfig *pConfig = ExtBuffer::Get(video);
        if(pConfig)
        {
            bETPPSadaptQM = IsOn(pConfig->AdaptiveQuantMatrices) && IsOn(pConfig->BRC) && !IsOff(pExtOpt->NalHrdConformance);
        }
        else
        {
            if (video.mfx.RateControlMethod == MFX_RATECONTROL_CBR || video.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
            {
                bETPPSadaptQM = !IsOff(pExtOpt3->AdaptiveCQM) && !IsOff(pExtOpt->NalHrdConformance);
            }
        }
    }
    return bETPPSadaptQM;
}

bool isSWLACondition(const mfxVideoParam& video)
{
    const mfxExtCodingOption2* pExtOpt2 = ExtBuffer::Get(video);
    return (pExtOpt2 &&
        (pExtOpt2->LookAheadDepth > video.mfx.GopRefDist));
}

void HevcEncTools::SetDefaultConfig(const mfxVideoParam &video, mfxExtEncToolsConfig &config, bool bMBQPSupport)
{
    const mfxExtCodingOption   *pExtOpt = ExtBuffer::Get(video);
    const mfxExtCodingOption2  *pExtOpt2 = ExtBuffer::Get(video);
    const mfxExtCodingOption3  *pExtOpt3 = ExtBuffer::Get(video);
    const mfxExtEncToolsConfig *pExtConfig = ExtBuffer::Get(video);
    bool bGameStreaming = IsGameStreaming(video);

    if (!pExtConfig || !IsEncToolsOptSet(*pExtConfig))
    {
        if (IsEncToolsImplicit(video))
        {
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
    else
        config = *pExtConfig;

    if (!bGameStreaming)
    {
        if (CheckSWEncCondition(video))
        {
            bool bAdaptiveI = !(pExtOpt2 && IsOff(pExtOpt2->AdaptiveI)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT);
            bool bAdaptiveB = !(pExtOpt2 && IsOff(pExtOpt2->AdaptiveB)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT);
            SetDefaultOpt(config.AdaptiveI, bAdaptiveI);
            SetDefaultOpt(config.AdaptiveB, bAdaptiveB);
            SetDefaultOpt(config.AdaptivePyramidQuantP, false);
            SetDefaultOpt(config.AdaptivePyramidQuantB, true);

            bool bAdaptiveRef = IsAdaptiveRefAllowed(video);
            SetDefaultOpt(config.AdaptiveRefP, bAdaptiveRef);
            SetDefaultOpt(config.AdaptiveRefB, bAdaptiveRef);
            SetDefaultOpt(config.AdaptiveLTR, bAdaptiveRef);
        }
        SetDefaultOpt(config.BRC, (video.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
            video.mfx.RateControlMethod == MFX_RATECONTROL_VBR));

        bool lplaAssistedBRC = IsOn(config.BRC) && isSWLACondition(video);
        bool bLA = pExtOpt2 && pExtOpt2->LookAheadDepth > 0 && IsOn(config.BRC);
        SetDefaultOpt(config.BRCBufferHints, lplaAssistedBRC);
        SetDefaultOpt(config.AdaptiveMBQP,  bMBQPSupport && lplaAssistedBRC && pExtOpt2 && IsOn(pExtOpt2->MBBRC));
        if (pExtOpt3)
            SetDefaultOpt(config.AdaptiveQuantMatrices, bLA && ((!IsOff(pExtOpt3->AdaptiveCQM) && pExtOpt && !IsOff(pExtOpt->NalHrdConformance)) || (!IsOff(pExtOpt3->AdaptiveCQM) && pExtOpt3->ContentInfo == MFX_CONTENT_NOISY_VIDEO)));
        else
            SetDefaultOpt(config.AdaptiveQuantMatrices, false);
    }
#ifdef MFX_ENABLE_ENCTOOLS_LPLA
    else
    {
        bool bLA = (pExtOpt2 && pExtOpt2->LookAheadDepth > 0 &&
            (video.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
                video.mfx.RateControlMethod == MFX_RATECONTROL_VBR));
        bool bAdaptiveI = (pExtOpt2 && IsOn(pExtOpt2->AdaptiveI)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT);
        bool bAdaptiveB = (pExtOpt2 && IsOn(pExtOpt2->AdaptiveB)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT);

       SetDefaultOpt(config.BRCBufferHints, bLA);
       SetDefaultOpt(config.AdaptivePyramidQuantP, bLA);
       SetDefaultOpt(config.AdaptivePyramidQuantB, bLA);
       SetDefaultOpt(config.AdaptiveQuantMatrices, bLA && !IsOff(pExtOpt3->AdaptiveCQM));
       SetDefaultOpt(config.AdaptiveI, bLA && bAdaptiveI);
       SetDefaultOpt(config.AdaptiveB, bLA && bAdaptiveB);
       SetDefaultOpt(config.AdaptiveMBQP, bLA && bMBQPSupport && IsOn(pExtOpt2->MBBRC) );
    }
#endif
}

mfxU32 HevcEncTools::CorrectVideoParams(mfxVideoParam & video, mfxExtEncToolsConfig & supportedConfig)
{
    mfxExtCodingOption2   *pExtOpt2 = ExtBuffer::Get(video);
    mfxExtCodingOption3   *pExtOpt3 = ExtBuffer::Get(video);
    mfxExtBRC             *pBRC = ExtBuffer::Get(video);
    mfxExtEncToolsConfig  *pConfig = ExtBuffer::Get(video);

    bool bIsEncToolsEnabled = CheckEncToolsCondition(video);
    mfxU32 changed = 0;

#ifdef MFX_ENABLE_ENCTOOLS_LPLA
    if (IsGameStreaming(video))
    {
        // Closed GOP for GS by default unless IdrInterval is set to max
        // Open GOP with arbitrary IdrInterval if GOP is strict (GopOptFlag == MFX_GOP_STRICT)
        if (video.mfx.GopOptFlag == 0 && video.mfx.IdrInterval != USHRT_MAX)
        {
            changed++;
            video.mfx.GopOptFlag = MFX_GOP_CLOSED;
        }
    }
#endif

    if (pConfig)
    {
        bool bAdaptiveI = !(pExtOpt2 && IsOff(pExtOpt2->AdaptiveI)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT);
        bool bAdaptiveB = !(pExtOpt2 && IsOff(pExtOpt2->AdaptiveB)) && !(video.mfx.GopOptFlag & MFX_GOP_STRICT);
        bool bAdaptiveRef = IsAdaptiveRefAllowed(video);
        bool bAdaptiveCQM = !(pExtOpt3 && IsOff(pExtOpt3->AdaptiveCQM));

        changed += CheckFlag(pConfig->AdaptiveI, bIsEncToolsEnabled && bAdaptiveI);
        changed += CheckFlag(pConfig->AdaptiveB, bIsEncToolsEnabled && bAdaptiveB);
        changed += CheckFlag(pConfig->AdaptivePyramidQuantB, bIsEncToolsEnabled);
        changed += CheckFlag(pConfig->AdaptivePyramidQuantP, bIsEncToolsEnabled);

        changed += CheckFlag(pConfig->AdaptiveRefP, bIsEncToolsEnabled  && bAdaptiveRef);
        changed += CheckFlag(pConfig->AdaptiveRefB, bIsEncToolsEnabled  && bAdaptiveRef);
        changed += CheckFlag(pConfig->AdaptiveLTR, bIsEncToolsEnabled  && bAdaptiveRef);
        changed += CheckFlag(pConfig->SceneChange, bIsEncToolsEnabled);
        changed += CheckFlag(pConfig->BRCBufferHints, bIsEncToolsEnabled);
        changed += CheckFlag(pConfig->AdaptiveQuantMatrices, bIsEncToolsEnabled && bAdaptiveCQM);
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
        changed += CheckFlag(pConfig->AdaptiveMBQP, IsOn(supportedConfig.AdaptiveMBQP));
    }
    if (pExtOpt2)
    {
        changed += CheckFlag(pExtOpt2->AdaptiveI, IsOn(supportedConfig.AdaptiveI));
        changed += CheckFlag(pExtOpt2->AdaptiveB, IsOn(supportedConfig.AdaptiveB));
        changed += CheckFlag(pExtOpt2->ExtBRC, IsOn(supportedConfig.BRC));
        changed += CheckFlag(pExtOpt2->MBBRC, IsOn(supportedConfig.AdaptiveMBQP));
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

mfxStatus HevcEncTools::InitEncToolsCtrl(mfxVideoParam const& par, mfxEncToolsCtrl* ctrl)
{
    mfxStatus sts = HevcEncToolsCommon::InitEncToolsCtrl(par, ctrl);
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

void HevcEncTools::Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push)
{
    Push(BLK_SetDefaultsCallChain,
        [this](const mfxVideoParam&, mfxVideoParam& par, StorageRW& strg) -> mfxStatus
        {
            MFX_CHECK(isFeatureEnabled(par), MFX_ERR_NONE);

            auto& defaults = Glob::Defaults::GetOrConstruct(strg);
            auto& bSet = defaults.SetForFeature[GetID()];
            MFX_CHECK(!bSet, MFX_ERR_NONE);
            defaults.GetMBBRC.Push(
                [this](Defaults::TChain<mfxU16>::TExt prev
                    , const Defaults::Param& par)
                {
                    bool bEncTools = isEncTools(par.mvp);
                    if (bEncTools && !IsGameStreaming(par.mvp) && isSWLACondition(par.mvp))
                    {
                        const mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par.mvp);
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

    HevcEncToolsCommon::Query1NoCaps(blocks, Push);
}

void HevcEncTools::Reset(const FeatureBlocks& /*blocks*/, TPushR Push)
{
    Push(BLK_ResetCheck
        , [this](
            const mfxVideoParam& /*par*/
            , StorageRW& global
            , StorageRW&) -> mfxStatus
    {
        auto& init = Glob::RealState::Get(global);
        auto& parOld = Glob::VideoParam::Get(init);
        auto& parNew = Glob::VideoParam::Get(global);

        MFX_CHECK(isFeatureEnabled(parNew), MFX_ERR_NONE);

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

mfxStatus HevcEncTools::BRCGetCtrl(StorageW& global , StorageW& s_task,
    mfxEncToolsBRCQuantControl &extQuantCtrl , mfxEncToolsBRCHRDPos  &extHRDPos,
    mfxEncToolsHintQPMap   &qpMapHint)
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

        auto& par = Glob::VideoParam::Get(global);
        auto& caps = Glob::EncodeCaps::Get(global);
        std::unique_ptr<FrameLocker> qpMap = nullptr;

        if (Legacy::GetMBQPMode(caps, par) == MBQPMode_FromEncToolsBRC &&
            global.Contains(Glob::AllocMBQP::Key))
        {
            qpMapHint.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_HINT_QPMAP;
            qpMapHint.Header.BufferSz = sizeof(qpMapHint);
            if (!task.CUQP.Mid)
            {
                task.CUQP = Glob::AllocMBQP::Get(global).Acquire();
                MFX_CHECK(task.CUQP.Mid, MFX_ERR_UNDEFINED_BEHAVIOR);
            }
            auto& allocInfo = Glob::MBQPAllocInfo::Get(global);
            auto& core = Glob::VideoCore::Get(global);
            qpMap = std::make_unique <FrameLocker>(core, task.CUQP.Mid);
            MFX_CHECK(qpMap->Y, MFX_ERR_LOCK_MEMORY);
            qpMapHint.ExtQpMap.BlockSize = (mfxU16)allocInfo.block_width;
            qpMapHint.ExtQpMap.QP = qpMap->Y;
            qpMapHint.ExtQpMap.Mode = MFX_MBQP_MODE_QP_VALUE;
            qpMapHint.ExtQpMap.NumQPAlloc = allocInfo.height_aligned * allocInfo.pitch;
            qpMapHint.QpMapPitch = (mfxU16)allocInfo.pitch;
            extParams.push_back(&qpMapHint.Header);
        }
 
        task_par.NumExtParam = (mfxU16)extParams.size();
        task_par.ExtParam = extParams.data();

        auto sts = m_pEncTools->Query(m_pEncTools->Context, &task_par, ENCTOOLS_QUERY_TIMEOUT);
        MFX_CHECK_STS(sts);

        if (qpMapHint.QpMapFilled)
            task.bCUQPMap = true;

    }
    return MFX_ERR_NONE;
 }

mfxStatus HevcEncTools::QueryPreEncTask(StorageW&  global, StorageW& s_task)
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
 
    auto& par  = Glob::VideoParam::Get(global);
    auto& caps = Glob::EncodeCaps::Get(global);
    std::unique_ptr <FrameLocker> qpMap = nullptr;

	if (Legacy::GetMBQPMode(caps, par) == MBQPMode_FromEncToolsLA &&
		global.Contains(Glob::AllocMBQP::Key))
	{
		qpMapHint.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_HINT_QPMAP;
		qpMapHint.Header.BufferSz = sizeof(qpMapHint);
		if (!task.CUQP.Mid)
		{
			task.CUQP = Glob::AllocMBQP::Get(global).Acquire();
			MFX_CHECK(task.CUQP.Mid, MFX_ERR_UNDEFINED_BEHAVIOR);
		}
		auto& allocInfo = Glob::MBQPAllocInfo::Get(global);
		auto& core = Glob::VideoCore::Get(global);
		qpMap = std::make_unique <FrameLocker>(core, task.CUQP.Mid);
		MFX_CHECK(qpMap->Y, MFX_ERR_LOCK_MEMORY);
		qpMapHint.ExtQpMap.BlockSize = (mfxU16)allocInfo.block_width;
		qpMapHint.ExtQpMap.QP = qpMap->Y;
		qpMapHint.ExtQpMap.Mode = MFX_MBQP_MODE_QP_DELTA;
		qpMapHint.ExtQpMap.NumQPAlloc = allocInfo.height_aligned * allocInfo.pitch;
		qpMapHint.QpMapPitch = (mfxU16)allocInfo.pitch;
		extParams.push_back(&qpMapHint.Header);
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
    if (qpMapHint.QpMapFilled)
        task.bCUQPMap = true;
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
        case CQM_HINT_INVALID:
            laStatus.CqmHint = CQM_HINT_INVALID;
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
        if (IsOn(m_EncToolConfig.AdaptivePyramidQuantB)) {
            if (preEncodeGOP.MiniGopSize <= 8) {
                task.GopHints.QPModulaton = MFX_QP_MODULATION_EXPLICIT;
                task.GopHints.QPDeltaExplicitModulation = (mfxI8)preEncodeGOP.QPDeltaExplicitModulation;
            }
            else {
                task.GopHints.QPModulaton = (mfxU8)preEncodeGOP.QPModulation;
            }
        }

        if (isLABRC)
        {
            task.BrcHints.LaAvgEncodedBits = bufHint.AvgEncodedSizeInBits;
            task.BrcHints.LaCurEncodedBits = bufHint.CurEncodedSizeInBits;
            task.BrcHints.LaDistToNextI = bufHint.DistToNextI;
        }
     }
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

void HevcEncTools::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_GetFrameCtrl
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(isFeatureEnabled(par), MFX_ERR_NONE);

        MFX_CHECK(IsOn(m_EncToolConfig.BRC) && m_pEncTools && m_pEncTools->Submit, MFX_ERR_NONE);

        auto&      task = Task::Common::Get(s_task);
        auto&      sh = Task::SSH::Get(s_task);
        auto&      sps = Glob::SPS::Get(global);
        auto&      pps = Glob::PPS::Get(global);

        bool bNegativeQpAllowed = !IsOn(par.mfx.LowPower);

        mfxI32 minQP = (-6 * sps.bit_depth_luma_minus8) * bNegativeQpAllowed;
        mfxI32 maxQP = 51;

        mfxEncToolsBRCQuantControl quantCtrl = {};
        mfxEncToolsHintQPMap   qpMapHint = {};
        mfxEncToolsBRCHRDPos  HRDPos = {};

        BRCGetCtrl(global, s_task, quantCtrl, HRDPos, qpMapHint);

        SetDefault(HRDPos.InitialCpbRemovalDelay, task.initial_cpb_removal_delay);
        SetDefault(HRDPos.InitialCpbRemovalDelayOffset, task.initial_cpb_removal_offset);

        task.initial_cpb_removal_delay = HRDPos.InitialCpbRemovalDelay;
        task.initial_cpb_removal_offset = HRDPos.InitialCpbRemovalDelayOffset;

        task.QpY = mfxI8(mfx::clamp((mfxI32)quantCtrl.QpY + (-6 * sps.bit_depth_luma_minus8) * bNegativeQpAllowed, minQP, maxQP));
        sh.slice_qp_delta = mfxI8(task.QpY - (pps.init_qp_minus26 + 26));

        sh.temporal_mvp_enabled_flag &= !(par.AsyncDepth > 1 && task.NumRecode); // WA



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
        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(isFeatureEnabled(par), MFX_ERR_NONE);

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
            MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        task.bForceSync |= task.bSkip;
        if(task.bSkip && !IsRef(task.FrameType))
        {
            task.SkipCMD = SKIPCMD_NeedCurrentFrameSkipping | SKIPCMD_NeedSkipSliceGen;
            task.bSkip = false;
        }

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_ENCTOOLS)
#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
