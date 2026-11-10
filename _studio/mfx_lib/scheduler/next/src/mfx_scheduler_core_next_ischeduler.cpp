// Copyright (c) 2010-2026 Intel Corporation
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

#include <mfx_scheduler_core_next.h>

#include <mfx_scheduler_core_next_task.h>
#include <mfx_scheduler_core_handle.h>

#include <mfx_trace.h>

#include "mfx_utils.h"

#include <algorithm>
#include <functional>
#include <cassert>
#include <list>



#if defined(MFX_SCHEDULER_LOG)
#include <stdio.h>
#endif // defined(MFX_SCHEDULER_LOG)

namespace mfx_sched_next
{

enum
{
    MFX_TIME_INFINITE           = 0x7fffffff,
    MFX_TIME_TO_WAIT            = 5
};

mfxStatus mfxSchedulerCoreNext::Initialize(const MFX_SCHEDULER_PARAM *pParam)
{
    MFX_SCHEDULER_PARAM2 param2;
    memset(&param2, 0, sizeof(param2));
    if (pParam) {
        MFX_SCHEDULER_PARAM* casted_param2 = &param2;
        *casted_param2 = *pParam;
    }
    if (!param2.numberOfThreads) {
        // that's just in case: core, which calls scheduler, is doing
        // exactly the same what we are doing below
        param2.numberOfThreads = std::thread::hardware_concurrency();
    }
    return Initialize2(&param2);
}

mfxStatus mfxSchedulerCoreNext::Initialize2(const MFX_SCHEDULER_PARAM2 *pParam)
{
    mfxU32 i;

    // release the object before initialization
    Close();


    // save the parameters
    if (pParam)
    {
        m_param = *pParam;
    }

#if defined(MFX_SCHEDULER_LOG)
    {
        char cPath[256];
        // initialize the log
        m_hLog = mfxLogInit(cPath);
    }
#endif // defined(MFX_SCHEDULER_LOG)

    // clean up the task look up table
    m_ppTaskLookUpTable.resize(MFX_MAX_NUMBER_TASK, nullptr);

    // allocate the dependency table
    m_pDependencyTable.resize(MFX_MAX_NUMBER_TASK * 2, MFX_DEPENDENCY_ITEM());

    // allocate the thread assignment object table.
    // its size should be equal to the number of task,
    // larger table is not required.
    m_occupancyTable.resize(MFX_MAX_NUMBER_TASK, MFX_THREAD_ASSIGNMENT());

    if (MFX_SINGLE_THREAD != m_param.flags)
    {
        if (m_param.numberOfThreads && m_param.params.NumThread) {
            // use user-overwritten number of threads
            m_param.numberOfThreads = m_param.params.NumThread;
        }
        if (!m_param.numberOfThreads) {
            return MFX_ERR_UNSUPPORTED;
        }
        if (m_param.numberOfThreads == 1) {
            // we need at least 2 threads to avoid dead locks
            return MFX_ERR_UNSUPPORTED;
        }


        try
        {
            // allocate thread contexts
            m_pThreadCtx = new MFX_SCHEDULER_THREAD_CONTEXT[m_param.numberOfThreads];

            // start threads
            for (i = 0; i < m_param.numberOfThreads; i += 1)
            {
                // prepare context
                m_pThreadCtx[i].threadNum = i;
                m_pThreadCtx[i].pSchedulerCore = this;

                // spawn a thread
                m_pThreadCtx[i].threadHandle = std::thread(
                    std::bind(&mfxSchedulerCoreNext::ThreadProc, this, &m_pThreadCtx[i]));

                if (!SetScheduling(m_pThreadCtx[i].threadHandle)) {
                    return MFX_ERR_UNSUPPORTED;
                }
            }
        }
        catch (...)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }


        SetThreadsAffinityToSockets();
    }
    else
    {
        // to run HW listen thread. Will be enabled if tests are OK

    }

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCoreNext::Initialize(mfxSchedulerFlags flags, mfxU32 numberOfThreads)

mfxStatus mfxSchedulerCoreNext::AddTask(const MFX_TASK &task, mfxSyncPoint *pSyncPoint)
{
    return AddTask(task, pSyncPoint, NULL, 0);

} // mfxStatus mfxSchedulerCoreNext::AddTask(const MFX_TASK &task, mfxSyncPoint *pSyncPoint)

mfxStatus mfxSchedulerCoreNext::Synchronize(mfxSyncPoint syncPoint, mfxU32 timeToWait)
{
    mfxTaskHandle handle;
    mfxStatus mfxRes;

    // check error(s)
    if (NULL == syncPoint)
    {
        return MFX_ERR_NULL_PTR;
    }

    // cast the pointer to handle and make syncing
    handle.handle = (size_t) syncPoint;

#if defined(MFX_SCHEDULER_LOG)
    auto start = std::chrono::steady_clock::now();;
#endif // defined(MFX_SCHEDULER_LOG)

    // waiting on task's handle
    mfxRes = Synchronize(handle, timeToWait);

    MFX_NORMAL_MSG("Sync Task: TaskId = %d, jobID = %d, syncPoint = %p, timeToWait = %d mfxRes = %d", handle.taskID, handle.jobID, syncPoint, timeToWait, mfxRes);

#if defined(MFX_SCHEDULER_LOG)

    auto stop = std::chrono::steady_clock::now();;


#endif // defined(MFX_SCHEDULER_LOG)

    return mfxRes;

} // mfxStatus mfxSchedulerCoreNext::Synchronize(mfxSyncPoint syncPoint, mfxU32 timeToWait)

mfxStatus mfxSchedulerCoreNext::Synchronize(mfxTaskHandle handle, mfxU32 timeToWait)
{
    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    // look up the task
    MFX_SCHEDULER_TASK *pTask = m_ppTaskLookUpTable.at(handle.taskID);

    if (nullptr == pTask)
    {
        return MFX_ERR_NULL_PTR;
    }
    PERF_UTILITY_SET_ASYNC_TASK_ID(pTask->param.task.nTaskId);

    if (MFX_SINGLE_THREAD == m_param.flags)
    {
        //let really run task to
        MFX_CALL_INFO call = {};
        mfxTaskHandle previousTaskHandle = {};

        mfxStatus task_sts = MFX_ERR_NONE;

        mfx::TimerMs<mfxU64> timer((mfxU64)timeToWait * 1000);

        while (MFX_WRN_IN_EXECUTION == pTask->opRes)
        {
            std::unique_lock<std::mutex> guard(m_guard);
            task_sts = GetTask(call, previousTaskHandle, 0);

            if (task_sts != MFX_ERR_NONE)
            {
                // No runnable task for this caller yet: the task it's waiting on is
                // being executed by another caller thread (single-thread mode runs
                // tasks on caller threads, tracked per-tid in m_timeToRunMap above).
                // Release m_guard and yield so that sibling can be scheduled and
                // complete the task, instead of hot-spinning on m_guard.
                guard.unlock();
                std::this_thread::yield();
                continue;
            }

            guard.unlock();

            call.res = call.pTask->entryPoint.pRoutine(call.pTask->entryPoint.pState,
                                                       call.pTask->entryPoint.pParam,
                                                       call.threadNum,
                                                       call.callNum);

            guard.lock();

            // save the previous task's handle
            previousTaskHandle = call.taskHandle;

            MarkTaskCompleted(&call, 0);

            if (timer.Expired())
                break;

            if (MFX_TASK_DONE!= call.res)
            {
                IncrementHWEventCounter();
            }
        }
        //
        // inspect the task
        //


        // the handle is outdated,
        // the previous task job is over and completed with successful status
        // NOTE: it make sense to read task result and job ID the following order.
        if ((MFX_ERR_NONE == pTask->opRes) ||
            (pTask->jobID != handle.jobID))
        {
            return MFX_ERR_NONE;
        }

        // wait the result on the event
        if (MFX_WRN_IN_EXECUTION == pTask->opRes)
        {
            return MFX_WRN_IN_EXECUTION;
        }

        // check error status
        if ((MFX_ERR_NONE != pTask->opRes) &&
            (pTask->jobID == handle.jobID))
        {
            return pTask->opRes;
        }

        // in all other cases task is complete
        return MFX_ERR_NONE;
    }
    else
    {
        std::unique_lock<std::mutex> guard(m_guard);

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_PRIVATE, "Scheduler::Wait");
        MFX_LTRACE_1(MFX_TRACE_LEVEL_SCHED, "^Depends^on", "%d", pTask->param.task.nParentId);
        MFX_LTRACE_I(MFX_TRACE_LEVEL_SCHED, timeToWait);

        pTask->done.wait_for(guard, std::chrono::milliseconds(timeToWait), [pTask, handle] {
            return (pTask->jobID != handle.jobID) || (MFX_WRN_IN_EXECUTION != pTask->opRes);
        });

        TRACE_EVENT(MFX_TRACE_API_SYNC_OPERATION_TASK, EVENT_TYPE_INFO, TR_KEY_INTERNAl, make_event_data(timeToWait, pTask->param.task.nTaskId, pTask->jobID, (int)handle.jobID, pTask->opRes));


        if (pTask->jobID == handle.jobID) {
            return pTask->opRes;
        } else {
            /* Notes:
             *  - task executes next job already, we _lost_ task status and can only assume that
             *  everything was OK or FAILED, we will assume that task succeeded
             */
            return MFX_ERR_NONE;
        }
    }
}

