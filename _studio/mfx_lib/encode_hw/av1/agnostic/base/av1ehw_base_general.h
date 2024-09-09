// Copyright (c) 2019-2023 Intel Corporation
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

#include "av1ehw_base.h"
#include "av1ehw_ddi.h"
#include "av1ehw_base_data.h"
#include <tuple>

namespace AV1EHW
{
namespace Base
{
    class General
        : public FeatureBase
    {
    public:

#define DECL_BLOCK_LIST\
    DECL_BLOCK(SetLogging                           )\
    DECL_BLOCK(Query0                               )\
    DECL_BLOCK(SetDefaultsCallChain                 )\
    DECL_BLOCK(PreCheckCodecId                      )\
    DECL_BLOCK(PreCheckChromaFormat                 )\
    DECL_BLOCK(PreCheckExtBuffers                   )\
    DECL_BLOCK(CopyConfigurable                     )\
    DECL_BLOCK(FixParam                             )\
    DECL_BLOCK(CheckAndFixLowPower                  )\
    DECL_BLOCK(CheckAndFixLevel                     )\
    DECL_BLOCK(CheckFormat                          )\
    DECL_BLOCK(CheckPicStruct                       )\
    DECL_BLOCK(CheckSurfSize                        )\
    DECL_BLOCK(CheckCodedPicSize                    )\
    DECL_BLOCK(CheckTU                              )\
    DECL_BLOCK(CheckDeltaQ                          )\
    DECL_BLOCK(CheckFrameOBU                        )\
    DECL_BLOCK(CheckOrderHint                       )\
    DECL_BLOCK(CheckOrderHintBits                   )\
    DECL_BLOCK(CheckCDEF                            )\
    DECL_BLOCK(CheckTemporalLayers                  )\
    DECL_BLOCK(CheckGopRefDist                      )\
    DECL_BLOCK(CheckStillPicture                    )\
    DECL_BLOCK(CheckGPB                             )\
    DECL_BLOCK(CheckNumRefFrame                     )\
    DECL_BLOCK(CheckIOPattern                       )\
    DECL_BLOCK(CheckProtected                       )\
    DECL_BLOCK(CheckRateControl                     )\
    DECL_BLOCK(CheckCrops                           )\
    DECL_BLOCK(CheckShift                           )\
    DECL_BLOCK(CheckFrameRate                       )\
    DECL_BLOCK(CheckProfile                         )\
    DECL_BLOCK(CheckEncodedOrder                    )\
    DECL_BLOCK(CheckLevelConstraints                )\
    DECL_BLOCK(CheckTCBRC                           )\
    DECL_BLOCK(CheckCdfUpdate                       )\
    DECL_BLOCK(Query1NoCaps                         )\
    DECL_BLOCK(CheckColorConfig                     )\
    DECL_BLOCK(Query1WithCaps                       )\
    DECL_BLOCK(SetUnalignedDefaults                 )\
    DECL_BLOCK(SetDefaults                          )\
    DECL_BLOCK(AttachMissingBuffers                 )\
    DECL_BLOCK(SetFrameAllocRequest                 )\
    DECL_BLOCK(SetSH                                )\
    DECL_BLOCK(SetFH                                )\
    DECL_BLOCK(SetReorder                           )\
    DECL_BLOCK(SetRepeat                            )\
    DECL_BLOCK(AllocRaw                             )\
    DECL_BLOCK(AllocRec                             )\
    DECL_BLOCK(AllocBS                              )\
    DECL_BLOCK(ResetInit                            )\
    DECL_BLOCK(ResetCheck                           )\
    DECL_BLOCK(ResetState                           )\
    DECL_BLOCK(CheckSurf                            )\
    DECL_BLOCK(CheckBS                              )\
    DECL_BLOCK(AllocTask                            )\
    DECL_BLOCK(InitTask                             )\
    DECL_BLOCK(PrepareTask                          )\
    DECL_BLOCK(ConfigureTask                        )\
    DECL_BLOCK(GetRawHDL                            )\
    DECL_BLOCK(CopySysToRaw                         )\
    DECL_BLOCK(CopyBS                               )\
    DECL_BLOCK(UpdateBsInfo                         )\
    DECL_BLOCK(SetRecInfo                           )\
    DECL_BLOCK(CheckAndFixSlidingWindow             )\
    DECL_BLOCK(FreeTask                             )\
    DECL_BLOCK(Close                                )
#define DECL_FEATURE_NAME "Base_General"
#include "av1ehw_decl_blocks.h"

        General(mfxU32 id)
            : FeatureBase(id)
        {
        }

    protected:
        TaskCommonPar m_prevTask;
        mfxU32 m_frameOrder             = 0
               , m_lastKeyFrame         = 0
               , m_temporalUnitOrder    = 0;
        bool   m_insertIVFSeq           = true;

        std::function<std::tuple<mfxU16, mfxU16>(const mfxVideoParam&)> m_GetMaxRef;
        std::unique_ptr<Defaults::Param> m_pQWCDefaults;
        NotNull<Defaults*> m_pQNCDefaults;
        eMFXHWType m_hw = MFX_HW_UNKNOWN;

        void ResetState()
        {
            m_frameOrder        = 0;
            m_lastKeyFrame      = 0;
            m_temporalUnitOrder = 0;
            Invalidate(m_prevTask);
        }

    public:
        virtual void SetSupported(ParamSupport& par) override;
        virtual void SetInherited(ParamInheritance& par) override;
        virtual void Query0(const FeatureBlocks& blocks, TPushQ0 Push) override;
        virtual void Query1NoCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
        virtual void Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push) override;
        virtual void SetDefaults(const FeatureBlocks& blocks, TPushSD Push) override;
        virtual void QueryIOSurf(const FeatureBlocks& blocks, TPushQIS Push) override;
        virtual void InitExternal(const FeatureBlocks& blocks, TPushIE Push) override;
        virtual void InitInternal(const FeatureBlocks& blocks, TPushII Push) override;
        virtual void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override;
        virtual void FrameSubmit(const FeatureBlocks& blocks, TPushFS Push) override;
        virtual void AllocTask(const FeatureBlocks& blocks, TPushAT Push) override;
        virtual void InitTask(const FeatureBlocks& blocks, TPushIT Push) override;
        virtual void PreReorderTask(const FeatureBlocks& blocks, TPushPreRT Push) override;
        virtual void PostReorderTask(const FeatureBlocks& blocks, TPushPostRT Push) override;
        virtual void SubmitTask(const FeatureBlocks& blocks, TPushST Push) override;
        virtual void QueryTask(const FeatureBlocks& blocks, TPushQT Push) override;
        virtual void FreeTask(const FeatureBlocks& blocks, TPushFT Push) override;
        virtual void GetVideoParam(const FeatureBlocks& blocks, TPushGVP Push) override;
        virtual void Close(const FeatureBlocks& blocks, TPushCLS Push) override;
        virtual void Reset(const FeatureBlocks& blocks, TPushR Push) override;
        virtual void ResetState(const FeatureBlocks& blocks, TPushRS Push) override;

