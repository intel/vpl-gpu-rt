// Copyright (c) 2023-2024 Intel Corporation
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
#include "av1ehw_base_interpo_filter.h"

using namespace AV1EHW;

namespace AV1EHW
{
namespace Base
{
static inline void UpdateInterpFilterForNonRAB(mfxU16 width, mfxU8 QP, Base::FH& currFH)
{
    if (width <= 500 && QP <= 220)
    {
        currFH.interpolation_filter = EIGHTTAP_SHARP;
    }
}

static inline void UpdateInterpFilterForBPyramid(mfxU16 width, mfxU8 QP, mfxU16 GopRefDist, Base::FH& currFH)
{
    if ((currFH.order_hint % (GopRefDist / 2)) == 0)
    {
        if (width <= 900 && QP <= 255)
        {
            currFH.interpolation_filter = EIGHTTAP_SHARP;
        }
    }
    else
    {
        if (width <= 2000 && QP <= 255)
        {
            currFH.interpolation_filter = EIGHTTAP_SHARP;
        }
    }
}

static mfxStatus PostUpdateInterpFilter(
    const mfxVideoParam& par
    , mfxU8 QP
    , Base::FH& currFH)
{
    if (currFH.frame_type != KEY_FRAME)
    {
        const mfxExtCodingOption2& CO2 = ExtBuffer::Get(par);
        const mfxU16 Width = par.mfx.FrameInfo.CropW;
        const mfxU16 GopRefDist = par.mfx.GopRefDist;
        bool haveRAB = HaveRABFrames(par);
        bool isBPyramid = (CO2.BRefType == MFX_B_REF_PYRAMID) ? true : false;

        if (!haveRAB)
        {
            // for no RAB Gop.
            UpdateInterpFilterForNonRAB(Width, QP, currFH);
        }
        else if (isBPyramid && GopRefDist == 8)
        {
            // for RAB BPyramid
            UpdateInterpFilterForBPyramid(Width, QP, GopRefDist, currFH);
        }
    }

    return MFX_ERR_NONE;
}

void InterpoFilter::PostReorderTask(const FeatureBlocks& /*blocks*/, TPushPostRT Push)
{
    Push(BLK_ConfigureTask
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
        {
            const auto             &par    = GetRTDefaults(global).mvp;
            const mfxExtAV1AuxData &auxPar = ExtBuffer::Get(par);

            if (auxPar.InterpFilter == MFX_AV1_INTERP_DEFAULT)
            {
                return PostUpdateInterpFilter(par, Task::Common::Get(s_task).QpY, Task::FH::Get(s_task));
            }

            return MFX_ERR_NONE;
        });
}

} //namespace Base
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
