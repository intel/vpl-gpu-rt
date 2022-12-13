// Copyright (c) 2021-2022 Intel Corporation
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

#include "av1ehw_base_lin.h"
#include "av1ehw_base_data.h"
#include "av1ehw_base_alloc.h"
#include "av1ehw_base_general.h"
#include "av1ehw_base_packer.h"
#include "av1ehw_base_task.h"
#include "av1ehw_base_va_lin.h"
#include "av1ehw_base_va_packer_lin.h"
#include "av1ehw_base_tile.h"
#include "av1ehw_base_query_impl_desc.h"
#include "av1ehw_base_qmatrix_lin.h"
#include "av1ehw_base_max_frame_size_lin.h"
#if defined(MFX_ENABLE_ENCTOOLS)
#include "av1ehw_base_enctools.h"
#endif
#include "av1ehw_base_hdr.h"
using namespace AV1EHW;
using namespace AV1EHW::Base;

Linux::Base::MFXVideoENCODEAV1_HW::MFXVideoENCODEAV1_HW(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
    : AV1EHW::Base::MFXVideoENCODEAV1_HW(core)
{
    status = MFX_ERR_UNKNOWN;
    auto vaType = core.GetVAType();

    m_features.emplace_back(new Allocator(FEATURE_ALLOCATOR));

    if(vaType == MFX_HW_VAAPI)
    {
        m_features.emplace_back(new DDI_VA(FEATURE_DDI));
    }
    else
    {
        status = MFX_ERR_UNSUPPORTED;
        return;
    }

    m_features.emplace_back(new VAPacker(FEATURE_DDI_PACKER));
    m_features.emplace_back(new General(FEATURE_GENERAL));
    m_features.emplace_back(new TaskManager(FEATURE_TASK_MANAGER));
    m_features.emplace_back(new Packer(FEATURE_PACKER));
    m_features.emplace_back(new MaxFrameSize(FEATURE_MAX_FRAME_SIZE));
    m_features.emplace_back(new Tile(FEATURE_TILE));
    m_features.emplace_back(new QueryImplDesc(FEATURE_QUERY_IMPL_DESC));
    m_features.emplace_back(new QMatrix(FEATURE_QMATRIX));
#if defined(MFX_ENABLE_ENCTOOLS)
    m_features.emplace_back(new AV1EncTools(FEATURE_ENCTOOLS));
#endif
    m_features.emplace_back(new Hdr(FEATURE_HDR));

    InternalInitFeatures(status, mode);
}

mfxStatus Linux::Base::MFXVideoENCODEAV1_HW::Init(mfxVideoParam *par)
{
    mfxStatus sts = AV1EHW::Base::MFXVideoENCODEAV1_HW::Init(par);
    MFX_CHECK(sts >= MFX_ERR_NONE, sts);

    return sts;
}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
