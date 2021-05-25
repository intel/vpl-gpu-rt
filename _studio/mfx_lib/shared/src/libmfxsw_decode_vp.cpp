// Copyright (c) 2020 Intel Corporation
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

#include <mfxvideo.h>
#include <mfx_session.h>
#include <mfx_tools.h>
#include <mfx_task.h>
#include <libmfx_core.h>

#include<set>

constexpr mfxU16 decoderChannelID = 0;

mfxStatus MFXVideoDECODE_VPP_Init(mfxSession session, mfxVideoParam* decode_par, mfxVideoChannelParam** vpp_par_array, mfxU32 num_channel_par)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK_NULL_PTR2(decode_par, vpp_par_array);

    mfxStatus mfxRes = MFX_ERR_NONE;

    try
    {
        //check VPP config
        std::set<mfxU16> ids;
        for (mfxU32 channelIdx = 0; channelIdx < num_channel_par; channelIdx++)
        {
            mfxVideoChannelParam* channelPar = vpp_par_array[channelIdx];
            MFX_CHECK(channelPar, MFX_ERR_NULL_PTR);

            if (channelPar->Protected != 0)
            {
                MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
            }

            mfxU16 id = channelPar->VPP.ChannelId;
            if (id == decoderChannelID || ids.find(id) != ids.end())
            {
                MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
            }
            ids.insert(id);

            if (channelPar->NumExtParam > 1)
            {
                MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
            }

            if (channelPar->NumExtParam == 1)
            {
                MFX_CHECK_NULL_PTR2(channelPar->ExtParam, channelPar->ExtParam[0]);
                if (channelPar->ExtParam[0]->BufferId != MFX_EXTBUFF_VPP_SCALING)
                {
                    MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
                }

                switch (reinterpret_cast<mfxExtVPPScaling*>(channelPar->ExtParam[0])->ScalingMode) {
                case MFX_SCALING_MODE_INTEL_GEN_VDBOX:
                case MFX_SCALING_MODE_INTEL_GEN_VEBOX:
                case MFX_SCALING_MODE_INTEL_GEN_COMPUTE:
                    break;
                default:
                    MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
                }
            }

            if (decode_par->mfx.FrameInfo.FrameRateExtN != channelPar->VPP.FrameRateExtN ||
                decode_par->mfx.FrameInfo.FrameRateExtD != channelPar->VPP.FrameRateExtD ||
                decode_par->mfx.FrameInfo.PicStruct != channelPar->VPP.PicStruct)
            {
                MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
            }
        }


        //create  DVP
        if (!session->m_pDVP)
        {
            session->m_pDVP.reset(new _mfxSession::DVP());
            session->m_pDVP->sfcChannelID = 0;
        }


        //parse VPP params
        mfxU16 sfcChannelID = 0; //zero ID is reserved for DEC channel, it is invalid value for SFC
        std::map<mfxU16, mfxU16> vppScalingModeMap;
        for (mfxU32 channelIdx = 0; channelIdx < num_channel_par; channelIdx++)
        {
            mfxVideoChannelParam* channelPar = vpp_par_array[channelIdx];
            MFX_CHECK(channelPar, MFX_ERR_NULL_PTR);
            mfxU16 id = channelPar->VPP.ChannelId;
            mfxVideoParam& VppParams = session->m_pDVP->VppParams[id];

            //extract and convert scaling modes
            vppScalingModeMap[id] = MFX_SCALING_MODE_DEFAULT;
            for (mfxU32 i = 0; i < channelPar->NumExtParam; i++) {
                MFX_CHECK_NULL_PTR2(channelPar->ExtParam, channelPar->ExtParam[i]);
                if (channelPar->ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_SCALING) {
                    switch (reinterpret_cast<mfxExtVPPScaling*>(channelPar->ExtParam[i])->ScalingMode) {
                    case MFX_SCALING_MODE_INTEL_GEN_VDBOX:
                        session->m_pDVP->sfcChannelID = sfcChannelID = id;
                        break;
                    case MFX_SCALING_MODE_INTEL_GEN_VEBOX:
                        vppScalingModeMap[id] = MFX_SCALING_MODE_LOWPOWER;
                        break;
                    case MFX_SCALING_MODE_INTEL_GEN_COMPUTE:
                        vppScalingModeMap[id] = MFX_SCALING_MODE_QUALITY;
                        break;
                    default:
                        assert(0); //MFX_SCALING_MODE_DEFAULT in release
                    }
                }
            }

            //set i/o pattern, limit to homogeneous cases, we can relax it later
            VppParams.IOPattern = static_cast<mfxU16>(
                channelPar->IOPattern |
                (decode_par->IOPattern == MFX_IOPATTERN_OUT_SYSTEM_MEMORY ? MFX_IOPATTERN_IN_SYSTEM_MEMORY : MFX_IOPATTERN_IN_VIDEO_MEMORY));
            MFX_CHECK(VppParams.IOPattern == (MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY) ||
                VppParams.IOPattern == (MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY), MFX_ERR_UNSUPPORTED);

            //copy the rest of parameters, we don't need ext buffers,
            //DVP does not support features that require ExtBuffers 
            VppParams.AsyncDepth = decode_par->AsyncDepth;
            VppParams.vpp.In = decode_par->mfx.FrameInfo;
            VppParams.vpp.Out = channelPar->VPP;

            //add scaling mode buffer
            if (vppScalingModeMap[id] != MFX_SCALING_MODE_DEFAULT) {
                mfxExtVPPScaling& vppScalingExtBuf = session->m_pDVP->ScalingModeBuffers[id];
                vppScalingExtBuf.Header.BufferId = MFX_EXTBUFF_VPP_SCALING;
                vppScalingExtBuf.Header.BufferSz = sizeof(mfxExtVPPScaling);
                vppScalingExtBuf.ScalingMode = vppScalingModeMap[id];

                VppParams.ExtParam = &session->m_pDVP->ExtBuffers[id];
                VppParams.ExtParam[0] = &vppScalingExtBuf.Header;
                VppParams.NumExtParam = 1;
            }
        }


        //create and init decoder
        MFX_CHECK(decode_par->IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY ||
            decode_par->IOPattern == MFX_IOPATTERN_OUT_SYSTEM_MEMORY, MFX_ERR_UNSUPPORTED);

        if (!session->m_pDECODE)
        {
            session->m_pDECODE.reset(session->Create<VideoDECODE>(*decode_par));
        }

        if (sfcChannelID != 0) {
            //create "SFC" ext buffer
            mfxVideoParam& VppParams = session->m_pDVP->VppParams[sfcChannelID];

            mfxExtDecVideoProcessing decVppParams{};
            decVppParams.Header.BufferId = MFX_EXTBUFF_DEC_VIDEO_PROCESSING;
            decVppParams.Header.BufferSz = sizeof(mfxExtDecVideoProcessing);

            decVppParams.In.CropX = decode_par->mfx.FrameInfo.CropX;
            decVppParams.In.CropY = decode_par->mfx.FrameInfo.CropY;
            decVppParams.In.CropW = decode_par->mfx.FrameInfo.CropW;
            decVppParams.In.CropH = decode_par->mfx.FrameInfo.CropH;

            decVppParams.Out.FourCC = VppParams.vpp.Out.FourCC;
            decVppParams.Out.ChromaFormat = VppParams.vpp.Out.ChromaFormat;

            decVppParams.Out.Width = VppParams.vpp.Out.Width;
            decVppParams.Out.Height = VppParams.vpp.Out.Height;
            decVppParams.Out.CropX = VppParams.vpp.Out.CropX;
            decVppParams.Out.CropY = VppParams.vpp.Out.CropY;
            decVppParams.Out.CropW = VppParams.vpp.Out.CropW;
            decVppParams.Out.CropH = VppParams.vpp.Out.CropH;

            //create local copy of decoder params, to preserve input params in case of exception or early exit
            mfxVideoParam decParams = *decode_par;
            std::vector<mfxExtBuffer*> decExtBuffers(decParams.ExtParam, decParams.ExtParam + decParams.NumExtParam);
            decExtBuffers.push_back(&decVppParams.Header);
            decParams.ExtParam = decExtBuffers.data();
            decParams.NumExtParam = static_cast<mfxU16>(decExtBuffers.size());

            //init
            mfxRes = session->m_pDECODE->Init(&decParams);
            MFX_CHECK_STS(mfxRes);
        }
        else
        {
            mfxRes = session->m_pDECODE->Init(decode_par);
            MFX_CHECK_STS(mfxRes);
        }


        //create and init VPPs
        for (auto& p : session->m_pDVP->VppParams)
        {
            mfxU16 id = p.first;
            mfxVideoParam& VppParams = p.second;

            if (id == sfcChannelID) {
                continue;
            }

            if (!session->m_pDVP->VPPs[id])
            {
                session->m_pDVP->VPPs[id].reset(session->Create<VideoVPP>(VppParams));
            }

            mfxRes = session->m_pDVP->VPPs[id]->Init(&VppParams);
            MFX_CHECK_STS(mfxRes);

            CommonCORE20* base_core20 = dynamic_cast<CommonCORE20*>(session->m_pCORE.get());
            MFX_CHECK_HDL(base_core20);
            session->m_pDVP->VppPools[id].reset(new SurfaceCache(*base_core20, mfxU16(MFX_MEMTYPE_FROM_VPPOUT), session->m_pDVP->VppParams[id].vpp.Out));
        }
    }
    catch (...)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }

    return mfxRes;
}

