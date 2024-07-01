// Copyright (c) 2022 Intel Corporation
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

#ifndef __MFXENCTOOLS_DL_INT_H__
#define __MFXENCTOOLS_DL_INT_H__

#include "mfx_config.h"
#include "mfxvideo++.h"
#include "mfx_functions.h"

class MFXDLVideoSession : MFXVideoSessionBase {
public:
    decltype(MFXInitEx)* InitExPtr;
    decltype(MFXClose)* ClosePtr;
    decltype(MFXQueryVersion)* QueryVersionPtr;
    decltype(MFXVideoCORE_SetFrameAllocator)* SetFrameAllocatorPtr;
    decltype(MFXVideoCORE_SetHandle)* SetHandlePtr;
    decltype(MFXVideoCORE_SyncOperation)* SyncOperationPtr;
    decltype(MFXMemory_GetSurfaceForEncode)* GetSurfaceForEncodePtr;
    decltype(MFXJoinSession)* JoinSessionPtr;
    decltype(MFXDisjoinSession)* DisjoinSessionPtr;

    MFXDLVideoSession(void* module);
    virtual ~MFXDLVideoSession(void) {
        Close();
    }

    //override to invoke function pointer
    mfxStatus Init(mfxIMPL, mfxVersion*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    mfxStatus InitEx(mfxInitParam par) override {
        return InitExPtr(par, &m_session);
    }

    mfxStatus Close(void) override {
        mfxStatus sts = ClosePtr(m_session);

        if (sts == MFX_ERR_NONE)
            m_session = nullptr;

        return sts;
    }

    mfxStatus QueryIMPL(mfxIMPL*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    mfxStatus QueryVersion(mfxVersion* version) override {
        return QueryVersionPtr(m_session, version);
    }

    mfxStatus JoinSession(mfxSession child) override {
        return JoinSessionPtr(m_session, child);
    }
    mfxStatus DisjoinSession() override {
        return DisjoinSessionPtr(m_session);
    }
    mfxStatus CloneSession(mfxSession*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    mfxStatus SetPriority(mfxPriority) override {
        return MFX_ERR_UNSUPPORTED;
    }
    mfxStatus GetPriority(mfxPriority*) override {
        return MFX_ERR_UNSUPPORTED;
    }

    mfxStatus SetFrameAllocator(mfxFrameAllocator* allocator) override {
        return SetFrameAllocatorPtr(m_session, allocator);
    }
    mfxStatus SetHandle(mfxHandleType type, mfxHDL hdl) override {
        return SetHandlePtr(m_session, type, hdl);
    }
    mfxStatus GetHandle(mfxHandleType, mfxHDL*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    mfxStatus QueryPlatform(mfxPlatform*) override {
        return MFX_ERR_UNSUPPORTED;
    }

    mfxStatus SyncOperation(mfxSyncPoint syncp, mfxU32 wait) override {
        return SyncOperationPtr(m_session, syncp, wait);
    }

    mfxStatus GetSurfaceForEncode(mfxFrameSurface1** surface) override {
        return GetSurfaceForEncodePtr(m_session, surface);
    }
    mfxStatus GetSurfaceForDecode(mfxFrameSurface1**) override {
        return MFX_ERR_UNSUPPORTED;
    }
    mfxStatus GetSurfaceForVPP(mfxFrameSurface1**) override {
        return MFX_ERR_UNSUPPORTED;
    }
    mfxStatus GetSurfaceForVPPOut(mfxFrameSurface1**) override {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual operator mfxSession(void) override {
        return m_session;
    }

protected:
    mfxSession m_session; // (mfxSession) handle to the owning session
private:
    MFXDLVideoSession(const MFXDLVideoSession&);
    void operator=(MFXDLVideoSession&);
};

class MFXDLVideoENCODE : public MFXVideoENCODEBase {
public:
    decltype(MFXVideoENCODE_Init)* InitPtr;
    decltype(MFXVideoENCODE_Close)* ClosePtr;
    decltype(MFXVideoENCODE_EncodeFrameAsync)* EncodeFrameAsyncPtr;

    explicit MFXDLVideoENCODE(mfxSession session, void* module);
    virtual ~MFXDLVideoENCODE(void) {
        Close();
    }

    virtual mfxStatus Query(mfxVideoParam*, mfxVideoParam*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam*, mfxFrameAllocRequest*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus Init(mfxVideoParam* par) override {
        return InitPtr(m_session, par);
    }
    virtual mfxStatus Reset(mfxVideoParam*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus Close(void) override {
        return ClosePtr(m_session);
    }

    virtual mfxStatus GetVideoParam(mfxVideoParam*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus GetEncodeStat(mfxEncodeStat*) override {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus EncodeFrameAsync(mfxEncodeCtrl* ctrl,
        mfxFrameSurface1* surface,
        mfxBitstream* bs,
        mfxSyncPoint* syncp) override {
        return EncodeFrameAsyncPtr(m_session, ctrl, surface, bs, syncp);
    }

    virtual mfxStatus GetSurface(mfxFrameSurface1**) override {
        return MFX_ERR_UNSUPPORTED;
    }

protected:
    mfxSession m_session; // (mfxSession) handle to the owning session
};

class MFXDLVideoDECODE : public MFXVideoDECODEBase {
public:
    explicit MFXDLVideoDECODE(mfxSession session, void*) {
        m_session = session;
    }
    virtual ~MFXDLVideoDECODE(void) {
        Close();
    }

    virtual mfxStatus Query(mfxVideoParam*, mfxVideoParam*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus DecodeHeader(mfxBitstream*, mfxVideoParam*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam*, mfxFrameAllocRequest*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus Init(mfxVideoParam*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus Reset(mfxVideoParam*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus Close(void) override {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus GetVideoParam(mfxVideoParam*) override {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus GetDecodeStat(mfxDecodeStat*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus GetPayload(mfxU64*, mfxPayload*) override {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus SetSkipMode(mfxSkipMode) override {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus DecodeFrameAsync(mfxBitstream*,
        mfxFrameSurface1*,
        mfxFrameSurface1**,
        mfxSyncPoint*) override {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus GetSurface(mfxFrameSurface1**) override {
        return MFX_ERR_UNSUPPORTED;
    }

protected:
    mfxSession m_session; // (mfxSession) handle to the owning session
};

class MFXDLVideoVPP : public MFXVideoVPPBase {
public:
    decltype(MFXVideoVPP_QueryIOSurf)* QueryIOSurfPtr;
    decltype(MFXVideoVPP_Init)* InitPtr;
    decltype(MFXVideoVPP_Close)* ClosePtr;
    decltype(MFXVideoVPP_RunFrameVPPAsync)* RunFrameVPPAsyncPtr;

    explicit MFXDLVideoVPP(mfxSession session, void* module);
    virtual ~MFXDLVideoVPP(void) {
        Close();
    }

    virtual mfxStatus Query(mfxVideoParam*, mfxVideoParam*) override {
        return MFX_ERR_UNSUPPORTED;;
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam* par, mfxFrameAllocRequest request[2]) override {
        return QueryIOSurfPtr(m_session, par, request);
    }
    virtual mfxStatus Init(mfxVideoParam* par) override {
        return InitPtr(m_session, par);
    }
    virtual mfxStatus Reset(mfxVideoParam*) override {
        return MFX_ERR_UNSUPPORTED;;
    }
    virtual mfxStatus Close(void) override {
        return ClosePtr(m_session);
    }

    virtual mfxStatus GetVideoParam(mfxVideoParam*) override {
        return MFX_ERR_UNSUPPORTED;;
    }
    virtual mfxStatus GetVPPStat(mfxVPPStat*) override {
        return MFX_ERR_UNSUPPORTED;;
    }
    virtual mfxStatus RunFrameVPPAsync(mfxFrameSurface1* in,
        mfxFrameSurface1* out,
        mfxExtVppAuxData* aux,
        mfxSyncPoint* syncp) override {
        return RunFrameVPPAsyncPtr(m_session, in, out, aux, syncp);
    }

    virtual mfxStatus GetSurfaceIn(mfxFrameSurface1**) override {
        return MFX_ERR_UNSUPPORTED;;
    }
    virtual mfxStatus GetSurfaceOut(mfxFrameSurface1**) override {
        return MFX_ERR_UNSUPPORTED;;
    }

    virtual mfxStatus ProcessFrameAsync(mfxFrameSurface1*, mfxFrameSurface1**) override {
        return MFX_ERR_UNSUPPORTED;
    }

protected:
    mfxSession m_session; // (mfxSession) handle to the owning session
};

#endif
