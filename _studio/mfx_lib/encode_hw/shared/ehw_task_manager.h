// Copyright (c) 2020-2021 Intel Corporation
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
#include <mutex>
#include <condition_variable>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>

#include "ehw_utils.h"
#include "feature_blocks/mfx_feature_blocks_utils.h"

namespace MfxEncodeHW
{
    class CachedBitstream
    {
    public:
        mfxU32  DisplayOrder = 0;
        mfxU32  BsDataLength = 0;
        std::shared_ptr<std::vector<mfxU8>> Data;
        bool isHiden = true;
        CachedBitstream(mfxU32 length, mfxU8* pData=nullptr)
        {
            Data = std::make_shared<std::vector<mfxU8>>(std::vector<mfxU8>(length));
            BsDataLength = length;
            if (pData != nullptr)
            {
                std::copy(pData, pData + length, Data->data());
            }
        }
    };

    using namespace MfxFeatureBlocks;

    class TaskManager
    {
    private:
        std::map<mfxU32, std::deque<CachedBitstream>> m_cachedBitstream  = {};
        std::map<mfxU32, bool>                        m_outputReady      = {};
        std::mutex                                    m_mtx, m_closeMtx;
        std::condition_variable                       m_cv;

    public:

        class TMRefWrapper
        {
        public:
            TMRefWrapper(TaskManager& tm)
                : m_tm(tm)
            {}

            TaskManager& m_tm;
        };

        using TTaskList  = std::list<StorageRW>;
        using TTaskIt    = TTaskList::iterator;
        using TFnGetTask = std::function<TTaskIt(TTaskIt, TTaskIt)>;

        virtual ~TaskManager() {}

        virtual mfxU32        GetNumTask           () const = 0;
        virtual mfxU16        GetBufferSize        () const = 0;
        virtual mfxU16        GetMaxParallelSubmits() const = 0;
        virtual void          SetActiveTask        (StorageW& /*task*/) = 0;
        virtual bool          IsInputTask          (const StorageR& /*task*/) const = 0;
        virtual mfxU32        GetStage             (const StorageR& /*task*/) const = 0;
        virtual void          SetStage             (StorageW& /*task*/, mfxU32) const = 0;
        virtual bool          IsReorderBypass      () const = 0;
        virtual TTaskIt       GetNextTaskToEncode  (TTaskIt /*begin*/, TTaskIt /*end*/, bool /*bFlush*/) = 0;
        virtual bool          IsForceSync          (const StorageR& /*task*/) const = 0;
        virtual mfxBitstream* GetBS                (const StorageR& /*task*/) const = 0;
        virtual void          SetBS                (StorageW& /*task*/, mfxBitstream* /*pBS*/) const = 0;
        virtual bool          GetRecode            (const StorageR& /*task*/) const = 0;
        virtual void          SetRecode            (StorageW& /*task*/, bool) const = 0;
        virtual bool          GetFreed             (const StorageR& /*task*/) const = 0;
        virtual void          SetFreed             (StorageW& /*task*/, bool) const = 0;
        virtual mfxU32        GetBsDataLength      (const StorageR& /*task*/) const = 0;
        virtual void          SetBsDataLength      (StorageW& /*task*/, mfxU32) const = 0;
        virtual void          AddNumRecode         (StorageW& /*task*/, mfxU16) const = 0;

        virtual TTaskIt GetDestToPushQuery(TTaskIt begin, TTaskIt /*end*/, StorageW& /*task*/) { return begin; }

        virtual bool IsCached(StorageW& /*task*/) const { return false; }
        virtual void SetCached(StorageW& /*task*/, bool) const { }
        virtual void ClearBRCUpdateFlag(StorageW& /*task*/) const { }

        virtual mfxStatus RunQueueTaskAlloc(StorageRW& /*task*/) = 0;
        virtual mfxStatus RunQueueTaskInit(
            mfxEncodeCtrl* /*pCtrl*/
            , mfxFrameSurface1* /*pSurf*/
            , mfxBitstream* /*pBs*/
            , StorageW& /*task*/) = 0;
        virtual mfxStatus RunQueueTaskPreReorder(StorageW& /*task*/) = 0;
        virtual mfxStatus RunQueueTaskPostReorder(StorageW& /*task*/) = 0;
        virtual mfxStatus RunQueueTaskSubmit(StorageW& /*task*/) = 0;
        virtual bool RunQueueTaskQuery(
            StorageW& /*task*/
            , std::function<bool(const mfxStatus&)> /*stopAt*/) = 0;
        virtual mfxStatus RunQueueTaskFree(StorageW& /*task*/) = 0;
        virtual mfxStatus ManagerInit();

        mfxStatus TaskNew(
            mfxEncodeCtrl* /*pCtrl*/
            , mfxFrameSurface1* /*pSurf*/
            , mfxBitstream& /*bs*/);
        mfxStatus TaskPrepare(StorageW& task);
        mfxStatus TaskReorder(StorageW& task);
        mfxStatus TaskSubmit(StorageW& task);
        mfxStatus TaskQuery(StorageW& task);