class RAIISurfaceArray
{
public:
    RAIISurfaceArray()
    {
        SurfArray = mfxSurfaceArrayImpl::CreateSurfaceArray();
    };

    RAIISurfaceArray(const RAIISurfaceArray&) = delete;
    RAIISurfaceArray& operator =(const RAIISurfaceArray&) = delete;
    RAIISurfaceArray(RAIISurfaceArray&&) = default;
    RAIISurfaceArray& operator=(RAIISurfaceArray&&) = default;

    ~RAIISurfaceArray()
    {
        if (SurfArray)
        {
            for (mfxU32 i = 0; i < SurfArray->NumSurfaces; i++)
            {
                mfxFrameSurface1* s = SurfArray->Surfaces[i];
                std::ignore = MFX_STS_TRACE(s->FrameInterface->Release(s));
            }
            std::ignore = MFX_STS_TRACE(SurfArray->mfxRefCountableBase::Release());
        }
    };

    mfxSurfaceArray* ReleaseContent()
    {
        mfxSurfaceArray* ret = SurfArray;
        SurfArray = nullptr;
        return ret;
    };

    mfxSurfaceArrayImpl* operator->()
    {
        return SurfArray;
    };

protected:
    mfxSurfaceArrayImpl* SurfArray;
};

mfxStatus MFXVideoDECODE_VPP_DecodeFrameAsync(mfxSession session, mfxBitstream* bs, mfxU32* skip_channels, mfxU32 num_skip_channels, mfxSurfaceArray** surf_array_out)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(surf_array_out, MFX_ERR_NULL_PTR);
    if (num_skip_channels != 0)
    {
        MFX_CHECK(skip_channels, MFX_ERR_NULL_PTR);
    }

    mfxStatus mfxRes = MFX_ERR_NONE;
    try
    {
        //skip channels
        std::vector<mfxU32> skipChannels;
        if (num_skip_channels != 0)
        {
            skipChannels.assign(skip_channels, skip_channels + num_skip_channels);
        }


        //DEC
        mfxFrameSurface1* decWork{}, * decOut{}, * decChannelSurf{}, * sfcChannelSurf{};
        mfxSyncPoint decSyncp{};

        do {
            mfxRes = MFXMemory_GetSurfaceForDecode(session, &decWork);
            MFX_CHECK_STS(mfxRes);

            mfxRes = MFXVideoDECODE_DecodeFrameAsync(session, bs, decWork, &decOut, &decSyncp);
            MFX_SAFE_CALL(decWork->FrameInterface->Release(decWork));

            if (mfxRes == MFX_WRN_VIDEO_PARAM_CHANGED && (decOut != nullptr || decSyncp != nullptr))
            {
                throw std::exception();
            }

            // Ignore new SPS warning, repeat DEC call with new surface is necessary
        } while (mfxRes == MFX_ERR_MORE_SURFACE || mfxRes == MFX_WRN_VIDEO_PARAM_CHANGED);

        if (mfxRes != MFX_ERR_NONE)
        {
            return mfxRes;
        }

        if (!decOut || !decSyncp)
        {
            throw std::exception();
        }


        //output DEC and SFC surfaces
        RAIISurfaceArray surfArray;
        mfxU16 sfcChannelID = session->m_pDVP->sfcChannelID;
        if (sfcChannelID != 0)
        {
            decChannelSurf = session->m_pDECODE->GetInternalSurface(decOut);
            MFX_CHECK(decChannelSurf, MFX_ERR_NULL_PTR);
            mfxRes = decChannelSurf->FrameInterface->AddRef(decChannelSurf);
            MFX_CHECK_STS(mfxRes);
            decChannelSurf->Info.ChannelId = decoderChannelID;

            sfcChannelSurf = decOut;
            sfcChannelSurf->Info.ChannelId = sfcChannelID;
        }
        else
        {
            decChannelSurf = decOut;
            decChannelSurf->Info.ChannelId = decoderChannelID;
        }

        //output DEC channel
        if (skipChannels.end() == std::find(skipChannels.begin(), skipChannels.end(), decoderChannelID))
        {
            surfArray->AddSurface(decChannelSurf);
        }
        else
        {
            MFX_SAFE_CALL(decChannelSurf->FrameInterface->Release(decChannelSurf));
        }


        //output SFC channel
        if (sfcChannelID != 0)
        {
            if (skipChannels.end() == std::find(skipChannels.begin(), skipChannels.end(), sfcChannelID))
            {
                surfArray->AddSurface(sfcChannelSurf);
            }
            else
            {
                MFX_SAFE_CALL(sfcChannelSurf->FrameInterface->Release(sfcChannelSurf));
            }
        }


        //VPP
        for (const auto& p : session->m_pDVP->VPPs)
        {
            const mfxU16 id = p.first;
            const auto& vpp = p.second;

            if (skipChannels.end() != std::find(skipChannels.begin(), skipChannels.end(), id))
            {
                //skip this channel
                continue;
            }

            mfxFrameSurface1* vppOut = session->m_pDVP->VppPools[id]->GetSurface();
            MFX_CHECK(vppOut, MFX_ERR_MEMORY_ALLOC);

            //update output crops, they may change only after reset
            mfxVideoParam& VppParams = session->m_pDVP->VppParams[id];
            vppOut->Info.CropX = VppParams.vpp.Out.CropX;
            vppOut->Info.CropY = VppParams.vpp.Out.CropY;
            vppOut->Info.CropW = VppParams.vpp.Out.CropW;
            vppOut->Info.CropH = VppParams.vpp.Out.CropH;

            mfxSyncPoint vppSyncp{};

            static const mfxU32 MFX_NUM_ENTRY_POINTS = 2;
            MFX_ENTRY_POINT entryPoints[MFX_NUM_ENTRY_POINTS]{};
            mfxU32 numEntryPoints = MFX_NUM_ENTRY_POINTS;
            mfxRes = vpp->VppFrameCheck(decChannelSurf, vppOut, nullptr, entryPoints, numEntryPoints);

            if ((MFX_ERR_NONE == mfxRes) ||
                (MFX_ERR_MORE_DATA_SUBMIT_TASK == static_cast<int>(mfxRes)) ||
                (MFX_ERR_MORE_SURFACE == mfxRes) ||
                (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfxRes))
            {
                if (numEntryPoints == 1)
                {
                    if (mfxRes != MFX_ERR_NONE || !entryPoints[0].pRoutine)
                    {
                        throw std::exception();
                    }

                    MFX_TASK task{};
                    task.pOwner = vpp.get();
                    task.entryPoint = entryPoints[0];
                    task.priority = session->m_priority;
                    task.threadingPolicy = MFX_TASK_THREADING_DEDICATED;
                    task.pSrc[0] = decChannelSurf;
                    task.pDst[0] = vppOut;
                    if (MFX_ERR_MORE_DATA_SUBMIT_TASK == static_cast<int>(mfxRes))
                    {
                        task.pDst[0] = nullptr;
                    }
                    //task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_VPP;

                    MFX_SAFE_CALL(session->m_pScheduler->AddTask(task, &vppSyncp));

                    if (vppSyncp == nullptr)
                    {
                        throw std::exception();
                    }
                }
                else
                {
                    if (mfxRes != MFX_ERR_NONE || !entryPoints[0].pRoutine || !entryPoints[1].pRoutine || numEntryPoints != 2)
                    {
                        throw std::exception();
                    }

                    MFX_TASK task{};
                    task.pOwner = vpp.get();
                    task.entryPoint = entryPoints[0];
                    task.priority = session->m_priority;
                    task.threadingPolicy = MFX_TASK_THREADING_DEDICATED;
                    task.pSrc[0] = decChannelSurf;
                    task.pDst[0] = entryPoints[0].pParam;
                    //task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_VPP;

                    MFX_SAFE_CALL(session->m_pScheduler->AddTask(task, &vppSyncp));

                    if (vppSyncp == nullptr)
                    {
                        throw std::exception();
                    }

                    task = {};
                    task.pOwner = vpp.get();
                    task.entryPoint = entryPoints[1];
                    task.priority = session->m_priority;
                    task.threadingPolicy = MFX_TASK_THREADING_DEDICATED;
                    task.pSrc[0] = entryPoints[0].pParam;;
                    task.pDst[0] = vppOut;
                    if (MFX_ERR_MORE_DATA_SUBMIT_TASK == static_cast<int>(mfxRes))
                    {
                        task.pDst[0] = nullptr;
                    }

                    //task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_VPP;

                    MFX_SAFE_CALL(session->m_pScheduler->AddTask(task, &vppSyncp));

                    if (vppSyncp == nullptr)
                    {
                        throw std::exception();
                    }
                }

                if (vppSyncp && vppOut->FrameInterface)
                {
                    MFX_CHECK_HDL(vppOut->FrameInterface->Context);
                    static_cast<mfxFrameSurfaceBaseInterface*>(vppOut->FrameInterface->Context)->SetSyncPoint(vppSyncp);
                }
            }

            if (MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes)
            {
                mfxRes = MFX_ERR_MORE_DATA;
                vppSyncp = nullptr;
            }

            vppOut->Info.ChannelId = id;
            surfArray->AddSurface(vppOut);
        }

        //check for skipChannels
        if (surfArray->NumSurfaces == 0) {
            *surf_array_out = nullptr;
        }
        else {
            *surf_array_out = surfArray.ReleaseContent();
        }
    }
    catch (...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    return mfxRes;
}

