// Copyright (c) 2008-2021 Intel Corporation
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

#include "mfxstructures-int.h"

#define FUNCTION_DEPRECATED_IMPL(component, func_name, formal_param_list) \
mfxStatus APIImpl_MFXVideo##component##_##func_name formal_param_list \
{ \
    return MFX_ERR_UNSUPPORTED; \
}

FUNCTION_DEPRECATED_IMPL(ENC, QueryIOSurf,       (mfxSession /*session*/, mfxVideoParam * /*par*/, mfxFrameAllocRequest * /*request*/))
FUNCTION_DEPRECATED_IMPL(ENC, Init,              (mfxSession /*session*/, mfxVideoParam * /*par*/))
FUNCTION_DEPRECATED_IMPL(ENC, Close,             (mfxSession /*session*/))
FUNCTION_DEPRECATED_IMPL(ENC, Reset,             (mfxSession /*session*/, mfxVideoParam * /*par*/))
FUNCTION_DEPRECATED_IMPL(ENC, Query,             (mfxSession /*session*/, mfxVideoParam * /*in*/, mfxVideoParam * /*out*/))
FUNCTION_DEPRECATED_IMPL(ENC, GetVideoParam,     (mfxSession /*session*/, mfxVideoParam * /*par*/))
FUNCTION_DEPRECATED_IMPL(ENC, ProcessFrameAsync, (mfxSession /*session*/, mfxENCInput * /*in*/, mfxENCOutput * /*out*/, mfxSyncPoint * /*syncp*/))
#undef FUNCTION_DEPRECATED_IMPL
