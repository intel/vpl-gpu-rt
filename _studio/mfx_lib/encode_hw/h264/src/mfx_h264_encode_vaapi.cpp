// Copyright (c) 2011-2022 Intel Corporation
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

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE)

#include <va/va.h>
#include <va/va_enc_h264.h>
#include "libmfx_core_vaapi.h"
#include "mfx_common_int.h"
#include "mfx_h264_encode_vaapi.h"
#include "mfx_h264_encode_hw_utils.h"

#if defined(_DEBUG)
//#define mdprintf fprintf
#define mdprintf(...)
#else
#define mdprintf(...)
#endif

using namespace MfxHwH264Encode;

mfxU32 ConvertMfxFourcc2VaapiRTFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
        case MFX_FOURCC_NV12:
            return VA_RT_FORMAT_YUV420;
        case MFX_FOURCC_YUY2:
            return VA_RT_FORMAT_YUV422;
        case MFX_FOURCC_AYUV:
            return VA_RT_FORMAT_YUV444;
        case MFX_FOURCC_RGB4:
        case MFX_FOURCC_BGR4:
            return VA_RT_FORMAT_RGB32;
        default: assert(!"Unsupported fourcc");
            return 0;
    }
}

static mfxU8 ConvertMfxFrameType2VaapiSliceType(mfxU8 type)
{
    switch (type & MFX_FRAMETYPE_IPB)
    {
    case MFX_FRAMETYPE_I:
    case MFX_FRAMETYPE_P:
    case MFX_FRAMETYPE_B:
        return ConvertMfxFrameType2SliceType(type) % 5;

    default:
        assert("bad codingType");
        return 0xff;
    }
}

uint32_t ConvertRateControlMFX2VAAPI(mfxU8 rateControl)
{
    switch (rateControl)
    {
    case MFX_RATECONTROL_CBR:  return VA_RC_CBR;
    case MFX_RATECONTROL_VBR:  return VA_RC_VBR;
    case MFX_RATECONTROL_AVBR: return VA_RC_AVBR;
#ifdef MFX_ENABLE_QVBR
    case MFX_RATECONTROL_QVBR: return VA_RC_QVBR;
#endif
    case MFX_RATECONTROL_CQP:  return VA_RC_CQP;
    case MFX_RATECONTROL_ICQ:  return VA_RC_ICQ;
    case MFX_RATECONTROL_VCM:  return VA_RC_VCM;
    default:                   return 0;
    }
}

VAProfile ConvertProfileTypeMFX2VAAPI(mfxU32 type)
{
    switch (type)
    {
        case MFX_PROFILE_AVC_CONSTRAINED_BASELINE:
        case MFX_PROFILE_AVC_BASELINE:
            return VAProfileH264ConstrainedBaseline;
        case MFX_PROFILE_AVC_MAIN:
            return VAProfileH264Main;
        case MFX_PROFILE_AVC_HIGH:
            return VAProfileH264High;
        default:
            //assert(!"Unsupported profile type");
            return VAProfileH264High; //VAProfileNone;
    }

} // VAProfile ConvertProfileTypeMFX2VAAPI(mfxU8 type)

SliceDividerType ConvertSliceStructureVAAPIToMFX(mfxU8 structure)
{
    if (structure & VA_ENC_SLICE_STRUCTURE_ARBITRARY_MACROBLOCKS)
        return SliceDividerType::ARBITRARY_MB_SLICE;
    if (structure & VA_ENC_SLICE_STRUCTURE_ARBITRARY_ROWS)
        return SliceDividerType::ARBITRARY_ROW_SLICE;
    if (structure & VA_ENC_SLICE_STRUCTURE_EQUAL_ROWS ||
        structure & VA_ENC_SLICE_STRUCTURE_EQUAL_MULTI_ROWS)
        return SliceDividerType::ROWSLICE;
    if (structure & VA_ENC_SLICE_STRUCTURE_POWER_OF_TWO_ROWS)
        return SliceDividerType::ROW2ROW;
    return SliceDividerType::ONESLICE;
}

mfxStatus SetHRD(
    MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & hrdBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterHRD *hrd_param;

    mfxStatus mfxSts = CheckAndDestroyVAbuffer(vaDisplay, hrdBuf_id);
    MFX_CHECK_STS(mfxSts);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaCreateBuffer");
        vaSts = vaCreateBuffer(vaDisplay,
            vaContextEncode,
            VAEncMiscParameterBufferType,
            sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterHRD),
            1,
            NULL,
            &hrdBuf_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
                 hrdBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeHRD;
    hrd_param = (VAEncMiscParameterHRD *)misc_param->data;

    hrd_param->initial_buffer_fullness = par.calcParam.initialDelayInKB * 8000;
    hrd_param->buffer_size = par.calcParam.bufferSizeInKB * 8000;

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        vaSts = vaUnmapBuffer(vaDisplay, hrdBuf_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
} // void SetHRD(...)

void FillBrcStructures(
    MfxVideoParam const & par,
    VAEncMiscParameterRateControl & vaBrcPar,
    VAEncMiscParameterFrameRate   & vaFrameRate)
{
    Zero(vaBrcPar);
    Zero(vaFrameRate);
    vaBrcPar.bits_per_second = GetMaxBitrateValue(par.calcParam.maxKbps) << (6 + SCALE_FROM_DRIVER);
    if(par.calcParam.maxKbps)
        vaBrcPar.target_percentage = (unsigned int)(100.0 * (mfxF64)par.calcParam.targetKbps / (mfxF64)par.calcParam.maxKbps);
    vaBrcPar.ICQ_quality_factor = par.mfx.ICQQuality;
    PackMfxFrameRate(par.mfx.FrameInfo.FrameRateExtN, par.mfx.FrameInfo.FrameRateExtD, vaFrameRate.framerate);
}

mfxStatus SetRateControl(
    MfxVideoParam const & par,
    mfxU32       mbbrc,
    mfxU8        minQP,
    mfxU8        maxQP,
    mfxU32       TargetFrameSize,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & rateParamBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterRateControl *rate_param;
#ifdef MFX_ENABLE_QVBR
    mfxExtCodingOption3 const & extOpt3 = GetExtBufferRef(par);
#endif

    mfxStatus mfxSts = CheckAndDestroyVAbuffer(vaDisplay, rateParamBuf_id);
    MFX_CHECK_STS(mfxSts);

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterRateControl),
                   1,
                   NULL,
                   &rateParamBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
                 rateParamBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeRateControl;
    rate_param = (VAEncMiscParameterRateControl *)misc_param->data;

    rate_param->bits_per_second = GetMaxBitrateValue(par.calcParam.maxKbps) << (6 + SCALE_FROM_DRIVER);
    rate_param->window_size     = par.mfx.Convergence * 100;

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        rate_param->window_size = par.mfx.Convergence;
        rate_param->bits_per_second = ((1000 * par.calcParam.targetKbps) >> (6 + SCALE_FROM_DRIVER)) << (6 + SCALE_FROM_DRIVER);
    }

    rate_param->min_qp = minQP;
    rate_param->max_qp = maxQP;
#if VA_CHECK_VERSION(1, 10, 0)
    rate_param->target_frame_size = TargetFrameSize;
#endif
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
        rate_param->ICQ_quality_factor = par.mfx.ICQQuality;
