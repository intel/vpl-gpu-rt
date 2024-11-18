// Copyright (c) 2019-2024 Intel Corporation
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
#include "av1ehw_ddi.h"
#include "ehw_device.h"
#include "ehw_task_manager.h"
#include <vector>
#include <set>

namespace AV1EHW
{
namespace Base
{
    const uint32_t SB_SIZE                      = 64;
    const uint8_t  AV1_MAX_Q_INDEX              = 255;
    const uint8_t  AV1_MIN_Q_INDEX              = 1;
    const uint8_t  AV1_NUM_QMATRIX              = 15;

    const uint8_t AV1_MAX_NUM_OF_SEGMENTS       = 8;
    const uint8_t AV1_MAX_LOOP_FILTER           = 63;

    const uint8_t PRIMARY_REF_BITS              = 3;
    const uint8_t PRIMARY_REF_NONE              = 7;

    const uint16_t MIN_FRAME_WIDTH              = 128;
    const uint16_t MIN_FRAME_HEIGHT             = 96;

    const uint8_t FRAME_CONTEXTS_LOG2           = 3;
    const uint8_t MAX_MODE_LF_DELTAS            = 2;

    const uint8_t REFS_PER_FRAME                = 7;
    const uint8_t TOTAL_REFS_PER_FRAME          = 8;

    const uint8_t REF_FRAMES_LOG2               = 3;
    const uint8_t NUM_REF_FRAMES                = 1 << REF_FRAMES_LOG2;

    const uint8_t CDEF_MAX_STRENGTHS            = 8;
    const uint8_t CDEF_STRENGTH_BITS            = 6;
    const uint8_t CDEF_STRENGTH_DIVISOR         = 4;

    const uint8_t MAX_MB_PLANE                  = 3;

    const uint8_t MAX_POINTS_IN_SCALING_FUNCTION_LUMA    = 14;
    const uint8_t MAX_POINTS_IN_SCALING_FUNCTION_CHROMA  = 10;
    const uint8_t MAX_AUTOREG_COEFFS_LUMA                = 24;
    const uint8_t MAX_AUTOREG_COEFFS_CHROMA              = MAX_AUTOREG_COEFFS_LUMA + 1;

    const uint8_t MAX_NUM_TEMPORAL_LAYERS       = 8;
    const uint8_t MAX_NUM_SPATIAL_LAYERS        = 4;
    const uint8_t MAX_NUM_OPERATING_POINTS      = MAX_NUM_TEMPORAL_LAYERS * MAX_NUM_SPATIAL_LAYERS;

    const uint32_t AV1_MAX_TILE_WIDTH    = 4096;
    const uint32_t AV1_MAX_TILE_AREA     = (4096 * 2304);

    const uint32_t AV1_MAX_TILE_WIDTH_SB  = AV1_MAX_TILE_WIDTH / SB_SIZE;
    const uint32_t AV1_MAX_TILE_AREA_SB   = AV1_MAX_TILE_AREA / (SB_SIZE * SB_SIZE);

    const uint32_t AV1_MAX_NUM_TILE_ROWS = 64;
    const uint32_t AV1_MAX_NUM_TILE_COLS = 64;

    const uint16_t DEFAULT_TARGET_USAGE  = MFX_TARGETUSAGE_BALANCED;
    const uint16_t DEFAULT_GOP_PIC_SIZE  = 248;
    const uint16_t DEFAULT_GOP_REF_DIST  = 8;
    const mfxU8    IDX_INVALID           = 0xff;

    const int16_t  DEFAULT_BPYR_QP_OFFSETS[8] = {32, 40, 48, 48, 48, 48, 48, 48};

    static const uint8_t LoopFilterLevelsLuma[256] = {
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  2,
        2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,
        4,  4,  4,  4,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,
        6,  7,  7,  7,  8,  8,  8,  8,  9,  9,  9,  9, 10, 10, 10, 10,
        11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15,
        15, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 19, 19, 20, 20, 20,
        21, 21, 21, 22, 22, 22, 23, 23, 24, 24, 24, 25, 25, 25, 26, 26,
        27, 27, 27, 28, 28, 29, 29, 29, 30, 30, 31, 31, 31, 32, 32, 33,
        33, 34, 34, 34, 35, 35, 36, 36, 37, 37, 38, 38, 39, 39, 40, 41,
        41, 42, 42, 43, 44, 45, 45, 46, 47, 48, 49, 50, 51, 52, 53, 55,
        56, 58, 59, 61, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
        63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63,
        63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63
    };

    static const uint8_t LoopFilterLevelsChroma[256] = {
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  2,
        2,  2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  3,  3,  3,  3,
        3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
        5,  5,  5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,  6,  6,  6,
        6,  6,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  8,  8,
        8,  8,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9, 10,
        10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15,
        16, 17, 18, 19, 20, 21, 22, 24, 25, 26, 28, 30, 31, 31, 31, 31,
        31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
        31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
        31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
        31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
        31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31
    };

    inline bool IsValid(mfxU8 idx)
    {
        return idx != IDX_INVALID;
    }

    enum {
        BITDEPTH_8 = 8,
        BITDEPTH_10 = 10
    };

    enum
    {
        GOP_INFINITE            = 0xFFFF,

        HW_SURF_ALIGN_W         = 16,
        HW_SURF_ALIGN_H         = 16,
    };

    struct ScalingList
    {
        mfxU8 scalingLists0[6][16]                        = {};
        mfxU8 scalingLists1[6][64]                        = {};
        mfxU8 scalingLists2[6][64]                        = {};
        mfxU8 scalingLists3[2][64]                        = {};
        mfxU8 scalingListDCCoefSizeID2[6]                 = {};
        mfxU8 scalingListDCCoefSizeID3[2]                 = {};
    };

    struct TimingInfo
    {
        uint32_t num_units_in_display_tick                = 0;
        uint32_t time_scale                               = 0;
        uint32_t equal_picture_interval                   = 0;
        uint32_t num_ticks_per_picture_minus_1            = 0;
    };

    struct DecoderModelInfo
    {
        uint32_t buffer_delay_length_minus_1              = 0;
        uint32_t num_units_in_decoding_tick               = 0;
        uint32_t buffer_removal_time_length_minus_1       = 0;
        uint32_t frame_presentation_time_length_minus_1   = 0;
    };

    struct OperatingParametersInfo
    {
        uint32_t decoder_buffer_delay        = 0;
        uint32_t encoder_buffer_delay        = 0;
        uint32_t low_delay_mode_flag         = 0;
    };

    struct ColorConfig
    {
        uint32_t BitDepth                       = 0;
        uint32_t mono_chrome                    = 0;
        uint32_t color_primaries                = 0;
        uint32_t transfer_characteristics       = 0;
        uint32_t matrix_coefficients            = 0;
        uint32_t color_description_present_flag = 0;
        uint32_t color_range                    = 0;
        uint32_t chroma_sample_position         = 0;
        uint32_t subsampling_x                  = 0;
        uint32_t subsampling_y                  = 0;
        uint32_t separate_uv_delta_q            = 0;
    };

