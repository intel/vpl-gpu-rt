// Copyright (c) 2019-2022 Intel Corporation
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

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base_task.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;

typedef TaskItWrap<DpbFrame, Task::Common::Key> TItWrap;

mfxU32 TaskManager::GetNumTask() const
{
    return m_pPar->AsyncDepth + m_pReorder->BufferSize + (m_pPar->AsyncDepth > 1) + m_ResourceExtra;
}

mfxU16 TaskManager::GetBufferSize() const
{
    return !m_pPar->mfx.EncodedOrder * (m_pReorder->BufferSize + (m_pPar->AsyncDepth > 1) + m_ResourceExtra);
}

mfxU16 TaskManager::GetMaxParallelSubmits() const
{
    const auto& vp = Glob::VideoParam::Get(*m_pGlob);
    if (vp.AsyncDepth == 1)
    {
        return mfxU16(1);
    }

    return std::min<mfxU16>(2, Glob::AllocBS::Get(*m_pGlob).GetResponse().NumFrameActual);
}

void TaskManager::SetActiveTask(StorageW& task)
{
    m_pFrameCheckLocal->Insert(Tmp::CurrTask::Key, new StorableRef<StorageW>(task));
}

bool TaskManager::IsInputTask(const StorageR& task) const
{
    return !!Task::Common::Get(task).pSurfIn;
}

mfxU32 TaskManager::GetStage(const StorageR& task) const
{
    return Task::Common::Get(task).stage;
}

void TaskManager::SetStage(StorageW& task, mfxU32 stage) const
{
    Task::Common::Get(task).stage = stage;
}

bool TaskManager::IsReorderBypass() const
{
    return !!m_pPar->mfx.EncodedOrder;
}

template<class T>
static T GetFirstFrameToDisplay(
    T begin
    , T end
    , T cur)
{
    // In the future this logic might be implemented in post-reordering stage
    // In this case removal of current frame from reorder list will not be required
    const size_t framesInBuffer = std::distance(begin, end);
    if (framesInBuffer < 2)
        return end;

    std::list<T> exceptCur;
    T top = begin;

    std::generate_n(
        std::back_inserter(exceptCur)
        , framesInBuffer
        , [&]() { return top++; });

    exceptCur.remove(cur);

    const auto firstToDisplay = std::min_element(
        exceptCur.begin(),
        exceptCur.end(),
        [](T& a, T& b) { return a->DisplayOrderInGOP < b->DisplayOrderInGOP; });

    return *firstToDisplay;
}

TTaskIt TaskManager::GetNextTaskToEncode(TTaskIt begin, TTaskIt end, bool bFlush)
{
    auto IsIdrTask = [](const StorageR& rTask) { return IsIdr(Task::Common::Get(rTask).FrameType); };
    auto stopIt = std::find_if(begin, end, IsIdrTask);
    bFlush |= (stopIt != end);

    // taskIt returned from m_pReorder might be equal to stopIt, which might be both valid or invalid iterator
    // In the former case, stopIt points to IDR frame. In the latter case, stopIt points to end iterator in the original container
    auto taskIt = (*m_pReorder)(begin, stopIt, bFlush);
    if (taskIt == end)
        return taskIt;

    TItWrap task(taskIt);
    auto firstToDisplay = GetFirstFrameToDisplay(TItWrap(begin), TItWrap(stopIt), task);
    if (firstToDisplay != stopIt)
    {
        task->NextBufferedDisplayOrder = firstToDisplay->DisplayOrderInGOP;
    }
    else if (std::distance(begin, end) < 2)
    {
        // need to show all hidden frames in minGop
        task->NextBufferedDisplayOrder = std::numeric_limits<mfxI32>::max();
    }
    else if (bFlush)
    {
        // need to show all hidden frames before end of GOP or end of sequence
        task->NextBufferedDisplayOrder = std::numeric_limits<mfxI32>::max();
    }

    return taskIt;
}

