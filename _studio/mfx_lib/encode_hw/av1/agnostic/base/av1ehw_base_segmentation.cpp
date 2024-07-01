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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base_general.h"
#include "av1ehw_base_segmentation.h"

namespace AV1EHW
{

namespace Base
{

template<typename T>
inline bool CheckAndFixFeature(
    const ENCODE_CAPS_AV1& caps
    , mfxU16& featureEnabled
    , T& value
    , SEG_LVL_FEATURES feature)
{
    bool changed = false;

    if (IsFeatureEnabled(featureEnabled, feature) && !IsFeatureSupported(caps, feature))
    {
        DisableFeature(featureEnabled, feature);
        changed = true;
    }

    if (value && !IsFeatureEnabled(featureEnabled, feature))
    {
        value = 0;
        changed = true;
    }

    mfxI32 limit = SEGMENTATION_FEATURE_MAX[feature];
    if (SEGMENTATION_FEATURE_SIGNED[feature])
        changed |= CheckRangeOrClip(value, -limit, limit);
    else
        changed |= CheckRangeOrClip(value, 0, limit);

    return changed;
}

mfxU32 CheckAndFixSegmentParam(
    const ENCODE_CAPS_AV1& caps
    , mfxAV1SegmentParam& seg)
{
    mfxU32 changed = 0;

    changed += CheckAndFixFeature(caps, seg.FeatureEnabled, seg.AltQIndex, SEG_LVL_ALT_Q);

    return changed;
}

inline bool CheckAndZeroSegmentParam(
    mfxAV1SegmentParam& seg)
{
    bool changed = false;

    changed |= seg.FeatureEnabled != 0;
    changed |= seg.AltQIndex != 0;

    if (changed)
        seg = {};

    return changed;
}

static mfxU32 CheckSegmentMap(
    const mfxU32 width
    , const mfxU32 height
    , mfxExtAV1Segmentation& segPar)
{
    mfxU32 invalid = 0;

    const mfxU32 blockSize      = segPar.SegmentIdBlockSize > 0 ? segPar.SegmentIdBlockSize : MFX_AV1_SEGMENT_ID_BLOCK_SIZE_32x32;
    const mfxU32 widthInBlocks  = mfx::CeilDiv(width, blockSize);
    const mfxU32 heightInBlocks = mfx::CeilDiv(height, blockSize);

    if (segPar.NumSegmentIdAlloc && segPar.NumSegmentIdAlloc < widthInBlocks * heightInBlocks)
        invalid++;

    if (segPar.SegmentIds && segPar.NumSegments)
    {
        for (mfxU32 i = 0; i < segPar.NumSegmentIdAlloc; ++i)
        {
            if (segPar.SegmentIds[i] >= segPar.NumSegments)
            {
                invalid++;
                break;
            }
        }
    }

    return invalid;
}

static mfxU8 GetLastActiveSegId(const mfxAV1SegmentParam (&segParams)[AV1_MAX_NUM_OF_SEGMENTS])
{
    // the logic is from AV1 spec 5.9.14

    mfxU8 LastActiveSegId = 0;
    for (mfxU8 i = 0; i < AV1_MAX_NUM_OF_SEGMENTS; i++)
        if (segParams[i].FeatureEnabled)
            LastActiveSegId = i;

    return LastActiveSegId;
}

static mfxU32 CheckNumSegments(mfxExtAV1Segmentation& segPar)
{
    mfxU32 invalid = 0;

    invalid += CheckMaxOrZero(segPar.NumSegments, AV1_MAX_NUM_OF_SEGMENTS);
    // By AV1 spec 6.10.8 segment_id must be in the range 0 to LastActiveSegId. Following check helps to ensure it.
    invalid += CheckMaxOrZero(segPar.NumSegments, GetLastActiveSegId(segPar.Segment) + 1);

    return invalid;
}

//this function not needed for Silicon
mfxU32 CheckHWLimitations(mfxExtAV1Segmentation& segPar)
{
    mfxU32 invalid = 0;

    if (segPar.NumSegments && segPar.NumSegments <= AV1_MAX_NUM_OF_SEGMENTS)
    {
        const mfxAV1SegmentParam lastSeg = segPar.Segment[segPar.NumSegments - 1];

        if (IsFeatureEnabled(lastSeg.FeatureEnabled, SEG_LVL_ALT_Q) && !lastSeg.AltQIndex)
        {
            // last segment with zero delta QP isn't supported by HW
            invalid++;
        }
    }

    return invalid;
}

inline mfxU32 CheckSegFrameQP(const mfxU16& qp, mfxExtAV1Segmentation& segPar)
{
    mfxU32 invalid = 0;
    mfxI32 tempQpForSegs = 0;

    for (mfxU8 i = 0; i < AV1_MAX_NUM_OF_SEGMENTS; i++)
    {
        tempQpForSegs = qp + segPar.Segment[i].AltQIndex;

        // HW restriction: QpForSegs cannot be negative, and when QpForSegs =0 and segDeltaQ < 0 should be treated as error
        if (tempQpForSegs < 0 || (tempQpForSegs == 0 && segPar.Segment[i].AltQIndex < 0))
        {
            segPar.Segment[i].AltQIndex = 0;
            invalid += 1;
        }
    }

    return invalid;
}

inline void GetGopPattern(const mfxVideoParam& par, bool& bIOnly, bool& bIPOnly, bool& bIPB)
{
    bIOnly  = par.mfx.GopPicSize == 1;
    bIPOnly = !bIOnly && par.mfx.GopRefDist <= 1;
    bIPB    = !bIOnly && !bIPOnly && par.mfx.GopRefDist > 1;
}

inline mfxU32 CheckSegQP(const mfxVideoParam& par, mfxExtAV1Segmentation& segPar)
{
    mfxU32 invalid = 0;
    bool bIOnly    = false;
    bool bIPOnly   = false;
    bool bIPB      = false;

    GetGopPattern(par, bIOnly, bIPOnly, bIPB);

    if (bIOnly)
        invalid += CheckSegFrameQP(par.mfx.QPI, segPar);
    else if (bIPOnly)
    {
        invalid += CheckSegFrameQP(par.mfx.QPI, segPar);
        invalid += CheckSegFrameQP(par.mfx.QPP, segPar);
    }
    else if (bIPB)
    {
        invalid += CheckSegFrameQP(par.mfx.QPI, segPar);
        invalid += CheckSegFrameQP(par.mfx.QPP, segPar);
        invalid += CheckSegFrameQP(par.mfx.QPB, segPar);
    }
    return invalid;
}

inline mfxU32 CheckSegDeltaForLossless(const mfxVideoParam& par, mfxExtAV1Segmentation& segPar)
{
    bool bIOnly    = false;
    bool bIPOnly   = false;
    bool bIPB      = false;

    GetGopPattern(par, bIOnly, bIPOnly, bIPB);

    const mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);

