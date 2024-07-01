// Copyright (c) 2019-2022 Intel Corporation
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
#if defined(MFX_ENABLE_ENCTOOLS_SW)

#include <numeric>
#include "hevcehw_base_enctools_qmatrix_lin.h"
#include "hevcehw_base_va_packer_lin.h"
#include "hevcehw_base_packer.h"


#include "hevcehw_base_enctools.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;
using namespace HEVCEHW::Linux;
using namespace HEVCEHW::Linux::Base;

const uint8_t scalingList0_flat[16] =
{
    16, 16, 16, 16,
    16, 16, 16, 16,
    16, 16, 16, 16,
    16, 16, 16, 16
};

const uint8_t scalingList_flat[64] =
{
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16
};

const uint8_t scalingList0_intraET_ForNoisy[16] =
{
    16, 20, 26, 40,
    20, 26, 40, 44,
    26, 40, 44, 48,
    40, 44, 48, 60
};

const uint8_t scalingList0_interET_ForNoisy[16] =
{
    16, 16, 16, 16,
    16, 16, 16, 16,
    16, 16, 16, 16,
    16, 16, 16, 16
};

const uint8_t scalingList_intraET_ForNoisy[64] =
{
    16, 18, 20, 22, 25, 28, 31, 34,
    18, 20, 22, 25, 28, 31, 34, 79,
    20, 22, 25, 28, 31, 34, 79, 82,
    22, 25, 28, 31, 34, 79, 82, 85,
    25, 28, 31, 34, 79, 82, 85, 88,
    28, 31, 34, 79, 82, 85, 88, 91,
    31, 34, 79, 82, 85, 88, 91, 94,
    34, 79, 82, 85, 88, 91, 94, 97
};

const uint8_t scalingList_interET_ForNoisy[64] =
{
    16,16,16,16,17,18,20,24,
    16,16,16,17,18,20,24,25,
    16,16,17,18,20,24,25,28,
    16,17,18,20,24,25,28,33,
    17,18,20,24,25,28,33,41,
    18,20,24,25,28,33,41,54,
    20,24,25,28,33,41,54,71,
    24,25,28,33,41,54,71,91
};

const uint8_t scalingList0_intraET_unified[ET_CQM_NUM_CUST_MATRIX][16] =
{
    // multiple CQM, medium
    {
        19,26,36,46,
        26,36,46,57,
        36,46,57,68,
        46,57,68,80
    },
    // multiple CQM, strong
    {
        30,31,33,38,
        31,33,38,47,
        33,38,47,60,
        38,47,60,77
    },
    // multiple CQM, extrem
    {
        50,51,53,58,
        51,53,58,67,
        53,58,67,80,
        58,67,80,97
    }
};

const uint8_t scalingList0_interET_unified[ET_CQM_NUM_CUST_MATRIX][16] =
{
    // multiple CQM, medium
    {
        19,26,36,46,
        26,36,46,57,
        36,46,57,68,
        46,57,68,80
    },
    // multiple CQM, strong
    {
        25,26,36,46,
        26,36,46,57,
        36,46,57,68,
        46,57,68,80
    },
    // multiple CQM, extrem
    {
        60,61,63,68,
        61,63,68,77,
        63,68,77,90,
        68,77,90,107
    }
};