TTaskIt TaskManager::GetDestToPushQuery(TTaskIt begin, TTaskIt end, StorageW& task)
{
    auto taskPar = Task::Common::Get(task);
    // Move current task to some location after first shown frame
    TTaskIt it = begin;
    for (; it != end; it++)
    {
        auto tempPar = Task::Common::Get(*it);
        if (tempPar.DisplayOrder == taskPar.DisplayOrder)
            continue;

        if (!IsHiddenFrame(tempPar))
        {
            it++;
            break;
        }
    }

    for (; it != end; it++)
    {
        auto tempPar = Task::Common::Get(*it);
        if (tempPar.DisplayOrder > taskPar.DisplayOrder)
            break;
    }

    return it;
}

bool TaskManager::IsForceSync(const StorageR& task) const
{
    return Task::Common::Get(task).bForceSync;
}

mfxBitstream* TaskManager::GetBS(const StorageR& task) const
{
    return Task::Common::Get(task).pBsOut;
}

void TaskManager::SetBS(StorageW& task, mfxBitstream* pBS) const
{
    Task::Common::Get(task).pBsOut = pBS;
}

bool TaskManager::GetRecode(const StorageR& task) const
{
    return Task::Common::Get(task).bRecode;
}

void TaskManager::SetRecode(StorageW& task, bool bRecode) const
{
    Task::Common::Get(task).bRecode = bRecode;
}

bool TaskManager::GetFreed(const StorageR& task) const
{
    return Task::Common::Get(task).bFreed;
}

void TaskManager::SetFreed(StorageW& task, bool bFreed) const
{
    Task::Common::Get(task).bFreed = bFreed;
}

mfxU32 TaskManager::GetBsDataLength(const StorageR& task) const
{
    return Task::Common::Get(task).BsDataLength;
}

void TaskManager::SetBsDataLength(StorageW& task, mfxU32 len) const
{
    Task::Common::Get(task).BsDataLength = len;
}

void TaskManager::AddNumRecode(StorageW& task, mfxU16 n) const
{
    Task::Common::Get(task).NumRecode += n;
}

bool TaskManager::IsCached(StorageW& task) const
{
    return Task::Common::Get(task).bCached;
}

void TaskManager::SetCached(StorageW& task, bool bCached) const
{
    Task::Common::Get(task).bCached = bCached;
}

void TaskManager::ClearBRCUpdateFlag(StorageW& task) const
{
#if defined(MFX_ENABLE_ENCTOOLS)
    Task::Common::Get(task).bBRCUpdated = false;
#else
    std::ignore = task;
#endif
}

mfxStatus TaskManager::RunQueueTaskAlloc(StorageRW& task)
{
    return RunBlocks(
        Check<mfxStatus, MFX_ERR_NONE>
        , FeatureBlocks::BQ<AT>::Get(*m_pBlocks)
        , *m_pGlob
        , task);
}

mfxStatus TaskManager::RunQueueTaskInit(
    mfxEncodeCtrl* pCtrl
    , mfxFrameSurface1* pSurf
    , mfxBitstream* pBs
    , StorageW& task)
{
    return RunBlocks(
        CheckGE<mfxStatus, MFX_ERR_NONE>
        , FeatureBlocks::BQ<IT>::Get(*m_pBlocks)
        , pCtrl, pSurf, pBs, *m_pGlob, task);
}

mfxStatus TaskManager::RunQueueTaskPreReorder(StorageW& task)
{
    return RunBlocks(
        Check<mfxStatus, MFX_ERR_NONE>
        , FeatureBlocks::BQ<PreRT>::Get(*m_pBlocks)
        , *m_pGlob, task);
}

mfxStatus TaskManager::RunQueueTaskPostReorder(StorageW& task)
{
    return RunBlocks(
        Check<mfxStatus, MFX_ERR_NONE>
        , FeatureBlocks::BQ<PostRT>::Get(*m_pBlocks)
        , *m_pGlob
        , task);
}

mfxStatus TaskManager::RunQueueTaskSubmit(StorageW& task)
{
    return RunBlocks(
        Check<mfxStatus, MFX_ERR_NONE>
        , FeatureBlocks::BQ<ST>::Get(*m_pBlocks)
        , *m_pGlob
        , task);
}