#ifdef MFX_ENABLE_QVBR
    else if (par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
        rate_param->quality_factor = extOpt3.QVBRQuality;
#endif

    if(par.calcParam.maxKbps)
        rate_param->target_percentage = (unsigned int)((mfxU64)par.calcParam.targetKbps * 100 / par.calcParam.maxKbps);

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        rate_param->target_percentage = par.mfx.Accuracy;
    }

    rate_param->rc_flags.bits.frame_tolerance_mode = ConvertLowDelayBRCMfx2Ddi(extOpt3.LowDelayBRC, par.calcParam.TCBRCTargetFrameSize);
    // Activate frame tolerance sliding window mode
    if (extOpt3.WinBRCSize)
    {
        rate_param->rc_flags.bits.frame_tolerance_mode = eFrameSizeTolerance_Low;
    }

    //  MBBRC control
    // Control VA_RC_MB 0: default, 1: enable, 2: disable, other: reserved
    rate_param->rc_flags.bits.mb_rate_control = mbbrc & 0xf;

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        vaSts = vaUnmapBuffer(vaDisplay, rateParamBuf_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

mfxStatus SetBrcResetRequired(
    VADisplay             vaDisplay,
    VABufferID            rateParamBuf_id,
    bool                  isBrcResetRequired)
{
    if (rateParamBuf_id == VA_INVALID_ID)
        return MFX_ERR_NONE; // if buf wasn't created

    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterRateControl *rate_param;

    vaSts = vaMapBuffer(vaDisplay,
                        rateParamBuf_id,
                        (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    rate_param = (VAEncMiscParameterRateControl *)misc_param->data;
    rate_param->rc_flags.bits.reset = isBrcResetRequired;

    vaSts = vaUnmapBuffer(vaDisplay,
                          rateParamBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    return MFX_ERR_NONE;
}

mfxStatus SetFrameRate(
    MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & frameRateBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterFrameRate *frameRate_param;

    mfxStatus mfxSts = CheckAndDestroyVAbuffer(vaDisplay, frameRateBuf_id);
    MFX_CHECK_STS(mfxSts);

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterFrameRate),
                   1,
                   NULL,
                   &frameRateBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
                 frameRateBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeFrameRate;

    frameRate_param = (VAEncMiscParameterFrameRate *)misc_param->data;
    PackMfxFrameRate(par.mfx.FrameInfo.FrameRateExtN, par.mfx.FrameInfo.FrameRateExtD, frameRate_param->framerate);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        vaSts = vaUnmapBuffer(vaDisplay, frameRateBuf_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

static mfxStatus SetMaxSliceSize(
    const UINT   userMaxSliceSize,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & maxSliceSizeBuf_id)
{
    mfxStatus sts = CheckAndDestroyVAbuffer(vaDisplay, maxSliceSizeBuf_id);
    MFX_CHECK_STS(sts);

    VAStatus vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterMaxSliceSize),
                   1,
                   nullptr,
                   &maxSliceSizeBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    VAEncMiscParameterBuffer *misc_param;
    vaSts = vaMapBuffer(vaDisplay,
                 maxSliceSizeBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeMaxSliceSize;
    auto maxSliceSize_param = reinterpret_cast<VAEncMiscParameterMaxSliceSize *>(misc_param->data);

    maxSliceSize_param->max_slice_size = userMaxSliceSize;

    vaSts = vaUnmapBuffer(vaDisplay, maxSliceSizeBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

static mfxStatus SetMaxFrameSize(
    const UINT   userMaxFrameSize,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & frameSizeBuf_id)
{
    VAEncMiscParameterBuffer             *misc_param;
    VAEncMiscParameterBufferMaxFrameSize *p_maxFrameSize;

    mfxStatus mfxSts = CheckAndDestroyVAbuffer(vaDisplay, frameSizeBuf_id);
    MFX_CHECK_STS(mfxSts);

    VAStatus vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterBufferMaxFrameSize),
                   1,
                   NULL,
                   &frameSizeBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay, frameSizeBuf_id, (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeMaxFrameSize;
    p_maxFrameSize = (VAEncMiscParameterBufferMaxFrameSize *)misc_param->data;

    p_maxFrameSize->max_frame_size = userMaxFrameSize*8;    // in bits for libva

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        vaSts = vaUnmapBuffer(vaDisplay, frameSizeBuf_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

#if VA_CHECK_VERSION(1,3,0)
static mfxStatus SetMultiPassFrameSize( DdiTask const & task,
    mfxU32       /*fieldId*/,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & frameSizeBuf_id)
{
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterBufferMultiPassFrameSize *p_multiPassFrameSize;

    mfxStatus mfxSts = CheckAndDestroyVAbuffer(vaDisplay, frameSizeBuf_id);
    MFX_CHECK_STS(mfxSts);

    VAStatus vaSts = vaCreateBuffer(vaDisplay,
        vaContextEncode,
        VAEncMiscParameterBufferType,
        sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterBufferMultiPassFrameSize),
        1,
        NULL,
        &frameSizeBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
        vaSts = vaMapBuffer(vaDisplay, frameSizeBuf_id, (void **)&misc_param);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    misc_param->type = VAEncMiscParameterTypeMultiPassFrameSize;
    p_multiPassFrameSize = (VAEncMiscParameterBufferMultiPassFrameSize *)misc_param->data;

    p_multiPassFrameSize->max_frame_size = task.m_brcFrameCtrl.MaxFrameSize;
    p_multiPassFrameSize->num_passes = task.m_brcFrameCtrl.MaxNumRepak;
    p_multiPassFrameSize->delta_qp = (unsigned char *)task.m_brcFrameCtrl.DeltaQP;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        vaSts = vaUnmapBuffer(vaDisplay, frameSizeBuf_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    return MFX_ERR_NONE;
}
#endif

#if !defined(ANDROID)
static mfxStatus SetTrellisQuantization(
    mfxU32       trellis,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & trellisBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer        *misc_param;
    VAEncMiscParameterQuantization  *trellis_param;

    mfxStatus mfxSts = CheckAndDestroyVAbuffer(vaDisplay, trellisBuf_id);
    MFX_CHECK_STS(mfxSts);

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterQuantization),
                   1,
                   NULL,
                   &trellisBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay, trellisBuf_id, (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeQuantization;
    trellis_param = (VAEncMiscParameterQuantization *)misc_param->data;

    trellis_param->quantization_flags.value = trellis;

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        vaSts = vaUnmapBuffer(vaDisplay, trellisBuf_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
} // void SetTrellisQuantization(...)
#endif

static mfxStatus SetRollingIntraRefresh(
    IntraRefreshState const & rirState,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & rirBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterRIR    *rir_param;

    mfxStatus mfxSts = CheckAndDestroyVAbuffer(vaDisplay, rirBuf_id);
    MFX_CHECK_STS(mfxSts);

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterRIR),
                   1,
                   NULL,
                   &rirBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay, rirBuf_id, (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeRIR;
    rir_param = (VAEncMiscParameterRIR *)misc_param->data;

    rir_param->rir_flags.value             = rirState.refrType;
    rir_param->intra_insertion_location    = rirState.IntraLocation;
    rir_param->intra_insert_size           = rirState.IntraSize;
    rir_param->qp_delta_for_inserted_intra = mfxU8(rirState.IntRefQPDelta);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        vaSts = vaUnmapBuffer(vaDisplay, rirBuf_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
} // void SetRollingIntraRefresh(...)

mfxStatus SetQualityParams(
    MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & qualityParams_id,
    DdiTask const * pTask)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterEncQuality *quality_param;
    mfxExtCodingOption2 const * extOpt2  = GetExtBuffer(par);
    mfxExtCodingOption3 const * extOpt3  = GetExtBuffer(par);

    mfxStatus mfxSts = CheckAndDestroyVAbuffer(vaDisplay, qualityParams_id);
    MFX_CHECK_STS(mfxSts);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaCreateBuffer");

        vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterEncQuality),
                   1,
                   NULL,
                   &qualityParams_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
        vaSts = vaMapBuffer(vaDisplay,
                 qualityParams_id,
                (void **)&misc_param);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = (VAEncMiscParameterType)VAEncMiscParameterTypeEncQuality;
    quality_param = (VAEncMiscParameterEncQuality *)misc_param->data;

    quality_param->useRawPicForRef = extOpt2 && IsOn(extOpt2->UseRawRef);

    if (extOpt3)
    {
        quality_param->directBiasAdjustmentEnable       = IsOn(extOpt3->DirectBiasAdjustment);
        quality_param->globalMotionBiasAdjustmentEnable = IsOn(extOpt3->GlobalMotionBiasAdjustment);

        if (quality_param->globalMotionBiasAdjustmentEnable && extOpt3->MVCostScalingFactor < 4)
            quality_param->HMEMVCostScalingFactor = extOpt3->MVCostScalingFactor;

        quality_param->PanicModeDisable = IsOff(extOpt3->BRCPanicMode);

#ifdef MFX_ENABLE_H264_REPARTITION_CHECK
        switch (extOpt3->RepartitionCheckEnable)
        {
        case MFX_CODINGOPTION_ON:
            quality_param->ForceRepartitionCheck = 1;
            break;
        case MFX_CODINGOPTION_OFF:
            quality_param->ForceRepartitionCheck = 2;
            break;
        case MFX_CODINGOPTION_UNKNOWN:
        case MFX_CODINGOPTION_ADAPTIVE:
        default:
            quality_param->ForceRepartitionCheck = 0;
        }
#endif // MFX_ENABLE_H264_REPARTITION_CHECK

    }

    if (pTask)
    {
        mfxExtCodingOption3 const * extOpt3rt  = GetExtBuffer(pTask->m_ctrl);

        quality_param->useRawPicForRef = pTask->m_isUseRawRef;

        if (extOpt3rt)
        {
            quality_param->directBiasAdjustmentEnable       = IsOn(extOpt3rt->DirectBiasAdjustment);
            quality_param->globalMotionBiasAdjustmentEnable = IsOn(extOpt3rt->GlobalMotionBiasAdjustment);

            if (quality_param->globalMotionBiasAdjustmentEnable && extOpt3rt->MVCostScalingFactor < 4)
                quality_param->HMEMVCostScalingFactor = extOpt3rt->MVCostScalingFactor;

#ifdef MFX_ENABLE_H264_REPARTITION_CHECK
            switch (extOpt3rt->RepartitionCheckEnable)
            {
            case MFX_CODINGOPTION_ON:
                quality_param->ForceRepartitionCheck = 1;
                break;
            case MFX_CODINGOPTION_OFF:
                quality_param->ForceRepartitionCheck = 2;
                break;
            case MFX_CODINGOPTION_UNKNOWN:
                // lets stick with option specified on init if not specified per-frame
                break;
            case MFX_CODINGOPTION_ADAPTIVE:
            default:
                quality_param->ForceRepartitionCheck = 0;
            }
#endif // MFX_ENABLE_H264_REPARTITION_CHECK

        } // if (extOpt3rt)

#ifdef MFX_ENABLE_H264_PRIVATE_CTRL
        mfxExtAVCEncodeCtrl const * extPCQC    = GetExtBuffer(pTask->m_ctrl);
        if (extPCQC)
        {
            quality_param->skipCheckDisable = (extPCQC->SkipCheck & 0x0F) == MFX_SKIP_CHECK_DISABLE;
            quality_param->FTQEnable        = (extPCQC->SkipCheck & 0x0F) == MFX_SKIP_CHECK_FTQ_ON;
            quality_param->FTQOverride      = quality_param->FTQEnable || (extPCQC->SkipCheck & 0x0F) == MFX_SKIP_CHECK_FTQ_OFF;

            if (extPCQC->SkipCheck & MFX_SKIP_CHECK_SET_THRESHOLDS)
            {
                if (quality_param->FTQEnable)
                {
                    quality_param->FTQSkipThresholdLUTInput = 1;
                    for (mfxU32 i = 0; i < 52; i ++)
                        quality_param->FTQSkipThresholdLUT[i] = (mfxU8)extPCQC->SkipThreshold[i];
                }
                else
                {
                    quality_param->NonFTQSkipThresholdLUTInput = 1;
                    Copy(quality_param->NonFTQSkipThresholdLUT, extPCQC->SkipThreshold);
                }
            }
        }
#endif
    }

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        vaSts = vaUnmapBuffer(vaDisplay, qualityParams_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
} // void SetQualityParams(...)

mfxStatus SetQualityLevel(
    MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & qualityParams_id,
    mfxEncodeCtrl const * /* pCtrl */)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterBufferQualityLevel *quality_param;

    mfxStatus mfxSts = CheckAndDestroyVAbuffer(vaDisplay, qualityParams_id);
    MFX_CHECK_STS(mfxSts);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaCreateBuffer");
        vaSts = vaCreateBuffer(vaDisplay,
            vaContextEncode,
            VAEncMiscParameterBufferType,
            sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterBufferQualityLevel),
            1,
            NULL,
            &qualityParams_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
        vaSts = vaMapBuffer(vaDisplay,
            qualityParams_id,
            (void **)&misc_param);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = (VAEncMiscParameterType)VAEncMiscParameterTypeQualityLevel;
    quality_param = (VAEncMiscParameterBufferQualityLevel *)misc_param->data;

    quality_param->quality_level = (unsigned int)(par.mfx.TargetUsage);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        vaSts = vaUnmapBuffer(vaDisplay, qualityParams_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
} // SetQualityLevel

mfxStatus SetSkipFrame(
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & skipParam_id,
    mfxU8 skipFlag,
    mfxU8 numSkipFrames,
    mfxU32 sizeSkipFrames)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterSkipFrame *skipParam;

    mfxStatus mfxSts = CheckAndDestroyVAbuffer(vaDisplay, skipParam_id);
    MFX_CHECK_STS(mfxSts);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaCreateBuffer");
        vaSts = vaCreateBuffer(vaDisplay,
            vaContextEncode,
            VAEncMiscParameterBufferType,
            sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterSkipFrame),
            1,
            NULL,
            &skipParam_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
        vaSts = vaMapBuffer(vaDisplay,
            skipParam_id,
            (void **)&misc_param);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = (VAEncMiscParameterType)VAEncMiscParameterTypeSkipFrame;
    skipParam = (VAEncMiscParameterSkipFrame *)misc_param->data;

    skipParam->skip_frame_flag  = skipFlag;
    skipParam->num_skip_frames  = numSkipFrames;
    skipParam->size_skip_frames = sizeSkipFrames;

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        vaSts = vaUnmapBuffer(vaDisplay, skipParam_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

static mfxStatus SetROI(
    DdiTask const & task,
    std::vector<VAEncROI> & arrayVAEncROI,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & roiParam_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;

    VAEncMiscParameterBufferROI *roi_Param;
    unsigned int roi_buffer_size = sizeof(VAEncMiscParameterBufferROI);

    mfxStatus mfxSts = CheckAndDestroyVAbuffer(vaDisplay, roiParam_id);
    MFX_CHECK_STS(mfxSts);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaCreateBuffer");
        vaSts = vaCreateBuffer(vaDisplay,
            vaContextEncode,
            VAEncMiscParameterBufferType,
            sizeof(VAEncMiscParameterBuffer) + roi_buffer_size,
            1,
            NULL,
            &roiParam_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
        vaSts = vaMapBuffer(vaDisplay,
            roiParam_id,
            (void **)&misc_param);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = (VAEncMiscParameterType)VAEncMiscParameterTypeROI;
    roi_Param = (VAEncMiscParameterBufferROI *)misc_param->data;
    memset(roi_Param, 0, roi_buffer_size);

    if (task.m_numRoi)
    {
        roi_Param->num_roi = task.m_numRoi;

        if (arrayVAEncROI.size() < task.m_numRoi)
        {
            arrayVAEncROI.resize(task.m_numRoi);
        }
        roi_Param->roi = Begin(arrayVAEncROI);
        memset(roi_Param->roi, 0, task.m_numRoi * sizeof(VAEncROI));

        for (mfxU32 i = 0; i < task.m_numRoi; i++)
        {
            roi_Param->roi[i].roi_rectangle.x = task.m_roi[i].Left;
            roi_Param->roi[i].roi_rectangle.y = task.m_roi[i].Top;
            roi_Param->roi[i].roi_rectangle.width  = task.m_roi[i].Right  - task.m_roi[i].Left;
            roi_Param->roi[i].roi_rectangle.height = task.m_roi[i].Bottom - task.m_roi[i].Top;
            roi_Param->roi[i].roi_value = task.m_roi[i].ROIValue;
        }
        roi_Param->max_delta_qp = 51;
        roi_Param->min_delta_qp = -51;

        roi_Param->roi_flags.bits.roi_value_is_qp_delta = 0;
        if (task.m_roiMode == MFX_ROI_MODE_QP_DELTA) {
            roi_Param->roi_flags.bits.roi_value_is_qp_delta = 1;
        }
    }

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        vaSts = vaUnmapBuffer(vaDisplay, roiParam_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);


    return MFX_ERR_NONE;
}

#if defined (MFX_ENABLE_H264_ROUNDING_OFFSET)
mfxStatus SetRoundingOffset(
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    mfxExtAVCRoundingOffset const & roundingOffset,
    VABufferID & roundingOffsetBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterCustomRoundingControl *rounding_param;

    mfxStatus mfxSts = CheckAndDestroyVAbuffer(vaDisplay, roundingOffsetBuf_id);
    MFX_CHECK_STS(mfxSts);

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterCustomRoundingControl),
                   1,
                   NULL,
                   &roundingOffsetBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
                 roundingOffsetBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = (VAEncMiscParameterType)VAEncMiscParameterTypeCustomRoundingControl;

    rounding_param = (VAEncMiscParameterCustomRoundingControl *)misc_param->data;
    if (IsOn(roundingOffset.EnableRoundingIntra))
    {
        rounding_param->rounding_offset_setting.bits.enable_custom_rouding_intra  = 1;
        rounding_param->rounding_offset_setting.bits.rounding_offset_intra        = roundingOffset.RoundingOffsetIntra;
    }
    else
    {
        rounding_param->rounding_offset_setting.bits.enable_custom_rouding_intra  = 0;
        rounding_param->rounding_offset_setting.bits.rounding_offset_intra        = 0;
    }

    if (IsOn(roundingOffset.EnableRoundingInter))
    {
        rounding_param->rounding_offset_setting.bits.enable_custom_rounding_inter = 1;
        rounding_param->rounding_offset_setting.bits.rounding_offset_inter        = roundingOffset.RoundingOffsetInter;
    }
    else
    {
        rounding_param->rounding_offset_setting.bits.enable_custom_rounding_inter = 0;
        rounding_param->rounding_offset_setting.bits.rounding_offset_inter        = 0;
    }

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        vaSts = vaUnmapBuffer(vaDisplay, roundingOffsetBuf_id);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}
#endif

void FillConstPartOfPps(
    MfxVideoParam const & par,
    VAEncPictureParameterBufferH264 & pps)
{
    mfxExtPpsHeader const *       extPps = GetExtBuffer(par);
    mfxExtSpsHeader const *       extSps = GetExtBuffer(par);
    assert( extPps != 0 );
    assert( extSps != 0 );

    if (!extPps || !extSps)
        return;

    pps.seq_parameter_set_id = 0; // extSps->seqParameterSetId; - driver doesn't support non-zero sps_id
    pps.pic_parameter_set_id = 0; // extPps->picParameterSetId; - driver doesn't support non-zero pps_id

    pps.last_picture = 0;
    pps.frame_num = 0;

    pps.pic_init_qp = extPps->picInitQpMinus26 + 26;
    pps.num_ref_idx_l0_active_minus1            = extPps->numRefIdxL0DefaultActiveMinus1;
    pps.num_ref_idx_l1_active_minus1            = extPps->numRefIdxL1DefaultActiveMinus1;
    pps.chroma_qp_index_offset                  = extPps->chromaQpIndexOffset;
    pps.second_chroma_qp_index_offset           = extPps->secondChromaQpIndexOffset;

    pps.pic_fields.bits.deblocking_filter_control_present_flag  = 1;
    pps.pic_fields.bits.entropy_coding_mode_flag                = extPps->entropyCodingModeFlag;

    pps.pic_fields.bits.pic_order_present_flag                  = extPps->bottomFieldPicOrderInframePresentFlag;
    pps.pic_fields.bits.weighted_pred_flag                      = extPps->weightedPredFlag;
    pps.pic_fields.bits.weighted_bipred_idc                     = extPps->weightedBipredIdc;
    pps.pic_fields.bits.constrained_intra_pred_flag             = extPps->constrainedIntraPredFlag;
    pps.pic_fields.bits.transform_8x8_mode_flag                 = extPps->transform8x8ModeFlag;
    pps.pic_fields.bits.pic_scaling_matrix_present_flag         = extPps->picScalingMatrixPresentFlag;

    mfxU32 i = 0;
    for( i = 0; i < 16; i++ )
    {
        pps.ReferenceFrames[i].picture_id = VA_INVALID_ID;
    }

} // void FillConstPartOfPps(...)
namespace MfxHwH264Encode
{
void UpdatePPS(
    DdiTask const & task,
    mfxU32          fieldId,
    VAEncPictureParameterBufferH264 & pps,
    std::vector<ExtVASurface> const & reconQueue
#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
    , mfxU32                          cqmPpsNum
#endif
)
{
    pps.frame_num = task.m_frameNum;

    pps.pic_fields.bits.idr_pic_flag       = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) ? 1 : 0;
    pps.pic_fields.bits.reference_pic_flag = (task.m_type[fieldId] & MFX_FRAMETYPE_REF) ? 1 : 0;

    pps.CurrPic.TopFieldOrderCnt    = task.GetPoc(TFIELD);
    pps.CurrPic.BottomFieldOrderCnt = task.GetPoc(BFIELD);
    if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
        pps.CurrPic.flags = TFIELD == fieldId ? VA_PICTURE_H264_TOP_FIELD : VA_PICTURE_H264_BOTTOM_FIELD;
    else
        pps.CurrPic.flags = 0;

    mfxU32 i   = 0;
    mfxU32 idx = 0;

    for( i = 0; i < task.m_dpb[fieldId].Size(); i++ )
    {
        pps.ReferenceFrames[i].frame_idx = idx     = task.m_dpb[fieldId][i].m_frameIdx & 0x7f;
        pps.ReferenceFrames[i].picture_id          = reconQueue[idx].surface;
        pps.ReferenceFrames[i].flags               = task.m_dpb[fieldId][i].m_longterm ? VA_PICTURE_H264_LONG_TERM_REFERENCE : VA_PICTURE_H264_SHORT_TERM_REFERENCE;
        pps.ReferenceFrames[i].TopFieldOrderCnt    = task.m_dpb[fieldId][i].m_poc[0];
        pps.ReferenceFrames[i].BottomFieldOrderCnt = task.m_dpb[fieldId][i].m_poc[1];
    }

    for (; i < 16; i++)
    {
        pps.ReferenceFrames[i].picture_id          = VA_INVALID_ID;
        pps.ReferenceFrames[i].frame_idx           = 0xff;
        pps.ReferenceFrames[i].flags               = VA_PICTURE_H264_INVALID;
        pps.ReferenceFrames[i].TopFieldOrderCnt    = 0;
        pps.ReferenceFrames[i].BottomFieldOrderCnt = 0;
    }

#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
    if (IsCustMatrix(task.m_adaptiveCQMHint) && cqmPpsNum > 0)
    {
        pps.pic_parameter_set_id = static_cast<uint8_t>(task.m_adaptiveCQMHint);
        pps.pic_fields.bits.pic_scaling_matrix_present_flag = true;
    }
    else
    {   // CqmHint is CQM_HINT_USE_FLAT_MATRIX or CQM_HINT_INVALID, or no available extended CQM pps
        pps.pic_parameter_set_id = 0;
        pps.pic_fields.bits.pic_scaling_matrix_present_flag = false;
    }
#endif
} // void UpdatePPS(...)

void FillPWT(
    MFX_ENCODE_CAPS const &                     hwCaps,
    VAEncPictureParameterBufferH264 const &     pps,
    mfxExtPredWeightTable const &               pwt,
    VAEncSliceParameterBufferH264 &             slice)
{
    mfxU32 iNumRefL0 = 0;
    mfxU32 iNumRefL1 = 0;
    const mfxU32 iWeight = 0, iOffset = 1, iY = 0, iCb = 1, iCr = 2, iCbVa = 0, iCrVa = 1;
    mfxU32 ref = 0;

    // check parameter
    if (!((pps.pic_fields.bits.weighted_pred_flag == 1 && slice.slice_type % 5 == SLICE_TYPE_P) ||
          (pps.pic_fields.bits.weighted_bipred_idc == 1 && slice.slice_type % 5 == SLICE_TYPE_B)))
        return;

    iNumRefL0 = hwCaps.ddi_caps.MaxNum_WeightedPredL0 < slice.num_ref_idx_l0_active_minus1 + 1 ? hwCaps.ddi_caps.MaxNum_WeightedPredL0 : slice.num_ref_idx_l0_active_minus1 + 1;
    iNumRefL1 = hwCaps.ddi_caps.MaxNum_WeightedPredL1 < slice.num_ref_idx_l1_active_minus1 + 1 ? hwCaps.ddi_caps.MaxNum_WeightedPredL1 : slice.num_ref_idx_l1_active_minus1 + 1;

    // initialize
    Zero(slice.luma_log2_weight_denom);
    Zero(slice.chroma_log2_weight_denom);
    Zero(slice.luma_weight_l0_flag);
    Zero(slice.luma_weight_l1_flag);
    Zero(slice.chroma_weight_l0_flag);
    Zero(slice.chroma_weight_l1_flag);
    Zero(slice.luma_weight_l0);
    Zero(slice.luma_weight_l1);
    Zero(slice.luma_offset_l0);
    Zero(slice.luma_offset_l1);
    Zero(slice.chroma_weight_l0);
    Zero(slice.chroma_weight_l1);
    Zero(slice.chroma_offset_l0);
    Zero(slice.chroma_offset_l1);

    slice.luma_log2_weight_denom = (mfxU8)pwt.LumaLog2WeightDenom;
    slice.chroma_log2_weight_denom = (mfxU8)pwt.ChromaLog2WeightDenom;

    if (hwCaps.ddi_caps.LumaWeightedPred)
    {
        // Set Luma L0
        if ((slice.slice_type % 5) == SLICE_TYPE_P || (slice.slice_type % 5) == SLICE_TYPE_B)
        {
            for (ref = 0; ref < iNumRefL0; ref++)
            {
                if (pwt.LumaWeightFlag[0][ref])
                {
                    slice.luma_weight_l0_flag |= 1 << ref;
                    Copy(slice.luma_weight_l0[ref], pwt.Weights[0][ref][iY][iWeight]);
                    Copy(slice.luma_offset_l0[ref], pwt.Weights[0][ref][iY][iOffset]);
                }
                else
                {
                    slice.luma_weight_l0[ref] = (1 << slice.luma_log2_weight_denom);
                    slice.luma_offset_l0[ref] = 0;
                }
            }
        }

        // Set Luma L1
        if ((slice.slice_type % 5) == SLICE_TYPE_B)
        {
            for (ref = 0; ref < iNumRefL1; ref++)
            {
                if (pwt.LumaWeightFlag[1][ref])
                {
                    slice.luma_weight_l1_flag |= 1 << ref;
                    Copy(slice.luma_weight_l1[ref], pwt.Weights[1][ref][iY][iWeight]);
                    Copy(slice.luma_offset_l1[ref], pwt.Weights[1][ref][iY][iOffset]);
                }
                else
                {
                    slice.luma_weight_l1[ref] = (1 << slice.luma_log2_weight_denom);
                    slice.luma_offset_l1[ref] = 0;
                }
            }
        }
    }

    if (hwCaps.ddi_caps.ChromaWeightedPred)
    {
        // Set Chroma L0
        if ((slice.slice_type % 5) == SLICE_TYPE_P || (slice.slice_type % 5) == SLICE_TYPE_B)
        {
            for (ref = 0; ref < iNumRefL0; ref++)
            {
                if (pwt.ChromaWeightFlag[0][ref])
                {
                    slice.chroma_weight_l0_flag |= 1 << ref;
                    Copy(slice.chroma_weight_l0[ref][iCbVa], pwt.Weights[0][ref][iCb][iWeight]);
                    Copy(slice.chroma_weight_l0[ref][iCrVa], pwt.Weights[0][ref][iCr][iWeight]);
                    Copy(slice.chroma_offset_l0[ref][iCbVa], pwt.Weights[0][ref][iCb][iOffset]);
                    Copy(slice.chroma_offset_l0[ref][iCrVa], pwt.Weights[0][ref][iCr][iOffset]);
                }
                else
                {
                    slice.chroma_weight_l0[ref][iCbVa] = (1 << slice.chroma_log2_weight_denom);
                    slice.chroma_weight_l0[ref][iCrVa] = (1 << slice.chroma_log2_weight_denom);
                    slice.chroma_offset_l0[ref][iCbVa] = 0;
                    slice.chroma_offset_l0[ref][iCrVa] = 0;
                }
            }
        }

        // Set Chroma L1
        if ((slice.slice_type % 5) == SLICE_TYPE_B)
        {
            for (ref = 0; ref < iNumRefL1; ref++)
            {
                if (pwt.ChromaWeightFlag[1][ref])
                {
                    slice.chroma_weight_l1_flag |= 1 << ref;
                    Copy(slice.chroma_weight_l1[ref][iCbVa], pwt.Weights[1][ref][iCb][iWeight]);
                    Copy(slice.chroma_weight_l1[ref][iCrVa], pwt.Weights[1][ref][iCr][iWeight]);
                    Copy(slice.chroma_offset_l1[ref][iCbVa], pwt.Weights[1][ref][iCb][iOffset]);
                    Copy(slice.chroma_offset_l1[ref][iCrVa], pwt.Weights[1][ref][iCr][iOffset]);
                }
                else
                {
                    slice.chroma_weight_l1[ref][iCbVa] = (1 << slice.chroma_log2_weight_denom);
                    slice.chroma_weight_l1[ref][iCrVa] = (1 << slice.chroma_log2_weight_denom);
                    slice.chroma_offset_l1[ref][iCbVa] = 0;
                    slice.chroma_offset_l1[ref][iCrVa] = 0;
                }
            }
        }
    }
}

    void UpdateSlice(
        MFX_ENCODE_CAPS const &                     hwCaps,
        DdiTask const &                             task,
        mfxU32                                      fieldId,
        VAEncSequenceParameterBufferH264 const     & sps,
        VAEncPictureParameterBufferH264 const      & pps,
        std::vector<VAEncSliceParameterBufferH264> & slice,
        MfxVideoParam const                        & par,
        std::vector<ExtVASurface> const & reconQueue)
    {
        mfxU32 numPics  = task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
        if (task.m_numSlice[fieldId])
            slice.resize(task.m_numSlice[fieldId]);
        mfxU32 numSlice = slice.size();
        mfxU32 idx = 0, ref = 0;

        // In case of single-field processing, only one buffer is attached
        mfxU32 idxToPickBuffer = task.m_singleFieldMode ? 0 : task.m_fid[fieldId];

        mfxExtCodingOptionDDI * extDdi      = GetExtBuffer(par);
        mfxExtCodingOption2   & extOpt2     = GetExtBufferRef(par);
        assert(extDdi      != 0);

        mfxExtPredWeightTable const * pPWT = GetExtBuffer(task.m_ctrl, idxToPickBuffer);
#ifdef MFX_ENABLE_FADE_DETECTION
        if (!pPWT)
            pPWT = &task.m_pwt[fieldId];
#endif
        SliceDivider divider = MakeSliceDivider(
            SliceDividerType(hwCaps.ddi_caps.SliceStructure),
            task.m_numMbPerSlice,
            numSlice,
            sps.picture_width_in_mbs,
            sps.picture_height_in_mbs / numPics);

        ArrayDpbFrame const & dpb = task.m_dpb[fieldId];
        ArrayU8x33 const & list0 = task.m_list0[fieldId];
        ArrayU8x33 const & list1 = task.m_list1[fieldId];

        for( size_t i = 0; i < slice.size(); ++i, divider.Next() )
        {
            slice[i].macroblock_address = divider.GetFirstMbInSlice();
            slice[i].num_macroblocks    = divider.GetNumMbInSlice();
            slice[i].macroblock_info    = VA_INVALID_ID;

            for (ref = 0; ref < list0.Size(); ref++)
            {
                slice[i].RefPicList0[ref].frame_idx    = idx  = dpb[list0[ref] & 0x7f].m_frameIdx & 0x7f;
                slice[i].RefPicList0[ref].picture_id          = reconQueue[idx].surface;
                if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
                    slice[i].RefPicList0[ref].flags               = list0[ref] >> 7 ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
            }
            for (; ref < 32; ref++)
            {
                slice[i].RefPicList0[ref].picture_id = VA_INVALID_ID;
                slice[i].RefPicList0[ref].flags      = VA_PICTURE_H264_INVALID;
            }

            for (ref = 0; ref < list1.Size(); ref++)
            {
                slice[i].RefPicList1[ref].frame_idx    = idx  = dpb[list1[ref] & 0x7f].m_frameIdx & 0x7f;
                slice[i].RefPicList1[ref].picture_id          = reconQueue[idx].surface;
                if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
                    slice[i].RefPicList1[ref].flags               = list1[ref] >> 7 ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
            }
            for (; ref < 32; ref++)
            {
                slice[i].RefPicList1[ref].picture_id = VA_INVALID_ID;
                slice[i].RefPicList1[ref].flags      = VA_PICTURE_H264_INVALID;
            }

            slice[i].pic_parameter_set_id = pps.pic_parameter_set_id;
            slice[i].slice_type = ConvertMfxFrameType2VaapiSliceType( task.m_type[fieldId] );

            slice[i].direct_spatial_mv_pred_flag = 1;

            slice[i].num_ref_idx_l0_active_minus1 = std::max<mfxU8>(1, task.m_list0[fieldId].Size()) - 1;
            slice[i].num_ref_idx_l1_active_minus1 = std::max<mfxU8>(1, task.m_list1[fieldId].Size()) - 1;
            slice[i].num_ref_idx_active_override_flag   =
                        slice[i].num_ref_idx_l0_active_minus1 != pps.num_ref_idx_l0_active_minus1 ||
                        slice[i].num_ref_idx_l1_active_minus1 != pps.num_ref_idx_l1_active_minus1;

            slice[i].idr_pic_id        = task.m_idrPicId;
            slice[i].pic_order_cnt_lsb = mfxU16(task.GetPoc(fieldId));

            slice[i].delta_pic_order_cnt_bottom         = 0;
            slice[i].delta_pic_order_cnt[0]             = 0;
            slice[i].delta_pic_order_cnt[1]             = 0;
            slice[i].luma_log2_weight_denom             = 0;
            slice[i].chroma_log2_weight_denom           = 0;

            slice[i].cabac_init_idc                     = extDdi ? (mfxU8)extDdi->CabacInitIdcPlus1 - 1 : 0;
            slice[i].slice_qp_delta                     = mfxI8(task.m_cqpValue[fieldId] - pps.pic_init_qp);

            {
                slice[i].disable_deblocking_filter_idc = extOpt2.DisableDeblockingIdc;
                slice[i].slice_alpha_c0_offset_div2    = 0;
                slice[i].slice_beta_offset_div2        = 0;
            }

            if (pPWT)
                FillPWT(hwCaps, pps, *pPWT, slice[i]);
        }

    } // void UpdateSlice(...)
} // namespace MfxHwH264Encode

void UpdateSliceSizeLimited(
    MFX_ENCODE_CAPS const &                     hwCaps,
    DdiTask const &                             task,
    mfxU32                                      fieldId,
    VAEncSequenceParameterBufferH264 const     & sps,
    VAEncPictureParameterBufferH264 const      & pps,
    std::vector<VAEncSliceParameterBufferH264> & slice,
    MfxVideoParam const                        & par,
    std::vector<ExtVASurface> const & reconQueue)
{
    (void)sps;

    mfxU32 idx = 0, ref = 0;

    mfxExtCodingOptionDDI * extDdi      = GetExtBuffer(par);
    mfxExtCodingOption2   & extOpt2     = GetExtBufferRef(par);
    assert(extDdi      != 0);

    mfxExtPredWeightTable const * pPWT = GetExtBuffer(task.m_ctrl, task.m_fid[fieldId]);
#ifdef MFX_ENABLE_FADE_DETECTION
    if (!pPWT)
        pPWT = &task.m_pwt[fieldId];
#endif
    size_t numSlices = task.m_SliceInfo.size();
    if (numSlices != slice.size())
    {
        size_t old_size = slice.size();
        slice.resize(numSlices);
        for (size_t i = old_size; i < slice.size(); ++i)
        {
            slice[i] = slice[0];
            //slice[i].slice_id = (mfxU32)i;
        }
    }

    ArrayDpbFrame const & dpb = task.m_dpb[fieldId];
    ArrayU8x33 const & list0 = task.m_list0[fieldId];
    ArrayU8x33 const & list1 = task.m_list1[fieldId];

    for( size_t i = 0; i < slice.size(); ++i)
    {
        slice[i].macroblock_address = task.m_SliceInfo[i].startMB;
        slice[i].num_macroblocks    = task.m_SliceInfo[i].numMB;
        slice[i].macroblock_info    = VA_INVALID_ID;

        for (ref = 0; ref < list0.Size(); ref++)
        {
            slice[i].RefPicList0[ref].frame_idx    = idx  = dpb[list0[ref] & 0x7f].m_frameIdx & 0x7f;
            slice[i].RefPicList0[ref].picture_id          = reconQueue[idx].surface;
            if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
                slice[i].RefPicList0[ref].flags               = list0[ref] >> 7 ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
        }
        for (; ref < 32; ref++)
        {
            slice[i].RefPicList0[ref].picture_id = VA_INVALID_ID;
            slice[i].RefPicList0[ref].flags      = VA_PICTURE_H264_INVALID;
        }

        for (ref = 0; ref < list1.Size(); ref++)
        {
            slice[i].RefPicList1[ref].frame_idx    = idx  = dpb[list1[ref] & 0x7f].m_frameIdx & 0x7f;
            slice[i].RefPicList1[ref].picture_id          = reconQueue[idx].surface;
            if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
                slice[i].RefPicList1[ref].flags               = list1[ref] >> 7 ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
        }
        for (; ref < 32; ref++)
        {
            slice[i].RefPicList1[ref].picture_id = VA_INVALID_ID;
            slice[i].RefPicList1[ref].flags      = VA_PICTURE_H264_INVALID;
        }

        slice[i].pic_parameter_set_id = pps.pic_parameter_set_id;
        slice[i].slice_type = ConvertMfxFrameType2VaapiSliceType( task.m_type[fieldId] );

        slice[i].direct_spatial_mv_pred_flag = 1;

        slice[i].num_ref_idx_l0_active_minus1 = std::max<mfxU8>(1, task.m_list0[fieldId].Size()) - 1;
        slice[i].num_ref_idx_l1_active_minus1 = std::max<mfxU8>(1, task.m_list1[fieldId].Size()) - 1;
        slice[i].num_ref_idx_active_override_flag   =
                    slice[i].num_ref_idx_l0_active_minus1 != pps.num_ref_idx_l0_active_minus1 ||
                    slice[i].num_ref_idx_l1_active_minus1 != pps.num_ref_idx_l1_active_minus1;

        slice[i].idr_pic_id = task.m_idrPicId;
        slice[i].pic_order_cnt_lsb = mfxU16(task.GetPoc(fieldId));

        slice[i].delta_pic_order_cnt_bottom         = 0;
        slice[i].delta_pic_order_cnt[0]             = 0;
        slice[i].delta_pic_order_cnt[1]             = 0;
        slice[i].luma_log2_weight_denom             = 0;
        slice[i].chroma_log2_weight_denom           = 0;

        slice[i].cabac_init_idc                     = extDdi ? (mfxU8)extDdi->CabacInitIdcPlus1 - 1 : 0;
        slice[i].slice_qp_delta                     = mfxI8(task.m_cqpValue[fieldId] - pps.pic_init_qp);

        {
            slice[i].disable_deblocking_filter_idc = extOpt2.DisableDeblockingIdc;
            slice[i].slice_alpha_c0_offset_div2    = 0;
            slice[i].slice_beta_offset_div2        = 0;
        }

        if (pPWT)
            FillPWT(hwCaps, pps, *pPWT, slice[i]);
    }

} // void UpdateSliceSizeLimited(...)


////////////////////////////////////////////////////////////////////////////////////////////
//                    HWEncoder based on VAAPI
////////////////////////////////////////////////////////////////////////////////////////////

using namespace MfxHwH264Encode;

VAAPIEncoder::VAAPIEncoder()
    : m_core(NULL)
    , m_videoParam()
    , m_vaDisplay()
    , m_vaContextEncode(VA_INVALID_ID)
    , m_vaConfig(VA_INVALID_ID)
    , m_sps()
    , m_pps()
    , m_slice()
    , m_spsBufferId(VA_INVALID_ID)
    , m_hrdBufferId(VA_INVALID_ID)
    , m_rateParamBufferId(VA_INVALID_ID)
    , m_frameRateId(VA_INVALID_ID)
    , m_qualityLevelId(VA_INVALID_ID)
    , m_maxFrameSizeId(VA_INVALID_ID)
    , m_multiPassFrameSizeId(VA_INVALID_ID)
    , m_quantizationId(VA_INVALID_ID)
    , m_rirId(VA_INVALID_ID)
    , m_qualityParamsId(VA_INVALID_ID)
    , m_miscParameterSkipBufferId(VA_INVALID_ID)
    , m_maxSliceSizeId(VA_INVALID_ID)
#if defined (MFX_ENABLE_H264_ROUNDING_OFFSET)
    , m_roundingOffsetId(VA_INVALID_ID)
#endif
    , m_roiBufferId(VA_INVALID_ID)
    , m_ppsBufferId(VA_INVALID_ID)
    , m_mbqpBufferId(VA_INVALID_ID)
#if defined (MFX_ENABLE_AVC_CUSTOM_QMATRIX)
    , m_qmBufferId(VA_INVALID_ID)
#endif
#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
    , m_triggerGpuHangBufferId(VA_INVALID_ID)
#endif
    , m_mbNoSkipBufferId(VA_INVALID_ID)
    , m_packedAudHeaderBufferId(VA_INVALID_ID)
    , m_packedAudBufferId(VA_INVALID_ID)
    , m_packedSpsHeaderBufferId(VA_INVALID_ID)
    , m_packedSpsBufferId(VA_INVALID_ID)
    , m_packedPpsHeaderBufferId(VA_INVALID_ID)
    , m_packedPpsBufferId(VA_INVALID_ID)
    , m_packedSeiHeaderBufferId(VA_INVALID_ID)
    , m_packedSeiBufferId(VA_INVALID_ID)
    , m_packedSkippedSliceHeaderBufferId(VA_INVALID_ID)
    , m_packedSkippedSliceBufferId(VA_INVALID_ID)
#if defined (MFX_ENABLE_AVC_CUSTOM_QMATRIX)
    , m_packedCqmPpsHeaderBufferId(CQM_HINT_NUM_CUST_MATRIX, VA_INVALID_ID)
    , m_packedCqmPpsBufferId(CQM_HINT_NUM_CUST_MATRIX, VA_INVALID_ID)
#endif
    , m_feedbackCache()
    , m_bsQueue()
    , m_reconQueue()
    , m_width()
    , m_height()
    , m_userMaxFrameSize()
    , m_mbbrc()
    , m_caps()
    , m_RIRState()
    , m_curTrellisQuantization()
    , m_newTrellisQuantization()
    , m_arrayVAEncROI()
    , m_guard()
    , m_headerPacker()
    , m_numSkipFrames()
    , m_sizeSkipFrames()
    , m_skipMode()
    , m_isENCPAK(false)
    , m_isBrcResetRequired(false)
    , m_vaBrcPar()
    , m_vaFrameRate()
    , m_mb_noskip_buffer()
{
    m_videoParam.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH; // QueryHwCaps will use this value

} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)


VAAPIEncoder::~VAAPIEncoder()
{
    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()


void VAAPIEncoder::FillSps(
    MfxVideoParam const & par,
    VAEncSequenceParameterBufferH264 & sps)
{
        mfxExtSpsHeader const * extSps = GetExtBuffer(par);
        assert( extSps != 0 );
        if (!extSps)
            return;

        sps.picture_width_in_mbs  = mfx::align2_value(par.mfx.FrameInfo.Width,  16) >> 4;
        sps.picture_height_in_mbs = mfx::align2_value(par.mfx.FrameInfo.Height, 16) >> 4;

        sps.level_idc    = par.mfx.CodecLevel;

        sps.intra_period = par.mfx.GopPicSize;
        sps.ip_period    = par.mfx.GopRefDist;

        sps.bits_per_second     = GetMaxBitrateValue(par.calcParam.maxKbps) << (6 + SCALE_FROM_DRIVER);

        sps.time_scale        = extSps->vui.timeScale;
        sps.num_units_in_tick = extSps->vui.numUnitsInTick;

        sps.max_num_ref_frames   =  mfxU8((extSps->maxNumRefFrames + 1) / 2);
        sps.seq_parameter_set_id = 0; // extSps->seqParameterSetId; - driver doesn't support non-zero sps_id

        sps.bit_depth_luma_minus8    = extSps->bitDepthLumaMinus8;
        sps.bit_depth_chroma_minus8  = extSps->bitDepthChromaMinus8;

        sps.seq_fields.bits.chroma_format_idc                 = extSps->chromaFormatIdc;
        sps.seq_fields.bits.log2_max_frame_num_minus4         = extSps->log2MaxFrameNumMinus4;
        sps.seq_fields.bits.pic_order_cnt_type                = extSps->picOrderCntType;
        sps.seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4 = extSps->log2MaxPicOrderCntLsbMinus4;

        sps.num_ref_frames_in_pic_order_cnt_cycle   = extSps->numRefFramesInPicOrderCntCycle;
        sps.offset_for_non_ref_pic                  = extSps->offsetForNonRefPic;
        sps.offset_for_top_to_bottom_field          = extSps->offsetForTopToBottomField;

        Copy(sps.offset_for_ref_frame,  extSps->offsetForRefFrame);

        sps.frame_crop_left_offset                  = mfxU16(extSps->frameCropLeftOffset);
        sps.frame_crop_right_offset                 = mfxU16(extSps->frameCropRightOffset);
        sps.frame_crop_top_offset                   = mfxU16(extSps->frameCropTopOffset);
        sps.frame_crop_bottom_offset                = mfxU16(extSps->frameCropBottomOffset);

        sps.seq_fields.bits.seq_scaling_matrix_present_flag         = extSps->seqScalingMatrixPresentFlag;
        sps.seq_fields.bits.delta_pic_order_always_zero_flag        = extSps->deltaPicOrderAlwaysZeroFlag;
        sps.seq_fields.bits.frame_mbs_only_flag                     = extSps->frameMbsOnlyFlag;
        sps.seq_fields.bits.mb_adaptive_frame_field_flag            = extSps->mbAdaptiveFrameFieldFlag;
        sps.seq_fields.bits.direct_8x8_inference_flag               = extSps->direct8x8InferenceFlag;

        sps.vui_parameters_present_flag                    = extSps->vuiParametersPresentFlag;
        sps.vui_fields.bits.timing_info_present_flag       = extSps->vui.flags.timingInfoPresent;
        sps.vui_fields.bits.bitstream_restriction_flag     = extSps->vui.flags.bitstreamRestriction;
        sps.vui_fields.bits.log2_max_mv_length_horizontal  = extSps->vui.log2MaxMvLengthHorizontal;
        sps.vui_fields.bits.log2_max_mv_length_vertical    = extSps->vui.log2MaxMvLengthVertical;

        sps.frame_cropping_flag                            = extSps->frameCroppingFlag;

        sps.sar_height        = extSps->vui.sarHeight;
        sps.sar_width         = extSps->vui.sarWidth;
        sps.aspect_ratio_idc  = extSps->vui.aspectRatioIdc;

        mfxExtCodingOption2 const * extOpt2 = GetExtBuffer(par);
        assert( extOpt2 != 0 );
        m_newTrellisQuantization = extOpt2 ? extOpt2->Trellis : 0;

} // void FillSps(...)

VAEntrypoint GetVAEntrypointType(bool lowPower, VideoCORE* core)
{
    return lowPower && CommonCaps::IsVAEncSliceLPSupported(core->GetHWType()) ? VAEntrypointEncSliceLP : VAEntrypointEncSlice;
}

mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(
    VideoCORE* core,
    GUID guid,
    mfxU32 width,
    mfxU32 height,
    bool /*isTemporal*/,
    mfxU16 /*chromaFormat*/,
    mfxU16 /*bitDepth*/)
{
    MFX_CHECK_NULL_PTR1(core);

    m_core = core;

    VAAPIVideoCORE_VPL* vaapi_core_vpl = reinterpret_cast<VAAPIVideoCORE_VPL*>(m_core->QueryCoreInterface(MFXIVAAPIVideoCORE_VPL_GUID));
    MFX_CHECK_WITH_ASSERT(vaapi_core_vpl, MFX_ERR_DEVICE_FAILED);

    MFX_SAFE_CALL(vaapi_core_vpl->GetVAService(&m_vaDisplay));

    m_caps = {};
    m_caps.ddi_caps.FrameSizeToleranceSupport = 
        H264ECaps::IsFrameSizeTolerenceSupported(m_core->GetHWType());

    m_width  = width;
    m_height = height;

    m_caps.ddi_caps.BRCReset        = 1; // no bitrate resolution control
    m_caps.ddi_caps.HeaderInsertion = 0; // we will provide headers (SPS, PPS) in binary format to the driver

    // libva doesn't accept AdaptiveMaxFrameSize and MaxFrameSizeP, so unsupported
    m_caps.AdaptiveMaxFrameSizeSupport = false;

    std::map<VAConfigAttribType, int> idx_map;
    VAConfigAttribType attr_types[] = {
        VAConfigAttribRTFormat,
        VAConfigAttribRateControl,
        VAConfigAttribEncQuantization,
        VAConfigAttribEncIntraRefresh,
        VAConfigAttribMaxPictureHeight,
        VAConfigAttribMaxPictureWidth,
        VAConfigAttribEncInterlaced,
        VAConfigAttribEncMaxRefFrames,
        VAConfigAttribEncSliceStructure,
        VAConfigAttribEncROI,
        VAConfigAttribEncSkipFrame,
        VAConfigAttribCustomRoundingControl
    };

    std::vector<VAConfigAttrib> attrs;
    attrs.reserve(sizeof(attr_types) / sizeof(attr_types[0]));
    auto AV = [&attrs, &idx_map](VAConfigAttribType t) -> uint32_t& { return attrs[idx_map[t]].value; };

    mfxI32 i = 0;
    std::for_each(std::begin(attr_types), std::end(attr_types)
        , [&attrs, &idx_map, &i](decltype(*std::begin(attr_types)) type)
    {
        attrs.push_back({ type, 0 });
        idx_map[type] = i;
        ++i;
    });

    bool lowPower = (MSDK_Private_Guid_Encode_AVC_LowPower_Query == guid) || (DXVA2_INTEL_LOWPOWERENCODE_AVC == guid);
    VAEntrypoint entrypoint = GetVAEntrypointType(lowPower, m_core);

    VAStatus vaSts = vaGetConfigAttributes(m_vaDisplay,
                          ConvertProfileTypeMFX2VAAPI(m_videoParam.mfx.CodecProfile),
                          entrypoint,
                          Begin(attrs), attrs.size());

    MFX_CHECK(!(VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT == vaSts ||
                VA_STATUS_ERROR_UNSUPPORTED_PROFILE    == vaSts),
                MFX_ERR_UNSUPPORTED);

    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    m_caps.CQPSupport = !!(AV(VAConfigAttribRateControl) & VA_RC_CQP);
    m_caps.CBRSupport = !!(AV(VAConfigAttribRateControl) & VA_RC_CBR);
    m_caps.VBRSupport = !!(AV(VAConfigAttribRateControl) & VA_RC_VBR);
    m_caps.ddi_caps.VCMBitrateControl = !!(AV(VAConfigAttribRateControl) & VA_RC_VCM);
    m_caps.ddi_caps.ICQBRCSupport = !!(AV(VAConfigAttribRateControl) & VA_RC_ICQ);
#ifdef MFX_ENABLE_QVBR
    m_caps.ddi_caps.QVBRBRCSupport = !!(AV(VAConfigAttribRateControl) & VA_RC_QVBR);
#endif
#if VA_CHECK_VERSION(1,3,0)
    m_caps.AVBRSupport = !!(AV(VAConfigAttribRateControl) & VA_RC_AVBR)
                      && H264ECaps::IsVmeSupported(m_core->GetHWType());
#endif
#if VA_CHECK_VERSION(1, 10, 0)
    m_caps.ddi_caps.TCBRCSupport = !!(AV(VAConfigAttribRateControl) & VA_RC_TCBRC);
#endif
    m_caps.ddi_caps.TrelisQuantization = !!(AV(VAConfigAttribEncQuantization) & (~VA_ATTRIB_NOT_SUPPORTED));
    m_caps.ddi_caps.vaTrellisQuantization = AV(VAConfigAttribEncQuantization);
    m_caps.ddi_caps.RollingIntraRefresh = !!(AV(VAConfigAttribEncIntraRefresh) & (~VA_ATTRIB_NOT_SUPPORTED));
    m_caps.ddi_caps.vaRollingIntraRefresh = AV(VAConfigAttribEncIntraRefresh);
    m_caps.ddi_caps.SkipFrame = !!(AV(VAConfigAttribEncSkipFrame) & (~VA_ATTRIB_NOT_SUPPORTED));
#if defined (MFX_ENABLE_H264_ROUNDING_OFFSET)
    m_caps.ddi_caps.RoundingOffset = !!(AV(VAConfigAttribCustomRoundingControl) & (~VA_ATTRIB_NOT_SUPPORTED));
#endif

    m_caps.ddi_caps.UserMaxFrameSizeSupport = 1;
    m_caps.ddi_caps.MBBRCSupport            = 1;
    m_caps.ddi_caps.MbQpDataSupport         = 1;
    m_caps.ddi_caps.NoWeightedPred          = 0;
    m_caps.ddi_caps.LumaWeightedPred        = 1;
    m_caps.ddi_caps.ChromaWeightedPred      = 1;
    m_caps.ddi_caps.MaxNum_WeightedPredL0   = 4;
    m_caps.ddi_caps.MaxNum_WeightedPredL1   = 2;


    m_caps.ddi_caps.Color420Only = !(AV(VAConfigAttribRTFormat) & (VA_RT_FORMAT_YUV422 | VA_RT_FORMAT_YUV444));

    m_caps.ddi_caps.MaxPicWidth = AV(VAConfigAttribMaxPictureWidth);
    if (m_caps.ddi_caps.MaxPicWidth == VA_ATTRIB_NOT_SUPPORTED || m_caps.ddi_caps.MaxPicWidth == 0)
        m_caps.ddi_caps.MaxPicWidth = 1920;

    m_caps.ddi_caps.MaxPicHeight = AV(VAConfigAttribMaxPictureHeight);
    if (m_caps.ddi_caps.MaxPicHeight == VA_ATTRIB_NOT_SUPPORTED || m_caps.ddi_caps.MaxPicHeight == 0)
        m_caps.ddi_caps.MaxPicHeight = 1088;

    if (AV(VAConfigAttribEncSliceStructure) != VA_ATTRIB_NOT_SUPPORTED)
    {
        // Attribute for VAConfigAttribEncSliceStructure includes both (1) information about supported slice structure and
        // (2) indication of support for max slice size feature
        const auto sliceCapabilities = AV(VAConfigAttribEncSliceStructure);
        const auto sliceStructure = sliceCapabilities & ~VA_ENC_SLICE_STRUCTURE_MAX_SLICE_SIZE;
        m_caps.ddi_caps.SliceStructure = static_cast<unsigned>(ConvertSliceStructureVAAPIToMFX(sliceStructure));
        m_caps.ddi_caps.SliceLevelRateCtrl = sliceCapabilities & VA_ENC_SLICE_STRUCTURE_MAX_SLICE_SIZE ? 1 : 0;
    }
    else
    {
        m_caps.ddi_caps.SliceStructure = 4;// 4 - arbitrary slice size in MBs; the other - SliceDividerOneSlice
    }

    m_caps.ddi_caps.NoInterlacedField = !(AV(VAConfigAttribEncInterlaced) & VA_ENC_INTERLACED_FIELD); // 0 - Interlace is supported, 1 - Interlace is not supported

    if (AV(VAConfigAttribEncMaxRefFrames) != VA_ATTRIB_NOT_SUPPORTED)
    {
        m_caps.ddi_caps.MaxNum_Reference  = AV(VAConfigAttribEncMaxRefFrames) & 0xffff;
        m_caps.ddi_caps.MaxNum_Reference1 = (AV(VAConfigAttribEncMaxRefFrames) >> 16) & 0xffff;
    }
    else
    {
        m_caps.ddi_caps.MaxNum_Reference  = 1;
        m_caps.ddi_caps.MaxNum_Reference1 = 1;
    }

    if (AV(VAConfigAttribEncROI) != VA_ATTRIB_NOT_SUPPORTED)
    {
        auto VaEncROIValPtr = reinterpret_cast<VAConfigAttribValEncROI *>(&AV(VAConfigAttribEncROI));
        assert(VaEncROIValPtr->bits.num_roi_regions < 32);

        m_caps.ddi_caps.MaxNumOfROI                = VaEncROIValPtr->bits.num_roi_regions;
        m_caps.ddi_caps.ROIBRCPriorityLevelSupport = VaEncROIValPtr->bits.roi_rc_priority_support;
        m_caps.ddi_caps.ROIBRCDeltaQPLevelSupport  = VaEncROIValPtr->bits.roi_rc_qp_delta_support;
    }
    else
    {
        m_caps.ddi_caps.MaxNumOfROI = 0;
    }

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(VideoCORE* core, GUID guid, mfxU32 width, mfxU32 height)


mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VAAPIEncoder::CreateAccelerationService");
    if(IsMvcProfile(par.mfx.CodecProfile))
        MFX_RETURN(MFX_ERR_UNSUPPORTED);

    if(0 == m_reconQueue.size())
    {
    /* We need to pass reconstructed surfaces wheh call vaCreateContext().
     * Here we don't have this info.
     */
        m_videoParam = par;
        return MFX_ERR_NONE;
    }

    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts;

    mfxI32 entrypointsIndx = 0;
    mfxI32 numEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    MFX_CHECK(numEntrypoints, MFX_ERR_DEVICE_FAILED);

    std::vector<VAEntrypoint> pEntrypoints(numEntrypoints);

    vaSts = vaQueryConfigEntrypoints(
                m_vaDisplay,
                ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
                Begin(pEntrypoints),
                &numEntrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    VAEntrypoint entryPoint = GetVAEntrypointType(IsOn(par.mfx.LowPower), m_core);
    if( !m_isENCPAK )
    {
        bool bEncodeEnable = false;
        for( entrypointsIndx = 0; entrypointsIndx < numEntrypoints; entrypointsIndx++ )
        {
            if( entryPoint == pEntrypoints[entrypointsIndx] )
            {
                bEncodeEnable = true;
                break;
            }
        }
        if( !bEncodeEnable )
        {
            MFX_RETURN(MFX_ERR_DEVICE_FAILED);
        }
    }

    // Configuration
    VAConfigAttrib attrib[3];
    mfxI32 numAttrib = 0;
    mfxU32 flag = VA_PROGRESSIVE;

    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    numAttrib += 2;

    vaSts = vaGetConfigAttributes(m_vaDisplay,
                          ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
                          entryPoint,
                          &attrib[0], numAttrib);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU32 vaRTFormat = ConvertMfxFourcc2VaapiRTFormat(par.mfx.FrameInfo.FourCC);
    if ((attrib[0].value & vaRTFormat) == 0)
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    uint32_t vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);

    mfxExtCodingOption2 const *extOpt2 = GetExtBuffer(par);
    if( NULL == extOpt2 )
    {
        assert( extOpt2 );
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }
    m_mbbrc = IsOn(extOpt2->MBBRC)  ? 1 : IsOff(extOpt2->MBBRC) ? 2 : 0;
    m_skipMode = extOpt2->SkipFrame;

    if ((attrib[1].value & vaRCType) == 0)
    {
        MFX_RETURN(MFX_ERR_DEVICE_FAILED);
    }

    attrib[0].value = VA_RT_FORMAT_YUV420;
    attrib[1].value = vaRCType;

    vaSts = vaCreateConfig(
        m_vaDisplay,
        ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
        entryPoint,
        attrib,
        numAttrib,
        &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> reconSurf;
    for(unsigned int i = 0; i < m_reconQueue.size(); i++)
        reconSurf.push_back(m_reconQueue[i].surface);

    // Encoder create

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaCreateContext");
        vaSts = vaCreateContext(
            m_vaDisplay,
            m_vaConfig,
            m_width,
            m_height,
            flag,
            Begin(reconSurf),
            reconSurf.size(),
            &m_vaContextEncode);
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU16 maxNumSlices = GetMaxNumSlices(par);

    m_slice.resize(maxNumSlices);
    m_sliceBufferId.resize(maxNumSlices);
    m_packedSliceHeaderBufferId.resize(maxNumSlices);
    m_packedSliceBufferId.resize(maxNumSlices);

    std::fill(m_sliceBufferId.begin(),             m_sliceBufferId.end(),             VA_INVALID_ID);
    std::fill(m_packedSliceHeaderBufferId.begin(), m_packedSliceHeaderBufferId.end(), VA_INVALID_ID);
    std::fill(m_packedSliceBufferId.begin(),       m_packedSliceBufferId.end(),       VA_INVALID_ID);

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    //------------------------------------------------------------------

    FillSps(par, m_sps);
    FillBrcStructures(par, m_vaBrcPar, m_vaFrameRate);

    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId),                                 MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRateControl(par, m_mbbrc, 0, 0, 0, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId),                           MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetQualityLevel(par, m_vaDisplay, m_vaContextEncode, m_qualityLevelId),                     MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetQualityParams(par, m_vaDisplay, m_vaContextEncode, m_qualityParamsId),                   MFX_ERR_DEVICE_FAILED);

    if (extOpt2->MaxSliceSize != 0)
    {
        mfxStatus sts = SetMaxSliceSize(extOpt2->MaxSliceSize, m_vaDisplay, m_vaContextEncode, m_maxSliceSizeId);
        MFX_CHECK_WITH_ASSERT(sts == MFX_ERR_NONE, MFX_ERR_DEVICE_FAILED);
    }

    FillConstPartOfPps(par, m_pps);

    if (m_caps.ddi_caps.HeaderInsertion == 0)
    {
        m_headerPacker.Init(par, m_caps);

        if (m_headerPacker.isSvcPrefixUsed())
        {
            m_packedSvcPrefixHeaderBufferId.resize(maxNumSlices, VA_INVALID_ID);
            m_packedSvcPrefixBufferId.resize(maxNumSlices, VA_INVALID_ID);
        }
    }

    mfxExtCodingOption3 const *extOpt3 = GetExtBuffer(par);

    if (extOpt3)
    {
        if (IsOn(extOpt3->MBDisableSkipMap))
            m_mb_noskip_buffer.resize(mfx::align2_value(m_width / 16, 64) * mfx::align2_value(m_height / 16, 8));
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)


mfxStatus VAAPIEncoder::Reset(MfxVideoParam const & par)
{
//    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc Reset");

    m_videoParam = par;
    mfxExtCodingOption2 const *extOpt2 = GetExtBuffer(par);
    mfxExtCodingOption3 const *extOpt3 = GetExtBuffer(par);
    if( NULL == extOpt2 )
    {
        assert( extOpt2 );
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }
    m_mbbrc = IsOn(extOpt2->MBBRC) ? 1 : IsOff(extOpt2->MBBRC) ? 2 : 0;
    m_skipMode = extOpt2->SkipFrame;

    FillSps(par, m_sps);
    VAEncMiscParameterRateControl oldBrcPar = m_vaBrcPar;
    VAEncMiscParameterFrameRate oldFrameRate = m_vaFrameRate;
    FillBrcStructures(par, m_vaBrcPar, m_vaFrameRate);

    // BRC reset is processed by driver inside vaRenderPicture(),
    // so set it here and use in 1st subsequent Execute()
    m_isBrcResetRequired =
           !Equal(m_vaBrcPar, oldBrcPar)
        || !Equal(m_vaFrameRate, oldFrameRate)
        || m_userMaxFrameSize != extOpt2->MaxFrameSize;

    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId),                                  MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRateControl(par, m_mbbrc, 0, 0, 0, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId),  MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId),                            MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetQualityLevel(par, m_vaDisplay, m_vaContextEncode, m_qualityLevelId),                      MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetQualityParams(par, m_vaDisplay, m_vaContextEncode, m_qualityParamsId),                    MFX_ERR_DEVICE_FAILED);

    if (extOpt2->MaxSliceSize != 0)
    {
        mfxStatus sts = SetMaxSliceSize(extOpt2->MaxSliceSize, m_vaDisplay, m_vaContextEncode, m_maxSliceSizeId);
        MFX_CHECK_WITH_ASSERT(sts == MFX_ERR_NONE, MFX_ERR_DEVICE_FAILED);
    }

    FillConstPartOfPps(par, m_pps);

    if (m_caps.ddi_caps.HeaderInsertion == 0)
        m_headerPacker.Init(par, m_caps);

    if (extOpt3)
    {
        if (IsOn(extOpt3->MBDisableSkipMap))
            m_mb_noskip_buffer.resize(mfx::align2_value(m_width / 16, 64) * mfx::align2_value(m_height / 16, 8));
    }
    /* Destroy existing FEI buffers
     * For next Execute() call new buffer sets will re-allocated */
    if (m_isENCPAK)
    {
        mfxStatus mfxSts;

        for( mfxU32 i = 0; i < m_vaFeiMBStatId.size(); i++ )
        {
            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_vaFeiMBStatId[i]);
            MFX_CHECK_STS(mfxSts);
        }

        for( mfxU32 i = 0; i < m_vaFeiMVOutId.size(); i++ )
        {
            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_vaFeiMVOutId[i]);
            MFX_CHECK_STS(mfxSts);
        }

        for( mfxU32 i = 0; i < m_vaFeiMCODEOutId.size(); i++ )
        {
            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_vaFeiMCODEOutId[i]);
            MFX_CHECK_STS(mfxSts);
        }

        std::fill(m_vaFeiMBStatBufSize.begin(),   m_vaFeiMBStatBufSize.end(),   0);
        std::fill(m_vaFeiMVOutBufSize.begin(),    m_vaFeiMVOutBufSize.end(),    0);
        std::fill(m_vaFeiMCODEOutBufSize.begin(), m_vaFeiMCODEOutBufSize.end(), 0);
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Reset(MfxVideoParam const & par)


mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT /*type*/, mfxFrameAllocRequest& request)
{
    // request linear buffer
    request.Info.FourCC = MFX_FOURCC_P8;

    // context_id required for allocation video memory (tmp solution)
    request.AllocId = m_vaContextEncode;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)


mfxStatus VAAPIEncoder::QueryEncodeCaps(MFX_ENCODE_CAPS& caps)
{

    caps = m_caps;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryEncodeCaps(MFX_ENCODE_CAPS& caps)

mfxStatus VAAPIEncoder::QueryMbPerSec(mfxVideoParam const & par, mfxU32 (&mbPerSec)[16])
{
    VAConfigID config = VA_INVALID_ID;

    VAConfigAttrib attrib[2];
    attrib[0].type = VAConfigAttribRTFormat;
    attrib[0].value = VA_RT_FORMAT_YUV420;
    attrib[1].type = VAConfigAttribRateControl;
    attrib[1].value = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);

    VAStatus vaSts = vaCreateConfig(
        m_vaDisplay,
        ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
        GetVAEntrypointType(IsOn(par.mfx.LowPower), m_core),
        attrib,
        2,
        &config);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    VAProcessingRateParameter proc_rate_buf = { };
    mfxU32 & processing_rate = mbPerSec[0];

    proc_rate_buf.proc_buf_enc.level_idc = par.mfx.CodecLevel ? par.mfx.CodecLevel : 0xff;
    proc_rate_buf.proc_buf_enc.quality_level = par.mfx.TargetUsage ? par.mfx.TargetUsage : 0xffff;
    proc_rate_buf.proc_buf_enc.intra_period = par.mfx.GopPicSize ? par.mfx.GopPicSize : 0xffff;
    proc_rate_buf.proc_buf_enc.ip_period = par.mfx.GopRefDist ? par.mfx.GopRefDist : 0xffff;

    vaSts = vaQueryProcessingRate(m_vaDisplay, config, &proc_rate_buf, &processing_rate);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaDestroyConfig(m_vaDisplay, config);

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::QueryHWGUID(VideoCORE * /*core*/, GUID /*guid*/, bool /*isTemporal*/)
{
    MFX_RETURN(MFX_ERR_UNSUPPORTED);
}

mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if( D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type )
    {
        pQueue = &m_bsQueue;
    }
    else
    {
        pQueue = &m_reconQueue;
    }

    {
        // we should register allocated HW bitstreams and recon surfaces
        MFX_CHECK( response.mids, MFX_ERR_NULL_PTR );

        ExtVASurface extSurf;
        VASurfaceID *pSurface = NULL;

        for (mfxU32 i = 0; i < response.NumFrameActual; i++)
        {

            sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&pSurface);
            MFX_CHECK_STS(sts);

            extSurf.number  = i;
            extSurf.surface = *pSurface;

            pQueue->push_back( extSurf );
        }
    }
    if( D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type )
    {
        sts = CreateAccelerationService(m_videoParam);
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)


bool operator==(const ENCODE_ENC_CTRL_CAPS& l, const ENCODE_ENC_CTRL_CAPS& r)
{
    return memcmp(&l, &r, sizeof(ENCODE_ENC_CTRL_CAPS)) == 0;
}

bool operator!=(const ENCODE_ENC_CTRL_CAPS& l, const ENCODE_ENC_CTRL_CAPS& r)
{
    return !(l == r);
}
//static int debug_frame_bum = 0;

mfxStatus VAAPIEncoder::Execute(
    mfxHDLPair      pair,
    DdiTask const & task,
    mfxU32          fieldId,
    PreAllocatedVector const & sei)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "H264 encode DDISubmitTask");

    mfxHDL surface = pair.first;
    VAEncPackedHeaderParameterBuffer packed_header_param_buffer;
    VASurfaceID reconSurface;
    VASurfaceID *inputSurface = (VASurfaceID*)surface;
    VABufferID  codedBuffer;
    std::vector<VABufferID> configBuffers;
    std::vector<mfxU32> packedBufferIndexes;
    mfxU32      i;
    VAStatus    vaSts;
    mfxStatus   mfxSts;
    mfxU8 skipFlag  = task.SkipFlag();
    mfxU16 skipMode = m_skipMode;
    mfxExtCodingOption2     const * ctrlOpt2      = GetExtBuffer(task.m_ctrl);
    mfxExtCodingOption3     const * ctrlOpt3      = GetExtBuffer(task.m_ctrl);
    mfxExtMBDisableSkipMap  const * ctrlNoSkipMap = GetExtBuffer(task.m_ctrl);
#if defined (MFX_ENABLE_H264_ROUNDING_OFFSET)
    mfxExtAVCRoundingOffset const * ctrlRoundingOffset  = GetExtBuffer(task.m_ctrl, task.m_fid[fieldId]);
#endif

    if (ctrlOpt2 && ctrlOpt2->SkipFrame <= MFX_SKIPFRAME_BRC_ONLY)
        skipMode = ctrlOpt2->SkipFrame;

    if (skipMode == MFX_SKIPFRAME_BRC_ONLY)
    {
        skipFlag = 0; // encode current frame as normal
        m_numSkipFrames += (mfxU8)task.m_ctrl.SkipFrame;
    }

    // update params
    {
        size_t slice_size_old = m_slice.size();
        //Destroy old buffers
        for(size_t i = 0; i < slice_size_old; i++)
        {
            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_sliceBufferId[i]);
            MFX_CHECK_STS(mfxSts);

            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSliceHeaderBufferId[i]);
            MFX_CHECK_STS(mfxSts);

            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSliceBufferId[i]);
            MFX_CHECK_STS(mfxSts);
        }

        for(size_t i = 0; i < m_packedSvcPrefixBufferId.size(); i++)
        {
            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSvcPrefixHeaderBufferId[i]);
            MFX_CHECK_STS(mfxSts);

            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSvcPrefixBufferId[i]);
            MFX_CHECK_STS(mfxSts);
        }

        UpdatePPS(task, fieldId, m_pps, m_reconQueue
#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
            , m_headerPacker.GetPackedCqmPpsNum()
#endif
        );

        if (task.m_SliceInfo.size())
            UpdateSliceSizeLimited(m_caps, task, fieldId, m_sps, m_pps, m_slice, m_videoParam, m_reconQueue);
        else
            UpdateSlice(m_caps, task, fieldId, m_sps, m_pps, m_slice, m_videoParam, m_reconQueue);

        if (slice_size_old != m_slice.size())
        {
            m_sliceBufferId.resize(m_slice.size());
            m_packedSliceHeaderBufferId.resize(m_slice.size());
            m_packedSliceBufferId.resize(m_slice.size());

            if (m_headerPacker.isSvcPrefixUsed())
            {
                m_packedSvcPrefixHeaderBufferId.resize(m_slice.size());
                m_packedSvcPrefixBufferId.resize(m_slice.size());
            }
        }
    }
    configBuffers.reserve(MAX_CONFIG_BUFFERS_COUNT + m_slice.size() * 2 + m_packedSvcPrefixBufferId.size() * 2);

    /* for debug only */
    //fprintf(stderr, "----> Encoding frame = %u, type = %u\n", debug_frame_bum++, ConvertMfxFrameType2SliceType( task.m_type[fieldId]) -5 );

    //------------------------------------------------------------------
    // find bitstream
    mfxU32 idxBs = task.m_idxBs[fieldId];
    if( idxBs < m_bsQueue.size() )
    {
        codedBuffer = m_bsQueue[idxBs].surface;
    }
    else
    {
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }

    // find reconstructed surface
    mfxU32 idxRecon = task.m_idxRecon;
    if( idxRecon < m_reconQueue.size())
    {
        reconSurface = m_reconQueue[ idxRecon ].surface;
    }
    else
    {
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }

    m_pps.coded_buf = codedBuffer;
    m_pps.CurrPic.picture_id = reconSurface;

    //------------------------------------------------------------------
    // buffer creation & configuration
    //------------------------------------------------------------------
    {
        // 1. sequence level
        {
            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_spsBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncSequenceParameterBufferType,
                                   sizeof(m_sps),
                                   1,
                                   &m_sps,
                                   &m_spsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers.push_back(m_spsBufferId);
        }

        // 2. Picture level
        {
            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_ppsBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncPictureParameterBufferType,
                                   sizeof(m_pps),
                                   1,
                                   &m_pps,
                                   &m_ppsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers.push_back(m_ppsBufferId);
        }

        // 3. Slice level
        for( i = 0; i < m_slice.size(); i++ )
        {
            // Buffer destroyed in the beginning of the function
            vaSts = vaCreateBuffer(m_vaDisplay,
                                    m_vaContextEncode,
                                    VAEncSliceParameterBufferType,
                                    sizeof(m_slice[i]),
                                    1,
                                    &m_slice[i],
                                    &m_sliceBufferId[i]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }

    if (m_caps.ddi_caps.HeaderInsertion == 1 && skipFlag == NO_SKIP)
    {
        // SEI
        if (sei.Size() > 0)
        {
            packed_header_param_buffer.type = VAEncPackedHeaderRawData;
            packed_header_param_buffer.has_emulation_bytes = 1;
            packed_header_param_buffer.bit_length = sei.Size()*8;

            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSeiHeaderBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedSeiHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);


            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSeiBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                sei.Size(), 1, RemoveConst(sei.Buffer()),
                                &m_packedSeiBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers.push_back(m_packedSeiHeaderBufferId);
            configBuffers.push_back(m_packedSeiBufferId);
        }
    }
    else
    {
        // AUD
        if (task.m_insertAud[fieldId])
        {
            ENCODE_PACKEDHEADER_DATA const & packedAud = m_headerPacker.PackAud(task, fieldId);

            packed_header_param_buffer.type = VAEncPackedHeaderRawData;
            packed_header_param_buffer.has_emulation_bytes = !packedAud.SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedAud.DataLength*8;

            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedAudHeaderBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedAudHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);


            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedAudBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                packedAud.DataLength, 1, packedAud.pData,
                                &m_packedAudBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(configBuffers.size());
            configBuffers.push_back(m_packedAudHeaderBufferId);
            configBuffers.push_back(m_packedAudBufferId);
        }
        // SPS
        if (task.m_insertSps[fieldId])
        {
            std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSpsArray = m_headerPacker.GetSps();
            ENCODE_PACKEDHEADER_DATA const & packedSps = packedSpsArray[0];

            packed_header_param_buffer.type = VAEncPackedHeaderSequence;
            packed_header_param_buffer.has_emulation_bytes = !packedSps.SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedSps.DataLength*8;

            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSpsHeaderBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedSpsHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);


            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSpsBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                packedSps.DataLength, 1, packedSps.pData,
                                &m_packedSpsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(configBuffers.size());
            configBuffers.push_back(m_packedSpsHeaderBufferId);
            configBuffers.push_back(m_packedSpsBufferId);
        }

        // PPS
        if (task.m_insertPps[fieldId])
        {
            std::vector<ENCODE_PACKEDHEADER_DATA> const & packedPpsArray = m_headerPacker.GetPps();
            ENCODE_PACKEDHEADER_DATA const & packedPps = packedPpsArray[0];

            packed_header_param_buffer.type = VAEncPackedHeaderPicture;
            packed_header_param_buffer.has_emulation_bytes = !packedPps.SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedPps.DataLength*8;

            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedPpsHeaderBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedPpsHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);


            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedPpsBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                packedPps.DataLength, 1, packedPps.pData,
                                &m_packedPpsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(configBuffers.size());
            configBuffers.push_back(m_packedPpsHeaderBufferId);
            configBuffers.push_back(m_packedPpsBufferId);

