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

#include "av1ehw_base_packer.h"
#include "av1ehw_base_task.h"
#include <numeric>

namespace AV1EHW
{

    namespace Base
    {
BitstreamWriter::BitstreamWriter(mfxU8* bs, mfxU32 size, mfxU8 bitOffset)
    : m_bsStart(bs)
    , m_bsEnd(bs + size)
    , m_bs(bs)
    , m_bitStart(bitOffset & 7)
    , m_bitOffset(bitOffset & 7)
    , m_bitsOutstanding(0)
    , m_BinCountsInNALunits(0)
    , m_firstBitFlag(true)
{
    assert(bitOffset < 8);
    *m_bs &= 0xFF << (8 - m_bitOffset);
}

BitstreamWriter::~BitstreamWriter()
{
}

void BitstreamWriter::Reset(mfxU8* bs, mfxU32 size, mfxU8 bitOffset)
{
    if (bs)
    {
        m_bsStart   = bs;
        m_bsEnd     = bs + size;
        m_bs        = bs;
        m_bitOffset = (bitOffset & 7);
        m_bitStart  = (bitOffset & 7);
    }
    else
    {
        m_bs        = m_bsStart;
        m_bitOffset = m_bitStart;
    }
}

void BitstreamWriter::PutBitsBuffer(mfxU32 n, void* bb, mfxU32 o)
{
    mfxU8* b = (mfxU8*)bb;
    mfxU32 N, B;

    assert(bb);

    auto SkipOffsetBytes = [&]()
    {
        N = o / 8;
        b += N;
        o &= 7;
        return o;
    };
    auto PutBitsAfterOffsetOnes = [&]()
    {
        N = (n < (8 - o)) * n;
        PutBits(8 - o, ((b[0] & (0xff >> o)) >> N));
        n -= (N + !N * (8 - o));
        ++b;
        return n;
    };
    auto PutBytesAligned = [&]()
    {
        N = n / 8;
        n &= 7;

        assert(std::ptrdiff_t(N + !!n) < std::ptrdiff_t(m_bsEnd - m_bs));
        std::copy(b, b + N, m_bs);

        m_bs += N;

        return !n;
    };
    auto PutLastByteBitsAligned = [&]()
    {
        m_bs[0] = b[N];
        m_bs[0] &= (0xff << (8 - n));
        m_bitOffset = (mfxU8)n;
        return true;
    };
    auto CopyAlignedToUnaligned = [&]()
    {
        assert(std::ptrdiff_t(n + 7 - m_bitOffset) / 8 < std::ptrdiff_t(m_bsEnd - m_bs));

        while (n >= 24)
        {
            B = ((((mfxU32)b[0] << 24) | ((mfxU32)b[1] << 16) | ((mfxU32)b[2] << 8)) >> m_bitOffset);

            m_bs[0] |= (mfxU8)(B >> 24);
            m_bs[1] = (mfxU8)(B >> 16);
            m_bs[2] = (mfxU8)(B >> 8);
            m_bs[3] = (mfxU8)B;

            m_bs += 3;
            b += 3;
            n -= 24;
        }

        while (n >= 8)
        {
            B = ((mfxU32)b[0] << 8) >> m_bitOffset;

            m_bs[0] |= (mfxU8)(B >> 8);
            m_bs[1] = (mfxU8)B;

            m_bs++;
            b++;
            n -= 8;
        }

        if (n)
            PutBits(n, (b[0] >> (8 - n)));

        return true;
    };
    auto CopyUnalignedPartToAny = [&]()
    {
        return o && SkipOffsetBytes() && PutBitsAfterOffsetOnes();
    };
    auto CopyAlignedToAligned = [&]()
    {
        return !m_bitOffset && (PutBytesAligned() || PutLastByteBitsAligned());
    };

    bool bDone =
           CopyUnalignedPartToAny()
        || CopyAlignedToAligned()
        || CopyAlignedToUnaligned();

    ThrowAssert(!bDone, "BitstreamWriter::PutBitsBuffer failed");
}

void BitstreamWriter::PutBits(mfxU32 n, mfxU32 b)
{
    assert(n <= sizeof(b) * 8);
    while (n > 24)
    {
        n -= 16;
        PutBits(16, (b >> n));
    }

    b <<= (32 - n);

    if (!m_bitOffset)
    {
        m_bs[0] = (mfxU8)(b >> 24);
        m_bs[1] = (mfxU8)(b >> 16);
    }
    else
    {
        b >>= m_bitOffset;
        n  += m_bitOffset;

        m_bs[0] |= (mfxU8)(b >> 24);
        m_bs[1]  = (mfxU8)(b >> 16);
    }

    if (n > 16)
    {
        m_bs[2] = (mfxU8)(b >> 8);
        m_bs[3] = (mfxU8)b;
    }

    m_bs += (n >> 3);
    m_bitOffset = (n & 7);
}

void BitstreamWriter::PutBit(mfxU32 b)
{
    switch(m_bitOffset)
    {
    case 0:
        m_bs[0] = (mfxU8)(b << 7);
        m_bitOffset = 1;
        break;
    case 7:
        m_bs[0] |= (mfxU8)(b & 1);
        m_bs ++;
        m_bitOffset = 0;
        break;
    default:
        if (b & 1)
            m_bs[0] |= (mfxU8)(1 << (7 - m_bitOffset));
        m_bitOffset ++;
        break;
    }
}

void BitstreamWriter::PutBitC(mfxU32 B)
{
    if (m_firstBitFlag)
        m_firstBitFlag = false;
    else
        PutBit(B);

    while (m_bitsOutstanding > 0)
    {
        PutBit(1 - B);
        m_bitsOutstanding--;
    }
}

void Packer::PackIVF(BitstreamWriter& bs, FH const& fh, mfxU32 insertHeaders, mfxVideoParam const& vp)
{
    if (insertHeaders & INSERT_IVF_SEQ)
    {
        // IVF SEQ header
        mfxU32 ivfSeqHeader[8] = {0x46494B44, 0x00200000, 0x31305641,
            (mfxU32)(fh.UpscaledWidth + (fh.FrameHeight << 16)),
            vp.mfx.FrameInfo.FrameRateExtN,
            vp.mfx.FrameInfo.FrameRateExtD,
            0/*numFramesInFile*/,
            0x00000000 };

        mfxU8 *ptr = reinterpret_cast<mfxU8 *>(ivfSeqHeader);
        for (size_t i = 0; i < 32; i++)
        {
            bs.PutBitsBuffer(8, ptr + i);
        }
    }

    // IVF PIC header
    if (insertHeaders & INSERT_IVF_FRM)
    {
        const mfxU32 ivfPicHeaderSizeInBytes = 12;
        for (size_t i = 0; i < ivfPicHeaderSizeInBytes; i++)
        {
            bs.PutBits(8, mfxU8(0));
        }
    }
}

void Packer::PackOBUHeader(BitstreamWriter& bs, AV1_OBU_TYPE obu_type, mfxU32 obu_extension_flag, ObuExtensionHeader const& oeh)
{
    bs.PutBit(0); //forbidden bit
    bs.PutBits(4, obu_type); //type
    bs.PutBit(obu_extension_flag);
    bs.PutBit(1); //obu_has_size_field
    bs.PutBit(0); //reserved

    if (obu_extension_flag) {
        bs.PutBits(3, oeh.temporal_id);
        bs.PutBits(2, oeh.spatial_id);
        bs.PutBits(3, 0);//reserved
    }
}

void Packer::PackOBUHeaderSize(BitstreamWriter& bs, mfxU32 const obu_size_in_bytes, mfxU8 const fixed_output_len)
{
    Leb128Data leb128 = {};
    EncodeLeb128(leb128, obu_size_in_bytes, fixed_output_len);

    const mfxU8 *ptr = reinterpret_cast<mfxU8 *>(&leb128.buf);
    for (mfxU8 i = 0; i < leb128.size; i++)
        bs.PutBits(8, ptr[i]);
}

inline void PackOperatingPoints(BitstreamWriter& bs, SH const& sh)
{
    bs.PutBits(5, sh.operating_points_cnt_minus_1);

    for (mfxU8 i = 0; i <= sh.operating_points_cnt_minus_1; i++)
    {
        bs.PutBits(12, sh.operating_point_idc[i]);
        bs.PutBits(5, sh.seq_level_idx[i]);
        if (sh.seq_level_idx[i] > 7)
        {
            bs.PutBits(1, sh.seq_tier[i]);
        }
    }
}

inline void PackFrameSizeInfo(BitstreamWriter& bs, FH const& fh)
{
    //number of bits required to store width/height
    mfxU8 frame_width_bits_minus_1 = 15;
    bs.PutBits(4, frame_width_bits_minus_1);
    mfxU8 frame_height_bits_minus_1 = 15;
    bs.PutBits(4, frame_height_bits_minus_1);

    // max width/height of the stream
    mfxU32 max_frame_width_minus_1 = fh.UpscaledWidth - 1;
    bs.PutBits(frame_width_bits_minus_1 + 1, max_frame_width_minus_1);
    mfxU32 max_frame_height_minus_1 = fh.FrameHeight - 1;
    bs.PutBits(frame_height_bits_minus_1 + 1, max_frame_height_minus_1);
}

inline void PackColorConfig(BitstreamWriter& bs, SH const& sh)
{

    const bool high_bitdepth = sh.color_config.BitDepth == BITDEPTH_10;
    bs.PutBit(high_bitdepth ? 1 : 0); //high_bitdepth

    if (sh.seq_profile == 2 && high_bitdepth)
        bs.PutBit(0); //twelve_bit

    if (sh.seq_profile != 1)
        bs.PutBit(0); //mono_chrome

    bs.PutBit(sh.color_config.color_description_present_flag);

    if (sh.color_config.color_description_present_flag)
    {
        bs.PutBits(8, sh.color_config.color_primaries);
        bs.PutBits(8, sh.color_config.transfer_characteristics);
        bs.PutBits(8, sh.color_config.matrix_coefficients);
    }

    bs.PutBit(sh.color_config.color_range); //color_range
    if (sh.seq_profile == 0)
        bs.PutBits(2, 0); //chroma_sample_position

    bs.PutBit(sh.color_config.separate_uv_delta_q); //separate_uv_delta_q
}

void Packer::PackSPS(BitstreamWriter& bs, SH const& sh, FH const& fh, ObuExtensionHeader const& oeh, mfxVideoParam const& vp)
{
    //alloc tmp buff for the header data
    const mfxU32 av1_max_header_size = 1024;
    std::vector<mfxU8> tmpBuf(av1_max_header_size);
    BitstreamWriter tmpBitstream(&tmpBuf[0], av1_max_header_size);

    //adding header data to tmp_buff to calculate size before adding to bitstream
    tmpBitstream.PutBits(3, sh.seq_profile); //seq_profile
    tmpBitstream.PutBit(sh.still_picture); //still_picture
    tmpBitstream.PutBit(0); //reduced_still_picture_header
    tmpBitstream.PutBit(sh.timing_info_present_flag); //timing_info_present_flag
    if (sh.timing_info_present_flag)
    {
        tmpBitstream.PutBits(32, vp.mfx.FrameInfo.FrameRateExtD); //num_units_in_display_tick 
        tmpBitstream.PutBits(32, vp.mfx.FrameInfo.FrameRateExtN); //time_scale 
        tmpBitstream.PutBit(1); //equal_picture_interval
        tmpBitstream.PutBit(1); //num_ticks_per_picture_minus_1 = 0 = uvlc(1)
        tmpBitstream.PutBit(0); //decoder_model_info_present_flag

    }
    tmpBitstream.PutBit(0); //initial_display_delay_present_flag

    PackOperatingPoints(tmpBitstream, sh);
    PackFrameSizeInfo(tmpBitstream, fh);

    tmpBitstream.PutBit(0); //frame_id_numbers_present_flag (affects FH)

    tmpBitstream.PutBit(0); //use_128x128_superblock
    tmpBitstream.PutBit(sh.enable_filter_intra); //enable_filter_intra
    tmpBitstream.PutBit(sh.enable_intra_edge_filter); //enable_intra_edge_filter
    tmpBitstream.PutBit(sh.enable_interintra_compound); //enable_interintra_compound
    tmpBitstream.PutBit(sh.enable_masked_compound); //enable_masked_compound
    tmpBitstream.PutBit(sh.enable_warped_motion); //enable_warped_motion
    tmpBitstream.PutBit(sh.enable_dual_filter); //enable_dual_filter
    tmpBitstream.PutBit(sh.enable_order_hint); //enable_order_hint

    if (sh.enable_order_hint)
    {
        tmpBitstream.PutBit(0); //enable_jnt_comp
        tmpBitstream.PutBit(fh.use_ref_frame_mvs); //enable_ref_frame_mvs
    }

    tmpBitstream.PutBit(1); //seq_choose_screen_content_tools
    tmpBitstream.PutBit(sh.seq_force_integer_mv); //seq_choose_integer_mv
    if (!sh.seq_force_integer_mv)
    {
        tmpBitstream.PutBit(0); //seq_force_integer_mv
    }

    if (sh.enable_order_hint)
    {
        tmpBitstream.PutBits(3, sh.order_hint_bits_minus1); //affects FH
    }

    tmpBitstream.PutBit(sh.enable_superres); //enable_superres
    tmpBitstream.PutBit(sh.enable_cdef); //enable_cdef
    tmpBitstream.PutBit(sh.enable_restoration); //enable_restoration

    PackColorConfig(tmpBitstream, sh);

    tmpBitstream.PutBit(0); //film_grain_params_present

    tmpBitstream.PutTrailingBits();

    const bool ext = oeh.temporal_id | oeh.spatial_id;
    PackOBUHeader(bs, OBU_SEQUENCE_HEADER, ext, oeh);

    mfxU32 const obu_size_in_bytes = (tmpBitstream.GetOffset() + 7) / 8;
    PackOBUHeaderSize(bs, obu_size_in_bytes);

    bs.PutBitsBuffer(tmpBitstream.GetOffset(), tmpBitstream.GetStart());
}

static int GetUnsignedBits(unsigned int num_values)
{
    int cat = 0;
    if (num_values <= 1)
        return 0;
    num_values--;
    while (num_values > 0) {
        cat++;
        num_values >>= 1;
    }
    return cat;
}

static void WriteUniform(BitstreamWriter& bs, int n, int v)
{
    const int l = GetUnsignedBits(n);
    const int m = (1 << l) - n;
    if (l == 0) return;
    if (v < m)
    {
        bs.PutBits(l - 1, v);
    }
    else
    {
        bs.PutBits(l - 1, m + ((v - m) >> 1));
        bs.PutBits(1, (v - m) & 1);
    }
}

mfxU16 WriteSU(int32_t value, mfxU16 n)
{
    mfxI16 signMask = 1 << (n - 1);
    if (value & signMask)
    {
        value = value - 2 * signMask;
    }
    return static_cast<mfxU16>(value);
}

inline void PackShowFrame(BitstreamWriter& bs, FH const& fh)
{
    bs.PutBit(fh.show_frame); //show_frame

    if (!fh.show_frame)
        bs.PutBit(fh.showable_frame); //showable_frame
}

inline void PackErrorResilientMode(BitstreamWriter& bs, FH const& fh)
{
    if (fh.frame_type == SWITCH_FRAME || (fh.frame_type == KEY_FRAME && fh.show_frame))
        return;
    else
        bs.PutBit(fh.error_resilient_mode); //error_resilient_mode
}

inline void PackOrderHint(BitstreamWriter& bs, SH const& sh, FH const& fh)
{
    if (!sh.enable_order_hint)
        return;

    bs.PutBits(sh.order_hint_bits_minus1 + 1, fh.order_hint); //order_hint
}

inline void PackRefFrameFlags(BitstreamWriter& bs, FH const& fh)
{
    const mfxU8 frameIsIntra = FrameIsIntra(fh);

    if (!(frameIsIntra || fh.error_resilient_mode))
        bs.PutBits(3, 0); //primary_ref_frame

    if (!(fh.frame_type == SWITCH_FRAME || (fh.frame_type == KEY_FRAME && fh.show_frame)))
        bs.PutBits(NUM_REF_FRAMES, fh.refresh_frame_flags);
}

inline void PackInterpolationFilter(BitstreamWriter& bs, FH const& fh)
{
    const mfxU8 is_filter_switchable = (fh.interpolation_filter == 4 ? 1 : 0);
    bs.PutBit(is_filter_switchable); //is_filter_switchable
    if (!is_filter_switchable)
    {
        bs.PutBits(2, fh.interpolation_filter); //interpolation_filter
    }
}

inline void PackSuperresParams(BitstreamWriter& bs, SH const& sh, FH const& fh)
{
    if (sh.enable_superres)
    {
        bs.PutBit(fh.use_superres); //use_superres
        if (fh.use_superres)
        {
            bs.PutBits(3, fh.SuperresDenom - 9);  // coded_denom
        }
    }
}

inline void PackFrameSize(
    BitstreamWriter& bs,
    SH const& sh,
    FH const& fh)
{
    if (fh.frame_size_override_flag)
    {
        bs.PutBits(sh.frame_width_bits + 1, fh.UpscaledWidth - 1); //frame_width_minus_1
        bs.PutBits(sh.frame_height_bits + 1, fh.FrameHeight - 1); //frame_height_minus_1
    }

    PackSuperresParams(bs, sh, fh);
}

inline void PackRenderSize(
    BitstreamWriter& bs)
{
    mfxU32 render_and_frame_size_different = 0;

    bs.PutBit(render_and_frame_size_different); //render_and_frame_size_different
}

inline void PackFrameSizeWithRefs(
    BitstreamWriter& bs,
    SH const& sh,
    FH const& fh)
{
    mfxU32 found_ref = 0;

    for (mfxI8 ref = 0; ref < REFS_PER_FRAME; ref++)
        bs.PutBit(found_ref); //found_ref

    PackFrameSize(bs, sh, fh);
    PackRenderSize(bs);
}

static void PackFrameRefInfo(BitstreamWriter& bs, SH const& sh, FH const& fh)
{
    if (sh.enable_order_hint)
        bs.PutBit(0); //frame_refs_short_signaling

    for (mfxU8 ref = 0; ref < REFS_PER_FRAME; ref++)
        bs.PutBits(REF_FRAMES_LOG2, fh.ref_frame_idx[ref]);

    if (fh.frame_size_override_flag && !fh.error_resilient_mode)
    {
        PackFrameSizeWithRefs(bs, sh, fh);
    }
    else
    {
        PackFrameSize(bs, sh, fh);
        PackRenderSize(bs);
    }

    bs.PutBit(fh.allow_high_precision_mv); //allow_high_precision_mv

    PackInterpolationFilter(bs, fh);

    bs.PutBit(0); //is_motion_switchable

    if (fh.use_ref_frame_mvs)
        bs.PutBit(1); //use_ref_frame_mvs
}

static void PackRefOrderHint(BitstreamWriter& bs, SH const& sh, FH const& fh)
{
    if(fh.error_resilient_mode && sh.enable_order_hint)
    {
        for (mfxU8 ref = 0; ref < NUM_REF_FRAMES; ref++)
        {
            bs.PutBits(sh.order_hint_bits_minus1 + 1, fh.ref_order_hint[ref]); //ref_order_hint[i]
        }

    }
}

inline void PackUniformTile(BitstreamWriter& bs, TileInfoAv1 const& tileInfo)
{
    assert(tileInfo.TileColsLog2 >= tileInfo.tileLimits.MinLog2TileCols);
    mfxU32 ones = tileInfo.TileColsLog2 - tileInfo.tileLimits.MinLog2TileCols;
    while (ones--)
        bs.PutBit(1); //increment_tile_cols_log2

    if (tileInfo.TileColsLog2 < tileInfo.tileLimits.MaxLog2TileCols)
        bs.PutBit(0); //end of increment_tile_cols_log2

    assert(tileInfo.TileRowsLog2 >= tileInfo.tileLimits.MinLog2TileRows);
    ones = tileInfo.TileRowsLog2 - tileInfo.tileLimits.MinLog2TileRows;
    while (ones--)
        bs.PutBit(1); //increment_tile_rows_log2

    if (tileInfo.TileRowsLog2 < tileInfo.tileLimits.MaxLog2TileRows)
        bs.PutBit(0); //end of increment_tile_rows_log2
}


inline void PackNonUniformTile(BitstreamWriter& bs, mfxU32 sbCols, mfxU32 sbRows, TileInfoAv1 const& tileInfo)
{
    mfxU32 sizeSb = 0;

    for (mfxU16 i = 0; i < tileInfo.TileCols; i++)
    {
        sizeSb = tileInfo.TileWidthInSB[i];
        WriteUniform(bs, std::min<mfxU32>(sbCols, tileInfo.tileLimits.MaxTileWidthSb), sizeSb - 1);
        sbCols -= sizeSb;
    }

    for (mfxU16 i = 0; i < tileInfo.TileRows; i++)
    {
        sizeSb = tileInfo.TileHeightInSB[i];
        WriteUniform(bs, std::min<mfxU32>(sbRows, tileInfo.tileLimits.MaxTileHeightSbNonUniform), sizeSb - 1);
        sbRows -= sizeSb;
    }
}

static void PackTileInfo(BitstreamWriter& bs, mfxU32 sbCols, mfxU32 sbRows, TileInfoAv1 const& tileInfo)
{
    bs.PutBit(tileInfo.uniform_tile_spacing_flag);

    if (tileInfo.uniform_tile_spacing_flag)
    {
        PackUniformTile(bs, tileInfo);
    }
    else
    {
        PackNonUniformTile(bs, sbCols, sbRows, tileInfo);
    }

    if (tileInfo.TileRowsLog2 || tileInfo.TileColsLog2)
    {
        bs.PutBits(tileInfo.TileColsLog2 + tileInfo.TileRowsLog2, tileInfo.context_update_tile_id); // context_update_tile_id
        bs.PutBits(2, tileInfo.TileSizeBytes - 1); // tile_size_bytes_minus_1
    }
}

inline void PackDeltaQValue(BitstreamWriter& bs, int32_t deltaQ)
{
    if (deltaQ)
    {
        bs.PutBit(1);
        bs.PutBits(7, WriteSU(deltaQ, 7));
    }
    else
        bs.PutBit(0);
}

inline void PackQuantizationParams(BitstreamWriter& bs, SH const& sh, FH const& fh)
{
    bs.PutBits(8, fh.quantization_params.base_q_idx); //base_q_idx

    PackDeltaQValue(bs, fh.quantization_params.DeltaQYDc);

    bool diff_uv_delta = false;
    if (fh.quantization_params.DeltaQUDc != fh.quantization_params.DeltaQVDc
        || fh.quantization_params.DeltaQUAc != fh.quantization_params.DeltaQVAc)
        diff_uv_delta = true;

    if (sh.color_config.separate_uv_delta_q)
        bs.PutBit(diff_uv_delta);

    PackDeltaQValue(bs, fh.quantization_params.DeltaQUDc);
    PackDeltaQValue(bs, fh.quantization_params.DeltaQUAc);

    if (diff_uv_delta)
    {
        PackDeltaQValue(bs, fh.quantization_params.DeltaQVDc);
        PackDeltaQValue(bs, fh.quantization_params.DeltaQVAc);
    }

    bs.PutBit(fh.quantization_params.using_qmatrix); //using_qmatrix
    if (fh.quantization_params.using_qmatrix)
    {
        bs.PutBits(4, fh.quantization_params.qm_y);
        bs.PutBits(4, fh.quantization_params.qm_u);
        if (sh.color_config.separate_uv_delta_q)
            bs.PutBits(4, fh.quantization_params.qm_v);
    }
}


inline void PackSegementFeatures(BitstreamWriter& bs, SegmentationParams const& seg)
{
    for (mfxU8 i = 0; i < AV1_MAX_NUM_OF_SEGMENTS; i++)
    {
        for (mfxU8 j = 0; j < SEG_LVL_MAX; j++)
        {
            bool feature_enabled = seg.FeatureMask[i] & (1 << j);
            bs.PutBit(feature_enabled);
            if (!feature_enabled)
                continue;

            auto feature_value = seg.FeatureData[i][j];
            auto bitsToRead = SEGMENTATION_FEATURE_BITS[j];
            if (SEGMENTATION_FEATURE_SIGNED[j])
                bs.PutBits(1 + bitsToRead, WriteSU(feature_value, 1 + bitsToRead));
            else
                bs.PutBits(bitsToRead, feature_value);
        }
    }
}

inline void PackSegmentationParams(BitstreamWriter& bs, FH const& fh)
{
    const auto& seg = fh.segmentation_params;

    bs.PutBit(seg.segmentation_enabled); //segmentation_enabled
    if (!seg.segmentation_enabled)
        return;

    if (fh.primary_ref_frame != PRIMARY_REF_NONE)
    {
        bs.PutBit(seg.segmentation_update_map);
        if (seg.segmentation_update_map)
            bs.PutBit(seg.segmentation_temporal_update);
        bs.PutBit(seg.segmentation_update_data);
    }

    if (seg.segmentation_update_data)
        PackSegementFeatures(bs, seg);
}

inline void PackDeltaQParams(BitstreamWriter& bs, FH const& fh)
{
    if (fh.quantization_params.base_q_idx)
        bs.PutBit(fh.delta_q_present); //delta_q_present
    if (fh.delta_q_present)
    {
        bs.PutBits(2, 0); //delta_q_res
        bs.PutBit(fh.delta_lf_present); //delta_lf_present
        bs.PutBits(2, 0); //delta_lf_res
        bs.PutBit(fh.delta_lf_multi); //delta_lf_multi
    }
}

inline void PackLoopFilterParams(BitstreamWriter& bs, FH const& fh)
{
    if (fh.CodedLossless || fh.allow_intrabc)
        return;
    
    bs.PutBits(6, fh.loop_filter_params.loop_filter_level[0]); //loop_filter_level[0]
    bs.PutBits(6, fh.loop_filter_params.loop_filter_level[1]); //loop_filter_level[1]
    if (fh.loop_filter_params.loop_filter_level[0] || fh.loop_filter_params.loop_filter_level[1])
    {
        bs.PutBits(6, fh.loop_filter_params.loop_filter_level[2]);  //loop_filter_level[2]
        bs.PutBits(6, fh.loop_filter_params.loop_filter_level[3]);  //loop_filter_level[3]
    }

    bs.PutBits(3, fh.loop_filter_params.loop_filter_sharpness); //loop_filter_sharpness
    bs.PutBit(0); //loop_filter_delta_enabled
}

void PackCdefParams(BitstreamWriter& bs, SH const& sh, FH const& fh)
{
    if (!sh.enable_cdef || fh.CodedLossless || fh.allow_intrabc)
        return;

    mfxU16 num_planes = sh.color_config.mono_chrome ? 1 : 3;
    const auto& cdef = fh.cdef_params;

    bs.PutBits(2, cdef.cdef_damping - 3); //cdef_damping_minus_3
    bs.PutBits(2, cdef.cdef_bits); //cdef_bits

    for (mfxU16 i = 0; i < (1 << cdef.cdef_bits); ++i)
    {
        bs.PutBits(4, cdef.cdef_y_pri_strength[i]); //cdef_y_pri_strength[0]
        bs.PutBits(2, cdef.cdef_y_sec_strength[i]); //cdef_y_sec_strength[0]
        if (num_planes > 1)
        {
            bs.PutBits(4, cdef.cdef_uv_pri_strength[i]); //cdef_uv_pri_strength[0]
            bs.PutBits(2, cdef.cdef_uv_sec_strength[i]); //cdef_uv_sec_strength[0]
        }
    }
}

inline void PackLRParams(BitstreamWriter& bs, SH const& sh, FH const& fh)
{
    if (fh.AllLossless || fh.allow_intrabc || !sh.enable_restoration)
        return;

    bool usesLR = false;
    bool usesChromaLR = false;

    auto const lr = fh.lr_params;
    for (auto i = 0; i < MAX_MB_PLANE; i++)
    {
        bs.PutBits(2, lr.lr_type[i]);
        if (lr.lr_type[i] != RESTORE_NONE)
        {
            usesLR = true;
            if (i > 0)
            {
                usesChromaLR = true;
            }
        }
    }

    if (usesLR)
    {
        bs.PutBits(1, lr.lr_unit_shift);

        if (sh.sbSize != 1 && lr.lr_unit_shift) // sbSize == 1 means PAK supports 128x128 size superblock
        {
            bs.PutBits(1, lr.lr_unit_extra_shift);
        }

        if (sh.color_config.subsampling_x && sh.color_config.subsampling_y && usesChromaLR)
        {
           bs.PutBits(1, lr.lr_uv_shift);
        }
    }

}

inline void PackFrameReferenceMode(BitstreamWriter& bs, FH const& fh, bool const frameIsIntra)
{
    if (frameIsIntra)
        return;

    bs.PutBit(fh.reference_select); //reference_select
}

inline void PackSkipModeParams(BitstreamWriter& bs, FH const& fh)
{
    if (fh.skipModeAllowed)
        bs.PutBit(fh.skip_mode_present); //skip_mode_present
}

inline void PackWrappedMotion(BitstreamWriter& bs, SH const& sh, bool const frameIsIntra)
{
    if (frameIsIntra)
        return;

    if (sh.enable_warped_motion)
        bs.PutBit(0); //allow_warped_motion
}

inline void PackGlobalMotionParams(BitstreamWriter& bs, bool const frameIsIntra)
{
    if (frameIsIntra)
        return;

    for (mfxU8 i = LAST_FRAME; i <= ALTREF_FRAME; i++)
        bs.PutBit(0); //is_global[7]
}

inline void PackShowExistingFrame(BitstreamWriter& bs, SH const& sh, FH const& fh)
{
    bs.PutBits(3, fh.frame_to_show_map_idx);

    if (sh.frame_id_numbers_present_flag)
        assert(false && "No support for frame_id_numbers_present_flag");
}

inline void PackFrameHeader(
    BitstreamWriter& bs
    , BitOffsets& offsets
    , SH const& sh
    , FH const& fh)
{
    //frame_type
    const mfxU8 frameIsIntra = FrameIsIntra(fh);
    bs.PutBits(2, fh.frame_type);

    PackShowFrame(bs, fh);

    PackErrorResilientMode(bs, fh);

    bs.PutBit(fh.disable_cdf_update);
    bs.PutBit(fh.allow_screen_content_tools);
    bs.PutBit(fh.frame_size_override_flag);

    PackOrderHint(bs, sh, fh);

    PackRefFrameFlags(bs, fh);

    const int allFrames = (1 << NUM_REF_FRAMES) - 1;
    if(!frameIsIntra || fh.refresh_frame_flags != allFrames)
        PackRefOrderHint(bs, sh, fh);

    if (!frameIsIntra)
        PackFrameRefInfo(bs, sh, fh);
    else
    {
        PackFrameSize(bs, sh, fh);
        PackRenderSize(bs);
        if (fh.allow_screen_content_tools && fh.UpscaledWidth == fh.FrameWidth)
            bs.PutBit(fh.allow_intrabc);
    }

    if (!fh.disable_cdf_update)
        bs.PutBit(fh.disable_frame_end_update_cdf); //disable_frame_end_update_cdf

    PackTileInfo(bs, fh.sbCols, fh.sbRows, fh.tile_info);

    //quantization_params
    offsets.QIndexBitOffset = static_cast<mfxU16>(bs.GetOffset());
    PackQuantizationParams(bs, sh, fh);

    //segmentation_params
    offsets.SegmentationBitOffset = static_cast<mfxU16>(bs.GetOffset());
    PackSegmentationParams(bs, fh);
    offsets.SegmentationBitSize = bs.GetOffset() - offsets.SegmentationBitOffset;

    PackDeltaQParams(bs, fh);

    offsets.LoopFilterParamsBitOffset = static_cast<mfxU16>(bs.GetOffset());
    PackLoopFilterParams(bs, fh);

    offsets.CDEFParamsBitOffset = static_cast<mfxU16>(bs.GetOffset());
    PackCdefParams(bs, sh, fh);
    offsets.CDEFParamsSizeInBits = bs.GetOffset() - offsets.CDEFParamsBitOffset;

    PackLRParams(bs, sh, fh);

    const mfxU8 tx_mode_select = fh.TxMode ? 1 : 0;
   
    if (!fh.CodedLossless)
        bs.PutBit(tx_mode_select); //tx_mode_select

    PackFrameReferenceMode(bs, fh, frameIsIntra);
    PackSkipModeParams(bs, fh);
    PackWrappedMotion(bs, sh, frameIsIntra);

    bs.PutBit(fh.reduced_tx_set); //reduced_tx_set

    PackGlobalMotionParams(bs, frameIsIntra);
}

void Packer::PackPPS(
    BitstreamWriter& bs
    , BitOffsets& offsets
    , const SH& sh
    , const FH& fh
    , const ObuExtensionHeader& oeh
    , mfxU32 insertHeaders)
{
    //alloc tmp buff for the header data
    const mfxU32 av1_max_header_size = 1024;
    std::vector<mfxU8> tmpBuf(av1_max_header_size);
    BitstreamWriter tmpBitstream(&tmpBuf[0], av1_max_header_size);
    BitOffsets tmp_offsets = {};

    //adding header data to tmp_buff to calculate size before adding to bitstream
    tmpBitstream.PutBit(fh.show_existing_frame);

    if (fh.show_existing_frame)
        PackShowExistingFrame(tmpBitstream, sh, fh);
    else
        PackFrameHeader(tmpBitstream, tmp_offsets, sh, fh);

    const bool obu_extension_flag = oeh.temporal_id | oeh.spatial_id;
    const mfxU32 obu_header_offset  = bs.GetOffset();
    if (insertHeaders & INSERT_FRM_OBU)
    {
        tmp_offsets.FrameHeaderOBUSizeInBits = tmpBitstream.GetOffset();
        tmpBitstream.PutAlignmentBits();
        PackOBUHeader(bs, OBU_FRAME, obu_extension_flag, oeh);
    }
    else
    {
        tmp_offsets.FrameHeaderOBUSizeInBits = tmpBitstream.GetOffset() + 1;  // trailing 1 bit is included
        tmpBitstream.PutTrailingBits(); //Trailing bit
        PackOBUHeader(bs, OBU_FRAME_HEADER, obu_extension_flag, oeh);
    }

    offsets.FrameHeaderOBUSizeByteOffset = (bs.GetOffset() >> 3);
    if (insertHeaders & INSERT_HDR)
    {
        offsets.FrameHeaderOBUSizeByteOffset += offsets.HDRHeaderByteOffset;
    }

    const mfxU32 obu_size_in_bytes = (tmpBitstream.GetOffset() + 7) / 8;
    PackOBUHeaderSize(bs, obu_size_in_bytes, fh.show_existing_frame? 0: 4);

    if (!fh.show_existing_frame)
    {
        // The offset is related to frame or frame header OBU. IVF, sequence, and other headers should not be counted.
        const mfxU32 obuPayloadOffset     = bs.GetOffset() - obu_header_offset;
        offsets.QIndexBitOffset           = obuPayloadOffset + tmp_offsets.QIndexBitOffset;
        offsets.SegmentationBitOffset     = obuPayloadOffset + tmp_offsets.SegmentationBitOffset;
        offsets.LoopFilterParamsBitOffset = obuPayloadOffset + tmp_offsets.LoopFilterParamsBitOffset;
        offsets.CDEFParamsBitOffset       = obuPayloadOffset + tmp_offsets.CDEFParamsBitOffset;
        offsets.CDEFParamsSizeInBits      = tmp_offsets.CDEFParamsSizeInBits;
        offsets.FrameHeaderOBUSizeInBits  = obuPayloadOffset + tmp_offsets.FrameHeaderOBUSizeInBits;
    }

    bs.PutBitsBuffer(tmpBitstream.GetOffset(), tmpBitstream.GetStart());
}

void Packer::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopyPtrs[MFX_EXTBUFF_CODING_OPTION_SPSPPS].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtCodingOptionSPSPPS*)pSrc;
        auto& buf_dst = *(mfxExtCodingOptionSPSPPS*)pDst;

