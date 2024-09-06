// Copyright (c) 2023 Intel Corporation
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
#include "av1ehw_base_tile.h"
#include "av1ehw_xe2_lin.h"
#include "av1ehw_xe2_scc.h"
#include "av1ehw_xe2_cdef.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;

Linux::Xe2::MFXVideoENCODEAV1_HW::MFXVideoENCODEAV1_HW(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
    : TBaseImpl(core, status, mode)
{
    TFeatureList newFeatures;

    newFeatures.emplace_back(new AV1EHW::Base::Segmentation(AV1EHW::Base::FEATURE_SEGMENTATION));
    newFeatures.emplace_back(new AV1EHW::Xe2::SCC(FEATURE_SCC));
    newFeatures.emplace_back(new AV1EHW::Xe2::CDEF(FEATURE_XE2CDEF));

    for (auto& pFeature : newFeatures)
        pFeature->Init(mode, *this);

    TBaseImpl::m_features.splice(TBaseImpl::m_features.end(), newFeatures);

    if (mode & (QUERY_IO_SURF | INIT))
    {
        auto& queue = BQ<BQ_SetDefaults>::Get(*this);

        queue.splice(queue.begin(), queue, Get(queue, { FEATURE_SCC, AV1EHW::Xe2::SCC::BLK_SetDefaults }));
    }

    if (mode & (QUERY1 | QUERY_IO_SURF | INIT))
    {
        auto& queue = BQ<BQ_Query1WithCaps>::Get(*this);
        Reorder(queue, { FEATURE_TILE, AV1EHW::Base::Tile::BLK_CheckAndFix }
        , { FEATURE_SCC, AV1EHW::Xe2::SCC::BLK_CheckAndFix }
        , PLACE_AFTER);
    }
}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
