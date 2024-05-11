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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base_packer.h"
#include "av1ehw_base_hdr.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;

inline void SetDefaultMasteringDisplayColourVolume(mfxExtMasteringDisplayColourVolume* pMDCV)
{
    SetDefault<mfxU32>(pMDCV->MaxDisplayMasteringLuminance, 1u);
    SetDefault<mfxU32>(pMDCV->MinDisplayMasteringLuminance, 1u);
}

inline void SetDefaultContentLightLevel(mfxExtContentLightLevelInfo* pCLLI)
{
    SetDefault<mfxU16>(pCLLI->MaxContentLightLevel, 1u);
    SetDefault<mfxU16>(pCLLI->MaxPicAverageLightLevel, 1u);
}

inline mfxStatus CheckAndFixMasteringDisplayColourVolumeInfo(mfxExtMasteringDisplayColourVolume* pMDCV)
{
    mfxU32 changed = 0;

    changed += CheckOrZero<mfxU16, MFX_PAYLOAD_OFF, MFX_PAYLOAD_IDR>(pMDCV->InsertPayloadToggle);
    changed += CheckMaxOrClip(pMDCV->WhitePointX, 50000u);
    changed += CheckMaxOrClip(pMDCV->WhitePointY, 50000u);
    changed += CheckMaxOrClip(pMDCV->DisplayPrimariesX[0], 50000u);
    changed += CheckMaxOrClip(pMDCV->DisplayPrimariesX[1], 50000u);
    changed += CheckMaxOrClip(pMDCV->DisplayPrimariesX[2], 50000u);
    changed += CheckMaxOrClip(pMDCV->DisplayPrimariesY[0], 50000u);
    changed += CheckMaxOrClip(pMDCV->DisplayPrimariesY[1], 50000u);
    changed += CheckMaxOrClip(pMDCV->DisplayPrimariesY[2], 50000u);

    return changed ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

inline mfxStatus CheckAndFixContentLightLevelInfo(mfxExtContentLightLevelInfo* pCLLI)
{
    mfxU32 changed = 0;

    changed += CheckOrZero<mfxU16, MFX_PAYLOAD_OFF, MFX_PAYLOAD_IDR>(pCLLI->InsertPayloadToggle);
    changed += CheckMinOrClip(pCLLI->MaxContentLightLevel, 1u);
    changed += CheckMaxOrClip(pCLLI->MaxContentLightLevel, 65535u);
    changed += CheckMinOrClip(pCLLI->MaxPicAverageLightLevel, 1u);
    changed += CheckMaxOrClip(pCLLI->MaxPicAverageLightLevel, 65535u);

    return changed ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

void Hdr::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtContentLightLevelInfo*)pSrc;
        auto& buf_dst = *(mfxExtContentLightLevelInfo*)pDst;

        MFX_COPY_FIELD(InsertPayloadToggle);
        MFX_COPY_FIELD(MaxContentLightLevel);
        MFX_COPY_FIELD(MaxPicAverageLightLevel);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtMasteringDisplayColourVolume*)pSrc;
        auto& buf_dst = *(mfxExtMasteringDisplayColourVolume*)pDst;

        MFX_COPY_FIELD(InsertPayloadToggle);
        MFX_COPY_FIELD(WhitePointX);
        MFX_COPY_FIELD(WhitePointY);
        MFX_COPY_FIELD(MaxDisplayMasteringLuminance);
        MFX_COPY_FIELD(MinDisplayMasteringLuminance);
        MFX_COPY_ARRAY_FIELD(DisplayPrimariesX);
        MFX_COPY_ARRAY_FIELD(DisplayPrimariesY);
    });
}

void Hdr::MetadataType(BitstreamWriter& bs, mfxU32 const value)
{
    Leb128Data leb128 = {};
    EncodeLeb128(leb128, value);

    const mfxU8* ptr = reinterpret_cast<mfxU8*>(&leb128.buf);
    for (mfxU8 i = 0; i < leb128.size; i++)
        bs.PutBits(8, ptr[i]);
}

