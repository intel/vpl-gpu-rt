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

#include "mfx_enctools.h"
#include "mfx_enctools_brc.h"

#include <algorithm>
#include <math.h>
#include "mfx_loader_utils.h"

constexpr mfxU32 ENC_TOOLS_WAIT_INTERVAL = 300000;

mfxStatus InitCtrl(mfxVideoParam const & par, mfxEncToolsCtrl *ctrl)
{
    MFX_CHECK_NULL_PTR1(ctrl);

    mfxExtCodingOption *CO = (mfxExtCodingOption *)Et_GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_CODING_OPTION);
    mfxExtCodingOption2 *CO2 = (mfxExtCodingOption2 *)Et_GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_CODING_OPTION2);
    mfxExtCodingOption3 *CO3 = (mfxExtCodingOption3 *)Et_GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_CODING_OPTION3);
    MFX_CHECK_NULL_PTR3(CO, CO2, CO3);
    mfxExtCodingOptionDDI* extDdi = (mfxExtCodingOptionDDI*)Et_GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_DDI);
    MFX_CHECK_NULL_PTR1(extDdi);

    *ctrl = {};

    ctrl->CodecId = par.mfx.CodecId;
    ctrl->CodecProfile = par.mfx.CodecProfile;
    ctrl->CodecLevel = par.mfx.CodecLevel;
    ctrl->LowPower = par.mfx.LowPower;
    ctrl->AsyncDepth = par.AsyncDepth;

    ctrl->FrameInfo = par.mfx.FrameInfo;
    ctrl->IOPattern = par.IOPattern;
    ctrl->MaxDelayInFrames = CO2->LookAheadDepth;

    ctrl->NumRefP = std::min(par.mfx.NumRefFrame, extDdi->NumActiveRefP);
    ctrl->MaxGopSize = par.mfx.GopPicSize;
    ctrl->MaxGopRefDist = par.mfx.GopRefDist;
    ctrl->MaxIDRDist = par.mfx.GopPicSize * (par.mfx.IdrInterval + !!(ctrl->CodecId == MFX_CODEC_AVC));
    // For !AVC IdrInterval 0 defaults to CRA
    if (par.mfx.IdrInterval == 0 && ctrl->CodecId != MFX_CODEC_AVC && par.mfx.GopPicSize != 0) {
        ctrl->MaxIDRDist = par.mfx.GopPicSize * (UINT_MAX / par.mfx.GopPicSize);
    }
    ctrl->BRefType = CO2->BRefType;

    ctrl->ScenarioInfo = CO3->ScenarioInfo;
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
        ctrl->TargetKbps = par.mfx.TargetKbps*mult;
        ctrl->MaxKbps = par.mfx.MaxKbps*mult;

        ctrl->HRDConformance = MFX_BRC_NO_HRD;
        if (!IsOff(CO->NalHrdConformance) && !IsOff(CO->VuiNalHrdParameters))
            ctrl->HRDConformance = MFX_BRC_HRD_STRONG;
        else if (IsOn(CO->NalHrdConformance) && IsOff(CO->VuiNalHrdParameters))
            ctrl->HRDConformance = MFX_BRC_HRD_WEAK;

        ctrl->BufferSizeInKB = par.mfx.BufferSizeInKB*mult;            //Bitstream size
        if (ctrl->HRDConformance)
        {
            ctrl->InitialDelayInKB = par.mfx.InitialDelayInKB*mult;    //if HRDConformance is ON
        }
        else
        {
            ctrl->ConvergencePeriod = 0;     //if HRDConformance is OFF, 0 - the period is whole stream,
            ctrl->Accuracy = 10;              //if HRDConformance is OFF
        }
        ctrl->WinBRCMaxAvgKbps = CO3->WinBRCMaxAvgKbps*mult;
        ctrl->WinBRCSize = CO3->WinBRCSize;
        ctrl->MaxFrameSizeInBytes[0] = CO3->MaxFrameSizeI ? CO3->MaxFrameSizeI : CO2->MaxFrameSize;     // MaxFrameSize limitation
        ctrl->MaxFrameSizeInBytes[1] = CO3->MaxFrameSizeP ? CO3->MaxFrameSizeP : CO2->MaxFrameSize;
        ctrl->MaxFrameSizeInBytes[2] = CO2->MaxFrameSize;

        ctrl->MinQPLevel[0] = CO2->MinQPI;       //QP range  limitations
        ctrl->MinQPLevel[1] = CO2->MinQPP;
        ctrl->MinQPLevel[2] = CO2->MinQPB;

        ctrl->MaxQPLevel[0] = CO2->MaxQPI;       //QP range limitations
        ctrl->MaxQPLevel[1] = CO2->MaxQPP;
        ctrl->MaxQPLevel[2] = CO2->MaxQPB;

        ctrl->PanicMode = CO3->BRCPanicMode;
    }

    if (ctrl->NumExtParam > 1)
    {
        ctrl->ExtParam[0] = Et_GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_ENCTOOLS_DEVICE);
        ctrl->ExtParam[1] = Et_GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_ENCTOOLS_ALLOCATOR);
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
        }
        ctrl->LaQp = 26;
    }

    return MFX_ERR_NONE;
}

