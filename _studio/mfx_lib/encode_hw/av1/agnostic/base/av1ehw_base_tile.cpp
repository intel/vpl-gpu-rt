// Copyright (c) 2020-2022 Intel Corporation
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

#include "av1ehw_base_constraints.h"
#include "av1ehw_base_tile.h"
#include <algorithm>
#include <numeric>

using namespace AV1EHW::Base;

namespace AV1EHW
{

inline bool IsValidTilesSize(
    const mfxU16 sbNum
    , const mfxU16 TileNum
    , const TileSizeArrayType& tileSizeInSB)
{
    mfxU32 sum = 0;
    mfxU16 idx = 0;
    while (idx < TileNum && tileSizeInSB[idx] > 0)
    {
        sum += tileSizeInSB[idx];
        idx++;
    }

    return idx == TileNum && sum == sbNum;
}

/*
* Set uniform tile size, refer to spec 5.9.15. Tile info syntax
* Return true iff the input params are applicable for uniform tile size
*/
inline bool SetUniformTileSize(
    const mfxU16 sbNum
    , const mfxU16 tileNum
    , TileSizeArrayType& tileSizeInSB)
{
    assert(tileNum > 0);

    const mfxU16 tileNumLog2 = TileLog2(mfxU16(1), tileNum);
    const mfxU16 tileSize    = (sbNum + (1 << tileNumLog2) - 1) >> tileNumLog2;

    mfxU16 cnt = 0;
    mfxU16 sum = 0;
    while (sum + tileSize < sbNum)
    {
        tileSizeInSB[cnt++] = tileSize;
        sum += tileSize;
    }
    tileSizeInSB[cnt++] = sbNum - sum;

    if (tileNum != cnt)
    {
        std::fill_n(tileSizeInSB, cnt, mfxU16(0));
        return false;
    }

    return true;
}

inline void SetNonUniformTileSize(
    const mfxU16 sbNum
    , const mfxU16 tileNum
    , TileSizeArrayType& tileSizeInSB)
{
    assert(tileNum > 0);

    mfxU16 cnt = 0;
    auto CalcTileSize = [sbNum, tileNum, &cnt]()
    {
        mfxU16 tileSize = ((cnt + 1) * sbNum) / tileNum - (cnt * sbNum) / tileNum;
        SetIf(tileSize, !tileSize, 1);
        cnt++;
        return tileSize;
    };

    std::generate(tileSizeInSB, tileSizeInSB + tileNum, CalcTileSize);
}

inline void SetTileLimits(
    const mfxU16 sbCols
    , const mfxU16 sbRows
    , const mfxU32 numTileColumns
    , const TileSizeArrayType &tileWidthInSB
    , TileLimits& tileLimits)
{
    assert(tileWidthInSB[0] > 0);

    const mfxU32 tileAreaSb   = sbCols * sbRows;
    const mfxU32 TileColsLog2 = TileLog2(mfxU32(1), numTileColumns);

    tileLimits.MaxTileWidthSb  = AV1_MAX_TILE_WIDTH_SB;
    mfxU32 maxTileAreaSb       = AV1_MAX_TILE_AREA_SB;

    tileLimits.MinLog2TileCols = TileLog2(tileLimits.MaxTileWidthSb, mfxU32(sbCols));
    tileLimits.MaxLog2TileCols = TileLog2(mfxU32(1), std::min(mfxU32(sbCols), AV1_MAX_NUM_TILE_COLS));
    tileLimits.MaxLog2TileRows = TileLog2(mfxU32(1), std::min(mfxU32(sbRows), AV1_MAX_NUM_TILE_ROWS));

    const mfxU32 minLog2Tiles  = std::max(tileLimits.MinLog2TileCols, TileLog2(maxTileAreaSb, tileAreaSb));
    tileLimits.MinLog2TileRows = mfxU32(std::max(mfxI32(minLog2Tiles) - mfxI32(TileColsLog2), mfxI32(0)));

    maxTileAreaSb = tileAreaSb;
    if (minLog2Tiles)
    {
        maxTileAreaSb >>= (minLog2Tiles + 1);
    }

    const mfxU32 widestTileSb  = *std::max_element(tileWidthInSB, tileWidthInSB + numTileColumns);
    tileLimits.MaxTileHeightSb = std::max(mfxU32(1), maxTileAreaSb / widestTileSb);
}

inline mfxU32 CheckTileLimits(
    const mfxU32 numTileColumns
    , const mfxU32 numTileRows
    , const TileSizeArrayType& tileWidthInSB
    , const TileSizeArrayType& tileHeightInSB
    , const TileLimits& tileLimits)
{
    mfxU32 invalid = 0;
    for (mfxU32 i = 0; i < numTileColumns; i++)
    {
        invalid += tileWidthInSB[i] > tileLimits.MaxTileWidthSb;
    }

    for (mfxU32 i = 0; i < numTileRows; i++)
    {
        invalid += tileHeightInSB[i] > tileLimits.MaxTileHeightSb;
    }

    mfxU32 TileColsLog2 = TileLog2(mfxU32(1), numTileColumns);
    mfxU32 TileRowsLog2 = TileLog2(mfxU32(1), numTileRows);

    invalid = CheckRangeOrClip(TileColsLog2, tileLimits.MinLog2TileCols, tileLimits.MaxLog2TileCols);
    invalid = CheckRangeOrClip(TileRowsLog2, tileLimits.MinLog2TileRows, tileLimits.MaxLog2TileRows);

    return invalid;
}

inline void CleanTileBuffers(mfxExtAV1TileParam* pTilePar, mfxExtAV1AuxData* pAuxPar)
{
    if (pTilePar)
    {
        pTilePar->NumTileRows    = 0;
        pTilePar->NumTileColumns = 0;
        pTilePar->NumTileGroups  = 0;
    }

    if (pAuxPar)
    {
        pAuxPar->UniformTileSpacing       = 0;
        pAuxPar->ContextUpdateTileIdPlus1 = 0;
        std::fill_n(pAuxPar->TileWidthInSB, sizeof(mfxExtAV1AuxData::TileWidthInSB) / sizeof(mfxU16), mfxU16(0));
        std::fill_n(pAuxPar->TileHeightInSB, sizeof(mfxExtAV1AuxData::TileHeightInSB) / sizeof(mfxU16), mfxU16(0));
        std::fill_n(pAuxPar->NumTilesPerTileGroup, sizeof(mfxExtAV1AuxData::NumTilesPerTileGroup) / sizeof(mfxU16), mfxU16(0));
    }
}

/*
* This function returns true iff bTryUniformTiles is true and input params are applicable for uniform tile size
*/
inline bool SetDefaultTileSize(
    const mfxU16 sbNum
    , const mfxU16 tileNum
    , const bool bTryUniformTiles
    , TileSizeArrayType& tileSizeInSB)
{
    if (tileNum == 0 || tileSizeInSB[0] != 0)
    {
        return false;
    }

    if (bTryUniformTiles)
    {
        const bool bSet = SetUniformTileSize(sbNum, tileNum, tileSizeInSB);
        if (bSet)
        {
            return true;
        }
    }

    SetNonUniformTileSize(sbNum, tileNum, tileSizeInSB);

    return false;
}

inline mfxU16 GetMinTileCols(const mfxU16 sbCols)
{
    return  mfx::CeilDiv(sbCols, mfxU16(AV1_MAX_TILE_WIDTH_SB));
}

inline mfxU16 GetMaxTileCols(const mfxU16 sbCols)
{
    return std::min(sbCols, mfxU16(AV1_MAX_NUM_TILE_COLS));
}

inline mfxU16 GetMinTileRows(const mfxU16 sbRows, const TileLimits& tileLimits)
{
    return mfx::CeilDiv(sbRows, mfxU16(tileLimits.MaxTileHeightSb));
}

inline mfxU16 GetMaxTileRows(const mfxU16 sbRows)
{
    return std::min(sbRows, mfxU16(AV1_MAX_NUM_TILE_ROWS));
}

inline void SetDefaultTileParams(
    const mfxU16 sbCols
    , const mfxU16 sbRows
    , mfxExtAV1TileParam& tilePar
    , mfxExtAV1AuxData* pAuxPar)
{
    SetDefault(tilePar.NumTileGroups, 1);

    if (pAuxPar != nullptr)
    {
        SetDefault(tilePar.NumTileColumns, CountTileNumber(pAuxPar->TileWidthInSB));
        SetDefault(tilePar.NumTileRows, CountTileNumber(pAuxPar->TileHeightInSB));
    }

    SetDefault(tilePar.NumTileColumns, GetMinTileCols(sbCols));

    TileSizeArrayType  tileWidthInSB = {};
    if (pAuxPar != nullptr)
    {
        std::copy_n(pAuxPar->TileWidthInSB, tilePar.NumTileColumns, tileWidthInSB);
    }

    bool bTryUniformTiles = pAuxPar == nullptr || (!IsOff(pAuxPar->UniformTileSpacing) && pAuxPar->TileWidthInSB[0] == 0 && pAuxPar->TileHeightInSB[0] == 0);
    if (!IsValidTilesSize(sbCols, tilePar.NumTileColumns, tileWidthInSB))
    {
        std::fill_n(tileWidthInSB, tilePar.NumTileColumns, mfxU16(0));
        SetDefaultTileSize(sbCols, tilePar.NumTileColumns, bTryUniformTiles, tileWidthInSB);
    }

    TileLimits tileLimits = {};
    SetTileLimits(sbCols, sbRows, tilePar.NumTileColumns, tileWidthInSB, tileLimits);

    SetDefault(tilePar.NumTileRows, GetMinTileRows(sbRows, tileLimits));

    if (pAuxPar != nullptr)
    {
        bool bUniformTiles = true;
        bUniformTiles &= SetDefaultTileSize(sbCols, tilePar.NumTileColumns, bTryUniformTiles, pAuxPar->TileWidthInSB);
        bUniformTiles &= SetDefaultTileSize(sbRows, tilePar.NumTileRows, bTryUniformTiles, pAuxPar->TileHeightInSB);

        SetDefault(pAuxPar->UniformTileSpacing, bUniformTiles ? mfxU8(MFX_CODINGOPTION_ON) : mfxU8(MFX_CODINGOPTION_OFF));
        SetDefault(pAuxPar->ContextUpdateTileIdPlus1, mfxU8(tilePar.NumTileColumns * tilePar.NumTileRows));
    }
}

inline mfxU32 CheckUniformTileSpacing(
    const mfxU16 sbCols
    , const mfxU16 sbRows
    , const mfxU16 numTileColumns
    , const mfxU16 numTileRows
    , const TileSizeArrayType& tileWidthInSB
    , const TileSizeArrayType& tileHeightInSB
    , const mfxU8 UniformTileSpacing)
{
    mfxU32 changed = 0;

    changed += Check<mfxU8
        , mfxU8(MFX_CODINGOPTION_ON)
        , mfxU8(MFX_CODINGOPTION_OFF)
        , mfxU8(MFX_CODINGOPTION_UNKNOWN)>
        (UniformTileSpacing);

    if (!IsOn(UniformTileSpacing))
    {
        return changed;
    }

    // Check if uniform tile spacing is valid
    TileSizeArrayType tileSizeInSB = {};
    if (!SetUniformTileSize(sbCols, numTileColumns, tileSizeInSB))
    {
        return ++changed;
    }

    for (mfxU32 i = 0; i < numTileColumns; ++i)
    {
        if (tileWidthInSB[i] != tileSizeInSB[i])
        {
            return ++changed;
        }
    }

    if (!SetUniformTileSize(sbRows, numTileRows, tileSizeInSB))
    {
        return ++changed;
    }

    for (mfxU32 i = 0; i < numTileRows; ++i)
    {
        if (tileHeightInSB[i] != tileSizeInSB[i])
        {
            return ++changed;
        }
    }

    return changed;
}

inline mfxU32 CheckAndFixTileAuxParams(
    const mfxU16 sbCols
    , const mfxU16 sbRows
    , const mfxU16 numTileCols
    , const mfxU16 numTileRows
    , const TileSizeArrayType& tileWidthInSB
    , const TileSizeArrayType& tileHeightInSB
    , mfxU8& UniformTileSpacing
    , mfxU8& ContextUpdateTileIdPlus1)
{
    mfxU32 changed = 0;
    if (CheckUniformTileSpacing(sbCols, sbRows, numTileCols, numTileRows, tileWidthInSB, tileHeightInSB, UniformTileSpacing))
    {
        UniformTileSpacing = 0;
        changed++;
    }

    changed += CheckMaxOrClip(ContextUpdateTileIdPlus1, mfxU8(numTileCols * numTileRows));

    return changed;
}

/*
* This function uses temporal buffers for checking in order to not set default value in ext-buffers (expected behavior for Query())
*/
mfxStatus CheckAndFixBuffers(
    const mfxU16 sbCols
    , const mfxU16 sbRows
    , const mfxU16 maxTileColsByLevel
    , const mfxU32 maxTilesByLevel
    , mfxExtAV1TileParam& tilePar
    , mfxExtAV1AuxData* pAuxPar)
{
    mfxU32 changed = 0, invalid = 0;

    // Make sure tile numbers are alinged between buffers
    auto numTiles = GetNumTiles(&tilePar, pAuxPar);
    changed += SetIf(tilePar.NumTileColumns, tilePar.NumTileColumns != std::get<0>(numTiles), std::get<0>(numTiles));
    changed += SetIf(tilePar.NumTileRows, tilePar.NumTileRows != std::get<1>(numTiles), std::get<1>(numTiles));

    // Prepare buffers for tile number and tile size check
    TileSizeArrayType  tileWidthInSB   = {};
    TileSizeArrayType  tileHeightInSB  = {};

    // If tile size does exist in input buffer, make sure tile sizes are copied after checking
    const bool bTileSizeSet = pAuxPar != nullptr && (pAuxPar->TileWidthInSB[0] != 0 || pAuxPar->TileHeightInSB[0] != 0);
    if (bTileSizeSet)
    {
        std::copy_n(pAuxPar->TileWidthInSB, tilePar.NumTileColumns, tileWidthInSB);
        std::copy_n(pAuxPar->TileHeightInSB, tilePar.NumTileRows, tileHeightInSB);
    }

    // Check number of tile columns
    const mfxU16 maxTileCols = std::min(GetMaxTileCols(sbCols), maxTileColsByLevel);
    const mfxU16 minTileCols = GetMinTileCols(sbCols);
    changed += CheckRangeOrClip(tilePar.NumTileColumns, minTileCols, maxTileCols);

    // Check tile widths
    mfxU32 sizeChanged = 0;
    const bool bTryUniformTiles = pAuxPar == nullptr || (!IsOff(pAuxPar->UniformTileSpacing) && tileWidthInSB[0] == 0 && tileHeightInSB[0] == 0);
    if (!IsValidTilesSize(sbCols, tilePar.NumTileColumns, tileWidthInSB))
    {
        sizeChanged++;
        std::fill_n(tileWidthInSB, tilePar.NumTileColumns, mfxU16(0));
        SetDefaultTileSize(sbCols, tilePar.NumTileColumns, bTryUniformTiles, tileWidthInSB);
        invalid += !IsValidTilesSize(sbCols, tilePar.NumTileColumns, tileWidthInSB);
    }

    if (invalid)
    {
        CleanTileBuffers(&tilePar, pAuxPar);
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    // Check number of tile rows
    TileLimits tileLimits = {};
    SetTileLimits(sbCols, sbRows, tilePar.NumTileColumns, tileWidthInSB, tileLimits);

    MFX_CHECK(tilePar.NumTileColumns != 0, MFX_ERR_UNKNOWN);

    const mfxU16 maxTileRows = std::min(GetMaxTileRows(sbRows), mfxU16(maxTilesByLevel / tilePar.NumTileColumns));
    const mfxU16 minTileRows = GetMinTileRows(sbRows, tileLimits);

    changed += CheckRangeOrClip(tilePar.NumTileRows, minTileRows, maxTileRows);
    changed += CheckRangeOrClip(tilePar.NumTileGroups, mfxU16(0), mfxU16(tilePar.NumTileColumns * tilePar.NumTileRows));

    // Check tile heights
    if (!IsValidTilesSize(sbRows, tilePar.NumTileRows, tileHeightInSB))
    {
        sizeChanged++;
        std::fill_n(tileHeightInSB, tilePar.NumTileRows, mfxU16(0));
        SetDefaultTileSize(sbRows, tilePar.NumTileRows, bTryUniformTiles, tileHeightInSB);
        invalid += !IsValidTilesSize(sbRows, tilePar.NumTileRows, tileHeightInSB);
    }

    if (invalid)
    {
        CleanTileBuffers(&tilePar, pAuxPar);
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    if (bTileSizeSet)
    {
        changed += sizeChanged;
        std::copy_n(tileWidthInSB, tilePar.NumTileColumns, pAuxPar->TileWidthInSB);
        std::copy_n(tileHeightInSB, tilePar.NumTileRows, pAuxPar->TileHeightInSB);
    }

    // Check tile size against limits
    invalid += CheckTileLimits(tilePar.NumTileColumns, tilePar.NumTileRows, tileWidthInSB, tileHeightInSB, tileLimits);

    if (invalid)
    {
        CleanTileBuffers(&tilePar, pAuxPar);
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    if (pAuxPar != nullptr)
    {
        changed += CheckAndFixTileAuxParams(sbCols, sbRows, tilePar.NumTileColumns, tilePar.NumTileRows, tileWidthInSB, tileHeightInSB,
            pAuxPar->UniformTileSpacing, pAuxPar->ContextUpdateTileIdPlus1);
    }

    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

inline void SetTileInfo(
    const mfxU16 sbCols
    , const mfxU16 sbRows
    , const mfxExtAV1TileParam& tilePar
    , const mfxExtAV1AuxData& auxPar
    , const EncodeCapsAv1& caps
    , TileInfoAv1& tileInfo)
{
    tileInfo = {};

    tileInfo.TileCols     = tilePar.NumTileColumns;
    tileInfo.TileRows     = tilePar.NumTileRows;
    tileInfo.TileColsLog2 = TileLog2(mfxU32(1), mfxU32(tilePar.NumTileColumns));
    tileInfo.TileRowsLog2 = TileLog2(mfxU32(1), mfxU32(tilePar.NumTileRows));

    SetTileLimits(sbCols, sbRows, tilePar.NumTileColumns, auxPar.TileWidthInSB, tileInfo.tileLimits);

    for (int i = 0; i < tilePar.NumTileColumns; i++)
    {
        tileInfo.TileWidthInSB[i] = auxPar.TileWidthInSB[i];
    }

    for (int i = 0; i < tilePar.NumTileRows; i++)
    {
        tileInfo.TileHeightInSB[i] = auxPar.TileHeightInSB[i];
    }

    tileInfo.uniform_tile_spacing_flag = CO2Flag(auxPar.UniformTileSpacing);
    tileInfo.context_update_tile_id    = auxPar.ContextUpdateTileIdPlus1 - 1;
    tileInfo.TileSizeBytes             = caps.TileSizeBytesMinus1 + 1;
}

inline void SetTileGroupsInfo(
    const mfxU32 numTileGroups
    , const mfxU32 numTiles
    , TileGroupInfos& infos)
{
    infos.clear();

    if (numTiles == 0)
    {
        infos.push_back({ 0, 0 });
        return;
    }

    if (numTileGroups <= 1 || numTileGroups > numTiles)
    {
        // several tiles in one tile group
        infos.push_back({0, numTiles - 1});
        return;
    }

    // several tiles in several tile groups
    uint32_t cnt = 0;
    uint32_t size = numTiles / numTileGroups;
    for (uint16_t i = 0; i < numTileGroups; i++)
    {
        TileGroupInfo tgi = { cnt, cnt + size - 1 };
        cnt += size;

        if (i == numTileGroups - 1)
        {
            tgi.TgEnd = numTiles - 1;
        }

        infos.push_back(std::move(tgi));
    }
}

void Tile::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_AV1_TILE_PARAM].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtAV1TileParam*)pSrc;
        auto& buf_dst = *(mfxExtAV1TileParam*)pDst;

        MFX_COPY_FIELD(NumTileRows);
        MFX_COPY_FIELD(NumTileColumns);
        MFX_COPY_FIELD(NumTileGroups);
    });
}

