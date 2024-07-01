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

#if defined(ONEVPL_EXPERIMENTAL)

#include "mfx_perc_enc_vpp_avx2.h"

#include <immintrin.h>
#include <cmath>
#include <array>
#include <memory>

#ifndef RT_FUNC_ENTER
// for cases where this code is used elsewhere
#define RT_FUNC_ENTER(A)
#endif

namespace PercEncPrefilter
{
    constexpr int log2(int x)
    {
        return x == 1 ? 0 : log2(x >> 1) + 1;
    }

    Filter::Filter(
        const Parameters::PerFrame &parametersFrame,
        const std::array<Parameters::PerBlock, 2> &parametersBlock,
        int width)
        : parametersFrame{parametersFrame},
          parametersBlock{parametersBlock}
    {
        const auto coefficientsSize = (width + 32) / 16 * 16;
        coefficientsVertical[0].resize(coefficientsSize);
        coefficientsVertical[1].resize(coefficientsSize);

        if (parametersFrame.qpAdaptive)
        {
            coefficientsVerticalULDR[0].resize(coefficientsSize);
            coefficientsVerticalULDR[1].resize(coefficientsSize);
            coefficientsVerticalURDL[0].resize(coefficientsSize);
            coefficientsVerticalURDL[1].resize(coefficientsSize);
        }

        coefficientsHorizontal.resize(coefficientsSize);

        if (parametersFrame.temporalEnabled)
            coefficientsTemporal.resize(coefficientsSize);

        modulatedParametersSpatial.resize((width + 32) / 16);
        modulatedParametersTemporal.resize((width + 32) / 16);
        nullModulation.resize((width + 32) / 16);
    }


    void Filter::calculateCoefficients(const __m128i &LeftShift, const Filter::ModulatedParameters<int16_t> *parameters, const uint8_t *a, const uint8_t *b, int16_t *c, int width)
    {
        for (int i = 0; i < width; i += 16)
        {
            auto left = _mm_loadu_si128(reinterpret_cast<const __m128i *>(a));
            auto dataL = _mm256_cvtepu8_epi16(left);
            auto right = _mm_loadu_si128(reinterpret_cast<const __m128i *>(b));
            auto dataR = _mm256_cvtepu8_epi16(right);
            auto diff = _mm256_subs_epi16(dataL, dataR);
            auto absdiff = _mm256_abs_epi16(diff);
            auto coeff = _mm256_sub_epi16(_mm256_set1_epi16(parameters->pivot), absdiff);
            coeff = _mm256_sll_epi16(coeff, LeftShift);
            coeff = _mm256_min_epi16(coeff, _mm256_set1_epi16(parameters->maximum));
            if(parametersFrame.qpAdaptive)
            {
                auto min = _mm256_set1_epi16(parameters->minimum);
                const __m128i RightShift = _mm_set1_epi64x(9);
                __m256i minmod = _mm256_subs_epi16(_mm256_set1_epi16(512), absdiff);
                min = _mm256_sra_epi16(_mm256_mullo_epi16(min, minmod), RightShift);
                coeff = _mm256_max_epi16(coeff, min);
            }
            else
            {
                coeff = _mm256_max_epi16(coeff, _mm256_set1_epi16(parameters->minimum));
            }
            _mm256_storeu_si256(reinterpret_cast<__m256i *>(c), coeff);
            a += 16;
            b += 16;
            c += 16;
            ++parameters;
        }
    }