mfxStatus mfxSchedulerCoreNext::GetTimeout(mfxU32& maxTimeToRun)
{
    (void)maxTimeToRun;

    return MFX_ERR_UNSUPPORTED;
}


mfxStatus mfxSchedulerCoreNext::WaitForDependencyResolved(const void *pDependency)
{
    mfxTaskHandle waitHandle = {};
    bool bFind = false;

    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
    if (NULL == pDependency)
    {
        return MFX_ERR_NONE;
    }

    // find a handle to wait
    {
        std::lock_guard<std::mutex> guard(m_guard);
        mfxU32 curIdx;

        for (curIdx = 0; curIdx < m_numDependencies; curIdx += 1)
        {
            if (m_pDependencyTable.at(curIdx).p == pDependency)
            {
                // get the handle before leaving protected section
                waitHandle.taskID = m_pDependencyTable[curIdx].pTask->taskID;
                waitHandle.jobID = m_pDependencyTable[curIdx].pTask->jobID;

                // handle is found, go to wait
                bFind = true;
                break;
            }
        }
        // leave the protected section
    }

    // the dependency is still in the table,
    // wait until it leaves the table.
    if (bFind)
    {
        return Synchronize(waitHandle, MFX_TIME_INFINITE);
    }

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCoreNext::WaitForDependencyResolved(const void *pDependency)

mfxStatus mfxSchedulerCoreNext::WaitForAllTasksCompletion(const void *pOwner)
{
    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
    if (NULL == pOwner)
    {
        return MFX_ERR_NULL_PTR;
    }

    // make sure that threads are running
    {
        std::lock_guard<std::mutex> guard(m_guard);

        ResetWaitingTasks(pOwner);
        WakeUpThreads();
    }

    std::list<mfxTaskHandle> tasks;

    {
        std::lock_guard<std::mutex> guard(m_guard);

        ForEachTask(
            [&pOwner, &tasks](MFX_SCHEDULER_TASK* task)
            {
                //make a list of all 'active' tasks of given owner
                if ((task->param.task.pOwner == pOwner) &&
                    ((MFX_WRN_IN_EXECUTION == task->opRes)
                    ))
                {
                    mfxTaskHandle waitHandle;
                    waitHandle.taskID = task->taskID;
                    waitHandle.jobID = task->jobID;

                    tasks.emplace_back(waitHandle);
                }
            }
        );
    }

    auto handle = tasks.begin();
    while (!tasks.empty())
    {
        if (tasks.end() == handle)
            handle = tasks.begin();

        // wait for a while
        // for a while and infinite are different things
        mfxStatus syncRes = Synchronize(*handle, MFX_TIME_TO_WAIT);
        if (MFX_WRN_IN_EXECUTION == syncRes // Still working
           )
        {
            handle++;
        }
        else // Done or failed
        {
            handle = tasks.erase(handle);
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCoreNext::WaitForAllTasksCompletion(const void *pOwner)

mfxStatus mfxSchedulerCoreNext::ResetWaitingStatus(const void *pOwner)
{
    std::lock_guard<std::mutex> guard(m_guard);

    // reset 'waiting' tasks belong to the given state
    ResetWaitingTasks(pOwner);

    // wake up sleeping threads
    WakeUpThreads();

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCoreNext::ResetWaitingStatus(const void *pOwner)

mfxStatus mfxSchedulerCoreNext::GetState(void)
{
    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCoreNext::GetState(void)

mfxStatus mfxSchedulerCoreNext::GetParam(MFX_SCHEDULER_PARAM *pParam)
{
    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
    if (NULL == pParam)
    {
        return MFX_ERR_NULL_PTR;
    }

    // copy the parameters
    *pParam = m_param;

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCoreNext::GetParam(MFX_SCHEDULER_PARAM *pParam)

mfxStatus mfxSchedulerCoreNext::Reset(void)
{
    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if (NULL == m_pFailedTasks)
    {
        return MFX_ERR_NONE;
    }

    // enter guarded section
    {
        std::lock_guard<std::mutex> guard(m_guard);

        // clean up the working queue
        ScrubCompletedTasks(true);
    }

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCoreNext::Reset(void)

mfxStatus mfxSchedulerCoreNext::AdjustPerformance(const mfxSchedulerMessage message)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    switch(message)
    {
        // reset the scheduler to the performance default state
    case MFX_SCHEDULER_RESET_TO_DEFAULTS:
        break;

    case MFX_SCHEDULER_START_HW_LISTENING:
        if (m_param.flags != MFX_SINGLE_THREAD)
        {
            mfxRes = StartWakeUpThread();
        }
        break;

    case MFX_SCHEDULER_STOP_HW_LISTENING:
        if (m_param.flags != MFX_SINGLE_THREAD)
        {
            mfxRes = StopWakeUpThread();
        }
        break;

        // unknown message
    default:
        mfxRes = MFX_ERR_UNKNOWN;
        break;
    }

    return mfxRes;

} // mfxStatus mfxSchedulerCoreNext::AdjustPerformance(const mfxSchedulerMessage message)

#if defined(MFX_SCHEDULER_LOG)

const
char *taskType[] =
{
    "UNKNOWN (0)",
    "MFX_TASK_THREADING_INTRA",
    "MFX_TASK_THREADING_INTER",
    "UNKNOWN (3)",
    "UNKNOWN (4)",
    "MFX_TASK_THREADING_DEDICATED",
    "UNKNOWN (6)",
    "UNKNOWN (7)",
    "MFX_TASK_THREADING_SHARED",
    "UNKNOWN (9)",
    "UNKNOWN (10)",
    "UNKNOWN (11)",
    "UNKNOWN (12)",
    "UNKNOWN (13)",
    "UNKNOWN (14)",
    "UNKNOWN (15)",
    "UNKNOWN (16)",
    "UNKNOWN (17)",
    "UNKNOWN (18)",
    "UNKNOWN (19)",
    "UNKNOWN (20)",
    "UNKNOWN (21)",
    "MFX_TASK_THREADING_DEDICATED_WAIT"
};

void GetNumTasks(MFX_SCHEDULER_TASK * const (ppTasks[MFX_PRIORITY_NUMBER][MFX_TYPE_NUMBER]), TASK_STAT &stat)
{
    mfxU32 priority;

    memset(&stat, 0, sizeof(stat));

    for (priority = 0; priority < MFX_PRIORITY_NUMBER; priority += 1)
    {
        int type;

        for (type = MFX_TYPE_HARDWARE; type <= MFX_TYPE_SOFTWARE; type += 1)
        {
            const MFX_SCHEDULER_TASK *pTask = ppTasks[priority][type];

            // run over the tasks with particular priority
            while (pTask)
            {
                stat.total += 1;
                if ((MFX_WRN_IN_EXECUTION == pTask->curStatus) ||
                    (MFX_TASK_WORKING == pTask->curStatus) ||
                    (MFX_TASK_BUSY == pTask->curStatus))
                {
                    if (pTask->IsDependenciesResolved())
                    {
                        stat.ready += 1;
                    }
                    else
                    {
                        stat.waiting += 1;
                    }
                }
                else if (isFailed(pTask->curStatus))
                {
                    stat.failed += 1;
                }
                else
                {
                    stat.done += 1;
                }

                // advance the task pointer
                pTask = pTask->pNext;
            }
        }
    }

} // void GetNumTasks(MFX_SCHEDULER_TASK * const (ppTasks[MFX_PRIORITY_NUMBER][MFX_TYPE_NUMBER]), TASK_STAT &stat)

#endif // defined(MFX_SCHEDULER_LOG)

mfxStatus mfxSchedulerCoreNext::AddTask(const MFX_TASK &task, mfxSyncPoint *pSyncPoint,
                                    const char *pFileName, int lineNumber)
{
    MFX_AUTO_TRACE(__FUNCTION__);
#ifdef MFX_TRACE_ENABLE
    MFX_LTRACE_1(MFX_TRACE_LEVEL_SCHED, "^Enqueue^", "%d", task.nTaskId);
#endif


    // check error(s)
    if (0 == m_param.numberOfThreads)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
    if ((NULL == task.entryPoint.pRoutine) ||
        (NULL == pSyncPoint))
    {
        return MFX_ERR_NULL_PTR;
    }


    // enter protected section
    {
        std::unique_lock<std::mutex> guard(m_guard);
        // make sure that there is enough free task objects
        m_freeTasks.wait(guard, [this](){return m_freeTasksCount > 0;});
        --m_freeTasksCount;

        // give the free task slot back if the task is not queued. It is
        // declared after 'guard', so it runs with the guard still held.
        struct FreeSlotGuard
        {
            mfxSchedulerCoreNext *pCore;
            bool committed;
            ~FreeSlotGuard()
            {
                if (!committed)
                {
                    ++pCore->m_freeTasksCount;
                    pCore->m_freeTasks.notify_one();
                }
            }
        } slotGuard = { this, false };

        mfxStatus mfxRes;
        MFX_SCHEDULER_TASK *pTask, **ppTemp;
        mfxTaskHandle handle = {};
        MFX_THREAD_ASSIGNMENT *pAssignment = nullptr;
        mfxU32 occupancyIdx;
        int type;

        // Make sure that there is an empty task object

        mfxRes = AllocateEmptyTask();
        if (MFX_ERR_NONE != mfxRes)
        {
            // better to return error instead of WRN  (two-tasks per component scheme)
            return MFX_ERR_MEMORY_ALLOC;
        }

        // initialize the task
        m_pFreeTasks->ResetDependency();
        mfxRes = m_pFreeTasks->Reset();
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }
        m_pFreeTasks->param.task = task;
        mfxRes = GetOccupancyTableIndex(occupancyIdx, &task);
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }
        if (m_occupancyTable.size() <= occupancyIdx)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        pAssignment = &(m_occupancyTable[occupancyIdx]);

        // update the thread assignment parameters
        if (MFX_TASK_INTRA & task.threadingPolicy)
        {
            // last entries in the dependency arrays must be empty
            if ((m_pFreeTasks->param.task.pSrc[MFX_TASK_NUM_DEPENDENCIES - 1]) ||
                (m_pFreeTasks->param.task.pDst[MFX_TASK_NUM_DEPENDENCIES - 1]))
            {
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }

            // fill INTRA task dependencies
            m_pFreeTasks->param.task.pSrc[MFX_TASK_NUM_DEPENDENCIES - 1] = pAssignment->pLastTask;
            m_pFreeTasks->param.task.pDst[MFX_TASK_NUM_DEPENDENCIES - 1] = m_pFreeTasks;
            // update the last intra task pointer
            pAssignment->pLastTask = m_pFreeTasks;
        }

        // no error return below, the slot now belongs to the queued task
        slotGuard.committed = true;

        // do not save the pointer to thread assigment instance
        // until all checking have been done
        m_pFreeTasks->param.pThreadAssignment = pAssignment;
        pAssignment->m_numRefs += 1;

        // saturate the number of available threads
        uint32_t numThreads = m_pFreeTasks->param.task.entryPoint.requiredNumThreads;
        numThreads = (0 == numThreads) ? m_param.numberOfThreads : numThreads;

        numThreads = std::min<uint32_t>({m_param.numberOfThreads, numThreads, sizeof(pAssignment->threadMask) * 8});
        m_pFreeTasks->param.task.entryPoint.requiredNumThreads = numThreads;

        // set the advanced task's info
        m_pFreeTasks->param.sourceInfo.pFileName = pFileName;
        m_pFreeTasks->param.sourceInfo.lineNumber = lineNumber;
        // set the sync point for the task
        handle.handle = 0;
        handle.taskID = m_pFreeTasks->taskID;
        handle.jobID = m_pFreeTasks->jobID;
        *pSyncPoint = (mfxSyncPoint) handle.handle;

        // Register task dependencies
        RegisterTaskDependencies(m_pFreeTasks);

        TRACE_EVENT(MFX_TRACE_API_ADD_TASK, EVENT_TYPE_INFO, TR_KEY_INTERNAl, make_event_data(task.nTaskId, m_pFreeTasks->jobID, *pSyncPoint));
        MFX_NORMAL_MSG("AddTask: TaskId = %d, jobID = %d, pSyncPoint = %p", task.nTaskId, m_pFreeTasks->jobID, *pSyncPoint);
#if defined(MFX_SCHEDULER_LOG)
        TASK_STAT stat;

        GetNumTasks(m_pTasks, stat);
        stat.total += 1;
        if (m_pFreeTasks->IsDependenciesResolved())
        {
            stat.ready += 1;
        }
        else
        {
            stat.waiting += 1;
        }
        mfxLogWriteA(m_hLog,
                     "[    ] Tasks: total - %u, ready - %u, waiting - %u, done - %u, failed - %u\n",
                     stat.total,
                     stat.ready,
                     stat.waiting,
                     stat.done,
                     stat.failed);
#endif // defined(MFX_SCHEDULER_LOG)

        //
        // move task to the corresponding task
        //

        // remove the task from the 'free' queue
        pTask = m_pFreeTasks;
        m_pFreeTasks = m_pFreeTasks->pNext;
        pTask->pNext = NULL;

        // find the end of the corresponding queue
        type = (task.threadingPolicy & MFX_TASK_DEDICATED) ? (MFX_TYPE_HARDWARE) : (MFX_TYPE_SOFTWARE);
        ppTemp = m_pTasks[task.priority] + type;
        while (*ppTemp)
        {
            ppTemp = &((*ppTemp)->pNext);
        }

        // add the task to the end of the corresponding queue
        *ppTemp = pTask;

        // reset all 'waiting' tasks to prevent freezing
        // so called 'permanent' tasks.
        ResetWaitingTasks(pTask->param.task.pOwner);

        // wake up working threads for every task ready to run, not only for
        // the new one. The new task may be not ready (unresolved dependency or
        // a sibling holding the shared occupancy) while other tasks are
        // runnable, including the ones just un-parked by ResetWaitingTasks.
        // Otherwise they would wait for the worker wait timeout. Note, the
        // 'waiting' tasks past their waiting period are re-polled as well.
        // One spare thread is woken on top of that, see the function.
        WakeUpThreadsForReadyTasks();

        // leave the protected section
    }

    return MFX_ERR_NONE;

} // mfxStatus mfxSchedulerCoreNext::AddTask(const MFX_TASK &task, mfxSyncPoint *pSyncPoint,

mfxStatus mfxSchedulerCoreNext::DoWork()
{
    return MFX_ERR_UNSUPPORTED;
} // mfxStatus mfxSchedulerCoreNext::DoWork()



} // namespace mfx_sched_next