void Tile::SetInherited(ParamInheritance& par)
{
#define INIT_EB(TYPE)\
    if (!pSrc || !pDst) return;\
    auto& ebInit = *(TYPE*)pSrc;\
    auto& ebReset = *(TYPE*)pDst;
#define INHERIT_OPT(OPT) InheritOption(ebInit.OPT, ebReset.OPT);

    par.m_ebInheritDefault[MFX_EXTBUFF_AV1_TILE_PARAM].emplace_back(
        [](const mfxVideoParam& /*parInit*/
            , const mfxExtBuffer* pSrc
            , const mfxVideoParam& /*parReset*/
            , mfxExtBuffer* pDst)
    {
        INIT_EB(mfxExtAV1TileParam);

        INHERIT_OPT(NumTileRows);
        INHERIT_OPT(NumTileColumns);
        INHERIT_OPT(NumTileGroups);
    });
}

void Tile::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [this](mfxVideoParam& par, StorageW&, StorageRW&)
    {
        // Not check any tile related params in AuxData if pTilePar is nullptr
        mfxExtAV1TileParam* pTilePar = ExtBuffer::Get(par);
        if (!pTilePar)
        {
            return;
        }

        mfxU16 sbCols = 0, sbRows = 0;
        std::tie(sbCols, sbRows)  = GetSBNum(par);

        mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
        SetDefaultTileParams(sbCols, sbRows, *pTilePar, pAuxPar);
    });
}

