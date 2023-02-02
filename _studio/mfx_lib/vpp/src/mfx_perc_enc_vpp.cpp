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

#include "mfx_common.h"

#if defined(ONEVPL_EXPERIMENTAL)

#include "mfx_perc_enc_vpp.h"
#include "mfx_ext_buffers.h"
#include "mfx_common_int.h"

#include <immintrin.h>
#include <cmath>
#include <array>

namespace PercEncPrefilter
{
    constexpr bool isPowerOfTwo(int x)
    {
        return !(x & (x - 1));
    }

    constexpr int log2(int x)
    {
        return x == 1 ? 0 : log2(x >> 1) + 1;
    }

#ifdef __AVX2__
    struct Filter
    {
        PercEncFilter::Parameters::NotModulated parametersNotModulated;
        PercEncFilter::Parameters::Modulated parametersModulated;

        static constexpr int unityLog2 = 8;
        static constexpr int unity = 1 << unityLog2;

        struct Parameters
        {
            __m256i pivot;
            __m256i min;
            __m256i max;
            __m128i leftshift;
        };

        Filter(PercEncFilter::Parameters::NotModulated &parametersNotModulated, PercEncFilter::Parameters::Modulated &parametersModulated, int width)
            : parametersNotModulated{parametersNotModulated},
              parametersModulated{parametersModulated}
        {
            const auto coefficientsSize = (width + 32) / 16 * 16;
            coefficientsVertical[0].resize(coefficientsSize);
            coefficientsVertical[1].resize(coefficientsSize);
            coefficientsHorizontal.resize(coefficientsSize);

            if (parametersNotModulated.temporalEnabled)
                coefficientsTemporal.resize(coefficientsSize);
        }

        std::array<std::vector<int16_t>, 2> coefficientsVertical;
        std::vector<int16_t> coefficientsHorizontal;
        std::vector<int16_t> coefficientsTemporal;

        bool haveFilteredOneFrame = false;

        void calculateCoefficients(const Parameters &parameters, uint8_t const *a, uint8_t const *b, int16_t *c, int width)
        {
            for (int i = 0; i < width; i += 16)
            {
                auto left = _mm_loadu_si128(reinterpret_cast<__m128i const *>(a));
                auto dataL = _mm256_cvtepu8_epi16(left);
                auto right = _mm_loadu_si128(reinterpret_cast<__m128i const *>(b));
                auto dataR = _mm256_cvtepu8_epi16(right);
                auto diff = _mm256_subs_epi16(dataL, dataR);
                auto absdiff = _mm256_abs_epi16(diff);
                auto coeff = _mm256_sub_epi16(parameters.pivot, absdiff);
                coeff = _mm256_sll_epi16(coeff, parameters.leftshift);
                coeff = _mm256_min_epi16(coeff, parameters.max);
                coeff = _mm256_max_epi16(coeff, parameters.min);
                _mm256_storeu_si256(reinterpret_cast<__m256i *>(c), coeff);
                a += 16;
                b += 16;
                c += 16;
            }
        }

        inline void processLine(const Parameters &spatial, const Parameters &temporal, uint8_t const *input, uint8_t const *inputLineAbove, uint8_t const *previousOutput, uint8_t const *inputLineBelow, uint8_t *output, int width, int y)
        {
            calculateCoefficients(spatial, input, input + 1, coefficientsHorizontal.data() + 1, width);
            coefficientsHorizontal[width] = 0;
            calculateCoefficients(spatial, input, inputLineBelow, coefficientsVertical[y % 2].data(), width);
            calculateCoefficients(temporal, input, previousOutput, coefficientsTemporal.data(), width);

            auto h = coefficientsHorizontal.data() + 1;
            auto d = coefficientsVertical[y % 2].data();
            auto u = coefficientsVertical[!(y % 2)].data();
            auto t = coefficientsTemporal.data();

            const __m128i l2 = _mm_setr_epi32(unityLog2, 0, 0, 0);

            for (int i = 0; i < width; i += 16)
            {
                __m256i coeffTotal;
                __m256i accumulator = _mm256_set1_epi16(unity / 2); // rounding offset

                {
                    // left
                    auto x = _mm_loadu_si128(reinterpret_cast<__m128i const *>(input - 1));
                    auto x2 = _mm256_cvtepu8_epi16(x);
                    auto c = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(h - 1));
                    accumulator = _mm256_add_epi16(accumulator, _mm256_mullo_epi16(x2, c));
                    coeffTotal = c;
                }

                {
                    // right
                    auto x = _mm_loadu_si128(reinterpret_cast<__m128i const *>(input + 1));
                    auto x2 = _mm256_cvtepu8_epi16(x);
                    auto c = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(h));
                    accumulator = _mm256_add_epi16(accumulator, _mm256_mullo_epi16(x2, c));
                    coeffTotal = _mm256_add_epi16(coeffTotal, c);
                }