static inline bool CmpFrameInfoIgnoreCrops(const mfxFrameInfo& l, const mfxFrameInfo& r)
{
    return MFX_EQ_FIELD(BitDepthLuma)
        && MFX_EQ_FIELD(BitDepthChroma)
        && MFX_EQ_FIELD(Shift)
        && MFX_EQ_FIELD(FourCC)
        && MFX_EQ_FIELD(Width)
        && MFX_EQ_FIELD(Height)
        && MFX_EQ_FIELD(FrameRateExtN)
        && MFX_EQ_FIELD(FrameRateExtD)
        && MFX_EQ_FIELD(AspectRatioW)
        && MFX_EQ_FIELD(AspectRatioH)
        && MFX_EQ_FIELD(PicStruct)
        && MFX_EQ_FIELD(ChromaFormat);
}

mfxStatus MFXVideoDECODE_VPP_Reset(mfxSession session, mfxVideoParam* decode_par, mfxVideoChannelParam** vpp_par_array, mfxU32 num_channel_par)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);

    //check if we need full reset
    if (decode_par != nullptr) {
        MFX_SAFE_CALL(MFXVideoDECODE_VPP_Close(session));
        MFX_RETURN(MFXVideoDECODE_VPP_Init(session, decode_par, vpp_par_array, num_channel_par));
    }
    
    //only VPP reset is requested
    MFX_CHECK(vpp_par_array, MFX_ERR_NULL_PTR);

    //reset VPPs one by one
    for (mfxU32 channelIdx = 0; channelIdx < num_channel_par; channelIdx++)
    {
        mfxVideoChannelParam* channelPar = vpp_par_array[channelIdx];
        MFX_CHECK(channelPar, MFX_ERR_NULL_PTR);

        //check that channel ID is valid
        mfxU16 id = channelPar->VPP.ChannelId;
        auto& p = session->m_pDVP->VppParams;
        if (p.find(id) == p.end()) {
            MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
        }
        mfxVideoParam& VppParams = p[id];

        //check that only crops were changed
        mfxFrameInfo& frameInfoCur = VppParams.vpp.Out;
        mfxFrameInfo& frameInfoNew = channelPar->VPP;
        if (channelPar->Protected != VppParams.Protected ||
            channelPar->IOPattern != (0xf0 & VppParams.IOPattern) || //0xf0 selects MFX_IOPATTERN_OUT_xxx
            !CmpFrameInfoIgnoreCrops(frameInfoCur,   frameInfoNew)
            )
        {
            MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
        }

        //check crops
        if (frameInfoNew.CropX >= frameInfoNew.Width ||
            frameInfoNew.CropY >= frameInfoNew.Height ||
            frameInfoNew.CropW == 0 ||
            frameInfoNew.CropH == 0 ||
            frameInfoNew.CropX + frameInfoNew.CropW > frameInfoNew.Width ||
            frameInfoNew.CropY + frameInfoNew.CropH > frameInfoNew.Height
            )
        {
            MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
        }

        //update crops
        VppParams.vpp.Out = channelPar->VPP;
    }
    
    return MFX_ERR_NONE;
}