    void Filter::processLine(
        const __m128i &spatialLeftShift,
        const __m128i &temporalLeftShift,
        const uint8_t *input,
        const uint8_t *inputLineAbove,
        const uint8_t *previousOutput,
        const uint8_t *inputLineBelow,
        uint8_t *output,
        int width,
        int y,
        int16_t qpClamp)
    {
        calculateCoefficients(spatialLeftShift, modulatedParametersSpatial.data(), input, input + 1, coefficientsHorizontal.data() + 1, width);
        coefficientsHorizontal[0] = 0;
        coefficientsHorizontal[width] = 0;
        calculateCoefficients(spatialLeftShift, modulatedParametersSpatial.data(), input, inputLineBelow, coefficientsVertical[y % 2].data(), width);
        if(parametersFrame.qpAdaptive)
        {
            calculateCoefficients(spatialLeftShift, modulatedParametersSpatial.data(), input, inputLineBelow + 1, coefficientsVerticalULDR[y % 2].data() + 1, width);
            coefficientsVerticalULDR[y % 2][0] = 0;
            coefficientsVerticalULDR[y % 2][width] = 0;
            calculateCoefficients(spatialLeftShift, modulatedParametersSpatial.data(), input, inputLineBelow - 1, coefficientsVerticalURDL[y % 2].data(), width);
            coefficientsVerticalURDL[y % 2][0] = 0;
            coefficientsVerticalURDL[y % 2][width] = 0;
        }
        calculateCoefficients(temporalLeftShift, modulatedParametersTemporal.data(), input, previousOutput, coefficientsTemporal.data(), width);

        auto h = coefficientsHorizontal.data() + 1;
        auto d = coefficientsVertical[y % 2].data();
        auto u = coefficientsVertical[!(y % 2)].data();

        auto dr = coefficientsVerticalULDR[y % 2].data() + 1;
        auto ul = coefficientsVerticalULDR[!(y % 2)].data() + 1;
        auto dl = coefficientsVerticalURDL[y % 2].data();
        auto ur = coefficientsVerticalURDL[!(y % 2)].data();

        auto t = coefficientsTemporal.data();

        const __m128i l2 = _mm_setr_epi32(unityLog2, 0, 0, 0);

        for (int i = 0; i < width; i += 16)
        {
            __m256i coeffTotal;
            __m256i accumulator = _mm256_set1_epi16(unity / 2); // rounding offset

            {
                // left
                auto x = _mm_loadu_si128(reinterpret_cast<const __m128i *>(input - 1));
                auto x2 = _mm256_cvtepu8_epi16(x);
                auto c = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(h - 1));
                accumulator = _mm256_add_epi16(accumulator, _mm256_mullo_epi16(x2, c));
                coeffTotal = c;
            }

            {
                // right
                auto x = _mm_loadu_si128(reinterpret_cast<const __m128i *>(input + 1));
                auto x2 = _mm256_cvtepu8_epi16(x);
                auto c = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(h));
                accumulator = _mm256_add_epi16(accumulator, _mm256_mullo_epi16(x2, c));
                coeffTotal = _mm256_add_epi16(coeffTotal, c);
            }

