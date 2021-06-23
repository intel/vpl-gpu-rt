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

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_ENCTOOLS)

#include "hevcehw_base.h"
#include "hevcehw_base_data.h"
#include "mfxenctools-int.h"
#include "mfx_enc_common.h"

namespace HEVCEHW
{
namespace Base
{
    class HevcEncTools
        : public FeatureBase
    {
    public:
        mfxStatus SubmitPreEncTask(StorageW&  global, StorageW& s_task);
        mfxStatus QueryPreEncTask(StorageW&  global, StorageW& s_task);
        mfxStatus BRCGetCtrl(StorageW&  global, StorageW& s_task,
            mfxEncToolsBRCQuantControl &extQuantCtrl, mfxEncToolsBRCHRDPos  &extHRDPos);
        mfxStatus BRCUpdate(StorageW&  global, StorageW& s_task,
            mfxEncToolsBRCStatus &sts);

#define DECL_BLOCK_LIST\
    DECL_BLOCK(Check)\
    DECL_BLOCK(Init)\
    DECL_BLOCK(ResetCheck)\
    DECL_BLOCK(Reset)\
    DECL_BLOCK(SetCallChains)\
    DECL_BLOCK(AddTask)\
    DECL_BLOCK(PreEncSubmit)\
    DECL_BLOCK(PreEncQuery)\
    DECL_BLOCK(GetFrameCtrl)\
    DECL_BLOCK(UpdateTask)\
    DECL_BLOCK(Update)\
    DECL_BLOCK(Discard)\
    DECL_BLOCK(Close)\
    DECL_BLOCK(SetDefaults)\
    DECL_BLOCK(QueryIOSurf)
#define DECL_FEATURE_NAME "Base_EncTools"
#include "hevcehw_decl_blocks.h"

        HevcEncTools(mfxU32 FeatureId)
            : FeatureBase(FeatureId)
        {}

    protected:
        virtual void SetSupported(ParamSupport& par) override;
        virtual void SetInherited(ParamInheritance& par) override;
        virtual void Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
        virtual void SetDefaults(const FeatureBlocks& blocks, TPushSD Push) override;
        virtual void InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push) override;
        virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
        virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;
        virtual void FreeTask(const FeatureBlocks& blocks, TPushQT Push) override;

        virtual void Reset(const FeatureBlocks& blocks, TPushR Push) override;
        virtual void ResetState(const FeatureBlocks& blocks, TPushRS Push) override;
        virtual void Close(const FeatureBlocks& blocks, TPushCLS Push) override;
        virtual void QueryIOSurf(const FeatureBlocks&, TPushQIS Push) override;

        mfxEncTools*            m_pEncTools = nullptr;
        mfxEncToolsCtrl         m_EncToolCtrl = {};
        mfxExtEncToolsConfig    m_EncToolConfig = {};
        bool                    m_bEncToolsInner = false;
        mfxU32                  m_maxDelay = 0;

        mfxU16        S_ET_SUBMIT = mfxU16(-1);
        mfxU16        S_ET_QUERY = mfxU16(-1);

        std::list<mfxLplastatus>         LpLaStatus;

        OnExit    m_destroy;
    };

    bool IsEncToolsOptOn(const mfxExtEncToolsConfig &config, bool bGameStreaming);
    bool IsLPLAEncToolsOn(const mfxExtEncToolsConfig &config, bool bGameStreaming);

} //Base
} //namespace HEVCEHW
#endif
