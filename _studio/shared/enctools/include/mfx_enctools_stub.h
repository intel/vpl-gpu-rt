// Copyright (c) 2022 Intel Corporation
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

#ifndef __MFX_ENCTOOLS_STUB_H__
#define __MFX_ENCTOOLS_STUB_H__

#include "mfxcommon.h"
#include "mfx_ienctools.h"

class EncToolsStub : public IEncTools
{
public:
    EncToolsStub() {}

    virtual ~EncToolsStub() { }

    virtual mfxStatus Init(mfxExtEncToolsConfig const* pEncToolConfig, mfxEncToolsCtrl const* ctrl)
    {
        (void*)pEncToolConfig;
        (void*)ctrl;
        return MFX_ERR_NOT_IMPLEMENTED;
    }
    virtual mfxStatus GetSupportedConfig(mfxExtEncToolsConfig* pConfig, mfxEncToolsCtrl const* ctrl)
    {
        (void*)pConfig;
        (void*)ctrl;
        return MFX_ERR_NOT_IMPLEMENTED;
    }
    virtual mfxStatus GetActiveConfig(mfxExtEncToolsConfig* pConfig) { (void*)pConfig; return MFX_ERR_NOT_IMPLEMENTED; }
    virtual mfxStatus GetDelayInFrames(mfxExtEncToolsConfig const* pConfig, mfxEncToolsCtrl const* ctrl, mfxU32* numFrames)
    {
        (void*)pConfig;
        (void*)ctrl;
        (void*)numFrames;
        return MFX_ERR_NOT_IMPLEMENTED;
    }
    virtual mfxStatus Reset(mfxExtEncToolsConfig const* pEncToolConfig, mfxEncToolsCtrl const* ctrl)
    {
        (void*)pEncToolConfig;
        (void*)ctrl;
        return MFX_ERR_NOT_IMPLEMENTED;
    }
    virtual mfxStatus Close() { return MFX_ERR_NOT_IMPLEMENTED;};
    virtual mfxStatus Submit(mfxEncToolsTaskParam const* par) { (void*)par; return MFX_ERR_NOT_IMPLEMENTED; }
    virtual mfxStatus Query(mfxEncToolsTaskParam* par, mfxU32 timeOut) { (void*)par; (void)timeOut;  return MFX_ERR_NOT_IMPLEMENTED; }
    virtual mfxStatus Discard(mfxU32 displayOrder) { (void)displayOrder; return MFX_ERR_NOT_IMPLEMENTED; }

};

#endif
