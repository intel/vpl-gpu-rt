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
#include "hevcehw_base_recon422.h"

namespace HEVCEHW
{
namespace Base
{
class Recon422EXT
    : public Recon422
{
public:
    Recon422EXT(mfxU32 FeatureId)
        : Recon422(FeatureId)
    {}

protected:
    virtual void Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push) override
    {
        Push(BLK_HardcodeCaps
            , [this](const mfxVideoParam&, mfxVideoParam&, StorageRW& strg) -> mfxStatus
        {
            auto& caps = HEVCEHW::Base::Glob::EncodeCaps::Get(strg);
            caps.YUV422ReconSupport = !caps.Color420Only;
            return MFX_ERR_NONE;
        });
    }

    virtual void Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push) override
    {
        using Base::Glob;
        using Base::Defaults;

        PushCommonBlksQuery1NoCaps(blocks, Push);

        Push(BLK_SetCallChain,
            [this](const mfxVideoParam&, mfxVideoParam&, StorageRW& strg) -> mfxStatus
        {
            auto& defaults = Glob::Defaults::GetOrConstruct(strg);
            auto& bSet = defaults.SetForFeature[GetID()];
            MFX_CHECK(!bSet, MFX_ERR_NONE);

            PushDefaultsCommonCallChains(defaults);

            defaults.GetPreSetBufferSizeInKB.Push([](
                Base::Defaults::TChain<mfxU32>::TExt
                , const Defaults::Param& par)
            {
                auto& mfx = par.mvp.mfx;

                // Always use default settings for 422BRC
                if (mfx.BufferSizeInKB && !(par.base.GetTargetChromaFormat(par) - 1 == MFX_CHROMAFORMAT_YUV422))
                {
                    return mfx.BufferSizeInKB * std::max<const mfxU32>(1, mfx.BRCParamMultiplier);
                }

                return (mfxU32)0;
            });

            bSet = true;

            return MFX_ERR_NONE;
        });
    }
};
} // Base
} // namespace HEVCEHW

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
