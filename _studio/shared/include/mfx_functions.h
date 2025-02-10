// Copyright (c) 2021-2025 Intel Corporation
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

#ifndef _MFX_FUNCTIONS_H_
#define _MFX_FUNCTIONS_H_

/*
When public API function is called from RT library - the call goes to dispatcher's exported function instead of internal (RT's) one.
Since session object differs between RT and dispatcher this can lead to seg. fault. To avoid this we
    1. have to mangle (w/ some prefix) RT's functions implementation and internally use only mangled functions
    2. to mangle the function we will use a macro-based substitution, in such way that any public API function will be automatically aliased w/ help of APIImpl_ symbol, for example MFXClose -> APIImpl_MFXClose
    3. to export correct (de-mangled) symbols we need to create dedicated translation unit (w/o macro-based substitution) where we will dispatch calls to mangled functions.
*/

#include "mfxvideo.h"
#include "mfximplcaps.h"
#include "mfxdeprecated.h"
#include "mfxplugin.h"

#if !defined(MFX_API_FUNCTION_IMPL)
#define MFX_API_FUNCTION_IMPL(NAME, RTYPE, ARGS_DECL, ARGS) RTYPE APIImpl_##NAME ARGS_DECL;

#define MFXInit                              APIImpl_MFXInit
#define MFXClose                             APIImpl_MFXClose
#define MFXQueryIMPL                         APIImpl_MFXQueryIMPL
#define MFXQueryVersion                      APIImpl_MFXQueryVersion

#define MFXJoinSession                       APIImpl_MFXJoinSession
#define MFXDisjoinSession                    APIImpl_MFXDisjoinSession
#define MFXCloneSession                      APIImpl_MFXCloneSession
#define MFXSetPriority                       APIImpl_MFXSetPriority
#define MFXGetPriority                       APIImpl_MFXGetPriority

#define MFXVideoCORE_SetBufferAllocator      APIImpl_MFXVideoCORE_SetBufferAllocator
#define MFXVideoCORE_SetFrameAllocator       APIImpl_MFXVideoCORE_SetFrameAllocator
#define MFXVideoCORE_SetHandle               APIImpl_MFXVideoCORE_SetHandle
#define MFXVideoCORE_GetHandle               APIImpl_MFXVideoCORE_GetHandle
#define MFXVideoCORE_QueryPlatform           APIImpl_MFXVideoCORE_QueryPlatform
#define MFXVideoCORE_SyncOperation           APIImpl_MFXVideoCORE_SyncOperation

#define MFXVideoENCODE_Query                 APIImpl_MFXVideoENCODE_Query
#define MFXVideoENCODE_QueryIOSurf           APIImpl_MFXVideoENCODE_QueryIOSurf
#define MFXVideoENCODE_Init                  APIImpl_MFXVideoENCODE_Init
#define MFXVideoENCODE_Reset                 APIImpl_MFXVideoENCODE_Reset
#define MFXVideoENCODE_Close                 APIImpl_MFXVideoENCODE_Close
#define MFXVideoENCODE_GetVideoParam         APIImpl_MFXVideoENCODE_GetVideoParam
#define MFXVideoENCODE_GetEncodeStat         APIImpl_MFXVideoENCODE_GetEncodeStat
#define MFXVideoENCODE_EncodeFrameAsync      APIImpl_MFXVideoENCODE_EncodeFrameAsync

#define MFXVideoDECODE_Query                 APIImpl_MFXVideoDECODE_Query
#define MFXVideoDECODE_DecodeHeader          APIImpl_MFXVideoDECODE_DecodeHeader
#define MFXVideoDECODE_QueryIOSurf           APIImpl_MFXVideoDECODE_QueryIOSurf
#define MFXVideoDECODE_Init                  APIImpl_MFXVideoDECODE_Init
#define MFXVideoDECODE_Reset                 APIImpl_MFXVideoDECODE_Reset
#define MFXVideoDECODE_Close                 APIImpl_MFXVideoDECODE_Close
#define MFXVideoDECODE_GetVideoParam         APIImpl_MFXVideoDECODE_GetVideoParam
#define MFXVideoDECODE_GetDecodeStat         APIImpl_MFXVideoDECODE_GetDecodeStat
#define MFXVideoDECODE_SetSkipMode           APIImpl_MFXVideoDECODE_SetSkipMode
#define MFXVideoDECODE_GetPayload            APIImpl_MFXVideoDECODE_GetPayload
#define MFXVideoDECODE_DecodeFrameAsync      APIImpl_MFXVideoDECODE_DecodeFrameAsync

