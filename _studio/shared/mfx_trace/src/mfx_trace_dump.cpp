/* ****************************************************************************** *\

Copyright (C) 2012-2022 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_trace_dump.cpp

\* ****************************************************************************** */

#include "mfx_trace_dump.h"

#include "unistd.h"

std::string PointerToHexString(void* x)
{
    std::ostringstream result;
    result << std::setw(16) << std::setfill('0') << std::hex << std::uppercase << ((mfxU64)x);
    return result.str();
}

struct IdTable
{
    mfxI32 id;
    const char* str;
};

#define TABLE_ENTRY(_name) \
    { _name, #_name }


static IdTable tbl_BufferId[] =
{
    TABLE_ENTRY(MFX_EXTBUFF_AVC_REFLIST_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_AVC_TEMPORAL_LAYERS),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION2),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION3),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION_SPSPPS),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODER_CAPABILITY),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODED_FRAME_INFO),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODER_RESET_OPTION),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODER_ROI),
    TABLE_ENTRY(MFX_EXTBUFF_PICTURE_TIMING_SEI),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_AUXDATA),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_COMPOSITE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DEINTERLACING),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DENOISE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DETAIL),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DONOTUSE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DOUSE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_IMAGE_STABILIZATION),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_PROCAMP),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_SCENE_ANALYSIS),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_SCENE_CHANGE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO),
    TABLE_ENTRY(MFX_EXTBUFF_VIDEO_SIGNAL_INFO),
    TABLE_ENTRY(MFX_EXTBUFF_AVC_REFLIST_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_AVC_TEMPORAL_LAYERS),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODED_FRAME_INFO),
    TABLE_ENTRY(MFX_EXTBUFF_AVC_REFLISTS),
    TABLE_ENTRY(MFX_EXTBUFF_HEVC_TILES),
    TABLE_ENTRY(MFX_EXTBUFF_HEVC_PARAM),
    TABLE_ENTRY(MFX_EXTBUFF_HEVC_REGION),
    TABLE_ENTRY(MFX_EXTBUFF_DECODED_FRAME_INFO),
    TABLE_ENTRY(MFX_EXTBUFF_TIME_CODE),
    TABLE_ENTRY(MFX_EXTBUFF_THREADS_PARAM),
    TABLE_ENTRY(MFX_EXTBUFF_PRED_WEIGHT_TABLE),
    TABLE_ENTRY(MFX_EXTBUFF_DIRTY_RECTANGLES),
    TABLE_ENTRY(MFX_EXTBUFF_MOVING_RECTANGLES),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION_VPS),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_ROTATION),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODED_SLICES_INFO),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_SCALING),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODER_IPCM_AREA),
    TABLE_ENTRY(MFX_EXTBUFF_INSERT_HEADERS),
    TABLE_ENTRY(MFX_EXTBUFF_VP9_SEGMENTATION),
    TABLE_ENTRY(MFX_EXTBUFF_VP9_TEMPORAL_LAYERS),
    TABLE_ENTRY(MFX_EXTBUFF_VP9_PARAM),
#if defined(ONEVPL_EXPERIMENTAL)
    TABLE_ENTRY(MFX_EXTBUFF_ENCODED_QUALITY_INFO_MODE),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODED_QUALITY_INFO_OUTPUT),
    TABLE_ENTRY(MFX_EXTBUFF_AV1_SCREEN_CONTENT_TOOLS),
    TABLE_ENTRY(MFX_EXTBUFF_ALPHA_CHANNEL_ENC_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_AI_ENC_CTRL)
#endif
};

