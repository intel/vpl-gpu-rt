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

#include <mfx_vpp_main.h>

constexpr mfxU16 decoderChannelID = 0;

class DVP_impl : public _mfxSession::DVP_base
{
    virtual surface_cache_controller<SurfaceCache>* GetSurfacePool(mfxU16 channel_id) override
    {
        if (VppPools.find(channel_id) == VppPools.end())
            return nullptr;

        return VppPools[channel_id].get();
    }

    virtual void AssignPool(mfxU16 channel_id, SurfaceCache* cache) override
    {
        VppPools[channel_id].reset(new surface_cache_controller<SurfaceCache>(cache, ComponentType::VPP, MFX_VPP_POOL_OUT));
    }

private:
    std::map<mfxU16, std::unique_ptr<surface_cache_controller<SurfaceCache>>> VppPools;
};

mfxStatus MFXVideoDECODE_VPP_Init(mfxSession session, mfxVideoParam* decode_par, mfxVideoChannelParam** vpp_par_array, mfxU32 num_channel_par)
{

    TRACE_EVENT(MFX_TRACE_API_DECODE_VPP_INIT_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(0));

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

            if (channelPar->NumExtParam)
            {
                MFX_CHECK_NULL_PTR1(channelPar->ExtParam);

                // Check that all ext buffer pointers are valid
                MFX_CHECK(std::all_of(channelPar->ExtParam, channelPar->ExtParam + channelPar->NumExtParam, [](mfxExtBuffer* buffer) { return !!buffer; }), MFX_ERR_NULL_PTR);

                // Check that all buffer ids are unique
                std::vector<mfxExtBuffer*> ext_buf(channelPar->ExtParam, channelPar->ExtParam + channelPar->NumExtParam);
                MFX_CHECK(std::unique(std::begin(ext_buf), std::end(ext_buf),
                    [](mfxExtBuffer* buffer, mfxExtBuffer* other_buffer)
                      {
                          return buffer->BufferId == other_buffer->BufferId;
                      }) == std::end(ext_buf), MFX_ERR_INVALID_VIDEO_PARAM);

                // Check that only allowed buffers passed
                bool valid = std::all_of(channelPar->ExtParam, channelPar->ExtParam + channelPar->NumExtParam,
                    [](mfxExtBuffer* buffer)
                    {
                        return buffer->BufferId == MFX_EXTBUFF_VPP_SCALING
                            || buffer->BufferId == MFX_EXTBUFF_ALLOCATION_HINTS
                            ;
                    });

                MFX_CHECK(valid, MFX_ERR_INVALID_VIDEO_PARAM);

                // Check mfxExtVPPScaling buffer validity
                auto it = std::find_if(std::begin(ext_buf), std::end(ext_buf), [](mfxExtBuffer* buffer) { return buffer->BufferId == MFX_EXTBUFF_VPP_SCALING; });

                if (it != std::end(ext_buf))
                {
                    switch (reinterpret_cast<mfxExtVPPScaling*>(*it)->ScalingMode)
                    {
                    case MFX_SCALING_MODE_DEFAULT:
                    case MFX_SCALING_MODE_LOWPOWER:
                    case MFX_SCALING_MODE_QUALITY:
                    case MFX_SCALING_MODE_INTEL_GEN_VDBOX:
                    case MFX_SCALING_MODE_INTEL_GEN_VEBOX:
                    case MFX_SCALING_MODE_INTEL_GEN_COMPUTE:
                        break;
                    default:
                        MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
                    }
                }

                // Check mfxExtAllocationHints buffer validity
                it = std::find_if(std::begin(ext_buf), std::end(ext_buf), [](mfxExtBuffer* buffer) { return buffer->BufferId == MFX_EXTBUFF_ALLOCATION_HINTS; });

                if (it != std::end(ext_buf))
                {
                    mfxExtAllocationHints& hints_buffer = *reinterpret_cast<mfxExtAllocationHints*>(*it);

                    MFX_SAFE_CALL(CheckAllocationHintsBuffer(hints_buffer));
                    MFX_CHECK(hints_buffer.VPPPoolType == MFX_VPP_POOL_OUT, MFX_ERR_INVALID_VIDEO_PARAM);
                }
            }

            if (decode_par->mfx.FrameInfo.FrameRateExtN != channelPar->VPP.FrameRateExtN ||
                decode_par->mfx.FrameInfo.FrameRateExtD != channelPar->VPP.FrameRateExtD ||
                decode_par->mfx.FrameInfo.PicStruct     != channelPar->VPP.PicStruct)
            {
                MFX_RETURN(MFX_ERR_INVALID_VIDEO_PARAM);
            }
        }

        //create  DVP
        if (!session->m_pDVP)
        {
            session->m_pDVP.reset(new DVP_impl());
            session->m_pDVP->skipOriginalOutput = IsOn(decode_par->mfx.SkipOutput);
        }


        //parse VPP params
        mfxU16 sfcChannelID = 0; //zero ID is reserved for DEC channel, it is invalid value for SFC

        // Need to adjust scaling methods to Intel specific, so need tmp storage to not touch user's buffers
        std::list<std::vector<mfxExtBuffer*>> ext_buffers;
        std::list<mfxExtVPPScaling>           scaling_buffers;

        for (mfxU32 channelIdx = 0; channelIdx < num_channel_par; channelIdx++)
        {
            mfxVideoChannelParam* channelPar = vpp_par_array[channelIdx];
            MFX_CHECK(channelPar, MFX_ERR_NULL_PTR);
            mfxU16 id = channelPar->VPP.ChannelId;
            mfxVideoParam& VppParams = session->m_pDVP->VppParams[id];

            //extract and convert scaling modes
            mfxU16 vppScalingMode = MFX_SCALING_MODE_DEFAULT;

            bool remap_buffer = false;
            auto scaling_buffer = reinterpret_cast<mfxExtVPPScaling*>(mfx::GetExtBuffer(channelPar->ExtParam, channelPar->NumExtParam, MFX_EXTBUFF_VPP_SCALING));

            if (scaling_buffer)
            {
                switch (scaling_buffer->ScalingMode)
                {
                case MFX_SCALING_MODE_INTEL_GEN_VDBOX:
                    {
                        session->m_pDVP->sfcChannelID = sfcChannelID = id;
                    }
                    remap_buffer = true;
                    break;
                case MFX_SCALING_MODE_INTEL_GEN_VEBOX:
                    remap_buffer = true;
                case MFX_SCALING_MODE_LOWPOWER:
                    vppScalingMode = MFX_SCALING_MODE_LOWPOWER;
                    break;
                case MFX_SCALING_MODE_INTEL_GEN_COMPUTE:
                    remap_buffer = true;
                case MFX_SCALING_MODE_QUALITY:
                    vppScalingMode = MFX_SCALING_MODE_QUALITY;
                    break;
                case MFX_SCALING_MODE_DEFAULT:
                    break;
                default:
                    assert(0); //MFX_SCALING_MODE_DEFAULT in release
                }
            }

            //set i/o pattern, limit to homogeneous cases, we can relax it later
            VppParams.IOPattern = static_cast<mfxU16>(
                channelPar->IOPattern |
                (decode_par->IOPattern == MFX_IOPATTERN_OUT_SYSTEM_MEMORY ? MFX_IOPATTERN_IN_SYSTEM_MEMORY : MFX_IOPATTERN_IN_VIDEO_MEMORY));
            MFX_CHECK(VppParams.IOPattern == (MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY) ||
                VppParams.IOPattern == (MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY), MFX_ERR_UNSUPPORTED);

            VppParams.AsyncDepth = decode_par->AsyncDepth;
            VppParams.vpp.In     = decode_par->mfx.FrameInfo;
            VppParams.vpp.Out    = channelPar->VPP;

            VppParams.ExtParam    = channelPar->ExtParam;
            VppParams.NumExtParam = channelPar->NumExtParam;

            if (remap_buffer)
            {
                ext_buffers.emplace_back(VppParams.ExtParam, VppParams.ExtParam + VppParams.NumExtParam);

                auto it_erase = std::remove_if(std::begin(ext_buffers.back()), std::end(ext_buffers.back()),
                         [](mfxExtBuffer* buf)
                         {
                            return buf->BufferId == MFX_EXTBUFF_VPP_SCALING;
                         });
                ext_buffers.back().erase(it_erase, std::end(ext_buffers.back()));

                //add scaling mode buffer
                if (vppScalingMode != MFX_SCALING_MODE_DEFAULT)
                {
                    scaling_buffers.emplace_back();

                    mfxExtVPPScaling& vppScalingExtBuf = scaling_buffers.back();
                    vppScalingExtBuf = {};
                    vppScalingExtBuf.Header.BufferId = MFX_EXTBUFF_VPP_SCALING;
                    vppScalingExtBuf.Header.BufferSz = sizeof(mfxExtVPPScaling);
                    vppScalingExtBuf.ScalingMode     = vppScalingMode;

                    ext_buffers.back().push_back(&vppScalingExtBuf.Header);
                }

                VppParams.ExtParam    = ext_buffers.back().data();
                VppParams.NumExtParam = mfxU16(ext_buffers.back().size());
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

            decVppParams.Out.FourCC       = VppParams.vpp.Out.FourCC;
            decVppParams.Out.ChromaFormat = VppParams.vpp.Out.ChromaFormat;

            decVppParams.Out.Width  = VppParams.vpp.Out.Width;
            decVppParams.Out.Height = VppParams.vpp.Out.Height;
            decVppParams.Out.CropX  = VppParams.vpp.Out.CropX;
            decVppParams.Out.CropY  = VppParams.vpp.Out.CropY;
            decVppParams.Out.CropW  = VppParams.vpp.Out.CropW;
            decVppParams.Out.CropH  = VppParams.vpp.Out.CropH;

            //create local copy of decoder params, to preserve input params in case of exception or early exit
            mfxVideoParam decParams = *decode_par;
            std::vector<mfxExtBuffer*> decExtBuffers(decParams.ExtParam, decParams.ExtParam + decParams.NumExtParam);
            decExtBuffers.push_back(&decVppParams.Header);
            decParams.ExtParam    = decExtBuffers.data();
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

            if (id == sfcChannelID) {
                continue;
            }

            mfxVideoParam& VppParams = p.second;

            if (!session->m_pDVP->VPPs[id])
            {
                session->m_pDVP->VPPs[id].reset(session->Create<VideoVPP>(VppParams));
            }

            // Process cache hints buffer
            if (VppParams.NumExtParam)
            {
                // Remove cache control buffers (they are actually shifted to the end)
                auto p_new_end = std::remove_if(VppParams.ExtParam, VppParams.ExtParam + VppParams.NumExtParam,
                    [](mfxExtBuffer* buffer)
                    {
                        return buffer->BufferId == MFX_EXTBUFF_ALLOCATION_HINTS;
                    });

                if (p_new_end != VppParams.ExtParam + VppParams.NumExtParam)
                {
                    mfxU16 n_original = VppParams.NumExtParam;
                    VppParams.NumExtParam = mfxU16(p_new_end - VppParams.ExtParam);

                    // Only one allocation hint buffer is allowed for any of VPPs inside DVP
                    MFX_CHECK(n_original - VppParams.NumExtParam <= 1, MFX_ERR_INVALID_VIDEO_PARAM);
                }
            }

            mfxRes = session->m_pDVP->VPPs[id]->Init(&VppParams);
            MFX_CHECK_STS(mfxRes);

            // Will not keep (deep copy) Ext Buffers, so clean up here
            VppParams.NumExtParam = 0;
            VppParams.ExtParam    = nullptr;

            CommonCORE_VPL* base_core_vpl = dynamic_cast<CommonCORE_VPL*>(session->m_pCORE.get());
            MFX_CHECK_HDL(base_core_vpl);

            mfxU16 vpp_memtype = mfxU16(MFX_MEMTYPE_FROM_VPPOUT | ((VppParams.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) ? MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET : MFX_MEMTYPE_SYSTEM_MEMORY));

            std::unique_ptr<SurfaceCache> scoped_cache_ptr(SurfaceCache::Create(*base_core_vpl, vpp_memtype, session->m_pDVP->VppParams[id].vpp.Out));
            session->m_pDVP->AssignPool(id, scoped_cache_ptr.get());
            scoped_cache_ptr.release();

            MFX_SAFE_CALL(session->m_pDVP->GetSurfacePool(id)->SetupCache(session, VppParams));
        }
    }
    catch (...)
    {
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }

    TRACE_EVENT(MFX_TRACE_API_DECODE_VPP_INIT_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(MFX_ERR_NONE));

    return MFX_ERR_NONE;
}

