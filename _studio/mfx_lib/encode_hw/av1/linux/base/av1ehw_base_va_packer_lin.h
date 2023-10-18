// Copyright (c) 2021 Intel Corporation
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

#include "av1ehw_base.h"
#include "av1ehw_base_data.h"
#include "av1ehw_base_iddi_packer.h"
#include "ehw_utils_vaapi.h"

namespace AV1EHW
{
namespace Linux
{
namespace Base
{
using namespace AV1EHW::Base;
using namespace MfxEncodeHW;

class VAPacker
    : public IDDIPacker
    , protected VAAPIParPacker
{
public:
    VAPacker(mfxU32 FeatureId)
        : IDDIPacker(FeatureId)
    {
        SetTraceName("Base_DDIPacker");
    }

    struct CallChains
        : Storable
    {
        using TReadFeedback = CallChain<mfxStatus
            , const StorageR& //glob
            , StorageW& //task
            , const VACodedBufferSegment&>;
        TReadFeedback ReadFeedback;

        using TInitSPS = CallChain<void
            , const StorageR& //glob
            , VAEncSequenceParameterBufferAV1&>;
        TInitSPS InitSPS;

        using TInitPPS = CallChain<void
            , const StorageR& //glob
            , VAEncPictureParameterBufferAV1&>;
        TInitPPS InitPPS;

        using TUpdatePPS = CallChain<void
            , const StorageR& //glob
            , const StorageR& //task
            , VAEncPictureParameterBufferAV1&>;
        TUpdatePPS UpdatePPS;

        using TAddMiscData = CallChain<bool
            , const StorageR& //glob
            , const StorageR& //task
            , std::list<std::vector<mfxU8>>&>;
        std::map<VAEncMiscParameterType, TAddMiscData> AddPerPicMiscData;
        std::map<VAEncMiscParameterType, TAddMiscData> AddPerSeqMiscData;
    };

    using CC = StorageVar<Base::Glob::CallChainsKey, CallChains>;

protected:

    virtual void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override;
    virtual void InitInternal(const FeatureBlocks& blocks, TPushII Push) override;
    virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
    virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;
    virtual void ResetState(const FeatureBlocks& blocks, TPushRS Push) override;

    VAEncSequenceParameterBufferAV1      m_sps                = {};
    VAEncPictureParameterBufferAV1       m_pps                = {};
    VAEncSegMapBufferAV1                 m_segment            = {};
    std::vector<VAEncTileGroupBufferAV1> m_tile_groups_global = {};
    std::vector<VAEncTileGroupBufferAV1> m_tile_groups_task   = {};
    std::vector<mfxU8>                   m_segment_map        = {};
    std::list<std::vector<mfxU8>>        m_vaPerSeqMiscData   = {};
    std::list<std::vector<mfxU8>>        m_vaPerPicMiscData   = {};
};

} //Base
} //Linux
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
