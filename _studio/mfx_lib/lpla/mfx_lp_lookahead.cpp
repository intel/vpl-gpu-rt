// Copyright (c) 2014-2020 Intel Corporation
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

#include "mfx_lp_lookahead.h"
#include "mfx_feature_blocks_base.h"
#include "hevcehw_base_data.h"
#include "hevcehw_disp.h"
#include "mfx_task.h"
#include "mfx_vpp_main.h"

#if defined (MFX_ENABLE_LP_LOOKAHEAD)

#ifndef ALIGN16
#define ALIGN16(value)                     (((value + 15) >> 4) << 4)
#endif

mfxStatus MfxLpLookAhead::Init(mfxVideoParam* param)
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(param);

    if (m_bInitialized)
        return MFX_ERR_NONE;

    if (NeedDownScaling(*param))
    {
        mfxRes = CreateVpp(*param);
        MFX_CHECK_STS(mfxRes);
    }

    m_pEnc = HEVCEHW::Create(*m_core, mfxRes);
    MFX_CHECK_NULL_PTR1(m_pEnc);

    mfxExtLplaParam* extBufLPLA = HEVCEHW::ExtBuffer::Get(*param);
    if (extBufLPLA)
    {
        extBufLPLA->GopRefDist = param->mfx.GopRefDist; // Save GopRefDist of encode pass because it will be overwritten
    }

    // following configuration comes from HW recommendation
    mfxVideoParam par         = *param;
    par.AsyncDepth            = 1;
    par.mfx.CodecId           = MFX_CODEC_HEVC;
    par.mfx.LowPower          = MFX_CODINGOPTION_ON;
    par.mfx.NumRefFrame       = 1;
    par.mfx.TargetUsage       = 7;
    par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    par.mfx.CodecProfile      = MFX_PROFILE_HEVC_MAIN;
    par.mfx.CodecLevel        = MFX_LEVEL_HEVC_52;
    par.mfx.QPI               = 30;
    par.mfx.QPP               = 32;
    par.mfx.QPB               = 32;
    par.mfx.NumSlice          = 1;
    par.mfx.GopRefDist        = 1;

    if (m_bNeedDownscale)
    {
        par.mfx.FrameInfo.CropX = 0;
        par.mfx.FrameInfo.CropY = 0;
        par.mfx.FrameInfo.CropW = m_dstWidth;
        par.mfx.FrameInfo.CropH = m_dstHeight;
        par.mfx.FrameInfo.Width = ALIGN16(m_dstWidth);
        par.mfx.FrameInfo.Height= ALIGN16(m_dstHeight);
    }

    //create the bitstream buffer
    memset(&m_bitstream, 0, sizeof(mfxBitstream));
    mfxU32 bufferSize = param->mfx.FrameInfo.Width * param->mfx.FrameInfo.Height * 3/2;
    m_bitstream.Data = new mfxU8[bufferSize];
    m_bitstream.MaxLength = bufferSize;

    mfxRes = m_pEnc->Init(&par);
    m_bInitialized = true;

    return mfxRes;
}

