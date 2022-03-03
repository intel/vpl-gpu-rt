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

// mfx_enctools.cpp : Defines the enctools interfaces for the RT to call.

#include "mfx_enctools_stub.h"
#include "mfx_enctools_loader.h"

mfxEncTools* MFXVideoENCODE_CreateEncTools(const mfxVideoParam&)
{
    std::unique_ptr<mfxEncTools> et(new mfxEncTools);

    et->Context = MFXNewEncTools(mfxEncToolsLoader::LoadEncToolsLib());
    if (!et->Context)
    {
        et->Context = new EncToolsStub;
    }

    et->Init = EncToolsFuncs::Init;
    et->Reset = EncToolsFuncs::Reset;
    et->Close = EncToolsFuncs::Close;
    et->Submit = EncToolsFuncs::Submit;
    et->Query = EncToolsFuncs::Query;
    et->Discard = EncToolsFuncs::Discard;
    et->GetSupportedConfig = EncToolsFuncs::GetSupportedConfig;
    et->GetActiveConfig = EncToolsFuncs::GetActiveConfig;
    et->GetDelayInFrames = EncToolsFuncs::GetDelayInFrames;

    return et.release();
}

void  MFX_CDECL MFXVideoENCODE_DestroyEncTools(mfxEncTools* et)
{
    if (et != nullptr && et->Context != nullptr)
    {
        void* etmodule = ((IEncTools*)et->Context)->m_etModule;
        delete (IEncTools*)et->Context;
        delete et;
        et = nullptr;

        mfxEncToolsLoader::UnLoadEncToolsLib(&etmodule);
    }
}
