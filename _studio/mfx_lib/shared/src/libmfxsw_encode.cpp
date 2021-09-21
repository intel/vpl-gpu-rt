// Copyright (c) 2008-2021 Intel Corporation
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

#include <mfxvideo.h>

#include <assert.h>
#include <functional>

#include <mfx_session.h>
#include <mfx_tools.h>
#include <mfx_common.h>

#include "mfx_reflect.h"

// sheduling and threading stuff
#include <mfx_task.h>

#include "mfxvideo++int.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE)
#include "mfx_h264_encode_hw.h"
#endif //MFX_ENABLE_H264_VIDEO_ENCODE

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)
#include "mfx_mpeg2_encode_hw.h"
#endif

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE)
#include "mfx_mjpeg_encode_hw.h"
#endif

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
#include "../../encode_hw/hevc/hevcehw_disp.h"
#endif

#if defined (MFX_ENABLE_VP9_VIDEO_ENCODE)
#include "mfx_vp9_encode_hw.h"
#endif



#include "libmfx_core.h"

struct CodecKey {
    const mfxU32 codecId;

    CodecKey(mfxU32 codecId) : codecId(codecId) {}

    enum {
        // special value for codecId to denote plugin, it must be
        // different from other MFX_CODEC_* used in codecId2Handlers
        MFX_CODEC_DUMMY_FOR_PLUGIN = 0
    };

    // Exact ordering rule is unsignificant as far as it provides strict weak ordering.
    friend bool operator<(CodecKey l, CodecKey r)
    {
        return l.codecId < r.codecId;
    }
};

struct EHandlers {
    typedef std::function<VideoENCODE*(VideoCORE* core, mfxU16 codecProfile, mfxStatus *mfxRes)> CtorType;

    struct Funcs {
        CtorType ctor;
        std::function<mfxStatus(mfxSession s, mfxVideoParam *in, mfxVideoParam *out)> query;
        std::function<mfxStatus(mfxSession s, mfxVideoParam *par, mfxFrameAllocRequest *request)> queryIOSurf;
        std::function<mfxStatus(VideoCORE&, mfxEncoderDescription::encoder&, mfx::PODArraysHolder&)> QueryImplsDescription;
    };

    Funcs primary;
    Funcs fallback;
};

typedef std::map<CodecKey, EHandlers> CodecId2Handlers;

