// Copyright (c) 2019-2020 Intel Corporation
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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base.h"
#include "av1ehw_base_data.h"
#include "ehw_task_manager.h"

namespace AV1EHW
{
namespace Base
{
    class TaskManager
        : public FeatureBase
        , public MfxEncodeHW::TaskManager
    {
    public:
#define DECL_BLOCK_LIST\
    DECL_BLOCK(Init) \
    DECL_BLOCK(InitAlloc) \
    DECL_BLOCK(ResetState) \
    DECL_BLOCK(NewTask) \
    DECL_BLOCK(PrepareTask) \
    DECL_BLOCK(ReorderTask) \
    DECL_BLOCK(SubmitTask) \
    DECL_BLOCK(UpdateTask) \
    DECL_BLOCK(QueryTask) \
    DECL_BLOCK(Close)
#define DECL_FEATURE_NAME "Base_TaskManager"
#include "av1ehw_decl_blocks.h"

        TaskManager(mfxU32 id)
            : FeatureBase(id)
        {
        }

    protected:
        NotNull<const Glob::VideoParam::TRef*> m_pPar;
        NotNull<Glob::Reorder::TRef*> m_pReorder;
        NotNull<const FeatureBlocks*> m_pBlocks;

        NotNull<StorageRW*> m_pFrameCheckLocal;

        virtual void InitInternal(const FeatureBlocks& blocks, TPushII Push) override;
        virtual void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override;
        virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
        virtual void ResetState(const FeatureBlocks& blocks, TPushRS Push) override;
        virtual void FrameSubmit(const FeatureBlocks& blocks, TPushFS Push) override;
        virtual void AsyncRoutine(const FeatureBlocks& blocks, TPushAR Push) override;
        virtual void Close(const FeatureBlocks& blocks, TPushCLS Push) override;

        virtual mfxU32 GetNumTask() const override;
        virtual mfxU16 GetBufferSize() const override;
        virtual mfxU16 GetMaxParallelSubmits() const override;
        virtual void SetActiveTask(StorageW& task) override;
        virtual bool IsInputTask(const StorageR& task) const override;
        virtual mfxU32 GetStage(const StorageR& task) const override;
        virtual void SetStage(StorageW& task, mfxU32 stage) const override;
        virtual bool IsReorderBypass() const override;
        virtual TTaskIt GetNextTaskToEncode(TTaskIt begin, TTaskIt end, bool bFlush) override;
        virtual bool IsForceSync(const StorageR& task) const override;
        virtual mfxBitstream* GetBS(const StorageR& task) const override;
        virtual void SetBS(StorageW& task, mfxBitstream* pBS) const override;
        virtual bool GetRecode(const StorageR& task) const override;
        virtual void SetRecode(StorageW& task, bool bRecode) const override;
        virtual bool GetFreed(const StorageR& task) const override;
        virtual void SetFreed(StorageW& task, bool bFreed) const override;
        virtual mfxU32 GetBsDataLength(const StorageR& task) const override;
        virtual void SetBsDataLength(StorageW& task, mfxU32 len) const override;
        virtual void AddNumRecode(StorageW& task, mfxU16 n) const override;
        virtual TTaskIt GetDestToPushQuery(TTaskIt begin, TTaskIt end, StorageW& task ) override;
        virtual bool IsCached(StorageW& /*task*/) const override;
        virtual void SetCached(StorageW& /*task*/, bool) const override;
        virtual void ClearBRCUpdateFlag(StorageW& /*task*/) const override;

        virtual mfxStatus RunQueueTaskAlloc(StorageRW& task) override;
        virtual mfxStatus RunQueueTaskInit(
            mfxEncodeCtrl* pCtrl
            , mfxFrameSurface1* pSurf
            , mfxBitstream* pBs
            , StorageW& task) override;
        virtual mfxStatus RunQueueTaskPreReorder(StorageW& task) override;
        virtual mfxStatus RunQueueTaskPostReorder(StorageW& task) override;
        virtual mfxStatus RunQueueTaskSubmit(StorageW& task) override;
        virtual bool RunQueueTaskQuery(
            StorageW& task
            , std::function<bool(const mfxStatus&)> stopAt) override;
        virtual mfxStatus RunQueueTaskFree(StorageW& task) override;

    };

} //Base
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
