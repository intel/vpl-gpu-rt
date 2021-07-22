// Copyright (c) 2017-2020 Intel Corporation
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
#include <algorithm>
#include <mfxdeprecated.h>

const mfxPluginUID NativePlugins[] =
{
    MFX_PLUGINID_HEVCD_HW,
    MFX_PLUGINID_VP8D_HW,
    MFX_PLUGINID_VP9D_HW,
    MFX_PLUGINID_HEVCE_HW,
    MFX_PLUGINID_VP9E_HW,
    MFX_PLUGINID_HEVC_FEI_ENCODE
};

#include "mfxstructures-int.h"
#define FUNCTION_DEPRECATED_IMPL(component, func_name, formal_param_list) \
mfxStatus APIImpl_MFXVideo##component##_##func_name formal_param_list \
{ \
    return MFX_ERR_UNSUPPORTED; \
}

FUNCTION_DEPRECATED_IMPL(USER, Unregister,        (mfxSession /*session*/, mfxU32 /*type*/))
FUNCTION_DEPRECATED_IMPL(USER, ProcessFrameAsync, (mfxSession /*session*/, const mfxHDL * /*in*/, mfxU32 /*in_num*/, const mfxHDL * /*out*/, mfxU32 /*out_num*/, mfxSyncPoint * /*syncp*/))
FUNCTION_DEPRECATED_IMPL(USER, GetPlugin,         (mfxSession /*session*/, mfxU32 /*type*/, mfxPlugin * /*par*/))
#undef FUNCTION_DEPRECATED_IMPL

mfxStatus MFXVideoUSER_Register(mfxSession session, mfxU32 /*type*/,
                                const mfxPlugin *par)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK_NULL_PTR1(par->GetPluginParam);

    //check is this plugin was included into MSDK lib as a native component
    mfxPluginParam pluginParam = {};
    mfxRes = par->GetPluginParam(par->pthis, &pluginParam);
    MFX_CHECK_STS(mfxRes);
    if (std::find(std::begin(NativePlugins), std::end(NativePlugins), pluginParam.PluginUID) != std::end(NativePlugins)) {
        return MFX_ERR_NONE;
    }
    return MFX_ERR_UNSUPPORTED;
}