void Tile::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW& global) -> mfxStatus
    {
        // Not checking any tile related params in AuxData if pTilePar is nullptr
        mfxExtAV1TileParam* pTilePar = ExtBuffer::Get(out);
        MFX_CHECK(pTilePar, MFX_ERR_NONE);

        mfxU16 sbCols = 0, sbRows = 0;
        std::tie(sbCols, sbRows)  = GetSBNum(out);

        const auto&            caps               = Glob::EncodeCaps::Get(global);
        const auto&            defchain           = Glob::Defaults::Get(global);
        const Defaults::Param& defPar             = Defaults::Param(out, caps, defchain);

        const mfxU16           minLevel           = GetMinLevel(defPar, std::max(out.mfx.CodecLevel, mfxU16(MFX_LEVEL_AV1_2)));
        const mfxU16           maxTileColsByLevel = GetMaxTileCols(minLevel);
        const mfxU32           maxTilesByLevel    = GetMaxTiles(minLevel);

        mfxExtAV1AuxData*      pAuxPar            = ExtBuffer::Get(out);

        return CheckAndFixBuffers(sbCols, sbRows, maxTileColsByLevel, maxTilesByLevel, *pTilePar, pAuxPar);
    });
}

void Tile::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_SetTileInfo
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        //In current design FH should be initialized in General feature first
        MFX_CHECK(strg.Contains(Glob::FH::Key), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(strg.Contains(Glob::EncodeCaps::Key), MFX_ERR_UNDEFINED_BEHAVIOR);

        auto& fh   = Glob::FH::Get(strg);
        auto& caps = Glob::EncodeCaps::Get(strg);

        auto& par  = Glob::VideoParam::Get(strg);
        mfxU16 sbCols = 0, sbRows = 0;
        std::tie(sbCols, sbRows)  = GetSBNum(par);

        fh.sbCols = sbCols;
        fh.sbRows = sbRows;

        const mfxExtAV1TileParam& tilePar = ExtBuffer::Get(par);
        const mfxExtAV1AuxData&   auxPar  = ExtBuffer::Get(par);
        SetTileInfo(sbCols, sbRows, tilePar, auxPar, caps, fh.tile_info);

        return MFX_ERR_NONE;
    });

    Push(BLK_SetTileGroups
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto&                     par      = Glob::VideoParam::Get(strg);
        const mfxExtAV1TileParam& tilePar  = ExtBuffer::Get(par);
        auto&                     infos    = Glob::TileGroups::GetOrConstruct(strg);
        const mfxU32              numTiles = tilePar.NumTileColumns * tilePar.NumTileRows;
        SetTileGroupsInfo(tilePar.NumTileGroups, numTiles, infos);

        return MFX_ERR_NONE;
    });
}

