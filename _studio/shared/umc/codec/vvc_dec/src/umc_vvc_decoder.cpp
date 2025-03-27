// Copyright (c) 2022-2024 Intel Corporation
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

#include "umc_defs.h"
#ifdef MFX_ENABLE_VVC_VIDEO_DECODE

#include "umc_vvc_decoder.h"
#include "umc_vvc_mfx_utils.h"

#if defined(MFX_ENABLE_PXP)
#include "mfx_pxp_vvc_nal_spl.h"
#endif

namespace UMC_VVC_DECODER
{
    const uint32_t levelIdxArray[] = {
        VVC_LEVEL_1,
        VVC_LEVEL_2,
        VVC_LEVEL_21,

        VVC_LEVEL_3,
        VVC_LEVEL_31,

        VVC_LEVEL_4,
        VVC_LEVEL_41,

        VVC_LEVEL_5,
        VVC_LEVEL_51,
        VVC_LEVEL_52,

        VVC_LEVEL_6,
        VVC_LEVEL_61,
        VVC_LEVEL_62
    };

    // Find an index of specified level
    static uint32_t GetLevelIDCIndex(uint32_t level_idc)
    {
        for (uint32_t i = 0; i < sizeof(levelIdxArray)/sizeof(levelIdxArray[0]); i++)
        {
            if (levelIdxArray[i] == level_idc)
                return i;
        }

        return sizeof(levelIdxArray)/sizeof(levelIdxArray[0]) - 1;
    }

    inline void SetDecodeErrorTypes(NalUnitType nalUnit, mfxExtDecodeErrorReport *pDecodeErrorReport)
    {
        if (!pDecodeErrorReport)
            return;

        switch (nalUnit)
        {
            case NAL_UNIT_SPS: pDecodeErrorReport->ErrorTypes |= MFX_ERROR_SPS; break;
            case NAL_UNIT_PPS: pDecodeErrorReport->ErrorTypes |= MFX_ERROR_PPS; break;
            default: break;
        };
    }

    // Check if bitstream resolution has changed
    static bool IsNeedSPSInvalidate(const VVCSeqParamSet* old_sps, const VVCSeqParamSet* new_sps)
    {
        if (!old_sps || !new_sps)
            return false;

        if (old_sps->sps_pic_width_max_in_luma_samples != new_sps->sps_pic_width_max_in_luma_samples)
            return true;

        if (old_sps->sps_pic_height_max_in_luma_samples != new_sps->sps_pic_height_max_in_luma_samples)
            return true;

        if (old_sps->sps_bitdepth_minus8 != new_sps->sps_bitdepth_minus8)
            return true;

        if (old_sps->sps_chroma_format_idc != new_sps->sps_chroma_format_idc)
            return true;

        return false;
    }

/****************************************************************************************/
// VVCDecoder class implementation
/****************************************************************************************/

    VVCDecoder::VVCDecoder()
        : m_allocator(nullptr)
        , m_counter(0)
        , m_firstMfxVideoParams()
        , m_currFrame(nullptr)
        , m_frameOrder(0)
        , m_dpbSize(0)
        , m_DPBSizeEx(0)
        , m_sps_max_dec_pic_buffering(0)
        , m_sps_max_num_reorder_pics(0)
        , m_localDeltaFrameTime(0)
        , m_useExternalFramerate(false)
        , m_decOrder(false)
        , m_localFrameTime(0)
        , m_maxUIDWhenDisplayed(0)
        , m_level_idc(0)
        , m_currHeaders(&m_ObjHeap)
        , m_va(nullptr)
        , m_RA_POC(0)
        , m_noRaslOutputFlag(0)
        , m_IRAPType()
        , m_checkCRAInsideResetProcess(false)
        , m_FirstAU(nullptr)
        , m_IsShouldQuit(false)
        , repeateFrame()
        , m_prevSliceSkipped(false)
        , m_skippedPOC(0)
        , m_skippedLayerID(0)
        , m_pocRandomAccess(VVC_MAX_INT)
        , m_tOlsIdxTidExternalSet(false)
        , m_tOlsIdxTidOpiSet(false)
    {
        std::fill_n(m_lastNoOutputBeforeRecoveryFlag, VVC_MAX_VPS_LAYERS, false);
        std::fill_n(m_firstSliceInSequence, VVC_MAX_VPS_LAYERS, true);
        std::fill_n(m_prevGDRInSameLayerPOC, VVC_MAX_VPS_LAYERS, -VVC_MAX_INT);
        std::fill_n(m_prevGDRInSameLayerRecoveryPOC, VVC_MAX_VPS_LAYERS, -VVC_MAX_INT);
        std::fill_n(m_gdrRecoveryPeriod, VVC_MAX_VPS_LAYERS, false);
    }

    VVCDecoder::~VVCDecoder()
    {
        std::for_each(std::begin(m_dpb), std::end(m_dpb),
            std::default_delete<VVCDecoderFrame>()
        );
    }

    UMC::Status VVCDecoder::Init(UMC::BaseCodecParams *vp)
    {
        auto dp = dynamic_cast<VVCDecoderParams*> (vp);
        MFX_CHECK(dp, UMC::UMC_ERR_INVALID_PARAMS);
        MFX_CHECK(dp->allocator, UMC::UMC_ERR_INVALID_PARAMS);

        m_allocator = dp->allocator;

        m_useExternalFramerate = 0 < dp->info.framerate;

        m_localDeltaFrameTime = m_useExternalFramerate ? 1 / dp->info.framerate : 1.0/30;

        m_params = *dp;
        m_seiMessages.reset(new SEI_Storer_VVC);
        m_seiMessages->Init();

#if defined(MFX_ENABLE_PXP)
        m_splitter.reset(new PXPNALUnitSplitter_VVC());
#else
        m_splitter.reset(new Splitter_VVC);
#endif
        m_DPBSizeEx = dp->info.bitrate;
        m_splitter->Reset();
        m_currHeaders.Reset(false);
        m_ObjHeap.Release();

        m_frameOrder = 0;

        return SetParams(vp);
    }

    UMC::Status VVCDecoder::Close()
    {
        return Reset();
    }

    UMC::Status VVCDecoder::Reset()
    {
        m_counter = 0;

        VVCDecoderFrame* pFrame;

        for (DPBType::iterator it = m_dpb.begin(); it != m_dpb.end(); it++)
        {
            pFrame = *it;
            pFrame->FreeResources();
        }

        for (DPBType::iterator it = m_dpb.begin(); it != m_dpb.end(); it++)
        {
            pFrame = *it;
            pFrame->Reset();
        }

        m_lastSlice = 0;
        m_localFrameTime = 0;
        m_currFrame = nullptr;
        m_FirstAU = 0;
        m_level_idc = 0;

        m_rawHeaders.Reset();
        m_splitter->Reset();
        m_currHeaders.Reset(false);
        m_ObjHeap.Release();
        m_pocDecoding.Reset();
        m_decodingQueue.clear();
        m_completedQueue.clear();

        if (m_seiMessages)
        {
            m_seiMessages->Reset();
        }

        Skipping_VVC::Reset();

        m_frameOrder = 0;

        return UMC::UMC_OK;
    }

    void VVCDecoder::SetVideoParams(const mfxVideoParam &par)
    {
        m_firstMfxVideoParams = par;
        m_decOrder = m_firstMfxVideoParams.mfx.DecodedOrder != 0;
    }

    UMC::Status VVCDecoder::GetInfo(UMC::BaseCodecParams *info)
    {
        auto vp = dynamic_cast<UMC::VideoDecoderParams*> (info);
        MFX_CHECK(vp, UMC::UMC_ERR_INVALID_PARAMS);

        *vp = m_params;
        return UMC::UMC_OK;
    }

    // Calculate maximum DPB size based on level and resolution
    uint32_t VVCDecoder::CalculateDPBSize(uint32_t profile_idc, uint32_t &level_idc, int32_t width, int32_t height, uint32_t num_ref_frames)
    {
        // Max luma picture size array
        uint32_t lumaPsArray[] = { 36864, 122880, 245760,552960,983040, 2228224, 2228224, 8912896,
                                   8912896, 8912896, 35651584, 35651584, 35651584 };
        uint32_t MaxDpbSize = 16;

        for (;;)
        {
            uint32_t index = GetLevelIDCIndex(level_idc);

            uint32_t MaxLumaPs = lumaPsArray[index];
            uint32_t const maxDpbPicBuf = 6; //second version of current reference (twoVersionsOfCurrDecPicFlag)
            (void)profile_idc;

            uint32_t PicSizeInSamplesY = width * height;

            if (PicSizeInSamplesY  <=  (MaxLumaPs  >>  2 ))
            {
                MaxDpbSize = std::min(4 * maxDpbPicBuf, 16u);
            }
            else if (PicSizeInSamplesY  <=  (MaxLumaPs  >>  1 ))
            {
                MaxDpbSize = std::min(2 * maxDpbPicBuf, 16u);
            }
            else if (PicSizeInSamplesY  <=  ((3 * MaxLumaPs)  >>  2 ))
            {
                MaxDpbSize = std::min((4 * maxDpbPicBuf) / 3, 16u);
            }
            else
            {
                MaxDpbSize = maxDpbPicBuf;
            }

            if (num_ref_frames <= MaxDpbSize)
                break;


            if (index >= sizeof(levelIdxArray)/sizeof(levelIdxArray[0]) - 1)
                break;

            level_idc = levelIdxArray[index + 1];

        }

        return MaxDpbSize;
    }

