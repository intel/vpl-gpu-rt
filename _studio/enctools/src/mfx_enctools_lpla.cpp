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

#include "mfx_enctools.h"
#include "mfx_enctools_lpla.h"

#include <algorithm>
#include <math.h>
#include <climits>

mfxStatus LPLA_EncTool::Init(mfxEncToolsCtrl const & ctrl, mfxExtEncToolsConfig const & config)
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxEncToolsCtrlExtDevice *extDevice = (mfxEncToolsCtrlExtDevice *)Et_GetExtBuffer(ctrl.ExtParam, ctrl.NumExtParam, MFX_EXTBUFF_ENCTOOLS_DEVICE);
    if (extDevice)
    {
        m_device = extDevice->DeviceHdl;
        m_deviceType = extDevice->HdlType;
    }
    if (!m_pAllocator)
    {
        mfxEncToolsCtrlExtAllocator *extAlloc = (mfxEncToolsCtrlExtAllocator *)Et_GetExtBuffer(ctrl.ExtParam, ctrl.NumExtParam, MFX_EXTBUFF_ENCTOOLS_ALLOCATOR);
        if (extAlloc)
            m_pAllocator = extAlloc->pAllocator;
    }
    MFX_CHECK_NULL_PTR2(m_device, m_pAllocator);

    sts = InitSession();
    MFX_CHECK_STS(sts);

    m_pmfxENC = new MFXDLVideoENCODE(m_mfxSession, m_hRTModule);
    MFX_CHECK_NULL_PTR1(m_pmfxENC);

    m_GopPicSize = ctrl.MaxGopSize;
    if (m_GopPicSize)
        m_IdrInterval = (mfxU16)std::min<mfxU32>(ctrl.MaxIDRDist / m_GopPicSize, USHRT_MAX);

    m_GopRefDist = ctrl.MaxGopRefDist;
    m_codecId = ctrl.CodecId; // main codec's ID

    sts = InitEncParams(ctrl, config);
    MFX_CHECK_STS(sts);

    memset(&m_bitstream, 0, sizeof(mfxBitstream));
    mfxU32 bufferSize = std::max((mfxU32)m_encParams.mfx.FrameInfo.Width * m_encParams.mfx.FrameInfo.Height * 3 / 2, ctrl.BufferSizeInKB * 1000);
    m_bitstream.Data = new mfxU8[bufferSize];
    m_bitstream.MaxLength = bufferSize;

    sts = ConfigureExtBuffs(ctrl, config);
    MFX_CHECK_STS(sts);

    sts = m_pmfxENC->Init(&m_encParams);
    if (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts)
    {
        sts = MFX_ERR_NONE;
    }
    MFX_CHECK_STS(sts);

    m_curDispOrder = -1;
    m_config = config;
    m_bInit = true;
    return sts;
}


mfxStatus LPLA_EncTool::Reset(mfxEncToolsCtrl const& ctrl, mfxExtEncToolsConfig const& config)
{
    mfxStatus sts = MFX_ERR_NONE;

    m_curDispOrder = -1;

    m_GopPicSize = ctrl.MaxGopSize;
    if (m_GopPicSize)
        m_IdrInterval = (mfxU16)std::min<mfxU32>(ctrl.MaxIDRDist / m_GopPicSize, USHRT_MAX);

    m_GopRefDist = ctrl.MaxGopRefDist;
    m_codecId = ctrl.CodecId; // main codec's ID

    sts = InitEncParams(ctrl, config);
    MFX_CHECK_STS(sts);

    mfxU32 bufferSize = std::max((mfxU32)m_encParams.mfx.FrameInfo.Width * m_encParams.mfx.FrameInfo.Height * 3 / 2, ctrl.BufferSizeInKB * 1000);
    if (!m_bitstream.Data || bufferSize > m_bitstream.MaxLength)
    {
        if (m_bitstream.Data)
            delete[] m_bitstream.Data;
        memset(&m_bitstream, 0, sizeof(mfxBitstream));
        m_bitstream.Data = new mfxU8[bufferSize];
        m_bitstream.MaxLength = bufferSize;
    }

    sts = ConfigureExtBuffs(ctrl, config);
    MFX_CHECK_STS(sts);

    m_pmfxENC->Close();
    sts = m_pmfxENC->Init(&m_encParams);

    if (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts)
    {
        sts = MFX_ERR_NONE;
    }
    MFX_CHECK_STS(sts);
    m_config = config;

    return sts;

}

