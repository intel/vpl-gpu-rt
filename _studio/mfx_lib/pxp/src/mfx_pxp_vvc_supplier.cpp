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

#include "umc_defs.h"
#ifdef MFX_ENABLE_VVC_VIDEO_DECODE

#include "mfx_pxp_vvc_supplier.h"

#if defined(MFX_ENABLE_PXP)

namespace UMC_VVC_DECODER
{

PXPVVCSupplier::PXPVVCSupplier()
{
}

UMC::Status PXPVVCSupplier::Init(UMC::BaseCodecParams* pInit)
{
    UMC::Status umcRes = VVCDecoder::Init(pInit);
    if (m_va->GetProtectedVA())
    {
        static_cast<PXPNALUnitSplitter_VVC*>(m_splitter.get())->SetVA(m_va);
    }
    return umcRes;
}

UMC::Status PXPVVCSupplier::AddOneFrame(UMC::MediaData * pSource)
{
    if (m_va->GetProtectedVA())
    {
        if (!m_lastSlice && pSource)
        {
            UMC::Status umcRes = UpdatePXPParams(pSource);
            if(umcRes != UMC::UMC_OK)
                return umcRes;
        }
    }
    return VVCDecoder::AddOneFrame(pSource);
}

UMC::Status PXPVVCSupplier::UpdatePXPParams(UMC::MediaData const* pSource)
{
    UMC_CHECK(pSource != nullptr, UMC::UMC_ERR_INVALID_PARAMS);

    static_cast<UMC::PXPVA*>(m_va->GetProtectedVA())->SetPXPParams(nullptr);
    static_cast<UMC::PXPVA*>(m_va->GetProtectedVA())->m_curSegment = 0;

    mfxPXPCtxHDL curPXPCtxHdl = static_cast<mfxPXPCtxHDL>(static_cast<UMC::PXPVA*>(m_va->GetProtectedVA())->GetPXPCtxHdl());

    auto mapptr = std::find_if(curPXPCtxHdl->decodeParamMapHdl, curPXPCtxHdl->decodeParamMapHdl + curPXPCtxHdl->decodeParamMapCnt,
        [key = (uint8_t*)pSource->GetBufferPointer()](const mfxDecodeParamMap & data)
    { return data.pMfxBitstream->Data == key; }
    );
    UMC_CHECK(mapptr != curPXPCtxHdl->decodeParamMapHdl + curPXPCtxHdl->decodeParamMapCnt, UMC::UMC_ERR_FAILED);

    static_cast<UMC::PXPVA*>(m_va->GetProtectedVA())->SetPXPParams(mapptr->pPXPParams);

    UMC_CHECK(static_cast<UMC::PXPVA*>(m_va->GetProtectedVA())->GetPXPParams() != nullptr, UMC::UMC_ERR_INVALID_PARAMS);

    return UMC::UMC_OK;
}

}
#endif // MFX_ENABLE_PXP
#endif // MFX_ENABLE_VVC_VIDEO_DECODE
