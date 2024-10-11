// Copyright (c) 2021-2022 Intel Corporation
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

#include "av1ehw_base_data.h"
#include "av1ehw_base_cdef.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;

namespace AV1EHW
{
namespace Base
{

inline static void TuneCDEFLowQP(uint32_t* strength, int32_t qp)
{
    if (!(qp < 90))
        assert(false && "Only called if qp < 90");

    strength[0] = 5;
    strength[1] = 41;
    strength[3] = 6;
    strength[5] = 16;
}

inline static void TuneCDEFMediumQP(const FH& bs_fh, CdefParams& cdef, uint32_t* strength, int32_t qp)
{
    if (!(qp > 130 && qp <= 140))
        assert(false && "Only called if qp > 130 && qp <= 140");

    cdef.cdef_bits = 2;
    strength[1] = 63;

    if (bs_fh.FrameWidth < 1600 && bs_fh.FrameHeight < 1600)
        strength[3] = 1;
    else
        strength[3] = 32;
}

inline static void TuneCDEFHighQP(CdefParams& cdef, uint32_t* strength, int32_t qp)
{
    if (!(qp > 140))
        assert(false && "Only called if qp > 140");

    cdef.cdef_bits = 2;
    strength[1] = 63;
    if (qp > 210)
    {
        cdef.cdef_bits = 1;
        strength[0] = 0;
    }
}

inline static void TuneCDEFUV(uint32_t* ystrength, uint32_t* uvstrength)
{
    for (uint8_t i = 0; i < CDEF_MAX_STRENGTHS; i++)
    {
        uvstrength[i] = ystrength[i];
    }
}
inline static void TuneCDEFYHighestQP(uint32_t* strength, CdefParams& cdef, int32_t qp)
{
    if (!( qp > 210 ))
        assert(false && "Only called if qp > 210");

    strength[0]    = 0;
    strength[1]    = 63;
    strength[2]    = 0;
    strength[3]    = 24;
    cdef.cdef_bits = 1;
}

inline static void TuneCDEFYHigherQP(uint32_t* strength, int32_t qp)
{
    if (!( qp > 150 ))
        assert(false && "Only called if qp > 150");

    strength[0] = 36;
    strength[1] = 63;
    strength[2] = 0;
    strength[3] = 24;
}

inline static void TuneCDEFYHighQP(uint32_t* strength, int32_t qp)
{
    if (!( qp > 140 && qp <= 150))
        assert(false && "Only called if qp > 140 && qp <= 150");

    strength[0] = 4;
    strength[1] = 12;
    strength[2] = 0;
    strength[3] = 42;
}

inline static void TuneCDEFYMediumQP(uint32_t* strength, int32_t qp)
{
    if (!( qp > 130 && qp <= 140))
        assert(false && "Only called if qp > 130 && qp <= 140");

    strength[0] = 3;
    strength[1] = 10;
    strength[2] = 0;
    strength[3] = 37;
}

inline static void TuneCDEFYLowQP(uint32_t* strength, int32_t qp)
{
    if (!( qp > 100 && qp <= 130))
        assert(false && "Only called if qp > 100 && qp <= 130");

    strength[1] = 2;
    strength[2] = 8;
    strength[3] = 20;
}

inline static void TuneCDEFUVHighQP(uint32_t* strength, int32_t qp)
{
    if (!(qp > 140))
        assert(false && "Only called if qp > 140");

    strength[0] = 0;
    strength[1] = 4;
    strength[2] = 0;
    strength[3] = 16;
}

inline static void TuneCDEFUVMediumQP(uint32_t* strength, int32_t qp)
{
    if (!(qp > 130 && qp <= 140))
        assert(false && "qp > 130 && qp <= 140");

    strength[0] = 0;
    strength[1] = 4;
    strength[2] = 0;
    strength[3] = 12;
}

inline static void TuneCDEFUVLowQP(uint32_t* strength, int32_t qp)
{
    if (!(qp <= 130))
        assert(false && "Only called if qp <= 130");

    strength[1] = 0;
    strength[2] = 4;
    strength[3] = 10;
}

void CDEF::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    using Base::Glob;
    using Base::Defaults;

