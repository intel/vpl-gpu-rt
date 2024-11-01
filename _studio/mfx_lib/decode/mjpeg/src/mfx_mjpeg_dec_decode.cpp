// Copyright (c) 2004-2024 Intel Corporation
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

#include <limits>
#include "mfx_mjpeg_dec_decode.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)


#include "mfx_common.h"
#include "mfx_common_decode_int.h"

#include "mfx_enc_common.h"

#include "umc_jpeg_frame_constructor.h"
#include "umc_mjpeg_mfx_decode_hw.h"



#include "ippcore.h" // MfxIppInit in case of bundled IPP

#include "libmfx_core_interface.h"

// Declare skipping constants
enum
{
    JPEG_MIN_SKIP_RATE = 0,
    JPEG_MAX_SKIP_RATE = 9,

    JPEG_SKIP_BOUND = JPEG_MAX_SKIP_RATE + 1
};


struct ThreadTaskInfoJpeg
{
    mfxFrameSurface1 *surface_work;
    mfxFrameSurface1 *surface_out;
    UMC::FrameData   *dst;
    mfxU32            decodeTaskID;
    mfxU32            vppTaskID;
    bool              needCheckVppStatus;
    mfxU32            numDecodeTasksToCheck;
};

static void SetFrameType(mfxFrameSurface1 &surface_out)
{
    auto extFrameInfo = reinterpret_cast<mfxExtDecodedFrameInfo *>(GetExtendedBuffer(surface_out.Data.ExtParam, surface_out.Data.NumExtParam, MFX_EXTBUFF_DECODED_FRAME_INFO));
    if (extFrameInfo == nullptr)
        return;

    // terms I/P/B frames not applicable for JPEG,
    // so all frames marked as I
    extFrameInfo->FrameType = MFX_FRAMETYPE_I;
}

class MFX_JPEG_Utility
{
public:

    static eMFXPlatform GetPlatform(VideoCORE * core, mfxVideoParam * par);
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out, eMFXHWType type);
    static bool CheckVideoParam(mfxVideoParam *in, eMFXHWType type);

private:

    static bool IsFormatSupport(mfxVideoParam * in);
};

VideoDECODEMJPEG::VideoDECODEMJPEG(VideoCORE *core, mfxStatus * sts)
    : VideoDECODE()
    , m_core(core)
    , m_isInit(false)
    , m_frameOrder((mfxU16)MFX_FRAMEORDER_UNKNOWN)
    , m_response()
    , m_response_alien()
    , m_platform(MFX_PLATFORM_SOFTWARE)
{
    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }

    m_isHeaderFound = false;
    m_isHeaderParsed = false;

    m_skipRate = 0;
    m_skipCount = 0;

    m_maxCropW = 0;
    m_maxCropH = 0;
}

VideoDECODEMJPEG::~VideoDECODEMJPEG(void)
{
    Close();
}

mfxStatus VideoDECODEMJPEG::Init(mfxVideoParam *par)
{
    std::lock_guard<std::mutex> guard(m_mGuard);

    if (m_isInit)
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);

    MFX_CHECK_NULL_PTR1(par);

    m_platform = MFX_JPEG_Utility::GetPlatform(m_core, par);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (m_platform == MFX_PLATFORM_HARDWARE)
    {
        type = m_core->GetHWType();
    }

    if (CheckVideoParamDecoders(par, type) < MFX_ERR_NONE)
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);

    if (!MFX_JPEG_Utility::CheckVideoParam(par, type))
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);

    m_vFirstPar = *par;
    m_vFirstPar.mfx.NumThread = 0;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_vFirstPar);
    m_vPar = m_vFirstPar;

    m_maxCropW = m_vPar.mfx.FrameInfo.CropW;
    m_maxCropH = m_vPar.mfx.FrameInfo.CropH;

    m_vPar.mfx.NumThread = (mfxU16)(m_vPar.AsyncDepth ? m_vPar.AsyncDepth : m_core->GetAutoAsyncDepth());
    if (MFX_PLATFORM_SOFTWARE != m_platform)
        m_vPar.mfx.NumThread = 1;

    int32_t useInternal = (MFX_PLATFORM_SOFTWARE == m_platform) ?
        (m_vPar.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) :
        (m_vPar.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    // allocate memory
    mfxFrameAllocRequest request;
    mfxFrameAllocRequest request_internal;
    memset(&request, 0, sizeof(request));
    memset(&m_response, 0, sizeof(m_response));
    memset(&m_response_alien, 0, sizeof(m_response_alien));

    MFX_SAFE_CALL(QueryIOSurfInternal(m_core, &m_vPar, &request));

    request_internal = request;

    if (IsD3D9Simulation(*m_core))
        useInternal = true;

    if (useInternal)
        request.Type |= MFX_MEMTYPE_INTERNAL_FRAME;
    else
        request.Type |= MFX_MEMTYPE_EXTERNAL_FRAME;

    mfxStatus mfxSts = MFX_ERR_NONE;

    if (MFX_PLATFORM_SOFTWARE == m_platform)
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }
    else
    {
        VideoDECODEMJPEGBase_HW * dec = new VideoDECODEMJPEGBase_HW;
        decoder.reset(dec);
        mfxU32 bNeedVpp = dec->AdjustFrameAllocRequest(&request_internal,
            &m_vPar.mfx,
            m_core,
            VideoDECODEMJPEGBase_HW::isVideoPostprocEnabled(m_core));
        useInternal |= bNeedVpp;

        if (bNeedVpp)
        {
            if (request_internal.Type & MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)
            {
                request_internal.Type &= ~(MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPOUT);
            }

            request_internal.Type |= MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

            try
            {
                dec->m_surface_source.reset(new SurfaceSourceJPEG(m_core, *par, m_platform, request, request_internal, m_response, m_response_alien));
            }
            catch (const std::system_error& ex)
            {
                MFX_CHECK_STS(mfxStatus(ex.code().value()));
            }
        }
    }

    decoder->m_vPar = m_vPar;

    if (!decoder->m_surface_source)
    {
        try
        {
            decoder->m_surface_source.reset(new SurfaceSource(m_core, *par, m_platform, request, request_internal, m_response, m_response_alien));
        }
        catch (const std::system_error& ex)
        {
            MFX_CHECK_STS(mfxStatus(ex.code().value()));
        }
    }

    mfxVideoParam decPar = *par;
    decPar.mfx.FrameInfo = request.Info;
    m_frameConstructor.reset(new UMC::JpegFrameConstructor());

    mfxSts = decoder->Init(&decPar, &request, &m_response, &request_internal, !useInternal, m_core);

    MFX_CHECK(mfxSts >= MFX_ERR_NONE, mfxSts);

    m_isInit = true;
    m_isHeaderFound = false;
    m_isHeaderParsed = false;

    m_frameOrder = 0;

    if (m_platform != m_core->GetPlatformType())
    {
        assert(m_platform == MFX_PLATFORM_SOFTWARE);
        MFX_RETURN(MFX_WRN_PARTIAL_ACCELERATION);
    }

    MFX_CHECK(!isNeedChangeVideoParamWarning, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::QueryImplsDescription(
    VideoCORE&,
    mfxDecoderDescription::decoder& caps,
    mfx::PODArraysHolder& ah)
{
    const mfxU32 SupportedProfiles[] =
    {
        MFX_PROFILE_JPEG_BASELINE
    };
    const mfxResourceType SupportedMemTypes[] =
    {
        MFX_RESOURCE_SYSTEM_SURFACE
        , MFX_RESOURCE_VA_SURFACE
    };
    const mfxU32 SupportedFourCC[] =
    {
        MFX_FOURCC_NV12
        , MFX_FOURCC_RGB4
        , MFX_FOURCC_YUY2
    };

    caps.CodecID = MFX_CODEC_JPEG;
    caps.MaxcodecLevel = MFX_LEVEL_UNKNOWN;

    for (mfxU32 profile : SupportedProfiles)
    {
        auto& pfCaps = ah.PushBack(caps.Profiles);
        pfCaps.Profile = profile;

        for (auto memType : SupportedMemTypes)
        {
            auto& memCaps = ah.PushBack(pfCaps.MemDesc);
            memCaps.MemHandleType = memType;
            memCaps.Width = { 16, 16384, 16 };
            memCaps.Height = { 16, 16384, 16 };

            for (auto fcc : SupportedFourCC)
            {
                ah.PushBack(memCaps.ColorFormats) = fcc;
                ++memCaps.NumColorFormats;
            }
            ++pfCaps.NumMemTypes;
        }
        ++caps.NumProfiles;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::Reset(mfxVideoParam *par)
{
    TRACE_EVENT(MFX_TRACE_API_DECODE_RESET_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(par ? par->mfx.FrameInfo.Width : 0,
        par ? par->mfx.FrameInfo.Height : 0, par ? par->mfx.CodecId : 0));

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(par);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (m_platform == MFX_PLATFORM_HARDWARE)
    {
        type = m_core->GetHWType();
    }

    if (CheckVideoParamDecoders(par, type) < MFX_ERR_NONE)
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);

    if (!MFX_JPEG_Utility::CheckVideoParam(par, type))
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);

    if (!IsSameVideoParam(par, &m_vFirstPar))
        MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    // need to sw acceleration
    if (m_platform != MFX_JPEG_Utility::GetPlatform(m_core, par))
    {
        MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    }

    MFX_SAFE_CALL(decoder->Reset(par));

    m_frameOrder = 0;
    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(par);
    m_vPar = *par;

    if (isNeedChangeVideoParamWarning)
    {
        MFX_RETURN(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    }

    m_isHeaderFound = false;
    m_isHeaderParsed = false;

    m_skipRate = 0;
    m_skipCount = 0;

    m_frameConstructor->Reset();

    if (m_platform != m_core->GetPlatformType())
    {
        assert(m_platform == MFX_PLATFORM_SOFTWARE);
        MFX_RETURN(MFX_WRN_PARTIAL_ACCELERATION);
    }

    TRACE_EVENT(MFX_TRACE_API_DECODE_RESET_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(MFX_ERR_NONE));

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::Close(void)
{
    if (!m_isInit)
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);

    decoder->Close();

    m_isInit = false;
    m_isHeaderFound = false;
    m_isHeaderParsed = false;
    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;

    m_frameConstructor->Close();

    return MFX_ERR_NONE;
}

mfxTaskThreadingPolicy VideoDECODEMJPEG::GetThreadingPolicy(void)
{
    return MFX_TASK_THREADING_INTER;
}

mfxStatus VideoDECODEMJPEG::Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);

    eMFXHWType type = core->GetHWType();
    return MFX_JPEG_Utility::Query(core, in, out, type);
}