const uint8_t scalingList_intraET_unified[ET_CQM_NUM_CUST_MATRIX][64] =
{
    // multiple CQM, medium
    {
        19,19,26,31,36,41,46,52,
        19,26,31,36,41,46,52,57,
        26,31,36,41,46,52,57,63,
        31,36,41,46,52,57,63,68,
        36,41,46,52,57,63,68,74,
        41,46,52,57,63,68,74,80,
        46,52,57,63,68,74,80,86,
        52,57,63,68,74,80,86,91
    },
    // multiple CQM, strong
    {
        30,30,31,32,33,35,38,42,
        30,31,32,33,35,38,42,47,
        31,32,33,35,38,42,47,53,
        32,33,35,38,42,47,53,60,
        33,35,38,42,47,53,60,68,
        35,38,42,47,53,60,68,77,
        38,42,47,53,60,68,77,87,
        42,47,53,60,68,77,87,98,
    },
    // multiple CQM, extreme
    {
        50,50,51,52,53,55,58,62,
        50,51,52,53,55,58,62,67,
        51,52,53,55,58,62,67,73,
        52,53,55,58,62,67,73,80,
        53,55,58,62,67,73,80,88,
        55,58,62,67,73,80,88,97,
        58,62,67,73,80,88,97,107,
        62,67,73,80,88,97,107,118,
    }
};

const uint8_t scalingList_interET_unified[ET_CQM_NUM_CUST_MATRIX][64] =
{
    // multiple CQM, medium
    {
        19,19,26,31,36,41,46,52,
        19,26,31,36,41,46,52,57,
        26,31,36,41,46,52,57,63,
        31,36,41,46,52,57,63,68,
        36,41,46,52,57,63,68,74,
        41,46,52,57,63,68,74,80,
        46,52,57,63,68,74,80,86,
        52,57,63,68,74,80,86,91
    },
    // multiple CQM, strong
    {
        40,40,41,42,43,45,48,52,
        40,41,42,43,45,48,52,57,
        41,42,43,45,48,52,57,63,
        42,43,45,48,52,57,63,70,
        43,45,48,52,57,63,70,78,
        45,48,52,57,63,70,78,87,
        48,52,57,63,70,78,87,97,
        52,57,63,70,78,87,97,108
    },
    // multiple CQM, extreme
    {
        60,60,61,62,63,65,68,72,
        60,61,62,63,65,68,72,77,
        61,62,63,65,68,72,77,83,
        62,63,65,68,72,77,83,90,
        63,65,68,72,77,83,90,98,
        65,68,72,77,83,90,98,107,
        68,72,77,83,90,98,107,117,
        72,77,83,90,98,107,117,128
    }
};

template<typename F>
void ProcessUpRight(size_t size, F&& f)
{
    int y = 0, x = 0;
    size_t i = 0;
    while (i < size * size)
    {
        while (y >= 0)
        {
            if ((((size_t)x) < size) && (((size_t)y) < size))
            {
                std::forward<F>(f)(x, y, i);
                ++i;
            }
            --y;
            ++x;
        }
        y = x;
        x = 0;
    }
}

/// 1 2 3    1 2 4
/// 4 5 6 -> 3 5 7
/// 7 8 9    6 8 9
/// For scanning from plane to up-right
template<typename T>
void MakeUpRight(T const * in, size_t size, T * out)
{
    ProcessUpRight(size, [=](size_t x, size_t y, size_t i) { out[x * size + y] = in[i]; });
}

template<typename C>
void UpRightToPlane(C const* in, size_t size, C* out)
{
    ProcessUpRight(size, [=](size_t x, size_t y, size_t i) { out[i] = in[x * size + y]; });
}

void Linux::Base::EncToolsSwQMatrix::QpHistory::Add(mfxU8 qp)
{
    if (qp != 0xff)
    {
        if(history.size() >= HIST_SIZE) {
            history.pop_front();
        }
        history.push_back(qp);
    }
}

mfxU8 Linux::Base::EncToolsSwQMatrix::QpHistory::GetAverageQp() const
{
    mfxU32 averageQP = std::accumulate(history.begin(), history.end(), 0);;
    mfxU32 numQPs = history.size();
    if (numQPs > 0)
        averageQP = (averageQP + numQPs / 2) / numQPs;
    return static_cast<mfxU8>(averageQP);
}

inline bool IsCustETMatrix(mfxU32 CqmHint)
{
    return CqmHint > ET_CQM_USE_FLAT_MATRIX && CqmHint <= ET_CQM_NUM_CUST_MATRIX;
}

