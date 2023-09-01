// Copyright (c) 2023 Intel Corporation
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

#include "mfx_common.h"

#if defined (ONEVPL_EXPERIMENTAL)

#include "mfx_perc_enc_vpp.h"
#include "mfx_ext_buffers.h"
#include "mfx_common_int.h"

namespace PercEncPrefilter
{

mfxStatus PercEncFilter::Query(mfxExtBuffer* hint)
{
    std::ignore = hint;
    return MFX_ERR_NONE;
}

PercEncFilter::PercEncFilter(VideoCORE* pCore, mfxVideoParam const& par)
{
    m_core = dynamic_cast <CommonCORE_VPL*>(pCore);
    MFX_CHECK_WITH_THROW_STS(m_core, MFX_ERR_NULL_PTR);
    std::ignore = par;
}

PercEncFilter::~PercEncFilter()
{
    std::ignore = Close();
}

mfxStatus PercEncFilter::Init(mfxFrameInfo* in, mfxFrameInfo* out)
{
    const bool cpuHasAvx2 = __builtin_cpu_supports("avx2");
    MFX_CHECK(cpuHasAvx2, MFX_ERR_UNSUPPORTED)

    MFX_CHECK_NULL_PTR1(in);
    MFX_CHECK_NULL_PTR1(out);

    if (m_initialized)
        return MFX_ERR_NONE;

    MFX_CHECK(in->CropW          == out->CropW,          MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(in->CropH          == out->CropH,          MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(in->FourCC         == out->FourCC,         MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(in->BitDepthLuma   == out->BitDepthLuma,   MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(in->BitDepthChroma == out->BitDepthChroma, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(in->ChromaFormat   == out->ChromaFormat,   MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(in->Shift          == out->Shift,          MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(in->CropW >= 16, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(in->CropH >= 2, MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(in->FourCC         == MFX_FOURCC_NV12,         MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(in->ChromaFormat   == MFX_CHROMAFORMAT_YUV420, MFX_ERR_INVALID_VIDEO_PARAM);

    width = in->CropW;
    height = in->CropH;
    previousOutput.resize(size_t(width) * height);

    parametersFrame.spatialSlope = 2;
    parametersFrame.temporalSlope = 5;
    // VPP does not support QP Adaptive
    parametersFrame.qpAdaptive = false;
    if(!parametersFrame.qpAdaptive)
    {
        // Orig
        parametersBlock[0].spatial.pivot = 0.005909118892594739f;
        parametersBlock[1].spatial.pivot = 0.008541855174858726f;
        parametersBlock[0].spatial.minimum = -0.02285621848581362f;
        parametersBlock[1].spatial.minimum = -0.04005541977955759f;
        parametersBlock[0].spatial.maximum = 0.041140246241535394f;
        parametersBlock[1].spatial.maximum = 0.f;
    }
    else
    {
        // New
        parametersBlock[0].spatial.pivot = 0.005909118892594739f;
        parametersBlock[1].spatial.pivot = 0.008541855174858726f;
        parametersBlock[0].spatial.minimum = -0.02285621848581362f/3.0f;
        parametersBlock[1].spatial.minimum = -0.04005541977955759f/3.0f;
        parametersBlock[0].spatial.maximum = 0.041140246241535394f/3.0f;
        parametersBlock[1].spatial.maximum = 0.f;
    }

    parametersBlock[0].temporal.pivot = 0.f;
    parametersBlock[1].temporal.pivot = 0.f;
    parametersBlock[0].temporal.minimum = 0.f;
    parametersBlock[1].temporal.minimum = 0.f;
    parametersBlock[0].temporal.maximum = 0.f;
    parametersBlock[1].temporal.maximum = 0.f;

    filter = std::make_unique<Filter>(parametersFrame, parametersBlock, width);

#if defined(MFX_ENABLE_ENCTOOLS)
    //modulation map
    m_frameCounter = 0;
    m_saliencyMapSupported = false;

    mfxVideoParam par{};
    m_encTools = MFXVideoENCODE_CreateEncTools(par);

    if(m_encTools)
    {
        mfxExtEncToolsConfig config{};
        mfxEncToolsCtrl ctrl{};

        config.SaliencyMapHint = MFX_CODINGOPTION_ON;
        ctrl.CodecId = MFX_CODEC_AVC;

        mfxEncToolsCtrlExtAllocator extAllocBut{};
        extAllocBut.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_ALLOCATOR;
        extAllocBut.Header.BufferSz = sizeof(mfxEncToolsCtrlExtAllocator);

        mfxFrameAllocator* pFrameAlloc = QueryCoreInterface<mfxFrameAllocator>(m_core, MFXIEXTERNALLOC_GUID);
        MFX_CHECK_NULL_PTR1(pFrameAlloc);
        extAllocBut.pAllocator = pFrameAlloc;

        std::vector<mfxExtBuffer*> extParams;
        extParams.push_back(&extAllocBut.Header);

        ctrl.ExtParam = extParams.data();
        ctrl.NumExtParam = (mfxU16)extParams.size();

        ctrl.FrameInfo.CropH = in->CropH;
        ctrl.FrameInfo.CropW = in->CropW;

        mfxStatus sts = m_encTools->Init(m_encTools->Context, &config, &ctrl);
        m_saliencyMapSupported = (sts == MFX_ERR_NONE);

        modulationStride = (width + blockSizeFilter - 1) / blockSizeFilter;
        modulation.resize(size_t(modulationStride) * ((height + blockSizeFilter - 1) / blockSizeFilter));
    }
#endif
    m_initialized = true;

    return MFX_ERR_NONE;
}

mfxStatus PercEncFilter::Close()
{
#if defined(MFX_ENABLE_ENCTOOLS)
    if(m_encTools)
    {
        m_encTools->Close(m_encTools->Context);
        MFXVideoENCODE_DestroyEncTools(m_encTools);
    }
#endif
    return MFX_ERR_NONE;
}

mfxStatus PercEncFilter::Reset(mfxVideoParam* video_param)
{
    MFX_CHECK_NULL_PTR1(video_param);

    MFX_SAFE_CALL(Close());

    MFX_SAFE_CALL(Init(&video_param->vpp.In, &video_param->vpp.Out));

    return MFX_ERR_NONE;
}

mfxStatus PercEncFilter::SetParam(mfxExtBuffer*)
{
    return MFX_ERR_NONE;
}

mfxStatus PercEncFilter::RunFrameVPPTask(mfxFrameSurface1* in, mfxFrameSurface1* out, InternalParam* param)
{
    return RunFrameVPP(in, out, param);
}

mfxStatus PercEncFilter::RunFrameVPP(mfxFrameSurface1* in, mfxFrameSurface1* out, InternalParam*)
{
    MFX_CHECK_NULL_PTR1(in);
    MFX_CHECK_NULL_PTR1(out);

    //skip filtering if cropping or resizing is required
    if( in->Info.CropX != out->Info.CropX || in->Info.CropX != 0 ||
        in->Info.CropY != out->Info.CropY || in->Info.CropY != 0 ||
        in->Info.CropW != out->Info.CropW ||
        in->Info.CropH != out->Info.CropH
    ){
        return MFX_ERR_NONE;
    }
#if defined(MFX_ENABLE_ENCTOOLS)
    //get modulation map
    mfxStatus sts = MFX_ERR_NONE;

    if(m_saliencyMapSupported)
    {
        {
            mfxEncToolsFrameToAnalyze extFrameData = {};
            extFrameData.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_FRAME_TO_ANALYZE;
            extFrameData.Header.BufferSz = sizeof(extFrameData);
            extFrameData.Surface = in;

            std::vector<mfxExtBuffer*> extParams;
            extParams.push_back(&extFrameData.Header);

            mfxEncToolsTaskParam param{};
            param.ExtParam = extParams.data();
            param.NumExtParam = (mfxU16)extParams.size();
            param.DisplayOrder = m_frameCounter;

            sts = m_encTools->Submit(m_encTools->Context, &param);
            MFX_CHECK_STS(sts);
        }

        {
            mfxEncToolsHintSaliencyMap extSM = {};
            extSM.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_HINT_SALIENCY_MAP;
            extSM.Header.BufferSz = sizeof(extSM);

            const mfxU32 blockSize = 8; // source saliency is on 8x8 block granularity

            extSM.AllocatedSize = in->Info.Width * in->Info.Height / (blockSize * blockSize);
            std::unique_ptr<mfxF32[]> smBuffer(new mfxF32[extSM.AllocatedSize]);
            extSM.SaliencyMap = smBuffer.get();

            std::vector<mfxExtBuffer*> extParams;
            extParams.push_back(&extSM.Header);

            mfxEncToolsTaskParam param{};
            param.ExtParam = extParams.data();
            param.NumExtParam = (mfxU16)extParams.size();
            param.DisplayOrder = m_frameCounter;
            m_frameCounter++;

            sts = m_encTools->Query(m_encTools->Context, &param, 0 /*timeout*/);
            if (sts == MFX_ERR_NOT_ENOUGH_BUFFER)
            {
                extSM.AllocatedSize = extSM.Width * extSM.Height;
                smBuffer.reset(new mfxF32[extSM.AllocatedSize]);
                extSM.SaliencyMap = smBuffer.get();
                sts = m_encTools->Query(m_encTools->Context, &param, 0 /*timeout*/);
            }

            MFX_CHECK_STS(sts);

            MFX_CHECK(extSM.BlockSize == blockSize, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            for (size_t y = 0; y < size_t(height); y += blockSizeFilter)
                for (size_t x = 0; x < size_t(width); x += blockSizeFilter)
                {
                    float m = 0.f;
                    int count = 0;

                    for (size_t dy = 0; dy < std::min<size_t>(blockSizeFilter, height - y); dy += blockSize)
                        for (size_t dx = 0; dx <  std::min<size_t>(blockSizeFilter, width - x); dx +=blockSize)
                        {
                            m += smBuffer[(x + dx) / blockSize + (y + dy) / blockSize * extSM.Width];
                            ++count;
                        }

                    count = count ? count : 1;
                    int mod = int(256.f * m /count);
                    mod = std::max(mod, 0);
                    mod = std::min(mod, 255);
                    modulation[x / blockSizeFilter + y / blockSizeFilter * modulationStride] = (uint8_t)mod;
                }
        }
    }
#endif

    mfxFrameSurface1_scoped_lock inLock(in, m_core), outLock(out, m_core);
    MFX_SAFE_CALL(inLock.lock(MFX_MAP_READ));
    MFX_SAFE_CALL(outLock.lock(MFX_MAP_WRITE));

    if (filter)
    {
        filter->processFrame(in->Data.Y, in->Data.Pitch, modulation.data(), modulationStride, previousOutput.data(), width, out->Data.Y, out->Data.Pitch, width, height);
    }
    else
    {
        for (size_t y = 0; y < size_t(height); ++y)
        {
            std::copy(
                &in->Data.Y[in->Data.Pitch * y],
                &in->Data.Y[in->Data.Pitch * y + width],
                &out->Data.Y[out->Data.Pitch * y]);
        }
    }

    // retain a copy of the output for next time... (it would be nice to avoid this copy)
    for (size_t y = 0; y < size_t(height); ++y)
    {
        std::copy(
            &out->Data.Y[out->Data.Pitch * y],
            &out->Data.Y[out->Data.Pitch * y + width],
            &previousOutput[width * y]);
    }

    // copy chroma
    for (int y = 0; y < height / 2; ++y)
    {
        std::copy(
            &in->Data.UV[in->Data.Pitch * y],
            &in->Data.UV[in->Data.Pitch * y + width],
            &out->Data.UV[out->Data.Pitch * y]);
    }

    return MFX_ERR_NONE;
}

bool PercEncFilter::IsReadyOutput(mfxRequestType)
{
    //TBD: temporary do processing in sync. part therefore always return true
    return true;
}

}//namespace

#endif
