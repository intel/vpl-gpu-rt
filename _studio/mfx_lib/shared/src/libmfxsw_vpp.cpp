// Copyright (c) 2008-2025 Intel Corporation
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

#include <libmfx_allocator.h>

#ifdef MFX_ENABLE_VPP
// VPP include files here
#include "mfx_vpp_main.h"       // this VideoVPP class builds VPP pipeline and run the VPP pipeline
#include "mfx_vpp_hw.h"
#endif

template<>
VideoVPP* _mfxSession::Create<VideoVPP>(mfxVideoParam& /*par*/)
{
    VideoVPP *pVPP = nullptr;

#ifdef MFX_ENABLE_VPP
    VideoCORE* core = m_pCORE.get();
    mfxStatus mfxRes = MFX_ERR_MEMORY_ALLOC;

    pVPP = new VideoVPPMain(core, &mfxRes);
    if (MFX_ERR_NONE != mfxRes)
    {
        delete pVPP;
        pVPP = nullptr;
    }
#endif // MFX_ENABLE_VPP

    return pVPP;

}

mfxStatus MFXVideoVPP_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_PARAMS, "In:  ", in);

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(out, MFX_ERR_NULL_PTR);

    mfxStatus mfxRes = MFX_ERR_UNSUPPORTED;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVideoVPP_Query");
    TRACE_EVENT(MFX_TRACE_API_VPP_QUERY_TASK, EVENT_TYPE_INFO, 0, make_event_data(session, in ? in->mfx.FrameInfo.Width : 0, in ? in->mfx.FrameInfo.Height : 0, in ? in->mfx.CodecId : 0, out->mfx.FrameInfo.Width, out->mfx.FrameInfo.Height, out->mfx.CodecId));

    if ((0 != in) && (MFX_HW_VAAPI == session->m_pCORE->GetVAType()))
    {
        // protected content not supported on Linux
        if(0 != in->Protected)
        {
            out->Protected = 0;
            return MFX_ERR_UNSUPPORTED;
        }
    }

    try
    {
#ifdef MFX_ENABLE_VPP
        mfxRes = VideoVPPMain::Query(session->m_pCORE.get(), in, out);
#endif // MFX_ENABLE_VPP
    }
    // handle error(s)
    catch(...)
    {
        mfxRes = MFX_ERR_NULL_PTR;
    }

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_PARAMS, "Out:  ", out);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_PARAMS, mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoVPP_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_PARAMS, "In:  ", par);

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);
    MFX_CHECK(request, MFX_ERR_NULL_PTR);

    mfxStatus mfxRes = MFX_ERR_UNSUPPORTED;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVideoVPP_QueryIOSurf");
    TRACE_EVENT(MFX_TRACE_API_VPP_QUERY_IOSURF_TASK, EVENT_TYPE_INFO, 0, make_event_data(session, par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height, par->mfx.CodecId));

    try
    {
#ifdef MFX_ENABLE_VPP
        mfxRes = VideoVPPMain::QueryIOSurf(session->m_pCORE.get(), par, request/*, session->m_adapterNum*/);
#endif // MFX_ENABLE_VPP
    }
    // handle error(s)
    catch(...)
    {
        mfxRes = MFX_ERR_NULL_PTR;
    }

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_PARAMS, "Out:  ", request);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_PARAMS, mfxRes);
    return mfxRes;
}