void Hdr::PackHDR(
    BitstreamWriter& bs, ObuExtensionHeader const& oeh
    , mfxExtMasteringDisplayColourVolume const& DisplayColour)
{
    std::vector<mfxU8> tmpBuf(HDR_SIZE);
    BitstreamWriter tmpBitstream(&tmpBuf[0], HDR_SIZE);

    MetadataType(tmpBitstream, METADATA_TYPE_HDR_MDCV);

    for (int i = 0; i < 3; i++)
    {
        tmpBitstream.PutBits(16, DisplayColour.DisplayPrimariesX[i]);
        tmpBitstream.PutBits(16, DisplayColour.DisplayPrimariesY[i]);
    }
    tmpBitstream.PutBits(16, DisplayColour.WhitePointX);
    tmpBitstream.PutBits(16, DisplayColour.WhitePointY);
    tmpBitstream.PutBits(32, DisplayColour.MaxDisplayMasteringLuminance);
    tmpBitstream.PutBits(32, DisplayColour.MinDisplayMasteringLuminance);

    tmpBitstream.PutTrailingBits();

    const bool obu_extension_flag = oeh.temporal_id | oeh.spatial_id;
    Packer::PackOBUHeader(bs, OBU_METADATA, obu_extension_flag, oeh);

    mfxU32 const obu_size_in_bytes = (tmpBitstream.GetOffset() + 7) / 8;
    Packer::PackOBUHeaderSize(bs, obu_size_in_bytes);

    bs.PutBitsBuffer(tmpBitstream.GetOffset(), tmpBitstream.GetStart());
}

void Hdr::PackHDR(
    BitstreamWriter& bs, ObuExtensionHeader const& oeh
    , mfxExtContentLightLevelInfo const& LightLevel)
{
    std::vector<mfxU8> tmpBuf(HDR_SIZE);
    BitstreamWriter tmpBitstream(&tmpBuf[0], HDR_SIZE);

    MetadataType(tmpBitstream, METADATA_TYPE_HDR_CLL);

    tmpBitstream.PutBits(16, LightLevel.MaxContentLightLevel);
    tmpBitstream.PutBits(16, LightLevel.MaxPicAverageLightLevel);

    tmpBitstream.PutTrailingBits();

    const bool obu_extension_flag = oeh.temporal_id | oeh.spatial_id;
    Packer::PackOBUHeader(bs, OBU_METADATA, obu_extension_flag, oeh);

    mfxU32 const obu_size_in_bytes = (tmpBitstream.GetOffset() + 7) / 8;
    Packer::PackOBUHeaderSize(bs, obu_size_in_bytes);

    bs.PutBitsBuffer(tmpBitstream.GetOffset(), tmpBitstream.GetStart());
}

void Hdr::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaultsMDCV
        , [](mfxVideoParam& par, StorageW& /*strg*/, StorageRW&)
        {
            mfxExtMasteringDisplayColourVolume* pMDCV = ExtBuffer::Get(par);
            MFX_CHECK(pMDCV, MFX_ERR_NONE);
            SetDefaultMasteringDisplayColourVolume(pMDCV);

            return MFX_ERR_NONE;
        });

    Push(BLK_SetDefaultsCLLI
        , [](mfxVideoParam& par, StorageW& /*strg*/, StorageRW&)
        {
            mfxExtContentLightLevelInfo* pCLLI = ExtBuffer::Get(par);
            MFX_CHECK(pCLLI, MFX_ERR_NONE);
            SetDefaultContentLightLevel(pCLLI);

            return MFX_ERR_NONE;
        });
}

void Hdr::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFixMDCV
        , [](const mfxVideoParam& /*in*/, mfxVideoParam& par, StorageW& /*global*/) -> mfxStatus
    {
        mfxExtMasteringDisplayColourVolume* pMDCV = ExtBuffer::Get(par);
        MFX_CHECK(pMDCV, MFX_ERR_NONE);
        return CheckAndFixMasteringDisplayColourVolumeInfo(pMDCV);
    });

    Push(BLK_CheckAndFixCLLI
        , [](const mfxVideoParam& /*in*/, mfxVideoParam& par, StorageW& /*global*/) -> mfxStatus
    {
        mfxExtContentLightLevelInfo* pCLLI = ExtBuffer::Get(par);
        MFX_CHECK(pCLLI, MFX_ERR_NONE);
        return CheckAndFixContentLightLevelInfo(pCLLI);
    });
}