inline void SetToolsStatus(mfxExtEncToolsConfig* conf, bool bOn)
{
    conf->SceneChange =
        conf->AdaptiveI =
        conf->AdaptiveB =
        conf->AdaptiveRefP =
        conf->AdaptiveRefB =
        conf->AdaptiveLTR =
        conf->AdaptivePyramidQuantP =
        conf->AdaptivePyramidQuantB =
        conf->AdaptiveQuantMatrices =
        conf->BRCBufferHints =
        conf->AdaptiveMBQP =
        conf->BRC =
        conf->SaliencyMapHint = mfxU16(bOn ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
}

inline void CopyPreEncSCTools(mfxExtEncToolsConfig const & confIn, mfxExtEncToolsConfig* confOut)
{
    confOut->AdaptiveI = confIn.AdaptiveI;
    confOut->AdaptiveB = confIn.AdaptiveB;
    confOut->AdaptiveRefP = confIn.AdaptiveRefP;
    confOut->AdaptiveRefB = confIn.AdaptiveRefB;
    confOut->AdaptiveLTR = confIn.AdaptiveLTR;
    confOut->AdaptivePyramidQuantP = confIn.AdaptivePyramidQuantP;
    confOut->AdaptivePyramidQuantB = confIn.AdaptivePyramidQuantB;
}

inline void OffPreEncSCDTools(mfxExtEncToolsConfig* conf)
{
    mfxExtEncToolsConfig confIn = {};
    SetToolsStatus(&confIn, false);
    CopyPreEncSCTools(confIn, conf);
}

inline void CopyPreEncLATools(mfxExtEncToolsConfig const & confIn, mfxExtEncToolsConfig* confOut)
{
    confOut->AdaptiveQuantMatrices = confIn.AdaptiveQuantMatrices;
    confOut->BRCBufferHints = confIn.BRCBufferHints;
    confOut->AdaptivePyramidQuantP = confIn.AdaptivePyramidQuantP;
    confOut->AdaptivePyramidQuantB = confIn.AdaptivePyramidQuantB;
    confOut->AdaptiveI = confIn.AdaptiveI;
    confOut->AdaptiveB = confIn.AdaptiveB;
    confOut->AdaptiveMBQP = confIn.AdaptiveMBQP;
}

inline void OffPreEncLATools(mfxExtEncToolsConfig* conf)
{
    mfxExtEncToolsConfig confIn = {};
    SetToolsStatus(&confIn, false);
    CopyPreEncLATools(confIn, conf);
}

inline bool isPreEncSCD(mfxExtEncToolsConfig const & conf, mfxEncToolsCtrl const & ctrl)
{
    return ((IsOn(conf.AdaptiveI) ||
        IsOn(conf.AdaptiveB) ||
        IsOn(conf.AdaptiveRefP) ||
        IsOn(conf.AdaptiveRefB) ||
        IsOn(conf.AdaptiveLTR) ||
        IsOn(conf.AdaptivePyramidQuantP) ||
        IsOn(conf.AdaptivePyramidQuantB) ||
        IsOn(conf.AdaptiveMBQP)) && ctrl.ScenarioInfo != MFX_SCENARIO_GAME_STREAMING);
}
inline bool isPreEncLA(mfxExtEncToolsConfig const & conf, mfxEncToolsCtrl const & ctrl)
{
    return ((IsOn(conf.BRCBufferHints) && IsOn(conf.BRC)) ||
        (ctrl.ScenarioInfo == MFX_SCENARIO_GAME_STREAMING  &&
        (IsOn(conf.AdaptiveI) ||
         IsOn(conf.AdaptiveB) ||
         IsOn(conf.AdaptiveQuantMatrices) ||
         IsOn(conf.BRCBufferHints) ||
         IsOn(conf.AdaptivePyramidQuantP) ||
         IsOn(conf.AdaptivePyramidQuantB) ||
         IsOn(conf.AdaptiveMBQP))));
}

EncTools::EncTools(void* rtmodule, void* etmodule)
    : m_bVPPInit(false)
    , m_bInit(false)
    , m_brc(new EncToolsBRC::BRC_EncTool())
    , m_lpLookAhead(rtmodule)
    , m_config()
    , m_ctrl()
    , m_device(0)
    , m_deviceType(0)
    , m_pAllocator(nullptr)
    , m_pETAllocator(nullptr)
    , m_pmfxAllocatorParams(nullptr)
    , m_mfxSession_LA_ENC(nullptr)
    , m_mfxSession_LA_VPP(rtmodule)
    , m_mfxSession_SCD(rtmodule)
    , m_mfxVppParams_LA()
    , m_mfxVppParams_AEnc()
    , m_VppResponse()
    , m_IntSurfaces_SCD()
    , m_hRTModule(rtmodule)
    , m_FFPrefilterSession(rtmodule)
{
    m_etModule = etmodule;
}

mfxStatus EncTools::GetSupportedConfig(mfxExtEncToolsConfig* config, mfxEncToolsCtrl const * ctrl)
{
    MFX_CHECK_NULL_PTR2(config, ctrl);
    SetToolsStatus(config, false);

    if (ctrl->ScenarioInfo != MFX_SCENARIO_GAME_STREAMING)
    {
        config->BRC = (mfxU16)((ctrl->RateControlMethod == MFX_RATECONTROL_CBR ||
            ctrl->RateControlMethod == MFX_RATECONTROL_VBR) ?
            MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);

        if (ctrl->MaxGopRefDist == 16 ||
            ctrl->MaxGopRefDist == 8 ||
            ctrl->MaxGopRefDist == 4 ||
            ctrl->MaxGopRefDist == 2 ||
            ctrl->MaxGopRefDist == 1)
        {
            config->SceneChange = MFX_CODINGOPTION_ON;
            config->AdaptiveI = MFX_CODINGOPTION_ON;
            config->AdaptiveB = MFX_CODINGOPTION_ON;
            config->AdaptiveRefP = MFX_CODINGOPTION_ON;
            config->AdaptiveRefB = MFX_CODINGOPTION_ON;
            config->AdaptiveLTR = MFX_CODINGOPTION_ON;
            config->AdaptivePyramidQuantP = MFX_CODINGOPTION_ON;
            config->AdaptivePyramidQuantB = MFX_CODINGOPTION_ON;
            config->AdaptiveMBQP = MFX_CODINGOPTION_ON;
            config->AdaptiveQuantMatrices = MFX_CODINGOPTION_ON;
            if (ctrl->MaxDelayInFrames > ctrl->MaxGopRefDist && IsOn(config->BRC))
                config->BRCBufferHints = MFX_CODINGOPTION_ON;
        }
    }
#if defined (MFX_ENABLE_ENCTOOLS_LPLA)
    else
    {
        config->AdaptiveQuantMatrices = MFX_CODINGOPTION_ON;
        config->BRCBufferHints = MFX_CODINGOPTION_ON;
        config->SceneChange = MFX_CODINGOPTION_ON;
        config->AdaptivePyramidQuantP = MFX_CODINGOPTION_ON;
        config->AdaptiveI = MFX_CODINGOPTION_ON;
        config->AdaptiveB = MFX_CODINGOPTION_ON;
        config->AdaptivePyramidQuantB = MFX_CODINGOPTION_ON;
     }
#endif
    return MFX_ERR_NONE;
}
mfxStatus EncTools::GetActiveConfig(mfxExtEncToolsConfig* pConfig)
{
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(pConfig);

    *pConfig = m_config;

    return MFX_ERR_NONE;
}

mfxStatus EncTools::GetDelayInFrames(mfxExtEncToolsConfig const * config, mfxEncToolsCtrl const * ctrl, mfxU32 *numFrames)
{
    MFX_CHECK_NULL_PTR3(config, ctrl, numFrames);

    *numFrames = (isPreEncSCD(*config, *ctrl)) ? ctrl->MaxGopRefDist : 0; // MaxMiniGOP delay needed

    if (isPreEncLA(*config, *ctrl))
    {
        *numFrames = std::max(*numFrames, (mfxU32)ctrl->MaxDelayInFrames);
    }

    return MFX_ERR_NONE;
}

mfxStatus EncTools::InitVPPSession(MFXDLVideoSession* pmfxSession)
{
    MFX_CHECK_NULL_PTR1(pmfxSession);
    mfxStatus sts;

    if (mfxSession(*pmfxSession) == 0)
    {
        mfxInitParam initPar = {};
        initPar.Version.Major = 1;
        initPar.Version.Minor = 0;
        initPar.Implementation = MFX_IMPL_HARDWARE;
        initPar.Implementation |= (m_deviceType == MFX_HANDLE_D3D11_DEVICE ? MFX_IMPL_VIA_D3D11 :
            (m_deviceType == MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9 ? MFX_IMPL_VIA_D3D9 : MFX_IMPL_VIA_VAAPI));
        initPar.GPUCopy = MFX_GPUCOPY_DEFAULT;

        sts = pmfxSession->InitEx(initPar);
        MFX_CHECK_STS(sts);
    }

    sts = pmfxSession->SetFrameAllocator(m_pAllocator);
    MFX_CHECK_STS(sts);

    sts = pmfxSession->SetHandle((mfxHandleType)m_deviceType, m_device);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

inline bool VPPParamsChanged(mfxVideoParam & prev, mfxVideoParam & cur)
{
    bool changed = ((prev.vpp.In.CropW != cur.vpp.In.CropW) || (prev.vpp.In.CropH != cur.vpp.In.CropH)
        || (prev.vpp.In.Width != cur.vpp.In.Width) || (prev.vpp.In.Height != cur.vpp.In.Height));
    changed = changed ||
        ((prev.vpp.Out.CropW != cur.vpp.Out.CropW) || (prev.vpp.Out.CropH != cur.vpp.Out.CropH)
        || (prev.vpp.Out.Width != cur.vpp.Out.Width) || (prev.vpp.Out.Height != cur.vpp.Out.Height));
    return changed;
}

mfxStatus EncTools::ResetVPP(mfxEncToolsCtrl const& ctrl)
{
    MFX_CHECK(m_bVPPInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(m_device && m_pAllocator, MFX_ERR_UNDEFINED_BEHAVIOR);
    mfxStatus sts = MFX_ERR_NONE;

    // Init sessions
    if (isPreEncSCD(m_config, ctrl) && !m_mfxSession_SCD)
    {
        sts = InitVPPSession(&m_mfxSession_SCD);
        MFX_CHECK_STS(sts);
    }

    //common LA and SCD

    mfxVideoParam prev_mfxVppParams_LA = m_mfxVppParams_LA;
    mfxVideoParam prev_mfxVppParams_AEnc = m_mfxVppParams_AEnc;
    sts = InitMfxVppParams(ctrl);
    MFX_CHECK_STS(sts);

    if (isPreEncLA(m_config, ctrl))
    {
        if (!m_pmfxVPP_LA)
        {
            InitVPP_LA(ctrl);
        }
        else 
        {
            if(VPPParamsChanged(prev_mfxVppParams_LA, m_mfxVppParams_LA))
            {
                CloseVPP_LA();
                InitVPP_LA(ctrl);
            }
        }
    }

    //SCD VPP
    if (isPreEncSCD(m_config, ctrl))
    {
        bool toInit = true;
        if (!m_pmfxVPP_SCD)
        {
            m_pmfxVPP_SCD.reset(new MFXDLVideoVPP(m_mfxSession_SCD, m_hRTModule));
            MFX_CHECK(m_pmfxVPP_SCD, MFX_ERR_MEMORY_ALLOC);
        }
        else
        {
            if (VPPParamsChanged(prev_mfxVppParams_AEnc, m_mfxVppParams_AEnc))
            {
                m_pmfxVPP_SCD->Close();
                if (m_mfxVppParams_AEnc.vpp.Out.Width * m_mfxVppParams_AEnc.vpp.Out.Height >
                    prev_mfxVppParams_AEnc.vpp.Out.Width * prev_mfxVppParams_AEnc.vpp.Out.Height)
                {
                    if (m_IntSurfaces_SCD.Data.Y)
                        delete[] m_IntSurfaces_SCD.Data.Y;
                    //memory allocation for SCD
                    m_IntSurfaces_SCD.Data.Y = new mfxU8[m_mfxVppParams_AEnc.vpp.Out.Width * m_mfxVppParams_AEnc.vpp.Out.Height * 3 / 2];
                }
            }
            else
                toInit = false;
        }

        if (toInit)
        {
            sts = m_pmfxVPP_SCD->Init(&m_mfxVppParams_AEnc);
            MFX_CHECK_STS(sts);
            m_IntSurfaces_SCD.Info = m_mfxVppParams_AEnc.vpp.Out;
            m_IntSurfaces_SCD.Data.UV = m_IntSurfaces_SCD.Data.Y + m_IntSurfaces_SCD.Info.Width * m_IntSurfaces_SCD.Info.Height;
            m_IntSurfaces_SCD.Data.Pitch = m_IntSurfaces_SCD.Info.Width;
        }
    }

    return sts;
}

mfxStatus EncTools::InitVPP(mfxEncToolsCtrl const& ctrl)
{
    MFX_CHECK(!m_bVPPInit, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_device && m_pAllocator, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxStatus sts;

    // Init sessions
    if (isPreEncSCD(m_config, ctrl))
    {
        sts = InitVPPSession(&m_mfxSession_SCD);
        MFX_CHECK_STS(sts);
    }
    //common LA and SCD
    sts = InitMfxVppParams(ctrl);
    MFX_CHECK_STS(sts);

    //LA VPP
    if (isPreEncLA(m_config, ctrl))
    {
        InitVPP_LA(ctrl);
    }

    //SCD VPP
    if (isPreEncSCD(m_config, ctrl))
    {
        m_pmfxVPP_SCD.reset(new MFXDLVideoVPP(m_mfxSession_SCD, m_hRTModule));
        MFX_CHECK(m_pmfxVPP_SCD, MFX_ERR_MEMORY_ALLOC);

        sts = m_pmfxVPP_SCD->Init(&m_mfxVppParams_AEnc);
        MFX_CHECK_STS(sts);

        //memory allocation for SCD
        m_IntSurfaces_SCD = {};
        m_IntSurfaces_SCD.Info = m_mfxVppParams_AEnc.vpp.Out;
        m_IntSurfaces_SCD.Data.Y = new mfxU8[m_IntSurfaces_SCD.Info.Width * m_IntSurfaces_SCD.Info.Height * 3 / 2];
        m_IntSurfaces_SCD.Data.UV = m_IntSurfaces_SCD.Data.Y + m_IntSurfaces_SCD.Info.Width * m_IntSurfaces_SCD.Info.Height;
        m_IntSurfaces_SCD.Data.Pitch = m_IntSurfaces_SCD.Info.Width;
    }

    m_bVPPInit = true;
    return MFX_ERR_NONE;
}

mfxStatus EncTools::InitVPP_LA(mfxEncToolsCtrl const& /*ctrl*/){
    //create LA VPP session and join it to LA ENC session
    m_mfxSession_LA_ENC = m_lpLookAhead.GetEncSession();
    MFX_CHECK(m_mfxSession_LA_ENC != nullptr, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxStatus sts{};
    sts = InitVPPSession(&m_mfxSession_LA_VPP);
    MFX_CHECK_STS(sts);

    sts = m_mfxSession_LA_VPP.JoinSession(*m_mfxSession_LA_ENC);
    MFX_CHECK_STS(sts);

    //init LA VPP
    m_pmfxVPP_LA.reset(new MFXDLVideoVPP(m_mfxSession_LA_VPP, m_hRTModule));
    MFX_CHECK(m_pmfxVPP_LA, MFX_ERR_MEMORY_ALLOC);
    mfxExtVPPScaling vppScalingMode = {};
    vppScalingMode.Header.BufferId = MFX_EXTBUFF_VPP_SCALING;
    vppScalingMode.Header.BufferSz = sizeof(vppScalingMode);
    vppScalingMode.ScalingMode = MFX_SCALING_MODE_LOWPOWER;
    vppScalingMode.InterpolationMethod = MFX_INTERPOLATION_NEAREST_NEIGHBOR;
    std::vector<mfxExtBuffer*> extParams;
    extParams.push_back(&vppScalingMode.Header);
    m_mfxVppParams_LA.ExtParam = extParams.data();
    m_mfxVppParams_LA.NumExtParam = (mfxU16)extParams.size();

    sts = m_pmfxVPP_LA->Init(&m_mfxVppParams_LA);
    m_mfxVppParams_LA.ExtParam = nullptr;
    m_mfxVppParams_LA.NumExtParam = 0;
    MFX_CHECK_STS(sts);

    //allocate surfaces for LA
    mfxFrameSurface1* surf = nullptr;
    sts = m_mfxSession_LA_ENC->GetSurfaceForEncode(&surf);
    MFX_CHECK_STS(sts);
    m_pIntSurfaces_LA.push_back(*surf);

    return MFX_ERR_NONE;
}

mfxStatus EncTools::InitMfxVppParams(mfxEncToolsCtrl const & ctrl)
{
    //common for LA and SCD
    mfxVideoParam mfxVppParams_Common{};
    mfxVppParams_Common.vpp.In = ctrl.FrameInfo;
    mfxVppParams_Common.vpp.Out = mfxVppParams_Common.vpp.In;

    if (!mfxVppParams_Common.vpp.In.CropW)
        mfxVppParams_Common.vpp.In.CropW = mfxVppParams_Common.vpp.In.Width;

    if (!mfxVppParams_Common.vpp.In.CropH)
        mfxVppParams_Common.vpp.In.CropH = mfxVppParams_Common.vpp.In.Height;

    if(ctrl.ScenarioInfo != MFX_SCENARIO_GAME_STREAMING && (mfxVppParams_Common.vpp.Out.BitDepthLuma == 10 || mfxVppParams_Common.vpp.Out.FourCC == MFX_FOURCC_YUY2)){
        mfxVppParams_Common.vpp.Out.FourCC = MFX_FOURCC_NV12;
        mfxVppParams_Common.vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        mfxVppParams_Common.vpp.Out.BitDepthLuma = 8;
        mfxVppParams_Common.vpp.Out.BitDepthChroma = 8;
        mfxVppParams_Common.vpp.Out.Shift = 0;
    }

    MFXDLVideoSession* pSession = nullptr;
    if (isPreEncSCD(m_config, ctrl))
        pSession = &m_mfxSession_SCD;
    else if (isPreEncLA(m_config, ctrl))
        pSession = m_lpLookAhead.GetEncSession();

    MFX_CHECK(pSession != nullptr, MFX_ERR_UNDEFINED_BEHAVIOR);

    //LA
    if (isPreEncLA(m_config, ctrl))
    {
        m_mfxVppParams_LA = mfxVppParams_Common;
        mfxU32 ignore = 0;
        m_lpLookAhead.GetDownScaleParams(m_mfxVppParams_LA.vpp.Out, ignore);
        m_mfxVppParams_LA.IOPattern = ctrl.IOPattern | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }
    //SCD
    if (isPreEncSCD(m_config, ctrl))
    {
        m_mfxVppParams_AEnc = mfxVppParams_Common;
        mfxFrameInfo frameInfo;
        mfxStatus sts = m_scd.GetInputFrameInfo(frameInfo);
        MFX_CHECK_STS(sts);
        m_mfxVppParams_AEnc.vpp.Out.Width = frameInfo.Width;
        m_mfxVppParams_AEnc.vpp.Out.Height = frameInfo.Height;
        m_mfxVppParams_AEnc.vpp.Out.CropW = m_mfxVppParams_AEnc.vpp.Out.Width;
        m_mfxVppParams_AEnc.vpp.Out.CropH = m_mfxVppParams_AEnc.vpp.Out.Height;
        m_mfxVppParams_AEnc.IOPattern = ctrl.IOPattern | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }
    return MFX_ERR_NONE;
}

mfxStatus EncTools::CloseVPP()
{
    MFX_CHECK(m_bVPPInit, MFX_ERR_NOT_INITIALIZED);
    mfxStatus sts = MFX_ERR_NONE;

    CloseVPP_LA();

    if (m_IntSurfaces_SCD.Data.Y)
    {
        delete[] m_IntSurfaces_SCD.Data.Y;
        m_IntSurfaces_SCD = {};
    }

    if (m_pmfxVPP_SCD)
    {
        m_pmfxVPP_SCD->Close();
        m_pmfxVPP_SCD.reset();
    }
    if (m_mfxSession_SCD)
    {
        sts = m_mfxSession_SCD.Close();
        MFX_CHECK_STS(sts);
    }
    m_bVPPInit = false;
    return sts;
}

static void UpdateStatus(mfxStatus& res, const mfxStatus cur)
{
    res = res < 0 ? res : cur < 0 ? cur : res > 0 ? res : cur;
}

mfxStatus EncTools::CloseVPP_LA(){
    mfxStatus res{}, cur{};

    for(mfxFrameSurface1& s : m_pIntSurfaces_LA)
    {
        if(s.FrameInterface)
        {
            cur = s.FrameInterface->Release(&s);
            UpdateStatus(res, cur);
        }
    }
    m_pIntSurfaces_LA.clear();

    if (m_pmfxVPP_LA)
    {
        cur = m_pmfxVPP_LA->Close();
        UpdateStatus(res, cur);
        m_pmfxVPP_LA.reset();
    }

    if (m_mfxSession_LA_VPP)
    {
        cur = m_mfxSession_LA_ENC->DisjoinSession();
        UpdateStatus(res, cur);

        if(cur == MFX_ERR_NONE)
        {
            cur = m_mfxSession_LA_VPP.Close();
            UpdateStatus(res, cur);
        }
    }

    m_mfxSession_LA_ENC = nullptr;

    return res;
}

mfxStatus EncTools::GetDeviceAllocator(mfxEncToolsCtrl const* ctrl)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxEncToolsCtrlExtDevice* extDevice = (mfxEncToolsCtrlExtDevice*)Et_GetExtBuffer(ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_ENCTOOLS_DEVICE);
    if (extDevice)
    {
        m_device = extDevice->DeviceHdl;
        m_deviceType = extDevice->HdlType;
    }
    if (!m_device)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    mfxEncToolsCtrlExtAllocator* extAlloc = (mfxEncToolsCtrlExtAllocator*)Et_GetExtBuffer(ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_ENCTOOLS_ALLOCATOR);
    if (extAlloc)
        m_pAllocator = extAlloc->pAllocator;

    if (!m_pAllocator)
    {
            if (m_deviceType == MFX_HANDLE_VA_DISPLAY)
            {
                m_pETAllocator = new vaapiFrameAllocator;
                MFX_CHECK_NULL_PTR1(m_pETAllocator);

                vaapiAllocatorParams* pvaapiAllocParams = new vaapiAllocatorParams;
                MFX_CHECK_NULL_PTR1(pvaapiAllocParams);

                pvaapiAllocParams->m_dpy = (VADisplay)m_device;
                m_pmfxAllocatorParams = pvaapiAllocParams;
            }
            else
                return MFX_ERR_UNDEFINED_BEHAVIOR;

        MFX_CHECK_NULL_PTR1(m_pETAllocator);

        sts = m_pETAllocator->Init(m_pmfxAllocatorParams);
        MFX_CHECK_STS(sts);
        m_pAllocator = m_pETAllocator;
    }
    return sts;
}

mfxStatus EncTools::Init(mfxExtEncToolsConfig const * pConfig, mfxEncToolsCtrl const * ctrl)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR2(pConfig, ctrl);
    MFX_CHECK(!m_bInit, MFX_ERR_UNDEFINED_BEHAVIOR);

    if(IsOn(pConfig->SaliencyMapHint))
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    if(m_UseFFPrefilter && IsOn(pConfig->SaliencyMapHint)){
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    m_ctrl = *ctrl;

    bool needVPP = isPreEncSCD(*pConfig, *ctrl) || isPreEncLA(*pConfig, *ctrl);
    if (needVPP)
    {
        sts = GetDeviceAllocator(ctrl);
        MFX_CHECK_STS(sts);
    }

    SetToolsStatus(&m_config, false);
    if (IsOn(pConfig->BRC))
    {
        sts = m_brc->Init(*ctrl, IsOn(pConfig->AdaptiveMBQP), IsOn(pConfig->AdaptiveLTR));
        MFX_CHECK_STS(sts);
        m_config.BRC = MFX_CODINGOPTION_ON;
    }
    if (isPreEncSCD(*pConfig, *ctrl))
    {
        sts = m_scd.Init(*ctrl, *pConfig);
        MFX_CHECK_STS(sts);
        // to add request to m_scd about supported tools
        CopyPreEncSCTools(*pConfig, &m_config);
    }

    if (isPreEncLA(*pConfig, *ctrl))
    {
        m_lpLookAhead.SetAllocator(m_pAllocator);
        sts = m_lpLookAhead.Init(*ctrl, *pConfig);
        MFX_CHECK_STS(sts);
        CopyPreEncLATools(*pConfig, &m_config);
    }

    if (needVPP)
    {
        sts = InitVPP(*ctrl);
        MFX_CHECK_STS(sts);
    }


    if(m_UseFFPrefilter)
    {
        sts = InitFFPrefilter(*ctrl);
        MFX_CHECK_STS(sts);
    }
    else
    {
        m_PercEncFilter.Init(ctrl->FrameInfo);
    }

    m_bInit = true;
    return sts;
}

mfxStatus EncTools::Close()
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);

    if (m_bVPPInit)
        sts = CloseVPP();

    if (isPreEncSCD(m_config, m_ctrl))
    {
        m_scd.Close();
        OffPreEncSCDTools(&m_config);
    }

    if (isPreEncLA(m_config,  m_ctrl))
    {
        m_lpLookAhead.Close();
        OffPreEncLATools(&m_config);
    }

    if (IsOn(m_config.BRC))
    {
        m_brc->Close();
        m_config.BRC = false;
    }
    

    CloseFFPrefilter();

    m_bInit = false;

    return sts;
}

mfxStatus EncTools::Reset(mfxExtEncToolsConfig const * config, mfxEncToolsCtrl const * ctrl)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR2(config,ctrl);
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);

    bool needVPP = isPreEncSCD(*config, *ctrl) || isPreEncLA(*config, *ctrl);
    if (needVPP)
    {
        mfxHDL curDevice = m_device;
        mfxFrameAllocator* curpAlloc = m_pAllocator;
        sts = GetDeviceAllocator(ctrl);
        MFX_CHECK_STS(sts);

        if (m_pAllocator != curpAlloc || m_device != curDevice)
        {
            Close();
            sts  = Init(config, ctrl);
            return sts;
        }
    }

    if (IsOn(config->BRC))
    {
        MFX_CHECK(m_config.BRC, MFX_ERR_UNSUPPORTED);
        sts = m_brc->Reset(*ctrl, IsOn(config->AdaptiveMBQP), IsOn(config->AdaptiveLTR));
        MFX_CHECK_STS(sts);
    }
    if (isPreEncSCD(*config, *ctrl))
    {
        // to add check if Close/Init is real needed
        if (isPreEncSCD(m_config, m_ctrl))
            m_scd.Close();
        sts = m_scd.Init(*ctrl, *config);
        MFX_CHECK_STS(sts);
    }

     if (isPreEncLA(*config, *ctrl))
     {
         if (isPreEncLA(m_config, m_ctrl))
            sts = m_lpLookAhead.Reset(*ctrl, *config);
         else
            sts = m_lpLookAhead.Init(*ctrl, *config);
         MFX_CHECK_STS(sts);
     }

     if (needVPP)
     {
         sts = ResetVPP(*ctrl);
     }

    return sts;
}