void Linux::Base::EncToolsSwQMatrix::InitInternal(const FeatureBlocks& , TPushII Push)
{
    Push(BLK_UpdateSPS
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        bool bETSpsACQM = IsSwEncToolsSpsACQM(par);
        bool bETadaptQM = bETSpsACQM || IsSwEncToolsPpsACQM(par);

        MFX_CHECK(bETadaptQM, MFX_ERR_NONE);

        auto& sps = Glob::SPS::Get(global);
        sps.scaling_list_enabled_flag = 1; // Scaling List Enabled Flag Must Be Set if SPS or PPS is used

        if(bETadaptQM) {
            sps.scaling_list_data_present_flag = 1; // Only Set SPS Scaling List Present Flag if SPS is used

            const uint8_t* scalingList0Ptr = scalingList0_flat;
            const uint8_t* scalingList0_interPtr = scalingList0_flat;
            const uint8_t* scalingList_intraPtr = scalingList_flat;
            const uint8_t* scalingList_interPtr = scalingList_flat;

            if (bETSpsACQM)
            {
                scalingList0Ptr = scalingList0_intraET_ForNoisy;
                scalingList0_interPtr = scalingList0_interET_ForNoisy;
                scalingList_intraPtr = scalingList_intraET_ForNoisy;
                scalingList_interPtr = scalingList_interET_ForNoisy;
            }

            UpRightToPlane(scalingList0Ptr, 4, sps.scl.scalingLists0[0]);
            UpRightToPlane(scalingList_intraPtr, 8, sps.scl.scalingLists1[0]);
            UpRightToPlane(scalingList_intraPtr, 8, sps.scl.scalingLists2[0]);
            UpRightToPlane(scalingList_intraPtr, 8, sps.scl.scalingLists3[0]);
            sps.scl.scalingListDCCoefSizeID3[0] = sps.scl.scalingLists3[0][0];
            sps.scl.scalingListDCCoefSizeID2[0] = sps.scl.scalingLists2[0][0];

            UpRightToPlane(scalingList0Ptr, 4, sps.scl.scalingLists0[1]);
            UpRightToPlane(scalingList_intraPtr, 8, sps.scl.scalingLists1[1]);
            UpRightToPlane(scalingList_intraPtr, 8, sps.scl.scalingLists2[1]);
            UpRightToPlane(scalingList_interPtr, 8, sps.scl.scalingLists3[1]);
            sps.scl.scalingListDCCoefSizeID3[1] = sps.scl.scalingLists3[1][0];
            sps.scl.scalingListDCCoefSizeID2[1] = sps.scl.scalingLists2[1][0];

            UpRightToPlane(scalingList0Ptr, 4, sps.scl.scalingLists0[2]);
            UpRightToPlane(scalingList_intraPtr, 8, sps.scl.scalingLists1[2]);
            UpRightToPlane(scalingList_intraPtr, 8, sps.scl.scalingLists2[2]);
            sps.scl.scalingListDCCoefSizeID2[2] = sps.scl.scalingLists2[2][0];

            UpRightToPlane(scalingList0_interPtr, 4, sps.scl.scalingLists0[3]);
            UpRightToPlane(scalingList_interPtr, 8, sps.scl.scalingLists1[3]);
            UpRightToPlane(scalingList_interPtr, 8, sps.scl.scalingLists2[3]);
            sps.scl.scalingListDCCoefSizeID2[3] = sps.scl.scalingLists2[3][0];

            UpRightToPlane(scalingList0_interPtr, 4, sps.scl.scalingLists0[4]);
            UpRightToPlane(scalingList_interPtr, 8, sps.scl.scalingLists1[4]);
            UpRightToPlane(scalingList_interPtr, 8, sps.scl.scalingLists2[4]);
            sps.scl.scalingListDCCoefSizeID2[4] = sps.scl.scalingLists2[4][0];

            UpRightToPlane(scalingList0_interPtr, 4, sps.scl.scalingLists0[5]);
            UpRightToPlane(scalingList_interPtr, 8, sps.scl.scalingLists1[5]);
            UpRightToPlane(scalingList_interPtr, 8, sps.scl.scalingLists2[5]);
            sps.scl.scalingListDCCoefSizeID2[5] = sps.scl.scalingLists2[5][0];
        }

        if (global.Contains(Glob::RealState::Key))
        {
            auto& hint = Glob::ResetHint::Get(global);
            const SPS& oldSPS = Glob::SPS::Get(Glob::RealState::Get(global));
            SPS& newSPS = Glob::SPS::Get(global);

            bool bSPSChanged = !!memcmp(&newSPS, &oldSPS, sizeof(SPS));

            hint.Flags = RF_SPS_CHANGED * (bSPSChanged || (hint.Flags & RF_IDR_REQUIRED));
        }

        return MFX_ERR_NONE;
    });

    Push(BLK_UpdatePPS
        , [this](StorageRW& global, StorageRW&)->mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        auto& CO3 = (const mfxExtCodingOption3&)ExtBuffer::Get(par);
        bool bETadaptQM = IsSwEncToolsPpsACQM(par);

        MFX_CHECK(bETadaptQM, MFX_ERR_NONE);

        auto& pps = Glob::PPS::Get(global);

        auto cqmpps = make_unique<MakeStorable<std::vector<Base::PPS>>>();
        for (mfxU8 idx = 0; idx < ET_CQM_NUM_CUST_MATRIX; idx++)
        {
            const uint8_t (*scalingList0PtrIntra)[16] = scalingList0_intraET_unified;
            const uint8_t (*scalingList0PtrInter)[16] = scalingList0_interET_unified;
            const uint8_t (*scalingListsPtrIntra)[64] = scalingList_intraET_unified;
            const uint8_t (*scalingListsPtrInter)[64] = scalingList_interET_unified;

            PPS cqmPPS = pps;
            cqmPPS.pic_parameter_set_id = idx + 1;
            cqmPPS.scaling_list_data_present_flag = 1;

            UpRightToPlane(scalingList0PtrIntra[idx], 4, cqmPPS.sld.scalingLists0[0]);
            UpRightToPlane(scalingListsPtrIntra[idx], 8, cqmPPS.sld.scalingLists1[0]);
            UpRightToPlane(scalingListsPtrIntra[idx], 8, cqmPPS.sld.scalingLists2[0]);
            UpRightToPlane(scalingListsPtrIntra[idx], 8, cqmPPS.sld.scalingLists3[0]);
            cqmPPS.sld.scalingListDCCoefSizeID3[0] = cqmPPS.sld.scalingLists3[0][0];
            cqmPPS.sld.scalingListDCCoefSizeID2[0] = cqmPPS.sld.scalingLists2[0][0];

            UpRightToPlane(scalingList0PtrIntra[idx], 4, cqmPPS.sld.scalingLists0[1]);
            UpRightToPlane(scalingListsPtrIntra[idx], 8, cqmPPS.sld.scalingLists1[1]);
            UpRightToPlane(scalingListsPtrIntra[idx], 8, cqmPPS.sld.scalingLists2[1]);
            UpRightToPlane(scalingListsPtrInter[idx], 8, cqmPPS.sld.scalingLists3[1]);
            cqmPPS.sld.scalingListDCCoefSizeID3[1] = cqmPPS.sld.scalingLists3[1][0];
            cqmPPS.sld.scalingListDCCoefSizeID2[1] = cqmPPS.sld.scalingLists2[1][0];

            UpRightToPlane(scalingList0PtrIntra[idx], 4, cqmPPS.sld.scalingLists0[2]);
            UpRightToPlane(scalingListsPtrIntra[idx], 8, cqmPPS.sld.scalingLists1[2]);
            UpRightToPlane(scalingListsPtrIntra[idx], 8, cqmPPS.sld.scalingLists2[2]);
            cqmPPS.sld.scalingListDCCoefSizeID2[2] = cqmPPS.sld.scalingLists2[2][0];


            UpRightToPlane(scalingList0PtrInter[idx], 4, cqmPPS.sld.scalingLists0[3]);

            UpRightToPlane(scalingListsPtrInter[idx], 8, cqmPPS.sld.scalingLists1[3]);
            UpRightToPlane(scalingListsPtrInter[idx], 8, cqmPPS.sld.scalingLists2[3]);
            cqmPPS.sld.scalingListDCCoefSizeID2[3] = cqmPPS.sld.scalingLists2[3][0];


            UpRightToPlane(scalingList0PtrInter[idx], 4, cqmPPS.sld.scalingLists0[4]);

            UpRightToPlane(scalingListsPtrInter[idx], 8, cqmPPS.sld.scalingLists1[4]);
            UpRightToPlane(scalingListsPtrInter[idx], 8, cqmPPS.sld.scalingLists2[4]);
            cqmPPS.sld.scalingListDCCoefSizeID2[4] = cqmPPS.sld.scalingLists2[4][0];


            UpRightToPlane(scalingList0PtrInter[idx], 4, cqmPPS.sld.scalingLists0[5]);

            UpRightToPlane(scalingListsPtrInter[idx], 8, cqmPPS.sld.scalingLists1[5]);
            UpRightToPlane(scalingListsPtrInter[idx], 8, cqmPPS.sld.scalingLists2[5]);
            cqmPPS.sld.scalingListDCCoefSizeID2[5] = cqmPPS.sld.scalingLists2[5][0];

            cqmpps->push_back(cqmPPS);
        }
        global.Insert(Glob::CqmPPS::Key, std::move(cqmpps));

        return MFX_ERR_NONE;
    });

    Push(BLK_SetCallChains
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {

        auto& ddiCC = VAPacker::CC::GetOrConstruct(global);
        using TCC = VAPacker::CallChains;

        auto& packerCC = Packer::CC::GetOrConstruct(global);
        using TPackerCC = Packer::CallChains;

        auto& par = Glob::VideoParam::Get(global);
        bAdaptiveCQMPpsEnabled = IsSwEncToolsPpsACQM(par);

        // Check when it is needed to pack ETSW cqm PPS to driver.
        ddiCC.PackETSWAdaptiveCqmPPS.Push([this](
            TCC::TPackETSWAdaptiveCqmPPS::TExt
            , const StorageR&
            , const StorageR& s_task)
        {
            auto& task = Task::Common::Get(s_task);

            return (task.InsertHeaders & INSERT_PPS) && bAdaptiveCQMPpsEnabled;
        });

        ddiCC.UpdateEncQP.Push([this](
            TCC::TUpdateEncQP::TExt
            , const StorageR&
            , TaskCommonPar&
            , uint8_t QpY)
        {
            MFX_CHECK(bAdaptiveCQMPpsEnabled, MFX_ERR_NONE);

            avgQP.Add(QpY);
            return MFX_ERR_NONE;
        });

        // Check whether to pack cqm PPS header.
        packerCC.PackSWETAdaptiveCqmHeader.Push([this](
            TPackerCC::TPackSWETAdaptiveCqmHeader::TExt
            , StorageW*)
        {
            return bAdaptiveCQMPpsEnabled;
        });
        // Update related slice header params.
        packerCC.UpdateAdaptiveCqmSH.Push([this](
            TPackerCC::TUpdateAdaptiveCqmSH::TExt
            , const StorageR&
            , StorageW& s_task)
        {
            MFX_CHECK(bAdaptiveCQMPpsEnabled, MFX_ERR_NONE);

            auto& slice = Task::SSH::Get(s_task);
            auto& task = Task::Common::Get(s_task);
            const mfxU32 averageQP = avgQP.GetAverageQp();
            uint8_t cqmHint = 0;

            // Panic CQM
            if (task.NumRecode > 0 && task.QpY == 51) {
                // Force Extreme QM in attempt to avoid skip frame
                cqmHint = ET_CQM_USE_CUST_EXTREME_MATRIX;
            }
            else {
                // not enough history QP
                if (averageQP == 0)
                {
                    cqmHint = ET_CQM_USE_FLAT_MATRIX;
                }
                else
                {
                    const mfxU32 FLAT_QM_QP_THRESHOLD = 46;
                    const mfxU32 MEDIUM_QM_QP_THRESHOLD = 48;
                    const mfxU32 STRONG_QM_QP_THRESHOLD = 50;

                    cqmHint
                        = (mfxU8)(averageQP < FLAT_QM_QP_THRESHOLD ? ET_CQM_USE_FLAT_MATRIX
                            : averageQP < MEDIUM_QM_QP_THRESHOLD ? ET_CQM_USE_CUST_MEDIUM_MATRIX
                            : averageQP < STRONG_QM_QP_THRESHOLD ? ET_CQM_USE_CUST_STRONG_MATRIX
                            : ET_CQM_USE_CUST_EXTREME_MATRIX);
                }
            }
            if (IsCustETMatrix(cqmHint))
            {
                slice.pic_parameter_set_id = cqmHint;
            }
            return MFX_ERR_NONE;
        });

        return MFX_ERR_NONE;
    });
}

