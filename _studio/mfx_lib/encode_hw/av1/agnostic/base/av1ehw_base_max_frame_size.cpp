// Copyright (c) 2022 Intel Corporation
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

#include "av1ehw_base_max_frame_size.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;

void MaxFrameSize::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_CODING_OPTION2].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtCodingOption2*)pSrc;
        auto& buf_dst = *(mfxExtCodingOption2*)pDst;

        MFX_COPY_FIELD(MaxFrameSize);
    });
}

void MaxFrameSize::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [](const mfxVideoParam& /*in*/, mfxVideoParam& par, StorageW& global) -> mfxStatus
    {
        mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);
        MFX_CHECK(pCO2 && pCO2->MaxFrameSize, MFX_ERR_NONE);

        auto& caps = Glob::EncodeCaps::Get(global);
        auto& maxFrameSize = pCO2->MaxFrameSize;

        mfxU32 changed = 0;
        bool   bSupported = caps.UserMaxFrameSizeSupport
            && !Check(par.mfx.RateControlMethod, mfxU16(MFX_RATECONTROL_VBR));

        mfxU32 minSizeValid = 0;
        mfxU32 maxSizeValid = bSupported ? maxFrameSize : 0;

        if (par.mfx.FrameInfo.FrameRateExtN && par.mfx.FrameInfo.FrameRateExtD)
        {
            minSizeValid = GetAvgFrameSizeInBytes(par);
            maxSizeValid = bSupported ? std::max<mfxU32>(maxSizeValid, minSizeValid) : 0;
        }

        changed += CheckMinOrClip(maxFrameSize, minSizeValid);
        changed += CheckMaxOrZero(maxFrameSize, maxSizeValid);

        MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        return MFX_ERR_NONE;
    });
}

void MaxFrameSize::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [](mfxVideoParam& par, StorageW& /*strg*/, StorageRW&)
    {
        mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);
        mfxExtCodingOption3* pCO3 = ExtBuffer::Get(par);

        MFX_CHECK(pCO2 && pCO3 && IsOn(pCO3->LowDelayBRC), MFX_ERR_NONE);

        // Default vaule suggested by arch.
        mfxU32 avgFrameSizeInBytes = GetAvgFrameSizeInBytes(par);
        SetDefault<mfxU32>(pCO2->MaxFrameSize, mfxU32(6.0 * avgFrameSizeInBytes / 5.0));

        return MFX_ERR_NONE;
    });
}

void MaxFrameSize::Reset(const FeatureBlocks& /*blocks*/, TPushR Push)
{
    Push(BLK_Reset
        , [](
            const mfxVideoParam& /*par*/
            , StorageRW& global
            , StorageRW& /*local*/) -> mfxStatus
    {
        auto& parOld = Glob::VideoParam::Get(Glob::RealState::Get(global));
        auto& parNew = Glob::VideoParam::Get(global);
        mfxExtCodingOption2& CO2Old = ExtBuffer::Get(parOld);
        mfxExtCodingOption2& CO2New = ExtBuffer::Get(parNew);
        auto& hint = Glob::ResetHint::Get(global);

        const mfxExtCodingOption3& CO3 = ExtBuffer::Get(parNew);
        hint.Flags |= RF_BRC_RESET *
            ((     parOld.mfx.RateControlMethod == MFX_RATECONTROL_CBR
                || parOld.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
            && !IsOn(CO3.LowDelayBRC)
            && (CO2Old.MaxFrameSize != CO2New.MaxFrameSize));

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