static const CodecId2Handlers codecId2Handlers =
{
#if defined(MFX_ENABLE_H264_VIDEO_ENCODE)
    {
        {
            MFX_CODEC_AVC,
        },
        {
            // .primary =
            {
                // .ctor =
                [](VideoCORE* core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    return new MFXHWVideoENCODEH264(core, mfxRes);
                },
                // .query =
                [](mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
                {
                    if (!session->m_pENCODE.get())
                        return MFXHWVideoENCODEH264::Query(session->m_pCORE.get(), in, out);
                    else
                        return MFXHWVideoENCODEH264::Query(session->m_pCORE.get(), in, out, session->m_pENCODE.get());
                },
                // .queryIOSurf =
                [](mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    return MFXHWVideoENCODEH264::QueryIOSurf(session->m_pCORE.get(), par, request);
                }
                // .QueryImplsDescription =
                , [](VideoCORE& core, mfxEncoderDescription::encoder& caps, mfx::PODArraysHolder& ah)
                {
                    return MFXHWVideoENCODEH264::QueryImplsDescription(core, caps, ah);
                }
            },
            // .fallback =
            {
            }
        }
    },
#endif // MFX_ENABLE_H264_VIDEO_ENCODE

#ifdef MFX_ENABLE_MPEG2_VIDEO_ENCODE
    {
        {
            MFX_CODEC_MPEG2
        },
        {
            // .primary =
            {
                // .ctor =
                [](VideoCORE* core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    return new MFXVideoENCODEMPEG2_HW(core, mfxRes);
                },
                // .query =
                [](mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
                {
                    return MFXVideoENCODEMPEG2_HW::Query(session->m_pCORE.get(), in, out);
                },
                // .queryIOSurf =
                [](mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    return MFXVideoENCODEMPEG2_HW::QueryIOSurf(session->m_pCORE.get(), par, request);
                }
                // .QueryImplsDescription =
                , [](VideoCORE& core, mfxEncoderDescription::encoder& caps, mfx::PODArraysHolder& ah)
                {
                    return MFXVideoENCODEMPEG2_HW::QueryImplsDescription(core, caps, ah);
                }
            },
            // .fallback =
            {
            }
        }
    },
#endif // MFX_ENABLE_MPEG2_VIDEO_ENCODE

#if defined(MFX_ENABLE_MJPEG_VIDEO_ENCODE)
    {
        {
            MFX_CODEC_JPEG
        },
        {
            // .primary =
            {
                // .ctor =
                [](VideoCORE* core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    return new MFXVideoENCODEMJPEG_HW(core, mfxRes);
                },
                // .query =
                [](mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
                {
                    return MFXVideoENCODEMJPEG_HW::Query(session->m_pCORE.get(), in, out);
                },
                // .queryIOSurf =
                [](mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    return MFXVideoENCODEMJPEG_HW::QueryIOSurf(session->m_pCORE.get(), par, request);
                }
                // .QueryImplsDescription =
                , [](VideoCORE& core, mfxEncoderDescription::encoder& caps, mfx::PODArraysHolder& ah)
                {
                    return MFXVideoENCODEMJPEG_HW::QueryImplsDescription(core, caps, ah);
                }
            },
            // .fallback =
            {
            }
        }
    },
#endif // MFX_ENABLE_MJPEG_VIDEO_ENCODE

#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)
    {
        {
            MFX_CODEC_HEVC
        },
        {
            // .primary =
            {
                // .ctor =
                [](VideoCORE* core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    if (core && mfxRes)
                        return HEVCEHW::Create(*core, *mfxRes);
                    return nullptr;
                },
                // .query =
                [](mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
                { return HEVCEHW::Query(session->m_pCORE.get(), in, out); },
                // .queryIOSurf =
                [](mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
                { return HEVCEHW::QueryIOSurf(session->m_pCORE.get(), par, request); }
                // .QueryImplsDescription =
                , [](VideoCORE& core, mfxEncoderDescription::encoder& caps, mfx::PODArraysHolder& ah)
                { return HEVCEHW::QueryImplsDescription(core, caps, ah); }
            },
            // .fallback =
            {
            }
        }
    },
#endif // MFX_ENABLE_H265_VIDEO_ENCODE

#if defined(MFX_ENABLE_VP9_VIDEO_ENCODE)
    {
        {
            MFX_CODEC_VP9
        },
        {
            // .primary =
            {
                // .ctor =
                [](VideoCORE *core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    return new MfxHwVP9Encode::MFXVideoENCODEVP9_HW(core, mfxRes);
                },
                // .query =
                [](mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
                {
                    return MfxHwVP9Encode::MFXVideoENCODEVP9_HW::Query(session->m_pCORE.get(), in, out);
                },
                // .queryIOSurf =
                [](mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    return MfxHwVP9Encode::MFXVideoENCODEVP9_HW::QueryIOSurf(session->m_pCORE.get(), par, request);
                }
                // .QueryImplsDescription =
                , [](VideoCORE& core, mfxEncoderDescription::encoder& caps, mfx::PODArraysHolder& ah)
                {
                    return MfxHwVP9Encode::MFXVideoENCODEVP9_HW::QueryImplsDescription(core, caps, ah);
                }
            },
            // .fallback =
            {
            }
        }
    },
#endif // MFX_ENABLE_VP9_VIDEO_ENCODE

}; // codecId2Handlers


template<>
VideoENCODE* _mfxSession::Create<VideoENCODE>(mfxVideoParam& par)
{
    VideoCORE* core = m_pCORE.get();
    mfxU32 CodecId = par.mfx.CodecId;
    mfxStatus mfxRes = MFX_ERR_MEMORY_ALLOC;
    std::unique_ptr<VideoENCODE> pENCODE;

    {
        // create a codec instance
        auto handler = codecId2Handlers.find(CodecKey(CodecId));
        if (handler == codecId2Handlers.end())
        {
            return nullptr;
        }

        const EHandlers::CtorType& ctor = m_bIsHWENCSupport ?
            handler->second.primary.ctor : handler->second.fallback.ctor;
        if (!ctor)
        {
            return nullptr;
        }

        pENCODE.reset(ctor(core, par.mfx.CodecProfile, &mfxRes));
        // check error(s)
        if (MFX_ERR_NONE != MFX_STS_TRACE(mfxRes))
        {
            return nullptr;
        }
    }

    return pENCODE.release();
} // VideoENCODE *CreateENCODESpecificClass(mfxU32 CodecId, VideoCORE *core)

mfxStatus MFXVideoENCODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(out, MFX_ERR_NULL_PTR);

#if !defined(ANDROID)
    if ((0 != in) && (MFX_HW_VAAPI == session->m_pCORE->GetVAType()))
    {
        // protected content not supported on Linux
        if(0 != in->Protected)
        {
            out->Protected = 0;
            return MFX_ERR_UNSUPPORTED;
        }
    }
#endif

    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVideoENCODE_Query");
    PERF_EVENT(MFX_TRACE_API_ENCODE_QUERY_TASK, 0, make_event_data(session, in ? in->mfx.FrameInfo.Width : 0, in ? in->mfx.FrameInfo.Height : 0, in ? in->mfx.CodecId : 0, in ? in->mfx.TargetUsage : 0, in ? in->mfx.LowPower : 0), [&](){ return make_event_data(mfxRes);});
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, in);

    bool bIsHWENCSupport = false;

    try
    {
        {
            CodecId2Handlers::const_iterator handler;
            handler = codecId2Handlers.find(CodecKey(out->mfx.CodecId));
            
            mfxRes = handler == codecId2Handlers.end() ? MFX_ERR_UNSUPPORTED
                : (handler->second.primary.query)(session, in, out);

            if (MFX_WRN_PARTIAL_ACCELERATION == mfxRes)
            {
                assert(handler != codecId2Handlers.end());

                mfxRes = !handler->second.fallback.query ? MFX_ERR_UNSUPPORTED
                    : (handler->second.fallback.query)(session, in, out);
            }
            else
            {
                bIsHWENCSupport = true;
            }
        }
    }
    // handle error(s)
    catch(...)
    {
        mfxRes = MFX_ERR_NULL_PTR;
    }

    if (MFX_PLATFORM_HARDWARE == session->m_currentPlatform &&
        !bIsHWENCSupport &&
        MFX_ERR_NONE <= mfxRes)
    {
        mfxRes = MFX_ERR_UNSUPPORTED;
    }

    if (mfxRes == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM || mfxRes == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
    {
        try
        {
            mfx_reflect::AccessibleTypesCollection g_Reflection = GetReflection();
            if (g_Reflection.m_bIsInitialized)
            {
                std::string result = mfx_reflect::CompareStructsToString(g_Reflection.Access(in), g_Reflection.Access(out));
                MFX_LTRACE_MSG(MFX_TRACE_LEVEL_INTERNAL, result.c_str())
            }
        }
        catch (const std::exception& e)
        {
            MFX_LTRACE_MSG(MFX_TRACE_LEVEL_INTERNAL, e.what());
        }
        catch (...)
        {
            MFX_LTRACE_MSG(MFX_TRACE_LEVEL_INTERNAL, "Unknown exception was caught while comparing In and Out VideoParams.");
        }
    }


    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, out);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoENCODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);
    MFX_CHECK(request, MFX_ERR_NULL_PTR);

    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVideoENCODE_QueryIOSurf");
    PERF_EVENT(MFX_TRACE_API_ENCODE_QUERY_IOSURF_TASK, 0, make_event_data(session, par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height, par->mfx.CodecId, par->mfx.TargetUsage, par->mfx.LowPower), [&](){ return make_event_data(mfxRes);});
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, par);

    bool bIsHWENCSupport = false;

    try
    {
        {
            CodecId2Handlers::const_iterator handler;
            handler = codecId2Handlers.find(CodecKey(par->mfx.CodecId));

            mfxRes = handler == codecId2Handlers.end() ? MFX_ERR_INVALID_VIDEO_PARAM
                : (handler->second.primary.queryIOSurf)(session, par, request);

            if (MFX_WRN_PARTIAL_ACCELERATION == mfxRes)
            {
                assert(handler != codecId2Handlers.end());

                mfxRes = !handler->second.fallback.query ? MFX_ERR_INVALID_VIDEO_PARAM
                    : (handler->second.fallback.queryIOSurf)(session, par, request);

            }
            else
            {
                bIsHWENCSupport = true;
            }
        }
    }
    // handle error(s)
    catch(...)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }

    if (MFX_PLATFORM_HARDWARE == session->m_currentPlatform &&
        !bIsHWENCSupport &&
        MFX_ERR_NONE <= mfxRes)
    {
        mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
    }


    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, request);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

static mfxStatus SetupCache(mfxSession session, const mfxVideoParam& par)
{
    // No internal alloc if Ext Allocator set
    if (session->m_pCORE->IsExternalFrameAllocator())
        return MFX_ERR_NONE;

    mfxU16 memory_type = mfxU16(par.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ? MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET);

    auto& pCache = session->m_pENCODE->m_pSurfaceCache;

    if (!pCache)
    {
        auto base_core_vpl = dynamic_cast<CommonCORE_VPL*>(session->m_pCORE.get());
        MFX_CHECK_HDL(base_core_vpl);

        std::unique_ptr<SurfaceCache> scoped_cache_ptr(SurfaceCache::Create(*base_core_vpl, memory_type, par.mfx.FrameInfo));

        using cache_controller = surface_cache_controller<SurfaceCache>;
        using TCachePtr = std::remove_reference<decltype(pCache)>::type;

        pCache = TCachePtr(new cache_controller(scoped_cache_ptr.get()), std::default_delete<cache_controller>());

        scoped_cache_ptr.release();
    }

    // Setup cache limits
    MFX_SAFE_CALL(pCache->SetupCache(session, par));

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODE_Init(mfxSession session, mfxVideoParam *par)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVideoENCODE_Init");
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, par);

    PERF_EVENT(MFX_TRACE_API_ENCODE_INIT_TASK, 0, make_event_data(session, par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height, par->mfx.CodecId, par->mfx.TargetUsage, par->mfx.LowPower), [&](){ return make_event_data(mfxRes);});

    try
    {
        // check existence of component
        if (!session->m_pENCODE)
        {
            // create a new instance
            session->m_bIsHWENCSupport = true;
            session->m_pENCODE.reset(session->Create<VideoENCODE>(*par));
            MFX_CHECK(session->m_pENCODE.get(), MFX_ERR_INVALID_VIDEO_PARAM);
        }

        mfxRes = session->m_pENCODE->Init(par);

        if (MFX_WRN_PARTIAL_ACCELERATION == mfxRes)
        {
            session->m_bIsHWENCSupport = false;
            mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
        }
        else if (mfxRes >= MFX_ERR_NONE)
            session->m_bIsHWENCSupport = true;

        if (MFX_PLATFORM_HARDWARE == session->m_currentPlatform &&
            !session->m_bIsHWENCSupport &&
            MFX_ERR_NONE <= mfxRes)
        {
            mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
        }

        if (mfxRes >= MFX_ERR_NONE)
        {
            MFX_SAFE_CALL(SetupCache(session, *par));
        }
    }
    // handle std::exception(s)
    catch (const std::exception & ex)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
#if defined(_DEBUG)
        printf("EHW Exception: %s\n", ex.what());
        fflush(stdout);
#else
        std::ignore = ex;
#endif
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }


    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoENCODE_Close(mfxSession session)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVideoENCODE_Close");
    PERF_EVENT(MFX_TRACE_API_ENCODE_CLOSE_TASK, 0, make_event_data(session), [&](){ return make_event_data(mfxRes);});

    MFX_CHECK(session,               MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(session->m_pENCODE,    MFX_ERR_NOT_INITIALIZED);

    try
    {
        // wait until all tasks are processed
        std::ignore = MFX_STS_TRACE(session->m_pScheduler->WaitForAllTasksCompletion(session->m_pENCODE.get()));

        mfxRes = session->m_pENCODE->Close();

        session->m_pENCODE.reset(nullptr);
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

static
mfxStatus MFXVideoENCODELegacyRoutine(void *pState, void *pParam,
                                      mfxU32 threadNumber, mfxU32 callNumber)
{
    (void)callNumber;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "EncodeFrame");
    VideoENCODE *pENCODE = (VideoENCODE *) pState;
    MFX_THREAD_TASK_PARAMETERS *pTaskParam = (MFX_THREAD_TASK_PARAMETERS *) pParam;
    mfxStatus mfxRes;

    // check error(s)
    if ((NULL == pState) ||
        (NULL == pParam) ||
        (0 != threadNumber))
    {
        return MFX_ERR_NULL_PTR;
    }

    // call the obsolete method
    mfxRes = pENCODE->EncodeFrame(pTaskParam->encode.ctrl,
                                  &pTaskParam->encode.internal_params,
                                  pTaskParam->encode.surface,
                                  pTaskParam->encode.bs);

    return mfxRes;

} // mfxStatus MFXVideoENCODELegacyRoutine(void *pState, void *pParam,

enum
{
    MFX_NUM_ENTRY_POINTS = 2
};

mfxStatus MFXVideoENCODE_EncodeFrameAsync(mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)
{
    mfxStatus mfxRes;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVideoENCODE_EncodeFrameAsync");
    PERF_EVENT(MFX_TRACE_API_ENCODE_FRAME_ASYNC_TASK, 0, make_event_data(session, surface), [&](){ return make_event_data(mfxRes, syncp ? *syncp : nullptr);});
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, ctrl);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, surface);

    MFX_CHECK_HDL(session);
    MFX_CHECK(session->m_pENCODE.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(syncp,                    MFX_ERR_NULL_PTR);

    try
    {
        if (surface && session->m_pENCODE->m_pSurfaceCache)
        {
            MFX_SAFE_CALL(session->m_pENCODE->m_pSurfaceCache->Update(*surface));
        }
        {
            mfxSyncPoint syncPoint = NULL;
            mfxFrameSurface1* reordered_surface = NULL;
            mfxEncodeInternalParams internal_params;
            MFX_ENTRY_POINT entryPoints[MFX_NUM_ENTRY_POINTS];
            mfxU32 numEntryPoints = MFX_NUM_ENTRY_POINTS;

            memset(&entryPoints, 0, sizeof(entryPoints));
            mfxRes = session->m_pENCODE->EncodeFrameCheck(ctrl,
                surface,
                bs,
                &reordered_surface,
                &internal_params,
                entryPoints,
                numEntryPoints);
            // source data is OK, go forward
            if ((MFX_ERR_NONE == mfxRes) ||
                (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfxRes) ||
                (MFX_WRN_OUT_OF_RANGE == mfxRes) ||
                // WHAT IS IT??? IT SHOULD BE REMOVED
                ((mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes) ||
                (MFX_ERR_MORE_BITSTREAM == mfxRes))
            {
                // prepare the obsolete kind of task.
                // it is obsolete and must be removed.
                if (NULL == entryPoints[0].pRoutine)
                {
                    MFX_TASK task;

                    memset(&task, 0, sizeof(task));
                    // BEGIN OF OBSOLETE PART
                    task.bObsoleteTask = true;
                    task.obsolete_params.encode.internal_params = internal_params;
                    // fill task info
                    task.pOwner = session->m_pENCODE.get();
                    task.entryPoint.pRoutine = &MFXVideoENCODELegacyRoutine;
                    task.entryPoint.pState = session->m_pENCODE.get();
                    task.entryPoint.requiredNumThreads = 1;

                    // fill legacy parameters
                    task.obsolete_params.encode.ctrl = ctrl;
                    task.obsolete_params.encode.surface = reordered_surface;
                    task.obsolete_params.encode.bs = bs;
                    // END OF OBSOLETE PART

                    task.priority = session->m_priority;
                    task.threadingPolicy = session->m_pENCODE->GetThreadingPolicy();
                    // fill dependencies
                    task.pSrc[0] = surface;
                    task.pDst[0] = ((mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes) ? 0 : bs;
                    task.pSrc[1] = bs;
                    task.pSrc[2] = ctrl ? ctrl->ExtParam : 0;

#ifdef MFX_TRACE_ENABLE
                    task.nParentId = MFX_AUTO_TRACE_GETID();
                    task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_ENCODE;
#endif // MFX_TRACE_ENABLE

                    // register input and call the task
                    MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));
                }
                else if (1 == numEntryPoints)
                {
                    MFX_TASK task;

                    memset(&task, 0, sizeof(task));
                    task.pOwner = session->m_pENCODE.get();
                    task.entryPoint = entryPoints[0];
                    task.priority = session->m_priority;
                    task.threadingPolicy = session->m_pENCODE->GetThreadingPolicy();
                    // fill dependencies
                    task.pSrc[0] = surface;
                    task.pSrc[1] = bs;
                    task.pSrc[2] = ctrl ? ctrl->ExtParam : 0;
                    task.pDst[0] = ((mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes) ? 0 : bs;


#ifdef MFX_TRACE_ENABLE
                    task.nParentId = MFX_AUTO_TRACE_GETID();
                    task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_ENCODE;
#endif
                    // register input and call the task
                    MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));
                }
                else
                {
                    MFX_TASK task;

                    memset(&task, 0, sizeof(task));
                    task.pOwner = session->m_pENCODE.get();
                    task.entryPoint = entryPoints[0];
                    task.priority = session->m_priority;
                    task.threadingPolicy = session->m_pENCODE->GetThreadingPolicy();
                    // fill dependencies
                    task.pSrc[0] = surface;
                    task.pSrc[1] = ctrl ? ctrl->ExtParam : 0;
                    task.pDst[0] = entryPoints[0].pParam;

#ifdef MFX_TRACE_ENABLE
                    task.nParentId = MFX_AUTO_TRACE_GETID();
                    task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_ENCODE;
#endif
                    // register input and call the task
                    MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));

                    memset(&task, 0, sizeof(task));
                    task.pOwner = session->m_pENCODE.get();
                    task.entryPoint = entryPoints[1];
                    task.priority = session->m_priority;
                    task.threadingPolicy = session->m_pENCODE->GetThreadingPolicy();
                    // fill dependencies
                    task.pSrc[0] = entryPoints[0].pParam;
                    task.pDst[0] = ((mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes) ? 0 : bs;

#ifdef MFX_TRACE_ENABLE
                    task.nParentId = MFX_AUTO_TRACE_GETID();
                    task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_ENCODE2;
#endif
                    // register input and call the task
                    MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));
                }

                // IT SHOULD BE REMOVED
                if ((mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes)
                {
                    mfxRes = MFX_ERR_MORE_DATA;
                    syncPoint = NULL;
                }
            }

            // return pointer to synchronization point
            *syncp = syncPoint;
        }
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, bs);
    if (mfxRes == MFX_ERR_NONE && syncp)
    {
        MFX_LTRACE_P(MFX_TRACE_LEVEL_API, *syncp);
    }
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;

} // mfxStatus MFXVideoENCODE_EncodeFrameAsync(mfxSession session, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)