#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
            if (IsCustMatrix(task.m_adaptiveCQMHint))
            {
                mfxU32 extCqmNum = m_headerPacker.GetPackedCqmPpsNum();
                if (extCqmNum > 0)
                {
                    std::vector<ENCODE_PACKEDHEADER_DATA> const & packedCqmPps = m_headerPacker.GetPps(true);
                    
                    for (mfxU32 cqmIndex = 0; cqmIndex < extCqmNum; cqmIndex++)
                    {
                        packed_header_param_buffer.has_emulation_bytes = !packedCqmPps[cqmIndex].SkipEmulationByteCount;
                        packed_header_param_buffer.bit_length = packedCqmPps[cqmIndex].DataLength * 8;

                        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedCqmPpsHeaderBufferId[cqmIndex]);
                        MFX_CHECK_STS(mfxSts);

                        vaSts = vaCreateBuffer(m_vaDisplay,
                                               m_vaContextEncode,
                                               VAEncPackedHeaderParameterBufferType,
                                               sizeof(packed_header_param_buffer),
                                               1,
                                               &packed_header_param_buffer,
                                               &m_packedCqmPpsHeaderBufferId[cqmIndex]);
                        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);


                        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedCqmPpsBufferId[cqmIndex]);
                        MFX_CHECK_STS(mfxSts);

                        vaSts = vaCreateBuffer(m_vaDisplay,
                                               m_vaContextEncode,
                                               VAEncPackedHeaderDataBufferType,
                                               packedCqmPps[cqmIndex].DataLength, 1, packedCqmPps[cqmIndex].pData,
                                               &m_packedCqmPpsBufferId[cqmIndex]);
                        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                        packedBufferIndexes.push_back(configBuffers.size());
                        configBuffers.push_back(m_packedCqmPpsHeaderBufferId[cqmIndex]);
                        configBuffers.push_back(m_packedCqmPpsBufferId[cqmIndex]);
                    }
                }
            }
