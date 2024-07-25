// Copyright (c) 2022-2024 Intel Corporation
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

#ifdef MFX_ENABLE_VVC_VIDEO_DECODE

#ifndef __UMC_VVC_DEC_DEFS_DEC_H__
#define __UMC_VVC_DEC_DEFS_DEC_H__

#include <stdexcept>
#include <vector>
#include "umc_structures.h"
#include "mfx_utils.h"

namespace UMC_VVC_DECODER
{
    class VVCDecoderFrame;
    typedef std::list<VVCDecoderFrame*> DPBType;
    constexpr uint16_t MFX_MAX_PICTURE_INDEX_VALUE_VVC   = 126;
    constexpr uint8_t MINIMAL_DATA_SIZE                  = 4;
    constexpr uint8_t start_code_prefix[]                = {0, 0, 1};
    constexpr size_t prefix_size                         = sizeof(start_code_prefix);
    constexpr uint32_t DEFAULT_UNIT_TAIL_VALUE           = 0xff;
    constexpr uint8_t DEFAULT_UNIT_TAIL_SIZE             = 8;

    constexpr uint8_t  VVC_MAX_VPS_LAYERS                = 64;
    constexpr uint8_t  VVC_MAX_VPS_SUBLAYERS             = 7;
    constexpr uint16_t VVC_MAX_NUM_OLSS                  = 256;
    constexpr uint8_t  VVC_MAX_VPS_OLS_MODE_IDC          = 2;
    constexpr uint8_t  VVC_MAX_TLAYER                    = 7;
    constexpr uint8_t  VVC_MAX_CPB_CNT                   = 32;
    constexpr uint32_t VVC_MAX_NUM_SUB_PICS              = (1 << 16);
    constexpr uint32_t VVC_MAX_SUBPIC_ID_LEN             = 15;
    constexpr uint8_t  VVC_MAX_NUM_CQP_MAPPING_TABLES    = 3;
    constexpr uint8_t  VVC_MAX_NUM_CHANNEL_TYPE          = 2;
    constexpr uint8_t  VVC_MAX_NUM_REF_PICS              = 29;
    constexpr uint8_t  VVC_MAX_NUM_REF                   = 16;
    constexpr uint8_t  VVC_MAX_NUM_REF_PIC_LISTS         = 2;
    constexpr int32_t  VVC_MAX_INT                       = 2147483647;
    constexpr uint8_t  VVC_MRG_MAX_NUM_CANDS             = 6;
    constexpr uint8_t  VVC_AFFINE_MRG_MAX_NUM_CANDS      = 5;
    constexpr uint8_t  VVC_IBC_MRG_MAX_NUM_CANDS         = 6;
    constexpr uint8_t  VVC_MAX_LADF_INTERVALS            = 4;
    constexpr uint8_t  VVC_MAX_TILE_COLS                 = 30;
    constexpr uint16_t VVC_MAX_SLICES                    = 1000;
    constexpr uint8_t  VVC_MAX_QP_OFFSET_LIST_SIZE       = 6;
    constexpr uint8_t  VVC_MAX_PIC_PARAMETER_SET_ID      = 63;
    constexpr uint8_t  VVC_MAX_SEQ_PARAMETER_SET_ID      = 15;
    constexpr uint8_t  VVC_MAX_PPS_SUBPIC_ID_LEN         = 16;
    constexpr uint8_t  VVC_MAX_PPS_LOG2_CTU_SIZE         = 7;
    constexpr uint8_t  VVC_MAX_NUM_REF_IDX               = 15;
    constexpr int8_t   VVC_CHROMA_QP_OFFSET_UPPER_BOUND  = 12;
    constexpr int8_t   VVC_CHROMA_QP_OFFSET_LOWER_BOUND  = -12;
    constexpr int8_t   VVC_DEBLOCKING_OFFSET_UPPER_BOUND = 12;
    constexpr int8_t   VVC_DEBLOCKING_OFFSET_LOWER_BOUND = -12;
    constexpr uint8_t  VVC_MIN_CTB_SIZE                  = 64;
    constexpr uint8_t  VVC_NUM_REF_IDX_ACTIVATE          = 2;
    constexpr uint8_t  VVC_RPL_NUM                       = 2;
    constexpr int32_t  VVC_MAX_QP                        = 63;
    constexpr int32_t  VVC_MAX_SUB_PICS                  = (1 << 16);
    constexpr uint8_t  VVC_NUM_APS_TYPE_LEN              = 3;
    constexpr uint8_t  VVC_APS_RANGE                     = 7;
    constexpr uint8_t  VVC_APS_LMCS_RANGE                = 3;
    constexpr uint8_t  VVC_MAX_NUM_ALF_CLASSES           = 25;
    constexpr uint8_t  VVC_MAX_NUM_ALF_LUMA_FILTERS      = 25;
    constexpr uint8_t  VVC_MAX_NUM_ALF_CHROMA_FILTERS    = 8;
    constexpr int8_t   VVC_ALF_LUMA_COEFF_LOWER_BOUND    = -128;
    constexpr int8_t   VVC_ALF_LUMA_COEFF_UPPER_BOUND    = 127;
    constexpr uint8_t  VVC_MAX_LUMA_NUM_COEFF            = 12;
    constexpr uint8_t  VVC_MAX_CHROMA_NUM_COEFF          = 6;
    constexpr uint8_t  VVC_MAX_CC_ALF_FILTERS            = 4;
    constexpr uint8_t  VVC_MAX_CC_ALF_COEFF              = 7;
    constexpr uint8_t  VVC_MAX_NUM_CC_ALF_CHROMA_COEFF   = 8;
    constexpr uint8_t  VVC_MAX_CC_ID_COEFF               = 2;
    constexpr uint8_t  LMCS_DELTA_CW_NUM                 = 16;
    constexpr uint8_t  VVC_MAX_SCALING_LIST_ID           = 28;
    constexpr uint8_t  SCALING_LIST_START_VALUE          = 8;
    constexpr uint16_t VVC_MAX_CU_SIZE                   = 128;
    constexpr uint8_t  SCALING_LIST_DC                   = 16;
    constexpr uint8_t  SCALING_LIST_NUM                  = 6;
    constexpr uint8_t  SCALING_LIST_VALUE                = 30;
    constexpr uint8_t  VVC_MAX_THREAD_NUM                = 128;
    constexpr uint8_t  VVC_MAX_FRAME_PIXEL_ALIGNED       = 16;
    constexpr uint32_t VVC_MAX_FRAME_WIDTH               = 16384;
    constexpr uint32_t VVC_MAX_FRAME_HEIGHT              = 16384;
    constexpr uint8_t  VVC_MAX_UNSUPPORTED_FRAME_ORDER   = 1;
    constexpr uint8_t  VVC_DXVA_SUBPIC_DECODE_BUFFER     = 10;
    constexpr uint8_t  VVC_DXVA_SLICE_STRUCT_BUFFER      = 11;
    constexpr uint8_t  VVC_DXVA_RPL_DECODE_BUFFER        = 12;
    constexpr uint8_t  VVC_DXVA_TILE_DECODE_BUFFER       = 13;

    enum ComponentID
    {
      COMPONENT_Y         = 0,
      COMPONENT_Cb        = 1,
      COMPONENT_Cr        = 2,
      MAX_NUM_COMPONENT   = 3,
      JOINT_CbCr          = MAX_NUM_COMPONENT,
      MAX_NUM_TBLOCKS     = MAX_NUM_COMPONENT
    };

    enum
    {
        NUMBER_OF_STATUS = 512,
    };

    // Display structure modes
    enum DisplayPictureStruct {
        DPS_FRAME     = 0,
        DPS_TOP,         // one field
        DPS_BOTTOM,      // one field
        DPS_TOP_BOTTOM,
        DPS_BOTTOM_TOP,
        DPS_TOP_BOTTOM_TOP,
        DPS_BOTTOM_TOP_BOTTOM,
        DPS_FRAME_DOUBLING,
        DPS_FRAME_TRIPLING,
        DPS_TOP_BOTTOM_PREV, // 9 - Top field paired with previous bottom field in output order
        DPS_BOTTOM_TOP_PREV, //10 - Bottom field paired with previous top field in output order
        DPS_TOP_BOTTOM_NEXT, //11 - Top field paired with next bottom field in output order
        DPS_BOTTOM_TOP_NEXT, //12 - Bottom field paired with next top field in output order
    };

    // VVC profile identifiers
    enum Profile
    {
        VVC_NONE                                 = 0,
        VVC_INTRA                                = 8,
        VVC_STILL_PICTURE                        = 64,
        VVC_MAIN_10                              = 1,
        VVC_MAIN_10_STILL_PICTURE                = VVC_MAIN_10 | VVC_STILL_PICTURE,
        VVC_MULTILAYER_MAIN_10                   = 17,
        VVC_MULTILAYER_MAIN_10_STILL_PICTURE     = VVC_MULTILAYER_MAIN_10 | VVC_STILL_PICTURE,
        VVC_MAIN_10_444                          = 33,
        VVC_MAIN_10_444_STILL_PICTURE            = VVC_MAIN_10_444 | VVC_STILL_PICTURE,
        VVC_MULTILAYER_MAIN_10_444               = 49,
        VVC_MULTILAYER_MAIN_10_444_STILL_PICTURE = VVC_MULTILAYER_MAIN_10_444 | VVC_STILL_PICTURE,
        VVC_MAIN_12                              = 2,
        VVC_MAIN_12_444                          = 34,
        VVC_MAIN_16_444                          = 35,
        VVC_MAIN_12_INTRA                        = VVC_MAIN_12 | VVC_INTRA,
        VVC_MAIN_12_444_INTRA                    = VVC_MAIN_12_444 | VVC_INTRA,
        VVC_MAIN_16_444_INTRA                    = VVC_MAIN_16_444 | VVC_INTRA,
        VVC_MAIN_12_STILL_PICTURE                = VVC_MAIN_12 | VVC_STILL_PICTURE,
        VVC_MAIN_12_444_STILL_PICTURE            = VVC_MAIN_12_444 | VVC_STILL_PICTURE,
        VVC_MAIN_16_444_STILL_PICTURE            = VVC_MAIN_16_444 | VVC_STILL_PICTURE,
    };

