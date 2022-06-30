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

#include "av1ehw_base_data.h"
#include "av1ehw_xe_hpm_segmentation.h"

namespace AV1EHW
{
namespace Xe_HPM
{

static mfxU8 FindUnusedSegmentId(const mfxExtAV1Segmentation& segPar)
{
    assert(segPar.SegmentIds);
    assert(segPar.NumSegmentIdAlloc);

    std::array<bool, Base::AV1_MAX_NUM_OF_SEGMENTS> usedIDs = {};

    for (mfxU32 i = 0; i < segPar.NumSegmentIdAlloc; i++)
        usedIDs.at(segPar.SegmentIds[i]) = true;

    auto unusedId = std::find(usedIDs.begin(), usedIDs.end(), false);

    return static_cast<mfxU8>(std::distance(usedIDs.begin(), unusedId));
}

void Segmentation::InitTask(const FeatureBlocks& blocks, TPushIT Push)
{
    Base::Segmentation::InitTask(blocks, Push);
    Push(BLK_PatchSegmentParam
        , [this, &blocks](
            mfxEncodeCtrl* /*pCtrl*/
            , mfxFrameSurface1* /*pSurf*/
            , mfxBitstream* /*pBs*/
            , StorageW& global
            , StorageW& task) -> mfxStatus
    {
        mfxExtAV1Segmentation& segPar = Base::Task::Segment::Get(task);
        MFX_CHECK(IsSegmentationEnabled(&segPar), MFX_ERR_NONE);

        /* "PatchSegmentParam" does 2 workarounds
            1) Sets SEG_LVL_SKIP feature for one of segments to force "SegIdPreSkip" to 1.
               SegIdPreSkip" should be set to 1 to WA HW issue.
               So far this WA is always applied. The condition could be revised/narrowed in the future.
            2) Disables temporal_update (HW limitation). */

        const mfxU8 id = FindUnusedSegmentId(segPar);

        if (id >= Base::AV1_MAX_NUM_OF_SEGMENTS)
        {
            // All segment_ids 0-7 are present in segmentation map
            // WA cannot be applied
            // Don't apply segmentation for current frame and return warning
            segPar = mfxExtAV1Segmentation{};

            MFX_LOG_WARN("Can't apply SegIdPreSkip WA - no unused surfaces in seg map!\n");

            return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        EnableFeature(segPar.Segment[id].FeatureEnabled, Base::SEG_LVL_SKIP);
        if (segPar.NumSegments < id + 1)
            segPar.NumSegments = id + 1;

        auto& par = Base::Glob::VideoParam::Get(global);
        mfxExtAV1AuxData& auxPar = ExtBuffer::Get(par);
        auxPar.SegmentTemporalUpdate = MFX_CODINGOPTION_OFF;
        return MFX_ERR_NONE;
    });
}

inline bool IsSegBlockSmallerThanSB(mfxU32 segBlock, mfxU32 sb)
{
    return sb == 128 ||
        (sb == 64 && segBlock < MFX_AV1_SEGMENT_ID_BLOCK_SIZE_64x64);
}

void Segmentation::PostReorderTask(const FeatureBlocks& blocks, TPushPostRT Push)
{
    Base::Segmentation::PostReorderTask(blocks, Push);

    Push(BLK_PatchTask
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        Base::FH& fh = Base::Task::FH::Get(s_task);
        mfxExtAV1Segmentation& segPar = Base::Task::Segment::Get(s_task);

        if (IsSegmentationSwitchedOff(&segPar))
            return MFX_ERR_NONE;

        if (!fh.segmentation_params.segmentation_update_map)
        {
            // in Xe_HPM "segmentation_update_map = 0" mode is implemented by forcing segmentation through VDEnc Stream in
            // there are following related restrictions/requirements:
            // 1) segmentation map should be always sent to the driver explicitly (even if there is no map update)
            // 2) "segmentation_update_map = 0" cannot be safely applied for SegmentIdBlockSize smaller than SB size

            const Base::SH& sh = Base::Glob::SH::Get(global);

            auto &segDpb = Base::Glob::SegDpb::Get(global);
            assert(!segDpb.empty());

            const mfxU8 primaryRefFrameDpbIdx = static_cast<mfxU8>(fh.ref_frame_idx[fh.primary_ref_frame]);
            const mfxExtAV1Segmentation* pRefPar = segDpb.at(primaryRefFrameDpbIdx).get();
            assert(pRefPar);

            segPar.SegmentIds = pRefPar->SegmentIds;
            segPar.NumSegmentIdAlloc = pRefPar->NumSegmentIdAlloc;

            if (IsSegBlockSmallerThanSB(segPar.SegmentIdBlockSize, sh.sbSize))
            {
                // force map update if SegmentIdBlockSize is smaller than SB size
                fh.segmentation_params.segmentation_update_map = 1;
            }
        }

        return MFX_ERR_NONE;
    });
}

} //namespace Xe_HPM
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
