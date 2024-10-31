// Copyright (c) 2021-2023 Intel Corporation
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
#include "mfx_enc_common.h"
#include "mfx_platform_caps.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base_va_lin.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;
using namespace AV1EHW::Linux::Base;

mfxStatus DDI_VA::SetDDIID(mfxU16 targetBitDepth, mfxU16 targetChromaFormat)
{
    MFX_CHECK(!m_vaid, MFX_ERR_NONE);

    static const std::map<mfxU16, std::map<mfxU16, VAID>> VAIDSupported[2] =
    {
        // EncSlice Support
        {
            {
                mfxU16(BITDEPTH_8),
                {
                    {mfxU16(MFX_CHROMAFORMAT_YUV420), VAID{VAProfileAV1Profile0, VAEntrypointEncSlice}}
                }
            }
            , {
                mfxU16(BITDEPTH_10),
                {
                    {mfxU16(MFX_CHROMAFORMAT_YUV420), VAID{VAProfileAV1Profile0, VAEntrypointEncSlice}}
                }
            }
        }
        // EncSliceLP Support
        ,{
            {
                mfxU16(BITDEPTH_8),
                {
                    {mfxU16(MFX_CHROMAFORMAT_YUV420), VAID{VAProfileAV1Profile0, VAEntrypointEncSliceLP}}
                }
            }
            , {
                mfxU16(BITDEPTH_10),
                {
                    {mfxU16(MFX_CHROMAFORMAT_YUV420), VAID{VAProfileAV1Profile0, VAEntrypointEncSliceLP}}
                }
            }
        }
    };

    bool bEncSliceLPSupported = CommonCaps::IsVAEncSliceLPSupported(m_hw);

    // Check that list of VAIDs contains VAID for resulting BitDepth, ChromaFormat
    bool bSupported =
        VAIDSupported[bEncSliceLPSupported].count(targetBitDepth)
        && VAIDSupported[bEncSliceLPSupported].at(targetBitDepth).count(targetChromaFormat);

    MFX_CHECK(bSupported, MFX_ERR_UNSUPPORTED);

    // Choose and return VAID
    m_vaid = const_cast<VAID *>(&VAIDSupported[bEncSliceLPSupported].at(targetBitDepth).at(targetChromaFormat));

    return MFX_ERR_NONE;
}

void DDI_VA::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_SetCallChains
        , [this](const mfxVideoParam&, mfxVideoParam& /*par*/, StorageRW& strg) -> mfxStatus
    {
        auto& ddiExec = Glob::DDI_Execute::GetOrConstruct(strg);

        MFX_CHECK(!ddiExec, MFX_ERR_NONE);

        ddiExec.Push([&](Glob::DDI_Execute::TRef::TExt, const DDIExecParam& ep)
        {
            return Execute(ep);
        });

        m_callVa = ddiExec;

        return MFX_ERR_NONE;
    });

    Push(BLK_SetDDIID
        , [this](const mfxVideoParam& par, mfxVideoParam&, StorageRW& strg) -> mfxStatus
    {
        EncodeCapsAv1 fakeCaps;
        std::unique_ptr<Defaults::Param> m_pDefaults(
            new Defaults::Param(
                par
                , fakeCaps
                , Glob::Defaults::Get(strg)));
        MFX_CHECK_NULL_PTR1(m_pDefaults);

        m_hw = Glob::VideoCore::Get(strg).GetHWType();
        const mfxU16 bitDepth     = m_pDefaults->base.GetBitDepthLuma(*m_pDefaults);
        const mfxU16 profile            = m_pDefaults->base.GetProfile(*m_pDefaults);
        const mfxExtCodingOption3* pCO3 = ExtBuffer::Get(m_pDefaults->mvp);
        mfxU16 targetChromaFormat       = m_pDefaults->base.GetTargetChromaFormatPlus1(*m_pDefaults) - 1;

    
        if (pCO3)
        {
            SetIf(targetChromaFormat, pCO3->TargetChromaFormatPlus1, pCO3->TargetChromaFormatPlus1 - 1);
        }

        MFX_SAFE_CALL(SetDDIID(bitDepth, targetChromaFormat));

        return MFX_ERR_NONE;
    });
}

