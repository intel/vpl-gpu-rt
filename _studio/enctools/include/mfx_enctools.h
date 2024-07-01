// Copyright (c) 2019-2022 Intel Corporation
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

#ifndef __MFX_ENCTOOLS_H__
#define __MFX_ENCTOOLS_H__

#include "mfx_ienctools.h"
#include "mfx_enctools_defs.h"
#include "mfx_enctools_aenc.h"
#include "mfx_enctools_lpla.h"
#include "mfx_enctools_utils.h"
#include "mfx_enctools_allocator.h"
#include "mfxenctools_dl_int.h"

#include <vector>
#include <memory>
#include <assert.h>
#include <algorithm>


#include "mfx_perc_enc_vpp_avx2.h"

class PercEncFilterWrapper
{
public:
    mfxStatus Init(const mfxFrameInfo& info);
    mfxStatus SetModulationMap(const mfxEncToolsHintSaliencyMap &sm);
    mfxStatus RunFrame(mfxFrameSurface1& in, mfxFrameSurface1& out, mfxU32 QpY);

private:
    static bool AVX2Supported();

    static const mfxU32 blockSizeFilter = 16;
    bool initialized = false;
    int width = 0;
    int height = 0;

    std::vector<uint8_t> previousOutput;

    int modulationStride = 0;
    std::vector<uint8_t> modulation;

    std::array<PercEncPrefilter::Parameters::PerBlock, 2> parametersBlock;
    PercEncPrefilter::Parameters::PerFrame parametersFrame;
    std::unique_ptr<PercEncPrefilter::Filter> filter;
};


using namespace EncToolsUtils;

mfxStatus InitCtrl(mfxVideoParam const & par, mfxEncToolsCtrl *ctrl);

class EncTools : public IEncTools
{
private:

    bool       m_bVPPInit;
    bool       m_bInit;

    std::unique_ptr<IEncToolsBRC> m_brc;

    AEnc_EncTool m_scd;
    LPLA_EncTool m_lpLookAhead;
    mfxExtEncToolsConfig m_config;
    mfxEncToolsCtrl  m_ctrl;
    mfxHDL m_device;
    mfxU32 m_deviceType;
    mfxFrameAllocator *m_pAllocator;
    MFXFrameAllocator *m_pETAllocator;
    mfxAllocatorParams *m_pmfxAllocatorParams;
    MFXDLVideoSession* m_mfxSession_LA_ENC;
    MFXDLVideoSession m_mfxSession_LA_VPP;
    MFXDLVideoSession m_mfxSession_SCD;

    std::unique_ptr<MFXDLVideoVPP> m_pmfxVPP_LA;
    std::unique_ptr<MFXDLVideoVPP> m_pmfxVPP_SCD;

    mfxVideoParam m_mfxVppParams_LA;
    mfxVideoParam m_mfxVppParams_AEnc;
    mfxFrameAllocResponse m_VppResponse;
    std::vector<mfxFrameSurface1> m_pIntSurfaces_LA;
    mfxFrameSurface1 m_IntSurfaces_SCD;

    void* m_hRTModule = nullptr;


    PercEncFilterWrapper m_PercEncFilter;
    MFXDLVideoSession m_FFPrefilterSession;
    std::unique_ptr<MFXDLVideoVPP>m_FFPrefilterVPP;
    bool m_UseFFPrefilter = false;

    mfxStatus InitFFPrefilter(mfxEncToolsCtrl const & ctrl);
    mfxStatus RunFFPrefilter(mfxEncToolsPrefilterParam *pPrefilterParam);
    mfxStatus CloseFFPrefilter();

public:
    EncTools(void* rtmodule, void* etmodule);
    ~EncTools() override { Close(); }

    mfxStatus Init(mfxExtEncToolsConfig const * pEncToolConfig, mfxEncToolsCtrl const * ctrl) override;
    mfxStatus GetSupportedConfig(mfxExtEncToolsConfig* pConfig, mfxEncToolsCtrl const * ctrl) override;
    mfxStatus GetActiveConfig(mfxExtEncToolsConfig* pConfig) override;
    mfxStatus GetDelayInFrames(mfxExtEncToolsConfig const * pConfig, mfxEncToolsCtrl const * ctrl, mfxU32 *numFrames) override;
    mfxStatus Reset(mfxExtEncToolsConfig const * pEncToolConfig, mfxEncToolsCtrl const * ctrl) override;
    mfxStatus Close() override;