    // VVC tier identifiers
    enum Tier
    {
        MAIN_TIER = 0,
        HIGH_TIER = 1,
        NUMBER_OF_TIERS=2
    };

    // VVC level identifiers
    enum Level
    {
        // code = (major_level * 16 + minor_level * 3)
        VVC_LEVEL_1    = 16,
        VVC_LEVEL_2    = 32,
        VVC_LEVEL_21   = 35,
        VVC_LEVEL_3    = 48,
        VVC_LEVEL_31   = 51,
        VVC_LEVEL_4    = 64,
        VVC_LEVEL_41   = 67,
        VVC_LEVEL_5    = 80,
        VVC_LEVEL_51   = 83,
        VVC_LEVEL_52   = 86,
        VVC_LEVEL_6    = 96,
        VVC_LEVEL_61   = 99,
        VVC_LEVEL_62   = 102,
    };

    typedef uint8_t PlaneY;
    typedef uint8_t PlaneUV;
    typedef int16_t Coeffs;

    typedef Coeffs* CoeffsPtr;
    typedef PlaneY* PlanePtrY;
    typedef PlaneUV* PlanePtrUV;

    enum SPSExtensionFlagIndex
    {
        SPS_EXT__REXT = 0,
        NUM_SPS_EXTENSION_FLAGS = 8
    };

    // VVC NAL unit types
    // [robust] clean up unused macros if the spec is stable.
    typedef enum
    {
      NAL_UNIT_CODED_SLICE_TRAIL = 0,   // 0
      NAL_UNIT_CODED_SLICE_STSA,        // 1
      NAL_UNIT_CODED_SLICE_RADL,        // 2
      NAL_UNIT_CODED_SLICE_RASL,        // 3

      NAL_UNIT_RESERVED_VCL_4,
      NAL_UNIT_RESERVED_VCL_5,
      NAL_UNIT_RESERVED_VCL_6,

      NAL_UNIT_CODED_SLICE_IDR_W_RADL,  // 7
      NAL_UNIT_CODED_SLICE_IDR_N_LP,    // 8
      NAL_UNIT_CODED_SLICE_CRA,         // 9
      NAL_UNIT_CODED_SLICE_GDR,         // 10

      NAL_UNIT_RESERVED_IRAP_VCL_11,
      NAL_UNIT_OPI,                     // 12
      NAL_UNIT_DCI,                     // 13
      NAL_UNIT_VPS,                     // 14
      NAL_UNIT_SPS,                     // 15
      NAL_UNIT_PPS,                     // 16
      NAL_UNIT_PREFIX_APS,              // 17
      NAL_UNIT_SUFFIX_APS,              // 18
      NAL_UNIT_PH,                      // 19
      NAL_UNIT_ACCESS_UNIT_DELIMITER,   // 20
      NAL_UNIT_EOS,                     // 21
      NAL_UNIT_EOB,                     // 22
      NAL_UNIT_PREFIX_SEI,              // 23
      NAL_UNIT_SUFFIX_SEI,              // 24
      NAL_UNIT_FD,                      // 25

      NAL_UNIT_RESERVED_NVCL_26,
      NAL_UNIT_RESERVED_NVCL_27,

      NAL_UNIT_UNSPECIFIED_28,
      NAL_UNIT_UNSPECIFIED_29,
      NAL_UNIT_UNSPECIFIED_30,
      NAL_UNIT_UNSPECIFIED_31,
      NAL_UNIT_INVALID
    } NalUnitType;

    // SEI identifiers
    typedef enum
    {
        SEI_BUFFERING_PERIOD                     = 0,
        SEI_PICTURE_TIMING                       = 1,
        SEI_FILLER_PAYLOAD                       = 3,
        SEI_USER_DATA_REGISTERED_ITU_T_T35       = 4,
        SEI_USER_DATA_UNREGISTERED               = 5,
        SEI_FILM_GRAIN_CHARACTERISTICS           = 19,
        SEI_FRAME_PACKING                        = 45,
        SEI_DISPLAY_ORIENTATION                  = 47,
        SEI_PARAMETER_SETS_INCLUSION_INDICATION  = 129,
        SEI_DECODING_UNIT_INFO                   = 130,
        SEI_DECODED_PICTURE_HASH                 = 132,
        SEI_SCALABLE_NESTING                     = 133,
        SEI_MASTERING_DISPLAY_COLOUR_VOLUME      = 137,
        SEI_COLOUR_TRANSFORM_INFO                = 142,
        SEI_DEPENDENT_RAP_INDICATION             = 145,
        SEI_EQUIRECTANGULAR_PROJECTION           = 150,
        SEI_SPHERE_ROTATION                      = 154,
        SEI_REGION_WISE_PACKING                  = 155,
        SEI_OMNI_VIEWPORT                        = 156,
        SEI_GENERALIZED_CUBEMAP_PROJECTION       = 153,
        SEI_ALPHA_CHANNEL_INFO                   = 165,
        SEI_FRAME_FIELD_INFO                     = 168,
        SEI_DEPTH_REPRESENTATION_INFO            = 177,
        SEI_MULTIVIEW_ACQUISITION_INFO           = 179,
        SEI_MULTIVIEW_VIEW_POSITION              = 180,
        SEI_SUBPICTURE_LEVEL_INFO                = 203,
        SEI_SAMPLE_ASPECT_RATIO_INFO             = 204,
        SEI_CONTENT_LIGHT_LEVEL_INFO             = 144,
        SEI_ALTERNATIVE_TRANSFER_CHARACTERISTICS = 147,
        SEI_AMBIENT_VIEWING_ENVIRONMENT          = 148,
        SEI_CONTENT_COLOUR_VOLUME                = 149,
        SEI_ANNOTATED_REGIONS                    = 202,
        SEI_SCALABILITY_DIMENSION_INFO           = 205,
        SEI_EXTENDED_DRAP_INDICATION             = 206,
        SEI_CONSTRAINED_RASL_ENCODING            = 207,
    } SEI_TYPE;

    enum
    {
        CHROMA_FORMAT_400 = 0,
        CHROMA_FORMAT_420,
        CHROMA_FORMAT_422,
        CHROMA_FORMAT_444
    };

    // VVC APS parameters type
    enum ApsType
    {
        ALF_APS = 0,
        LMCS_APS = 1,
        SCALING_LIST_APS = 2,
    };

    // ScalingList parameters
    enum CoeffScanGroupType
    {
        SCAN_UNGROUPED = 0,
        SCAN_GROUPED_4x4 = 1,
        SCAN_NUMBER_OF_GROUP_TYPES = 2
    };

    enum CoeffScanType
    {
        SCAN_DIAG = 0,          ///< up-right diagonal scan
        SCAN_TRAV_HOR = 1,      ///< horizontal scan
        SCAN_TRAV_VER = 2,      ///< vertical scan
        SCAN_NUMBER_OF_TYPES
    };

#define SCALING_LIST_REM_NUM 6     ///< remainder of QP/6

    enum ScalingListSize
    {
        SCALING_LIST_1x1 = 0,
        SCALING_LIST_2x2,
        SCALING_LIST_4x4,
        SCALING_LIST_8x8,
        SCALING_LIST_16x16,
        SCALING_LIST_32x32,
        SCALING_LIST_64x64,
        SCALING_LIST_128x128,
        SCALING_LIST_SIZE_NUM,
        //for user define matrix
        SCALING_LIST_FIRST_CODED = SCALING_LIST_2x2,
        SCALING_LIST_LAST_CODED = SCALING_LIST_64x64
    };

    enum ScalingList1dStartIdx
    {
        SCALING_LIST_1D_START_2x2 = 0,
        SCALING_LIST_1D_START_4x4 = 2,
        SCALING_LIST_1D_START_8x8 = 8,
        SCALING_LIST_1D_START_16x16 = 14,
        SCALING_LIST_1D_START_32x32 = 20,
        SCALING_LIST_1D_START_64x64 = 26,
    };

    static const uint32_t scalingListIdx[/*SCALING_LIST_SIZE_NUM*/8][/*SCALING_LIST_NUM*/6] =
    {
      {  0,  0,  0,  0,  0,  0},  // SCALING_LIST_1x1
      {  0,  0,  0,  0,  0,  1},  // SCALING_LIST_2x2
      {  2,  3,  4,  5,  6,  7},  // SCALING_LIST_4x4
      {  8,  9, 10, 11, 12, 13},  // SCALING_LIST_8x8
      { 14, 15, 16, 17, 18, 19},  // SCALING_LIST_16x16
      { 20, 21, 22, 23, 24, 25},  // SCALING_LIST_32x32
      { 26, 21, 22, 27, 24, 25},  // SCALING_LIST_64x64
      {  0,  0,  0,  0,  0,  0},  // SCALING_LIST_128x128
    };;

    static const uint32_t scalingListSize[/*SCALING_LIST_SIZE_NUM*/8] = { 1, 4, 16, 64, 256, 1024, 4096, 16384 };

    struct ScanElement
    {
        uint32_t idx;
        uint32_t x;
        uint32_t y;
    };

