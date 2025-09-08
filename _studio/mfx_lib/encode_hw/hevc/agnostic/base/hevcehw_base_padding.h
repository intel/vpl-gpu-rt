// Copyright (c) 2025 Intel Corporation
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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_base.h"
#include "hevcehw_base_data.h"

namespace HEVCEHW
{
    namespace Base
    {
        class Padding
            : public FeatureBase
        {
        public:
#define DECL_BLOCK_LIST\
        DECL_BLOCK(ResetFramePadding)\
        DECL_BLOCK(InsertFramePadding)\
        DECL_BLOCK(ResetStateResetFramePadding)\
        DECL_BLOCK(ResetStateInsertFramePadding)

#define DECL_FEATURE_NAME "Base_Padding"
#include "hevcehw_decl_blocks.h"

            Padding(mfxU32 FeatureId)
                : FeatureBase(FeatureId)
            {
            }

        protected:
            bool IsPaddingNeeded(StorageRW& global, StorageRW& local);
            void InsertFramePadding(StorageRW& global);
            void ResetFramePadding(StorageRW& global);
            void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override;
            void ResetState(const FeatureBlocks& /*blocks*/, TPushRS Push) override;

        };

    } // Base
} // namespace HEVCEHW

#endif // defined(MFX_ENABLE_H265_VIDEO_ENCODE)