                {
                    // up
                    auto x = _mm_loadu_si128(reinterpret_cast<__m128i const *>(inputLineAbove));
                    auto x2 = _mm256_cvtepu8_epi16(x);
                    auto c = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(u));
                    accumulator = _mm256_add_epi16(accumulator, _mm256_mullo_epi16(x2, c));
                    coeffTotal = _mm256_add_epi16(coeffTotal, c);
                }

                {
                    // down
                    auto x = _mm_loadu_si128(reinterpret_cast<__m128i const *>(inputLineBelow));
                    auto x2 = _mm256_cvtepu8_epi16(x);
                    auto c = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(d));
                    accumulator = _mm256_add_epi16(accumulator, _mm256_mullo_epi16(x2, c));
                    coeffTotal = _mm256_add_epi16(coeffTotal, c);
                }

                {
                    // temporal
                    auto x = _mm_loadu_si128(reinterpret_cast<__m128i const *>(previousOutput));
                    auto x2 = _mm256_cvtepu8_epi16(x);
                    auto c = _mm256_loadu_si256(reinterpret_cast<__m256i const *>(t));
                    accumulator = _mm256_add_epi16(accumulator, _mm256_mullo_epi16(x2, c));
                    coeffTotal = _mm256_add_epi16(coeffTotal, c);
                }

                auto central2 = _mm256_cvtepu8_epi16(_mm_loadu_si128(reinterpret_cast<__m128i const *>(input)));
                accumulator = _mm256_sub_epi16(accumulator, _mm256_mullo_epi16(central2, coeffTotal));

                accumulator = _mm256_sra_epi16(accumulator, l2);
                accumulator = _mm256_add_epi16(accumulator, central2);
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
                t += 16;
            }
        }

        void processFrame(uint8_t const *input, int inputStride, uint8_t const *previousOutput, int previousOutputStride, uint8_t *output, int outputStride, int width, int height)
        {
            Parameters spatial;
            spatial.pivot = _mm256_set1_epi16((int16_t)std::round(parametersModulated.spatial.pivot * 256));
            spatial.leftshift = _mm_set1_epi64x(log2(parametersNotModulated.spatialSlope));
            spatial.max = _mm256_set1_epi16((int16_t)round(parametersModulated.spatial.maximum * unity));
            spatial.min = _mm256_set1_epi16((int16_t)round(parametersModulated.spatial.minimum * unity));

            Parameters temporal{};
            if (haveFilteredOneFrame && parametersNotModulated.temporalEnabled)
            {
                temporal.pivot = _mm256_set1_epi16((int16_t)round(parametersModulated.temporal.pivot * 256));
                temporal.leftshift = _mm_set1_epi64x(log2(parametersNotModulated.temporalSlope));
                temporal.max = _mm256_set1_epi16((int16_t)round(parametersModulated.temporal.maximum * unity));
                temporal.min = _mm256_set1_epi16((int16_t)round(parametersModulated.temporal.minimum * unity));
            }

            const auto width16 = width / 16 * 16;

            // copy top line
            std::copy(
                &input[inputStride * 0],
                &input[inputStride * 0 + width],
                &output[outputStride * 0]);

            // don't filter top and bottom picture lines to avoid out-of-bounds read access
            for (int y = 1; y < height - 1; ++y)
            {
                processLine(
                    spatial,
                    temporal,
                    &input[inputStride * y],
                    &input[inputStride * (y - 1)],
                    &previousOutput[previousOutputStride * y],
                    &input[inputStride * (y + 1)],
                    &output[outputStride * y],
                    width16,
                    y);

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
    };
#endif

    mfxStatus PercEncFilter::Query(mfxExtBuffer *hint)
    {
        std::ignore = hint;
        return MFX_ERR_NONE;
    }

    PercEncFilter::PercEncFilter(VideoCORE *pCore, mfxVideoParam const &par)
    {
        m_core = dynamic_cast<CommonCORE_VPL *>(pCore);
        MFX_CHECK_WITH_THROW_STS(m_core, MFX_ERR_NULL_PTR);
        std::ignore = par;
    }

    PercEncFilter::~PercEncFilter()
    {
        std::ignore = Close();
    }

    mfxStatus PercEncFilter::Init(mfxFrameInfo *in, mfxFrameInfo *out)
    {
#ifdef __AVX2__
        MFX_CHECK_NULL_PTR1(in);
        MFX_CHECK_NULL_PTR1(out);

        MFX_CHECK(in->CropW == out->CropW, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(in->CropH == out->CropH, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(in->FourCC == out->FourCC, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(in->BitDepthLuma == out->BitDepthLuma, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(in->BitDepthChroma == out->BitDepthChroma, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(in->ChromaFormat == out->ChromaFormat, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(in->Shift == out->Shift, MFX_ERR_INVALID_VIDEO_PARAM);

        MFX_CHECK(in->CropW >= 16, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(in->CropH >= 2, MFX_ERR_INVALID_VIDEO_PARAM);

        const auto alreadyInitialised = !!filter;
        MFX_CHECK(!alreadyInitialised, MFX_ERR_UNKNOWN);
        width = in->CropW;
        height = in->CropH;
        previousOutput.resize(width * height);

        filter = std::make_unique<Filter>(parametersNotModulated, parametersModulated, width);

        return MFX_ERR_NONE;
#else
        MFX_RETURN(MFX_ERR_UNKNOWN)
#endif
    }

    mfxStatus PercEncFilter::Close()
    {
        return MFX_ERR_NONE;
    }

    mfxStatus PercEncFilter::Reset(mfxVideoParam *video_param)
    {
        MFX_CHECK_NULL_PTR1(video_param);

        MFX_SAFE_CALL(Close());

        MFX_SAFE_CALL(Init(&video_param->vpp.In, &video_param->vpp.Out));

        return MFX_ERR_NONE;
    }

    mfxStatus PercEncFilter::SetParam(mfxExtBuffer *)
    {
        return MFX_ERR_NONE;
    }

    mfxStatus PercEncFilter::RunFrameVPPTask(mfxFrameSurface1 *in, mfxFrameSurface1 *out, InternalParam *param)
    {
        return RunFrameVPP(in, out, param);
    }

    mfxStatus PercEncFilter::RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, InternalParam *)
    {
#ifdef __AVX2__
        MFX_CHECK_NULL_PTR1(in);
        MFX_CHECK_NULL_PTR1(out);

        // skip filtering if cropping or resizing is required
        if (in->Info.CropX != out->Info.CropX || in->Info.CropX != 0 ||
            in->Info.CropY != out->Info.CropY || in->Info.CropY != 0 ||
            in->Info.CropW != out->Info.CropW ||
            in->Info.CropH != out->Info.CropH)
        {
            return MFX_ERR_NONE;
        }

        if (in->Info.CropW != width ||
            in->Info.CropH != height)
        {
            return MFX_ERR_NONE;
        }

        mfxFrameSurface1_scoped_lock inLock(in, m_core), outLock(out, m_core);
        MFX_SAFE_CALL(inLock.lock(MFX_MAP_READ));
        MFX_SAFE_CALL(outLock.lock(MFX_MAP_WRITE));

        filter->processFrame(
            in->Data.Y, in->Data.Pitch,
            previousOutput.data(), width,
            out->Data.Y, out->Data.Pitch,
            width, height);

        // retain a copy of the output for next time... (it would be nice to avoid this copy)
        for (int y = 0; y < height; ++y)
            std::copy(
                &out->Data.Y[out->Data.Pitch * y],
                &out->Data.Y[out->Data.Pitch * y + width],
                &previousOutput[width * y]);

        // copy chroma
        std::copy(
            &in->Data.UV[0],
            &in->Data.UV[in->Data.Pitch * height / 2],
            &out->Data.UV[0]);

        return MFX_ERR_NONE;
#else
        MFX_RETURN(MFX_ERR_UNKNOWN)
#endif
    }

    bool PercEncFilter::IsReadyOutput(mfxRequestType)
    {
        // TBD: temporary do processing in sync. part therefore always return true
        return true;
    }
} // namespace

#endif
