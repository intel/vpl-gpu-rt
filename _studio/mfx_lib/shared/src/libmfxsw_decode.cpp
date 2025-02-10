// Copyright (c) 2018-2025 Intel Corporation
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

#include <mfx_session.h>
#include <mfx_tools.h>
#include <mfx_common.h>

// sheduling and threading stuff
#include <mfx_task.h>

#include <libmfx_core.h>
#include <functional>

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)
#include "mfx_vc1_decode.h"
#endif

#if defined (MFX_ENABLE_H264_VIDEO_DECODE)
#include "mfx_h264_dec_decode.h"
#endif

#if defined (MFX_ENABLE_H265_VIDEO_DECODE)
#include "mfx_h265_dec_decode.h"
#endif

#if defined (MFX_ENABLE_MPEG2_VIDEO_DECODE)
#include "mfx_mpeg2_decode.h"
#endif

#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)
#include "mfx_mjpeg_dec_decode.h"
#endif

#if defined (MFX_ENABLE_VP8_VIDEO_DECODE)
#include "mfx_vp8_dec_decode_hw.h"
#endif

#if defined (MFX_ENABLE_VP9_VIDEO_DECODE)
#include "mfx_vp9_dec_decode_hw.h"
#endif

#if defined (MFX_ENABLE_AV1_VIDEO_DECODE)
#include "mfx_av1_dec_decode.h"
#endif

#if defined (MFX_ENABLE_VVC_VIDEO_DECODE)
#include "mfx_vvc_dec_decode.h"
#endif

#include "mfx_unified_decode_logging.h"

template<>
VideoDECODE* _mfxSession::Create<VideoDECODE>(mfxVideoParam& par)
{
    VideoDECODE* pDECODE = nullptr;
    VideoCORE* core = m_pCORE.get();
    mfxStatus mfxRes = MFX_ERR_MEMORY_ALLOC;
    mfxU32 CodecId = par.mfx.CodecId;

    // create a codec instance
    switch (CodecId)
    {
#if defined (MFX_ENABLE_MPEG2_VIDEO_DECODE)
    case MFX_CODEC_MPEG2:
        pDECODE = new VideoDECODEMPEG2(core, &mfxRes);
        break;
#endif

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)
    case MFX_CODEC_VC1:
        pDECODE = new MFXVideoDECODEVC1(core, &mfxRes);
        break;
#endif

#if defined (MFX_ENABLE_H264_VIDEO_DECODE)
    case MFX_CODEC_AVC:
        pDECODE = new VideoDECODEH264(core, &mfxRes);
        break;
#endif

#if defined (MFX_ENABLE_H265_VIDEO_DECODE)
    case MFX_CODEC_HEVC:
        pDECODE = new VideoDECODEH265(core, &mfxRes);
        break;
#endif

#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)
    case MFX_CODEC_JPEG:
        pDECODE = new VideoDECODEMJPEG(core, &mfxRes);
        break;
#endif

#if defined (MFX_ENABLE_VP8_VIDEO_DECODE)
    case MFX_CODEC_VP8:
        pDECODE = new VideoDECODEVP8_HW(core, &mfxRes);
        break;
#endif // MFX_ENABLE_VP8_VIDEO_DECODE

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE)
    case MFX_CODEC_VP9:
        pDECODE = new VideoDECODEVP9_HW(core, &mfxRes);
        break;
#endif

#if defined (MFX_ENABLE_AV1_VIDEO_DECODE)
     case MFX_CODEC_AV1:
         pDECODE = new VideoDECODEAV1(core, &mfxRes);
         break;
#endif

#if defined (MFX_ENABLE_VVC_VIDEO_DECODE)
     case MFX_CODEC_VVC:
         pDECODE = new VideoDECODEVVC(core, &mfxRes);
         break;
#endif
    default:
        break;
    }

    // check error(s)
    if (MFX_ERR_NONE != mfxRes)
    {
        delete pDECODE;
        pDECODE = nullptr;
    }

    return pDECODE;

}

