// Copyright (c) 2009-2018 Intel Corporation
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

#include <assert.h>
#include "mfx_h264_enc_common.h"
#include "mfx_ext_buffers.h"
#include "mfxsvc.h"

// only to avoid ICL warnings from assert(!"fatal_error_text");
inline bool suppress_icl_warning(const void* p) { return p!=0; }

#if defined(MFX_ENABLE_H264_VIDEO_ENCODER_COMMON)

UMC_H264_ENCODER::SB_Type mapSBTypeMFXUMC_B[4][4] = {
    { SBTYPE_FORWARD_8x8, SBTYPE_BACKWARD_8x8, SBTYPE_BIDIR_8x8,SBTYPE_DIRECT },
    { SBTYPE_FORWARD_8x4, SBTYPE_BACKWARD_8x4, SBTYPE_BIDIR_8x4,SBTYPE_DIRECT },
    { SBTYPE_FORWARD_4x8, SBTYPE_BACKWARD_4x8, SBTYPE_BIDIR_4x8,SBTYPE_DIRECT },
    { SBTYPE_FORWARD_4x4, SBTYPE_BACKWARD_4x4, SBTYPE_BIDIR_4x4,SBTYPE_DIRECT }
};

UMC_H264_ENCODER::SB_Type mapSBTypeMFXUMC_P[4] = { SBTYPE_8x8, SBTYPE_8x4, SBTYPE_4x4, SBTYPE_DIRECT };
mfxU8 mfx_mbtype[32]={
   0, //  MBTYPE_INTRA            = 0,  // 4x4 or 8x8
   0, //  MBTYPE_INTRA_16x16      = 1,
   MFX_MBTYPE_INTRA_PCM, //  MBTYPE_PCM              = 2,  // Raw Pixel Coding, qualifies as a INTRA type...
   MFX_MBTYPE_INTER_16X16_0, //  MBTYPE_INTER            = 3,  // 16x16
   MFX_MBTYPE_INTER_16X8_00, //  MBTYPE_INTER_16x8       = 4,
   MFX_MBTYPE_INTER_8X16_00, //  MBTYPE_INTER_8x16       = 5,
   MFX_MBTYPE_INTER_OTHERS, //  MBTYPE_INTER_8x8        = 6,
   MFX_MBTYPE_INTER_OTHERS, //  MBTYPE_INTER_8x8_REF0   = 7,  // same as MBTYPE_INTER_8x8, with all RefIdx=0
   MFX_MBTYPE_INTER_16X16_0, //  MBTYPE_FORWARD          = 8,
   MFX_MBTYPE_INTER_16X16_1, //  MBTYPE_BACKWARD         = 9,
   MFX_MBTYPE_SKIP_16X16_0, //  MBTYPE_SKIPPED          = 10,
   MFX_MBTYPE_INTER_16X16_0, //  MBTYPE_DIRECT           = 11,
   MFX_MBTYPE_INTER_16X16_2, //  MBTYPE_BIDIR            = 12,
   MFX_MBTYPE_INTER_16X8_00, //  MBTYPE_FWD_FWD_16x8     = 13,
   MFX_MBTYPE_INTER_8X16_00, //  MBTYPE_FWD_FWD_8x16     = 14,
   MFX_MBTYPE_INTER_16X8_11, //  MBTYPE_BWD_BWD_16x8     = 15,
   MFX_MBTYPE_INTER_8X16_11, //  MBTYPE_BWD_BWD_8x16     = 16,
   MFX_MBTYPE_INTER_16X8_01, //  MBTYPE_FWD_BWD_16x8     = 17,
   MFX_MBTYPE_INTER_8X16_01, //  MBTYPE_FWD_BWD_8x16     = 18,
   MFX_MBTYPE_INTER_16X8_10, //  MBTYPE_BWD_FWD_16x8     = 19,
   MFX_MBTYPE_INTER_16X8_10, //  MBTYPE_BWD_FWD_8x16     = 20,
   MFX_MBTYPE_INTER_16X8_20, //  MBTYPE_BIDIR_FWD_16x8   = 21,
   MFX_MBTYPE_INTER_8X16_20, //  MBTYPE_BIDIR_FWD_8x16   = 22,
   MFX_MBTYPE_INTER_16X8_21, //  MBTYPE_BIDIR_BWD_16x8   = 23,
   MFX_MBTYPE_INTER_8X16_21, //  MBTYPE_BIDIR_BWD_8x16   = 24,
   MFX_MBTYPE_INTER_16X8_02, //  MBTYPE_FWD_BIDIR_16x8   = 25,
   MFX_MBTYPE_INTER_8X16_02, //  MBTYPE_FWD_BIDIR_8x16   = 26,
   MFX_MBTYPE_INTER_16X8_12, //  MBTYPE_BWD_BIDIR_16x8   = 27,
   MFX_MBTYPE_INTER_8X16_12, //  MBTYPE_BWD_BIDIR_8x16   = 28,
   MFX_MBTYPE_INTER_16X8_22, //  MBTYPE_BIDIR_BIDIR_16x8 = 29,
   MFX_MBTYPE_INTER_8X16_22, //  MBTYPE_BIDIR_BIDIR_8x16 = 30,
   MFX_MBTYPE_INTER_OTHERS //  MBTYPE_B_8x8 = 31,
};

MFX_SBTYPE mfx_sbtype[17] = {
    {MFX_SUBSHP_NO_SPLIT, MFX_SUBDIR_REF_0}, // SBTYPE_8x8          = 0,     // P slice modes
    {MFX_SUBSHP_TWO_8X4, MFX_SUBDIR_REF_0}, // SBTYPE_8x4          = 1,
    {MFX_SUBSHP_TWO_4X8, MFX_SUBDIR_REF_0},// SBTYPE_4x8          = 2,
    {MFX_SUBSHP_FOUR_4X4, MFX_SUBDIR_REF_0}, // SBTYPE_4x4          = 3,
    {MFX_SUBSHP_NO_SPLIT, MFX_SUBDIR_INTRA}, // SBTYPE_DIRECT       = 4,     // B Slice modes
    {MFX_SUBSHP_NO_SPLIT, MFX_SUBDIR_REF_0}, // SBTYPE_FORWARD_8x8  = 5,     // Subtract 4 for mode #
    {MFX_SUBSHP_NO_SPLIT, MFX_SUBDIR_REF_1}, // SBTYPE_BACKWARD_8x8 = 6,
    {MFX_SUBSHP_NO_SPLIT, MFX_SUBDIR_BIDIR},// SBTYPE_BIDIR_8x8    = 7,
    {MFX_SUBSHP_TWO_8X4, MFX_SUBDIR_REF_0},// SBTYPE_FORWARD_8x4  = 8,
    {MFX_SUBSHP_TWO_4X8, MFX_SUBDIR_REF_0},// SBTYPE_FORWARD_4x8  = 9,
    {MFX_SUBSHP_TWO_8X4, MFX_SUBDIR_REF_1},// SBTYPE_BACKWARD_8x4 = 10,
    {MFX_SUBSHP_TWO_4X8, MFX_SUBDIR_REF_1},// SBTYPE_BACKWARD_4x8 = 11,
    {MFX_SUBSHP_TWO_8X4, MFX_SUBDIR_BIDIR},// SBTYPE_BIDIR_8x4    = 12,
    {MFX_SUBSHP_TWO_4X8, MFX_SUBDIR_BIDIR},// SBTYPE_BIDIR_4x8    = 13,
    {MFX_SUBSHP_FOUR_4X4, MFX_SUBDIR_REF_0},// SBTYPE_FORWARD_4x4  = 14,
    {MFX_SUBSHP_FOUR_4X4, MFX_SUBDIR_REF_1},// SBTYPE_BACKWARD_4x4 = 15,
    {MFX_SUBSHP_FOUR_4X4, MFX_SUBDIR_BIDIR},// SBTYPE_BIDIR_4x4    = 16
};