    bool bZeroFrameDelta = !pAuxPar || (pAuxPar->QP.UAcDeltaQ == 0 && pAuxPar->QP.UDcDeltaQ == 0
        && pAuxPar->QP.VAcDeltaQ == 0 && pAuxPar->QP.VDcDeltaQ == 0 && pAuxPar->QP.YDcDeltaQ == 0);

    bool bLossI  = par.mfx.QPI == 0 && bZeroFrameDelta;
    bool bLossP  = par.mfx.QPP == 0 && bZeroFrameDelta && (bIPOnly || bIPB);
    bool bLossB  = par.mfx.QPB == 0 && bZeroFrameDelta && bIPB;

    mfxU32 invalid = 0;
    MFX_CHECK((bLossI || bLossP || bLossB), invalid);

    for (mfxU8 i = 0; i < AV1_MAX_NUM_OF_SEGMENTS; ++i)
    {
        if (segPar.Segment[i].AltQIndex != 0)
        {
            segPar.Segment[i].AltQIndex = 0;
            invalid += 1;
        }
    }
  
    return invalid;
}

mfxStatus Segmentation::UpdateSegmentBuffers(mfxExtAV1Segmentation& segPar, const mfxExtAV1Segmentation* extSegPar)
{
    MFX_CHECK(IsSegmentationEnabled(extSegPar), MFX_ERR_NONE);

    segPar.NumSegments        = extSegPar->NumSegments;
    segPar.NumSegmentIdAlloc  = extSegPar->NumSegmentIdAlloc;
    segPar.SegmentIdBlockSize = extSegPar->SegmentIdBlockSize;
    segPar.SegmentIds         = extSegPar->SegmentIds;

    for (mfxU8 i = 0; i < AV1_MAX_NUM_OF_SEGMENTS; i++)
    {
        segPar.Segment[i].FeatureEnabled = extSegPar->Segment[i].FeatureEnabled;
        segPar.Segment[i].AltQIndex      = extSegPar->Segment[i].AltQIndex;
    }

    return MFX_ERR_NONE;
}

mfxStatus Segmentation::CheckAndFixSegmentBuffers(mfxExtAV1Segmentation* pSegPar, const Defaults::Param& defPar)
{
    const mfxVideoParam& par    = defPar.mvp;
    const ENCODE_CAPS_AV1& caps = defPar.caps;

    MFX_CHECK(IsSegmentationEnabled(pSegPar), MFX_ERR_NONE);

    mfxU32 invalid = 0, changed = 0;

    invalid += !!!caps.ForcedSegmentationSupport;

    // Further parameter check hardly rely on NumSegments. Cannot fix value of NumSegments and then use
    // modified value for checks. Need to drop and return MFX_ERR_UNSUPPORTED
    invalid += CheckNumSegments(*pSegPar);

    mfxU32 width = 0, height = 0;
    const mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);

