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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_base_scc.h"
#include "va/va.h"
#include "hevcehw_base_va_packer_lin.h"

namespace HEVCEHW
{
namespace Linux
{
namespace Base
{
class SCC
    : public HEVCEHW::Base::SCC
{
public:
    SCC(mfxU32 FeatureId)
        : HEVCEHW::Base::SCC(FeatureId)
    {}
protected:

    void SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push) override
    {
        Push(BLK_PatchDDITask
            , [this](StorageW& global, StorageW& /*s_task*/) -> mfxStatus
        {
            MFX_CHECK(m_bPatchNextDDITask || m_bPatchDDISlices, MFX_ERR_NONE);
            auto& ddiPar = HEVCEHW::Base::Glob::DDI_SubmitParam::Get(global);
            auto  itPPS  = std::find_if(std::begin(ddiPar), std::end(ddiPar)
                , [](HEVCEHW::Base::DDIExecParam& ep) { return (ep.Function == VAEncPictureParameterBufferType); });
            MFX_CHECK(itPPS != std::end(ddiPar) && itPPS->In.pData, MFX_ERR_NOT_FOUND);
            auto& ddiPPS    = *(VAEncPictureParameterBufferHEVC*)itPPS->In.pData;

            auto& sccflags = Glob::SCCFlags::Get(global);

            // Per-frame current-pic referencing (TU 6/7 only): keep it on for the key frame
            // (coding_type I), turn it off for inter frames so the driver builds the HW ref list
            // from temporal refs (no current-pic self-ref); the inter slices then reference the
            // 2nd PPS (curr_pic_ref=0). Other target usages keep it on for every frame (legacy).
            if (sccflags.IBCEnable)
            {
                ddiPPS.scc_fields.bits.pps_curr_pic_ref_enabled_flag =
                    (IsInterCurrPicRefDisabled(global) && ddiPPS.pic_fields.bits.coding_type != 1 /*CODING_TYPE_I*/)
                        ? 0
                        : PpsExt::Get(global).curr_pic_ref_enabled_flag;
            }

            if(m_bPatchNextDDITask)
            {
                m_bPatchNextDDITask = false;
                auto  itSPS   = std::find_if(std::begin(ddiPar), std::end(ddiPar)
                    , [](HEVCEHW::Base::DDIExecParam& ep) { return (ep.Function == VAEncSequenceParameterBufferType); });
                MFX_CHECK(itSPS != std::end(ddiPar) && itSPS->In.pData, MFX_ERR_NOT_FOUND);
                auto& ddiSPS    = *(VAEncSequenceParameterBufferHEVC*)itSPS->In.pData;

                ddiSPS.scc_fields.bits.palette_mode_enabled_flag = SpsExt::Get(global).palette_mode_enabled_flag;
            }
            return MFX_ERR_NONE;
        });
    }
};
} //Base
} //Linux
} //namespace HEVCEHW

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
