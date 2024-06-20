// Copyright (c) 2024 Intel Corporation
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

#include "mfxvideo++int.h"
#include "libmfx_core_interface.h"

#include "mfx_vpp_ai_frame_interpolation.h"
#include "mfx_vpp_defs.h"
#include "mfx_vpp_hw.h"

MFXVideoFrameInterpolation::MFXVideoFrameInterpolation() :
	m_inputFwd(),
	m_inputBkwd(),
	m_output(),
	m_memIdBkwd(0),
	m_memIdFwd(0),
	m_enableScd(false),
	m_scdNeedCsc(false),
	m_preWorkCscForFi(false),
	m_postWrokForFi(false),
	m_rgbBkwd(),
	m_rgbfwd(),
	m_rgbFiOut(),
	m_fiOut()
{
}

bool MFXVideoFrameInterpolation::IsScdSupportedFormat(mfxU32 fourcc)
{
	if (fourcc == MFX_FOURCC_NV12 ||
		fourcc == MFX_FOURCC_RGB4 ||
		fourcc == MFX_FOURCC_BGR4)
	{
		return true;
	}
	return false;
}

MFXVideoFrameInterpolation::~MFXVideoFrameInterpolation()
{
	m_core->FreeFrames(&m_responseIn);
	m_core->FreeFrames(&m_responseOut);
	if (m_scdNeedCsc)
	{
		m_core->FreeFrames(&m_scdAllocation);
	
	}
	m_core->FreeFrames(&m_rgbSurfForFiOut);
	m_core->FreeFrames(&m_rgbSurfForFiIn);
	m_core->FreeFrames(&m_outSurfForFi);
}

mfxStatus MFXVideoFrameInterpolation::InitScd(const mfxFrameInfo& frameInfo)
{
	mfxStatus sts = MFX_ERR_NONE;

	if (IsScdSupportedFormat(frameInfo.FourCC))
	{
		m_enableScd = true;
	}

	if (!m_enableScd)
		return MFX_ERR_NONE;
	MFX_CHECK_STS(m_scd.Init(frameInfo.Width, frameInfo.Height, frameInfo.Width, MFX_PICSTRUCT_PROGRESSIVE, false));
	m_scd.SetGoPSize(ns_asc::Immediate_GoP);

	if (frameInfo.FourCC != MFX_FOURCC_NV12)
	{
		m_scdNeedCsc = true;

		m_vppForScd.reset(new MfxVppHelper(m_core, &sts));
		MFX_CHECK_STS(sts);

		mfxVideoParam vppParams = {};
		vppParams.AsyncDepth = 1;
		vppParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
		vppParams.vpp.In = frameInfo;
		vppParams.vpp.Out = frameInfo;
		//vppParams.vpp.Out.CropX = 0;
		//vppParams.vpp.Out.CropY = 0;
		//vppParams.vpp.Out.CropW = m_scd.Get_asc_subsampling_width();
		//vppParams.vpp.Out.CropH = m_scd.Get_asc_subsampling_height();
		//vppParams.vpp.Out.Width = m_scd.Get_asc_subsampling_width();
		//vppParams.vpp.Out.Height = m_scd.Get_asc_subsampling_height();
		vppParams.vpp.Out.FourCC = MFX_FOURCC_NV12;

		//mfxExtVPPScaling m_scalingConfig    = {};
		//m_scalingConfig.Header.BufferId     = MFX_EXTBUFF_VPP_SCALING;
		//m_scalingConfig.Header.BufferSz     = sizeof(mfxExtVPPScaling);
		//m_scalingConfig.ScalingMode         = MFX_SCALING_MODE_INTEL_GEN_COMPUTE;
		////m_scalingConfig.InterpolationMethod = MFX_INTERPOLATION_NEAREST_NEIGHBOR;

		//mfxExtBuffer* extBuffer = &m_scalingConfig.Header;
		//vppParams.NumExtParam = 1;
		//vppParams.ExtParam = &extBuffer;

		sts = m_vppForScd->Init(&vppParams);
		MFX_CHECK_STS(sts);

		mfxFrameAllocRequest request = {};
		request.Info = vppParams.vpp.Out;
		request.Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_VPPOUT;
		request.NumFrameMin = 1;
		request.NumFrameSuggested = 1;
		MFX_CHECK_STS(m_core->AllocFrames(&request, &m_scdAllocation));

		m_scdImage.Info = vppParams.vpp.Out;
		m_scdImage = MakeSurface(m_scdImage.Info, m_scdAllocation.mids[0]);
	}
	

	return MFX_ERR_NONE;
}

