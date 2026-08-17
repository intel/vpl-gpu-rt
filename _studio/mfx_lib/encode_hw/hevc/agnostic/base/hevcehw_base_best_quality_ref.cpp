// Copyright (c) 2026 Intel Corporation
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

#include "hevcehw_base_best_quality_ref.h"
#include "hevcehw_base_legacy.h"
#include "mfx_platform_caps.h"
#include <climits>

using namespace HEVCEHW;
using namespace HEVCEHW::Base;

// Low delay primary reference pattern (4-frame cycle) [cycleIdx][3 refs as POC deltas]
static const mfxI32 kLDPrimaryPattern4[4][3] =
{
    { -1,  -4,  -8 },
    { -1, -13, -25 },
    { -1, -10, -22 },
    { -1, -11, -23 },
};

// Low delay fallback reference pattern (4-frame cycle) [cycleIdx][3 refs as POC deltas]
static const mfxI32 kLDFallbackPattern4[4][3] =
{
    { -1, -4, -8 },
    { -1, -5, -9 },
    { -1, -2, -6 },
    { -1, -3, -7 },
};

void BestQualityRef::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_SetDefaultsCallChain,
        [this](const mfxVideoParam& in, mfxVideoParam& /*out*/, StorageRW& strg) -> mfxStatus
    {
        auto& defaults = Glob::Defaults::GetOrConstruct(strg);
        auto& bSet = defaults.SetForFeature[GetID()];
        MFX_CHECK(!bSet, MFX_ERR_NONE);

        auto hw = Glob::VideoCore::Get(strg).GetHWType();
        MFX_CHECK(HEVCECaps::IsBestQualityRefSupported(hw), MFX_ERR_NONE);

        // Disable feature if external SPSPPS is provided
        const mfxExtCodingOptionSPSPPS* pSPSPPS = ExtBuffer::Get(in);
        MFX_CHECK(!pSPSPPS, MFX_ERR_NONE);

        // GetPRefType: force MFX_P_REF_SIMPLE for low delay
        defaults.GetPRefType.Push([](
            Defaults::TChain<mfxU16>::TExt prev
            , const Defaults::Param& par)
        {
            auto GopRefDist = par.base.GetGopRefDist(par);
            // The low delay BQR reference logic assumes a flat single temporal
            // layer where every POC in the sliding window is a stored reference.
            // With temporal scalability the higher sub-layers are disposable
            // (non-reference) frames at interleaved POCs, so the fixed POC-delta
            // patterns and the POC%4 eviction below would reference/evict frames
            // that are not in the DPB -> dangling references -> HW engine hang.
            // Keep the legacy behavior for multi temporal layer streams.
            if (GopRefDist == 1 && par.base.GetNumTemporalLayers(par) == 1)
                return mfxU16(MFX_P_REF_SIMPLE);
            return prev(par);
        });

        // GetNumRefNoPyramid: custom DPB size for low delay
        defaults.GetNumRefNoPyramid.Push([](
            Defaults::TChain<mfxU16>::TExt prev
            , const Defaults::Param& par)
        {
            auto GopRefDist = par.base.GetGopRefDist(par);
            // Only size the DPB for the low delay path we actually take below;
            // multi temporal layer streams use the legacy sizing (see GetPRefType).
            if (GopRefDist != 1 || par.base.GetNumTemporalLayers(par) > 1)
                return prev(par);

            mfxU16 NumRefActiveP[8], NumRefActiveBL0[8];
            par.base.GetNumRefActive(par, &NumRefActiveP, &NumRefActiveBL0, nullptr);
            mfxU16 maxP   = *std::max_element(NumRefActiveP, NumRefActiveP + 8);
            mfxU16 maxBL0 = *std::max_element(NumRefActiveBL0, NumRefActiveBL0 + 8);
            mfxU16 maxRef = std::max(maxP, maxBL0);
            return mfxU16(3 * (maxRef - 1) + 1);
        });

        // GetNumRefBPyramid: custom DPB size for random access
        defaults.GetNumRefBPyramid.Push([](
            Defaults::TChain<mfxU16>::TExt prev
            , const Defaults::Param& par)
        {
            auto GopRefDist = par.base.GetGopRefDist(par);
            auto BRefType = par.base.GetBRefType(par);

            if (!(GopRefDist > 3 && GopRefDist < 9 && BRefType == MFX_B_REF_PYRAMID))
                return prev(par);

            mfxU16 MinRefForBPyramid = par.base.GetMinRefForBPyramid(par);
            mfxU16 NumRefActiveP[8], NumRefActiveBL0[8];
            par.base.GetNumRefActive(par, &NumRefActiveP, &NumRefActiveBL0, nullptr);
            mfxU16 maxP   = *std::max_element(NumRefActiveP, NumRefActiveP + 8);
            mfxU16 maxBL0 = *std::max_element(NumRefActiveBL0, NumRefActiveBL0 + 8);
            mfxU16 maxRef = std::max(maxP, maxBL0);
            return std::max<mfxU16>(MinRefForBPyramid, mfxU16(maxRef * GopRefDist / 2));
        });

        // GetRefPicList: custom RPL for low delay and random access
        defaults.GetRefPicList.Push([](
            Defaults::TGetRPL::TExt prev
            , const Defaults::Param& par
            , const DpbArray& DPB
            , mfxU16 maxL0
            , mfxU16 maxL1
            , const FrameBaseInfo& cur
            , mfxU8(&RefPicList)[2][MAX_DPB_SIZE])
        {
            auto GopRefDist = par.base.GetGopRefDist(par);
            auto PRefType   = par.base.GetPRefType(par);

            // External ref control: check null and then check for non-zero data
            const mfxExtAVCRefLists*    pExtLists    = ExtBuffer::Get(par.mvp);
            const mfxExtAVCRefListCtrl* pExtListCtrl = ExtBuffer::Get(par.mvp);
            if (HasExtBufferData(pExtLists) || HasExtBufferData(pExtListCtrl))
                return prev(par, DPB, maxL0, maxL1, cur, RefPicList);

            // Shared helper: order references by POC distance to the current frame
            struct Ref { mfxU8 idx; mfxI32 poc; };
            auto byPocDist = [&cur](const Ref& a, const Ref& b)
            {
                return std::abs(cur.POC - a.poc) < std::abs(cur.POC - b.poc);
            };

            // Low delay mode (only when PRefType is SIMPLE, not user-set PYRAMID,
            // and only for a single temporal layer: the fixed POC-delta patterns
            // assume every POC is a stored reference, which is false when higher
            // temporal sub-layers hold disposable frames at interleaved POCs).
            if (GopRefDist == 1 && PRefType == MFX_P_REF_SIMPLE
                && par.base.GetNumTemporalLayers(par) == 1)
            {
                std::fill(RefPicList[0], RefPicList[0] + MAX_DPB_SIZE, mfxU8(IDX_INVALID));
                std::fill(RefPicList[1], RefPicList[1] + MAX_DPB_SIZE, mfxU8(IDX_INVALID));

                mfxU8 l0 = 0;

                mfxU32 cycleIdx = ((mfxU32)cur.POC) % 4;

                // Try primary pattern first
                bool primaryOk = true;
                for (mfxU32 i = 0; i < 3 && i < maxL0; i++)
                {
                    mfxI32 refPOC = cur.POC + kLDPrimaryPattern4[cycleIdx][i];
                    mfxU8 idx = Legacy::GetDPBIdxByPoc(DPB, refPOC);
                    if (idx >= MAX_DPB_SIZE)
                    {
                        primaryOk = false;
                        break;
                    }
                    RefPicList[0][l0++] = idx;
                }

                // Fallback if any primary ref not found
                if (!primaryOk)
                {
                    l0 = 0;
                    std::fill(RefPicList[0], RefPicList[0] + MAX_DPB_SIZE, mfxU8(IDX_INVALID));
                    for (mfxU32 i = 0; i < 3 && l0 < maxL0; i++)
                    {
                        mfxI32 refPOC = cur.POC + kLDFallbackPattern4[cycleIdx][i];
                        mfxU8 idx = Legacy::GetDPBIdxByPoc(DPB, refPOC);
                        if (idx < MAX_DPB_SIZE)
                            RefPicList[0][l0++] = idx;
                    }
                }

                // L1 handled by RPLMod (copies L0 -> L1 for LDB/VDENC)
                return std::make_tuple(l0, mfxU8(0));
            }

            // Random access mode (GopRefDist > 1)
            auto BRefType = par.base.GetBRefType(par);
            if (!(GopRefDist > 3 && GopRefDist < 9 && BRefType == MFX_B_REF_PYRAMID))
                return prev(par, DPB, maxL0, maxL1, cur, RefPicList);

            std::fill(RefPicList[0], RefPicList[0] + MAX_DPB_SIZE, mfxU8(IDX_INVALID));
            std::fill(RefPicList[1], RefPicList[1] + MAX_DPB_SIZE, mfxU8(IDX_INVALID));

            mfxU8 l0 = 0, l1 = 0;

            if (cur.PyramidLevel <= 1)
            {
                // Level 0&1: use POC-nearest references
                std::vector<Ref> refs;

                for (mfxU8 i = 0; !isDpbEnd(DPB, i); i++)
                {
                    if (isValid(DPB[i]) && DPB[i].PyramidLevel == 0 && DPB[i].TemporalID <= cur.TemporalID)
                        refs.push_back({ i, DPB[i].POC });
                }

                std::sort(refs.begin(), refs.end(), byPocDist);

                for (auto& r : refs)
                {
                    if (r.poc < cur.POC && l0 < maxL0)
                        RefPicList[0][l0++] = r.idx;
                    else if (r.poc > cur.POC && l1 < maxL1)
                        RefPicList[1][l1++] = r.idx;
                }
            }
            else if (cur.PyramidLevel == 2)
            {
                // Level 2: nearest from Level 0/1
                mfxU8 nearestL0Idx = MAX_DPB_SIZE;
                mfxU8 nearestL1Idx = MAX_DPB_SIZE;
                mfxI32 nearestL0Dist = INT_MAX, nearestL1Dist = INT_MAX;

                for (mfxU8 i = 0; !isDpbEnd(DPB, i); i++)
                {
                    if (!isValid(DPB[i]) || DPB[i].PyramidLevel > 1 || DPB[i].TemporalID > cur.TemporalID)
                        continue;

                    mfxI32 dist = std::abs(cur.POC - DPB[i].POC);

                    if (DPB[i].POC < cur.POC && dist < nearestL0Dist)
                    {
                        nearestL0Dist = dist;
                        nearestL0Idx = i;
                    }
                    else if (DPB[i].POC > cur.POC && dist < nearestL1Dist)
                    {
                        nearestL1Dist = dist;
                        nearestL1Idx = i;
                    }
                }

                if (nearestL0Idx < MAX_DPB_SIZE)
                    RefPicList[0][l0++] = nearestL0Idx;
                if (nearestL1Idx < MAX_DPB_SIZE)
                    RefPicList[1][l1++] = nearestL1Idx;

                // If L0 requested count is > 1 and we still have room, add the
                // second-nearest PyramidLevel 2 frame to L0.
                if (maxL0 > 1 && l0 < maxL0)
                {
                    std::vector<Ref> pyr2Refs;
                    for (mfxU8 i = 0; !isDpbEnd(DPB, i); i++)
                    {
                        if (isValid(DPB[i]) && DPB[i].PyramidLevel == 2
                            && DPB[i].POC != cur.POC && DPB[i].TemporalID <= cur.TemporalID)
                            pyr2Refs.push_back({ i, DPB[i].POC });
                    }
                    // nearest first; tie-break by POC ascending for determinism
                    std::sort(pyr2Refs.begin(), pyr2Refs.end(),
                        [&](const Ref& a, const Ref& b)
                        {
                            mfxI32 da = std::abs(cur.POC - a.poc), db = std::abs(cur.POC - b.poc);
                            return da != db ? da < db : a.poc < b.poc;
                        });
                    if (pyr2Refs.size() >= 2)
                        RefPicList[0][l0++] = pyr2Refs[1].idx;
                }
            }
            else
            {
                // Level 3+: use POC-nearest from all
                std::vector<Ref> l0Refs, l1Refs;

                for (mfxU8 i = 0; !isDpbEnd(DPB, i); i++)
                {
                    if (isValid(DPB[i]) && DPB[i].TemporalID <= cur.TemporalID)
                    {
                        if (DPB[i].POC < cur.POC)
                            l0Refs.push_back({ i, DPB[i].POC });
                        else if (DPB[i].POC > cur.POC)
                            l1Refs.push_back({ i, DPB[i].POC });
                    }
                }

                std::sort(l0Refs.begin(), l0Refs.end(), byPocDist);
                std::sort(l1Refs.begin(), l1Refs.end(), byPocDist);

                for (auto& r : l0Refs)
                    if (l0 < maxL0)
                        RefPicList[0][l0++] = r.idx;
                for (auto& r : l1Refs)
                    if (l1 < maxL1)
                        RefPicList[1][l1++] = r.idx;
            }

            // L1->L0 spillover: when L0 count < maxL0, move L1 refs to L0
            if (l0 < maxL0 && l1 > 0)
            {
                for (mfxU8 i = 0; i < l1 && l0 < maxL0; i++)
                    RefPicList[0][l0++] = RefPicList[1][i];
            }

            return std::make_tuple(l0, l1);
        });

        // GetWeakRef: custom DPB eviction
        defaults.GetWeakRef.Push([](
            Defaults::TGetWeakRef::TExt prev
            , const Defaults::Param& par
            , const FrameBaseInfo& cur
            , const DpbFrame* begin
            , const DpbFrame* end)
        {
            auto GopRefDist = par.base.GetGopRefDist(par);
            auto PRefType   = par.base.GetPRefType(par);

            // External ref control: check null and then check for non-zero data
            const mfxExtAVCRefLists*    pExtLists    = ExtBuffer::Get(par.mvp);
            const mfxExtAVCRefListCtrl* pExtListCtrl = ExtBuffer::Get(par.mvp);
            if (HasExtBufferData(pExtLists) || HasExtBufferData(pExtListCtrl))
                return prev(par, cur, begin, end);

            // Low delay: only when PRefType is SIMPLE, not user-set PYRAMID, and
            // only for a single temporal layer. The POC%4 eviction below treats
            // POC%4!=0 frames as weak; with temporal scalability the live TID0
            // anchors sit at even POCs (incl. POC%4==2) and would be evicted while
            // still referenced -> dangling reference -> HW hang. Multi temporal
            // layer streams fall through to the legacy eviction.
            if (GopRefDist == 1 && PRefType == MFX_P_REF_SIMPLE
                && par.base.GetNumTemporalLayers(par) == 1)
            {
                // Low delay: remove frames where (POC % 4) != 0
                const DpbFrame* weakRef = nullptr;
                for (const DpbFrame* it = begin; it != end; ++it)
                {
                    if (!isValid(*it)) continue;
                    if ((it->POC % 4) != 0)
                    {
                        if (!weakRef || it->POC < weakRef->POC)
                            weakRef = it;
                    }
                }
                // If none found, remove frame with smallest POC
                if (!weakRef)
                {
                    auto POCLess = [](const DpbFrame& a, const DpbFrame& b) { return a.POC < b.POC; };
                    weakRef = std::min_element(begin, end, POCLess);
                }
                return weakRef;
            }

            // Random access: prioritize removing smallest POC
            auto BRefType = par.base.GetBRefType(par);
            if (GopRefDist > 3 && GopRefDist < 9 && BRefType == MFX_B_REF_PYRAMID)
            {
                auto cmp = [](const DpbFrame& a, const DpbFrame& b) {
                    return a.POC < b.POC;
                };
                return std::min_element(begin, end, cmp);
            }

            return prev(par, cur, begin, end);
        });

        bSet = true;
        return MFX_ERR_NONE;
    });
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
