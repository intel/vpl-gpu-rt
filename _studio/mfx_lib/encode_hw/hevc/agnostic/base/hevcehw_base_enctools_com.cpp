// Copyright (c) 2023 Intel Corporation
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
#if defined(MFX_ENABLE_ENCTOOLS_BASE)

#include "hevcehw_base.h"
#include "hevcehw_base_data.h"
#include "hevcehw_base_enctools_com.h"
#include "hevcehw_base_task.h"
#include "hevcehw_base_legacy.h"
#include <climits>

using namespace HEVCEHW;
using namespace HEVCEHW::Base;


bool HevcEncToolsCommon::isEncTools(const mfxVideoParam& par)
{
    const mfxExtEncToolsConfig* pConfig = ExtBuffer::Get(par);
    bool bEncTools = false;
    if (pConfig)
    {
        mfxExtEncToolsConfig config = {};
        SetDefaultConfig(par, config, true);
        bEncTools = IsEncToolsConfigOn(config, IsGameStreaming(par));
    }
    else
        bEncTools = IsEncToolsImplicit(par);

    return bEncTools;
}

mfxStatus HevcEncToolsCommon::InitEncToolsCtrl(
    mfxVideoParam const& par
    , mfxEncToolsCtrl* ctrl)
{
    MFX_CHECK_NULL_PTR1(ctrl);

    const mfxExtCodingOption* pCO = ExtBuffer::Get(par);
    const mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);
    const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);
    const mfxExtCodingOptionDDI* pCODDI = ExtBuffer::Get(par);

    ctrl->CodecId = par.mfx.CodecId;
    ctrl->CodecProfile = par.mfx.CodecProfile;
    ctrl->CodecLevel = par.mfx.CodecLevel;
    ctrl->LowPower = par.mfx.LowPower;
    ctrl->AsyncDepth = par.AsyncDepth;

    ctrl->FrameInfo = par.mfx.FrameInfo;
    ctrl->IOPattern = par.IOPattern;
    ctrl->MaxDelayInFrames = pCO2 ? pCO2->LookAheadDepth : 0;

    ctrl->NumRefP = pCODDI ? std::min(par.mfx.NumRefFrame, pCODDI->NumActiveRefP) : par.mfx.NumRefFrame;
    ctrl->MaxGopSize = par.mfx.GopPicSize;
    ctrl->MaxGopRefDist = par.mfx.GopRefDist;
    ctrl->MaxIDRDist = par.mfx.GopPicSize * par.mfx.IdrInterval;
    // HEVC Defaults to CRA for IdrInterval == 0
    if (par.mfx.IdrInterval == 0 && par.mfx.GopPicSize != 0) {
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

        ctrl->BufferSizeInKB = par.mfx.BufferSizeInKB * mult;          //Bitstream size
        if (ctrl->HRDConformance)
        {
            ctrl->InitialDelayInKB = par.mfx.InitialDelayInKB * mult;    //if HRDConformance is ON
        }
        else
        {
            ctrl->ConvergencePeriod = 0;     //if HRDConformance is OFF, 0 - the period is whole stream,
            ctrl->Accuracy = 10;              //if HRDConformance is OFF
        }

        mfxU32 maxFrameSize = pCO2 ? pCO2->MaxFrameSize : 0;
        ctrl->WinBRCMaxAvgKbps = pCO3 ? pCO3->WinBRCMaxAvgKbps * mult : 0;
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

    return MFX_ERR_NONE;
}

mfxStatus HevcEncToolsCommon::SubmitPreEncTask(StorageW&  /*global*/, StorageW& s_task)
{
    MFX_CHECK(m_pEncTools && m_pEncTools->Submit, MFX_ERR_NONE);

    mfxEncToolsTaskParam task_par = {};
    auto& task = Task::Common::Get(s_task);
    std::vector<mfxExtBuffer*> extParams;
    mfxEncToolsFrameToAnalyze extFrameData = {};

    //"m_numPicBuffered > 0" indicates: there are some PreEnc frames buffered for async_depth > 1 case, while not queried
    if (task.pSurfIn || m_numPicBuffered > 0)
    {
        extFrameData.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_FRAME_TO_ANALYZE;
        extFrameData.Header.BufferSz = sizeof(extFrameData);
        extFrameData.Surface = task.pSurfIn;
        extParams.push_back(&extFrameData.Header);
        task_par.ExtParam = extParams.data();

        if (!task.pSurfIn)
        {
            m_numPicBuffered--;
        }
    }
    task_par.DisplayOrder = task.DisplayOrder;
    task_par.NumExtParam = (mfxU16)extParams.size();

    auto sts = m_pEncTools->Submit(m_pEncTools->Context, &task_par);
    if (sts == MFX_ERR_MORE_DATA)
    {
        m_numPicBuffered++; // current PreEnc frame buffered, while not queried
        sts = MFX_ERR_NONE;
    }

    return (sts);
}

