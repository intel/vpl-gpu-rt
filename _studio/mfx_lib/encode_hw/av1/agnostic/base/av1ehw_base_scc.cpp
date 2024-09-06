// Copyright (c) 2021-2023 Intel Corporation
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

#include "av1ehw_base_data.h"
#include "av1ehw_base_scc.h"

namespace AV1EHW
{
namespace Base
{

void SCC::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_AV1_SCREEN_CONTENT_TOOLS].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtAV1ScreenContentTools*)pSrc;
        auto& buf_dst = *(mfxExtAV1ScreenContentTools*)pDst;
        MFX_COPY_FIELD(IntraBlockCopy);
        MFX_COPY_FIELD(Palette);
    });
}

void SCC::SetInherited(ParamInheritance& par)
{
    par.m_ebInheritDefault[MFX_EXTBUFF_AV1_SCREEN_CONTENT_TOOLS].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        const auto& src = *(const mfxExtAV1ScreenContentTools*)pSrc;
        auto& dst = *(mfxExtAV1ScreenContentTools*)pDst;

        InheritOption(src.IntraBlockCopy, dst.IntraBlockCopy);
        InheritOption(src.Palette, dst.Palette);
    });
}

std::pair<mfxU32, mfxU32> SCC::CheckAndFix(mfxVideoParam& par, const EncodeCapsAv1& caps)
{
    mfxU32 changed = 0;
    mfxU32 invalid = 0;
    mfxExtAV1ScreenContentTools* pScreenContentTools = ExtBuffer::Get(par);
    if (pScreenContentTools)
    {
        changed += SetIf(pScreenContentTools->Palette, (pScreenContentTools->Palette != MFX_CODINGOPTION_UNKNOWN
            && pScreenContentTools->Palette != MFX_CODINGOPTION_ON
            && pScreenContentTools->Palette != MFX_CODINGOPTION_OFF), MFX_CODINGOPTION_UNKNOWN);

        changed += SetIf(pScreenContentTools->IntraBlockCopy, (pScreenContentTools->IntraBlockCopy != MFX_CODINGOPTION_UNKNOWN
            && pScreenContentTools->IntraBlockCopy != MFX_CODINGOPTION_ON
            && pScreenContentTools->IntraBlockCopy != MFX_CODINGOPTION_OFF), MFX_CODINGOPTION_UNKNOWN);

        if (IsOn(pScreenContentTools->Palette))
        {
            invalid += SetIf(pScreenContentTools->Palette, !caps.AV1ToolSupportFlags.fields.PaletteMode, 0);
        }

        if (IsOn(pScreenContentTools->IntraBlockCopy))
        {
            invalid += SetIf(pScreenContentTools->IntraBlockCopy, !caps.AV1ToolSupportFlags.fields.allow_intrabc, 0);
            mfxExtAV1AuxData* pAux = ExtBuffer::Get(par);
            if (pAux)
            {
                changed += SetIf(
                    pAux->EnableSuperres,
                    IsOn(pAux->EnableSuperres) && IsOn(pScreenContentTools->IntraBlockCopy),
                    MFX_CODINGOPTION_OFF);
            }
        }

    }
    return std::make_pair(changed, invalid);
}

void SCC::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [this](const mfxVideoParam&, mfxVideoParam& par, StorageRW& global) -> mfxStatus
    {
        mfxU32 changed = 0;
        mfxU32 invalid = 0;

        const auto& caps = Base::Glob::EncodeCaps::Get(global);
        std::tie(changed, invalid) = CheckAndFix(par, caps);

        MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);
        MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        return MFX_ERR_NONE;
    });
}

void SCC::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [](mfxVideoParam& par, StorageW& /*strg*/, StorageRW&)
    {
        mfxExtAV1ScreenContentTools* pScreenContentTools = ExtBuffer::Get(par);
        if (!pScreenContentTools)
            return;

        mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);

        if (pCO3 && pCO3->ScenarioInfo == MFX_SCENARIO_DISPLAY_REMOTING)
        {
            SetDefault(pScreenContentTools->Palette, MFX_CODINGOPTION_ON);
            SetDefault(pScreenContentTools->IntraBlockCopy, MFX_CODINGOPTION_ON);
        }

        if ((par.mfx.TargetUsage != MFX_TARGETUSAGE_6) && (par.mfx.TargetUsage != MFX_TARGETUSAGE_7))
            SetDefault(pScreenContentTools->Palette, MFX_CODINGOPTION_ON);
        else
            SetDefault(pScreenContentTools->Palette, MFX_CODINGOPTION_OFF);

        SetDefault(pScreenContentTools->IntraBlockCopy, MFX_CODINGOPTION_OFF);

        mfxExtAV1AuxData* pAux = ExtBuffer::Get(par);
        if (pAux && IsOn(pScreenContentTools->IntraBlockCopy))
        {
            SetDefault(pAux->EnableSuperres, MFX_CODINGOPTION_OFF);
            SetDefault(pAux->EnableRestoration, MFX_CODINGOPTION_OFF);
        }
    });
}

void SCC::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetFH
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(strg);
        mfxExtAV1ScreenContentTools* pScreenContentTools = ExtBuffer::Get(par);
        MFX_CHECK(pScreenContentTools, MFX_ERR_NONE);

        MFX_CHECK(strg.Contains(Glob::FH::Key), MFX_ERR_NOT_FOUND);
        auto& fh = Glob::FH::Get(strg);

        fh.allow_screen_content_tools = CO2Flag(pScreenContentTools->Palette);
        return MFX_ERR_NONE;
    });
}

void SCC::PostReorderTask(const FeatureBlocks& /*blocks*/, TPushPostRT Push)
{
    Push(BLK_ConfigureTask
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        FH& fh = Task::FH::Get(s_task);
        auto& par = Glob::VideoParam::Get(global);
        const mfxExtAV1ScreenContentTools& pScreenContentTools = ExtBuffer::Get(par);

        fh.allow_intrabc = FrameIsIntra(fh) && CO2Flag(pScreenContentTools.IntraBlockCopy);

        if (fh.allow_intrabc)
        {
            DisableLoopFilter(fh);
            DisableCDEF(fh);
            DisableLoopRestoration(fh);

            fh.allow_screen_content_tools = 1;
        }
        return MFX_ERR_NONE;
    });
}

} //namespace Base
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)