    // Scaling list initialization scan lookup table
    static ScanElement sigLastScanCG32x32[64] =
    {
        {0}, {8}, {1}, {16}, {9}, {2}, {24}, {17},
        {10}, {3}, {32}, {25}, {18}, {11}, {4}, {40},
        {33}, {26}, {19}, {12}, {5}, {48}, {41}, {34},
        {27}, {20}, {13}, {6}, {56}, {49}, {42}, {35},
        {28}, {21}, {14}, {7}, {57}, {50}, {43}, {36},
        {29}, {22}, {15}, {58}, {51}, {44}, {37}, {30},
        {23}, {59}, {52}, {45}, {38}, {31}, {60}, {53},
        {46}, {39}, {61}, {54}, {47}, {62}, {55}, {63}
    };

    // Scaling list initialization scan lookup table
    static ScanElement ScanTableDiag4x4[16] =
    {
        {0}, {4}, {1}, {8},
        {5}, {2}, {12}, {9},
        {6}, {3}, {13}, {10},
        {7}, {14}, {11}, {15}
    };
    // Default scaling list 4x4
    static const int quantTSDefault4x4[4 * 4] =
    {
      16,16,16,16,
      16,16,16,16,
      16,16,16,16,
      16,16,16,16
    };

    static const int quantIntraDefault8x8[8 * 8] =
    {
      16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16
    };

    static const int quantInterDefault8x8[8 * 8] =
    {
      16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16,
      16,16,16,16,16,16,16,16
    };

    // Inverse QP scale lookup table
    static const int quantScales[2][SCALING_LIST_REM_NUM] = // can be represented as a 9 element table
    {
        { 26214,23302,20560,18396,16384,14564 },
        { 18396,16384,14564,13107,11651,10280 } // Note: last 3 values of second row == half of the first 3 values of the first row
    };

    static const int invQuantScales[2][SCALING_LIST_REM_NUM] = // can be represented as a 9 element table
    {
      { 40,45,51,57,64,72 },
      { 57,64,72,80,90,102 } // Note: last 3 values of second row == double of the first 3 values of the first row
    };

    struct ScalingList 
    {
        bool                scaling_list_pred_mode_flag_is_copy[SCALING_LIST_VALUE];
        int                 scaling_list_DC[SCALING_LIST_VALUE];   // DC value of 16x16 matrix coefficient
        uint32_t            ref_matrix_id[SCALING_LIST_VALUE];
        bool                scaling_list_pred_mode_flag[SCALING_LIST_VALUE];
        std::vector<int>    scaling_list_coef[SCALING_LIST_VALUE];   // quantization matrix
        bool                chroma_scaling_list_present_flag;
        int32_t             ScalingMatrixDCRec[14];
        int32_t             ScalingMatrixRec2x2[SCALING_LIST_1D_START_4x4][2][2];
        int32_t             ScalingMatrixRec4x4[SCALING_LIST_1D_START_8x8 - SCALING_LIST_1D_START_4x4][4][4];
        int32_t             ScalingMatrixRec8x8[VVC_MAX_SCALING_LIST_ID - SCALING_LIST_1D_START_8x8][8][8];
    };

    // Memory reference counting base class
    class RefCounter
    {
    public:

        RefCounter() : m_refCounter(0)
        {
        }

        void IncrementReference() const { m_refCounter++; };

        void DecrementReference()
        {
            m_refCounter--;

            assert(m_refCounter >= 0);
            if (!m_refCounter)
            {
                Free();
            };
        }

        void ResetRefCounter() { m_refCounter = 0; }

        uint32_t GetRefCounter() const { return m_refCounter; }

    protected:

        mutable int32_t m_refCounter;

        virtual ~RefCounter()
        {
        }

        virtual void Free()
        {
        }
    };

    // Basic ref counting heap object class
    class HeapObject : public RefCounter
    {
    public:

        virtual ~HeapObject() {}

        virtual void Reset()
        {
        }

        virtual void Free();
    };

    struct VVCAPSBase
    {
        ApsType                     aps_params_type;
        uint32_t                    aps_adaptation_parameter_set_id;
        bool                        aps_chroma_present_flag;
        bool                        aps_extension_flag;
        bool                        aps_extension_data_flag;
        uint8_t                     aps_size;

        bool                        alf_luma_filter_signal_flag;
        bool                        alf_chroma_filter_signal_flag;
        bool                        alf_cc_cb_filter_signal_flag;
        bool                        alf_cc_cr_filter_signal_flag;
        bool                        alf_luma_clip_flag;
        bool                        alf_chroma_clip_flag;
        uint32_t                    alf_luma_num_filters_signalled_minus1;
        std::vector<uint32_t>       alf_luma_coeff_delta_idx;
        uint32_t                    alf_chroma_num_alt_filters_minus1;
        uint32_t                    alf_luma_coeff_abs[VVC_MAX_NUM_ALF_LUMA_FILTERS][VVC_MAX_LUMA_NUM_COEFF];
        uint32_t                    alf_chroma_coeff_abs[VVC_MAX_NUM_ALF_CHROMA_FILTERS][VVC_MAX_CHROMA_NUM_COEFF];
        int8_t                      alf_luma_coeff_sign[VVC_MAX_NUM_ALF_LUMA_FILTERS][VVC_MAX_LUMA_NUM_COEFF];
        uint32_t                    alf_chroma_coeff_sign[VVC_MAX_NUM_ALF_CHROMA_FILTERS][VVC_MAX_CHROMA_NUM_COEFF];
        uint32_t                    alf_luma_clip_idx[VVC_MAX_NUM_ALF_LUMA_FILTERS][VVC_MAX_LUMA_NUM_COEFF];
        uint32_t                    alf_chroma_clip_idx[VVC_MAX_NUM_ALF_CHROMA_FILTERS][VVC_MAX_CHROMA_NUM_COEFF];
        uint32_t                    alf_cc_cb_filters_signalled_minus1;
        uint32_t                    alf_cc_cr_filters_signalled_minus1;
        uint32_t                    alf_cc_filters_signalled_minus1;
        uint32_t                    alf_cc_cr_mapped_coeff_abs[VVC_MAX_CC_ALF_FILTERS][VVC_MAX_CC_ALF_COEFF];
        uint32_t                    alf_cc_cr_coeff_sign[VVC_MAX_CC_ALF_FILTERS][VVC_MAX_CC_ALF_COEFF];
        uint32_t                    CcAlfApsCoeffCr[VVC_MAX_CC_ALF_FILTERS][VVC_MAX_CC_ALF_COEFF];
        uint32_t                    alf_cc_cb_mapped_coeff_abs[VVC_MAX_CC_ALF_FILTERS][VVC_MAX_CC_ALF_COEFF];
        uint32_t                    alf_cc_cb_coeff_sign[VVC_MAX_CC_ALF_FILTERS][VVC_MAX_CC_ALF_COEFF];
        uint32_t                    CcAlfApsCoeffCb[VVC_MAX_CC_ALF_FILTERS][VVC_MAX_CC_ALF_COEFF];
        bool                        alf_cc_filter_Idx_enabled[VVC_MAX_CC_ID_COEFF][VVC_MAX_CC_ALF_FILTERS];
        uint32_t                    alf_cc_coeff[VVC_MAX_CC_ID_COEFF][VVC_MAX_CC_ALF_FILTERS][VVC_MAX_NUM_CC_ALF_CHROMA_COEFF];

        uint32_t                    lmcs_min_bin_idx;
        uint32_t                    lmcs_delta_max_bin_idx;
        uint32_t                    lmcs_delta_cw_prec_minus1;
        uint32_t                    lmcs_delta_abs_cw[LMCS_DELTA_CW_NUM];
        uint32_t                    lmcs_delta_sign_cw_flag[LMCS_DELTA_CW_NUM];
        uint32_t                    lmcs_delta_abs_crs;
        uint32_t                    lmcs_delta_sign_crs_flag;
        bool                        chromaPresentFlag;

        ScalingList                 scalingListInfo;

        void Reset()
        {
            VVCAPSBase aps = {};
            *this = aps;
        }
    };

    struct VVCAPS : public HeapObject, public VVCAPSBase
    {
        VVCAPS()
            : HeapObject()
            , VVCAPSBase()
        {
            Reset();
        }

        ~VVCAPS()
        {}

        int32_t GetID() const
        {
            return (aps_adaptation_parameter_set_id << VVC_NUM_APS_TYPE_LEN) + aps_params_type;
        }

        virtual void Reset()
        {
            VVCAPSBase::Reset();
        }

        bool m_fresh = true;
    };

    // Slice types
    enum SliceType
    {
        B_SLICE = 0,
        P_SLICE = 1,
        I_SLICE = 2,
        NUMBER_OF_SLICE_TYPES = 3
    };

    // Convert one enum to another
    inline
        UMC::FrameType SliceTypeToFrameType(SliceType slice_type)
    {
        switch (slice_type)
        {
        case P_SLICE:
            return UMC::P_PICTURE;
        case B_SLICE:
            return UMC::B_PICTURE;
        case I_SLICE:
            return UMC::I_PICTURE;
        default:
            return UMC::NONE_PICTURE;
        }
    }

    enum ChannelType
    {
        CHANNEL_TYPE_LUMA = 0,
        CHANNEL_TYPE_CHROMA = 1,
        MAX_NUM_CHANNEL_TYPE = 2
    };

    /// reference list index
    enum RefPicList
    {
        REF_PIC_LIST_0 = 0,   ///< reference list 0
        REF_PIC_LIST_1 = 1,   ///< reference list 1
        NUM_REF_PIC_LIST_01 = 2,
        REF_PIC_LIST_X = 100  ///< special mark
    };