        MFX_COPY_FIELD(SPSBuffer);
        MFX_COPY_FIELD(SPSBufSize);
        MFX_COPY_FIELD(PPSBuffer);
        MFX_COPY_FIELD(PPSBufSize);
    });
}

void Packer::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_Init
        , [this](
            StorageRW& global
            , StorageRW&) -> mfxStatus
    {
        auto ph = make_unique<MakeStorable<PackedHeaders>>(PackedHeaders{});

        global.Insert(Glob::PackedHeaders::Key, std::move(ph));
        m_pGlob = &global;

        return MFX_ERR_NONE;
    });
}

void Packer::ResetState(const FeatureBlocks& /*blocks*/, TPushRS Push)
{
    Push(BLK_Reset
        , [this](
            StorageRW& /*global*/
            , StorageRW&) -> mfxStatus
    {
        return MFX_ERR_NONE;
    });
}

void Packer::SubmitTask(const FeatureBlocks& blocks, TPushST Push)
{
    Push(BLK_SubmitTask
        , [this, &blocks](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        BitstreamWriter bitstream(m_bitstream.data(), (mfxU32)m_bitstream.size());
        PackedHeaders& ph = Glob::PackedHeaders::Get(global);
        mfxVideoParam& videoParam = Glob::VideoParam::Get(global);
        SH& sh = Glob::SH::Get(global);
        FH& fh = Task::FH::Get(s_task);
        auto& task = Task::Common::Get(s_task);
        ObuExtensionHeader oeh = {task.TemporalID, 0};

        mfxU8* start        = bitstream.GetStart();
        mfxU32 headerOffset = bitstream.GetOffset();

        PackIVF(bitstream, fh, task.InsertHeaders, videoParam);
        ph.IVF.pData  = start + headerOffset / 8;
        ph.IVF.BitLen = bitstream.GetOffset() - headerOffset;
        assert(ph.IVF.BitLen % 8 == 0);

        headerOffset = bitstream.GetOffset();
        if (task.InsertHeaders & INSERT_TD)
        {
            const bool ext = oeh.temporal_id | oeh.spatial_id;
            PackOBUHeader(bitstream, OBU_TEMPORAL_DELIMITER, ext, oeh);
            PackOBUHeaderSize(bitstream, 0);
        }
        ph.TD.pData  = start + headerOffset / 8;
        ph.TD.BitLen = bitstream.GetOffset() - headerOffset;
        assert(ph.TD.BitLen % 8 == 0);

        headerOffset = bitstream.GetOffset();
        if (IsI(task.FrameType))
        {
            PackSPS(bitstream, sh, fh, oeh, videoParam);
        }
        ph.SPS.pData  = start + headerOffset / 8;
        ph.SPS.BitLen = bitstream.GetOffset() - headerOffset;
        assert(ph.SPS.BitLen % 8 == 0);

        headerOffset = bitstream.GetOffset();
        task.Offsets.UncompressedHeaderByteOffset = headerOffset / 8;
        PackPPS(bitstream, task.Offsets, sh, fh, oeh, task.InsertHeaders);
        ph.PPS.pData = bitstream.GetStart() + headerOffset / 8;
        ph.PPS.BitLen = bitstream.GetOffset() - headerOffset;
        assert(ph.PPS.BitLen % 8 == 0);

        return MFX_ERR_NONE;
    });
}