static IdTable tbl_FourCC[] = {
    TABLE_ENTRY(MFX_FOURCC_NV12),
    TABLE_ENTRY(MFX_FOURCC_YV12),
    TABLE_ENTRY(MFX_FOURCC_NV16),
    TABLE_ENTRY(MFX_FOURCC_YUY2),
    TABLE_ENTRY(MFX_FOURCC_RGB3),
    TABLE_ENTRY(MFX_FOURCC_RGB4),
    TABLE_ENTRY(MFX_FOURCC_P8),
    TABLE_ENTRY(MFX_FOURCC_P8_TEXTURE),
    TABLE_ENTRY(MFX_FOURCC_P010),
    TABLE_ENTRY(MFX_FOURCC_P016),
    TABLE_ENTRY(MFX_FOURCC_P210),
    TABLE_ENTRY(MFX_FOURCC_BGR4),
    TABLE_ENTRY(MFX_FOURCC_A2RGB10),
    TABLE_ENTRY(MFX_FOURCC_ARGB16),
    TABLE_ENTRY(MFX_FOURCC_R16),
    TABLE_ENTRY(MFX_FOURCC_ABGR16),
    TABLE_ENTRY(MFX_FOURCC_AYUV),
    TABLE_ENTRY(MFX_FOURCC_AYUV_RGB4),
    TABLE_ENTRY(MFX_FOURCC_UYVY),
    TABLE_ENTRY(MFX_FOURCC_Y210),
    TABLE_ENTRY(MFX_FOURCC_Y410),
    TABLE_ENTRY(MFX_FOURCC_NV21),
    TABLE_ENTRY(MFX_FOURCC_IYUV),
    TABLE_ENTRY(MFX_FOURCC_I010),
    TABLE_ENTRY(MFX_FOURCC_Y216),
    TABLE_ENTRY(MFX_FOURCC_Y416),
    TABLE_ENTRY(MFX_FOURCC_RGBP),
    TABLE_ENTRY(MFX_FOURCC_I210),
    TABLE_ENTRY(MFX_FOURCC_I420),
    TABLE_ENTRY(MFX_FOURCC_I422),
    TABLE_ENTRY(MFX_FOURCC_BGRA),
    TABLE_ENTRY(MFX_FOURCC_BGRP),
#ifdef ONEVPL_EXPERIMENTAL
    TABLE_ENTRY(MFX_FOURCC_XYUV),
    TABLE_ENTRY(MFX_FOURCC_ABGR16F),
#endif
};

static IdTable tbl_CodecId[] = {
    TABLE_ENTRY(MFX_CODEC_AVC),
    TABLE_ENTRY(MFX_CODEC_HEVC),
    TABLE_ENTRY(MFX_CODEC_MPEG2),
    TABLE_ENTRY(MFX_CODEC_VC1),
    TABLE_ENTRY(MFX_CODEC_CAPTURE),
    TABLE_ENTRY(MFX_CODEC_VP9),
    TABLE_ENTRY(MFX_CODEC_AV1)
};

static IdTable tbl_IOPattern[] = {
    TABLE_ENTRY(MFX_IOPATTERN_IN_VIDEO_MEMORY),
    TABLE_ENTRY(MFX_IOPATTERN_IN_SYSTEM_MEMORY),
    TABLE_ENTRY(MFX_IOPATTERN_OUT_VIDEO_MEMORY),
    TABLE_ENTRY(MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
};


std::string DumpContext::GetBufferIdInString(mfxU32 bufferid)
{
    std::string str;
    for (size_t i = 0; i < sizeof(tbl_BufferId) / sizeof(tbl_BufferId[0]); ++i)
    {
        if (tbl_BufferId[i].id == static_cast<int>(bufferid))
        {
            str = tbl_BufferId[i].str;
            break;
        }
    }
    return str;
}

std::string GetFourCCInString(mfxU32 fourcc) {

    std::basic_stringstream<char> stream;
    std::string name = "UNKNOWN";
    int j = 0;
    for (unsigned int i = 0; i < (sizeof(tbl_FourCC) / sizeof(tbl_FourCC[0])); i++)
    {
        if (tbl_FourCC[i].id == static_cast<int>(fourcc))
        {
            name = "";
            while (tbl_FourCC[i].str[j + 11] != '\0')
            {
                name = name + tbl_FourCC[i].str[j + 11];
                j++;
            }
            name = name + "\0";
            break;
        }
    }
    stream << name;
    return stream.str();
}

std::string GetCodecIdInString(mfxU32 id) {

    std::basic_stringstream<char> stream;
    std::string name = "UNKNOWN";
    for (unsigned int i = 0; i < (sizeof(tbl_CodecId) / sizeof(tbl_CodecId[0])); i++)
    {
        if (tbl_CodecId[i].id == static_cast<int>(id))
        {
            name = tbl_CodecId[i].str;
            break;
        }

    }
    stream << name;
    return stream.str();
}

std::string GetIOPatternInString(mfxU32 io) {

    std::basic_stringstream<char> stream;
    std::string name;
    for (unsigned int i = 0; i < (sizeof(tbl_IOPattern) / sizeof(tbl_IOPattern[0])); i++)
    {
        if (tbl_IOPattern[i].id & static_cast<int>(io))
        {
            name += tbl_IOPattern[i].str;
            name += "; ";
        }

    }
    if (!name.length())
    {
        name = "UNKNOWN";
        name += "(" + ToString(io) + ")";
    }
    stream << name;
    return stream.str();
}

bool _IsBadReadPtr(void *ptr, size_t size)
{
    int fb[2];
    if (pipe(fb) >= 0)
    {
        bool tmp = (write(fb[1], ptr, size) <= 0);
        close(fb[0]);
        close(fb[1]);
        return tmp;
    }
    return true;
}