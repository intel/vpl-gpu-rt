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

#ifndef __MFX_IENCTOOLS_H__
#define __MFX_IENCTOOLS_H__

#include "mfxcommon.h"
#include "mfxenctools-int.h"
#include "mfx_enctools_defs.h"

struct IEncTools
{
    virtual ~IEncTools() {};

    virtual mfxStatus Init(mfxExtEncToolsConfig const*, mfxEncToolsCtrl const*) = 0;
    virtual mfxStatus GetSupportedConfig(mfxExtEncToolsConfig*, mfxEncToolsCtrl const*) = 0;
    virtual mfxStatus GetActiveConfig(mfxExtEncToolsConfig*) = 0;
    virtual mfxStatus GetDelayInFrames(mfxExtEncToolsConfig const*, mfxEncToolsCtrl const*, mfxU32* /*numFrames*/) = 0;
    virtual mfxStatus Reset(mfxExtEncToolsConfig const*, mfxEncToolsCtrl const*) = 0;
    virtual mfxStatus Close() = 0;
    virtual mfxStatus Submit(mfxEncToolsTaskParam const*) = 0;
    virtual mfxStatus Query(mfxEncToolsTaskParam*, mfxU32 /*timeOut*/) = 0;
    virtual mfxStatus Discard(mfxU32 /*displayOrder*/) = 0;

    void* m_etModule = nullptr;
};

struct IEncToolsBRC
{
    virtual ~IEncToolsBRC() {}

    virtual mfxStatus Init(mfxEncToolsCtrl const&, bool /*bMBBRC*/, bool) = 0;
    virtual mfxStatus Reset(mfxEncToolsCtrl const&, bool /*bMBBRC*/, bool) = 0;
    virtual void Close() = 0;

    virtual mfxStatus ReportEncResult(mfxU32 /*dispOrder*/, mfxEncToolsBRCEncodeResult const&) = 0;
    virtual mfxStatus SetFrameStruct(mfxU32 /*dispOrder*/, mfxEncToolsBRCFrameParams  const&) = 0;
    virtual mfxStatus ReportBufferHints(mfxU32 /*dispOrder*/, mfxEncToolsBRCBufferHint const&) = 0;
    virtual mfxStatus ReportGopHints(mfxU32 /*dispOrder*/, mfxEncToolsHintPreEncodeGOP const&) = 0;
    virtual mfxStatus ProcessFrame(mfxU32 /*dispOrder*/, mfxEncToolsBRCQuantControl*, mfxEncToolsHintQPMap*) = 0;
    virtual mfxStatus UpdateFrame(mfxU32 /*dispOrder*/, mfxEncToolsBRCStatus*) = 0;
    virtual mfxStatus GetHRDPos(mfxU32 /*dispOrder*/, mfxEncToolsBRCHRDPos*) = 0;
    virtual mfxStatus DiscardFrame(mfxU32 /*dispOrder*/) = 0;
};

namespace EncToolsFuncs
{
    inline mfxStatus Init(mfxHDL pthis, mfxExtEncToolsConfig* config, mfxEncToolsCtrl* ctrl)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((IEncTools*)pthis)->Init(config, ctrl);
    }

    inline mfxStatus GetSupportedConfig(mfxHDL pthis, mfxExtEncToolsConfig* config, mfxEncToolsCtrl* ctrl)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((IEncTools*)pthis)->GetSupportedConfig(config, ctrl);
    }
    inline mfxStatus GetActiveConfig(mfxHDL pthis, mfxExtEncToolsConfig* config)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((IEncTools*)pthis)->GetActiveConfig(config);
    }
    inline mfxStatus Reset(mfxHDL pthis, mfxExtEncToolsConfig* config, mfxEncToolsCtrl* ctrl)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((IEncTools*)pthis)->Reset(config, ctrl);
    }
    inline mfxStatus Close(mfxHDL pthis)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((IEncTools*)pthis)->Close();
    }

    // Submit info to tool
    inline mfxStatus Submit(mfxHDL pthis, mfxEncToolsTaskParam* submitParam)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((IEncTools*)pthis)->Submit(submitParam);
    }

    // Query info from tool
    inline mfxStatus Query(mfxHDL pthis, mfxEncToolsTaskParam* queryParam, mfxU32 timeOut)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((IEncTools*)pthis)->Query(queryParam, timeOut);
    }

    // Query info from tool
    inline mfxStatus Discard(mfxHDL pthis, mfxU32 displayOrder)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((IEncTools*)pthis)->Discard(displayOrder);
    }
    inline mfxStatus GetDelayInFrames(mfxHDL pthis, mfxExtEncToolsConfig* config, mfxEncToolsCtrl* ctrl, mfxU32* numFrames)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((IEncTools*)pthis)->GetDelayInFrames(config, ctrl, numFrames);
    }

}

#endif