mfxStatus LPLA_EncTool::InitSession()
{
    MFX_CHECK_NULL_PTR2(m_device, m_pAllocator);
    mfxStatus sts = MFX_ERR_NONE;

    mfxInitParam initPar = {};
    mfxVersion version;     // real API version with which library is initialized

    initPar.Version.Major = 1;
    initPar.Version.Minor = 0;
    initPar.Implementation = MFX_IMPL_HARDWARE;
    initPar.Implementation |= (m_deviceType == MFX_HANDLE_D3D11_DEVICE ? MFX_IMPL_VIA_D3D11 :
        (m_deviceType == MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9 ? MFX_IMPL_VIA_D3D9 : MFX_IMPL_VIA_VAAPI));
    initPar.GPUCopy = MFX_GPUCOPY_DEFAULT;

    sts = m_mfxSession.InitEx(initPar);
    MFX_CHECK_STS(sts);

    sts = m_mfxSession.QueryVersion(&version); // get real API version of the loaded library
    MFX_CHECK_STS(sts);

    sts = m_mfxSession.SetHandle((mfxHandleType)m_deviceType, m_device);
    MFX_CHECK_STS(sts);

    return sts;
}

mfxStatus LPLA_EncTool::InitEncParams(mfxEncToolsCtrl const & ctrl, mfxExtEncToolsConfig const & pConfig)
{
    mfxStatus sts = MFX_ERR_NONE;

    // following configuration comes from HW recommendation
    m_encParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    m_encParams.AsyncDepth = 1;
    m_encParams.mfx.CodecId = MFX_CODEC_HEVC;
    m_encParams.mfx.LowPower = MFX_CODINGOPTION_ON;
    m_encParams.mfx.NumRefFrame = 1;
    m_encParams.mfx.TargetUsage = 7;
    m_encParams.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    m_encParams.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
    m_encParams.mfx.CodecLevel = MFX_LEVEL_HEVC_52;
    m_encParams.mfx.QPI = ctrl.LaQp;
    m_encParams.mfx.QPP = ctrl.LaQp+2;
    m_encParams.mfx.QPB = ctrl.LaQp+2;
    m_encParams.mfx.NumSlice = 1;
    m_encParams.mfx.GopOptFlag = ctrl.GopOptFlag;

    if (ctrl.CodecId != MFX_CODEC_AVC)
    {
        m_encParams.mfx.IdrInterval = m_IdrInterval;
    }

    if (IsOn(pConfig.AdaptiveI))
        m_encParams.mfx.GopPicSize = 0xffff; // infinite GOP
    else
        m_encParams.mfx.GopPicSize = ctrl.MaxGopSize;

    m_encParams.mfx.GopRefDist = 1; //ctrl.MaxGopRefDist;

    m_encParams.mfx.FrameInfo = ctrl.FrameInfo;
    m_lookAheadScale = ctrl.LaScale;
    m_lookAheadDepth = ctrl.MaxDelayInFrames;

    mfxU16 crW = m_encParams.mfx.FrameInfo.CropW ? m_encParams.mfx.FrameInfo.CropW : m_encParams.mfx.FrameInfo.Width;
    mfxU16 crH = m_encParams.mfx.FrameInfo.CropH ? m_encParams.mfx.FrameInfo.CropH : m_encParams.mfx.FrameInfo.Height;

    if (m_lookAheadScale)
    {
        m_encParams.mfx.FrameInfo.CropW = (crW >> m_lookAheadScale);
        m_encParams.mfx.FrameInfo.CropH = (crH >> m_lookAheadScale);
        m_encParams.mfx.FrameInfo.Width = (m_encParams.mfx.FrameInfo.CropW + 15) & ~0xF;
        m_encParams.mfx.FrameInfo.Height = (m_encParams.mfx.FrameInfo.CropH + 15) & ~0xF;
    }

    if(ctrl.ScenarioInfo != MFX_SCENARIO_GAME_STREAMING && (m_encParams.mfx.FrameInfo.BitDepthLuma == 10 || m_encParams.mfx.FrameInfo.FourCC == MFX_FOURCC_YUY2))
    {
        m_encParams.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        m_encParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_encParams.mfx.FrameInfo.BitDepthLuma = 8;
        m_encParams.mfx.FrameInfo.BitDepthChroma = 8;
        m_encParams.mfx.FrameInfo.Shift = 0;
    }

    return sts;
}

