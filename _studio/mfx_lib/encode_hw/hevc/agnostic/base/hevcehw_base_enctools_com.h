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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)
#if defined(MFX_ENABLE_ENCTOOLS_BASE)

#include "hevcehw_base.h"
#include "hevcehw_base_data.h"
#include "mfxenctools-int.h"
#include "mfx_enc_common.h"

inline mfxEncTools* GetEncTools(mfxVideoParam& video)
{
    return (mfxEncTools*)mfx::GetExtBuffer(video.ExtParam, video.NumExtParam, MFX_EXTBUFF_ENCTOOLS);
}
inline bool IsGameStreaming(const mfxVideoParam& video)
{
    const mfxExtCodingOption3* pExtOpt3 = HEVCEHW::ExtBuffer::Get(video);
    return (pExtOpt3 && pExtOpt3->ScenarioInfo == MFX_SCENARIO_GAME_STREAMING);
}
inline mfxU32 GetNumTempLayers(const mfxVideoParam& video)
{
    const  mfxExtAvcTemporalLayers* tempLayers = HEVCEHW::ExtBuffer::Get(video);

    if (tempLayers == nullptr)
        return 0;

    mfxU32 numTemporalLayer = 0;
    if (tempLayers)
    {
        for (mfxU32 i = 0; i < 8; i++)
        {
            if (tempLayers->Layer[i].Scale != 0)
                numTemporalLayer++;
        }
    }
    return numTemporalLayer;
}
inline bool CheckEncToolsCondition(const mfxVideoParam& video)
{
    return ((video.mfx.FrameInfo.PicStruct == 0
        || video.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
        && GetNumTempLayers(video) <= 1);
}
inline mfxU32 CheckFlag(mfxU16& flag, bool bCond)
{
    return HEVCEHW::CheckOrZero<mfxU16>(flag
        , mfxU16(MFX_CODINGOPTION_UNKNOWN)
        , mfxU16(MFX_CODINGOPTION_OFF)
        , mfxU16(MFX_CODINGOPTION_ON * (bCond)));
}
inline void InitEncToolsCtrlExtDevice(mfxEncToolsCtrlExtDevice& extDevice, mfxHandleType h_type, mfxHDL device_handle)
{
    extDevice.Header.BufferId = MFX_EXTBUFF_ENCTOOLS_DEVICE;
    extDevice.Header.BufferSz = sizeof(mfxEncToolsCtrlExtDevice);
    extDevice.HdlType = h_type;
    extDevice.DeviceHdl = device_handle;
}
inline void InitEncToolsCtrlExtAllocator(mfxEncToolsCtrlExtAllocator& extAllocator, mfxFrameAllocator& allocator)
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
constexpr mfxU32 ENCTOOLS_QUERY_TIMEOUT = 5000;

namespace HEVCEHW
{
namespace Base
{
    class HevcEncToolsCommon : public FeatureBase
    {
    public:
        bool SelectPureHWPath(const mfxVideoParam& par) { return IsGameStreaming(par);}

        bool isEncTools(const mfxVideoParam& par);
        mfxStatus SubmitPreEncTask(StorageW&  global, StorageW& s_task);

        virtual bool isFeatureEnabled(const mfxVideoParam& par) = 0;
        virtual void SetDefaultConfig(const mfxVideoParam& video, mfxExtEncToolsConfig& config, bool bMBQPSupport) = 0;
        virtual bool IsEncToolsConfigOn(const mfxExtEncToolsConfig& config, bool bGameStreaming) = 0;
        virtual bool IsEncToolsImplicit(const mfxVideoParam& video) = 0;
        virtual mfxU32 CorrectVideoParams(mfxVideoParam& video, mfxExtEncToolsConfig& supportedConfig) = 0;
        virtual mfxStatus QueryPreEncTask(StorageW&  global, StorageW& s_task) = 0;
        virtual mfxStatus InitEncToolsCtrl(mfxVideoParam const& par, mfxEncToolsCtrl* ctrl);
        eMFXHWType m_hwType = MFX_HW_UNKNOWN;
#define DECL_BLOCK_LIST\
    DECL_BLOCK(Check)\
    DECL_BLOCK(Init)\
    DECL_BLOCK(Reset)\
    DECL_BLOCK(AddTask)\
    DECL_BLOCK(UpdateTask)\
    DECL_BLOCK(Discard)\
    DECL_BLOCK(Close)\
    DECL_BLOCK(SetDefaults)\
    DECL_BLOCK(QueryIOSurf)\
    DECL_BLOCK(ResetCheck)\
    DECL_BLOCK(SetDefaultsCallChain)\
    DECL_BLOCK(GetFrameCtrl)\
    DECL_BLOCK(Update)

#define DECL_FEATURE_NAME "Base_EncTools"
#include "hevcehw_decl_blocks.h"

        HevcEncToolsCommon(mfxU32 FeatureId)
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
    };
} //Base
} //namespace HEVCEHW

#endif //defined(MFX_ENABLE_ENCTOOLS_BASE)
#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)

