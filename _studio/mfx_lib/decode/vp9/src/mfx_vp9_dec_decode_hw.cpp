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

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE)

#include "mfx_vp9_dec_decode.h"
#include "mfx_vp9_dec_decode_hw.h"

#include "umc_vp9_utils.h"
#include "umc_vp9_bitstream.h"
#include "umc_vp9_va_packer.h"

#include "mfx_common_decode_int.h"
#include "mfx_vpx_dec_common.h"
#include <libmfx_core_vaapi.h>


#include "umc_va_video_processing.h"

#include "libmfx_core_interface.h"
using namespace UMC_VP9_DECODER;

static bool IsSameVideoParam(mfxVideoParam *newPar, mfxVideoParam *oldPar);

// function checks hardware support with IsGuidSupported function and GUIDs
inline
bool CheckHardwareSupport(VideoCORE *p_core, mfxVideoParam *p_video_param)
{
    MFX_CHECK(p_core, false);
    MFX_CHECK(p_video_param, false);

GUID guid;

    mfxU32 profile = p_video_param->mfx.CodecProfile;

    if (!profile)
    {
        profile = UMC_VP9_DECODER::GetMinProfile(p_video_param->mfx.FrameInfo.BitDepthLuma, p_video_param->mfx.FrameInfo.ChromaFormat);
    }

    switch(profile)
    {
    case(MFX_PROFILE_VP9_0):
    {
        guid = DXVA_Intel_ModeVP9_Profile0_VLD;
        break;
    }
    case(MFX_PROFILE_VP9_1):
    {
        guid = DXVA_Intel_ModeVP9_Profile1_YUV444_VLD;
        break;
    }
    case(MFX_PROFILE_VP9_2):
    {
        guid = DXVA_Intel_ModeVP9_Profile2_10bit_VLD;
        break;
    }
    case(MFX_PROFILE_VP9_3):
    {
        guid = DXVA_Intel_ModeVP9_Profile3_YUV444_10bit_VLD;
        break;
    }
    default: return false;
    }

    mfxStatus mfxSts = p_core->IsGuidSupported(guid, p_video_param);
    MFX_CHECK(mfxSts == MFX_ERR_NONE, false);

    return true;
}

static void SetFrameType(const VP9DecoderFrame &frameInfo, mfxFrameSurface1 &surface_out)
{
    auto extFrameInfo = reinterpret_cast<mfxExtDecodedFrameInfo *>(GetExtendedBuffer(surface_out.Data.ExtParam, surface_out.Data.NumExtParam, MFX_EXTBUFF_DECODED_FRAME_INFO));
    if (extFrameInfo == nullptr)
        return;

    switch (frameInfo.frameType)
    {
    case KEY_FRAME:
        extFrameInfo->FrameType = MFX_FRAMETYPE_I;
        break;
    case INTER_FRAME:
        extFrameInfo->FrameType = MFX_FRAMETYPE_P;
        break;
    default:
        extFrameInfo->FrameType = MFX_FRAMETYPE_UNKNOWN;
    }
}

#ifdef UMC_VA_DXVA

// The class looks for the first free DXVA index.
// E.g., for the simplest stream it uses only two indices zero and one,
// decode #0 Intra-frame refs -1 curr frame 0
// decode #1 refs 0 curr frame 1
// decode #2 refs 1 curr frame 0
class FirstFreeIndexRemapper : public DXVAIndexRemapper {
private:
    std::map<UMC::FrameMemID, UCHAR> m_idToIndexMap;
public:
    UCHAR GetDXVAIndex(UMC::FrameMemID memId) override
    {
        return (m_idToIndexMap.find(memId) != m_idToIndexMap.end()) ? m_idToIndexMap[memId] : UCHAR_MAX;
    }

    void UpdateDXVAIndices(const UMC::FrameMemID currFrame, const UMC::FrameMemID refs[], int refsSize) override
    {
        // Use DXVA indices in range [0; refsSize]
        // refsSize slots for different reference frames [0; refsSize-1]
        // plus one slot for current frame
        std::set<UCHAR> freeSlots;
        for (int i = 0; i <= refsSize; i += 1)
        {
            freeSlots.insert((UCHAR)i);
        }

        std::map<UMC::FrameMemID, UCHAR> newMap;
        // propagate previously assigned indices to the new state
        for (mfxU8 idx = 0; idx < refsSize; idx += 1)
        {
            UMC::FrameMemID memId = refs[idx];
            if (memId != (UMC::FrameMemID) - 1)
            {
                UCHAR prevIndex = GetDXVAIndex(memId);
                newMap[memId] = prevIndex;
                freeSlots.erase(prevIndex);
            }
        }

        // assigne the first free index to the current frame
        newMap[currFrame] = *freeSlots.begin();

        m_idToIndexMap = newMap;
    }
};

// Unique remapper stores all used memIds and associated DXVA indices.
// Such techique has a limitation only 128 buffers can be used
// because DXVA index has only 7 bit.
// Such remapper is used on Skylake platform because on Skylake
// decoder doesn't work properly if the same index was assigned
// to two different surfaces.
class UniqueRemapper : public DXVAIndexRemapper {
private:
    std::map<UMC::FrameMemID, UCHAR> m_idToIndexMap;
public:
    UCHAR GetDXVAIndex(UMC::FrameMemID memId) override
    {
        if (m_idToIndexMap.find(memId) == m_idToIndexMap.end())
        {
            // Add new surface to the map
            m_idToIndexMap[memId] = (UCHAR)m_idToIndexMap.size();
        }

        if (m_idToIndexMap.size() > 128)
        {
            return UCHAR_MAX;
        }

        return m_idToIndexMap[memId];
    }

    void UpdateDXVAIndices(const UMC::FrameMemID currFrame, const UMC::FrameMemID refs[], int refsSize) override
    {
        (void)currFrame;
        (void)refs;
        (void)refsSize;
    }
};

#endif

mfxStatus VideoDECODEVP9_HW::UpdateRefFrames()
{
    if (m_frameInfo.show_existing_frame)
        return MFX_ERR_NONE;

    for (mfxI32 ref_index = 0; ref_index < NUM_REF_FRAMES; ++ref_index)
    {
        mfxU8 mask = m_frameInfo.refreshFrameFlags & (1 << ref_index);
        if (mask != 0 // we should update the reference according to the bitstream data

             // The next condition is for decoder robustness.
            // After the first keyframe is decoded this frame occupies all ref slots
            // its mask is 011111111 and the condition "minus one" never hits.
            // But if the first frame is Inter-frame encoded in the intra-only mode
            // then we can receive "minus one" ref_frame_map item.
            // The decoder never should touch these references.
            // But if the stream is broken then decode may touch not initialized reference
            // So to prevent crash let's put something as the reference frame.
            || (m_frameInfo.ref_frame_map[ref_index] == (UMC::FrameMemID) - 1))
        {
            MFX_CHECK((m_surface_source->IncreaseReference(m_frameInfo.currFrame) == UMC::UMC_OK), MFX_ERR_UNKNOWN);

            if (m_frameInfo.ref_frame_map[ref_index] >= 0)
                MFX_CHECK((m_surface_source->DecreaseReference(m_frameInfo.ref_frame_map[ref_index]) == UMC::UMC_OK), MFX_ERR_UNKNOWN);
            m_frameInfo.ref_frame_map[ref_index] = m_frameInfo.currFrame;

            m_frameInfo.sizesOfRefFrame[ref_index].width = m_frameInfo.width;
            m_frameInfo.sizesOfRefFrame[ref_index].height = m_frameInfo.height;
        }
    }
    return MFX_ERR_NONE;
}
mfxStatus VideoDECODEVP9_HW::CleanRefList()
{
    for (mfxI32 ref_index = 0; ref_index < NUM_REF_FRAMES; ++ref_index)
    {
        if (m_frameInfo.ref_frame_map[ref_index] >= 0)
            MFX_CHECK((m_surface_source->DecreaseReference(m_frameInfo.ref_frame_map[ref_index]) == UMC::UMC_OK), MFX_ERR_UNKNOWN);

        m_frameInfo.ref_frame_map[ref_index] = -1;
    }
    return MFX_ERR_NONE;
}


