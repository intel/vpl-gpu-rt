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

#if defined (ONEVPL_EXPERIMENTAL)

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"
#include "libmfx_core.h"

namespace PercEncPrefilter
{

template <class T>
struct Raster
{
    T *p;
    intptr_t stride;
    T &operator()(int x, int y)
    {
        return this->p[x + y * stride];
    }
};

template <class T>
struct Plane : Raster<T>
{
    void resize(int w, int h)
    {
        this->width = w;
        this->height = h;
        this->v.resize(w * h);
        this->p = v.data();
        this->stride = w;
    }

    std::vector<T> v;
    int width, height;
};

template <class T>
struct Picture
{
    Picture(){
    };

    Picture(int w, int h)
    {
        Resize(w, h);
    }

    void Resize(int w, int h)
    {
        this->planes[0].resize(w, h);
        this->planes[1].resize(w / 2, h / 2);
        this->planes[2].resize(w / 2, h / 2);
    }

    template <class U>
    Picture(Picture<U>& src, int shift)
    {
        if (shift >= 0)
        {
            for (int c = 0; c < 3; ++c)
            {
                this->planes[c].resize(src.planes[c].width, src.planes[c].height);
                for (int y = 0; y < src.planes[c].height; ++y)
                    for (int x = 0; x < src.planes[c].width; ++x)
                        this->planes[c](x, y) = static_cast<T>(src.planes[c](x, y) << shift);
            }
        }
        else
        {
            shift = -shift;
            int const add = (1 << shift) >> 1;
            for (int c = 0; c < 3; ++c)
            {
                this->planes[c].resize(src.planes[c].width, src.planes[c].height);
                for (int y = 0; y < src.planes[c].height; ++y)
                    for (int x = 0; x < src.planes[c].width; ++x)
                    {
                        auto val = (src.planes[c](x, y) + add) >> shift;
                        constexpr auto maxVal = (1 << (8 * sizeof(T))) - 1;
                        if (val > maxVal)
                            val = maxVal;
                        this->planes[c](x, y) = static_cast<T>(val);
                    }
            }
        }
    }

    Plane<T> planes[3];
};

struct PEConfig
{
    int pivot;
    int slope;
    int limitSoft;
    int limitSharp;
    int spatialPasses;
    bool temporal;
    int temporalPivot;
    int temporalSlope;
    int temporalLimitSoft;

    PEConfig()
    {
        std::vector<int> data{13, 0, 1, 5, 1, 49, 0, 1};

        pivot = data[0];
        slope = data[1];
        limitSoft = data[2];
        limitSharp = data[3];
        spatialPasses = data[4];
        temporal = !!data[7];
        if (temporal)
        {
            temporalPivot = data[5];
            temporalSlope = data[6];
            temporalLimitSoft = data[7];
        }
    }
};

struct Params
{
    virtual int coeff(int neighbour, int current, int modulation) const = 0;
};

struct ParamsOff
    : Params
{
    int coeff(int neighbour, int current, int modulation) const override
    {
        std::ignore = neighbour;
        std::ignore = current;
        std::ignore = modulation;
        return 0;
    }
};

struct ParamsSharpening
    : Params
{
    PEConfig *config;

    int coeff(int neighbour, int current, int modulation) const override
    {
        std::ignore = modulation;
        int x = config->pivot - abs(neighbour - current);
        x <<= config->slope;
        if (x > config->limitSoft)
            x = config->limitSoft;
        if (x < -config->limitSharp)
            x = -config->limitSharp;
        return x;
    }
};

struct ParamsTemporal
    : Params
{
    PEConfig *config;

    int coeff(int neighbour, int current, int modulation) const override
    {
        std::ignore = modulation;
        int x = config->temporalPivot - abs(neighbour - current);
        x <<= config->temporalSlope;
        if (x > config->temporalLimitSoft)
            x = config->temporalLimitSoft;
        if (x < 0)
            x = 0;
        return x;
    }
};

template <int bpp>
struct Filter
{
    const Params *spatial[2];
    const Params *temporal;
    int iterations;

    typedef uint16_t T;
    static unsigned const shift = 12;

