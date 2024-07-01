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

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base.h"
#include "av1ehw_base_constraints.h"
#include <algorithm>
#include <math.h>

namespace AV1EHW
{
namespace Base
{

    enum TABLE_A1_COLS_INDEX
    {
        MAX_PIC_SIZE_INDEX = 0,
        MAX_H_SIZE_INDEX,
        MAX_V_SIZE_INDEX,
        MAX_DISPLAY_RATE_INDEX,
        MAX_DECODE_RATE_INDEX,
        TABLE_A1_COLS_NUM,
    };

    enum TABLE_A2_COLS_INDEX
    {
        MAX_HEADER_RATE_INDEX = 0,
        MAIN_BPS_INDEX,
        HIGH_BPS_INDEX,
        MAIN_CR_INDEX,
        HIGH_CR_INDEX,
        MAX_TILES_INDEX,
        MAX_TILE_COLS_INDEX,
        TABLE_A2_COLS_NUM,
    };

    const mfxU64 TableA1[][TABLE_A1_COLS_NUM] =
    {
        //  Level   |MaxPicSize | MaxHSize | MaxVSize | MaxDiaplayRate | MaxDecodeRate
        /*  2.0 */ {  147456,       2048,     1152,        4423680,         5529600},
        /*  2.1 */ {  278784,       2816,     1584,        8363520,        10454400},
        /*  3.0 */ {  665856,       4352,     2448,       19975680,        24969600},
        /*  3.1 */ { 1065024,       5504,     3096,       31950720,        39938400},
        /*  4.0 */ { 2359296,       6144,     3456,       70778880,        77856768},
        /*  4.1 */ { 2359296,       6144,     3456,      141557760,       155713536},
        /*  5.0 */ { 8912896,       8192,     4352,      267386880,       273705200},
        /*  5.1 */ { 8912896,       8192,     4352,      534773760,       547430400},
        /*  5.2 */ { 8912896,       8192,     4352,     1069547520,      1094860800},
        /*  5.3 */ { 8912896,       8192,     4352,     1069547520,      1176502272},
        /*  6.0 */ {35651584,      16384,     8704,     1069547520,      1176502272},
        /*  6.1 */ {35651584,      16384,     8704,     2139095040,      2189721600},
        /*  6.2 */ {35651584,      16384,     8704,     4278190080,      4379443200},
        /*  6.3 */ {35651584,      16384,     8704,     4278190080,      4706009088},
    };

    const mfxU32 TableA2[][TABLE_A2_COLS_NUM] =
    {
        //  Level   | MaxHeaderRate |    Mainbps    |    Highbps    | MainCR | HighCR | MaxTiles | MaxTileCols
        /*  2.0  */ {      150,          1500000,              0,       2,       0,         8,         4},
        /*  2.1  */ {      150,          3000000,              0,       2,       0,         8,         4},
        /*  3.0  */ {      150,          6000000,              0,       2,       0,        16,         6},
        /*  3.1  */ {      150,         10000000,              0,       2,       0,        16,         6},
        /*  4.0  */ {      300,         12000000,       30000000,       4,       4,        32,         8},
        /*  4.1  */ {      300,         20000000,       50000000,       4,       4,        32,         8},
        /*  5.0  */ {      300,         30000000,      100000000,       6,       4,        64,         8},
        /*  5.1  */ {      300,         40000000,      160000000,       8,       4,        64,         8},
        /*  5.2  */ {      300,         60000000,      240000000,       8,       4,        64,         8},
        /*  5.3  */ {      300,         60000000,      240000000,       8,       4,        64,         8},
        /*  6.0  */ {      300,         60000000,      240000000,       8,       4,       128,        16},
        /*  6.1  */ {      300,        100000000,      480000000,       8,       4,       128,        16},
        /*  6.2  */ {      300,        160000000,      800000000,       8,       4,       128,        16},
        /*  6.3  */ {      300,        160000000,      800000000,       8,       4,       128,        16},
    };

    const mfxU16 MAX_LEVEL_INDEX = (sizeof(TableA1) / sizeof(TableA1[0]));

    inline mfxU16 GetLevelIndexInTable(mfxU16 CodecLevel)
    {
        return LevelIndexInTable.at(CodecLevel);
    }

    mfxU32 GetMaxHSizeByLevel(mfxU16 CodecLevel)
    {
        const mfxU16 levelIndex = GetLevelIndexInTable(CodecLevel);
        return mfxU32(TableA1[levelIndex][MAX_H_SIZE_INDEX]);
    }

    mfxU32 GetMaxVSizeByLevel(mfxU16 CodecLevel)
    {
        const mfxU16 levelIndex = GetLevelIndexInTable(CodecLevel);
        return mfxU32(TableA1[levelIndex][MAX_V_SIZE_INDEX]);
    }