    struct ClpRng
    {
        int32_t     min;
        int32_t     max;
        int32_t     bd;
        int32_t     n;
    };

    struct ClpRngs
    {
        ClpRng comp[MAX_NUM_COMPONENT]; ///< the bit depth as indicated in the SPS
        bool used;
        bool chroma;
    };

    struct VVCConstraintInfo
    {
        bool     gci_present_flag;
        /* general */
        bool     gci_intra_only_constraint_flag;
        bool     gci_all_layers_independent_constraint_flag;
        bool     gci_one_au_only_constraint_flag;
        /* picture format */
        uint32_t gci_sixteen_minus_max_bitdepth_constraint_idc;
        uint32_t gci_three_minus_max_chroma_format_constraint_idc;
        /* NAL unit type related */
        bool     gci_no_mixed_nalu_types_in_pic_constraint_flag;
        bool     gci_no_trail_constraint_flag;
        bool     gci_no_stsa_constraint_flag;
        bool     gci_no_rasl_constraint_flag;
        bool     gci_no_radl_constraint_flag;
        bool     gci_no_idr_constraint_flag;
        bool     gci_no_cra_constraint_flag;
        bool     gci_no_gdr_constraint_flag;
        bool     gci_no_aps_constraint_flag;
        bool     gci_no_idr_rpl_constraint_flag;
        /* tile, slice, subpicture partitioning */
        bool     gci_one_tile_per_pic_constraint_flag;
        bool     gci_pic_header_in_slice_header_constraint_flag;
        bool     gci_one_slice_per_pic_constraint_flag;
        bool     gci_no_rectangular_slice_constraint_flag;
        bool     gci_one_slice_per_subpic_constraint_flag;
        bool     gci_no_subpic_info_constraint_flag;
        /* CTU and block partitioning */
        uint32_t gci_three_minus_max_log2_ctu_size_constraint_idc;
        bool     gci_no_partition_constraints_override_constraint_flag;
        bool     gci_no_mtt_constraint_flag;
        bool     gci_no_qtbtt_dual_tree_intra_constraint_flag;
        /* intra */
        bool     gci_no_palette_constraint_flag;
        bool     gci_no_ibc_constraint_flag;
        bool     gci_no_isp_constraint_flag;
        bool     gci_no_mrl_constraint_flag;
        bool     gci_no_mip_constraint_flag;
        bool     gci_no_cclm_constraint_flag;
        /* inter */
        bool     gci_no_ref_pic_resampling_constraint_flag;
        bool     gci_no_res_change_in_clvs_constraint_flag;
        bool     gci_no_weighted_prediction_constraint_flag;
        bool     gci_no_ref_wraparound_constraint_flag;
        bool     gci_no_temporal_mvp_constraint_flag;
        bool     gci_no_sbtmvp_constraint_flag;
        bool     gci_no_amvr_constraint_flag;
        bool     gci_no_bdof_constraint_flag;
        bool     gci_no_smvd_constraint_flag;
        bool     gci_no_dmvr_constraint_flag;
        bool     gci_no_mmvd_constraint_flag;
        bool     gci_no_affine_motion_constraint_flag;
        bool     gci_no_prof_constraint_flag;
        bool     gci_no_bcw_constraint_flag;
        bool     gci_no_ciip_constraint_flag;
        bool     gci_no_gpm_constraint_flag;
        /* transform, quantization, residual */
        bool     gci_no_luma_transform_size_64_constraint_flag;
        bool     gci_no_transform_skip_constraint_flag;
        bool     gci_no_bdpcm_constraint_flag;
        bool     gci_no_mts_constraint_flag;
        bool     gci_no_lfnst_constraint_flag;
        bool     gci_no_joint_cbcr_constraint_flag;
        bool     gci_no_sbt_constraint_flag;
        bool     gci_no_act_constraint_flag;
        bool     gci_no_explicit_scaling_list_constraint_flag;
        bool     gci_no_dep_quant_constraint_flag;
        bool     gci_no_sign_data_hiding_constraint_flag;
        bool     gci_no_cu_qp_delta_constraint_flag;
        bool     gci_no_chroma_qp_offset_constraint_flag;
        /* loop filter */
        bool     gci_no_sao_constraint_flag;
        bool     gci_no_alf_constraint_flag;
        bool     gci_no_ccalf_constraint_flag;
        bool     gci_no_lmcs_constraint_flag;
        bool     gci_no_ladf_constraint_flag;
        bool     gci_no_virtual_boundaries_constraint_flag;
        uint32_t gci_num_reserved_bits;
    };

    struct VVCProfileTierLevel
    {
        uint32_t              general_profile_idc;
        bool                  general_tier_flag;
        uint32_t              general_level_idc;
        bool                  ptl_frame_only_constraint_flag;
        bool                  ptl_multilayer_enabled_flag;
        VVCConstraintInfo     general_constraints_info;
        bool                  ptl_sublayer_level_present_flag[VVC_MAX_TLAYER];
        uint32_t              sublayer_level_idc[VVC_MAX_TLAYER];
        uint32_t              ptl_num_sub_profiles;
        std::vector<uint32_t> general_sub_profile_idc;
    };

    struct VVCDpbParameter
    {
        uint32_t dpb_max_dec_pic_buffering_minus1[VVC_MAX_TLAYER];
        uint32_t dpb_max_num_reorder_pics[VVC_MAX_TLAYER];
        uint32_t dpb_max_latency_increase_plus1[VVC_MAX_TLAYER];
    };

    struct VVCGeneralTimingHrdParams
    {
        uint32_t num_units_in_tick;
        uint32_t time_scale;
        bool     general_nal_hrd_params_present_flag;
        bool     general_vcl_hrd_params_present_flag;
        bool     general_same_pic_timing_in_all_ols_flag;
        bool     general_du_hrd_params_present_flag;
        uint32_t tick_divisor_minus2;
        uint32_t bit_rate_scale;
        uint32_t cpb_size_scale;
        uint32_t cpb_size_du_scale;
        uint32_t hrd_cpb_cnt_minus1;
    };

    struct VVCOlsTimingHrdParams
    {
        bool     fixed_pic_rate_general_flag[VVC_MAX_VPS_SUBLAYERS];
        bool     fixed_pic_rate_within_cvs_flag[VVC_MAX_VPS_SUBLAYERS];
        uint32_t elemental_duration_in_tc_minus1[VVC_MAX_VPS_SUBLAYERS];
        bool     low_delay_hrd_flag[VVC_MAX_VPS_SUBLAYERS];

        uint32_t bit_rate_value_minus1[VVC_MAX_VPS_SUBLAYERS][VVC_MAX_CPB_CNT][2];
        uint32_t cpb_size_value_minus1[VVC_MAX_VPS_SUBLAYERS][VVC_MAX_CPB_CNT][2];
        uint32_t cpb_size_du_value_minus1[VVC_MAX_VPS_SUBLAYERS][VVC_MAX_CPB_CNT][2];
        uint32_t bit_rate_du_value_minus1[VVC_MAX_VPS_SUBLAYERS][VVC_MAX_CPB_CNT][2];
        bool     cbr_flag[VVC_MAX_VPS_SUBLAYERS][VVC_MAX_CPB_CNT][2];
    };

    struct VVCVideoParamSetBase
    {
        uint32_t                                        vps_video_parameter_set_id;
        uint32_t                                        vps_max_layers_minus1;
        uint32_t                                        vps_max_sublayers_minus1;
        bool                                            vps_default_ptl_dpb_hrd_max_tid_flag;
        bool                                            vps_all_independent_layers_flag;
        uint32_t                                        vps_layer_id[VVC_MAX_VPS_LAYERS];
        bool                                            vps_independent_layer_flag[VVC_MAX_VPS_LAYERS];
        bool                                            vps_max_tid_ref_present_flag[VVC_MAX_VPS_LAYERS];
        bool                                            vps_direct_ref_layer_flag[VVC_MAX_VPS_LAYERS][VVC_MAX_VPS_LAYERS];
        uint32_t                                        m_directRefLayerIdx[VVC_MAX_VPS_LAYERS][VVC_MAX_VPS_LAYERS];
        uint32_t                                        vps_max_tid_il_ref_pics_plus1[VVC_MAX_VPS_LAYERS][VVC_MAX_VPS_LAYERS];
        bool                                            vps_each_layer_is_an_ols_flag;
        uint32_t                                        vps_ols_mode_idc;
        uint32_t                                        vps_num_output_layer_sets_minus2;
        bool                                            vps_ols_output_layer_flag[VVC_MAX_NUM_OLSS][VVC_MAX_VPS_LAYERS];
        uint32_t                                        vps_num_ptls_minus1;
        bool                                            vps_pt_present_flag[VVC_MAX_NUM_OLSS];
        uint32_t                                        vps_ptl_max_tid[VVC_MAX_NUM_OLSS];
        std::vector<VVCProfileTierLevel>                profile_tier_level;
        uint32_t                                        vps_ols_ptl_idx[VVC_MAX_NUM_OLSS];
        uint32_t                                        vps_num_dpb_params_minus1;
        bool                                            vps_sublayer_dpb_params_present_flag;
        std::vector<uint32_t>                           vps_dpb_max_tid;
        std::vector<VVCDpbParameter>                    dpb_parameters;
        std::vector<uint32_t>                           vps_ols_dpb_pic_width;
        std::vector<uint32_t>                           vps_ols_dpb_pic_height;
        std::vector<uint32_t>                           vps_ols_dpb_chroma_format;
        std::vector<uint32_t>                           vps_ols_dpb_bitdepth_minus8;
        std::vector<uint32_t>                           vps_ols_dpb_params_idx;
        bool                                            vps_timing_hrd_params_present_flag;
        VVCGeneralTimingHrdParams                       general_timing_hrd_parameters;
        bool                                            vps_sublayer_cpb_params_present_flag;
        uint32_t                                        vps_num_ols_timing_hrd_params_minus1;
        uint32_t                                        vps_hrd_max_tid[VVC_MAX_NUM_OLSS];
        std::vector<VVCOlsTimingHrdParams>              ols_timing_hrd_parameters;
        uint32_t                                        vps_ols_timing_hrd_idx[VVC_MAX_NUM_OLSS];
        bool                                            vps_extension_flag;
        bool                                            vps_extension_data_flag;
        // derived variables
        uint32_t                                        vps_total_num_olss;
        std::vector<uint32_t>                           vps_num_layers_in_ols;
        uint32_t                                        vps_num_multi_layered_olss;
        std::vector<std::vector<uint32_t>>              vps_layer_id_in_ols;
        std::vector<uint32_t>                           vps_num_output_layers_in_ols;
        std::vector<std::vector<uint32_t>>              vps_output_layer_id_in_ols;
        std::vector<std::vector<uint32_t>>              vps_num_sub_layers_in_layer_in_ols;
        std::vector<uint32_t>                           vps_target_output_layer_id_set;
        std::vector<uint32_t>                           vps_target_layer_id_set;
        uint32_t                                        vps_target_ols_idx;

