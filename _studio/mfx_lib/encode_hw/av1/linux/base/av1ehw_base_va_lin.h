// Copyright (c) 2021-2023 Intel Corporation
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
#include "av1ehw_ddi.h"
#include "av1ehw_base_data.h"
#include "av1ehw_base_iddi.h"
#include "ehw_device_vaapi.h"

namespace AV1EHW
{
namespace Linux
{
namespace Base
{
using namespace AV1EHW::Base;

class DDI_VA
    : public IDDI
    , public MfxEncodeHW::DeviceVAAPI
{
public:
    DDI_VA(mfxU32 FeatureId)
        : IDDI(FeatureId)
    {
        SetTraceName("Base_DDI_VA");
    }

protected:
    virtual void Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
    virtual void Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
    virtual void InitExternal(const FeatureBlocks& blocks, TPushIE Push) override;
    virtual void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override;
    virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
    virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;
    virtual void ResetState(const FeatureBlocks& blocks, TPushRS Push) override;
    virtual void SetDefaults(const FeatureBlocks& blocks, TPushSD Push) override;

    virtual mfxStatus SetDDIID(mfxU16 targetBitDepth, mfxU16 targetChromaFormat) override;

    mfxStatus CreateVABuffers(
        const std::list<DDIExecParam>& par
        , std::vector<VABufferID>& pool);

    mfxStatus DestroyVABuffers(std::vector<VABufferID>& pool);

    using MfxEncodeHW::DeviceVAAPI::QueryCaps;
    mfxStatus QueryCaps();
    mfxStatus CreateAndQueryCaps(const mfxVideoParam& par, StorageW& strg);
    uint32_t  ConvertRateControlMFX2VAAPI(mfxU16 rateControl);

    EncodeCapsAv1           m_caps;
    std::vector<VABufferID> m_perSeqPar;
    std::vector<VABufferID> m_perPicPar;
    std::vector<VABufferID> m_bs;
    VAID* m_vaid = nullptr;
    eMFXHWType m_hw = MFX_HW_UNKNOWN;
};

} //Base
} //Linux
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