#endif
        }

        // SEI
        if (sei.Size() > 0)
        {
            packed_header_param_buffer.type = VAEncPackedHeaderRawData;
            packed_header_param_buffer.has_emulation_bytes = 1;
            packed_header_param_buffer.bit_length = sei.Size()*8;

            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSeiHeaderBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedSeiHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);


            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSeiBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                sei.Size(), 1, RemoveConst(sei.Buffer()),
                                &m_packedSeiBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(configBuffers.size());
            configBuffers.push_back(m_packedSeiHeaderBufferId);
            configBuffers.push_back(m_packedSeiBufferId);
        }

        if (skipFlag != NO_SKIP)
        {
            // The whole slice
            ENCODE_PACKEDHEADER_DATA const & packedSkippedSlice = m_headerPacker.PackSkippedSlice(task, fieldId);

            packed_header_param_buffer.type = VAEncPackedHeaderRawData;
            packed_header_param_buffer.has_emulation_bytes = !packedSkippedSlice.SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedSkippedSlice.DataLength*8;

            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSkippedSliceHeaderBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderParameterBufferType,
                sizeof(packed_header_param_buffer),
                1,
                &packed_header_param_buffer,
                &m_packedSkippedSliceHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);


            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSkippedSliceBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderDataBufferType,
                packedSkippedSlice.DataLength, 1, packedSkippedSlice.pData,
                &m_packedSkippedSliceBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(configBuffers.size());
            configBuffers.push_back(m_packedSkippedSliceHeaderBufferId);
            configBuffers.push_back(m_packedSkippedSliceBufferId);

        }
        else
        {
            // length of Prefix NAL unit: now it's always 8 bytes for reference picture, and 7 bytes for non-reference picture
            // need to add zero_byte if AU starts from Prefix NAL unit
            mfxU32 prefix_bytes = (7 + !!task.m_nalRefIdc[fieldId]) * m_headerPacker.isSvcPrefixUsed();

            //Slice headers only
            std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSlices = m_headerPacker.PackSlices(task, fieldId);
            for (size_t i = 0; i < packedSlices.size(); i++)
            {
                ENCODE_PACKEDHEADER_DATA const & packedSlice = packedSlices[i];

                if (prefix_bytes)
                {
                    packed_header_param_buffer.type = VAEncPackedHeaderRawData;
                    packed_header_param_buffer.has_emulation_bytes = 1;
                    packed_header_param_buffer.bit_length = (prefix_bytes * 8);

                    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPackedHeaderParameterBufferType,
                            sizeof(packed_header_param_buffer),
                            1,
                            &packed_header_param_buffer,
                            &m_packedSvcPrefixHeaderBufferId[i]);
                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                    vaSts = vaCreateBuffer(m_vaDisplay,
                                        m_vaContextEncode,
                                        VAEncPackedHeaderDataBufferType,
                                         prefix_bytes, 1, packedSlice.pData,
                                        &m_packedSvcPrefixBufferId[i]);
                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                    configBuffers.push_back(m_packedSvcPrefixHeaderBufferId[i]);
                    configBuffers.push_back(m_packedSvcPrefixBufferId[i]);
                }

                packed_header_param_buffer.type = VAEncPackedHeaderH264_Slice;
                packed_header_param_buffer.has_emulation_bytes = 0;
                packed_header_param_buffer.bit_length = packedSlice.DataLength - (prefix_bytes * 8); // DataLength is already in bits !

                // Buffer destroyed in the beginning of the function
                vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextEncode,
                        VAEncPackedHeaderParameterBufferType,
                        sizeof(packed_header_param_buffer),
                        1,
                        &packed_header_param_buffer,
                        &m_packedSliceHeaderBufferId[i]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                // Buffer destroyed in the beginning of the function
                vaSts = vaCreateBuffer(m_vaDisplay,
                                    m_vaContextEncode,
                                    VAEncPackedHeaderDataBufferType,
                                    (packedSlice.DataLength + 7) / 8 - prefix_bytes, 1, packedSlice.pData + prefix_bytes,
                                    &m_packedSliceBufferId[i]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                configBuffers.push_back(m_packedSliceHeaderBufferId[i]);
                configBuffers.push_back(m_packedSliceBufferId[i]);
            }
        }
    }

    configBuffers.push_back(m_hrdBufferId);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRateControl(m_videoParam, m_mbbrc, task.m_minQP, task.m_maxQP,
        task.m_TCBRCTargetFrameSize, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId), MFX_ERR_DEVICE_FAILED);

    configBuffers.push_back(m_rateParamBufferId);
    configBuffers.push_back(m_frameRateId);
    configBuffers.push_back(m_qualityLevelId);

    mfxExtCodingOption2 const *extOpt2 = GetExtBuffer(m_videoParam);
    if (extOpt2 && extOpt2->MaxSliceSize != 0)
        configBuffers.push_back(m_maxSliceSizeId);

    if (m_isBrcResetRequired)
    {
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetBrcResetRequired(m_vaDisplay, m_rateParamBufferId, m_isBrcResetRequired),
                              MFX_ERR_DEVICE_FAILED);
        m_isBrcResetRequired = false;
    }

