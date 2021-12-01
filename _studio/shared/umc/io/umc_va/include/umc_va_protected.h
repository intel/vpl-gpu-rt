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

#ifndef __UMC_VA_PROTECTED_H
#define __UMC_VA_PROTECTED_H

#pragma once

#include "mfx_common.h"

#if defined(MFX_ENABLE_PROTECT)

namespace UMC
{
enum ProtectionType
{
    PROTECTION_TYPE_PXP,
    PROTECTION_TYPE_PAVP
};

class ProtectedVA
{
public:

    ProtectedVA(mfxU16 p = 0) : m_bs(nullptr) { (void)p; };
    virtual mfxBitstream* GetBitstream() { return m_bs; }
    virtual void SetBitstream(mfxBitstream* bs) { m_bs = bs; }

    mfxU32 GetType() 
    { 
#if defined(MFX_ENABLE_PXP)
        return PROTECTION_TYPE_PXP;
#else
        return PROTECTION_TYPE_PAVP;
#endif
    }


protected:
    mfxBitstream *m_bs;

};

}

#endif // MFX_ENABLE_PROTECT
#endif //__UMC_VA_PROTECTED_H
