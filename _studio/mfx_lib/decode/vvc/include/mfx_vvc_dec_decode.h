// Copyright (c) 2020-2024 Intel Corporation
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

#if defined(MFX_ENABLE_VVC_VIDEO_DECODE)

#include "mfx_common_int.h"
#include "mfx_umc_alloc_wrapper.h"
#include "umc_vvc_mfx_utils.h"
#include "umc_vvc_decoder.h"
#include "umc_vvc_decoder_va.h"
#include "umc_defs.h"
#include "umc_video_decoder.h"
#include "mfx_umc_alloc_wrapper.h"

#include <mutex>
#include <memory>

#ifndef _MFX_VVC_DEC_DECODE_H_
#define _MFX_VVC_DEC_DECODE_H_

namespace UMC_VVC_DECODER
{
    class VVCDecoder;
    class VVCDecoderFrame;
}

using UMC_VVC_DECODER::VVCDecoderFrame;

// VVC decoder interface class
class VideoDECODEVVC : public VideoDECODE
{
    struct ThreadTaskInfoVVC
    {
        mfxFrameSurface1 *surface_work;
        mfxFrameSurface1 *surface_out;
        UMC::FrameMemID  copyfromframe;
        bool             is_decoding_done;
        VVCDecoderFrame  *pFrame;
    };

public:

    VideoDECODEVVC(VideoCORE *core, mfxStatus *sts);
    virtual ~VideoDECODEVVC(void);

    // VPL DECODE_Query API function
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
    // VPL DECODE_QueryIOSurf API function
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    // Decode bitstream header and exctract parameters from it
    static mfxStatus DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par);
    static mfxStatus QueryImplsDescription(VideoCORE &, mfxDecoderDescription::decoder &, mfx::PODArraysHolder &);

    // Initialize decoder instance
    mfxStatus Init(mfxVideoParam *par) override;
    // Reset decoder with new parameters
    virtual mfxStatus Reset(mfxVideoParam *par) override;
    // Free decoder resources
    virtual mfxStatus Close(void) override;
    // Returns decoder threading mode
    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void) override;

    // VPL DECODE_GetVideoParam API function
    virtual mfxStatus GetVideoParam(mfxVideoParam *par) override;
    // VPL DECODE_GetDecodeStat API function
    virtual mfxStatus GetDecodeStat(mfxDecodeStat *stat) override;
    // Initialize threads callbacks
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, MFX_ENTRY_POINT *pEntryPoint) override;
    // Returns closed caption data
    virtual mfxStatus GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts);
    // Returns stored SEI messages
    virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload) override;
    // VPL DECODE_SetSkipMode API function
    virtual mfxStatus SetSkipMode(mfxSkipMode mode) override;

    // Decoder instance threads entry point. Do async tasks here
    mfxStatus RunThread(void *params);
    virtual mfxStatus GetSurface(mfxFrameSurface1* &surface, mfxSurfaceHeader* import_surface) override;

private:
    // Actually calculate needed frames number
    static mfxStatus QueryIOSurfInternal(mfxVideoParam *par, mfxFrameAllocRequest *request);

    // Check if new parameters are compatible with old parameters
    bool IsNeedChangeVideoParam(mfxVideoParam *newPar, mfxVideoParam *oldPar, eMFXHWType type);

    // Fill up frame parameters before returning it to application
    void FillOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC_VVC_DECODER::VVCDecoderFrame * pFrame);

    // Find a next frame ready to be output from decoder
    UMC_VVC_DECODER::VVCDecoderFrame * GetFrameToDisplay(bool force);

    // Wait until a frame is ready to be output and set necessary surface flags
    mfxStatus DecodeFrame(mfxFrameSurface1 *surface_out, UMC_VVC_DECODER::VVCDecoderFrame *pFrame = 0);

    // Check if there is enough data to start decoding in async mode
    mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out);

    // Fill up resolution information if new header arrived
    void FillVideoParam(mfxVideoParamWrapper *par, bool full);

    // Submit frame decoding
    mfxStatus SubmitFrame(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out);

    // Check whether frame decoding is completed or not
    mfxStatus QueryFrame(mfxThreadTask);

    // Check frame format
    mfxStatus CheckFrameInfo(mfxFrameInfo& info);

    // Decoder threads entry point
    static mfxStatus DecodeRoutine(void *state, void  *param, mfxU32, mfxU32);

    // Threads complete proc callback
    static mfxStatus CompleteProc(void*, void* param, mfxStatus);

private:

    std::unique_ptr<UMC_VVC_DECODER::VVCDecoder>  m_decoder;
    VideoCORE                                     *m_core;
    mfx_UMC_MemAllocator                          m_allocator;
    std::unique_ptr<SurfaceSource>                m_surface_source;
    std::mutex                                    m_guard;

    mfxVideoParamWrapper                          m_initPar;
    mfxVideoParamWrapper                          m_firstPar;
    mfxVideoParamWrapper                          m_videoPar;
    ExtendedBuffer                                m_extBuffers;

    mfxFrameAllocRequest                          m_request;
    mfxFrameAllocResponse                         m_response;
    mfxFrameAllocResponse                         m_response_alien;
    mfxDecodeStat                                 m_stat;

    bool                                          m_isInit;
    bool                                          m_isFirstRun;
    bool                                          m_useDelayedDisplay;
    mfxU16                                        m_frameOrder;
    bool                                          m_is_cscInUse;
};

#endif // MFX_ENABLE_VVC_VIDEO_DECODE

#endif // _MFX_VVC_DEC_DECODE_H_
