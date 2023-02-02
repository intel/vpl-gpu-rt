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

#if defined(ONEVPL_EXPERIMENTAL)

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"
#include "libmfx_core.h"
#include <memory>

namespace PercEncPrefilter
{

    struct Filter;

    class PercEncFilter
        : public FilterVPP
    {
    public:
        static mfxStatus Query(mfxExtBuffer *);

        PercEncFilter(VideoCORE *, mfxVideoParam const &);
        ~PercEncFilter();

        mfxStatus Init(mfxFrameInfo *, mfxFrameInfo *) override;
        mfxStatus Close() override;
        mfxStatus Reset(mfxVideoParam *) override;

        mfxStatus SetParam(mfxExtBuffer *) override;

        mfxStatus RunFrameVPP(mfxFrameSurface1 *, mfxFrameSurface1 *, InternalParam *) override;
        mfxStatus RunFrameVPPTask(mfxFrameSurface1 *, mfxFrameSurface1 *, InternalParam *) override;

        mfxU8 GetInFramesCount() override
        {
            return 1;
        }
        mfxU8 GetOutFramesCount() override
        {
            return 1;
        }

        mfxStatus GetBufferSize(mfxU32 * /*size*/) override
        {
            MFX_RETURN(MFX_ERR_UNSUPPORTED);
        }
        mfxStatus SetBuffer(mfxU8 * /*buffer*/) override
        {
            MFX_RETURN(MFX_ERR_UNSUPPORTED);
        }

        mfxStatus CheckProduceOutput(mfxFrameSurface1 *, mfxFrameSurface1 *) override
        {
            MFX_RETURN(MFX_ERR_UNSUPPORTED);
        }

        bool IsReadyOutput(mfxRequestType) override;

        CommonCORE_VPL *m_core = nullptr;

        std::unique_ptr<Filter> filter;

        std::vector<uint8_t> previousOutput;
        int width, height;

        struct Parameters
        {
            struct NotModulated
            {
                bool temporalEnabled = true;
                int temporalSlope = 2;
                int spatialIterations = 1;
                int spatialSlope = 4;
            };

            struct Modulated
            {
                struct Curve
                {
                    double pivot;
                    double minimum;
                    double maximum;
                };

                Curve temporal{ 0.19140625, 0.0, 0.0234375};
                Curve spatial{0.05078125, -0.03125, 0.015625};
            };
        };

        void Setup(const Parameters &parameters);

        Parameters::Modulated parametersModulated;
        Parameters::NotModulated parametersNotModulated;

    };

} // namespace
#endif