            {
                // up
                auto x = _mm_loadu_si128(reinterpret_cast<const __m128i *>(inputLineAbove));
                auto x2 = _mm256_cvtepu8_epi16(x);
                auto c = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(u));
                accumulator = _mm256_add_epi16(accumulator, _mm256_mullo_epi16(x2, c));
                coeffTotal = _mm256_add_epi16(coeffTotal, c);
            }

            {
                // down
                auto x = _mm_loadu_si128(reinterpret_cast<const __m128i *>(inputLineBelow));
                auto x2 = _mm256_cvtepu8_epi16(x);
                auto c = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(d));
                accumulator = _mm256_add_epi16(accumulator, _mm256_mullo_epi16(x2, c));
                coeffTotal = _mm256_add_epi16(coeffTotal, c);
            }
            if(parametersFrame.qpAdaptive)
            {
                {
                // up left
                auto x = _mm_loadu_si128(reinterpret_cast<const __m128i *>(inputLineAbove - 1));
                auto x2 = _mm256_cvtepu8_epi16(x);
                auto c = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(ul - 1));
                accumulator = _mm256_add_epi16(accumulator, _mm256_mullo_epi16(x2, c));
                coeffTotal = _mm256_add_epi16(coeffTotal, c);
                }
                {
                // down right
                auto x = _mm_loadu_si128(reinterpret_cast<const __m128i *>(inputLineBelow + 1));
                auto x2 = _mm256_cvtepu8_epi16(x);
                auto c = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(dr));
                accumulator = _mm256_add_epi16(accumulator, _mm256_mullo_epi16(x2, c));
                coeffTotal = _mm256_add_epi16(coeffTotal, c);
                }
                {
                // up right
                auto x = _mm_loadu_si128(reinterpret_cast<const __m128i *>(inputLineAbove + 1));
                auto x2 = _mm256_cvtepu8_epi16(x);
                auto c = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(ur + 1));
                accumulator = _mm256_add_epi16(accumulator, _mm256_mullo_epi16(x2, c));
                coeffTotal = _mm256_add_epi16(coeffTotal, c);
                }
                {
                // down left
                auto x = _mm_loadu_si128(reinterpret_cast<const __m128i *>(inputLineBelow - 1));
                auto x2 = _mm256_cvtepu8_epi16(x);
                auto c = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(dl));
                accumulator = _mm256_add_epi16(accumulator, _mm256_mullo_epi16(x2, c));
                coeffTotal = _mm256_add_epi16(coeffTotal, c);
                }
            }

            {
                // temporal
                auto x = _mm_loadu_si128(reinterpret_cast<const __m128i *>(previousOutput));
                auto x2 = _mm256_cvtepu8_epi16(x);
                auto c = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(t));
                accumulator = _mm256_add_epi16(accumulator, _mm256_mullo_epi16(x2, c));
                coeffTotal = _mm256_add_epi16(coeffTotal, c);
            }

            auto central2 = _mm256_cvtepu8_epi16(_mm_loadu_si128(reinterpret_cast<const __m128i *>(input)));
            accumulator = _mm256_sub_epi16(accumulator, _mm256_mullo_epi16(central2, coeffTotal));

            accumulator = _mm256_sra_epi16(accumulator, l2);
            accumulator = _mm256_add_epi16(accumulator, central2);

            if(parametersFrame.qpAdaptive)
            {
                __m256i clamp = _mm256_set1_epi16(qpClamp); // clamp
                auto central2max = _mm256_add_epi16(central2, clamp);
                auto central2min = _mm256_sub_epi16(central2, clamp);
                accumulator = _mm256_min_epi16(accumulator, central2max);
                accumulator = _mm256_max_epi16(accumulator, central2min);
            }
            else
            {
                qpClamp = 0;
            }

            __m128i lo_lane = _mm256_castsi256_si128(accumulator);
            __m128i hi_lane = _mm256_extracti128_si256(accumulator, 1);
            auto out = _mm_packus_epi16(lo_lane, hi_lane);

            _mm_storeu_si128(reinterpret_cast<__m128i *>(output), out);

            input += 16;
            inputLineAbove += 16;
            inputLineBelow += 16;
            previousOutput += 16;
            output += 16;
            h += 16;
            d += 16;
            u += 16;
            if(parametersFrame.qpAdaptive)
            {
                dr += 16;
                ul += 16;
                dl += 16;
                ur += 16;
            }
            t += 16;
        }
    }

    void Filter::processFrame(const uint8_t *input, int inputStride, const uint8_t *modulation, int modulationStride,
                            const uint8_t *previousOutput, int previousOutputStride, uint8_t *output, int outputStride,
                            int width, int height, int qp)
    {
        const auto width16 = width / 16 * 16;
        qp = std::min(MAX_QP, std::max(MIN_QP, qp));
        // copy top line
        std::copy(
            &input[inputStride * 0],
            &input[inputStride * 0 + width],
            &output[outputStride * 0]);

        __m128i spatialLeftShift, temporalLeftShift{};
        spatialLeftShift = _mm_set1_epi64x(log2(parametersFrame.spatialSlope));
        if (haveFilteredOneFrame && parametersFrame.temporalEnabled)
            temporalLeftShift = _mm_set1_epi64x(log2(parametersFrame.temporalSlope));

        // don't filter top and bottom picture lines to avoid out-of-bounds read access
        fill(coefficientsHorizontal.begin(),  coefficientsHorizontal.end(),  int16_t(0));
        fill(coefficientsVertical[0].begin(), coefficientsVertical[0].end(), int16_t(0));
        fill(coefficientsVertical[1].begin(), coefficientsVertical[1].end(), int16_t(0));
        if(parametersFrame.qpAdaptive)
        {
            fill(coefficientsVerticalULDR[0].begin(), coefficientsVerticalULDR[0].end(), int16_t(0));
            fill(coefficientsVerticalULDR[1].begin(), coefficientsVerticalULDR[1].end(), int16_t(0));
            fill(coefficientsVerticalURDL[0].begin(), coefficientsVerticalURDL[0].end(), int16_t(0));
            fill(coefficientsVerticalURDL[1].begin(), coefficientsVerticalURDL[1].end(), int16_t(0));
        }
        int16_t clamp = 255;
        if(parametersFrame.qpAdaptive)
            clamp = (int16_t) (pow(2.0,((double)qp-4.0)/6.0)/4.0);

        for (int y = 1; y < height - 1; ++y)
        {
            if (y == 1 || y % 16 == 0)
                for (int x = 0; x < width16 / 16; ++x)
                {
                    ModulatedParameters<float> spatial = {};
                    ModulatedParameters<float> temporal = {};

                    const int modulationValue = modulationStride ? int(modulation[x + y / 16 * modulationStride]) : 0;
                    const int m[2] = {256 - modulationValue, modulationValue};

                    for (int i = 0; i < 2; ++i)
                    {
                        spatial.pivot += m[i] * parametersBlock[i].spatial.pivot * 256;
                        spatial.maximum += m[i] * parametersBlock[i].spatial.maximum * unity;
                        spatial.minimum += m[i] * parametersBlock[i].spatial.minimum * unity;

                        if (haveFilteredOneFrame && parametersFrame.temporalEnabled)
                        {
                            temporal.pivot += m[i] * parametersBlock[i].temporal.pivot * 256;
                            temporal.maximum += m[i] * parametersBlock[i].temporal.maximum * unity;
                            temporal.minimum += m[i] * parametersBlock[i].temporal.minimum * unity;
                        }
                    }

                    modulatedParametersSpatial[x].pivot = int16_t(round(spatial.pivot / 256.f));
                    modulatedParametersSpatial[x].maximum = int16_t(round(spatial.maximum / 256.f));
                    modulatedParametersSpatial[x].minimum = int16_t(round(spatial.minimum / 256.f));

                    modulatedParametersTemporal[x].pivot = int16_t(round(temporal.pivot / 256.f));
                    modulatedParametersTemporal[x].maximum = int16_t(round(temporal.maximum / 256.f));
                    modulatedParametersTemporal[x].minimum = int16_t(round(temporal.minimum / 256.f));
                }

            processLine(
                spatialLeftShift,
                temporalLeftShift,
                &input[inputStride * y],
                &input[inputStride * (y - 1)],
                &previousOutput[previousOutputStride * y],
                &input[inputStride * (y + 1)],
                &output[outputStride * y],
                width16,
                y,
                clamp);

            // if width is not multiple of 16, any odd pixels at right are unfiltered
            std::copy(
                &input[inputStride * y + width16],
                &input[inputStride * y + width],
                &output[outputStride * y + width16]);
        }

        // copy bottom line
        std::copy(
            &input[inputStride * (height - 1)],
            &input[inputStride * (height - 1) + width],
            &output[outputStride * (height - 1)]);

        haveFilteredOneFrame = true;
    }

} // namespace

#endif