#define MFXVideoVPP_Query                    APIImpl_MFXVideoVPP_Query
#define MFXVideoVPP_QueryIOSurf              APIImpl_MFXVideoVPP_QueryIOSurf
#define MFXVideoVPP_Init                     APIImpl_MFXVideoVPP_Init
#define MFXVideoVPP_Reset                    APIImpl_MFXVideoVPP_Reset
#define MFXVideoVPP_Close                    APIImpl_MFXVideoVPP_Close

#define MFXVideoVPP_GetVideoParam            APIImpl_MFXVideoVPP_GetVideoParam
#define MFXVideoVPP_GetVPPStat               APIImpl_MFXVideoVPP_GetVPPStat
#define MFXVideoVPP_RunFrameVPPAsync         APIImpl_MFXVideoVPP_RunFrameVPPAsync
#define MFXVideoVPP_RunFrameVPPAsyncEx       APIImpl_MFXVideoVPP_RunFrameVPPAsyncEx
#define MFXVideoVPP_ProcessFrameAsync        APIImpl_MFXVideoVPP_ProcessFrameAsync

#define MFXVideoUSER_Register                APIImpl_MFXVideoUSER_Register
#define MFXVideoUSER_Unregister              APIImpl_MFXVideoUSER_Unregister
#define MFXVideoUSER_GetPlugin               APIImpl_MFXVideoUSER_GetPlugin
#define MFXVideoUSER_ProcessFrameAsync       APIImpl_MFXVideoUSER_ProcessFrameAsync

#define MFXVideoENC_Query                    APIImpl_MFXVideoENC_Query
#define MFXVideoENC_QueryIOSurf              APIImpl_MFXVideoENC_QueryIOSurf
#define MFXVideoENC_Init                     APIImpl_MFXVideoENC_Init
#define MFXVideoENC_Reset                    APIImpl_MFXVideoENC_Reset
#define MFXVideoENC_Close                    APIImpl_MFXVideoENC_Close
#define MFXVideoENC_ProcessFrameAsync        APIImpl_MFXVideoENC_ProcessFrameAsync

#define MFXVideoPAK_Query                    APIImpl_MFXVideoPAK_Query
#define MFXVideoPAK_QueryIOSurf              APIImpl_MFXVideoPAK_QueryIOSurf
#define MFXVideoPAK_Init                     APIImpl_MFXVideoPAK_Init
#define MFXVideoPAK_Reset                    APIImpl_MFXVideoPAK_Reset
#define MFXVideoPAK_Close                    APIImpl_MFXVideoPAK_Close
#define MFXVideoPAK_ProcessFrameAsync        APIImpl_MFXVideoPAK_ProcessFrameAsync

#define MFXInitEx                            APIImpl_MFXInitEx
#define MFXDoWork                            APIImpl_MFXDoWork

#define MFXVideoENC_GetVideoParam            APIImpl_MFXVideoENC_GetVideoParam
#define MFXVideoPAK_GetVideoParam            APIImpl_MFXVideoPAK_GetVideoParam

#define MFXMemory_GetSurfaceForEncode        APIImpl_MFXMemory_GetSurfaceForEncode
#define MFXMemory_GetSurfaceForDecode        APIImpl_MFXMemory_GetSurfaceForDecode
#define MFXMemory_GetSurfaceForVPP           APIImpl_MFXMemory_GetSurfaceForVPP
#define MFXMemory_GetSurfaceForVPPOut        APIImpl_MFXMemory_GetSurfaceForVPPOut

#define MFXVideoDECODE_VPP_Init              APIImpl_MFXVideoDECODE_VPP_Init
#define MFXVideoDECODE_VPP_DecodeFrameAsync  APIImpl_MFXVideoDECODE_VPP_DecodeFrameAsync
#define MFXVideoDECODE_VPP_Reset             APIImpl_MFXVideoDECODE_VPP_Reset
#define MFXVideoDECODE_VPP_Close             APIImpl_MFXVideoDECODE_VPP_Close
#define MFXVideoDECODE_VPP_GetChannelParam   APIImpl_MFXVideoDECODE_VPP_GetChannelParam

#define MFXInitialize                        APIImpl_MFXInitialize
#define MFXQueryImplsDescription             APIImpl_MFXQueryImplsDescription
#define MFXReleaseImplDescription            APIImpl_MFXReleaseImplDescription
#if defined(ONEVPL_EXPERIMENTAL)
#define MFXQueryImplsProperties              APIImpl_MFXQueryImplsProperties
#endif

#endif //defined(MFX_DECLARE_API_FUNCTIONS_WRAPPERS)

