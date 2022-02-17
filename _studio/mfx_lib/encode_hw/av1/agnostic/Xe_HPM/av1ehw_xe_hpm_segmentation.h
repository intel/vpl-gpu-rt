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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base_segmentation.h"

namespace AV1EHW
{
namespace Xe_HPM
{
    class Segmentation
        : public Base::Segmentation
    {
    public:
#define BLOCK_ID_OFFSET Base::Segmentation::eBlockId::NUM_BLOCKS
#define DECL_BLOCK_LIST\
        DECL_BLOCK(PatchSegmentParam)\
        DECL_BLOCK(PatchTask)
#define DECL_FEATURE_NAME "Xe_HPM_Segmentation"
#include "av1ehw_decl_blocks.h"

        Segmentation(mfxU32 FeatureId)
            : Base::Segmentation(FeatureId)
        {
#if defined(MFX_ENABLE_LOG_UTILITY)
            m_trace.second.insert(Base::Segmentation::m_trace.second.begin(), Base::Segmentation::m_trace.second.end());
#endif
        }

    protected:
        virtual void InitTask(const FeatureBlocks& blocks, TPushIT Push) override;
        virtual void PostReorderTask(const FeatureBlocks& blocks, TPushPostRT Push) override;
    };

} //Xe_HPM
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)