mfxStatus MFXVideoFrameInterpolation::Init(VideoCORE* core, mfxFrameInfo& inInfo, mfxFrameInfo& outInfo, mfxU16 IOPattern)
{
	m_core = core;
	MFX_CHECK_NULL_PTR1(m_core);

	mfxFrameAllocRequest request = {};
	request.Info = outInfo;
	request.Type = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_INTERNAL_FRAME;
	request.NumFrameMin = request.NumFrameSuggested = 2;
	MFX_CHECK_STS(m_core->AllocFrames(&request, &m_responseIn));
	request.NumFrameMin = request.NumFrameSuggested = 3;
	MFX_CHECK_STS(m_core->AllocFrames(&request, &m_responseOut));
	m_memIdFwd = m_responseIn.mids[1];

	m_IOPattern            = IOPattern;
	m_frcRational[VPP_IN]  = {inInfo.FrameRateExtN, inInfo.FrameRateExtD};
	m_frcRational[VPP_OUT] = { outInfo.FrameRateExtN, outInfo.FrameRateExtD };

	mfxF64 inRate = 100 * (((mfxF64)m_frcRational[VPP_IN].FrameRateExtN / (mfxF64)m_frcRational[VPP_IN].FrameRateExtD));
	mfxF64 outRate = 100 * (((mfxF64)m_frcRational[VPP_OUT].FrameRateExtN / (mfxF64)m_frcRational[VPP_OUT].FrameRateExtD));

	mfxF64 mul = outRate / inRate;
	if (fabs(mul - 4.) < 1e-3)
	{
		m_ratio = ratio_4x;
	}
	else if (fabs(mul - 2.) < 1e-3)
	{
		m_ratio = ratio_2x;
	}
	else
	{
		m_ratio = ratio_unsupported;
		MFX_RETURN(MFX_ERR_UNSUPPORTED);
	}

	m_outStamp = 0;
	m_outTick = (mfxU16)m_ratio;

	MFX_CHECK_STS(InitScd(inInfo));

#if ENABLE_VFI
	D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(core);
	// Init
	xeAIVfiConfig config = {
		outInfo.Width, outInfo.Height,
		(mfxU32)core->GetHWType(),
		pD3d11->GetD3D11Device(),
		pD3d11->GetD3D11DeviceContext(), DXGI_FORMAT_R8G8B8A8_UNORM };
	xeAIVfiStatus xeSts = m_aiIntp.Init(config);
	if (xeSts != XE_AIVFI_SUCCESS)
		MFX_RETURN(MFX_ERR_UNKNOWN);
#endif
	m_preWorkCscForFi = true;
	{
		mfxStatus sts = MFX_ERR_NONE;
		m_vppBeforeFi0.reset(new MfxVppHelper(m_core, &sts));
		MFX_CHECK_STS(sts);
		m_vppBeforeFi1.reset(new MfxVppHelper(m_core, &sts));
		MFX_CHECK_STS(sts);

		mfxVideoParam vppParams     = {};
		vppParams.AsyncDepth        = 1;
		vppParams.IOPattern         = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
		vppParams.vpp.In            = outInfo;
		vppParams.vpp.In.PicStruct  = inInfo.PicStruct;
		vppParams.vpp.Out           = outInfo;
		vppParams.vpp.Out.FourCC    = MFX_FOURCC_BGR4;
		vppParams.vpp.Out.PicStruct = inInfo.PicStruct;

		sts = m_vppBeforeFi0->Init(&vppParams);
		MFX_CHECK_STS(sts);
		sts = m_vppBeforeFi1->Init(&vppParams);
		MFX_CHECK_STS(sts);

		mfxFrameAllocRequest requestRGB = {};
		requestRGB.Info                 = vppParams.vpp.Out;
		requestRGB.Type                 = MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPOUT;
		requestRGB.NumFrameMin          = 2;
		requestRGB.NumFrameSuggested    = 2;
		MFX_CHECK_STS(m_core->AllocFrames(&requestRGB, &m_rgbSurfForFiIn));
		m_rgbBkwd.Info = vppParams.vpp.Out;
		m_rgbBkwd      = MakeSurface(m_rgbBkwd.Info, m_rgbSurfForFiIn.mids[0]);
		m_rgbfwd.Info  = vppParams.vpp.Out;
		m_rgbfwd       = MakeSurface(m_rgbfwd.Info, m_rgbSurfForFiIn.mids[1]);
	}

	m_postWrokForFi = true;
	{
		mfxStatus sts = MFX_ERR_NONE;
		m_vppAfterFi.reset(new MfxVppHelper(m_core, &sts));
		MFX_CHECK_STS(sts);

		mfxVideoParam vppParams    = {};
		vppParams.AsyncDepth       = 1;
		vppParams.IOPattern        = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
		vppParams.vpp.In           = outInfo;
		vppParams.vpp.In.PicStruct = inInfo.PicStruct;
		vppParams.vpp.In.FourCC    = MFX_FOURCC_BGR4;
		vppParams.vpp.Out          = outInfo;

		sts = m_vppAfterFi->Init(&vppParams);
		MFX_CHECK_STS(sts);

		mfxFrameAllocRequest requestFiOut = {};
		requestFiOut.Info                 = vppParams.vpp.In;
		requestFiOut.Type                 = MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPOUT;
		requestFiOut.NumFrameMin          = 1;
		requestFiOut.NumFrameSuggested    = 1;
		MFX_CHECK_STS(m_core->AllocFrames(&requestFiOut, &m_rgbSurfForFiOut));
		m_rgbFiOut.Info = vppParams.vpp.In;
		m_rgbFiOut      = MakeSurface(m_rgbFiOut.Info, m_rgbSurfForFiOut.mids[0]);

		mfxFrameAllocRequest requestFinalOut = {};
		requestFinalOut.Info                 = vppParams.vpp.Out;
		requestFinalOut.Type                 = MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPOUT;
		requestFinalOut.NumFrameMin          = 1;
		requestFinalOut.NumFrameSuggested    = 1;
		MFX_CHECK_STS(m_core->AllocFrames(&requestFinalOut, &m_outSurfForFi));
		m_fiOut.Info = vppParams.vpp.Out;
		m_fiOut      = MakeSurface(m_fiOut.Info, m_outSurfForFi.mids[0]);
	}

	return MFX_ERR_NONE;
}