mfxStatus LPLA_EncTool::ConfigureExtBuffs(mfxEncToolsCtrl const & 
#if defined (MFX_ENABLE_ENCTOOLS_LPLA)
    ctrl
#endif
    , mfxExtEncToolsConfig const & pConfig)
{
    mfxStatus sts = MFX_ERR_NONE;
    // create ext buffer for lpla
    mfxExtBuffer** extBuf;
    m_encParams.NumExtParam = 0;
#if defined (MFX_ENABLE_ENCTOOLS_LPLA)
    if (ctrl.ScenarioInfo == MFX_SCENARIO_GAME_STREAMING) // regular LPLA - not LPLA-assisted ET BRC; to be removed if BRC starts using the hints 
    {
        // Attach buffer to bitstream for get la hints from enoder
        mfxExtBuffer** extParams = new mfxExtBuffer *[1];
        extBuf = new mfxExtBuffer *[4];

        m_lplaHints = {};
        m_lplaHints.Header.BufferId = MFX_EXTBUFF_LPLA_STATUS;
        m_lplaHints.Header.BufferSz = sizeof(m_lplaHints);
        extParams[0] = &m_lplaHints.Header;
        m_bitstream.ExtParam = extParams;
        m_bitstream.NumExtParam = 1;

        m_extBufLPLA = {};
        m_extBufLPLA.Header.BufferId = MFX_EXTBUFF_LP_LOOKAHEAD;
        m_extBufLPLA.Header.BufferSz = sizeof(m_extBufLPLA);
        m_extBufLPLA.LookAheadDepth = ctrl.MaxDelayInFrames;
        m_extBufLPLA.InitialDelayInKB = (mfxU16)ctrl.InitialDelayInKB;
        m_extBufLPLA.BufferSizeInKB = (mfxU16)ctrl.BufferSizeInKB;
        m_extBufLPLA.TargetKbps = (mfxU16)ctrl.TargetKbps;
        m_extBufLPLA.LookAheadScaleX = m_extBufLPLA.LookAheadScaleY = (mfxU8)m_lookAheadScale;

        m_extBufLPLA.GopRefDist = ctrl.MaxGopRefDist;
        m_extBufLPLA.codecTypeInEncodePass = ctrl.CodecId;

        if (IsOn(pConfig.AdaptiveI))
        {
            m_extBufLPLA.MaxAdaptiveGopSize = ctrl.MaxGopSize;
            m_extBufLPLA.MinAdaptiveGopSize = (mfxU16)std::max(16, m_extBufLPLA.MaxAdaptiveGopSize / 4);
        }

        extBuf[m_encParams.NumExtParam] = &m_extBufLPLA.Header;
        m_encParams.NumExtParam++;

        m_extBufCO2 = {};
        m_extBufCO2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
        m_extBufCO2.Header.BufferSz = sizeof(m_extBufCO2);
        m_extBufCO2.AdaptiveB = (mfxU16)(IsOn(pConfig.AdaptiveB) ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);

        extBuf[m_encParams.NumExtParam] = &m_extBufCO2.Header;
        m_encParams.NumExtParam++;
    } 
    else
#endif
        extBuf = new mfxExtBuffer *[2];

    m_extBufHevcParam = {};
    m_extBufHevcParam.Header.BufferId = MFX_EXTBUFF_HEVC_PARAM;
    m_extBufHevcParam.Header.BufferSz = sizeof(m_extBufHevcParam);

    m_extBufHevcParam.SampleAdaptiveOffset = MFX_SAO_DISABLE;

    extBuf[m_encParams.NumExtParam] = &m_extBufHevcParam.Header;
    m_encParams.NumExtParam++;

    m_extBufCO3 = {};
    m_extBufCO3.Header.BufferId = MFX_EXTBUFF_CODING_OPTION3;
    m_extBufCO3.Header.BufferSz = sizeof(m_extBufCO3);
    m_extBufCO3.PRefType = (mfxU16)(IsOn(pConfig.AdaptivePyramidQuantP) ? MFX_P_REF_PYRAMID : MFX_P_REF_SIMPLE);
    extBuf[m_encParams.NumExtParam] = &m_extBufCO3.Header;
    m_encParams.NumExtParam++;

    m_encParams.ExtParam = extBuf;

    return sts;
}