inline mfxStatus Packer::GenerateSPS(mfxVideoParam& out, const StorageR& global)
{
    mfxExtCodingOptionSPSPPS* dst = ExtBuffer::Get(out);
    MFX_CHECK(dst, MFX_ERR_NONE);

    dst->PPSBufSize = 0;

    if (dst->SPSBuffer)
    {
        const mfxU16 tmpSize = 128;
        std::vector<mfxU8> tmpBuf(tmpSize);
        BitstreamWriter bs(tmpBuf.data(), tmpSize);

        const SH& sh = Glob::SH::Get(global);
        const FH& fh = Glob::FH::Get(global);
        ObuExtensionHeader oeh = {0};

        PackSPS(bs, sh, fh, oeh, out);
        const mfxU16 spsBufSize = mfxU16((bs.GetOffset() + 7) / 8);

        // Only thrown status could be returned from GetVideoParam() which ignores return value
        // (Check MFXVideoENCODEAV1_HW::GetVideoParam in av1ehw_base_impl.cpp)
        ThrowIf(dst->SPSBufSize < spsBufSize, MFX_ERR_NOT_ENOUGH_BUFFER);

        std::copy_n(tmpBuf.begin(), spsBufSize, dst->SPSBuffer);
        dst->SPSBufSize = spsBufSize;
    }

    return MFX_ERR_NONE;
}