    struct SH
    {
        //Rev 0.85 parameters (AV1 spec version 1.0) in order of appearance/calculation in sequence_header_obu()
        uint32_t seq_profile                        = 0;
        uint32_t still_picture                      = 0;
        uint32_t reduced_still_picture_header       = 0;

        uint32_t timing_info_present_flag           = 0;
        TimingInfo timing_info;

        uint32_t decoder_model_info_present_flag    = 0;
        DecoderModelInfo decoder_model_info;

        uint32_t operating_points_cnt_minus_1       = 0;
        uint32_t operating_point_idc[MAX_NUM_OPERATING_POINTS]                                   = { 0 };
        uint32_t seq_level_idx[MAX_NUM_OPERATING_POINTS]                                         = { 0 };
        uint32_t seq_tier[MAX_NUM_OPERATING_POINTS]                                              = { 0 };
        uint32_t decoder_model_present_for_this_op[MAX_NUM_OPERATING_POINTS]                     = { 0 };
        OperatingParametersInfo operating_parameters_info[MAX_NUM_OPERATING_POINTS];
        uint32_t initial_display_delay_minus_1[MAX_NUM_OPERATING_POINTS]                         = { 0 };

        uint32_t frame_width_bits                   = 0;
        uint32_t frame_height_bits                  = 0;
        uint32_t max_frame_width                    = 0;
        uint32_t max_frame_height                   = 0;
        uint32_t frame_id_numbers_present_flag      = 0;
        uint32_t delta_frame_id_length              = 0;
        uint32_t idLen                              = 0;
        uint32_t sbSize                             = 0;
        uint32_t enable_filter_intra                = 0;
        uint32_t enable_intra_edge_filter           = 0;
        uint32_t enable_interintra_compound         = 0;
        uint32_t enable_masked_compound             = 0;
        uint32_t enable_warped_motion               = 0;
        uint32_t enable_dual_filter                 = 0;
        uint32_t enable_order_hint                  = 0;
        uint32_t enable_jnt_comp                    = 0;
        uint32_t enable_ref_frame_mvs               = 0;
        uint32_t seq_force_screen_content_tools     = 0;
        uint32_t seq_force_integer_mv               = 0;
        uint32_t order_hint_bits_minus1             = 0;
        uint32_t enable_superres                    = 0;
        uint32_t enable_cdef                        = 0;
        uint32_t enable_restoration                 = 0;

        ColorConfig color_config;

        uint32_t film_grain_param_present           = 0;
    };

    enum FRAME_TYPE
    {
        KEY_FRAME = 0,
        INTER_FRAME = 1,
        INTRA_ONLY_FRAME = 2,  // replaces intra-only
        SWITCH_FRAME = 3,
        NUM_FRAME_TYPES,
    };

    enum {
        INTRA_FRAME     = 0,
        LAST_FRAME      = 1,
        LAST2_FRAME     = 2,
        LAST3_FRAME     = 3,
        GOLDEN_FRAME    = 4,
        BWDREF_FRAME    = 5,
        ALTREF2_FRAME   = 6,
        ALTREF_FRAME    = 7,
        MAX_REF_FRAMES  = 8
    };

    enum INTERP_FILTER{
        EIGHTTAP_REGULAR,
        EIGHTTAP_SMOOTH,
        EIGHTTAP_SHARP,
        BILINEAR,
        SWITCHABLE,
        INTERP_FILTERS_ALL
    };

    enum SEG_LVL_FEATURES {
        SEG_LVL_ALT_Q = 0,      // Use alternate Quantizer
        SEG_LVL_ALT_LF_Y_V,     // Use alternate loop filter value on y plane vertical
        SEG_LVL_ALT_LF_Y_H,     // Use alternate loop filter value on y plane horizontal
        SEG_LVL_ALT_LF_U,       // Use alternate loop filter value on u plane
        SEG_LVL_ALT_LF_V,       // Use alternate loop filter value on v plane
        SEG_LVL_REF_FRAME,      // Optional Segment reference frame
        SEG_LVL_SKIP,           // Optional Segment (0,0) + skip mode
        SEG_LVL_GLOBALMV,
        SEG_LVL_MAX
    };

    const uint8_t SEGMENTATION_FEATURE_BITS[SEG_LVL_MAX]    = { 8, 6, 6, 6, 6, 3, 0, 0 };
    const uint8_t SEGMENTATION_FEATURE_SIGNED[SEG_LVL_MAX]  = { 1, 1, 1, 1, 1, 0, 0, 0 };
    const uint8_t SEGMENTATION_FEATURE_MAX[SEG_LVL_MAX]     = {
        AV1_MAX_Q_INDEX, AV1_MAX_LOOP_FILTER, AV1_MAX_LOOP_FILTER, AV1_MAX_LOOP_FILTER,
        AV1_MAX_LOOP_FILTER, 7, 0, 0 };

    enum
    {
        BLOCK_16x16 = 0,
        BLOCK_32x32 = 1,
        BLOCK_64x64 = 2,
        BLOCK_8x8   = 3
    };

    struct TileSizeParams
    {
        mfxU16 UniformTileSpacing         = 0;
        mfxU16 NumTileRows                = 0;
        mfxU16 NumTileColumns             = 0;
        mfxU16 TileWidthInSB[128]         = { 0 };
        mfxU16 TileHeightInSB[128]        = { 0 };
    };

    struct TileLimits
    {
        uint32_t MaxTileWidthSb;
        uint32_t MaxTileHeightSbNonUniform;
        uint32_t MaxTileHeightSbUniform;
        uint32_t MinLog2TileCols;
        uint32_t MaxLog2TileCols;
        uint32_t MinLog2TileRows;
        uint32_t MaxLog2TileRows;
    };

    struct TileInfoAv1
    {
        uint32_t uniform_tile_spacing_flag;
        uint32_t TileColsLog2;
        uint32_t TileRowsLog2;
        uint32_t TileCols;
        uint32_t TileRows;
        uint32_t TileWidthInSB[AV1_MAX_NUM_TILE_COLS];  // valid for 0 <= i < TileCols
        uint32_t TileHeightInSB[AV1_MAX_NUM_TILE_ROWS]; // valid for 0 <= i < TileRows
        uint32_t context_update_tile_id;
        uint32_t TileSizeBytes;
        struct TileLimits tileLimits;
    };

    struct QuantizationParams
    {
        uint32_t base_q_idx;
        int32_t DeltaQYDc;
        int32_t DeltaQUDc;
        int32_t DeltaQUAc;
        int32_t DeltaQVDc;
        int32_t DeltaQVAc;
        uint32_t using_qmatrix;
        uint32_t qm_y;
        uint32_t qm_u;
        uint32_t qm_v;
    };

    struct SegmentationParams
    {
        uint8_t segmentation_enabled;
        uint8_t segmentation_update_map;
        uint8_t segmentation_temporal_update;
        uint8_t segmentation_update_data;