bool TaskManager::RunQueueTaskQuery(
    StorageW& task
    , std::function<bool(const mfxStatus&)> stopAt)
{
    auto& q = FeatureBlocks::BQ<QT>::Get(*m_pBlocks);
    auto RunBlock = [&](FeatureBlocks::BQ<QT>::TQueue::const_reference block)
    {
        return stopAt(block.Call(*m_pGlob, task));
    };
    return std::any_of(q.begin(), q.end(), RunBlock);
}

mfxStatus TaskManager::RunQueueTaskFree(StorageW& task)
{
    return RunBlocks(
        Check<mfxStatus, MFX_ERR_NONE>
        , FeatureBlocks::BQ<FT>::Get(*m_pBlocks)
        , *m_pGlob
        , task);
}

void TaskManager::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_Init
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        Glob::TaskManager::GetOrConstruct(strg, TMRefWrapper(*this));
        return MFX_ERR_NONE;
    });
}

void TaskManager::InitAlloc(const FeatureBlocks& blocks, TPushIA Push)
{
    Push(BLK_InitAlloc
        , [this, &blocks](StorageRW& strg, StorageRW&) -> mfxStatus
        {
            m_pBlocks = &blocks;
            m_pGlob = &strg;
            m_pPar = &Glob::VideoParam::Get(strg);
            m_pReorder = &Glob::Reorder::Get(strg);

            return ManagerInit();
        });
}

void TaskManager::SubmitTask(const FeatureBlocks& blocks, TPushST Push)
{
    Push(BLK_UpdateTask
        , [this, &blocks](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
        {
            auto& tm = Glob::TaskManager::Get(global).m_tm;
            if (tm.UpdateTask)
            {
                return tm.UpdateTask(global, &s_task);
            }

            return MFX_ERR_NONE;
        });
}

void TaskManager::FrameSubmit(const FeatureBlocks& /*blocks*/, TPushFS Push)
{
    Push(BLK_NewTask
        , [this](
            mfxEncodeCtrl* pCtrl
            , mfxFrameSurface1* pSurf
            , mfxBitstream& bs
            , StorageRW& global
            , StorageRW& local) -> mfxStatus
        {
            m_pGlob = &global;
            m_pFrameCheckLocal = &local;

            return TaskNew(pCtrl, pSurf, bs);
        });
}

void TaskManager::AsyncRoutine(const FeatureBlocks& /*blocks*/, TPushAR Push)
{
    Push(BLK_PrepareTask
        , [this](
            StorageW& /*global*/
            , StorageW& task) -> mfxStatus
        {
            return TaskPrepare(task);
        });

    Push(BLK_ReorderTask
        , [this](
            StorageW& /*global*/
            , StorageW& task) -> mfxStatus
        {
            return TaskReorder(task);
        });

    Push(BLK_SubmitTask
        , [this](
            StorageW& /*global*/
            , StorageW& task) -> mfxStatus
        {
            return TaskSubmit(task);
        });

    Push(BLK_QueryTask
        , [this](
            StorageW& /*global*/
            , StorageW& inTask) -> mfxStatus
        {
            return TaskQuery(inTask);
        });
}

void TaskManager::ResetState(const FeatureBlocks& /*blocks*/, TPushRS Push)
{
    Push(BLK_ResetState
        , [this](StorageW& global, StorageW&) -> mfxStatus
    {
        if (Glob::ResetHint::Get(global).Flags & (RF_IDR_REQUIRED | RF_CANCEL_TASKS))
        {
            CancelTasks();
        }

        auto& hint = Glob::ResetHint::Get(global);
        MFX_CHECK(hint.Flags & RF_IDR_REQUIRED, MFX_ERR_NONE);

        auto& real = Glob::RealState::Get(global);
        Glob::AllocRec::Get(real).UnlockAll();
        Glob::AllocBS::Get(real).UnlockAll();

        if (real.Contains(Glob::AllocRaw::Key))
            Glob::AllocRaw::Get(real).UnlockAll();

        return MFX_ERR_NONE;
    });
}

void TaskManager::Close(const FeatureBlocks& /*blocks*/, TPushCLS Push)
{
    Push(BLK_Close
        , [this](StorageW& /*global*/) -> mfxStatus
        {
            CancelTasks();
            return MFX_ERR_NONE;
        });
}


#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
