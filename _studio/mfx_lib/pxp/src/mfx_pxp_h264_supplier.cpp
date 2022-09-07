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

#include "mfx_pxp_h264_supplier.h"

#if defined(MFX_ENABLE_PXP)

namespace UMC
{

PXPH264Supplier::PXPH264Supplier()
{
}

Status PXPH264Supplier::Init(VideoDecoderParams* pInit)
{
    Status umcRes = VATaskSupplier::Init(pInit);
    if (m_va->GetProtectedVA())
    {
        static_cast<PXPNALUnitSplitter*>(m_pNALSplitter.get())->SetVA(m_va);
    }
    return umcRes;
}

Status PXPH264Supplier::AddOneFrame(MediaData * pSource)
{
    if (m_va->GetProtectedVA())
    {
        if (!m_pLastSlice && pSource)
            UMC_CHECK_STATUS(UpdatePXPParams(pSource));
    }
    return TaskSupplier::AddOneFrame(pSource);
}

Status PXPH264Supplier::UpdatePXPParams(MediaData const* pSource)
{
    UMC_CHECK(pSource != nullptr, UMC_ERR_INVALID_PARAMS);

    static_cast<PXPVA*>(m_va->GetProtectedVA())->SetPXPParams(nullptr);
    static_cast<PXPVA*>(m_va->GetProtectedVA())->m_curSegment = 0;

    mfxPXPCtxHDL curPXPCtxHdl = static_cast<mfxPXPCtxHDL>(static_cast<PXPVA*>(m_va->GetProtectedVA())->GetPXPCtxHdl());

    auto mapptr = std::find_if(curPXPCtxHdl->decodeParamMapHdl, curPXPCtxHdl->decodeParamMapHdl + curPXPCtxHdl->decodeParamMapCnt,
        [key = (uint8_t*)pSource->GetBufferPointer()](const mfxDecodeParamMap & data)
    { return data.pMfxBitstream->Data == key; }
    );
    UMC_CHECK(mapptr != curPXPCtxHdl->decodeParamMapHdl + curPXPCtxHdl->decodeParamMapCnt, UMC_ERR_FAILED);

    static_cast<PXPVA*>(m_va->GetProtectedVA())->SetPXPParams(mapptr->pPXPParams);

    UMC_CHECK(static_cast<PXPVA*>(m_va->GetProtectedVA())->GetPXPParams() != nullptr, UMC_ERR_INVALID_PARAMS);

    return UMC_OK;
}

bool PXPH264Supplier::ProcessNonPairedField(H264DecoderFrame * pFrame)
{
    if (pFrame)
    {
        H264Slice * pSlice = pFrame->GetAU(0)->GetSlice(0);
        if (pSlice && pSlice->GetSliceHeader()->field_pic_flag)
        {
            // accelerator will submit decode parameter twice to decode top and bottom field data
            // return false to re-use output surface
            // it will allocate new surface when decoded bottom field data
            return false;
        }
    }

    return VATaskSupplier::ProcessNonPairedField(pFrame);
}

}
#endif // MFX_ENABLE_PXP