    std::tie(width, height) = GetRealResolution(defPar.mvp);
    width = GetActualEncodeWidth(width, pAuxPar);

    invalid += CheckSegmentMap(width, height, *pSegPar);

    mfxU16 rateControlMethod      = defPar.base.GetRateControlMethod(defPar);
    invalid += !!(rateControlMethod == MFX_RATECONTROL_CQP && CheckSegQP(par, *pSegPar));
    invalid += !!(rateControlMethod == MFX_RATECONTROL_CQP && CheckSegDeltaForLossless(par, *pSegPar));

    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    for (mfxU16 i = 0; i < pSegPar->NumSegments; i++)
    {
        changed += CheckAndFixSegmentParam(caps, pSegPar->Segment[i]);
    }

    // clean out per-segment parameters for segments with numbers exceeding seg.NumSegments
    for (mfxU16 i = pSegPar->NumSegments; i < AV1_MAX_NUM_OF_SEGMENTS; ++i)
    {
        changed += CheckAndZeroSegmentParam(pSegPar->Segment[i]);
    }

    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);
    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

void Segmentation::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_AV1_SEGMENTATION].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtAV1Segmentation*)pSrc;
        auto& buf_dst = *(mfxExtAV1Segmentation*)pDst;

        MFX_COPY_FIELD(NumSegments);

        for (mfxU32 i = 0; i < buf_src.NumSegments; i++)
        {
            MFX_COPY_FIELD(Segment[i].FeatureEnabled);
            MFX_COPY_FIELD(Segment[i].AltQIndex);
        }

        MFX_COPY_FIELD(SegmentIdBlockSize);
        MFX_COPY_FIELD(NumSegmentIdAlloc);
        MFX_COPY_FIELD(SegmentIds);
    });
}

void Segmentation::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [this](mfxVideoParam& par, StorageW&, StorageRW&)
    {
        mfxExtAV1Segmentation* pSegPar = ExtBuffer::Get(par);
        if (pSegPar)
        {
            SetDefault(pSegPar->SegmentIdBlockSize, MFX_AV1_SEGMENT_ID_BLOCK_SIZE_32x32);
        }
    });
}

void Segmentation::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetSegDpb
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        strg.Insert(Glob::SegDpb::Key, new MakeStorable<Glob::SegDpb::TRef>);
        return MFX_ERR_NONE;
    });
}

void Segmentation::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [this](const mfxVideoParam& /*in*/, mfxVideoParam& out, StorageW& global) -> mfxStatus
    {
        const auto& caps               = Glob::EncodeCaps::Get(global);
        const auto& defchain           = Glob::Defaults::Get(global);
        const Defaults::Param& defPar  = Defaults::Param(out, caps, defchain);
        mfxExtAV1Segmentation* pSegPar = ExtBuffer::Get(out);

        return CheckAndFixSegmentBuffers(pSegPar, defPar);
    });
}

