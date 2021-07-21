// Copyright (c) 2021 Intel Corporation
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

#include "mfx_vpp_helper.h"
#include "mfx_task.h"
#include "mfx_vpp_main.h"

#if defined (MFX_ENABLE_VPP_HELPER)

MfxVppHelper::MfxVppHelper(VideoCORE* core, mfxStatus* mfxRes) : m_core(core)
{
    if (m_pVpp)
        DestroyVpp();

    m_pVpp = new VideoVPPMain(m_core, mfxRes);

    if (MFX_ERR_NONE != *mfxRes)
    {
        delete m_pVpp;
        m_pVpp = nullptr;
    }
}

mfxStatus MfxVppHelper::Init(mfxVideoParam* param)
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(param);

    if (m_bInitialized)
        return MFX_ERR_NONE;

    mfxRes = CreateVpp(param);
    MFX_CHECK_STS(mfxRes);

    m_bInitialized = true;

    return mfxRes;
}

mfxStatus MfxVppHelper::Reset(mfxVideoParam* /*param*/)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MfxVppHelper::Close()
{
    if (!m_bInitialized)
    {
        return MFX_ERR_NONE;
    }

    DestroyVpp();
    m_bInitialized = false;
    return MFX_ERR_NONE;
}

mfxStatus MfxVppHelper::Submit(mfxFrameSurface1* input, mfxFrameSurface1* output)
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(input);
    MFX_CHECK(m_bInitialized, MFX_ERR_NOT_INITIALIZED);

    mfxFrameSurface1* vppout = output ? output : &m_dsSurface;

    MFX_ENTRY_POINT entryPoint[2] = {};
    mfxU32 numEntryPoints = 2;

    mfxRes = m_pVpp->VppFrameCheck(input, vppout, nullptr, m_entryPoint, numEntryPoints);
    MFX_CHECK_STS(mfxRes);

    if (m_entryPoint[0].pRoutine)
    {
        mfxRes = m_entryPoint[0].pRoutine(m_entryPoint[0].pState, m_entryPoint[0].pParam, 0, 0);
        if (mfxRes != MFX_TASK_DONE && mfxRes != MFX_TASK_BUSY)
            return mfxRes;
    }

    if (m_entryPoint[1].pRoutine)
    {
        mfxRes = m_entryPoint[1].pRoutine(m_entryPoint[1].pState, m_entryPoint[1].pParam, 0, 0);
        if (mfxRes != MFX_TASK_DONE && mfxRes != MFX_TASK_BUSY)
            return mfxRes;
    }

    return mfxRes;
}

mfxStatus MfxVppHelper::CreateVpp(mfxVideoParam* param)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    mfxFrameAllocRequest vppRequest[2] = {};     // [0] - in, [1] - out
    mfxRes = m_pVpp->QueryIOSurf(m_core, param, vppRequest);
    MFX_CHECK_STS(mfxRes);

    vppRequest[1].Type &= ~MFX_MEMTYPE_EXTERNAL_FRAME;
    vppRequest[1].Type |= MFX_MEMTYPE_INTERNAL_FRAME;
    vppRequest[1].Type |= MFX_MEMTYPE_FROM_VPPOUT;

    mfxRes = m_core->AllocFrames(&vppRequest[1], &m_dsResponse, false);
    MFX_CHECK_STS(mfxRes);

    m_dsSurface.Info         = vppRequest[1].Info;
    m_dsSurface.Data.MemId   = m_dsResponse.mids[0];
    m_dsSurface.Data.MemType = vppRequest[1].Type;

    //m_scalingConfig.Header.BufferId     = MFX_EXTBUFF_VPP_SCALING;
    //m_scalingConfig.Header.BufferSz     = sizeof(mfxExtVPPScaling);
    //m_scalingConfig.ScalingMode         = MFX_SCALING_MODE_LOWPOWER;
    //m_scalingConfig.InterpolationMethod = MFX_INTERPOLATION_NEAREST_NEIGHBOR;

    //m_extBuffer = (mfxExtBuffer*)&m_scalingConfig;

    //vppParams.NumExtParam = 1;
    //vppParams.ExtParam    = &extBuffer;

    mfxRes = m_pVpp->Init(param);

    m_core->LockFrame(m_dsSurface.Data.MemId, &m_dsSurface.Data);

    return mfxRes;
}

void MfxVppHelper::DestroyVpp()
{
    m_core->UnlockFrame(m_dsSurface.Data.MemId, &m_dsSurface.Data);
    m_core->FreeFrames(&m_dsResponse, false);

    if (m_pVpp)
    {
        delete m_pVpp;
        m_pVpp = nullptr;
    }
}

mfxFrameSurface1* MfxVppHelper::GetOutptSurface()
{
    return &m_dsSurface;
}

#endif
