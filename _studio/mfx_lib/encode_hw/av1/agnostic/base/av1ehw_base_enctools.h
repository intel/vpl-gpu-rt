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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
#if defined(MFX_ENABLE_ENCTOOLS)

#include "av1ehw_base.h"
#include "av1ehw_base_enctools_com.h"
#include "av1ehw_base_data.h"
#include "mfxenctools-int.h"
#include "mfx_enc_common.h"

#include "libmfx_core.h"            //SurfaceCache

#include "av1ehw_base_enctools_com.h"

namespace AV1EHW
{
namespace Base
{
    class AV1EncTools
        : public AV1EncToolsCommon
    {
    public:
        virtual mfxStatus SubmitPreEncTask(StorageW&  global, StorageW& s_task) override; 
        mfxStatus BRCGetCtrl(StorageW&  global, StorageW& s_task, mfxEncToolsBRCQuantControl &extQuantCtrl, mfxEncToolsBRCHRDPos  &extHRDPos);
        mfxStatus BRCUpdate(StorageW&  global, StorageW& s_task, mfxEncToolsBRCStatus &sts);

        mfxStatus QueryPreEncTask(StorageW& global, StorageW& s_task) override;
        virtual bool IsFeatureEnabled(const mfxVideoParam& par) override;

        virtual void SetDefaultConfig(const mfxVideoParam &video, mfxExtEncToolsConfig &config, bool bMBQPSupport) override;
        virtual mfxU32 CorrectVideoParams(mfxVideoParam& video, mfxExtEncToolsConfig& supportedConfig) override;
        virtual mfxStatus InitEncToolsCtrl(mfxVideoParam const& par, mfxEncToolsCtrl* ctrl) override;
        AV1EncTools(mfxU32 FeatureId)
            : AV1EncToolsCommon(FeatureId)
        {
#if defined(MFX_ENABLE_LOG_UTILITY)
            m_trace.second.insert(AV1EncToolsCommon::m_trace.second.begin(), AV1EncToolsCommon::m_trace.second.end());
#endif
        }

    protected:
    virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;
    virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
    virtual void Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
    virtual void SetSupported(ParamSupport& par) override;
    void SetInherited(ParamInheritance& par) override;
    virtual void Reset(const FeatureBlocks& blocks, TPushR Push) override;
    virtual void InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push) override;
    virtual void FreeTask(const FeatureBlocks& blocks, TPushQT Push) override;

    void AllocSegmentationData(mfxU16 frame_width, mfxU16 frame_height, mfxU8 blockSize);
    void ReleaseSegmentationData(void); //m_destroy

    bool m_enablePercEncPrefilter = false;
    std::unique_ptr<SurfaceCache> m_filteredFrameCache;
    bool m_saliencyMapSupported = false;
    size_t m_saliencyMapSize = 0;

    };

} //Base
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_ENCTOOLS)
#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
