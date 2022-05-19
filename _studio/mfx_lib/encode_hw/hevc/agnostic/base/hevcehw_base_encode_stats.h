// Copyright (c) 2022 Intel Corporation
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

#ifdef MFX_ENABLE_ENCODE_STATS
#ifdef MFX_ENABLE_H265_VIDEO_ENCODE

#include "hevcehw_base.h"
#include "hevcehw_base_data.h"
#include "libmfx_allocator.h"

namespace HEVCEHW
{
    namespace Base
    {
        class mfxEncodeStatsContainerHevc
            : public mfxEncodeStatsContainerImpl
        {
        public:
            static mfxEncodeStatsContainerImpl* Create()
            {
                auto container = new mfxEncodeStatsContainerHevc();
                container->AddRef();
                return container;
            }

        protected:
            mfxEncodeStatsContainerHevc() = default;

            mfxStatus AllocBlkStatsArray(mfxU32 numBlk) override
            {
                return AllocBlkStatsBuf(numBlk
                    , this->EncodeBlkStats->NumCTU
                    , this->EncodeBlkStats->HEVCCTUArray);
            }

            void DetroyBlkStatsArray() override
            {
                DeleteArray(this->EncodeBlkStats->HEVCCTUArray);
                this->EncodeBlkStats->NumCTU = 0;
            }
        };

        class EncodeStats
            : public FeatureBase
        {
        public:
#define DECL_BLOCK_LIST\
        DECL_BLOCK(CheckBS)\
        DECL_BLOCK(PatchDDITask)\
        DECL_BLOCK(QueryInit)\
        DECL_BLOCK(QueryTask)
#define DECL_FEATURE_NAME "Base_EncodeStats"
#include "hevcehw_decl_blocks.h"

            EncodeStats(mfxU32 FeatureId)
                : FeatureBase(FeatureId)
            {}

        protected:
            void FrameSubmit(const FeatureBlocks& /*blocks*/, TPushFS Push) override;
            void SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push) override;
            void QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push) override;

            virtual mfxStatus PatchDdi(StorageW& global) const = 0;
            virtual mfxStatus PatchFeedback(void* pDdiFeedback, const mfxEncodeStatsContainer& stats) const = 0;

        protected:
            bool m_frameLevelQueryEn = false;
            bool m_blockLevelQueryEn = false;
        };

    } //Base
} //namespace HEVCEHW

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
#endif // MFX_ENABLE_ENCODE_STATS
