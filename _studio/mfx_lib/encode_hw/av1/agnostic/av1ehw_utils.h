// Copyright (c) 2019-2021 Intel Corporation
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

#include "ehw_utils.h"

namespace AV1EHW
{
using namespace MfxEncodeHW::Utils;

inline mfxU8 CO2Flag(mfxU16 CO)
{
    return CO == MFX_CODINGOPTION_ON ? 1 : 0;
}

inline mfxU32 Ceil(mfxF64 x)
{
    return (mfxU32)(.999 + x);
}

template<typename T>
inline T TileLog2(T blkSize, T target)
{
    mfxU16 k;
    for (k = 0; (blkSize << k) < target; k++)
    {
    }
    return k;
}

inline mfxU16 CountTL(const mfxExtTemporalLayers& TL)
{
    return std::max(mfxU16(1), TL.NumLayers);
}
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
