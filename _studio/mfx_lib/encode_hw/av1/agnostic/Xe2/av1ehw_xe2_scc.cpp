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
#include "av1ehw_xe2_scc.h"

namespace AV1EHW
{
namespace Xe2
{
void SCC::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [this](const mfxVideoParam&, mfxVideoParam& par, StorageRW& global) -> mfxStatus
    {
        mfxU32 changed = 0;
        mfxU32 invalid = 0;

        const auto& caps = Base::Glob::EncodeCaps::Get(global);
        std::tie(changed, invalid) = CheckAndFix(par, caps);

        mfxExtAV1ScreenContentTools* pScreenContentTools = ExtBuffer::Get(par);
        if (pScreenContentTools)
        {
            mfxExtAV1TileParam* pTilePar = ExtBuffer::Get(par);
            if (pTilePar && (pTilePar->NumTileColumns > 1 || pTilePar->NumTileRows > 1))
            {
                changed += SetIf(
                    pScreenContentTools->IntraBlockCopy,
                    IsOn(pScreenContentTools->IntraBlockCopy),
                    MFX_CODINGOPTION_OFF);
            }

            auto& defchain = Base::Glob::Defaults::Get(global);
            const Base::Defaults::Param& defPar = Base::Defaults::Param(par, caps, defchain);
            mfxU32 width = 0, height = 0;
            std::tie(width, height) = Base::GetRealResolution(defPar.mvp);
            if (width > 3840)
            {
                changed += SetIf(
                    pScreenContentTools->IntraBlockCopy,
                    IsOn(pScreenContentTools->IntraBlockCopy),
                    MFX_CODINGOPTION_OFF);
            }
        }

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

        if ((par.mfx.TargetUsage == MFX_TARGETUSAGE_1) || (par.mfx.TargetUsage == MFX_TARGETUSAGE_2))
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

inline void AdjustDeltaQPForRemoteDisplay(const mfxVideoParam& par, AV1EHW::Base::FH& fh)
{
    const mfxExtAV1ScreenContentTools& screenContentTools = ExtBuffer::Get(par);
    const mfxExtCodingOption3& CO3 = ExtBuffer::Get(par);

    auto qp = fh.quantization_params.base_q_idx;

    if (CO3.ScenarioInfo == MFX_SCENARIO_DISPLAY_REMOTING
        && par.mfx.RateControlMethod == MFX_RATECONTROL_CQP
        && screenContentTools.Palette
        && fh.quantization_params.DeltaQUAc == 0 && fh.quantization_params.DeltaQVAc == 0)
    {
        // Apply Adaptive AcDeltaQp to improve palette visual quality on U/V channels
        if (qp >= 100 && qp < 164)
        {
            fh.quantization_params.DeltaQUAc = -8;
            fh.quantization_params.DeltaQVAc = -8;
        }
        else if (qp >= 164 && qp < 192)
        {
            fh.quantization_params.DeltaQUAc = -12;
            fh.quantization_params.DeltaQVAc = -12;
        }
        else if (qp >= 192)
        {
            fh.quantization_params.DeltaQUAc = -16;
            fh.quantization_params.DeltaQVAc = -16;
        }
    }
}

void SCC::PostReorderTask(const FeatureBlocks& blocks, TPushPostRT Push)
{
    AV1EHW::Base::SCC::PostReorderTask(blocks, Push);

    Push(BLK_ConfigureTask
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
        {
            auto& par  = AV1EHW::Base::Glob::VideoParam::Get(global);
            auto& fh   = AV1EHW::Base::Task::FH::Get(s_task);
            AdjustDeltaQPForRemoteDisplay(par, fh);

            return MFX_ERR_NONE;
        });
}

} //namespace Xe2
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
