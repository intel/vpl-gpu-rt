// Copyright (c) 2009-2026 Intel Corporation
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

// This file must be included at the bottom of mfx_scheduler_core.h, after the
// mfxSchedulerCore class definition is complete.

#if !defined(__MFX_SCHEDULER_CORE_INLINE_H)
#define __MFX_SCHEDULER_CORE_INLINE_H

inline
void mfxSchedulerCore::IncrementHWEventCounter(void)
{
    m_hwEventCounter += 1;

} // void mfxSchedulerCore::IncrementHWEventCounter(void)

inline
mfxU64 mfxSchedulerCore::GetHWEventCounter(void) const
{
    return m_hwEventCounter;

} // mfxU64 mfxSchedulerCore::GetHWEventCounter(void) const

inline
void mfxSchedulerCore::call_pRoutine(MFX_CALL_INFO& call)
{
    const char *pRoutineName = (call.pTask->entryPoint.pRoutineName)?
        call.pTask->entryPoint.pRoutineName:
        "MFX Async Task";
    mfxU64 start;

    (void)pRoutineName;
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, pRoutineName);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_SCHED, "^Child^of", "%d", call.pTask->nParentId);

    // mark beginning of working period
    start = GetHighPerformanceCounter();
    try {
        if (call.pTask->bObsoleteTask) {
            // NOTE: it is legacy task call, it should be eliminated soon
            call.res = call.pTask->entryPoint.pRoutine(
                call.pTask->entryPoint.pState,
                (void *) &call.pTask->obsolete_params,
                call.threadNum,
                call.callNum);
        } else {
            // NOTE: this is the only legal task calling process.
            // Should survive only this one.
            call.res = call.pTask->entryPoint.pRoutine(
                call.pTask->entryPoint.pState,
                call.pTask->entryPoint.pParam,
                call.threadNum,
                call.callNum);
        }
    } catch(...) {
        call.res = MFX_ERR_UNKNOWN;
        MFX_LOG_ERROR("Unknown Exception!\n");
    }

    call.timeSpend = (GetHighPerformanceCounter() - start);

    TRACE_EVENT(MFX_TRACE_API_QUERY_TASK, EVENT_TYPE_INFO, TR_KEY_INTERNAl, make_event_data(call.pTask->nTaskId, call.threadNum, call.res));
    MFX_NORMAL_MSG("Query Task: TaskId = %d, threadNum = %d, res = %d", call.pTask->nTaskId, call.threadNum, call.res);
}

#endif // !defined(__MFX_SCHEDULER_CORE_INLINE_H)
