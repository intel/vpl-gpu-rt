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
#include "mfx_pxp_video_accelerator_vaapi.h"

#if defined(MFX_ENABLE_PXP)

#include "mfx_pxp_video_accelerator.h"

PXPLinuxVideoAccelerator::PXPLinuxVideoAccelerator()
    : m_PXPCtxHdl(nullptr)
{
}

PXPLinuxVideoAccelerator::~PXPLinuxVideoAccelerator()
{
    m_PXPCtxHdl = nullptr;
    Close();
}

UMC::Status PXPLinuxVideoAccelerator::Init(UMC::VideoAcceleratorParams* pInfo)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "PXPLinuxVideoAccelerator::Init");
    UMC::Status         umcRes = UMC::UMC_OK;

    if(pInfo->m_pPXPCtxHdl)
    {
        m_PXPCtxHdl = reinterpret_cast<mfxPXPCtxHDL>(pInfo->m_pPXPCtxHdl);
    }

    umcRes = LinuxVideoAccelerator::Init(pInfo);

    UMC::LinuxVideoAcceleratorParams* pParams = DynamicCast<UMC::LinuxVideoAcceleratorParams>(pInfo);
    UMC_CHECK(pParams != nullptr, UMC::UMC_ERR_NULL_PTR);

    if(pParams->m_pContext)
    {
        m_PXPCtxHdl->secureDecodeCfg.ContextId = *(pParams->m_pContext);
        m_protectedVA = std::make_shared<UMC::PXPVA>(pInfo->m_pPXPCtxHdl);
    }

    return umcRes;
}

UMC::Status PXPLinuxVideoAccelerator::SetAttributes(VAProfile va_profile, UMC::LinuxVideoAcceleratorParams* pParams, VAConfigAttrib *attribute, int32_t *attribsNumber)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "PXPLinuxVideoAccelerator::SetAttributes");

    UMC_CHECK(pParams != nullptr, UMC::UMC_ERR_INVALID_PARAMS);
    UMC_CHECK(attribute != nullptr, UMC::UMC_ERR_INVALID_PARAMS);
    UMC_CHECK(attribsNumber != nullptr, UMC::UMC_ERR_INVALID_PARAMS);
    UMC_CHECK((*attribsNumber >= 0 && *attribsNumber < UMC_VA_LINUX_ATTRIB_SIZE), UMC::UMC_ERR_INVALID_PARAMS);

    // Check PXP handle and secure decode context handle
    UMC_CHECK(m_PXPCtxHdl != nullptr, UMC::UMC_ERR_INVALID_PARAMS);

    UMC::Status umcRes = LinuxVideoAccelerator::SetAttributes(va_profile, pParams, attribute, attribsNumber);

    if (UMC::UMC_OK == umcRes)
    {
        //check pxp capablity by attribute[3], and set pxp attribute to attribute[*attribsNumber]
        VAConfigAttrib *pxpAttrib = reinterpret_cast<VAConfigAttrib *>(m_PXPCtxHdl->secureDecodeCfg.pxpAttributesHdl);

        if (pxpAttrib 
        && (attribute[3].value & 
            (VA_ENCRYPTION_TYPE_SUBSAMPLE_CTR | VA_ENCRYPTION_TYPE_SUBSAMPLE_CBC | VA_ENCRYPTION_TYPE_FULLSAMPLE_CTR | VA_ENCRYPTION_TYPE_FULLSAMPLE_CBC))
        )
        {
            attribute[*attribsNumber].type = pxpAttrib->type;
            attribute[*attribsNumber].value = pxpAttrib->value;
            (*attribsNumber)++;
        }
    }

    return umcRes;
}

UMC::Status PXPLinuxVideoAccelerator::Execute()
{
    UMC_CHECK(m_PXPCtxHdl != nullptr, UMC::UMC_ERR_INVALID_PARAMS);
    UMC_CHECK(m_PXPCtxHdl->decodeParamMapHdl != nullptr, UMC::UMC_ERR_INVALID_PARAMS);

    mfxBitstream *curBS = GetProtectedVA()->GetBitstream();
    UMC_CHECK(curBS != nullptr, UMC::UMC_ERR_NOT_INITIALIZED);

    auto mapptr = std::find_if(m_PXPCtxHdl->decodeParamMapHdl, m_PXPCtxHdl->decodeParamMapHdl + m_PXPCtxHdl->decodeParamMapCnt,
        [key = curBS->Data](const mfxDecodeParamMap & data)
        { return data.pMfxBitstream->Data == key; }
    );
    UMC_CHECK(mapptr != m_PXPCtxHdl->decodeParamMapHdl + m_PXPCtxHdl->decodeParamMapCnt, UMC::UMC_ERR_FAILED);

    if(mapptr->pPXPParams)
    {
        UMC::UMCVACompBuffer *encParamBuf;
        VAEncryptionParameters* pEncParams = (VAEncryptionParameters*)UMC::LinuxVideoAccelerator::GetCompBuffer(VAEncryptionParameterBufferType, &encParamBuf, sizeof(VAEncryptionParameters), 0);
        UMC_CHECK(pEncParams != nullptr, UMC::UMC_ERR_ALLOC);

        VAEncryptionParameters *encParams = reinterpret_cast<VAEncryptionParameters*>(mapptr->pPXPParams);
        *pEncParams = *encParams;
        encParamBuf->SetDataSize(sizeof(VAEncryptionParameters));
    }

    return UMC::LinuxVideoAccelerator::Execute();
}
#endif // MFX_ENABLE_PXP
