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


#include "mfxdefs.h"
#ifdef MFX_ENABLE_KERNELS
#include "cmrt_cross_platform.h"
#include "genx_fcopy_gen12lp_isa.h"

inline mfxStatus InitVppCm(CmDevice* &cmDevice, CmProgram* &cmProgram, CmKernel* &cmKernel, CmQueue* &cmQueue, VideoCORE* &core)
{
    if(cmDevice)
    {
        int res = CM_FAILURE;
        if (nullptr == cmProgram)
        {
            eMFXHWType m_platform = core->GetHWType();
            switch (m_platform)
            {
            case MFX_HW_TGL_LP:
            case MFX_HW_DG1:
            case MFX_HW_RKL:
            case MFX_HW_ADL_S:
            case MFX_HW_ADL_P:
            case MFX_HW_ADL_N:
                res = cmDevice->LoadProgram((void*)genx_fcopy_gen12lp,sizeof(genx_fcopy_gen12lp),cmProgram,"nojitter");
                break;
            default:
                res = CM_FAILURE;
                break;
            }
            MFX_CHECK(res == CM_SUCCESS, MFX_ERR_DEVICE_FAILED);
        }

        if (NULL == cmKernel)
        {
            // MbCopyFieLd copies TOP or BOTTOM field from inSurf to OutSurf
            res = cmDevice->CreateKernel(cmProgram, CM_KERNEL_FUNCTION(MbCopyFieLd), cmKernel);
            MFX_CHECK(res == CM_SUCCESS, MFX_ERR_DEVICE_FAILED);
        }

        if (NULL == cmQueue)
        {
            res = cmDevice->CreateQueue(cmQueue);
            MFX_CHECK(res == CM_SUCCESS, MFX_ERR_DEVICE_FAILED);
        }
    }
    return MFX_ERR_NONE;
}
#endif
