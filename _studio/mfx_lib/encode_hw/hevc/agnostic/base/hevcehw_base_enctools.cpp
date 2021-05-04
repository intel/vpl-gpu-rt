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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_ENCTOOLS)

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
    return (mfxEncTools *)GetExtBuffer(video.ExtParam, video.NumExtParam, MFX_EXTBUFF_ENCTOOLS);
}

static void SetDefaultConfig(mfxVideoParam &video, mfxExtEncToolsConfig &config)
{
    mfxExtCodingOption2  *pExtOpt2 = ExtBuffer::Get(video);
    mfxExtCodingOption3  *pExtOpt3 = ExtBuffer::Get(video);
    mfxExtEncToolsConfig *pExtConfig = ExtBuffer::Get(video);
    bool bGameStreaming = pExtOpt3 && pExtOpt3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING;

    if (!pExtConfig || !IsEncToolsOptOn(*pExtConfig, bGameStreaming))
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
    config = *pExtConfig;

    if (!bGameStreaming)
    {
        if (CheckSWEncCondition(video))
        {
            mfxExtCodingOptionDDI extDdi = ExtBuffer::Get(video);

            bool bGopStrict = (video.mfx.GopOptFlag & MFX_GOP_STRICT);
            bool bAdaptiveI = !(pExtOpt2 && IsOff(pExtOpt2->AdaptiveI)) && (!bGopStrict);
            bool bAdaptiveB = (pExtOpt2 && pExtOpt2->AdaptiveB) ?  !IsOff(pExtOpt2->AdaptiveB): bAdaptiveI ;

            SetDefaultOpt(config.AdaptiveI, bAdaptiveI);
            SetDefaultOpt(config.AdaptiveB, bAdaptiveB);
            SetDefaultOpt(config.AdaptivePyramidQuantP, bAdaptiveI);
            SetDefaultOpt(config.AdaptivePyramidQuantB, bAdaptiveI);

            bool bAdaptRef = (extDdi.NumActiveRefP !=1) && bAdaptiveI;

            SetDefaultOpt(config.AdaptiveRefP, bAdaptRef);
            SetDefaultOpt(config.AdaptiveRefB, bAdaptRef);
            SetDefaultOpt(config.AdaptiveLTR,  bAdaptRef);
        }
        SetDefaultOpt(config.BRC, (video.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
            video.mfx.RateControlMethod == MFX_RATECONTROL_VBR));
    }
#ifdef MFX_ENABLE_ENCTOOLS_LPLA
    else
    {
        bool bLA = (pExtOpt2 && pExtOpt2->LookAheadDepth > 0 &&
            (video.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
                video.mfx.RateControlMethod == MFX_RATECONTROL_VBR));

       SetDefaultOpt(config.BRCBufferHints, bLA);
       SetDefaultOpt(config.AdaptivePyramidQuantP, bLA);
       SetDefaultOpt(config.AdaptiveQuantMatrices, bLA);
       SetDefaultOpt(config.AdaptiveI, bLA);
       SetDefaultOpt(config.AdaptiveB, bLA);
    }
#endif
}

inline mfxU32 CheckFlag(mfxU16 flag, bool bCond)
{
    return CheckOrZero<mfxU16>(flag
        , mfxU16(MFX_CODINGOPTION_UNKNOWN)
        , mfxU16(MFX_CODINGOPTION_OFF)
        , mfxU16(MFX_CODINGOPTION_ON * (bCond)));
}