/* ITU-T Rec. H.264 Table E-1 */
static const mfxU16 tab_AspectRatio[17][2] =
{
    { 1,  1},  // unspecified
    { 1,  1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, {24, 11}, {20, 11}, {32, 11},
    {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160,99}, { 4,  3}, { 3,  2}, { 2,  1}
};

//Defines from UMC encoder
#define MV_SEARCH_TYPE_FULL             0
#define MV_SEARCH_TYPE_CLASSIC_LOG      1
#define MV_SEARCH_TYPE_LOG              2
#define MV_SEARCH_TYPE_EPZS             3
#define MV_SEARCH_TYPE_FULL_ORTHOGONAL  4
#define MV_SEARCH_TYPE_LOG_ORTHOGONAL   5
#define MV_SEARCH_TYPE_TTS              6
#define MV_SEARCH_TYPE_NEW_EPZS         7
#define MV_SEARCH_TYPE_UMH              8
#define MV_SEARCH_TYPE_SQUARE           9
#define MV_SEARCH_TYPE_FTS             10
#define MV_SEARCH_TYPE_SMALL_DIAMOND   11

UMC_H264_ENCODER::MBTypeValue ConvertMBTypeToUMC(const mfxMbCodeAVC& mbCode, bool biDir)
{
    if (mbCode.IntraMbFlag)
    {
        if (mbCode.MbType5Bits == 0)
        {
            return MBTYPE_INTRA;
        }
        else if (mbCode.MbType5Bits < 25)
        {
            return MBTYPE_INTRA_16x16;
        }
        else if (mbCode.MbType5Bits == 25)
        {
            return MBTYPE_PCM;
        }
        else
        {
            assert(!suppress_icl_warning("bad mbtype"));
        }
    }
    else if (!biDir)
    {
        switch (mbCode.MbType5Bits)
        {
        case  1: return MBTYPE_INTER;
        case  4: return MBTYPE_INTER_16x8;
        case  5: return MBTYPE_INTER_8x16;
        case 22:
            if (mbCode.RefPicSelect[0][0] == 0 && mbCode.RefPicSelect[0][1] == 0 &&
                mbCode.RefPicSelect[0][2] == 0 && mbCode.RefPicSelect[0][3] == 0)
            {
                return MBTYPE_INTER_8x8_REF0;
            }
            else
            {
                return MBTYPE_INTER_8x8;
            }

        default: assert(!suppress_icl_warning("bad mbtype"));
        }
    }
    else
    {
        /*
        if (mbCode.reserved2b == 1) // SkipMbFlag
        {
            return MBTYPE_SKIPPED;
        }
        else*/ if (mbCode.Skip8x8Flag == 0xf)
        {
            return MBTYPE_DIRECT;
        }


        switch (mbCode.MbType5Bits)
        {
        case  1: return MBTYPE_FORWARD;
        case  2: return MBTYPE_BACKWARD;
        case  3: return MBTYPE_BIDIR;
        case  4: return MBTYPE_FWD_FWD_16x8;
        case  5: return MBTYPE_FWD_FWD_8x16;
        case  6: return MBTYPE_BWD_BWD_16x8;
        case  7: return MBTYPE_BWD_BWD_8x16;
        case  8: return MBTYPE_FWD_BWD_16x8;
        case  9: return MBTYPE_FWD_BWD_8x16;
        case 10: return MBTYPE_BWD_FWD_16x8;
        case 11: return MBTYPE_BWD_FWD_8x16;
        case 12: return MBTYPE_FWD_BIDIR_16x8;
        case 13: return MBTYPE_FWD_BIDIR_8x16;
        case 14: return MBTYPE_BWD_BIDIR_16x8;
        case 15: return MBTYPE_BWD_BIDIR_8x16;
        case 16: return MBTYPE_BIDIR_FWD_16x8;
        case 17: return MBTYPE_BIDIR_FWD_8x16;
        case 18: return MBTYPE_BIDIR_BWD_16x8;
        case 19: return MBTYPE_BIDIR_BWD_8x16;
        case 20: return MBTYPE_BIDIR_BIDIR_16x8;
        case 21: return MBTYPE_BIDIR_BIDIR_8x16;
        case 22: return MBTYPE_B_8x8;
        default: assert(!suppress_icl_warning("bad mbtype"));
        }
    }

    return NUMBER_OF_MBTYPES;
}

mfxI64 CalculateDTSFromPTS_H264enc(mfxFrameInfo info, mfxU16 dpb_output_delay, mfxU64 TimeStamp)
{
    if (TimeStamp != static_cast<mfxU64>(MFX_TIMESTAMP_UNKNOWN))
    {
        mfxF64 tcDuration90KHz = (mfxF64)info.FrameRateExtD / (info.FrameRateExtN * 2) * 90000; // calculate tick duration
        return mfxI64(TimeStamp - tcDuration90KHz * dpb_output_delay); // calculate DTS from PTS
    }
    
    return MFX_TIMESTAMP_UNKNOWN;
}

// check for known ExtBuffers, returns error code. or -1 if found unknown
// zero mfxExtBuffer* are OK
mfxStatus CheckExtBuffers_H264enc(mfxExtBuffer** ebuffers, mfxU32 nbuffers)
{
#ifdef UMC_ENABLE_MVC_VIDEO_ENCODER
    mfxU32 ID_list[] = { MFX_EXTBUFF_CODING_OPTION, MFX_EXTBUFF_CODING_OPTION_SPSPPS, MFX_EXTBUFF_VPP_AUXDATA, MFX_EXTBUFF_DDI, MFX_EXTBUFF_MVC_SEQ_DESC, MFX_EXTBUFF_VIDEO_SIGNAL_INFO,
        MFX_EXTBUFF_PICTURE_TIMING_SEI, MFX_EXTBUFF_AVC_TEMPORAL_LAYERS,
        MFX_EXTBUFF_SVC_SEQ_DESC, MFX_EXTBUFF_SVC_RATE_CONTROL, MFX_EXTBUFF_CODING_OPTION2, MFX_EXTBUFF_CODING_OPTION3 };
#else
    mfxU32 ID_list[] = { MFX_EXTBUFF_CODING_OPTION, MFX_EXTBUFF_CODING_OPTION_SPSPPS, MFX_EXTBUFF_VPP_AUXDATA, MFX_EXTBUFF_DDI, MFX_EXTBUFF_VIDEO_SIGNAL_INFO,
        MFX_EXTBUFF_PICTURE_TIMING_SEI, MFX_EXTBUFF_AVC_TEMPORAL_LAYERS,
        MFX_EXTBUFF_SVC_SEQ_DESC, MFX_EXTBUFF_SVC_RATE_CONTROL, MFX_EXTBUFF_CODING_OPTION2};
#endif
    mfxU32 ID_found[sizeof(ID_list)/sizeof(ID_list[0])] = {0,};
    if (!ebuffers) return MFX_ERR_NONE;
    for(mfxU32 i=0; i<nbuffers; i++) {
        bool is_known = false;
        if (!ebuffers[i]) return MFX_ERR_NULL_PTR; //continue;
        for (mfxU32 j=0; j<sizeof(ID_list)/sizeof(ID_list[0]); j++)
            if (ebuffers[i]->BufferId == ID_list[j]) {
                if (ID_found[j])
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                is_known = true;
                ID_found[j] = 1; // to avoid duplicated
                break;
            }
        if (!is_known)
            return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}

Ipp32s ConvertColorFormat_H264enc(mfxU16 ColorFormat)
{
    switch (ColorFormat) {
    case MFX_CHROMAFORMAT_MONOCHROME:
        return 0;
    case MFX_CHROMAFORMAT_YUV420:
        return 1;
    case MFX_CHROMAFORMAT_YUV422:
        return 2;
    case MFX_CHROMAFORMAT_YUV444:
    default:
        return -1; // not supported
    }
}

UMC::ColorFormat ConvertColorFormatToUMC(mfxU16 ColorFormat)
{
    switch (ColorFormat) {
    case MFX_CHROMAFORMAT_MONOCHROME:
        return UMC::GRAY;
    case MFX_CHROMAFORMAT_YUV420:
        return UMC::YUV420;
    case MFX_CHROMAFORMAT_YUV422:
        return UMC::YUV422;
    case MFX_CHROMAFORMAT_YUV444:
        return UMC::YUV444;
    default:
        return UMC::NONE; // not supported
    }
}

EnumPicCodType ConvertPicType_H264enc(mfxU16 FrameType)
{
    switch( FrameType & 0xf ){
        case MFX_FRAMETYPE_I:
        case MFX_FRAMETYPE_xI:
            return INTRAPIC;
        case MFX_FRAMETYPE_P:
        case MFX_FRAMETYPE_xP:
            return PREDPIC;
        case MFX_FRAMETYPE_B:
        case MFX_FRAMETYPE_xB:
            return BPREDPIC;
        default:
            return (EnumPicCodType)-1;
    }
}

EnumPicClass ConvertPicClass_H264enc(mfxU16 FrameType)
{
    if( FrameType & MFX_FRAMETYPE_IDR ) return IDR_PIC;
    if( FrameType & MFX_FRAMETYPE_REF) return REFERENCE_PIC;
    return DISPOSABLE_PIC;
}

Ipp32s ConvertPicStruct_H264enc(mfxU16 PicStruct)
{
    if ((PicStruct & MFX_PICSTRUCT_FIELD_TFF) || (PicStruct & MFX_PICSTRUCT_FIELD_BFF))
        return 1;
    if (PicStruct & MFX_PICSTRUCT_PROGRESSIVE)
        return 0;
    return 3;
}

static Ipp32u ConvertBitrate_H264enc(mfxU32 TargetKbps)
{
    return (TargetKbps * 1000);
}

mfxU32 ConvertSARtoIDC_H264enc(mfxU32 sarw, mfxU32 sarh)
{
    mfxU32 i;
    for (i=1; i<sizeof(tab_AspectRatio)/sizeof(tab_AspectRatio[0]); i++)
        if (sarw * tab_AspectRatio[i][1] == sarh * tab_AspectRatio[i][0])
            return i;
    return 0;
}

mfxStatus ConvertStatus_H264enc(UMC::Status status)
{
    switch (status) {
        case UMC::UMC_OK :
            return MFX_ERR_NONE;
        case UMC::UMC_ERR_NULL_PTR :
            return MFX_ERR_NULL_PTR;
        case UMC::UMC_ERR_NOT_ENOUGH_BUFFER :
            return MFX_ERR_NOT_ENOUGH_BUFFER;
        case UMC::UMC_ERR_ALLOC :
            return MFX_ERR_MEMORY_ALLOC;
        default :
            return MFX_ERR_UNSUPPORTED;
    }
}

mfxU16 ConvertCUCProfileToUMC( mfxU16 profile )
{
    switch( profile )
    {
    case  MFX_PROFILE_AVC_BASELINE:
    case  MFX_PROFILE_AVC_CONSTRAINED_BASELINE:
        return 66;
    case MFX_PROFILE_AVC_MAIN:
        return 77;
    case MFX_PROFILE_AVC_HIGH:
    case  MFX_PROFILE_AVC_PROGRESSIVE_HIGH:
    case  MFX_PROFILE_AVC_CONSTRAINED_HIGH:
        return 100;
#ifdef UMC_ENABLE_MVC_VIDEO_ENCODER
    case MFX_PROFILE_AVC_MULTIVIEW_HIGH:
        return 118;
    case MFX_PROFILE_AVC_STEREO_HIGH:
        return 128;
#endif
    case MFX_PROFILE_AVC_SCALABLE_BASELINE:
        return 83;
    case MFX_PROFILE_AVC_SCALABLE_HIGH:
        return 86;
    }
    return 0;
}

mfxU16 ConvertUMCProfileToCUC( mfxU16 profile )
{
    switch( profile )
    {
    case  66:
        return MFX_PROFILE_AVC_BASELINE;
    case 77:
        return MFX_PROFILE_AVC_MAIN;
    case 88:
    case 100:
    case 110:
    case 122:
    case 144:
        return MFX_PROFILE_AVC_HIGH;
#ifdef UMC_ENABLE_MVC_VIDEO_ENCODER
    case 118:
        return MFX_PROFILE_AVC_MULTIVIEW_HIGH;
    case 128:
        return MFX_PROFILE_AVC_STEREO_HIGH;
#endif
    case 83:
        return MFX_PROFILE_AVC_SCALABLE_BASELINE;
    case 86:
        return MFX_PROFILE_AVC_SCALABLE_HIGH;
    }
    return 0;
}


mfxU16 ConvertCUCLevelToUMC( mfxU16 level )
{
    switch( level )
    {
    case MFX_LEVEL_AVC_1:
        return 10;
    case MFX_LEVEL_AVC_1b:
        return 9;
    case MFX_LEVEL_AVC_11:
        return 11;
    case MFX_LEVEL_AVC_12:
        return 12;
    case MFX_LEVEL_AVC_13:
        return 13;
    case MFX_LEVEL_AVC_2:
        return 20;
    case MFX_LEVEL_AVC_21:
        return 21;
    case MFX_LEVEL_AVC_22:
        return 22;
    case MFX_LEVEL_AVC_3:
        return 30;
    case MFX_LEVEL_AVC_31:
        return 31;
    case MFX_LEVEL_AVC_32:
        return 32;
    case MFX_LEVEL_AVC_4:
        return 40;
    case MFX_LEVEL_AVC_41:
        return 41;
    case MFX_LEVEL_AVC_42:
        return 42;
    case MFX_LEVEL_AVC_5:
        return 50;
    case MFX_LEVEL_AVC_51:
        return 51;
    case MFX_LEVEL_AVC_52:
        return 52;
    }
    return 0;
}

mfxU16 ConvertUMCLevelToCUC( mfxU16 level )
{
    switch( level )
    {
    case 10:
        return MFX_LEVEL_AVC_1;
    case 9:
        return MFX_LEVEL_AVC_1b;
    case 11:
        return MFX_LEVEL_AVC_11;
    case 12:
        return MFX_LEVEL_AVC_12;
    case 13:
        return MFX_LEVEL_AVC_13;
    case 20:
        return MFX_LEVEL_AVC_2;
    case 21:
        return MFX_LEVEL_AVC_21;
    case 22:
        return MFX_LEVEL_AVC_22;
    case 30:
        return MFX_LEVEL_AVC_3;
    case 31:
        return MFX_LEVEL_AVC_31;
    case 32:
        return MFX_LEVEL_AVC_32;
    case 40:
        return MFX_LEVEL_AVC_4;
    case 41:
        return MFX_LEVEL_AVC_41;
    case 42:
        return MFX_LEVEL_AVC_42;
    case 50:
        return MFX_LEVEL_AVC_5;
    case 51:
        return MFX_LEVEL_AVC_51;
    case 52:
        return MFX_LEVEL_AVC_52;
    }
    return 0;
}

static Ipp64f h264enc_ConvertFramerate(mfxU32 FrameRateExtN, mfxU32 FrameRateExtD)
{
    return CalculateUMCFramerate(FrameRateExtN, FrameRateExtD);
}

// Fill mfxVideoParam with values from SPS/PPS headers
mfxStatus ConvertVideoParamFromSPSPPS_H264enc( mfxVideoInternalParam *parMFX, UMC::H264SeqParamSet *seq_parms, UMC::H264PicParamSet *pic_parms)
{
    mfxStatus st = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR3(parMFX, seq_parms, pic_parms);

    parMFX->mfx.FrameInfo.ChromaFormat = (mfxU16)seq_parms->chroma_format_idc;
    parMFX->mfx.FrameInfo.Width = (mfxU16)seq_parms->frame_width_in_mbs << 4;
    parMFX->mfx.FrameInfo.Height = (mfxU16)seq_parms->frame_height_in_mbs << 4;
    // assume chroma 420 for crops calculation
    if (seq_parms->frame_cropping_flag) {
        parMFX->mfx.FrameInfo.CropX = (mfxU16)seq_parms->frame_crop_left_offset * 2;
        parMFX->mfx.FrameInfo.CropY = (mfxU16)seq_parms->frame_crop_top_offset * 2 * (2 - seq_parms->frame_mbs_only_flag);
        parMFX->mfx.FrameInfo.CropW = (mfxU16)(parMFX->mfx.FrameInfo.Width - (seq_parms->frame_crop_left_offset + seq_parms->frame_crop_right_offset) * 2);
        parMFX->mfx.FrameInfo.CropH = (mfxU16)(parMFX->mfx.FrameInfo.Height - (seq_parms->frame_crop_top_offset + seq_parms->frame_crop_bottom_offset) * 2 * (2 - seq_parms->frame_mbs_only_flag));
    } else {
        parMFX->mfx.FrameInfo.CropX = parMFX->mfx.FrameInfo.CropY = parMFX->mfx.FrameInfo.CropW = parMFX->mfx.FrameInfo.CropH = 0;
    }
    if (seq_parms->vui_parameters.timing_info_present_flag) {
        parMFX->mfx.FrameInfo.FrameRateExtN = seq_parms->vui_parameters.time_scale >> (~seq_parms->vui_parameters.time_scale & 1);
        parMFX->mfx.FrameInfo.FrameRateExtD = seq_parms->vui_parameters.num_units_in_tick << (seq_parms->vui_parameters.time_scale & 1);
    }
    if (seq_parms->vui_parameters.aspect_ratio_info_present_flag) { // aspect_ratio_info_present_flag
        if(seq_parms->vui_parameters.aspect_ratio_idc == 255) {
            parMFX->mfx.FrameInfo.AspectRatioW = seq_parms->vui_parameters.sar_width;
            parMFX->mfx.FrameInfo.AspectRatioH = seq_parms->vui_parameters.sar_height;
        } else {
            // Limit valid range of the vui_parameters.aspect_ratio_idc to 1..16
            if(seq_parms->vui_parameters.aspect_ratio_idc==0 || seq_parms->vui_parameters.aspect_ratio_idc>=sizeof(tab_AspectRatio)/sizeof(tab_AspectRatio[0]) )
              return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            parMFX->mfx.FrameInfo.AspectRatioW = tab_AspectRatio[seq_parms->vui_parameters.aspect_ratio_idc][0];
            parMFX->mfx.FrameInfo.AspectRatioH = tab_AspectRatio[seq_parms->vui_parameters.aspect_ratio_idc][1];
        }
    }
    // FrameInfo.PicStruct, FrameInfo.ChromaFormat
    parMFX->mfx.FrameInfo.PicStruct = (mfxU16)(seq_parms->frame_mbs_only_flag ? MFX_PICSTRUCT_PROGRESSIVE : MFX_PICSTRUCT_UNKNOWN);
    // CodecId
    parMFX->mfx.CodecProfile = ConvertUMCProfileToCUC((mfxU16)seq_parms->profile_idc);
    parMFX->mfx.CodecLevel = ConvertUMCLevelToCUC((mfxU16)seq_parms->level_idc);
    // TargetUsage
    if (seq_parms->pic_order_cnt_type == 2) {
        if (parMFX->mfx.GopRefDist > 1)
            st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        parMFX->mfx.GopRefDist = 1;
    }
    // GopPicSize, GopRefDist, GopOptFlag, IdrInterval
    // RateControlMethod
    // InitialDelayInKB
    if (seq_parms->vui_parameters_present_flag && seq_parms->vui_parameters.nal_hrd_parameters_present_flag) {
        if (seq_parms->vui_parameters.hrd_params.cpb_cnt_minus1 + 1 > 0) {
            parMFX->mfx.RateControlMethod = (mfxU16)(seq_parms->vui_parameters.hrd_params.cbr_flag[0] ? MFX_RATECONTROL_CBR : MFX_RATECONTROL_VBR);
            parMFX->calcParam.BufferSizeInKB = (mfxU16)((((seq_parms->vui_parameters.hrd_params.cpb_size_value_minus1[0] + 1) <<
                (1 + seq_parms->vui_parameters.hrd_params.cpb_size_scale)) + 999) / 1000);
            // TargetKbps
            parMFX->calcParam.MaxKbps = (mfxU32)((((seq_parms->vui_parameters.hrd_params.bit_rate_value_minus1[0] + 1) <<
                (6 + seq_parms->vui_parameters.hrd_params.bit_rate_scale))+ 999) / 1000);
            if (parMFX->calcParam.TargetKbps > parMFX->calcParam.MaxKbps)
                parMFX->calcParam.TargetKbps = parMFX->calcParam.MaxKbps;
        }
    }
    // NumSlice
    parMFX->mfx.NumRefFrame = (mfxU16)seq_parms->num_ref_frames;

    mfxExtCodingOption* opts = (mfxExtCodingOption*)GetExtBuffer( parMFX->ExtParam, parMFX->NumExtParam, MFX_EXTBUFF_CODING_OPTION );

    if (opts) {
        if (seq_parms->vui_parameters.bitstream_restriction_flag)
            opts->MaxDecFrameBuffering = seq_parms->vui_parameters.max_dec_frame_buffering;
        else
            opts->MaxDecFrameBuffering = MFX_CODINGOPTION_UNKNOWN;
        opts->VuiNalHrdParameters = (mfxU16)(seq_parms->vui_parameters.nal_hrd_parameters_present_flag ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
        if (seq_parms->vui_parameters.pic_struct_present_flag == 0) {
            if (opts->PicTimingSEI == MFX_CODINGOPTION_ON)
                st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            opts->PicTimingSEI = MFX_CODINGOPTION_OFF;
        }
        if (opts->NalHrdConformance == MFX_CODINGOPTION_OFF && opts->VuiNalHrdParameters == MFX_CODINGOPTION_ON)
            opts->NalHrdConformance = MFX_CODINGOPTION_ON;
    }

    // now update MFX params
    if (opts) {
        opts->CAVLC = (mfxU16)(pic_parms->entropy_coding_mode ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON);
    }

    return st;
}

#define DEFAULT_B_FRAME_RATE              0
#define DEFAULT_B_REF                     1
#define DEFAULT_NUM_REF_FRAME             2
#define DEFAULT_NUM_SLICE                 3
#define DEFAULT_MV_SEARCH_METHOD          4
#define DEFAULT_MV_SUBPEL_SEARCH_METHOD   5
#define DEFAULT_SPLIT                     6
#define DEFAULT_ME_SEARCH_X               7
#define DEFAULT_ME_SEARCH_Y               8
#define DEFAULT_USE_CABAC                 9
#define DEFAULT_CABAC_INIT_IDC            10
#define DEFAULT_TRANSFORM_8x8             11
#define DEFAULT_TRELLIS                   12
#define DEFAULT_GOP_SIZE                  13
#define DEFAULT_IDR_INTERVAL              14
#define M_ANALYSE_LO_BIT                  15
#define M_ANALYSE_HO_BIT                  46



Ipp32s TargetUsageDefaults[47][7] = {
    //                              Quality       TU 2        TU 3      Balanced      TU 5        TU 6       Speed
    /*B_frame_rate*/                 { 1,          1,          1,          1,          0,          0,          0},
    /*treat_B_as_reference*/         { 1,          1,          0,          0,          0,          0,          0},
    /*num_ref_frames*/               { 3,          3,          2,          1,          1,          1,          1},
    /*num_slices*/                   { 0,          1,          1,          1,          1,          1,          1},
    /*mv_search_method*/             { 8, /*UMH*/  2, /*LOG*/  2, /*LOG*/  11,/*DIAMOND*/2, /*LOG*/  11,/*SQUARE*/9},
    /*mv_subpel_search_method*/      { 2,/*SQUARE*/2,/*SQUARE*/2,/*SQUARE*/4,/*DIAMOND*/2,/*SQUARE*/2,/*SQUARE*/4/*DIAMOND*/},
    /*me_split_mode*/                { 2,          2,          1,          1,          0,          0,          0},
    /*me_search_x*/                  { 32,         16,         12,         8,          8,          8,          12},
    /*me_search_y*/                  { 32,         16,         12,         8,          8,          2,          8},
    /*entropy_coding_mode*/          { 1,          1,          1,          1,          1,          0,          0},
    /*cabac_init_idc*/               { 2,          2,          1,          0,          0,          0,          0},
    /*transform_8x8_mode_flag*/      { 1,          1,          1,          1,          0,          1,          0},
    /*quant_opt_level*/              { 1,          1,          1,          0,          0,          0,          0},
    /*gop_size*/                     { 0x7fff,     0x7fff,     0x7fff,     0x7fff,     0x7fff,     0x7fff,     0},
    /*idr_interval*/                 { 0x7fff,     0x7fff,     0x7fff,     0x7fff,     0x7fff,     0x7fff,     0x7fff},
    /*ANALYSE_I_4x4*/                { 1,          1,          1,          1,          1,          0,          0},
    /*ANALYSE_I_8x8*/                { 1,          1,          1,          1,          0,          0,          0},
    /*ANALYSE_P_4x4*/                { 1,          1,          0,          0,          0,          0,          0},
    /*ANALYSE_P_8x8*/                { 1,          1,          1,          1,          0,          0,          0},
    /*ANALYSE_B_4x4*/                { 1,          1,          0,          1,          0,          0,          0},
    /*ANALYSE_B_8x8*/                { 1,          1,          1,          0,          0,          0,          0},
    /*ANALYSE_SAD*/                  { 0,          0,          0,          0,          1,          1,          1},
    /*ANALYSE_ME_EARLY_EXIT*/        { 0,          0,          0,          1,          0,          1,          1},
    /*ANALYSE_ME_ALL_REF*/           { 0,          0,          0,          0,          0,          0,          0},
    /*ANALYSE_ME_CHROMA*/            { 1,          1,          1,          0,          0,          0,          0},
    /*ANALYSE_ME_SUBPEL*/            { 1,          1,          1,          1,          1,          1,          0},
    /*ANALYSE_CBP_EMPTY*/            { 0,          0,          0,          1,          0,          1,          0},
    /*ANALYSE_RECODE_FRAME*/         { 1,          1,          1,          1,          1,          1,          1},
    /*ANALYSE_ME_AUTO_DIRECT*/       { 0,          0,          0,          0,          0,          0,          0},
    /*ANALYSE_FRAME_TYPE*/           { 1,          1,          1,          0,          0,          0,          0},
    /*ANALYSE_FLATNESS*/             { 0,          0,          0,          0,          1,          0,          0},
    /*ANALYSE_RD_MODE*/              { 1,          0,          0,          0,          0,          0,          0},
    /*ANALYSE_RD_OPT*/               { 1,          0,          0,          0,          0,          0,          0},
    /*ANALYSE_B_RD_OPT*/             { 1,          0,          0,          0,          0,          0,          0},
    /*ANALYSE_CHECK_SKIP_PREDICT*/   { 0,          0,          0,          0,          0,          0,          0},
    /*ANALYSE_CHECK_SKIP_INTPEL*/    { 1,          1,          1,          0,          1,          0,          1},
    /*ANALYSE_CHECK_SKIP_BESTCAND*/  { 1,          1,          1,          0,          1,          1,          1},
    /*ANALYSE_CHECK_SKIP_SUBPEL*/    { 1,          1,          1,          1,          1,          1,          0},
    /*ANALYSE_SPLIT_SMALL_RANGE*/    { 0,          0,          0,          0,          0,          0,          0},
    /*ANALYSE_ME_EXT_CANDIDATES*/    { 0,          0,          0,          1,          0,          0,          0},
    /*ANALYSE_ME_SUBPEL_SAD*/        { 1,          1,          1,          1,          1,          1,          1},
    /*ANALYSE_INTRA_IN_ME*/          { 0,          0,          0,          1,          1,          1,          0},
    /*ANALYSE_ME_FAST_MULTIREF*/     { 0,          0,          0,          0,          1,          0,          0},
    /*ANALYSE_FAST_INTRA*/           { 1,          1,          1,          0,          1,          0,          0},
    /*ANALYSE_ME_PRESEARCH*/         { 0,          0,          0,          0,          1,          0,          0},
    /*ANALYSE_ME_CONTINUED_SEARCH*/  { 0,          0,          0,          0,          0,          0,          0},
    /*ANALYSE_ME_BIDIR_REFINE*/      { 0,          0,          0,          0,          0,          0,          0}
};

mfxStatus ConvertVideoParam_H264enc( mfxVideoInternalParam *parMFX, UMC::H264EncoderParams *parUMC)
{
    Ipp64f ppkb;
    H264SeqParamSet &seq_parms = parUMC->m_SeqParamSet;
    H264PicParamSet &pic_parms = parUMC->m_PicParamSet;

    if (parMFX == NULL)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxExtCodingOption* opts = GetExtCodingOptions( parMFX->ExtParam, parMFX->NumExtParam );
    mfxExtCodingOptionSPSPPS* optsSP = (mfxExtCodingOptionSPSPPS*)GetExtBuffer( parMFX->ExtParam, parMFX->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS );
    mfxExtAvcTemporalLayers* tempLayers = (mfxExtAvcTemporalLayers*)GetExtBuffer( parMFX->ExtParam, parMFX->NumExtParam, MFX_EXTBUFF_AVC_TEMPORAL_LAYERS );
    mfxExtMVCSeqDesc *depinfo = (mfxExtMVCSeqDesc*)GetExtBuffer(parMFX->ExtParam, parMFX->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC );
    mfxExtSVCRateControl *svcinfo = (mfxExtSVCRateControl*)GetExtBuffer(parMFX->ExtParam, parMFX->NumExtParam, MFX_EXTBUFF_SVC_RATE_CONTROL );

    //Check bitdepth
    if ((parMFX->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420) && (optsSP == 0))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    // Here it is supposed that MFX params have no conflicts, including profile/level
    // Query() is used for that

    parUMC->info.framerate = h264enc_ConvertFramerate(parMFX->mfx.FrameInfo.FrameRateExtN, parMFX->mfx.FrameInfo.FrameRateExtD);

    // apply target usage first
    Ipp32s indexTU = parMFX->mfx.TargetUsage == MFX_TARGETUSAGE_UNKNOWN ? MFX_TARGETUSAGE_BALANCED - 1 : parMFX->mfx.TargetUsage - 1;
    if (indexTU < 0)
        indexTU = 0;    // fix for KW
    parUMC->B_frame_rate = TargetUsageDefaults[DEFAULT_B_FRAME_RATE][indexTU];
    parUMC->treat_B_as_reference =  TargetUsageDefaults[DEFAULT_B_REF][indexTU];
    parUMC->num_ref_frames = TargetUsageDefaults[DEFAULT_NUM_REF_FRAME][indexTU];
    parUMC->num_slices = (Ipp16s)TargetUsageDefaults[DEFAULT_NUM_SLICE][indexTU];
    parUMC->mv_search_method = TargetUsageDefaults[DEFAULT_MV_SEARCH_METHOD][indexTU];
    parUMC->mv_subpel_search_method = TargetUsageDefaults[DEFAULT_MV_SUBPEL_SEARCH_METHOD][indexTU];
    parUMC->me_split_mode = TargetUsageDefaults[DEFAULT_SPLIT][indexTU];
    parUMC->me_search_x = TargetUsageDefaults[DEFAULT_ME_SEARCH_X][indexTU];
    parUMC->me_search_y = TargetUsageDefaults[DEFAULT_ME_SEARCH_Y][indexTU];
    parUMC->entropy_coding_mode = (Ipp8s)TargetUsageDefaults[DEFAULT_USE_CABAC][indexTU];
    parUMC->cabac_init_idc = (Ipp8s)TargetUsageDefaults[DEFAULT_CABAC_INIT_IDC][indexTU];
    parUMC->transform_8x8_mode_flag = TargetUsageDefaults[DEFAULT_TRANSFORM_8x8][indexTU] ? true : false;
    parUMC->quant_opt_level = TargetUsageDefaults[DEFAULT_TRELLIS][indexTU];
    parUMC->key_frame_controls.interval = TargetUsageDefaults[DEFAULT_GOP_SIZE][indexTU];
    parUMC->key_frame_controls.idr_interval = TargetUsageDefaults[DEFAULT_IDR_INTERVAL][indexTU];

    //parUMC->treat_B_as_reference = 1;
    //parUMC->num_ref_frames = 5;

    for (Ipp32s i = M_ANALYSE_LO_BIT; i < M_ANALYSE_HO_BIT; i ++)
        parUMC->m_Analyse_on |= (TargetUsageDefaults[i][indexTU] << (i - M_ANALYSE_LO_BIT));

    if (parMFX->mfx.TargetUsage==MFX_TARGETUSAGE_BEST_SPEED && depinfo == NULL && svcinfo == NULL) {
        if (parMFX->mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            parUMC->m_Analyse_on |= ANALYSE_ME_PRESEARCH | ANALYSE_INTRA_IN_ME | ANALYSE_FAST_INTRA | ANALYSE_RECODE_FRAME | ANALYSE_ME_FAST_MULTIREF;
        else
        {
            if (!parMFX->calcParam.TargetKbps)
                return MFX_ERR_INVALID_VIDEO_PARAM;

            ppkb = ((Ipp64f)(parMFX->mfx.FrameInfo.Width * parMFX->mfx.FrameInfo.Height * parUMC->info.framerate)) / parMFX->calcParam.TargetKbps;
            parUMC->m_Analyse_ex = ANALYSE_SUPERFAST;

            if(ppkb >= 4500.0) {
                parUMC->m_Analyse_on |= ANALYSE_ME_SUBPEL | ANALYSE_CHECK_SKIP_SUBPEL;
                //parUMC->m_Analyse_on |= (ANALYSE_I_4x4 | ANALYSE_FAST_INTRA | ANALYSE_FLATNESS);
                //parUMC->m_Analyse_on &= ~(ANALYSE_ME_EARLY_EXIT);
            }
            if(ppkb >= 8500.0) {
                parUMC->m_Analyse_on |= ANALYSE_ME_PRESEARCH | ANALYSE_INTRA_IN_ME | ANALYSE_FAST_INTRA | ANALYSE_RECODE_FRAME | ANALYSE_ME_FAST_MULTIREF;
                parUMC->m_Analyse_ex &= ~(ANALYSE_SUPERFAST);
            }

            if(ppkb < 8500.0)
                parUMC->key_frame_controls.interval = 5;
            else if(ppkb < 10000.0)
                parUMC->key_frame_controls.interval = 150;
        }
    }

#ifdef USE_PSYINFO

    if (parMFX->mfx.TargetUsage!=MFX_TARGETUSAGE_BEST_SPEED) {
        // disabled while is not tuned
        if (parMFX->mfx.TargetUsage <= MFX_TARGETUSAGE_BALANCED)
            parUMC->m_Analyse_ex |= ANALYSE_PSY;
        else
            parUMC->m_Analyse_ex |= ANALYSE_PSY_STAT_MB | ANALYSE_PSY_STAT_FRAME | ANALYSE_PSY_INTERLACE;
    }

    // tuning mode:
    #define check_psy_option(mask, buffer, name) if(strstr(buf,#name)) {mask |= name;}
    FILE* cfg = fopen("psyconfig.txt", "rt");
    if (cfg) {
        int err, len = 0;
        err = fseek(cfg, 0, SEEK_END);
        if (!err) len = ftell(cfg);
        if (len) {
            char *buf = new char[len+1];
            buf[len] = 0;
            fseek(cfg, 0, SEEK_SET);
            fread(buf, 1, len, cfg);
            parUMC->m_Analyse_ex &= ~ANALYSE_PSY;
            check_psy_option(parUMC->m_Analyse_ex, buf, ANALYSE_PSY_STAT_MB    )
            check_psy_option(parUMC->m_Analyse_ex, buf, ANALYSE_PSY_STAT_FRAME )
            check_psy_option(parUMC->m_Analyse_ex, buf, ANALYSE_PSY_DYN_MB     )
            check_psy_option(parUMC->m_Analyse_ex, buf, ANALYSE_PSY_DYN_FRAME  )
            check_psy_option(parUMC->m_Analyse_ex, buf, ANALYSE_PSY_SCENECHANGE)
            check_psy_option(parUMC->m_Analyse_ex, buf, ANALYSE_PSY_INTERLACE  )
            check_psy_option(parUMC->m_Analyse_ex, buf, ANALYSE_PSY_TYPE_FR    )
            delete[] buf;
        }
        fclose(cfg);
    }
#endif // USE_PSYINFO

    if (parMFX->mfx.GopPicSize != 0)
        parUMC->key_frame_controls.interval = parMFX->mfx.GopPicSize;

    parUMC->m_Analyse_restrict = 0xffffffff;
    parUMC->numThreads = parMFX->mfx.NumThread;
    parUMC->m_pData = NULL;
    if (parMFX->mfx.GopPicSize == 0 || parMFX->mfx.GopRefDist == 0)
        parUMC->key_frame_controls.idr_interval = 0x7fff;
    else
        parUMC->key_frame_controls.idr_interval = parMFX->mfx.IdrInterval;

    if (parMFX->mfx.CodecProfile != MFX_PROFILE_UNKNOWN) {
        parUMC->profile_idc = (UMC::H264_PROFILE_IDC)ConvertCUCProfileToUMC( parMFX->mfx.CodecProfile );
        // correct target usage if it's features don't satisfy profile
        if (parUMC->profile_idc == H264_BASE_PROFILE)
        {
            parUMC->B_frame_rate = 0;
            parUMC->treat_B_as_reference = 0;
            parUMC->entropy_coding_mode = 0;
            parUMC->cabac_init_idc = 0;
            parUMC->quant_opt_level = 0;
            parUMC->transform_8x8_mode_flag = 0;
            if (parUMC->m_QualitySpeed > 1)
                parUMC->m_QualitySpeed = 1;
            //parUMC->coding_type = 0; //no effect
        }
        else if (parUMC->profile_idc == H264_MAIN_PROFILE)
        {
            parUMC->transform_8x8_mode_flag = 0;
        }
        else if (parUMC->profile_idc == static_cast<int>(MFX_PROFILE_AVC_SCALABLE_BASELINE))
        {
            //parUMC->transform_8x8_mode_flag = 0;
            parUMC->treat_B_as_reference = 0; // doesn't work in SVC
        }
        else if (parUMC->profile_idc == static_cast<int>(MFX_PROFILE_AVC_SCALABLE_HIGH))
        {
            parUMC->treat_B_as_reference = 0; // doesn't work in SVC
        }
        else if (parUMC->profile_idc == H264_HIGH_PROFILE &&
            parMFX->mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH)
        {
            parUMC->B_frame_rate = 0;
            parUMC->coding_type = 0;
        }
        else if (parUMC->profile_idc == H264_HIGH_PROFILE &&
            parMFX->mfx.CodecProfile == MFX_PROFILE_AVC_PROGRESSIVE_HIGH)
            parUMC->coding_type = 0;
    } else {
        parUMC->profile_idc = (UMC::H264_PROFILE_IDC)0;
    }

    // restrict default number of ref frames and number of B-frames with MaxDecFrameBuffering
    if (opts && opts->MaxDecFrameBuffering > 0) {
        if (opts->MaxDecFrameBuffering == 1)
            parUMC->B_frame_rate = 0;
        if (parUMC->num_ref_frames > opts->MaxDecFrameBuffering)
            parUMC->num_ref_frames = opts->MaxDecFrameBuffering;
    }

    parUMC->info.bitrate = ConvertBitrate_H264enc(parMFX->calcParam.TargetKbps);
    parUMC->info.clip_info.width = parMFX->mfx.FrameInfo.Width;
    parUMC->info.clip_info.height = parMFX->mfx.FrameInfo.Height;
    //parUMC->m_SuggestedInputSize;
    //parUMC->m_SuggestedOutputSize;
    //parUMC->numEncodedFrames;

    parUMC->frame_crop_x = parMFX->mfx.FrameInfo.CropX;
    parUMC->frame_crop_y = parMFX->mfx.FrameInfo.CropY;
    parUMC->frame_crop_w = parMFX->mfx.FrameInfo.CropW;
    parUMC->frame_crop_h = parMFX->mfx.FrameInfo.CropH;
    if( parMFX->mfx.GopOptFlag & MFX_GOP_CLOSED ){
        parUMC->use_reset_refpiclist_for_intra = true;
    } else
        parUMC->use_reset_refpiclist_for_intra = false;
    if( parMFX->mfx.GopOptFlag & MFX_GOP_STRICT ) {
        parUMC->m_Analyse_restrict |= ANALYSE_FRAME_TYPE;
    }
    parUMC->chroma_format_idc = 1; //ConvertColorFormat_H264enc(parMFX->mfx.FrameInfo.ChromaFormat);
    if (parMFX->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
        parUMC->coding_type = 0; // frames only
    else {
        if( opts && opts->FramePicture== MFX_CODINGOPTION_ON )
            parUMC->coding_type = 2; // MBAFF
        else
            parUMC->coding_type = 3; // PicAFF
    }
    if (parMFX->mfx.GopRefDist != 0)
        parUMC->B_frame_rate = parMFX->mfx.GopRefDist - 1;
    if (parMFX->mfx.NumRefFrame > 0)
        parUMC->num_ref_frames = parMFX->mfx.NumRefFrame;
    if (parMFX->mfx.GopRefDist > 1)
        parUMC->num_ref_frames = IPP_MAX(parUMC->num_ref_frames, 2);
    parUMC->num_ref_to_start_code_B_slice = 1;

    if (parUMC->profile_idc != static_cast<int>(MFX_PROFILE_AVC_SCALABLE_BASELINE) &&
        parUMC->profile_idc != static_cast<int>(MFX_PROFILE_AVC_SCALABLE_HIGH)) // doesn't work in SVC
    if (parMFX->mfx.GopRefDist > 3 && (parUMC->num_ref_frames > ((parMFX->mfx.GopRefDist - 1) / 2 + 1))) { // enable B-refs
        parUMC->treat_B_as_reference = 1;
    }

    if (parMFX->mfx.CodecLevel != MFX_LEVEL_UNKNOWN)
        parUMC->level_idc = (Ipp8s)ConvertCUCLevelToUMC( parMFX->mfx.CodecLevel );
    else
        parUMC->level_idc = 0; // autoselect
    //Set RC
    // set like in par-files
    if( parMFX->mfx.RateControlMethod == MFX_RATECONTROL_VBR )
        parUMC->rate_controls.method = (H264_Rate_Control_Method)H264_RCM_VBR;
    else if( parMFX->mfx.RateControlMethod == MFX_RATECONTROL_CQP ) {
        parUMC->rate_controls.method = (H264_Rate_Control_Method)H264_RCM_QUANT;
        parUMC->rate_controls.quantI = (Ipp8s)parMFX->mfx.QPI;
        parUMC->rate_controls.quantP = (Ipp8s)parMFX->mfx.QPP;
        parUMC->rate_controls.quantB = (Ipp8s)parMFX->mfx.QPB;
    } else if( parMFX->mfx.RateControlMethod == MFX_RATECONTROL_AVBR ) {
        parUMC->rate_controls.method      = (H264_Rate_Control_Method)H264_RCM_AVBR;
        parUMC->rate_controls.accuracy    = parMFX->mfx.Accuracy;
        parUMC->rate_controls.convergence = parMFX->mfx.Convergence;
    } else
        parUMC->rate_controls.method = (H264_Rate_Control_Method)H264_RCM_CBR;

    if (parMFX->mfx.NumSlice > 0)
        parUMC->num_slices = parMFX->mfx.NumSlice;
    parUMC->m_do_weak_forced_key_frames = false;
    parUMC->deblocking_filter_idc = 0; // 0-on // KL: can be disabled in SVCInit
    parUMC->deblocking_filter_alpha = 2;
    parUMC->deblocking_filter_beta = 2;

    if (parUMC->B_frame_rate)
        parUMC->num_ref_frames = MAX(parUMC->num_ref_frames, 2);

    if (opts) {
        if (opts->MVSearchWindow.x) parUMC->me_search_x = opts->MVSearchWindow.x;
        if (opts->MVSearchWindow.y) parUMC->me_search_y = opts->MVSearchWindow.y;

        switch(opts->IntraPredBlockSize){
            case MFX_BLOCKSIZE_MIN_16X16:
                parUMC->m_Analyse_on &= ~(ANALYSE_I_8x8 | ANALYSE_I_4x4);
                break;
            case MFX_BLOCKSIZE_MIN_8X8:
                if (parMFX->mfx.CodecProfile == MFX_PROFILE_UNKNOWN || parMFX->mfx.CodecProfile >= MFX_PROFILE_AVC_HIGH)
                {
                    parUMC->m_Analyse_on |= ANALYSE_I_8x8;
                    parUMC->transform_8x8_mode_flag = 1;
                }
                parUMC->m_Analyse_on &= ~ANALYSE_I_4x4;
                break;
            case MFX_BLOCKSIZE_MIN_4X4:
                if (parMFX->mfx.CodecProfile == MFX_PROFILE_UNKNOWN || parMFX->mfx.CodecProfile >= MFX_PROFILE_AVC_HIGH)
                {
                    parUMC->m_Analyse_on |= ANALYSE_I_8x8;
                    parUMC->transform_8x8_mode_flag = 1;
                }
                parUMC->m_Analyse_on |= ANALYSE_I_4x4;
                break;
            default:
                break;
        }

        switch( opts->InterPredBlockSize ){
            case MFX_BLOCKSIZE_MIN_16X16:
                parUMC->m_Analyse_on &= ~(ANALYSE_P_8x8 | ANALYSE_P_4x4 | ANALYSE_B_8x8 | ANALYSE_B_4x4);
                break;
            case MFX_BLOCKSIZE_MIN_8X8:
                parUMC->m_Analyse_on |= (ANALYSE_P_8x8 | ANALYSE_B_8x8);
                parUMC->m_Analyse_on &= ~(ANALYSE_P_4x4 | ANALYSE_B_4x4);
                break;
            case MFX_BLOCKSIZE_MIN_4X4:
                parUMC->m_Analyse_on |= ANALYSE_P_4x4 | ANALYSE_P_8x8 | ANALYSE_B_8x8 | ANALYSE_B_4x4;
                break;
            default:
                break;
        }

        if(opts) {
            if (opts->CAVLC == MFX_CODINGOPTION_ON) parUMC->entropy_coding_mode = 0;
            if (opts->CAVLC == MFX_CODINGOPTION_OFF) parUMC->entropy_coding_mode = 1;
        }

        if ( parUMC->entropy_coding_mode == 0)
            parUMC->quant_opt_level = 0;

        if( opts->RateDistortionOpt & MFX_CODINGOPTION_ON && parUMC->entropy_coding_mode){
            // RD optimizations exist only for CABAC in the current implementation
            parUMC->m_Analyse_on |= ANALYSE_RD_MODE | ANALYSE_RD_OPT | ANALYSE_B_RD_OPT | ANALYSE_ME_CHROMA;
        } else if( opts->RateDistortionOpt & MFX_CODINGOPTION_OFF ){
            parUMC->m_Analyse_on &= ~(ANALYSE_RD_MODE | ANALYSE_RD_OPT | ANALYSE_B_RD_OPT);
        }

        if( opts->MVPrecision == MFX_MVPRECISION_INTEGER ){
            parUMC->m_Analyse_on &= ~(ANALYSE_ME_SUBPEL);
        } else if (opts->MVPrecision != MFX_MVPRECISION_UNKNOWN){
            parUMC->m_Analyse_on |= ANALYSE_ME_SUBPEL ;
        }
    }

    parUMC->use_weighted_pred = 0;
    parUMC->use_weighted_bipred = 0;
    parUMC->use_implicit_weighted_bipred = 0;
    parUMC->direct_pred_mode = 1;
    parUMC->use_direct_inference = 1;

    parUMC->write_access_unit_delimiters = 0;
    parUMC->key_frame_controls.method = 1; // other fields are above
    parUMC->use_transform_for_intra_decision = true;

    parUMC->qpprime_y_zero_transform_bypass_flag = 0;
    parUMC->use_default_scaling_matrix = 0;
    parUMC->aux_format_idc = 0;
    //parUMC->alpha_incr_flag = 0;
    //parUMC->alpha_opaque_value = 0;
    //parUMC->alpha_transparent_value = 0;
    parUMC->bit_depth_aux = 8;
    parUMC->bit_depth_luma = 8;
    parUMC->bit_depth_chroma = 8;
    //parUMC->numFramesToEncode;


    //Now take params from SPS & PPS
    //Need more params here
    mfxStatus st = MFX_ERR_NONE;
    if(optsSP) {
        memset(&seq_parms, 0, sizeof(H264SeqParamSet));
        memset(&pic_parms, 0, sizeof(H264PicParamSet));
        st = LoadSPSPPS(parMFX, seq_parms, pic_parms);
        if(st == MFX_ERR_NONE) {
            if (optsSP->SPSBuffer) {
                if (seq_parms.vui_parameters.hrd_params.cpb_cnt_minus1 > 0) {
                    parUMC->info.bitrate = (seq_parms.vui_parameters.hrd_params.bit_rate_value_minus1[0] + 1) <<
                        (6 + seq_parms.vui_parameters.hrd_params.bit_rate_scale);
                }
                if (seq_parms.vui_parameters.timing_info_present_flag) {
                    parUMC->info.framerate = CalculateUMCFramerate(seq_parms.vui_parameters.time_scale, 2 * seq_parms.vui_parameters.num_units_in_tick);
                }
                parUMC->info.clip_info.width = seq_parms.frame_width_in_mbs * 16;
                parUMC->info.clip_info.height = seq_parms.frame_height_in_mbs * 16;
                // assume chroma 420 for crops calculation
                if (seq_parms.frame_cropping_flag) {
                    parUMC->frame_crop_x = seq_parms.frame_crop_left_offset * 2;
                    parUMC->frame_crop_y = seq_parms.frame_crop_top_offset * 2 * (2 - seq_parms.frame_mbs_only_flag);
                    parUMC->frame_crop_w = (parUMC->info.clip_info.width - (seq_parms.frame_crop_left_offset + seq_parms.frame_crop_right_offset) * 2);
                    parUMC->frame_crop_h = (parUMC->info.clip_info.height - (seq_parms.frame_crop_top_offset + seq_parms.frame_crop_bottom_offset) * 2 * (2 - seq_parms.frame_mbs_only_flag));
                } else {
                    parUMC->frame_crop_x = parUMC->frame_crop_y = parUMC->frame_crop_w = parUMC->frame_crop_h = 0;
                }
                if (seq_parms.vui_parameters.aspect_ratio_info_present_flag) { // aspect_ratio_info_present_flag
                    parUMC->aspect_ratio_idc = seq_parms.vui_parameters.aspect_ratio_idc;
                    if(seq_parms.vui_parameters.aspect_ratio_idc == 255) {
                        parUMC->info.aspect_ratio_width  = seq_parms.vui_parameters.sar_width;
                        parUMC->info.aspect_ratio_height = seq_parms.vui_parameters.sar_height;
                    } else {
                        // Limit valid range of the vui_parameters.aspect_ratio_idc to 1..16
                        if (seq_parms.vui_parameters.aspect_ratio_idc == 0 || seq_parms.vui_parameters.aspect_ratio_idc >= sizeof(tab_AspectRatio) / sizeof(tab_AspectRatio[0]) )
                            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

                        parUMC->info.aspect_ratio_width  = tab_AspectRatio[seq_parms.vui_parameters.aspect_ratio_idc][0];
                        parUMC->info.aspect_ratio_height = tab_AspectRatio[seq_parms.vui_parameters.aspect_ratio_idc][1];
                    }
                }
                parUMC->coding_type = seq_parms.frame_mbs_only_flag ? 0 : 3; // 0 = progressive, 3 = PicAFF
                if (seq_parms.pic_order_cnt_type == 2)
                    parUMC->B_frame_rate = 0;
                parUMC->num_ref_frames = seq_parms.num_ref_frames;
                parUMC->level_idc = seq_parms.level_idc;
                parUMC->profile_idc = seq_parms.profile_idc;
                parUMC->qpprime_y_zero_transform_bypass_flag = seq_parms.qpprime_y_zero_transform_bypass_flag;
                parUMC->use_direct_inference = !seq_parms.frame_mbs_only_flag || seq_parms.direct_8x8_inference_flag;
                parUMC->use_ext_sps = true;
            }
            if (optsSP->PPSBuffer) {
                parUMC->entropy_coding_mode = pic_parms.entropy_coding_mode;
                parUMC->transform_8x8_mode_flag = pic_parms.transform_8x8_mode_flag;
                parUMC->rate_controls.quantI =
                parUMC->rate_controls.quantP =
                parUMC->rate_controls.quantB = pic_parms.pic_init_qp;
                parUMC->use_ext_pps = true;
            }

            // some parameters could be set by default before parsing SPS/PPS
            // align them with profile
            if (parUMC->profile_idc == H264_BASE_PROFILE)
            {
                parUMC->B_frame_rate = 0;
                parUMC->treat_B_as_reference = 0;
                parUMC->entropy_coding_mode = 0;
                parUMC->cabac_init_idc = 0;
                parUMC->quant_opt_level = 0;
                parUMC->transform_8x8_mode_flag = 0;
                if (parUMC->m_QualitySpeed > 1)
                    parUMC->m_QualitySpeed = 1;
            }
            else if (parUMC->profile_idc == H264_MAIN_PROFILE)
            {
                parUMC->transform_8x8_mode_flag = 0;
            }

            // now fill mfxVideoParam with values from SPS/PPS header
            st = ConvertVideoParamFromSPSPPS_H264enc( parMFX, &seq_parms, &pic_parms);
        } else if (st == MFX_ERR_NOT_FOUND && tempLayers) {
            parUMC->m_ext_SPS_id = optsSP->SPSId;
            parUMC->m_ext_PPS_id = optsSP->PPSId;
            st = MFX_ERR_NONE;
        }
        else
            return st;
    } // end of load SPSPPS

    //Check sizes
    if( parUMC->info.clip_info.width & 0xf || parUMC->info.clip_info.height & 0xf )
        return MFX_ERR_INVALID_VIDEO_PARAM;
    if (parUMC->info.clip_info.width > 16384)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    if (parUMC->info.clip_info.height > 16384)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    if ((parUMC->frame_crop_x + parUMC->frame_crop_w) > parUMC->info.clip_info.width)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if ((parUMC->frame_crop_y + parUMC->frame_crop_h) > parUMC->info.clip_info.height)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    if (parUMC->num_slices > (parUMC->coding_type ? ((parUMC->info.clip_info.height + 31) >> 5) : ((parUMC->info.clip_info.height + 15) >> 4) ))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    return st;
}

//// Video Editing code
//// Initialize from packed SPS PPS fro Ext Buffer

mfxU32 CopyWOStartCodeAndPrevention(mfxU8* dst, const mfxU8* src, mfxU32 len)
{
    mfxU32 ii = 0, jj = 0;

    // skip NALU byte stream header
    if (src[0] == 0 && src[1] == 0) {
        if (src[2] == 1)
            ii = 3;
        else if (src[2] == 0 && src[3] == 1)
            ii = 4;
    }

    // skip NALU syntax header
    if ((src[ii] & 0x1f) == 0x07 || (src[ii] & 0x1f) == 0x08)
        ii++;

    // skip emulation prevention 0x03 byte
    for (; ii<len; ii++,jj++) {
        dst[jj] = src[ii];
        if (src[ii]==0 && ii>0 && src[ii-1]==0 && (ii+1)<len && src[ii+1]==3)
            ii++; // next 0x03 is skipped
    }
    return jj;
}

#define BS_DECLARE mfxU8* bptr = 0; mfxU32 bsize; mfxU32 bpos;
#define BS_ALLOC(size) {bptr = (mfxU8*)H264_Malloc(size); if (NULL == bptr) { return MFX_ERR_MEMORY_ALLOC;} }
#define BS_FREE_RETURN(sts) { if (bptr) H264_Free(bptr); return sts; }
#define BS_INIT(ptr,size) bptr=ptr; bsize=size; bpos=0;
#define BS_LOAD(src, size) bsize = CopyWOStartCodeAndPrevention(bptr, src, size); bpos = 0
#define BS_CHECK if (bpos > bsize*8) BS_FREE_RETURN(MFX_ERR_INVALID_VIDEO_PARAM)
#define BS_SKIP(n) bpos+=(n)
#define BS_BIT ((bptr[bpos>>3]>>(7-(bpos&7)))&1)
#define BS_BOOL (( bptr[bpos>>3] & (0x80>>(bpos&7)) )!=0)
#define BS_9BITS(n)  ( ( (mfxU32)(bptr[bpos>>3]<<(24+(bpos&7))) >> (32-(n)) ) | (bptr[(bpos>>3)+1] >> (16-(n)-(bpos&7)) ) )
#define BS_17BITS(n) ( ( (mfxU32)(((bptr[ bpos>>3   ]<<24) | \
    (bptr[(bpos>>3)+1]<<16) | \
    (bptr[(bpos>>3)+2]<<8 ) ) << (bpos&7)) >> (32-(n)) ) )
#define BS_25BITS(n) ( ( (mfxU32)(((bptr[ bpos>>3   ]<<24) | \
    (bptr[(bpos>>3)+1]<<16) | \
    (bptr[(bpos>>3)+2]<<8 ) | \
    (bptr[(bpos>>3)+3]    ) ) << (bpos&7)) >> (32-(n)) ) )