mfxStatus EncTools::VPPDownScaleSurface(MFXDLVideoSession* /*m_pmfxSession*/, MFXDLVideoVPP* pVPP, mfxSyncPoint* pVppSyncp, mfxFrameSurface1* pInSurface, mfxFrameSurface1* pOutSurface/*, bool doSync*/)
{
    mfxStatus sts;
    MFX_CHECK_NULL_PTR2(pVPP, pVppSyncp);
    MFX_CHECK_NULL_PTR2(pInSurface, pOutSurface);

    sts = pVPP->RunFrameVPPAsync(pInSurface, pOutSurface, NULL, pVppSyncp);
    return sts;
}

static void IgnoreMoreDataStatus(mfxStatus &sts)
{
    if (sts == MFX_ERR_MORE_DATA)
        sts = MFX_ERR_NONE;
}

// Compute AV1 segmentIDs and AltQIndexes
// Note: Due to HW limitation, one segment must be reserved for HW use.
//       PAQ assumes segment 7 is reserved for HW use.  This assumption imposes constraint on segMap file in commandline (ie,  segMap must has all numbers from 0 to 6).
static mfxStatus ProcessAV1SegmentationData(const mfxEncToolsTaskParam* par, mfxEncToolsHintQPMap* qpMapHint, mfxI32 QpY)
{
    MFX_CHECK(par && qpMapHint, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxExtAV1Segmentation* seg = (mfxExtAV1Segmentation *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_AV1_SEGMENTATION);
    MFX_CHECK(seg, MFX_ERR_UNDEFINED_BEHAVIOR);

    // AV1 segment IDs
    //  - QpY is HEVC QP for frame. qpMapHint.ExtQpMap.QP[] is HEVC QP for blocks/segments.
    //  - Note SegmentID is derived from HEVC QP

    mfxI16 segQPs[EncToolsBRC::TOTAL_NUM_AV1_SEGMENTS_FOR_ENCTOOLS] = {-1, -1, -1, -1, -1, -1, -1};  //"-1" means feature is disabled
    mfxU8 maxQpY = *(std::max_element(qpMapHint->ExtQpMap.QP, qpMapHint->ExtQpMap.QP + qpMapHint->ExtQpMap.NumQPAlloc));
    for(mfxU32 i = 0; i < qpMapHint->ExtQpMap.NumQPAlloc ; i++) {
        mfxI16 seg_qp = (mfxI16)(qpMapHint->ExtQpMap.QP[i]);
        mfxU8 id = mfxU8(mfx::clamp((mfxI16)maxQpY - seg_qp, 0, EncToolsBRC::TOTAL_NUM_AV1_SEGMENTS_FOR_ENCTOOLS - 1)); // PAQ uses segments [0..6]
        segQPs[id] = seg_qp;
        seg->SegmentIds[i] = id;
    }

    //Update new AltQIndex (feature_value)
    //  - Convert from HEVC QP to AV1 q-idx
    mfxI16 q_idx = mfxI16(EncToolsBRC::HEVC_QP_2_AV1_DC_Q_IDX[QpY]);
    for (mfxI16 id = 0; id < EncToolsBRC::TOTAL_NUM_AV1_SEGMENTS_FOR_ENCTOOLS; id++) {
        if (segQPs[id] != -1) {
            seg->Segment[id].FeatureEnabled |= MFX_VP9_SEGMENT_FEATURE_QINDEX;
            mfxI16 altQIndex = (mfxI16)EncToolsBRC::HEVC_QP_2_AV1_DC_Q_IDX[segQPs[id]] - q_idx;
            seg->Segment[id].AltQIndex = mfx::clamp(altQIndex, (mfxI16)-255, (mfxI16)255);
        } else {
            seg->Segment[id].FeatureEnabled &= (~MFX_VP9_SEGMENT_FEATURE_QINDEX);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus EncTools::Submit(mfxEncToolsTaskParam const * par)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);
    mfxEncToolsFrameToAnalyze  *pFrameData = (mfxEncToolsFrameToAnalyze *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_FRAME_TO_ANALYZE);

    if (pFrameData)
    {

        pFrameData->Surface->Data.FrameOrder = par->DisplayOrder;

        if (isPreEncSCD(m_config, m_ctrl) || isPreEncLA(m_config, m_ctrl))
        {
            MFX_CHECK(m_bVPPInit, MFX_ERR_NOT_INITIALIZED);

            mfxU16 FrameType = 0;
            mfxSyncPoint vppSyncp_SCD{};
            mfxSyncPoint vppSyncp_LA{};
            mfxSyncPoint encSyncp_LA{};
            
            //SCD only case
            if (isPreEncSCD(m_config, m_ctrl) && !isPreEncLA(m_config, m_ctrl))
            {
                m_IntSurfaces_SCD.Data.FrameOrder = par->DisplayOrder;

                sts = VPPDownScaleSurface(&m_mfxSession_SCD, m_pmfxVPP_SCD.get(), &vppSyncp_SCD, pFrameData->Surface, &m_IntSurfaces_SCD);
                MFX_CHECK_STS(sts);
                sts = m_mfxSession_SCD.SyncOperation(vppSyncp_SCD, ENC_TOOLS_WAIT_INTERVAL);
                MFX_CHECK_STS(sts);

                sts = m_scd.SubmitFrame(&m_IntSurfaces_SCD);
                IgnoreMoreDataStatus(sts);
                return sts;
            }

            //LA only case
            else if (!isPreEncSCD(m_config, m_ctrl) && isPreEncLA(m_config, m_ctrl)) 
            {
                m_pIntSurfaces_LA[0].Data.FrameOrder = par->DisplayOrder;

                sts = VPPDownScaleSurface(&m_mfxSession_LA_VPP, m_pmfxVPP_LA.get(), &vppSyncp_LA, pFrameData->Surface, m_pIntSurfaces_LA.data());
                MFX_CHECK_STS(sts);

                sts = m_lpLookAhead.Submit(m_pIntSurfaces_LA.data(), FrameType, &encSyncp_LA);
                MFX_CHECK_STS(sts);
                sts = m_mfxSession_LA_ENC->SyncOperation(encSyncp_LA, ENC_TOOLS_WAIT_INTERVAL);
                MFX_CHECK_STS(sts);

                sts = m_lpLookAhead.SaveEncodedFrameSize(m_pIntSurfaces_LA.data(), 0 /*frame type*/);
                return sts;
            }

            //SCD and LA case
            else if (isPreEncSCD(m_config, m_ctrl) && isPreEncLA(m_config, m_ctrl)) 
            {
                m_IntSurfaces_SCD.Data.FrameOrder = m_pIntSurfaces_LA[0].Data.FrameOrder = par->DisplayOrder;

                sts = VPPDownScaleSurface(&m_mfxSession_LA_VPP, m_pmfxVPP_LA.get(), &vppSyncp_LA, pFrameData->Surface, m_pIntSurfaces_LA.data());
                MFX_CHECK_STS(sts);
                sts = VPPDownScaleSurface(&m_mfxSession_SCD, m_pmfxVPP_SCD.get(), &vppSyncp_SCD, pFrameData->Surface, &m_IntSurfaces_SCD);
                MFX_CHECK_STS(sts);

                //LA depends on SCD
                if (IsOn(m_config.AdaptiveI))
                {
                    sts = m_mfxSession_SCD.SyncOperation(vppSyncp_SCD, ENC_TOOLS_WAIT_INTERVAL);
                    MFX_CHECK_STS(sts);
                    sts = m_scd.SubmitFrame(&m_IntSurfaces_SCD);
                    IgnoreMoreDataStatus(sts);
                    MFX_CHECK_STS(sts);

                    m_scd.GetIntraDecision(par->DisplayOrder, &FrameType);
                    if (FrameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR))
                    {
                        // convert to IREFIDR for Analysis
                        FrameType = (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR); 
                    }

                    sts = m_lpLookAhead.Submit(m_pIntSurfaces_LA.data(), FrameType, &encSyncp_LA);
                    MFX_CHECK_STS(sts);
                    sts = m_mfxSession_LA_ENC->SyncOperation(encSyncp_LA, ENC_TOOLS_WAIT_INTERVAL);
                    MFX_CHECK_STS(sts);

                    sts = m_lpLookAhead.SaveEncodedFrameSize(m_pIntSurfaces_LA.data(), FrameType);
                    return sts;
                }

                //LA can run in parallel with SCD
                else
                {
                    //run LA
                    sts = m_lpLookAhead.Submit(m_pIntSurfaces_LA.data(), FrameType, &encSyncp_LA);
                    MFX_CHECK_STS(sts);
                        
                    //run SCD
                    sts = m_mfxSession_SCD.SyncOperation(vppSyncp_SCD, ENC_TOOLS_WAIT_INTERVAL);
                    MFX_CHECK_STS(sts);
                    sts = m_scd.SubmitFrame(&m_IntSurfaces_SCD);
                    IgnoreMoreDataStatus(sts);
                    MFX_CHECK_STS(sts);

                    //save LA results
                    sts = m_mfxSession_LA_ENC->SyncOperation(encSyncp_LA, ENC_TOOLS_WAIT_INTERVAL);
                    MFX_CHECK_STS(sts);

                    m_scd.GetIntraDecision(par->DisplayOrder, &FrameType);
                    if (FrameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR))
                    {
                        // convert to IREFIDR for Analysis
                        FrameType = (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);
                    }

                    sts = m_lpLookAhead.SaveEncodedFrameSize(m_pIntSurfaces_LA.data(), FrameType);
                    return sts;
                }
            }
            else
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }


    }


    mfxEncToolsBRCEncodeResult  *pEncRes = (mfxEncToolsBRCEncodeResult *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_BRC_ENCODE_RESULT);

    // Since AV1 BRC uses HEVC BRC, so need this conversion
    if(pEncRes && m_ctrl.CodecId == MFX_CODEC_AV1) {
        pEncRes->QpY = EncToolsBRC::AV1_DC_Q_IDX_2_HEVC_QP[pEncRes->QpY];
    }

    if (pEncRes && isPreEncSCD(m_config, m_ctrl)) {
        m_scd.ReportEncResult(par->DisplayOrder, *pEncRes);
    }
    if (pEncRes && IsOn(m_config.BRC))
    {
        return m_brc->ReportEncResult(par->DisplayOrder, *pEncRes);
    }

    mfxEncToolsBRCFrameParams  *pFrameStruct = (mfxEncToolsBRCFrameParams *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_BRC_FRAME_PARAM );
    if (pFrameStruct && IsOn(m_config.BRC))
    {
        sts = m_brc->SetFrameStruct(par->DisplayOrder, *pFrameStruct);
        MFX_CHECK_STS(sts);
    }

    mfxEncToolsBRCBufferHint  *pBRCHints = (mfxEncToolsBRCBufferHint *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_BRC_BUFFER_HINT);
    if (pBRCHints && IsOn(m_config.BRC))
    {
        sts = m_brc->ReportBufferHints(par->DisplayOrder, *pBRCHints);
        MFX_CHECK_STS(sts);
    }

    mfxEncToolsHintPreEncodeGOP *pPreEncGOP = (mfxEncToolsHintPreEncodeGOP *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_HINT_GOP);
    if (pPreEncGOP && IsOn(m_config.BRC))
    {
        if (pPreEncGOP->QPModulation == MFX_QP_MODULATION_EXPLICIT) {
            //MLAPQ Inference is only done when explicit modulation is used
            sts = m_scd.GetMLApqDeltaQp(par->DisplayOrder, pPreEncGOP->QPDeltaExplicitModulation);
            MFX_CHECK_STS(sts);
        }
        return m_brc->ReportGopHints(par->DisplayOrder, *pPreEncGOP);
    }

    mfxEncToolsPrefilterParam *pPrefilterParam = (mfxEncToolsPrefilterParam *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_PREFILTER_PARAM);
    if(pPrefilterParam && !m_UseFFPrefilter)
    {
        mfxFrameSurface1 *in = pPrefilterParam->InSurface;
        mfxFrameSurface1 *out = pPrefilterParam->OutSurface;

        MFX_CHECK_NULL_PTR1(m_pAllocator);
        MFX_CHECK_NULL_PTR3(in, out, out->FrameInterface);

        sts = m_pAllocator->Lock(m_pAllocator->pthis, in->Data.MemId, &(in->Data));
        MFX_CHECK_STS(sts);

        mfx::OnExit unlock([&in, this]()
        {
            mfxStatus sts = m_pAllocator->Unlock(m_pAllocator->pthis, in->Data.MemId, &(in->Data));
            std::ignore = MFX_STS_TRACE(sts);
        });

        sts = out->FrameInterface->Map(out, MFX_MAP_WRITE);
        MFX_CHECK_STS(sts);

        mfx::OnExit unmap([&out]()
        {
            mfxStatus sts =     out->FrameInterface->Unmap(out);
            std::ignore = MFX_STS_TRACE(sts);
        });

        mfxEncToolsHintSaliencyMap *pSM = (mfxEncToolsHintSaliencyMap *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_HINT_SALIENCY_MAP);

        if(pSM)
        {
            sts = m_PercEncFilter.SetModulationMap(*pSM);
            MFX_CHECK_STS(sts);
        }

        sts = m_PercEncFilter.RunFrame(*in, *out, pPrefilterParam->QpY);
        MFX_CHECK_STS(sts);
    }


    if(pPrefilterParam && m_UseFFPrefilter)
    {
        sts = RunFFPrefilter(pPrefilterParam);
        MFX_CHECK_STS(sts);
    }

    return sts;
}