MFX_API_FUNCTION_IMPL(MFXInit, mfxStatus, (mfxIMPL implParam, mfxVersion* ver, mfxSession* session), (implParam, ver, session))
MFX_API_FUNCTION_IMPL(MFXClose, mfxStatus, (mfxSession session), (session))
MFX_API_FUNCTION_IMPL(MFXQueryIMPL, mfxStatus, (mfxSession session, mfxIMPL* impl), (session, impl))
MFX_API_FUNCTION_IMPL(MFXQueryVersion, mfxStatus, (mfxSession session, mfxVersion* pVersion), (session, pVersion))

MFX_API_FUNCTION_IMPL(MFXJoinSession, mfxStatus, (mfxSession session, mfxSession child_session), (session, child_session))
MFX_API_FUNCTION_IMPL(MFXDisjoinSession, mfxStatus, (mfxSession session), (session))
MFX_API_FUNCTION_IMPL(MFXCloneSession, mfxStatus, (mfxSession session, mfxSession* clone), (session, clone))
MFX_API_FUNCTION_IMPL(MFXSetPriority, mfxStatus, (mfxSession session, mfxPriority priority), (session, priority))
MFX_API_FUNCTION_IMPL(MFXGetPriority, mfxStatus, (mfxSession session, mfxPriority* priority), (session, priority))

MFX_API_FUNCTION_IMPL(MFXVideoCORE_SetBufferAllocator, mfxStatus, (mfxSession session, mfxBufferAllocator* allocator), (session, allocator))
MFX_API_FUNCTION_IMPL(MFXVideoCORE_SetFrameAllocator, mfxStatus, (mfxSession session, mfxFrameAllocator* allocator), (session, allocator))
MFX_API_FUNCTION_IMPL(MFXVideoCORE_SetHandle, mfxStatus, (mfxSession session, mfxHandleType type, mfxHDL hdl), (session, type, hdl))
MFX_API_FUNCTION_IMPL(MFXVideoCORE_GetHandle, mfxStatus, (mfxSession session, mfxHandleType type, mfxHDL* hdl), (session, type, hdl))
MFX_API_FUNCTION_IMPL(MFXVideoCORE_QueryPlatform, mfxStatus, (mfxSession session, mfxPlatform* platform), (session, platform))
MFX_API_FUNCTION_IMPL(MFXVideoCORE_SyncOperation, mfxStatus, (mfxSession session, mfxSyncPoint syncp, mfxU32 wait), (session, syncp, wait))

MFX_API_FUNCTION_IMPL(MFXVideoENCODE_Query, mfxStatus, (mfxSession session, mfxVideoParam* in, mfxVideoParam* out), (session, in, out))
MFX_API_FUNCTION_IMPL(MFXVideoENCODE_QueryIOSurf, mfxStatus, (mfxSession session, mfxVideoParam* par, mfxFrameAllocRequest* request), (session, par, request))
MFX_API_FUNCTION_IMPL(MFXVideoENCODE_Init, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))
MFX_API_FUNCTION_IMPL(MFXVideoENCODE_Reset, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))
MFX_API_FUNCTION_IMPL(MFXVideoENCODE_Close, mfxStatus, (mfxSession session), (session))
MFX_API_FUNCTION_IMPL(MFXVideoENCODE_GetVideoParam, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))
MFX_API_FUNCTION_IMPL(MFXVideoENCODE_GetEncodeStat, mfxStatus, (mfxSession session, mfxEncodeStat* stat), (session, stat))
MFX_API_FUNCTION_IMPL(MFXVideoENCODE_EncodeFrameAsync, mfxStatus, (mfxSession session, mfxEncodeCtrl* ctrl, mfxFrameSurface1* surface, mfxBitstream* bs, mfxSyncPoint* syncp), (session, ctrl, surface, bs, syncp))

