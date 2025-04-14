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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
#if defined(MFX_ENABLE_ENCTOOLS_BASE)

#include "av1ehw_base.h"
#include "av1ehw_base_data.h"
#include "mfxenctools-int.h"
#include "mfx_enc_common.h"
#include "mfx_utils.h"

#define IS_CUST_MATRIX(CqmHint)  ((CqmHint) >= CQM_HINT_USE_CUST_MATRIX1 && (CqmHint) < CQM_HINT_USE_CUST_MATRIX1 + CQM_HINT_NUM_CUST_MATRIX)
constexpr mfxU32 ENCTOOLS_QUERY_TIMEOUT = 5000;
inline bool IsGameStreaming(const mfxVideoParam& video)
{
    const mfxExtCodingOption3* pExtOpt3 = AV1EHW::ExtBuffer::Get(video);
    return (pExtOpt3 && pExtOpt3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING);
}

inline bool IsSwEncToolsImplicit(const mfxVideoParam& video)
{
    const mfxExtCodingOption2* pExtOpt2 = AV1EHW::ExtBuffer::Get(video);
    if (pExtOpt2 && pExtOpt2->LookAheadDepth > 0)
    {
        const mfxExtCodingOption3* pExtOpt3 = AV1EHW::ExtBuffer::Get(video);
        if (
            (video.mfx.GopRefDist == 1 || video.mfx.GopRefDist == 2 || video.mfx.GopRefDist == 4 || video.mfx.GopRefDist == 8 || video.mfx.GopRefDist == 16)
            && IsOn(pExtOpt2->ExtBRC)
            && !(pExtOpt3 && pExtOpt3->ScenarioInfo != MFX_SCENARIO_UNKNOWN)
            )
        {
            return true;
        }
    }
    return false;
}

inline bool IsHwLookAhead(const mfxExtEncToolsConfig &config, bool bGameStreaming)
{
    if (!bGameStreaming)
        return false;
    return
        (IsOn(config.AdaptiveI)
        || IsOn(config.AdaptiveB)
        || IsOn(config.SceneChange)
        || IsOn(config.AdaptivePyramidQuantP)
        || IsOn(config.AdaptivePyramidQuantB)
        || IsOn(config.BRCBufferHints)
        || IsOn(config.AdaptiveQuantMatrices));
}

inline mfxU32 CheckFlag(mfxU16& flag, bool bCond)
{
    return AV1EHW::CheckOrZero<mfxU16>(flag
        , mfxU16(MFX_CODINGOPTION_UNKNOWN)
        , mfxU16(MFX_CODINGOPTION_OFF)
        , mfxU16(MFX_CODINGOPTION_ON * (bCond)));
}

inline bool IsEncToolsConfigDefined(const mfxExtEncToolsConfig* config) {
    //this code is based on definition of MFX_CODINGOPTION_UNKNOWN=0 
    if (!config)
        return false;

    return
        config->AdaptiveI | config->AdaptiveB
        | config->AdaptiveRefP | config->AdaptiveRefB
        | config->SceneChange
        | config->AdaptiveLTR
        | config->AdaptivePyramidQuantP | config->AdaptivePyramidQuantB
        | config->AdaptiveQuantMatrices
        | config->AdaptiveMBQP
        | config->BRCBufferHints
        | config->BRC;
}


inline bool IsSwEncToolsSupported(const mfxVideoParam& video)
{
    return
        video.mfx.GopRefDist == 0
        || video.mfx.GopRefDist == 1
        || video.mfx.GopRefDist == 2
        || video.mfx.GopRefDist == 4
        || video.mfx.GopRefDist == 8
        || video.mfx.GopRefDist == 16;
}

inline void InitEncToolsCtrlExtDevice(mfxEncToolsCtrlExtDevice & extDevice
                            , mfxHandleType h_type
                            , mfxHDL device_handle)
{
    extDevice.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_DEVICE;
    extDevice.Header.BufferSz = sizeof(mfxEncToolsCtrlExtDevice);
    extDevice.HdlType = h_type;
    extDevice.DeviceHdl = device_handle;
}

inline void InitEncToolsCtrlExtAllocator(mfxEncToolsCtrlExtAllocator & extAllocator
                            , mfxFrameAllocator &allocator)
{
    extAllocator.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_ALLOCATOR;
    extAllocator.Header.BufferSz = sizeof(mfxEncToolsCtrlExtAllocator);
    extAllocator.pAllocator = &allocator;
}

