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

#ifndef __MFX_IENCTOOLS_H__
#define __MFX_IENCTOOLS_H__

#include "mfxcommon.h"
#include "mfxenctools-int.h"

class IEncTools
{
public:

    virtual ~IEncTools() {};
    virtual mfxStatus Init(mfxExtEncToolsConfig const* pEncToolConfig, mfxEncToolsCtrl const* ctrl) = 0;
    virtual mfxStatus GetSupportedConfig(mfxExtEncToolsConfig* pConfig, mfxEncToolsCtrl const* ctrl) = 0;
    virtual mfxStatus GetActiveConfig(mfxExtEncToolsConfig* pConfig) = 0;
    virtual mfxStatus GetDelayInFrames(mfxExtEncToolsConfig const* pConfig, mfxEncToolsCtrl const* ctrl, mfxU32* numFrames) = 0;   
    virtual mfxStatus Reset(mfxExtEncToolsConfig const* pEncToolConfig, mfxEncToolsCtrl const* ctrl) = 0;   
    virtual mfxStatus Close() = 0;
    virtual mfxStatus Submit(mfxEncToolsTaskParam const* par) = 0;
    virtual mfxStatus Query(mfxEncToolsTaskParam* par, mfxU32 timeOut) = 0;
    virtual mfxStatus Discard(mfxU32 displayOrder) = 0;

    void* m_etModule = nullptr;
};

namespace EncToolsFuncs
{

    inline mfxStatus Init(mfxHDL pthis, mfxExtEncToolsConfig* config, mfxEncToolsCtrl* ctrl)
    {
        if (!pthis)
        {
            return MFX_ERR_NULL_PTR;
        }
        return ((IEncTools*)pthis)->Init(config, ctrl);
    }

    inline mfxStatus GetSupportedConfig(mfxHDL pthis, mfxExtEncToolsConfig* config, mfxEncToolsCtrl* ctrl)
    {
        if (!pthis)
        {
            return MFX_ERR_NULL_PTR;
        }
        return ((IEncTools*)pthis)->GetSupportedConfig(config, ctrl);
    }
    inline mfxStatus GetActiveConfig(mfxHDL pthis, mfxExtEncToolsConfig* config)
    {
        if (!pthis)
        {
            return MFX_ERR_NULL_PTR;
        }
        return ((IEncTools*)pthis)->GetActiveConfig(config);
    }
    inline mfxStatus Reset(mfxHDL pthis, mfxExtEncToolsConfig* config, mfxEncToolsCtrl* ctrl)
    {
        if (!pthis)
        {
            return MFX_ERR_NULL_PTR;
        }
        return ((IEncTools*)pthis)->Reset(config, ctrl);
    }
    inline mfxStatus Close(mfxHDL pthis)
    {
        if (!pthis)
        {
            return MFX_ERR_NULL_PTR;
        }
        return ((IEncTools*)pthis)->Close();
    }

    // Submit info to tool
    inline mfxStatus Submit(mfxHDL pthis, mfxEncToolsTaskParam* submitParam)
    {
        if (!pthis)
        {
            return MFX_ERR_NULL_PTR;
        }
        return ((IEncTools*)pthis)->Submit(submitParam);
    }

    // Query info from tool
    inline mfxStatus Query(mfxHDL pthis, mfxEncToolsTaskParam* queryParam, mfxU32 timeOut)
    {
        if (!pthis)
        {
            return MFX_ERR_NULL_PTR;
        }
        return ((IEncTools*)pthis)->Query(queryParam, timeOut);
    }

    // Query info from tool
    inline mfxStatus Discard(mfxHDL pthis, mfxU32 displayOrder)
    {
        if (!pthis)
        {
            return MFX_ERR_NULL_PTR;
        }
        return ((IEncTools*)pthis)->Discard(displayOrder);
    }
    inline mfxStatus GetDelayInFrames(mfxHDL pthis, mfxExtEncToolsConfig* config, mfxEncToolsCtrl* ctrl, mfxU32 *numFrames)
    {
        if (!pthis)
        {
            return MFX_ERR_NULL_PTR;
        }
        return ((IEncTools*)pthis)->GetDelayInFrames(config, ctrl, numFrames);
    }

}

#endif