//preserve first error,  return first warning if there is no errors
static void UpdateStatus(mfxStatus& res, const mfxStatus cur)
{
    res = res < 0 ? res : cur < 0 ? cur : res > 0 ? res : cur;
}

mfxStatus MFXVideoDECODE_VPP_Close(mfxSession session)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);

    //close decoder, don't exit in case of error, close VPPs also
    mfxStatus res = MFXVideoDECODE_Close(session);

    try
    {
        if (session->m_pDVP) {
            //close VPPs 
            for (const auto& p : session->m_pDVP->VPPs)
            {
                const auto& vpp = p.second;
                mfxStatus cur = session->m_pScheduler->WaitForAllTasksCompletion(vpp.get());
                UpdateStatus(res, cur);
                cur = vpp->Close();
                UpdateStatus(res, cur);
            }

            //delete VPP params, surface pools and ext buffers
            session->m_pDVP.reset();
        }
    }
    catch (...)
    {
        UpdateStatus(res, MFX_ERR_UNKNOWN);
    }

    return res;
}

mfxStatus MFXVideoDECODE_VPP_GetChannelParam(mfxSession session, mfxVideoChannelParam* par, mfxU32 channel_id)
{
    MFX_CHECK_HDL(session);
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(session->m_pDVP, MFX_ERR_NOT_INITIALIZED);

    auto& vpp_params = session->m_pDVP->VppParams;
    MFX_CHECK(vpp_params.find(channel_id) != vpp_params.end(), MFX_ERR_NOT_FOUND);

    par->VPP         = vpp_params[channel_id].vpp.Out;
    par->Protected   = vpp_params[channel_id].Protected;
    par->IOPattern   = vpp_params[channel_id].IOPattern;
    par->NumExtParam = 0;

    return MFX_ERR_NONE;
}