/*
 * Limit frame size by application/user level
 */
    m_userMaxFrameSize = (UINT)task.m_maxIFrameSize;
//    if (task.m_frameOrder)
//        m_sps.bResetBRC = true;
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetMaxFrameSize(m_userMaxFrameSize, m_vaDisplay,
                                                          m_vaContextEncode, m_maxFrameSizeId), MFX_ERR_DEVICE_FAILED);
    configBuffers.push_back(m_maxFrameSizeId);
#if VA_CHECK_VERSION(1,3,0)
    if (task.m_brcFrameCtrl.MaxNumRepak && task.m_brcFrameCtrl.MaxFrameSize)
    {
        mfxSts = SetMultiPassFrameSize(task, fieldId, m_vaDisplay, m_vaContextEncode, m_multiPassFrameSizeId);
        MFX_CHECK_WITH_ASSERT(mfxSts == MFX_ERR_NONE, MFX_ERR_DEVICE_FAILED);
        configBuffers.push_back(m_multiPassFrameSizeId);
    }
#endif

#if !defined(ANDROID)
/*
 *  By default (0) - driver will decide.
 *  1 - disable trellis quantization
 *  x0E - enable for any type of frames
 */
    if (m_newTrellisQuantization != 0)
    {
        m_curTrellisQuantization = m_newTrellisQuantization;
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetTrellisQuantization(m_curTrellisQuantization, m_vaDisplay,
                                                                     m_vaContextEncode, m_quantizationId), MFX_ERR_DEVICE_FAILED);
        configBuffers.push_back(m_quantizationId);
    }