mfxStatus DDI_VA::CreateAndQueryCaps(const mfxVideoParam& par, StorageW& strg)
{
    bool  bNeedNewDevice = !IsValid() || !m_vaid || VAProfile(m_vaid->Profile) != m_profile
        || VAEntrypoint(m_vaid->Entrypoint) != m_entrypoint;

    m_callVa = Glob::DDI_Execute::Get(strg);

    if (bNeedNewDevice)
    {
        MFX_CHECK(m_vaid, MFX_ERR_UNDEFINED_BEHAVIOR);
        auto vap   = VAProfile(m_vaid->Profile);
        auto vaep  = VAEntrypoint(m_vaid->Entrypoint);
        auto& core = Glob::VideoCore::Get(strg);
        mfxStatus sts = Create(core, vap, vaep);
        MFX_CHECK_STS(sts);

        sts = QueryCaps();
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;
}

void DDI_VA::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_QueryCaps
        , [this](const mfxVideoParam& par, StorageW& strg, StorageRW&)
    {
        CreateAndQueryCaps(par, strg);
        auto& caps = Glob::EncodeCaps::Get(strg);
        caps = m_caps;
    });
}

void DDI_VA::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_QueryCaps
        , [this](const mfxVideoParam&, mfxVideoParam& par, StorageRW& strg) -> mfxStatus
    {
        auto sts = CreateAndQueryCaps(par, strg);
        MFX_CHECK_STS(sts);

        auto& caps = Glob::EncodeCaps::GetOrConstruct(strg);
        caps = m_caps;

        return sts;
    });
}

void DDI_VA::InitExternal(const FeatureBlocks& /*blocks*/, TPushIE Push)
{
    Push(BLK_CreateDevice
        , [this](const mfxVideoParam& par, StorageRW& strg, StorageRW&) -> mfxStatus
        {
            return CreateAndQueryCaps(par, strg);
        });
}

mfxStatus DDI_VA::CreateVABuffers(
    const std::list<DDIExecParam>& par
    , std::vector<VABufferID>& pool)
{
    pool.resize(par.size(), VA_INVALID_ID);

    std::transform(par.begin(), par.end(), pool.begin()
        , [this](const DDIExecParam& p){ return CreateVABuffer(p); });

    bool bFailed = pool.end() != std::find(pool.begin(), pool.end(), VA_INVALID_ID);
    MFX_CHECK(!bFailed, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

mfxStatus DDI_VA::DestroyVABuffers(std::vector<VABufferID>& pool)
{
    bool bFailed = std::any_of(pool.begin(), pool.end()
        , [this](VABufferID id) { return !!DestroyVABuffer(id); });

    pool.clear();

    MFX_CHECK(!bFailed, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

void DDI_VA::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_CreateService
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
        {
            const auto&                     par   = Glob::VideoParam::Get(strg);
            const mfxExtAV1ResolutionParam& rsPar = ExtBuffer::Get(par);
            mfxStatus sts;

            m_callVa = Glob::DDI_Execute::Get(strg);

            std::vector<VAConfigAttrib> attrib(2);

            attrib[0].type = VAConfigAttribRTFormat;
            attrib[1].type = VAConfigAttribRateControl;

            sts = MfxEncodeHW::DeviceVAAPI::QueryCaps(attrib.data(), attrib.size() * sizeof(VAConfigAttrib));
            MFX_CHECK_STS(sts);

            MFX_CHECK(attrib[0].value & (VA_RT_FORMAT_YUV420 | VA_RT_FORMAT_YUV420_10), MFX_ERR_DEVICE_FAILED);

            uint32_t vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);

            MFX_CHECK((attrib[1].value & vaRCType), MFX_ERR_DEVICE_FAILED);

            attrib[1].value = vaRCType;

            sts = MfxEncodeHW::DeviceVAAPI::Init(
                rsPar.FrameWidth
                , rsPar.FrameHeight
                , VA_PROGRESSIVE
                , Glob::AllocRec::Get(strg).GetResponse()
                , attrib.data()
                , int(attrib.size()));
            MFX_CHECK_STS(sts);

            auto& info = Tmp::BSAllocInfo::GetOrConstruct(local);

            // request linear buffer
            info.Info.FourCC = MFX_FOURCC_P8;

            // context_id required for allocation video memory (temp  solution)
            info.AllocId      = m_vaContextEncode;
            info.Info.Width   = rsPar.FrameWidth * 2;
            info.Info.Height  = rsPar.FrameHeight;

            return MFX_ERR_NONE;
        });

        Push(BLK_Register
            , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
        {
            auto& res = Glob::DDI_Resources::Get(strg);

            auto itBs = std::find_if(res.begin(), res.end()
                , [](decltype(*res.begin()) r)
            {
                return r.Function == MFX_FOURCC_P8;
            });
            MFX_CHECK(itBs != res.end(), MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(itBs->Resource.Size == sizeof(VABufferID), MFX_ERR_UNDEFINED_BEHAVIOR);

            VABufferID* pBsBegin = (VABufferID*)itBs->Resource.pData;
            m_bs.assign(pBsBegin, pBsBegin + itBs->Resource.Num);

            mfxStatus sts = CreateVABuffers(
                Tmp::DDI_InitParam::Get(local)
                , m_perSeqPar);
            MFX_CHECK_STS(sts);

            return MFX_ERR_NONE;
        });
}

void DDI_VA::ResetState(const FeatureBlocks& /*blocks*/, TPushRS Push)
{
    Push(BLK_Reset
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
        {
            mfxStatus sts;

            m_callVa = Glob::DDI_Execute::Get(strg);

            sts = DestroyVABuffers(m_perSeqPar);
            MFX_CHECK_STS(sts);

            sts = CreateVABuffers(Tmp::DDI_InitParam::Get(local), m_perSeqPar);
            MFX_CHECK_STS(sts);

            return MFX_ERR_NONE;
        });
}

void DDI_VA::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_SubmitTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        mfxStatus sts;
        auto& task = Task::Common::Get(s_task);

        m_callVa = Glob::DDI_Execute::Get(global);

        MFX_CHECK((task.SkipCMD & SKIPCMD_NeedDriverCall), MFX_ERR_NONE);

        sts = DestroyVABuffers(m_perPicPar);
        MFX_CHECK_STS(sts);

        sts = CreateVABuffers(Glob::DDI_SubmitParam::Get(global), m_perPicPar);
        MFX_CHECK_STS(sts);

        sts = BeginPicture(task.HDLRaw.first);
        MFX_CHECK_STS(sts);

        sts = RenderPicture(m_perPicPar.data(), (int)m_perPicPar.size());
        MFX_CHECK_STS(sts);

        sts = RenderPicture(m_perSeqPar.data(), (int)m_perSeqPar.size());
        MFX_CHECK_STS(sts);

        sts = EndPicture();
        MFX_CHECK_STS(sts);

        MFX_LTRACE_2(MFX_TRACE_LEVEL_HOTSPOTS
            , "A|ENCODE|AV1|PACKET_END|", "%d|%d"
            , m_vaContextEncode, task.StatusReportId);

        return MFX_ERR_NONE;
    });
}