mfxStatus EncTools::Query(mfxEncToolsTaskParam* par, mfxU32 /*timeOut*/)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(m_bInit, MFX_ERR_NOT_INITIALIZED);

    mfxEncToolsHintPreEncodeSceneChange *pPreEncSC = (mfxEncToolsHintPreEncodeSceneChange *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_HINT_SCENE_CHANGE);
    if (pPreEncSC && isPreEncSCD(m_config, m_ctrl))
    {
        sts = m_scd.GetSCDecision(par->DisplayOrder, pPreEncSC);
        sts = m_scd.GetPersistenceMap(par->DisplayOrder, pPreEncSC);
        MFX_CHECK_STS(sts);
    }
    mfxEncToolsHintPreEncodeGOP *pPreEncGOP = (mfxEncToolsHintPreEncodeGOP *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_HINT_GOP);
    if (pPreEncGOP)
    {
        if (isPreEncSCD(m_config, m_ctrl))
            sts = m_scd.GetGOPDecision(par->DisplayOrder, pPreEncGOP);
#if defined (MFX_ENABLE_ENCTOOLS_LPLA)
        else if (isPreEncLA(m_config, m_ctrl))
        {
            sts = m_lpLookAhead.Query(par->DisplayOrder, pPreEncGOP);
            if (sts == MFX_ERR_NOT_FOUND)
                sts = MFX_ERR_NONE;
        }
#endif
        MFX_CHECK_STS(sts);
    }
    mfxEncToolsHintPreEncodeARefFrames *pPreEncARef = (mfxEncToolsHintPreEncodeARefFrames *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_HINT_AREF);
    if (pPreEncARef && isPreEncSCD(m_config, m_ctrl))
    {
        sts = m_scd.GetARefDecision(par->DisplayOrder, pPreEncARef);
        MFX_CHECK_STS(sts);
    }

