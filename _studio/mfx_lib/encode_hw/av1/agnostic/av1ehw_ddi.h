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

typedef struct tagENCODE_CAPS_AV1
{
    union
    {
        struct
        {
            uint32_t   CodingLimitSet              : 1;
            uint32_t   ForcedSegmentationSupport   : 1;
            uint32_t   AutoSegmentationSupport     : 1;
            uint32_t   BRCReset                    : 1;
            uint32_t   TemporalLayerRateCtrl       : 3;
            uint32_t   DynamicScaling              : 1;
            uint32_t   NumScalablePipesMinus1      : 4;
            uint32_t   UserMaxFrameSizeSupport     : 1;
            uint32_t   DirtyRectSupport            : 1;
            uint32_t   MoveRectSupport             : 1;
            uint32_t   TileSizeBytesMinus1         : 2;
            uint32_t   FrameOBUSupport             : 1;
            uint32_t   SuperResSupport             : 1;
            uint32_t   CDEFChannelStrengthSupport  : 1;
            uint32_t   RandomAccessSupport         : 1; 
            uint32_t                               : 11;
        };
        uint32_t       CodingLimits;
    };

    union
    {
        struct
        {
            uint8_t   EncodeFunc    : 1; // Enc+Pak
            uint8_t   HybridPakFunc : 1; // Hybrid Pak function
            uint8_t   EncFunc       : 1; // Enc only function
            uint8_t   reserved      : 5; // 0
        };
        uint8_t       CodingFunction;
    };

    union {
        struct {
            uint8_t   SEG_LVL_ALT_Q      : 1;
            uint8_t   SEG_LVL_ALT_LF_Y_V : 1;
            uint8_t   SEG_LVL_ALT_LF_Y_H : 1;
            uint8_t   SEG_LVL_ALT_LF_U   : 1;
            uint8_t   SEG_LVL_ALT_LF_V   : 1;
            uint8_t   SEG_LVL_REF_FRAME  : 1;
            uint8_t   SEG_LVL_SKIP       : 1;
            uint8_t   SEG_LVL_GLOBALMV   : 1;
        };
        uint8_t SegmentFeatureSupport;
    };

    uint16_t reserved16b;
    uint32_t MaxPicWidth;
    uint32_t MaxPicHeight;

    uint8_t  MaxNum_ReferenceL0_P;  // [1..7]
    uint8_t  MaxNum_ReferenceL0_B;  // [1..7]
    uint8_t  MaxNum_ReferenceL1_B;  // [1..7]
    uint8_t  reserved8b;

    uint16_t MaxNumOfDirtyRect;
    uint16_t MaxNumOfMoveRect;

    union {
        struct {
            uint32_t   still_picture              : 1;
            uint32_t   use_128x128_superblock     : 1;
            uint32_t   enable_filter_intra        : 1;
            uint32_t   enable_intra_edge_filter   : 1;

            // read_compound_tools
            uint32_t   enable_interintra_compound : 1;
            uint32_t   enable_masked_compound     : 1;
            uint32_t   enable_warped_motion       : 1;
            uint32_t   PaletteMode                : 1;
            uint32_t   enable_dual_filter         : 1;
            uint32_t   enable_order_hint          : 1;
            uint32_t   enable_jnt_comp            : 1;
            uint32_t   enable_ref_frame_mvs       : 1;
            uint32_t   enable_superres            : 1;
            uint32_t   enable_cdef                : 1;
            uint32_t   enable_restoration         : 1;
            uint32_t   allow_intrabc              : 1;
            uint32_t   ReservedBits               : 16;
        } fields;
        uint32_t value;
    } AV1ToolSupportFlags;

    union {
        struct {
            uint8_t   i420        : 1;  // support I420
            uint8_t   i422        : 1;  // support I422
            uint8_t   i444        : 1;  // support I444
            uint8_t   mono_chrome : 1;  // support mono
            uint8_t   RGB         : 1;  // support RGB 
            uint8_t   reserved    : 3;  // [0]
        } fields;
        uint32_t value;
    } ChromaSupportFlags;

    union {
        struct {
            uint8_t   eight_bits  : 1;  // support 8 bits
            uint8_t   ten_bits    : 1;  // support 10 bits
            uint8_t   twelve_bits : 1;  // support 12 bits
            uint8_t   reserved    : 5;  // [0]
        } fields;
        uint32_t value;
    } BitDepthSupportFlags;

    union {
        struct {
            uint8_t  EIGHTTAP        : 1;
            uint8_t  EIGHTTAP_SMOOTH : 1;
            uint8_t  EIGHTTAP_SHARP  : 1;
            uint8_t  BILINEAR        : 1;
            uint8_t  SWITCHABLE      : 1;
            uint8_t  reserved        : 3;  // [0]
        } fields;
        uint8_t value;
    } SupportedInterpolationFilters;

    uint8_t  MinSegIdBlockSizeAccepted;

    union {
        struct {
            uint32_t  CQP                      : 1;
            uint32_t  CBR                      : 1;
            uint32_t  VBR                      : 1;
            uint32_t  AVBR                     : 1;
            uint32_t  ICQ                      : 1;
            uint32_t  VCM                      : 1;
            uint32_t  QVBR                     : 1;
            uint32_t  CQL                      : 1;
            uint32_t  reserved1                : 8; // [0]
            uint32_t  SlidingWindow            : 1;
            uint32_t  LowDelay                 : 1;
            uint32_t  LookaheadBRCSupport      : 1;  
            uint32_t  LookaheadAnalysisSupport : 1;  
            uint32_t  TCBRCSupport             : 1;
            uint32_t  reserved2                : 11; // [0]
        } fields;
        uint32_t value;
    } SupportedRateControlMethods;

    union {
        struct {
            uint8_t   enable_frame : 1;  // support frame level quality info
            uint8_t   enable_block : 1;  // support block level quality info
            uint8_t   reserved3    : 6;  // [0]
        } fields;
        uint8_t value;
    } QualityInfoSupportFlags;
    uint8_t    reserved8b2[3];

    uint32_t   reserved32b[15];

} ENCODE_CAPS_AV1;

typedef enum {
    SINGLE_REFERENCE      = 0,
    COMPOUND_REFERENCE    = 1,
    REFERENCE_MODE_SELECT = 2,
    REFERENCE_MODES       = 3,
} REFERENCE_MODE;
