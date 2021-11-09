// Copyright (c) 2006-2021 Intel Corporation
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

#include "mfx_pxp_vpp_vaapi.h"

#if defined(MFX_ENABLE_PXP)

#include "mfx_trace.h"
#include "mfxstructures.h"

using namespace MfxHwVideoProcessing;

mfxStatus PXPVAAPIVPP::CreateDevice(VideoCORE * core, mfxVideoParam* pParams, bool /*isTemporal*/)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "PXPVAAPIVPP::CreateDevice");
    MFX_CHECK_NULL_PTR1(core);

    mfxStatus sts = core->GetHandle(MFX_HANDLE_PXP_CONTEXT, (mfxHDL*)&m_PXPCtxHdl);
    MFX_CHECK(MFX_ERR_NONE == sts, MFX_ERR_DEVICE_FAILED);
    MFX_CHECK(m_PXPCtxHdl != nullptr, MFX_ERR_INVALID_HANDLE);

    sts = VAAPIVideoProcessing::CreateDevice(core, pParams);
    MFX_CHECK(MFX_ERR_NONE == sts, MFX_ERR_DEVICE_FAILED);

    m_PXPCtxHdl->secureVPPCfg.ContextId = m_vaContextVPP;

    return MFX_ERR_NONE;
}
#endif // (MFX_ENABLE_PXP)