        int32_t FeatureData[AV1_MAX_NUM_OF_SEGMENTS][SEG_LVL_MAX];
        uint32_t FeatureMask[AV1_MAX_NUM_OF_SEGMENTS];
    };

    using SegDpbType = std::vector<std::shared_ptr<mfxExtAV1Segmentation>>;

    struct LoopFilterParams
    {
        int32_t loop_filter_level[4];
        int32_t loop_filter_sharpness;
        uint8_t loop_filter_delta_enabled;
        uint8_t loop_filter_delta_update;
        // 0 = Intra, Last, Last2, Last3, GF, BWD, ARF
        int8_t loop_filter_ref_deltas[TOTAL_REFS_PER_FRAME];
        // 0 = ZERO_MV, MV
        int8_t loop_filter_mode_deltas[MAX_MODE_LF_DELTAS];
    };

    struct CdefParams
    {
        uint32_t cdef_damping;
        uint32_t cdef_bits;
        uint32_t cdef_y_pri_strength[CDEF_MAX_STRENGTHS];
        uint32_t cdef_y_sec_strength[CDEF_MAX_STRENGTHS];
        uint32_t cdef_uv_pri_strength[CDEF_MAX_STRENGTHS];
        uint32_t cdef_uv_sec_strength[CDEF_MAX_STRENGTHS];
    };

    enum RestorationType
    {
        RESTORE_NONE,
        RESTORE_SWITCHABLE,
        RESTORE_WIENER,
        RESTORE_SGRPROJ,
        RESTORE_TYPES = 4,
    };

    struct LRParams
    {
        RestorationType lr_type[MAX_MB_PLANE];
        uint32_t lr_unit_shift;
        uint32_t lr_uv_shift;
        uint32_t lr_unit_extra_shift;
    };

    enum TX_MODE{
        ONLY_4X4 = 0,     // only 4x4 transform used
        TX_MODE_LARGEST,  // transform size is the largest possible for pu size
        TX_MODE_SELECT,   // transform specified for each block
        TX_MODES,
    };

    enum TRANSFORMATION_TYPE {
        IDENTITY = 0,      // identity transformation, 0-parameter
        TRANSLATION = 1,   // translational motion 2-parameter
        ROTZOOM = 2,       // simplified affine with rotation + zoom only, 4-parameter
        AFFINE = 3,        // affine, 6-parameter
        HORTRAPEZOID = 4,  // constrained homography, hor trapezoid, 6-parameter
        VERTRAPEZOID = 5,  // constrained homography, ver trapezoid, 6-parameter
        HOMOGRAPHY = 6,    // homography, 8-parameter
        TRANS_TYPES = 7,
    };

    struct GlobalMotionParams
    {
        TRANSFORMATION_TYPE wmtype;
        int32_t wmmat[8];
        int16_t alpha;
        int16_t beta;
        int16_t gamma;
        int16_t delta;
        int8_t invalid;
    };

    struct FilmGrainParams{
        uint32_t apply_grain;
        uint32_t grain_seed;
        uint32_t update_grain;

        uint32_t film_grain_params_ref_idx;

        // 8 bit values
        int32_t num_y_points;  // value: 0..14
        int32_t point_y_value[MAX_POINTS_IN_SCALING_FUNCTION_LUMA];
        int32_t point_y_scaling[MAX_POINTS_IN_SCALING_FUNCTION_LUMA];

        int32_t chroma_scaling_from_luma;

        // 8 bit values
        int32_t num_cb_points;  // value: 0..10
        int32_t point_cb_value[MAX_POINTS_IN_SCALING_FUNCTION_CHROMA];
        int32_t point_cb_scaling[MAX_POINTS_IN_SCALING_FUNCTION_CHROMA];

        // 8 bit values
        int32_t num_cr_points;  // value: 0..10
        int32_t point_cr_value[MAX_POINTS_IN_SCALING_FUNCTION_CHROMA];
        int32_t point_cr_scaling[MAX_POINTS_IN_SCALING_FUNCTION_CHROMA];

        int32_t grain_scaling;

        int32_t ar_coeff_lag;  // values:  0..3

        // 8 bit values
        int32_t ar_coeffs_y[MAX_AUTOREG_COEFFS_LUMA];
        int32_t ar_coeffs_cb[MAX_AUTOREG_COEFFS_CHROMA];
        int32_t ar_coeffs_cr[MAX_AUTOREG_COEFFS_CHROMA];

        // Shift value: AR coeffs range
        // 6: [-2, 2)
        // 7: [-1, 1)
        // 8: [-0.5, 0.5)
        // 9: [-0.25, 0.25)
        int32_t ar_coeff_shift;  // values : 6..9
        int32_t grain_scale_shift;

        int32_t cb_mult;       // 8 bits
        int32_t cb_luma_mult;  // 8 bits
        int32_t cb_offset;     // 9 bits

        int32_t cr_mult;       // 8 bits
        int32_t cr_luma_mult;  // 8 bits
        int32_t cr_offset;     // 9 bits

        int32_t overlap_flag;

        int32_t clip_to_restricted_range;

        int32_t BitDepth;  // video bit depth
    };

    struct BitOffsets
    {
        mfxU32 QIndexBitOffset;
        mfxU32 SegmentationBitOffset;
        mfxU32 SegmentationBitSize;
        mfxU32 LoopFilterParamsBitOffset;
        mfxU32 FrameHeaderOBUSizeInBits;
        mfxU32 FrameHeaderOBUSizeByteOffset;
        mfxU32 UncompressedHeaderByteOffset;
        mfxU32 HDRHeaderByteOffset;
        mfxU32 CDEFParamsBitOffset;
        mfxU32 CDEFParamsSizeInBits;
    };

    typedef struct FrameHeader
    {
        //Rev 0.85 parameters (AV1 spec version 1.0) in order of appearance/calculation in uncompressed_header()
        uint32_t show_existing_frame;
        uint32_t frame_to_show_map_idx;
        uint64_t frame_presentation_time;
        uint32_t display_frame_id;
        FRAME_TYPE frame_type;
        uint32_t show_frame;
        uint32_t showable_frame;
        uint32_t error_resilient_mode;
        uint32_t disable_cdf_update;
        uint32_t allow_screen_content_tools;
        uint32_t force_integer_mv;
        uint32_t current_frame_id;
        uint32_t frame_size_override_flag;
        uint32_t order_hint;
        uint32_t primary_ref_frame;

        uint8_t refresh_frame_flags;
        uint32_t ref_order_hint[NUM_REF_FRAMES];

        uint32_t FrameWidth;
        uint32_t FrameHeight;
        uint32_t use_superres;
        uint32_t SuperresDenom;
        uint32_t UpscaledWidth;
        uint32_t MiCols;
        uint32_t MiRows;
        uint32_t RenderWidth;
        uint32_t RenderHeight;

        uint32_t allow_intrabc;
        int32_t ref_frame_idx[REFS_PER_FRAME];
        uint32_t allow_high_precision_mv;
        INTERP_FILTER interpolation_filter;
        uint32_t is_motion_mode_switchable;
        uint32_t use_ref_frame_mvs;
        uint32_t disable_frame_end_update_cdf;

        uint32_t sbCols;
        uint32_t sbRows;
        uint32_t sbSize;

        TileInfoAv1 tile_info;
        QuantizationParams quantization_params;
        SegmentationParams segmentation_params;

        uint32_t delta_q_present;
        uint32_t delta_q_res;

        uint32_t delta_lf_present;
        uint32_t delta_lf_res;
        uint32_t delta_lf_multi;

        uint32_t CodedLossless;
        uint32_t AllLossless;

        LoopFilterParams loop_filter_params;
        CdefParams cdef_params;
        LRParams lr_params;

        TX_MODE TxMode;
        uint32_t reference_select;
        uint32_t skipModeAllowed;
        uint32_t skipModeFrame[2];
        uint32_t skip_mode_present;
        uint32_t allow_warped_motion;
        uint32_t reduced_tx_set;

        GlobalMotionParams global_motion_params[TOTAL_REFS_PER_FRAME];

        FilmGrainParams film_grain_params;

        uint32_t NumPlanes;

        uint32_t large_scale_tile;
    } FH;