    void VVCDecoder::SetDPBSize(const VVCSeqParamSet* pSps/*, const VVCVideoParamSet* pVps, uint32_t& level_idc*/)
    {
        m_dpbSize = pSps->dpb_parameter.dpb_max_dec_pic_buffering_minus1[pSps->sps_max_sublayers_minus1] + 1 + m_DPBSizeEx;
        //multi-layer: use VPS->vps_num_layers_in_ols[ targetOlsIdx ] to use sps/vps dpb structure
    }

    VVCDecoderFrame* VVCDecoder::CreateUnavailablePicture(VVCSlice *pSlice, int rplx, int refPicIndex, const int iUnavailablePoc)
    {
        const VVCSeqParamSet* sps = m_currHeaders.m_seqParams.GetCurrentHeader();
        bool longTermFlag = pSlice->m_sliceHeader.sh_rpl[rplx].isLongReference[refPicIndex];
        VVCDecoderFrame* frame = GetOldestDisposable();
        if (!frame)
        {
            frame = new VVCDecoderFrame;
            m_dpb.push_back(frame);
        }

        frame->setAsUnavailableRef(true);
        frame->m_decOrder   = 0;
        frame->setAsReferenced(true);
        frame->IncrementReference();
        frame->SetisLongTermRef(longTermFlag);
        frame->setPicOrderCnt(iUnavailablePoc);
        frame->m_index = (UMC::FrameMemID)(sps->dpb_parameter.dpb_max_dec_pic_buffering_minus1[sps->sps_max_sublayers_minus1] + sps->dpb_parameter.dpb_max_num_reorder_pics[sps->sps_max_sublayers_minus1] + 1 + rplx);
        assert(frame->m_index <= MFX_MAX_PICTURE_INDEX_VALUE_VVC);

        return frame;
    }

    void VVCDecoder::DetectUnavailableRefFrame(VVCSlice *slice)
    {
        const VVCSeqParamSet* sps = m_currHeaders.m_seqParams.GetCurrentHeader();
        const VVCPicParamSet* pps = m_currHeaders.m_picParams.GetCurrentHeader();

        if (!sps || !pps)
            return;

        int lostPoc, refPicIndex;
        while ((lostPoc = slice->CheckAllRefPicsAvail(m_dpb, REF_PIC_LIST_0, &refPicIndex)) > 0)
        {
            if( !pps->pps_mixed_nalu_types_in_pic_flag && (
                ( ( slice->m_sliceHeader.nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL || slice->m_sliceHeader.nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_N_LP ) && ( sps->sps_idr_rpl_present_flag || pps->pps_rpl_info_in_ph_flag ) ) ||
                ( ( slice->m_sliceHeader.nal_unit_type == NAL_UNIT_CODED_SLICE_GDR || slice->m_sliceHeader.nal_unit_type == NAL_UNIT_CODED_SLICE_CRA ) && m_currHeaders.m_phParams.no_output_before_recovery_flag ) ) )
            {
                if (slice->m_sliceHeader.sh_rpl[0].inter_layer_ref_pic_flag[refPicIndex] == 0)
                {
                    CreateUnavailablePicture(slice, REF_PIC_LIST_0, refPicIndex, lostPoc);
                }
            }
            else
            {
                //Create Lost Picture for error handling
            }
        }
        while ((lostPoc = slice->CheckAllRefPicsAvail(m_dpb, REF_PIC_LIST_1, &refPicIndex)) > 0)
        {
            if( !pps->pps_mixed_nalu_types_in_pic_flag && (
                ( ( slice->m_sliceHeader.nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL || slice->m_sliceHeader.nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_N_LP ) && ( sps->sps_idr_rpl_present_flag || pps->pps_rpl_info_in_ph_flag ) ) ||
                ( ( slice->m_sliceHeader.nal_unit_type == NAL_UNIT_CODED_SLICE_GDR || slice->m_sliceHeader.nal_unit_type == NAL_UNIT_CODED_SLICE_CRA ) && m_currHeaders.m_phParams.no_output_before_recovery_flag ) ) )
            {
                if (slice->m_sliceHeader.sh_rpl[1].inter_layer_ref_pic_flag[refPicIndex] == 0)
                {
                    CreateUnavailablePicture(slice, REF_PIC_LIST_1, refPicIndex, lostPoc);
                }
            }
            else
            {
                //Create Lost Picture for error handling
            }
        }
    }

    // Update DPB contents marking frames for reuse
    void VVCDecoder::DPBUpdate(VVCSlice *slice)
    {
        DetectUnavailableRefFrame(slice);
        slice->UpdateRPLMarking(m_dpb);
        slice->ResetUnusedFrames(m_dpb);
    }

    UMC::Status VVCDecoder::FillVideoParam(mfxVideoParam *par/*, bool full*/)
    {
        const VVCVideoParamSet* vps = m_currHeaders.m_videoParams.GetCurrentHeader();
        const VVCSeqParamSet* sps = m_currHeaders.m_seqParams.GetCurrentHeader();
        const VVCPicParamSet* pps = m_currHeaders.m_picParams.GetCurrentHeader();

        if (!sps)
            return UMC::UMC_ERR_FAILED;

        if (MFX_Utility::FillVideoParam(vps, sps, pps, par) != UMC::UMC_OK)
            return UMC::UMC_ERR_FAILED;

        return UMC::UMC_OK;
    }

    bool VVCDecoder::isRandomAccessSkipPicture(VVCSlice *slc, bool mixedNaluInPicFlag, bool phOutputFlag)
    {
        if (slc->GetSliceHeader()->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL || slc->GetSliceHeader()->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_N_LP)
        {
            m_pocRandomAccess = -VVC_MAX_INT;   // able to decode, no need to skip the reordered pics in IDR
        }
        else if (m_pocRandomAccess == VVC_MAX_INT)  // start of random access point, m_pocRandomAccess has not been set yet.
        {
            if (slc->GetSliceHeader()->nal_unit_type == NAL_UNIT_CODED_SLICE_CRA || slc->GetSliceHeader()->nal_unit_type == NAL_UNIT_CODED_SLICE_GDR)
            {
                m_pocRandomAccess = slc->GetSliceHeader()->m_poc;
            }
            else
            {
                return true;
            }
        }
        // skipped the reordered pics
        else if (slc->GetSliceHeader()->m_poc < m_pocRandomAccess && phOutputFlag &&
            (slc->GetSliceHeader()->nal_unit_type == NAL_UNIT_CODED_SLICE_RASL || mixedNaluInPicFlag))
        {
            return true;
        }
        return false;
    }      