MFX_API_FUNCTION_IMPL(MFXVideoDECODE_Query, mfxStatus, (mfxSession session, mfxVideoParam* in, mfxVideoParam* out), (session, in, out))
MFX_API_FUNCTION_IMPL(MFXVideoDECODE_DecodeHeader, mfxStatus, (mfxSession session, mfxBitstream* bs, mfxVideoParam* par), (session, bs, par))
MFX_API_FUNCTION_IMPL(MFXVideoDECODE_QueryIOSurf, mfxStatus, (mfxSession session, mfxVideoParam* par, mfxFrameAllocRequest* request), (session, par, request))
MFX_API_FUNCTION_IMPL(MFXVideoDECODE_Init, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))
MFX_API_FUNCTION_IMPL(MFXVideoDECODE_Reset, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))
MFX_API_FUNCTION_IMPL(MFXVideoDECODE_Close, mfxStatus, (mfxSession session), (session))
MFX_API_FUNCTION_IMPL(MFXVideoDECODE_GetVideoParam, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))
MFX_API_FUNCTION_IMPL(MFXVideoDECODE_GetDecodeStat, mfxStatus, (mfxSession session, mfxDecodeStat* stat), (session, stat))
MFX_API_FUNCTION_IMPL(MFXVideoDECODE_SetSkipMode, mfxStatus, (mfxSession session, mfxSkipMode mode), (session, mode))
MFX_API_FUNCTION_IMPL(MFXVideoDECODE_GetPayload, mfxStatus, (mfxSession session, mfxU64* ts, mfxPayload* payload), (session, ts, payload))
MFX_API_FUNCTION_IMPL(MFXVideoDECODE_DecodeFrameAsync, mfxStatus, (mfxSession session, mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, mfxSyncPoint* syncp), (session, bs, surface_work, surface_out, syncp))

MFX_API_FUNCTION_IMPL(MFXVideoVPP_Query, mfxStatus, (mfxSession session, mfxVideoParam* in, mfxVideoParam* out), (session, in, out))
MFX_API_FUNCTION_IMPL(MFXVideoVPP_QueryIOSurf, mfxStatus, (mfxSession session, mfxVideoParam* par, mfxFrameAllocRequest request[2]), (session, par, request))
MFX_API_FUNCTION_IMPL(MFXVideoVPP_Init, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))
MFX_API_FUNCTION_IMPL(MFXVideoVPP_Reset, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))
MFX_API_FUNCTION_IMPL(MFXVideoVPP_Close, mfxStatus, (mfxSession session), (session))
MFX_API_FUNCTION_IMPL(MFXVideoVPP_GetVideoParam, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))
MFX_API_FUNCTION_IMPL(MFXVideoVPP_GetVPPStat, mfxStatus, (mfxSession session, mfxVPPStat* stat), (session, stat))
MFX_API_FUNCTION_IMPL(MFXVideoVPP_RunFrameVPPAsync, mfxStatus, (mfxSession session, mfxFrameSurface1* in, mfxFrameSurface1* out, mfxExtVppAuxData* aux, mfxSyncPoint* syncp), (session, in, out, aux, syncp))
MFX_API_FUNCTION_IMPL(MFXVideoVPP_RunFrameVPPAsyncEx, mfxStatus, (mfxSession session, mfxFrameSurface1* in, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, mfxSyncPoint* syncp), (session, in, surface_work, surface_out, syncp))

MFX_API_FUNCTION_IMPL(MFXVideoUSER_Register, mfxStatus, (mfxSession session, mfxU32 type, const mfxPlugin* par), (session, type, par))
MFX_API_FUNCTION_IMPL(MFXVideoUSER_Unregister, mfxStatus, (mfxSession session, mfxU32 type), (session, type))
MFX_API_FUNCTION_IMPL(MFXVideoUSER_GetPlugin, mfxStatus, (mfxSession session, mfxU32 type, mfxPlugin* par), (session, type, par))
MFX_API_FUNCTION_IMPL(MFXVideoUSER_ProcessFrameAsync, mfxStatus, (mfxSession session, const mfxHDL* in, mfxU32 in_num, const mfxHDL* out, mfxU32 out_num, mfxSyncPoint* syncp), (session, in, in_num, out, out_num, syncp))

MFX_API_FUNCTION_IMPL(MFXVideoENC_Query, mfxStatus, (mfxSession session, mfxVideoParam* in, mfxVideoParam* out), (session, in, out))
MFX_API_FUNCTION_IMPL(MFXVideoENC_QueryIOSurf, mfxStatus, (mfxSession session, mfxVideoParam* par, mfxFrameAllocRequest* request), (session, par, request))
MFX_API_FUNCTION_IMPL(MFXVideoENC_Init, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))
MFX_API_FUNCTION_IMPL(MFXVideoENC_Reset, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))
MFX_API_FUNCTION_IMPL(MFXVideoENC_Close, mfxStatus, (mfxSession session), (session))
MFX_API_FUNCTION_IMPL(MFXVideoENC_ProcessFrameAsync, mfxStatus, (mfxSession session, mfxENCInput* in, mfxENCOutput* out, mfxSyncPoint* syncp), (session, in, out, syncp))