#if defined (MFX_ENABLE_ENCTOOLS_LPLA)
    mfxEncToolsHintQuantMatrix *pCqmHint = (mfxEncToolsHintQuantMatrix  *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_HINT_MATRIX);
    if (pCqmHint && isPreEncLA(m_config, m_ctrl))
    {
        sts = m_lpLookAhead.Query(par->DisplayOrder, pCqmHint);
        if (sts == MFX_ERR_NOT_FOUND)
            sts = MFX_ERR_NONE;
        MFX_CHECK_STS(sts);
    }
#endif

    mfxEncToolsBRCBufferHint *bufferHint = (mfxEncToolsBRCBufferHint  *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_BRC_BUFFER_HINT);
    if (bufferHint && isPreEncLA(m_config, m_ctrl))
    {
        sts = m_lpLookAhead.Query(par->DisplayOrder, bufferHint);
        if (sts == MFX_ERR_NOT_FOUND)
            sts = MFX_ERR_NONE;
        MFX_CHECK_STS(sts);
    }

    mfxEncToolsBRCStatus  *pFrameSts = (mfxEncToolsBRCStatus *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_BRC_STATUS);
    if (pFrameSts && IsOn(m_config.BRC))
    {
        return m_brc->UpdateFrame(par->DisplayOrder, pFrameSts);
    }

    mfxEncToolsBRCQuantControl *pFrameQp = (mfxEncToolsBRCQuantControl *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_BRC_QUANT_CONTROL);
    mfxEncToolsHintQPMap* qpMapHint = (mfxEncToolsHintQPMap*)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_HINT_QPMAP);

    // Sanity check
    if (qpMapHint && m_ctrl.CodecId == MFX_CODEC_AV1) {
        mfxU16 w = m_ctrl.FrameInfo.Width;
        mfxU16 pitch = (w + qpMapHint->ExtQpMap.BlockSize - 1) / qpMapHint->ExtQpMap.BlockSize;
        MFX_CHECK(qpMapHint->QpMapPitch == pitch, MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    if (pFrameQp && IsOn(m_config.BRC))
    {
        sts = m_brc->ProcessFrame(par->DisplayOrder, pFrameQp, qpMapHint);
        MFX_CHECK_STS(sts);

        // Note: AV1 BRC is based on HEVC BRC, so need this conversion
        if (m_ctrl.CodecId == MFX_CODEC_AV1) {
            // Compute segmentIDs and AltQIndexes
            if (qpMapHint && qpMapHint->QpMapFilled) {
                ProcessAV1SegmentationData(par, qpMapHint, (mfxI32)(pFrameQp->QpY));
            }

            pFrameQp->QpY =  EncToolsBRC::HEVC_QP_2_AV1_DC_Q_IDX[pFrameQp->QpY];
        }
    }
    mfxEncToolsBRCHRDPos *pHRDPos = (mfxEncToolsBRCHRDPos *)Et_GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_ENCTOOLS_BRC_HRD_POS);
    if (pHRDPos && IsOn(m_config.BRC))
    {
        sts = m_brc->GetHRDPos(par->DisplayOrder, pHRDPos);
        MFX_CHECK_STS(sts);
    }


    return sts;
}

