// Copyright (c) 2019-2021 Intel Corporation
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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_base_task.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;

mfxU32 TaskManager::GetNumTask() const
{
    return m_pPar->AsyncDepth + m_pReorder->BufferSize + (m_pPar->AsyncDepth > 1) + TMInterface::Get(*m_pGlob).ResourceExtra;
}

mfxU16 TaskManager::GetBufferSize() const
{
    return !m_pPar->mfx.EncodedOrder * (m_pReorder->BufferSize + (m_pPar->AsyncDepth > 1) + TMInterface::Get(*m_pGlob).ResourceExtra);
}

mfxU16 TaskManager::GetMaxParallelSubmits() const
{
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

TTaskIt TaskManager::GetNextTaskToEncode(TTaskIt begin, TTaskIt end, bool bFlush)
{
    auto IsIdrTask = [](const StorageR& rTask) { return IsIdr(Task::Common::Get(rTask).FrameType); };
    auto it = std::find_if(begin, end, IsIdrTask);
    bFlush |= (it != end);
    return (*m_pReorder)(begin, it, bFlush);
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
    auto SetInterface = [this](StorageRW& strg, StorageRW&)
    {
        TMInterface::GetOrConstruct(strg, ExtTMInterface(*this));
        return MFX_ERR_NONE;
    };
    Push(BLK_SetTMInterface, SetInterface);
}

void TaskManager::InitAlloc(const FeatureBlocks& blocks, TPushIA Push)
{
    Push(BLK_Init
        , [this, &blocks](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        m_pBlocks   = &blocks;
        m_pGlob     = &strg;
        m_pPar      = &Glob::VideoParam::Get(strg);
        m_pReorder  = &Glob::Reorder::Get(strg);

        return ManagerInit();
    });
}

void TaskManager::FrameSubmit(const FeatureBlocks& /*blocks*/, TPushFS Push)
{
    Push(BLK_NewTask
        , [this](
             mfxEncodeCtrl* pCtrl
            , mfxFrameSurface1* pSurf
            , mfxBitstream& bs
            , StorageW& global
            , StorageRW& local) -> mfxStatus
    {
        m_pGlob            = &global;
        m_pFrameCheckLocal = &local;
        return TaskNew(pCtrl, pSurf, bs);
    });
}

mfxStatus TaskManager::RunExtraStages(mfxU16 beginStageID, mfxU16 endStageID, StorageW& task)
{
    MFX_CHECK(!m_pGlob.isNull(), MFX_ERR_NONE);
    auto& stages = TMInterface::Get(*m_pGlob).AsyncStages;
    auto itStage = stages.find(beginStageID);
    auto itEnd   = stages.find(endStageID);

    MFX_CHECK(itStage != stages.end(), MFX_ERR_NONE);

    for (; itStage != itEnd; ++itStage)
    {
        MFX_SAFE_CALL(itStage->second(*m_pGlob, task));
    }

    return MFX_ERR_NONE;
}

mfxStatus TaskManager::TaskPrepare(StorageW& task)
{
    MFX_SAFE_CALL(RunExtraStages(NextStage(S_NEW), Stage(S_PREPARE), task));
    return MfxEncodeHW::TaskManager::TaskPrepare(task);
}

mfxStatus TaskManager::TaskReorder(StorageW& task)
{
    MFX_SAFE_CALL(RunExtraStages(NextStage(S_PREPARE), Stage(S_REORDER), task));
    return MfxEncodeHW::TaskManager::TaskReorder(task);
}

mfxStatus TaskManager::TaskSubmit(StorageW& task)
{
    MFX_SAFE_CALL(RunExtraStages(NextStage(S_REORDER), Stage(S_SUBMIT), task));
    if (TMInterface::Get(*m_pGlob).UpdateTask) TMInterface::Get(*m_pGlob).UpdateTask(GetTask(Stage(S_SUBMIT)));
    return MfxEncodeHW::TaskManager::TaskSubmit(task);
}

mfxStatus TaskManager::TaskQuery(StorageW& task)
{
    MFX_SAFE_CALL(RunExtraStages(NextStage(S_SUBMIT), Stage(S_QUERY), task));
    return MfxEncodeHW::TaskManager::TaskQuery(task);
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
    Push(BLK_Reset
        , [this](StorageW& global, StorageW&) -> mfxStatus
    {
        if (Glob::ResetHint::Get(global).Flags & (RF_IDR_REQUIRED | RF_CANCEL_TASKS))
        {
            CancelTasks();
        }

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

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
