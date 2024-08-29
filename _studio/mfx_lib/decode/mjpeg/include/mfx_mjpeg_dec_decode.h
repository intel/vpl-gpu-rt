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

#include "mfx_common.h"
#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)

#include "umc_defs.h"

#ifndef _MFX_MJPEG_DEC_DECODE_H_
#define _MFX_MJPEG_DEC_DECODE_H_

#include "mfx_common_int.h"
#include "umc_video_decoder.h"
#include "mfx_umc_alloc_wrapper.h"

#include <mutex>

#include "mfx_task.h"
#include "umc_media_data.h"

#include "mfx_vpp_jpeg.h"

namespace UMC
{
    class MJPEGVideoDecoderBaseMFX;
    class JpegFrameConstructor;
    class MediaDataEx;
    class FrameData;
    class MediaData;
};

class VideoDECODEMJPEGBase
{
public:
    std::unique_ptr<SurfaceSource>         m_surface_source;

    UMC::VideoDecoderParams umcVideoParams;

    // Free tasks queue guard (if SW is used)
    std::mutex m_guard;
    mfxDecodeStat m_stat;
    mfxVideoParamWrapper m_vPar;

    VideoDECODEMJPEGBase();
    virtual ~VideoDECODEMJPEGBase(){};

    virtual mfxStatus Init(mfxVideoParam *decPar, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxFrameAllocRequest *request_internal, bool isUseExternalFrames, VideoCORE *core) = 0;
    virtual mfxStatus ReserveUMCDecoder(UMC::MJPEGVideoDecoderBaseMFX* &pMJPEGVideoDecoder, mfxFrameSurface1 *surf) = 0;
    virtual mfxStatus CheckTaskAvailability(mfxU32 maxTaskNumber) = 0;
    virtual mfxStatus GetVideoParam(mfxVideoParam *par) = 0;
    virtual mfxStatus RunThread(void *pParam, mfxU32 threadNumber, mfxU32 callNumber) = 0;
    virtual mfxStatus CompleteTask(void *pParam, mfxStatus taskRes) = 0;
    virtual void ReleaseReservedTask() = 0;
    virtual mfxStatus AddPicture(UMC::MediaDataEx *pSrcData, mfxU32 & numPic) = 0;
    virtual mfxStatus AllocateFrameData(UMC::FrameData *&data) = 0;
    virtual mfxStatus FillEntryPoint(MFX_ENTRY_POINT *pEntryPoint, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_out) = 0;

    virtual mfxStatus Reset(mfxVideoParam *par) = 0;
    virtual mfxStatus Close(void) = 0;

protected:

    mfxStatus GetVideoParam(mfxVideoParam *par, UMC::MJPEGVideoDecoderBaseMFX * mjpegDecoder);
};

namespace UMC
{
    class MJPEGVideoDecoderMFX_HW;
};

class VideoDECODEMJPEGBase_HW : public VideoDECODEMJPEGBase
{
public:
    VideoDECODEMJPEGBase_HW();

    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus Init(mfxVideoParam *decPar, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxFrameAllocRequest *request_internal, bool isUseExternalFrames, VideoCORE *core);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus RunThread(void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    virtual mfxStatus CompleteTask(void *pParam, mfxStatus taskRes);
    virtual mfxStatus CheckTaskAvailability(mfxU32 maxTaskNumber);
    virtual mfxStatus ReserveUMCDecoder(UMC::MJPEGVideoDecoderBaseMFX* &pMJPEGVideoDecoder, mfxFrameSurface1 *surf);
    virtual void ReleaseReservedTask();
    virtual mfxStatus AddPicture(UMC::MediaDataEx *pSrcData, mfxU32 & numPic);
    virtual mfxStatus AllocateFrameData(UMC::FrameData *&data);
    virtual mfxStatus FillEntryPoint(MFX_ENTRY_POINT *pEntryPoint, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_out);

    mfxU32 AdjustFrameAllocRequest(mfxFrameAllocRequest *request, mfxInfoMFX *info, VideoCORE * core, bool isPostProcEnable);

    static void AdjustFourCC(mfxFrameInfo *requestFrameInfo, const mfxInfoMFX *info, eMFXVAType vaType, bool isPostProcEnable, bool *needVpp);

    static mfxStatus CheckVPPCaps(VideoCORE * core, mfxVideoParam * par);
    static bool isVideoPostprocEnabled(VideoCORE * core);



protected:
    // Decoder's array
    std::unique_ptr<UMC::MJPEGVideoDecoderMFX_HW> m_pMJPEGVideoDecoder;
    // True if we need special VPP color conversion after decoding
    bool   m_needVpp;

    // Number of pictures collected
    mfxU32 m_numPic;
    // Output frame
    UMC::FrameData *m_dst;
    // Array of all currently using frames
    std::vector<UMC::FrameData*> m_dsts;

    UMC::VideoAccelerator * m_va;
};

class VideoDECODEMJPEG : public VideoDECODE
{
public:
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    static mfxStatus DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par);
    static mfxStatus QueryImplsDescription(VideoCORE&, mfxDecoderDescription::decoder&, mfx::PODArraysHolder&);

    VideoDECODEMJPEG(VideoCORE *core, mfxStatus * sts);
    virtual ~VideoDECODEMJPEG(void);

    mfxStatus Init(mfxVideoParam *par) override;
    virtual mfxStatus Reset(mfxVideoParam *par) override;
    virtual mfxStatus Close(void) override;
    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void) override;

    virtual mfxStatus GetVideoParam(mfxVideoParam *par) override;
    virtual mfxStatus GetDecodeStat(mfxDecodeStat *stat) override;
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out);
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, MFX_ENTRY_POINT *pEntryPoint) override;
    virtual mfxStatus DecodeFrame(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_out);
    virtual mfxStatus GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts);
    virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload) override;
    virtual mfxStatus SetSkipMode(mfxSkipMode mode) override;
    virtual mfxStatus GetSurface(mfxFrameSurface1* & surface, mfxSurfaceHeader* import_surface) override;

protected:
    static mfxStatus QueryIOSurfInternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);

    bool IsSameVideoParam(mfxVideoParam * newPar, mfxVideoParam * oldPar);

    // Frames collecting unit
    std::unique_ptr<UMC::JpegFrameConstructor> m_frameConstructor;

    mfxVideoParamWrapper m_vFirstPar;
    mfxVideoParamWrapper m_vPar;

    mfxU16 m_maxCropW;
    mfxU16 m_maxCropH;

    VideoCORE * m_core;

    bool    m_isInit;
    bool    m_isHeaderFound;
    bool    m_isHeaderParsed;

    mfxU32  m_frameOrder;

    std::unique_ptr<VideoDECODEMJPEGBase> decoder;

    mfxFrameAllocResponse m_response;
    mfxFrameAllocResponse m_response_alien;
    eMFXPlatform m_platform;

    std::mutex m_mGuard;

    // Frame skipping rate
    mfxU32 m_skipRate;
    // Frame skipping count
    mfxU32 m_skipCount;

    //
    // Asynchronous processing functions
    //

    static
    mfxStatus MJPEGDECODERoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static
    mfxStatus MJPEGCompleteProc(void *pState, void *pParam, mfxStatus taskRes);
};

#endif // _MFX_MJPEG_DEC_DECODE_H_
#endif // MFX_ENABLE_MJPEG_VIDEO_DECODE