static mfxStatus SetupCache(mfxSession session, const mfxVideoParam& par)
{
    // No internal alloc if Ext Allocator set
    if (session->m_pCORE->IsExternalFrameAllocator())
        return MFX_ERR_NONE;

    auto CacheInitRoutine = [session, &par](mfxVPPPoolType pool_type)
    {
        bool input_pool = pool_type == MFX_VPP_POOL_IN;

        auto& pCache = input_pool ? session->m_pVPP->m_pSurfaceCacheIn : session->m_pVPP->m_pSurfaceCacheOut;

        if (!pCache)
        {
            auto base_core_vpl = dynamic_cast<CommonCORE_VPL*>(session->m_pCORE.get());
            MFX_CHECK_HDL(base_core_vpl);

            mfxU16 memory_type = input_pool ? mfxU16(par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY  ? MFX_MEMTYPE_FROM_VPPIN  | MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_FROM_VPPIN  | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)
                                            : mfxU16(par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY ? MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET);

            const mfxFrameInfo& frame_info = input_pool ? par.vpp.In : par.vpp.Out;

            std::unique_ptr<SurfaceCache> scoped_cache_ptr(SurfaceCache::Create(*base_core_vpl, memory_type, frame_info));
            using cache_controller = surface_cache_controller<SurfaceCache>;
            using TCachePtr = std::remove_reference<decltype(pCache)>::type;

            pCache = TCachePtr(new cache_controller(scoped_cache_ptr.release(), ComponentType::VPP, pool_type), std::default_delete<cache_controller>());
        }

        // Setup cache limits
        MFX_SAFE_CALL(pCache->SetupCache(session, par));

        return MFX_ERR_NONE;
    };

    MFX_SAFE_CALL(CacheInitRoutine(MFX_VPP_POOL_IN));
    MFX_SAFE_CALL(CacheInitRoutine(MFX_VPP_POOL_OUT));

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoVPP_Init(mfxSession session, mfxVideoParam *par)
{
    mfxStatus mfxRes = MFX_ERR_UNSUPPORTED;

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_PARAMS, "In:  ", par);

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVideoVPP_Init");
    TRACE_EVENT(MFX_TRACE_API_VPP_INIT_TASK, EVENT_TYPE_INFO, 0, make_event_data(session, par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height, par->mfx.CodecId));

    try
    {
#ifdef MFX_ENABLE_VPP
        // close the existing video processor,
        // if it is initialized.
        if (session->m_pVPP.get())
        {
            MFXVideoVPP_Close(session);
        }

        // create a new instance
        session->m_pVPP.reset(session->Create<VideoVPP>(*par));
        MFX_CHECK(session->m_pVPP.get(), MFX_ERR_INVALID_VIDEO_PARAM);
        mfxRes = session->m_pVPP->Init(par);

        if (mfxRes >= MFX_ERR_NONE && SupportsVPLFeatureSet(*session->m_pCORE.get()))
        {
            MFX_SAFE_CALL(SetupCache(session, *par));
        }
#endif // MFX_ENABLE_VPP
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    MFX_LTRACE_I(MFX_TRACE_LEVEL_PARAMS, mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoVPP_Close(mfxSession session)
{
    mfxStatus mfxRes;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVideoVPP_Close");

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);
    TRACE_EVENT(MFX_TRACE_API_VPP_CLOSE_TASK, EVENT_TYPE_INFO, 0, make_event_data(session));

    try
    {
        MFX_CHECK(session->m_pVPP, MFX_ERR_NOT_INITIALIZED);

        // wait until all tasks are processed
        session->m_pScheduler->WaitForAllTasksCompletion(session->m_pVPP.get());

        mfxRes = session->m_pVPP->Close();

        session->m_pVPP.reset(nullptr);
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    MFX_LTRACE_I(MFX_TRACE_LEVEL_PARAMS, mfxRes);
    return mfxRes;
}

static
mfxStatus MFXVideoVPPLegacyRoutine(void *pState, void *pParam,
                                   mfxU32 threadNumber, mfxU32 callNumber)
{
    (void)callNumber;

    mfxStatus mfxRes;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVideoVPPLegacyRoutine");
    TRACE_EVENT(MFX_TRACE_API_VPP_LEGACY_ROUTINE_TASK, EVENT_TYPE_INFO, 0, make_event_data(threadNumber, callNumber));

    VideoVPP *pVPP = (VideoVPP *) pState;
    MFX_THREAD_TASK_PARAMETERS *pTaskParam = (MFX_THREAD_TASK_PARAMETERS *) pParam;

    // check error(s)
    if ((NULL == pState) ||
        (NULL == pParam) ||
        (0 != threadNumber))
    {
        return MFX_ERR_NULL_PTR;
    }

    // call the obsolete method
    mfxRes = pVPP->RunFrameVPP(pTaskParam->vpp.in,
                               pTaskParam->vpp.out,
                               pTaskParam->vpp.aux);

    return mfxRes;

} // mfxStatus MFXVideoVPPLegacyRoutine(void *pState, void *pParam,

enum
{
    MFX_NUM_ENTRY_POINTS = 2
};

mfxStatus MFXVideoVPP_RunFrameVPPAsync(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
{
    mfxStatus mfxRes;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVideoVPP_RunFrameVPPAsync");
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_PARAMS, "In:  ", aux);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_PARAMS, "In:  ", in);
    TRACE_EVENT(MFX_TRACE_API_VPP_RUN_FRAME_VPP_ASYNC_TASK, EVENT_TYPE_INFO, 0, make_event_data(session, in, out));

    MFX_CHECK_HDL(session);
    MFX_CHECK(session->m_pVPP, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(syncp,           MFX_ERR_NULL_PTR);

    try
    {
        if (in && session->m_pVPP->m_pSurfaceCacheIn)
        {
            MFX_SAFE_CALL(session->m_pVPP->m_pSurfaceCacheIn->Update(*in));
        }
        if (out && session->m_pVPP->m_pSurfaceCacheOut)
        {
            MFX_SAFE_CALL(session->m_pVPP->m_pSurfaceCacheOut->Update(*out));
        }

        mfxSyncPoint syncPoint = NULL;
        MFX_ENTRY_POINT entryPoints[MFX_NUM_ENTRY_POINTS];
        mfxU32 numEntryPoints = MFX_NUM_ENTRY_POINTS;

        memset(&entryPoints, 0, sizeof(entryPoints));
        mfxRes = session->m_pVPP->VppFrameCheck(in, out, aux, entryPoints, numEntryPoints);
        // source data is OK, go forward
        if ((MFX_ERR_NONE                     == mfxRes) ||
            (MFX_ERR_MORE_DATA_SUBMIT_TASK    == mfxRes) ||
            (MFX_ERR_MORE_SURFACE             == mfxRes) ||
            (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfxRes))
        {
            // prepare the obsolete kind of task.
            // it is obsolete and must be removed.
            if (NULL == entryPoints[0].pRoutine)
            {
                MFX_TASK task;

                memset(&task, 0, sizeof(task));
                // BEGIN OF OBSOLETE PART
                task.bObsoleteTask = true;
                // fill task info
                task.pOwner = session->m_pVPP.get();
                task.entryPoint.pRoutine = &MFXVideoVPPLegacyRoutine;
                task.entryPoint.pState = session->m_pVPP.get();
                task.entryPoint.requiredNumThreads = 1;

                // fill legacy parameters
                task.obsolete_params.vpp.in = in;
                task.obsolete_params.vpp.out = out;
                task.obsolete_params.vpp.aux = aux;
                // END OF OBSOLETE PART

                task.priority = session->m_priority;
                task.threadingPolicy = session->m_pVPP->GetThreadingPolicy();
                // fill dependencies
                task.pSrc[0] = in;
                task.pDst[0] = out;
                
                if (MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes)
                    task.pDst[0] = NULL;

#ifdef MFX_TRACE_ENABLE
                task.nParentId = MFX_AUTO_TRACE_GETID();
                task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_VPP;
#endif // MFX_TRACE_ENABLE

                // register input and call the task
                MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));
            }
            else if (1 == numEntryPoints)
            {
                MFX_TASK task;

                memset(&task, 0, sizeof(task));
                task.pOwner = session->m_pVPP.get();
                task.entryPoint = entryPoints[0];
                task.priority = session->m_priority;
                task.threadingPolicy = session->m_pVPP->GetThreadingPolicy();
                // fill dependencies
                task.pSrc[0] = in;
                task.pDst[0] = out;
                if (MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes)
                    task.pDst[0] = NULL;
                

#ifdef MFX_TRACE_ENABLE
                task.nParentId = MFX_AUTO_TRACE_GETID();
                task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_VPP;
#endif
                // register input and call the task
                MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));
            }
            else
            {
                MFX_TASK task;

                memset(&task, 0, sizeof(task));
                task.pOwner = session->m_pVPP.get();
                task.entryPoint = entryPoints[0];
                task.priority = session->m_priority;
                task.threadingPolicy = session->m_pVPP->GetThreadingPolicy();
                // fill dependencies
                task.pSrc[0] = in;
                task.pDst[0] = entryPoints[0].pParam;
               

#ifdef MFX_TRACE_ENABLE
                task.nParentId = MFX_AUTO_TRACE_GETID();
                task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_VPP;
#endif
                // register input and call the task
                MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));

                memset(&task, 0, sizeof(task));
                task.pOwner = session->m_pVPP.get();
                task.entryPoint = entryPoints[1];
                task.priority = session->m_priority;
                task.threadingPolicy = session->m_pVPP->GetThreadingPolicy();
                
                // fill dependencies
                task.pSrc[0] = entryPoints[0].pParam;
                task.pDst[0] = out;
                task.pDst[1] = aux;
                if (MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes)
                {
                    task.pDst[0] = NULL;
                    task.pDst[1] = NULL;
                }

#ifdef MFX_TRACE_ENABLE
                task.nParentId = MFX_AUTO_TRACE_GETID();
                task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_VPP2;
#endif
                // register input and call the task
                MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));
            }

            if (MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes)
            {
                mfxRes = MFX_ERR_MORE_DATA;
                syncPoint = NULL;
            }

            if (syncPoint && out && out->FrameInterface && out->FrameInterface->Synchronize && !session->m_pCORE->IsExternalFrameAllocator())
            {
                MFX_CHECK_HDL(out->FrameInterface->Context);
                static_cast<mfxFrameSurfaceBaseInterface*>(out->FrameInterface->Context)->SetSyncPoint(syncPoint);
            }
        }

        // return pointer to synchronization point
        *syncp = syncPoint;
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_PARAMS, "Out:  ", out);
    if (mfxRes == MFX_ERR_NONE && syncp)
    {
        MFX_LTRACE_P(MFX_TRACE_LEVEL_PARAMS, *syncp);
    }
    MFX_LTRACE_I(MFX_TRACE_LEVEL_PARAMS, mfxRes);
    return mfxRes;

} // mfxStatus MFXVideoVPP_RunFrameVPPAsync(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)