#define GetGolomb(dst)                    \
    if(BS_BIT) { dst=0; BS_SKIP(1); }     \
else {                                    \
    mfxU32 val, len;                      \
    BS_SKIP(1);                           \
    for(len=2; !BS_BIT; len++,BS_SKIP(1));\
    for(val=0;len>9;len-=9,BS_SKIP(9))    \
        val = (val<<9)|BS_9BITS(9);       \
    val = (val<<len)|BS_9BITS(len);       \
    BS_SKIP(len);                         \
    dst = val - 1;                        \
    BS_CHECK                              \
}

#define RESTORE_SIGN(in, out) {mfxI32 var = (in); out = (var&1)?((var+1)>>1):-(var>>1);}


mfxStatus LoadSPSPPS(const mfxVideoParam* in, H264SeqParamSet& seq_parms, H264PicParamSet& pic_parms)
{
    mfxExtCodingOptionSPSPPS* opt = 0;
    mfxExtCodingOption sExtOpt = {};
    mfxExtCodingOption* extopt;
    mfxU32 tmp, i;
    mfxI32 tmps;
    BS_DECLARE;

    opt = (mfxExtCodingOptionSPSPPS*)GetExtBuffer( in->ExtParam, in->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS );
    extopt = GetExtCodingOptions( in->ExtParam, in->NumExtParam );
    if (!extopt)
        extopt = &sExtOpt;
    memset(&seq_parms, 0, sizeof(seq_parms));
    memset(&pic_parms, 0, sizeof(pic_parms));

    if(opt == 0 || (!(opt->SPSBuffer && opt->SPSBufSize) && !(opt->PPSBuffer && opt->PPSBufSize)))
        BS_FREE_RETURN(MFX_ERR_NOT_FOUND)

    if (opt->SPSBuffer || opt->PPSBuffer)
        BS_ALLOC(MAX(opt->PPSBufSize, opt->SPSBufSize));

    // load Sequence Header
    if (opt->SPSBuffer) {

        BS_LOAD(opt->SPSBuffer, opt->SPSBufSize);

        seq_parms.profile_idc = (H264_PROFILE_IDC)BS_9BITS(8); BS_SKIP(8);
        if (seq_parms.profile_idc != 66 && seq_parms.profile_idc != 77 && seq_parms.profile_idc != 100)
            BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
        seq_parms.constraint_set0_flag = (Ipp8s)BS_BIT; BS_SKIP(1);
        seq_parms.constraint_set1_flag = (Ipp8s)BS_BIT; BS_SKIP(1);
        seq_parms.constraint_set2_flag = (Ipp8s)BS_BIT; BS_SKIP(1);
        seq_parms.constraint_set3_flag = (Ipp8s)BS_BIT; BS_SKIP(1 + 4);
        seq_parms.level_idc   = (Ipp8s)BS_9BITS(8); BS_SKIP(8);
        GetGolomb(tmp); seq_parms.seq_parameter_set_id = (Ipp8s)tmp;

        if (seq_parms.profile_idc == H264_HIGH_PROFILE ) { // don't support other
            GetGolomb(tmp); seq_parms.chroma_format_idc = (Ipp8s)tmp;
            if (seq_parms.chroma_format_idc != 1)
                BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
            if (!BS_BIT) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // bit_depth_luma
            BS_SKIP(1);
            seq_parms.bit_depth_luma = 8;
            if (!BS_BIT) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // bit_depth_chroma
            BS_SKIP(1);
            seq_parms.bit_depth_chroma = 8;
            if (BS_BIT) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // qpprime_y_zero_transform_bypass_flag
            BS_SKIP(1);
            seq_parms.qpprime_y_zero_transform_bypass_flag = 0;
            if (BS_BIT) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // seq_scaling_matrix_present_flag
            BS_SKIP(1);
            seq_parms.seq_scaling_matrix_present_flag = 0;
            // no seq_scaling_matrix
        } else if( seq_parms.profile_idc == H264_HIGH10_PROFILE ||
            seq_parms.profile_idc == H264_HIGH422_PROFILE ||
            seq_parms.profile_idc == H264_HIGH444_PROFILE )
            BS_FREE_RETURN(MFX_ERR_INVALID_VIDEO_PARAM) // unsupported
        else { // use default values for profiles other then High
            seq_parms.chroma_format_idc = 1;
        }

        GetGolomb(tmp); seq_parms.log2_max_frame_num = (Ipp8s)(tmp + 4);
        GetGolomb(tmp); seq_parms.pic_order_cnt_type = (Ipp8s)tmp;
        if (seq_parms.pic_order_cnt_type == 0) {
            GetGolomb(tmp); seq_parms.log2_max_pic_order_cnt_lsb = (Ipp32s)(tmp + 4); // TODO - derive smth
        } else if (seq_parms.pic_order_cnt_type != 2)
            BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // 1 unsupported, other - error
        GetGolomb(tmp); seq_parms.num_ref_frames = tmp;
        //if (BS_BIT) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // gaps_in_frame_num_value_allowed_flag
        seq_parms.gaps_in_frame_num_value_allowed_flag = (Ipp8s)BS_BIT; BS_SKIP(1);

        GetGolomb(tmp);  seq_parms.frame_width_in_mbs = tmp+1;
        GetGolomb(tmp);  seq_parms.frame_height_in_mbs = tmp+1; // PicHeightInMapUnits
        //out->mfx.FrameInfo.CropW =  out->mfx.FrameInfo.Width;
        //out->mfx.FrameInfo.CropH =  out->mfx.FrameInfo.Height;
        //out->mfx.FrameInfo.CropX = out->mfx.FrameInfo.CropY = 0;

        seq_parms.frame_mbs_only_flag = (Ipp8s)BS_BIT; BS_SKIP(1); // TODO - derive smth
        if (seq_parms.frame_mbs_only_flag == 0)
            seq_parms.frame_height_in_mbs <<= 1; // FrameHeightInMBs = (2 - frame+mbs_only_flag) * PicHeightInMapUnits
        //extopt->FramePicture = seq_parms.frame_mbs_only_flag;
        if(!seq_parms.frame_mbs_only_flag) {
            if (BS_BIT) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // mb_adaptive_frame_field_flag
            BS_SKIP(1);
            seq_parms.mb_adaptive_frame_field_flag = 0;
        }
        //if (BS_BIT) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // direct_8x8_inference_flag
        //BS_SKIP(1);
        seq_parms.direct_8x8_inference_flag = (Ipp8s)BS_BIT; BS_SKIP(1);

        seq_parms.frame_cropping_flag = (Ipp8s)BS_BIT; BS_SKIP(1);
        if (seq_parms.frame_cropping_flag) { // frame_cropping_flag
            GetGolomb(seq_parms.frame_crop_left_offset);
            GetGolomb(seq_parms.frame_crop_right_offset);
            GetGolomb(seq_parms.frame_crop_top_offset);
            GetGolomb(seq_parms.frame_crop_bottom_offset);
        }



        seq_parms.vui_parameters_present_flag = (Ipp8s)BS_BIT; BS_SKIP(1);
        if (seq_parms.vui_parameters_present_flag) { // vui_parameters_present_flag
            seq_parms.vui_parameters.aspect_ratio_info_present_flag = (Ipp8u)BS_BIT; BS_SKIP(1);
            if (seq_parms.vui_parameters.aspect_ratio_info_present_flag) { // aspect_ratio_info_present_flag
                seq_parms.vui_parameters.aspect_ratio_idc = (Ipp8u)BS_9BITS(8); BS_SKIP(8);
                if(seq_parms.vui_parameters.aspect_ratio_idc == 255) {
                    seq_parms.vui_parameters.sar_width = (Ipp16u)BS_17BITS(16); BS_SKIP(16);
                    seq_parms.vui_parameters.sar_height = (Ipp16u)BS_17BITS(16); BS_SKIP(16);
                } else {
                    // Limit valid range of the vui_parameters.aspect_ratio_idc to 1..16
                    if(seq_parms.vui_parameters.aspect_ratio_idc==0 || seq_parms.vui_parameters.aspect_ratio_idc>=sizeof(tab_AspectRatio)/sizeof(tab_AspectRatio[0]) )
                       BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
                }
            }
            seq_parms.vui_parameters.overscan_info_present_flag = (Ipp8u)BS_BIT; BS_SKIP(1);
            if (seq_parms.vui_parameters.overscan_info_present_flag) { // overscan_appropriate_flag
                seq_parms.vui_parameters.overscan_appropriate_flag = (Ipp8u)BS_BIT; BS_SKIP(1);
            }
            seq_parms.vui_parameters.video_signal_type_present_flag = (Ipp8u)BS_BIT; BS_SKIP(1);
            if (seq_parms.vui_parameters.video_signal_type_present_flag) { // video_signal_type_present_flag
                seq_parms.vui_parameters.video_format = (Ipp8u)BS_9BITS(3); BS_SKIP(3);
                seq_parms.vui_parameters.video_full_range_flag = (Ipp8u)BS_BIT; BS_SKIP(1);
                seq_parms.vui_parameters.colour_description_present_flag = (Ipp8u)BS_BIT; BS_SKIP(1);
                if (seq_parms.vui_parameters.video_signal_type_present_flag) { // video_signal_type_present_flag
                    seq_parms.vui_parameters.colour_primaries = (Ipp8u)BS_9BITS(8); BS_SKIP(8);
                    seq_parms.vui_parameters.transfer_characteristics = (Ipp8u)BS_9BITS(8); BS_SKIP(8);
                    seq_parms.vui_parameters.matrix_coefficients = (Ipp8u)BS_9BITS(8); BS_SKIP(8);
                }
            }

            seq_parms.vui_parameters.chroma_loc_info_present_flag = (Ipp8u)BS_BIT; BS_SKIP(1);
            if (seq_parms.vui_parameters.chroma_loc_info_present_flag) { // chroma_loc_info_present_flag
                GetGolomb(tmp);  seq_parms.vui_parameters.chroma_sample_loc_type_top_field = (Ipp8u)tmp;
                GetGolomb(tmp);  seq_parms.vui_parameters.chroma_sample_loc_type_bottom_field = (Ipp8u)tmp;
            }

            seq_parms.vui_parameters.timing_info_present_flag = (Ipp8u)BS_BIT; BS_SKIP(1);
            if (seq_parms.vui_parameters.timing_info_present_flag) { // timing_info_present_flag
                seq_parms.vui_parameters.num_units_in_tick = BS_17BITS(16)<<16; BS_SKIP(16);
                seq_parms.vui_parameters.num_units_in_tick |= BS_17BITS(16); BS_SKIP(16);
                seq_parms.vui_parameters.time_scale = BS_17BITS(16)<<16; BS_SKIP(16);
                seq_parms.vui_parameters.time_scale |= BS_17BITS(16); BS_SKIP(16);
                seq_parms.vui_parameters.fixed_frame_rate_flag = (Ipp8u)BS_BIT; BS_SKIP(1);
            }

            seq_parms.vui_parameters.nal_hrd_parameters_present_flag = (Ipp8u)BS_BIT; BS_SKIP(1);
            if (seq_parms.vui_parameters.nal_hrd_parameters_present_flag) { // nal_hrd_parameters_present_flag
                GetGolomb(tmp); seq_parms.vui_parameters.hrd_params.cpb_cnt_minus1 = (Ipp8u)tmp;
                if (seq_parms.vui_parameters.hrd_params.cpb_cnt_minus1 > 0) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // only single cpb
                seq_parms.vui_parameters.hrd_params.bit_rate_scale = (Ipp8u)BS_9BITS(4); BS_SKIP(4);
                seq_parms.vui_parameters.hrd_params.cpb_size_scale = (Ipp8u)BS_9BITS(4); BS_SKIP(4);
                for( i=0; i <= seq_parms.vui_parameters.hrd_params.cpb_cnt_minus1; i++ ){ //no loop - single cpb
                    GetGolomb(seq_parms.vui_parameters.hrd_params.bit_rate_value_minus1[i]);
                    GetGolomb(seq_parms.vui_parameters.hrd_params.cpb_size_value_minus1[i]);
                    if (seq_parms.vui_parameters.hrd_params.bit_rate_value_minus1[i] == 0 ||
                        seq_parms.vui_parameters.hrd_params.cpb_size_value_minus1[i] == 0)
                        BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
                    seq_parms.vui_parameters.hrd_params.cbr_flag[i] = (Ipp8u)BS_BIT; BS_SKIP(1);
                }
                seq_parms.vui_parameters.hrd_params.initial_cpb_removal_delay_length_minus1 = (Ipp8u)BS_9BITS(5); BS_SKIP(5);
                seq_parms.vui_parameters.hrd_params.cpb_removal_delay_length_minus1 = (Ipp8u)BS_9BITS(5); BS_SKIP(5);
                seq_parms.vui_parameters.hrd_params.dpb_output_delay_length_minus1 = (Ipp8u)BS_9BITS(5); BS_SKIP(5);
                seq_parms.vui_parameters.hrd_params.time_offset_length = (Ipp8u)BS_9BITS(5); BS_SKIP(5);
            }

            //if (BS_BIT) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // vcl_hrd_parameters_present_flag
            seq_parms.vui_parameters.vcl_hrd_parameters_present_flag = (Ipp8u)BS_BIT; BS_SKIP(1);
            if (seq_parms.vui_parameters.vcl_hrd_parameters_present_flag) { // nal_vcl_parameters_present_flag
                GetGolomb(tmp); seq_parms.vui_parameters.vcl_hrd_params.cpb_cnt_minus1 = (Ipp8u)tmp;
                if (seq_parms.vui_parameters.vcl_hrd_params.cpb_cnt_minus1 > 0) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // only single cpb
                seq_parms.vui_parameters.vcl_hrd_params.bit_rate_scale = (Ipp8u)BS_9BITS(4); BS_SKIP(4);
                seq_parms.vui_parameters.vcl_hrd_params.cpb_size_scale = (Ipp8u)BS_9BITS(4); BS_SKIP(4);
                for( i=0; i <= seq_parms.vui_parameters.vcl_hrd_params.cpb_cnt_minus1; i++ ){ //no loop - single cpb
                    GetGolomb(seq_parms.vui_parameters.vcl_hrd_params.bit_rate_value_minus1[i]);
                    GetGolomb(seq_parms.vui_parameters.vcl_hrd_params.cpb_size_value_minus1[i]);
                    if (seq_parms.vui_parameters.vcl_hrd_params.bit_rate_value_minus1[i] == 0 ||
                        seq_parms.vui_parameters.vcl_hrd_params.cpb_size_value_minus1[i] == 0)
                        BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
                    seq_parms.vui_parameters.vcl_hrd_params.cbr_flag[i] = (Ipp8u)BS_BIT; BS_SKIP(1);
                }
                seq_parms.vui_parameters.vcl_hrd_params.initial_cpb_removal_delay_length_minus1 = (Ipp8u)BS_9BITS(5); BS_SKIP(5);
                seq_parms.vui_parameters.vcl_hrd_params.cpb_removal_delay_length_minus1 = (Ipp8u)BS_9BITS(5); BS_SKIP(5);
                seq_parms.vui_parameters.vcl_hrd_params.dpb_output_delay_length_minus1 = (Ipp8u)BS_9BITS(5); BS_SKIP(5);
                seq_parms.vui_parameters.vcl_hrd_params.time_offset_length = (Ipp8u)BS_9BITS(5); BS_SKIP(5);
            }

            if (seq_parms.vui_parameters.nal_hrd_parameters_present_flag || seq_parms.vui_parameters.vcl_hrd_parameters_present_flag) {
                seq_parms.vui_parameters.low_delay_hrd_flag = (Ipp8u)BS_BIT; BS_SKIP(1); // low_delay_hrd_flag
            }
            seq_parms.vui_parameters.pic_struct_present_flag = (Ipp8u)BS_BIT; BS_SKIP(1);
            seq_parms.vui_parameters.bitstream_restriction_flag = (Ipp8u)BS_BIT; BS_SKIP(1);
            if (seq_parms.vui_parameters.bitstream_restriction_flag) { // skip all by now
                seq_parms.vui_parameters.motion_vectors_over_pic_boundaries_flag = (Ipp8u)BS_BIT; BS_SKIP(1);
                GetGolomb(seq_parms.vui_parameters.max_bytes_per_pic_denom);
                GetGolomb(seq_parms.vui_parameters.max_bits_per_mb_denom);
                GetGolomb(tmp); seq_parms.vui_parameters.log2_max_mv_length_horizontal = (Ipp8u)tmp;
                GetGolomb(tmp); seq_parms.vui_parameters.log2_max_mv_length_vertical = (Ipp8u)tmp;
                GetGolomb(tmp); seq_parms.vui_parameters.num_reorder_frames = (Ipp8u)tmp;
                GetGolomb(tmp); seq_parms.vui_parameters.max_dec_frame_buffering = (Ipp16u)tmp;
            }
        }
        BS_CHECK
    }

    // PPS now
    // load Picture Header
    if (opt->PPSBuffer) {

        BS_LOAD(opt->PPSBuffer, opt->PPSBufSize);

        GetGolomb(tmp); pic_parms.pic_parameter_set_id = (Ipp8s)tmp;
        GetGolomb(tmp); pic_parms.seq_parameter_set_id = (Ipp8s)tmp;
        //extopt->CAVLC = !BS_BIT; BS_SKIP(1);
        pic_parms.entropy_coding_mode = (Ipp8s)BS_BIT; BS_SKIP(1);
        pic_parms.pic_order_present_flag = (Ipp8s)BS_BIT; BS_SKIP(1);
        GetGolomb(tmp); pic_parms.num_slice_groups = (Ipp8s)(tmp + 1);
        if (tmp) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // num_slice_groups;

        GetGolomb(tmp); pic_parms.num_ref_idx_l0_active = (Ipp32s)(tmp + 1);
        GetGolomb(tmp); pic_parms.num_ref_idx_l1_active = (Ipp32s)(tmp + 1);

        pic_parms.weighted_pred_flag = (Ipp8s)BS_BIT; BS_SKIP(1);
        if (pic_parms.weighted_pred_flag) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // weighted_pred_flag;
        pic_parms.weighted_bipred_idc = (Ipp8s)BS_9BITS(2); BS_SKIP(2);
        if (pic_parms.weighted_bipred_idc != 0 && pic_parms.weighted_bipred_idc != 2) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // weighted_bipred_idc;

        GetGolomb(tmp); RESTORE_SIGN(tmp, tmps); pic_parms.pic_init_qp = (Ipp8s)(tmps+26); // pic_init_qp
        GetGolomb(tmp); RESTORE_SIGN(tmp, tmps); pic_parms.pic_init_qs = (Ipp8s)(tmps+26); // pic_init_qs
        GetGolomb(tmp); RESTORE_SIGN(tmp, tmps); pic_parms.chroma_qp_index_offset = (Ipp8s)tmps; // chroma_qp_index_offset

        pic_parms.deblocking_filter_variables_present_flag = (Ipp8s)BS_BIT; BS_SKIP(1);
        //if (!pic_parms.deblocking_filter_variables_present_flag) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // deblocking_filter_variables_present_flag
        pic_parms.constrained_intra_pred_flag = (Ipp8s)BS_BIT; BS_SKIP(1);
        if (pic_parms.constrained_intra_pred_flag) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // constrained_intra_pred_flag
        pic_parms.redundant_pic_cnt_present_flag = (Ipp8s)BS_BIT; BS_SKIP(1);
        if (pic_parms.redundant_pic_cnt_present_flag) BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // redundant_pic_cnt_present_flag

        if (seq_parms.profile_idc == H264_HIGH_PROFILE ) { // don't support other
            pic_parms.transform_8x8_mode_flag = BS_BOOL; BS_SKIP(1);
            pic_parms.pic_scaling_matrix_present_flag = BS_BOOL; BS_SKIP(1);
            if (pic_parms.pic_scaling_matrix_present_flag)
                BS_FREE_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM) // pic_scaling_matrix_present_flag
            GetGolomb(tmp); RESTORE_SIGN(tmp, tmps); pic_parms.second_chroma_qp_index_offset = (Ipp8s)tmps; // second_chroma_qp_index_offset
        }
        BS_CHECK
    }

    BS_FREE_RETURN(MFX_ERR_NONE)
}