void Linux::Base::EncToolsSwQMatrix::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_PatchDDITask
        , [this](StorageW& global, StorageW& task) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        bool bETSpsACQM = IsSwEncToolsSpsACQM(par);
        bool bETPpsACQM = IsSwEncToolsPpsACQM(par);
        MFX_CHECK(bETSpsACQM || bETPpsACQM, MFX_ERR_NONE);
        auto& sps = Glob::SPS::Get(global);
        MFX_CHECK(sps.scaling_list_enabled_flag, MFX_ERR_NONE);
        if (sps.scaling_list_data_present_flag) {
            auto& par    = Glob::DDI_SubmitParam::Get(global);
            auto itPPS    = std::find_if(std::begin(par), std::end(par)
                , [](DDIExecParam& ep) {return (ep.Function == VAEncPictureParameterBufferType);});
            MFX_CHECK(itPPS != std::end(par) && itPPS->In.pData, MFX_ERR_UNKNOWN);

            auto& pps = *(VAEncPictureParameterBufferHEVC *)itPPS->In.pData;
            pps.pic_fields.bits.scaling_list_data_present_flag = (sps.scaling_list_enabled_flag && sps.scaling_list_data_present_flag);

            auto itQMatrix = std::find_if(std::begin(par), std::end(par)
                , [](DDIExecParam& ep) {return (ep.Function == VAQMatrixBufferType);});
            MFX_CHECK(itQMatrix != std::end(par) && itQMatrix->In.pData, MFX_ERR_UNKNOWN);

            auto& qMatrix = *(VAQMatrixBufferHEVC *)itQMatrix->In.pData;

            for (mfxU8 j = 0; j < 2; j++)
            {
                for (mfxU8 i = 0; i < 3; i++)
                {
                    MakeUpRight(sps.scl.scalingLists0[3 * j + i], 4, qMatrix.scaling_lists_4x4[i][j]);
                    MakeUpRight(sps.scl.scalingLists1[3 * j + i], 8, qMatrix.scaling_lists_8x8[i][j]);
                    MakeUpRight(sps.scl.scalingLists2[3 * j + i], 8, qMatrix.scaling_lists_16x16[i][j]);
                    qMatrix.scaling_list_dc_16x16[i][j] = sps.scl.scalingListDCCoefSizeID2[3 * j + i];
                }
            }

            MakeUpRight(sps.scl.scalingLists3[0], 8, qMatrix.scaling_lists_32x32[0]);
            qMatrix.scaling_list_dc_32x32[0] = sps.scl.scalingListDCCoefSizeID3[0];

            MakeUpRight(sps.scl.scalingLists3[1], 8, qMatrix.scaling_lists_32x32[1]);
            qMatrix.scaling_list_dc_32x32[1] = sps.scl.scalingListDCCoefSizeID3[1];
        }

        if(bETPpsACQM)
        {
            MFX_CHECK(task.Contains(Task::SSH::Key), MFX_ERR_NONE);
            auto& slice = Task::SSH::Get(task);
            mfxU8 idx = slice.pic_parameter_set_id;

            auto& par    = Glob::DDI_SubmitParam::Get(global);
            auto itPPS    = std::find_if(std::begin(par), std::end(par)
                , [](DDIExecParam& ep) {return (ep.Function == VAEncPictureParameterBufferType);});
            MFX_CHECK(itPPS != std::end(par) && itPPS->In.pData, MFX_ERR_UNKNOWN);

            auto& pps = *(VAEncPictureParameterBufferHEVC *)itPPS->In.pData;
            pps.pic_fields.bits.scaling_list_data_present_flag = pps.pic_fields.bits.scaling_list_data_present_flag || (sps.scaling_list_enabled_flag && IsCustETMatrix(idx));

            MFX_CHECK(IsCustETMatrix(idx), MFX_ERR_NONE);
            auto itQMatrix = std::find_if(std::begin(par), std::end(par)
                , [](DDIExecParam& ep) {return (ep.Function == VAQMatrixBufferType);});
            MFX_CHECK(itQMatrix != std::end(par) && itQMatrix->In.pData, MFX_ERR_UNKNOWN);

            auto& qMatrix = *(VAQMatrixBufferHEVC *)itQMatrix->In.pData;

            MFX_CHECK(global.Contains(Glob::CqmPPS::Key), MFX_ERR_NONE);
            auto& cqmPPS = Glob::CqmPPS::Get(global);
            if (IsCustETMatrix(idx) && cqmPPS.size() >= idx)
            {
                 for (mfxU8 j = 0; j < 2; j++)
                {
                    for (mfxU8 i = 0; i < 3; i++)
                    {
                        MakeUpRight(cqmPPS[idx - 1].sld.scalingLists0[3 * j + i], 4, qMatrix.scaling_lists_4x4[i][j]);
                        MakeUpRight(cqmPPS[idx - 1].sld.scalingLists1[3 * j + i], 8, qMatrix.scaling_lists_8x8[i][j]);
                        MakeUpRight(cqmPPS[idx - 1].sld.scalingLists2[3 * j + i], 8, qMatrix.scaling_lists_16x16[i][j]);
                        qMatrix.scaling_list_dc_16x16[i][j] = cqmPPS[idx - 1].sld.scalingListDCCoefSizeID2[3 * j + i];
                    }
                }

                MakeUpRight(cqmPPS[idx - 1].sld.scalingLists3[0], 8, qMatrix.scaling_lists_32x32[0]);
                qMatrix.scaling_list_dc_32x32[0] = cqmPPS[idx - 1].sld.scalingListDCCoefSizeID3[0];
                MakeUpRight(cqmPPS[idx - 1].sld.scalingLists3[1], 8, qMatrix.scaling_lists_32x32[1]);
                qMatrix.scaling_list_dc_32x32[1] = cqmPPS[idx - 1].sld.scalingListDCCoefSizeID3[1];
            }
        }
        return MFX_ERR_NONE;
    });
}

#endif // defined(MFX_ENABLE_ENCTOOLS_SW)
#endif // defined(MFX_ENABLE_H265_VIDEO_ENCODE)