static mfxU32 CorrectVideoParams(mfxVideoParam &video, mfxExtEncToolsConfig& supportedConfig)
{
    mfxExtCodingOption2   *pExtOpt2 = ExtBuffer::Get(video);
    mfxExtCodingOption3   *pExtOpt3 = ExtBuffer::Get(video);
    mfxExtCodingOptionDDI *pExtDdi = ExtBuffer::Get(video);
    mfxExtBRC             *pBRC = ExtBuffer::Get(video);
    mfxExtEncToolsConfig  *pConfig = ExtBuffer::Get(video);

    bool bIsEncToolsEnabled = CheckEncToolsCondition(video);
    mfxU32 changed = 0;

#ifdef MFX_ENABLE_ENCTOOLS_LPLA
    if (pExtOpt3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING && video.mfx.GopRefDist > 4) {
        changed++;
        video.mfx.GopRefDist = 4;
    }
#endif

    if (pConfig)
    {
        bool bGopStrict = !!(video.mfx.GopOptFlag & MFX_GOP_STRICT);
        bool bMultiRef  = !((pExtDdi && pExtDdi->NumActiveRefP == 1) || (video.mfx.TargetUsage == 7));

        changed += CheckFlag(pConfig->AdaptiveI, bIsEncToolsEnabled && !bGopStrict);
        changed += CheckFlag(pConfig->AdaptiveB, bIsEncToolsEnabled && !bGopStrict);
        changed += CheckFlag(pConfig->AdaptivePyramidQuantB, bIsEncToolsEnabled);
        changed += CheckFlag(pConfig->AdaptivePyramidQuantP, bIsEncToolsEnabled);

        changed += CheckFlag(pConfig->AdaptiveRefP, bIsEncToolsEnabled  && bMultiRef);
        changed += CheckFlag(pConfig->AdaptiveRefB, bIsEncToolsEnabled  && bMultiRef );
        changed += CheckFlag(pConfig->AdaptiveLTR, bIsEncToolsEnabled  && bMultiRef);
        changed += CheckFlag(pConfig->SceneChange, bIsEncToolsEnabled);
        changed += CheckFlag(pConfig->BRCBufferHints, bIsEncToolsEnabled);
        changed += CheckFlag(pConfig->AdaptiveQuantMatrices, bIsEncToolsEnabled);
        changed += CheckFlag(pConfig->BRC, bIsEncToolsEnabled);

        changed += CheckFlag(pConfig->AdaptiveI, supportedConfig.AdaptiveI);
        changed += CheckFlag(pConfig->AdaptiveB, supportedConfig.AdaptiveB);
        changed += CheckFlag(pConfig->AdaptivePyramidQuantB, supportedConfig.AdaptivePyramidQuantB);
        changed += CheckFlag(pConfig->AdaptivePyramidQuantP, supportedConfig.AdaptivePyramidQuantP);
        changed += CheckFlag(pConfig->AdaptiveRefP, supportedConfig.AdaptiveRefP);
        changed += CheckFlag(pConfig->AdaptiveRefB, supportedConfig.AdaptiveRefB);
        changed += CheckFlag(pConfig->AdaptiveLTR, supportedConfig.AdaptiveLTR);
        changed += CheckFlag(pConfig->SceneChange, supportedConfig.SceneChange);
        changed += CheckFlag(pConfig->BRCBufferHints, supportedConfig.BRCBufferHints);
        changed += CheckFlag(pConfig->AdaptiveQuantMatrices, supportedConfig.AdaptiveQuantMatrices);
        changed += CheckFlag(pConfig->BRC, supportedConfig.BRC);
    }
    if (pExtOpt2)
    {
        changed += CheckFlag(pExtOpt2->AdaptiveI, supportedConfig.AdaptiveI);
        changed += CheckFlag(pExtOpt2->AdaptiveB, supportedConfig.AdaptiveB);
        changed += CheckFlag(pExtOpt2->ExtBRC, !bIsEncToolsEnabled);
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

static mfxStatus InitEncToolsCtrl(
    mfxVideoParam const & par
    , mfxEncToolsCtrl *ctrl)
{
    MFX_CHECK_NULL_PTR1(ctrl);

    const mfxExtCodingOption  *pCO  = ExtBuffer::Get(par);
    const mfxExtCodingOption2 *pCO2 = ExtBuffer::Get(par);
    const mfxExtCodingOption3 *pCO3 = ExtBuffer::Get(par);

    ctrl->CodecId = par.mfx.CodecId;
    ctrl->CodecProfile = par.mfx.CodecProfile;
    ctrl->CodecLevel = par.mfx.CodecLevel;

    ctrl->FrameInfo = par.mfx.FrameInfo;
    ctrl->IOPattern = par.IOPattern;
    ctrl->MaxDelayInFrames = pCO2 ? pCO2->LookAheadDepth : 0 ;

    ctrl->MaxGopSize = par.mfx.GopPicSize;
    ctrl->MaxGopRefDist = par.mfx.GopRefDist;
    ctrl->MaxIDRDist = par.mfx.GopPicSize * (par.mfx.IdrInterval + 1);
    ctrl->BRefType = pCO2 ? pCO2->BRefType : 0;

    ctrl->ScenarioInfo = pCO3 ? pCO3->ScenarioInfo : 0;

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
        ctrl->WinBRCSize = pCO3 ? pCO3->WinBRCSize:0;
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

        mfxStatus sts = InitEncToolsCtrl(par, &ctrl);
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
            if (Glob::ResetHint::Get(global).Flags & RF_IDR_REQUIRED)
            {
                mfxExtEncoderResetOption& rOpt = ExtBuffer::Get(par);
                rOpt.StartNewSequence = MFX_CODINGOPTION_ON;
            }

            mfxStatus sts = m_pEncTools->Reset(m_pEncTools->Context, &m_EncToolConfig, &m_EncToolCtrl);
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
    return vaType ? MFX_HANDLE_D3D11_DEVICE : (MFX_HW_D3D9 == vaType ? MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9 : MFX_HANDLE_VA_DISPLAY);
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

        if (bEncToolsPreEnc)
        {
            mfxU16 nExtraRaw = (pCO2 && pCO2->LookAheadDepth) ? 0 : 8; //LA is used in base_legacy
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
        extParams.push_back((mfxExtBuffer *)&extFrameData);
        task_par.ExtParam = &extParams[0];
    }
    task_par.DisplayOrder = task.DisplayOrder;
    task_par.NumExtParam = (mfxU16)extParams.size();

    auto sts = m_pEncTools->Submit(m_pEncTools->Context, &task_par);
    if (sts == MFX_ERR_MORE_DATA) sts = MFX_ERR_NONE;
    return (sts);
}

constexpr mfxU32 ENCTOOLS_QUERY_TIMEOUT = 5000;

mfxStatus HevcEncTools::QueryPreEncTask(StorageW&  /*global*/, StorageW& s_task)
{
    MFX_CHECK(m_pEncTools && m_pEncTools->Query, MFX_ERR_NONE);

    auto& task = Task::Common::Get(s_task);

    mfxEncToolsTaskParam task_par = {};
    std::vector<mfxExtBuffer*> extParams;

    MFX_CHECK(task.DisplayOrder!= (mfxU32)(-1), MFX_ERR_NONE);
    task_par.DisplayOrder = task.DisplayOrder;
    task_par.NumExtParam = (mfxU16)extParams.size();

    mfxEncToolsHintPreEncodeGOP preEncodeGOP = {};
    preEncodeGOP.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_HINT_GOP;
    preEncodeGOP.Header.BufferSz = sizeof(preEncodeGOP);
    extParams.push_back((mfxExtBuffer *)&preEncodeGOP);

#if defined MFX_ENABLE_ENCTOOLS_LPLA
    mfxEncToolsHintQuantMatrix cqmHint = {};
    cqmHint.MatrixType = CQM_HINT_USE_FLAT_MATRIX;
    if (IsOn(m_EncToolConfig.AdaptiveQuantMatrices))
    {
        cqmHint.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_HINT_MATRIX;
        cqmHint.Header.BufferSz = sizeof(cqmHint);
        extParams.push_back((mfxExtBuffer *)&cqmHint);
    }

    mfxEncToolsBRCBufferHint bufHint = {};
    if (IsOn(m_EncToolConfig.BRCBufferHints))
    {
        bufHint.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_BRC_BUFFER_HINT;
        bufHint.Header.BufferSz = sizeof(bufHint);
        extParams.push_back((mfxExtBuffer *)&bufHint);
    }
#endif
    task_par.ExtParam = &extParams[0];
    task_par.NumExtParam = (mfxU16)extParams.size();

    auto sts = m_pEncTools->Query(m_pEncTools->Context, &task_par, ENCTOOLS_QUERY_TIMEOUT);
    task.GopHints.MiniGopSize = preEncodeGOP.MiniGopSize;
#if defined(MFX_ENABLE_ENCTOOLS_LPLA)
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
#endif
    if (sts == MFX_ERR_MORE_DATA) sts = MFX_ERR_NONE;
    MFX_CHECK_STS(sts);

    return sts;
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
            MFX_CHECK_NULL_PTR1(pFrameAlloc);

            InitEncToolsCtrlExtDevice(extBufDevice, h_type, device_handle);
            InitEncToolsCtrlExtAllocator(extBufAlloc, *pFrameAlloc);

            ExtParam[0] = (mfxExtBuffer*)&extBufDevice;
            ExtParam[1] = (mfxExtBuffer*)&extBufAlloc;
            m_EncToolCtrl.ExtParam = ExtParam;
            m_EncToolCtrl.NumExtParam = 2;
        }

        auto sts = InitEncToolsCtrl(par, &m_EncToolCtrl);
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


    // Add S_ET_SUBMIT and S_ET_QUERY stages for LPLA
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
            // If In LookAhead Pass, it should be moved to the next stage

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

            //auto& taskMgrIface = TaskManager::TMInterface::Get(global);
            //auto& tm = taskMgrIface.Manager;
            bool       bFlush = !tm.IsInputTask(s_task);


            // Delay For LookAhead Depth
            MFX_CHECK(tm.m_stages.at(tm.Stage(S_ET_QUERY)).size() >= m_maxDelay  || bFlush,MFX_ERR_NONE);

            StorageW* pTask = tm.GetTask(tm.Stage(S_ET_QUERY));
            auto sts = QueryPreEncTask(global, *pTask);
            MFX_CHECK_STS(sts);

            tm.MoveTaskForward(tm.Stage(S_ET_QUERY), tm.FixedTask(*pTask));

            return MFX_ERR_NONE;
        };

        taskMgrIface.AsyncStages[tm.Stage(S_ET_SUBMIT)].Push(ETSubmit);
        taskMgrIface.AsyncStages[tm.Stage(S_ET_QUERY)].Push(ETQuery);

        // Extend Num of tasks and size of buffer.
        taskMgrIface.ResourceExtra += (mfxU16)m_maxDelay;
//-
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
                   // printf("copy dst task: %d,  Target %d\n", dst_task.DisplayOrder, dst_task.LplaStatus.TargetFrameSize);

                    LpLaStatus.pop_front();
                }
                //dst_task.LplaStatus = src_task.LplaStatus;
            }
            return MFX_ERR_NONE;
        };
        taskMgrIface.UpdateTask.Push(UpdateTask);

        return MFX_ERR_NONE;
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

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