        static void PushDefaults(Defaults& df);

        void CheckQuery0(const ParamSupport& sprt, mfxVideoParam& par);
        mfxStatus CheckBuffers(const ParamSupport& sprt, const mfxVideoParam& in, const mfxVideoParam* out = nullptr);
        mfxStatus CopyConfigurable(const ParamSupport& sprt, const mfxVideoParam& in, mfxVideoParam& out);

        mfxStatus CheckCodedPicSize(mfxVideoParam& par, const Defaults::Param& defPar);
        mfxStatus CheckTU(mfxVideoParam & par, const ENCODE_CAPS_AV1& caps);
        mfxStatus CheckDeltaQ(mfxVideoParam& par);
        mfxStatus CheckFrameOBU(mfxVideoParam& par, const ENCODE_CAPS_AV1& caps);
        mfxStatus CheckOrderHint(mfxVideoParam& par, const ENCODE_CAPS_AV1& caps);
        mfxStatus CheckOrderHintBits(mfxVideoParam& par);
        mfxStatus CheckCDEF(mfxVideoParam& par, const ENCODE_CAPS_AV1& caps);
        mfxStatus CheckTemporalLayers(mfxVideoParam& par);
        mfxStatus CheckStillPicture(mfxVideoParam& par);
        mfxStatus CheckGopRefDist(mfxVideoParam& par, const Defaults::Param& defPar);
        mfxStatus CheckGPB(mfxVideoParam& par);
        mfxStatus CheckIOPattern(mfxVideoParam& par);
        mfxStatus CheckRateControl(mfxVideoParam& par, const Defaults::Param& defPar);
        mfxStatus CheckCrops(mfxVideoParam& par, const Defaults::Param& defPar);
        mfxStatus CheckShift(mfxVideoParam& par);
        mfxStatus CheckFrameRate(mfxVideoParam& par);
        mfxStatus CheckNumRefFrame(mfxVideoParam& par, const Defaults::Param& defPar);
        mfxStatus CheckColorConfig(mfxVideoParam& par);
        mfxStatus CheckAndFixLevel(mfxVideoParam& par);
        mfxStatus CheckLevelConstraints(mfxVideoParam& par, const Defaults::Param& defPar);
        mfxStatus CheckTCBRC(mfxVideoParam& par, const ENCODE_CAPS_AV1& caps);
        mfxStatus CheckCdfUpdate(mfxVideoParam& par);
        mfxStatus CheckAndFixSlidingWindow(mfxVideoParam& par, const Defaults::Param& defPar);

