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

#include "hevcehw_base_ext_brc.h"
#include "mfx_brc_common.h"

#ifdef MFX_ENABLE_ENCTOOLS
#include "mfxenctools-int.h"
#include "hevcehw_base_enctools.h"
#endif

using namespace HEVCEHW;
using namespace HEVCEHW::Base;

void ExtBRC::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_BRC].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        auto& src = *(const mfxExtBRC*)pSrc;
        auto& dst = *(mfxExtBRC*)pDst;

        dst.pthis           = src.pthis;
        dst.Init            = src.Init;
        dst.Reset           = src.Reset;
        dst.Close           = src.Close;
        dst.GetFrameCtrl    = src.GetFrameCtrl;
        dst.Update          = src.Update;
    });
}

void ExtBRC::SetInherited(ParamInheritance& par)
{
    par.m_ebInheritDefault[MFX_EXTBUFF_CODING_OPTION2].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        auto& src = *(const mfxExtCodingOption2*)pSrc;
        auto& dst = *(mfxExtCodingOption2*)pDst;

        InheritOption(src.ExtBRC, dst.ExtBRC);
    });

    par.m_ebInheritDefault[MFX_EXTBUFF_BRC].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        auto& src = *(const mfxExtBRC*)pSrc;
        auto& dst = *(mfxExtBRC*)pDst;

        if (   !dst.pthis
            && !dst.Init
            && !dst.Reset
            && !dst.Close
            && !dst.GetFrameCtrl
            && !dst.Update)
        {
            dst.pthis           = src.pthis;
            dst.Init            = src.Init;
            dst.Reset           = src.Reset;
            dst.Close           = src.Close;
            dst.GetFrameCtrl    = src.GetFrameCtrl;
            dst.Update          = src.Update;
        }
    });
}


void ExtBRC::Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push)
{
    Push(BLK_Check,
        [&blocks](const mfxVideoParam&, mfxVideoParam& par, StorageW&) -> mfxStatus
    {
        mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);
        mfxExtBRC* pBRC = ExtBuffer::Get(par);
        mfxU32 changed = 0;

        MFX_CHECK(pCO2, MFX_ERR_NONE);

        bool bAllowed =
            par.mfx.RateControlMethod == MFX_RATECONTROL_CBR
            || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR;

        bool bInvalid =
            IsOn(pCO2->ExtBRC)
            && pBRC
            && (   pBRC->pthis
                || pBRC->Init
                || pBRC->Close
                || pBRC->GetFrameCtrl
                || pBRC->Update
                || pBRC->Reset)
            && (   !pBRC->pthis
                || !pBRC->Init
                || !pBRC->Close
                || !pBRC->GetFrameCtrl
                || !pBRC->Update
                || !pBRC->Reset);

        changed += CheckOrZero<mfxU16>(pCO2->ExtBRC
            , mfxU16(MFX_CODINGOPTION_UNKNOWN)
            , mfxU16(MFX_CODINGOPTION_OFF)
            , mfxU16(MFX_CODINGOPTION_ON * (bAllowed && !bInvalid)));

        bool bZeroPtrs =
            !IsOn(pCO2->ExtBRC)
            && pBRC
            && (   pBRC->pthis
                || pBRC->Init
                || pBRC->Close
                || pBRC->GetFrameCtrl
                || pBRC->Update
                || pBRC->Reset);

        if (bZeroPtrs)
        {
            pBRC->pthis         = nullptr;
            pBRC->Init          = nullptr;
            pBRC->Close         = nullptr;
            pBRC->GetFrameCtrl  = nullptr;
            pBRC->Update        = nullptr;
            pBRC->Reset         = nullptr;
            ++changed;
        }

        bool bInitMode = !FeatureBlocks::BQ<FeatureBlocks::BQ_InitAlloc>::Get(blocks).empty();

        MFX_CHECK(!(bInvalid && bInitMode), MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        return MFX_ERR_NONE;
    });
}

void ExtBRC::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [](mfxVideoParam& par, StorageW&, StorageRW&)
    {
        mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);

        if (pCO2 && pCO2->ExtBRC == MFX_CODINGOPTION_UNKNOWN)
            pCO2->ExtBRC = MFX_CODINGOPTION_OFF;
    });
}

void ExtBRC::Reset(const FeatureBlocks& /*blocks*/, TPushR Push)
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

        const mfxExtCodingOption2(&CO2)[2] = { ExtBuffer::Get(parOld), ExtBuffer::Get(parNew) };

        MFX_CHECK(CO2[0].ExtBRC == CO2[1].ExtBRC, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        if (IsOn(CO2[0].ExtBRC))
        {
            const mfxExtBRC(&BRC)[2] = { ExtBuffer::Get(parOld), ExtBuffer::Get(parNew) };

            MFX_CHECK(
                   BRC[0].pthis         == BRC[1].pthis
                && BRC[0].Init          == BRC[1].Init
                && BRC[0].Reset         == BRC[1].Reset
                && BRC[0].Close         == BRC[1].Close
                && BRC[0].GetFrameCtrl  == BRC[1].GetFrameCtrl
                && BRC[0].Update        == BRC[1].Update
                , MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        }

        return MFX_ERR_NONE;
    });
}

void ExtBRC::ResetState(const FeatureBlocks& /*blocks*/, TPushRS Push)
{
    Push(BLK_Reset
        , [this](
            StorageRW& global
            , StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);

        if (m_brc.Reset)
        {
            if (Glob::ResetHint::Get(global).Flags & RF_IDR_REQUIRED)
            {
                mfxExtEncoderResetOption& rOpt = ExtBuffer::Get(par);
                rOpt.StartNewSequence = MFX_CODINGOPTION_ON;
            }

            mfxStatus sts = m_brc.Reset(m_brc.pthis, &par);
            MFX_CHECK_STS(sts);
        }

        return MFX_ERR_NONE;
    });
}

