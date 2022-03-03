// Copyright (c) 2019-2020 Intel Corporation
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

#include "hevcehw_base_extddi.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;

void ExtDDI::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_DDI].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        auto& buf_src = *(const mfxExtCodingOptionDDI*)pSrc;
        auto& buf_dst = *(mfxExtCodingOptionDDI*)pDst;

        MFX_COPY_FIELD(LongStartCodes);
        MFX_COPY_FIELD(NumActiveRefP);
        MFX_COPY_FIELD(NumActiveRefBL0);
        MFX_COPY_FIELD(NumActiveRefBL1);
        MFX_COPY_FIELD(QpAdjust);
        MFX_COPY_FIELD(TMVP);
    });
}

void ExtDDI::SetInherited(ParamInheritance& par)
{
    par.m_ebInheritDefault[MFX_EXTBUFF_DDI].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        auto& src = *(const mfxExtCodingOptionDDI*)pSrc;
        auto& dst = *(mfxExtCodingOptionDDI*)pDst;

        InheritOption(src.LongStartCodes,   dst.LongStartCodes);
        InheritOption(src.NumActiveRefP,    dst.NumActiveRefP);
        InheritOption(src.NumActiveRefBL0,  dst.NumActiveRefBL0);
        InheritOption(src.NumActiveRefBL1,  dst.NumActiveRefBL1);
        InheritOption(src.QpAdjust,         dst.QpAdjust);
        InheritOption(src.TMVP,             dst.TMVP);
    });
}

void ExtDDI::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_SetCallChains,
        [this](const mfxVideoParam&, mfxVideoParam&, StorageRW& strg) -> mfxStatus
    {
        auto& defaults = Glob::Defaults::GetOrConstruct(strg);
        auto& bSet = defaults.SetForFeature[GetID()];
        MFX_CHECK(!bSet, MFX_ERR_NONE);

        defaults.GetSPS.Push([](
            Defaults::TGetSPS::TExt prev
            , const Defaults::Param& defPar
            , const VPS& vps
            , Base::SPS& sps)
        {
            const mfxExtCodingOptionDDI* pCODDI = ExtBuffer::Get(defPar.mvp);

            auto sts = prev(defPar, vps, sps);
            MFX_CHECK(pCODDI && pCODDI->TMVP, sts);

            sps.temporal_mvp_enabled_flag = !IsOff(pCODDI->TMVP);

            return sts;
        });

        bSet = true;

        return MFX_ERR_NONE;
    });
}

void ExtDDI::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [this](const mfxVideoParam& /*in*/, mfxVideoParam& par, StorageW& global) -> mfxStatus
    {
        mfxExtCodingOptionDDI* pCODDI = ExtBuffer::Get(par);
        MFX_CHECK(pCODDI, MFX_ERR_NONE);

        auto& coddi = *pCODDI;
        bool changed = false;
        auto& caps = Glob::EncodeCaps::Get(global);

        changed |= CheckTriStateOrZero(coddi.LongStartCodes);

        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            changed |= CheckTriStateOrZero(coddi.QpAdjust);
        else
            changed |= CheckOrZero<mfxU16, 0, MFX_CODINGOPTION_OFF>(coddi.QpAdjust);

        changed |= CheckMaxOrClip(coddi.NumActiveRefP, caps.MaxNum_Reference0);
        changed |= CheckMaxOrClip(coddi.NumActiveRefBL0, caps.MaxNum_Reference0);
        changed |= CheckMaxOrClip(coddi.NumActiveRefBL1, caps.MaxNum_Reference1);

        if (par.mfx.NumRefFrame)
        {
            changed |= CheckMaxOrClip(coddi.NumActiveRefP, par.mfx.NumRefFrame);
            changed |= CheckMaxOrClip(coddi.NumActiveRefBL0, par.mfx.NumRefFrame);
            changed |= CheckMaxOrClip(coddi.NumActiveRefBL1, par.mfx.NumRefFrame);
        }

        return changed ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
    });
}

void ExtDDI::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [this](mfxVideoParam& par, StorageW& /*strg*/, StorageRW&)
    {
        mfxExtCodingOptionDDI* pCODDI = ExtBuffer::Get(par);
        if(!pCODDI)
            return;

        auto& coddi = *pCODDI;
        mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);

        SetDefault<mfxU16>(coddi.LongStartCodes, MFX_CODINGOPTION_OFF);

        if (pCO3)
        {
            if (coddi.NumActiveRefP)
                std::fill_n(pCO3->NumRefActiveP, mfx::size(pCO3->NumRefActiveP), coddi.NumActiveRefP);
            SetDefault(coddi.NumActiveRefP, pCO3->NumRefActiveP[0]);

            if (coddi.NumActiveRefBL0)
                std::fill_n(pCO3->NumRefActiveBL0, mfx::size(pCO3->NumRefActiveBL0), coddi.NumActiveRefBL0);
            SetDefault(coddi.NumActiveRefBL0, pCO3->NumRefActiveBL0[0]);

            if (coddi.NumActiveRefBL1)
                std::fill_n(pCO3->NumRefActiveBL1, mfx::size(pCO3->NumRefActiveBL1), coddi.NumActiveRefBL1);
            SetDefault(coddi.NumActiveRefBL1, pCO3->NumRefActiveBL1[0]);
        }

        SetDefault<mfxU16>(coddi.QpAdjust, (mfxU16)MFX_CODINGOPTION_OFF);
    });
}

void ExtDDI::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_Init
        , [this](StorageRW& /*strg*/, StorageRW&) -> mfxStatus
    {
        m_bPatchNextDDITask = true;

        return MFX_ERR_NONE;
    });
}

void ExtDDI::PostReorderTask(const FeatureBlocks& /*blocks*/, TPushPostRT Push)
{
    Push(BLK_ConfigureTask
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        const mfxExtCodingOptionDDI& coddi = ExtBuffer::Get(Glob::VideoParam::Get(global));
        auto& task = Task::Common::Get(s_task);

        task.bForceLongStartCode = IsOn(coddi.LongStartCodes);

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)