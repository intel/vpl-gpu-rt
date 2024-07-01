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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)
#if defined(MFX_ENABLE_ENCTOOLS)

#include "hevcehw_base_enctools_com.h"

namespace HEVCEHW
{
namespace Base
{
    class HevcEncTools
        : public HevcEncToolsCommon
    {
    public:
        mfxStatus BRCGetCtrl(StorageW&  global, StorageW& s_task,
            mfxEncToolsBRCQuantControl &extQuantCtrl, mfxEncToolsBRCHRDPos  &extHRDPos, mfxEncToolsHintQPMap   &qpMapHint);
        mfxStatus BRCUpdate(StorageW&  global, StorageW& s_task,
            mfxEncToolsBRCStatus &sts);

        virtual bool isFeatureEnabled(const mfxVideoParam& par) override;
        virtual void SetDefaultConfig(const mfxVideoParam& video, mfxExtEncToolsConfig& config, bool bMBQPSupport) override;
        virtual bool IsEncToolsConfigOn(const mfxExtEncToolsConfig& config, bool bGameStreaming) override;
        virtual bool IsEncToolsImplicit(const mfxVideoParam& video) override;
        virtual mfxU32 CorrectVideoParams(mfxVideoParam& video, mfxExtEncToolsConfig& supportedConfig) override;
        virtual mfxStatus QueryPreEncTask(StorageW& global, StorageW& s_task) override;
        virtual mfxStatus InitEncToolsCtrl(mfxVideoParam const& par, mfxEncToolsCtrl* ctrl) override;

        HevcEncTools(mfxU32 FeatureId)
            : HevcEncToolsCommon(FeatureId)
        {}

    protected:
        virtual void SetSupported(ParamSupport& par) override;
        virtual void SetInherited(ParamInheritance& par) override;
        virtual void Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
        virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
        virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;
        virtual void Reset(const FeatureBlocks& blocks, TPushR Push) override;

    };

    bool IsEncToolsOptOn(const mfxExtEncToolsConfig &config, bool bGameStreaming);
    bool IsLPLAEncToolsOn(const mfxExtEncToolsConfig &config, bool bGameStreaming);
    bool IsHwEncToolsOn(const mfxVideoParam& video);
    bool IsSwEncToolsOn(const mfxVideoParam& video);
    bool IsSwEncToolsSpsACQM(const mfxVideoParam &video);
    bool IsSwEncToolsPpsACQM(const mfxVideoParam &video);
    int EncToolsDeblockingBetaOffset();
    int EncToolsDeblockingAlphaTcOffset();

} //Base
} //namespace HEVCEHW

#endif //defined(MFX_ENABLE_ENCTOOLS)
#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)