mfxU16 GetTemporalLayer( mfxU32 currFrameNum, mfxU16 numLayers)
{
    mfxU16 layerId = numLayers - 1;
    mfxU32 count = currFrameNum;
    // if the frame number is divisable by 2^n, it is in layer nLayersCount-1-n
    for ( ; layerId > 0; layerId-- )
    {
        if ( count & 1 )
            break;
        count = count >> 1;
    }

    return layerId;
}

// get frame_num of closest reference from lower temporal layer
mfxU32 GetRefFrameNum( mfxU32 currFrameNum, mfxU16 numLayers)
{
    mfxU16 pow2;
    mfxU16 layerId = GetTemporalLayer( currFrameNum, numLayers);

    pow2 = 1 << (numLayers - 1 - layerId);
    return currFrameNum - pow2;
}

mfxU8 GetMinCR(mfxU32 level)
{
    return level >= 31 && level <= 42 ? 4 : 2; // AVCHD spec requires MinCR = 4 for levels  4.1, 4.2
}

// Sets only actual parameters
mfxStatus ConvertVideoParamBack_H264enc(mfxVideoInternalParam *parMFX, const UMC_H264_ENCODER::H264CoreEncoder_8u16s *enc)
{
    MFX_CHECK_COND(parMFX != NULL);
    MFX_CHECK_COND(enc != NULL);

    parMFX->Protected = 0;
    //parMFX->IOPattern; extParam - no way

    parMFX->mfx.CodecId = MFX_CODEC_AVC;
    parMFX->mfx.CodecProfile  = (mfxU16)ConvertUMCProfileToCUC( (mfxU16)enc->m_info.profile_idc );
    if (parMFX->mfx.CodecProfile == MFX_PROFILE_AVC_HIGH &&
        enc->m_info.m_ext_constraint_flags[4] == 1 && enc->m_info.m_ext_constraint_flags[5] == 1)
        parMFX->mfx.CodecProfile = MFX_PROFILE_AVC_CONSTRAINED_HIGH;
    else if (parMFX->mfx.CodecProfile == MFX_PROFILE_AVC_HIGH && enc->m_info.m_ext_constraint_flags[4] == 1)
        parMFX->mfx.CodecProfile = MFX_PROFILE_AVC_PROGRESSIVE_HIGH;
    else if (parMFX->mfx.CodecProfile == MFX_PROFILE_AVC_BASELINE && enc->m_info.m_ext_constraint_flags[1] == 1)
        parMFX->mfx.CodecProfile = MFX_PROFILE_AVC_CONSTRAINED_BASELINE;
    parMFX->mfx.CodecLevel = ConvertUMCLevelToCUC( enc->m_info.level_idc );
    parMFX->mfx.NumThread = (mfxU16)enc->m_info.numThreads;

    parMFX->mfx.GopPicSize = (mfxU16)enc->m_info.key_frame_controls.interval;
    parMFX->mfx.GopRefDist = (mfxU16)(enc->m_info.B_frame_rate + 1);
    parMFX->mfx.GopOptFlag |= (enc->m_info.use_reset_refpiclist_for_intra == true)?MFX_GOP_CLOSED:0;
    parMFX->mfx.IdrInterval = (mfxU16)enc->m_info.key_frame_controls.idr_interval;

    if( enc->m_info.rate_controls.method == H264_RCM_VBR ){
        parMFX->mfx.RateControlMethod = MFX_RATECONTROL_VBR;
    }else if(enc->m_info.rate_controls.method == H264_RCM_QUANT){
        parMFX->mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    }else if(enc->m_info.rate_controls.method == H264_RCM_AVBR){
        parMFX->mfx.RateControlMethod = MFX_RATECONTROL_AVBR;
    }else{
        parMFX->mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    }

    // for const QP return QPI, QPP, QPB
    if (enc->m_info.rate_controls.method == H264_RCM_QUANT) {
        // Assure that application will get enough size for bitstream buffer allocation
        parMFX->calcParam.BufferSizeInKB = (mfxU32)((enc->requiredBsBufferSize + 999)/1000);
        parMFX->GetCalcParams(parMFX);
        parMFX->mfx.QPI = enc->m_info.rate_controls.quantI;
        parMFX->mfx.QPP = enc->m_info.rate_controls.quantP;
        parMFX->mfx.QPB = enc->m_info.rate_controls.quantB;
    }
    else if (enc->m_info.rate_controls.method == H264_RCM_AVBR) {
        parMFX->mfx.Accuracy    = (mfxU16)enc->m_info.rate_controls.accuracy;
        parMFX->mfx.Convergence = (mfxU16)enc->m_info.rate_controls.convergence;

        assert(enc->brc);
        VideoBrcParams curBRCParams;
        enc->brc->GetParams(&curBRCParams);
        parMFX->calcParam.TargetKbps = (mfxU32)(curBRCParams.targetBitrate/1000);
        parMFX->calcParam.BufferSizeInKB = (enc->requiredBsBufferSize + 999) / 1000;

    } else { // not const QP
        VideoBrcParams  curBRCParams;
        if (enc->brc) { // take targetBitrate from BRC
            enc->brc->GetParams(&curBRCParams);
            parMFX->calcParam.TargetKbps = (mfxU32)(curBRCParams.targetBitrate/1000);
        }
        else // take targetBitrate from UMC encoder
            parMFX->calcParam.TargetKbps = (mfxU32)(enc->m_info.info.bitrate/1000);

        if (enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag == 1)  // take maxBitrate, bufferSize directly from SPS
        {
            parMFX->calcParam.BufferSizeInKB = (mfxU32)((((enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_value_minus1[0] + 1) <<
                (1 + enc->m_SeqParamSet->vui_parameters.hrd_params.cpb_size_scale)) + 999) / 1000);
            parMFX->calcParam.MaxKbps = (mfxU32)((((enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_value_minus1[0] + 1) <<
                (6 + enc->m_SeqParamSet->vui_parameters.hrd_params.bit_rate_scale))+ 999) / 1000);
            // take InitialDelayInKB from BRC
            if (enc->brc)
                parMFX->calcParam.InitialDelayInKB = (mfxU32)((curBRCParams.HRDInitialDelayBytes + 999) / 1000);
            else
                parMFX->calcParam.InitialDelayInKB = 0;
        }
        else if (enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag == 1)
        {
            parMFX->calcParam.BufferSizeInKB = (mfxU32)((((enc->m_SeqParamSet->vui_parameters.vcl_hrd_params.cpb_size_value_minus1[0] + 1) <<
                (1 + enc->m_SeqParamSet->vui_parameters.vcl_hrd_params.cpb_size_scale)) + 999) / 1000);
            parMFX->calcParam.MaxKbps = (mfxU32)((((enc->m_SeqParamSet->vui_parameters.vcl_hrd_params.bit_rate_value_minus1[0] + 1) <<
                (6 + enc->m_SeqParamSet->vui_parameters.vcl_hrd_params.bit_rate_scale))+ 999) / 1000);
            // take InitialDelayInKB from BRC
            if (enc->brc)
                parMFX->calcParam.InitialDelayInKB = (mfxU32)((curBRCParams.HRDInitialDelayBytes + 999) / 1000);
            else
                parMFX->calcParam.InitialDelayInKB = 0;
        }
        else if (enc->brc) // HRD parameters are initialized, return them
        {
            parMFX->calcParam.BufferSizeInKB = (mfxU32)((curBRCParams.HRDBufferSizeBytes + 999) / 1000);
            parMFX->calcParam.InitialDelayInKB = (mfxU32)((curBRCParams.HRDInitialDelayBytes + 999) / 1000);
            parMFX->calcParam.MaxKbps = (mfxU32)((curBRCParams.maxBitrate + 999) / 1000);;
        }
        else
        {
            parMFX->calcParam.BufferSizeInKB = 0;
            parMFX->calcParam.MaxKbps = 0;
            parMFX->calcParam.InitialDelayInKB = 0;
        }

        if (parMFX->calcParam.BufferSizeInKB == 0) {
            // return BufferSizeInKB depending on MinCR (table A-1)
            Ipp8u levelMultiplier = 4 / GetMinCR(parMFX->mfx.CodecLevel);
            Ipp64u minBufSizeKB = (3 * levelMultiplier * (parMFX->mfx.FrameInfo.Width * parMFX->mfx.FrameInfo.Height >> 3) + 999) / 1000; // width * height * 1.5 / MinCR
            parMFX->calcParam.BufferSizeInKB = (mfxU32)minBufSizeKB;
        }
    }

    if (enc->free_slice_mode_enabled)
        parMFX->mfx.NumSlice = 0;
    else
        parMFX->mfx.NumSlice = enc->m_info.num_slices;
    parMFX->mfx.NumRefFrame = (mfxU16)enc->m_info.num_ref_frames;
    // parMFX->mfx.EncodedOrder = 0; // no way


    parMFX->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    parMFX->mfx.FrameInfo.Width = (mfxU16)enc->m_info.info.clip_info.width;
    parMFX->mfx.FrameInfo.Height = (mfxU16)enc->m_info.info.clip_info.height;
    parMFX->mfx.FrameInfo.CropX = (mfxU16)enc->m_info.frame_crop_x;
    parMFX->mfx.FrameInfo.CropY = (mfxU16)enc->m_info.frame_crop_y;
    parMFX->mfx.FrameInfo.CropW = (mfxU16)enc->m_info.frame_crop_w;
    parMFX->mfx.FrameInfo.CropH = (mfxU16)enc->m_info.frame_crop_h;
    // take frame rate from timing_info if presented
    if (enc->m_SeqParamSet->vui_parameters_present_flag && enc->m_SeqParamSet->vui_parameters.timing_info_present_flag &&
        enc->m_SeqParamSet->vui_parameters.num_units_in_tick && enc->m_SeqParamSet->vui_parameters.time_scale) {
            parMFX->mfx.FrameInfo.FrameRateExtN = enc->m_SeqParamSet->vui_parameters.time_scale / 2;
            parMFX->mfx.FrameInfo.FrameRateExtD = enc->m_SeqParamSet->vui_parameters.num_units_in_tick;
    }
    else
        CalculateMFXFramerate(enc->m_info.info.framerate, &parMFX->mfx.FrameInfo.FrameRateExtN, &parMFX->mfx.FrameInfo.FrameRateExtD);
    if (enc->m_SeqParamSet->vui_parameters.aspect_ratio_info_present_flag) {
        if (enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc == 255) {
            parMFX->mfx.FrameInfo.AspectRatioW = enc->m_SeqParamSet->vui_parameters.sar_width;
            parMFX->mfx.FrameInfo.AspectRatioH = enc->m_SeqParamSet->vui_parameters.sar_height;
        } else {
            // Limit valid range of the vui_parameters.aspect_ratio_idc to 1..16
            if (enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc == 0 || enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc >= sizeof(tab_AspectRatio) / sizeof(tab_AspectRatio[0]))
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            parMFX->mfx.FrameInfo.AspectRatioW = tab_AspectRatio[enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc][0];
            parMFX->mfx.FrameInfo.AspectRatioH = tab_AspectRatio[enc->m_SeqParamSet->vui_parameters.aspect_ratio_idc][1];
        }
    }
    else
        parMFX->mfx.FrameInfo.AspectRatioW = parMFX->mfx.FrameInfo.AspectRatioH = 0;

    parMFX->mfx.FrameInfo.PicStruct = enc->m_FieldStruct;

    parMFX->mfx.FrameInfo.ChromaFormat = (mfxU16)enc->m_info.chroma_format_idc;

    mfxExtCodingOption* opts = GetExtCodingOptions( parMFX->ExtParam, parMFX->NumExtParam );

    if (opts != 0) {
        opts->RateDistortionOpt = (mfxU16)((enc->m_Analyse & (ANALYSE_RD_MODE | ANALYSE_RD_OPT)) == (ANALYSE_RD_MODE | ANALYSE_RD_OPT) ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
        opts->MVSearchWindow.x = (mfxI16)enc->m_info.me_search_x;
        opts->MVSearchWindow.y = (mfxI16)enc->m_info.me_search_y;
        // not_here: opts->EndOfSequence = enc->m_putEOSeq ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
        opts->FramePicture = (mfxU16)((enc->m_info.coding_type==3) ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON);

        opts->CAVLC = (mfxU16)((enc->m_info.entropy_coding_mode==0) ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
        //opts->RefPicListReordering = MFX_CODINGOPTION_UNKNOWN; // unused
        // not_here: opts->ResetRefList = enc->m_resetRefList ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
        mfxU32 analyseIntraPartitions = enc->m_Analyse & (ANALYSE_I_4x4 | ANALYSE_I_8x8);
        mfxU32 analyseInterPartitions = enc->m_Analyse & (ANALYSE_P_4x4 | ANALYSE_P_8x8 | ANALYSE_B_4x4 | ANALYSE_B_8x8);
        if (analyseIntraPartitions == (ANALYSE_I_4x4 | ANALYSE_I_8x8))
            opts->IntraPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
        else if (analyseIntraPartitions == ANALYSE_I_8x8)
            opts->IntraPredBlockSize = MFX_BLOCKSIZE_MIN_8X8;
        else if (analyseIntraPartitions == 0)
            opts->IntraPredBlockSize = MFX_BLOCKSIZE_MIN_16X16;
        else
            opts->IntraPredBlockSize = MFX_BLOCKSIZE_UNKNOWN;
        if (analyseInterPartitions == (ANALYSE_P_4x4 | ANALYSE_P_8x8 | ANALYSE_B_4x4 | ANALYSE_B_8x8))
            opts->InterPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;
        else if (analyseInterPartitions == (ANALYSE_P_8x8 | ANALYSE_B_8x8))
            opts->InterPredBlockSize = MFX_BLOCKSIZE_MIN_8X8;
        else if (analyseInterPartitions == 0)
            opts->InterPredBlockSize = MFX_BLOCKSIZE_MIN_16X16;
        else
            opts->InterPredBlockSize = MFX_BLOCKSIZE_UNKNOWN;
        /*if (enc->m_Analyse & ANALYSE_I_4x4) opts->IntraPredBlockSize = MFX_BLOCKSIZE_MIN_4X4; else
        if (enc->m_Analyse & ANALYSE_I_8x8) opts->IntraPredBlockSize = MFX_BLOCKSIZE_MIN_8X8; else
            opts->IntraPredBlockSize = MFX_BLOCKSIZE_MIN_16X16;
        if (enc->m_Analyse & (ANALYSE_P_4x4 | ANALYSE_B_4x4)) opts->InterPredBlockSize = MFX_BLOCKSIZE_MIN_4X4; else
        if (enc->m_Analyse & (ANALYSE_P_8x8 | ANALYSE_B_8x8)) opts->InterPredBlockSize = MFX_BLOCKSIZE_MIN_8X8; else
            opts->InterPredBlockSize = MFX_BLOCKSIZE_MIN_16X16;*/
        opts->MVPrecision = (mfxU16)((enc->m_Analyse & ANALYSE_ME_SUBPEL) ? MFX_MVPRECISION_QUARTERPEL : MFX_MVPRECISION_INTEGER);
        if (enc->m_SeqParamSet->vui_parameters.bitstream_restriction_flag)
            opts->MaxDecFrameBuffering = enc->m_SeqParamSet->vui_parameters.max_dec_frame_buffering;
        else
            opts->MaxDecFrameBuffering = MFX_CODINGOPTION_UNKNOWN;
        opts->AUDelimiter = (mfxU16)(enc->m_info.write_access_unit_delimiters ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
        // not_here: opts->EndOfStream = enc->m_putEOStream ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
        // not_here: opts->PicTimingSEI = enc->m_putTimingSEI ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
        opts->VuiNalHrdParameters = (mfxU16)(enc->m_SeqParamSet->vui_parameters.nal_hrd_parameters_present_flag ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
        opts->VuiVclHrdParameters = (mfxU16)(enc->m_SeqParamSet->vui_parameters.vcl_hrd_parameters_present_flag ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
    }

    mfxExtVideoSignalInfo* videoSignalInfo = (mfxExtVideoSignalInfo*)GetExtBuffer( parMFX->ExtParam, parMFX->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO );

    if (videoSignalInfo && enc->m_SeqParamSet->vui_parameters.video_signal_type_present_flag) {
        videoSignalInfo->VideoFormat = enc->m_SeqParamSet->vui_parameters.video_format;
        videoSignalInfo->VideoFullRange = enc->m_SeqParamSet->vui_parameters.video_full_range_flag;
        videoSignalInfo->ColourDescriptionPresent = enc->m_SeqParamSet->vui_parameters.colour_description_present_flag;
        if (enc->m_SeqParamSet->vui_parameters.colour_description_present_flag) {
            videoSignalInfo->ColourPrimaries = enc->m_SeqParamSet->vui_parameters.colour_primaries;
            videoSignalInfo->TransferCharacteristics = enc->m_SeqParamSet->vui_parameters.transfer_characteristics;
            videoSignalInfo->MatrixCoefficients = enc->m_SeqParamSet->vui_parameters.matrix_coefficients;
        }
    }

    if (enc->m_info.rate_controls.method != H264_RCM_QUANT)
        parMFX->GetCalcParams(parMFX);

    return MFX_ERR_NONE;
}

LimitsTableA1 LevelProfileLimitsNal = {
    {
        // BASE_PROFILE, MAIN_PROFILE, EXTENDED_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    76800,     210000,    64},
        /* 1b */ {    1485,    99,    152064,    153600,    420000,    64},
        /* 11 */ {    3000,    396,   345600,    230400,    600000,    128},
        /* 12 */ {    6000,    396,   912384,    460800,    1200000,   128},
        /* 13 */ {    11880,   396,   912384,    921600,    2400000,   128},
        /* 2  */ {    11880,   396,   912384,    2400000,   2400000,   128},
        /* 21 */ {    19800,   792,   1824768,   4800000,   4800000,   256},
        /* 22 */ {    20250,   1620,  3110400,   4800000,   4800000,   256},
        /* 3  */ {    40500,   1620,  3110400,   12000000,  12000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   16800000,  16800000,  512},
        /* 32 */ {    216000,  5120,  7864320,   24000000,  24000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  24000000,  30000000,  512},
        /* 41 */ {    245760,  8192,  12582912,  60000000,  75000000,  512},
        /* 42 */ {    522240,  8704,  13369344,  60000000,  75000000,  512},
        /* 5  */ {    589824,  22080, 42393600,  162000000, 162000000, 512},
        /* 51 */ {    983040,  36864, 70778880,  288000000, 288000000, 512},
        /* 52 */ {   2073600,  36864, 70778880,  288000000, 288000000, 512},
    },
    {
        // HIGH_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    96000,     262500,    64},
        /* 1b */ {    1485,    99,    152064,    192000,    525000,    64},
        /* 11 */ {    3000,    396,   345600,    288000,    750000,    128},
        /* 12 */ {    6000,    396,   912384,    576000,    1500000,   128},
        /* 13 */ {    11880,   396,   912384,    1152000,   3000000,   128},
        /* 2  */ {    11880,   396,   912384,    3000000,   3000000,   128},
        /* 21 */ {    19800,   792,   1824768,   6000000,   6000000,   256},
        /* 22 */ {    20250,   1620,  3110400,   6000000,   6000000,   256},
        /* 3  */ {    40500,   1620,  3110400,   15000000,  15000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   21000000,  21000000,  512},
        /* 32 */ {    216000,  5120,  7864320,   30000000,  30000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  30000000,  37500000,  512},
        /* 41 */ {    245760,  8192,  12582912,  75000000,  93750000,  512},
        /* 42 */ {    522240,  8704,  13369344,  75000000,  93750000,  512},
        /* 5  */ {    589824,  22080, 42393600,  202500000, 202500000, 512},
        /* 51 */ {    983040,  36864, 70778880,  360000000, 360000000, 512},
        /* 52 */ {   2073600,  36864, 70778880,  360000000, 360000000, 512},
    },
    {
        // HIGH10_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    230400,    630000,    64},
        /* 1b */ {    1485,    99,    152064,    460800,    1260000,   64},
        /* 11 */ {    3000,    396,   345600,    691200,    1800000,   128},
        /* 12 */ {    6000,    396,   912384,    1382400,   3600000,   128},
        /* 13 */ {    11880,   396,   912384,    2764800,   7200000,   128},
        /* 2  */ {    11880,   396,   912384,    7200000,   7200000,   128},
        /* 21 */ {    19800,   792,   1824768,   14400000,  14400000,  256},
        /* 22 */ {    20250,   1620,  3110400,   14400000,  14400000,  256},
        /* 3  */ {    40500,   1620,  3110400,   36000000,  36000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   50400000,  50400000,  512},
        /* 32 */ {    216000,  5120,  7864320,   72000000,  72000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  72000000,  90000000,  512},
        /* 41 */ {    245760,  8192,  12582912,  180000000, 225000000, 512},
        /* 42 */ {    522240,  8704,  13369344,  180000000, 225000000, 512},
        /* 5  */ {    589824,  22080, 42393600,  486000000, 486000000, 512},
        /* 51 */ {    983040,  36864, 70778880,  864000000, 864000000, 512},
        /* 52 */ {   2073600,  36864, 70778880,  864000000, 864000000, 512},
    },
    {
        // HIGH422_PROFILE, HIGH444_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    307200,    840000,    64},
        /* 1b */ {    1485,    99,    152064,    614400,    1680000,   64},
        /* 11 */ {    3000,    396,   345600,    921600,    2400000,   128},
        /* 12 */ {    6000,    396,   912384,    1843200,   4800000,   128},
        /* 13 */ {    11880,   396,   912384,    3686400,   9600000,   128},
        /* 2  */ {    11880,   396,   912384,    9600000,   9600000,   128},
        /* 21 */ {    19800,   792,   1824768,   19200000,  19200000,  256},
        /* 22 */ {    20250,   1620,  3110400,   19200000,  19200000,  256},
        /* 3  */ {    40500,   1620,  3110400,   48000000,  48000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   67200000,  67200000,  512},
        /* 32 */ {    216000,  5120,  7864320,   96000000,  96000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  96000000,  120000000, 512},
        /* 41 */ {    245760,  8192,  12582912,  240000000, 300000000, 512},
        /* 42 */ {    522240,  8704,  13369344,  240000000, 300000000, 512},
        /* 5  */ {    589824,  22080, 42393600,  648000000, 648000000, 512},
        /* 51 */ {    983040,  36864, 70778880,  1152000000,1152000000,512},
        /* 52 */ {   2073600,  36864, 70778880,  1152000000,1152000000,512},
        },
};

LimitsTableA1 LevelProfileLimitsVcl = {
    {
        // BASE_PROFILE, MAIN_PROFILE, EXTENDED_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    64000,     175000,    64},
        /* 1b */ {    1485,    99,    152064,    128000,    350000,    64},
        /* 11 */ {    3000,    396,   345600,    192000,    500000,    128},
        /* 12 */ {    6000,    396,   912384,    384000,    1000000,   128},
        /* 13 */ {    11880,   396,   912384,    768000,    2000000,   128},
        /* 2  */ {    11880,   396,   912384,    2000000,   2000000,   128},
        /* 21 */ {    19800,   792,   1824768,   4000000,   4000000,   256},
        /* 22 */ {    20250,   1620,  3110400,   4000000,   4000000,   256},
        /* 3  */ {    40500,   1620,  3110400,   10000000,  10000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   14000000,  14000000,  512},
        /* 32 */ {    216000,  5120,  7864320,   20000000,  20000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  20000000,  25000000,  512},
        /* 41 */ {    245760,  8192,  12582912,  50000000,  62500000,  512},
        /* 42 */ {    522240,  8704,  13369344,  50000000,  62500000,  512},
        /* 5  */ {    589824,  22080, 42393600,  135000000, 135000000, 512},
        /* 51 */ {    983040,  36864, 70778880,  240000000, 240000000, 512},
        /* 52 */ {   2073600,  36864, 70778880,  240000000, 240000000, 512},
    },
    {
        // HIGH_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    80000,     218750,    64},
        /* 1b */ {    1485,    99,    152064,    160000,    437500,    64},
        /* 11 */ {    3000,    396,   345600,    240000,    625000,    128},
        /* 12 */ {    6000,    396,   912384,    480000,    1250000,   128},
        /* 13 */ {    11880,   396,   912384,    960000,    2500000,   128},
        /* 2  */ {    11880,   396,   912384,    2500000,   2500000,   128},
        /* 21 */ {    19800,   792,   1824768,   5000000,   5000000,   256},
        /* 22 */ {    20250,   1620,  3110400,   5000000,   5000000,   256},
        /* 3  */ {    40500,   1620,  3110400,   12500000,  12500000,  256},
        /* 31 */ {    108000,  3600,  6912000,   17500000,  17500000,  512},
        /* 32 */ {    216000,  5120,  7864320,   25000000,  25000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  25000000,  31250000,  512},
        /* 41 */ {    245760,  8192,  12582912,  62500000,  78125000,  512},
        /* 42 */ {    522240,  8704,  13369344,  62500000,  78125000,  512},
        /* 5  */ {    589824,  22080, 42393600,  168750000, 168750000, 512},
        /* 51 */ {    983040,  36864, 70778880,  300000000, 300000000, 512},
        /* 52 */ {   2073600,  36864, 70778880,  300000000, 300000000, 512},
    },
    {
        // HIGH10_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    256000,    700000,    64},
        /* 1b */ {    1485,    99,    152064,    512000,    1400000,   64},
        /* 11 */ {    3000,    396,   345600,    768000,    2000000,   128},
        /* 12 */ {    6000,    396,   912384,    1536000,   4000000,   128},
        /* 13 */ {    11880,   396,   912384,    3072000,   8000000,   128},
        /* 2  */ {    11880,   396,   912384,    8000000,   8000000,   128},
        /* 21 */ {    19800,   792,   1824768,   16000000,  16000000,  256},
        /* 22 */ {    20250,   1620,  3110400,   16000000,  16000000,  256},
        /* 3  */ {    40500,   1620,  3110400,   40000000,  40000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   56000000,  56000000,  512},
        /* 32 */ {    216000,  5120,  7864320,   80000000,  80000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  80000000,  100000000, 512},
        /* 41 */ {    245760,  8192,  12582912,  200000000, 250000000, 512},
        /* 42 */ {    522240,  8704,  13369344,  200000000, 250000000, 512},
        /* 5  */ {    589824,  22080, 42393600,  540000000, 540000000, 512},
        /* 51 */ {    983040,  36864, 70778880,  960000000, 960000000, 512},
        /* 52 */ {   2073600,  36864, 70778880,  960000000, 960000000, 512},
    },
    {
        // HIGH422_PROFILE, HIGH444_PROFILE
                 // MaxMBPS  MaxFS    Max DPB    MaxBR      MaxCPB     MaxMvV
        /* 1  */ {    1485,    99,    152064,    307200,    840000,    64},
        /* 1b */ {    1485,    99,    152064,    614400,    1680000,   64},
        /* 11 */ {    3000,    396,   345600,    921600,    2400000,   128},
        /* 12 */ {    6000,    396,   912384,    1843200,   4800000,   128},
        /* 13 */ {    11880,   396,   912384,    3686400,   9600000,   128},
        /* 2  */ {    11880,   396,   912384,    9600000,   9600000,   128},
        /* 21 */ {    19800,   792,   1824768,   19200000,  19200000,  256},
        /* 22 */ {    20250,   1620,  3110400,   19200000,  19200000,  256},
        /* 3  */ {    40500,   1620,  3110400,   48000000,  48000000,  256},
        /* 31 */ {    108000,  3600,  6912000,   67200000,  67200000,  512},
        /* 32 */ {    216000,  5120,  7864320,   96000000,  96000000,  512},
        /* 4  */ {    245760,  8192,  12582912,  96000000,  120000000, 512},
        /* 41 */ {    245760,  8192,  12582912,  240000000, 300000000, 512},
        /* 42 */ {    522240,  8704,  13369344,  240000000, 300000000, 512},
        /* 5  */ {    589824,  22080, 42393600,  648000000, 648000000, 512},
        /* 51 */ {    983040,  36864, 70778880,  1152000000,1152000000,512},
        /* 52 */ {   2073600,  36864, 70778880,  1152000000,1152000000,512},
        },
};

mfxStatus CorrectProfileLevel_H264enc(mfxVideoInternalParam *parMFX, bool queryMode, mfxVideoInternalParam *parMFXSetByTU)
{
    MFX_CHECK_COND(parMFX != NULL);

    mfxStatus st = MFX_ERR_NONE;
    Ipp32s profileUMC;
    Ipp16u levelUMC;
    Ipp32s profile_ind, level_ind, profile_ind_init;
    Ipp64u targetBitrate = 0;
    Ipp64u bufSizeBits = 0;
    Ipp64u maxBitrate = 0;
    Ipp64u frameSizeInSamples = 3 * ((parMFX->mfx.FrameInfo.Width * parMFX->mfx.FrameInfo.Height) >> 1); // 4:2:0
    Ipp64u DPBSize = parMFX->mfx.NumRefFrame * frameSizeInSamples; // in bytes for 8 bit 4:2:0
    Ipp32s bitsPerFrame;
    Ipp64f frameRate;
    Ipp64u max_of_BR_CPB_bits;
    Ipp32u MB_per_frame, wMB, hMB;
    Ipp32u MB_per_sec;

    mfxExtCodingOption* opts = GetExtCodingOptions( parMFX->ExtParam, parMFX->NumExtParam );

    LimitsTableA1* pLevelProfileLimits = opts && (opts->VuiVclHrdParameters == MFX_CODINGOPTION_ON) ? &LevelProfileLimitsVcl : &LevelProfileLimitsNal;
    LimitsTableA1& LevelProfileLimits = *pLevelProfileLimits;

    frameRate = CalculateUMCFramerate(parMFX->mfx.FrameInfo.FrameRateExtN, parMFX->mfx.FrameInfo.FrameRateExtD);

    if (frameRate <= 0)
    {
        if (!queryMode)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else {
            if (parMFX->mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH &&
                (parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE || parMFX->mfx.GopRefDist > 1)) {
                parMFX->mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
                return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            } else if (parMFX->mfx.CodecProfile == MFX_PROFILE_AVC_PROGRESSIVE_HIGH &&
                parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) {
                parMFX->mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
                return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }
            return MFX_ERR_NONE;
        }
    }

    // no correction for target bitrate, max bitrate, CPB size for const QP
    if (parMFX->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        targetBitrate = parMFX->calcParam.TargetKbps * 1000;
        // HRD-conformance required
        if (!opts || (opts && opts->NalHrdConformance != MFX_CODINGOPTION_OFF)) {
            bufSizeBits = (parMFX->calcParam.BufferSizeInKB * 1000) << 3;
            maxBitrate = parMFX->calcParam.MaxKbps * 1000;
        }
        // HRD-conformance not required
        else {
            maxBitrate = parMFX->calcParam.MaxKbps = 0;
            bufSizeBits = 0;
        }
    }

    // Correct NumRefFrame if maximum for supported profiles exceeded
    if (DPBSize > LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_DPB]) {
        Ipp16u maxNumRefFrame = IPP_MIN(16, (Ipp16u)(LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_DPB] / frameSizeInSamples));
        DPBSize = maxNumRefFrame * frameSizeInSamples;
        parMFX->mfx.NumRefFrame = maxNumRefFrame;
    }

    // Correction for TargetBitrate, MaxBitrate and BufferSize if not const QP and HRD-conformance required
    if (parMFX->mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
        (!opts || (opts && opts->NalHrdConformance != MFX_CODINGOPTION_OFF))) {
        if (parMFX->mfx.RateControlMethod == MFX_RATECONTROL_CBR)
            maxBitrate = targetBitrate;

        // Correct TargetBitrate, MaxBitrate and BufferSize if maximum for supported profiles exceeded
        if (targetBitrate > LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR])
            targetBitrate = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR];
        if (parMFX->mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
            maxBitrate > LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR])
            maxBitrate = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR];
        if (bufSizeBits > LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_CPB])
            bufSizeBits = LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_CPB];

        // Correct BufferSize if it is less then size of one frame for bitrate
        if (parMFX->mfx.RateControlMethod == MFX_RATECONTROL_VBR && maxBitrate >= targetBitrate)
            bitsPerFrame = (Ipp32s)(maxBitrate / frameRate);
        else
            bitsPerFrame = (Ipp32s)(targetBitrate / frameRate);

        if (bufSizeBits > 0 && bufSizeBits < static_cast<Ipp64u>(bitsPerFrame << 1))
        {
            bufSizeBits = (bitsPerFrame << 1);
            // fix if specified too small (why not later?)
            parMFX->calcParam.BufferSizeInKB = (mfxU32)((bufSizeBits >> 3)/ 1000);
        }
    }

    profileUMC = ConvertCUCProfileToUMC(parMFX->mfx.CodecProfile);
    levelUMC = ConvertCUCLevelToUMC(parMFX->mfx.CodecLevel);

    profile_ind = ConvertProfileToTable(profileUMC);
    profile_ind_init = profile_ind;
    if (profile_ind < 0)
        profile_ind = H264_LIMIT_TABLE_BASE_PROFILE;

    level_ind = ConvertLevelToTable(levelUMC);
    if (level_ind < 0) {
        // if CodecLevel and BufferSizeInKB undefined then use level 4.1 for fast encoding with huge CPB (if not const QP and HRD-conformance required)
        if (!queryMode && parMFX->mfx.TargetUsage == MFX_TARGETUSAGE_BEST_SPEED && bufSizeBits == 0 &&
            parMFX->mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
            (!opts || (opts && opts->NalHrdConformance != MFX_CODINGOPTION_OFF)))
            level_ind = H264_LIMIT_TABLE_LEVEL_41;
        else
            level_ind = H264_LIMIT_TABLE_LEVEL_1;
    }

    // Adjusting of target bitrate, max bitrate, CPB size, profile (if not const QP and HRD-conformance required)
    if (parMFX->mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
        (!opts || (opts && opts->NalHrdConformance != MFX_CODINGOPTION_OFF))) {
        // If !queryMode set undefined of MaxBitrate, BufferSize to appropriate values for profile/level (by table A-1)
        // If queryMode just prepare profile/level for further correction
        if (!queryMode)
        {
            if (maxBitrate < targetBitrate)
                maxBitrate = 0;

            if (maxBitrate == 0 && bufSizeBits == 0) {
                maxBitrate = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR];
                bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];
            } else if (bufSizeBits == 0) {
                for (; profile_ind <= H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++) {
                    for (; level_ind <= H264_LIMIT_TABLE_LEVEL_MAX; level_ind++) {
                        if (maxBitrate <= LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR])
                            break;
                    }
                    if (level_ind <= H264_LIMIT_TABLE_LEVEL_MAX)
                        break;
                    level_ind = 0;
                }
                if (profile_ind > H264_LIMIT_TABLE_HIGH_PROFILE) {
                    profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
                    level_ind = H264_LIMIT_TABLE_LEVEL_MAX;
                }
                bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];
            } else if (maxBitrate == 0) {
                for (; profile_ind <= H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++) {
                    for (; level_ind <= H264_LIMIT_TABLE_LEVEL_MAX; level_ind++) {
                        if (bufSizeBits <= LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB])
                            break;
                    }
                    if (level_ind <= H264_LIMIT_TABLE_LEVEL_MAX)
                        break;
                    level_ind = 0;
                }
                if (profile_ind > H264_LIMIT_TABLE_HIGH_PROFILE) {
                    profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
                    level_ind = H264_LIMIT_TABLE_LEVEL_MAX;
                }
                maxBitrate = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR];
            }

            if (maxBitrate < targetBitrate) {
                maxBitrate = targetBitrate;
                for (; profile_ind <= H264_LIMIT_TABLE_HIGH_PROFILE; profile_ind++) {
                    for (; level_ind <= H264_LIMIT_TABLE_LEVEL_MAX; level_ind++) {
                        if (maxBitrate <= LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR])
                            break;
                    }
                    if (level_ind <= H264_LIMIT_TABLE_LEVEL_MAX)
                        break;
                    level_ind = 0;
                }
                if (profile_ind > H264_LIMIT_TABLE_HIGH_PROFILE) {
                    profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
                    level_ind = H264_LIMIT_TABLE_LEVEL_MAX;
                }
                if (!parMFX->calcParam.BufferSizeInKB) // correction of BufferSize if it wasn't specified by user
                    bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];
            }

            parMFX->calcParam.TargetKbps = (mfxU32)(targetBitrate / 1000);
            parMFX->calcParam.BufferSizeInKB = (mfxU32)((bufSizeBits >> 3)/ 1000);

            if (parMFX->mfx.RateControlMethod != MFX_RATECONTROL_AVBR)
                parMFX->calcParam.MaxKbps = (mfxU32)(maxBitrate / 1000);
        }
        else {
            if (maxBitrate < targetBitrate)
                maxBitrate = targetBitrate;
        }

        // Correct profile if TargetBitrate, MaxBitrate or BufferSize exceed maximum for current profile
        max_of_BR_CPB_bits = IPP_MAX(maxBitrate, bufSizeBits);

        if (max_of_BR_CPB_bits > LevelProfileLimits[H264_LIMIT_TABLE_BASE_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR] &&
            max_of_BR_CPB_bits <= LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR] &&
            profile_ind < H264_LIMIT_TABLE_HIGH_PROFILE)
            profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;
        else if (max_of_BR_CPB_bits > LevelProfileLimits[H264_LIMIT_TABLE_HIGH_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR] &&
            max_of_BR_CPB_bits <= LevelProfileLimits[H264_LIMIT_TABLE_HIGH10_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR] &&
            profile_ind < H264_LIMIT_TABLE_HIGH10_PROFILE)
            profile_ind = H264_LIMIT_TABLE_HIGH10_PROFILE;
        else if (max_of_BR_CPB_bits > LevelProfileLimits[H264_LIMIT_TABLE_HIGH10_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR] &&
            max_of_BR_CPB_bits <= LevelProfileLimits[H264_LIMIT_TABLE_HIGH422_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR] &&
            profile_ind < H264_LIMIT_TABLE_HIGH422_PROFILE)
            profile_ind = H264_LIMIT_TABLE_HIGH422_PROFILE;
        else if (max_of_BR_CPB_bits > LevelProfileLimits[H264_LIMIT_TABLE_HIGH444_PROFILE][H264_LIMIT_TABLE_LEVEL_MAX][H264_LIMIT_TABLE_MAX_BR])
            return(MFX_ERR_INVALID_VIDEO_PARAM);
    }

    // Calculate the Level Based on encoding parameters.
    wMB = (parMFX->mfx.FrameInfo.Width+15)>>4;
    hMB = (parMFX->mfx.FrameInfo.Height+15)>>4;
    MB_per_frame = wMB * hMB;
    MB_per_sec = (Ipp32u)(MB_per_frame * frameRate);

    // modify level in compliance with table A-1
    // process user-defined and stream-defined parameters
    while ( MB_per_frame > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_FS] ||
            wMB*wMB > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_FS] * 8 ||
            hMB*hMB > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_FS] * 8 ||
            MB_per_sec > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_MBPS] ||
            maxBitrate > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR] ||
            bufSizeBits > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB] ||
            (opts && static_cast<Ipp64u>(opts->MVSearchWindow.y) > 2*LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_MVV]) ||
            (!(parMFXSetByTU && parMFXSetByTU->mfx.NumRefFrame) && (DPBSize > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_DPB])))
    {
        if (level_ind == H264_LIMIT_TABLE_LEVEL_MAX) {
            st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            break;
        }
        level_ind++;
    }

    bool IgnorePicStructCheck = ( // is interlaced coding for level > 4.1 allowed (AVC standard violation)
            MB_per_frame > LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_41][H264_LIMIT_TABLE_MAX_FS]
         || wMB*wMB      > LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_41][H264_LIMIT_TABLE_MAX_FS] * 8  
         || hMB*hMB      > LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_41][H264_LIMIT_TABLE_MAX_FS] * 8 );

    // process TU-defined parameters (such parameters shouldn't affect user-defined parameters)
    if (parMFXSetByTU && parMFXSetByTU->mfx.NumRefFrame) {
        // if CodecLevel is user-defined then truncate NumRefFrame by limitation of current level
        if (levelUMC !=0 && DPBSize > LevelProfileLimits[0][level_ind][H264_LIMIT_TABLE_MAX_DPB]){
            Ipp16u maxNumRefFrame = IPP_MIN(16, (Ipp16u)(LevelProfileLimits[0][level_ind][H264_LIMIT_TABLE_MAX_DPB] / frameSizeInSamples));
            DPBSize = maxNumRefFrame * frameSizeInSamples;
            parMFX->mfx.NumRefFrame = maxNumRefFrame;
        // if user set "field encoding" then TU-defined NumRefFrame shouldn't increase level greater then 4.1
        } else if (levelUMC == 0 && DPBSize > LevelProfileLimits[0][H264_LIMIT_TABLE_LEVEL_41][H264_LIMIT_TABLE_MAX_DPB] && level_ind < H264_LIMIT_TABLE_LEVEL_41 &&
            parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE && (!opts || opts->FramePicture != MFX_CODINGOPTION_ON)) {
            Ipp16u maxNumRefFrame = IPP_MIN(16, (Ipp16u)(LevelProfileLimits[0][H264_LIMIT_TABLE_LEVEL_41][H264_LIMIT_TABLE_MAX_DPB] / frameSizeInSamples));
            DPBSize = maxNumRefFrame * frameSizeInSamples;
            parMFX->mfx.NumRefFrame = maxNumRefFrame;
        }

        while (DPBSize > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_DPB]){
            if (level_ind == H264_LIMIT_TABLE_LEVEL_MAX) {
                st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                break;
            }
            level_ind++;
        }
    }

    if (profile_ind_init != profile_ind)
        profileUMC = ConvertProfileFromTable(profile_ind);
    levelUMC = (Ipp16u)ConvertLevelFromTable(level_ind);

    if ((profileUMC == H264_BASE_PROFILE) && (parMFX->mfx.GopRefDist > 1))
        profileUMC = H264_MAIN_PROFILE;

    if (opts)
    {
        if (profileUMC == H264_BASE_PROFILE)
        {
            if( opts->CAVLC == MFX_CODINGOPTION_OFF )
                profileUMC = H264_MAIN_PROFILE;
            if( opts->FramePicture != MFX_CODINGOPTION_ON && parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
                profileUMC = H264_MAIN_PROFILE;
        }
    }
    else if (profileUMC == H264_BASE_PROFILE && parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        profileUMC = H264_MAIN_PROFILE;

    // PicStruct != MFX_PICSTRUCT_PROGRESSIVE means use of MBAFF or PicAFF encoding (frame_mbs_only_flag should be 0)
    if (parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE ) {
        if (levelUMC < MFX_LEVEL_AVC_21)
            levelUMC = MFX_LEVEL_AVC_21;
        else if (levelUMC >= MFX_LEVEL_AVC_42 && !IgnorePicStructCheck ) {
            parMFX->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    if (parMFX->mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH &&
        (parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE || parMFX->mfx.GopRefDist > 1))
        parMFX->mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
    else if (parMFX->mfx.CodecProfile == MFX_PROFILE_AVC_PROGRESSIVE_HIGH &&
        parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        parMFX->mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
    else if (!(parMFX->mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_BASELINE && profileUMC == H264_BASE_PROFILE) &&
        parMFX->mfx.CodecProfile != MFX_PROFILE_AVC_CONSTRAINED_HIGH && parMFX->mfx.CodecProfile != MFX_PROFILE_AVC_PROGRESSIVE_HIGH)
    parMFX->mfx.CodecProfile = (mfxU16)ConvertUMCProfileToCUC((mfxU16)profileUMC);
    parMFX->mfx.CodecLevel = ConvertUMCLevelToCUC(levelUMC);

    // set BufferSizeiInKB in query
    if (queryMode) {
        if (parMFX->mfx.RateControlMethod == MFX_RATECONTROL_CQP && parMFX->calcParam.BufferSizeInKB && parMFX->calcParam.BufferSizeInKB < (Ipp32u)(parMFX->mfx.FrameInfo.Width * parMFX->mfx.FrameInfo.Height * 4 + 999) / 1000)
            parMFX->calcParam.BufferSizeInKB = (parMFX->mfx.FrameInfo.Width * parMFX->mfx.FrameInfo.Height * 4 + 999) / 1000;
        else if (opts && opts->NalHrdConformance == MFX_CODINGOPTION_OFF && parMFX->calcParam.BufferSizeInKB) {
            // return BufferSizeInKB depending on MinCR (table A-1)
            Ipp8u levelMultiplier = 4 / GetMinCR(parMFX->mfx.CodecLevel);
            Ipp64u minBufSizeKB = (3 * levelMultiplier * (parMFX->mfx.FrameInfo.Width * parMFX->mfx.FrameInfo.Height >> 3) + 999) / 1000; // width * height * 1.5 / MinCR
            if (parMFX->calcParam.BufferSizeInKB < minBufSizeKB)
                parMFX->calcParam.BufferSizeInKB = (mfxU32)minBufSizeKB;
        }
    }
    // zero BufferSizeInKB in Init for const QP and encoding w/o HRD
    else if (parMFX->mfx.RateControlMethod == MFX_RATECONTROL_CQP || (opts && opts->NalHrdConformance == MFX_CODINGOPTION_OFF))
        parMFX->calcParam.BufferSizeInKB = 0;

    return st;
}

mfxStatus CheckProfileLevelLimits_H264enc(mfxVideoInternalParam *parMFX, bool queryMode, mfxVideoInternalParam *parMFXSetByTU)
{
    MFX_CHECK_COND(parMFX != NULL);

    Ipp32s profileUMC;
    Ipp16u levelUMC;
    Ipp32s profile_ind, level_ind;
    Ipp64u targetBitrate = 0;
    Ipp64u bufSizeBits = 0;
    Ipp64u maxBitrate = 0;
    Ipp64u frameSizeInSamples = 3 * ((parMFX->mfx.FrameInfo.Width * parMFX->mfx.FrameInfo.Height) >> 1); // 4:2:0
    Ipp64u DPBSize = parMFX->mfx.NumRefFrame * frameSizeInSamples; // in bytes for 8 bit 4:2:0
    Ipp32s bitsPerFrame;
    Ipp64f frameRate;
    Ipp32u MB_per_frame, wMB, hMB;
    Ipp32u MB_per_sec;
    mfxStatus st = MFX_ERR_NONE;

    mfxExtCodingOption* opts = GetExtCodingOptions( parMFX->ExtParam, parMFX->NumExtParam );

    LimitsTableA1* pLevelProfileLimits = opts && (opts->VuiVclHrdParameters == MFX_CODINGOPTION_ON) ? &LevelProfileLimitsVcl : &LevelProfileLimitsNal;
    LimitsTableA1& LevelProfileLimits = *pLevelProfileLimits;

    frameRate = CalculateUMCFramerate(parMFX->mfx.FrameInfo.FrameRateExtN, parMFX->mfx.FrameInfo.FrameRateExtD);

    {
        //allow to correct level, but not profile (if specified)
        mfxU16 profile_initial = parMFX->mfx.CodecProfile;
        mfxU16 nrfInitial = parMFX->mfx.NumRefFrame;
        mfxU16 nrfForPyramid = (parMFX->mfx.GopRefDist > 2) ? ((parMFX->mfx.GopRefDist - 1) / 2 + 2) : 0;

        if(parMFXSetByTU && parMFXSetByTU->mfx.NumRefFrame && nrfForPyramid > nrfInitial
            && parMFX->mfx.CodecProfile != MFX_PROFILE_AVC_SCALABLE_BASELINE
            && parMFX->mfx.CodecProfile != MFX_PROFILE_AVC_SCALABLE_HIGH){
            //try to enable B-pyramid
            parMFX->mfx.NumRefFrame = nrfForPyramid;
        }

        st = CorrectProfileLevel_H264enc(parMFX, queryMode, parMFXSetByTU);

        if (profile_initial) 
            parMFX->mfx.CodecProfile = profile_initial;

        if(parMFXSetByTU && parMFXSetByTU->mfx.NumRefFrame && parMFX->mfx.NumRefFrame < nrfForPyramid)
            parMFX->mfx.NumRefFrame = IPP_MIN(parMFX->mfx.NumRefFrame, nrfInitial);
    }

    if (frameRate <= 0) {
        if (!queryMode)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else {
            if (parMFX->mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH) {
                if (parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) {
                    parMFX->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
                    st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                }
                if (parMFX->mfx.GopRefDist > 1) {
                    parMFX->mfx.GopRefDist = 1;
                    st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                }
                return st;
            } else if (parMFX->mfx.CodecProfile == MFX_PROFILE_AVC_PROGRESSIVE_HIGH) {
                if (parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) {
                    parMFX->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
                    return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                }
            }
            return MFX_ERR_NONE;
        }
    }

    // no correction for target bitrate, max bitrate, CPB size for const QP
    if (parMFX->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        targetBitrate = parMFX->calcParam.TargetKbps * 1000;
        // HRD-conformance required
        if (!opts || (opts && opts->NalHrdConformance != MFX_CODINGOPTION_OFF)) {
            bufSizeBits = (parMFX->calcParam.BufferSizeInKB * 1000) << 3;
            maxBitrate = parMFX->calcParam.MaxKbps * 1000;
        }
        // HRD-conformance not required
        else {
            maxBitrate = parMFX->calcParam.MaxKbps = 0;
            bufSizeBits = 0;
        }
    }

    profileUMC = ConvertCUCProfileToUMC(parMFX->mfx.CodecProfile);
    levelUMC = ConvertCUCLevelToUMC(parMFX->mfx.CodecLevel);

    profile_ind = ConvertProfileToTable(profileUMC);
    if (profile_ind < 0) profile_ind = H264_LIMIT_TABLE_HIGH_PROFILE;

    level_ind = ConvertLevelToTable(levelUMC);
    if (level_ind < 0) level_ind = H264_LIMIT_TABLE_LEVEL_41;

     // Correct NumRefFrame
    if (DPBSize > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_DPB]) {
        Ipp16u maxNumRefFrame = IPP_MIN(16, (Ipp16u)(LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_DPB] / frameSizeInSamples));
        DPBSize = maxNumRefFrame * frameSizeInSamples;
        parMFX->mfx.NumRefFrame = maxNumRefFrame;
        st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    // Correction for TargetBitrate, MaxBitrate and BufferSize if not const QP and HRD-conformance required
    if (parMFX->mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
        (!opts || (opts && opts->NalHrdConformance != MFX_CODINGOPTION_OFF))) {
        if (parMFX->mfx.RateControlMethod == MFX_RATECONTROL_CBR)
            maxBitrate = targetBitrate;

        // Correct TargetBitrate, MaxBitrate and BufferSize if maximum for profile exceeded
        if (targetBitrate > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR])
            targetBitrate = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR];
        if (parMFX->mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
            maxBitrate > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR])
            maxBitrate = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR];
        if (bufSizeBits > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB])
            bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];

        // Correct BufferSize if it is less then size of one frame for bitrate
        if (parMFX->mfx.RateControlMethod == MFX_RATECONTROL_VBR && maxBitrate >= targetBitrate)
            bitsPerFrame = (Ipp32s)(maxBitrate / frameRate);
        else
            bitsPerFrame = (Ipp32s)(targetBitrate / frameRate);

        if (bufSizeBits > 0 && bufSizeBits < static_cast<Ipp64u>(bitsPerFrame << 1))
        {
            bufSizeBits = (bitsPerFrame << 1);
            parMFX->calcParam.BufferSizeInKB = (mfxU32)((bufSizeBits >> 3)/ 1000);
        }

        if (maxBitrate > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR])
            maxBitrate = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR];

        if (bufSizeBits > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB])
            bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];

        if (maxBitrate < targetBitrate)
            maxBitrate = 0;

        if (bufSizeBits == 0)
            bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];

        if (maxBitrate == 0)
            maxBitrate = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR];

        if (maxBitrate < targetBitrate) {
            maxBitrate = targetBitrate;
            if (!parMFX->calcParam.BufferSizeInKB) // correction of BufferSize if it wasn't specified by user
                bufSizeBits = LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB];
        }

        parMFX->calcParam.TargetKbps = (mfxU32)(targetBitrate / 1000);
        parMFX->calcParam.BufferSizeInKB = (mfxU32)((bufSizeBits >> 3)/ 1000);

        if (   parMFX->mfx.RateControlMethod == MFX_RATECONTROL_CBR 
            && parMFX->calcParam.InitialDelayInKB > parMFX->calcParam.BufferSizeInKB) {
            parMFX->calcParam.InitialDelayInKB = parMFX->calcParam.BufferSizeInKB/2;
        }

        if (parMFX->mfx.RateControlMethod != MFX_RATECONTROL_AVBR){
            parMFX->calcParam.MaxKbps = (mfxU32)(maxBitrate / 1000);
        }
    }

    if (opts && static_cast<Ipp64u>(opts->MVSearchWindow.y) > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_MVV]) {
        opts->MVSearchWindow.y = (mfxI16)LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_MVV];
        st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }else if (opts && opts->MVSearchWindow.y < -(mfxI16)LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_MVV]) {
        opts->MVSearchWindow.y = -(mfxI16)LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_MVV];
        st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    // Calculate the Level Based on encoding parameters.
    wMB = (parMFX->mfx.FrameInfo.Width+15)>>4;
    hMB = (parMFX->mfx.FrameInfo.Height+15)>>4;
    MB_per_frame = wMB * hMB;
    MB_per_sec = (Ipp32u)(MB_per_frame * frameRate);

    //correct framerate
    if(MB_per_sec > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_MBPS]){
        frameRate = (Ipp64f)LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_MBPS]/MB_per_frame;
        MB_per_sec = (Ipp32u)(MB_per_frame * frameRate);
        CalculateMFXFramerate(frameRate, &parMFX->mfx.FrameInfo.FrameRateExtN, &parMFX->mfx.FrameInfo.FrameRateExtD);
        st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    // check level in compliance with table A-1
    // process user-defined and stream-defined parameters
    if ( MB_per_frame > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_FS] ||
         wMB*wMB > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_FS] * 8 ||
         hMB*hMB > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_FS] * 8 ||
         MB_per_sec > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_MBPS] ||
         maxBitrate > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_BR] ||
         bufSizeBits > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_CPB] ||
         (!(parMFXSetByTU && parMFXSetByTU->mfx.NumRefFrame) && (DPBSize > LevelProfileLimits[profile_ind][level_ind][H264_LIMIT_TABLE_MAX_DPB])))
    {
        if(level_ind == H264_LIMIT_TABLE_LEVEL_MAX)
            st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        else
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    bool IgnorePicStructCheck = ( // is interlaced coding for level > 4.1 allowed (AVC standard violation)
            MB_per_frame > LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_41][H264_LIMIT_TABLE_MAX_FS]
         || wMB*wMB      > LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_41][H264_LIMIT_TABLE_MAX_FS] * 8  
         || hMB*hMB      > LevelProfileLimits[profile_ind][H264_LIMIT_TABLE_LEVEL_41][H264_LIMIT_TABLE_MAX_FS] * 8 );

    if ((profileUMC == H264_BASE_PROFILE) && (parMFX->mfx.GopRefDist > 1)) {
        parMFX->mfx.GopRefDist = 1;
        st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (opts)
    {
        if (profileUMC == H264_BASE_PROFILE)
        {
            if( opts->CAVLC == MFX_CODINGOPTION_OFF ){
                opts->CAVLC = MFX_CODINGOPTION_ON;
                st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }
            if( opts->FramePicture != MFX_CODINGOPTION_ON && parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE){
                parMFX->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
                st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }
        }
    }
    else if (profileUMC == H264_BASE_PROFILE && parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE){
        parMFX->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    // PicStruct != MFX_PICSTRUCT_PROGRESSIVE means use of MBAFF or PicAFF encoding (frame_mbs_only_flag should be 0)
    if (parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE ) {
        if (levelUMC < MFX_LEVEL_AVC_21 || levelUMC >= MFX_LEVEL_AVC_42) {
            if(!IgnorePicStructCheck)
                parMFX->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    if (parMFX->mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH) {
        if (parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) {
            parMFX->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
        if (parMFX->mfx.GopRefDist > 1) {
            parMFX->mfx.GopRefDist = 1;
            st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    } else if (parMFX->mfx.CodecProfile == MFX_PROFILE_AVC_PROGRESSIVE_HIGH) {
        if (parMFX->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) {
            parMFX->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            st = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    // set BufferSizeiInKB in query
    if (queryMode) {
        if (   parMFX->mfx.RateControlMethod == MFX_RATECONTROL_CQP 
            && parMFX->calcParam.BufferSizeInKB 
            && parMFX->calcParam.BufferSizeInKB < (Ipp32u)(parMFX->mfx.FrameInfo.Width * parMFX->mfx.FrameInfo.Height * 4 + 999) / 1000)
            parMFX->calcParam.BufferSizeInKB = (parMFX->mfx.FrameInfo.Width * parMFX->mfx.FrameInfo.Height * 4 + 999) / 1000;
        else if (opts && opts->NalHrdConformance == MFX_CODINGOPTION_OFF && parMFX->calcParam.BufferSizeInKB) {
            // return BufferSizeInKB depending on MinCR (table A-1)
            Ipp8u levelMultiplier = 4 / GetMinCR(parMFX->mfx.CodecLevel);
            Ipp64u minBufSizeKB = (3 * levelMultiplier * (parMFX->mfx.FrameInfo.Width * parMFX->mfx.FrameInfo.Height >> 3) + 999) / 1000; // width * height * 1.5 / MinCR
            if (parMFX->calcParam.BufferSizeInKB < minBufSizeKB)
                parMFX->calcParam.BufferSizeInKB = (mfxU32)minBufSizeKB;
        }
        if (   parMFX->mfx.RateControlMethod == MFX_RATECONTROL_CBR 
            && parMFX->calcParam.InitialDelayInKB > parMFX->calcParam.BufferSizeInKB) {
            parMFX->calcParam.InitialDelayInKB = parMFX->calcParam.BufferSizeInKB/2;
        }
    }
    // zero BufferSizeInKB in Init for const QP and encoding w/o HRD
    else if (parMFX->mfx.RateControlMethod == MFX_RATECONTROL_CQP || (opts && opts->NalHrdConformance == MFX_CODINGOPTION_OFF))
        parMFX->calcParam.BufferSizeInKB = 0;
        

    return st;
}

// place refs from previous GOP to RejectedRefList
// is separated code branch for fields required???
void RejectPrevRefsEncOrder(
    mfxExtAVCRefListCtrl *pRefPicListCtrl,
    EncoderRefPicListStruct_8u16s *pRefList,
    RefPicListInfo *pRefPicListInfo,
    Ipp8u listID,
    H264EncoderFrame_8u16s *curFrm)
{
    Ipp32s i = 0;
    Ipp32s j = 0;
    Ipp32s numRefsInList = (listID == 0) ? pRefPicListInfo->NumRefsInL0List : pRefPicListInfo->NumRefsInL1List;

    for (i = 0; i < numRefsInList; i++) {
        H264EncoderFrame_8u16s *fr = pRefList->m_RefPicList[i];
        if (IsRejected(fr->m_FrameTag, pRefPicListCtrl) || H264EncoderFrame_isLongTermRef0_8u16s(fr) ||
            fr->m_EncFrameCount >= curFrm->m_LastIframeEncCount)
            continue;
        while (pRefPicListCtrl->RejectedRefList[j].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN))
            j++;
        // reject ref frame from previous GOP
        pRefPicListCtrl->RejectedRefList[j++].FrameOrder = fr->m_FrameTag;

        // update pRefPicListInfo accordingly
        if (fr->m_FrameCount < curFrm->m_FrameCount)
            pRefPicListInfo->NumForwardSTRefs--;
        else if (fr->m_FrameCount > curFrm->m_FrameCount)
            pRefPicListInfo->NumBackwardSTRefs--;
    }
}

// place past references to RejectedRefList
// is separated code branch for fields required???
void RejectPastRefs(
    mfxExtAVCRefListCtrl *pRefPicListCtrl,
    EncoderRefPicListStruct_8u16s *pRefList,
    Ipp32s numRefsInList,
    H264EncoderFrame_8u16s *curFrm)
{
    Ipp32s i = 0;
    Ipp32s j = 0;

    for (i = 0; i < numRefsInList; i++) {
        H264EncoderFrame_8u16s *fr = pRefList->m_RefPicList[i];
        if (fr->m_FrameCount >= curFrm->m_FrameCount || 
            IsRejected(fr->m_FrameTag, pRefPicListCtrl) || IsPreferred(fr->m_FrameTag, pRefPicListCtrl) ||
            H264EncoderFrame_isLongTermRef0_8u16s(fr))
            continue;
        while (pRefPicListCtrl->RejectedRefList[j].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN))
            j++;
        pRefPicListCtrl->RejectedRefList[j++].FrameOrder = fr->m_FrameTag;
    }
}

// place future references to RejectedRefList
// is separated code branch for fields required???
void RejectFutureRefs(
    mfxExtAVCRefListCtrl *pRefPicListCtrl,
    EncoderRefPicListStruct_8u16s *pRefList,
    Ipp32s numRefsInList,
    H264EncoderFrame_8u16s *curFrm)
{
    Ipp32s i = 0;
    Ipp32s j = 0;

    for (i = 0; i < numRefsInList; i++) {
        H264EncoderFrame_8u16s *fr = pRefList->m_RefPicList[i];
        if (fr->m_FrameCount <= curFrm->m_FrameCount || 
            IsRejected(fr->m_FrameTag, pRefPicListCtrl) || IsPreferred(fr->m_FrameTag, pRefPicListCtrl) ||
            H264EncoderFrame_isLongTermRef0_8u16s(fr))
            continue;
        while (pRefPicListCtrl->RejectedRefList[j].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN))
            j++;
        pRefPicListCtrl->RejectedRefList[j++].FrameOrder = fr->m_FrameTag;
    }
}

// Get level-dependent max vertical MV component
mfxU16 GetMaxVmvR(mfxU16 CodecLevel)
{
    mfxI32 levelInTable = ConvertLevelToTable(CodecLevel);
    if (levelInTable != -1)
        return (mfxU16)LevelProfileLimitsNal[0][levelInTable][H264_LIMIT_TABLE_MAX_MVV];
    else return 0;
}

// clear PreferredRefList, RejectedRefList, LongTermRefList
void ClearRefListCtrl(mfxExtAVCRefListCtrl *pRefPicListCtrl)
{
    mfxU8 i;
    memset(pRefPicListCtrl, 0, sizeof(mfxExtAVCRefListCtrl));
    for (i = 0; i < 16; i ++)
    {
        pRefPicListCtrl->LongTermRefList[i].FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
        pRefPicListCtrl->RejectedRefList[i].FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
    }

    for (i = 0; i < 32; i ++)
        pRefPicListCtrl->PreferredRefList[i].FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
}

// set per-stream const QP or check per-frame const QP if it was already specified
Status SetConstQP(mfxU16 isEncodedOrder, UMC_H264_ENCODER::H264EncoderFrame_8u16s* currFrame, const UMC::H264EncoderParams *encParam)
{
    H264ENC_UNREFERENCED_PARAMETER(isEncodedOrder);
    if (currFrame->m_PQUANT[0] == 0) {
        for (Ipp8u i = 0; i < 2; i++) {
            if (currFrame->m_PicCodTypeFields[i] == BPREDPIC)
                currFrame->m_PQUANT[i] = encParam->rate_controls.quantB;
            else if (currFrame->m_PicCodTypeFields[i] == PREDPIC)
                currFrame->m_PQUANT[i] = encParam->rate_controls.quantP;
            else if (currFrame->m_PicCodTypeFields[i] == INTRAPIC)
                currFrame->m_PQUANT[i] = encParam->rate_controls.quantI;
            else
                return UMC_ERR_UNSUPPORTED;
        }
    }
    else if (currFrame->m_PQUANT[0] > 51 || currFrame->m_PQUANT[1] > 51)
        return UMC_ERR_UNSUPPORTED;

    return UMC_OK;
}

void SetUMCFrame(H264EncoderFrame_8u16s* umcFrame, mfxExtAvcRefFrameParam* cucFrame)
{
    umcFrame->m_PicOrderCnt[0] = cucFrame->PicInfo[0].FieldOrderCnt[0];
    umcFrame->m_PicOrderCnt[1] = cucFrame->PicInfo[0].FieldOrderCnt[1];
    umcFrame->m_bottom_field_flag[0] = 0;
    umcFrame->m_bottom_field_flag[1] = 1;

    umcFrame->m_PictureStructureForDec =
    umcFrame->m_PictureStructureForRef =
        cucFrame->FieldPicFlag
            ? FLD_STRUCTURE
            : cucFrame->MbaffFrameFlag
                ? AFRM_STRUCTURE
                : FRM_STRUCTURE;

    H264EncoderFrame_SetisShortTermRef_8u16s(umcFrame, 0);

    mfxU32 numField = cucFrame->FieldPicFlag ? 2 : 1;
    for (mfxU32 field = 0; field < numField; field++)
    {
        mfxU32 umcMbOffset = (field == 0) ? 0 : cucFrame->PicInfo[0].MbParam->NumMb;
        mfxU32 umcSliceOffset = (field == 0) ? 0 : cucFrame->PicInfo[0].NumSlice;
        mfxExtAvcRefPicInfo& refPicInfo = cucFrame->PicInfo[field];

        for (mfxU32 slice = 0; slice < refPicInfo.NumSlice; slice++)
        {
            mfxExtAvcRefSliceInfo& sliceInfo = refPicInfo.SliceInfo[slice];

            for(mfxU32 mb = sliceInfo.FirstMBInSlice; mb < sliceInfo.NumMbInSlice; mb++)
            {
                mfxMbCodeAVC& mfxMb = refPicInfo.MbParam->Mb[mb].AVC;
                mfxI16Pair* mfxMv = refPicInfo.MvData->Mv + mfxMb.MvDataOffset / sizeof(mfxI16Pair);

                H264MacroblockGlobalInfo& umcMb = umcFrame->m_mbinfo.mbs[mb + umcMbOffset];
                H264MotionVector* umcMvL0 = umcFrame->m_mbinfo.MV[0][mb + umcMbOffset].MotionVectors;
                H264MotionVector* umcMvL1 = umcFrame->m_mbinfo.MV[1][mb + umcMbOffset].MotionVectors;
                T_RefIdx* umcRefL0 = umcFrame->m_mbinfo.RefIdxs[0][mb + umcMbOffset].RefIdxs;
                T_RefIdx* umcRefL1 = umcFrame->m_mbinfo.RefIdxs[1][mb + umcMbOffset].RefIdxs;

                umcMb.mbtype = ConvertMBTypeToUMC(mfxMb);
                pSetMB8x8TSFlag(&umcMb, mfxMb.TransformFlag);
                pSetMB8x8TSPackFlag(&umcMb, mfxMb.TransformFlag);

                if (mfxMb.IntraMbFlag)
                {
                    memset(umcMvL0, 0, 16 * sizeof(*umcMvL0));
                    memset(umcMvL1, 0, 16 * sizeof(*umcMvL1));
                    memset(umcRefL0, -1, 16 * sizeof(*umcRefL0));
                    memset(umcRefL1, -1, 16 * sizeof(*umcRefL1));
                }
                else
                {
                    if (umcMb.mbtype == MBTYPE_B_8x8 ||
                        umcMb.mbtype == MBTYPE_DIRECT ||
                        umcMb.mbtype == MBTYPE_SKIPPED)
                    {
                        for (mfxU32 j = 0; j < 4; j++)
                        {
                            mfxU32 skip = (mfxMb.Skip8x8Flag >> j) & 0x1;

                            if (skip)
                            {
                                umcMb.sbtype[j] = SBTYPE_DIRECT;
                            }
                            else
                            {
                                mfxU32 shape = (mfxMb.SubMbShape >> (2 * j)) & 0x3;
                                mfxU32 pred = (mfxMb.SubMbPredMode >> (2 * j)) & 0x3;

                                if (pred > 2)
                                {
                                    assert(!"bad SubMbPredMode");
                                    return /*MFX_ERR_UNKNOWN*/;
                                }

                                umcMb.sbtype[j] = BSliceSubMbTypeMfx2Umc[shape][pred];
                            }
                        }
                    }
                    else if (umcMb.mbtype == MBTYPE_INTER_8x8 || umcMb.mbtype == MBTYPE_INTER_8x8_REF0)
                    {
                        for (mfxI32 j = 0; j < 4; j++)
                        {
                            umcMb.sbtype[j] = (mfxU8)((mfxMb.SubMbShape >> (2 * j)) & 0x3);
                        }
                    }
                    else
                    {
                        for (mfxI32 j = 0; j < 4; j++)
                        {
                            umcMb.sbtype[j] = NUMBER_OF_MBTYPES;
                        }
                    }

                    if (mfxMb.MvUnpackedFlag > 6)
                    {
                        assert(!"unsupported mvFormat > 6");
                    }

                    if (umcMb.mbtype >= NUMBER_OF_MBTYPES)
                    {
                        umcMb.mbtype = MBTYPE_B_8x8;    // set latest mb type
                    }
                    UnPackMVs(&umcMb, mfxMv, umcMvL0, umcMvL1);
                    UnPackRefIdxs(&umcMb, &mfxMb, umcRefL0, umcRefL1);
                }
            }

            EncoderRefPicListStruct_8u16s* umcRefInfoL0 =
                H264EncoderFrame_GetRefPicList_8u16s(umcFrame, slice + umcSliceOffset, 0);
            EncoderRefPicListStruct_8u16s* umcRefInfoL1 =
                H264EncoderFrame_GetRefPicList_8u16s(umcFrame, slice + umcSliceOffset, 1);

            mfxI32* poc0 = umcRefInfoL0->m_POC;
            mfxI32* poc1 = umcRefInfoL1->m_POC;
            Ipp8s* fld0 = umcRefInfoL0->m_Prediction;
            Ipp8s* fld1 = umcRefInfoL1->m_Prediction;

            memset(poc0, 0, (MAX_NUM_REF_FRAMES + 1) * sizeof(*poc0));
            memset(poc1, 0, (MAX_NUM_REF_FRAMES + 1) * sizeof(*poc1));
            memset(fld0, 0, (MAX_NUM_REF_FRAMES + 1) * sizeof(*fld0));
            memset(fld1, 0, (MAX_NUM_REF_FRAMES + 1) * sizeof(*fld1));

            for (mfxU32 ref = 0; ref < 32; ref++)
            {
                mfxU32 idx = sliceInfo.RefPicList[0][ref] & 0x7f;
                mfxI8 field_index = sliceInfo.RefPicList[0][ref] >> 7;
                if (idx < 16)
                {
                    poc0[ref] = refPicInfo.FieldOrderCntList[idx][static_cast<int>(field_index)];
                    fld0[ref] = field_index;
                }

                idx = sliceInfo.RefPicList[1][ref] & 0x7f;
                field_index = sliceInfo.RefPicList[1][ref] >> 7;
                if (idx < 16)
                {
                    poc1[ref] = refPicInfo.FieldOrderCntList[idx][static_cast<int>(field_index)];
                    fld1[ref] = field_index;
                }
            }
        }
    }

}

void InitSliceFromCUC( int slice, mfxFrameCUC *cuc, H264Slice_8u16s *curr_slice )
{
    mfxSliceParam* sp = &cuc->SliceParam[slice];
    mfxFrameParam* fp = cuc->FrameParam;

    //Set slice type
    switch( sp->AVC.SliceType & 0x0f ){
        case MFX_SLICETYPE_I:
            curr_slice->m_slice_type = INTRASLICE;
            break;
        case MFX_SLICETYPE_P:
            curr_slice->m_slice_type = PREDSLICE;
            break;
        case MFX_SLICETYPE_B:
            curr_slice->m_slice_type = BPREDSLICE;
            break;
    }

    curr_slice->m_is_cur_mb_field = fp->AVC.FieldPicFlag;
//TEMP    curr_slice->m_is_cur_mb_bottom_field = fp->AVC.BottomFieldFlag;

    curr_slice->m_first_mb_in_slice = sp->AVC.FirstMbX + sp->AVC.FirstMbY*(fp->AVC.FrameWinMbMinus1 + 1);
    curr_slice->num_ref_idx_l0_active = sp->AVC.NumRefIdxL0Minus1 + 1;
    curr_slice->num_ref_idx_l1_active = sp->AVC.NumRefIdxL1Minus1 + 1;
// Do we need this?
//    curr_slice->num_ref_idx_active_override_flag = (curr_slice->num_ref_idx_l0_active != m_PicParamSet->num_ref_idx_l0_active)
//            || (curr_slice->num_ref_idx_l1_active != m_PicParamSet->num_ref_idx_l1_active)
    curr_slice->m_cabac_init_idc = sp->AVC.CabacInitIdc;
    curr_slice->m_slice_qp_delta = sp->AVC.SliceQpDelta;
    curr_slice->m_disable_deblocking_filter_idc = sp->AVC.DeblockDisableIdc;
    curr_slice->m_slice_alpha_c0_offset = sp->AVC.DeblockAlphaC0OffsetDiv2;
    curr_slice->m_slice_beta_offset = sp->AVC.DeblockBetaOffsetDiv2;

    curr_slice->m_NumRefsInL0List = sp->AVC.NumRefIdxL0Minus1 + 1;
    curr_slice->m_NumRefsInL1List = sp->AVC.NumRefIdxL1Minus1 + 1;

    curr_slice->num_ref_idx_l0_active = MAX(curr_slice->m_NumRefsInL0List, 1);
    curr_slice->num_ref_idx_l1_active = MAX(curr_slice->m_NumRefsInL1List, 1);

    curr_slice->m_disable_deblocking_filter_idc = sp->AVC.DeblockDisableIdc;

}

void SetPlanePointers(mfxU32 fourCC, const mfxFrameData& mfxFrame, H264EncoderFrame_8u16s& umcFrame)
{
    switch (fourCC)
    {
        case MFX_FOURCC_YV12:
            umcFrame.m_data.SetPlanePointer(mfxFrame.Y, 0);
            umcFrame.m_data.SetPlanePitch(mfxFrame.Pitch, 0);
            umcFrame.m_data.SetPlanePointer(mfxFrame.Cb, 1);
            umcFrame.m_data.SetPlanePitch(mfxFrame.Pitch >> 1, 1);
            umcFrame.m_data.SetPlanePointer(mfxFrame.Cr, 2);
            umcFrame.m_data.SetPlanePitch(mfxFrame.Pitch >> 1, 2);
            umcFrame.m_pitchPixels = mfxFrame.Pitch;
            umcFrame.m_pYPlane = mfxFrame.Y;
            umcFrame.m_pUPlane = mfxFrame.Cb;
            umcFrame.m_pVPlane = mfxFrame.Cr;
            break;
        case MFX_FOURCC_NV12:
            umcFrame.m_data.SetPlanePointer(mfxFrame.Y, 0);
            umcFrame.m_data.SetPlanePitch(mfxFrame.Pitch, 0);
            umcFrame.m_data.SetPlanePointer(mfxFrame.UV, 1);
            umcFrame.m_data.SetPlanePitch(mfxFrame.Pitch, 1);
            umcFrame.m_pitchPixels = mfxFrame.Pitch;
            umcFrame.m_pYPlane = mfxFrame.Y;
            umcFrame.m_pUPlane = mfxFrame.UV;
            umcFrame.m_pVPlane = mfxFrame.UV + 1;
            break;
        default:
            assert(!suppress_icl_warning("unsupported fourcc"));
    }
}

#define PACK_FOR(param) \
    pMV->x = mvL0[param].mvx; \
    pMV->y = mvL0[param].mvy; \
    pMV++;

#define PACK_BACK(param)\
    pMV->x = mvL1[param].mvx; \
    pMV->y = mvL1[param].mvy; \
    pMV++;

#define PACK_BIDIR(param)\
    PACK_FOR(param); \
    PACK_BACK(param);

#define UNPACK_FOR(param, w, h)\
    for(mfxI32 i=0;i<h;i++) \
    for(mfxI32 j=0;j<w;j++) { \
      mvL0[param+4*i+j].mvx = pMV->x;\
      mvL0[param+4*i+j].mvy = pMV->y;\
    }\
    pMV++;
#define UNPACK_BACK(param, w, h)\
    for(mfxI32 i=0;i<h;i++) \
    for(mfxI32 j=0;j<w;j++) { \
      mvL1[param+4*i+j].mvx = pMV->x;\
      mvL1[param+4*i+j].mvy = pMV->y;\
    } \
    pMV++;
#define UNPACK_BIDIR(param, w, h)\
    UNPACK_FOR(param, w, h)\
    UNPACK_BACK(param, w, h)


enum MBtype{
NOTYPE,
FORWARD,
BACKWARD,
BIDIR,
INTER_16x8,
INTER_8x16,
INTER_8x8,
INTER_8x8_REF0
};

enum DirSB{
    NODIR = 0,
    DIR_FWD = 1,
    DIR_BWD = 2,
    DIR_BIDIR = 3
};

typedef struct _MBinfo {
    MBtype type;
    DirSB  sbdir[2];
} MBinfo;

const MBinfo infoMBType[] = {
    { NOTYPE,  {NODIR,NODIR}},
    { NOTYPE,  {NODIR,NODIR}},
    { NOTYPE,  {NODIR,NODIR}},
    { FORWARD, {DIR_FWD,NODIR}},
    { INTER_16x8, {DIR_FWD,DIR_FWD}},
    { INTER_8x16, {DIR_FWD,DIR_FWD}},
    { INTER_8x8, {NODIR,NODIR}},
    { INTER_8x8_REF0, {NODIR,NODIR}},
    { FORWARD, {DIR_FWD,NODIR}},
    { BACKWARD, {DIR_BWD,NODIR}},
    { FORWARD, {NODIR,NODIR}},//SKIPPED only for P frame, for B should be changed to DIRECT
    { INTER_8x8, {NODIR,NODIR}},//DIRECT
    { BIDIR, {DIR_BIDIR,NODIR}},
    { INTER_16x8,{DIR_FWD,DIR_FWD}},
    { INTER_8x16,{DIR_FWD,DIR_FWD}},
    { INTER_16x8,{DIR_BWD,DIR_BWD}},
    { INTER_8x16,{DIR_BWD,DIR_BWD}},
    { INTER_16x8,{DIR_FWD,DIR_BWD}},
    { INTER_8x16,{DIR_FWD,DIR_BWD}},
    { INTER_16x8,{DIR_BWD,DIR_FWD}},
    { INTER_8x16,{DIR_BWD,DIR_FWD}},
    { INTER_16x8,{DIR_BIDIR,DIR_FWD}},
    { INTER_8x16,{DIR_BIDIR,DIR_FWD}},
    { INTER_16x8,{DIR_BIDIR,DIR_BWD}},
    { INTER_8x16,{DIR_BIDIR,DIR_BWD}},
    { INTER_16x8,{DIR_FWD,DIR_BIDIR}},
    { INTER_8x16,{DIR_FWD,DIR_BIDIR}},
    { INTER_16x8,{DIR_BWD,DIR_BIDIR}},
    { INTER_8x16,{DIR_BWD,DIR_BIDIR}},
    { INTER_16x8,{DIR_BIDIR,DIR_BIDIR}},
    { INTER_8x16,{DIR_BIDIR,DIR_BIDIR}},
    { INTER_8x8,{NODIR,NODIR}},
};

typedef struct _SBinfo{
    DirSB dir;
    mfxI32 num;
    mfxI32 offs[4];
    mfxI32 width;
    mfxI32 height;
} SBinfo;

const SBinfo infoSBType[17]={
    { DIR_FWD,1,{0,0,0,0,},2,2 }, // 8x8
    { DIR_FWD,1,{0,4,0,0},2,1 }, // 8x4
    { DIR_FWD,2,{0,1,0,0},1,2 }, // 4x8
    { DIR_FWD,4,{0,1,4,5},1,1 }, // 4x4
    { DIR_BIDIR,4,{0,1,4,5},1,1 }, //DIRECT
    { DIR_FWD,1,{0,0,0,0},2,2 },
    { DIR_BWD,1,{0,0,0,0},2,2 },
    { DIR_BIDIR,1,{0,0,0,0},2,2 },
    { DIR_FWD,2,{0,4,0,0},2,1 },
    { DIR_FWD,2,{0,1,0,0},1,2 },
    { DIR_BWD,2,{0,4,0,0},2,1 },
    { DIR_BWD,2,{0,1,0,0},1,2 },
    { DIR_BIDIR,2,{0,4,0,0},2,1 },
    { DIR_BIDIR,2,{0,1,0,0},1,2 },
    { DIR_FWD,4,{0,1,4,5},1,1 },
    { DIR_BWD,4,{0,1,4,5},1,1 },
    { DIR_BIDIR,4,{0,1,4,5},1,1 }
};

const int sbOff[4] = {0,2,8,10};

//Return pointer for next empty
mfxI16Pair* PackMVs(const H264MacroblockGlobalInfo* mbinfo, mfxI16Pair* pMV, H264MotionVector* mvL0, H264MotionVector* mvL1 )
{
    MBinfo minfo = infoMBType[mbinfo->mbtype];
    switch(minfo.type)
    {
        case FORWARD:
            PACK_FOR(0);
            break;
        case BACKWARD:
            PACK_BACK(0);
            break;
        case BIDIR:
            PACK_BIDIR(0);
            break;
        case INTER_16x8:
            switch(minfo.sbdir[0])
            {
                case DIR_FWD: PACK_FOR(0); break;
                case DIR_BWD: PACK_BACK(0); break;
                case DIR_BIDIR: PACK_BIDIR(0); break;
                default: VM_ASSERT(0);
            }
            switch(minfo.sbdir[1])
            {
                case DIR_FWD: PACK_FOR(8); break;
                case DIR_BWD: PACK_BACK(8); break;
                case DIR_BIDIR: PACK_BIDIR(8); break;
                default: VM_ASSERT(0);
            }
            break;
        case INTER_8x16:
            switch(minfo.sbdir[0])
            {
                case DIR_FWD: PACK_FOR(0); break;
                case DIR_BWD: PACK_BACK(0); break;
                case DIR_BIDIR: PACK_BIDIR(0); break;
                default: VM_ASSERT(0);
            }

            switch(minfo.sbdir[1])
            {
                case DIR_FWD: PACK_FOR(2); break;
                case DIR_BWD: PACK_BACK(2); break;
                case DIR_BIDIR: PACK_BIDIR(2); break;
                default: VM_ASSERT(0);
            }

            break;
        case INTER_8x8:
        case INTER_8x8_REF0:
            for(Ipp32s j = 0; j < 4; j++){
                Ipp8u sbtype = mbinfo->sbtype[j];
                SBinfo info = infoSBType[sbtype];

                Ipp32s sb0 = sbOff[j];
                switch(info.dir){
                     case DIR_FWD:
                          for(Ipp32s k=0; k<info.num; k++){
                              mfxI32 off = sb0 + info.offs[k];
                              PACK_FOR(off);
                          }
                          break;
                     case DIR_BWD:
                          for(Ipp32s k=0; k<info.num; k++){
                              mfxI32 off = sb0 + info.offs[k];
                              PACK_BACK(off);
                          }
                          break;
                     case DIR_BIDIR:
                          for(Ipp32s k=0; k<info.num; k++){
                             mfxI32 off = sb0 + info.offs[k];
                             PACK_BIDIR(off);
                          }
                          break;
                     default:
                          VM_ASSERT(0);
                          break;
                }
            }
            break;
        default:
            break;
        }
    return pMV;
}

void UnPackMVs(const H264MacroblockGlobalInfo *, mfxI16Pair* pMV, H264MotionVector* mvL0, H264MotionVector* mvL1 )
{
    // FIXME: only 32MV currently supported
    for(mfxU32 i = 0; i < 16; i++)
    {
        mvL0[i].mvx = pMV[i].x;
        mvL0[i].mvy = pMV[i].y;
        mvL1[i].mvx = pMV[i + 16].x;
        mvL1[i].mvy = pMV[i + 16].y;
    }
}

void PackRefIdxs(const H264MacroblockGlobalInfo* mbinfo, mfxMbCode* mb, T_RefIdx* refL0, T_RefIdx* refL1)
{
    MBinfo minfo = infoMBType[mbinfo->mbtype];
    mfxI32 i;

    switch(minfo.type)
    {
        case FORWARD:
            mb->AVC.RefPicSelect[0][0] = refL0[0];
            break;
        case BACKWARD:
            mb->AVC.RefPicSelect[1][0] = refL1[0];
            break;
        case BIDIR:  //16x16
            mb->AVC.RefPicSelect[0][0] = refL0[0];
            mb->AVC.RefPicSelect[1][0] = refL1[0];
            break;
        case INTER_16x8:
            if( minfo.sbdir[0] & DIR_FWD ) mb->AVC.RefPicSelect[0][0] = refL0[0];
            if( minfo.sbdir[0] & DIR_BWD ) mb->AVC.RefPicSelect[1][0] = refL1[0];
            if( minfo.sbdir[1] & DIR_FWD ) mb->AVC.RefPicSelect[0][1] = refL0[8];
            if( minfo.sbdir[1] & DIR_BWD ) mb->AVC.RefPicSelect[1][1] = refL1[8];
            break;
        case INTER_8x16:
            if( minfo.sbdir[0] & DIR_FWD ) mb->AVC.RefPicSelect[0][0] = refL0[0];
            if( minfo.sbdir[0] & DIR_BWD ) mb->AVC.RefPicSelect[1][0] = refL1[0];
            if( minfo.sbdir[1] & DIR_FWD ) mb->AVC.RefPicSelect[0][1] = refL0[2];
            if( minfo.sbdir[1] & DIR_BWD ) mb->AVC.RefPicSelect[1][1] = refL1[2];
            break;
        case INTER_8x8:
        case INTER_8x8_REF0:
            for(i = 0; i < 4; i++){
                Ipp8u sbtype = mbinfo->sbtype[i];
                SBinfo sinfo = infoSBType[sbtype];

                if( sinfo.dir & DIR_FWD ) mb->AVC.RefPicSelect[0][i] = refL0[sbOff[i]];
                if( sinfo.dir & DIR_BWD ) mb->AVC.RefPicSelect[1][i] = refL1[sbOff[i]];
            }
            break;
        default:
            break;
    }
}

static void SetIdx( T_RefIdx* ref, T_RefIdx idx, mfxI32 pos, mfxI32 w, mfxI32 h )
{
    mfxI32 i,j;
    T_RefIdx* r = ref+pos;
    for( i=0; i<h; i++ ){
        for( j=0; j<w; j++ ){
            r[j] = idx;
        }
        r += 4;
    }
}

void UnPackRefIdxs(const H264MacroblockGlobalInfo* mbinfo, mfxMbCodeAVC* mb, T_RefIdx* refL0, T_RefIdx* refL1)
{
    SetIdx(refL0, -1, 0, 4, 4 );
    SetIdx(refL1, -1, 0, 4, 4 );

    const MBinfo& mbInfo = infoMBType[mbinfo->mbtype];
    switch (mbInfo.type)
    {
    case FORWARD:
        SetIdx(refL0, mb->RefPicSelect[0][0],  0, 2, 2 );
        SetIdx(refL0, mb->RefPicSelect[0][1],  2, 2, 2 );
        SetIdx(refL0, mb->RefPicSelect[0][2],  8, 2, 2 );
        SetIdx(refL0, mb->RefPicSelect[0][3], 10, 2, 2 );
        break;
    case BACKWARD:
        SetIdx(refL1, mb->RefPicSelect[1][0],  0, 2, 2 );
        SetIdx(refL1, mb->RefPicSelect[1][1],  2, 2, 2 );
        SetIdx(refL1, mb->RefPicSelect[1][2],  8, 2, 2 );
        SetIdx(refL1, mb->RefPicSelect[1][3], 10, 2, 2 );
        break;
    case BIDIR:
        SetIdx(refL0, mb->RefPicSelect[0][0],  0, 2, 2 );
        SetIdx(refL0, mb->RefPicSelect[0][1],  2, 2, 2 );
        SetIdx(refL0, mb->RefPicSelect[0][2],  8, 2, 2 );
        SetIdx(refL0, mb->RefPicSelect[0][3], 10, 2, 2 );
        SetIdx(refL1, mb->RefPicSelect[1][0],  0, 2, 2 );
        SetIdx(refL1, mb->RefPicSelect[1][1],  2, 2, 2 );
        SetIdx(refL1, mb->RefPicSelect[1][2],  8, 2, 2 );
        SetIdx(refL1, mb->RefPicSelect[1][3], 10, 2, 2 );
        break;
    case INTER_16x8:
        if (mbInfo.sbdir[0] & DIR_FWD)
        {
            SetIdx(refL0, mb->RefPicSelect[0][0], 0, 2, 2);
            SetIdx(refL0, mb->RefPicSelect[0][1], 2, 2, 2);
        }
        if (mbInfo.sbdir[0] & DIR_BWD)
        {
            SetIdx(refL1, mb->RefPicSelect[1][0], 0, 2, 2);
            SetIdx(refL1, mb->RefPicSelect[1][1], 2, 2, 2);
        }
        if (mbInfo.sbdir[1] & DIR_FWD)
        {
            SetIdx(refL0, mb->RefPicSelect[0][2],  8, 2, 2);
            SetIdx(refL0, mb->RefPicSelect[0][3], 10, 2, 2);
        }
        if (mbInfo.sbdir[1] & DIR_BWD)
        {
            SetIdx(refL1, mb->RefPicSelect[1][2],  8, 2, 2);
            SetIdx(refL1, mb->RefPicSelect[1][3], 10, 2, 2);
        }
        break;
    case INTER_8x16:
        if (mbInfo.sbdir[0] & DIR_FWD)
        {
            SetIdx(refL0, mb->RefPicSelect[0][0], 0, 2, 2);
            SetIdx(refL0, mb->RefPicSelect[0][2], 8, 2, 2);
        }
        if (mbInfo.sbdir[0] & DIR_BWD)
        {
            SetIdx(refL1, mb->RefPicSelect[1][0], 0, 2, 2);
            SetIdx(refL1, mb->RefPicSelect[1][2], 8, 2, 2);
        }
        if (mbInfo.sbdir[1] & DIR_FWD)
        {
            SetIdx(refL0, mb->RefPicSelect[0][1],  2, 2, 2);
            SetIdx(refL0, mb->RefPicSelect[0][3], 10, 2, 2);
        }
        if (mbInfo.sbdir[1] & DIR_BWD)
        {
            SetIdx(refL1, mb->RefPicSelect[1][1],  2, 2, 2);
            SetIdx(refL1, mb->RefPicSelect[1][3], 10, 2, 2);
        }
        break;
    case INTER_8x8:
    case INTER_8x8_REF0:
        for (mfxU32 i = 0; i < 4; i++)
        {
            SBinfo sinfo = infoSBType[mbinfo->sbtype[i]];
            if (sinfo.dir & DIR_FWD)
            {
                SetIdx(refL0, mb->RefPicSelect[0][i], sbOff[i], 2, 2);
            }
            if (sinfo.dir & DIR_BWD)
            {
                SetIdx(refL1, mb->RefPicSelect[1][i], sbOff[i], 2, 2);
            }
        }
        break;
    default:
        break;
    }
}

MultiFrameLocker::MultiFrameLocker(VideoCORE& core, mfxFrameSurface& surface)
: m_core(core)
, m_surface(surface)
{
    memset(m_locked, 0, sizeof(m_locked));
}

MultiFrameLocker::~MultiFrameLocker()
{
    for (mfxU32 i = 0; i < m_surface.NumFrameData; i++)
        Unlock(i);
}

mfxStatus MultiFrameLocker::Lock(mfxU32 label)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_surface.Data[label]->Y == 0 && m_surface.Data[label]->U == 0 &&
        m_surface.Data[label]->V == 0 && m_surface.Data[label]->A == 0)
    {
        sts = m_core.LockFrame(m_surface.Data[label]->MemId, m_surface.Data[label]);
        m_locked[label] = (sts == MFX_ERR_NONE);
    }

    return sts;
}

mfxStatus MultiFrameLocker::Unlock(mfxU32 label)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_locked[label])
    {
        sts = m_core.UnlockFrame(m_surface.Data[label]->MemId, m_surface.Data[label]);
        m_locked[label] = (sts != MFX_ERR_NONE);
    }

    return sts;
}