    mfxStatus Submit(mfxEncToolsTaskParam const * par) override;
    mfxStatus Query(mfxEncToolsTaskParam* par, mfxU32 timeOut)override ;
    mfxStatus Discard(mfxU32 displayOrder) override;

protected:
    mfxStatus InitMfxVppParams(mfxEncToolsCtrl const & ctrl);
    mfxStatus InitVPP(mfxEncToolsCtrl const & ctrl);
    mfxStatus InitVPP_LA(mfxEncToolsCtrl const & ctrl);
    mfxStatus ResetVPP(mfxEncToolsCtrl const& ctrl);
    mfxStatus CloseVPP();
    mfxStatus CloseVPP_LA();

    mfxStatus GetDeviceAllocator(mfxEncToolsCtrl const* ctrl);
    mfxStatus InitVPPSession(MFXDLVideoSession* pmfxSession);
    mfxStatus VPPDownScaleSurface(MFXDLVideoSession* m_pmfxSession, MFXDLVideoVPP* pVPP, mfxSyncPoint* pVppSyncp, mfxFrameSurface1* pInSurface, mfxFrameSurface1* pOutSurface);
};

class ExtBRC : public EncTools
{
public:
    ExtBRC(void* rtmodule, void* etmodule) :
        EncTools(rtmodule, etmodule) {}
    mfxStatus GetFrameCtrl(mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl);
    mfxStatus Update(mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl, mfxBRCFrameStatus* status);
};

namespace ExtBRCFuncs
{
    inline mfxStatus Init(mfxHDL pthis, mfxVideoParam* par)
    {
        MFX_CHECK_NULL_PTR2(pthis,par);

        mfxExtEncToolsConfig config = {};
        mfxEncToolsCtrl ctrl = {};
        mfxStatus sts = MFX_ERR_NONE;

        sts = InitCtrl(*par, &ctrl);
        MFX_CHECK_STS(sts);

        sts = ((ExtBRC*)pthis)->GetSupportedConfig(&config, &ctrl);
        MFX_CHECK_STS(sts);
        MFX_CHECK(config.BRC != 0, MFX_ERR_UNSUPPORTED);

        config = {};
        config.BRC = MFX_CODINGOPTION_ON;

        return ((ExtBRC*)pthis)->Init(&config, &ctrl);
    }
    inline mfxStatus Close(mfxHDL pthis)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((EncTools*)pthis)->Close();
    }

    inline mfxStatus GetFrameCtrl(mfxHDL pthis, mfxBRCFrameParam* par, mfxBRCFrameCtrl* ctrl)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((ExtBRC*)pthis)->GetFrameCtrl(par, ctrl);
    }

    inline mfxStatus Update(mfxHDL pthis, mfxBRCFrameParam* par,  mfxBRCFrameCtrl* ctrl, mfxBRCFrameStatus* status)
    {
        MFX_CHECK_NULL_PTR1(pthis);
        return ((ExtBRC*)pthis)->Update(par, ctrl, status);
    }
    inline mfxStatus Reset(mfxHDL pthis, mfxVideoParam* par)
    {
        mfxExtEncToolsConfig config = {};
        mfxEncToolsCtrl ctrl = {};
        mfxStatus sts = MFX_ERR_NONE;

        config = {};
        config.BRC = MFX_CODINGOPTION_ON;

        sts = InitCtrl(*par, &ctrl);
        MFX_CHECK_STS(sts);

        MFX_CHECK_NULL_PTR1(pthis);
        return ((ExtBRC*)pthis)->Reset(&config, &ctrl);
    }
}

inline bool isFieldMode(mfxEncToolsCtrl const & ctrl)
{
    return ((ctrl.CodecId == MFX_CODEC_HEVC) && !(ctrl.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE));
}

#endif