mfxStatus MFXVideoFrameInterpolation::UpdateTsAndGetStatus(
	mfxFrameSurface1* input,
	mfxFrameSurface1* output,
	mfxStatus* intSts)
{
	if (nullptr == input) return MFX_ERR_MORE_DATA;
	mfxStatus sts = MFX_ERR_NONE;

	if (m_ratio == ratio_2x || m_ratio == ratio_4x)
	{
		if (m_outStamp == 0)
		{
			m_inputBkwd.Info = output->Info;
		}
		else if (m_outStamp == 1)
		{
			m_inputFwd.Info = output->Info;

			*intSts = MFX_ERR_MORE_SURFACE;
		}
		else
		{
			*intSts = MFX_ERR_MORE_SURFACE;
		}
	}
	else
	{
		sts = MFX_ERR_INVALID_VIDEO_PARAM;
	}
	MFX_RETURN(sts);
}

mfxStatus MFXVideoFrameInterpolation::ReturnSurface(mfxU32 taskIndex, mfxFrameSurface1* out, mfxMemId internalVidMemId)
{
	mfxStatus sts = MFX_ERR_NONE;

	while (taskIndex != m_taskQueue.front().first)
	{
	}

	mfxU32 scdDecision = 0;

	task t = m_taskQueue.front();
	//mfxU16 stamp = m_outStamp;
	mfxU16 stamp = t.second;
	if (stamp == 0)
	{
		// m_memIdBkwd = 0 means first frame
		if (!m_memIdBkwd)
		{
			m_memIdBkwd = m_responseIn.mids[0];
		}
		m_inputBkwd = MakeSurface(m_inputBkwd.Info, m_memIdBkwd);
		if (m_IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
		{
			// record internal vpp output vid mem for intp bkw frame
			mfxFrameSurface1 internalSurf = MakeSurface(m_inputBkwd.Info, internalVidMemId);
			sts = m_core->DoFastCopyWrapper(&m_inputBkwd, MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET, &internalSurf, MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET);
			MFX_CHECK_STS(sts);
			// copy vpp internal output to app output
			sts = m_core->DoFastCopyWrapper(out, MFX_MEMTYPE_SYSTEM_MEMORY, &m_inputBkwd, MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET);
			MFX_CHECK_STS(sts);
		}
		else
		{
			if (!internalVidMemId)
			{
				sts = m_core->DoFastCopyWrapper(&m_inputBkwd, MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET, out, MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET);
				MFX_CHECK_STS(sts);
			}
			else
			{
				MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
			}
		}
	}
	else if (stamp == 1)
	{
		m_output[1] = *out;
		if (m_ratio == ratio_4x) 
		{
			m_output[2] = MakeSurface(out->Info, m_responseOut.mids[1]);
			m_output[3] = MakeSurface(out->Info, m_responseOut.mids[2]);
		}

		m_inputFwd = MakeSurface(m_inputFwd.Info, m_memIdFwd);
		if (m_IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
		{
			// record internal vpp output vid mem for intp fwd frame
			mfxFrameSurface1 internalSurf = MakeSurface(m_inputFwd.Info, internalVidMemId);
			sts = m_core->DoFastCopyWrapper(&m_inputFwd, MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET, &internalSurf, MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET);
			MFX_CHECK_STS(sts);

			if (m_enableScd)
			{
				sts = SceneChangeDetect(&m_inputFwd, false, scdDecision);
				MFX_CHECK_STS(sts);
			}
		}
		else
		{
			if (!internalVidMemId)
			{
				sts = m_core->DoFastCopyWrapper(&m_inputFwd, MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET, out, MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET);
				MFX_CHECK_STS(sts);
				if (m_enableScd)
				{
					sts = SceneChangeDetect(&m_inputFwd, false, scdDecision);
					MFX_CHECK_STS(sts);
				}
			}
			else
			{
				MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
			}
		}

		if (!scdDecision)
		{
			// do intp
			sts = DoInterpolation();
		}
		else
		{
			sts = DuplicateFrame();
		}
		MFX_CHECK_STS(sts);
	}
	else if (stamp >= 2)
	{
		if (m_IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
		{
			sts = m_core->DoFastCopyWrapper(out, MFX_MEMTYPE_SYSTEM_MEMORY, &m_output[stamp], MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET);
		}
		else
		{
			sts = m_core->DoFastCopyWrapper(out, MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET, &m_output[stamp], MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET);
		}
		MFX_CHECK_STS(sts);
	}

	//stamp++;
	//if (stamp == m_outTick)
	//{
	//	m_outStamp = 0;
	//	m_inputBkwd.Data.MemId = m_inputFwd.Data.MemId;
	//	std::swap(m_memIdBkwd, m_memIdFwd);
	//	m_inputFwd = {};
	//	memset(m_output, 0, sizeof(m_output));
	//}
	m_taskQueue.pop();
	MFX_RETURN(sts);
}

mfxStatus MFXVideoFrameInterpolation::DuplicateFrame()
{
	mfxStatus sts = MFX_ERR_NONE;
	if (m_ratio == ratio_2x)
	{
		sts = m_core->DoFastCopyWrapper(&m_output[1], MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET, &m_inputBkwd, MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET);
		MFX_CHECK_STS(sts);
	}
	else if (m_ratio == ratio_4x)
	{
		sts = m_core->DoFastCopyWrapper(&m_output[1], MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET, &m_inputBkwd, MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET);
		sts = m_core->DoFastCopyWrapper(&m_output[2], MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET, &m_inputBkwd, MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET);
		sts = m_core->DoFastCopyWrapper(&m_output[3], MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET, &m_inputBkwd, MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET);
		MFX_CHECK_STS(sts);
	}

	return MFX_ERR_NONE;
}

mfxStatus MFXVideoFrameInterpolation::DoInterpolation()
{
	mfxStatus sts = MFX_ERR_NONE;
	if (m_ratio == ratio_2x)
	{
		sts = DoInterpolation2x();
		MFX_CHECK_STS(sts);
	}
	else if (m_ratio == ratio_4x)
	{
		sts = DoInterpolation4x();
		MFX_CHECK_STS(sts);
	}

	return MFX_ERR_NONE;
}

mfxStatus MFXVideoFrameInterpolation::DoInterpolation2x()
{
	mfxStatus sts = InterpolateAi(&m_inputBkwd, &m_inputFwd, &m_output[1]);
	MFX_RETURN(sts);
}

mfxStatus MFXVideoFrameInterpolation::DoInterpolation4x()
{
	MFX_CHECK_STS(InterpolateAi(&m_inputBkwd, &m_inputFwd, &m_output[2]));
	MFX_CHECK_STS(InterpolateAi(&m_inputBkwd, &m_output[2], &m_output[1]));
	MFX_CHECK_STS(InterpolateAi(&m_output[2], &m_inputFwd, &m_output[3]));

	return MFX_ERR_NONE;
}

mfxStatus MFXVideoFrameInterpolation::InterpolateAi(mfxFrameSurface1* bwd, mfxFrameSurface1* fwd, mfxFrameSurface1* out)
{
	mfxFrameSurface1 bkwTempSurface = *bwd, fwdTempSurface = *fwd, outTempSurface = *out;
	if (m_preWorkCscForFi)
	{
		MFX_CHECK_STS(m_vppBeforeFi0->Submit(bwd, &m_rgbBkwd));
		MFX_CHECK_STS(m_vppBeforeFi1->Submit(fwd, &m_rgbfwd));

		bkwTempSurface = m_rgbBkwd;
		fwdTempSurface = m_rgbfwd;
	}
	if (m_postWrokForFi)
	{
		outTempSurface = m_rgbFiOut;
	}

	bkwTempSurface.Data.MemType = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
	fwdTempSurface.Data.MemType = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
	outTempSurface.Data.MemType = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET;

	clear_frame_data(bkwTempSurface.Data);
	clear_frame_data(fwdTempSurface.Data);
	clear_frame_data(outTempSurface.Data);
	mfxHDLPair handle_pair_bkw, handle_pair_fwd, handle_pair_out;

	MFX_CHECK_STS(m_core->GetFrameHDL(bkwTempSurface, handle_pair_bkw));
	MFX_CHECK_STS(m_core->GetFrameHDL(fwdTempSurface, handle_pair_fwd));
	MFX_CHECK_STS(m_core->GetFrameHDL(outTempSurface, handle_pair_out));

#if ENABLE_VFI
	ID3D11Texture2D* bkwFrame = reinterpret_cast<ID3D11Texture2D*>(handle_pair_bkw.first);
	ID3D11Texture2D* fwdFrame = reinterpret_cast<ID3D11Texture2D*>(handle_pair_fwd.first);
	ID3D11Texture2D* outFrame = reinterpret_cast<ID3D11Texture2D*>(handle_pair_out.first);
	MFX_CHECK_NULL_PTR3(bkwFrame, fwdFrame, outFrame);

	xeAIVfiStatus sts = m_aiIntp.ProcessFrame(bkwFrame, fwdFrame, outFrame);
	if (sts != XE_AIVFI_SUCCESS)
	{
		MFX_RETURN(MFX_ERR_UNKNOWN);
	}
#endif
	if (m_postWrokForFi)
	{
		MFX_CHECK_STS(m_vppAfterFi->Submit(&m_rgbFiOut, &m_fiOut));
		return m_core->DoFastCopyWrapper(out, MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET, &m_fiOut, MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET);
	}
	else
	{
		return MFX_ERR_NONE;
	}
}

mfxStatus MFXVideoFrameInterpolation::SceneChangeDetect(mfxFrameSurface1* input, bool isExternal, mfxU32& decision)
{
	mfxStatus sts = MFX_ERR_NONE;

	mfxU8* dataY = nullptr;
	mfxI32 pitch = 0;
	
	if (input->Info.FourCC == MFX_FOURCC_NV12)
	{
		if (isExternal)
		{
			sts = m_core->LockExternalFrame(*input);
		}
		else
		{
			sts = m_core->LockFrame(*input);
		}
		MFX_CHECK_STS(sts);

		if (input->Data.Y == 0)
			MFX_RETURN(MFX_ERR_LOCK_MEMORY);
		pitch = (mfxI32)(input->Data.PitchLow + (input->Data.PitchHigh << 16));
		dataY = input->Data.Y;
	}
	else if(input->Info.FourCC == MFX_FOURCC_RGB4)
	{
		sts = m_vppForScd->Submit(input, &m_scdImage);
		MFX_CHECK_STS(sts);

		sts = m_core->LockFrame(m_scdImage);
		MFX_CHECK_STS(sts);

		dataY = m_scdImage.Data.Y;
		pitch = m_scdImage.Data.Pitch;
	}
	else
	{
		MFX_RETURN(MFX_ERR_UNSUPPORTED);
	}

	sts = m_scd.PutFrameProgressive(dataY, pitch);
	MFX_CHECK_STS(sts);

	decision = m_scd.Get_frame_shot_Decision();

	if (isExternal)
	{
		sts = m_core->UnlockExternalFrame(*input);
	}
	else
	{
		sts = m_core->UnlockFrame(*input);
	}
	MFX_CHECK_STS(sts);
	return sts;
}

mfxStatus MFXVideoFrameInterpolation::AddTaskQueue(mfxU32 taskIndex)
{
	m_taskQueue.push(task(taskIndex, m_outStamp));
	m_outStamp++;
	if (m_outStamp == m_outTick)
	{
		m_outStamp = 0;
	}
	return MFX_ERR_NONE;
}