void Hdr::InitTask(const FeatureBlocks& blocks, TPushIT Push)
{
    Push(BLK_InitTaskMDCV
        , [this, &blocks](
            mfxEncodeCtrl* /*pCtrl*/
            , mfxFrameSurface1* /*pSurf*/
            , mfxBitstream* /*pBs*/
            , StorageW& /*global*/
            , StorageW& task) -> mfxStatus
        {
            mfxExtMasteringDisplayColourVolume* pMDCV = ExtBuffer::Get(Task::Common::Get(task).ctrl);
            MFX_CHECK(pMDCV, MFX_ERR_NONE);
            SetDefaultMasteringDisplayColourVolume(pMDCV);
            return CheckAndFixMasteringDisplayColourVolumeInfo(pMDCV);
        });

    Push(BLK_InitTaskCLLI
        , [this, &blocks](
            mfxEncodeCtrl* /*pCtrl*/
            , mfxFrameSurface1* /*pSurf*/
            , mfxBitstream* /*pBs*/
            , StorageW& /*global*/
            , StorageW& task) -> mfxStatus
        {
            mfxExtContentLightLevelInfo* pCLLI = ExtBuffer::Get(Task::Common::Get(task).ctrl);
            MFX_CHECK(pCLLI, MFX_ERR_NONE);
            SetDefaultContentLightLevel(pCLLI);
            return CheckAndFixContentLightLevelInfo(pCLLI);
        });
}

void Hdr::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_InsertPayloads
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);
        const mfxExtMasteringDisplayColourVolume* pMDCV = ExtBuffer::Get(task.ctrl);
        bool bInsertMDCV = false;
        if (pMDCV != NULL)
        {
            bInsertMDCV = true;
        }
        const mfxExtContentLightLevelInfo* pCLLI = ExtBuffer::Get(task.ctrl);
        bool bInsertCLLI = false;
        if (pCLLI != NULL)
        {
            bInsertCLLI = true;
        }

        mfxVideoParam& videoParam = Glob::VideoParam::Get(global);
        SetDefault(pMDCV, (const mfxExtMasteringDisplayColourVolume*)ExtBuffer::Get(videoParam));
        SetDefault(pCLLI, (const mfxExtContentLightLevelInfo*)ExtBuffer::Get(videoParam));

        bInsertMDCV |= (pMDCV->InsertPayloadToggle == MFX_PAYLOAD_IDR) && (task.FrameType & MFX_FRAMETYPE_IDR);
        bInsertCLLI |= (pCLLI->InsertPayloadToggle == MFX_PAYLOAD_IDR) && (task.FrameType & MFX_FRAMETYPE_IDR);

        MFX_CHECK(bInsertMDCV || bInsertCLLI, MFX_ERR_NONE);

        BitstreamWriter bs(m_buf.data(), (mfxU32)m_buf.size(), 0);
        PackedHeaders& ph = Glob::PackedHeaders::Get(global);
        ObuExtensionHeader oeh = { task.TemporalID, 0 };

        mfxU8* start = bs.GetStart();
        mfxU32 headerOffset = bs.GetOffset();

        if (bInsertMDCV)
        {
            PackHDR(bs, oeh, *pMDCV);
        }
        if (bInsertCLLI)
        {
            PackHDR(bs, oeh, *pCLLI);
        }

        task.Offsets.HDRHeaderByteOffset = (bs.GetOffset() >> 3);

        ph.HDR.pData = start + headerOffset / 8;
        ph.HDR.BitLen = bs.GetOffset() - headerOffset;
        assert(ph.HDR.BitLen % 8 == 0);
        task.InsertHeaders |= INSERT_HDR;

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
