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
#include <mfx_session.h>
#include <mfx_trace.h>
#include <mfx_utils.h>
#include "libmfx_core_interface.h"



#include "mfx_unified_decode_logging.h"

mfxStatus MFXVideoCORE_SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait)
{
    PERF_UTILITY_AUTO(__FUNCTION__, PERF_LEVEL_API);
    mfxStatus mfxRes = MFX_ERR_NONE;
    TRACE_EVENT(MFX_TRACE_API_SYNC_OPERATION_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(FrameIndex, session, syncp, wait));

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  session = ", MFX_TRACE_FORMAT_P, session);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  syncp = ", MFX_TRACE_FORMAT_P, syncp);
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(syncp, MFX_ERR_NULL_PTR);

    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  wait = ", MFX_TRACE_FORMAT_I, wait);

    try {
        {
            // call the function
            mfxRes = session->m_pScheduler->Synchronize(syncp, wait);
        }

        TRACE_EVENT(MFX_TRACE_API_SYNC_OPERATION_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(FrameIndex, mfxRes, syncp));
    } catch(...) {
        // set the default error value
        mfxRes = MFX_ERR_ABORTED;
    }

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);

    return mfxRes;
}
