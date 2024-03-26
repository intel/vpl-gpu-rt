// Copyright (c) 2019-2023 Intel Corporation
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

#include "av1ehw_base_data.h"

namespace AV1EHW
{
namespace Base
{

    const std::map<mfxU16, mfxU16> LevelIndexInTable =
    {
        {(mfxU16)MFX_LEVEL_AV1_2 , (mfxU16)0 },
        {(mfxU16)MFX_LEVEL_AV1_21, (mfxU16)1 },
        {(mfxU16)MFX_LEVEL_AV1_22, (mfxU16)1 }, // reuse 2.1 restrictions
        {(mfxU16)MFX_LEVEL_AV1_23, (mfxU16)1 }, // reuse 2.1 restrictions
        {(mfxU16)MFX_LEVEL_AV1_3 , (mfxU16)2 },
        {(mfxU16)MFX_LEVEL_AV1_31, (mfxU16)3 },
        {(mfxU16)MFX_LEVEL_AV1_32, (mfxU16)3 }, // reuse 3.1 restrictions
        {(mfxU16)MFX_LEVEL_AV1_33, (mfxU16)3 }, // reuse 3.1 restrictions
        {(mfxU16)MFX_LEVEL_AV1_4 , (mfxU16)4 },
        {(mfxU16)MFX_LEVEL_AV1_41, (mfxU16)5 },
        {(mfxU16)MFX_LEVEL_AV1_42, (mfxU16)5 }, // reuse 4.1 restrictions
        {(mfxU16)MFX_LEVEL_AV1_43, (mfxU16)5 }, // reuse 4.1 restrictions
        {(mfxU16)MFX_LEVEL_AV1_5 , (mfxU16)6 },
        {(mfxU16)MFX_LEVEL_AV1_51, (mfxU16)7 },
        {(mfxU16)MFX_LEVEL_AV1_52, (mfxU16)8 },
        {(mfxU16)MFX_LEVEL_AV1_53, (mfxU16)9 },
        {(mfxU16)MFX_LEVEL_AV1_6 , (mfxU16)10},
        {(mfxU16)MFX_LEVEL_AV1_61, (mfxU16)11},
        {(mfxU16)MFX_LEVEL_AV1_62, (mfxU16)12},
        {(mfxU16)MFX_LEVEL_AV1_63, (mfxU16)13},
        {(mfxU16)MFX_LEVEL_AV1_7,  (mfxU16)13}, // reuse 6.3 restrictions
        {(mfxU16)MFX_LEVEL_AV1_71, (mfxU16)13}, // reuse 6.3 restrictions
        {(mfxU16)MFX_LEVEL_AV1_72, (mfxU16)13}, // reuse 6.3 restrictions
        {(mfxU16)MFX_LEVEL_AV1_73, (mfxU16)13}, // reuse 6.3 restrictions
    };

    inline bool isValidCodecLevel(mfxU16 CodecLevel)
    {
        return LevelIndexInTable.find(CodecLevel) != LevelIndexInTable.end();
    }

    mfxU32 GetMaxHSizeByLevel(mfxU16 CodecLevel);
    mfxU32 GetMaxVSizeByLevel(mfxU16 CodecLevel);
    mfxU32 GetMaxKbpsByLevel(mfxU16 CodecLevel, mfxU16 CodecProfile, mfxU16 seqTier = 0);
    mfxF64 GetMaxFrameRateByLevel(mfxU16 CodecLevel, mfxU32 width, mfxU32 height);
    mfxU32 GetMaxTilesByLevel(mfxU16 CodecLevel);
    mfxU16 GetMaxTileColsByLevel(mfxU16 CodecLevel);

    mfxU16 GetMinLevel(
        mfxU32 width
        , mfxU32 height
        , mfxU16 numTileCols
        , mfxU16 numTileRows
        , mfxU32 maxKbps
        , mfxU16 profile
        , mfxU16 startLevel);

    mfxU16 GetMinLevel(
        const Defaults::Param& defPar
        , mfxU16 startLevel);

} //Base
} //namespace AV1EHW

#endif