void Packer::GetVideoParam(const FeatureBlocks& blocks, TPushGVP Push)
{
    Push(BLK_GenerateSPS
        , [this, &blocks](mfxVideoParam& out, StorageR& global) -> mfxStatus
    {
        return GenerateSPS(out, global);
    });
}

#define IVF_SEQ_HEADER_SIZE_BYTES 32
#define IVF_PIC_HEADER_SIZE_BYTES 12

inline void PatchIVFFrameInfo(mfxU8* IVFHeaderStart, mfxU32 frameSize, mfxU64 pts, mfxU32 insertHeaders)
{
    const bool insertIvfSeqHeader = insertHeaders & INSERT_IVF_SEQ;
    mfxU32 frameLen = frameSize - IVF_PIC_HEADER_SIZE_BYTES - ((insertIvfSeqHeader) ? IVF_SEQ_HEADER_SIZE_BYTES : 0);

    // IVF is a simple file format that transports raw data and multi-byte numbers of little-endian
    MFX_PACK_BEGIN_USUAL_STRUCT()
    struct IVF
    {
        mfxU32 len;
        mfxU64 pts;
    } ivf = {frameLen, pts};
    MFX_PACK_END()
    auto begin = (mfxU8*)&ivf;

    mfxU8 * pIVFPicHeader = insertIvfSeqHeader ? (IVFHeaderStart + IVF_SEQ_HEADER_SIZE_BYTES) : IVFHeaderStart;
    std::copy(begin, begin + sizeof(ivf), pIVFPicHeader);
}

