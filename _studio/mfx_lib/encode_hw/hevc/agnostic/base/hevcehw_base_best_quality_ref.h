// Copyright (c) 2026 Intel Corporation
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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_base.h"
#include "hevcehw_base_data.h"

namespace HEVCEHW
{
namespace Base
{
// Returns true only when an ext buffer contains real user-provided data.
// BLK_SetDefaults pre-allocates all supported ext buffers as zeroed entries via NewEB,
// so a non-null pointer alone is not sufficient — the buffer content must be non-zero.
template<class TExtBuf>
inline bool HasExtBufferData(const TExtBuf* p)
{
    if (!p || p->Header.BufferSz <= sizeof(mfxExtBuffer))
        return false;
    const mfxU8* begin = reinterpret_cast<const mfxU8*>(p) + sizeof(mfxExtBuffer);
    const mfxU8* end   = reinterpret_cast<const mfxU8*>(p) + p->Header.BufferSz;
    return std::any_of(begin, end, [](mfxU8 b){ return b != 0; });
}

class BestQualityRef
    : public FeatureBase
{
public:
#define DECL_BLOCK_LIST\
    DECL_BLOCK(SetDefaultsCallChain)
#define DECL_FEATURE_NAME "Base_BestQualityRef"
#include "hevcehw_decl_blocks.h"

    BestQualityRef(mfxU32 FeatureId)
        : FeatureBase(FeatureId)
    {}

protected:
    virtual void Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
};

} //Base
} //namespace HEVCEHW

#endif