MFXDLVideoSession* LPLA_EncTool::GetEncSession()
{
    return &m_mfxSession;
}

mfxStatus LPLA_EncTool::Submit(mfxFrameSurface1* surface, mfxU16 FrameType, mfxSyncPoint* pEncSyncp)
{
    mfxStatus sts = MFX_ERR_NONE;

    m_bitstream.DataLength = 0;
    m_bitstream.DataOffset = 0;
    mfxEncodeCtrl ctrl = {};
    ctrl.FrameType = FrameType;
    sts = m_pmfxENC->EncodeFrameAsync(&ctrl, surface, &m_bitstream, pEncSyncp);
    MFX_CHECK_STS(sts);

    return sts;
}

mfxStatus LPLA_EncTool::SaveEncodedFrameSize(mfxFrameSurface1* surface, mfxU16 FrameType)
{
    mfxStatus sts = MFX_ERR_NONE;
    
    if (m_bitstream.DataLength == 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    //printf("LPLA_EncTool::Submit encoded frame size %7d\n", m_bitstream.DataLength);

    m_frameSizes.push_back({ surface->Data.FrameOrder, m_bitstream.DataLength, FrameType });

#if defined (MFX_ENABLE_ENCTOOLS_LPLA)
    mfxExtLpLaStatus* lplaHints = (mfxExtLpLaStatus*)Et_GetExtBuffer(m_bitstream.ExtParam, m_bitstream.NumExtParam, MFX_EXTBUFF_LPLA_STATUS);
    if (lplaHints)
    {
        if (lplaHints->CqmHint != CQM_HINT_INVALID)
        {
            //printf("Submit %d: CQM %d Intra %d FrmSize %d MiniGop %d QpModStrength %d \n", surface->Data.FrameOrder, lplaHints->CqmHint, lplaHints->IntraHint, lplaHints->TargetFrameSize, lplaHints->MiniGopSize, lplaHints->QpModulationStrength);
            m_encodeHints.push_back({
                lplaHints->StatusReportFeedbackNumber,
                lplaHints->CqmHint,
                lplaHints->IntraHint,
                lplaHints->MiniGopSize,
                lplaHints->QpModulationStrength,
                lplaHints->TargetFrameSize
            });
        }
    }
#endif
    return sts;
}

#if defined (MFX_ENABLE_ENCTOOLS_LPLA)
mfxStatus LPLA_EncTool::Query(mfxU32 dispOrder, mfxEncToolsHintPreEncodeGOP *pPreEncGOP)
{
    MFX_CHECK_NULL_PTR1(pPreEncGOP);
    mfxStatus sts = MFX_ERR_NONE;

    if (!m_bInit)
        return MFX_ERR_NOT_INITIALIZED;

    if ((mfxI32)dispOrder < m_curDispOrder)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    else if ((mfxI32)dispOrder > m_curDispOrder)
    {
        if (m_encodeHints.empty())
        {
            sts = MFX_ERR_NOT_FOUND;
            pPreEncGOP->FrameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            if (m_encParams.mfx.GopPicSize && (dispOrder - m_lastIFrameNumber >= (mfxU32)m_encParams.mfx.GopPicSize))
                pPreEncGOP->FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
        }
        else
        {
            m_curEncodeHints = m_encodeHints.front();
            m_curDispOrder = (mfxI32)dispOrder;
            m_encodeHints.pop_front();
        }
    }

    if (sts == MFX_ERR_NONE)
    {
        switch (m_curEncodeHints.MiniGopSize)
        {
        case 1:
        case 2:
        case 4:
        case 8:
            pPreEncGOP->MiniGopSize = m_curEncodeHints.MiniGopSize;
            break;
        default:
            pPreEncGOP->MiniGopSize = 0;
        }
        mfxU8 qpModulationHigh = MAX_QP_MODULATION;

        if (m_curEncodeHints.PyramidQpHint <= qpModulationHigh &&
            (IsOn(m_config.AdaptivePyramidQuantP) || IsOn(m_config.AdaptivePyramidQuantB)))
            pPreEncGOP->QPModulation = m_curEncodeHints.PyramidQpHint;
        else
            pPreEncGOP->QPModulation = MFX_QP_MODULATION_NOT_DEFINED;

        if (m_curEncodeHints.IntraHint && IsOn(m_config.AdaptiveI))
            m_nextPisIntra = true;

        pPreEncGOP->FrameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
    }

    if (dispOrder == 0)
    {
        pPreEncGOP->FrameType = MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
        m_lastIDRFrameNumber = dispOrder;
        m_nextPisIntra = false;
    }

    // for adaptiveI==off only: m_encParams.mfx.GopPicSize is "infinite" (0xffff) when adaptiveI==on
    if (m_encParams.mfx.GopPicSize && (dispOrder - m_lastIFrameNumber >= (mfxU32)m_encParams.mfx.GopPicSize))
        pPreEncGOP->FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;

    if (pPreEncGOP->FrameType & MFX_FRAMETYPE_I)
    {
        m_lastIFrameNumber = m_lastIPFrameNumber = dispOrder;
        if (m_GopPicSize && (dispOrder - m_lastIDRFrameNumber >= (mfxU32)m_GopPicSize * m_IdrInterval))
        {
            pPreEncGOP->FrameType |= MFX_FRAMETYPE_IDR;
            m_lastIDRFrameNumber = dispOrder;
        }
    }
    else if (m_GopRefDist > 1)
    {
        // closed GOP (GopOptFlag or IDR)
        if (((dispOrder - m_lastIFrameNumber + 1 == (mfxU32)m_encParams.mfx.GopPicSize) && (m_encParams.mfx.GopOptFlag & MFX_GOP_CLOSED))
            || (m_encParams.mfx.GopPicSize && (dispOrder - m_lastIDRFrameNumber + 1 == (mfxU32)m_encParams.mfx.GopPicSize * m_IdrInterval)))
        {
            pPreEncGOP->FrameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        }
        else
        {
            mfxU32 miniGopSize = pPreEncGOP->MiniGopSize ? pPreEncGOP->MiniGopSize : m_GopRefDist;
            if ((dispOrder - m_lastIPFrameNumber) % miniGopSize)
            {
                pPreEncGOP->FrameType = MFX_FRAMETYPE_B;
            }
            else
            {
                pPreEncGOP->FrameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                if (m_nextPisIntra)
                {
                    pPreEncGOP->FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
                    if (m_codecId == MFX_CODEC_AV1)
                        pPreEncGOP->FrameType |= MFX_FRAMETYPE_IDR;
                    m_nextPisIntra = false;
                    m_lastIFrameNumber = dispOrder;
                }
                m_lastIPFrameNumber = dispOrder;
            }
        }
    }
    else
    {
        pPreEncGOP->FrameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        if (m_nextPisIntra)
        {
            pPreEncGOP->FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
            if (m_codecId == MFX_CODEC_AV1)
                pPreEncGOP->FrameType |= MFX_FRAMETYPE_IDR;
            m_nextPisIntra = false;
            m_lastIFrameNumber = dispOrder;
        }
        m_lastIPFrameNumber = dispOrder;
    }

    return sts;
}
#endif

#if defined (MFX_ENABLE_ENCTOOLS_LPLA)
mfxStatus LPLA_EncTool::Query(mfxU32 dispOrder, mfxEncToolsHintQuantMatrix *pCqmHint)
{
    MFX_CHECK_NULL_PTR1(pCqmHint);

    if (!m_bInit)
        return MFX_ERR_NOT_INITIALIZED;

    if ((mfxI32)dispOrder < m_curDispOrder)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    else if ((mfxI32)dispOrder > m_curDispOrder)
    {
        if (m_encodeHints.empty())
        {
            pCqmHint->MatrixType = CQM_HINT_INVALID;
            return MFX_ERR_NOT_FOUND;
        }

        m_curEncodeHints = m_encodeHints.front();
        m_curDispOrder = (mfxI32)dispOrder;
        m_encodeHints.pop_front();
    }

    switch (m_curEncodeHints.CqmHint)
    {
    case CQM_HINT_USE_CUST_MATRIX1:
        pCqmHint->MatrixType = MFX_QUANT_MATRIX_WEAK;
        break;
    case CQM_HINT_USE_CUST_MATRIX2:
        pCqmHint->MatrixType = MFX_QUANT_MATRIX_MEDIUM;
        break;
    case CQM_HINT_USE_CUST_MATRIX3:
        pCqmHint->MatrixType = MFX_QUANT_MATRIX_STRONG;
        break;
    case CQM_HINT_USE_CUST_MATRIX4:
        pCqmHint->MatrixType = MFX_QUANT_MATRIX_EXTREME;
        break;
    case CQM_HINT_USE_FLAT_MATRIX:
    default:
        pCqmHint->MatrixType = MFX_QUANT_MATRIX_FLAT;
    }
    return MFX_ERR_NONE;
}
#endif
mfxStatus LPLA_EncTool::Query(mfxU32 dispOrder, mfxEncToolsBRCBufferHint *pBufHint)
{
    MFX_CHECK_NULL_PTR1(pBufHint)

    if (!m_bInit)
        return MFX_ERR_NOT_INITIALIZED;

    if (pBufHint->OutputMode == MFX_BUFFERHINT_OUTPUT_DISPORDER)
    {
        if ((mfxI32)dispOrder < m_curDispOrder)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        else if ((mfxI32)dispOrder > m_curDispOrder)
        {
            if (!m_frameSizes.empty()) {
                m_curDispOrder = (mfxU32)dispOrder;
                mfxU32 laAvgBits = 0;
                mfxU16 distToNextI = 0;
                for (std::list<MfxFrameSize>::iterator it = m_frameSizes.begin(); it != m_frameSizes.end(); it++) {
                    laAvgBits += it->encodedFrameSize;
                    if (!distToNextI && (it->frameType & MFX_FRAMETYPE_I)) 
                        distToNextI = mfxU16(it->dispOrder - dispOrder);
                }
                laAvgBits *= 8;
                laAvgBits /= (mfxU32)m_frameSizes.size();
                pBufHint->AvgEncodedSizeInBits = laAvgBits;
                pBufHint->CurEncodedSizeInBits = m_frameSizes.front().encodedFrameSize * 8;
                pBufHint->DistToNextI = distToNextI;
                m_frameSizes.pop_front();
            }
        }
        return MFX_ERR_NONE;
    }

#if defined (MFX_ENABLE_ENCTOOLS_LPLA)

    if ((mfxI32)dispOrder < m_curDispOrder)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    else if ((mfxI32)dispOrder > m_curDispOrder)
    {
        if (m_encodeHints.empty())
            return MFX_ERR_NOT_FOUND;
        m_curEncodeHints = m_encodeHints.front();
        m_curDispOrder = (mfxU32)dispOrder;
        m_encodeHints.pop_front();
    }

    pBufHint->OptimalFrameSizeInBytes = m_curEncodeHints.TargetFrameSize;
#endif

    return MFX_ERR_NONE;
}

mfxStatus LPLA_EncTool::Close()
{
    mfxStatus sts = MFX_ERR_NONE;
    if (m_bInit)
    {
        if(m_bitstream.Data)
        {
            delete[] m_bitstream.Data;
            m_bitstream.Data = nullptr;
        }

        if (m_bitstream.ExtParam)
        {
            delete[] m_bitstream.ExtParam;
            m_bitstream.ExtParam = nullptr;
        }

        if(m_encParams.ExtParam)
        {
            delete[] m_encParams.ExtParam;
            m_encParams.ExtParam = nullptr;
        }
        if (m_pmfxENC)
        {
            delete m_pmfxENC;
            m_pmfxENC = nullptr;
        }

        sts = m_mfxSession.Close();
        MFX_CHECK_STS(sts);
        m_bInit = false;
    }

    return sts;
}