mfxStatus MFXVideoDECODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    PERF_UTILITY_AUTO(__FUNCTION__, PERF_LEVEL_API);
    InitMfxLogging();

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(out, MFX_ERR_NULL_PTR);

    TRACE_EVENT(MFX_TRACE_API_DECODE_QUERY_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(session, in ? in->mfx.FrameInfo.Width : 0, in ? in->mfx.FrameInfo.Height : 0, in ? in->mfx.CodecId : 0));

#ifndef ANDROID
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

    mfxStatus mfxRes;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  session = ", MFX_TRACE_FORMAT_P, session);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API_PARAMS, "In:  ", in);

    try
    {
        switch (out->mfx.CodecId)
        {
#ifdef MFX_ENABLE_VC1_VIDEO_DECODE
        case MFX_CODEC_VC1:
            mfxRes = MFXVideoDECODEVC1::Query(session->m_pCORE.get(), in, out);
            break;
#endif

#ifdef MFX_ENABLE_H264_VIDEO_DECODE
        case MFX_CODEC_AVC:
            mfxRes = VideoDECODEH264::Query(session->m_pCORE.get(), in, out);
            break;
#endif

#ifdef MFX_ENABLE_H265_VIDEO_DECODE
        case MFX_CODEC_HEVC:
            mfxRes = VideoDECODEH265::Query(session->m_pCORE.get(), in, out);
            break;
#endif

#ifdef MFX_ENABLE_MPEG2_VIDEO_DECODE
        case MFX_CODEC_MPEG2:
            mfxRes = VideoDECODEMPEG2::Query(session->m_pCORE.get(), in, out);
            break;
#endif

#ifdef MFX_ENABLE_MJPEG_VIDEO_DECODE
        case MFX_CODEC_JPEG:
            mfxRes = VideoDECODEMJPEG::Query(session->m_pCORE.get(), in, out);
            break;
#endif

#if defined (MFX_ENABLE_VP8_VIDEO_DECODE)
        case MFX_CODEC_VP8:
            mfxRes = VideoDECODEVP8_HW::Query(session->m_pCORE.get(), in, out);
            break;
#endif // MFX_ENABLE_VP8_VIDEO_DECODE

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE)
        case MFX_CODEC_VP9:
            mfxRes = VideoDECODEVP9_HW::Query(session->m_pCORE.get(), in, out);
            break;
#endif

#ifdef MFX_ENABLE_AV1_VIDEO_DECODE
        case MFX_CODEC_AV1:
            mfxRes = VideoDECODEAV1::Query(session->m_pCORE.get(), in, out);
            break;
#endif

#ifdef MFX_ENABLE_VVC_VIDEO_DECODE
        case MFX_CODEC_VVC:
            mfxRes = VideoDECODEVVC::Query(session->m_pCORE.get(), in, out);
            break;
#endif
        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }

        TRACE_EVENT(MFX_TRACE_API_DECODE_QUERY_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(mfxRes));
    }
    // handle error(s)
    catch(...)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API_PARAMS, "Out:  ", out);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoDECODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    PERF_UTILITY_AUTO(__FUNCTION__, PERF_LEVEL_API);
    InitMfxLogging();

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);
    MFX_CHECK(request, MFX_ERR_NULL_PTR);
    TRACE_EVENT(MFX_TRACE_API_DECODE_QUERY_IOSURF_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(session, par ? (uint32_t)par->mfx.FrameInfo.Width : 0, par ? (uint32_t)par->mfx.FrameInfo.Height : 0, par ? par->mfx.CodecId : 0));

    mfxStatus mfxRes;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  session = ", MFX_TRACE_FORMAT_P, session);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API_PARAMS, "In:  ", par);

    try
    {
        switch (par->mfx.CodecId)
        {
#ifdef MFX_ENABLE_VC1_VIDEO_DECODE
        case MFX_CODEC_VC1:
            mfxRes = MFXVideoDECODEVC1::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif

#ifdef MFX_ENABLE_H264_VIDEO_DECODE
        case MFX_CODEC_AVC:
            mfxRes = VideoDECODEH264::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif

#ifdef MFX_ENABLE_H265_VIDEO_DECODE
        case MFX_CODEC_HEVC:
            mfxRes = VideoDECODEH265::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif

#ifdef MFX_ENABLE_MPEG2_VIDEO_DECODE
        case MFX_CODEC_MPEG2:
            mfxRes = VideoDECODEMPEG2::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif

#ifdef MFX_ENABLE_MJPEG_VIDEO_DECODE
        case MFX_CODEC_JPEG:
            mfxRes = VideoDECODEMJPEG::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif

#if defined (MFX_ENABLE_VP8_VIDEO_DECODE)
        case MFX_CODEC_VP8:
            mfxRes = VideoDECODEVP8_HW::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif // MFX_ENABLE_VP8_VIDEO_DECODE

#if defined (MFX_ENABLE_VP9_VIDEO_DECODE)
        case MFX_CODEC_VP9:
            mfxRes = VideoDECODEVP9_HW::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif // MFX_ENABLE_VP9_VIDEO_DECODE

#ifdef MFX_ENABLE_AV1_VIDEO_DECODE
        case MFX_CODEC_AV1:
            mfxRes = VideoDECODEAV1::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif

#ifdef MFX_ENABLE_VVC_VIDEO_DECODE
        case MFX_CODEC_VVC:
            mfxRes = VideoDECODEVVC::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif
        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }

        TRACE_EVENT(MFX_TRACE_API_DECODE_QUERY_IOSURF_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(mfxRes));
    }
    // handle error(s)
    catch(...)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }

    TRACE_BUFFER_EVENT(MFX_TRACE_API_DECODE_QUERY_IOSURF_TASK, EVENT_TYPE_INFO, TR_KEY_MFX_API,
        request, DecodeQueryParam, DECODE_QUERY);

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API_PARAMS, "Out:  ", request);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoDECODE_DecodeHeader(mfxSession session, mfxBitstream *bs, mfxVideoParam *par)
{
    PERF_UTILITY_AUTO(__FUNCTION__, PERF_LEVEL_API);
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(bs, MFX_ERR_NULL_PTR);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);
    TRACE_EVENT(MFX_TRACE_API_DECODE_HEADER_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(session, bs, bs ? bs->DataLength : 0));

    mfxStatus mfxRes;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  session = ", MFX_TRACE_FORMAT_P, session);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API_PARAMS, "In:  ", bs);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API_PARAMS, "In:  ", par);

    try
    {
        switch (par->mfx.CodecId)
        {
#ifdef MFX_ENABLE_VC1_VIDEO_DECODE
        case MFX_CODEC_VC1:
            mfxRes = MFXVideoDECODEVC1::DecodeHeader(session->m_pCORE.get(), bs, par);
            break;
#endif

#ifdef MFX_ENABLE_H264_VIDEO_DECODE
        case MFX_CODEC_AVC:
            mfxRes = VideoDECODEH264::DecodeHeader(session->m_pCORE.get(), bs, par);
            break;
#endif

#ifdef MFX_ENABLE_H265_VIDEO_DECODE
        case MFX_CODEC_HEVC:
            mfxRes = VideoDECODEH265::DecodeHeader(session->m_pCORE.get(), bs, par);
            break;
#endif

#ifdef MFX_ENABLE_MPEG2_VIDEO_DECODE
        case MFX_CODEC_MPEG2:
            mfxRes = VideoDECODEMPEG2::DecodeHeader(session->m_pCORE.get(), bs, par);
            break;
#endif

#ifdef MFX_ENABLE_MJPEG_VIDEO_DECODE
        case MFX_CODEC_JPEG:
            mfxRes = VideoDECODEMJPEG::DecodeHeader(session->m_pCORE.get(), bs, par);
            break;
#endif

#if defined(MFX_ENABLE_VP8_VIDEO_DECODE)
        case MFX_CODEC_VP8:
            mfxRes = VP8DecodeCommon::DecodeHeader(session->m_pCORE.get(), bs, par);
            break;
#endif

#if defined (MFX_ENABLE_VP9_VIDEO_DECODE)
        case MFX_CODEC_VP9:
            mfxRes = VideoDECODEVP9_HW::DecodeHeader(session->m_pCORE.get(), bs, par);
            break;
#endif // MFX_ENABLE_VP9_VIDEO_DECODE

#ifdef MFX_ENABLE_AV1_VIDEO_DECODE
        case MFX_CODEC_AV1:
            mfxRes = VideoDECODEAV1::DecodeHeader(session->m_pCORE.get(), bs, par);
            break;
#endif

#ifdef MFX_ENABLE_VVC_VIDEO_DECODE
        case MFX_CODEC_VVC:
            mfxRes = VideoDECODEVVC::DecodeHeader(session->m_pCORE.get(), bs, par);
            break;
#endif
        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }

        TRACE_EVENT(MFX_TRACE_API_DECODE_HEADER_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(mfxRes));
    }
    // handle error(s)
    catch(...)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoDECODE_Init(mfxSession session, mfxVideoParam *par)
{
    PERF_UTILITY_AUTO(__FUNCTION__, PERF_LEVEL_API);
    InitMfxLogging();
    TRACE_EVENT(MFX_TRACE_API_DECODE_INIT_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(session, par ? par->mfx.FrameInfo.Width : 0, 
        par ? par->mfx.FrameInfo.Height : 0, par ? par->AsyncDepth : 0, par ? par->mfx.DecodedOrder : 0, par ? par->mfx.CodecId : 0));

    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  session = ", MFX_TRACE_FORMAT_P, session);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API_PARAMS, "In:  ", par);

    try
    {
        // check existence of component
        if (!session->m_pDECODE)
        {
            // create a new instance
            session->m_pDECODE.reset(session->Create<VideoDECODE>(*par));
            MFX_CHECK(session->m_pDECODE.get(), MFX_ERR_INVALID_VIDEO_PARAM);
        }

        mfxRes = session->m_pDECODE->Init(par);

        TRACE_EVENT(MFX_TRACE_API_DECODE_INIT_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(mfxRes));
    }
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    TRACE_BUFFER_EVENT(MFX_TRACE_API_DECODE_INIT_TASK, EVENT_TYPE_INFO, TR_KEY_MFX_API,
        par, DecodeInitParam, DECODE_INIT);

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoDECODE_Close(mfxSession session)
{
    PERF_UTILITY_AUTO(__FUNCTION__, PERF_LEVEL_API);
    mfxStatus mfxRes = MFX_ERR_NONE;
    TRACE_EVENT(MFX_TRACE_API_DECODE_CLOSE_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(session));

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  session = ", MFX_TRACE_FORMAT_P, session);

    try
    {
        if (!session->m_pDECODE)
        {
            return MFX_ERR_NOT_INITIALIZED;
        }

        // wait until all tasks are processed
        session->m_pScheduler->WaitForAllTasksCompletion(session->m_pDECODE.get());

        mfxRes = session->m_pDECODE->Close();

        session->m_pDECODE.reset(nullptr);

        TRACE_EVENT(MFX_TRACE_API_DECODE_CLOSE_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(mfxRes));
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

mfxStatus MFXVideoDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    PERF_UTILITY_AUTO(__FUNCTION__, PERF_LEVEL_API);
    TRACE_EVENT(MFX_TRACE_API_DECODE_FRAME_ASYNC_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(session, surface_work, bs ? bs->DataLength : 0, bs ? bs->DataFlag : 0));

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  session = ", MFX_TRACE_FORMAT_P, session);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API_PARAMS, "In:  ", bs);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API_PARAMS, "In:  ", surface_work);

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(session->m_pDECODE.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(syncp);
    MFX_CHECK_NULL_PTR1(surface_out);

    try
    {
        mfxSyncPoint syncPoint = NULL;
        MFX_TASK task;

        // Wait for the bit stream
        mfxRes = session->m_pScheduler->WaitForDependencyResolved(bs);
        MFX_CHECK_STS(mfxRes);

        // reset the sync point
        *syncp = NULL;
        *surface_out = NULL;

        memset(&task, 0, sizeof(MFX_TASK));
        mfxRes = session->m_pDECODE->DecodeFrameCheck(bs, surface_work, surface_out, &task.entryPoint);
        MFX_CHECK(mfxRes >= 0 || MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes
                              || MFX_ERR_MORE_DATA             == mfxRes
                              || MFX_ERR_MORE_SURFACE          == mfxRes, mfxRes);

        // source data is OK, go forward
        if (task.entryPoint.pRoutine)
        {
            mfxStatus mfxAddRes;

            task.pOwner = session->m_pDECODE.get();
            task.priority = session->m_priority;
            task.threadingPolicy = session->m_pDECODE->GetThreadingPolicy();
            // fill dependencies
            task.pDst[0] = *surface_out;

#ifdef MFX_TRACE_ENABLE
            task.nParentId = MFX_AUTO_TRACE_GETID();
            task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_DECODE;
#endif

            // register input and call the task
            PERF_UTILITY_SET_ASYNC_TASK_ID(task.nTaskId);
            MFX_LTRACE_1(MFX_TRACE_LEVEL_SCHED, "Current Task ID = ", MFX_TRACE_FORMAT_I, task.nTaskId);
            mfxAddRes = session->m_pScheduler->AddTask(task, &syncPoint);
            MFX_CHECK_STS(mfxAddRes);

            if (syncPoint && *surface_out && (*surface_out)->FrameInterface && (*surface_out)->FrameInterface->Synchronize && !session->m_pCORE->IsExternalFrameAllocator())
            {
                MFX_CHECK_HDL((*surface_out)->FrameInterface->Context);
                static_cast<mfxFrameSurfaceBaseInterface*>((*surface_out)->FrameInterface->Context)->SetSyncPoint(syncPoint);
            }
        }

        if (MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes)
        {
            mfxRes = MFX_WRN_DEVICE_BUSY;
        }
        // Self allocation (i.e. memory model 3), GetSurface timeout expired
        else if (!surface_work && mfxRes == MFX_ERR_MORE_SURFACE)
        {
            mfxRes = MFX_WRN_ALLOC_TIMEOUT_EXPIRED;
        }

        // return pointer to synchronization point
        if (MFX_ERR_NONE == mfxRes || (mfxRes == MFX_WRN_VIDEO_PARAM_CHANGED && *surface_out != NULL))
        {
            *syncp = syncPoint;
        }

        TRACE_EVENT(MFX_TRACE_API_DECODE_FRAME_ASYNC_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(mfxRes, *syncp));
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    if (mfxRes == MFX_ERR_NONE)
    {
        if (surface_out && *surface_out)
        {
            MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API_PARAMS, "Out:  " , *surface_out);
        }
        if (syncp)
        {
            MFX_LTRACE_P(MFX_TRACE_LEVEL_API, *syncp);
        }
    }
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);

    return mfxRes;

} // mfxStatus MFXVideoDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_dec, mfxFrameSurface1 **surface_disp, mfxSyncPoint *syncp)


struct DHandlers {
    std::function<mfxStatus(VideoCORE&, mfxDecoderDescription::decoder&, mfx::PODArraysHolder&)> QueryImplsDescription;
};
typedef std::map<mfxU32, DHandlers> CodecId2Handlers;

mfxStatus QueryImplsDescription(VideoCORE& core, mfxDecoderDescription& caps, mfx::PODArraysHolder& ah, const std::vector<mfxU32>& codecIds)
{
    static const CodecId2Handlers codecId2Handlers =
    {
    #if defined (MFX_ENABLE_MPEG2_VIDEO_DECODE)
        {
            MFX_CODEC_MPEG2,
            {
            // .QueryImplsDescription =
            [](VideoCORE& core, mfxDecoderDescription::decoder& caps, mfx::PODArraysHolder& ah)
            {
                return VideoDECODEMPEG2::QueryImplsDescription(core, caps, ah);
            }
            }
    },
    #endif
    #if defined (MFX_ENABLE_VC1_VIDEO_DECODE)
        {
            MFX_CODEC_VC1,
            {
                // .QueryImplsDescription =
                [](VideoCORE& core, mfxDecoderDescription::decoder& caps, mfx::PODArraysHolder& ah)
                {
                    return MFXVideoDECODEVC1::QueryImplsDescription(core, caps, ah);
                }
            }
        },
    #endif
    #if defined (MFX_ENABLE_H264_VIDEO_DECODE)
        {
            MFX_CODEC_AVC,
            {
                // .QueryImplsDescription =
                [](VideoCORE& core, mfxDecoderDescription::decoder& caps, mfx::PODArraysHolder& ah)
                {
                    return VideoDECODEH264::QueryImplsDescription(core, caps, ah);
                }
            }
        },
    #endif
    #if defined (MFX_ENABLE_H265_VIDEO_DECODE)
        {
            MFX_CODEC_HEVC,
            {
                // .QueryImplsDescription =
                [](VideoCORE& core, mfxDecoderDescription::decoder& caps, mfx::PODArraysHolder& ah)
                {
                    return VideoDECODEH265::QueryImplsDescription(core, caps, ah);
                }
            }
        },
    #endif
    #if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)
        {
            MFX_CODEC_JPEG,
            {
                // .QueryImplsDescription =
                [](VideoCORE& core, mfxDecoderDescription::decoder& caps, mfx::PODArraysHolder& ah)
                {
                    return VideoDECODEMJPEG::QueryImplsDescription(core, caps, ah);
                }
            }
        },
    #endif
    #if defined (MFX_ENABLE_VP8_VIDEO_DECODE)
       {
           MFX_CODEC_VP8,
           {
               // .QueryImplsDescription =
               [](VideoCORE& core, mfxDecoderDescription::decoder& caps, mfx::PODArraysHolder& ah)
               {
                   return VideoDECODEVP8_HW::QueryImplsDescription(core, caps, ah);
               }
           }
       },
    #endif
    #if defined (MFX_ENABLE_VP9_VIDEO_DECODE)
       {
           MFX_CODEC_VP9,
           {
               // .QueryImplsDescription =
               [](VideoCORE& core, mfxDecoderDescription::decoder& caps, mfx::PODArraysHolder& ah)
               {
                   return VideoDECODEVP9_HW::QueryImplsDescription(core, caps, ah);
               }
           }
       },
    #endif
    #if defined (MFX_ENABLE_AV1_VIDEO_DECODE)
       {
           MFX_CODEC_AV1,
           {
               // .QueryImplsDescription =
               [](VideoCORE& core, mfxDecoderDescription::decoder& caps, mfx::PODArraysHolder& ah)
               {
                   return VideoDECODEAV1::QueryImplsDescription(core, caps, ah);
               }
           }
       },
    #endif
    #if defined (MFX_ENABLE_VVC_VIDEO_DECODE)
       {
           MFX_CODEC_VVC,
           {
               // .QueryImplsDescription =
               [](VideoCORE& core, mfxDecoderDescription::decoder& caps, mfx::PODArraysHolder& ah)
               {
                   return VideoDECODEVVC::QueryImplsDescription(core, caps, ah);
               }
           }
       },
    #endif
    };

    auto queryCodec = [&](auto& handler, const mfxU32& codecId)
    {
        if (!handler.QueryImplsDescription)
            return;

        mfxDecoderDescription::decoder dec = {};
        dec.CodecID = codecId;

        if (MFX_ERR_NONE != handler.QueryImplsDescription(core, dec, ah))
            return;

        ah.PushBack(caps.Codecs) = dec;
        ++caps.NumCodecs;
    };

    if (codecIds.size() == 0)
    {
        for (auto& c : codecId2Handlers)
        {
            queryCodec(c.second, c.first);
        }
    }
    else
    {
        for (auto& codecId : codecIds)
        {
            auto c = codecId2Handlers.find(codecId);

            if (c == codecId2Handlers.end())
                continue;

            queryCodec(c->second, codecId);
        }
    }

    return MFX_ERR_NONE;
}


//
// THE OTHER DECODE FUNCTIONS HAVE IMPLICIT IMPLEMENTATION
//

FUNCTION_RESET_IMPL(DECODE, Reset, (mfxSession session, mfxVideoParam *par), (par))

FUNCTION_IMPL(DECODE, GetVideoParam, (mfxSession session, mfxVideoParam *par), (par))
FUNCTION_IMPL(DECODE, GetDecodeStat, (mfxSession session, mfxDecodeStat *stat), (stat))
FUNCTION_IMPL(DECODE, SetSkipMode, (mfxSession session, mfxSkipMode mode), (mode))
FUNCTION_IMPL(DECODE, GetPayload, (mfxSession session, mfxU64 *ts, mfxPayload *payload), (ts, payload))
