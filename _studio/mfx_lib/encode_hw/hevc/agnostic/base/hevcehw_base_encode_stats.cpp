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

#ifdef MFX_ENABLE_ENCODE_STATS
#ifdef MFX_ENABLE_H265_VIDEO_ENCODE

#include "hevcehw_base_encode_stats.h"
#include "hevcehw_base_task.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;

void EncodeStats::FrameSubmit(const FeatureBlocks& /*blocks*/, TPushFS Push)
{
    Push(BLK_CheckBS
        , [&](
            const mfxEncodeCtrl* /*pCtrl*/
            , const mfxFrameSurface1* /*pSurf*/
            , mfxBitstream& bs
            , StorageRW& /*global*/
            , StorageRW& /*local*/) -> mfxStatus
    {
        mfxExtEncodeStatsOutput* pStats = ExtBuffer::Get(bs);

        m_frameLevelQueryEn = pStats == nullptr ? false :
            !!(pStats->EncodeStatsFlags & MFX_ENCODESTATS_LEVEL_FRAME);

        m_blockLevelQueryEn = pStats == nullptr ? false :
            !!(pStats->EncodeStatsFlags & MFX_ENCODESTATS_LEVEL_BLK);

        return MFX_ERR_NONE;
    });
}

void EncodeStats::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_PatchDDITask
        , [this](StorageW& global, StorageW& /*s_task*/) -> mfxStatus
    {
        return PatchDdi(global);
    });
}

void EncodeStats::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_QueryInit
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        MFX_CHECK(m_frameLevelQueryEn || m_blockLevelQueryEn, MFX_ERR_NONE);

        auto& taskPar = Task::Common::Get(s_task);
        MFX_CHECK(taskPar.pBsOut, MFX_ERR_NONE);

        mfxExtEncodeStatsOutput* pStats = ExtBuffer::Get(*taskPar.pBsOut);
        MFX_CHECK(pStats, MFX_ERR_NULL_PTR);

        if (pStats->EncodeStatsContainer == nullptr)
        {
            pStats->EncodeStatsContainer = mfxEncodeStatsContainerHevc::Create();
        }
        MFX_CHECK(pStats->EncodeStatsContainer, MFX_ERR_NULL_PTR);

        auto pContainer = reinterpret_cast<mfxEncodeStatsContainerHevc*>(
            pStats->EncodeStatsContainer->RefInterface.Context);
        MFX_CHECK(pContainer, MFX_ERR_NULL_PTR);

        const auto& par = Glob::VideoParam::Get(global);
        mfxU32 numCtu = ((par.mfx.FrameInfo.Width + 63) / 64) *
            ((par.mfx.FrameInfo.Height + 63) / 64);

        if (pStats->EncodeStatsFlags & MFX_ENCODESTATS_LEVEL_FRAME)
        {
            MFX_CHECK(m_frameLevelQueryEn, MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK_STS(pContainer->AllocFrameStatsBuf());
            MFX_CHECK(pStats->EncodeStatsContainer->EncodeFrameStats, MFX_ERR_NULL_PTR);

            pStats->EncodeStatsContainer->EncodeFrameStats->NumCTU = numCtu;
        }

        if (pStats->EncodeStatsFlags & MFX_ENCODESTATS_LEVEL_BLK)
        {
            MFX_CHECK(m_blockLevelQueryEn, MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK_STS(pContainer->AllocBlkStatsBuf(numCtu));
            MFX_CHECK(pStats->EncodeStatsContainer->EncodeBlkStats, MFX_ERR_NULL_PTR);
            MFX_CHECK(pStats->EncodeStatsContainer->EncodeBlkStats->NumCTU, MFX_ERR_NOT_INITIALIZED);
            MFX_CHECK(pStats->EncodeStatsContainer->EncodeBlkStats->HEVCCTUArray, MFX_ERR_NULL_PTR);
        }

        auto& ddiFb = Glob::DDI_Feedback::Get(global);
        for (auto& xPar : ddiFb.ExecParam)
        {
            MFX_CHECK_STS(PatchFeedback(xPar.Out.pData
                , *pStats->EncodeStatsContainer));
        }

        return MFX_ERR_NONE;
    });

    Push(BLK_QueryTask
        , [this](StorageW& /*global*/, StorageW& s_task) -> mfxStatus
    {
        MFX_CHECK(m_frameLevelQueryEn || m_blockLevelQueryEn, MFX_ERR_NONE);

        auto& taskPar = Task::Common::Get(s_task);
        MFX_CHECK(taskPar.pBsOut, MFX_ERR_UNDEFINED_BEHAVIOR);

        mfxExtEncodeStatsOutput* pStats = ExtBuffer::Get(*taskPar.pBsOut);
        MFX_CHECK(pStats, MFX_ERR_NULL_PTR);
        MFX_CHECK(pStats->EncodeStatsContainer, MFX_ERR_NULL_PTR);

        pStats->EncodeStatsContainer->DisplayOrder = taskPar.DisplayOrder;

        return MFX_ERR_NONE;
    });
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
#endif // MFX_ENABLE_ENCODE_STATS