        void SetDefaults(
            mfxVideoParam& par
            , const Defaults::Param& defPar
            , bool bExternalFrameAllocator);

        void SetSH(
            const ExtBuffer::Param<mfxVideoParam>& par
            , eMFXHWType hw
            , const EncodeCapsAv1& caps
            , SH& sh);

        void SetFH(
            const ExtBuffer::Param<mfxVideoParam>& par
            , eMFXHWType hw
            , const SH& sh
            , FH& fh);

        void ConfigureTask(
            TaskCommonPar& task
            , const Defaults::Param& dflts
            , IAllocation& recPool
            , const TFramesToShowInfo& framesToShowInfo
            , EncodedInfoAv1& encodedInfo
            , FH& fh);

        mfxStatus GetCurrentFrameHeader(
            const TaskCommonPar& task
            , const Defaults::Param& dflts
            , const SH& sps
            , const FH& pps
            , FH & s) const;

        TTaskIt ReorderWrap(const ExtBuffer::Param<mfxVideoParam> & par, TTaskIt begin, TTaskIt end, bool flush);
        static mfxU32 GetRawBytes(const Defaults::Param& par);
        static bool IsInVideoMem(const mfxVideoParam& par);

        mfxU16 GetMaxRaw(const mfxVideoParam& par)
        {
            // Extend extra Raw for frames buffered between LA submit and LA Query stage
            const mfxExtCodingOption2* pCO2 = ExtBuffer::Get(par);
            mfxU16 extRaw = pCO2 ? pCO2->LookAheadDepth : 0;
            mfxU16 numRaw = par.AsyncDepth + par.mfx.GopRefDist + (par.AsyncDepth > 1) + extRaw;
            if (par.mfx.GopRefDist > 0)
                numRaw--;

            return numRaw;
        }
        mfxU16 GetMaxRec(StorageR& strg, const mfxVideoParam& par)
        {
            if (!HaveRABFrames(par))
                return par.AsyncDepth + par.mfx.NumRefFrame + (par.AsyncDepth > 1);

            auto dflts = GetRTDefaults(strg);
            return par.AsyncDepth + dflts.base.GetNumBPyramidLayers(dflts) + par.mfx.NumRefFrame + 2;
        }
        mfxU16 GetMaxBS(StorageR& strg, const mfxVideoParam& par)
        {
            if (!HaveRABFrames(par))
                return par.AsyncDepth + (par.AsyncDepth > 1);

            auto dflts = GetRTDefaults(strg);
            return par.AsyncDepth + dflts.base.GetNumBPyramidLayers(dflts) + 2;
        }
        bool GetRecInfo(
            const mfxVideoParam& par
            , const mfxExtCodingOption3& CO3
            , eMFXHWType hw
            , mfxFrameInfo& rec);
        mfxU32 GetMinBsSize(
            const mfxVideoParam & par
            , const mfxExtAV1ResolutionParam& rsPar
            , const mfxExtCodingOption3& CO3);

    };

    inline void SetDefaultFrameInfo(mfxU32& frameWidth, mfxU32& frameHeight, mfxFrameInfo& fi)
    {
        if (frameWidth)
            SetDefault(fi.CropW, std::min(mfxU32(fi.Width), frameWidth));
        else
        {
            SetDefault(fi.CropW, fi.Width);
            SetDefault(frameWidth, fi.CropW);
        }

        if (frameHeight)
            SetDefault(fi.CropH, std::min(mfxU32(fi.Height), frameHeight));
        else
        {
            SetDefault(fi.CropH, fi.Height);
            SetDefault(frameHeight, fi.CropH);
        }

    }
} //Base
} //namespace AV1EHW

#endif
