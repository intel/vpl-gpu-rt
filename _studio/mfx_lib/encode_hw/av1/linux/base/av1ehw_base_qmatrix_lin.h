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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base_qmatrix.h"
#include "av1ehw_base_va_packer_lin.h"

namespace AV1EHW
{
namespace Linux
{
namespace Base
{
    class QMatrix
        : public AV1EHW::Base::QMatrix
    {
    public:

        QMatrix(mfxU32 FeatureId)
            : AV1EHW::Base::QMatrix(FeatureId)
        {}

    protected:
        virtual mfxStatus GetQPInfo(const void* pDdiFeedback, mfxU16& qp) override
        {
            MFX_CHECK(pDdiFeedback, MFX_ERR_UNDEFINED_BEHAVIOR);

            const auto pFeedback = static_cast<const VACodedBufferSegment*>(pDdiFeedback);
            qp = mfxU16(pFeedback->status & VA_CODED_BUF_STATUS_PICTURE_AVE_QP_MASK);

            return MFX_ERR_NONE;
        }
    };

} //Base
} //Linux
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