        void Reset()
        {
            VVCVideoParamSetBase vps = {};
            *this = vps;
        }
    };

    struct VVCVideoParamSet : public HeapObject, public VVCVideoParamSetBase
    {
        VVCVideoParamSet()
            : HeapObject()
            , VVCVideoParamSetBase()
        {
            Reset();
        }

        ~VVCVideoParamSet()
        {}

        int32_t GetID() const
        {
            return vps_video_parameter_set_id;
        }

        virtual void Reset()
        {
            VVCVideoParamSetBase::Reset();
        }
    };

    struct VVCReferencePictureList
    {
        bool       rpl_sps_flag;
        uint32_t   num_ref_entries;
        uint32_t   rpl_idx;
        bool       ltrp_in_header_flag;
        bool       inter_layer_ref_pic_flag[VVC_MAX_NUM_REF_PICS];
        uint32_t   ilrp_idx[VVC_MAX_NUM_REF_PICS];
        bool       st_ref_pic_flag[VVC_MAX_NUM_REF_PICS];
        uint32_t   abs_delta_poc_st[VVC_MAX_NUM_REF_PICS];
        bool       strp_entry_sign_flag[VVC_MAX_NUM_REF_PICS];
        uint32_t   DeltaPocValSt[VVC_MAX_NUM_REF_PICS];
        uint32_t   poc_lsb_lt[VVC_MAX_NUM_REF_PICS];
        // derived variables
        uint32_t   number_of_short_term_pictures;
        uint32_t   number_of_long_term_pictures;
        uint32_t   number_of_inter_layer_pictures;
        bool       inter_layer_present_flag;
        int32_t    ref_pic_identifier[VVC_MAX_NUM_REF_PICS];
        bool       is_long_term_ref_pic[VVC_MAX_NUM_REF_PICS];
        bool       delta_poc_msb_present_flag[VVC_MAX_NUM_REF_PICS];
        uint32_t   delta_poc_msb_cycle_lt[VVC_MAX_NUM_REF_PICS];
        bool       is_inter_layer_ref_pic[VVC_MAX_NUM_REF_PICS];
        uint32_t   inter_layer_ref_pic_idx[VVC_MAX_NUM_REF_PICS];

        // used for RPL construction
        VVCDecoderFrame*    refFrame;
        bool                isLongReference[VVC_MAX_NUM_REF + 1];
        int32_t             POC[VVC_MAX_NUM_REF + 1];
    };

    struct VVCVUI
    {
        bool       vui_progressive_source_flag;
        bool       vui_interlaced_source_flag;
        bool       vui_non_packed_constraint_flag;
        bool       vui_non_projected_constraint_flag;
        bool       vui_aspect_ratio_info_present_flag;
        bool       vui_aspect_ratio_constant_flag;
        uint32_t   vui_aspect_ratio_idc;
        uint32_t   vui_sar_width;
        uint32_t   vui_sar_height;
        bool       vui_overscan_info_present_flag;
        bool       vui_overscan_appropriate_flag;
        bool       vui_colour_description_present_flag;
        uint32_t   vui_colour_primaries;
        uint32_t   vui_transfer_characteristics;
        uint32_t   vui_matrix_coeffs;
        bool       vui_full_range_flag;
        bool       vui_chroma_loc_info_present_flag;
        uint32_t   vui_chroma_sample_loc_type_top_field;
        uint32_t   vui_chroma_sample_loc_type_bottom_field;
        uint32_t   vui_chroma_sample_loc_type;
        bool       vui_reserved_payload_extension_data;
    };