mfxStatus MFXVideoVPP_RunFrameVPPAsyncEx(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
{
    (void)in;
    (void)surface_work;
    (void)surface_out;

    mfxStatus mfxRes;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVideoVPP_RunFrameVPPAsyncEx")
    TRACE_EVENT(MFX_TRACE_API_VPP_RUN_FRAME_VPP_ASYNC_EX_TASK, EVENT_TYPE_INFO, 0, make_event_data(session, in, surface_work));

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_PARAMS, "In:  ", in)

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pVPP.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(syncp, MFX_ERR_NULL_PTR);

    try
    {
        //MediaSDK's VPP should not work through Ex function
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_PARAMS, "Out:  ", surface_work);
    if (mfxRes == MFX_ERR_NONE && syncp)
    {
        MFX_LTRACE_P(MFX_TRACE_LEVEL_PARAMS, *syncp);
    }
    MFX_LTRACE_I(MFX_TRACE_LEVEL_PARAMS, mfxRes);
    return mfxRes;

} // mfxStatus MFXVideoVPP_RunFrameVPPAsyncEx(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxThreadTask *task);

#define FUNCTION_GET_SURFACE_IMPL_VPP(FUNCTION_NAME, TYPE)                         \
mfxStatus FUNCTION_NAME##TYPE (mfxSession session, mfxFrameSurface1** output_surf) \
{                                                                                  \
    MFX_CHECK_HDL(session);                                                        \
    MFX_CHECK(session->m_pCORE,                       MFX_ERR_NOT_INITIALIZED);    \
    MFX_CHECK(session->m_pVPP,                        MFX_ERR_NOT_INITIALIZED);    \
                                                                                   \
    return session->m_pVPP->GetSurfaceFrom##TYPE (output_surf, nullptr);           \
}