void ExtBRC::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_Init
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto&                      par          = Glob::VideoParam::Get(strg);
        const mfxExtCodingOption2& CO2          = ExtBuffer::Get(par);
        const mfxExtCodingOption3& CO3          = ExtBuffer::Get(par);
        mfxExtBRC&                 brc          = ExtBuffer::Get(par);
        bool                       bInternalBRC = IsOn(CO2.ExtBRC) && !brc.pthis && !m_brc.pthis;
        bool                       bExternalBRC = IsOn(CO2.ExtBRC) && brc.pthis && !m_brc.pthis;

#ifdef MFX_ENABLE_ENCTOOLS
        mfxExtEncToolsConfig* pConfig = ExtBuffer::Get(par);
        bool bEncTools = (pConfig) ?
            IsEncToolsOptOn(*pConfig, CO3.ScenarioInfo == MFX_SCENARIO_GAME_STREAMING) :
            false;
        if (bEncTools)
            bInternalBRC = bExternalBRC = false;
#endif
        if (bInternalBRC)
        {
            auto sts = HEVCExtBRC::Create(m_brc);
            MFX_CHECK_STS(sts);

            m_destroy = [this]()
            {
                HEVCExtBRC::Destroy(m_brc);
            };
        }

        SetIf(m_brc, bExternalBRC, brc);

        if (m_brc.Init)
        {
            mfxStatus sts = m_brc.Init(m_brc.pthis, &par);
            MFX_CHECK_STS(sts);
        }

        m_bUseLevel = !(IGNORE_P_PYRAMID_LEVEL && CO3.PRefType == MFX_P_REF_PYRAMID);

        return MFX_ERR_NONE;
    });
}

mfxBRCFrameParam ExtBRC::MakeFrameParam(const TaskCommonPar& task)
{
    mfxBRCFrameParam    fp = {};

    fp.DisplayOrder   = task.DisplayOrder;
    fp.EncodedOrder   = task.EncodedOrder;
    fp.FrameType      = task.FrameType;
    fp.PyramidLayer   = mfxU16(task.PyramidLevel * m_bUseLevel + task.b2ndField);
    fp.NumRecode      = task.NumRecode;
    fp.CodedFrameSize = task.BsDataLength;

    return fp;
}

void ExtBRC::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_GetFrameCtrl
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        MFX_CHECK(m_brc.GetFrameCtrl, MFX_ERR_NONE);

        auto&      par  = Glob::VideoParam::Get(global);
        auto&      task = Task::Common::Get(s_task);
        auto&      sh   = Task::SSH::Get(s_task);
        auto&      sps  = Glob::SPS::Get(global);
        auto&      pps  = Glob::PPS::Get(global);

        bool bNegativeQpAllowed = !IsOn(par.mfx.LowPower);

        mfxI32 minQP = (-6 * sps.bit_depth_luma_minus8) * bNegativeQpAllowed;
        mfxI32 maxQP = 51;

        mfxBRCFrameParam fp = MakeFrameParam(task);
        mfxBRCFrameCtrl  fc = {};

        mfxStatus sts = m_brc.GetFrameCtrl(m_brc.pthis, &fp, &fc);
        MFX_CHECK_STS(sts);

        SetDefault(fc.InitialCpbRemovalDelay, task.initial_cpb_removal_delay);
        SetDefault(fc.InitialCpbRemovalOffset, task.initial_cpb_removal_offset);

        task.initial_cpb_removal_delay  = fc.InitialCpbRemovalDelay;
        task.initial_cpb_removal_offset = fc.InitialCpbRemovalOffset;

        task.QpY = mfxI8(mfx::clamp(fc.QpY, minQP, maxQP));
        sh.slice_qp_delta = mfxI8(task.QpY - (pps.init_qp_minus26 + 26));

        sh.temporal_mvp_enabled_flag &= !(par.AsyncDepth > 1 && task.NumRecode); // WA

        return MFX_ERR_NONE;
    });
}

void ExtBRC::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_Update
        , [this](
            StorageW& /*global*/
            , StorageW& s_task) -> mfxStatus
    {

        MFX_CHECK(m_brc.Update, MFX_ERR_NONE);

        auto& task = Task::Common::Get(s_task);

        mfxBRCFrameParam    fp = MakeFrameParam(task);
        mfxBRCFrameCtrl     fc = {};
        mfxBRCFrameStatus   fs = {};

        fc.QpY = task.QpY;

        mfxStatus sts = m_brc.Update(m_brc.pthis, &fp, &fc, &fs);
        MFX_CHECK_STS(sts);
        task.bSkip = false;

        switch (fs.BRCStatus)
        {
        case MFX_BRC_OK:
            break;
        case MFX_BRC_PANIC_SMALL_FRAME:
            task.MinFrameSize = (mfxU32)((fs.MinFrameSize + 7) >> 3);
            fp.NumRecode++;
            fp.CodedFrameSize = (mfxU32)((fs.MinFrameSize + 7) >> 3);

            sts = m_brc.Update(m_brc.pthis, &fp, &fc, &fs);
            MFX_CHECK_STS(sts);
            MFX_CHECK(fs.BRCStatus == MFX_BRC_OK, MFX_ERR_UNDEFINED_BEHAVIOR);

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

        return MFX_ERR_NONE;
    });
}

void ExtBRC::Close(const FeatureBlocks& /*blocks*/, TPushCLS Push)
{
    Push(BLK_Close
        , [this](StorageW& /*global*/)
    {
        if (m_brc.Close)
            m_brc.Close(m_brc.pthis);
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