mfxStatus MfxLpLookAhead::Reset(mfxVideoParam* param)
{
    (void*)param;
    // TODO: will implement it later
    m_bNeedDownscale = false;
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MfxLpLookAhead::Close()
{
    if (!m_bInitialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if (m_bitstream.Data)
    {
        delete[]  m_bitstream.Data;
        m_bitstream.Data = nullptr;
    }
    if (m_pEnc)
    {
        delete m_pEnc;
        m_pEnc = nullptr;
    }

    DestroyVpp();
    m_bNeedDownscale = false;
    m_bInitialized = false;
    return MFX_ERR_NONE;
}

mfxStatus MfxLpLookAhead::Submit(mfxFrameSurface1 * surface)
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(surface);

    if (!m_bInitialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if (!m_taskSubmitted)
    {
        if (m_bNeedDownscale)
        {
            MFX_ENTRY_POINT entryPoint[2] = {};
            mfxU32 numEntryPoints = 1;

            mfxRes = m_pVpp->VppFrameCheck(surface, &m_dsSurface, nullptr, entryPoint, numEntryPoints);
            MFX_CHECK_STS(mfxRes);

            if (entryPoint[0].pRoutine)
            {
                mfxRes = entryPoint[0].pRoutine(entryPoint[0].pState, entryPoint[0].pParam, 0, 0);
                if (mfxRes != MFX_TASK_DONE && mfxRes != MFX_TASK_BUSY)
                    return mfxRes;
            }

            if (entryPoint[1].pRoutine)
            {
                mfxRes = entryPoint[1].pRoutine(entryPoint[1].pState, entryPoint[1].pParam, 0, 0);
                if (mfxRes != MFX_TASK_DONE && mfxRes != MFX_TASK_BUSY)
                    return mfxRes;
            }
        }

        m_bitstream.DataLength = 0;
        m_bitstream.DataOffset = 0;

        mfxFrameSurface1 *reordered_surface = nullptr;
        mfxEncodeInternalParams internal_params;

        mfxRes = m_pEnc->EncodeFrameCheck(
            nullptr,
            m_bNeedDownscale ? &m_dsSurface : surface,
            &m_bitstream,
            &reordered_surface,
            &internal_params,
            &m_entryPoint);
        MFX_CHECK_STS(mfxRes);

        m_taskSubmitted = true;
    }

    if (m_taskSubmitted)
        mfxRes = m_entryPoint.pRoutine(m_entryPoint.pState, m_entryPoint.pParam, 0, 0);

    if (mfxRes == MFX_ERR_NONE)
    {
        m_taskSubmitted = false;
        auto& s_task = *(MfxFeatureBlocks::StorageRW*)m_entryPoint.pParam;
        auto& task = HEVCEHW::Base::Task::Common::Get(s_task);

        if (task.LplaStatus.ValidInfo)
        {
            m_lplastatus.push_back(task.LplaStatus);
        }
    }

    return mfxRes;
}

mfxStatus MfxLpLookAhead::Query(mfxLplastatus& laStatus)
{
    if (!m_bInitialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if (m_lplastatus.empty())
    {
        return MFX_ERR_NOT_FOUND;
    }

    laStatus = m_lplastatus.front();
    m_lplastatus.pop_front();

    return MFX_ERR_NONE;
}

mfxStatus MfxLpLookAhead::SetStatus(mfxLplastatus *laStatus)
{
    MFX_CHECK_NULL_PTR1(laStatus);
    m_lplastatus.insert(m_lplastatus.begin(), *laStatus);
    return MFX_ERR_NONE;
}

bool MfxLpLookAhead::NeedDownScaling(const mfxVideoParam& par)
{
    if (par.mfx.FrameInfo.CropW >= m_minDsWidth || par.mfx.FrameInfo.CropH >= m_minDsHeight)
    {
        m_bNeedDownscale = true;
        m_dstWidth  = par.mfx.FrameInfo.CropW/m_dsRatio;
        m_dstHeight = par.mfx.FrameInfo.CropH/m_dsRatio;
    }
    else
        m_bNeedDownscale = false;

    return m_bNeedDownscale;
}

mfxStatus MfxLpLookAhead::CreateVpp(const mfxVideoParam& par)
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    if (m_pVpp)
        DestroyVpp();

    m_pVpp = new VideoVPPMain(m_core, &mfxRes);
    if (MFX_ERR_NONE != mfxRes)
    {
        delete m_pVpp;
        m_pVpp = nullptr;
        return mfxRes;
    }

    mfxVideoParam vppParams = {};
    vppParams.AsyncDepth    = 1;
    vppParams.IOPattern     = par.IOPattern | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    vppParams.vpp.In        = par.mfx.FrameInfo;
    vppParams.vpp.Out       = par.mfx.FrameInfo;
    vppParams.vpp.Out.CropX = 0;
    vppParams.vpp.Out.CropY = 0;
    vppParams.vpp.Out.CropW = m_dstWidth;
    vppParams.vpp.Out.CropH = m_dstHeight;
    vppParams.vpp.Out.Width = ALIGN16(m_dstWidth);
    vppParams.vpp.Out.Height= ALIGN16(m_dstHeight);

    // Query number of required surfaces for VPP
    mfxFrameAllocRequest vppRequest[2] = {};     // [0] - in, [1] - out
    mfxRes = m_pVpp->QueryIOSurf(m_core, &vppParams, vppRequest);
    MFX_CHECK_STS(mfxRes);

    vppRequest[1].Type &= ~MFX_MEMTYPE_EXTERNAL_FRAME;
    vppRequest[1].Type |= MFX_MEMTYPE_INTERNAL_FRAME;
    vppRequest[1].Type |= MFX_MEMTYPE_FROM_VPPOUT;

    mfxRes = m_core->AllocFrames(&vppRequest[1], &m_dsResponse, false);
    MFX_CHECK_STS(mfxRes);

    m_dsSurface.Info         = vppRequest[1].Info;
    m_dsSurface.Data.MemId   = m_dsResponse.mids[0];
    m_dsSurface.Data.MemType = vppRequest[1].Type;

    m_scalingConfig.Header.BufferId     = MFX_EXTBUFF_VPP_SCALING;
    m_scalingConfig.Header.BufferSz     = sizeof(mfxExtVPPScaling);
    m_scalingConfig.ScalingMode         = MFX_SCALING_MODE_LOWPOWER;
    m_scalingConfig.InterpolationMethod = MFX_INTERPOLATION_NEAREST_NEIGHBOR;

    m_extBuffer = &m_scalingConfig.Header;

    vppParams.NumExtParam = 1;
    vppParams.ExtParam    = &m_extBuffer;

    mfxRes = m_pVpp->Init(&vppParams);

    return mfxRes;
}
void MfxLpLookAhead::DestroyVpp()
{
    if (m_bNeedDownscale)
        m_core->FreeFrames(&m_dsResponse, false);

    if (m_pVpp)
    {
        delete m_pVpp;
        m_pVpp = nullptr;
    }
}
#endif