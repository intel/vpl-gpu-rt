// Copyright (c) 2012-2020 Intel Corporation
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

#include "umc_defs.h"
#ifdef MFX_ENABLE_H265_VIDEO_DECODE

#ifndef __UMC_H265_DEC_TABLES_H__
#define __UMC_H265_DEC_TABLES_H__

#include "umc_h265_dec_defs.h"

namespace UMC_HEVC_DECODER
{
// Scaling list initialization scan lookup table
extern const uint16_t g_sigLastScanCG32x32[64];
// Scaling list initialization scan lookup table
extern const uint16_t ScanTableDiag4x4[16];
// Default scaling list 8x8 for intra prediction
extern const int32_t g_quantIntraDefault8x8[64];
// Default scaling list 8x8 for inter prediction
extern const int32_t g_quantInterDefault8x8[64];
// Default scaling list 4x4
extern const int32_t g_quantTSDefault4x4[16];

// Scaling list table sizes
static const uint32_t g_scalingListSize [4] = {16, 64, 256, 1024};
// Number of possible scaling lists of different sizes
static const uint32_t g_scalingListNum[SCALING_LIST_SIZE_NUM]={6, 6, 6, 2};

// Sample aspect ratios by aspect_ratio_idc index. HEVC spec E.3.1
const uint16_t SAspectRatio[17][2] =
{
    { 0,  0}, { 1,  1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, { 24, 11},
    {20, 11}, {32, 11}, {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160, 99},
    {4,   3}, {3,   2}, {2,   1}
};

// Inverse QP scale lookup table
extern const uint16_t g_invQuantScales[6];            // IQ(QP%6)

#ifndef MFX_VA
// Prediction unit partition index increment inside of CU
extern const uint32_t g_PUOffset[8];

#define QUANT_IQUANT_SHIFT    20 // Q(QP%6) * IQ(QP%6) = 2^20
#define QUANT_SHIFT           14 // Q(4) = 2^14
#define SCALE_BITS            15 // Inherited from TMuC, pressumably for fractional bit estimates in RDOQ
#define MAX_TR_DYNAMIC_RANGE  15 // Maximum transform dynamic range (excluding sign bit)

// Luma to chroma QP scale lookup table. HEVC spec 8.6.1
extern const uint8_t g_ChromaScale[2][58];

// chroma 422 pred mode from Luma IntraPredMode table 8-3 of spec
extern const uint8_t g_Chroma422IntraPredModeC[INTRA_DM_CHROMA_IDX];

// Lookup table used for decoding coefficients and groups of coefficients
extern ALIGN_DECL(32) const uint8_t scanGCZr[128];
// XY coefficient position inside of transform block lookup table
extern const uint32_t g_GroupIdx[32];
// Group index inside of transfrom block lookup table
extern const uint32_t g_MinInGroup[10];

// clip x, such that 0 <= x <= maximum luma value
// ML: OPT: called in hot loops, compiler does not seem to always honor 'inline'
// ML: OPT: TODO: Make sure compiler recognizes saturation idiom for vectorization
template <typename T>
static T inline H265_FORCEINLINE ClipY(T Value, int c_bitDepth = 8)
{
    Value = (Value < 0) ? 0 : Value;
    const int c_Mask = ((1 << c_bitDepth) - 1);
    Value = (Value >= c_Mask) ? c_Mask : Value;
    return ( Value );
}

// clip x, such that 0 <= x <= maximum chroma value
template <typename T>
static T inline H265_FORCEINLINE ClipC(T Value, int c_bitDepth = 8)
{
    Value = (Value < 0) ? 0 : Value;
    const int c_Mask = ((1 << c_bitDepth) - 1);
    Value = (Value >= c_Mask) ? c_Mask : Value;
    return ( Value );
}

// Lookup table for converting number log2(number)-2
extern const int8_t g_ConvertToBit[MAX_CU_SIZE + 1];   // from width to log2(width)-2

// Convert chroma samples width to luma samples with chroma_format_idc index
const uint32_t SubWidthC[4]  = { 1, 2, 2, 1 };
// Convert chroma samples height to luma samples with chroma_format_idc index
const uint32_t SubHeightC[4] = { 1, 2, 1, 1 };

// cabac tables and enums
// Syntax element type for HEVC
enum
{
    SPLIT_CODING_UNIT_FLAG_HEVC     = 0,
    SKIP_FLAG_HEVC                  = 1,
    MERGE_FLAG_HEVC                 = 2,
    MERGE_IDX_HEVC                  = 3,
    PART_SIZE_HEVC                  = 4,
    AMP_SPLIT_POSITION_HEVC         = 5,
    PRED_MODE_HEVC                  = 6,
    INTRA_LUMA_PRED_MODE_HEVC       = 7,
    INTRA_CHROMA_PRED_MODE_HEVC     = 8,
    INTER_DIR_HEVC                  = 9,
    MVD_HEVC                        = 10,
    REF_FRAME_IDX_HEVC              = 11,
    DQP_HEVC                        = 12,
    QT_CBF_HEVC                     = 13,
    QT_ROOT_CBF_HEVC                = 14,
    SIG_COEFF_GROUP_FLAG_HEVC       = 15,
    SIG_FLAG_HEVC                   = 16,
    LAST_X_HEVC                     = 17,
    LAST_Y_HEVC                     = 18,
    ONE_FLAG_HEVC                   = 19,
    ABS_FLAG_HEVC                   = 20,
    MVP_IDX_HEVC                    = 21,
    SAO_MERGE_FLAG_HEVC             = 22,
    SAO_TYPE_IDX_HEVC               = 23,
    TRANS_SUBDIV_FLAG_HEVC          = 24,
    TRANSFORM_SKIP_HEVC             = 25,
    TRANSQUANT_BYPASS_HEVC          = 26,
    CU_CHROMA_QP_OFFSET_FLAG        = 27,
    CU_CHROMA_QP_OFFSET_IDX         = 28,
    MAIN_SYNTAX_ELEMENT_NUMBER_HEVC
};

#define NUM_CTX 188

// CABAC contexts initialization values offset in a common table of all contexts
// ML: OPT: Moved into header to allow accesses be resolved at compile time
const
uint32_t ctxIdxOffsetHEVC[MAIN_SYNTAX_ELEMENT_NUMBER_HEVC] =
{
    0,   // SPLIT_CODING_UNIT_FLAG_HEVC
    3,   // SKIP_FLAG_HEVC
    6,   // MERGE_FLAG_HEVC
    7,   // MERGE_IDX_HEVC
    8,   // PART_SIZE_HEVC
    12,  // AMP_SPLIT_POSITION_HEVC
    13,  // PRED_MODE_HEVC
    14,  // INTRA_LUMA_PRED_MODE_HEVC
    15,  // INTRA_CHROMA_PRED_MODE_HEVC
    17,  // INTER_DIR_HEVC
    22,  // MVD_HEVC
    24,  // REF_FRAME_IDX_HEVC
    26,  // DQP_HEVC
    29,  // QT_CBF_HEVC
    39,  // QT_ROOT_CBF_HEVC
    40,  // SIG_COEFF_GROUP_FLAG_HEVC
    44,  // SIG_FLAG_HEVC
    86,  // LAST_X_HEVC
    116, // LAST_Y_HEVC
    146, // ONE_FLAG_HEVC
    170, // ABS_FLAG_HEVC
    176, // MVP_IDX_HEVC
    178, // SAO_MERGE_FLAG_HEVC
    179, // SAO_TYPE_IDX_HEVC
    180, // TRANS_SUBDIV_FLAG_HEVC
    183, // TRANSFORM_SKIP_HEVC
    185, // TRANSQUANT_BYPASS_HEVC
    186, // CU_CHROMA_QP_OFFSET_FLAG
    187, // CU_CHROMA_QP_OFFSET_IDX
};

// LPS precalculated probability ranges. HEVC spec 9.3.4.3.1
const
uint8_t rangeTabLPSH265[128][4]=
{
    { 128, 176, 208, 240},
    { 128, 176, 208, 240},
    { 128, 167, 197, 227},
    { 128, 167, 197, 227},
    { 128, 158, 187, 216},
    { 128, 158, 187, 216},
    { 123, 150, 178, 205},
    { 123, 150, 178, 205},
    { 116, 142, 169, 195},
    { 116, 142, 169, 195},
    { 111, 135, 160, 185},
    { 111, 135, 160, 185},
    { 105, 128, 152, 175},
    { 105, 128, 152, 175},
    { 100, 122, 144, 166},
    { 100, 122, 144, 166},
    {  95, 116, 137, 158},
    {  95, 116, 137, 158},
    {  90, 110, 130, 150},
    {  90, 110, 130, 150},
    {  85, 104, 123, 142},
    {  85, 104, 123, 142},
    {  81,  99, 117, 135},
    {  81,  99, 117, 135},
    {  77,  94, 111, 128},
    {  77,  94, 111, 128},
    {  73,  89, 105, 122},
    {  73,  89, 105, 122},
    {  69,  85, 100, 116},
    {  69,  85, 100, 116},
    {  66,  80,  95, 110},
    {  66,  80,  95, 110},
    {  62,  76,  90, 104},
    {  62,  76,  90, 104},
    {  59,  72,  86,  99},
    {  59,  72,  86,  99},
    {  56,  69,  81,  94},
    {  56,  69,  81,  94},
    {  53,  65,  77,  89},
    {  53,  65,  77,  89},
    {  51,  62,  73,  85},
    {  51,  62,  73,  85},
    {  48,  59,  69,  80},
    {  48,  59,  69,  80},
    {  46,  56,  66,  76},
    {  46,  56,  66,  76},
    {  43,  53,  63,  72},
    {  43,  53,  63,  72},
    {  41,  50,  59,  69},
    {  41,  50,  59,  69},
    {  39,  48,  56,  65},
    {  39,  48,  56,  65},
    {  37,  45,  54,  62},
    {  37,  45,  54,  62},
    {  35,  43,  51,  59},
    {  35,  43,  51,  59},
    {  33,  41,  48,  56},
    {  33,  41,  48,  56},
    {  32,  39,  46,  53},
    {  32,  39,  46,  53},
    {  30,  37,  43,  50},
    {  30,  37,  43,  50},
    {  29,  35,  41,  48},
    {  29,  35,  41,  48},
    {  27,  33,  39,  45},
    {  27,  33,  39,  45},
    {  26,  31,  37,  43},
    {  26,  31,  37,  43},
    {  24,  30,  35,  41},
    {  24,  30,  35,  41},
    {  23,  28,  33,  39},
    {  23,  28,  33,  39},
    {  22,  27,  32,  37},
    {  22,  27,  32,  37},
    {  21,  26,  30,  35},
    {  21,  26,  30,  35},
    {  20,  24,  29,  33},
    {  20,  24,  29,  33},
    {  19,  23,  27,  31},
    {  19,  23,  27,  31},
    {  18,  22,  26,  30},
    {  18,  22,  26,  30},
    {  17,  21,  25,  28},
    {  17,  21,  25,  28},
    {  16,  20,  23,  27},
    {  16,  20,  23,  27},
    {  15,  19,  22,  25},
    {  15,  19,  22,  25},
    {  14,  18,  21,  24},
    {  14,  18,  21,  24},
    {  14,  17,  20,  23},
    {  14,  17,  20,  23},
    {  13,  16,  19,  22},
    {  13,  16,  19,  22},
    {  12,  15,  18,  21},
    {  12,  15,  18,  21},
    {  12,  14,  17,  20},
    {  12,  14,  17,  20},
    {  11,  14,  16,  19},
    {  11,  14,  16,  19},
    {  11,  13,  15,  18},
    {  11,  13,  15,  18},
    {  10,  12,  15,  17},
    {  10,  12,  15,  17},
    {  10,  12,  14,  16},
    {  10,  12,  14,  16},
    {   9,  11,  13,  15},
    {   9,  11,  13,  15},
    {   9,  11,  12,  14},
    {   9,  11,  12,  14},
    {   8,  10,  12,  14},
    {   8,  10,  12,  14},
    {   8,   9,  11,  13},
    {   8,   9,  11,  13},
    {   7,   9,  11,  12},
    {   7,   9,  11,  12},
    {   7,   9,  10,  12},
    {   7,   9,  10,  12},
    {   7,   8,  10,  11},
    {   7,   8,  10,  11},
    {   6,   8,   9,  11},
    {   6,   8,   9,  11},
    {   6,   7,   9,  10},
    {   6,   7,   9,  10},
    {   6,   7,   8,   9},
    {   6,   7,   8,   9},
    {   2,   2,   2,   2},
    {   2,   2,   2,   2}
};

#define DECL(val) \
    (val) * 2, (val) * 2 + 1

// State transition table for MPS path. HEVC spec 9.3.4.3.2.2
const
uint8_t transIdxMPSH265[] =
{
    DECL( 1), DECL( 2), DECL( 3), DECL( 4), DECL( 5), DECL( 6), DECL( 7), DECL( 8),
    DECL( 9), DECL(10), DECL(11), DECL(12), DECL(13), DECL(14), DECL(15), DECL(16),
    DECL(17), DECL(18), DECL(19), DECL(20), DECL(21), DECL(22), DECL(23), DECL(24),
    DECL(25), DECL(26), DECL(27), DECL(28), DECL(29), DECL(30), DECL(31), DECL(32),
    DECL(33), DECL(34), DECL(35), DECL(36), DECL(37), DECL(38), DECL(39), DECL(40),
    DECL(41), DECL(42), DECL(43), DECL(44), DECL(45), DECL(46), DECL(47), DECL(48),
    DECL(49), DECL(50), DECL(51), DECL(52), DECL(53), DECL(54), DECL(55), DECL(56),
    DECL(57), DECL(58), DECL(59), DECL(60), DECL(61), DECL(62), DECL(62), DECL(63)
};

// State transition table for LPS path. HEVC spec 9.3.4.3.2.2
#if defined(_DEBUG) && defined(__linux__)
// workaround for ICC in -O0 mode: referencies to transIdxLPSH265 in inline assemly
// are processed incorrectly
extern uint8_t *transIdxLPSH265;

static
uint8_t transIdxLPSH265Impl[] =
#else
const
uint8_t transIdxLPSH265[] =
#endif // defined(_DEBUG)
{
    1,   0,   DECL( 0), DECL( 1), DECL( 2), DECL( 2), DECL( 4), DECL( 4), DECL( 5),
    DECL( 6), DECL( 7), DECL( 8), DECL( 9), DECL( 9), DECL(11), DECL(11), DECL(12),
    DECL(13), DECL(13), DECL(15), DECL(15), DECL(16), DECL(16), DECL(18), DECL(18),
    DECL(19), DECL(19), DECL(21), DECL(21), DECL(22), DECL(22), DECL(23), DECL(24),
    DECL(24), DECL(25), DECL(26), DECL(26), DECL(27), DECL(27), DECL(28), DECL(29),
    DECL(29), DECL(30), DECL(30), DECL(30), DECL(31), DECL(32), DECL(32), DECL(33),
    DECL(33), DECL(33), DECL(34), DECL(34), DECL(35), DECL(35), DECL(35), DECL(36),
    DECL(36), DECL(36), DECL(37), DECL(37), DECL(37), DECL(38), DECL(38), DECL(63)
};

#endif

} // namespace UMC_HEVC_DECODER

#endif //__UMC_H265_DEC_TABLES_H__
#endif // MFX_ENABLE_H265_VIDEO_DECODE