void DDI_VA::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_QueryTask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& task = Task::Common::Get(s_task);

        m_callVa = Glob::DDI_Execute::Get(global);

        MFX_CHECK((task.SkipCMD & SKIPCMD_NeedDriverCall), MFX_ERR_NONE);

        return QueryStatus(Glob::DDI_Feedback::Get(global), task.StatusReportId);
    });
}

mfxStatus DDI_VA::QueryCaps()
{
    m_caps = {};

    std::map<VAConfigAttribType, int> idx_map;
    VAConfigAttribType attr_types[] =
    {
        VAConfigAttribRTFormat
        , VAConfigAttribRateControl
        , VAConfigAttribMaxPictureHeight
        , VAConfigAttribMaxPictureWidth
        , VAConfigAttribEncMaxRefFrames
        , VAConfigAttribEncTileSupport
        , VAConfigAttribEncDynamicScaling
        , VAConfigAttribMaxFrameSize
        , (VAConfigAttribType)VAConfigAttribEncAV1
        , (VAConfigAttribType)VAConfigAttribEncAV1Ext1
        , (VAConfigAttribType)VAConfigAttribEncAV1Ext2
    };
    std::vector<VAConfigAttrib> attrs;
    auto AV = [&](VAConfigAttribType t) {return attrs[idx_map[t]].value; };

    mfxI32 i = 0;
    std::for_each(std::begin(attr_types), std::end(attr_types)
        , [&](decltype(*std::begin(attr_types)) type)
    {
        attrs.push_back({ type, 0 });
        idx_map[type] = i;
        i++;
    });

    auto sts = MfxEncodeHW::DeviceVAAPI::QueryCaps(attrs.data(), attrs.size() * sizeof(VAConfigAttrib));
    MFX_CHECK_STS(sts);

    m_caps.ChromaSupportFlags.fields.i420 = !!(AV(VAConfigAttribRTFormat) & VA_RT_FORMAT_YUV420);
    m_caps.ChromaSupportFlags.fields.RGB = !!(AV(VAConfigAttribRTFormat) & VA_RT_FORMAT_RGB32);

    m_caps.BitDepthSupportFlags.fields.eight_bits = !!(AV(VAConfigAttribRTFormat) & VA_RT_FORMAT_YUV420);
    m_caps.BitDepthSupportFlags.fields.ten_bits   = !!(AV(VAConfigAttribRTFormat) & VA_RT_FORMAT_YUV420_10BPP);

    m_caps.msdk.CBRSupport   = !!(AV(VAConfigAttribRateControl) & VA_RC_CBR);
    m_caps.msdk.VBRSupport   = !!(AV(VAConfigAttribRateControl) & VA_RC_VBR);
    m_caps.msdk.CQPSupport   = !!(AV(VAConfigAttribRateControl) & VA_RC_CQP);
    m_caps.msdk.ICQSupport   = !!(AV(VAConfigAttribRateControl) & VA_RC_ICQ);

    m_caps.UserMaxFrameSizeSupport = !!(AV(VAConfigAttribMaxFrameSize));

#if VA_CHECK_VERSION(1, 10, 0)
    m_caps.SupportedRateControlMethods.fields.TCBRCSupport = !!(AV(VAConfigAttribRateControl) & VA_RC_TCBRC);
#endif

    MFX_CHECK(AV(VAConfigAttribMaxPictureWidth) != VA_ATTRIB_NOT_SUPPORTED, MFX_ERR_UNSUPPORTED);
    MFX_CHECK(AV(VAConfigAttribMaxPictureHeight) != VA_ATTRIB_NOT_SUPPORTED, MFX_ERR_UNSUPPORTED);
    MFX_CHECK_COND(AV(VAConfigAttribMaxPictureWidth) && AV(VAConfigAttribMaxPictureHeight));
    m_caps.MaxPicWidth = AV(VAConfigAttribMaxPictureWidth);
    m_caps.MaxPicHeight = AV(VAConfigAttribMaxPictureHeight);

    if (AV(VAConfigAttribEncMaxRefFrames) != VA_ATTRIB_NOT_SUPPORTED)
    {
        m_caps.MaxNum_ReferenceL0_P = mfxU8(AV(VAConfigAttribEncMaxRefFrames) & 0xFF);
        m_caps.MaxNum_ReferenceL1_B = mfxU8((AV(VAConfigAttribEncMaxRefFrames) >> 16) & 0xFF);
        m_caps.MaxNum_ReferenceL0_B = m_caps.MaxNum_ReferenceL0_P - m_caps.MaxNum_ReferenceL1_B;
    }
    else
    {
        m_caps.MaxNum_ReferenceL0_P = 2;
        m_caps.MaxNum_ReferenceL0_B = 2;
        m_caps.MaxNum_ReferenceL1_B = 1;
    }

    m_caps.FrameOBUSupport           = 1;
    m_caps.ForcedSegmentationSupport = 1;
    m_caps.AV1ToolSupportFlags.fields.enable_order_hint = 1;
    m_caps.AV1ToolSupportFlags.fields.enable_cdef       = 1;

    auto attribValEncAV1 = *(VAConfigAttribValEncAV1 *)(&attrs[idx_map[(VAConfigAttribType)VAConfigAttribEncAV1]].value);
    m_caps.CDEFChannelStrengthSupport = attribValEncAV1.bits.support_cdef_channel_strength ? 1 : 0;
    m_caps.AV1ToolSupportFlags.fields.PaletteMode = attribValEncAV1.bits.support_palette_mode ? 1 : 0;
    m_caps.AV1ToolSupportFlags.fields.allow_intrabc = attribValEncAV1.bits.support_allow_intrabc ? 1 : 0;
    
    auto attribValEncAV1Ext1 = *(VAConfigAttribValEncAV1Ext1 *)(&attrs[idx_map[(VAConfigAttribType)VAConfigAttribEncAV1Ext1]].value);
    m_caps.SegmentFeatureSupport               = attribValEncAV1Ext1.bits.segment_feature_support;
    m_caps.MinSegIdBlockSizeAccepted           = attribValEncAV1Ext1.bits.min_segid_block_size_accepted;
    m_caps.SupportedInterpolationFilters.value = static_cast<mfxU8>(attribValEncAV1Ext1.bits.interpolation_filter);

    auto attribValEncAV1Ext2 = *(VAConfigAttribValEncAV1Ext2 *)(&attrs[idx_map[(VAConfigAttribType)VAConfigAttribEncAV1Ext2]].value);
    m_caps.TileSizeBytesMinus1 = attribValEncAV1Ext2.bits.tile_size_bytes_minus1;

    return MFX_ERR_NONE;
}

uint32_t DDI_VA::ConvertRateControlMFX2VAAPI(mfxU16 rateControl)
{
    static const std::map<mfxU16, uint32_t> RCMFX2VAAPI =
    {
        { mfxU16(MFX_RATECONTROL_CQP)   , uint32_t(VA_RC_CQP) },
        { mfxU16(MFX_RATECONTROL_CBR)   , uint32_t(VA_RC_CBR) },
        { mfxU16(MFX_RATECONTROL_VBR)   , uint32_t(VA_RC_VBR) },
        { mfxU16(MFX_RATECONTROL_ICQ)   , uint32_t(VA_RC_ICQ) },
    };

    auto itRC = RCMFX2VAAPI.find(rateControl);
    if (itRC != RCMFX2VAAPI.end())
    {
        return itRC->second;
    }

    return uint32_t(VA_RC_NONE);
}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
