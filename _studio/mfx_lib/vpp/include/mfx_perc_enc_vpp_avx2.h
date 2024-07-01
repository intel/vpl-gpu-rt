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

#if defined (ONEVPL_EXPERIMENTAL)

#include <memory>
#include <array>
#include <vector>
#include <cstdint>
#include <immintrin.h>

namespace PercEncPrefilter
{

constexpr int MAX_QP=51;
constexpr int MIN_QP=0;

struct Parameters
{
    struct PerFrame
    {
        bool temporalEnabled = true;
        int temporalSlope = 2;
        int spatialIterations = 1;
        int spatialSlope = 4;
        bool qpAdaptive = false;
    };

    struct PerBlock
    {
        struct Curve
        {
            float pivot;
            float minimum;
            float maximum;
        };

        Curve temporal{0.19140625f, 0.0f, 0.0234375f};
        Curve spatial{0.05078125f, -0.03125f, 0.015625f};
    };
};

struct Filter
{
    Parameters::PerFrame parametersFrame;
    std::array<Parameters::PerBlock, 2> parametersBlock;

    template <typename T>
    struct ModulatedParameters
    {
        T pivot;
        T minimum;
        T maximum;
    };

    static constexpr int unityLog2 = 8;
    static constexpr int unity = 1 << unityLog2;

    std::vector<uint8_t> nullModulation;

    std::vector<ModulatedParameters<int16_t>> modulatedParametersSpatial;
    std::vector<ModulatedParameters<int16_t>> modulatedParametersTemporal;

    std::array<std::vector<int16_t>, 2> coefficientsVertical;
    std::array<std::vector<int16_t>, 2> coefficientsVerticalULDR;
    std::array<std::vector<int16_t>, 2> coefficientsVerticalURDL;
    std::vector<int16_t> coefficientsHorizontal;
    std::vector<int16_t> coefficientsTemporal;

    bool haveFilteredOneFrame = false;

    Filter(
        const Parameters::PerFrame &parametersFrame,
        const std::array<Parameters::PerBlock, 2> &parametersBlock,
        int width);

    inline void calculateCoefficients(const __m128i &LeftShift, const Filter::ModulatedParameters<int16_t> *parameters, const uint8_t *a, const uint8_t *b, int16_t *c, int width);

    inline void processLine(
        const __m128i &spatialLeftShift,
        const __m128i &temporalLeftShift,
        const uint8_t *input,
        const uint8_t *inputLineAbove,
        const uint8_t *previousOutput,
        const uint8_t *inputLineBelow,
        uint8_t *output,
        int width,
        int y,
        int16_t clamp);

    void processFrame(const uint8_t *input, int inputStride, const uint8_t *modulation, int modulationStride,
                      const uint8_t *previousOutput, int previousOutputStride, uint8_t *output, int outputStride,
                      int width, int height, int qp=MAX_QP);
};

}//namespace
#endif