    inline bool FrameIsIntra(const FrameHeader& fh)
    {
        return (fh.frame_type == INTRA_ONLY_FRAME || fh.frame_type == KEY_FRAME);
    }

    inline void DisableLoopFilter(FH& fh)
    {
        // AV1 spec 5.9.11
        fh.loop_filter_params.loop_filter_level[0] = 0;
        fh.loop_filter_params.loop_filter_level[1] = 0;
        fh.loop_filter_params.loop_filter_ref_deltas[INTRA_FRAME] = 1;
        fh.loop_filter_params.loop_filter_ref_deltas[LAST_FRAME] = 0;
        fh.loop_filter_params.loop_filter_ref_deltas[LAST2_FRAME] = 0;
        fh.loop_filter_params.loop_filter_ref_deltas[LAST3_FRAME] = 0;
        fh.loop_filter_params.loop_filter_ref_deltas[BWDREF_FRAME] = 0;
        fh.loop_filter_params.loop_filter_ref_deltas[GOLDEN_FRAME] = -1;
        fh.loop_filter_params.loop_filter_ref_deltas[ALTREF_FRAME] = -1;
        fh.loop_filter_params.loop_filter_ref_deltas[ALTREF2_FRAME] = -1;
        for (int i = 0; i < MAX_MODE_LF_DELTAS; i++)
            fh.loop_filter_params.loop_filter_mode_deltas[i] = 0;
    }

    inline void DisableCDEF(FH& fh)
    {
        // AV1 spec 5.9.19
        fh.cdef_params.cdef_bits = 0;
        fh.cdef_params.cdef_y_pri_strength[0] = 0;
        fh.cdef_params.cdef_y_sec_strength[0] = 0;
        fh.cdef_params.cdef_uv_pri_strength[0] = 0;
        fh.cdef_params.cdef_uv_sec_strength[0] = 0;
        fh.cdef_params.cdef_damping = 3;
    }

    inline void DisableLoopRestoration(FH& fh)
    {
        // AV1 spec 5.9.20
        fh.lr_params.lr_type[0] = RESTORE_NONE;
        fh.lr_params.lr_type[1] = RESTORE_NONE;
        fh.lr_params.lr_type[2] = RESTORE_NONE;
    }

    struct ObuExtensionHeader {
        uint32_t temporal_id;
        uint32_t spatial_id;
    };
    enum AV1_OBU_TYPE
    {
        OBU_SEQUENCE_HEADER = 1,
        OBU_TEMPORAL_DELIMITER = 2,
        OBU_FRAME_HEADER = 3,
        OBU_TILE_GROUP = 4,
        OBU_METADATA = 5,
        OBU_FRAME = 6,
        OBU_REDUNDANT_FRAME_HEADER = 7,
        OBU_PADDING = 15,
    };

    struct Resource
    {
        mfxU8    Idx = IDX_INVALID;
        mfxMemId Mid = nullptr;
    };

    struct IAllocation
        : public Storable
    {
        using TAlloc = CallChain<mfxStatus, const mfxFrameAllocRequest&, bool /*isCopyRequired*/>;
        TAlloc Alloc;

        using TAllocOpaque = CallChain<mfxStatus
            , const mfxFrameInfo &
            , mfxU16 /*type*/
            , mfxFrameSurface1 ** /*surfaces*/
            , mfxU16 /*numSurface*/>;

        using TGetResponse = CallChain<mfxFrameAllocResponse>;
        TGetResponse GetResponse;

        using TGetInfo = CallChain<mfxFrameInfo>;
        TGetInfo GetInfo;

        using TAcquire = CallChain<Resource>;
        TAcquire Acquire;

        using TRelease = CallChain<void, mfxU32 /*idx*/>;
        TRelease Release;

        using TClearFlag = CallChain<void, mfxU32 /*idx*/>;
        TClearFlag ClearFlag;

        using TSetFlag = CallChain<void, mfxU32 /*idx*/, mfxU32 /*flag*/>;
        TSetFlag SetFlag;

        using TGetFlag = CallChain<mfxU32, mfxU32 /*idx*/>;
        TGetFlag GetFlag;

        using TUnlockAll = CallChain<void>;
        TUnlockAll UnlockAll;

        std::unique_ptr<Storable> m_pthis;
    };

    class IBsWriter
    {
    public:
        virtual ~IBsWriter() {}
        virtual void PutBits(mfxU32 n, mfxU32 b) = 0;
        virtual void PutBit(mfxU32 b) = 0;
    };

    struct TileGroupInfo
    {
        mfxU32 TgStart;
        mfxU32 TgEnd;
    };

    using TileGroupInfos = std::vector<TileGroupInfo>;

    constexpr mfxU8 CODING_TYPE_I  = 1;
    constexpr mfxU8 CODING_TYPE_P  = 2;
    constexpr mfxU8 CODING_TYPE_B  = 3; //regular B, or no reference to other B frames
    constexpr mfxU8 CODING_TYPE_B1 = 4; //B1, reference to only I, P or regular B frames
    constexpr mfxU8 CODING_TYPE_B2 = 5; //B2, references include B1

    constexpr mfxU8 DEFAULT_DENOM_FOR_SUPERRES_ON = 16;
    constexpr mfxU8 DEFAULT_DENOM_FOR_SUPERRES_OFF = 8;

    // Min frame params required for Reorder, RefPicList/DPB management
    struct FrameBaseInfo
        : Storable
    {
        mfxI32 DisplayOrderInGOP        = -1;
        mfxU32 EncodedOrderInGOP        = 0;
        mfxU32 RefOrderInGOP            = 0;
        mfxI32 NextBufferedDisplayOrder = -1; // Earliest DisplayOrderInGOP among buffered frames
        mfxU16 FrameType                = 0;
        bool   isLDB                    = false; // is "low-delay B"
        mfxU8  TemporalID               = 0;
        mfxU32 BPyramidOrder            = mfxU32(MFX_FRAMEORDER_UNKNOWN);
        mfxU32 PyramidLevel             = 0;
    };