MFX_API_FUNCTION_IMPL(MFXVideoPAK_Query, mfxStatus, (mfxSession session, mfxVideoParam* in, mfxVideoParam* out), (session, in, out))
MFX_API_FUNCTION_IMPL(MFXVideoPAK_QueryIOSurf, mfxStatus, (mfxSession session, mfxVideoParam* par, mfxFrameAllocRequest* request), (session, par, request))
MFX_API_FUNCTION_IMPL(MFXVideoPAK_Init, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))
MFX_API_FUNCTION_IMPL(MFXVideoPAK_Reset, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))
MFX_API_FUNCTION_IMPL(MFXVideoPAK_Close, mfxStatus, (mfxSession session), (session))
MFX_API_FUNCTION_IMPL(MFXVideoPAK_ProcessFrameAsync, mfxStatus, (mfxSession session, mfxPAKInput* in, mfxPAKOutput* out, mfxSyncPoint* syncp), (session, in, out, syncp))

MFX_API_FUNCTION_IMPL(MFXInitEx, mfxStatus, (mfxInitParam par, mfxSession* session), (par, session))
MFX_API_FUNCTION_IMPL(MFXDoWork, mfxStatus, (mfxSession session), (session))

MFX_API_FUNCTION_IMPL(MFXVideoENC_GetVideoParam, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))
MFX_API_FUNCTION_IMPL(MFXVideoPAK_GetVideoParam, mfxStatus, (mfxSession session, mfxVideoParam* par), (session, par))

MFX_API_FUNCTION_IMPL(MFXVideoVPP_ProcessFrameAsync, mfxStatus, (mfxSession session, mfxFrameSurface1* in, mfxFrameSurface1** out), (session, in, out))

MFX_API_FUNCTION_IMPL(MFXMemory_GetSurfaceForEncode, mfxStatus, (mfxSession session, mfxFrameSurface1** output_surf), (session, output_surf))
MFX_API_FUNCTION_IMPL(MFXMemory_GetSurfaceForDecode, mfxStatus, (mfxSession session, mfxFrameSurface1** output_surf), (session, output_surf))
MFX_API_FUNCTION_IMPL(MFXMemory_GetSurfaceForVPP, mfxStatus, (mfxSession session, mfxFrameSurface1** output_surf), (session, output_surf))
MFX_API_FUNCTION_IMPL(MFXMemory_GetSurfaceForVPPOut, mfxStatus, (mfxSession session, mfxFrameSurface1** output_surf), (session, output_surf))

MFX_API_FUNCTION_IMPL(MFXVideoDECODE_VPP_Init, mfxStatus, (mfxSession session, mfxVideoParam* decode_par, mfxVideoChannelParam** vpp_par_array, mfxU32 num_channel_par), (session, decode_par, vpp_par_array, num_channel_par))
MFX_API_FUNCTION_IMPL(MFXVideoDECODE_VPP_DecodeFrameAsync, mfxStatus, (mfxSession session, mfxBitstream* bs, mfxU32* skip_channels, mfxU32 num_skip_channels, mfxSurfaceArray** surf_array_out), (session, bs, skip_channels, num_skip_channels, surf_array_out))
MFX_API_FUNCTION_IMPL(MFXVideoDECODE_VPP_Reset, mfxStatus, (mfxSession session, mfxVideoParam* decode_par, mfxVideoChannelParam** vpp_par_array, mfxU32 num_channel_par), (session, decode_par, vpp_par_array, num_channel_par))
MFX_API_FUNCTION_IMPL(MFXVideoDECODE_VPP_Close, mfxStatus, (mfxSession session), (session))
MFX_API_FUNCTION_IMPL(MFXVideoDECODE_VPP_GetChannelParam, mfxStatus, (mfxSession session, mfxVideoChannelParam* par, mfxU32 channel_id), (session, par, channel_id))

MFX_API_FUNCTION_IMPL(MFXInitialize, mfxStatus, (mfxInitializationParam param, mfxSession* session), (param, session))
MFX_API_FUNCTION_IMPL(MFXQueryImplsDescription, mfxHDL*, (mfxImplCapsDeliveryFormat format, mfxU32* num_impls), (format, num_impls))
MFX_API_FUNCTION_IMPL(MFXReleaseImplDescription, mfxStatus, (mfxHDL hdl), (hdl))
#if defined(ONEVPL_EXPERIMENTAL)
MFX_API_FUNCTION_IMPL(MFXQueryImplsProperties, mfxHDL*, (mfxQueryProperty** properties, mfxU32 num_properties, mfxU32* num_impls), (properties, num_properties, num_impls))
#endif

#undef MFX_API_FUNCTION_IMPL

#endif //_MFX_FUNCTIONS_H_