void HevcEncToolsCommon::Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push)
{
    Push(BLK_Check,
        [this, &blocks](const mfxVideoParam&, mfxVideoParam& par, StorageW&) -> mfxStatus
        {
            MFX_CHECK(isFeatureEnabled(par), MFX_ERR_NONE);

            bool bEncTools = isEncTools(par);
            mfxU32 changed = 0;
            MFX_CHECK(bEncTools, MFX_ERR_NONE);
            mfxEncTools* pEncTools = GetEncTools(par);
            bool bCreated = false;
            if (!pEncTools)
            {
                pEncTools = MFXVideoENCODE_CreateEncTools(par);
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

void HevcEncToolsCommon::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [this](mfxVideoParam& par, StorageW& global, StorageRW&)
    {
        if (!isFeatureEnabled(par))
            return;

        mfxExtEncToolsConfig *pConfig = ExtBuffer::Get(par);
        auto& caps = Glob::EncodeCaps::Get(global);
        if (pConfig)
            SetDefaultConfig(par, *pConfig, caps.MBBRCSupport);
    });
}

void HevcEncToolsCommon::ResetState(const FeatureBlocks& /*blocks*/, TPushRS Push)
{
    Push(BLK_Reset
        , [this](
            StorageRW& global
            , StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(isFeatureEnabled(par), MFX_ERR_NONE);

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
            auto sts = InitEncToolsCtrl(par, &m_EncToolCtrl);
            MFX_CHECK_STS(sts);

            sts = m_pEncTools->Reset(m_pEncTools->Context, &m_EncToolConfig, &m_EncToolCtrl);
            MFX_CHECK_STS(sts);
        }

        return MFX_ERR_NONE;
    });
}

void HevcEncToolsCommon::QueryIOSurf(const FeatureBlocks&, TPushQIS Push)
{
    Push(BLK_QueryIOSurf
        , [this](const mfxVideoParam& parInput, mfxFrameAllocRequest& req, StorageRW& strg) -> mfxStatus
    {
            ExtBuffer::Param<mfxVideoParam> par = parInput;
            auto& caps = Glob::EncodeCaps::Get(strg);

            MFX_CHECK(isFeatureEnabled(par), MFX_ERR_NONE);

            bool bEncTools = isEncTools(par);
            mfxU32 changed = 0;
            MFX_CHECK(bEncTools, MFX_ERR_NONE);

            mfxEncTools* pEncTools = GetEncTools(par);
            bool bCreated = false;

            if (!pEncTools)
            {
                pEncTools = MFXVideoENCODE_CreateEncTools(par);
                bCreated = !!pEncTools;
            }
            MFX_CHECK_NULL_PTR1(pEncTools);

            mfxEncToolsCtrl ctrl = {};
            mfxExtEncToolsConfig supportedConfig = {};

            mfxStatus sts = InitEncToolsCtrl(par, &ctrl);
            MFX_CHECK_STS(sts);

            pEncTools->GetSupportedConfig(pEncTools->Context, &supportedConfig, &ctrl);

            changed += CorrectVideoParams(par, supportedConfig);

            mfxExtEncToolsConfig config = {};
            SetDefaultConfig(par, config, caps.MbQpDataSupport);           
 
            mfxU32 maxDelay = 0;
            pEncTools->GetDelayInFrames(pEncTools->Context, &config, &ctrl, &maxDelay);
            
            mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);
            if (pCO2)
                maxDelay = (mfxU32)std::max<mfxI32>(0, maxDelay - pCO2->LookAheadDepth); //LA is used in base_legacy
            req.NumFrameMin += (mfxU16)maxDelay;
            req.NumFrameSuggested += (mfxU16)maxDelay;

            if (bCreated)
                MFXVideoENCODE_DestroyEncTools(pEncTools);

            MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            return MFX_ERR_NONE;
    });
}