mfxStatus VideoDECODEMJPEG::GetVideoParam(mfxVideoParam *par)
{
    if (!m_isInit)
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(par);

    par->mfx = m_vPar.mfx;

    par->Protected = m_vPar.Protected;
    par->IOPattern = m_vPar.IOPattern;
    par->AsyncDepth = m_vPar.AsyncDepth;

    if (!par->mfx.FrameInfo.FrameRateExtD && !par->mfx.FrameInfo.FrameRateExtN)
    {
        par->mfx.FrameInfo.FrameRateExtD = m_vPar.mfx.FrameInfo.FrameRateExtD;
        par->mfx.FrameInfo.FrameRateExtN = m_vPar.mfx.FrameInfo.FrameRateExtN;

        if (!par->mfx.FrameInfo.FrameRateExtD && !par->mfx.FrameInfo.FrameRateExtN)
        {
            par->mfx.FrameInfo.FrameRateExtN = 0;
            par->mfx.FrameInfo.FrameRateExtD = 0;
        }
    }

    if (!par->mfx.FrameInfo.AspectRatioH && !par->mfx.FrameInfo.AspectRatioW)
    {
        par->mfx.FrameInfo.AspectRatioH = m_vPar.mfx.FrameInfo.AspectRatioH;
        par->mfx.FrameInfo.AspectRatioW = m_vPar.mfx.FrameInfo.AspectRatioW;

        if (!par->mfx.FrameInfo.AspectRatioH && !par->mfx.FrameInfo.AspectRatioW)
        {
            par->mfx.FrameInfo.AspectRatioH = 1;
            par->mfx.FrameInfo.AspectRatioW = 1;
        }
    }

    return decoder->GetVideoParam(par);
}