    UMC::Status VVCDecoder::DecodeHeader(mfxBitstream *bs, mfxVideoParam *par)
    {
        UMC::Status sts = UMC::UMC_OK;

        MFXMediaDataAdapter in(bs);

        // Remove FLAG_VIDEO_DATA_NOT_FULL_FRAME flag so that we
        // can handle partial unit (corrupted) in a proper way.
        in.SetFlags(0);
        UMC::MediaData *data = &in;

        if (!data)
        {
            return UMC::UMC_ERR_NULL_PTR;
        }

        if (!data->GetDataSize())
        {
            return UMC::UMC_ERR_NOT_ENOUGH_DATA;
        }

        VVCSeqParamSet sps = {};
        VVCPicParamSet pps = {};
        VVCVideoParamSet vps = {};
        bool isSPSFound = false;
        bool isPPSFound = false;
        bool isVPSFound = false;

        Splitter_VVC  naluSplitter;
        VVCHeadersBitstream bitStream;
        bool isEnough = false; // Check if got sps/pps to fill video params

        for ( ;data->GetDataSize() >= MINIMAL_DATA_SIZE; )
        {
            isEnough = isSPSFound && isPPSFound;
            naluSplitter.MoveToStartCode(data); // move data pointer to start code

            if (!isSPSFound && !isVPSFound) // sps/vps not found yet, move pointer to first start code
            {
                bs->DataOffset = (mfxU32)((mfxU8*)data->GetDataPointer() - (mfxU8*)data->GetBufferPointer());
                bs->DataLength = (mfxU32)data->GetDataSize();
            }

            UMC::MediaDataEx *nalUnit = naluSplitter.GetNalUnits(data);

            if (!nalUnit)
            {
                return UMC::UMC_ERR_NOT_ENOUGH_DATA;
            }

            UMC::MediaDataEx::_MediaDataEx* mediaDataEx = nalUnit->GetExData();

            if (!mediaDataEx)
            {
                return UMC::UMC_ERR_NULL_PTR;
            }

            auto nut = mediaDataEx->values[mediaDataEx->index];

            MemoryPiece srcData;
            MemoryPiece swappedMem;

            srcData.SetData(nalUnit);
            swappedMem.Allocate(nalUnit->GetDataSize() + DEFAULT_UNIT_TAIL_VALUE);

            // convert bistream data to little endian integers and remove emu-prevention bytes
            naluSplitter.SwapMemory(&swappedMem, &srcData, 0);
            bitStream.Reset((uint8_t*)swappedMem.GetPointer(), (uint32_t)swappedMem.GetDataSize());

            NalUnitType nal_unit_type;
            uint32_t temporal_id = 0;
            uint32_t nuh_layer_id = 0;

            bitStream.GetNALUnitType(nal_unit_type, temporal_id, nuh_layer_id);

            if (nut == NAL_UNIT_SPS)
            {
                try
                {
                    sts = bitStream.GetSequenceParamSet(&sps);
                    if (sts == UMC::UMC_OK)
                    {
                        isSPSFound = true;
                    }
                }
                catch(const vvc_exception&)
                {
                    continue;
                }
            }
            else if (nut == NAL_UNIT_PPS)
            {
                // pps already found
                if (isPPSFound)
                {
                    continue;
                }

                try
                {
                    sts = bitStream.GetPictureParamSet(&pps);
                    if (sts == UMC::UMC_OK)
                    {
                        isPPSFound = true;
                    }
                }
                catch(const vvc_exception&)
                {
                    continue;
                }
            }
            else if (nut == NAL_UNIT_VPS)
            {
                try
                {
                    sts = bitStream.GetVideoParamSet(&vps);
                    if (sts == UMC::UMC_OK)
                    {
                        isVPSFound = true;
                    }
                }
                catch (const vvc_exception&)
                {
                    continue;
                }
            }
            else if (isEnough)
            {
                break;
            }
        }

        for (uint32_t i = 0; i < pps.pps_rect_slices.size(); i++)
        {
            delete pps.pps_rect_slices[i];
        }
        pps.pps_rect_slices.clear();
        for (uint32_t k = 0; k < pps.pps_sub_pics.size(); k++)
        {
            delete pps.pps_sub_pics[k];
        }
        pps.pps_sub_pics.clear();
        for (uint32_t j = 0; j < pps.pps_slice_map.size(); j++)
        {
            delete pps.pps_slice_map[j];
        }
        pps.pps_slice_map.clear();

        // Got sps and pps, it is enough to fill mfx video params
        if (isEnough)
        {
            sts = UMC_VVC_DECODER::MFX_Utility::FillVideoParam(&vps , &sps, &pps, par);
            return sts;
        }

        // If didn't find headers after consumed all bs data
        bs->DataOffset = (mfxU32)((uint8_t*)in.GetDataPointer() - (uint8_t*)in.GetBufferPointer());
        bs->DataLength = (mfxU32)in.GetDataSize();

        // Request more data
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    VVCSlice *VVCDecoder::DecodeSliceHeader(UMC::MediaData *nalUnit)
    {
        // Check availability of headers
        if ((0 > m_currHeaders.m_seqParams.GetCurrentID()) ||
            (0 > m_currHeaders.m_picParams.GetCurrentID()))
        {
            return nullptr;
        }

        std::unique_ptr<VVCSlice> slice (new VVCSlice); // unique_ptr is to prevent a possible memory leak

        const size_t size = nalUnit->GetDataSize() + DEFAULT_UNIT_TAIL_SIZE;

        MemoryPiece memCopy;
        memCopy.SetData(nalUnit);

        slice->m_source.Allocate(size);

        // TODO: notifier0<MemoryPiece> memory_leak_preventing(&pSlice->m_source, &MemoryPiece::Release)
        std::vector<uint32_t> removed_offsets(0);
        m_splitter->SwapMemory(&slice->m_source, &memCopy, &removed_offsets);

        if (slice->m_source.GetSize() < size)
            throw vvc_exception(UMC::UMC_ERR_ALLOC);

        slice->m_source.SetDataSize(size);

        int prevPicPoc = m_lastSlice ? m_lastSlice->GetSliceHeader()->m_poc : VVC_MAX_INT;
        bool sliceReady = slice->Reset(&m_currHeaders, &m_pocDecoding, prevPicPoc);
        if (!sliceReady)
        {
            return 0;
        }

        VVCSliceHeader* sliceHdr = slice->GetSliceHeader();
        assert(sliceHdr);

        VVCPicParamSet const* pps = slice->GetPPS();
        if (!pps)
        {
            return 0;
        }

        if (isRandomAccessSkipPicture(slice.get(), pps->pps_mixed_nalu_types_in_pic_flag, slice->GetPictureHeader()->ph_pic_output_flag))
        {
            return 0;
        }

        // Handle multiple APS
        slice->SetAPS(&m_currHeaders.m_adaptionParams);

        if (sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL
            || sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_N_LP
            || sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_CRA
            || sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_GDR)
        {
            if (!pps->pps_mixed_nalu_types_in_pic_flag)
            {
                if (m_firstSliceInSequence[sliceHdr->nuh_layer_id])
                {
                    m_currHeaders.m_phParams.no_output_before_recovery_flag = true;
                }
                else if (sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_W_RADL || sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_IDR_N_LP)
                {
                    m_currHeaders.m_phParams.no_output_before_recovery_flag = true;
                }
            }
            else
            {
                m_currHeaders.m_phParams.no_output_before_recovery_flag = false;
            }

            //the inference for NoOutputOfPriorPicsFlag
            if (!sliceHdr->first_slice_in_sequence && m_currHeaders.m_phParams.no_output_before_recovery_flag)
            {
                sliceHdr->sh_no_output_of_prior_pics_flag = true;
            }
            else
            {
                sliceHdr->sh_no_output_of_prior_pics_flag = false;
            }

            if (sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_CRA || sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_GDR)
            {
                m_lastNoOutputBeforeRecoveryFlag[sliceHdr->nuh_layer_id] = m_currHeaders.m_phParams.no_output_before_recovery_flag;
            }
        }

        //For inference of PicOutputFlag
        if (!pps->pps_mixed_nalu_types_in_pic_flag && (sliceHdr->nal_unit_type == NAL_UNIT_CODED_SLICE_RASL))
        {
            if (m_lastNoOutputBeforeRecoveryFlag[sliceHdr->nuh_layer_id])
            {
                m_currHeaders.m_phParams.ph_pic_output_flag = false;
            }
        }
        m_firstSliceInSequence[sliceHdr->nuh_layer_id] = false;
        sliceHdr->first_slice_in_sequence = false;

        //update picheader params in slice
        slice->GetPictureHeader()->no_output_before_recovery_flag = m_currHeaders.m_phParams.no_output_before_recovery_flag;
        slice->GetPictureHeader()->ph_pic_output_flag = m_currHeaders.m_phParams.ph_pic_output_flag;

        // skip pictures due to random access
        if (isRandomAccessSkipPicture(slice.get(), pps->pps_mixed_nalu_types_in_pic_flag, m_currHeaders.m_phParams.ph_pic_output_flag))
        {
            m_prevSliceSkipped = true;
            m_skippedPOC = sliceHdr->m_poc;
            m_skippedLayerID = sliceHdr->nuh_layer_id;
            return nullptr;
        }
        
        uint32_t currOffset = sliceHdr->m_HeaderBitstreamOffset;
        uint32_t currOffsetWithEmul = currOffset;
        
        uint32_t headersEmuls = 0;
        for (; headersEmuls < removed_offsets.size(); headersEmuls++)
        {
            if (removed_offsets[headersEmuls] < currOffsetWithEmul)
                currOffsetWithEmul++;
            else
                break;
        }
        
        slice->m_NumEmuPrevnBytesInSliceHdr = headersEmuls;

        
        if (nalUnit->GetFlags() & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME)
        {
            slice->m_source.Allocate(nalUnit->GetDataSize() + DEFAULT_UNIT_TAIL_SIZE);
            MFX_INTERNAL_CPY(slice->m_source.GetPointer(), nalUnit->GetDataPointer(), (uint32_t)nalUnit->GetDataSize());
            memset(slice->m_source.GetPointer() + nalUnit->GetDataSize(), DEFAULT_UNIT_TAIL_VALUE, DEFAULT_UNIT_TAIL_SIZE);
            slice->m_source.SetDataSize(nalUnit->GetDataSize());
            slice->m_source.SetTime(nalUnit->GetTime());
        }
        else
        {
            slice->m_source.SetData(nalUnit);
        }

        uint32_t* pbs;
        uint32_t bitOffset;
        slice->GetBitStream()->GetState(&pbs, &bitOffset);
        size_t bytes = slice->GetBitStream()->BytesDecodedRoundOff();
        slice->GetBitStream()->Reset(slice->m_source.GetPointer(), bitOffset,
            (uint32_t)slice->m_source.GetDataSize());
        slice->GetBitStream()->SetState((uint32_t*)(slice->m_source.GetPointer() + bytes), bitOffset);

        VVCSlice* slc = slice.release();

        return slc;
    }

    // Returns whether frame decoding is finished
    bool VVCDecoder::IsFrameCompleted(VVCDecoderFrame* pFrame) const
    {
        if (!pFrame)
            return true;

        VVCDecoderFrameInfo::FillnessStatus status = pFrame->GetAU()->GetStatus();

        bool ret;
        switch (status)
        {
        case VVCDecoderFrameInfo::STATUS_NONE:
            ret = true;
            break;
        case VVCDecoderFrameInfo::STATUS_NOT_FILLED:
            ret = false;
            break;
        case VVCDecoderFrameInfo::STATUS_COMPLETED:
            ret = true;
            break;
        default:
            ret = (pFrame->GetAU()->GetStatus() == VVCDecoderFrameInfo::STATUS_COMPLETED);
            break;
        }

        return ret;
    }

    // Check whether frame is prepared
    bool VVCDecoder::PrepareFrame(VVCDecoderFrame* pFrame)
    {
        if (!pFrame || pFrame->prepared)
        {
            return true;
        }

        if (pFrame->prepared)
            return true;

        if (pFrame->GetAU()->GetStatus() == VVCDecoderFrameInfo::STATUS_FILLED || pFrame->GetAU()->GetStatus() == VVCDecoderFrameInfo::STATUS_STARTED)
        {
            pFrame->prepared = true;
        }

        return true;
    }

    // Update DPB & Ref picture contents marking for reuse
    UMC::Status VVCDecoder::UpdateDPB(const VVCSlice &slice)
    {

        UMC::Status umcRes = UMC::UMC_OK;

        VVCSliceHeader const* sliceHeader = slice.GetSliceHeader();
        if (sliceHeader->IdrPicFlag)
        {
            // clean up DPB with marking all reference pictures as unused
        }
        else
        {
            // ...
        }    // not IDR picture
        return umcRes;
    }

    // Return number of active short and long term reference frames.
    void VVCDecoder::countActiveRefs(uint32_t& NumShortTerm, uint32_t& NumLongTerm)
    {
        NumShortTerm = 0;
        NumLongTerm = 0;

        VVCDecoderFrame* pFrame;

        for (DPBType::iterator it = m_dpb.begin(); it != m_dpb.end(); it++)
        {
            pFrame = *it;
            if (pFrame->isShortTermRef())
                NumShortTerm++;
            else if (pFrame->isLongTermRef())
                NumLongTerm++;
        }
    }

    bool VVCDecoder::IsEnoughForStartDecoding(bool)
    {
        std::unique_lock<std::mutex> l(m_guard);

        InitAUs();
        return m_FirstAU != 0;
    }

    void VVCDecoder::PreventDPBFullness() 
    {
        std::unique_lock<std::mutex> l(m_guard);
        m_FirstAU = 0;
        m_decodingQueue.clear();
        m_completedQueue.clear();

        VVCDecoderFrame* pFrame;

        for (DPBType::iterator it = m_dpb.begin(); it != m_dpb.end(); it++)
        {
            pFrame = *it;
            pFrame->FreeResources();
            pFrame->Reset();
        }

        if (m_lastSlice)
        {
            m_lastSlice->Release();
            m_ObjHeap.FreeObject(&m_lastSlice);
            m_lastSlice = 0;
        }

        m_ObjHeap.Release();

        m_decOrder = false;
        m_checkCRAInsideResetProcess = false;
        m_maxUIDWhenDisplayed = 0;
    }

    UMC::Status VVCDecoder::AddSource(UMC::MediaData *pSource)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);
        VVCDecoderFrame* completed = 0;
        UMC::Status umcRes = CompleteDecodedFrames(&completed);
        if (umcRes != UMC::UMC_OK)
            return pSource || !completed ? umcRes : UMC::UMC_OK;