void HevcEncToolsCommon::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
     Push(BLK_Init
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(strg);
        auto& caps = Glob::EncodeCaps::Get(strg);

        MFX_CHECK(isFeatureEnabled(par), MFX_ERR_NONE);

        mfxExtEncToolsConfig* pConfig = ExtBuffer::Get(par);


        bool bEncTools = (pConfig) ?
            IsEncToolsConfigOn(*pConfig, IsGameStreaming(par)) :
            false;
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
        m_hwType = Glob::VideoCore::Get(strg).GetHWType();
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
        };

        if (m_pEncTools)
        {
            mfxExtEncToolsConfig supportedConfig = {};

            m_pEncTools->GetSupportedConfig(m_pEncTools->Context, &supportedConfig, &m_EncToolCtrl);


            if (CorrectVideoParams(par, supportedConfig))
                MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            SetDefaultConfig(par, m_EncToolConfig,caps.MbQpDataSupport);

            sts = m_pEncTools->Init(m_pEncTools->Context, &m_EncToolConfig, &m_EncToolCtrl);
            MFX_CHECK_STS(sts);

            sts = m_pEncTools->GetActiveConfig(m_pEncTools->Context, &m_EncToolConfig);
            MFX_CHECK_STS(sts);

            m_pEncTools->GetDelayInFrames(m_pEncTools->Context, &m_EncToolConfig, &m_EncToolCtrl, &m_maxDelay);

            auto& tm = Glob::TaskManager::Get(strg).m_tm;

            S_ET_SUBMIT = tm.AddStage(tm.S_NEW);
            S_ET_QUERY = tm.AddStage(S_ET_SUBMIT);
        }

        return MFX_ERR_NONE;
    });
    // Add S_ET_SUBMIT and S_ET_QUERY stages for EncTools
    Push(BLK_AddTask
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(isFeatureEnabled(par), MFX_ERR_NONE);

        MFX_CHECK(S_ET_SUBMIT != mfxU16(-1) && S_ET_QUERY != mfxU16(-1), MFX_ERR_NONE);
        auto& tm = Glob::TaskManager::Get(global).m_tm;

        auto  ETSubmit = [&](
            MfxEncodeHW::TaskManager::TAsyncStage::TExt
            , StorageW& global
            , StorageW& /*s_task*/) -> mfxStatus
        {
            if(tm.m_nRecodeTasks)
            {
                return MFX_ERR_NONE;
            }

            if (StorageW* pTask = tm.GetTask(tm.Stage(S_ET_SUBMIT)))
            {
                MFX_CHECK_STS(SubmitPreEncTask(global, *pTask));
                tm.MoveTaskForward(tm.Stage(S_ET_SUBMIT), tm.FixedTask(*pTask));
            }

            return MFX_ERR_NONE;
        };

        auto  ETQuery = [&](
            MfxEncodeHW::TaskManager::TAsyncStage::TExt
            , StorageW&  global
            , StorageW&  s_task) -> mfxStatus
        {
            if(tm.m_nRecodeTasks)
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
}

void HevcEncToolsCommon::FreeTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_Discard
        , [this](StorageW& global, StorageW& s_task)->mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(isFeatureEnabled(par), MFX_ERR_NONE);

        MFX_CHECK(m_pEncTools && m_pEncTools->Discard, MFX_ERR_NONE);

        auto& task = Task::Common::Get(s_task);

        return m_pEncTools->Discard(m_pEncTools->Context, task.DisplayOrder);
    });
}

void HevcEncToolsCommon::Close(const FeatureBlocks& /*blocks*/, TPushCLS Push)
{
    Push(BLK_Close
        , [this](StorageW& global)
    {
        auto& par = Glob::VideoParam::Get(global);
        if (!isFeatureEnabled(par))
            return;

        if (m_pEncTools && m_pEncTools->Close)
            m_pEncTools->Close(m_pEncTools->Context);
    });
}

#endif //defined(MFX_ENABLE_ENCTOOLS_BASE)
#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
