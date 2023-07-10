// Copyright (c) 2020-2022 Intel Corporation
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

namespace AV1EHW
{
namespace Base
{
    struct QpHistory
    {
        QpHistory() { Reset(); }
        void Reset() { std::fill_n(m_history, HIST_SIZE, mfxU8(52)); }
        void Add(mfxU32 qp);
        mfxU8 GetAverageQp() const;

    private:
        static const mfxU32 HIST_SIZE = 16;
        mfxU8 m_history[HIST_SIZE];
    };

    class QMatrix
        : public FeatureBase
    {
    public:
#define DECL_BLOCK_LIST\
        DECL_BLOCK(SetDefaults)\
        DECL_BLOCK(CheckAndFix)\
        DECL_BLOCK(ConfigureTask)\
        DECL_BLOCK(PatchDDITask)\
        DECL_BLOCK(Update)
#define DECL_FEATURE_NAME "Base_QMatrix"
#include "av1ehw_decl_blocks.h"

        QMatrix(mfxU32 FeatureId)
            : FeatureBase(FeatureId)
        {}

    protected:
        virtual void SetSupported(ParamSupport& par) override;
        virtual void SetDefaults(const FeatureBlocks& blocks, TPushSD Push) override;
        virtual void Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
        virtual void PostReorderTask(const FeatureBlocks& blocks, TPushPostRT Push) override;
        virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;

        virtual mfxStatus GetQPInfo(const void* pDdiFeedback, mfxU16& qp) = 0;

        QpHistory  m_qpHistory;
    };

} //Base
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