void Segmentation::AllocTask(const FeatureBlocks& blocks, TPushAT Push)
{
    Push(BLK_AllocTask
        , [this, &blocks](
            StorageR& /*global*/
            , StorageRW& task) -> mfxStatus
        {
            task.Insert(Task::Segment::Key, new MakeStorable<Task::Segment::TRef>);
            return MFX_ERR_NONE;
        });
}

static void MergeSegParam(
    const mfxExtAV1Segmentation& initSegPar
    , const mfxExtAV1Segmentation& frameSegPar
    , mfxExtAV1Segmentation& mergedSegPar)
{
    assert(frameSegPar.NumSegments); // we assume that MergeSegParam is called for active per-frame segment configuration

    // (1) take Init ext buffer
    mergedSegPar = initSegPar;

    // (2) switch to Frame per-segment param
    mergedSegPar.NumSegments = frameSegPar.NumSegments;
    const auto emptyPar = mfxAV1SegmentParam{};
    std::fill_n(mergedSegPar.Segment, AV1_MAX_NUM_OF_SEGMENTS, emptyPar);
    std::copy_n(frameSegPar.Segment, frameSegPar.NumSegments, mergedSegPar.Segment);

    // (3) switch to Frame segment map (if provided)
    if (frameSegPar.SegmentIds)
    {
        mergedSegPar.NumSegmentIdAlloc = frameSegPar.NumSegmentIdAlloc;
        mergedSegPar.SegmentIds = frameSegPar.SegmentIds;
        mergedSegPar.SegmentIdBlockSize = frameSegPar.SegmentIdBlockSize;
    }
}

inline bool CheckForCompleteness(const mfxExtAV1Segmentation& segPar)
{
    return segPar.NumSegments && segPar.SegmentIds && segPar.NumSegmentIdAlloc && segPar.SegmentIdBlockSize;
}