    typedef struct _DpbFrame
        : FrameBaseInfo
    {
        mfxU32   DisplayOrder = mfxU32(-1);
        mfxU32   EncodedOrder = mfxU32(-1);
        mfxU64   TimestampIn  = 0;
        mfxU16   PicStructIn  = 0;
        mfxU32   FrameOrderIn = mfxU32(-1);
        bool     isLTR        = false; // is "long-term"
        bool     isRejected   = false; // rejected ref frame should be refreshed asap
        mfxU16   LongTermIdx  = mfxU16(0);
        mfxU8    CodingType   = 0;
        Resource Raw;
        Resource Rec;
        mfxFrameSurface1* pSurfIn = nullptr; //input surface, may be opaque
    } DpbFrame;

    enum eSkipMode
    {
        SKIPFRAME_NO = 0
        , SKIPFRAME_INSERT_DUMMY
        , SKIPFRAME_INSERT_DUMMY_PROTECTED
        , SKIPFRAME_INSERT_NOTHING
        , SKIPFRAME_BRC_ONLY
        , SKIPFRAME_EXT_PSEUDO
    };

    enum eSkipCmd
    {
        SKIPCMD_NeedInputReplacement        = (1 << 0)
        , SKIPCMD_NeedDriverCall            = (1 << 1)
        , SKIPCMD_NeedSkipSliceGen          = (1 << 2)
        , SKIPCMD_NeedCurrentFrameSkipping  = (1 << 3)
        , SKIPCMD_NeedNumSkipAdding         = (1 << 4)
    };

    enum eInsertHeader
    {
        INSERT_IVF_SEQ          = (1 << 0)
        , INSERT_IVF_FRM        = (1 << 1)
        , INSERT_TD             = (1 << 2)
        , INSERT_SPS            = (1 << 3)
        , INSERT_PPS            = (1 << 4)
        , INSERT_FRM_OBU        = (1 << 5)
        , INSERT_HDR            = (1 << 6)
    };

    using DpbType = std::vector<std::shared_ptr<DpbFrame>>;
    using DpbIterType = DpbType::iterator;
    using RefListType = std::array<mfxU8, REFS_PER_FRAME>;
    using DpbRefreshType = std::array<mfxU8, NUM_REF_FRAMES>;

    inline bool IsValidRefFrame(const RefListType refList, mfxU8 refFrame)
    {
        return IsValid(refList[refFrame - LAST_FRAME]);
    }

    struct RepeatedFrameInfo
    {
        mfxU8  FrameToShowMapIdx = 0;
        mfxU32 DisplayOrder      = 0;
    };

    struct mfxGopHints {
        mfxU32              MiniGopSize = 0;
        mfxU16              FrameType   = 0;
        mfxU32              QPModulaton = 0;
        /* Scene Change parameter */
        mfxU16              SceneChange = 0;
#if defined(MFX_ENABLE_ENCTOOLS)
        mfxU16              SpatialComplexity = 0;
        mfxI8               QPDeltaExplicitModulation = 0;
        /* Persistence Map */
        mfxU16              PersistenceMapNZ;
        mfxU8               PersistenceMap[MFX_ENCTOOLS_PREENC_MAP_SIZE];
#endif
    };

    using RepeatedFrames    = std::vector<RepeatedFrameInfo>;
    using TileSizeType      = decltype(mfxExtAV1AuxData::TileWidthInSB[0]);
    using TileSizeArrayType = decltype(mfxExtAV1AuxData::TileWidthInSB);

    struct mfxBRCHints {
        /* Look ahead parameters */
        mfxU32              LaAvgEncodedBits = 0;         /* Average size of encoded Lookahead frames in bits */
        mfxU32              LaCurEncodedBits = 0;         /* Size of encoded Lookahead frame at current frame location in bits */
        mfxU16              LaDistToNextI    = 0;         /* First I Frame in Lookahead frames (0 if not found) */
    };

    struct TaskCommonPar
        : DpbFrame
    {
        mfxU32            stage                  = 0;
        mfxU32            MinFrameSize           = 0;
        mfxU32            BsDataLength           = 0;
        mfxU32            BsBytesAvailable       = 0;
        mfxU8*            pBsData                = nullptr;
        mfxU32*           pBsDataLength          = nullptr;
        mfxBitstream*     pBsOut                 = nullptr;
        mfxFrameSurface1* pSurfReal              = nullptr;
        mfxEncodeCtrl     ctrl                   = {};
        mfxU32            SkipCMD                = SKIPCMD_NeedDriverCall;
        Resource          BS;
        Resource          CUQP;
        mfxHDLPair        HDLRaw                 = {};
        bool              bCUQPMap               = false;
#if defined(MFX_ENABLE_ENCTOOLS_BASE)
        mfxLplastatus     LplaStatus           = {};
        mfxBRCHints      BrcHints              = {};
        bool             bBRCUpdated           = false;
        mfxFrameSurface1* pFilteredSurface     = nullptr;
        mfxEncToolsHintSaliencyMap saliencyMap = {};
#endif
        mfxGopHints       GopHints               = {};
        bool              bForceSync             = false;
        bool              bSkip                  = false;
        bool              bResetBRC              = false;
        bool              bRecode                = false;
        bool              bFreed                 = false;
        bool              bCached                = false;
        mfxI32            PrevRAP                = -1;
        mfxU16            NumRecode              = 0;
        mfxU8             QpY                    = 0;
        mfxU32            InsertHeaders          = 0;
        mfxU32            StatusReportId         = mfxU32(-1);
        DpbRefreshType    RefreshFrameFlags      = {};
        BitOffsets        Offsets                = {};
        mfxU8             MinBaseQIndex          = 0;
        mfxU8             MaxBaseQIndex          = 0;
        mfxU8             AdaptiveTUEnabled      = 0;

        RefListType       RefList                = {};
        DpbType           DPB;
        RepeatedFrames    FramesToShow           = {};
        mfxU16            RepeatedFrameBytes     = 0;
        mfxU32            TCBRCTargetFrameSize   = 0;

        mfxExtRefListCtrl InternalListCtrl       = {};
        bool              InternalListCtrlPresent = false;
    };

    inline void Invalidate(TaskCommonPar& par)
    {
        par = TaskCommonPar();
    }

    inline bool IsHiddenFrame(const TaskCommonPar& frame)
    {
        return frame.NextBufferedDisplayOrder != -1 &&
            frame.DisplayOrderInGOP > frame.NextBufferedDisplayOrder;
    }

    inline bool isValid(DpbFrame const & frame) { return IDX_INVALID != frame.Rec.Idx; }

    inline bool HaveRABFrames(const mfxVideoParam& par)
    {
        return par.mfx.GopPicSize > 2 && par.mfx.GopRefDist > 1;
    }

