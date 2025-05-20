/* ****************************************************************************** *\

Copyright (C) 2012-2024 Intel Corporation.  All rights reserved.

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

File Name: mfx_trace_dump.h

\* ****************************************************************************** */

#ifndef _MFX_TRACE_DUMP_H_
#define _MFX_TRACE_DUMP_H_

#include <string>
#include <sstream>
#include <iomanip>
#include <iterator>
#include <typeinfo>
#include <type_traits>
#include "mfxstructures.h"

std::string PointerToHexString(void* x);
std::string GetFourCCInString(mfxU32 fourcc);
std::string GetCodecIdInString(mfxU32 id);
std::string GetIOPatternInString(mfxU32 io);
bool _IsBadReadPtr(void *ptr, size_t size);

using LongTermRefList = std::remove_extent<decltype(mfxExtAVCRefListCtrl::RejectedRefList)>::type;

#define DUMP_RESERVED_ARRAY(r) dump_reserved_array(&(r[0]), sizeof(r)/sizeof(r[0]))

#define DUMP_FIELD(_field) \
    str += structName + "." #_field "=" + ToString(_struct._field) + "\n";

#define DUMP_FIELD_UCHAR(_field) \
    str += structName + "." #_field "=" + ToString((int)_struct._field) + "\n";

#define DUMP_FIELD_RESERVED(_field) \
    str += structName + "." #_field "[]=" + DUMP_RESERVED_ARRAY(_struct._field) + "\n";

#define ToString(x)  static_cast<std::ostringstream const &>( \
    ( std::ostringstream() << std::dec << (x) ) ).str()

#define ToHexFormatString(x) (static_cast<std::ostringstream const &>( \
    ( std::ostringstream() << std::hex << PointerToHexString((void*)(x)) ) ).str() )

#define DEFINE_DUMP_FUNCTION(type) \
    std::string dump(const std::string structName, const type & var);

enum eDumpContect {
    DUMPCONTEXT_MFX,
    DUMPCONTEXT_VPP,
    DUMPCONTEXT_ALL,
};

enum eDumpFormat {
    DUMP_DEC,
    DUMP_HEX
};

class DumpContext
{
public:
    eDumpContect context;

    DumpContext(void) {
        context = DUMPCONTEXT_ALL;
    }
    ~DumpContext(void) {}

    template<typename T>
    inline std::string toString(T x, eDumpFormat format = DUMP_DEC){
        return static_cast<std::ostringstream const &>
            (( std::ostringstream() << ((format == DUMP_DEC) ? std::dec : std::hex) << x )).str();
    }

    std::string GetBufferIdInString(mfxU32 bufferid);

    template<typename T>
    std::string dump_reserved_array(T* data, size_t size)
    {
        std::stringstream result;

        result << "{ ";
        for (size_t i = 0; i < size; ++i) {
            result << data[i];
            if (i < (size-1)) result << ", ";
        }
        result << " }";
        return result.str();
    }

    template<typename T0, typename T1>
    std::string dump_array_with_cast(T0* data, size_t size)
    {
        std::stringstream result;

        result << "{ ";
        for (size_t i = 0; i < size; ++i) {
            result << (T1)data[i];
            if (i < (size - 1)) result << ", ";
        }
        result << " }";
        return result.str();
    }

    template<typename T>
    std::string dump_hex_array(T* data, size_t size)
    {
        std::stringstream result;
        result << "{ "  << std::hex << std::uppercase;
        for (size_t i = 0; i < size; ++i)
            result << std::setw(sizeof(T) * 2) << std::setfill('0') << (mfxU64)data[i];
        result << " }";
        return result.str();
    }
    template<typename T, size_t N>
    inline std::string dump_hex_array(T (&data)[N]) { return dump_hex_array(data, N); }

    template<typename T>
    inline const char* get_type(){ return typeid(T).name(); }

    template<typename T>
    std::string dump(const std::string structName, const T&)
    {
        return static_cast < std::ostringstream const & >
            (std::ostringstream() << "Not support dump " << get_type<T>() << "!!!").str();
    }