void Tile::AllocTask(const FeatureBlocks& blocks, TPushAT Push)
{
    Push(BLK_AllocTask
        , [this, &blocks](
            StorageR&
            , StorageRW& task) -> mfxStatus
    {
        task.Insert(Task::TileInfo::Key, new MakeStorable<Task::TileInfo::TRef>);
        task.Insert(Task::TileGroups::Key, new MakeStorable<Task::TileGroups::TRef>);
        return MFX_ERR_NONE;
    });
}

inline bool IsTileUpdated(const mfxExtAV1TileParam* pFrameTilePar)
{
    return pFrameTilePar && (pFrameTilePar->NumTileColumns > 0 || pFrameTilePar->NumTileRows > 0);
}

void Tile::InitTask(const FeatureBlocks& blocks, TPushIT Push)
{
    Push(BLK_InitTask
        , [this, &blocks](
            mfxEncodeCtrl* /*pCtrl*/
            , mfxFrameSurface1* /*pSurf*/
            , mfxBitstream* /*pBs*/
            , StorageW& global
            , StorageW& task) -> mfxStatus
    {
        mfxExtAV1TileParam* pFrameTilePar = ExtBuffer::Get(Task::Common::Get(task).ctrl);
        MFX_CHECK(IsTileUpdated(pFrameTilePar), MFX_ERR_NONE);

        auto&                      par      = Glob::VideoParam::Get(global);
        const mfxExtAV1TileParam*  pTilePar = ExtBuffer::Get(par);
        const mfxExtAV1AuxData*    pAuxPar  = ExtBuffer::Get(par);
        MFX_CHECK(pTilePar && pAuxPar, MFX_ERR_UNDEFINED_BEHAVIOR);

        mfxU16 sbCols = 0, sbRows = 0;
        std::tie(sbCols, sbRows)  = GetSBNum(par);

        mfxExtAV1AuxData* pFrameAuxPar = ExtBuffer::Get(Task::Common::Get(task).ctrl);
        mfxExtAV1AuxData  tempAuxPar   = {};
        if (!pFrameAuxPar)
        {
            pFrameAuxPar = &tempAuxPar;
        }

        const mfxU16 maxTileColsByLevel = GetMaxTileCols(par.mfx.CodecLevel);
        const mfxU32 maxTilesByLevel    = GetMaxTiles(par.mfx.CodecLevel);
        SetDefaultTileParams(sbCols, sbRows, *pFrameTilePar, pFrameAuxPar);
        mfxStatus sts = CheckAndFixBuffers(sbCols, sbRows, maxTileColsByLevel, maxTilesByLevel, *pFrameTilePar, pFrameAuxPar);
        if (sts < MFX_ERR_NONE)
        {
            // Ignore Frame Tile param and return warning if there are issues MSDK can't fix
            MFX_LOG_WARN("Issue in Frame tile settings - it's ignored!\n");
            return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        const auto& caps     = Glob::EncodeCaps::Get(global);
        auto&       tileInfo = Task::TileInfo::Get(task);
        SetTileInfo(sbCols, sbRows, *pFrameTilePar, *pFrameAuxPar, caps, tileInfo);

        return sts;
    });
}

inline void UpdateFrameTileInfo(FH& currFH, const TileInfoAv1& tileInfo)
{
    currFH.tile_info = tileInfo;
}

void Tile::PostReorderTask(const FeatureBlocks& blocks, TPushPostRT Push)
{
    Push(BLK_ConfigureTask
        , [this, &blocks](
            StorageW& /*global*/
            , StorageW& s_task) -> mfxStatus
    {
        mfxExtAV1TileParam* pFrameTilePar = ExtBuffer::Get(Task::Common::Get(s_task).ctrl);
        if (!IsTileUpdated(pFrameTilePar))
        {
            Task::TileGroups::Get(s_task) = {};
            return MFX_ERR_NONE;
        }

        auto& fh        = Task::FH::Get(s_task);
        auto& tileInfo  = Task::TileInfo::Get(s_task);
        UpdateFrameTileInfo(fh, tileInfo);

        mfxU32 numTiles       = pFrameTilePar->NumTileColumns * pFrameTilePar->NumTileColumns;
        auto&  tileGroupInfos = Task::TileGroups::Get(s_task);
        SetTileGroupsInfo(pFrameTilePar->NumTileGroups, numTiles, tileGroupInfos);

        return MFX_ERR_NONE;
    });
}

void Tile::ResetState(const FeatureBlocks& blocks, TPushRS Push)
{
    Push(BLK_ResetState
        , [this, &blocks](
            StorageRW& global
            , StorageRW&) -> mfxStatus
    {
        auto& real = Glob::RealState::Get(global);
        Glob::TileGroups::Get(real) = Glob::TileGroups::Get(global);

        return MFX_ERR_NONE;
    });
}

} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