mfxStatus EncTools::Discard(mfxU32 displayOrder)
{
    mfxStatus sts = MFX_ERR_NONE;
    if (isPreEncSCD(m_config, m_ctrl))
        sts = m_scd.CompleteFrame(displayOrder);
    if (IsOn(m_config.BRC))
    {
        m_brc->DiscardFrame(displayOrder);
    }
    return sts;
}

mfxStatus PercEncFilterWrapper::Init(const mfxFrameInfo& info)
{
    if(!AVX2Supported())
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    if (initialized)
        return MFX_ERR_NONE;

    MFX_CHECK(info.CropW >= 16, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(info.CropH >= 2, MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(info.FourCC         == MFX_FOURCC_NV12,         MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(info.ChromaFormat   == MFX_CHROMAFORMAT_YUV420, MFX_ERR_INVALID_VIDEO_PARAM);

    width = info.CropW;
    height = info.CropH;
    previousOutput.resize(size_t(width) * height);

    modulationStride = (width + blockSizeFilter - 1) / blockSizeFilter;
    modulation.resize(size_t(modulationStride) * ((height + blockSizeFilter - 1) / blockSizeFilter));
    parametersFrame.spatialSlope = 2;
    parametersFrame.temporalSlope = 5;
    // Using QP Adaptive as Encoder integration has QP feedback
    parametersFrame.qpAdaptive = true;
    if(!parametersFrame.qpAdaptive)
    {
        // Orig
        parametersBlock[0].spatial.pivot = 0.005909118892594739f;
        parametersBlock[1].spatial.pivot = 0.008541855174858726f;
        parametersBlock[0].spatial.minimum = -0.02285621848581362f;
        parametersBlock[1].spatial.minimum = -0.04005541977955759f;
        parametersBlock[0].spatial.maximum = 0.041140246241535394f;
        parametersBlock[1].spatial.maximum = 0.f;
    }
    else
    {
        parametersBlock[0].spatial.pivot = 0.023914614115868146f;
        parametersBlock[0].spatial.minimum = -0.011074017123454209f;
        parametersBlock[0].spatial.maximum = 0.06830047767528989f;
        parametersBlock[1].spatial.pivot = 0.017164396880008734f;
        parametersBlock[1].spatial.minimum = -0.03023741792966293f;
        parametersBlock[1].spatial.maximum = 0.0436184808511757f;
    }

    parametersBlock[0].temporal.pivot = 0.f;
    parametersBlock[1].temporal.pivot = 0.f;
    parametersBlock[0].temporal.minimum = 0.f;
    parametersBlock[1].temporal.minimum = 0.f;
    parametersBlock[0].temporal.maximum = 0.f;
    parametersBlock[1].temporal.maximum = 0.f;

    filter = std::make_unique<PercEncPrefilter::Filter>(parametersFrame, parametersBlock, width);

    initialized = true;

    return MFX_ERR_NONE;
}

mfxStatus PercEncFilterWrapper::SetModulationMap(const mfxEncToolsHintSaliencyMap &sm)
{
    constexpr mfxU32 blockSize = 8; // source saliency is on 8x8 block granularity
    if(sm.BlockSize != blockSize)
    {
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }

    for (size_t y = 0; y < size_t(height); y += blockSizeFilter)
    {
        for (size_t x = 0; x < size_t(width); x += blockSizeFilter)
        {
            float m = 0.f;
            int count = 0;

            for (size_t dy = 0; dy < std::min<size_t>(blockSizeFilter, height - y); dy += blockSize)
            {
                for (size_t dx = 0; dx <  std::min<size_t>(blockSizeFilter, width - x); dx +=blockSize)
                {
                    m += sm.SaliencyMap[(x + dx) / blockSize + (y + dy) / blockSize * sm.Width];
                    ++count;
                }
            }

            count = count ? count : 1;
            int mod = int(256.f * m /count);
            mod = std::max(mod, 0);
            mod = std::min(mod, 255);
            modulation[x / blockSizeFilter + y / blockSizeFilter * modulationStride] = (uint8_t)mod;
        }
    }

    return MFX_ERR_NONE;

}

mfxStatus PercEncFilterWrapper::RunFrame(mfxFrameSurface1& in, mfxFrameSurface1& out, mfxU32 QpY)
{

    if(!initialized)
    {
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);
    }

    if(!filter || !in.Data.Y || in.Data.Pitch == 0 || !in.Data.Y || in.Data.Pitch == 0)
    {
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }

    filter->processFrame(in.Data.Y, in.Data.Pitch,
                        modulation.data(), modulationStride,
                        previousOutput.data(), width,
                        out.Data.Y, out.Data.Pitch,
                        width, height, QpY);

    //retain a copy of the output for next time...
    for (size_t y = 0; y < size_t(height); ++y)
    {
        std::copy(
            &out.Data.Y[out.Data.Pitch * y],
            &out.Data.Y[out.Data.Pitch * y + width],
            &previousOutput[width * y]);
    }

    // copy chroma
    for (int y = 0; y < height / 2; ++y)
    {
        std::copy(
            &in.Data.UV[in.Data.Pitch * y],
            &in.Data.UV[in.Data.Pitch * y + width],
            &out.Data.UV[out.Data.Pitch * y]);
    }

    return MFX_ERR_NONE;
}

bool PercEncFilterWrapper::AVX2Supported()
{
#if defined(__GNUC__)
    return __builtin_cpu_supports("avx2") > 0;
#else
    int info[4] = {};
    constexpr int EBX_REGISTER = 1;
    constexpr int ECX_REGISTER = 2;
    constexpr int AVX_OSXSAVE_MASK = (1 << 27) | (1 << 28);
    constexpr int AVX2_MASK = 1 << 5;
    constexpr int XMM_YMM_STATES_MASK = 0x6;

    //check that AVX is supported and _xgetbv is enabled
    __cpuidex(info, 0x1, 0);
    if((info[ECX_REGISTER] & AVX_OSXSAVE_MASK) != AVX_OSXSAVE_MASK)
    {
        return false;
    }

    //check that AVX2 is supported
    __cpuidex(info, 0x7, 0);
    if((info[EBX_REGISTER] & AVX2_MASK) != AVX2_MASK)
    {
        return false;
    }

    //check that OS enabled XMM and YMM states support
    return (_xgetbv(_XCR_XFEATURE_ENABLED_MASK) & XMM_YMM_STATES_MASK) == XMM_YMM_STATES_MASK;
#endif
}

mfxStatus EncTools::InitFFPrefilter(mfxEncToolsCtrl const & ctrl)
{
    mfxStatus sts = MFX_ERR_NONE;

    //init session
    MFX_CHECK(m_deviceType==MFX_HANDLE_VA_DISPLAY, MFX_ERR_NOT_IMPLEMENTED);

    mfxInitParam initParam{};
    initParam.Version.Major = MFX_VERSION_MAJOR;
    initParam.Version.Minor = MFX_VERSION_MINOR;
    initParam.Implementation = MFX_IMPL_HARDWARE | MFX_IMPL_VIA_VAAPI;

    sts = m_FFPrefilterSession.InitEx(initParam);
    MFX_CHECK_STS(sts);

    sts = m_FFPrefilterSession.SetFrameAllocator(m_pAllocator);
    MFX_CHECK_STS(sts);

    sts = m_FFPrefilterSession.SetHandle((mfxHandleType)m_deviceType, m_device);
    MFX_CHECK_STS(sts);

    //init VPP video params
    mfxVideoParam videoParam{};
    videoParam.vpp.In = ctrl.FrameInfo;
    videoParam.vpp.Out = videoParam.vpp.In;
    videoParam.IOPattern = ctrl.IOPattern | MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    mfxExtVPPDetail detailBuffer{};
    detailBuffer.Header.BufferId = MFX_EXTBUFF_VPP_DETAIL;
    detailBuffer.Header.BufferSz = sizeof(mfxExtVPPDetail);
    detailBuffer.DetailFactor = 20;

    std::vector<mfxExtBuffer*> extParams;
    extParams.push_back(&detailBuffer.Header);
    videoParam.ExtParam = extParams.data();
    videoParam.NumExtParam = (mfxU16)extParams.size();

    //init VPP
    m_FFPrefilterVPP.reset(new MFXDLVideoVPP(m_FFPrefilterSession, m_hRTModule));
    sts = m_FFPrefilterVPP->Init(&videoParam);

    MFX_RETURN(sts);
}

mfxStatus EncTools::CloseFFPrefilter()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_FFPrefilterVPP)
    {
        m_FFPrefilterVPP->Close();
        m_FFPrefilterVPP.reset();
    }

    if (m_FFPrefilterSession)
    {
        sts = m_FFPrefilterSession.Close();
    }

    return sts;
}

mfxStatus EncTools::RunFFPrefilter(mfxEncToolsPrefilterParam *pPrefilterParam)
{
    MFX_CHECK_NULL_PTR1(pPrefilterParam);
    mfxFrameSurface1 *in = pPrefilterParam->InSurface;
    mfxFrameSurface1 *out = pPrefilterParam->OutSurface;
    MFX_CHECK_NULL_PTR2(in, out);
    MFX_CHECK_NULL_PTR1(m_FFPrefilterVPP);

    mfxSyncPoint prefilterSyncPoint{};
    mfxStatus sts = m_FFPrefilterVPP->RunFrameVPPAsync(in, out, NULL, &prefilterSyncPoint);
    MFX_CHECK_STS(sts);
    sts = m_FFPrefilterSession.SyncOperation(prefilterSyncPoint, ENC_TOOLS_WAIT_INTERVAL);
    MFX_RETURN(sts);
}