    Push(BLK_SetDefaultsCallChain,
        [this](const mfxVideoParam&, mfxVideoParam&, StorageRW& strg)->mfxStatus
    {
        auto& defaults = Glob::Defaults::GetOrConstruct(strg);
        auto& bSet = defaults.SetForFeature[GetID()];
        MFX_CHECK(!bSet, MFX_ERR_NONE);

        defaults.GetCDEF.Push([](
            Defaults::TGetCDEF::TExt
            , FH& bs_fh)
        {
            const int32_t qp = bs_fh.quantization_params.base_q_idx;
            auto& cdef       = bs_fh.cdef_params;

            uint32_t YStrengths[CDEF_MAX_STRENGTHS]  = { 0 };
            uint32_t UVStrengths[CDEF_MAX_STRENGTHS] = { 0 };

            if (bs_fh.frame_type == KEY_FRAME)
            {
                YStrengths[0]  = 36;
                YStrengths[1]  = 50;
                YStrengths[2]  = 0;
                YStrengths[3]  = 24;
                YStrengths[4]  = 8;
                YStrengths[5]  = 17;
                YStrengths[6]  = 4;
                YStrengths[7]  = 9;
                cdef.cdef_bits = 3;

                if (qp < 90)
                {
                    TuneCDEFLowQP(YStrengths, qp);
                }
                else if (qp > 140)
                {
                    TuneCDEFHighQP(cdef, YStrengths, qp);
                }
                else if (qp > 130)
                {
                    TuneCDEFMediumQP(bs_fh, cdef, YStrengths, qp);
                }
                if (bs_fh.FrameWidth < 1600 && bs_fh.FrameHeight < 1600)
                {
                    YStrengths[3] = 5;
                }

                TuneCDEFUV(YStrengths, UVStrengths);
            }
            else
            {
                YStrengths[0]  = 0;
                YStrengths[1]  = 1;
                YStrengths[2]  = 8;
                YStrengths[3]  = 16;
                UVStrengths[0] = 0;
                UVStrengths[1] = 0;
                UVStrengths[2] = 4;
                UVStrengths[3] = 8;
                cdef.cdef_bits = 2;

                if (qp > 210)
                {
                    TuneCDEFYHighestQP(YStrengths, cdef, qp);
                    TuneCDEFUV(YStrengths, UVStrengths);
                }
                else if (qp > 150)
                {
                    TuneCDEFYHigherQP(YStrengths, qp);
                    TuneCDEFUV(YStrengths, UVStrengths);
                }
                else if (qp > 140)
                {
                    TuneCDEFYHighQP(YStrengths, qp);
                    TuneCDEFUVHighQP(UVStrengths, qp);
                }
                else if (qp > 130)
                {
                    TuneCDEFYMediumQP(YStrengths, qp);
                    TuneCDEFUVMediumQP(UVStrengths, qp);
                }
                else if (qp > 100)
                {
                    TuneCDEFYLowQP(YStrengths, qp);
                    TuneCDEFUVLowQP(UVStrengths, qp);
                }
            }

            for (mfxU8 i = 0; i < CDEF_MAX_STRENGTHS; i++)
            {
                cdef.cdef_y_pri_strength[i]  = YStrengths[i] / CDEF_STRENGTH_DIVISOR;
                cdef.cdef_y_sec_strength[i]  = YStrengths[i] % CDEF_STRENGTH_DIVISOR;
                cdef.cdef_uv_pri_strength[i] = UVStrengths[i] / CDEF_STRENGTH_DIVISOR;
                cdef.cdef_uv_sec_strength[i] = UVStrengths[i] % CDEF_STRENGTH_DIVISOR;
            }

            cdef.cdef_damping = (qp >> 6) + 3;
        });

        return MFX_ERR_NONE;
    });
}

} //namespace Base
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
