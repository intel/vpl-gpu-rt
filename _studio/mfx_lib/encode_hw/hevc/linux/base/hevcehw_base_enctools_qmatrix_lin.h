// Copyright (c) 2019-2022  Intel Corporation
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
#include <deque>

#include "hevcehw_base.h"
#include "hevcehw_base_data.h"

#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)
#if defined(MFX_ENABLE_ENCTOOLS_SW)

namespace HEVCEHW
{
namespace Linux
{
namespace Base
{
    enum
    {
        ET_CQM_USE_FLAT_MATRIX  = 0,   //use flat matrix
        ET_CQM_USE_CUST_MEDIUM_MATRIX = 1,
        ET_CQM_USE_CUST_STRONG_MATRIX = 2,
        ET_CQM_USE_CUST_EXTREME_MATRIX = 3,
        ET_CQM_NUM_CUST_MATRIX  = 3,
        ET_CQM_INVALID          = 0xFF  //invalid hint
    };

    class EncToolsSwQMatrix
        : public FeatureBase
    {
    public:
#define DECL_BLOCK_LIST\
    DECL_BLOCK(UpdateSPS)\
    DECL_BLOCK(UpdatePPS)\
    DECL_BLOCK(PatchDDITask)\
    DECL_BLOCK(SetCallChains)
#define DECL_FEATURE_NAME "Base_EncTools_QMatrix"
#include "hevcehw_decl_blocks.h"

        EncToolsSwQMatrix(mfxU32 FeatureId)
                : FeatureBase(FeatureId)
        {}

    protected:
        virtual void InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push) override;
        virtual void SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push) override;

    private:
        bool                             bAdaptiveCQMPpsEnabled = false; // Whether AdaptiveCQM is enabled

        struct QpHistory
        {
            QpHistory() { Reset(); }
            void Reset() { history.clear();}
            void Add(mfxU8 qp);
            mfxU8 GetAverageQp() const;
        private:
            static constexpr mfxU32 HIST_SIZE = 16;
            std::deque<mfxU8> history;
        };
        QpHistory avgQP;
    };

} // Base
} // Linux
} // namespace HEVCEHW

#endif // defined(MFX_ENABLE_ENCTOOLS_SW)
#endif // defined(MFX_ENABLE_H265_VIDEO_ENCODE)