void SetDefaultParamForReset(mfxVideoParam& parNew, const mfxVideoParam& parOld){
    mfxExtCodingOption* optsOld = GetExtCodingOptions( parOld.ExtParam, parOld.NumExtParam );
    mfxExtCodingOption* optsNew = GetExtCodingOptions( parNew.ExtParam, parNew.NumExtParam );
    mfxExtSVCSeqDesc* pSvcLayers = (mfxExtSVCSeqDesc*)GetExtBuffer( parOld.ExtParam, parOld.NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC );
    mfxExtSVCRateControl *rc = (mfxExtSVCRateControl*)GetExtBuffer( parOld.ExtParam, parOld.NumExtParam, MFX_EXTBUFF_SVC_RATE_CONTROL );

    //set unspecified parameters
    if (!parNew.AsyncDepth)             parNew.AsyncDepth           = parOld.AsyncDepth;
    if (!parNew.IOPattern)              parNew.IOPattern            = parOld.IOPattern;
    if (!parNew.mfx.FrameInfo.FourCC)   parNew.mfx.FrameInfo.FourCC = parOld.mfx.FrameInfo.FourCC;
    if (!parNew.mfx.FrameInfo.FrameRateExtN) parNew.mfx.FrameInfo.FrameRateExtN = parOld.mfx.FrameInfo.FrameRateExtN;
    if (!parNew.mfx.FrameInfo.FrameRateExtD) parNew.mfx.FrameInfo.FrameRateExtD = parOld.mfx.FrameInfo.FrameRateExtD;
    if (!parNew.mfx.FrameInfo.AspectRatioW)  parNew.mfx.FrameInfo.AspectRatioW  = parOld.mfx.FrameInfo.AspectRatioW;
    if (!parNew.mfx.FrameInfo.AspectRatioH)  parNew.mfx.FrameInfo.AspectRatioH  = parOld.mfx.FrameInfo.AspectRatioH;
    if (!parNew.mfx.FrameInfo.PicStruct)     parNew.mfx.FrameInfo.PicStruct     = parOld.mfx.FrameInfo.PicStruct;
    //if (!parNew.mfx.FrameInfo.ChromaFormat) parNew.mfx.FrameInfo.ChromaFormat = parOld.mfx.FrameInfo.ChromaFormat;
    if (!parNew.mfx.CodecProfile)       parNew.mfx.CodecProfile      = parOld.mfx.CodecProfile;
    if (!parNew.mfx.CodecLevel)         parNew.mfx.CodecLevel        = parOld.mfx.CodecLevel;
    if (!parNew.mfx.NumThread)          parNew.mfx.NumThread         = parOld.mfx.NumThread;
    if (!parNew.mfx.TargetUsage)        parNew.mfx.TargetUsage       = parOld.mfx.TargetUsage;
    if (!pSvcLayers && !rc) { // except SVC layers params
        if (!parNew.mfx.FrameInfo.Width)    parNew.mfx.FrameInfo.Width  = parOld.mfx.FrameInfo.Width;
        if (!parNew.mfx.FrameInfo.Height)   parNew.mfx.FrameInfo.Height = parOld.mfx.FrameInfo.Height;
        //if (!parNew.mfx.FrameInfo.CropX)    parNew.mfx.FrameInfo.CropX  = parOld.mfx.FrameInfo.CropX;
        //if (!parNew.mfx.FrameInfo.CropY)    parNew.mfx.FrameInfo.CropY  = parOld.mfx.FrameInfo.CropY;
        if (!parNew.mfx.FrameInfo.CropW)    parNew.mfx.FrameInfo.CropW  = parOld.mfx.FrameInfo.CropW;
        if (!parNew.mfx.FrameInfo.CropH)    parNew.mfx.FrameInfo.CropH  = parOld.mfx.FrameInfo.CropH;
        if (!parNew.mfx.GopRefDist)         parNew.mfx.GopRefDist        = parOld.mfx.GopRefDist;
        if (!parNew.mfx.GopOptFlag)         parNew.mfx.GopOptFlag        = parOld.mfx.GopOptFlag;
        if (!parNew.mfx.RateControlMethod)  parNew.mfx.RateControlMethod = parOld.mfx.RateControlMethod;

        if(    parNew.mfx.CodecProfile != MFX_PROFILE_AVC_STEREO_HIGH
            && parNew.mfx.CodecProfile != MFX_PROFILE_AVC_MULTIVIEW_HIGH){
            mfxU16 old_multiplier = IPP_MAX(parOld.mfx.BRCParamMultiplier, 1);
            mfxU16 new_multiplier = IPP_MAX(parNew.mfx.BRCParamMultiplier, 1);

            if (old_multiplier > new_multiplier) {
                parNew.mfx.BufferSizeInKB = (mfxU16)((mfxU64)parNew.mfx.BufferSizeInKB*new_multiplier/old_multiplier);
                if (parNew.mfx.RateControlMethod == MFX_RATECONTROL_CBR || parNew.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
                    parNew.mfx.InitialDelayInKB = (mfxU16)((mfxU64)parNew.mfx.InitialDelayInKB*new_multiplier/old_multiplier);
                    parNew.mfx.TargetKbps       = (mfxU16)((mfxU64)parNew.mfx.TargetKbps      *new_multiplier/old_multiplier);
                    parNew.mfx.MaxKbps          = (mfxU16)((mfxU64)parNew.mfx.MaxKbps         *new_multiplier/old_multiplier);
                }
                new_multiplier = old_multiplier;
                parNew.mfx.BRCParamMultiplier = new_multiplier;
            }
            if (!parNew.mfx.BufferSizeInKB) parNew.mfx.BufferSizeInKB = (mfxU16)((mfxU64)parOld.mfx.BufferSizeInKB*old_multiplier/new_multiplier);
            if (parNew.mfx.RateControlMethod == parOld.mfx.RateControlMethod) {
                if (parNew.mfx.RateControlMethod > MFX_RATECONTROL_VBR)
                    old_multiplier = new_multiplier = 1;
                if (!parNew.mfx.InitialDelayInKB) parNew.mfx.InitialDelayInKB = (mfxU16)((mfxU64)parOld.mfx.InitialDelayInKB*old_multiplier/new_multiplier);
                if (!parNew.mfx.TargetKbps)       parNew.mfx.TargetKbps       = (mfxU16)((mfxU64)parOld.mfx.TargetKbps      *old_multiplier/new_multiplier);
                if (!parNew.mfx.MaxKbps)          parNew.mfx.MaxKbps          = (mfxU16)((mfxU64)parOld.mfx.MaxKbps         *old_multiplier/new_multiplier);
            }
        }
    }
    if (!parNew.mfx.NumSlice)       parNew.mfx.NumSlice       = parOld.mfx.NumSlice;
    if (!parNew.mfx.NumRefFrame)    parNew.mfx.NumRefFrame    = parOld.mfx.NumRefFrame;

    if(optsOld && optsNew){
        if (!optsOld->RateDistortionOpt) optsOld->RateDistortionOpt   = optsNew->RateDistortionOpt;
        if (!optsOld->MECostType)        optsOld->MECostType          = optsNew->MECostType;
        if (!optsOld->MESearchType)      optsOld->MESearchType        = optsNew->MESearchType;
        if (!optsOld->MVSearchWindow.x)  optsOld->MVSearchWindow.x    = optsNew->MVSearchWindow.x;
        if (!optsOld->MVSearchWindow.y)  optsOld->MVSearchWindow.y    = optsNew->MVSearchWindow.y;
        if (!optsOld->EndOfSequence)     optsOld->EndOfSequence       = optsNew->EndOfSequence;
        if (!optsOld->FramePicture)      optsOld->FramePicture        = optsNew->FramePicture;

        if (!optsOld->CAVLC)               optsOld->CAVLC               = optsNew->CAVLC;
        if (!optsOld->RecoveryPointSEI)    optsOld->RecoveryPointSEI    = optsNew->RecoveryPointSEI;
        if (!optsOld->ViewOutput)          optsOld->ViewOutput          = optsNew->ViewOutput;
        if (!optsOld->NalHrdConformance)   optsOld->NalHrdConformance   = optsNew->NalHrdConformance;
        if (!optsOld->SingleSeiNalUnit)    optsOld->SingleSeiNalUnit    = optsNew->SingleSeiNalUnit;
        if (!optsOld->VuiVclHrdParameters) optsOld->VuiVclHrdParameters = optsNew->VuiVclHrdParameters;

        if (!optsOld->RefPicListReordering) optsOld->RefPicListReordering = optsNew->RefPicListReordering;
        if (!optsOld->ResetRefList)         optsOld->ResetRefList         = optsNew->ResetRefList;
        if (!optsOld->RefPicMarkRep)        optsOld->RefPicMarkRep        = optsNew->RefPicMarkRep;
        if (!optsOld->FieldOutput)          optsOld->FieldOutput          = optsNew->FieldOutput;

        if (!optsOld->IntraPredBlockSize)   optsOld->IntraPredBlockSize   = optsNew->IntraPredBlockSize;
        if (!optsOld->InterPredBlockSize)   optsOld->InterPredBlockSize   = optsNew->InterPredBlockSize;
        if (!optsOld->MVPrecision)          optsOld->MVPrecision          = optsNew->MVPrecision;
        if (!optsOld->MaxDecFrameBuffering) optsOld->MaxDecFrameBuffering = optsNew->MaxDecFrameBuffering;

        if (!optsOld->AUDelimiter)         optsOld->AUDelimiter         = optsNew->AUDelimiter;
        if (!optsOld->EndOfStream)         optsOld->EndOfStream         = optsNew->EndOfStream;
        if (!optsOld->PicTimingSEI)        optsOld->PicTimingSEI        = optsNew->PicTimingSEI;
        if (!optsOld->VuiNalHrdParameters) optsOld->VuiNalHrdParameters = optsNew->VuiNalHrdParameters;
    }
}

