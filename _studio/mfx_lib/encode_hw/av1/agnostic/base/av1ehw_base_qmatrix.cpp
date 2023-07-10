// Copyright (c) 2020-2023 Intel Corporation
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

#include "av1ehw_base_qmatrix.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;

void QpHistory::Add(mfxU32 qp)
{
    std::copy_n(m_history, HIST_SIZE - 1, m_history + 1);
    m_history[0] = static_cast<mfxU8>(qp);
}

mfxU8 QpHistory::GetAverageQp() const
{
    mfxU32 averageQP = 0;
    mfxU32 numQPs    = 0;
    for (mfxU8 qp : m_history)
    {
        if (qp < 52)
        {
            averageQP += qp;
            numQPs++;
        }
    }

    if (numQPs > 0)
        averageQP = (averageQP + numQPs / 2) / numQPs;

    return static_cast<mfxU8>(averageQP);
}

static mfxU8 GetAdaptiveCQM(const mfxVideoParam& par, const QpHistory qpHistory)
{
    mfxU8 qMatrix = AV1_NUM_QMATRIX;
    const mfxExtCodingOption3* CO3 = ExtBuffer::Get(par);
    if (CO3 && IsOn(CO3->AdaptiveCQM))
    {
        const mfxU32 averageQP = qpHistory.GetAverageQp();

        if (averageQP == 0) // not enough history QP
        {
            const mfxU32 MBSIZE = 16;
            const mfxU32 BITRATE_SCALE = 2000;
            const mfxU32 numMB = (par.mfx.FrameInfo.Width / MBSIZE) * (par.mfx.FrameInfo.Height / MBSIZE);
            const mfxU32 normalizedBitrate = mfxU32(mfxU64(BITRATE_SCALE) * par.mfx.BufferSizeInKB*1000*
                (par.mfx.BRCParamMultiplier ? par.mfx.BRCParamMultiplier:1)
                * par.mfx.FrameInfo.FrameRateExtD / par.mfx.FrameInfo.FrameRateExtN / numMB);

            const mfxU32 STRONG_QM_BR_THRESHOLD = 25;
            const mfxU32 MEDIUM_QM_BR_THRESHOLD = 50;

            qMatrix
                = (normalizedBitrate < STRONG_QM_BR_THRESHOLD) ? 8
                : (normalizedBitrate < MEDIUM_QM_BR_THRESHOLD) ? 10
                : 12;
        }
        else
        {
            const mfxU32 FLAT_QM_QP_THRESHOLD   = 50;
            const mfxU32 WEAK_QM_QP_THRESHOLD   = 100;
            const mfxU32 MEDIUM_QM_QP_THRESHOLD = 150;
            const mfxU32 STRONG_QM_QP_THRESHOLD = 220;
            qMatrix
                = averageQP < FLAT_QM_QP_THRESHOLD ? 14
                : averageQP < WEAK_QM_QP_THRESHOLD ? 12
                : averageQP < MEDIUM_QM_QP_THRESHOLD ? 10
                : averageQP < STRONG_QM_QP_THRESHOLD ? 8
                : 6;
        }
    }

    return qMatrix;
}

void QMatrix::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_CODING_OPTION3].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtCodingOption3*)pSrc;
        auto& buf_dst = *(mfxExtCodingOption3*)pDst;

        MFX_COPY_FIELD(AdaptiveCQM);
        MFX_COPY_FIELD(ScenarioInfo);
    });
}

static bool isAdaptiveCQMSupported(const mfxU16 scenarioInfo)
{
    return scenarioInfo == MFX_SCENARIO_GAME_STREAMING || scenarioInfo == MFX_SCENARIO_REMOTE_GAMING;
}

void QMatrix::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [this](mfxVideoParam& par, StorageW& /*global*/, StorageRW&)
    {
        mfxExtCodingOption3* CO3 = ExtBuffer::Get(par);
        if (CO3 && CO3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING)
        {
            SetDefault(CO3->AdaptiveCQM, MFX_CODINGOPTION_ON);
        }
    });
}

void QMatrix::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [this](const mfxVideoParam& /*in*/, mfxVideoParam& par, StorageW& /*global*/) -> mfxStatus
    {
        mfxExtCodingOption3* CO3 = ExtBuffer::Get(par);
        MFX_CHECK(CO3, MFX_ERR_NONE);

        if (IsOn(CO3->AdaptiveCQM)
            && !isAdaptiveCQMSupported(CO3->ScenarioInfo))
        {
            CO3->AdaptiveCQM = 0;
            return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        return MFX_ERR_NONE;
    });
}

void QMatrix::PostReorderTask(const FeatureBlocks& /*blocks*/, TPushPostRT Push)
{
    Push(BLK_ConfigureTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        const auto& par = Glob::VideoParam::Get(global);
        const mfxExtCodingOption2* CO2 = ExtBuffer::Get(par);
        MFX_CHECK(!(CO2 && CO2->LookAheadDepth > 0), MFX_ERR_NONE);

        const mfxU8 nQMatrix = GetAdaptiveCQM(par, m_qpHistory);
        auto&       fh       = Task::FH::Get(s_task);
        MFX_CHECK(fh.quantization_params.using_qmatrix == 0, MFX_ERR_NONE);

        fh.quantization_params.using_qmatrix = nQMatrix < AV1_NUM_QMATRIX ? 1 : 0;
        if (fh.quantization_params.using_qmatrix)
        {
            fh.quantization_params.qm_y = nQMatrix;
            fh.quantization_params.qm_u = nQMatrix;
            fh.quantization_params.qm_v = nQMatrix;
        }

        return MFX_ERR_NONE;
    });
}

void QMatrix::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_Update
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        const auto& par = Glob::VideoParam::Get(global);
        const mfxExtCodingOption2* CO2 = ExtBuffer::Get(par);
        MFX_CHECK(!(CO2 && CO2->LookAheadDepth > 0), MFX_ERR_NONE);

        const auto& fb = Glob::DDI_Feedback::Get(global);
        MFX_CHECK(!fb.bNotReady, MFX_TASK_BUSY);

        const void* pFeedback = fb.Get(Task::Common::Get(s_task).StatusReportId);
        if (pFeedback)
        {
            mfxU16 qp = 0;
            MFX_SAFE_CALL(GetQPInfo(pFeedback, qp));
            m_qpHistory.Add(qp);
        }

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
