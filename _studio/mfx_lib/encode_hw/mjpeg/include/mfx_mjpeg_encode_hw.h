// Copyright (c) 2008-2020 Intel Corporation
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

#ifndef __MFX_MJPEG_ENCODE_HW_H__
#define __MFX_MJPEG_ENCODE_HW_H__

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE)

#include "mfxvideo++int.h"
#include "mfx_mjpeg_encode_hw_utils.h"
#include "mfx_mjpeg_encode_interface.h"

class MfxFrameAllocResponse : public mfxFrameAllocResponse
{
public:
    MfxFrameAllocResponse();

    ~MfxFrameAllocResponse();

    mfxStatus Alloc(
        VideoCORE * core,
        mfxFrameAllocRequest & req,
        bool  isCopyRequired);

    void Free();


private:
    MfxFrameAllocResponse(MfxFrameAllocResponse const &);
    MfxFrameAllocResponse & operator =(MfxFrameAllocResponse const &);

    VideoCORE * m_core;
    std::vector<mfxFrameAllocResponse> m_responseQueue;
    std::vector<mfxMemId> m_mids;
};

class MFXVideoENCODEMJPEG_HW : public VideoENCODE {
public:
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    static mfxStatus QueryImplsDescription(VideoCORE& core, mfxEncoderDescription::encoder& caps, mfx::PODArraysHolder& ah);

    MFXVideoENCODEMJPEG_HW(VideoCORE *core, mfxStatus *sts);
    virtual ~MFXVideoENCODEMJPEG_HW() override;
    virtual mfxStatus Init(mfxVideoParam *par) override;
    virtual mfxStatus Reset(mfxVideoParam *par) override;
    virtual mfxStatus Close(void) override;

    virtual mfxStatus GetVideoParam(mfxVideoParam *par) override;
    virtual mfxStatus GetFrameParam(mfxFrameParam *par) override;
    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat) override;

    virtual
    mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl,
                               mfxFrameSurface1 *surface,
                               mfxBitstream *bs,
                               mfxFrameSurface1 **reordered_surface,
                               mfxEncodeInternalParams *pInternalParams,
                               MFX_ENTRY_POINT pEntryPoints[],
                               mfxU32 &numEntryPoints) override;

   // previous scheduling model - functions are not need to be implemented, only to be compatible
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *,
                                       mfxFrameSurface1 *,
                                       mfxBitstream *,
                                       mfxFrameSurface1 **,
                                       mfxEncodeInternalParams *) override
    {
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }
    virtual mfxStatus EncodeFrame(mfxEncodeCtrl *,
                                  mfxEncodeInternalParams *,
                                  mfxFrameSurface1 *,
                                  mfxBitstream *) override
    {
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }
    virtual mfxStatus CancelFrame(mfxEncodeCtrl *,
                                  mfxEncodeInternalParams *,
                                  mfxFrameSurface1 *,
                                  mfxBitstream *) override
    {
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    mfxU16 GetMemType(mfxVideoParam par) override
    {
        mfxU16 memory_type = mfxU16(par.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ? MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET);

        if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_BGR4)
            memory_type |= MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET;

        return memory_type;
    }

    MFX_PROPAGATE_GetSurface_VideoENCODE_Definition;

protected:
    // callbacks to work with scheduler
    static mfxStatus TaskRoutineSubmitFrame(void * state,
                                            void * param,
                                            mfxU32 threadNumber,
                                            mfxU32 callNumber);

    static mfxStatus TaskRoutineQueryFrame(void * state,
                                           void * param,
                                           mfxU32 threadNumber,
                                           mfxU32 callNumber);

    mfxStatus UpdateDeviceStatus(mfxStatus sts);
    mfxStatus CheckDevice();

    mfxStatus CheckEncodeFrameParam(mfxFrameSurface1    * surface,
                                    mfxBitstream        * bs,
                                    bool                  isExternalFrameAllocator);

    // pointer to video core - class for memory/frames management and allocation
    VideoCORE*          m_pCore;
    mfxVideoParam       m_vFirstParam;
    mfxVideoParam       m_vParam;
    std::unique_ptr<MfxHwMJpegEncode::DriverEncoder> m_ddi;

    bool                m_bInitialized;
    bool                m_deviceFailed;

    mfxFrameAllocResponse m_raw;        // raw surface, for input raw is in system memory case
    MfxFrameAllocResponse m_bitstream;  // bitstream surface
    mfxU32                m_counter;    // task number (StatusReportFeedbackNumber)

    MfxHwMJpegEncode::TaskManager m_TaskManager;

    mfxExtJPEGQuantTables    m_checkedJpegQT;
    mfxExtJPEGHuffmanTables  m_checkedJpegHT;

    mfxExtBuffer*            m_pCheckedExt[3] = {};

    bool                     m_bUseInternalMem;
};

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE)
#endif // __MFX_MJPEG_ENCODE_HW_H__