void mfxVideoInternalParam::SetCalcParams( mfxVideoParam *parMFX) {

    mfxU32 mult = IPP_MAX( parMFX->mfx.BRCParamMultiplier, 1);

    calcParam.TargetKbps = (mfxU32)parMFX->mfx.TargetKbps * (mfxU32)mult;
    calcParam.MaxKbps = (mfxU32)parMFX->mfx.MaxKbps * (mfxU32)mult;
    calcParam.BufferSizeInKB = (mfxU32)parMFX->mfx.BufferSizeInKB * (mfxU32)mult;
    calcParam.InitialDelayInKB = (mfxU32)parMFX->mfx.InitialDelayInKB * (mfxU32)mult;
}
void mfxVideoInternalParam::GetCalcParams( mfxVideoParam *parMFX) {

    mfxU32 maxVal = IPP_MAX( IPP_MAX( calcParam.TargetKbps, calcParam.MaxKbps), IPP_MAX( calcParam.BufferSizeInKB, calcParam.InitialDelayInKB));
    mfxU32 mult = (maxVal + 0xffff) / 0x10000;

    if (mult) {
        parMFX->mfx.BRCParamMultiplier = (mfxU16)mult;
        parMFX->mfx.TargetKbps = (mfxU16)(calcParam.TargetKbps / mult);
        parMFX->mfx.MaxKbps = (mfxU16)(calcParam.MaxKbps / mult);
        parMFX->mfx.BufferSizeInKB = (mfxU16)(calcParam.BufferSizeInKB / mult);
        parMFX->mfx.InitialDelayInKB = (mfxU16)(calcParam.InitialDelayInKB / mult);
    }
}

mfxVideoInternalParam::mfxVideoInternalParam(mfxVideoParam const & par)
{
    mfxVideoParam & base = *this;
    base = par;
    SetCalcParams( &base);
}

mfxVideoInternalParam& mfxVideoInternalParam::operator=(mfxVideoParam const & par)
{
    mfxVideoParam & base = *this;
    base = par;
    SetCalcParams( &base);
    return *this;
}

#endif // defined(MFX_ENABLE_H264_VIDEO_ENCODER_COMMON)