void Packer::PostReorderTask(const FeatureBlocks& blocks, TPushPostRT Push)
{
    Push(BLK_AddRepeatedFrames
        , [this, &blocks](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        // Add more bytes to encoded bitstream will make BRC statistics to diverge with real bitstream size.
        // BRC should be notified about additional bytes, probably Skip frame DDI interface can help.
        auto& task = Task::Common::Get(s_task);
        MFX_CHECK(!task.FramesToShow.empty(), MFX_ERR_NONE);

        const SH& sh = Glob::SH::Get(global);
        mfxVideoParam& vp = Glob::VideoParam::Get(global);
        ObuExtensionHeader oeh = { task.TemporalID, 0 };
        FH                 tempFh = {};
        tempFh.show_existing_frame = 1;

        auto& tm = Glob::TaskManager::Get(global).m_tm;
        for (const auto& frame : task.FramesToShow)
        {
            mfxU8 tempData[64];
            mfxU8* dst = tempData;
            BitstreamWriter bitstream(dst, sizeof(tempData));
            mfxU32 insertHeaders = INSERT_PPS;
            tempFh.frame_to_show_map_idx = frame.FrameToShowMapIdx;
            const mfxExtAV1BitstreamParam& bsPar = ExtBuffer::Get(vp);
            if (IsOn(bsPar.WriteIVFHeaders))
            {
                insertHeaders |= INSERT_IVF_FRM;
                PackIVF(bitstream, tempFh, insertHeaders, vp);
            }

            const mfxExtAV1AuxData& auxPar = ExtBuffer::Get(vp);
            if (IsOn(auxPar.InsertTemporalDelimiter))
            {
                // Add temporal delimiter for shown frame
                const bool ext = oeh.temporal_id | oeh.spatial_id;
                PackOBUHeader(bitstream, OBU_TEMPORAL_DELIMITER, ext, oeh);
                PackOBUHeaderSize(bitstream, 0);
            }

            PackPPS(bitstream, task.Offsets, sh, tempFh, oeh, insertHeaders);
            const mfxU32 repeatedFrameSize = (bitstream.GetOffset() + 7) / 8;

            if (IsOn(bsPar.WriteIVFHeaders))
                PatchIVFFrameInfo(dst, repeatedFrameSize, frame.DisplayOrder, insertHeaders);

            MfxEncodeHW::CachedBitstream cachedBs(repeatedFrameSize, dst);
            cachedBs.isHiden = false;
            cachedBs.DisplayOrder = frame.DisplayOrder;
            tm.PushBitstream(frame.DisplayOrder, std::move(cachedBs));

            auto& repeatFrameSizesInfo = Glob::RepeatFrameSizeInfo::Get(global);
            repeatFrameSizesInfo[task.EncodedOrder] += repeatedFrameSize; // currently repeat frame size is maximum to 3(frame_header) + 2(TD) + 12(IVF frame) bytes
        }

        return MFX_ERR_NONE;
    });
}

void Packer::QueryTask(const FeatureBlocks&, TPushQT Push)
{
    Push(BLK_UpdateHeader
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        //NB: currently this block is being called after all General Feature blocks, it means
        //that only bits overwritten are possible, bit shifting and deleting will break the stream!
        mfxVideoParam&                 vp    = Glob::VideoParam::Get(global);
        const mfxExtAV1BitstreamParam& bsPar = ExtBuffer::Get(vp);
        MFX_CHECK(IsOn(bsPar.WriteIVFHeaders), MFX_ERR_NONE);

        auto& task = Task::Common::Get(s_task);
        MFX_CHECK(task.BsDataLength > 0, MFX_ERR_NONE);
        PatchIVFFrameInfo(task.pBsData, *task.pBsDataLength, task.DisplayOrder, task.InsertHeaders);

        return MFX_ERR_NONE;
    });
}

}

}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