    inline std::tuple<mfxU32, mfxU32> GetRealResolution(const mfxVideoParam& vp)
    {
        const mfxExtAV1ResolutionParam* pRsPar  = ExtBuffer::Get(vp);
        const bool                      rsValid = pRsPar != nullptr && pRsPar->FrameWidth > 0 && pRsPar->FrameHeight > 0;
        if (rsValid)
        {
            return std::make_tuple(pRsPar->FrameWidth, pRsPar->FrameHeight);
        }

        const auto& fi = vp.mfx.FrameInfo;
        if (fi.CropW > 0 && fi.CropH > 0)
        {
            return std::make_tuple(fi.CropW, fi.CropH);
        }
        else
        {
            return std::make_tuple(fi.Width, fi.Height);
        }
    }

    inline mfxU32 GetActualEncodeWidth(mfxU32 frameWidth, const mfxExtAV1AuxData *pAuxPar)
    {
        const bool useSuperRes = pAuxPar != nullptr && IsOn(pAuxPar->EnableSuperres);
        if (!useSuperRes)
            return frameWidth;

        mfxU16 denom = pAuxPar->SuperresScaleDenominator != 0 ? pAuxPar->SuperresScaleDenominator : DEFAULT_DENOM_FOR_SUPERRES_ON;
        return ((frameWidth << 3) + (denom >> 1)) / denom;
    }

    inline mfxU16 CountTileNumber(const TileSizeArrayType& tileSizeInSB)
    {
        mfxU16 cnt = 0;
        while (cnt < sizeof(TileSizeArrayType) / sizeof(TileSizeType) && tileSizeInSB[cnt] > 0)
        {
            cnt++;
        }

        return cnt;
    }

    inline std::pair<mfxU16, mfxU16> GetSBNum(const mfxVideoParam& par)
    {
        mfxU32 width = 0, height = 0;
        std::tie(width, height) = GetRealResolution(par);

        const mfxExtAV1AuxData* pAuxPar = ExtBuffer::Get(par);
        width                           = GetActualEncodeWidth(width, pAuxPar);
        mfxU16                  sbCols  = static_cast<mfxU16>(mfx::CeilDiv(width, SB_SIZE));
        mfxU16                  sbRows  = static_cast<mfxU16>(mfx::CeilDiv(height, SB_SIZE));

        return std::make_pair(sbCols, sbRows);
    }

    inline std::tuple<mfxU16, mfxU16> GetNumTiles(
        const mfxExtAV1TileParam* pTiles
        , const mfxExtAV1AuxData* pAuxPar)
    {
        mfxU16 numTileCols = 0;
        mfxU16 numTileRows = 0;

        if (pTiles)
        {
            numTileCols = pTiles->NumTileColumns;
            numTileRows = pTiles->NumTileRows;
        }

        if (pAuxPar)
        {
            SetIf(numTileCols, numTileCols == 0, CountTileNumber(pAuxPar->TileWidthInSB));
            SetIf(numTileRows, numTileRows == 0, CountTileNumber(pAuxPar->TileHeightInSB));
        }

        numTileCols = std::max(mfxU16(1), numTileCols);
        numTileRows = std::max(mfxU16(1), numTileRows);

        return std::make_tuple(numTileCols, numTileRows);
    }

    inline mfxU32 GetAvgFrameSizeInBytes(const mfxVideoParam& par)
    {
        mfxU32 avgFrameSizeInBytes = 0;

        if (par.mfx.FrameInfo.FrameRateExtN && par.mfx.FrameInfo.FrameRateExtD)
        {
            // Use type mfxU64 to avoid value overflow with most value precise obtained.
            mfxU64 targetbps    = mfxU64(TargetKbps(par.mfx)) * 1000;
            avgFrameSizeInBytes = mfxU32(targetbps * par.mfx.FrameInfo.FrameRateExtD / par.mfx.FrameInfo.FrameRateExtN / 8);
        }

        return avgFrameSizeInBytes;
    }

    class FrameLocker
        : public mfxFrameData
    {
    public:
        mfxU32 Pitch;

        FrameLocker(VideoCORE& core, mfxMemId memId, bool external = false)
            : m_data(*this)
            , m_core(core)
            , m_memId(memId)
        {
            m_data   = {};
            m_status = Lock(external);
            Pitch    = (mfxFrameData::PitchHigh << 16) + mfxFrameData::PitchLow;
        }
        FrameLocker(VideoCORE& core, mfxFrameData& data, bool external = false)
            : m_data(data)
            , m_core(core)
            , m_memId(data.MemId)
            , m_status(Lock(external))
        {
            Pitch = (mfxFrameData::PitchHigh << 16) + mfxFrameData::PitchLow;
        }

        ~FrameLocker()
        {
            Unlock();
        }

        enum { LOCK_NO, LOCK_INT, LOCK_EXT };

        mfxStatus Unlock()
        {
            mfxStatus mfxSts = MFX_ERR_NONE;
            if (m_status == LOCK_INT)
                mfxSts = m_core.UnlockFrame(m_memId, &m_data);
            else if (m_status == LOCK_EXT)
                mfxSts = m_core.UnlockExternalFrame(m_memId, &m_data);
            m_status = LOCK_NO;
            return mfxSts;
        }


        mfxU32 Lock(bool external)
        {
            mfxU32 status = LOCK_NO;
            if (!m_data.Y && !m_data.Y410)
            {
                status = external
                    ? (m_core.LockExternalFrame(m_memId, &m_data) == MFX_ERR_NONE ? LOCK_EXT : LOCK_NO)
                    : (m_core.LockFrame(m_memId, &m_data) == MFX_ERR_NONE ? LOCK_INT : LOCK_NO);
            }
            return status;
        }

    private:
        FrameLocker(FrameLocker const &) = delete;
        FrameLocker & operator =(FrameLocker const &) = delete;

        mfxFrameData& m_data;
        VideoCORE&    m_core;
        mfxMemId      m_memId;
        mfxU32        m_status;
    };

    using PackedData = MfxEncodeHW::PackedData;

    struct PackedHeaders
    {
        PackedData IVF;
        PackedData SPS;
        PackedData PPS;
        PackedData TD;
        PackedData HDR;
    };

    using DDIExecParam     = MfxEncodeHW::DDIExecParam;
    using DDIFeedbackParam = MfxEncodeHW::DDIFeedback;

    struct BsDataInfo
    {
        mfxU8  *Data;
        mfxU32 DataOffset;
        mfxU32 DataLength;
        mfxU32 MaxLength;
    };

    enum eResetFlags
    {
        RF_SPS_CHANGED      = (1 << 0)
        , RF_PPS_CHANGED    = (1 << 1)
        , RF_IDR_REQUIRED   = (1 << 2)
        , RF_BRC_RESET      = (1 << 3)
        , RF_CANCEL_TASKS   = (1 << 4)
    };

    struct ResetHint
    {
        mfxU32 Flags;
    };

    struct DeviceCreationMask
    {
        mfxU32 DisableTemporalDevice = 0;
    };

    struct VAID
    {
        mfxU32 Profile;
        mfxU32 Entrypoint;
    };

