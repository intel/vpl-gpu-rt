// Copyright (c) 2019-2024 Intel Corporation
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

#include "hevcehw_disp.h"
#include "hevcehw_base.h"

#include "hevcehw_g12_lin.h"
namespace HEVCEHWDisp
{
    namespace TGL { using namespace HEVCEHW::Linux::Gen12; };
    namespace DG1 { using namespace HEVCEHW::Linux::Gen12; };
};

#include "hevcehw_xe_hpm_lin.h"
namespace HEVCEHWDisp
{
    namespace Xe_HPM { using namespace HEVCEHW::Linux::Xe_HPM; };
};

#include "hevcehw_xe_lpm_plus_lin.h"
namespace HEVCEHWDisp
{
    namespace Xe_LPM_Plus { using namespace HEVCEHW::Linux::Xe_LPM_Plus; };
};



    #include "hevcehw_xe2_lin.h"
    namespace HEVCEHWDisp
    {
        namespace Xe2 { using namespace HEVCEHW::Linux::Xe2; };
    };


namespace HEVCEHW
{

static ImplBase* CreateSpecific(
    eMFXHWType HW
    , VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
{
    if (HW >= MFX_HW_BMG)
        return new HEVCEHWDisp::Xe2::MFXVideoENCODEH265_HW(core, status, mode);
    if (HW >= MFX_HW_MTL)
        return new HEVCEHWDisp::Xe_LPM_Plus::MFXVideoENCODEH265_HW(core, status, mode);
    if (HW >= MFX_HW_DG2)
        return new HEVCEHWDisp::Xe_HPM::MFXVideoENCODEH265_HW(core, status, mode);
    if (HW == MFX_HW_DG1)
        return new HEVCEHWDisp::DG1::MFXVideoENCODEH265_HW(core, status, mode);

    return new HEVCEHWDisp::TGL::MFXVideoENCODEH265_HW(core, status, mode);
}

VideoENCODE* Create(
    VideoCORE& core
    , mfxStatus& status)
{
    auto hw = core.GetHWType();

    if (hw < MFX_HW_TGL_LP)
    {
        status = MFX_ERR_UNSUPPORTED;
        return nullptr;
    }

    auto p = CreateSpecific(hw, core, status, eFeatureMode::INIT);

    return p;
}

mfxStatus QueryIOSurf(
    VideoCORE *core
    , mfxVideoParam *par
    , mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR3(core, par, request);

    auto hw = core->GetHWType();

    if (hw < MFX_HW_TGL_LP)
        MFX_RETURN(MFX_ERR_UNSUPPORTED);

    mfxStatus sts = MFX_ERR_NONE;
    std::unique_ptr<ImplBase> impl(CreateSpecific(hw, *core, sts, eFeatureMode::QUERY_IO_SURF));

    MFX_CHECK_STS(sts);
    MFX_CHECK(impl, MFX_ERR_UNKNOWN);

    impl.reset(impl.release()->ApplyMode(IMPL_MODE_DEFAULT));
    MFX_CHECK(impl, MFX_ERR_UNKNOWN);

    return impl->InternalQueryIOSurf(*core, *par, *request);
}

mfxStatus Query(
    VideoCORE *core
    , mfxVideoParam *in
    , mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR2(core, out);

    auto hw = core->GetHWType();

    if (hw < MFX_HW_TGL_LP)
        MFX_RETURN(MFX_ERR_UNSUPPORTED);

    if (in)
    {
        const mfxExtEncoderResetOption* pResetOpt = ExtBuffer::Get(*in);
        MFX_CHECK(!pResetOpt, MFX_ERR_NONE);
    }

    mfxStatus sts = MFX_ERR_NONE;
    std::unique_ptr<ImplBase> impl(CreateSpecific(hw, *core, sts, in ? eFeatureMode::QUERY1 : eFeatureMode::QUERY0));

    MFX_CHECK_STS(sts);
    MFX_CHECK(impl, MFX_ERR_UNKNOWN);

    impl.reset(impl.release()->ApplyMode(IMPL_MODE_DEFAULT));
    MFX_CHECK(impl, MFX_ERR_UNKNOWN);

    return impl->InternalQuery(*core, in, *out);
}

mfxStatus QueryImplsDescription(
    VideoCORE& core
    , mfxEncoderDescription::encoder& caps
    , mfx::PODArraysHolder& ah)
{
    auto hw = core.GetHWType();

    if (hw < MFX_HW_TGL_LP)
        MFX_RETURN(MFX_ERR_UNSUPPORTED);

    mfxStatus sts = MFX_ERR_NONE;
    std::unique_ptr<ImplBase> impl(CreateSpecific(hw, core, sts, eFeatureMode::QUERY_IMPLS_DESCRIPTION));

    MFX_CHECK_STS(sts);
    MFX_CHECK(impl, MFX_ERR_UNKNOWN);

    return impl->QueryImplsDescription(core, caps, ah);
}

} //namespace HEVCEHW

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