static mfxStatus SetFinalSegParam(
    mfxExtAV1Segmentation& finalSegPar
    , const Defaults::Param& defPar
    , const mfxExtAV1Segmentation& initSegPar
    , const mfxExtAV1Segmentation* pFrameSegPar)
{
    const auto emptySeg = mfxExtAV1Segmentation{};

    if (IsSegmentationSwitchedOff(pFrameSegPar))
    {
        MFX_LOG_INFO("Segmentation was switched off for current frame!\n");
        finalSegPar = emptySeg;
        return MFX_ERR_NONE;
    }

    if (pFrameSegPar)
    {
        MergeSegParam(initSegPar, *pFrameSegPar, finalSegPar);

        const mfxStatus checkSts = Segmentation::CheckAndFixSegmentBuffers(&finalSegPar, defPar);
        if (checkSts < MFX_ERR_NONE)
        {
            MFX_LOG_WARN("Merge of Init and Frame mfxExtAV1Segmentation resulted in erroneous configuration!\n");
            finalSegPar = emptySeg;
            return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }
    else
        finalSegPar = initSegPar;

    if (!CheckForCompleteness(finalSegPar))
    {
        MFX_LOG_WARN("Merge of Init and Frame mfxExtAV1Segmentation didn't give complete segment params!\n");
        finalSegPar = emptySeg;
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

void Segmentation::InitTask(const FeatureBlocks& blocks, TPushIT Push)
{
    Push(BLK_InitTask
        , [this, &blocks](
            mfxEncodeCtrl* /*pCtrl*/
            , mfxFrameSurface1* /*pSurf*/
            , mfxBitstream* /*pBs*/
            , StorageW& global
            , StorageW& task) -> mfxStatus
        {
            const auto&                  par         = Glob::VideoParam::Get(global);
            const auto&                  caps        = Glob::EncodeCaps::Get(global);
            const auto&                  defchain    = Glob::Defaults::Get(global);
            const Defaults::Param&       defPar      = Defaults::Param(par, caps, defchain);
            const mfxExtAV1Segmentation& initSegPar  = ExtBuffer::Get(par);
            mfxExtAV1Segmentation&       finalSegPar = Task::Segment::Get(task);

            if (!IsSegmentationEnabled(&initSegPar))
            {
                finalSegPar = {};
                return MFX_ERR_NONE;
            }

            mfxStatus              checkSts  = MFX_ERR_NONE;

            mfxExtAV1Segmentation* pFrameSegPar = ExtBuffer::Get(Task::Common::Get(task).ctrl);

            if (IsSegmentationEnabled(pFrameSegPar))
            {
                checkSts = CheckAndFixSegmentBuffers(pFrameSegPar, defPar);
                if (checkSts < MFX_ERR_NONE)
                {
                    // ignore Frame segment param and return warning if there are issues MSDK can't fix
                    pFrameSegPar = nullptr;
                    checkSts     = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                    MFX_LOG_WARN("Critical issue in Frame mfxExtAV1Segmentation - it's ignored!\n");
                }
            }

            const mfxStatus mergeSts = SetFinalSegParam(finalSegPar, defPar, initSegPar, pFrameSegPar);

            return checkSts == MFX_ERR_NONE ? mergeSts : checkSts;
        });
}

inline std::tuple<bool, bool> NeedMapUpdate(
    const mfxExtAV1Segmentation& curSegPar
    , const mfxExtAV1AuxData& auxData
    , const mfxExtAV1Segmentation& refSegPar)
{
    bool updateMap = true;
    bool temporalUpdate = false;

    if (!IsSegmentationSwitchedOff(&refSegPar))
    {
        if (curSegPar.SegmentIdBlockSize == refSegPar.SegmentIdBlockSize
            && curSegPar.NumSegments == refSegPar.NumSegments)
        {
            assert(curSegPar.SegmentIds);
            assert(refSegPar.SegmentIds);

            const mfxU32 mapSize = std::min(curSegPar.NumSegmentIdAlloc, refSegPar.NumSegmentIdAlloc);

            if (std::equal(curSegPar.SegmentIds, curSegPar.SegmentIds + mapSize, refSegPar.SegmentIds))
                updateMap = false;
        }

        // default value of TemporalUpdate (MFX_CODINGOPTION_UNKNOWN) is treated as ON
        // (we assume that in average enabled segmentation_temporal_update saves bits in comparison with disabled)
        temporalUpdate = updateMap && !IsOff(auxData.SegmentTemporalUpdate);
    }

    return std::make_tuple(updateMap, temporalUpdate);
}

inline bool NeedParUpdate(const mfxExtAV1Segmentation& curSegPar, const mfxExtAV1Segmentation& refSegPar)
{
    if (curSegPar.NumSegments != refSegPar.NumSegments)
        return true;

    auto EqualParam = [](const mfxAV1SegmentParam& left, const mfxAV1SegmentParam& right)
    {
        return left.FeatureEnabled  == right.FeatureEnabled
            && left.AltQIndex       == right.AltQIndex;
    };

    if (std::equal(curSegPar.Segment, curSegPar.Segment + curSegPar.NumSegments, refSegPar.Segment, EqualParam))
        return false;
    else
        return true;
}

mfxStatus UpdateFrameHeader(
    const mfxExtAV1Segmentation& curSegPar
    , const mfxExtAV1AuxData& auxData
    , const FH& fh
    , SegmentationParams& seg
    , SegDpbType& segDpb)
{
    seg = SegmentationParams{};

    MFX_CHECK(curSegPar.NumSegments, MFX_ERR_NONE);

    seg.segmentation_enabled = 1;

    if (fh.primary_ref_frame == PRIMARY_REF_NONE) // AV1 spec 5.9.14
    {
        seg.segmentation_update_map = 1;
        seg.segmentation_temporal_update = 0;
        seg.segmentation_update_data = 1;
    }
    else
    {
        const mfxU8 primaryRefFrameDpbIdx    = static_cast<mfxU8>(fh.ref_frame_idx[fh.primary_ref_frame]);
        const mfxExtAV1Segmentation* pRefPar = segDpb.at(primaryRefFrameDpbIdx).get();
        assert(pRefPar);
        std::tie(seg.segmentation_update_map, seg.segmentation_temporal_update) =
            NeedMapUpdate(curSegPar, auxData, *pRefPar);
        seg.segmentation_update_data = NeedParUpdate(curSegPar, *pRefPar);
    }

    for (mfxU8 i = 0; i < AV1_MAX_NUM_OF_SEGMENTS; ++i)
    {
        seg.FeatureMask[i]                = static_cast<uint32_t>(curSegPar.Segment[i].FeatureEnabled);
        seg.FeatureData[i][SEG_LVL_ALT_Q] = curSegPar.Segment[i].AltQIndex;

        if (IsFeatureEnabled(curSegPar.Segment[i].FeatureEnabled, SEG_LVL_SKIP))
            seg.FeatureData[i][SEG_LVL_SKIP]     = 1;
        if (IsFeatureEnabled(curSegPar.Segment[i].FeatureEnabled, SEG_LVL_GLOBALMV))
            seg.FeatureData[i][SEG_LVL_GLOBALMV] = 1;
    }

    return MFX_ERR_NONE;
}

static void RetainSegMap(mfxExtAV1Segmentation& segPar)
{
    const mfxU8* pMap = segPar.SegmentIds;
    segPar.SegmentIds = new mfxU8[segPar.NumSegmentIdAlloc];
    std::copy_n(pMap, segPar.NumSegmentIdAlloc, segPar.SegmentIds);
}

static void PutSegParamToDPB(
    const mfxExtAV1Segmentation& segPar
    , mfxU16 numRefFrame
    , const DpbRefreshType& refreshFrameFlags
    , SegDpbType& segDpb)
{
    if (segDpb.empty())
        segDpb.resize(numRefFrame);

    auto SegParamReleaser = [](mfxExtAV1Segmentation* pSegPar)
    {
        delete []pSegPar->SegmentIds;
        delete pSegPar;
    };

    UpdateDPB(segDpb, segPar, refreshFrameFlags, SegParamReleaser);
}

inline void UpdateSegmentLossless(mfxExtAV1Segmentation& segPar, FH& fh)
{
    if (segPar.NumSegments && fh.CodedLossless)
    {
        memset(&segPar.Segment[0], 0, AV1_MAX_NUM_OF_SEGMENTS * sizeof(mfxAV1SegmentParam));
        fh.segmentation_params.segmentation_enabled = 0;
    }
}

mfxStatus Segmentation::PostUpdateSegmentParam(
    mfxExtAV1Segmentation& segPar
    , FH& fh
    , SegDpbType& segDpb
    , const mfxExtAV1AuxData& auxData
    , const mfxU16 numRefFrame
    , const DpbRefreshType refreshFrameFlags)
{
    auto sts = UpdateFrameHeader(segPar, auxData, fh, fh.segmentation_params, segDpb);
    MFX_CHECK_STS(sts);

    UpdateSegmentLossless(segPar, fh);

    if (fh.refresh_frame_flags)
    {
        RetainSegMap(segPar);

        PutSegParamToDPB(segPar, numRefFrame, refreshFrameFlags, segDpb);
    }

    if (!fh.segmentation_params.segmentation_update_map)
    {
        segPar.SegmentIds        = nullptr;
        segPar.NumSegmentIdAlloc = 0;
    }

    return MFX_ERR_NONE;
}

void Segmentation::PostReorderTask(const FeatureBlocks& /*blocks*/, TPushPostRT Push)
{
    Push(BLK_ConfigureTask
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        mfxExtAV1Segmentation& segPar   = Task::Segment::Get(s_task);
        const auto& vp                  = Glob::VideoParam::Get(global);
        const mfxExtAV1AuxData& auxData = ExtBuffer::Get(vp);
        FH& fh                          = Task::FH::Get(s_task);
        auto& segDpb                    =  Glob::SegDpb::Get(global);
        const auto& numRefFrame         = Glob::VideoParam::Get(global).mfx.NumRefFrame;
        const auto& refreshFrameFlags   = Task::Common::Get(s_task).RefreshFrameFlags;

        return PostUpdateSegmentParam(segPar, fh, segDpb, auxData, numRefFrame, refreshFrameFlags);
    });
}

} //namespace AV1EHW

}
#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