    template<typename T>
    inline std::string dump_mfxExtParams(const std::string structName, const T& _struct)
    {
        std::string str;
        std::string name;

        str += structName + ".NumExtParam=" + ToString(_struct.NumExtParam) + "\n";
        str += structName + ".ExtParam=" + ToString(_struct.ExtParam) + "\n";

        if (_struct.ExtParam) {
            for (mfxU16 i = 0; i < _struct.NumExtParam; ++i)
            {
                if ((!_IsBadReadPtr(_struct.ExtParam, sizeof(mfxExtBuffer**))) && (!_IsBadReadPtr(_struct.ExtParam[i], sizeof(mfxExtBuffer*))))
                {
                    name = structName + ".ExtParam[" + ToString(i) + "]";
                    str += name + "=" + ToString(_struct.ExtParam[i]) + "\n";
                    switch (_struct.ExtParam[i]->BufferId)
                    {
                    case MFX_EXTBUFF_CODING_OPTION:
                        str += dump(name, *((mfxExtCodingOption*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_CODING_OPTION2:
                        str += dump(name, *((mfxExtCodingOption2*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_CODING_OPTION3:
                        str += dump(name, *((mfxExtCodingOption3*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_ENCODER_RESET_OPTION:
                        str += dump(name, *((mfxExtEncoderResetOption*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_AVC_REFLIST_CTRL:
                        str += dump(name, *((mfxExtAVCRefListCtrl*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_AVC_TEMPORAL_LAYERS:
                        str += dump(name, *((mfxExtAvcTemporalLayers*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_ENCODED_FRAME_INFO:
                        str += dump(name, *((mfxExtAVCEncodedFrameInfo*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_AVC_REFLISTS:
                        str += dump(name, *((mfxExtAVCRefLists*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_VPP_DENOISE:
                        str += dump(name, *((mfxExtVPPDenoise*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_VPP_DETAIL:
                        str += dump(name, *((mfxExtVPPDetail*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_VPP_PROCAMP:
                        str += dump(name, *((mfxExtVPPProcAmp*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_CODING_OPTION_SPSPPS:
                        str += dump(name, *((mfxExtCodingOptionSPSPPS*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_VIDEO_SIGNAL_INFO:
                        str += dump(name, *((mfxExtVideoSignalInfo*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_VPP_DOUSE:
                        str += dump(name, *((mfxExtVPPDoUse*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_PICTURE_TIMING_SEI:
                        str += dump(name, *((mfxExtPictureTimingSEI*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_VPP_COMPOSITE:
                        str += dump(name, *((mfxExtVPPComposite*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO:
                        str += dump(name, *((mfxExtVPPVideoSignalInfo*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_VPP_DEINTERLACING:
                        str += dump(name, *((mfxExtVPPDeinterlacing*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_HEVC_TILES:
                        str += dump(name, *((mfxExtHEVCTiles*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_HEVC_PARAM:
                        str += dump(name, *((mfxExtHEVCParam*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_HEVC_REGION:
                        str += dump(name, *((mfxExtHEVCRegion*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_DECODED_FRAME_INFO:
                        str += dump(name, *((mfxExtDecodedFrameInfo*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_TIME_CODE:
                        str += dump(name, *((mfxExtTimeCode*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_PRED_WEIGHT_TABLE:
                        str += dump(name, *((mfxExtPredWeightTable*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_ENCODER_CAPABILITY:
                        str += dump(name, *((mfxExtEncoderCapability*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_DIRTY_RECTANGLES:
                        str += dump(name, *((mfxExtDirtyRect*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_MOVING_RECTANGLES:
                        str += dump(name, *((mfxExtMoveRect*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION:
                        str += dump(name, *((mfxExtVPPFrameRateConversion*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_VPP_IMAGE_STABILIZATION:
                        str += dump(name, *((mfxExtVPPImageStab*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_ENCODER_ROI:
                        str += dump(name, *((mfxExtEncoderROI*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_CODING_OPTION_VPS:
                        str += dump(name, *((mfxExtCodingOptionVPS*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_VPP_ROTATION:
                        str += dump(name, *((mfxExtVPPRotation*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_ENCODED_SLICES_INFO:
                        str += dump(name, *((mfxExtEncodedSlicesInfo*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_VPP_SCALING:
                        str += dump(name, *((mfxExtVPPScaling*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_VPP_MIRRORING:
                        str += dump(name, *((mfxExtVPPMirroring*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_MV_OVER_PIC_BOUNDARIES:
                        str += dump(name, *((mfxExtMVOverPicBoundaries*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_VPP_COLORFILL:
                        str += dump(name, *((mfxExtVPPColorFill*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_DEC_VIDEO_PROCESSING:
                        str += dump(name, *((mfxExtDecVideoProcessing*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_MBQP:
                        str += dump(name, *((mfxExtMBQP*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_ENCODER_IPCM_AREA:
                        str += dump(name, *((mfxExtEncoderIPCMArea*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_INSERT_HEADERS:
                        str += dump(name, *((mfxExtInsertHeaders*)_struct.ExtParam[i])) + "\n";
                        break;
                    case  MFX_EXTBUFF_DECODE_ERROR_REPORT:
                        str += dump(name, *((mfxExtDecodeErrorReport*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME:
                        str += dump(name, *((mfxExtMasteringDisplayColourVolume*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO:
                        str += dump(name, *((mfxExtContentLightLevelInfo*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_ENCODED_UNITS_INFO:
                        str += dump(name, *((mfxExtEncodedUnitsInfo*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_VPP_COLOR_CONVERSION:
                        str += dump(name, *((mfxExtColorConversion*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_VPP_MCTF:
                        str += dump(name, *((mfxExtVppMctf*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_VP9_SEGMENTATION:
                        str += dump(name, *((mfxExtVP9Segmentation*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_VP9_TEMPORAL_LAYERS:
                        str += dump(name, *((mfxExtVP9TemporalLayers*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_VP9_PARAM:
                        str += dump(name, *((mfxExtVP9Param*)_struct.ExtParam[i])) + "\n";
                        break;
#if defined(ONEVPL_EXPERIMENTAL)
                    case MFX_EXTBUFF_ENCODED_QUALITY_INFO_MODE:
                        str += dump(name, *((mfxExtQualityInfoMode*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_ENCODED_QUALITY_INFO_OUTPUT:
                        str += dump(name, *((mfxExtQualityInfoOutput*)_struct.ExtParam[i])) + "\n";
                        break;                       
                    case MFX_EXTBUFF_AV1_SCREEN_CONTENT_TOOLS:
                        str += dump(name, *((mfxExtAV1ScreenContentTools*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_ALPHA_CHANNEL_ENC_CTRL:
                        str += dump(name, *((mfxExtAlphaChannelEncCtrl*)_struct.ExtParam[i])) + "\n";
                        break;
                    case MFX_EXTBUFF_AI_ENC_CTRL:
                        str += dump(name, *((mfxExtAIEncCtrl*)_struct.ExtParam[i])) + "\n";
                        break;
#endif
                    default:
                        str += dump(name, *(_struct.ExtParam[i])) + "\n";
                        break;
                    };
                }
                else
                {
                    str += "WARNING: Can't read from ExtParam[" + ToString(i) + "]!\n";
                    return str;
                }
            }
        }
        return str;
    }

    DEFINE_DUMP_FUNCTION(mfxEncodeCtrl);
    DEFINE_DUMP_FUNCTION(mfxExtBuffer);
    DEFINE_DUMP_FUNCTION(mfxVideoParam);
    DEFINE_DUMP_FUNCTION(mfxInfoMFX);
    DEFINE_DUMP_FUNCTION(mfxInfoVPP);
    DEFINE_DUMP_FUNCTION(mfxFrameAllocRequest);
    DEFINE_DUMP_FUNCTION(mfxFrameInfo);
    DEFINE_DUMP_FUNCTION(mfxFrameId);
    DEFINE_DUMP_FUNCTION(mfxBitstream);
    DEFINE_DUMP_FUNCTION(mfxFrameSurface1);
    DEFINE_DUMP_FUNCTION(mfxFrameData);

    DEFINE_DUMP_FUNCTION(mfxExtCodingOption);
    DEFINE_DUMP_FUNCTION(mfxExtCodingOption2);
    DEFINE_DUMP_FUNCTION(mfxExtCodingOption3);
    DEFINE_DUMP_FUNCTION(mfxExtEncoderResetOption);
    DEFINE_DUMP_FUNCTION(mfxExtAVCRefListCtrl);
    DEFINE_DUMP_FUNCTION(mfxExtAvcTemporalLayers);
    DEFINE_DUMP_FUNCTION(mfxExtAVCEncodedFrameInfo);
    DEFINE_DUMP_FUNCTION(mfxExtAVCRefLists);
    DEFINE_DUMP_FUNCTION(mfxExtVPPDenoise);
    DEFINE_DUMP_FUNCTION(mfxExtVPPDetail);
    DEFINE_DUMP_FUNCTION(mfxExtVPPProcAmp);
    DEFINE_DUMP_FUNCTION(mfxExtCodingOptionSPSPPS);
    DEFINE_DUMP_FUNCTION(mfxExtVideoSignalInfo);
    DEFINE_DUMP_FUNCTION(mfxExtVPPDoUse);
    DEFINE_DUMP_FUNCTION(mfxExtPictureTimingSEI);
    DEFINE_DUMP_FUNCTION(mfxExtVPPComposite);
    DEFINE_DUMP_FUNCTION(mfxExtVPPVideoSignalInfo);
    DEFINE_DUMP_FUNCTION(mfxExtVPPDeinterlacing);
    DEFINE_DUMP_FUNCTION(mfxExtHEVCTiles);
    DEFINE_DUMP_FUNCTION(mfxExtHEVCParam);
    DEFINE_DUMP_FUNCTION(mfxExtHEVCRegion);
    DEFINE_DUMP_FUNCTION(mfxExtDecodedFrameInfo);
    DEFINE_DUMP_FUNCTION(mfxExtTimeCode);
    DEFINE_DUMP_FUNCTION(mfxExtPredWeightTable);
    DEFINE_DUMP_FUNCTION(mfxExtEncoderCapability);
    DEFINE_DUMP_FUNCTION(mfxExtDirtyRect);
    DEFINE_DUMP_FUNCTION(mfxExtMoveRect);
    DEFINE_DUMP_FUNCTION(mfxExtVPPFrameRateConversion);
    DEFINE_DUMP_FUNCTION(mfxExtVPPImageStab);
    DEFINE_DUMP_FUNCTION(mfxExtEncoderROI);
    DEFINE_DUMP_FUNCTION(mfxExtCodingOptionVPS);
    DEFINE_DUMP_FUNCTION(mfxExtVPPRotation);
    DEFINE_DUMP_FUNCTION(mfxExtEncodedSlicesInfo);
    DEFINE_DUMP_FUNCTION(mfxExtVPPScaling);
    DEFINE_DUMP_FUNCTION(mfxExtVPPMirroring);
    DEFINE_DUMP_FUNCTION(mfxExtMVOverPicBoundaries);
    DEFINE_DUMP_FUNCTION(mfxExtVPPColorFill);
    DEFINE_DUMP_FUNCTION(mfxExtDecVideoProcessing);
    DEFINE_DUMP_FUNCTION(mfxExtMBQP);
    DEFINE_DUMP_FUNCTION(mfxExtEncoderIPCMArea);
    DEFINE_DUMP_FUNCTION(mfxExtInsertHeaders);
    DEFINE_DUMP_FUNCTION(mfxExtDecodeErrorReport);
    DEFINE_DUMP_FUNCTION(mfxExtMasteringDisplayColourVolume);
    DEFINE_DUMP_FUNCTION(mfxExtContentLightLevelInfo);
    DEFINE_DUMP_FUNCTION(mfxExtEncodedUnitsInfo);
    DEFINE_DUMP_FUNCTION(mfxExtColorConversion);
    DEFINE_DUMP_FUNCTION(mfxExtVppMctf);
    DEFINE_DUMP_FUNCTION(mfxExtVP9Segmentation);
    DEFINE_DUMP_FUNCTION(mfxVP9SegmentParam);
    DEFINE_DUMP_FUNCTION(mfxExtVP9TemporalLayers);
    DEFINE_DUMP_FUNCTION(mfxVP9TemporalLayer);
    DEFINE_DUMP_FUNCTION(mfxExtVP9Param);
    DEFINE_DUMP_FUNCTION(LongTermRefList);
#if defined(ONEVPL_EXPERIMENTAL)
    DEFINE_DUMP_FUNCTION(mfxExtQualityInfoMode);
    DEFINE_DUMP_FUNCTION(mfxExtQualityInfoOutput);
    DEFINE_DUMP_FUNCTION(mfxExtAV1ScreenContentTools);
    DEFINE_DUMP_FUNCTION(mfxExtAlphaChannelEncCtrl);
    DEFINE_DUMP_FUNCTION(mfxExtAIEncCtrl);
#endif
};
#endif //_MFX_TRACE_DUMP_H_