class FrameStorage
{
private:
    std::vector<UMC_VP9_DECODER::VP9DecoderFrame> m_submittedFrames;
    SurfaceSource *surface_source;

    void LockResources(const UMC_VP9_DECODER::VP9DecoderFrame & frame) const
    {
        // lock current frame for decode
        VP9_CHECK_AND_THROW((surface_source->IncreaseReference(frame.currFrame) == UMC::UMC_OK), MFX_ERR_UNKNOWN);
        // lock frame for copy
        if (frame.show_existing_frame)
        {
            VP9_CHECK_AND_THROW((surface_source->IncreaseReference(frame.ref_frame_map[frame.frame_to_show]) == UMC::UMC_OK), MFX_ERR_UNKNOWN);
        }
        // lock all references
        else
        {
            for (mfxI32 ref_index = 0; ref_index < NUM_REF_FRAMES; ++ref_index)
            {
                const UMC::FrameMemID mid = frame.ref_frame_map[ref_index];
                if (mid >= 0)
                {
                    VP9_CHECK_AND_THROW((surface_source->IncreaseReference(mid) == UMC::UMC_OK), MFX_ERR_UNKNOWN);
                }
            }
        }
    }

    void UnLockResources(const UMC_VP9_DECODER::VP9DecoderFrame & frame) const
    {
        // decoded frame should be unlocked in async routine

         // unlock frame for copy
        if (frame.show_existing_frame)
        {
            VP9_CHECK_AND_THROW((surface_source->DecreaseReference(frame.ref_frame_map[frame.frame_to_show]) == UMC::UMC_OK), MFX_ERR_UNKNOWN);
        }
        // unlock all references
        else
        {
            for (mfxI32 ref_index = 0; ref_index < NUM_REF_FRAMES; ++ref_index)
            {
                const UMC::FrameMemID mid = frame.ref_frame_map[ref_index];
                if (mid >= 0)
                {
                    VP9_CHECK_AND_THROW((surface_source->DecreaseReference(mid) == UMC::UMC_OK), MFX_ERR_UNKNOWN);
                }
            }
        }
    }
public:
    FrameStorage(SurfaceSource *frameAllocator) :
        m_submittedFrames(),
        surface_source(frameAllocator)
    {
    }

    ~FrameStorage()
    {
        // unlock all locked resources
        try
        {
            for (auto & it : m_submittedFrames)
                UnLockResources(it);
        }
        catch (vp9_exception const& e)
        {
            m_submittedFrames.shrink_to_fit();
            m_submittedFrames.clear();
            VM_ASSERT(0);
        }
        m_submittedFrames.shrink_to_fit();
        m_submittedFrames.clear();
    }

    void Add(UMC_VP9_DECODER::VP9DecoderFrame & frame)
    {
        VP9_CHECK_AND_THROW((frame.currFrame >= 0), MFX_ERR_UNKNOWN);

        // lock refereces for current frame
        LockResources(frame);

        // avoid double submit
        auto find_it = std::find_if(m_submittedFrames.begin(), m_submittedFrames.end(),
            [frame](const UMC_VP9_DECODER::VP9DecoderFrame & item) { return item.currFrame == frame.currFrame; });
        VP9_CHECK_AND_THROW((find_it == m_submittedFrames.end()), MFX_ERR_UNKNOWN);

        m_submittedFrames.push_back(frame);
    }

    void DecodeFrame(UMC::FrameMemID frameId)
    {
        auto find_it = std::find_if(m_submittedFrames.begin(), m_submittedFrames.end(),
            [frameId](const UMC_VP9_DECODER::VP9DecoderFrame & item) { return item.currFrame == frameId; });

        if (find_it != m_submittedFrames.end())
        {
            find_it->isDecoded = true;
        }
    }

    void CompleteFrames()
    {
        for (auto it = m_submittedFrames.begin(); it != m_submittedFrames.end(); )
        {
            if (it->isDecoded)
            {
                UnLockResources(*it);
                it = m_submittedFrames.erase(it);
            }
            else
                it = std::next(it);
        }
    }

    bool IsAllFramesCompleted() const
    {
        return m_submittedFrames.empty();
    }
};

VideoDECODEVP9_HW::VideoDECODEVP9_HW(VideoCORE *p_core, mfxStatus *sts)
    : m_isInit(false),
      m_is_opaque_memory(false),
      m_core(p_core),
      m_platform(MFX_PLATFORM_HARDWARE),
      m_num_output_frames(0),
      m_in_framerate(0),
      m_frameOrder(0),
      m_statusReportFeedbackNumber(0),
      m_mGuard(),
      m_mCopyGuard(),
      m_adaptiveMode(false),
      m_index(0),
      m_surface_source(),
#ifdef UMC_VA_DXVA
      m_dxvaRemapper(),