        if (GetFrameToDisplay(false))
            return UMC::UMC_OK;

        umcRes = AddOneFrame(pSource);

        if (UMC::UMC_ERR_NOT_ENOUGH_BUFFER == umcRes) 
        {
            VVCDecoderFrame* pTmp = nullptr;
            for (DPBType::iterator it = m_dpb.begin(); it != m_dpb.end(); it++)
            {
                pTmp = *it;
                // Frame is being processed. Wait for asynchronous end of operation.
                if (pTmp->isDisposable() || pTmp->isInDisplayingStage() || pTmp->isAlmostDisposable())
                {
                    return UMC::UMC_WRN_INFO_NOT_READY;
                }
            }

            if (GetFrameToDisplay(true))
                return UMC::UMC_ERR_NEED_FORCE_OUTPUT;

            // More hard reasosn of frame lack
            if (!IsEnoughForStartDecoding(true)) 
            {
                umcRes = CompleteDecodedFrames(&completed);
                if (umcRes != UMC::UMC_OK)
                    return umcRes;
                else if (completed)
                    return UMC::UMC_WRN_INFO_NOT_READY;

                if (GetFrameToDisplay(true))
                    return UMC::UMC_ERR_NEED_FORCE_OUTPUT;

                // Try to reset in case DPB has overflown
                PreventDPBFullness();
                return UMC::UMC_WRN_INFO_NOT_READY;
            }
            
        }