    struct VVCSeqParamSetBase
    {
        uint32_t                             sps_seq_parameter_set_id;
        uint32_t                             sps_video_parameter_set_id;
        uint32_t                             sps_max_sublayers_minus1;
        uint32_t                             sps_chroma_format_idc;
        uint32_t                             sps_log2_ctu_size_minus5;
        bool                                 sps_ptl_dpb_hrd_params_present_flag;
        VVCProfileTierLevel                  profile_tier_level;
        bool                                 sps_gdr_enabled_flag;
        bool                                 sps_ref_pic_resampling_enabled_flag;
        bool                                 sps_res_change_in_clvs_allowed_flag;
        uint32_t                             sps_pic_width_max_in_luma_samples;
        uint32_t                             sps_pic_height_max_in_luma_samples;
        bool                                 sps_conformance_window_flag;
        uint32_t                             sps_conf_win_left_offset;
        uint32_t                             sps_conf_win_right_offset;
        uint32_t                             sps_conf_win_top_offset;
        uint32_t                             sps_conf_win_bottom_offset;
        bool                                 sps_subpic_info_present_flag;
        uint32_t                             sps_num_subpics_minus1;
        bool                                 sps_independent_subpics_flag;
        bool                                 sps_subpic_same_size_flag;
        std::vector<uint32_t>                sps_subpic_ctu_top_left_x;
        std::vector<uint32_t>                sps_subpic_ctu_top_left_y;
        std::vector<uint32_t>                sps_subpic_width_minus1;
        std::vector<uint32_t>                sps_subpic_height_minus1;
        std::vector<bool>                    sps_subpic_treated_as_pic_flag;
        std::vector<bool>                    sps_loop_filter_across_subpic_enabled_flag;
        uint32_t                             sps_subpic_id_len_minus1;
        bool                                 sps_subpic_id_mapping_explicitly_signalled_flag;
        bool                                 sps_subpic_id_mapping_present_flag;
        std::vector<uint32_t>                sps_subpic_id;
        uint32_t                             sps_bitdepth_minus8;
        bool                                 sps_entropy_coding_sync_enabled_flag;
        bool                                 sps_entry_point_offsets_present_flag;
        uint32_t                             sps_log2_max_pic_order_cnt_lsb_minus4;
        bool                                 sps_poc_msb_cycle_flag;
        uint32_t                             sps_poc_msb_cycle_len_minus1;
        uint32_t                             sps_num_extra_ph_bytes;
        std::vector<bool>                    sps_extra_ph_bit_present_flag;
        uint32_t                             sps_num_extra_sh_bytes;
        std::vector<bool>                    sps_extra_sh_bit_present_flag;
        bool                                 sps_sublayer_dpb_params_flag;
        VVCDpbParameter                      dpb_parameter;
        uint32_t                             sps_log2_min_luma_coding_block_size_minus2;
        bool                                 sps_partition_constraints_override_enabled_flag;
        uint32_t                             sps_log2_diff_min_qt_min_cb_intra_slice_luma;
        uint32_t                             sps_max_mtt_hierarchy_depth_intra_slice_luma;
        uint32_t                             sps_log2_diff_max_bt_min_qt_intra_slice_luma;
        uint32_t                             sps_log2_diff_max_tt_min_qt_intra_slice_luma;
        bool                                 sps_qtbtt_dual_tree_intra_flag;
        uint32_t                             sps_log2_diff_min_qt_min_cb_intra_slice_chroma;
        uint32_t                             sps_max_mtt_hierarchy_depth_intra_slice_chroma;
        uint32_t                             sps_log2_diff_max_bt_min_qt_intra_slice_chroma;
        uint32_t                             sps_log2_diff_max_tt_min_qt_intra_slice_chroma;
        uint32_t                             sps_log2_diff_min_qt_min_cb_inter_slice;
        uint32_t                             sps_max_mtt_hierarchy_depth_inter_slice;
        uint32_t                             sps_log2_diff_max_bt_min_qt_inter_slice;
        uint32_t                             sps_log2_diff_max_tt_min_qt_inter_slice;
        bool                                 sps_max_luma_transform_size_64_flag;
        bool                                 sps_transform_skip_enabled_flag;
        uint32_t                             sps_log2_transform_skip_max_size_minus2;
        bool                                 sps_bdpcm_enabled_flag;
        bool                                 sps_mts_enabled_flag;
        bool                                 sps_explicit_mts_intra_enabled_flag;
        bool                                 sps_explicit_mts_inter_enabled_flag;
        bool                                 sps_lfnst_enabled_flag;
        bool                                 sps_joint_cbcr_enabled_flag;
        bool                                 sps_same_qp_table_for_chroma_flag;
        int32_t                              sps_qp_table_start_minus26[VVC_MAX_NUM_CQP_MAPPING_TABLES];
        int32_t                              sps_num_points_in_qp_table_minus1[VVC_MAX_NUM_CQP_MAPPING_TABLES];
        std::vector<int32_t>                 sps_delta_qp_in_val_minus1[VVC_MAX_NUM_CQP_MAPPING_TABLES];
        std::vector<int32_t>                 sps_delta_qp_diff_val[VVC_MAX_NUM_CQP_MAPPING_TABLES];
        bool                                 sps_sao_enabled_flag;
        bool                                 sps_alf_enabled_flag;
        bool                                 sps_ccalf_enabled_flag;
        bool                                 sps_lmcs_enabled_flag;
        bool                                 sps_weighted_pred_flag;
        bool                                 sps_weighted_bipred_flag;
        bool                                 sps_long_term_ref_pics_flag;
        bool                                 sps_inter_layer_prediction_enabled_flag;
        bool                                 sps_idr_rpl_present_flag;
        bool                                 sps_rpl1_same_as_rpl0_flag;
        uint32_t                             sps_num_ref_pic_lists[VVC_MAX_NUM_REF_PIC_LISTS];
        std::vector<VVCReferencePictureList> sps_ref_pic_lists[VVC_MAX_NUM_REF_PIC_LISTS];
        bool                                 sps_ref_wraparound_enabled_flag;
        bool                                 sps_temporal_mvp_enabled_flag;
        bool                                 sps_sbtmvp_enabled_flag;
        bool                                 sps_amvr_enabled_flag;
        bool                                 sps_bdof_enabled_flag;
        bool                                 sps_bdof_control_present_in_ph_flag;
        bool                                 sps_smvd_enabled_flag;
        bool                                 sps_dmvr_enabled_flag;
        bool                                 sps_dmvr_control_present_in_ph_flag;
        bool                                 sps_mmvd_enabled_flag;
        bool                                 sps_mmvd_fullpel_only_enabled_flag;
        uint32_t                             sps_six_minus_max_num_merge_cand;
        bool                                 sps_sbt_enabled_flag;
        bool                                 sps_affine_enabled_flag;
        uint32_t                             sps_five_minus_max_num_subblock_merge_cand;
        bool                                 sps_6param_affine_enabled_flag;
        bool                                 sps_affine_amvr_enabled_flag;
        bool                                 sps_affine_prof_enabled_flag;
        bool                                 sps_prof_control_present_in_ph_flag;
        bool                                 sps_bcw_enabled_flag;
        bool                                 sps_ciip_enabled_flag;
        bool                                 sps_gpm_enabled_flag;
        uint32_t                             sps_max_num_merge_cand_minus_max_num_gpm_cand;
        uint32_t                             sps_log2_parallel_merge_level_minus2;
        bool                                 sps_isp_enabled_flag;
        bool                                 sps_mrl_enabled_flag;
        bool                                 sps_mip_enabled_flag;
        bool                                 sps_cclm_enabled_flag;
        bool                                 sps_chroma_horizontal_collocated_flag;
        bool                                 sps_chroma_vertical_collocated_flag;
        bool                                 sps_palette_enabled_flag;
        bool                                 sps_act_enabled_flag;
        uint32_t                             sps_min_qp_prime_ts;
        bool                                 sps_ibc_enabled_flag;
        uint32_t                             sps_six_minus_max_num_ibc_merge_cand;
        bool                                 sps_ladf_enabled_flag;
        uint32_t                             sps_num_ladf_intervals_minus2;
        int32_t                              sps_ladf_lowest_interval_qp_offset;
        int32_t                              sps_ladf_qp_offset[VVC_MAX_LADF_INTERVALS];
        uint32_t                             sps_ladf_delta_threshold_minus1[VVC_MAX_LADF_INTERVALS];
        bool                                 sps_explicit_scaling_list_enabled_flag;
        bool                                 sps_scaling_matrix_for_lfnst_disabled_flag;
        bool                                 sps_scaling_matrix_for_alternative_colour_space_disabled_flag;
        bool                                 sps_scaling_matrix_designated_colour_space_flag;
        bool                                 sps_dep_quant_enabled_flag;
        bool                                 sps_sign_data_hiding_enabled_flag;
        bool                                 sps_virtual_boundaries_enabled_flag;
        bool                                 sps_virtual_boundaries_present_flag;
        uint32_t                             sps_num_ver_virtual_boundaries;
        uint32_t                             sps_virtual_boundary_pos_x_minus1[3];
        uint32_t                             sps_num_hor_virtual_boundaries;
        uint32_t                             sps_virtual_boundary_pos_y_minus1[3];
        bool                                 sps_timing_hrd_params_present_flag;
        VVCGeneralTimingHrdParams            general_timing_hrd_parameters;
        bool                                 sps_sublayer_cpb_params_present_flag;
        VVCOlsTimingHrdParams                ols_timing_hrd_parameters;
        bool                                 sps_field_seq_flag;
        bool                                 sps_vui_parameters_present_flag;
        uint32_t                             sps_vui_payload_size_minus1;
        bool                                 sps_extension_flag;
        bool                                 sps_extension_data_flag;
        VVCVUI                               vui;
        //derived variables
        uint32_t                             sps_ctu_size;
        uint32_t                             sps_max_cu_width;
        uint32_t                             sps_max_cu_height;
        uint32_t                             sps_min_qt[3];
        uint32_t                             sps_max_mtt_hierarchy_depth[3];
        uint32_t                             sps_max_bt_size[3];
        uint32_t                             sps_max_tt_size[3];
        uint32_t                             sps_log2_max_tb_size;
        uint32_t                             sps_num_qp_tables;
        int32_t                              sps_qp_bd_offset[VVC_MAX_NUM_CHANNEL_TYPE];
        uint32_t                             sps_max_num_geo_cand;
        bool                                 extended_precision_processing_flag;
        bool                                 sps_ts_residual_coding_rice_present_in_sh_flag;
        bool                                 rrc_rice_extension_flag;
        bool                                 persistent_rice_adaptation_enabled_flag;
        bool                                 reverse_last_position_enabled_flag;

        void Reset()
        {
            VVCSeqParamSetBase sps = {};
            *this = sps;
        }
    };

    struct VVCSeqParamSet : public HeapObject, public VVCSeqParamSetBase
    {
        bool m_changed;

        VVCSeqParamSet()
            : HeapObject()
            , VVCSeqParamSetBase()
        {
            Reset();
            m_changed = false;
        }

        ~VVCSeqParamSet()
        {}

        int32_t GetID() const
        {
            return sps_seq_parameter_set_id;
        }

        virtual void Reset()
        {
            VVCSeqParamSetBase::Reset();
        }
    };

    struct VVCRectSlice
    {
        uint32_t         pps_tile_idx;
        uint32_t         pps_slice_width_in_tiles_minus1;
        uint32_t         pps_slice_height_in_tiles_minus1;
        uint32_t         pps_num_slices_in_tile;
        uint32_t         pps_slice_height_in_ctu_minus1;
        bool             pps_derived_exp_slice_height_flag;
    };

    struct SubPic 
    {
        uint32_t                sub_pic_id;
        uint32_t                sub_pic_idx;
        uint32_t                sub_pic_ctu_top_leftx;
        uint32_t                sub_pic_ctu_top_lefty;
        uint32_t                sub_pic_width;
        uint32_t                sub_pic_height;
        uint32_t                first_ctu_in_subpic;
        uint32_t                last_ctu_in_subpic;
        uint32_t                sub_pic_left;
        uint32_t                sub_pic_right;
        uint32_t                subpic_width_in_luma_sample;
        uint32_t                subpic_height_in_luma_sample;
        uint32_t                sub_pic_top;
        uint32_t                sub_pic_bottom;
        std::vector<uint32_t>   ctu_addr_in_subpic;
        uint32_t                num_slices_in_subPic;
    };

    struct SliceMap
    {
        uint32_t                slice_id;
        uint32_t                num_tiles_in_slice;
        uint32_t                num_ctu_in_slice;
        std::vector<uint32_t>   ctu_addr_in_slice;
    };