class RAIISurfaceArray
{
public:
    RAIISurfaceArray()
    {
        SurfArray = mfxSurfaceArrayImpl::Create();
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
            std::ignore = MFX_STS_TRACE(SurfArray->Release());
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

    TRACE_EVENT(MFX_TRACE_API_DECODE_VPP_FRAMEASYNC_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(0));

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

        MFX_CHECK_STS(mfxRes);

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
        if (skipChannels.end() == std::find(skipChannels.begin(), skipChannels.end(), decoderChannelID)
            && !session->m_pDVP->skipOriginalOutput)
        {
            surfArray->AddSurface(decChannelSurf);
        }
        else
        {
            MFX_SAFE_CALL(ReleaseSurface(*decChannelSurf));
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

            if (skipChannels.end() != std::find(skipChannels.begin(), skipChannels.end(), id))
            {
                //skip this channel
                continue;
            }

            mfxFrameSurface1* vppOut = nullptr;
            MFX_SAFE_CALL((*session->m_pDVP->GetSurfacePool(id))->GetSurface(vppOut));
            MFX_CHECK(vppOut, MFX_ERR_NULL_PTR);

            const auto& vpp = p.second;

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
                    task.pOwner          = vpp.get();
                    task.entryPoint      = entryPoints[0];
                    task.priority        = session->m_priority;
                    task.threadingPolicy = MFX_TASK_THREADING_DEDICATED;
                    task.pSrc[0]         = decChannelSurf;
                    task.pDst[0]         = vppOut;
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
                    task.pOwner          = vpp.get();
                    task.entryPoint      = entryPoints[0];
                    task.priority        = session->m_priority;
                    task.threadingPolicy = MFX_TASK_THREADING_DEDICATED;
                    task.pSrc[0]         = decChannelSurf;
                    task.pDst[0]         = entryPoints[0].pParam;
                    //task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_VPP;

                    MFX_SAFE_CALL(session->m_pScheduler->AddTask(task, &vppSyncp));

                    if (vppSyncp == nullptr)
                    {
                        throw std::exception();
                    }

                    task = {};
                    task.pOwner          = vpp.get();
                    task.entryPoint      = entryPoints[1];
                    task.priority        = session->m_priority;
                    task.threadingPolicy = MFX_TASK_THREADING_DEDICATED;
                    task.pSrc[0]         = entryPoints[0].pParam;;
                    task.pDst[0]         = vppOut;
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

                if (vppSyncp && vppOut->FrameInterface && vppOut->FrameInterface->Synchronize && !session->m_pCORE->IsExternalFrameAllocator())
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

    TRACE_EVENT(MFX_TRACE_API_DECODE_VPP_FRAMEASYNC_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(mfxRes));

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

    TRACE_EVENT(MFX_TRACE_API_DECODE_VPP_RESET_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(0));

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

        MFX_SAFE_CALL(session->m_pDVP->GetSurfacePool(id)->ResetCache(channelPar->ExtParam, channelPar->NumExtParam));

        //MFX_SAFE_CALL(session->m_pDVP->VPPs[id]->Reset(&VppParams));
    }

    TRACE_EVENT(MFX_TRACE_API_DECODE_VPP_RESET_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(MFX_ERR_NONE));

    return MFX_ERR_NONE;
}

//preserve first error,  return first warning if there is no errors
static void UpdateStatus(mfxStatus& res, const mfxStatus cur)
{
    res = res < 0 ? res : cur < 0 ? cur : res > 0 ? res : cur;
}

mfxStatus MFXVideoDECODE_VPP_Close(mfxSession session)
{

    TRACE_EVENT(MFX_TRACE_API_DECODE_VPP_CLOSE_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(0));

    MFX_CHECK(session,               MFX_ERR_INVALID_HANDLE);
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

    TRACE_EVENT(MFX_TRACE_API_DECODE_VPP_CLOSE_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(res));

    return res;
}

mfxStatus MFXVideoDECODE_VPP_GetChannelParam(mfxSession session, mfxVideoChannelParam* par, mfxU32 channel_id)
{
    MFX_CHECK_HDL(session);
    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK(session->m_pDVP, MFX_ERR_NOT_INITIALIZED);

    mfxU16 ChannelId = static_cast<mfxU16>(channel_id);
    auto& vpp_params = session->m_pDVP->VppParams;
    MFX_CHECK(vpp_params.find(ChannelId) != vpp_params.end(), MFX_ERR_NOT_FOUND);

    par->VPP         = vpp_params[ChannelId].vpp.Out;
    par->Protected   = vpp_params[ChannelId].Protected;
    par->IOPattern   = vpp_params[ChannelId].IOPattern;
    par->NumExtParam = 0;

    return MFX_ERR_NONE;
}