        return umcRes;
    }

    UMC::Status VVCDecoder::AddOneFrame(UMC::MediaData *pSource)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);
        if (m_lastSlice)
        {
            UMC::Status sts = AddSlice(m_lastSlice.release());
            if (sts == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || sts == UMC::UMC_OK)
                return sts;
        }

        if (m_checkCRAInsideResetProcess && !pSource)
            return UMC::UMC_ERR_FAILED;

        size_t moveToSpsOffset = m_checkCRAInsideResetProcess ? pSource->GetDataSize() : 0;

        do
        {
            UMC::MediaDataEx* nalUnit = m_splitter->GetNalUnits(pSource);

            if (!nalUnit)
            {
                break;
            }

            UMC::MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();

            UMC::MediaData::AuxInfo* aux = (pSource) ? pSource->GetAuxInfo(MFX_EXTBUFF_DECODE_ERROR_REPORT) : NULL;
            mfxExtDecodeErrorReport* pDecodeErrorReport = (aux) ? reinterpret_cast<mfxExtDecodeErrorReport*>(aux->ptr) : NULL;

            for (int32_t i = 0; i < (int32_t)pMediaDataEx->count; i++, pMediaDataEx->index++)
            {
                if (m_checkCRAInsideResetProcess)
                {
                    switch ((NalUnitType)pMediaDataEx->values[i])
                    {
                    case NAL_UNIT_CODED_SLICE_TRAIL:
                    case NAL_UNIT_CODED_SLICE_STSA:
                    case NAL_UNIT_CODED_SLICE_RADL:
                    case NAL_UNIT_CODED_SLICE_RASL:
                    case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
                    case NAL_UNIT_CODED_SLICE_IDR_N_LP:
                    case NAL_UNIT_CODED_SLICE_CRA:
                    case NAL_UNIT_CODED_SLICE_GDR:
                    {
                        std::unique_ptr<VVCSlice> slice(new VVCSlice); // unique_ptr is to prevent a possible memory leak
                        
                        const size_t size = nalUnit->GetDataSize() + DEFAULT_UNIT_TAIL_SIZE;

                        nalUnit->SetDataSize(100);  // is enought to retrieve

                        MemoryPiece memCopy;
                        memCopy.SetData(nalUnit);

                        slice->m_source.Allocate(size);

                        m_splitter->SwapMemory(&slice->m_source, &memCopy, 0);

                        m_checkCRAInsideResetProcess = false;
                        pSource->MoveDataPointer(int32_t(pSource->GetDataSize() - moveToSpsOffset));
                        m_splitter->Reset();
                        return UMC::UMC_NTF_NEW_RESOLUTION;
                    }
                    break;

                    case NAL_UNIT_VPS:
                    case NAL_UNIT_SPS:
                    case NAL_UNIT_PPS:
                    {
                        UMC::Status sts = DecodeHeaders(nalUnit);
                        if (pDecodeErrorReport && sts == UMC::UMC_ERR_INVALID_STREAM)
                            SetDecodeErrorTypes((NalUnitType)pMediaDataEx->values[i], pDecodeErrorReport);
                    }
                    break;

                    default:
                        break;
                    };

                    continue;
                }

                auto nut = static_cast<NalUnitType>(pMediaDataEx->values[pMediaDataEx->index]);
                switch (nut)
                {
                case NAL_UNIT_VPS:
                case NAL_UNIT_SPS:
                case NAL_UNIT_PPS:
                case NAL_UNIT_PH:
                case NAL_UNIT_PREFIX_APS:
                case NAL_UNIT_SUFFIX_APS:
                    {
                        UMC::Status sts = UMC::UMC_OK;
                        if (nut == NAL_UNIT_PH)
                        {
                            sts = AddSlice(0);
                        }
                        if (sts == UMC::UMC_OK || sts == UMC::UMC_ERR_NOT_ENOUGH_DATA)
                        {
                            sts = DecodeHeaders(nalUnit);
                        }
                        if (sts != UMC::UMC_OK)
                        {
                            if (sts == UMC::UMC_NTF_NEW_RESOLUTION ||
                                (nut == NAL_UNIT_SPS && sts == UMC::UMC_ERR_INVALID_STREAM))
                            {
                                int32_t nalIndex = pMediaDataEx->index;
                                int32_t size = pMediaDataEx->offsets[nalIndex + 1] - pMediaDataEx->offsets[nalIndex];

                                m_checkCRAInsideResetProcess = true;

                                if (AddSlice(0) == UMC::UMC_OK)
                                {
                                    pSource->MoveDataPointer(-size - 3);
                                    return UMC::UMC_OK;
                                }
                                moveToSpsOffset = pSource->GetDataSize() + size + 3;
                                continue;
                            }
                            if (pDecodeErrorReport && sts == UMC::UMC_ERR_INVALID_STREAM)
                            {
                                SetDecodeErrorTypes(nut, pDecodeErrorReport);
                            }
                            return sts;
                        }
                        if (nut == NAL_UNIT_PH)
                        {
                            if (sts == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || sts == UMC::UMC_OK)
                            {
                                return sts;
                            }
                        }
                    }
                    break;

                case NAL_UNIT_CODED_SLICE_TRAIL:
                case NAL_UNIT_CODED_SLICE_STSA:
                case NAL_UNIT_CODED_SLICE_RADL:
                case NAL_UNIT_CODED_SLICE_RASL:
                case NAL_UNIT_CODED_SLICE_IDR_W_RADL:
                case NAL_UNIT_CODED_SLICE_IDR_N_LP:
                case NAL_UNIT_CODED_SLICE_CRA:
                case NAL_UNIT_CODED_SLICE_GDR:
                    if (VVCSlice* pSlice = DecodeSliceHeader(nalUnit))
                    {
                        UMC::Status sts = AddSlice(pSlice);
                        if (sts == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || sts == UMC::UMC_OK)
                        {
                            return sts;
                        }
                    }
                    break;

                case NAL_UNIT_ACCESS_UNIT_DELIMITER:
                case NAL_UNIT_OPI:
                case NAL_UNIT_DCI:
                    if (AddSlice(0) == UMC::UMC_OK)
                    {
                        return UMC::UMC_OK;
                    }
                    break;

                case NAL_UNIT_EOS:
                case NAL_UNIT_EOB:
                    AddSlice(0);
                    m_RA_POC = 0;
                    m_IRAPType = NAL_UNIT_INVALID;
                    IncreaseRefPicListResetCount(m_dpb, 0);
                    //TODO: reset m_firstSliceInSequence w / layer id
                    m_noRaslOutputFlag = 0;
                    if (nut == NAL_UNIT_EOS)
                        std::fill_n(m_firstSliceInSequence, VVC_MAX_VPS_LAYERS, true);
                    return UMC::UMC_OK;
                    break;

                default:
                    break;
                }
            }
        } while ((pSource) && (MINIMAL_DATA_SIZE < pSource->GetDataSize()));

        if (pSource && m_checkCRAInsideResetProcess)
        {
            pSource->MoveDataPointer(int32_t(pSource->GetDataSize() - moveToSpsOffset));
            m_splitter->Reset();
        }

        if (!pSource)
        {
            return AddSlice(0);
        }
        else
        {
            uint32_t flags = pSource->GetFlags();

            if (!(flags & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME))
            {
                return AddSlice(0);
            }
        }

        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    UMC::Status VVCDecoder::AddSlice(VVCSlice *pSlice)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);
        m_lastSlice = 0;

        if (!pSlice) // complete frame
        {
            UMC::Status umcRes = UMC::UMC_ERR_NOT_ENOUGH_DATA;

            if (NULL == m_currFrame)
            {
                return umcRes;
            }
            OnFullFrame(m_currFrame);
            umcRes = CompletePicture(m_currFrame);
            if (umcRes != UMC::UMC_OK)
            {
                MFX_RETURN(umcRes);
            }

            m_currFrame = NULL;
            umcRes = UMC::UMC_OK;
            return umcRes;
        }

        VVCDecoderFrame *pFrame = m_currFrame;
        if (pFrame)
        {
            const VVCSlice *firstSlice = pFrame->GetAU()->GetSlice(0);
            assert(firstSlice);

            const VVCSeqParamSet* sps = m_currHeaders.m_seqParams.GetHeader(pSlice->GetSPS()->GetID());
            const VVCPicParamSet* pps = m_currHeaders.m_picParams.GetHeader(pSlice->GetPPS()->GetID());

            if (!sps || !pps) // undefined behavior
            {
                return UMC::UMC_ERR_FAILED;
            }

            // TODO: Add CopyFromBaseSlice here
            bool changed =
                sps->m_changed ||
                pps->m_changed ||
                !SlicesInSamePic(firstSlice, pSlice);
            if (changed)
            {
                OnFullFrame(m_currFrame);
                CompletePicture(m_currFrame);
                m_currFrame = NULL;
                m_lastSlice.reset(pSlice);
                return UMC::UMC_OK;
            }
        }
        else
        {
            VVCSeqParamSet* sps = m_currHeaders.m_seqParams.GetHeader(pSlice->GetSPS()->GetID());
            VVCPicParamSet* pps = m_currHeaders.m_picParams.GetHeader(pSlice->GetPPS()->GetID());

            if (!sps || !pps) // undefined behavior
            {
                return UMC::UMC_ERR_FAILED;
            }

            // clear change flags when get first VCL NAL
            sps->m_changed = false;
            pps->m_changed = false;

            // allocate a new frame, initialize it with slice's parameters.
            pFrame = GetFrameBuffer(pSlice);
            if (!pFrame)
            {
                m_currFrame = NULL;
                m_lastSlice.reset(pSlice);
                return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
            }

            // set the current being processed frame
            m_currFrame = pFrame;
        }

        // add the next slice to the initialized frame.
        pSlice->SetCurrentFrame(pFrame);

        // Include a new slice into a set of frame slices
        if (pFrame->m_FrameType < SliceTypeToFrameType(pSlice->GetSliceHeader()->slice_type))
        {
            pFrame->m_FrameType = SliceTypeToFrameType(pSlice->GetSliceHeader()->slice_type);
        }

        // countActiveRefs
        if (pSlice->m_sliceHeader.slice_type != I_SLICE)
        {
            uint32_t NumShortTermRefs = 0, NumLongTermRefs = 0;
            countActiveRefs(NumShortTermRefs, NumLongTermRefs);
        }

        // AddSelfReferenceFrame

        // UpdateReferenceList
        pSlice->ConstructReferenceList(m_dpb/*, pFrame*/);

        if (pSlice->m_sliceHeader.slice_type != I_SLICE)
        {
            pFrame->setAsInter(true);
            bool bLowDelay = true;

            for (uint32_t i = 0; i < pSlice->m_sliceHeader.sh_rpl[REF_PIC_LIST_0].num_ref_entries && bLowDelay; i++)
            {
                if (pSlice->m_sliceHeader.sh_rpl[REF_PIC_LIST_0].POC[i] > pSlice->m_sliceHeader.m_poc)
                    bLowDelay = false;
            }

            if (pSlice->m_sliceHeader.slice_type == B_SLICE)
            {
                for (uint32_t i = 0; i < pSlice->m_sliceHeader.sh_rpl[REF_PIC_LIST_1].num_ref_entries && bLowDelay; i++)
                {
                    if (pSlice->m_sliceHeader.sh_rpl[REF_PIC_LIST_1].POC[i] > pSlice->m_sliceHeader.m_poc)
                        bLowDelay = false;
                }
            }

            pSlice->m_sliceHeader.NoBackwardPredFlag = bLowDelay;
        }
        else
        {
            pFrame->setAsInter(false);
        }
        pFrame->AddSlice(pSlice);

        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    UMC::Status VVCDecoder::DecodeHeaders(UMC::MediaDataEx *nalUnit)
    {
        UMC::Status sts = UMC::UMC_OK;

        VVCHeadersBitstream bitStream;

        try
        {
            MemoryPiece mem;
            mem.SetData(nalUnit);

            MemoryPiece swappedMem;

            swappedMem.Allocate(nalUnit->GetDataSize() + DEFAULT_UNIT_TAIL_SIZE);

            m_splitter->SwapMemory(&swappedMem, &mem, 0);
            bitStream.Reset((uint8_t*)swappedMem.GetPointer(), (uint32_t)swappedMem.GetDataSize());

            NalUnitType nal_unit_type;
            uint32_t temporal_id = 0;
            uint32_t nuh_layer_id = 0;

            bitStream.GetNALUnitType(nal_unit_type, temporal_id, nuh_layer_id);

            switch (nal_unit_type)
            {
            case NAL_UNIT_VPS:
                sts = xDecodeVPS(&bitStream);
                break;
            case NAL_UNIT_SPS:
                sts = xDecodeSPS(&bitStream);
                break;
            case NAL_UNIT_PPS:
                sts = xDecodePPS(&bitStream);
                break;
            case NAL_UNIT_PREFIX_APS:
            case NAL_UNIT_SUFFIX_APS:
                sts = xDecodeAPS(&bitStream);
                break;
            case NAL_UNIT_PH:
                if (sts != UMC::UMC_OK &&
                    sts != UMC::UMC_ERR_NOT_ENOUGH_DATA)
                {
                    return sts;
                }
                sts = xDecodePH(&bitStream);
                break;
            case NAL_UNIT_OPI:
                sts = xDecodeOPI(&bitStream);
                break;
            default:
                break;
            }
        }
        catch (const vvc_exception& ex)
        {
            return ex.GetStatus();
        }
        catch (...)
        {
            return UMC::UMC_ERR_INVALID_STREAM;
        }

        UMC::MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();
        if ((NalUnitType)pMediaDataEx->values[0] == NAL_UNIT_SPS && m_firstMfxVideoParams.mfx.FrameInfo.Width)
        {
            VVCSeqParamSet* currSPS = m_currHeaders.m_seqParams.GetCurrentHeader();

            if (currSPS)
            {
                if (m_firstMfxVideoParams.mfx.FrameInfo.Width < (currSPS->sps_pic_width_max_in_luma_samples) ||
                    m_firstMfxVideoParams.mfx.FrameInfo.Height < (currSPS->sps_pic_height_max_in_luma_samples) ||
                    (m_firstMfxVideoParams.mfx.FrameInfo.BitDepthChroma != (currSPS->sps_bitdepth_minus8 + 8)) ||
                    (m_firstMfxVideoParams.mfx.FrameInfo.BitDepthLuma != (currSPS->sps_bitdepth_minus8 + 8)))
                {
                    return UMC::UMC_NTF_NEW_RESOLUTION;
                }
            }

            return UMC::UMC_WRN_REPOSITION_INPROGRESS;
        }

        return sts;
    }

    UMC::Status VVCDecoder::xDecodeVPS(VVCHeadersBitstream *bs)
    {
        VVCVideoParamSet vps = {};

        UMC::Status sts = bs->GetVideoParamSet(&vps);
        if (sts != UMC::UMC_OK)
            return sts;

        if (GetTOlsIdxExternalFlag())
        {
            vps.vps_target_ols_idx = 0; // Current no external set OlsIdx, later to add if needed
        }
        else if (GetTOlsIdxOpiFlag())
        {
            VVCOPI *opi = m_currHeaders.m_opiParams.GetCurrentHeader();
            vps.vps_target_ols_idx = opi->opi_ols_idx;
        }

        m_currHeaders.m_videoParams.AddHeader(&vps);

        return UMC::UMC_OK;
    }

    UMC::Status VVCDecoder::xDecodeSPS(VVCHeadersBitstream *bs)
    {
        VVCSeqParamSet sps = {};
        sps.m_changed = false;

        UMC::Status sts = bs->GetSequenceParamSet(&sps);
        if (sts != UMC::UMC_OK)
        {
            return sts;
        }

        const VVCSeqParamSet* old_sps = m_currHeaders.m_seqParams.GetCurrentHeader();
        bool newResolution = false;
        if (IsNeedSPSInvalidate(old_sps, &sps))
        {
            newResolution = true;
        }

        m_currHeaders.m_seqParams.AddHeader(&sps);

        m_splitter->SetSuggestedSize(CalculateSuggestedSize(&sps));

        if (newResolution)
        {
            return UMC::UMC_NTF_NEW_RESOLUTION;
        }

        return UMC::UMC_OK;
    }

    UMC::Status VVCDecoder::xDecodePPS(VVCHeadersBitstream *bs)
    {
        VVCPicParamSet pps = {};
        pps.pps_num_slices_in_pic = 1;
        pps.m_changed = false;
        
        UMC::Status sts = bs->GetPictureParamSet(&pps);
        if (sts != UMC::UMC_OK)
        {
            return sts;
        }

        if (m_currHeaders.m_picParams.GetHeader(pps.GetID()))
        {
            VVCPicParamSet* pPicParamSet = m_currHeaders.m_picParams.GetHeader(pps.GetID());
            for (uint32_t i = 0; i < pPicParamSet->pps_rect_slices.size(); i++)
            {
                delete pPicParamSet->pps_rect_slices[i];
            }
            pPicParamSet->pps_rect_slices.clear();
            for (uint32_t j = 0; j < pPicParamSet->pps_slice_map.size(); j++)
            {
                delete pPicParamSet->pps_slice_map[j];
            }
            pPicParamSet->pps_slice_map.clear();
            for (uint32_t k = 0; k < pPicParamSet->pps_sub_pics.size(); k++)
            {
                delete pPicParamSet->pps_sub_pics[k];
            }
            pPicParamSet->pps_sub_pics.clear();
        }
        m_currHeaders.m_picParams.AddHeader(&pps);

        return UMC::UMC_OK;
    }

    UMC::Status VVCDecoder::xDecodePH(VVCHeadersBitstream *bs)
    {
        VVCPicHeader ph = {};
        ph.ph_collocated_from_l0_flag = true;
        ph.ph_temporal_mvp_enabled_flag = true;
        UMC::Status sts = bs->GetPictureHeader(&ph, &m_currHeaders, true);
        if (sts != UMC::UMC_OK)
        {
            return sts;
        }

        m_currHeaders.m_phParams = ph;

        return UMC::UMC_OK;
    }

    UMC::Status VVCDecoder::xDecodeAPS(VVCHeadersBitstream* bs)
    {
        VVCAPS aps = {};

        UMC::Status sts = bs->GetAdaptionParamSet(&aps);
        if (sts != UMC::UMC_OK)
        {
            return sts;
        }

        m_currHeaders.m_adaptionParams.AddHeader(&aps);

        return UMC::UMC_OK;
    }

    UMC::Status VVCDecoder::xDecodeOPI(VVCHeadersBitstream *bs)
    {
        VVCOPI opi = {};

        UMC::Status sts = bs->GetOperatingPointInformation(&opi);
        if (sts != UMC::UMC_OK)
        {
            return sts;
        }

        m_currHeaders.m_opiParams.AddHeader(&opi);

        return UMC::UMC_OK;
    }

    UMC::Status VVCDecoder::CompleteDecodedFrames(VVCDecoderFrame** decoded)
    {
        VVCDecoderFrame* completed = 0;
        UMC::Status sts = UMC::UMC_OK;

        for (;;)
        {
            bool isOneToAdd = true;
            VVCDecoderFrame* frameToAdd = 0;

            for (DPBType::iterator it = m_dpb.begin(); it != m_dpb.end(); it++)
            {
                VVCDecoderFrame *frame = *it;
                int32_t const frm_error = frame->GetError();

                if (sts == UMC::UMC_OK && frm_error < 0)
                {
                    sts = static_cast<UMC::Status>(frm_error);
                }

                if (!frame->IsDecoded())
                {
                    if (!frame->IsDecodingStarted() && frame->IsFullFrame())
                    {
                        if (frameToAdd)
                        {
                            isOneToAdd = false;
                            if (frameToAdd->m_UID < frame->m_UID) // add first with min UID
                                continue;
                        }

                        frameToAdd = frame;
                    }

                    if (!frame->IsDecodingCompleted())
                    {
                        continue;
                    }

                    frame->OnDecodingCompleted();
                    completed = frame;
                }
            }

            if (sts != UMC::UMC_OK)
                break;

            if (frameToAdd)
            {
                if (!AddFrameToDecoding(frameToAdd))
                    break;
            }

            if (isOneToAdd)
                break;
        }

        if (decoded)
        {
            *decoded = completed;
        }

        return sts;
    }

    // Initialize just allocated frame with slice parameters
    UMC::Status VVCDecoder::InitFreeFrame(VVCDecoderFrame* pFrame, VVCSlice* pSlice)
    {
        UMC::Status umcRes = UMC::UMC_OK;
        const VVCSeqParamSet* pSeqParam = pSlice->GetSPS();
        const VVCPicParamSet* pPicParam = pSlice->GetPPS();

        pFrame->m_FrameType = SliceTypeToFrameType(pSlice->GetSliceHeader()->slice_type);
        pFrame->m_dFrameTime = pSlice->m_source.GetTime();
        pFrame->m_cropLeft = pSeqParam->sps_conf_win_left_offset;
        pFrame->m_cropRight = pSeqParam->sps_conf_win_right_offset;
        pFrame->m_cropTop = pSeqParam->sps_conf_win_top_offset;
        pFrame->m_cropBottom = pSeqParam->sps_conf_win_bottom_offset;
        pFrame->m_cropFlag = pSeqParam->sps_conformance_window_flag;

        pFrame->m_aspectWidth = pSeqParam->vui.vui_sar_width;
        pFrame->m_aspectHeight = pSeqParam->vui.vui_sar_height;

        int32_t chroma_format_idc = pSeqParam->sps_chroma_format_idc;
        int32_t bit_depth = (uint8_t)pSeqParam->sps_bitdepth_minus8 + 8;
        mfxSize dimensions = { static_cast<int>(pPicParam->pps_pic_width_in_luma_samples), static_cast<int>(pPicParam->pps_pic_height_in_luma_samples) };

        UMC::ColorFormat cf;
        switch (chroma_format_idc) 
        {
        case CHROMA_FORMAT_400:
            cf = UMC::GRAY;
            break;
        case CHROMA_FORMAT_422:
            cf = UMC::YUV422;
            break;
        case CHROMA_FORMAT_444:
            cf = UMC::YUV444;
            break;
        case CHROMA_FORMAT_420:
        default:
            cf = UMC::YUV420;
            break;
        }

        if (cf == UMC::YUV420)
            cf = UMC::NV12;

        UMC::VideoDataInfo info;
        info.Init(dimensions.width, dimensions.height, cf, bit_depth);

        pFrame->Init(&info);
        int unitX = 1;
        int unitY = 1;
        umcRes = UMC_VVC_DECODER::MFX_Utility::getWinUnit(pSeqParam->sps_chroma_format_idc, unitX, unitY);
        if (umcRes != UMC::UMC_OK)
            return UMC::UMC_ERR_FAILED;
        if (pPicParam->pps_conformance_window_flag)
        {
            pFrame->m_cropLeft = pPicParam->pps_conf_win_left_offset * unitX;
            pFrame->m_cropTop = pPicParam->pps_conf_win_top_offset * unitY;
            pFrame->m_cropRight = pPicParam->pps_conf_win_right_offset * unitX;
            pFrame->m_cropBottom = pPicParam->pps_conf_win_bottom_offset * unitY;
        }
        else if ((pPicParam->pps_pic_width_in_luma_samples == pSeqParam->sps_pic_width_max_in_luma_samples)
            && (pPicParam->pps_pic_height_in_luma_samples == pSeqParam->sps_pic_height_max_in_luma_samples))
        {
            pFrame->m_cropLeft = pSeqParam->sps_conf_win_left_offset * unitX;
            pFrame->m_cropTop = pSeqParam->sps_conf_win_top_offset * unitY;
            pFrame->m_cropRight = pSeqParam->sps_conf_win_right_offset * unitX;
            pFrame->m_cropBottom = pSeqParam->sps_conf_win_bottom_offset * unitY;
        }
        else
        {
            pFrame->m_cropLeft = 0;
            pFrame->m_cropTop = 0;
            pFrame->m_cropRight = 0;
            pFrame->m_cropBottom = 0;
        }

        return umcRes;
    }

    VVCDecoderFrame* VVCDecoder::GetFrameBuffer(VVCSlice *pSlice)
    {
        if (!pSlice)
        {
            return NULL;
        }

        SetDPBSize(pSlice->GetSPS());

        DPBUpdate(pSlice);
        auto frame = GetFreeFrame();
        if (!frame)
            return nullptr;

        pSlice->checkCRA(m_dpb, pSlice->m_pocCRA[frame->layerId]);

        frame->m_pic_output = (pSlice->GetPictureHeader()->ph_pic_output_flag != 0);
        frame->SetisShortTermRef(true);
        frame->setAsReferenced(true);

        UMC::Status umcRes = InitFreeFrame(frame, pSlice);
        if (umcRes != UMC::UMC_OK)
        {
            return 0;
        }

        UMC::VideoDataInfo info;
        auto sts = info.Init(frame->m_lumaSize.width, frame->m_lumaSize.height, m_params.info.color_format, 0);
        if (sts != UMC::UMC_OK)
            throw vvc_exception(sts);

        UMC::FrameMemID id;
        sts = m_allocator->Alloc(&id, &info, 0);
        if (sts != UMC::UMC_OK)
            throw vvc_exception(UMC::UMC_ERR_ALLOC);

        AllocateFrameData(&info, id, frame);

        if (frame->m_index < 0)
        {
            return NULL;
        }

        InitFrameCounter(frame, pSlice);
        return frame;
    }


    void VVCDecoder::IncreaseRefPicListResetCount(DPBType dpb, VVCDecoderFrame* ExcludeFrame) const
    {
        VVCDecoderFrame* pCurr = nullptr;
        for (DPBType::iterator it = dpb.begin(); it != dpb.end(); it++)
        {
            pCurr = *it;
            if (pCurr != ExcludeFrame)
            {
                pCurr->IncreaseRefPicListResetCount();
            }
        }
    }

    // Initialize frame's counter and corresponding params
    void VVCDecoder::InitFrameCounter(VVCDecoderFrame* pFrame, const VVCSlice* pSlice)
    {
        const VVCSliceHeader* sh = pSlice->GetSliceHeader();

        if (sh->IdrPicFlag)
        {
            IncreaseRefPicListResetCount(m_dpb, pFrame);
        }

        pFrame->setPicOrderCnt(sh->m_poc);
        pFrame->layerId = sh->nuh_layer_id;

        pFrame->InitRefPicListResetCount();
    }

    bool VVCDecoder::IsNewPicture()
    {
        m_currFrame->SetFullFrame(true);
        CompletePicture(m_currFrame);

        m_currFrame = nullptr;

        return true; // Full frame
    }

    void VVCDecoder::EliminateSliceErrors(VVCDecoderFrame *pFrame)
    {
        VVCDecoderFrameInfo & frameInfo = *pFrame->GetAU();
        size_t sliceCount = frameInfo.GetSliceCount();

        for (size_t sliceNum = 0; sliceNum < sliceCount; sliceNum++)
        {
            auto slice = frameInfo.GetSlice(sliceNum);
            auto sliceHeader = slice->GetSliceHeader();
            (void)sliceHeader;

            // TODO, add logic here
        }
        return;
    }

    bool VVCDecoder::IsSkipForCRA(const VVCSlice *pSlice)
    {
        if (pSlice && m_noRaslOutputFlag)
        {
            if (pSlice->m_sliceHeader.m_poc == m_RA_POC)
                return false;

            if (pSlice->m_sliceHeader.m_poc < m_RA_POC &&
                (pSlice->m_sliceHeader.nal_unit_type == NAL_UNIT_CODED_SLICE_RADL || pSlice->m_sliceHeader.nal_unit_type == NAL_UNIT_CODED_SLICE_RASL))
            {
                return true;
            }
            // ...
        }

        return false;
    }

    // Add frame to decoding queue
    bool VVCDecoder::AddFrameToDecoding(VVCDecoderFrame* frame)
    {
        VVCDecoderFrameInfo* frameInfo = frame->GetAU();
        bool IsExistTasks = frameInfo->IsFilled();

        if (!frame || frame->IsDecodingStarted() || !IsExistTasks)
            return false;

        std::unique_lock<std::mutex> l(m_guard);

#ifdef VM_DEBUG
        FrameQueue::iterator iter = m_decodingQueue.begin();
        FrameQueue::iterator end_iter = m_decodingQueue.end();

        for (; iter != end_iter; ++iter)
        {
            if ((*iter) == frame)
            {
                assert(false);
            }
        }
#endif

        m_decodingQueue.push_back(frame);
        frame->StartDecoding();
        return true;
    }

    // Mark frame as full with slices
    void VVCDecoder::OnFullFrame(VVCDecoderFrame* pFrame)
    {
        pFrame->SetFullFrame(true);

        if (!pFrame->GetAU()->GetSlice(0)) // seems that it was skipped and slices was dropped
            return;

        VVCPicHeader* picHdr    = pFrame->GetAU()->GetSlice(0)->GetPictureHeader();
        VVCSliceHeader* slcHdr  = pFrame->GetAU()->GetSlice(0)->GetSliceHeader();
        NalUnitType nut  = slcHdr->nal_unit_type;
        uint32_t layerId = slcHdr->nuh_layer_id;
        int32_t poc      = slcHdr->m_poc;
        if ( m_prevPicSkipped && nut == NAL_UNIT_CODED_SLICE_GDR )
        {
            m_gdrRecoveryPeriod[layerId] = true;
        }
        if( nut != NAL_UNIT_CODED_SLICE_RASL)
        {
            m_prevPicSkipped = false;
        }
        if (picHdr->ph_gdr_pic_flag && m_prevGDRInSameLayerPOC[layerId] == -VVC_MAX_INT ) // Only care about recovery POC if it is the first coded GDR picture in the layer
        {
            m_prevGDRInSameLayerRecoveryPOC[layerId] = poc + picHdr->ph_recovery_poc_cnt;
        }
        if (nut == NAL_UNIT_CODED_SLICE_GDR && !pFrame->GetAU()->GetSlice(0)->GetPPS()->pps_mixed_nalu_types_in_pic_flag)
        {
            m_prevGDRInSameLayerPOC[layerId] = poc;
        }

        if (m_gdrRecoveryPeriod[layerId] && (poc < m_prevGDRInSameLayerRecoveryPOC[layerId]))
        {
            pFrame->SetisDisplayable(false);
        }
        else
        {
            m_gdrRecoveryPeriod[layerId] = false;
            pFrame->SetisDisplayable(picHdr->ph_pic_output_flag != 0);
            pFrame->m_pic_output = (picHdr->ph_pic_output_flag != 0) && pFrame->IsDisplayable();
        }

        if (slcHdr->IdrPicFlag && !(pFrame->GetError() & UMC::ERROR_FRAME_DPB))
        {
            pFrame->m_isDPBErrorFound = 0;
        }
        pFrame->DecrementReference();
    }

    UMC::Status VVCDecoder::CompletePicture(VVCDecoderFrame *pFrame)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, __FUNCTION__);
        UMC::Status sts = UMC::UMC_OK;

        VVCDecoderFrameInfo *pFrameInfo = pFrame->GetAU();
        pFrameInfo->SetFilled();

        const VVCSlice *slice = pFrameInfo->GetSlice(0);

        // skipping algorithm
        if (!slice && IsShouldSkipFrame(pFrame) && IsSkipForCRA(slice))
        {
            pFrame->SetAsRef(false);
            pFrame->SetSkipped(true);
            pFrame->OnDecodingCompleted();
            pFrame->m_pic_output = false;
            return UMC::UMC_OK;
        }

        size_t sliceCount = pFrameInfo->GetSliceCount();

        if (sliceCount)
        {
            EliminateSliceErrors(pFrame);
            sts = Submit(pFrame);
        }
        else
        {
            pFrame->OnDecodingCompleted();
        }
        
        pFrameInfo->SetStatus(VVCDecoderFrameInfo::STATUS_FILLED);

        return sts;
    }

    VVCDecoderFrame* VVCDecoder::FindOldestDisplayable(DPBType dpb)
    {
        VVCDecoderFrame* pCurr = nullptr;
        VVCDecoderFrame* pOldest = NULL;
        int32_t  SmallestPicOrderCnt = 0x7fffffff;    // very large positive
        int32_t  LargestRefPicListResetCount = 0;
        int32_t  uid = 0x7fffffff;

        int32_t count = 0;
        for (DPBType::iterator it = m_dpb.begin(); it != m_dpb.end(); it++)
        {
            pCurr = *it;
            if (pCurr->IsDisplayable() && !pCurr->IsOutputted())
            {
                // corresponding frame
                if (pCurr->RefPicListResetCount() > LargestRefPicListResetCount)
                {
                    pOldest = pCurr;
                    SmallestPicOrderCnt = pCurr->PicOrderCnt();
                    LargestRefPicListResetCount = pCurr->RefPicListResetCount();
                }
                else if (pCurr->PicOrderCnt() <= SmallestPicOrderCnt && pCurr->RefPicListResetCount() == LargestRefPicListResetCount)
                {
                    pOldest = pCurr;
                    SmallestPicOrderCnt = pCurr->PicOrderCnt();
                }
                count++;
            }
        }

        if (!pOldest)
            return 0;

        for (DPBType::iterator it = m_dpb.begin(); it != m_dpb.end(); it++)
        {
            pCurr = *it;
            if (pCurr->IsDisplayable() && !pCurr->IsOutputted())
            {
                // corresponding frame
                if (pCurr->RefPicListResetCount() == LargestRefPicListResetCount && pCurr->PicOrderCnt() == SmallestPicOrderCnt && pCurr->m_UID < uid)
                {
                    pOldest = pCurr;
                    SmallestPicOrderCnt = pCurr->PicOrderCnt();
                    LargestRefPicListResetCount = pCurr->RefPicListResetCount();
                    uid = pCurr->m_UID;
                }
            }
        }

        return pOldest;
    }    // findOldestDisplayable

    void VVCDecoder::CalculateInfoForDisplay(DPBType, uint32_t& countDisplayable, uint32_t& countDPBFullness, int32_t& maxUID) 
    {
        VVCDecoderFrame* pCurr = nullptr;

        countDisplayable = 0;
        countDPBFullness = 0;
        maxUID = 0;

        int resetCounter = -1;

        for (DPBType::iterator it = m_dpb.begin(); it != m_dpb.end(); it++)
        {
            pCurr = *it;
            if (pCurr->IsDisplayable() && !pCurr->IsOutputted())
            {
                countDisplayable++;
                if (resetCounter == -1)
                    resetCounter = pCurr->RefPicListResetCount();
                else
                {
                    if (resetCounter != pCurr->RefPicListResetCount()) // DPB contain new IDR and frames from prev sequence
                        countDisplayable += 16;
                }
            }

            if (((pCurr->isShortTermRef() || pCurr->isLongTermRef()) && pCurr->IsFullFrame()) || (pCurr->IsDisplayable() && !pCurr->IsOutputted()))
            {
                countDPBFullness++;
                if (maxUID < pCurr->m_UID)
                    maxUID = pCurr->m_UID;
            }
        }
    }    // calculateInfoForDisplay

    VVCDecoderFrame* VVCDecoder::GetFrameToDisplay(bool force)
    {
        std::unique_lock<std::mutex> l(m_guard);

        if (m_decOrder)
        {
            VVCDecoderFrame* pTmp = FindOldestDisplayable(m_dpb);
            if (pTmp)
                pTmp->m_decFlush = true;
            return pTmp;
        }

        const VVCSeqParamSet * sps = m_currHeaders.m_seqParams.GetCurrentHeader();
        uint32_t sps_max_num_reorder_pics = sps ? sps->dpb_parameter.dpb_max_num_reorder_pics[sps->sps_max_sublayers_minus1] : m_sps_max_num_reorder_pics;
        uint32_t sps_max_dec_pic_buffering = sps ? std::max(sps->dpb_parameter.dpb_max_dec_pic_buffering_minus1[sps->sps_max_sublayers_minus1] + 1, (uint32_t)m_dpb.size()) : m_sps_max_dec_pic_buffering;

        for (;;)
        {
            // show oldest frame

            uint32_t countDisplayable = 0;
            int32_t maxUID = 0;
            uint32_t countDPBFullness = 0;

            CalculateInfoForDisplay(m_dpb, countDisplayable, countDPBFullness, maxUID);

            if (countDisplayable > sps_max_num_reorder_pics || countDPBFullness > sps_max_dec_pic_buffering || force)
            {
                VVCDecoderFrame* pTmp = FindOldestDisplayable(m_dpb);

                if (pTmp)
                {
                    if (!force && countDisplayable <= sps_max_num_reorder_pics && countDPBFullness <= sps_max_dec_pic_buffering)
                        return 0;
                    if (!pTmp->m_pic_output)
                    {
                        pTmp->SetDisplayed();
                        pTmp->SetOutputted();
                        continue;
                    }

                    pTmp->m_maxUIDWhenDisplayed = maxUID;
                    pTmp->m_decFlush = true;
                    return pTmp;
                }
            }
            break;
        }
        
        return 0;
    }

    void VVCDecoder::PostProcessDisplayFrame(VVCDecoderFrame *frame)
    {
        if (!frame || frame->m_isPostProccesComplete)
            return;

        frame->m_isOriginalPTS = frame->m_dFrameTime > -1.0;
        if (frame->m_isOriginalPTS)
        {
            m_localFrameTime = frame->m_dFrameTime;
        }
        else
        {
            frame->m_dFrameTime = m_localFrameTime;
        }

        frame->m_frameOrder = m_frameOrder;
        switch (frame->m_displayPictureStruct)
        {
        case DPS_TOP_BOTTOM_TOP:
        case DPS_BOTTOM_TOP_BOTTOM:
            if (m_params.lFlags & UMC::FLAG_VDEC_TELECINE_PTS)
            {
                m_localFrameTime += (m_localDeltaFrameTime / 2);
            }
            break;
        default:
            break;
        }

        m_localFrameTime += m_localDeltaFrameTime;

        m_frameOrder++;

        frame->m_isPostProccesComplete = true;

        m_maxUIDWhenDisplayed = std::max(m_maxUIDWhenDisplayed, frame->m_maxUIDWhenDisplayed);
    }

    template <typename F>
    VVCDecoderFrame *FindFrame(F pred, const DPBType &dpb)
    {
        auto i = std::find_if(std::begin(dpb), std::end(dpb), pred);
        return i != std::end(dpb) ? (*i) : nullptr;
    }

    VVCDecoderFrame *VVCDecoder::FindFrameByMemID(UMC::FrameMemID id)
    {
        std::unique_lock<std::mutex> l(m_guard);
        return FindFrame(
            [id](VVCDecoderFrame const* f)
            { return f->GetMemID() == id; }, m_dpb
        );
    }

    inline VVCDecoderFrame *FindFreeFrame(const DPBType &dpb)
    {
        auto i = std::find_if(std::begin(dpb), std::end(dpb),
                [](VVCDecoderFrame const* frame)
                { return frame->Empty(); }
            );
        return (i != std::end(dpb)) ? *i : nullptr;
    }

    VVCDecoderFrame* VVCDecoder::GetOldestDisposable()
    {
        VVCDecoderFrame* pOldest = NULL;
        int32_t  SmallestPicOrderCnt = 0x7fffffff;    // very large positive
        int32_t  LargestRefPicListResetCount = 0;

        VVCDecoderFrame* pTmp;
        for (DPBType::iterator it = m_dpb.begin(); it != m_dpb.end(); it++)
        {
            pTmp = *it;
            if (pTmp->isDisposable())
            {
                if (pTmp->RefPicListResetCount() > LargestRefPicListResetCount)
                {
                    pOldest = pTmp;
                    SmallestPicOrderCnt = pTmp->PicOrderCnt();
                    LargestRefPicListResetCount = pTmp->RefPicListResetCount();
                }
                else if ((pTmp->PicOrderCnt() < SmallestPicOrderCnt) &&
                    (pTmp->RefPicListResetCount() == LargestRefPicListResetCount))
                {
                    pOldest = pTmp;
                    SmallestPicOrderCnt = pTmp->PicOrderCnt();
                }
            }
        }
        return pOldest;
    }

    VVCDecoderFrame *VVCDecoder::GetFreeFrame()
    {
        std::unique_lock<std::mutex> l(m_guard);

        VVCDecoderFrame* frame = GetOldestDisposable();

        // If nothing found
        if (!frame)
        {
            // Can not allocate more
            if (m_dpb.size() >= m_dpbSize)
            {
                return nullptr;
            }

            // Didn't find any. Let's create a new one
            frame = new VVCDecoderFrame;

            // Add to DPB
            m_dpb.push_back(frame);
        }
        frame->Reset();

        // Set current as not displayable (yet) and not outputted. Will be
        // updated to displayable after successful decode.
        frame->IncrementReference();

        m_counter++;
        frame->m_UID = m_counter;

        return frame;
    }

    uint8_t VVCDecoder::GetNumCachedFrames() const
    {
        return (uint8_t)std::count_if(m_dpb.begin(), m_dpb.end(),
                [](VVCDecoderFrame const * f) { return (f->IsDecodingStarted() && !f->IsOutputted()); }
        );
    }