    template<typename T> struct is_shared_ptr : std::false_type {};
    template<typename T> struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

    template <typename DPB
        , typename Releaser>
        static typename std::enable_if<is_shared_ptr<typename DPB::value_type>::value, void>::type UpdateDPB(
            DPB& dpb
            , const typename DPB::value_type::element_type& obj
            , const DpbRefreshType& refreshRefFrames
            , Releaser&& releaser)
    {
        using ElemType = typename DPB::value_type::element_type;

        if (std::find(refreshRefFrames.begin(), refreshRefFrames.end(), 1)
            == refreshRefFrames.end())
            return;

        typename DPB::value_type curr(new ElemType(obj), std::forward<Releaser>(releaser));

        for (mfxU8 i = 0; i < dpb.size(); i++)
        {
            if (refreshRefFrames[i])
                dpb.at(i) = curr;
        }
        return;
    }

    typedef std::list<StorageRW>::iterator TTaskIt;

    template<class T, mfxU32 K>
    struct TaskItWrap
    {
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using pointer = value_type*;
        using reference = value_type&;
        using difference_type = ptrdiff_t;
        using iterator_type = TaskItWrap;

        TaskItWrap(TTaskIt _it) : it(_it) {}

        bool operator==(const TTaskIt& other) const
        {
            return it == other;
        }
        bool operator!=(const TTaskIt& other) const
        {
            return it != other;
        }
        bool operator==(const iterator_type& other) const
        {
            return it == other.it;
        }
        bool operator!=(const iterator_type& other) const
        {
            return it != other.it;
        }
        pointer operator->()
        {
            return &it->Write<value_type>(K);
        }
        reference operator*() const
        {
            return it->Write<value_type>(K);
        }
        iterator_type& operator++()
        {
            ++it; return *this;
        }
        iterator_type operator++(int)
        {
            iterator_type tmp(it); ++it; return tmp;
        }
        iterator_type& operator--()
        {
            --it; return *this;
        }
        iterator_type operator--(int)
        {
            iterator_type tmp(it); --it; return tmp;
        }

        TTaskIt it;
    };

    struct Reorderer
        : CallChain<TTaskIt, TTaskIt, TTaskIt, bool>
    {
        mfxU16 BufferSize = 0;
        //NotNull<DpbArray*> DPB;
    };

    class EncodeCapsAv1
        : public ENCODE_CAPS_AV1
        , public Storable
    {
    public:
        EncodeCapsAv1()
            : ENCODE_CAPS_AV1({})
        {}

        struct MsdkExt
        {
            bool CQPSupport            = false;
            bool CBRSupport            = false;
            bool VBRSupport            = false;
            bool ICQSupport            = false;
        } msdk;
    };

    struct Defaults
    {
        struct Param
        {
            Param(
                const mfxVideoParam& _par
                , const EncodeCapsAv1& _caps
                , const Defaults& _d)
                : mvp(_par)
                , caps(_caps)
                , base(_d)
            {}
            const mfxVideoParam&    mvp;
            const EncodeCapsAv1&   caps;
            const Defaults&         base;
        };

        template<class TRV>
        using TChain = CallChain<TRV, const Param&>;
        std::map<mfxU32, bool> SetForFeature;

        TChain<mfxU16> GetCodedPicWidth;
        TChain<mfxU16> GetCodedPicHeight;
        TChain<mfxU16> GetCodedPicAlignment;
        TChain<mfxU16> GetGopPicSize;
        TChain<mfxU16> GetGopRefDist;
        TChain<mfxU16> GetTargetUsage;
        TChain<mfxU16> GetNumBPyramidLayers;
        TChain<mfxU16> GetNumRefFrames;
        TChain<mfxU16> GetNumRefBPyramid;
        TChain<mfxU16> GetNumRefNoPyramid;
        TChain<mfxU16> GetMinRefForBPyramid;
        TChain<mfxU16> GetMinRefForBNoPyramid;
        TChain<mfxU16> GetBRefType;
        TChain<mfxU16> GetPRefType;
        TChain<mfxU16> GetBitDepthLuma;
        TChain<mfxU16> GetTargetBitDepthLuma;
        TChain<mfxU16> GetTargetChromaFormatPlus1;
        TChain<mfxU16> GetRateControlMethod;
        TChain<mfxU16> GetProfile;
        TChain<mfxU16> GetMBBRC;
        TChain<mfxU16> GetAsyncDepth;
        TChain<mfxU32> GetBufferSizeInKB;
        TChain<mfxU32> GetTargetKbps;
        TChain<mfxU32> GetMaxKbps;
        TChain<mfxU8>  GetMinQPMFX;
        TChain<mfxU8>  GetMaxQPMFX;
        TChain<mfxU16> GetNumTemporalLayers;
        TChain<mfxU8>  GetNumReorderFrames;
        TChain<bool>   GetNonStdReordering;
        TChain<mfxU32> GetTemporalUnitCacheSize;
        TChain<std::tuple<mfxU16, mfxU16, mfxU16>> GetMaxNumRef;
        TChain<std::tuple<mfxU32, mfxU32>>         GetFrameRate;
        TChain<std::tuple<mfxU16, mfxU16, mfxU16>> GetQPMFX; //I,P,B

        using TGetNumRefActive = CallChain<
            bool //bExternal
            , const Defaults::Param&
            , mfxU16(*)[8] //P
            , mfxU16(*)[8] //BL0
            , mfxU16(*)[8]>; //BL1
        TGetNumRefActive GetNumRefActive;

        using TGetQPOffset = CallChain<
            void
            , const Defaults::Param&
            , mfxU16& //EnableQPOffset
            , mfxI16(&)[8]>;//QPOffset
        TGetQPOffset GetQPOffset;

        using TGetFrameType = CallChain<
            mfxU16 //frameType
            , const Defaults::Param&
            , mfxU32 //displayOrder
            , mfxU32 //lastKeyFrame
            , mfxGopHints>; //adaptive GOP hints
        TGetFrameType GetFrameType;

        using TGetPreReorderInfo = CallChain<
            mfxStatus
            , const Defaults::Param&
            , FrameBaseInfo&
            , const mfxFrameSurface1*   //pSurfIn
            , const mfxEncodeCtrl*      //pCtrl
            , mfxU32                    //prevKeyFrameOrder
            , mfxU32                    //frameOrder
            , mfxGopHints>;             //adaptive GOP hints
        TGetPreReorderInfo GetPreReorderInfo; // fill all info available at pre-reorder

        using TGetLoopFilterLevels = CallChain<
            void
            , const Defaults::Param&
            , FH&>;
        TGetLoopFilterLevels GetLoopFilterLevels;

        using TGetCDEF = CallChain<
            void
            , FH&>;
        TGetCDEF GetCDEF;

        //for Query w/o caps:
        using TPreCheck = CallChain<mfxStatus, const mfxVideoParam&>;
        TPreCheck PreCheckCodecId;
        TPreCheck PreCheckChromaFormat;