FUNCTION_GET_SURFACE_IMPL_VPP(MFXMemory_GetSurfaceForVPP, In)
FUNCTION_GET_SURFACE_IMPL_VPP(MFXMemory_GetSurfaceForVPP, Out)

#undef FUNCTION_GET_SURFACE_IMPL_VPP

mfxStatus MFXVideoVPP_ProcessFrameAsync(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 **out)
{
    MFX_CHECK_NULL_PTR1(out);

    MFX_CHECK_HDL(session);
    MFX_CHECK(session->m_pVPP,                     MFX_ERR_NOT_INITIALIZED);

    mfxFrameSurface1* surf;
    MFX_SAFE_CALL(session->m_pVPP->GetSurfaceFromOut(&surf, nullptr));
    surface_refcount_scoped_lock surf_out(surf);
    MFX_CHECK(surf_out, MFX_ERR_MEMORY_ALLOC);

    mfxSyncPoint syncPoint;
    mfxStatus mfxRes = MFXVideoVPP_RunFrameVPPAsync(session, in, surf_out.get(), nullptr, &syncPoint);

    // If output is not available then release out_surf (which should mark it as free) and return status
    MFX_CHECK(syncPoint, mfxRes);

    *out = surf_out.release();
    return mfxRes;
}
//
// THE OTHER VPP FUNCTIONS HAVE IMPLICIT IMPLEMENTATION
//

FUNCTION_RESET_IMPL(VPP, Reset, (mfxSession session, mfxVideoParam *par), (par))

FUNCTION_IMPL(VPP, GetVideoParam, (mfxSession session, mfxVideoParam *par), (par))
FUNCTION_IMPL(VPP, GetVPPStat, (mfxSession session, mfxVPPStat *stat), (stat))

mfxStatus QueryImplsDescription(VideoCORE& core, mfxVPPDescription& caps, mfx::PODArraysHolder& arrayHolder, const std::vector<mfxU32>& filterIds)
{
    return MfxHwVideoProcessing::VideoVPPHW::QueryImplsDescription(&core, caps, arrayHolder, filterIds);
}