        mfxStatus RunExtraStages(mfxU16 beginStageID, mfxU16 endStageID, StorageW& task);
        mfxStatus TaskPrepareInner(StorageW& task);
        mfxStatus TaskReorderInner(StorageW& task);
        mfxStatus TaskSubmitInner(StorageW& task);
        mfxStatus TaskQueryInner(StorageW& task);

        virtual void CancelTasks();

        using TUpdateTask = CallChain<mfxStatus
            , StorageW& /*glob*/
            , StorageW* /*dstTask*/>;
        TUpdateTask UpdateTask = {};

        static constexpr mfxU16 S_NEW      = 0;
        static constexpr mfxU16 S_PREPARE  = 1;
        static constexpr mfxU16 S_REORDER  = 2;
        static constexpr mfxU16 S_SUBMIT   = 3;
        static constexpr mfxU16 S_QUERY    = 4;

        std::map<mfxU16,mfxU16> m_stageID =
        {
              {S_NEW,       S_NEW}
            , {S_PREPARE,   S_PREPARE}
            , {S_REORDER,   S_REORDER}
            , {S_SUBMIT,    S_SUBMIT}
            , {S_QUERY,     S_QUERY}
        };

        const mfxU16 max_num_of_stages               = 7; // 5 "regular" (see above) + 2 added by EncTools (S_ET_SUBMIT,  S_ET_QUERY)
        std::vector<TTaskList>  m_stages             = std::vector<TTaskList>(max_num_of_stages);
        mfxU16                  m_nPicBuffered       = 0;
        mfxU16                  m_bufferSize         = 0;
        mfxU16                  m_maxParallelSubmits = 0;
        mfxU16                  m_nTasksInExecution  = 0;
        mfxU16                  m_nRecodeTasks       = 0;
        bool                    m_bPostponeQuery     = false;

        using TAsyncStage = CallChain<mfxStatus
            , StorageW& /*glob*/
            , StorageW& /*task*/>;
        std::map<mfxU16, TAsyncStage> m_AsyncStages;
        MfxEncodeHW::Utils::NotNull<StorageW*> m_pGlob;
        mfxU16 m_ResourceExtra = 0;

        mfxU16 AddStage(mfxU16 stageBefore)
        {
            mfxU16 stageNew = mfxU16(m_stageID.size());
            mfxU16 idx = m_stageID.at(stageBefore);
            auto UpdateStage = [idx](std::map<mfxU16, mfxU16>::reference pair)
            {
                pair.second += (pair.second > idx);
            };
            std::for_each(m_stageID.begin(), m_stageID.end(), UpdateStage);
            m_stageID[stageNew] = idx + 1;
            return stageNew;
        }

        static TTaskIt    FirstTask     (TTaskIt begin, TTaskIt) { return begin; }
        static TTaskIt    EndTask       (TTaskIt, TTaskIt end) { return end; }
        static TFnGetTask SimpleCheck   (std::function<bool(StorageR&)> cond)
        {
            using namespace std::placeholders;
            return std::bind(std::find_if<TTaskIt, std::function<bool(StorageR&)>>, _1, _2, cond);
        }
        static TFnGetTask FixedTask(const StorageR& task)
        {
            auto pTask = &task;
            return SimpleCheck([pTask](StorageR& b) { return &b == pTask; });
        }
        mfxU16 Stage(mfxU16 s) { return m_stageID.at(s); }
        mfxU16 NextStage(mfxU16 s) { return Stage(s) + 1; }

        //blocking
        mfxStatus ManagerReset(mfxU32 numTask);
        //non-blocking
        StorageRW* MoveTask(
            mfxU16 from
            , mfxU16 to
            , TFnGetTask which = FirstTask
            , TFnGetTask where = EndTask);
        StorageRW* MoveTaskForward(
            mfxU16 from
            , TFnGetTask which = FirstTask
            , TFnGetTask where = EndTask)
        {
            return MoveTask(from, from + 1, which, where);
        }
        StorageRW* GetTask(mfxU16 stage, TFnGetTask which = FirstTask);

        bool IsCacheReady(mfxU32 order)
        {
            return m_outputReady[order];
        }

        void PushBitstream(mfxU32 order, CachedBitstream&& bs)
        {
            assert(!m_outputReady[order]);

            m_cachedBitstream[order].push_back(bs);
            if (!bs.isHiden)
                m_outputReady[order] = true;
        }

        std::deque<CachedBitstream>& GetBitstreams(mfxU32 order)
        {
           return m_cachedBitstream[order];
        }

        void ClearBitstreams(mfxU32 order)
        {
            m_cachedBitstream.erase(order);
            m_outputReady.erase(order);
        }

        mfxU32 PeekCachedSize(mfxU32 order) const
        {
            auto it = m_cachedBitstream.find(order);
            if (it == m_cachedBitstream.end())
                return 0;

            mfxU32 size = 0;
            for (const auto& bs : it->second)
            {
                size += bs.BsDataLength;
            }

            return size;
        }

    };
}