    struct VVCPicParamSetBase
    {
        uint32_t                  pps_pic_parameter_set_id;
        uint32_t                  pps_seq_parameter_set_id;
        bool                      pps_mixed_nalu_types_in_pic_flag;
        uint32_t                  pps_pic_width_in_luma_samples;
        uint32_t                  pps_pic_height_in_luma_samples;
        bool                      pps_conformance_window_flag;
        uint32_t                  pps_conf_win_left_offset;
        uint32_t                  pps_conf_win_right_offset;
        uint32_t                  pps_conf_win_top_offset;
        uint32_t                  pps_conf_win_bottom_offset;
        bool                      pps_scaling_window_explicit_signalling_flag;
        int32_t                   pps_scaling_win_left_offset;
        int32_t                   pps_scaling_win_right_offset;
        int32_t                   pps_scaling_win_top_offset;
        int32_t                   pps_scaling_win_bottom_offset;
        bool                      pps_output_flag_present_flag;
        bool                      pps_no_pic_partition_flag;
        bool                      pps_subpic_id_mapping_present_flag;
        uint32_t                  pps_num_subpics;
        uint32_t                  pps_subpic_id_len;
        std::vector<uint32_t>     pps_subpic_id;
        uint32_t                  pps_log2_ctu_size;
        uint32_t                  pps_num_exp_tile_columns_minus1;
        uint32_t                  pps_num_exp_tile_rows_minus1;
        std::vector<uint32_t>     pps_tile_column_width;
        std::vector<uint32_t>     pps_tile_row_height;
        bool                      pps_loop_filter_across_tiles_enabled_flag;
        bool                      pps_rect_slice_flag;
        bool                      pps_single_slice_per_subpic_flag;
        std::vector<uint32_t>     pps_ctu_to_subpic_idx;
        uint32_t                  pps_num_slices_in_pic_minus1;
        bool                      pps_tile_idx_delta_present_flag;
        bool                      pps_loop_filter_across_slices_enabled_flag;
        bool                      pps_cabac_init_present_flag;
        uint32_t                  pps_num_ref_idx_default_active_minus1[2];
        bool                      pps_rpl1_idx_present_flag;
        bool                      pps_weighted_pred_flag;
        bool                      pps_weighted_bipred_flag;
        bool                      pps_ref_wraparound_enabled_flag;
        uint32_t                  pps_pic_width_minus_wraparound_offset;
        int32_t                   pps_init_qp_minus26;
        bool                      pps_cu_qp_delta_enabled_flag;
        bool                      pps_chroma_tool_offsets_present_flag;
        int32_t                   pps_cb_qp_offset;
        int32_t                   pps_cr_qp_offset;
        bool                      pps_joint_cbcr_qp_offset_present_flag;
        int32_t                   pps_joint_cbcr_qp_offset_value;
        bool                      pps_slice_chroma_qp_offsets_present_flag;
        bool                      pps_cu_chroma_qp_offset_list_enabled_flag;
        uint32_t                  pps_chroma_qp_offset_list_len;
        int32_t                   pps_cb_qp_offset_list[VVC_MAX_QP_OFFSET_LIST_SIZE];
        int32_t                   pps_cr_qp_offset_list[VVC_MAX_QP_OFFSET_LIST_SIZE];
        int32_t                   pps_joint_cbcr_qp_offset_list[VVC_MAX_QP_OFFSET_LIST_SIZE];
        bool                      pps_deblocking_filter_control_present_flag;
        bool                      pps_deblocking_filter_override_enabled_flag;
        bool                      pps_deblocking_filter_disabled_flag;
        bool                      pps_dbf_info_in_ph_flag;
        int32_t                   pps_luma_beta_offset_div2;
        int32_t                   pps_luma_tc_offset_div2;
        int32_t                   pps_cb_beta_offset_div2;
        int32_t                   pps_cb_tc_offset_div2;
        int32_t                   pps_cr_beta_offset_div2;
        int32_t                   pps_cr_tc_offset_div2;
        bool                      pps_rpl_info_in_ph_flag;
        bool                      pps_sao_info_in_ph_flag;
        bool                      pps_alf_info_in_ph_flag;
        bool                      pps_wp_info_in_ph_flag;
        bool                      pps_qp_delta_info_in_ph_flag;
        bool                      pps_picture_header_extension_present_flag;
        bool                      pps_slice_header_extension_present_flag;
        bool                      pps_extension_flag;
        bool                      pps_extension_data_flag;
        // derived variables
        uint32_t                  pps_ctu_size;
        uint32_t                  pps_pic_width_in_ctu;
        uint32_t                  pps_pic_height_in_ctu;
        uint32_t                  pps_num_tile_cols;
        uint32_t                  pps_num_tile_rows;
        uint32_t                  pps_num_tiles;
        std::vector<uint32_t>     pps_tile_col_bd;
        std::vector<uint32_t>     pps_tile_row_bd;
        std::vector<uint32_t>     pps_ctu_to_tile_col;
        std::vector<uint32_t>     pps_ctu_to_tile_row;
        std::vector<VVCRectSlice*>pps_rect_slices;
        std::vector<SubPic*>      pps_sub_pics;
        std::vector<SliceMap*>    pps_slice_map;
        uint32_t                  pps_num_slices_in_pic;

        void Reset()
        {
            VVCPicParamSetBase pps = {};
            *this = pps;
        }
    };

    struct VVCPicParamSet : public HeapObject, public VVCPicParamSetBase
    {
        bool m_changed;

        VVCPicParamSet()
            : HeapObject()
            , VVCPicParamSetBase()
        {
            Reset();
            m_changed = false;
        }

        ~VVCPicParamSet()
        {}

        int32_t GetID() const
        {
            return pps_pic_parameter_set_id;
        }

        virtual void Reset()
        {
            VVCPicParamSetBase::Reset();
        }
    };

    //Weighted prediction parameters
    struct VVCParsedWP
    {
        uint32_t                luma_log2_weight_denom;
        int32_t                 delta_chroma_log2_weight_denom;

        uint32_t                num_l0_weights;
        uint8_t                 luma_weight_l0_flag[15];
        uint8_t                 chroma_weight_l0_flag[15];
        int32_t                 delta_luma_weight_l0[15];
        int32_t                 luma_offset_l0[15];
        int32_t                 delta_chroma_weight_l0[15][2];
        int32_t                 delta_chroma_offset_l0[15][2];

        uint32_t                num_l1_weights;
        uint8_t                 luma_weight_l1_flag[15];
        uint8_t                 chroma_weight_l1_flag[15];
        int32_t                 delta_luma_weight_l1[15];
        int32_t                 luma_offset_l1[15];
        int32_t                 delta_chroma_weight_l1[15][2];
        int32_t                 delta_chroma_offset_l1[15][2];
    };

    struct VVCPicHeaderBase
    {
        bool                    ph_gdr_or_irap_pic_flag;
        bool                    ph_non_ref_pic_flag;
        bool                    ph_gdr_pic_flag;
        bool                    ph_inter_slice_allowed_flag;
        bool                    ph_intra_slice_allowed_flag;
        uint32_t                ph_pic_parameter_set_id;
        int32_t                 ph_pic_order_cnt_lsb;
        int32_t                 ph_recovery_poc_cnt;
        std::vector<uint32_t>   ph_extra_bit;
        bool                    ph_poc_msb_cycle_present_flag;
        int32_t                 ph_poc_msb_cycle_val;
        bool                    ph_alf_enabled_flag;
        std::vector<uint8_t>    ph_alf_aps_id_luma;
        uint32_t                ph_num_alf_aps_ids_luma;
        bool                    ph_alf_cb_enabled_flag;
        bool                    ph_alf_cr_enabled_flag;
        uint32_t                ph_alf_aps_id_chroma;
        bool                    ph_alf_cc_cb_enabled_flag;
        int32_t                 ph_alf_cc_cb_aps_id;
        bool                    ph_alf_cc_cr_enabled_flag;
        int32_t                 ph_alf_cc_cr_aps_id;
        bool                    ph_lmcs_enabled_flag;
        uint32_t                ph_lmcs_aps_id;
        bool                    ph_chroma_residual_scale_flag;
        bool                    ph_explicit_scaling_list_enabled_flag;
        uint32_t                ph_scaling_list_aps_id;
        bool                    ph_virtual_boundaries_present_flag;
        uint32_t                ph_num_ver_virtual_boundaries;
        uint32_t                ph_virtual_boundary_pos_x_minus1[3];
        uint32_t                ph_num_hor_virtual_boundaries;
        uint32_t                ph_virtual_boundary_pos_y_minus1[3];
        bool                    ph_pic_output_flag;
        bool                    rpl_sps_flag[2];
        int32_t                 rpl_idx[2];
        int32_t                 poc_lbs_lt[VVC_MAX_NUM_REF_PICS];
        uint8_t                 delta_poc_msb_present_flag[VVC_MAX_NUM_REF_PICS];
        uint8_t                 delta_poc_msb_cycle_lt[VVC_MAX_NUM_REF_PICS];
        bool                    ph_partition_constraints_override_flag;
        uint32_t                ph_log2_diff_min_qt_min_cb_intra_slice_luma;
        uint32_t                ph_max_mtt_hierarchy_depth_intra_slice_luma;
        uint32_t                ph_log2_diff_max_bt_min_qt_intra_slice_luma;
        uint32_t                ph_log2_diff_max_tt_min_qt_intra_slice_luma;
        uint32_t                ph_log2_diff_min_qt_min_cb_intra_slice_chroma;
        uint32_t                ph_max_mtt_hierarchy_depth_intra_slice_chroma;
        uint32_t                ph_log2_diff_max_bt_min_qt_intra_slice_chroma;
        uint32_t                ph_log2_diff_max_tt_min_qt_intra_slice_chroma;
        uint32_t                ph_cu_qp_delta_subdiv_intra_slice;
        uint32_t                ph_cu_chroma_qp_offset_subdiv_intra_slice;
        uint32_t                ph_log2_diff_min_qt_min_cb_inter_slice;
        uint32_t                ph_max_mtt_hierarchy_depth_inter_slice;
        uint32_t                ph_log2_diff_max_bt_min_qt_inter_slice;
        uint32_t                ph_log2_diff_max_tt_min_qt_inter_slice;
        uint32_t                ph_cu_qp_delta_subdiv_inter_slice;
        uint32_t                ph_cu_chroma_qp_offset_subdiv_inter_slice;
        bool                    ph_temporal_mvp_enabled_flag;
        bool                    ph_collocated_from_l0_flag;
        uint32_t                ph_collocated_ref_idx;
        bool                    ph_mmvd_fullpel_only_flag;
        bool                    ph_mvd_l1_zero_flag;
        bool                    ph_bdof_disabled_flag;
        bool                    ph_dmvr_disabled_flag;
        bool                    ph_prof_disabled_flag;
        int32_t                 ph_qp_delta;
        bool                    ph_joint_cbcr_sign_flag;
        bool                    ph_sao_luma_enabled_flag;
        bool                    ph_sao_chroma_enabled_flag;
        bool                    ph_deblocking_params_present_flag;
        bool                    ph_deblocking_filter_disabled_flag;
        int32_t                 ph_luma_beta_offset_div2;
        int32_t                 ph_luma_tc_offset_div2;
        int32_t                 ph_cb_beta_offset_div2;
        int32_t                 ph_cb_tc_offset_div2;
        int32_t                 ph_cr_beta_offset_div2;
        int32_t                 ph_cr_tc_offset_div2;
        VVCParsedWP             weightPredTable;
        VVCReferencePictureList rpl[2];
        uint32_t                ph_extension_length;
        std::vector<uint32_t>   ph_extension_data_byte;
        int                     ph_num_lx_weights[NUM_REF_PIC_LIST_01];
        bool                    no_output_before_recovery_flag;
        bool                    ph_num_ref_entries_rpl_larger_than0[NUM_REF_PIC_LIST_01];

