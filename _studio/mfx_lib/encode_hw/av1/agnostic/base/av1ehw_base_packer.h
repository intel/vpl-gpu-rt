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

#include "av1ehw_base.h"
#include "av1ehw_base_data.h"
#include <array>

namespace AV1EHW
{
namespace Base
{
    struct Leb128Data {
        mfxU64 buf;
        mfxU8  size;
    };

    // leb128 - [in] Leb128Data to hold encoded data
    // value - [in] integer value to encoded as leb128
    // fixed_output_len - [optional in] fixed len for the output, WA for driver part (value = len, 0 - not used)
    // return - N/A
    inline void EncodeLeb128(Leb128Data& leb128, uint64_t value, const mfxU8 fixed_output_len = 0) {
        mfxU8* buf = reinterpret_cast<mfxU8*>(&(leb128.buf));
        mfxU8& cnt = leb128.size;

        cnt = 0;
        if (!fixed_output_len)
        {
            // general encoding
            do {
                buf[cnt] = value & 0x7fU;
                if (value >>= 7)
                {
                    buf[cnt] |= 0x80U;
                }
                cnt++;
            } while (value);
        }
        else
        {
            // WA to get fixed len of output
            mfxU8 value_byte_count = 0;
            do {
                buf[value_byte_count++] = value & 0x7fU;
                value >>= 7;
            } while (value);

            for (int i = 0; i < fixed_output_len - 1; i++)
            {
                buf[i] |= 0x80U;
                cnt++;
            }
            cnt++;
        }
    }

    class BitstreamWriter
        : public IBsWriter
    {
    public:
        BitstreamWriter(mfxU8* bs, mfxU32 size, mfxU8 bitOffset = 0);
        ~BitstreamWriter();

        virtual void PutBits(mfxU32 n, mfxU32 b) override;
        void PutBitsBuffer(mfxU32 n, void* b, mfxU32 offset = 0);
        virtual void PutBit(mfxU32 b) override;

        mfxU32 GetOffset() { return mfxU32(m_bs - m_bsStart) * 8 + m_bitOffset - m_bitStart; }
        mfxU8* GetStart() { return m_bsStart; }
        mfxU8* GetEnd() { return m_bsEnd; }

        void Reset(mfxU8* bs = 0, mfxU32 size = 0, mfxU8 bitOffset = 0);
        void PutBitC(mfxU32 B);

        void PutTrailingBits()
        {
            PutBit(1); //trailing_one_bit
            while (GetOffset() & 7)
                PutBit(0); //trailing_zero_bit
        }

        void PutAlignmentBits()
        {
            while (GetOffset() & 7)
                PutBit(0); //Alignment_bit
        }

        void AddInfo(mfxU32 key, mfxU32 value)
        {
            if (m_pInfo)
                m_pInfo[0][key] = value;
        }

        void SetInfo(std::map<mfxU32, mfxU32> *pInfo)
        {
            m_pInfo = pInfo;
        }

    private:
        mfxU8* m_bsStart;
        mfxU8* m_bsEnd;
        mfxU8* m_bs;
        mfxU8  m_bitStart;
        mfxU8  m_bitOffset;

        mfxU32 m_bitsOutstanding;
        mfxU32 m_BinCountsInNALunits;
        bool   m_firstBitFlag;
        std::map<mfxU32, mfxU32> *m_pInfo = nullptr;
    };

    class Packer
        : public FeatureBase
    {
    public:
#define DECL_BLOCK_LIST\
    DECL_BLOCK(Init             )\
    DECL_BLOCK(Reset            )\
    DECL_BLOCK(AddRepeatedFrames)\
    DECL_BLOCK(GenerateSPS      )\
    DECL_BLOCK(SubmitTask       )\
    DECL_BLOCK(UpdateHeader     )
#define DECL_FEATURE_NAME "Base_Packer"
#include "av1ehw_decl_blocks.h"

        Packer(mfxU32 id)
            : FeatureBase(id)
        {}

    //protected:
        static const mfxU32 BITSTREAM_SIZE = 1024;
        static const mfxU32 SPS_ES_SIZE = 1024;
        static const mfxU32 PPS_ES_SIZE = 128;
        static const mfxU32 ES_SIZE =
            SPS_ES_SIZE
            + PPS_ES_SIZE;
        std::array<mfxU8, ES_SIZE> m_es;
        mfxU8 *m_pRTBufBegin = nullptr
            , *m_pRTBufEnd = nullptr;
        NotNull<StorageW*> m_pGlob;
        std::array<mfxU8, BITSTREAM_SIZE> m_bitstream;

        virtual void SetSupported(ParamSupport& par) override;
        virtual void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override;
        virtual void ResetState(const FeatureBlocks& blocks, TPushRS Push) override;
        virtual void PostReorderTask(const FeatureBlocks& blocks, TPushPostRT Push) override;
        virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
        virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;
        virtual void GetVideoParam(const FeatureBlocks& blocks, TPushGVP Push) override;

        void      PackIVF(BitstreamWriter& bs, const FH& fh, mfxU32 insertHeaders, const mfxVideoParam& vp);
        void      PackSPS(BitstreamWriter& bs, const SH& sh, const FH& fh, const ObuExtensionHeader& oeh, const mfxVideoParam& vp);
        void      PackPPS(BitstreamWriter& bs, BitOffsets& offsets, const SH& sh, const FH& fh, const ObuExtensionHeader& oeh, mfxU32 insertHeaders);
        mfxStatus GenerateSPS(mfxVideoParam& out,  const StorageR& global);

        static void PackOBUHeader(BitstreamWriter& bs, AV1_OBU_TYPE obu_type, mfxU32 obu_extension_flag, const ObuExtensionHeader& oeh);
        static void PackOBUHeaderSize(BitstreamWriter& bs, const mfxU32 obu_size_in_bytes, const mfxU8 fixed_output_len = 0);
        static bool PutBit (BitstreamWriter& bs, mfxU32 b) { bs.PutBit(!!b); return true; };
        static bool PutBits(BitstreamWriter& bs, mfxU32 n, mfxU32 b) { if (n) bs.PutBits(n, b); return !!n; };
    };

} //Base
} //namespace AV1EHW

#endif
