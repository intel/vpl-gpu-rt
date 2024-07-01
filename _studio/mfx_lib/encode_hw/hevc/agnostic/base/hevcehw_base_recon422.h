// Copyright (c) 2021 Intel Corporation
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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_base.h"
#include "hevcehw_base_data.h"

namespace HEVCEHW
{
namespace Base
{
class Recon422
    : public FeatureBase
{
public:
#define DECL_BLOCK_LIST\
        DECL_BLOCK(SetCallChain)\
        DECL_BLOCK(SetRecon422Caps)\
        DECL_BLOCK(HardcodeCaps)\
        DECL_BLOCK(HardcodeCapsExt)
#define DECL_FEATURE_NAME "Base_RECON422"
#include "hevcehw_decl_blocks.h"

    Recon422(mfxU32 FeatureId)
        : FeatureBase(FeatureId)
    {}

protected:
    virtual void Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push) override
    {
        PushCommonBlksQuery1NoCaps(blocks, Push);

        Push(BLK_SetCallChain,
            [this](const mfxVideoParam&, mfxVideoParam&, StorageRW& strg) -> mfxStatus
        {
            auto& defaults = Glob::Defaults::GetOrConstruct(strg);
            auto& bSet = defaults.SetForFeature[GetID()];
            MFX_CHECK(!bSet, MFX_ERR_NONE);

            PushDefaultsCommonCallChains(defaults);

            bSet= true;

            return MFX_ERR_NONE;
        });
    }

    void PushCommonBlksQuery1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
    {
        Push(BLK_SetRecon422Caps
            , [](const mfxVideoParam&, mfxVideoParam& /*out*/, StorageRW& strg) -> mfxStatus
        {
                if (strg.Contains(Glob::RealState::Key))
                {
                    //don't insert recon422 setting in Reset
                    return MFX_ERR_NONE;
                }
                auto & ddiidSetting = Glob::DDIIDSetting::GetOrConstruct(strg);
                ddiidSetting.EnableRecon422 = true;

                return MFX_ERR_NONE;
        });
    }

    void PushDefaultsCommonCallChains(Defaults& defaults)
    {
        defaults.CheckTargetChromaFormat.Push([](
            Defaults::TCheckAndFix::TExt prev
            , const Defaults::Param& dpar
            , mfxVideoParam& par)
        {
            // Impl. should align with CheckAndFix::TargetChromaFormat within Legacy Defaults
            // except for differentiated 422 support
            mfxU32 invalid = 0;
            mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);

            MFX_CHECK(pCO3, MFX_ERR_NONE);

            static const mfxU16 cfTbl[] =
            {
                0
                , 1 + MFX_CHROMAFORMAT_YUV420
                , 1 + MFX_CHROMAFORMAT_YUV422
                , 1 + MFX_CHROMAFORMAT_YUV444
            };

            mfxU16 maxChromaPlus1 = dpar.base.GetMaxChroma(dpar) + 1;
            maxChromaPlus1 *= (maxChromaPlus1 <= mfx::size(cfTbl));
            SetDefault(maxChromaPlus1, mfxU16(MFX_CHROMAFORMAT_YUV420 + 1));

            invalid += !std::count(cfTbl, cfTbl + maxChromaPlus1, pCO3->TargetChromaFormatPlus1);
            invalid += (pCO3->TargetChromaFormatPlus1 == (1 + MFX_CHROMAFORMAT_YUV444) && !dpar.caps.YUV444ReconSupport);
            invalid += (pCO3->TargetChromaFormatPlus1 == (1 + MFX_CHROMAFORMAT_YUV422) && !dpar.caps.YUV422ReconSupport);

            pCO3->TargetChromaFormatPlus1 *= !invalid;
            MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);
            return MFX_ERR_NONE;
        });
    }

};
} // Base
} // namespace HEVCEHW

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