mfxStatus MFXMemory_GetSurfaceForEncode(mfxSession session, mfxFrameSurface1** output_surf)
{
    MFX_CHECK_NULL_PTR1(output_surf);
    MFX_CHECK_HDL(session);
    MFX_CHECK(session->m_pENCODE,                  MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(session->m_pENCODE->m_pSurfaceCache, MFX_ERR_NOT_INITIALIZED);

    try
    {
        MFX_RETURN((*session->m_pENCODE->m_pSurfaceCache)->GetSurface(*output_surf));
    }
    catch (...)
    {
        MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
    }
}

mfxStatus QueryImplsDescription(VideoCORE& core, mfxEncoderDescription& caps, mfx::PODArraysHolder& ah)
{
    for (auto& c : codecId2Handlers)
    {
        if (!c.second.primary.QueryImplsDescription)
            continue;

        mfxEncoderDescription::encoder enc = {};
        enc.CodecID = c.first.codecId;

        if (MFX_ERR_NONE != c.second.primary.QueryImplsDescription(core, enc, ah))
            continue;

        ah.PushBack(caps.Codecs) = enc;
        ++caps.NumCodecs;
    }

    return MFX_ERR_NONE;
}

//
// THE OTHER ENCODE FUNCTIONS HAVE IMPLICIT IMPLEMENTATION
//

FUNCTION_RESET_IMPL(ENCODE, Reset, (mfxSession session, mfxVideoParam *par), (par))
FUNCTION_IMPL(ENCODE, GetVideoParam, (mfxSession session, mfxVideoParam *par), (par))
FUNCTION_IMPL(ENCODE, GetEncodeStat, (mfxSession session, mfxEncodeStat *stat), (stat))