        //for Query w/caps (check + fix)
        using TCheckAndFix = CallChain<mfxStatus, const Defaults::Param&, mfxVideoParam&>;
        TCheckAndFix CheckSurfSize;
        TCheckAndFix CheckProfile;
        TCheckAndFix CheckFourCC;
        TCheckAndFix CheckInputFormatByFourCC;
        TCheckAndFix CheckTargetChromaFormat;
        TCheckAndFix CheckTargetBitDepth;
        TCheckAndFix CheckFourCCByTargetFormat;
        TCheckAndFix CheckNumRefActive;
    };

    enum eFeatureId
    {
          FEATURE_GENERAL = 0
        , FEATURE_SCC
        , FEATURE_DDI
        , FEATURE_DDI_PACKER
        , FEATURE_ALLOCATOR
        , FEATURE_TASK_MANAGER
        , FEATURE_PACKER
        , FEATURE_BLOCKING_SYNC
        , FEATURE_TILE
        , FEATURE_SEGMENTATION
        , FEATURE_ENCODED_FRAME_INFO
        , FEATURE_QMATRIX
        , FEATURE_QUERY_IMPL_DESC
        , FEATURE_ENCTOOLS
        , FEATURE_MAX_FRAME_SIZE
        , FEATURE_HDR
        , FEATURE_QUALITYINFO
        , FEATURE_INTERPO_FILTER
        , FEATURE_CDEF
        , NUM_FEATURES
    };

    enum eStages
    {
        S_NEW = 0
        , S_PREPARE
        , S_REORDER
        , S_SUBMIT
        , S_QUERY
        , NUM_STAGES
    };

    using TFramesToShowInfo    = std::set<mfxU32>;
    using TRepeatFrameSizeInfo = std::map<mfxU32, mfxU32>;
    struct Glob
    {
        static const StorageR::TKey _KD = __LINE__ + 1;
        using VideoCore           = StorageVar<__LINE__ - _KD, VideoCORE>;
        using RTErr               = StorageVar<__LINE__ - _KD, mfxStatus>;
        using EncodeCaps          = StorageVar<__LINE__ - _KD, EncodeCapsAv1>;
        using VideoParam          = StorageVar<__LINE__ - _KD, ExtBuffer::Param<mfxVideoParam>>;
        using SH                  = StorageVar<__LINE__ - _KD, Base::SH>;
        using FH                  = StorageVar<__LINE__ - _KD, Base::FH>;
        using FramesToShowInfo    = StorageVar<__LINE__ - _KD, TFramesToShowInfo>;
        using RepeatFrameSizeInfo = StorageVar<__LINE__ - _KD, TRepeatFrameSizeInfo>;
        using TileGroups          = StorageVar<__LINE__ - _KD, TileGroupInfos>;
        using AllocRaw            = StorageVar<__LINE__ - _KD, IAllocation>;
        using AllocRec            = StorageVar<__LINE__ - _KD, IAllocation>;
        using AllocBS             = StorageVar<__LINE__ - _KD, IAllocation>;
        using PackedHeaders       = StorageVar<__LINE__ - _KD, Base::PackedHeaders>;
        using DDI_Resources       = StorageVar<__LINE__ - _KD, std::list<DDIExecParam>>;
        using DDI_SubmitParam     = StorageVar<__LINE__ - _KD, std::list<DDIExecParam>>;
        using DDI_Feedback        = StorageVar<__LINE__ - _KD, DDIFeedbackParam>;
        using DDI_Execute         = StorageVar<__LINE__ - _KD, CallChain<mfxStatus, const DDIExecParam&>>;
        using RealState           = StorageVar<__LINE__ - _KD, StorageW>;//available during Reset
        using ResetHint           = StorageVar<__LINE__ - _KD, Base::ResetHint>; //available during Reset
        using Reorder             = StorageVar<__LINE__ - _KD, Reorderer>;
        using Defaults            = StorageVar<__LINE__ - _KD, Base::Defaults>;
        using SegDpb              = StorageVar<__LINE__ - _KD, SegDpbType>;
        using DeviceCreation      = StorageVar<__LINE__ - _KD, Base::DeviceCreationMask>;
        using TaskManager         = StorageVar<__LINE__ - _KD, MakeStorable<MfxEncodeHW::TaskManager::TMRefWrapper>>;
        static const StorageR::TKey CallChainsKey = __LINE__ - _KD;
        static const StorageR::TKey ReservedKey0    = __LINE__ - _KD;
        static const StorageR::TKey NUM_KEYS = __LINE__ - _KD;
    };

    struct Tmp
    {
        static const StorageR::TKey _KD = __LINE__ + 1;
        using MakeAlloc     = StorageVar<__LINE__ - _KD, std::function<IAllocation*(VideoCORE&)>>;
        using BSAllocInfo   = StorageVar<__LINE__ - _KD, mfxFrameAllocRequest>;
        using RecInfo       = StorageVar<__LINE__ - _KD, mfxFrameAllocRequest>;
        using RawInfo       = StorageVar<__LINE__ - _KD, mfxFrameAllocRequest>;
        using CurrTask      = StorageVar<__LINE__ - _KD, StorageW>;
        using BsDataInfo    = StorageVar<__LINE__ - _KD, Base::BsDataInfo>;
        using DDI_InitParam = StorageVar<__LINE__ - _KD, std::list<DDIExecParam>>;
        static const StorageR::TKey NUM_KEYS = __LINE__ - _KD;
    };

    using TUsedRefList = std::remove_reference<decltype(mfxExtAVCEncodedFrameInfo::UsedRefListL0)>::type;
    struct EncodedInfoAv1
    {
        TUsedRefList UsedRefListL0 = {};
        TUsedRefList UsedRefListL1 = {};
        mfxU16       QpY           = 0;
        mfxU32       DisplayOrder  = 0;
        mfxU16       LongTermIdx   = MFX_LONGTERM_IDX_NO_IDX;
        bool         isLTR         = false;
        mfxU32       MSE[3];
    };

    struct Task
    {
        static const  StorageR::TKey _KD = __LINE__ + 1;
        using Common      = StorageVar<__LINE__ - _KD, TaskCommonPar>;
        using FH          = StorageVar<__LINE__ - _KD, Base::FH>;
        using Segment     = StorageVar<__LINE__ - _KD, mfxExtAV1Segmentation>;
        using EncodedInfo = StorageVar<__LINE__ - _KD, EncodedInfoAv1>;
        using TileInfo    = StorageVar<__LINE__ - _KD, TileInfoAv1>;
        using TileGroups  = StorageVar<__LINE__ - _KD, TileGroupInfos>;
        static const StorageR::TKey TaskEventKey = __LINE__ - _KD;
        static const StorageR::TKey NUM_KEYS = __LINE__ - _KD;
    };

    inline Defaults::Param GetRTDefaults(StorageR& strg)
    {
        return Defaults::Param(
            Glob::VideoParam::Get(strg)
            , Glob::EncodeCaps::Get(strg)
            , Glob::Defaults::Get(strg));
    }

} //namespace Base
} //namespace AV1EHW

#endif