#endif

 /*
 *   RollingIntraRefresh
 */
    if (task.m_IRState != m_RIRState)
    {
        m_RIRState = task.m_IRState;
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRollingIntraRefresh(m_RIRState, m_vaDisplay,
                                                                     m_vaContextEncode, m_rirId), MFX_ERR_DEVICE_FAILED);
        configBuffers.push_back(m_rirId);
    }

    if (task.m_numRoi)
    {
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetROI(task, m_arrayVAEncROI, m_vaDisplay, m_vaContextEncode, m_roiBufferId),
                              MFX_ERR_DEVICE_FAILED);
        configBuffers.push_back(m_roiBufferId);
    }

    /*FEI has its own interface for MBQp*/
    if ((task.m_isMBQP[fieldId]) && (!m_isENCPAK))
    {
        mfxFrameData qpMap = {};
        FrameLocker lock(m_core, qpMap, task.m_midMBQP[fieldId]);
        static_assert(sizeof(VAEncQPBufferH264) == 1, "qpMapBuffer must be changed");
        mfxU32 mbH = mfx::align2_value(m_sps.picture_height_in_mbs >> (!!task.m_fieldPicFlag),8);
        mfxU32 mbW = mfx::align2_value(m_sps.picture_width_in_mbs, 64);
        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_mbqpBufferId);
        MFX_CHECK_STS(mfxSts);
        // LibVA expect full buffer size w/o interlace adjustments
        vaSts = vaCreateBuffer(m_vaDisplay,
            m_vaContextEncode,
            VAEncQPBufferType,
            mbW, mbH,
            qpMap.Y,
            &m_mbqpBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        configBuffers.push_back(m_mbqpBufferId);
       
    }

    if (ctrlNoSkipMap)
    {
        mfxU32 mbW = m_sps.picture_width_in_mbs;
        mfxU32 mbH = m_sps.picture_height_in_mbs / (2 - !task.m_fieldPicFlag);
        //width(64byte alignment) height(8byte alignment)
        mfxU32 bufW = mfx::align2_value(mbW, 64);
        mfxU32 bufH = mfx::align2_value(mbH, 8);
        mfxU32 fieldOffset = (mfxU32)fieldId * (mbH * mbW) * (mfxU32)!!task.m_fieldPicFlag;

        if (   m_mb_noskip_buffer.size() >= (bufW * bufH)
            && ctrlNoSkipMap->Map
            && ctrlNoSkipMap->MapSize >= (mbW * m_sps.picture_height_in_mbs))
        {
            Zero(m_mb_noskip_buffer);
            for (mfxU32 mbRow = 0; mbRow < mbH; mbRow ++)
                MFX_INTERNAL_CPY(&m_mb_noskip_buffer[mbRow * bufW], &ctrlNoSkipMap->Map[fieldOffset + mbRow * mbW], mbW);

            mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_mbNoSkipBufferId);
            MFX_CHECK_STS(mfxSts);

            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncMacroblockDisableSkipMapBufferType,
                    (bufW * bufH),
                    1,
                    &m_mb_noskip_buffer[0],
                    &m_mbNoSkipBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers.push_back(m_mbNoSkipBufferId);
        }
    }

