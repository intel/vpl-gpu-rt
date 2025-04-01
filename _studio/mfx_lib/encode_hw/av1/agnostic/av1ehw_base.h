// Copyright (c) 2019-2021 Intel Corporation
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

#include "mfxvideo.h"

#include "mfxbrc.h"
#include "mfxav1.h"
#include "mfx_ext_buffers.h"

#include "mfxvideo++int.h"

#include "mfx_utils_logging.h"
#include "mfx_utils_extbuf.h"

#include "feature_blocks/mfx_feature_blocks_init_macros.h"
#include "feature_blocks/mfx_feature_blocks_base.h"
#include "av1ehw_utils.h"
#include "av1eimplbase.h"

namespace AV1EHW
{
using namespace MfxFeatureBlocks;

namespace ExtBuffer
{
    using namespace MfxExtBuffer;
}

struct FeatureBlocks
    : FeatureBlocksCommon<BlockTracer>
{
MFX_FEATURE_BLOCKS_DECLARE_BQ_UTILS_IN_FEATURE_BLOCK
#define DEF_BLOCK_Q MFX_FEATURE_BLOCKS_DECLARE_QUEUES_IN_FEATURE_BLOCK
    #include "av1ehw_block_queues.h"
#undef DEF_BLOCK_Q

    virtual const char* GetFeatureName(mfxU32 featureID) override;
    virtual const char* GetBlockName(ID id) override;

    std::map<mfxU32, const BlockTracer::TFeatureTrace*> m_trace;
};

#define DEF_BLOCK_Q MFX_FEATURE_BLOCKS_DECLARE_QUEUES_EXTERNAL
    #include "av1ehw_block_queues.h"
#undef DEF_BLOCK_Q

class FeatureBase
    : public FeatureBaseCommon<FeatureBlocks>
{
public:
    FeatureBase() = delete;
    virtual ~FeatureBase() {}

    virtual void Init(
        mfxU32 mode/*eFeatureMode*/
        , FeatureBlocks& blocks) override;

protected:
#define DEF_BLOCK_Q MFX_FEATURE_BLOCKS_DECLARE_QUEUES_IN_FEATURE_BASE
#include "av1ehw_block_queues.h"
#undef DEF_BLOCK_Q
    typedef TPushQ1NC TPushQ1;
    typedef TPushCLOSE TPushCLS;

    FeatureBase(mfxU32 id)
        : FeatureBaseCommon<FeatureBlocks>(id)
    {}

    virtual const BlockTracer::TFeatureTrace* GetTrace() { return nullptr; }
    virtual void SetTraceName(std::string&& /*name*/) {}
};

}; //namespace AV1EHW

#endif
