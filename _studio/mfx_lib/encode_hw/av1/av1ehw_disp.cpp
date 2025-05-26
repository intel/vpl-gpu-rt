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

#include "av1ehw_disp.h"
#include "av1ehw_base.h"

    #include "av1ehw_base_lin.h"
    #include "av1ehw_xe_hpm_lin.h"
    namespace AV1EHWDisp
    {
        namespace Xe_HPM { using namespace AV1EHW::Linux::Xe_HPM; };
    };

    #include "av1ehw_xe_lpm_plus_lin.h"
    namespace AV1EHWDisp
    {
        namespace Xe_LPM_Plus { using namespace AV1EHW::Linux::Xe_LPM_Plus; };
    };

    #include "av1ehw_xe2_lin.h"
    namespace AV1EHWDisp
    {
        namespace Xe2 { using namespace AV1EHW::Linux::Xe2; };
    };

    #include "av1ehw_base_next_lin.h"
    namespace AV1EHWDisp
    {
        namespace Base_Next {using namespace AV1EHW::Linux::Base_Next; };
    }

namespace AV1EHW
{
static ImplBase* CreateSpecific(
    VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
{
    ImplBase* impl = nullptr;

    auto hw = core.GetHWType();
    if (hw < MFX_HW_SCL)
        status = MFX_ERR_UNSUPPORTED;

    if (hw == MFX_HW_DG2)
        impl = new AV1EHWDisp::Xe_HPM::MFXVideoENCODEAV1_HW(core, status, mode);
    if (hw == MFX_HW_MTL)
        impl = new AV1EHWDisp::Xe_LPM_Plus::MFXVideoENCODEAV1_HW(core, status, mode);
    else if (hw == MFX_HW_ARL)
        impl = new AV1EHWDisp::Xe_LPM_Plus::MFXVideoENCODEAV1_HW(core, status, mode);
    if (hw == MFX_HW_BMG || hw == MFX_HW_LNL)
        impl = new AV1EHWDisp::Xe2::MFXVideoENCODEAV1_HW(core, status, mode);
    if (hw >= MFX_HW_PTL)
        impl = new AV1EHWDisp::Base_Next::MFXVideoENCODEAV1_HW(core, status, mode);



    if (impl == nullptr)
        status = MFX_ERR_UNSUPPORTED;

    return impl;
}

VideoENCODE* Create(
    VideoCORE& core
    , mfxStatus& status)
{
    return CreateSpecific(core, status, eFeatureMode::INIT);
}

mfxStatus QueryIOSurf(
    VideoCORE *core
    , mfxVideoParam *par
    , mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR3(core, par, request);

    mfxStatus sts = MFX_ERR_NONE;
    std::unique_ptr<ImplBase> impl(CreateSpecific(*core, sts, eFeatureMode::QUERY_IO_SURF));

    MFX_CHECK_STS(sts);
    MFX_CHECK(impl, MFX_ERR_UNKNOWN);

    return impl->InternalQueryIOSurf(*core, *par, *request);
}

mfxStatus Query(
    VideoCORE *core
    , mfxVideoParam *in
    , mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR2(core, out);

    mfxStatus sts = MFX_ERR_NONE;
    eFeatureMode mode = in ? eFeatureMode::QUERY1 : eFeatureMode::QUERY0;
    std::unique_ptr<ImplBase> impl(CreateSpecific(*core, sts, mode));

    MFX_CHECK_STS(sts);
    MFX_CHECK(impl, MFX_ERR_UNKNOWN);

    return impl->InternalQuery(*core, in, *out);
}

mfxStatus QueryImplsDescription(
    VideoCORE& core
    , mfxEncoderDescription::encoder& caps
    , mfx::PODArraysHolder& ah)
{
    auto hw = core.GetHWType();
    std::ignore = hw;

    mfxStatus sts = MFX_ERR_NONE;
    std::unique_ptr<ImplBase> impl(CreateSpecific(core, sts, eFeatureMode::QUERY_IMPLS_DESCRIPTION));

    MFX_CHECK_STS(sts);
    MFX_CHECK(impl, MFX_ERR_UNKNOWN);

    return impl->QueryImplsDescription(core, caps, ah);
}

} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