#if defined(MFX_ENABLE_AVC_CUSTOM_QMATRIX)
    if (m_pps.pic_fields.bits.pic_scaling_matrix_present_flag ||
        m_sps.seq_fields.bits.seq_scaling_matrix_present_flag)
    {
        VAIQMatrixBufferH264 m_qMatrix;
        memcpy(m_qMatrix.ScalingList4x4, task.m_qMatrix.scalingList4x4, sizeof(task.m_qMatrix.scalingList4x4));
        memcpy(m_qMatrix.ScalingList8x8, task.m_qMatrix.scalingList8x8, sizeof(task.m_qMatrix.scalingList8x8));

        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_qmBufferId);
        MFX_CHECK_STS(mfxSts);

        vaSts = vaCreateBuffer(m_vaDisplay,
                               m_vaContextEncode,
                               VAQMatrixBufferType,
                               sizeof(m_qMatrix),
                               1,
                               &m_qMatrix,
                               &m_qmBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        if (m_qmBufferId != VA_INVALID_ID)
            configBuffers.push_back(m_qmBufferId);
    }
#endif

#if defined (MFX_ENABLE_H264_ROUNDING_OFFSET)
    if (m_caps.ddi_caps.RoundingOffset && ctrlRoundingOffset)
    {
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRoundingOffset(m_vaDisplay, m_vaContextEncode, *ctrlRoundingOffset, m_roundingOffsetId), MFX_ERR_DEVICE_FAILED);

        configBuffers.push_back(m_roundingOffsetId);
    }