/****************************************************************************************/
// SEI_Storer_VVC class implementation
/****************************************************************************************/

    SEI_Storer_VVC::SEI_Storer_VVC()
    {
        m_offset = 0;
        Reset();
    }

    SEI_Storer_VVC::~SEI_Storer_VVC()
    {
        Close();
    }

    void SEI_Storer_VVC::Init()
    {
        Close();
        // ...
        m_offset = 0;
        m_lastUsed = 2;
    }

    void SEI_Storer_VVC::Close()
    {
        Reset();
        m_data.clear();
        m_payloads.clear();
    }

    void SEI_Storer_VVC::Reset()
    {
        m_lastUsed = 2;
        std::for_each(m_payloads.begin(), m_payloads.end(), [](SEI_Message& m) { m.isUsed = 0; } );
    }

/****************************************************************************************************/
// Skipping_VVC class implementation
/****************************************************************************************************/
    void Skipping_VVC::Reset()
    {
    }

    // Check if frame should be skipped to decrease decoding delays
    bool Skipping_VVC::IsShouldSkipFrame(VVCDecoderFrame *pFrame)
    {
        bool isShouldSkip = false;
        (void)pFrame;

        return isShouldSkip;
    }

    // Set decoding skip frame mode
    void Skipping_VVC::ChangeVideoDecodingSpeed(int32_t & val)
    {
        (void)val;
    }
}

#endif //MFX_ENABLE_VVC_VIDEO_DECODE