    /*
    * Refer to AV1 Spec section A.3 Levels
      These requirements depend on the following level, tier, and profile dependent variables:
      If seq_tier is equal to 0, MaxBitrate is equal to MainMbps multiplied by 1,000,000
      Otherwise (seq_tier is equal to 1), MaxBitrate is equal to HighMbps multiplied by 1,000,000
      MaxBufferSize is equal to MaxBitrate multiplied by 1 second
      If seq_profile is equal to 0, BitrateProfileFactor is equal to 1.0
      If seq_profile is equal to 1, BitrateProfileFactor is equal to 2.0
      If seq_profile is equal to 2, BitrateProfileFactor is equal to 3.0
    */
    mfxU32 GetMaxKbpsByLevel(mfxU16 CodecLevel, mfxU16 CodecProfile, mfxU16 seqTier)
    {
        const mfxU16 levelIndex = GetLevelIndexInTable(CodecLevel);
        mfxU32 BitrateProfileFactor = 1;
        switch (CodecProfile)
        {
        case(MFX_PROFILE_AV1_MAIN):
            BitrateProfileFactor = 1;
            break;
        case(MFX_PROFILE_AV1_HIGH):
            BitrateProfileFactor = 2;
            break;
        case(MFX_PROFILE_AV1_PRO):
            BitrateProfileFactor = 3;
            break;
        default:
            BitrateProfileFactor = 1;
        }

        const mfxU32 maxKbps = (seqTier == 0) ? TableA2[levelIndex][MAIN_BPS_INDEX] : TableA2[levelIndex][HIGH_BPS_INDEX];
        return maxKbps / 1000 * BitrateProfileFactor;
    }

    mfxF64 GetMaxFrameRateByLevel(mfxU16 CodecLevel, mfxU32 width, mfxU32 height)
    {
        assert(width != 0 && height != 0);

        const mfxU16 levelIndex     = GetLevelIndexInTable(CodecLevel);
        const mfxF64 maxDisplayRate = mfxF64(TableA1[levelIndex][MAX_DISPLAY_RATE_INDEX]);
        return maxDisplayRate / width / height;
    }

    mfxU32 GetMaxTilesByLevel(mfxU16 CodecLevel)
    {
        const mfxU16 levelIndex = GetLevelIndexInTable(CodecLevel);
        return mfxU32(TableA2[levelIndex][MAX_TILES_INDEX]);
    }

    mfxU16 GetMaxTileColsByLevel(mfxU16 CodecLevel)
    {
        const mfxU16 levelIndex = GetLevelIndexInTable(CodecLevel);
        return mfxU16(TableA2[levelIndex][MAX_TILE_COLS_INDEX]);
    }

    mfxU16 GetMinLevel(
        mfxU32 width
        , mfxU32 height
        , mfxU16 numTileCols
        , mfxU16 numTileRows
        , mfxU32 maxKbps
        , mfxU16 profile
        , mfxU16 startLevel)
    {
        assert(isValidCodecLevel(startLevel));

        mfxU16 lastLevel = startLevel;
        auto   levelIt   = LevelIndexInTable.find(startLevel);
        while (levelIt != LevelIndexInTable.end())
        {
            lastLevel         = levelIt->first;
            mfxU16 level      = levelIt->first;
            mfxU16 levelIndex = levelIt->second;

            const mfxU32 picSizeInSamplesY = width * height;
            const mfxU32 numTiles          = mfxU32(numTileCols) * mfxU32(numTileRows);
            const mfxU64 maxPicSize        = TableA1[levelIndex][MAX_PIC_SIZE_INDEX];
            const mfxU64 maxHSize          = TableA1[levelIndex][MAX_H_SIZE_INDEX];
            const mfxU64 maxVSize          = TableA1[levelIndex][MAX_V_SIZE_INDEX];
            const mfxU32 maxTileCols       = TableA2[levelIndex][MAX_TILE_COLS_INDEX];
            const mfxU32 maxTiles          = TableA2[levelIndex][MAX_TILES_INDEX];

            bool bNextLevel =
                picSizeInSamplesY > maxPicSize
                || width > maxHSize
                || height > maxVSize
                || numTileCols > maxTileCols
                || numTiles > maxTiles;

            if (!bNextLevel && maxKbps > 0)
            {
                mfxU32 maxKbpsByLevel = GetMaxKbpsByLevel(level, profile);
                // Note: HighMbps is not defined for levels below level 4.0. Try seq_tier 1 for higher Mbps
                if (level >= MFX_LEVEL_AV1_4 && maxKbps > maxKbpsByLevel)
                {
                    maxKbpsByLevel = GetMaxKbpsByLevel(level, profile, 1);
                }

                bNextLevel |= (maxKbps > maxKbpsByLevel);
            }

            if (!bNextLevel)
            {
                break;
            }
            else
            {
                levelIt++;
            }
        }

        if (levelIt != LevelIndexInTable.end())
        {
            return levelIt->first;
        }
        else
        {
            return lastLevel; // Return maximum supported level
        }
    }

    mfxU16 GetMinLevel(
        const Defaults::Param& defPar
        , mfxU16 startLevel)
    {
        const mfxU16 rc = defPar.base.GetRateControlMethod(defPar);
        mfxU32 maxKbps  = 0;
        SetIf(maxKbps, rc != MFX_RATECONTROL_CQP && rc != MFX_RATECONTROL_ICQ, [&]() { return defPar.base.GetMaxKbps(defPar); });

        const auto                res     = GetRealResolution(defPar.mvp);
        const mfxExtAV1TileParam* pTiles  = ExtBuffer::Get(defPar.mvp);
        const mfxExtAV1AuxData*   pAuxPar = ExtBuffer::Get(defPar.mvp);
        const auto                tiles   = GetNumTiles(pTiles, pAuxPar);

        return GetMinLevel(
            std::get<0>(res)
            , std::get<1>(res)
            , std::get<0>(tiles)
            , std::get<1>(tiles)
            , maxKbps
            , defPar.mvp.mfx.CodecProfile
            , startLevel);
    }

} //namespace Base
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)