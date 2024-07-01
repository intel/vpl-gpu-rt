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

#pragma once
#include "mfx_common.h"

#if defined (ONEVPL_EXPERIMENTAL)
#include "mfx_perc_enc_vpp_avx2.h"
#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"
#include "libmfx_core.h"

#if defined(MFX_ENABLE_ENCTOOLS)
#include "mfxenctools-int.h"
#endif

namespace PercEncPrefilter
{

/*
Perceptual Prefilter

This filter is designed to enhance video prior to encoding. A description of the algorithm follows.

The perceptual prefilter can act as a softening and/or as a sharpening filter depending upon its
spatial control parameters (pS) and the characteristics of the input image. Temporal filtering is
also supported: this is motion adaptive and recursive, controlled by the temporal parameters (pT).
Both temporal and spatial parameters may be modulated per 16x16 block to enable the filter to
be responsive to a signal such as saliency or importance.

Each output luminance pixel value is a function of
 * the collocated input pixel value (yC collocated),
 * the values of the input four nearest spatial neighbors (yU up, yD down, yL left, yR right) and
 * the value of the collocated pixel in the previous frame's output from the filter (yP previous)

Each output luminance pixel value is determined according to the following formula:

yOut = yC * rC
     + yU * f(yU, yC, pS)
     + yD * f(yD, yC, pS)
     + yL * f(yL, yC, pS)
     + yR * f(yR, yC, pS)
     + yP * f(yP, yC, pT)

Where

rC = 1
   - f(yU, yC, pS)
   - f(yD, yC, pS)
   - f(yL, yC, pS)
   - f(yR, yC, pS)
   - f(yP, yC, pT)

Where pS are the spatial parameters and pT are the temporal parameters applicable to the current
block; where f(yN, yC, p) is a range function that allows the filter to behave non-linearly according to
the local spatial or temporal pixel differences:

    f(yN, yC, p) = clamp((p.pivot - abs(yN - yC)) * p.slope, p.min, p.max)

Here, "clamp" and "abs" have the same semantics as the similarly named functions in the C++
standard library. The parameter p.min is generally assigned a zero or negative value, p.max a zero or
positive value and p.slope a positive value.

In the case of the spatial filter, p.pivot specifies a level of texture intensity at which where the filter
transitions from softening fine details to sharpening more intense features. The limit parameters p.min
and p.max control sharpening of strong details and softening of weak features respectively. Setting p.min
or p.max to zero will disable sharpening or softening, respectively. The p.slope parameter controls the
transition between sharpening and softening: higher values represent a more abrupt transition.

In the case of the temporal filter, p.pivot specifies a threshold of temporal luminance change below which
the filter will take effect. The abruptness of filter adaptation to luminance change is influenced by p.slope.
The parameter p.max controls the maximum strength of temporal filtering. Parameter p.min should be
zero - a negative value would potentially make the filter oscillate.

The filter parameters may be modulated by supplying a modulation map. The modulation map has the
same shape as the video image but is 16th the resolution of the luminance plane. In other words, each
value in the modulation map controls filter behavior for a 16x16 block of the filtered image.

Each modulation value can take a value in the range 0 <= m < 1, and affects the pivot, min and max
parameters:

pT.slope = (1 - m) * qT.slope[0] + m * qT.slope[1]
pT.min   = (1 - m) * qT.min  [0] + m * qT.min  [1]
pT.max   = (1 - m) * qT.max  [0] + m * qT.max  [1]
pS.slope = (1 - m) * qS.slope[0] + m * qS.slope[1]
pS.min   = (1 - m) * qS.min  [0] + m * qS.min  [1]
pS.max   = (1 - m) * qS.max  [0] + m * qS.max  [1]

Here, qS and qT each contain a set of parameter pairs. The first of each pairs is used for blocks where
modulation, m = 0. The second of each pair is used where modulation, m = 1. In blocks having m between
0 and 1, a proportionate blend of the pair is used to control the filter. Thus the modulation map controls
the "blend" of two different filters that can have very different characteristics.

Qp Adaptive Option

By enabling QP adaptive option the filter output is clamped in a range with respect to the original where
the range is calculated from the QP.
The filter is also modified by adding extra taps and Spatial parameter p.min is modified by contrast.

The modified filter is given by:

yOut = clamp(yOut', yC-qpClamp, yC+qpClamp)

yOut' = yC * rC
     + yU * f(yU, yC, pS)
     + yD * f(yD, yC, pS)
     + yL * f(yL, yC, pS)
     + yR * f(yR, yC, pS)
     + yUL * f(yR, yC, pS)
     + yDR * f(yR, yC, pS)
     + yUR * f(yR, yC, pS)
     + yDL * f(yR, yC, pS)
     + yP * f(yP, yC, pT)

qpClamp = (int) pow(2, (qp-4.0)/6.0)/4.0;

Where

rC = 1
   - f(yU, yC, pS)
   - f(yD, yC, pS)
   - f(yL, yC, pS)
   - f(yR, yC, pS)
   - f(yUL, yC, pS)
   - f(yDR, yC, pS)
   - f(yUR, yC, pS)
   - f(yDL, yC, pS)
   - f(yP, yC, pT)

The following diagonal input spatial neighbors are additionally used :
yUL upper left, yDR down right, yUR upper right, yDL down left.

The spatial parameter p.min is modified for local constrast as follows :
p.min' = (p.min * (512 - abs(yN - yC)) >> 9

and used in weight calculation as follows :
f(yN, yC, p) = clamp((p.pivot - abs(yN - yC)) * p.slope, p.min', p.max)

The modulation scheme remains unchanged.
*/
class PercEncFilter
    : public FilterVPP
{
public:
    static mfxStatus Query(mfxExtBuffer*);

    PercEncFilter(VideoCORE*, mfxVideoParam const&);
    ~PercEncFilter();

    mfxStatus Init(mfxFrameInfo*, mfxFrameInfo*) override;
    mfxStatus Close() override;
    mfxStatus Reset(mfxVideoParam*) override;

    mfxStatus SetParam(mfxExtBuffer*) override;

    mfxStatus RunFrameVPP(mfxFrameSurface1*, mfxFrameSurface1*, InternalParam*) override;
    mfxStatus RunFrameVPPTask(mfxFrameSurface1*, mfxFrameSurface1*, InternalParam*) override;

    mfxU8 GetInFramesCount() override
    { return 1; }
    mfxU8 GetOutFramesCount() override
    { return 1; }

    mfxStatus GetBufferSize(mfxU32* /*size*/) override
    { MFX_RETURN(MFX_ERR_UNSUPPORTED); }
    mfxStatus SetBuffer(mfxU8* /*buffer*/) override
    { MFX_RETURN(MFX_ERR_UNSUPPORTED); }

    mfxStatus CheckProduceOutput(mfxFrameSurface1*, mfxFrameSurface1*) override
    { MFX_RETURN(MFX_ERR_UNSUPPORTED); }

    bool IsReadyOutput(mfxRequestType) override;

private:
    CommonCORE_VPL* m_core = nullptr;

    bool m_initialized = false;
    std::unique_ptr<Filter> filter;

    std::vector<uint8_t> previousOutput;
    int width = 0;
    int height = 0;

    std::array<Parameters::PerBlock, 2> parametersBlock;
    Parameters::PerFrame parametersFrame;
    
    std::vector<uint8_t> modulation;
    int modulationStride{};

#if defined(MFX_ENABLE_ENCTOOLS)
    static const mfxU32 blockSizeFilter = 16;

    //modulation map support
    mfxU32 m_frameCounter = 0;
    bool m_saliencyMapSupported = false;
    mfxEncTools *m_encTools = nullptr;
#endif
};

}//namespace
#endif