#endif

#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
    if ((mfxExtIntGPUHang*)GetExtBuffer(task.m_ctrl))
    {
        unsigned int trigger_hang = 1;
        mfxStatus mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_triggerGpuHangBufferId);
        MFX_CHECK_STS(mfxSts);
        vaSts = vaCreateBuffer(m_vaDisplay,
                               m_vaContextEncode,
                               VATriggerCodecHangBufferType,
                               sizeof(trigger_hang),
                               1,
                               &trigger_hang,
                               &m_triggerGpuHangBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        configBuffers.push_back(m_triggerGpuHangBufferId);
    }
#endif

    if (ctrlOpt2 || ctrlOpt3)
    {
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetQualityParams(m_videoParam, m_vaDisplay,
                                                               m_vaContextEncode, m_qualityParamsId, &task), MFX_ERR_DEVICE_FAILED);
    }
    if (VA_INVALID_ID != m_qualityParamsId) configBuffers.push_back(m_qualityParamsId);

    mfxU32 storedSize = 0;

#ifndef MFX_AVC_ENCODING_UNIT_DISABLE
    if (task.m_collectUnitsInfo)
    {
        m_headerPacker.GetHeadersInfo(task.m_headersCache[fieldId], task, fieldId);
    }
#endif

    if (skipFlag != NORMAL_MODE)
    {
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetSkipFrame(m_vaDisplay, m_vaContextEncode, m_miscParameterSkipBufferId,
                                                           skipFlag ? skipFlag : !!m_numSkipFrames,
                                                           m_numSkipFrames, m_sizeSkipFrames), MFX_ERR_DEVICE_FAILED);

        configBuffers.push_back(m_miscParameterSkipBufferId);

        m_numSkipFrames  = 0;
        m_sizeSkipFrames = 0;

        mdprintf(stderr, "task.m_frameNum=%d\n", task.m_frameNum);
        mdprintf(stderr, "inputSurface=%d\n", *inputSurface);
        mdprintf(stderr, "m_pps.CurrPic.picture_id=%d\n", m_pps.CurrPic.picture_id);
        mdprintf(stderr, "m_pps.ReferenceFrames[0]=%d\n", m_pps.ReferenceFrames[0].picture_id);
        mdprintf(stderr, "m_pps.ReferenceFrames[1]=%d\n", m_pps.ReferenceFrames[1].picture_id);
        //------------------------------------------------------------------
        // Rendering
        //------------------------------------------------------------------
        MFX_LTRACE_2(MFX_TRACE_LEVEL_HOTSPOTS, "A|ENCODE|AVC|PACKET_START|", "%d|%d", m_vaContextEncode, task.m_frameNum);
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaBeginPicture");

            vaSts = vaBeginPicture(
                m_vaDisplay,
                m_vaContextEncode,
                *inputSurface);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaRenderPicture");
            vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                configBuffers.data(),
                configBuffers.size());
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            for(i = 0; i < m_slice.size(); i++)
            {
                vaSts = vaRenderPicture(
                    m_vaDisplay,
                    m_vaContextEncode,
                    &m_sliceBufferId[i],
                    1);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        }
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaEndPicture");

            vaSts = vaEndPicture(m_vaDisplay, m_vaContextEncode);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
        MFX_LTRACE_2(MFX_TRACE_LEVEL_HOTSPOTS, "A|ENCODE|AVC|PACKET_END|", "%d|%d", m_vaContextEncode, task.m_frameNum);
    }
    else
    {
        VACodedBufferSegment *codedBufferSegment;
        {
            codedBuffer = m_bsQueue[task.m_idxBs[fieldId]].surface;
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
            vaSts = vaMapBuffer(
                m_vaDisplay,
                codedBuffer,
                (void **)(&codedBufferSegment));
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
        codedBufferSegment->next = 0;
        codedBufferSegment->reserved = 0;
        codedBufferSegment->status = 0;

        MFX_CHECK_WITH_ASSERT(codedBufferSegment->buf, MFX_ERR_DEVICE_FAILED);

        mfxU8 *  bsDataStart = (mfxU8 *)codedBufferSegment->buf;
        mfxU8 *  bsDataEnd   = bsDataStart;

        if (skipMode != MFX_SKIPFRAME_INSERT_NOTHING)
        {
            for (size_t i = 0; i < packedBufferIndexes.size(); i++)
            {
                size_t headerIndex = packedBufferIndexes[i];

                void *pBufferHeader;
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
                    vaSts = vaMapBuffer(m_vaDisplay, configBuffers[headerIndex], &pBufferHeader);
                }
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                void *pData;
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
                    vaSts = vaMapBuffer(m_vaDisplay, configBuffers[headerIndex + 1], &pData);
                }
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                if (pBufferHeader && pData)
                {
                    VAEncPackedHeaderParameterBuffer const & header = *(VAEncPackedHeaderParameterBuffer const *)pBufferHeader;

                    mfxU32 lenght = (header.bit_length+7)/8;

                    assert(mfxU32(bsDataStart + m_width*m_height - bsDataEnd) > lenght);
                    assert(header.has_emulation_bytes);

                    MFX_INTERNAL_CPY(bsDataEnd, pData, lenght);
                    bsDataEnd += lenght;
                }

                vaSts = vaUnmapBuffer(m_vaDisplay, configBuffers[headerIndex]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                vaSts = vaUnmapBuffer(m_vaDisplay, configBuffers[headerIndex + 1]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        }

        storedSize = mfxU32(bsDataEnd - bsDataStart);
        codedBufferSegment->size = storedSize;

        m_numSkipFrames ++;
        m_sizeSkipFrames += (skipMode != MFX_SKIPFRAME_INSERT_NOTHING) ? storedSize : 0;

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
            vaSts = vaUnmapBuffer( m_vaDisplay, codedBuffer );
        }
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    //------------------------------------------------------------------
    // PostStage
    //------------------------------------------------------------------
    // put to cache
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback;
        currentFeedback.number  = task.m_statusReportNumber[fieldId];
        currentFeedback.surface = (NORMAL_MODE == skipFlag) ? VA_INVALID_SURFACE : *inputSurface;
        currentFeedback.idxBs   = task.m_idxBs[fieldId];
        currentFeedback.size    = storedSize;

        m_feedbackCache.push_back( currentFeedback );
    }

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus VAAPIEncoder::QueryStatus(
    DdiTask & task,
    mfxU32    fieldId,
    bool      /*useEvent*/)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "H264 encode DDIWaitTaskSync");

    bool isFound = false;
    VASurfaceID waitSurface;
    mfxU32 waitIdxBs;
    mfxU32 waitSize;
    mfxU32 indxSurf;
    ExtVASurface currentFeedback;

    UMC::AutomaticUMCMutex guard(m_guard);

    for( indxSurf = 0; indxSurf < m_feedbackCache.size(); indxSurf++ )
    {
        currentFeedback = m_feedbackCache[ indxSurf ];

        if( currentFeedback.number == task.m_statusReportNumber[fieldId] )
        {
            waitSurface = currentFeedback.surface;
            waitIdxBs   = currentFeedback.idxBs;
            waitSize    = currentFeedback.size;

            isFound  = true;
            break;
        }
    }

    if( !isFound )
    {
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }

    if (VA_INVALID_SURFACE == waitSurface) //skipped frame
    {
        task.m_bsDataLength[fieldId] = waitSize;
        m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);

        return MFX_ERR_NONE;
    }

    // find used bitstream
    VABufferID codedBuffer;
    if(waitIdxBs < m_bsQueue.size())
    {
        codedBuffer = m_bsQueue[waitIdxBs].surface;
    }
    else
    {
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }

    VAStatus vaSts = VA_STATUS_SUCCESS;

#if defined(SYNCHRONIZATION_BY_VA_MAP_BUFFER)
    m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
    guard.Unlock();

#else
#   if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)
    m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
    guard.Unlock();

#    else
    VASurfaceStatus surfSts = VASurfaceSkipped;

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if (VASurfaceReady == surfSts)
    {
        m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
        guard.Unlock();
    }
    else if (VASurfaceRendering == surfSts || VASurfaceDisplaying == surfSts)
    {
        MFX_RETURN(MFX_WRN_DEVICE_BUSY);
    }
    else
    {
        assert(!"bad feedback status");
        MFX_RETURN(MFX_ERR_DEVICE_FAILED);
    }

#endif // #if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)

#endif // #if defined(SYNCHRONIZATION_BY_VA_MAP_BUFFER)

#if VA_CHECK_VERSION(1,9,0)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaSyncBuffer");
        vaSts = vaSyncBuffer(m_vaDisplay, codedBuffer, VA_TIMEOUT_INFINITE);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
#endif
    VACodedBufferSegment *codedBufferSegment = 0;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
        vaSts = vaMapBuffer(
            m_vaDisplay,
            codedBuffer,
            (void **)(&codedBufferSegment));
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    task.m_bsDataLength[fieldId] = codedBufferSegment->size;

    // Disable QP report on Linux until driver fix.
    task.m_qpY[fieldId] = (codedBufferSegment->status & VA_CODED_BUF_STATUS_PICTURE_AVE_QP_MASK);

    if (codedBufferSegment->status & VA_CODED_BUF_STATUS_BAD_BITSTREAM)
        sts = MFX_ERR_GPU_HANG;
    else if (!codedBufferSegment->size || !codedBufferSegment->buf)
        sts = MFX_ERR_DEVICE_FAILED;


    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
        vaSts = vaUnmapBuffer( m_vaDisplay, codedBuffer );
    }
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    MFX_CHECK_STS(sts);

    if (m_isENCPAK)
    {
        // Use parity to order conversion here (FEI ext buffers attached by field order, but MSDK operates in terms of fields parity)
        sts = VAAPIEncoder::QueryStatusFEI(task, task.m_fid[fieldId],
                                           currentFeedback, codedBufferSegment->status);
        MFX_CHECK_STS(sts);
    }

    return sts;
} //mfxStatus VAAPIEncoder::QueryStatus

mfxStatus VAAPIEncoder::QueryStatusFEI(
    DdiTask const & task,
    mfxU32  feiFieldId,
    ExtVASurface const & curFeedback,
    mfxU32  codedStatus)
{
    (void)codedStatus;
    (void)task;
    (void)feiFieldId;
    (void)curFeedback;

    MFX_RETURN(MFX_ERR_UNKNOWN);
} //mfxStatus VAAPIEncoder::QueryStatusFEI

mfxStatus VAAPIEncoder::Destroy()
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VAAPIEncoder::Destroy");

    VAStatus vaSts;
    mfxStatus mfxSts;

    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_spsBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_hrdBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_rateParamBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_frameRateId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_qualityLevelId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_maxFrameSizeId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_multiPassFrameSizeId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_quantizationId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_rirId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_qualityParamsId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_miscParameterSkipBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_maxSliceSizeId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_roiBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_ppsBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_mbqpBufferId);
    MFX_CHECK_STS(mfxSts);
#if defined (MFX_ENABLE_AVC_CUSTOM_QMATRIX)
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_qmBufferId);
    MFX_CHECK_STS(mfxSts);
#endif
#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_triggerGpuHangBufferId);
    MFX_CHECK_STS(mfxSts);
#endif
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_mbNoSkipBufferId);
    MFX_CHECK_STS(mfxSts);

    for(mfxU32 i = 0; i < m_slice.size(); i++)
    {
        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_sliceBufferId[i]);
        MFX_CHECK_STS(mfxSts);

        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSliceHeaderBufferId[i]);
        MFX_CHECK_STS(mfxSts);

        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSliceBufferId[i]);
        MFX_CHECK_STS(mfxSts);
    }

    for(size_t i = 0; i < m_packedSvcPrefixBufferId.size(); i++)
    {
        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSvcPrefixHeaderBufferId[i]);
        MFX_CHECK_STS(mfxSts);

        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSvcPrefixBufferId[i]);
        MFX_CHECK_STS(mfxSts);
    }

#if defined (MFX_ENABLE_AVC_CUSTOM_QMATRIX)
    for (size_t i = 0; i < m_packedCqmPpsBufferId.size(); i++)
    {
        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedCqmPpsHeaderBufferId[i]);
        MFX_CHECK_STS(mfxSts);

        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedCqmPpsBufferId[i]);
        MFX_CHECK_STS(mfxSts);
    }
#endif  

    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedAudHeaderBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedAudBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSpsHeaderBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSpsBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedPpsHeaderBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedPpsBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSeiHeaderBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSeiBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSkippedSliceHeaderBufferId);
    MFX_CHECK_STS(mfxSts);
    mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_packedSkippedSliceBufferId);
    MFX_CHECK_STS(mfxSts);

    for( mfxU32 i = 0; i < m_vaFeiMBStatId.size(); i++ )
    {
        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_vaFeiMBStatId[i]);
        MFX_CHECK_STS(mfxSts);
    }

    for( mfxU32 i = 0; i < m_vaFeiMVOutId.size(); i++ )
    {
        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_vaFeiMVOutId[i]);
        MFX_CHECK_STS(mfxSts);
    }

    for( mfxU32 i = 0; i < m_vaFeiMCODEOutId.size(); i++ )
    {
        mfxSts = CheckAndDestroyVAbuffer(m_vaDisplay, m_vaFeiMCODEOutId[i]);
        MFX_CHECK_STS(mfxSts);
    }

    if (m_vaContextEncode != VA_INVALID_ID)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaDestroyContext");
        vaSts = vaDestroyContext(m_vaDisplay, m_vaContextEncode);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        m_vaContextEncode = VA_INVALID_ID;
    }

    if (m_vaConfig != VA_INVALID_ID)
    {
        vaSts = vaDestroyConfig(m_vaDisplay, m_vaConfig);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        m_vaConfig = VA_INVALID_ID;
    }

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::Destroy()

#endif // MFX_ENABLE_H264_VIDEO_ENCODE
/* EOF */


