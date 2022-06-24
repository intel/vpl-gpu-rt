/*******************************************************************************

Copyright (C) 2021 Intel Corporation.  All rights reserved.

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

File Name: mfxpxp.h

*******************************************************************************/
#ifndef __MFX_PXP_H__
#define __MFX_PXP_H__

#include "mfxdefs.h"
#include "mfxcommon.h"

#ifdef __cplusplus
extern "C" {
#endif

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! Describes secure decode configuration. */
typedef struct {
    mfxHDL                   pxpAttributesHdl;  /*!< Pointer to VAConfigAttrib. */
    mfxU32                   ContextId;         /*!< context id of decoder. */
    mfxU8                    reserved[56];      /*!< Reserved for future use. */
} *mfxSecureDecodeConfigHDL, mfxSecureDecodeConfig;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Describes secure encode configuration. */
typedef struct {
    mfxU32                   pxpFlag;           /*!< flag to create secure encode context. */
    mfxU32                   ContextId;         /*!< context id of encoder. */
    mfxU8                    reserved[24];      /*!< Reserved for future use. */
} *mfxSecureEncodeConfigHDL, mfxSecureEncodeConfig;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
/*! Describes secure vpp configuration. */
typedef struct {
    mfxU32                   ContextId;         /*!< context id of VPP. */
    mfxU8                    reserved[28];      /*!< Reserved for future use. */
} *mfxSecureVPPConfigHDL, mfxSecureVPPConfig;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! This is a table to map pxp parameter associacted with decode input.
    PXP parameter point to VAEncryptionParameters on linux.
    pMfxBitstream is the key to search pPXPParams
    {pMfxBitstream | pPXPParams}
*/
typedef struct {
    mfxBitstream            *pMfxBitstream;     /*!< Pointer to input bitstream buffer. */
    mfxHDL                   pPXPParams;        /*!< Pointer to PXP parameters. */
    mfxU8                    reserved[24];      /*!< Reserved for future use. */
} *mfxDecodeParamMapHDL, mfxDecodeParamMap;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! This is a table to map pxp parameter associacted with encode input/output.
    PXP parameter point to VAEncryptionParameters on linux.
    {bitstream_ptr | pxp_param_ptr | pOutput | outputSize}
*/
typedef struct {
    mfxBitstream            *pMfxBitstream;     /*!< Pointer to output bitstream buffer. */
    mfxHDL                   pPXPParams;        /*!< Pointer to PXP parameters */
    mfxHDL                   pOutput;           /*!< Pointer to encode output meta data. */
    mfxU32                   outputSize;        /*!< size of encode output meta data. */
    mfxU8                    reserved[16];      /*!< Reserved for future use. */
} *mfxEncodeParamMapHDL, mfxEncodeParamMap;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*! Describes pxp parameter of a frame.
    Simple usage example on linux:
    1. Initialize oneVPL library and Create an oneVPL session.
        mfxLoader loader = MFXLoad();
        mfxConfig cfg = MFXCreateConfig(loader);
        mfxVariant ImplValue;
        ImplValue.Type = MFX_VARIANT_TYPE_U32;
        ImplValue.Data.U32 = MFX_IMPL_TYPE_HARDWARE;
        MFXSetConfigFilterProperty(cfg,"mfxImplDescription.Impl",ImplValue);
        MFXCreateSession(loader,0,&session);
    2. Create PXP session and do authentication
        va_dpy   = vaGetDisplayDRM(m_fd);1. 
        vaStatus = vaInitialize(va_dpy, &major_version, &minor_version);
        mfxHDL     pxpSessionHdl;
        VAConfigID config_id;
        vaStatus = vaCreatedProtectedSession(va_dpy, config_id, &pxpSessionHdl);
    3. Create mfxPXPCtx and set handle to VPL
        mfxPXPCtx pxp_ctx_hdl;
        pxp_ctx_hdl.pxpSessionHdl = pxpSessionHdl;
        MfxStatus = MFXCORE_SetHandle(mfx_session, MFX_HANDLE_CP_CONTEXT, (mfxHDL)&pxp_ctx_hdl);
    4. Set structure mfxSecureDecodeConfig and Initialize decoder
        //set pxp_ctx_hdl.secureDecodeCfg reference describe of mfxSecureDecodeConfig
        mfxStatus = MFXVideoDECODE_Init(mfx_session, &init_param);
    5. Set structure mfxSecureEncodeConfig and Initialize encoder
        //set pxp_ctx_hdl.secureEncodeCfg reference describe of mfxSecureEncodeConfig
        mfxStatus = MFXVideoENCODE_Init(mfx_session, &init_param);
    7. Set structure mfxSecureVPPConfig and Initialize VPP
        //set pxp_ctx_hdl.secureVPPCfg reference describe of mfxSecureVPPConfig
        mfxStatus = MFXVideoVPP_Init(mfx_session, &init_param);
    8. Attach PXP session and context
        vaStatus  = vaAttachProtectedSession(va_dpy, pxpSessionHdl, pxp_ctx_hdl.secureDecodeCfg.ContextId);
        vaStatus  = vaAttachProtectedSession(va_dpy, pxpSessionHdl, pxp_ctx_hdl.secureEncodeCfg.ContextId);
        vaStatus  = vaAttachProtectedSession(va_dpy, pxpSessionHdl, pxp_ctx_hdl.secureVPPCfg.ContextId);
    9. Decode frames
        mfxBitstrem bs_0;
        mfxFrameSurface1 *surface_work;
        mfxFrameSurface1 *surface_out_0;
        mfxHDL pPXPParams_0;
        mfxSyncPoint syncpD;
        //set pxp_ctx_hdl.decodeParamMapHdl reference describe of mfxDecodeParamMapHDL
        //pxp_ctx_hdl.decodeParamMapHdl = {&bs_0, &surface_out_0, &pPXPParams_0}
        mfxStatus = MFXVideoDECODE_DecodeFrameAsync(mfx_session, &bs_0, surface_work, &surface_out_0, &syncpD);
        mfxBitstrem bs_1;
        mfxFrameSurface1 *surface_out_1;
        mfxHDL pPXPParams_1;
        //set pxp_ctx_hdl.decodeParamMapHdl reference describe of mfxDecodeParamMapHDL
        //pxp_ctx_hdl.decodeParamMapHdl = {&bs_0, &surface_out_0, &pPXPParams_0}, {&bs_1, &surface_out_1, &pPXPParams_1}
        mfxStatus = MFXVideoDECODE_DecodeFrameAsync(mfx_session, &bs_1, surface_work, &surface_out_1, &syncpD);
        MFXVideoCORE_SyncOperation(mfx_session, syncpD, INFINITE);
        //Pop up unused pxp_ctx_hdl.decodeParamMapHdl
    10.VPP
        mfxFrameSurface1 *surface_out_0;
        mfxFrameSurface1 *surface_vpp_out_0;
        mfxSyncPoint syncpP;
        mfxStatus = MFXVideoVPP_RunFrameVPPAsync(mfx_session, surface_out_0, surface_vpp_out_0, NULL, &syncpP);
        MFXVideoCORE_SyncOperation(mfx_session, syncpP, INFINITE);
    11.Encode frames
        mfxBitstrem bs_out_0;
        mfxHDL pPXPParams_enc_0;
        mfxHDL   PXPCounter_0;
        mfxSyncPoint syncpE;
        //pxp_ctx_hdl.encodeParamMapHdl = {surface_vpp_out_0, &bs_out_0, &pPXPParams_enc_0, PXPCounter_0}
        mfxStatus = MFXVideoENCODE_EncodeFrameAsync(mfx_session, NULL, surface_vpp_out_0, &bs_out_0, &syncpE);
        MFXVideoCORE_SyncOperation(mfx_session, syncpE, INFINITE);
        //Pop up unused pxp_ctx_hdl.encodeParamMapHdl
    12.Detach contexts
        vaDetachProtectedSession(va_dpy, pxp_ctx_hdl.secureDecodeCfg.ContextId); 
        vaDetachProtectedSession(va_dpy, pxp_ctx_hdl.secureEncodeCfg.ContextId);
        vaDetachProtectedSession(va_dpy, pxp_ctx_hdl.secureVPPCfg.ContextId);
    13.Destroy PXP session and terninate VA display
        vaDestroyProtectedSession(pxpSessionHdl);
        vaTerminate(va_dpy);
    14. release VPL session
        MFXVideoENCODE_Close(mfx_session);
        MFXUnload(mfx_loader);
*/
typedef struct {
    mfxHDL                   pxpSessionHdl;     /*!< The handle of PXP Session. */
    mfxSecureDecodeConfig    secureDecodeCfg;   /*!< Pointer to mfxSecureDecodeConfig structure. */
    mfxSecureEncodeConfig    secureEncodeCfg;   /*!< Pointer to mfxSecureEncodeConfig structure. */
    mfxSecureVPPConfig       secureVPPCfg;      /*!< Pointer to mfxSecureVPPConfig structure. */

    mfxDecodeParamMapHDL     decodeParamMapHdl; /*!< Pointer to mfxDecodeParamMapHDL structure. */
    mfxU32                   decodeParamMapCnt; /*!< The count of PXP parameters. */
    mfxEncodeParamMapHDL     encodeParamMapHdl; /*!< Pointer to mfxEncodeParamMapHDL structure. */
    mfxU32                   encodeParamMapCnt; /*!< The count of PXP parameters. */
    mfxU8                    reserved[12];      /*!< Reserved for future use. */
} mfxPXPCtx, *mfxPXPCtxHDL;
MFX_PACK_END()

#ifdef __cplusplus
} // extern "C"
#endif

#endif //__MFX_PXP_H__
