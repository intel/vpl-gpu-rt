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

    m_paramsSharpening.config = &m_config;
    m_paramsTemporal.config = &m_config;
    m_filter.spatial[0] = &m_paramsSharpening;
    m_filter.spatial[1] = &m_paramsSharpening;
    m_filter.temporal = &m_paramsTemporal;

    m_modulation.Resize(in->CropW, in->CropH);
    auto &v = m_modulation.planes[0].v;
    std::fill(v.begin(), v.end(), uint8_t(128));

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
        {   mfxEncToolsFrameToAnalyze extFrameData = {};
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

        {   mfxEncToolsHintSaliencyMap extSM = {};
            extSM.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_HINT_SALIENCY_MAP;
            extSM.Header.BufferSz = sizeof(extSM);

            mfxU32 blockSize = 8;
            mfxU32 numOfBlocks = in->Info.Width * in->Info.Height / (blockSize * blockSize);
            std::unique_ptr<mfxF32[]> smBuffer(new mfxF32[numOfBlocks]);

            extSM.AllocatedSize = numOfBlocks;
            extSM.SaliencyMap = smBuffer.get();

            std::vector<mfxExtBuffer*> extParams;
            extParams.push_back(&extSM.Header);

            mfxEncToolsTaskParam param{};
            param.ExtParam = extParams.data();
            param.NumExtParam = (mfxU16)extParams.size();
            param.DisplayOrder = m_frameCounter;
            m_frameCounter++;

            sts = m_encTools->Query(m_encTools->Context, &param, 0 /*timeout*/);
            MFX_CHECK_STS(sts);
        }
    }
#endif

    mfxFrameSurface1_scoped_lock inLock(in, m_core), outLock(out, m_core);
    MFX_SAFE_CALL(inLock.lock(MFX_MAP_READ));
    MFX_SAFE_CALL(outLock.lock(MFX_MAP_WRITE));

    Picture<uint8_t> in8(in->Info.CropW, in->Info.CropH);
    for(uint32_t i=0; i<in->Info.CropH; i++){
        mfxU8 *first = in->Data.Y + i*in->Data.Pitch;
        mfxU8* last = first + in->Info.CropW;
        mfxU8* d_first = in8.planes[0].v.data() + i*in->Info.CropW;
        std::copy(first, last, d_first);
    }

    Picture<uint16_t> in10(in8, 2);
    Picture<uint16_t> out10(in->Info.CropW, in->Info.CropH);
    m_filter.filter(out10.planes[0], in10.planes[0], out10.planes[0], m_modulation.planes[0],
        in10.planes[0].width, in10.planes[0].height, m_config, m_first);
    m_first = false;

    Picture<uint8_t> out8(out10, -2);
    for(uint32_t i=0; i<in->Info.CropH; i++){
        mfxU8 *first = out8.planes[0].v.data() + i*in->Info.CropW;
        mfxU8* last = first + in->Info.CropW;
        mfxU8* d_first = out->Data.Y + i*out->Data.Pitch;
        std::copy(first, last, d_first);
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