inline mfxHandleType GetHandleType(eMFXVAType vaType)
{
    switch (vaType)
    {
    case MFX_HW_D3D9:  return MFX_HANDLE_D3D9_DEVICE_MANAGER;
    case MFX_HW_VAAPI: return MFX_HANDLE_VA_DISPLAY;
    default:
      break;
    }
    return MFX_HANDLE_D3D11_DEVICE;
}

inline mfxEncTools *GetEncTools(const mfxVideoParam &video)
{
    return (mfxEncTools *)mfx::GetExtBuffer(video.ExtParam, video.NumExtParam, MFX_EXTBUFF_ENCTOOLS);
}

namespace AV1EHW
{
namespace Base
{
    class AV1EncToolsCommon
        : public FeatureBase
    {
    public:
        virtual mfxStatus SubmitPreEncTask(StorageW&  global, StorageW& s_task);
        virtual mfxStatus QueryPreEncTask(StorageW&  global, StorageW& s_task) = 0;
        virtual bool IsFeatureEnabled(const mfxVideoParam& par) = 0;
        bool SelectPureHWPath(const mfxVideoParam& par) { return IsGameStreaming(par);}
        virtual void SetDefaultConfig(const mfxVideoParam &video, mfxExtEncToolsConfig &config, bool bMBQPSupport) = 0;
        virtual mfxU32 CorrectVideoParams(mfxVideoParam& video, mfxExtEncToolsConfig& supportedConfig) = 0;
        virtual mfxStatus InitEncToolsCtrl(mfxVideoParam const& par, mfxEncToolsCtrl* ctrl);
        bool IsSwEncToolsOn(const mfxVideoParam& video);
        eMFXHWType m_hwType = MFX_HW_UNKNOWN;
#define DECL_BLOCK_LIST\
    DECL_BLOCK(SetDefaultsCallChain)\
    DECL_BLOCK(Check)\
    DECL_BLOCK(Init)\
    DECL_BLOCK(ResetCheck)\
    DECL_BLOCK(Reset)\
    DECL_BLOCK(SetCallChains)\
    DECL_BLOCK(AddTask)\
    DECL_BLOCK(PreEncSubmit)\
    DECL_BLOCK(PreEncQuery)\
    DECL_BLOCK(GetFrameCtrl)\
    DECL_BLOCK(UpdateTask)\
    DECL_BLOCK(Update)\
    DECL_BLOCK(Discard)\
    DECL_BLOCK(Close)\
    DECL_BLOCK(SetDefaults)\
    DECL_BLOCK(QueryIOSurf)
#define DECL_FEATURE_NAME "Base_EncTools"
#include "av1ehw_decl_blocks.h"

        AV1EncToolsCommon(mfxU32 FeatureId)
            : FeatureBase(FeatureId)
        {}

    protected:
        virtual void Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
        virtual void SetDefaults(const FeatureBlocks& blocks, TPushSD Push) override; 
        virtual void InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push) override;
        virtual void FreeTask(const FeatureBlocks& blocks, TPushQT Push) override; 
        virtual void ResetState(const FeatureBlocks& blocks, TPushRS Push) override;
        virtual void Close(const FeatureBlocks& blocks, TPushCLS Push) override;
        virtual void QueryIOSurf(const FeatureBlocks&, TPushQIS Push) override;

        mfxEncTools*            m_pEncTools = nullptr;
        mfxEncToolsCtrl         m_EncToolCtrl = {};
        mfxExtEncToolsConfig    m_EncToolConfig = {};
        bool                    m_bEncToolsInner = false;
        mfxU32                  m_maxDelay = 0;
        mfxU32                  m_numPicBuffered = 0;

        mfxU16        S_ET_SUBMIT = mfxU16(-1);
        mfxU16        S_ET_QUERY = mfxU16(-1);

        std::list<mfxLplastatus>         LpLaStatus;

        mfx::OnExit    m_destroy;

        mfxU8* m_pSegmentQPMap = nullptr;  //Note: QP in this map is HEVC QP
        mfxU8* m_pSegmentIDMap = nullptr;
        mfxExtAV1Segmentation m_SegmentationInfo = {};
        
        
        bool IsEncToolsOn(const mfxVideoParam& video);
        
    };

    bool IsHwEncToolsOn(const mfxVideoParam& video);
    
} //Base
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_ENCTOOLS_BASE)
#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