    template <int i>
    T inline filterPoint(int above, int left, int current, int right, int below, int previous, uint8_t modulation)
    {
        assert(8 * sizeof(T) >= bpp);

        int accumulator = 1 << (shift - 1);

        int coeffCurrent = 1 << shift;

        auto const coeffAbove = this->spatial[i]->coeff(above, current, modulation);
        accumulator += coeffAbove * above;
        coeffCurrent -= coeffAbove;

        auto const coeffLeft = this->spatial[i]->coeff(left, current, modulation);
        accumulator += coeffLeft * left;
        coeffCurrent -= coeffLeft;

        auto const coeffRight = this->spatial[i]->coeff(right, current, modulation);
        accumulator += coeffRight * right;
        coeffCurrent -= coeffRight;

        auto const coeffBelow = this->spatial[i]->coeff(below, current, modulation);
        accumulator += coeffBelow * below;
        coeffCurrent -= coeffBelow;

        //"warning C4127: conditional expression is constant"
        #pragma warning(push)
        #pragma warning(disable : 4127)
        if (i == 1)
        {
            auto const coeffPrevious = this->temporal->coeff(previous, current, modulation);
            accumulator += coeffPrevious * previous;
            coeffCurrent -= coeffPrevious;
        }
        #pragma warning(pop)

        accumulator += coeffCurrent * current;

        auto constexpr max = (1 << bpp << shift) - 1;
        if (accumulator < 0)
            accumulator = 0;
        if (accumulator > max)
            accumulator = max;

        accumulator >>= shift;

        return static_cast<T>(accumulator);
    }

    template <int i>
    void filterRow(T *dst, T const *above, T const *current, T const *below, T const *previous, uint8_t const *modulation, int width)
    {
        assert(width >= 2);
        int x = 0;
        constexpr bool leftright = false;
        if (leftright)
            for (; x < width / 2; ++x)
                dst[x] = current[x];
        dst[x] = filterPoint<i>(above[x], current[x], current[x], current[x + 1], below[x], previous[x], modulation[x]);
        for (++x; x < width - 1; ++x)
            dst[x] = filterPoint<i>(above[x], current[x - 1], current[x], current[x + 1], below[x], previous[x], modulation[x]);
        dst[x] = filterPoint<i>(above[x], current[x - 1], current[x], current[x], below[x], previous[x], modulation[x]);
    }

    template <int i>
    void filterIteration(Raster<T> &dst, Raster<T> &src, Raster<T> &previous, Raster<uint8_t> &modulation, int width, int height)
    {
        assert(height >= 2);
        int y = 0;
        filterRow<i>(&dst(0, y), &src(0, y), &src(0, y), &src(0, y + 1), &previous(0, y), &modulation(0, y), width);
        for (++y; y < height - 1; ++y)
            filterRow<i>(&dst(0, y), &src(0, y - 1), &src(0, y), &src(0, y + 1), &previous(0, y), &modulation(0, y), width);
        filterRow<i>(&dst(0, y), &src(0, y - 1), &src(0, y), &src(0, y), &previous(0, y), &modulation(0, y), width);
    }

    void filter(Raster<T> &dst, Raster<T> &src, Raster<T> &previous, Raster<uint8_t> &modulation, int width, int height, PEConfig const &config, bool first)
    {
        Plane<T> intermediate;

        intermediate.resize(width, height);

        if (config.spatialPasses == 1)
        {
            if (config.temporal && !first)
                filterIteration<1>(dst, src, previous, modulation, width, height);
            else
                filterIteration<0>(dst, src, previous, modulation, width, height);
        }
        else if (config.spatialPasses == 2)
        {
            if (config.temporal && !first)
                filterIteration<1>(intermediate, src, previous, modulation, width, height);
            else
                filterIteration<0>(dst, intermediate, previous, modulation, width, height);
        }
        else{
            assert(false);
        }
    }
};

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

    bool m_first = true;
    Filter<10> m_filter;
    PEConfig m_config;
    ParamsSharpening m_paramsSharpening;
    ParamsTemporal m_paramsTemporal;
    Picture<uint8_t> m_modulation;

};

}//namespace
#endif