#endif
      m_Packer(),
      m_request(),
      m_response(),
      m_stat(),
      m_va(nullptr),
      m_completedList(),
      m_firstSizes(),
      m_bs(),
      m_baseQIndex(0)
{
    memset(&m_sizesOfRefFrame, 0, sizeof(m_sizesOfRefFrame));
    memset(&m_frameInfo.ref_frame_map, VP9_INVALID_REF_FRAME, sizeof(m_frameInfo.ref_frame_map)); // TODO: move to another place
    ResetFrameInfo();

    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

VideoDECODEVP9_HW::~VideoDECODEVP9_HW(void)
{
    Close();
}


static bool CheckVP9BitDepthRestriction(mfxFrameInfo *info)
{
    return ((info->BitDepthLuma == FourCcBitDepth(info->FourCC)) &&
     (info->BitDepthLuma == info->BitDepthChroma));
}

mfxStatus VideoDECODEVP9_HW::Init(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(!m_isInit, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxStatus sts   = MFX_ERR_NONE;
    UMC::Status umcSts   = UMC::UMC_OK;
    eMFXHWType type = m_core->GetHWType();

    m_platform = m_core->GetPlatformType();

#ifdef UMC_VA_DXVA
    if (type == MFX_HW_SCL) {
        m_dxvaRemapper = std::make_unique<UniqueRemapper>();
    }
    else {
        m_dxvaRemapper = std::make_unique<FirstFreeIndexRemapper>();
    }
#endif

    MFX_CHECK(MFX_ERR_NONE <= CheckVideoParamDecoders(par, m_core->IsExternalFrameAllocator(), type, m_core->IsCompatibleForOpaq()), MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(CheckHardwareSupport(m_core, par), MFX_ERR_UNSUPPORTED);

    MFX_CHECK(MFX_VPX_Utility::CheckVideoParam(par, MFX_CODEC_VP9, m_platform, type), MFX_ERR_INVALID_VIDEO_PARAM);

    m_vInitPar = *par;

    MFX_CHECK(InitBitDepthFields(&m_vInitPar.mfx.FrameInfo), MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(CheckVP9BitDepthRestriction(&m_vInitPar.mfx.FrameInfo), MFX_ERR_INVALID_VIDEO_PARAM);

    m_vPar = m_vInitPar;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    /* There are following conditions for post processing via HW fixed function engine:
     * (1): Progressive only for ICL and Before, interlace is supported for ICL following patforms
     * (2): Supported on SKL (Core) and APL (Atom) platform and above
     * (3): Only video memory supported (so, OPAQ memory does not supported!)
     * */
    if (videoProcessing)
    {
        MFX_CHECK(m_core->GetHWType() >= MFX_HW_SCL &&
                  (m_vPar.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY),
                  MFX_ERR_UNSUPPORTED);

        //PicStruct support differs, need to check per-platform
        if (m_core->GetHWType() <= MFX_HW_ICL_LP)
        {
           MFX_CHECK(m_vPar.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE, MFX_ERR_UNSUPPORTED);
        }
        MFX_CHECK(par->mfx.FrameInfo.FourCC == videoProcessing->Out.FourCC, MFX_ERR_UNSUPPORTED);//This is to avoid CSC cases, will remove once CSC is fully tested
        bool is_fourcc_supported = false;
        if (m_core->GetHWType() < MFX_HW_TGL_LP)
        {
            is_fourcc_supported =
                      (  videoProcessing->Out.FourCC == MFX_FOURCC_RGB4
                      || videoProcessing->Out.FourCC == MFX_FOURCC_NV12
                      || videoProcessing->Out.FourCC == MFX_FOURCC_P010
                      || videoProcessing->Out.FourCC == MFX_FOURCC_YUY2
                      || videoProcessing->Out.FourCC == MFX_FOURCC_AYUV);
        }
        else
        {
            is_fourcc_supported =
                      (  videoProcessing->Out.FourCC == MFX_FOURCC_RGB4
                      || videoProcessing->Out.FourCC == MFX_FOURCC_NV12
                      || videoProcessing->Out.FourCC == MFX_FOURCC_P010
                      || videoProcessing->Out.FourCC == MFX_FOURCC_YUY2
                      || videoProcessing->Out.FourCC == MFX_FOURCC_AYUV
#if (MFX_VERSION >= 1027)
                      || videoProcessing->Out.FourCC == MFX_FOURCC_Y410
                      || videoProcessing->Out.FourCC == MFX_FOURCC_Y210
#endif
#if (MFX_VERSION >= 1031)
                      || videoProcessing->Out.FourCC == MFX_FOURCC_Y216
                      || videoProcessing->Out.FourCC == MFX_FOURCC_Y416
                      || videoProcessing->Out.FourCC == MFX_FOURCC_P016
#endif
                      );
        }
       MFX_CHECK(is_fourcc_supported,MFX_ERR_UNSUPPORTED);
    }
#endif
    m_vInitPar.IOPattern = (
           m_vInitPar.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        | (m_vInitPar.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
        ;

    if (0 == m_vInitPar.mfx.FrameInfo.FrameRateExtN || 0 == m_vInitPar.mfx.FrameInfo.FrameRateExtD)
    {
        m_vInitPar.mfx.FrameInfo.FrameRateExtD = 1000;
        m_vInitPar.mfx.FrameInfo.FrameRateExtN = 30000;
    }

    m_in_framerate = (mfxF64) m_vInitPar.mfx.FrameInfo.FrameRateExtD / m_vInitPar.mfx.FrameInfo.FrameRateExtN;

    if ((m_vPar.mfx.FrameInfo.AspectRatioH == 0) && (m_vPar.mfx.FrameInfo.AspectRatioW == 0))
    {
        m_vPar.mfx.FrameInfo.AspectRatioH = 1;
        m_vPar.mfx.FrameInfo.AspectRatioW = 1;
    }

    // allocate memory
    memset(&m_request, 0, sizeof(m_request));
    memset(&m_response, 0, sizeof(m_response));
    memset(&m_response_alien, 0, sizeof(m_response_alien));
    memset(&m_firstSizes, 0, sizeof(m_firstSizes));

    sts = MFX_VPX_Utility::QueryIOSurfInternal(&m_vInitPar, &m_request);
    MFX_CHECK_STS(sts);

    mfxFrameAllocRequest request_internal = m_request;

    {
        m_request.AllocId = par->AllocId;
    }

    MFX_CHECK_STS(sts);

    try
    {
        m_surface_source.reset(new SurfaceSource(m_core, *par, m_platform, m_request, request_internal, m_response, m_response_alien, 
        nullptr,
        m_is_opaque_memory));
    }
    catch (const mfx::mfxStatus_exception& ex)
    {
        MFX_CHECK_STS(ex.sts);
    }

    ResetFrameInfo();

    sts = m_core->CreateVA(&m_vInitPar, &m_request, &m_response, m_surface_source.get());
    MFX_CHECK_STS(sts);

    m_core->GetVA((mfxHDL*)&m_va, MFX_MEMTYPE_FROM_DECODE);

    bool isUseExternalFrames = (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) || m_is_opaque_memory;
    bool reallocFrames = (par->mfx.EnableReallocRequest == MFX_CODINGOPTION_ON);
    m_adaptiveMode = reallocFrames;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    if (videoProcessing)
    {
        MFX_CHECK(MFX_HW_D3D11 == m_core->GetVAType(), MFX_ERR_UNSUPPORTED);
        if (m_va->GetVideoProcessingVA())
        {
            umcSts = m_va->GetVideoProcessingVA()->Init(par, videoProcessing);
            MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_INVALID_VIDEO_PARAM);
        }
    }
#endif
    m_frameOrder = 0;
    m_statusReportFeedbackNumber = 0;
    m_isInit = true;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::QueryImplsDescription(
    VideoCORE&,
    mfxDecoderDescription::decoder& caps,
    mfx::PODArraysHolder& ah)
{
    const mfxU32 SupportedProfiles[] =
    {
        MFX_PROFILE_VP9_0
        , MFX_PROFILE_VP9_1
        , MFX_PROFILE_VP9_2
        , MFX_PROFILE_VP9_3
    };
    const mfxResourceType SupportedMemTypes[] =
    {
        MFX_RESOURCE_SYSTEM_SURFACE
        , MFX_RESOURCE_VA_SURFACE
    };
    const mfxU32 SupportedFourCC[] =
    {
        MFX_FOURCC_NV12
        , MFX_FOURCC_P010
        , MFX_FOURCC_P016
        , MFX_FOURCC_AYUV
        , MFX_FOURCC_Y410
        , MFX_FOURCC_Y416
    };

    caps.CodecID = MFX_CODEC_VP9;
    caps.MaxcodecLevel = MFX_LEVEL_UNKNOWN;

    mfxStatus sts = MFX_ERR_NONE;
    for (mfxU32 profile : SupportedProfiles)
    {
        auto& pfCaps = ah.PushBack(caps.Profiles);
        pfCaps.Profile = profile;

        for (auto memType : SupportedMemTypes)
        {
            auto& memCaps = ah.PushBack(pfCaps.MemDesc);
            memCaps.MemHandleType = memType;
            memCaps.Width = { 16, 16384, 16 };
            memCaps.Height = { 16, 16384, 16 };

            for (auto fcc : SupportedFourCC)
            {
                ah.PushBack(memCaps.ColorFormats) = fcc;
                ++memCaps.NumColorFormats;
            }
            ++pfCaps.NumMemTypes;
        }
        ++caps.NumProfiles;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::Reset(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(par);

    eMFXHWType type = m_core->GetHWType();

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(m_vInitPar.ExtParam, m_vInitPar.NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);

    if (videoProcessing != nullptr)
    {
        if (videoProcessing->Out.Width >= par->mfx.FrameInfo.Width ||
            videoProcessing->Out.Height >= par->mfx.FrameInfo.Height)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }
#endif

    MFX_CHECK(MFX_ERR_NONE <= CheckVideoParamDecoders(par, m_core->IsExternalFrameAllocator(), type, m_core->IsCompatibleForOpaq()), MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(MFX_VPX_Utility::CheckVideoParam(par, MFX_CODEC_VP9, m_core->GetPlatformType(), type), MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(CheckHardwareSupport(m_core, par), MFX_ERR_UNSUPPORTED);

    MFX_CHECK(IsSameVideoParam(par, &m_vInitPar), MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);


    MFX_CHECK(m_platform == m_core->GetPlatformType(), MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    ResetFrameInfo();
    UMC::Status sts = m_surface_source->Reset();
    MFX_CHECK(sts == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);

    m_frameOrder = 0;
    m_statusReportFeedbackNumber = 0;
    memset(&m_stat, 0, sizeof(m_stat));
    memset(&m_firstSizes, 0, sizeof(m_firstSizes));

    m_vPar = *par;

    if (0 == m_vPar.mfx.FrameInfo.FrameRateExtN || 0 == m_vPar.mfx.FrameInfo.FrameRateExtD)
    {
        m_vPar.mfx.FrameInfo.FrameRateExtD = m_vInitPar.mfx.FrameInfo.FrameRateExtD;
        m_vPar.mfx.FrameInfo.FrameRateExtN = m_vInitPar.mfx.FrameInfo.FrameRateExtN;
    }

    m_in_framerate = (mfxF64) m_vPar.mfx.FrameInfo.FrameRateExtD / m_vPar.mfx.FrameInfo.FrameRateExtN;
    m_index = 0;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::Close()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    ResetFrameInfo();
#ifdef UMC_VA_DXVA
    m_dxvaRemapper.reset(nullptr);
#endif
    m_surface_source->Close();

    m_isInit = false;
    m_adaptiveMode = false;
    m_is_opaque_memory = false;

    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;
    m_statusReportFeedbackNumber = 0;

    m_va = NULL;

    memset(&m_request, 0, sizeof(m_request));
    memset(&m_response, 0, sizeof(m_response));
    memset(&m_stat, 0, sizeof(m_stat));

    return MFX_ERR_NONE;
}

void VideoDECODEVP9_HW::ResetFrameInfo()
{
    CleanRefList();
    m_framesStorage.reset(new FrameStorage(m_surface_source.get()));

    memset(&m_frameInfo, 0, sizeof(m_frameInfo));
    m_frameInfo.currFrame = -1;
    m_frameInfo.frameCountInBS = 0;
    m_frameInfo.currFrameInBS = 0;
    memset(&m_frameInfo.ref_frame_map, VP9_INVALID_REF_FRAME, sizeof(m_frameInfo.ref_frame_map)); // TODO: move to another place
}

mfxStatus VideoDECODEVP9_HW::DecodeHeader(VideoCORE* core, mfxBitstream* bs, mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);

    mfxStatus sts = MFX_VP9_Utility::DecodeHeader(core, bs, par);
    MFX_CHECK_STS(sts);

    return sts;
}

static bool IsSameVideoParam(mfxVideoParam *newPar, mfxVideoParam *oldPar)
{
    if (newPar->IOPattern != oldPar->IOPattern)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.BitDepthLuma != oldPar->mfx.FrameInfo.BitDepthLuma ||
        newPar->mfx.FrameInfo.BitDepthChroma != oldPar->mfx.FrameInfo.BitDepthChroma)
    {
        return false;
    }

    if (newPar->Protected != oldPar->Protected)
    {
        return false;
    }

    int32_t asyncDepth = std::min<int32_t>(newPar->AsyncDepth, MFX_MAX_ASYNC_DEPTH_VALUE);
    if (asyncDepth != oldPar->AsyncDepth)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.Height > oldPar->mfx.FrameInfo.Height)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.Width > oldPar->mfx.FrameInfo.Width)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.ChromaFormat != oldPar->mfx.FrameInfo.ChromaFormat)
    {
        return false;
    }

    if (newPar->mfx.NumThread > oldPar->mfx.NumThread && oldPar->mfx.NumThread > 0) //  need more surfaces for efficient decoding
    {
        return false;
    }

    return true;
}

mfxTaskThreadingPolicy VideoDECODEVP9_HW::GetThreadingPolicy()
{
    return MFX_TASK_THREADING_SHARED;
}

mfxStatus VideoDECODEVP9_HW::Query(VideoCORE *p_core, mfxVideoParam *p_in, mfxVideoParam *p_out)
{
    MFX_CHECK_NULL_PTR1(p_out);

    mfxVideoParam localIn{};
    if (p_in == p_out)
    {
        MFX_CHECK_NULL_PTR1(p_in);
        MFX_INTERNAL_CPY(&localIn, p_in, sizeof(mfxVideoParam));
        p_in = &localIn;
    }

    eMFXHWType type = p_core->GetHWType();
    mfxVideoParam *p_check_hw_par = p_in;

    mfxVideoParam check_hw_par{};
    if (p_in == NULL)
    {
        check_hw_par.mfx.CodecId = MFX_CODEC_VP9;
        p_check_hw_par = &check_hw_par;
    }

    MFX_CHECK(CheckHardwareSupport(p_core, p_check_hw_par), MFX_ERR_UNSUPPORTED);

    mfxStatus status = MFX_VPX_Utility::Query(p_core, p_in, p_out, MFX_CODEC_VP9, type);

    if (p_in == NULL)
    {
        p_out->mfx.EnableReallocRequest = 1;
    }
    else
    {
        // Disable DRC realloc surface, unless the user explicitly enable it.
        p_out->mfx.EnableReallocRequest = MFX_CODINGOPTION_OFF;

        if (p_in->mfx.EnableReallocRequest == MFX_CODINGOPTION_ON)
        {
            p_out->mfx.EnableReallocRequest = MFX_CODINGOPTION_ON;
        }
    }

    return status;
}

mfxStatus VideoDECODEVP9_HW::QueryIOSurf(VideoCORE *p_core, mfxVideoParam *p_video_param, mfxFrameAllocRequest *p_request)
{

    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR3(p_core, p_video_param, p_request);

    const mfxVideoParam p_params = *p_video_param;

    auto const supportedMemoryType =
           (p_params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        || (p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
        ;

    MFX_CHECK(supportedMemoryType, MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(!(
        p_params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY &&
        p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY),
        MFX_ERR_INVALID_VIDEO_PARAM);


    if (p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        p_request->Info = p_params.mfx.FrameInfo;
        p_request->NumFrameMin = 1;
        p_request->NumFrameSuggested = p_request->NumFrameMin + (p_params.AsyncDepth ? p_params.AsyncDepth : MFX_AUTO_ASYNC_DEPTH_VALUE);
        p_request->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        sts = MFX_VPX_Utility::QueryIOSurfInternal(p_video_param, p_request);
    }

    {
        p_request->Type |= MFX_MEMTYPE_EXTERNAL_FRAME;
    }

    MFX_CHECK(CheckHardwareSupport(p_core, p_video_param), MFX_ERR_UNSUPPORTED);

    return sts;
}

mfxStatus VideoDECODEVP9_HW::GetVideoParam(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(par);

    par->mfx = m_vPar.mfx;

    par->Protected = m_vInitPar.Protected;
    par->IOPattern = m_vInitPar.IOPattern;
    par->AsyncDepth = m_vInitPar.AsyncDepth;

    par->mfx.FrameInfo.FrameRateExtD = m_vInitPar.mfx.FrameInfo.FrameRateExtD;
    par->mfx.FrameInfo.FrameRateExtN = m_vInitPar.mfx.FrameInfo.FrameRateExtN;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    if (videoProcessing)
    {
        mfxExtDecVideoProcessing * videoProcessingInternal = m_vPar.GetExtendedBuffer<mfxExtDecVideoProcessing>(MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
        *videoProcessing = *videoProcessingInternal;
    }
#endif

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::GetDecodeStat(mfxDecodeStat *pStat)
{
    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR1(pStat);

    m_stat.NumSkippedFrame = 0;
    m_stat.NumCachedFrame = 0;

    *pStat = m_stat;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::GetOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC::FrameMemID index)
{
    mfxFrameSurface1 *pNativeSurface = m_surface_source->GetSurface(index, surface_work, &m_vInitPar);

    MFX_CHECK(pNativeSurface, MFX_ERR_UNDEFINED_BEHAVIOR);

    *surface_out = m_is_opaque_memory ? m_core->GetOpaqSurface(pNativeSurface->Data.MemId) : pNativeSurface;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::GetUserData(mfxU8 *pUserData, mfxU32 *pSize, mfxU64 *pTimeStamp)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR3(pUserData, pSize, pTimeStamp);

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEVP9_HW::GetPayload(mfxU64 *pTimeStamp, mfxPayload *pPayload)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    MFX_CHECK_NULL_PTR3(pTimeStamp, pPayload, pPayload->Data);

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEVP9_HW::SetSkipMode(mfxSkipMode /*mode*/)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);

    return MFX_ERR_NONE;
}

void VideoDECODEVP9_HW::CalculateTimeSteps(mfxFrameSurface1 *p_surface)
{
    p_surface->Data.TimeStamp = GetMfxTimeStamp(m_num_output_frames * m_in_framerate);
    p_surface->Data.FrameOrder = m_num_output_frames;
    m_num_output_frames += 1;

    p_surface->Info.FrameRateExtD = m_vInitPar.mfx.FrameInfo.FrameRateExtD;
    p_surface->Info.FrameRateExtN = m_vInitPar.mfx.FrameInfo.FrameRateExtN;

    p_surface->Info.CropX = m_vInitPar.mfx.FrameInfo.CropX;
    p_surface->Info.CropY = m_vInitPar.mfx.FrameInfo.CropY;
    p_surface->Info.PicStruct = m_vInitPar.mfx.FrameInfo.PicStruct;

    p_surface->Info.AspectRatioH = 1;
    p_surface->Info.AspectRatioW = 1;
}

enum
{
    NUMBER_OF_STATUS = 32,
};

mfxStatus MFX_CDECL VP9DECODERoutine(void *p_state, void * /* pp_param */, mfxU32 /* thread_number */, mfxU32)
{
    VideoDECODEVP9_HW::VP9DECODERoutineData& data = *(VideoDECODEVP9_HW::VP9DECODERoutineData*)p_state;
    VideoDECODEVP9_HW& decoder = *data.decoder;

    if (data.copyFromFrame != UMC::FRAME_MID_INVALID)
    {
        MFX_CHECK(data.surface_work, MFX_ERR_UNDEFINED_BEHAVIOR);

        UMC::AutomaticUMCMutex guard(decoder.m_mGuard);

        mfxFrameSurface1 surfaceDst = *data.surface_work;
        surfaceDst.Info.Width = (surfaceDst.Info.CropW + 15) & ~0x0f;
        surfaceDst.Info.Height = (surfaceDst.Info.CropH + 15) & ~0x0f;

        mfxFrameSurface1 *surfaceSrc = decoder.m_surface_source->GetSurfaceByIndex(data.copyFromFrame);
        MFX_CHECK(surfaceSrc, MFX_ERR_UNDEFINED_BEHAVIOR);

        bool systemMemory = (decoder.m_vInitPar.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) != 0;
        bool opaqMemory = false;
        mfxU16 srcMemType = MFX_MEMTYPE_DXVA2_DECODER_TARGET;
        srcMemType |= (systemMemory || opaqMemory) ? MFX_MEMTYPE_INTERNAL_FRAME : MFX_MEMTYPE_EXTERNAL_FRAME;
        mfxU16 dstMemType = opaqMemory ? (mfxU16)MFX_MEMTYPE_INTERNAL_FRAME : (mfxU16)MFX_MEMTYPE_EXTERNAL_FRAME;
        dstMemType |= systemMemory ? MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_DXVA2_DECODER_TARGET;
        mfxStatus sts = MFX_ERR_NONE;
        {
            if (data.copyFromFrame >= (mfxI32)decoder.m_mCopyGuard.size())
            {
                decoder.m_mCopyGuard.resize(data.copyFromFrame + 1);
            }

            UMC::AutomaticUMCMutex guardCopy(decoder.m_mCopyGuard[data.copyFromFrame]);
            MFX_SAFE_CALL(decoder.m_core->DoFastCopyWrapper(&surfaceDst, dstMemType, surfaceSrc, srcMemType));
        }

        if (data.currFrameId != -1)
        {
            // This flag is allowing proper frame release in case of SW surfaces, when refcounter reaches zero after decrease
            decoder.m_surface_source->SetFreeSurfaceAllowedFlag(true);
            decoder.m_surface_source->DecreaseReference(data.currFrameId);
            decoder.m_surface_source->SetFreeSurfaceAllowedFlag(false);
        }
        decoder.m_framesStorage->DecodeFrame(data.currFrameId);

        return MFX_ERR_NONE;
    }


    {
        UMC::Status status = decoder.m_va->SyncTask(data.currFrameId);
        if (status != UMC::UMC_OK && status != UMC::UMC_ERR_TIMEOUT)
        {
            mfxStatus CriticalErrorStatus = (status == UMC::UMC_ERR_GPU_HANG) ? MFX_ERR_GPU_HANG : MFX_ERR_DEVICE_FAILED;
            decoder.SetCriticalErrorOccured(CriticalErrorStatus);
            return CriticalErrorStatus;
        }
    }

    MFX_SAFE_CALL(decoder.ReportDecodeStatus(data.surface_work));

    if ((decoder.m_vInitPar.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && data.showFrame)
    {
        MFX_CHECK(data.surface_work, MFX_ERR_UNDEFINED_BEHAVIOR);

        if (data.currFrameId >= (mfxI32)decoder.m_mCopyGuard.size())
        {
            decoder.m_mCopyGuard.resize(data.currFrameId + 1);
        }

        UMC::AutomaticUMCMutex guardCopy(decoder.m_mCopyGuard[data.currFrameId]);
        mfxStatus sts = decoder.m_surface_source->PrepareToOutput(data.surface_work, data.currFrameId, 0, false);
        MFX_CHECK_STS(sts);
    }

    if(decoder.m_va && data.surface_work)
        MFX_CHECK(!decoder.m_va->UnwrapBuffer(data.surface_work->Data.MemId), MFX_ERR_UNDEFINED_BEHAVIOR);

    UMC::AutomaticUMCMutex guard(decoder.m_mGuard);

    if (data.currFrameId != -1)
        decoder.m_surface_source->DecreaseReference(data.currFrameId);
    decoder.m_framesStorage->DecodeFrame(data.currFrameId);

    return MFX_TASK_DONE;
}

mfxStatus VP9CompleteProc(void *p_state, void * /* pp_param */, mfxStatus)
{
    delete (VideoDECODEVP9_HW::VP9DECODERoutineData*)p_state;
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::ReportDecodeStatus(mfxFrameSurface1* surface_work)
{
    std::ignore = surface_work;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::PrepareInternalSurface(UMC::FrameMemID &mid)
{
    UMC::VideoDataInfo videoInfo;

    UMC::ColorFormat const cf = GetUMCColorFormat_VP9(&m_frameInfo);

    UMC::Status umcSts = videoInfo.Init(m_vPar.mfx.FrameInfo.Width, m_vPar.mfx.FrameInfo.Height, cf, m_frameInfo.bit_depth);
    MFX_CHECK(UMC::UMC_OK == umcSts, MFX_ERR_MEMORY_ALLOC);

    UMC::Status umc_sts = m_surface_source->Alloc(&mid, &videoInfo, mfx_UMC_ReallocAllowed);
    if (UMC::UMC_ERR_NOT_ENOUGH_BUFFER == umc_sts && m_adaptiveMode)
    {
        mfxFrameSurface1 *surf = m_surface_source->GetSurfaceByIndex(mid);
        MFX_CHECK(surf, MFX_ERR_INVALID_HANDLE);

        surf->Info.Width = m_vPar.mfx.FrameInfo.Width;
        surf->Info.Height = m_vPar.mfx.FrameInfo.Height;

        if (VAAPIVideoCORE *vaapi_core_10 = dynamic_cast<VAAPIVideoCORE *>(m_core))
        {
            // Legacy MSDK 1.x case
            return vaapi_core_10->ReallocFrame(surf);
        }

        // MSDK 2.0 case
        VAAPIVideoCORE20* vaapi_core_20 = dynamic_cast<VAAPIVideoCORE20*>(m_core);
        MFX_CHECK_NULL_PTR1(vaapi_core_20);

        return vaapi_core_20->ReallocFrame(surf);
        }
    else
        MFX_CHECK(UMC::UMC_OK == umc_sts, MFX_ERR_MEMORY_ALLOC);

    return MFX_ERR_NONE;
}

static mfxStatus CheckFrameInfo(mfxFrameInfo const &currInfo, mfxFrameInfo &info)
{
    MFX_SAFE_CALL(CheckFrameInfoCommon(&info, MFX_CODEC_VP9));

    switch (info.FourCC)
    {
        case MFX_FOURCC_NV12:
        case MFX_FOURCC_AYUV:
#if (MFX_VERSION >= 1027)
        case MFX_FOURCC_Y410:
#endif
            break;
#if (MFX_VERSION >= 1031)
        case MFX_FOURCC_P016:
        case MFX_FOURCC_Y416:
#endif
        case MFX_FOURCC_P010:
#if (MFX_VERSION >= 1027)
        case MFX_FOURCC_Y210:
#endif
            MFX_CHECK(info.Shift == 1, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            break;
        default:
            MFX_CHECK_STS(MFX_ERR_INVALID_VIDEO_PARAM);
    }

    switch(info.ChromaFormat)
    {
        case MFX_CHROMAFORMAT_YUV420:
        case MFX_CHROMAFORMAT_YUV444:
            break;
        default:
            MFX_CHECK_STS(MFX_ERR_INVALID_VIDEO_PARAM);
    }

    MFX_CHECK(currInfo.FourCC == info.FourCC, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, MFX_ENTRY_POINT * p_entry_point)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    mfxStatus sts = MFX_ERR_NONE;
    UMC::FrameMemID repeateFrame = UMC::FRAME_MID_INVALID;

    MFX_CHECK(m_isInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(surface_out);
    *surface_out = nullptr;

    bool allow_null_work_surface = Supports20FeatureSet(*m_core);

    if (!allow_null_work_surface)
    {
        MFX_CHECK_NULL_PTR1(surface_work);
    }

    if (surface_work)
    {
        if (m_is_opaque_memory)
        {
            bool opaqueSfsIsEmpty = IsSurfaceEmpty(*surface_work);

            MFX_CHECK(opaqueSfsIsEmpty, MFX_ERR_UNDEFINED_BEHAVIOR);

            // work with the native (original) surface
            surface_work = GetOriginalSurface(surface_work);
            MFX_CHECK(surface_work, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        sts = CheckFrameInfo(m_vPar.mfx.FrameInfo, surface_work->Info);
        MFX_CHECK_STS(sts);

        sts = CheckFrameData(surface_work);
        MFX_CHECK_STS(sts);
    }

    do
    {
        try
        {
            m_framesStorage->CompleteFrames();
        }
        catch (vp9_exception const& e)
        {
            UMC::Status status = e.GetStatus();
            if (status != UMC::UMC_OK)
                return MFX_ERR_UNKNOWN;
        }

        if (NeedToReturnCriticalStatus(bs))
            return ReturningCriticalStatus();

        if (bs && !bs->DataLength)
            return MFX_ERR_MORE_DATA;

        if (bs == nullptr)
        {
            sts = CleanRefList();
            MFX_CHECK_STS(sts);

            if (m_framesStorage->IsAllFramesCompleted())
            {
                return MFX_ERR_MORE_DATA;
            }

            return MFX_WRN_DEVICE_BUSY;
        }

        sts = CheckBitstream(bs);
        MFX_CHECK_STS(sts);

        VP9DecoderFrame frameInfo = m_frameInfo;
        sts = DecodeFrameHeader(bs, frameInfo);
        MFX_CHECK_STS(sts);

        MFX_VP9_Utility::FillVideoParam(m_core->GetPlatformType(), frameInfo, m_vPar);

        if (surface_work)
        {
            // check bit depth change
            MFX_CHECK((m_vPar.mfx.FrameInfo.BitDepthLuma == surface_work->Info.BitDepthLuma &&
                m_vPar.mfx.FrameInfo.BitDepthChroma == surface_work->Info.BitDepthChroma),
                MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            // check resize
            if (m_vPar.mfx.FrameInfo.Width > surface_work->Info.Width ||
                m_vPar.mfx.FrameInfo.Height > surface_work->Info.Height)
            {
                MFX_CHECK(m_adaptiveMode, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
                return MFX_ERR_REALLOC_SURFACE;
            }
        }

        // possible system memory case. when external surface is enough, but internal is not.
        if (!m_adaptiveMode)
        {
            MFX_CHECK(
                m_vPar.mfx.FrameInfo.Width <= m_vInitPar.mfx.FrameInfo.Width &&
                m_vPar.mfx.FrameInfo.Height <= m_vInitPar.mfx.FrameInfo.Height,
                MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        }

        //update frame info
        m_frameInfo = frameInfo;
        m_index++;

        sts = m_surface_source->SetCurrentMFXSurface(surface_work, m_is_opaque_memory);
        MFX_CHECK_STS(sts);

        if (!m_surface_source->HasFreeSurface())
        {
            return MFX_WRN_DEVICE_BUSY;
        }

        sts = PrepareInternalSurface(m_frameInfo.currFrame);
        MFX_CHECK_STS(sts);

        if (!m_frameInfo.frameCountInBS)
        {
            bs->DataOffset += bs->DataLength;
            bs->DataLength = 0;
        }

        if (!m_frameInfo.show_existing_frame)
        {
            UMC::Status umcSts = m_va->BeginFrame(m_frameInfo.currFrame, 0);
            MFX_CHECK(UMC::UMC_OK == umcSts, MFX_ERR_DEVICE_FAILED);

            sts = PackHeaders(&m_bs, m_frameInfo);
            MFX_CHECK_STS(sts);

            umcSts = m_va->Execute();
            MFX_CHECK(UMC::UMC_OK == umcSts, MFX_ERR_DEVICE_FAILED);


            umcSts = m_va->EndFrame();
            MFX_CHECK(UMC::UMC_OK == umcSts, MFX_ERR_DEVICE_FAILED);
        }
        else
        {
            repeateFrame = m_frameInfo.ref_frame_map[m_frameInfo.frame_to_show];
            ++m_statusReportFeedbackNumber;
        }

        try
        {
            m_framesStorage->Add(m_frameInfo);
        }
        catch (vp9_exception const& e)
        {
            UMC::Status status = e.GetStatus();
            MFX_CHECK(status == UMC::UMC_OK, MFX_ERR_UNKNOWN);
        }

        // update list of references for next frame
        sts = UpdateRefFrames();
        MFX_CHECK_STS(sts);

        if (m_frameInfo.showFrame)
        {
            sts = GetOutputSurface(surface_out, surface_work, m_frameInfo.currFrame);
            MFX_CHECK_STS(sts);

            SetFrameType(m_frameInfo, **surface_out);

            (*surface_out)->Data.TimeStamp = bs->TimeStamp != static_cast<mfxU64>(MFX_TIMESTAMP_UNKNOWN) ? bs->TimeStamp : GetMfxTimeStamp(m_frameOrder * m_in_framerate);
            (*surface_out)->Data.Corrupted = 0;
            (*surface_out)->Data.FrameOrder = m_frameOrder;
            (*surface_out)->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            (*surface_out)->Info.CropW = (mfxU16)m_frameInfo.width;
            (*surface_out)->Info.CropH = (mfxU16)m_frameInfo.height;
            (*surface_out)->Info.AspectRatioW = m_vPar.mfx.FrameInfo.AspectRatioW;
            (*surface_out)->Info.AspectRatioH = m_vPar.mfx.FrameInfo.AspectRatioH;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
            mfxExtDecVideoProcessing *videoProcessing = (mfxExtDecVideoProcessing *)GetExtendedBuffer(m_vInitPar.ExtParam, m_vInitPar.NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
            if (videoProcessing)
            {
                (*surface_out)->Info.CropH = videoProcessing->Out.CropH;
                (*surface_out)->Info.CropW = videoProcessing->Out.CropW;
                (*surface_out)->Info.CropX = videoProcessing->Out.CropX;
                (*surface_out)->Info.CropY = videoProcessing->Out.CropY;
            }
#endif

            m_frameOrder++;
            sts = MFX_ERR_NONE;
        }
        else
        {
            sts = (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK;
            if (surface_work) // for model 3 we don't need to zero surface_out because we can alloc next frame inside decoder
            {
                surface_out = nullptr;
            }
            else // for model 3 if we need more surface we can alloc it inside decoder
            {
                if (m_vPar.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
                {
                    MFX_SAFE_CALL(ReportDecodeStatus(surface_work));
                }

                if (m_frameInfo.currFrame != -1)
                    m_surface_source->DecreaseReference(m_frameInfo.currFrame);
                m_framesStorage->DecodeFrame(m_frameInfo.currFrame);
            }
        }
    } while (sts == MFX_ERR_MORE_DATA_SUBMIT_TASK && !surface_work);

    p_entry_point->pRoutine = &VP9DECODERoutine;
    p_entry_point->pCompleteProc = &VP9CompleteProc;

    VP9DECODERoutineData* routineData = new VP9DECODERoutineData();
    routineData->decoder = this;
    routineData->currFrameId = m_frameInfo.currFrame;
    routineData->copyFromFrame = repeateFrame;
    routineData->index = m_index;
    routineData->showFrame = m_frameInfo.showFrame;

    if (surface_out)
        routineData->surface_work = *surface_out;

    p_entry_point->pState = routineData;
    p_entry_point->requiredNumThreads = 1;


    return sts;
}

mfxStatus VideoDECODEVP9_HW::DecodeSuperFrame(mfxBitstream *in, VP9DecoderFrame & info)
{
    mfxU32 frameSizes[8] = { 0 };
    mfxU32 frameCount = 0;

    m_bs = *in;

    MfxVP9Decode::ParseSuperFrameIndex(in->Data + in->DataOffset, in->DataLength, frameSizes, &frameCount);

    if (frameCount > 1)
    {
        if (info.frameCountInBS == 0) // first call we meet super frame
        {
            info.frameCountInBS = frameCount;
            info.currFrameInBS = 0;
        }

        // it is not what we met before
        MFX_CHECK(info.frameCountInBS == frameCount, MFX_ERR_UNDEFINED_BEHAVIOR);

        m_bs.DataLength = frameSizes[info.currFrameInBS];
        m_bs.DataOffset = in->DataOffset;
        for (mfxU32 i = 0; i < info.currFrameInBS; i++)
            m_bs.DataOffset += frameSizes[i];

        info.currFrameInBS++;
        if (info.currFrameInBS < info.frameCountInBS)
            return MFX_ERR_NONE;
    }

    info.currFrameInBS = 0;
    info.frameCountInBS = 0;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::DecodeFrameHeader(mfxBitstream *in, VP9DecoderFrame & info)
{
    MFX_CHECK_NULL_PTR2(in, in->Data);

    try
    {
        mfxStatus sts = DecodeSuperFrame(in, info);
        MFX_CHECK_STS(sts);
        in = &m_bs;

        VP9Bitstream bsReader(in->Data + in->DataOffset, in->DataOffset + in->DataLength);

        if (VP9_FRAME_MARKER != bsReader.GetBits(2))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        info.profile = bsReader.GetBit();
        info.profile |= bsReader.GetBit() << 1;
        if (info.profile > 2)
            info.profile += bsReader.GetBit();

        MFX_CHECK(info.profile < 4, MFX_ERR_UNDEFINED_BEHAVIOR);

        info.show_existing_frame = bsReader.GetBit();
        if (info.show_existing_frame)
        {
            info.frame_to_show = bsReader.GetBits(3);
            info.width = info.sizesOfRefFrame[info.frame_to_show].width;
            info.height = info.sizesOfRefFrame[info.frame_to_show].height;
            info.showFrame = 1;
            return MFX_ERR_NONE;
        }

        info.frameType = (VP9_FRAME_TYPE) bsReader.GetBit();
        info.showFrame = bsReader.GetBit();
        info.errorResilientMode = bsReader.GetBit();

        if (KEY_FRAME == info.frameType)
        {
            MFX_CHECK(CheckSyncCode(&bsReader), MFX_ERR_UNDEFINED_BEHAVIOR);

            try
            { GetBitDepthAndColorSpace(&bsReader, &info); }
            catch (vp9_exception const& e)
            {
                UMC::Status status = e.GetStatus();
                MFX_CHECK(status == UMC::UMC_OK, MFX_ERR_UNDEFINED_BEHAVIOR);
            }

            info.refreshFrameFlags = (1 << NUM_REF_FRAMES) - 1;

            for (mfxU8 i = 0; i < REFS_PER_FRAME; ++i)
            {
                info.activeRefIdx[i] = 0;
            }

            GetFrameSize(&bsReader, &info);
        }
        else
        {
            info.intraOnly = info.showFrame ? 0 : bsReader.GetBit();
            info.resetFrameContext = info.errorResilientMode ? 0 : bsReader.GetBits(2);

            if (info.intraOnly)
            {
                MFX_CHECK(CheckSyncCode(&bsReader), MFX_ERR_UNDEFINED_BEHAVIOR);

                try
                {
                    GetBitDepthAndColorSpace(&bsReader, &info);

                    info.refreshFrameFlags = (mfxU8)bsReader.GetBits(NUM_REF_FRAMES);
                    GetFrameSize(&bsReader, &info);
                }
                catch (vp9_exception const& e)
                {
                    UMC::Status status = e.GetStatus();
                    MFX_CHECK(status == UMC::UMC_OK, MFX_ERR_UNDEFINED_BEHAVIOR);
                }
            }
            else
            {
                info.refreshFrameFlags = (mfxU8)bsReader.GetBits(NUM_REF_FRAMES);

                for (mfxU8 i = 0; i < REFS_PER_FRAME; ++i)
                {
                    const mfxI32 ref = bsReader.GetBits(REF_FRAMES_LOG2);
                    info.activeRefIdx[i] = ref;
                    info.refFrameSignBias[LAST_FRAME + i] = bsReader.GetBit();
                }

                GetFrameSizeWithRefs(&bsReader, &info);

                info.allowHighPrecisionMv = bsReader.GetBit();

                static const INTERP_FILTER literal2Filter[] =
                    { EIGHTTAP_SMOOTH, EIGHTTAP, EIGHTTAP_SHARP, BILINEAR };
                info.interpFilter = bsReader.GetBit() ? SWITCHABLE : literal2Filter[bsReader.GetBits(2)];
            }
        }

        if (!info.errorResilientMode)
        {
            info.refreshFrameContext = bsReader.GetBit();
            info.frameParallelDecodingMode = bsReader.GetBit();
        }
        else
        {
            info.refreshFrameContext = 0;
            info.frameParallelDecodingMode = 1;
        }

        // This flag will be overridden by the call to vp9_setup_past_independence
        // below, forcing the use of context 0 for those frame types.
        info.frameContextIdx = bsReader.GetBits(FRAME_CONTEXTS_LOG2);

        if (info.frameType == KEY_FRAME || info.intraOnly || info.errorResilientMode)
        {
           SetupPastIndependence(info);
        }

        SetupLoopFilter(&bsReader, &info.lf);

        //setup_quantization()
        {
            info.baseQIndex = bsReader.GetBits(QINDEX_BITS);
            mfxI32 old_y_dc_delta_q = info.y_dc_delta_q;
            mfxI32 old_uv_dc_delta_q = info.uv_dc_delta_q;
            mfxI32 old_uv_ac_delta_q = info.uv_ac_delta_q;

            if (bsReader.GetBit())
            {
                info.y_dc_delta_q = bsReader.GetBits(4);
                info.y_dc_delta_q = bsReader.GetBit() ? -info.y_dc_delta_q : info.y_dc_delta_q;
            }
            else
                info.y_dc_delta_q = 0;

            if (bsReader.GetBit())
            {
                info.uv_dc_delta_q = bsReader.GetBits(4);
                info.uv_dc_delta_q = bsReader.GetBit() ? -info.uv_dc_delta_q : info.uv_dc_delta_q;
            }
            else
                info.uv_dc_delta_q = 0;

            if (bsReader.GetBit())
            {
                info.uv_ac_delta_q = bsReader.GetBits(4);
                info.uv_ac_delta_q = bsReader.GetBit() ? -info.uv_ac_delta_q : info.uv_ac_delta_q;
            }
            else
                info.uv_ac_delta_q = 0;

            if (old_y_dc_delta_q  != info.y_dc_delta_q  ||
                old_uv_dc_delta_q != info.uv_dc_delta_q ||
                old_uv_ac_delta_q != info.uv_ac_delta_q ||
                0 == m_frameOrder)
            {
                InitDequantizer(&info);
            }

            info.lossless = (0 == info.baseQIndex &&
                             0 == info.y_dc_delta_q &&
                             0 == info.uv_dc_delta_q &&
                             0 == info.uv_ac_delta_q);
        }

        // setup_segmentation()
        {
            info.segmentation.updateMap = 0;
            info.segmentation.updateData = 0;

            info.segmentation.enabled = (mfxU8)bsReader.GetBit();
            if (info.segmentation.enabled)
            {
                // Segmentation map update
                info.segmentation.updateMap = (mfxU8)bsReader.GetBit();
                if (info.segmentation.updateMap)
                {
                    for (mfxU8 i = 0; i < VP9_NUM_OF_SEGMENT_TREE_PROBS; ++i)
                        info.segmentation.treeProbs[i] = (mfxU8) (bsReader.GetBit() ? bsReader.GetBits(8) : VP9_MAX_PROB);

                    info.segmentation.temporalUpdate = (mfxU8)bsReader.GetBit();
                    if (info.segmentation.temporalUpdate)
                    {
                        for (mfxU8 i = 0; i < VP9_NUM_OF_PREDICTION_PROBS; ++i)
                            info.segmentation.predProbs[i] = (mfxU8) (bsReader.GetBit() ? bsReader.GetBits(8) : VP9_MAX_PROB);
                    }
                    else
                    {
                        for (mfxU8 i = 0; i < VP9_NUM_OF_PREDICTION_PROBS; ++i)
                            info.segmentation.predProbs[i] = VP9_MAX_PROB;
                    }
                }

                info.segmentation.updateData = (mfxU8)bsReader.GetBit();
                if (info.segmentation.updateData)
                {
                    info.segmentation.absDelta = (mfxU8)bsReader.GetBit();

                    ClearAllSegFeatures(info.segmentation);

                    mfxI32 data = 0;
                    mfxU32 nBits = 0;
                    for (mfxU8 i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; ++i)
                    {
                        for (mfxU8 j = 0; j < SEG_LVL_MAX; ++j)
                        {
                            data = 0;
                            if (bsReader.GetBit()) // feature_enabled
                            {
                                EnableSegFeature(info.segmentation, i, (SEG_LVL_FEATURES) j);

                                nBits = GetUnsignedBits(SEG_FEATURE_DATA_MAX[j]);
                                data = bsReader.GetBits(nBits);
                                data = data > SEG_FEATURE_DATA_MAX[j] ? SEG_FEATURE_DATA_MAX[j] : data;

                                if (IsSegFeatureSigned( (SEG_LVL_FEATURES) j))
                                    data = bsReader.GetBit() ? -data : data;
                            }

                            SetSegData(info.segmentation, i, (SEG_LVL_FEATURES) j, data);
                        }
                    }
                }
            }
        }

        // setup_tile_info()
        {
            const mfxI32 alignedWidth = AlignPowerOfTwo(info.width, MI_SIZE_LOG2);
            int minLog2TileColumns = 0, maxLog2TileColumns = 0, maxOnes = 0;
            mfxU32 miCols = alignedWidth >> MI_SIZE_LOG2;
            GetTileNBits(miCols, minLog2TileColumns, maxLog2TileColumns);

            maxOnes = maxLog2TileColumns - minLog2TileColumns;
            info.log2TileColumns = minLog2TileColumns;
            while (maxOnes-- && bsReader.GetBit())
                info.log2TileColumns++;

            info.log2TileRows = bsReader.GetBit();
            if (info.log2TileRows)
                info.log2TileRows += bsReader.GetBit();
        }

        info.firstPartitionSize = bsReader.GetBits(16);
        MFX_CHECK(0 != info.firstPartitionSize, MFX_ERR_UNSUPPORTED);

        info.frameHeaderLength = uint32_t(bsReader.BitsDecoded() / 8 + (bsReader.BitsDecoded() % 8 > 0));
        info.frameDataSize = in->DataLength;

        // vp9_loop_filter_frame_init()
        if (info.lf.filterLevel)
        {
            const mfxI32 scale = 1 << (info.lf.filterLevel >> 5);

            LoopFilterInfo & lf_info = info.lf_info;

            for (mfxU8 segmentId = 0; segmentId < VP9_MAX_NUM_OF_SEGMENTS; ++segmentId)
            {
                mfxI32 segmentFilterLevel = info.lf.filterLevel;
                if (IsSegFeatureActive(info.segmentation, segmentId, SEG_LVL_ALT_LF))
                {
                    const mfxI32 data = GetSegData(info.segmentation, segmentId, SEG_LVL_ALT_LF);
                    segmentFilterLevel = clamp(info.segmentation.absDelta == SEGMENT_ABSDATA ? data : info.lf.filterLevel + data,
                                               0,
                                               MAX_LOOP_FILTER);
                }

                if (!info.lf.modeRefDeltaEnabled)
                {
                    memset(lf_info.level[segmentId], segmentFilterLevel, sizeof(lf_info.level[segmentId]) );
                }
                else
                {
                    const mfxI32 intra_lvl = segmentFilterLevel + info.lf.refDeltas[INTRA_FRAME] * scale;
                    lf_info.level[segmentId][INTRA_FRAME][0] = (mfxU8) clamp(intra_lvl, 0, MAX_LOOP_FILTER);

                    for (mfxU8 ref = LAST_FRAME; ref < MAX_REF_FRAMES; ++ref)
                    {
                        for (mfxU8 mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode)
                        {
                            const mfxI32 inter_lvl = segmentFilterLevel + info.lf.refDeltas[ref] * scale
                                                            + info.lf.modeDeltas[mode] * scale;
                            lf_info.level[segmentId][ref][mode] = (mfxU8) clamp(inter_lvl, 0, MAX_LOOP_FILTER);
                        }
                    }
                }
            }
        }
    }
    catch(const vp9_exception & ex)
    {
        mfxStatus sts = ConvertStatusUmc2Mfx(ex.GetStatus());
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;
}

#ifdef UMC_VA_DXVA

VP9DecoderFrame VideoDECODEVP9_HW::MemIdToDXVAIndices(VP9DecoderFrame const & info)
{
    VP9DecoderFrame dxvaInfo = info;
    m_dxvaRemapper->UpdateDXVAIndices(dxvaInfo.currFrame, dxvaInfo.ref_frame_map, NUM_REF_FRAMES);

    if (dxvaInfo.frameType != KEY_FRAME)
    {
        for (mfxU8 ref = 0; ref < NUM_REF_FRAMES; ++ref)
        {
            dxvaInfo.ref_frame_map[ref] = m_dxvaRemapper->GetDXVAIndex(dxvaInfo.ref_frame_map[ref]);
        }
    }
    dxvaInfo.currFrame = m_dxvaRemapper->GetDXVAIndex(dxvaInfo.currFrame);
    return dxvaInfo;
}
#endif

mfxStatus VideoDECODEVP9_HW::PackHeaders(mfxBitstream *bs, VP9DecoderFrame const & info)
{
    MFX_CHECK_NULL_PTR2(bs, bs->Data);

    if (!m_Packer.get())
    {
        m_Packer.reset(Packer::CreatePacker(m_va));
        MFX_CHECK(m_Packer, MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    VP9Bitstream vp9bs(bs->Data + bs->DataOffset, bs->DataLength);

    try
    {
        m_Packer->BeginFrame();
        VP9DecoderFrame packerInfo = info;
#ifdef UMC_VA_DXVA
        MFX_CHECK(m_dxvaRemapper, MFX_ERR_UNDEFINED_BEHAVIOR);
        packerInfo = MemIdToDXVAIndices(info);
#endif
        m_Packer->PackAU(&vp9bs, &packerInfo);
        m_Packer->EndFrame();
    }
    catch (vp9_exception const&)
    {
        MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    return MFX_ERR_NONE;
}

mfxFrameSurface1 * VideoDECODEVP9_HW::GetOriginalSurface(mfxFrameSurface1 *p_surface)
{
    if (m_is_opaque_memory)
    {
        return m_core->GetNativeSurface(p_surface);
    }

    return p_surface;
}

mfxFrameSurface1* VideoDECODEVP9_HW::GetSurface()
{
    if (!m_surface_source)
    {
        std::ignore = MFX_STS_TRACE(MFX_ERR_NOT_INITIALIZED);
        return nullptr;
    }

    return m_surface_source->GetSurface();
}

#endif //MFX_ENABLE_VP9_VIDEO_DECODE