        void Reset()
        {
            VVCPicHeaderBase ph = {};
            *this = ph;
        }
    };

    struct VVCPicHeader : public HeapObject, public VVCPicHeaderBase
    {
        VVCPicHeader()
            : HeapObject()
            , VVCPicHeaderBase()
        {
            Reset();
        }

        ~VVCPicHeader()
        {}

        int32_t GetID() const
        {
            return ph_pic_parameter_set_id;
        }

        virtual void Reset()
        {
            VVCPicHeaderBase::Reset();
        }
    };

    struct VVCSliceHeader
    {
        // from nal unit header
        NalUnitType                 nal_unit_type;
        uint32_t                    nuh_layer_id;
        uint32_t                    nuh_temporal_id;

        //VVC Spec params
        uint32_t                    sh_subpic_id;
        uint32_t                    slice_subPic_id;
        int32_t                     sh_slice_address;
        std::vector<bool>           sh_extra_bit;
        int32_t                     sh_num_tiles_in_slice_minus1;
        SliceType                   slice_type;
        int32_t                     slice_pic_order_cnt_lsb;
        int32_t                     slice_poc_lsb_lt[VVC_MAX_NUM_REF_PICS];
        uint8_t                     sh_no_output_of_prior_pics_flag;
        uint32_t                    slice_pic_parameter_set_id;

        // calculated  params
        bool                        sh_picture_header_in_slice_header_flag;
        bool                        IdrPicFlag;
        bool                        IDRflag;
        int                         m_poc;
        VVCReferencePictureList     sh_rpl[VVC_RPL_NUM];
        int32_t                     sh_rpl_idx[VVC_RPL_NUM];
        bool                        deblocking_filter_disable_flag;
        bool                        alf_enabled_flag[MAX_NUM_COMPONENT];
        int                         num_alf_aps_ids_luma;
        std::vector<uint8_t>        alf_aps_ids_luma;
        uint32_t                    alf_aps_id_chroma;
        bool                        alf_cc_cb_enabled_flag;
        uint32_t                    alf_cc_cb_aps_id;
        bool                        alf_cc_cr_enabled_flag;
        uint32_t                    alf_cc_cr_aps_id;
        bool                        alf_cc_enabled_flag[MAX_NUM_COMPONENT - 1];
        bool                        sh_lmcs_used_flag;
        bool                        sh_explicit_scaling_list_used_flag;
        bool                        ref_pic_list_sps_flag[VVC_MAX_NUM_REF_PIC_LISTS];
        int32_t                     slice_delta_poc_msb_cycle_lt[VVC_MAX_NUM_REF_PICS];
        int32_t                     num_ref_Idx[NUM_REF_PIC_LIST_01];
        bool                        m_checkLDC;
        bool                        sh_num_ref_idx_active_override_flag;
        uint32_t                    sh_num_ref_idx_active_minus1[VVC_NUM_REF_IDX_ACTIVATE];
        bool                        cabac_init_flag;
        SliceType                   enc_CABA_table_idx;                   // Used to transmit table section across slices
        bool                        sh_collocated_from_l0_flag;
        uint32_t                    sh_collocated_ref_idx;
        VVCParsedWP                 weightPredTable;
        int32_t                     sh_qp_delta;
        int32_t                     sliceQP;
        int32_t                     sh_cb_qp_offset;
        int32_t                     sh_cr_qp_offset;
        int32_t                     sh_chroma_qp_delta[MAX_NUM_COMPONENT + 1];
        int32_t                     sh_joint_cbcr_qp_offset;
        bool                        sh_cu_chroma_qp_offset_enabled_flag;
        bool                        sh_sao_luma_used_flag;
        bool                        sh_sao_chroma_used_flag;
        bool                        sh_deblocking_params_present_flag;
        int32_t                     sh_luma_beta_offset_div2;
        int32_t                     sh_luma_tc_offset_div2;
        int32_t                     sh_cb_beta_offset_div2;
        int32_t                     sh_cb_tc_offset_div2;
        int32_t                     sh_cr_beta_offset_div2;
        int32_t                     sh_cr_tc_offset_div2;
        bool                        sh_dep_quant_used_flag;
        bool                        sh_sign_data_hiding_used_flag;
        bool                        sh_ts_residual_coding_disabled_flag;
        int32_t                     sh_ts_residual_coding_rice_idx_minus1;
        bool                        sh_reverse_last_sig_coeff_flag;
        uint32_t                    num_entry_point_offsets;
        uint32_t                    NumEntryPoints;
        uint16_t                    slice_data_offset;
        uint16_t                    slice_data_bit_offset;
        uint16_t                    slice_segment_address;
        bool                        first_slice_in_sequence;
        bool                        NoBackwardPredFlag;

        // calculated params which are calculated from values above and not written to the bitstream
        uint32_t                    m_HeaderBitstreamOffset;
    };

    struct VVCSEIPayLoad
    {
        SEI_TYPE    payLoadType;
        uint32_t    payLoadSize;

        // ...
    };

    struct VVCOPIBase
    {
        bool      opi_ols_info_present_flag;
        bool      opi_htid_info_present_flag;
        uint32_t  opi_ols_idx;
        uint32_t  opi_htid_plus1;
        bool      opi_extension_flag;
        bool      opi_extension_data_flag;

        void Reset()
        {
            VVCOPIBase opi = {};
            *this = opi;
        }
    };

    struct VVCOPI : public HeapObject, public VVCOPIBase
    {
        VVCOPI()
            : HeapObject()
            , VVCOPIBase()
        {
            Reset();
        }

        ~VVCOPI()
        {}

        int32_t GetID() const
        {
            return opi_ols_idx;
        }

        virtual void Reset()
        {
            VVCOPIBase::Reset();
        }
    };

    // POC container
    class PocDecoding
    {
    public:
        int32_t prevPocPicOrderCntLsb;
        int32_t prevPicOrderCntMsb;

        PocDecoding()
            : prevPocPicOrderCntLsb(0)
            , prevPicOrderCntMsb(0)
        {
        }

        void Reset()
        {
            prevPocPicOrderCntLsb = 0;
            prevPicOrderCntMsb = 0;
        }
    };


    // HRD VUI information
    struct VVCHRD
    {
        uint8_t       nal_hrd_parameters_present_flag;
        uint8_t       vcl_hrd_parameters_present_flag;
        uint8_t       sub_pic_hrd_params_present_flag;
        // ...
    };

    struct RPL
    {
        uint8_t       rpl_sps_flag[2];
        // ...
    };

    // RPS data structure
    struct ReferencePictureSet
    {
        uint8_t       inter_ref_pic_set_prediction_flag;

        uint32_t      num_negative_pics;
        uint32_t      num_positive_pics;
        // ...
    };


    struct WPScalingParam
    {
      // Explicit weighted prediction parameters parsed in slice header,
      // or Implicit weighted prediction parameters (8 bits depth values).
      bool           presentFlag;
      // ...
    };

    // Container for raw header binary data
    class RawHeader_VVC
    {
    public:
        void Reset()
        { m_buffer.clear(); }

        size_t GetSize() const
        { return m_buffer.size(); }

        uint8_t * GetPointer()
        { return m_buffer.empty() ? nullptr : m_buffer.data(); }

        void Resize(size_t newSize)
        { m_buffer.resize(newSize); }

    protected:
        typedef std::vector<uint8_t> BufferType;
        BufferType  m_buffer;
    };

    // Error exception
    class vvc_exception
        : public std::runtime_error
    {
        public:
            vvc_exception(int32_t status)
                : std::runtime_error("VVC error")
                , m_status(status)
            {}
            int32_t GetStatus() const
            {
                return m_status;
            }
        private:
            int32_t m_status;
    };

    // Calculate maximum allowed bitstream NAL unit size based on picture width/height
    inline size_t CalculateSuggestedSize(const VVCSeqParamSet * sps)
    {
        size_t base_size = sps->sps_pic_width_max_in_luma_samples * sps->sps_pic_height_max_in_luma_samples;
        size_t size = 0;

        switch (sps->sps_chroma_format_idc)
        {
        case 0:  // YUV400
            size = base_size;
            break;
        case 1:  // YUV420
            size = (base_size * 3) / 2;
            break;
        case 2: // YUV422
            size = base_size + base_size;
            break;
        case 3: // YUV444
            size = base_size + base_size + base_size;
            break;
        };

        return 2 * size;
    }

    //#define Clip3(_min, _max, _x) std::min(std::max(_min, _x), _max)
    template <typename T> inline T Clip3(const T minVal, const T maxVal, const T a)
    {
        return std::min<T>(std::max<T>(minVal, a), maxVal);
    }
}

#endif // __UMC_VVC_DEC_DEFS_DEC_H__
#endif // MFX_ENABLE_VVC_VIDEO_DECODE