mfxStatus VideoDECODEMJPEG::DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR2(bs, par);

    mfxStatus sts = CheckBitstream(bs);
    if (sts != MFX_ERR_NONE)
    {
        MFX_CHECK_INIT(sts == MFX_ERR_NULL_PTR);
        MFX_RETURN(MFX_ERR_NULL_PTR);
    }

    MFXMediaDataAdapter in(bs);

    mfx_UMC_MemAllocator  tempAllocator;
    tempAllocator.InitMem(0, core);

    mfxExtJPEGQuantTables*    jpegQT = (mfxExtJPEGQuantTables*)   mfx::GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_QT );
    mfxExtJPEGHuffmanTables*  jpegHT = (mfxExtJPEGHuffmanTables*) mfx::GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );

    // DecodeHeader
    UMC::MJPEGVideoDecoderBaseMFX decoder;

    UMC::VideoDecoderParams umcVideoParams;
    umcVideoParams.info.clip_info.height = par->mfx.FrameInfo.Height;
    umcVideoParams.info.clip_info.width = par->mfx.FrameInfo.Width;

    umcVideoParams.lpMemoryAllocator = &tempAllocator;

    UMC::Status umcRes = decoder.Init(&umcVideoParams);
    MFX_CHECK_INIT(umcRes == UMC::UMC_OK);

    mfxExtBuffer* extbuf = GetExtendedBuffer(bs->ExtParam, bs->NumExtParam, MFX_EXTBUFF_DECODE_ERROR_REPORT);

    if (extbuf)
    {
        reinterpret_cast<mfxExtDecodeErrorReport *>(extbuf)->ErrorTypes = 0;
        in.SetExtBuffer(extbuf);
    }

    umcRes = decoder.DecodeHeader(&in);

    in.Save(bs);

    MFX_CHECK_INIT(umcRes == UMC::UMC_OK);

    mfxVideoParam temp;

    umcRes = decoder.FillVideoParam(&temp, false);
    MFX_CHECK_INIT(umcRes == UMC::UMC_OK);

    if(jpegQT)
    {
        umcRes = decoder.FillQuantTableExtBuf(jpegQT);
        MFX_CHECK_INIT(umcRes == UMC::UMC_OK);
    }

    if(jpegHT)
    {
        umcRes = decoder.FillHuffmanTableExtBuf(jpegHT);
        MFX_CHECK_INIT(umcRes == UMC::UMC_OK);
    }

    decoder.Close();
    tempAllocator.Close();

    par->mfx.FrameInfo = temp.mfx.FrameInfo;
    par->mfx.JPEGChromaFormat = temp.mfx.JPEGChromaFormat;
    par->mfx.JPEGColorFormat = temp.mfx.JPEGColorFormat;
    par->mfx.Rotation  = temp.mfx.Rotation;
    par->mfx.InterleavedDec  = temp.mfx.InterleavedDec;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR2(par, request);

    eMFXPlatform platform = MFX_JPEG_Utility::GetPlatform(core, par);

    mfxVideoParam params = *par;
    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&params);

    if (   !(par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        && !(par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);

    if ((par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_SAFE_CALL(QueryIOSurfInternal(core, &params, request));

    int32_t isInternalManaging = (MFX_PLATFORM_SOFTWARE == platform) ?
        (params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    if (isInternalManaging)
    {
        request->NumFrameSuggested = request->NumFrameMin = par->AsyncDepth ? par->AsyncDepth : core->GetAutoAsyncDepth();

        request->Type = MFX_MEMTYPE_FROM_DECODE;
        if (MFX_PLATFORM_SOFTWARE == platform)
        {
            if ((request->Info.FourCC == MFX_FOURCC_RGB4 || request->Info.FourCC == MFX_FOURCC_YUY2) && MFX_HW_D3D11 == core->GetVAType())
                request->Type |= MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;
            else
                request->Type |= MFX_MEMTYPE_DXVA2_DECODER_TARGET;
        }
        else
        {
            request->Type |= MFX_MEMTYPE_SYSTEM_MEMORY;
        }
    }

    request->Type |= MFX_MEMTYPE_EXTERNAL_FRAME;

    if (platform != core->GetPlatformType())
    {
        assert(platform == MFX_PLATFORM_SOFTWARE);
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    if (isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::QueryIOSurfInternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    eMFXPlatform platform = MFX_JPEG_Utility::GetPlatform(core, par);

    request->Info = par->mfx.FrameInfo;

    mfxU32 asyncDepth = (par->AsyncDepth ? par->AsyncDepth : core->GetAutoAsyncDepth());

    request->NumFrameMin = mfxU16 (asyncDepth);

    request->NumFrameSuggested = request->NumFrameMin;

    request->Type = MFX_MEMTYPE_FROM_DECODE;

    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    if (videoProcessing)
    {
        // need to substitute output format
        // number of surfaces is same
        request->Info.FourCC = videoProcessing->Out.FourCC;

        request->Info.ChromaFormat = videoProcessing->Out.ChromaFormat;
        request->Info.Width = videoProcessing->Out.Width;
        request->Info.Height = videoProcessing->Out.Height;
        request->Info.CropX = videoProcessing->Out.CropX;
        request->Info.CropY = videoProcessing->Out.CropY;
        request->Info.CropW = videoProcessing->Out.CropW;
        request->Info.CropH = videoProcessing->Out.CropH;
    }

    if(MFX_ROTATION_90 == par->mfx.Rotation || MFX_ROTATION_270 == par->mfx.Rotation)
    {
        std::swap(request->Info.Height, request->Info.Width);
        std::swap(request->Info.AspectRatioH, request->Info.AspectRatioW);
        std::swap(request->Info.CropH, request->Info.CropW);
        std::swap(request->Info.CropY, request->Info.CropX);
    }

    request->Info.Width  = mfx::align2_value(request->Info.Width, 0x10);
    request->Info.Height = mfx::align2_value(request->Info.Height,
        (request->Info.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 0x8 : 0x10);

    if (MFX_PLATFORM_SOFTWARE == platform)
    {
        request->Type |= MFX_MEMTYPE_SYSTEM_MEMORY;
    }
    else
    {
        bool needVpp = false;
        if(par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF ||
           par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF)
        {
            needVpp = true;
        }

        mfxFrameAllocRequest request_internal = *request;
        bool isPostProcEnable = VideoDECODEMJPEGBase_HW::isVideoPostprocEnabled(core);
        VideoDECODEMJPEGBase_HW::AdjustFourCC(&request_internal.Info, &par->mfx, core->GetVAType(),
                                              isPostProcEnable, &needVpp);


        if (needVpp && MFX_HW_D3D11 == core->GetVAType())
        {
            request->Type |= MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET;
            request->Type |= MFX_MEMTYPE_FROM_VPPOUT;
        }
        else
            request->Type |= MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::GetDecodeStat(mfxDecodeStat *stat)
{
    TRACE_EVENT(MFX_TRACE_API_DECODE_GETSTAT_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(0));

    if (!m_isInit)
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(stat);

    decoder->m_stat.NumCachedFrame = 0;
    decoder->m_stat.NumError = 0;

    *stat = decoder->m_stat;

    TRACE_EVENT(MFX_TRACE_API_DECODE_GETSTAT_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(stat ? stat->NumFrame : 0, MFX_ERR_NONE));

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::MJPEGDECODERoutine(void *pState, void *pParam,
                                               mfxU32 threadNumber, mfxU32 callNumber)
{
    VideoDECODEMJPEG &obj = *((VideoDECODEMJPEG *) pState);
    MFX::AutoTimer timer("DecodeFrame");
    return obj.decoder->RunThread(pParam, threadNumber, callNumber);
} // mfxStatus VideoDECODEMJPEG::MJPEGDECODERoutine(void *pState, void *pParam,

mfxStatus VideoDECODEMJPEG::MJPEGCompleteProc(void *pState, void *pParam,
                                              mfxStatus taskRes)
{
    VideoDECODEMJPEG &obj = *((VideoDECODEMJPEG *) pState);
    return obj.decoder->CompleteTask(pParam, taskRes);
}

mfxStatus VideoDECODEMJPEG::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, MFX_ENTRY_POINT *pEntryPoint)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);
    // It can be useful to run threads right after first frame receive
    MFX_SAFE_CALL(DecodeFrameCheck(bs, surface_work, surface_out));

    UMC::FrameData *dst = nullptr;
    MFX_SAFE_CALL(decoder->AllocateFrameData(dst));

    {
        // output surface is always working surface
        MFX_CHECK_NULL_PTR1(dst);
        *surface_out = decoder->m_surface_source->GetSurface(dst->GetFrameMID(),
                                                    surface_work,
                                                    &m_vPar);

        MFX_CHECK(*surface_out != nullptr, MFX_ERR_INVALID_HANDLE);

        SetFrameType(**surface_out);

        (*surface_out)->Info.FrameId.ViewId = 0; // (mfxU16)pFrame->m_viewId;

        bool isShouldUpdate = !(m_vFirstPar.mfx.FrameInfo.AspectRatioH || m_vFirstPar.mfx.FrameInfo.AspectRatioW);

        if(MFX_ROTATION_0 == m_vFirstPar.mfx.Rotation || MFX_ROTATION_180 == m_vFirstPar.mfx.Rotation)
        {
            (*surface_out)->Info.CropH = m_vPar.mfx.FrameInfo.CropH;
            (*surface_out)->Info.CropW = m_vPar.mfx.FrameInfo.CropW;
            (*surface_out)->Info.AspectRatioH = isShouldUpdate ? (mfxU16) 1 : m_vFirstPar.mfx.FrameInfo.AspectRatioH;
            (*surface_out)->Info.AspectRatioW = isShouldUpdate ? (mfxU16) 1 : m_vFirstPar.mfx.FrameInfo.AspectRatioW;
        }
        else
        {
            (*surface_out)->Info.CropH = m_vPar.mfx.FrameInfo.CropW;
            (*surface_out)->Info.CropW = m_vPar.mfx.FrameInfo.CropH;
            (*surface_out)->Info.AspectRatioH = isShouldUpdate ? (mfxU16) 1 : m_vFirstPar.mfx.FrameInfo.AspectRatioW;
            (*surface_out)->Info.AspectRatioW = isShouldUpdate ? (mfxU16) 1 : m_vFirstPar.mfx.FrameInfo.AspectRatioH;
        }

        (*surface_out)->Info.CropX = 0;
        (*surface_out)->Info.CropY = 0;

        mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(m_vFirstPar.ExtParam, m_vFirstPar.NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
        if (videoProcessing)
        {
            (*surface_out)->Info.CropH = videoProcessing->Out.CropH;
            (*surface_out)->Info.CropW = videoProcessing->Out.CropW;
            (*surface_out)->Info.CropX = videoProcessing->Out.CropX;
            (*surface_out)->Info.CropY = videoProcessing->Out.CropY;
        }

        isShouldUpdate = !(m_vFirstPar.mfx.FrameInfo.FrameRateExtD || m_vFirstPar.mfx.FrameInfo.FrameRateExtN);

        (*surface_out)->Info.FrameRateExtD = isShouldUpdate ? m_vPar.mfx.FrameInfo.FrameRateExtD : m_vFirstPar.mfx.FrameInfo.FrameRateExtD;
        (*surface_out)->Info.FrameRateExtN = isShouldUpdate ? m_vPar.mfx.FrameInfo.FrameRateExtN : m_vFirstPar.mfx.FrameInfo.FrameRateExtN;
        (*surface_out)->Info.PicStruct = (mfxU16) ((MFX_PICSTRUCT_PROGRESSIVE == m_vPar.mfx.FrameInfo.PicStruct) ?
                                                    (MFX_PICSTRUCT_PROGRESSIVE) :
                                                    (MFX_PICSTRUCT_FIELD_TFF));
        (*surface_out)->Data.TimeStamp = GetMfxTimeStamp(dst->GetTime());

        if ((MFX_TIME_STAMP_INVALID == (*surface_out)->Data.TimeStamp) && m_vPar.mfx.FrameInfo.FrameRateExtN)
        {
            (*surface_out)->Data.TimeStamp = ((mfxU64)m_frameOrder * m_vPar.mfx.FrameInfo.FrameRateExtD * MFX_TIME_STAMP_FREQUENCY) / m_vPar.mfx.FrameInfo.FrameRateExtN;
        }

        (*surface_out)->Data.FrameOrder = m_frameOrder;
        m_frameOrder++;

        (*surface_out)->Data.Corrupted = 0;
    }

    // prepare output structure
    pEntryPoint->pRoutine = &MJPEGDECODERoutine;
    pEntryPoint->pCompleteProc = &MJPEGCompleteProc;
    pEntryPoint->pState = this;
    pEntryPoint->pRoutineName = (char *)"DecodeMJPEG";

    return decoder->FillEntryPoint(pEntryPoint, surface_work, *surface_out);
}

mfxStatus VideoDECODEMJPEG::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);
    UMC::Status umcRes = UMC::UMC_OK;

    if (!m_isInit)
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);

    // make sure that there is a free task
    MFX_SAFE_CALL(decoder->CheckTaskAvailability(m_vPar.AsyncDepth ? m_vPar.AsyncDepth : m_core->GetAutoAsyncDepth()));

    bool allow_null_work_surface = SupportsVPLFeatureSet(*m_core);

    if (allow_null_work_surface)
    {
        MFX_CHECK_NULL_PTR1(surface_out);
    }
    else
    {
        MFX_CHECK_NULL_PTR2(surface_work, surface_out);
    }

    if (bs)
        MFX_SAFE_CALL(CheckBitstream(bs));

    *surface_out = nullptr;

    if (surface_work)
    {
        MFX_CHECK_COND(CheckFrameInfoCodecs(&surface_work->Info, MFX_CODEC_JPEG) == MFX_ERR_NONE);

        MFX_SAFE_CALL(CheckFrameData(surface_work));
    }

    mfxU32 numPic = 0;
    mfxU32 picToCollect = (MFX_PICSTRUCT_PROGRESSIVE == m_vPar.mfx.FrameInfo.PicStruct) ? 1 : 2;

    do
    {
        UMC::MJPEGVideoDecoderBaseMFX* pMJPEGVideoDecoder;
        MFX_SAFE_CALL(decoder->ReserveUMCDecoder(pMJPEGVideoDecoder, surface_work));

        MFXMediaDataAdapter src(bs);
        UMC::MediaDataEx *pSrcData;

        mfxExtBuffer* extbuf = (bs) ? GetExtendedBuffer(bs->ExtParam, bs->NumExtParam, MFX_EXTBUFF_DECODE_ERROR_REPORT) : NULL;

        if (extbuf)
        {
            reinterpret_cast<mfxExtDecodeErrorReport *>(extbuf)->ErrorTypes = 0;
            src.SetExtBuffer(extbuf);
        }

        if (!m_isHeaderFound && bs)
        {
            umcRes = pMJPEGVideoDecoder->FindStartOfImage(&src);
            if (umcRes != UMC::UMC_OK)
            {
                if(umcRes != UMC::UMC_ERR_NOT_ENOUGH_DATA)
                    decoder->ReleaseReservedTask();
                return (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA) ? MFX_ERR_MORE_DATA : ConvertUMCStatusToMfx(umcRes);
            }
            src.Save(bs);
            m_isHeaderFound = true;
        }

        if (!m_isHeaderParsed && bs)
        {
            umcRes = pMJPEGVideoDecoder->_GetFrameInfo((uint8_t*)src.GetDataPointer(), src.GetDataSize(), &src);

            if (umcRes != UMC::UMC_OK)
            {
                if(umcRes != UMC::UMC_ERR_NOT_ENOUGH_DATA)
                    decoder->ReleaseReservedTask();
                return (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA) ? MFX_ERR_MORE_DATA : ConvertUMCStatusToMfx(umcRes);
            }

            mfxVideoParam temp;
            pMJPEGVideoDecoder->FillVideoParam(&temp, false);

            if(m_maxCropW < temp.mfx.FrameInfo.CropW ||
                m_maxCropH < temp.mfx.FrameInfo.CropH)
            {
                decoder->ReleaseReservedTask();
                MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            }

            m_vPar.mfx.FrameInfo.CropW = temp.mfx.FrameInfo.CropW;
            m_vPar.mfx.FrameInfo.CropH = temp.mfx.FrameInfo.CropH;

            m_isHeaderParsed = true;
        }

        mfxU32 maxBitstreamSize = m_vPar.mfx.FrameInfo.CropW * m_vPar.mfx.FrameInfo.CropH;
        switch(m_vPar.mfx.JPEGChromaFormat)
        {
        case CHROMA_TYPE_YUV411:
        case CHROMA_TYPE_YUV420:
            maxBitstreamSize = maxBitstreamSize * 3 / 2;
            break;
        case CHROMA_TYPE_YUV422H_2Y:
        case CHROMA_TYPE_YUV422V_2Y:
        case CHROMA_TYPE_YUV422H_4Y:
        case CHROMA_TYPE_YUV422V_4Y:
            maxBitstreamSize = maxBitstreamSize * 2;
            break;
        case CHROMA_TYPE_YUV444:
            maxBitstreamSize = maxBitstreamSize * 3;
            break;
        case CHROMA_TYPE_YUV400:
        default:
            break;
        };

        pSrcData = m_frameConstructor->GetFrame(bs ? &src : 0, maxBitstreamSize);

        // update the bytes used in bitstream
        src.Save(bs);
        if (!pSrcData)
        {
            decoder->ReleaseReservedTask();
            MFX_RETURN(MFX_ERR_MORE_DATA);
        }

        m_isHeaderFound = false;
        m_isHeaderParsed = false;

        try
        {
            MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "Submit bitstream length with no start code: ", MFX_TRACE_FORMAT_I, pSrcData->GetDataSize());
            MFX_SAFE_CALL(decoder->AddPicture(pSrcData, numPic));
        }
        catch(const UMC::eUMC_Status& sts)
        {
            if(sts == UMC::UMC_ERR_INVALID_STREAM)
            {
                continue;
            }
            else
            {
                return ConvertUMCStatusToMfx(sts);
            }
        }
    // make sure, that we collected BOTH fields
    } while (picToCollect > numPic);

    // check if skipping is enabled
    m_skipCount += m_skipRate;
    if (JPEG_SKIP_BOUND <= m_skipCount)
    {
        m_skipCount -= JPEG_SKIP_BOUND;

        decoder->m_stat.NumSkippedFrame++;

        // it is time to skip a frame
        decoder->ReleaseReservedTask();
        MFX_RETURN(MFX_ERR_MORE_DATA);
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::DecodeFrame(mfxBitstream *, mfxFrameSurface1 *, mfxFrameSurface1 *)
{
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEG::GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts)
{
    if (!m_isInit)
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR3(ud, sz, ts);
    MFX_RETURN(MFX_ERR_UNSUPPORTED);
}

mfxStatus VideoDECODEMJPEG::GetPayload( mfxU64 *ts, mfxPayload *payload )
{
    TRACE_EVENT(MFX_TRACE_API_DECODE_GETPAYLOAD_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(payload));
    if (!m_isInit)
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR3(ts, payload, payload->Data);
    TRACE_EVENT(MFX_TRACE_API_DECODE_GETPAYLOAD_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(MFX_ERR_UNSUPPORTED));

    MFX_RETURN(MFX_ERR_UNSUPPORTED);
}

mfxStatus VideoDECODEMJPEG::SetSkipMode(mfxSkipMode mode)
{
    TRACE_EVENT(MFX_TRACE_API_DECODE_SETSKIPMODE_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(mode));
    // check error(s)
    if (!m_isInit)
    {
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);
    }

    // check if we reached bounds of skipping
    if (((JPEG_MIN_SKIP_RATE == m_skipRate) && (MFX_SKIPMODE_LESS == mode)) ||
        ((JPEG_MAX_SKIP_RATE == m_skipRate) && (MFX_SKIPMODE_MORE == mode)) ||
        ((JPEG_MIN_SKIP_RATE == m_skipRate) && (MFX_SKIPMODE_NOSKIP == mode)))
    {
        return MFX_WRN_VALUE_NOT_CHANGED;
    }

    // set new skip rate
    switch (mode)
    {
    case MFX_SKIPMODE_LESS:
        m_skipRate -= 1;
        break;

    case MFX_SKIPMODE_MORE:
        m_skipRate += 1;
        break;

    default:
        m_skipRate = 0;
        break;
    }
    TRACE_EVENT(MFX_TRACE_API_DECODE_SETSKIPMODE_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(MFX_ERR_NONE));

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEMJPEG::SetSkipMode(mfxSkipMode mode)

bool VideoDECODEMJPEG::IsSameVideoParam(mfxVideoParam * newPar, mfxVideoParam * oldPar)
{
    auto const mask =
          MFX_IOPATTERN_OUT_SYSTEM_MEMORY
        | MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    if ((newPar->IOPattern & mask) !=
        (oldPar->IOPattern & mask) )
    {
        return false;
    }

    if (newPar->Protected != oldPar->Protected)
    {
        return false;
    }

    if (newPar->AsyncDepth != oldPar->AsyncDepth)
    {
        return false;
    }

    mfxFrameAllocRequest requestOld;
    memset(&requestOld, 0, sizeof(requestOld));
    mfxFrameAllocRequest requestNew;
    memset(&requestNew, 0, sizeof(requestNew));

    mfxStatus mfxSts = QueryIOSurfInternal(m_core, oldPar, &requestOld);
    if (mfxSts != MFX_ERR_NONE)
        return false;

    mfxSts = QueryIOSurfInternal(m_core, newPar, &requestNew);
    if (mfxSts != MFX_ERR_NONE)
        return false;

    if (newPar->mfx.FrameInfo.Height > oldPar->mfx.FrameInfo.Height)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.Width > oldPar->mfx.FrameInfo.Width)
    {
        return false;
    }

    if (m_response.NumFrameActual)
    {
        if (requestNew.NumFrameMin > m_response.NumFrameActual)
            return false;
    }
    else
    {
        if (requestNew.NumFrameMin > requestOld.NumFrameMin || requestNew.Type != requestOld.Type)
            return false;
    }

    if (newPar->mfx.FrameInfo.ChromaFormat != oldPar->mfx.FrameInfo.ChromaFormat)
    {
        return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MFX_JPEG_Utility implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

eMFXPlatform MFX_JPEG_Utility::GetPlatform(VideoCORE * core, mfxVideoParam * par)
{
    eMFXPlatform platform = core->GetPlatformType();

    if (platform != MFX_PLATFORM_SOFTWARE)
    {
        if (MFX_ERR_NONE != core->IsGuidSupported(sDXVA2_Intel_IVB_ModeJPEG_VLD_NoFGT, par))
        {
            return MFX_PLATFORM_SOFTWARE;
        }

        bool needVpp = false;
        if (par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF ||
            par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF)
        {
            needVpp = true;
        }

        mfxFrameAllocRequest request;
        memset(&request, 0, sizeof(request));
        request.Info = par->mfx.FrameInfo;
        bool isPostProcEnable = VideoDECODEMJPEGBase_HW::isVideoPostprocEnabled(core);

        VideoDECODEMJPEGBase_HW::AdjustFourCC(&request.Info, &par->mfx, core->GetVAType(),
                                              isPostProcEnable, &needVpp);

        if (needVpp)
        {

            if (MFX_ERR_NONE != VideoDECODEMJPEGBase_HW::CheckVPPCaps(core, par))
            {
                return MFX_PLATFORM_SOFTWARE;
            }
        }
    }

    return platform;
}

bool MFX_JPEG_Utility::IsFormatSupport(mfxVideoParam * in)
{
    bool support = false;

    mfxU32 fourCC = in->mfx.FrameInfo.FourCC;
    mfxU16 chromaFormat = in->mfx.JPEGChromaFormat;
    mfxU16 colorFormat  = in->mfx.JPEGColorFormat;

    if (colorFormat == MFX_JPEG_COLORFORMAT_RGB && chromaFormat != MFX_CHROMAFORMAT_YUV444)
        return support;

    switch (chromaFormat)
    {
    case MFX_CHROMAFORMAT_MONOCHROME:
        if (fourCC == 0 || fourCC == MFX_FOURCC_NV12 || fourCC == MFX_FOURCC_YUV400 || fourCC == MFX_FOURCC_YUY2 || fourCC == MFX_FOURCC_RGB4)
            support = true;
        break;
    case MFX_CHROMAFORMAT_YUV420:
        if (in->mfx.InterleavedDec == MFX_SCANTYPE_NONINTERLEAVED && fourCC != MFX_FOURCC_IMC3)
            break;
        if (fourCC == MFX_FOURCC_IMC3 || fourCC == MFX_FOURCC_NV12 || fourCC == MFX_FOURCC_YUY2 || fourCC == MFX_FOURCC_UYVY || fourCC == MFX_FOURCC_RGB4)
            support = true;
        break;
    case MFX_CHROMAFORMAT_YUV411:
        if (in->mfx.InterleavedDec == MFX_SCANTYPE_NONINTERLEAVED && fourCC != MFX_FOURCC_YUV411)
            break;
        if (fourCC == MFX_FOURCC_YUV411 || fourCC == MFX_FOURCC_NV12)
            support = true;
        break;
    case MFX_CHROMAFORMAT_YUV422H:
        if (in->mfx.InterleavedDec == MFX_SCANTYPE_NONINTERLEAVED && fourCC != MFX_FOURCC_YUV422H)
            break;
        if ((fourCC == MFX_FOURCC_YUY2 || fourCC == MFX_FOURCC_YUV422H || fourCC == MFX_FOURCC_NV12 || fourCC == MFX_FOURCC_UYVY || fourCC == MFX_FOURCC_RGB4))
            support = true;
        break;
    case MFX_CHROMAFORMAT_YUV422V:
        if (in->mfx.InterleavedDec == MFX_SCANTYPE_NONINTERLEAVED && fourCC != MFX_FOURCC_YUV422V)
            break;
        if ((fourCC == MFX_FOURCC_YUV422V || fourCC == MFX_FOURCC_NV12 || fourCC == MFX_FOURCC_YUY2 || fourCC == MFX_FOURCC_RGB4))
            support = true;
        break;
    case MFX_CHROMAFORMAT_YUV444:
        if (in->mfx.InterleavedDec == MFX_SCANTYPE_NONINTERLEAVED &&
            ((colorFormat == MFX_JPEG_COLORFORMAT_YCbCr && fourCC != MFX_FOURCC_YUV444) ||
             (colorFormat == MFX_JPEG_COLORFORMAT_RGB && fourCC != MFX_FOURCC_RGBP)))
            break;
        if (fourCC == MFX_FOURCC_RGB4 || fourCC == MFX_FOURCC_NV12 || fourCC == MFX_FOURCC_YUY2)
            support = true;
        else if (colorFormat == MFX_JPEG_COLORFORMAT_RGB && (fourCC == MFX_FOURCC_RGBP || fourCC == MFX_FOURCC_BGRP))
            support = true;
        else if (colorFormat == MFX_JPEG_COLORFORMAT_YCbCr && fourCC == MFX_FOURCC_YUV444)
            support = true;
        break;
    default:
        break;
    }

    return support;
}


mfxStatus MFX_JPEG_Utility::Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out, eMFXHWType type)
{
    MFX_CHECK_NULL_PTR1(out);
    mfxStatus  sts = MFX_ERR_NONE;

    if (in == out)
    {
        mfxVideoParam in1 = *in;
        return Query(core, &in1, out, type);
    }

    memset(&out->mfx, 0, sizeof(mfxInfoMFX));

    if (in)
    {
        if (in->mfx.CodecId == MFX_CODEC_JPEG)
            out->mfx.CodecId = in->mfx.CodecId;

        if ((MFX_PROFILE_JPEG_BASELINE == in->mfx.CodecProfile))
            out->mfx.CodecProfile = in->mfx.CodecProfile;

        switch (in->mfx.CodecLevel)
        {
        case MFX_LEVEL_UNKNOWN:
            out->mfx.CodecLevel = in->mfx.CodecLevel;
            break;
        }

        if (in->mfx.NumThread < 128)
            out->mfx.NumThread = in->mfx.NumThread;

        if (in->AsyncDepth < MFX_MAX_ASYNC_DEPTH_VALUE) // Actually AsyncDepth > 5-7 is for debugging only.
            out->AsyncDepth = in->AsyncDepth;

        if (in->IOPattern)
        {
            if ((in->IOPattern == MFX_IOPATTERN_OUT_SYSTEM_MEMORY) || (in->IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY))
                out->IOPattern = in->IOPattern;
            else
                sts = MFX_STS_TRACE(MFX_ERR_UNSUPPORTED);
        }

        if (IsFormatSupport(in))
        {
            out->mfx.FrameInfo.FourCC = in->mfx.FrameInfo.FourCC;
            out->mfx.FrameInfo.ChromaFormat = in->mfx.FrameInfo.ChromaFormat;
        }
        else
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        out->mfx.FrameInfo.Width  = mfx::align2_value(in->mfx.FrameInfo.Width, 0x10);
        out->mfx.FrameInfo.Height = mfx::align2_value(in->mfx.FrameInfo.Height,
            (in->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 0x8 : 0x10);

        if (in->mfx.FrameInfo.CropX <= out->mfx.FrameInfo.Width)
            out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX;

        if (in->mfx.FrameInfo.CropY <= out->mfx.FrameInfo.Height)
            out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY;

        if (out->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW <= out->mfx.FrameInfo.Width)
            out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;

        if (out->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH <= out->mfx.FrameInfo.Height)
            out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;

        out->mfx.FrameInfo.FrameRateExtN = in->mfx.FrameInfo.FrameRateExtN;
        out->mfx.FrameInfo.FrameRateExtD = in->mfx.FrameInfo.FrameRateExtD;

        out->mfx.FrameInfo.AspectRatioW = in->mfx.FrameInfo.AspectRatioW;
        out->mfx.FrameInfo.AspectRatioH = in->mfx.FrameInfo.AspectRatioH;

        switch (in->mfx.FrameInfo.PicStruct)
        {
        case MFX_PICSTRUCT_PROGRESSIVE:
        case MFX_PICSTRUCT_FIELD_TFF:
        case MFX_PICSTRUCT_FIELD_BFF:
        //case MFX_PICSTRUCT_FIELD_REPEATED:
        //case MFX_PICSTRUCT_FRAME_DOUBLING:
        //case MFX_PICSTRUCT_FRAME_TRIPLING:
            out->mfx.FrameInfo.PicStruct = in->mfx.FrameInfo.PicStruct;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        switch (in->mfx.JPEGChromaFormat)
        {
        case MFX_CHROMAFORMAT_MONOCHROME:
        case MFX_CHROMAFORMAT_YUV420:
        case MFX_CHROMAFORMAT_YUV422:
        case MFX_CHROMAFORMAT_YUV444:
        case MFX_CHROMAFORMAT_YUV411:
        case MFX_CHROMAFORMAT_YUV422V:
            out->mfx.JPEGChromaFormat = in->mfx.JPEGChromaFormat;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        switch (in->mfx.JPEGColorFormat)
        {
        case MFX_JPEG_COLORFORMAT_UNKNOWN:
        case MFX_JPEG_COLORFORMAT_YCbCr:
        case MFX_JPEG_COLORFORMAT_RGB:
            out->mfx.JPEGColorFormat = in->mfx.JPEGColorFormat;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        switch (in->mfx.InterleavedDec)
        {
        case MFX_SCANTYPE_UNKNOWN:
        case MFX_SCANTYPE_INTERLEAVED:
        case MFX_SCANTYPE_NONINTERLEAVED:
            out->mfx.InterleavedDec = in->mfx.InterleavedDec;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        switch (in->mfx.Rotation)
        {
        case MFX_ROTATION_0:
        case MFX_ROTATION_90:
        case MFX_ROTATION_180:
        case MFX_ROTATION_270:
            out->mfx.Rotation = in->mfx.Rotation;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        if(in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
           in->mfx.Rotation != MFX_ROTATION_0)
            sts = MFX_ERR_UNSUPPORTED;

        mfxStatus stsExt = CheckDecodersExtendedBuffers(in);
        if (stsExt < MFX_ERR_NONE)
            sts = MFX_ERR_UNSUPPORTED;

        if (in->Protected)
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (GetPlatform(core, out) != core->GetPlatformType() && sts == MFX_ERR_NONE)
        {
            assert(GetPlatform(core, out) == MFX_PLATFORM_SOFTWARE);
            sts = MFX_WRN_PARTIAL_ACCELERATION;
        }

        if (sts == MFX_ERR_NONE)
        {
            /*SFC*/
            mfxExtDecVideoProcessing * videoProcessingTargetIn = (mfxExtDecVideoProcessing *)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
            mfxExtDecVideoProcessing * videoProcessingTargetOut = (mfxExtDecVideoProcessing *)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
            if (videoProcessingTargetIn && videoProcessingTargetOut)
            {
                // limits are from media_driver/agnostic/common/hw/mhw_sfc.h
                // get them via API
                const short unsigned MHW_SFC_MIN_HEIGHT        = 128;
                const short unsigned MHW_SFC_MIN_WIDTH         = 128;
                const short unsigned MHW_SFC_MAX_HEIGHT        = 4096;
                const short unsigned MHW_SFC_MAX_WIDTH         = 4096;

                if ( (MFX_PICSTRUCT_PROGRESSIVE == in->mfx.FrameInfo.PicStruct) &&
                     (in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) &&
                     (videoProcessingTargetIn->Out.Width  >= MHW_SFC_MIN_WIDTH  &&
                      videoProcessingTargetIn->Out.Width  <= MHW_SFC_MAX_WIDTH)  &&
                     (videoProcessingTargetIn->Out.Height >= MHW_SFC_MIN_HEIGHT &&
                      videoProcessingTargetIn->Out.Height <= MHW_SFC_MAX_HEIGHT) &&
                     // only conversion to RGB4 is supported by driver
                     (in->mfx.FrameInfo.FourCC == MFX_FOURCC_RGB4|| videoProcessingTargetIn->Out.FourCC == MFX_FOURCC_RGB4) &&
                     // resize is not supported by driver
                     (videoProcessingTargetIn->In.CropX == videoProcessingTargetIn->Out.CropX &&
                      videoProcessingTargetIn->In.CropY == videoProcessingTargetIn->Out.CropY &&
                      videoProcessingTargetIn->In.CropW == videoProcessingTargetIn->Out.CropW &&
                      videoProcessingTargetIn->In.CropH == videoProcessingTargetIn->Out.CropH)
                    )
                {
                    *videoProcessingTargetOut = *videoProcessingTargetIn;
                }
                else
                {
                    sts = MFX_ERR_UNSUPPORTED;
                }
            }
        }
    }
    else
    {
        out->mfx.CodecId = MFX_CODEC_JPEG;
        out->mfx.CodecProfile = 1;
        out->mfx.CodecLevel = 1;

        out->mfx.NumThread = 1;

        out->AsyncDepth = 1;

        // mfxFrameInfo
        out->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        out->mfx.FrameInfo.Width = 1;
        out->mfx.FrameInfo.Height = 1;

        //out->mfx.FrameInfo.CropX = 1;
        //out->mfx.FrameInfo.CropY = 1;
        //out->mfx.FrameInfo.CropW = 1;
        //out->mfx.FrameInfo.CropH = 1;

        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;

        out->mfx.FrameInfo.AspectRatioW = 1;
        out->mfx.FrameInfo.AspectRatioH = 1;

        out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

        out->mfx.JPEGChromaFormat = 1;
        out->mfx.JPEGColorFormat = 1;
        out->mfx.Rotation = 1;

        if (type == MFX_HW_UNKNOWN)
        {
            out->IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        }
        else
        {
            out->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        }
    }

    return sts;
}

bool MFX_JPEG_Utility::CheckVideoParam(mfxVideoParam *in, eMFXHWType )
{
    if (!in)
        return false;

    if (in->Protected)
       return false;

    if (MFX_CODEC_JPEG != in->mfx.CodecId)
        return false;

    if (in->mfx.FrameInfo.Width % 16)
        return false;

    if (in->mfx.FrameInfo.Height % 8)
        return false;

    // both zero or not zero
    if ((in->mfx.FrameInfo.AspectRatioW || in->mfx.FrameInfo.AspectRatioH) && !(in->mfx.FrameInfo.AspectRatioW && in->mfx.FrameInfo.AspectRatioH))
        return false;

    switch (in->mfx.FrameInfo.PicStruct)
    {
    case MFX_PICSTRUCT_PROGRESSIVE:
    case MFX_PICSTRUCT_FIELD_TFF:
    case MFX_PICSTRUCT_FIELD_BFF:
        break;
    default:
        return false;
    }

    if(in->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
       in->mfx.Rotation != MFX_ROTATION_0)
        return false;

    if (   !(in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        && !(in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return false;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return false;

    return true;
}

mfxStatus VideoDECODEMJPEG::GetSurface(mfxFrameSurface1* & surface, mfxSurfaceHeader* import_surface)
{
    MFX_CHECK(decoder && decoder->m_surface_source, MFX_ERR_NOT_INITIALIZED);

    return decoder->m_surface_source->GetSurface(surface, import_surface);
}

VideoDECODEMJPEGBase::VideoDECODEMJPEGBase()
{
     memset(&m_stat, 0, sizeof(m_stat));
}

mfxStatus VideoDECODEMJPEGBase::GetVideoParam(mfxVideoParam *par, UMC::MJPEGVideoDecoderBaseMFX * mjpegDecoder)
{
    TRACE_EVENT(MFX_TRACE_API_DECODE_GETVIDEOPARAM_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(par ? par->mfx.FrameInfo.Width : 0,
        par ? par->mfx.FrameInfo.Height : 0, par ? par->mfx.CodecId : 0));

    mfxExtJPEGQuantTables*    jpegQT = (mfxExtJPEGQuantTables*)   mfx::GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_QT );
    mfxExtJPEGHuffmanTables*  jpegHT = (mfxExtJPEGHuffmanTables*) mfx::GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );

    if(!jpegQT && !jpegHT)
        return MFX_ERR_NONE;

    UMC::Status umcRes = UMC::UMC_OK;

    if(jpegQT)
    {
        umcRes = mjpegDecoder->FillQuantTableExtBuf(jpegQT);
        if (umcRes != UMC::UMC_OK)
            return ConvertUMCStatusToMfx(umcRes);
    }

    if(jpegHT)
    {
        umcRes = mjpegDecoder->FillHuffmanTableExtBuf(jpegHT);
        if (umcRes != UMC::UMC_OK)
            return ConvertUMCStatusToMfx(umcRes);
    }

    TRACE_EVENT(MFX_TRACE_API_DECODE_GETVIDEOPARAM_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(MFX_ERR_NONE));

    return MFX_ERR_NONE;
}

VideoDECODEMJPEGBase_HW::VideoDECODEMJPEGBase_HW()
{
    m_pMJPEGVideoDecoder.reset(new UMC::MJPEGVideoDecoderMFX_HW()); // HW
    m_va = 0;
    m_dst = 0;
    m_numPic = 0;
    m_needVpp = false;
}

mfxStatus VideoDECODEMJPEGBase_HW::Init(mfxVideoParam *decPar, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxFrameAllocRequest *request_internal,
                                            bool, VideoCORE *core)
{
    ConvertMFXParamsToUMC(decPar, &umcVideoParams);
    umcVideoParams.numThreads = m_vPar.mfx.NumThread;

    mfxStatus mfxSts = core->CreateVA(decPar, request, response, m_surface_source.get());
    if (mfxSts < MFX_ERR_NONE)
        return mfxSts;

    core->GetVA((mfxHDL*)&m_va, MFX_MEMTYPE_FROM_DECODE);

    m_pMJPEGVideoDecoder->SetFrameAllocator(m_surface_source.get());
    umcVideoParams.pVideoAccelerator = m_va;

    UMC::Status umcSts = m_pMJPEGVideoDecoder->Init(&umcVideoParams);
    if (umcSts != UMC::UMC_OK)
    {
        return ConvertUMCStatusToMfx(umcSts);
    }
    m_pMJPEGVideoDecoder->SetFourCC(request_internal->Info.FourCC);
    m_numPic = 0;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEGBase_HW::Reset(mfxVideoParam *par)
{
    m_pMJPEGVideoDecoder->Reset();
    m_numPic = 0;

    m_vPar = *par;

    {
        std::lock_guard<std::mutex> guard(m_guard);

        mfxU32 picToCollect = (MFX_PICSTRUCT_PROGRESSIVE == m_vPar.mfx.FrameInfo.PicStruct) ?
            (1) : (2);

        while(!m_dsts.empty())
        {
            for(mfxU32 i=0; i<picToCollect; i++)
                m_pMJPEGVideoDecoder->CloseFrame(&(m_dsts.back()), i);
            delete [] m_dsts.back();
            m_dsts.pop_back();
        }
    }

    if (m_surface_source->Reset() != UMC::UMC_OK)
    {
        MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
    }

    memset(&m_stat, 0, sizeof(mfxDecodeStat));
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEGBase_HW::Close(void)
{
    if (!m_pMJPEGVideoDecoder.get())
        MFX_RETURN(MFX_ERR_NOT_INITIALIZED);

    m_pMJPEGVideoDecoder->Close();
    m_numPic = 0;

    {
        std::lock_guard<std::mutex> guard(m_guard);

        mfxU32 picToCollect = (MFX_PICSTRUCT_PROGRESSIVE == m_vPar.mfx.FrameInfo.PicStruct) ?
            (1) : (2);

        while(!m_dsts.empty())
        {
            for(mfxU32 i=0; i<picToCollect; i++)
                m_pMJPEGVideoDecoder->CloseFrame(&(m_dsts.back()), i);
            delete [] m_dsts.back();
            m_dsts.pop_back();
        }
    }

    memset(&m_stat, 0, sizeof(mfxDecodeStat));

    m_va = 0;
    m_surface_source->Close();

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEGBase_HW::GetVideoParam(mfxVideoParam *par)
{
    return VideoDECODEMJPEGBase::GetVideoParam(par, m_pMJPEGVideoDecoder.get());
}

mfxStatus VideoDECODEMJPEGBase_HW::CheckVPPCaps(VideoCORE * core, mfxVideoParam * par)
{
    VideoVppJpeg cc(core, false);

    return cc.Init(par);
}

mfxU32 VideoDECODEMJPEGBase_HW::AdjustFrameAllocRequest(mfxFrameAllocRequest *request,
                                               mfxInfoMFX *info,
                                               VideoCORE *core,
                                               bool isPostProcEnable)
{
    bool needVpp = false;

    // update request in case of interlaced stream
    if(request->Info.PicStruct == MFX_PICSTRUCT_FIELD_TFF || request->Info.PicStruct == MFX_PICSTRUCT_FIELD_BFF)
    {
        request->Info.Height >>= 1;
        request->Info.CropH >>= 1;
        request->NumFrameMin <<= 1;
        request->NumFrameSuggested <<= 1;

        needVpp = true;
    }

    // set FourCC
    AdjustFourCC(&request->Info, info, core->GetVAType(), isPostProcEnable, &needVpp);

#ifdef MFX_ENABLE_MJPEG_ROTATE_VPP
    if(info->Rotation == MFX_ROTATION_90 || info->Rotation == MFX_ROTATION_180 || info->Rotation == MFX_ROTATION_270)
    {
        needVpp = true;
    }

    if(info->Rotation == MFX_ROTATION_90 || info->Rotation == MFX_ROTATION_270)
    {
        std::swap(request->Info.Height, request->Info.Width);
        std::swap(request->Info.AspectRatioH, request->Info.AspectRatioW);
        std::swap(request->Info.CropH, request->Info.CropW);
        std::swap(request->Info.CropY, request->Info.CropX);
    }
#else  // MFX_ENABLE_MJPEG_ROTATE_VPP

    // WA for rotation of unaligned images
    mfxU16 mcuWidth;
    mfxU16 mcuHeight;
    mfxU16 paddingWidth;
    mfxU16 paddingHeight;

    switch(info->JPEGChromaFormat)
    {
    case MFX_CHROMAFORMAT_YUV411:
        mcuWidth  = 32;
        mcuHeight = 8;
        break;
    case MFX_CHROMAFORMAT_YUV420:
        mcuWidth  = 16;
        mcuHeight = 16;
        break;
    case MFX_CHROMAFORMAT_YUV422H:
        mcuWidth  = 16;
        mcuHeight = 8;
        break;
    case MFX_CHROMAFORMAT_YUV422V:
        mcuWidth  = 8;
        mcuHeight = 16;
        break;
    case MFX_CHROMAFORMAT_YUV400:
    case MFX_CHROMAFORMAT_YUV444:
    default:
        mcuWidth  = 8;
        mcuHeight = 8;
        break;
    }

    if(info->Rotation == MFX_ROTATION_90 || info->Rotation == MFX_ROTATION_270)
        std::swap(mcuWidth, mcuHeight);

    paddingWidth = (mfxU16)((2<<16) - request->Info.CropW) % mcuWidth;
    paddingHeight = (mfxU16)((2<<16) - request->Info.CropH) % mcuHeight;

    switch(info->Rotation)
    {
    case MFX_ROTATION_90:
        request->Info.CropX = paddingWidth;
        request->Info.CropW = request->Info.CropW + paddingWidth;
        break;
    case MFX_ROTATION_180:
        request->Info.CropX = paddingWidth;
        request->Info.CropW = request->Info.CropW + paddingWidth;
        request->Info.CropY = paddingHeight;
        request->Info.CropH = request->Info.CropH + paddingHeight;
        break;
    case MFX_ROTATION_270:
        request->Info.CropY = paddingHeight;
        request->Info.CropH = request->Info.CropH + paddingHeight;
        break;
    }

#endif // MFX_ENABLE_MJPEG_ROTATE_VPP
    m_needVpp = needVpp;

    return m_needVpp ? 1 : 0;
}

bool VideoDECODEMJPEGBase_HW::isVideoPostprocEnabled(VideoCORE * core)
{
    // check sfc capability for some platform may not support sfc
    VAProfile vaProfile = VAProfileJPEGBaseline;
    VAEntrypoint vaEntrypoint = VAEntrypointVLD;
    VAConfigAttrib vaAttrib;
    memset(&vaAttrib, 0, sizeof(vaAttrib));
    vaAttrib.type = VAConfigAttribDecProcessing;
    vaAttrib.value = 0;
    VADisplay vaDisplay;
    mfxStatus mfxSts = core->GetHandle(MFX_HANDLE_VA_DISPLAY, &vaDisplay);
    if (MFX_ERR_NONE != mfxSts)
        return false;

    // Had to duplicate that functionality here, as LinuxVideoAccelerator::Init() has not
    // called at this point.
    VAStatus vaStatus = vaGetConfigAttributes(vaDisplay, vaProfile, vaEntrypoint, &vaAttrib, 1);
    if (VA_STATUS_SUCCESS != vaStatus)
        return false;

    return vaAttrib.value == VA_DEC_PROCESSING;
}

void VideoDECODEMJPEGBase_HW::AdjustFourCC(mfxFrameInfo *requestFrameInfo, const mfxInfoMFX *info, eMFXVAType vaType, bool isVideoPostprocEnabled, bool *needVpp)
{
    (void)vaType;

    if (info->JPEGColorFormat == MFX_JPEG_COLORFORMAT_UNKNOWN || info->JPEGColorFormat == MFX_JPEG_COLORFORMAT_YCbCr)
    {
        switch(info->JPEGChromaFormat)
        {
        case MFX_CHROMAFORMAT_MONOCHROME:
            if (requestFrameInfo->FourCC == MFX_FOURCC_NV12 ||
                requestFrameInfo->FourCC == MFX_FOURCC_YUY2 ||
                (requestFrameInfo->FourCC == MFX_FOURCC_RGB4 && !isVideoPostprocEnabled))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_YUV400;
                *needVpp = true;
            }
            break;
        case MFX_CHROMAFORMAT_YUV420:
            if (requestFrameInfo->FourCC == MFX_FOURCC_RGB4 && !isVideoPostprocEnabled)
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
                *needVpp = true;
            }
            break;
        case MFX_CHROMAFORMAT_YUV411:
            if (requestFrameInfo->FourCC == MFX_FOURCC_NV12 || requestFrameInfo->FourCC == MFX_FOURCC_RGB4)
            {
                requestFrameInfo->FourCC = MFX_FOURCC_YUV411;
                *needVpp = true;
            }
            break;
        case MFX_CHROMAFORMAT_YUV422H:
            break;
        case MFX_CHROMAFORMAT_YUV422V:
            // 422V can not do hw decode to YUY2/RGB4 directly, so use VPP to do csc
            if (info->Rotation == MFX_ROTATION_0 &&
               (requestFrameInfo->FourCC == MFX_FOURCC_YUY2 || requestFrameInfo->FourCC == MFX_FOURCC_RGB4))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_NV12;
                *needVpp = true;
            }
            break;
        case MFX_CHROMAFORMAT_YUV444:
            if (info->Rotation == MFX_ROTATION_0 &&
                // for YUV444 jpeg, decoded stream is YUV444P from driver,
                // to get NV12/YUY2 we must use VPP
                (requestFrameInfo->FourCC == MFX_FOURCC_NV12 ||
                 requestFrameInfo->FourCC == MFX_FOURCC_YUY2 ||
                 (requestFrameInfo->FourCC == MFX_FOURCC_RGB4 && !isVideoPostprocEnabled)))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_YUV444;
                *needVpp = true;
            }
            break;
        default:
            assert(false);
            break;
        }
    }
    else if(info->JPEGColorFormat == MFX_JPEG_COLORFORMAT_RGB)
    {
        if(info->JPEGChromaFormat == MFX_CHROMAFORMAT_YUV444)
            if( vaType == MFX_HW_VAAPI &&
                info->Rotation == MFX_ROTATION_0 &&
                info->InterleavedDec == MFX_SCANTYPE_INTERLEAVED &&
                !(*needVpp))
            {
                requestFrameInfo->FourCC = MFX_FOURCC_RGBP;
                *needVpp = true;
            }
    }
    return;
}


mfxStatus VideoDECODEMJPEGBase_HW::RunThread(void *params, mfxU32, mfxU32 )
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(params);

    ThreadTaskInfoJpeg * info = (ThreadTaskInfoJpeg *)params;

    if (m_needVpp)
    {
        if(info->needCheckVppStatus)
        {
            mfxSts = ((SurfaceSourceJPEG*)m_surface_source.get())->CheckPreparingToOutput(info->surface_out,
                                                                                                              info->dst,
                                                                                                              &m_vPar,
                                                                                                              (mfxU16)info->vppTaskID);

            if(mfxSts != MFX_TASK_DONE)
            {
                return mfxSts;
            }
            info->needCheckVppStatus = false;
        }

        mfxU16 corruptedField = 0;
        if(info->numDecodeTasksToCheck == 2)
        {
            mfxSts = m_pMJPEGVideoDecoder->CheckStatusReportNumber(info->decodeTaskID-1, &corruptedField);
            if(mfxSts != MFX_TASK_DONE)
            {
                return mfxSts;
            }
            info->numDecodeTasksToCheck--;
            info->surface_out->Data.Corrupted |= corruptedField;
        }

        if(info->numDecodeTasksToCheck == 1)
        {
            mfxSts = m_pMJPEGVideoDecoder->CheckStatusReportNumber(info->decodeTaskID, &corruptedField);
            if(mfxSts != MFX_TASK_DONE)
            {
                return mfxSts;
            }
            info->numDecodeTasksToCheck--;
            info->surface_out->Data.Corrupted |= corruptedField;
        }
    }
    else
    {
        mfxSts = m_pMJPEGVideoDecoder->CheckStatusReportNumber(info->decodeTaskID, &(info->surface_out->Data.Corrupted));
        if(mfxSts != MFX_TASK_DONE)
        {
            return mfxSts;
        }

        mfxSts = m_surface_source->PrepareToOutput(info->surface_out, info->dst->GetFrameMID(), &m_vPar, mfxU32(~MFX_COPY_USE_VACOPY_ANY));
        if (mfxSts < MFX_ERR_NONE)
        {
            return mfxSts;
        }
    }

    info = 0;

    return MFX_TASK_DONE;
}

mfxStatus VideoDECODEMJPEGBase_HW::ReserveUMCDecoder(UMC::MJPEGVideoDecoderBaseMFX* &pMJPEGVideoDecoder, mfxFrameSurface1 *surf)
{
    pMJPEGVideoDecoder = nullptr;
    MFX_SAFE_CALL(m_surface_source->SetCurrentMFXSurface(surf));

    if (m_numPic == 0)
    {
        int picToCollect = (MFX_PICSTRUCT_PROGRESSIVE == m_vPar.mfx.FrameInfo.PicStruct) ?
                       (1) : (2);
        delete[] m_dst;
        m_dst = new UMC::FrameData[picToCollect];
    }

    pMJPEGVideoDecoder = m_pMJPEGVideoDecoder.get();
    return MFX_ERR_NONE;
}

void VideoDECODEMJPEGBase_HW::ReleaseReservedTask()
{
    if (m_numPic == 0)
    {
        delete[] m_dst;
        m_dst = 0;
        m_numPic = 0;
    }
}

mfxStatus VideoDECODEMJPEGBase_HW::AddPicture(UMC::MediaDataEx *pSrcData, mfxU32 & numPic)
{
    mfxU32 fieldPos = m_numPic;
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);

    if (MFX_PICSTRUCT_FIELD_BFF == m_vPar.mfx.FrameInfo.PicStruct)
    {
        // change field order in BFF case
        fieldPos ^= 1;
    }

    UMC::Status umcRes = UMC::UMC_OK;
#ifndef MFX_ENABLE_MJPEG_ROTATE_VPP
    switch(m_vPar.mfx.Rotation)
    {
    case MFX_ROTATION_0:
        umcRes = m_pMJPEGVideoDecoder->SetRotation(0);
        break;
    case MFX_ROTATION_90:
        umcRes = m_pMJPEGVideoDecoder->SetRotation(90);
        break;
    case MFX_ROTATION_180:
        umcRes = m_pMJPEGVideoDecoder->SetRotation(180);
        break;
    case MFX_ROTATION_270:
        umcRes = m_pMJPEGVideoDecoder->SetRotation(270);
        break;
    }
#else
    umcRes = m_pMJPEGVideoDecoder->SetRotation(0);
#endif
    if (umcRes != UMC::UMC_OK)
    {
        delete[] m_dst;
        m_dst = 0;
        return ConvertUMCStatusToMfx(umcRes);
    }

    umcRes = m_pMJPEGVideoDecoder->GetFrame(pSrcData, &m_dst, fieldPos);

    mfxStatus sts = MFX_ERR_NONE;

    // convert status
    if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA || umcRes == UMC::UMC_ERR_SYNC)
    {
        if (m_numPic == 0)
        {
            delete[] m_dst;
            m_dst = 0;
        }

        sts = MFX_ERR_MORE_DATA;
    }
    else
    {
        if (umcRes != UMC::UMC_OK)
        {
            delete[] m_dst;
            m_dst = 0;
            sts = ConvertUMCStatusToMfx(umcRes);
        }
    }

    if (!(umcRes == UMC::UMC_OK && m_dst))    // return frame to display
        return sts;

    m_numPic++;
    numPic = m_numPic;

    return sts;
}

mfxStatus VideoDECODEMJPEGBase_HW::AllocateFrameData(UMC::FrameData *&data)
{
    std::lock_guard<std::mutex> guard(m_guard);
    m_dsts.push_back(m_dst);
    data = m_dst;
    m_dst = 0;
    m_numPic = 0;
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEGBase_HW::FillEntryPoint(MFX_ENTRY_POINT *pEntryPoint, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_out)
{
    mfxU16 taskId = 0;

    // add lock to avoid m_dsts is modified in CompleteTask
    std::lock_guard<std::mutex> guard(m_guard);

    if (m_dsts.empty())
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);

    UMC::FrameData *dst = m_dsts.back();
    if (m_needVpp)
    {
        UMC::ConvertInfo * convertInfo = m_pMJPEGVideoDecoder->GetConvertInfo();
        JPEG_Info info;
        info.colorFormat = convertInfo->colorFormat;
        info.UOffset = convertInfo->UOffset;
        info.VOffset = convertInfo->VOffset;

        ((SurfaceSourceJPEG*)m_surface_source.get())->SetJPEGInfo(&info);

        // decoding is ready. prepare to output:
        mfxStatus mfxSts = ((SurfaceSourceJPEG*)m_surface_source.get())->StartPreparingToOutput(surface_out, dst, &m_vPar, &taskId);
        if (mfxSts < MFX_ERR_NONE)
        {
            return mfxSts;
        }
    }

    ThreadTaskInfoJpeg * info = new ThreadTaskInfoJpeg();
    info->surface_work = surface_work;
    info->surface_out = surface_out;
    info->decodeTaskID = m_pMJPEGVideoDecoder->GetStatusReportNumber();
    info->vppTaskID = (mfxU32)taskId;
    info->needCheckVppStatus = m_needVpp;
    info->numDecodeTasksToCheck = MFX_PICSTRUCT_PROGRESSIVE == m_vPar.mfx.FrameInfo.PicStruct ? 1 : 2;
    info->dst = dst;

    pEntryPoint->requiredNumThreads = m_vPar.mfx.NumThread;
    pEntryPoint->pParam = info;
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEGBase_HW::CheckTaskAvailability(mfxU32 maxTaskNumber)
{
    std::lock_guard<std::mutex> guard(m_guard);

    if (m_dsts.size() >= maxTaskNumber)
    {
        return MFX_WRN_DEVICE_BUSY;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEMJPEGBase_HW::CompleteTask(void *pParam, mfxStatus )
{
    ThreadTaskInfoJpeg * info = (ThreadTaskInfoJpeg *)pParam;

    std::lock_guard<std::mutex> guard(m_guard);

    mfxI32 index = -1;
    for (size_t i = 0; i < m_dsts.size(); i++)
        if(m_dsts[i]->GetFrameMID() == info->dst->GetFrameMID())
        {
            index = (mfxI32)i;
        }

    if(index != -1)
    {
        mfxU32 picToCollect = (MFX_PICSTRUCT_PROGRESSIVE == m_vPar.mfx.FrameInfo.PicStruct) ?
            (1) : (2);

        for(mfxU32 i=0; i<picToCollect; i++)
            m_pMJPEGVideoDecoder->CloseFrame(&(m_dsts[index]), i);
        delete [] m_dsts[index];
        m_dsts.erase(m_dsts.begin() + index);
    }

    delete (ThreadTaskInfoJpeg *)pParam;
    return MFX_ERR_NONE;
}

#endif // MFX_ENABLE_MJPEG_VIDEO_DECODE