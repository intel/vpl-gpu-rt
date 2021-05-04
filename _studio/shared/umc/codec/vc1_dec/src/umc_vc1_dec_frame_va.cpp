// Copyright (c) 2004-2019 Intel Corporation
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

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)

#include "umc_va_base.h"
#include "umc_vc1_dec_frame_descr_va.h"
#include "umc_vc1_dec_frame_descr.h"
#include "umc_vc1_dec_task_store.h"
#include "umc_vc1_common.h"
using namespace UMC::VC1Common;

#define DXVA2_VC1PICTURE_PARAMS_EXT_BUFFER 21
#define DXVA2_VC1BITPLANE_EXT_BUFFER       22

namespace UMC
{
    using namespace VC1Exceptions;

#ifdef UMC_VA_DXVA
    void VC1PackerDXVA::VC1SetSliceBuffer()
    {
        UMCVACompBuffer* CompBuf;
        m_pSliceInfo = (DXVA_SliceInfo*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER,&CompBuf);
        if (CompBuf->GetBufferSize() < sizeof(DXVA_SliceInfo))
            throw vc1_exception(mem_allocation_er);
    }
    void VC1PackerDXVA::VC1SetPictureBuffer()
    {
        UMCVACompBuffer* CompBuf;
        m_pPicPtr = (DXVA_PictureParameters*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &CompBuf,sizeof(DXVA_PictureParameters));
        if (CompBuf->GetBufferSize() < sizeof(DXVA_PictureParameters))
            throw vc1_exception(mem_allocation_er);
    }
    //to support multislice mode. Save number of slices value in reserved bits of first slice
    void VC1PackerDXVA::VC1SetBuffersSize(uint32_t SliceBufIndex)
    {
        //_MAY_BE_
        UMCVACompBuffer* CompBuf;
        m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER,&CompBuf);
        CompBuf->SetDataSize((int32_t)(sizeof(DXVA_SliceInfo)*SliceBufIndex));
        CompBuf->SetNumOfItem(SliceBufIndex);
        m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER,&CompBuf);
        CompBuf->SetDataSize(sizeof(DXVA_PictureParameters));
    }

    void VC1PackerDXVA::VC1PackOneSlice  (VC1Context* pContext,
        SliceParams* slparams,
        uint32_t BufIndex, // only in future realisations
        uint32_t MBOffset,
        uint32_t SliceDataSize,
        uint32_t StartCodeOffset,
        uint32_t ChoppingType)
    {
        // we should use next buffer for next slice in case of "Whole" mode
        uint32_t mbShift = 0;
        if (BufIndex)
            ++m_pSliceInfo;
        if (pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
            mbShift = 32; //SC shift
        m_pSliceInfo->wHorizontalPosition = 0;
        m_pSliceInfo->wVerticalPosition = slparams->MBStartRow;
        m_pSliceInfo->dwSliceBitsInBuffer = SliceDataSize*8;
        m_pSliceInfo->wMBbitOffset = (WORD)((31-(BYTE)slparams->m_bitOffset) + MBOffset*8 + mbShift); //32 - Start Code
        m_pSliceInfo->dwSliceDataLocation = StartCodeOffset;
        m_pSliceInfo->bStartCodeBitOffset = 0;
        m_pSliceInfo->bReservedBits = 0;
        m_pSliceInfo->wQuantizerScaleCode = 0;
        m_pSliceInfo->wNumberMBsInSlice = slparams->MBStartRow;

        m_pSliceInfo->wBadSliceChopping = (uint8_t)ChoppingType;
    }
    void VC1PackerDXVA::VC1PackWholeSliceSM (VC1Context* pContext,
        uint32_t MBOffset,
        uint32_t SliceDataSize)
    {
        m_pSliceInfo->wHorizontalPosition = 0;
        m_pSliceInfo->wVerticalPosition = 0;
        m_pSliceInfo->dwSliceBitsInBuffer = SliceDataSize*8;
        m_pSliceInfo->wMBbitOffset = (WORD)((31-(BYTE)pContext->m_bitstream.bitOffset) + MBOffset*8);
        m_pSliceInfo->dwSliceDataLocation = 0;
        m_pSliceInfo->bStartCodeBitOffset = 0;
        m_pSliceInfo->bReservedBits = 0;
        m_pSliceInfo->wQuantizerScaleCode = 0;
        m_pSliceInfo->wBadSliceChopping = 0;
    }

    void VC1PackerDXVA::VC1PackPicParams (VC1Context* pContext,
        DXVA_PictureParameters* ptr,
        VideoAccelerator*)
    {
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {
            if (VC1_FieldInterlace == pContext->m_picLayerHeader->FCM)
            {
                ptr->wPicHeightInMBminus1 = (WORD)pContext->m_seqLayerHeader.CODED_HEIGHT;
            }
            else
            {
                ptr->wPicHeightInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_HEIGHT + 1) -1);
            }

            ptr->wPicWidthInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_WIDTH + 1) - 1);
        }
        else
        {
            /* Height in MBs minus 1 */
            ptr->wPicHeightInMBminus1 = (WORD)(pContext->m_seqLayerHeader.heightMB - 1);
            /* Width in MBs minus 1 */
            ptr->wPicWidthInMBminus1 = (WORD)(pContext->m_seqLayerHeader.widthMB - 1);
        }

        ptr->bMacroblockWidthMinus1  = 15;
        ptr->bMacroblockHeightMinus1 = 15;
        ptr->bBlockWidthMinus1       = 7;
        ptr->bBlockHeightMinus1      = 7;
        ptr->bBPPminus1              = 7;

        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {
            // interlace fields
            if (VC1_FieldInterlace == pContext->m_picLayerHeader->FCM)
            {
                ptr->bPicExtrapolation = 0x2;
                if (pContext->m_picLayerHeader->CurrField == 0)
                {
                    if (pContext->m_seqLayerHeader.PULLDOWN)
                    {
                        if (pContext->m_picLayerHeader->TFF)
                            ptr->bPicStructure = 1;
                        else
                            ptr->bPicStructure = 2;
                    }
                    else
                        ptr->bPicStructure = 1;
                }
                else
                {
                    if (pContext->m_picLayerHeader->TFF)
                        ptr->bPicStructure = 2;
                    else
                        ptr->bPicStructure = 1;
                }
            }
            else  // frames
            {
                ptr->bPicStructure = 0x03;    //Frame
                if (VC1_Progressive == pContext->m_picLayerHeader->FCM)
                    ptr->bPicExtrapolation = 1; //Progressive
                else
                    ptr->bPicExtrapolation = 2; //Interlace
            }
        }
        else
        {
            // all frames are progressivw
            ptr->bPicExtrapolation = 1;
            ptr->bPicStructure = 3;
        }

        ptr->bSecondField = pContext->m_picLayerHeader->CurrField;
        ptr->bPicIntra = 0;
        ptr->bBidirectionalAveragingMode = 0x80;

        if (VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicIntra = 1;
        }
        else if  (VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicBackwardPrediction = 1;
            ptr->bPicIntra = 1;
        }
        else if (VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicBackwardPrediction = 1;
        }


        //iWMA
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
            ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,3,1,1);

        // intensity compensation
        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if (pContext->m_bIntensityCompensation)
                ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,4,1,1);

        }

        //bicubic as default
        ptr->bMVprecisionAndChromaRelation = 0x04;
        if (VC1_IS_PRED(pContext->m_picLayerHeader->PTYPE)&&
            (pContext->m_picLayerHeader->FCM != VC1_FrameInterlace))
        {
            if (VC1_MVMODE_HPELBI_1MV == pContext->m_picLayerHeader->MVMODE)
                ptr->bMVprecisionAndChromaRelation |= 0x08;

        }

        if (pContext->m_seqLayerHeader.FASTUVMC)
            ptr->bMVprecisionAndChromaRelation |= 0x01;

        // 4:2:0
        ptr->bChromaFormat = 1;
        ptr->bPicScanFixed  = 1;
        ptr->bPicScanMethod = 0;
        ptr->bPicReadbackRequests = 0;
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
            ptr->bRcontrol = (BYTE)(pContext->m_picLayerHeader->RNDCTRL);
        else
            ptr->bRcontrol = (BYTE)(pContext->m_seqLayerHeader.RNDCTRL);

        ptr->bPicDeblocked = 0;

        //range reduction
        if (VC1_PROFILE_MAIN == pContext->m_seqLayerHeader.PROFILE &&
            pContext->m_seqLayerHeader.RANGERED)
        {
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked,5,1,pContext->m_picLayerHeader->RANGEREDFRM);
        }
        if (pContext->m_seqLayerHeader.LOOPFILTER && 
            pContext->DeblockInfo.isNeedDbl)
        {
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 1, 1, 1);
        }
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {

            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 2, 1, pContext->m_picLayerHeader->POSTPROC);
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 3, 1, pContext->m_picLayerHeader->POSTPROC);
        }


        if (VC1_B_FRAME != pContext->m_picLayerHeader->PTYPE)
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 6, 1, pContext->m_seqLayerHeader.OVERLAP); // overlap

        if (VC1_PROFILE_MAIN == pContext->m_seqLayerHeader.PROFILE &&
            VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE &&
            pContext->m_seqLayerHeader.LOOPFILTER)
        {
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 7, 1, 1);
        }

        ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 7, 1, pContext->m_seqLayerHeader.POSTPROCFLAG);
        ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 6, 1, pContext->m_seqLayerHeader.PULLDOWN);
        ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 5, 1, pContext->m_seqLayerHeader.INTERLACE);
        ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 4, 1, pContext->m_seqLayerHeader.TFCNTRFLAG);
        ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 3, 1, pContext->m_seqLayerHeader.FINTERPFLAG);

        if (VC1_PROFILE_ADVANCED != pContext->m_seqLayerHeader.PROFILE)
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 2, 1, 0); //RESERVED!!!!!!!!
        else
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 2, 1, 1);

        ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 1, 1, 0); // PSF _MAY_BE need
        ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 0, 1, pContext->m_seqLayerHeader.EXTENDED_DMV);

        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 7, 1, pContext->m_seqLayerHeader.PANSCAN_FLAG);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 6, 1, pContext->m_seqLayerHeader.REFDIST_FLAG);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 5, 1, pContext->m_seqLayerHeader.LOOPFILTER);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 4, 1, pContext->m_seqLayerHeader.FASTUVMC);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 3, 1, pContext->m_seqLayerHeader.EXTENDED_MV);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 1, 2, pContext->m_seqLayerHeader.DQUANT);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 0, 1, pContext->m_seqLayerHeader.VSTRANSFORM);

        ptr->bPicOverflowBlocks = bit_set(ptr->bPicOverflowBlocks, 6, 2, pContext->m_seqLayerHeader.QUANTIZER);
        if (VC1_PROFILE_ADVANCED != pContext->m_seqLayerHeader.PROFILE)
        {
            ptr->bPicOverflowBlocks = bit_set(ptr->bPicOverflowBlocks, 5, 1, pContext->m_seqLayerHeader.MULTIRES);
            ptr->bPicOverflowBlocks = bit_set(ptr->bPicOverflowBlocks, 4, 1, pContext->m_seqLayerHeader.SYNCMARKER);
            ptr->bPicOverflowBlocks = bit_set(ptr->bPicOverflowBlocks, 3, 1, pContext->m_seqLayerHeader.RANGERED);
            ptr->bPicOverflowBlocks = bit_set(ptr->bPicOverflowBlocks, 0, 3, pContext->m_seqLayerHeader.MAXBFRAMES);
        }

        ptr->bPic4MVallowed = 0;
        if (VC1_FrameInterlace == pContext->m_picLayerHeader->FCM &&
            1 == pContext->m_picLayerHeader->MV4SWITCH &&
            VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPic4MVallowed = 1;
        }
        else if (VC1_IS_PRED(pContext->m_picLayerHeader->PTYPE))
        {
            if (VC1_MVMODE_MIXED_MV == pContext->m_picLayerHeader->MVMODE)
                ptr->bPic4MVallowed = 1;
        }
        ptr->wDecodedPictureIndex = (WORD)(pContext->m_frmBuff.m_iCurrIndex);

        if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ||
            pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG ||
            pContext->m_picLayerHeader->RANGEREDFRM)
        {
            if(VC1_IS_REFERENCE(pContext->m_picLayerHeader->PTYPE))
                ptr->wDeblockedPictureIndex = (WORD)(pContext->m_frmBuff.m_iRangeMapIndex);
            else
                ptr->wDeblockedPictureIndex = (WORD)(pContext->m_frmBuff.m_iRangeMapIndexPrev);

        }

        if (1 == ptr->bPicIntra)
        {
            ptr->wForwardRefPictureIndex = 0xFFFF;
            ptr->wBackwardRefPictureIndex = 0xFFFF;
        }
        else if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->wBackwardRefPictureIndex = 0xFFFF;
            if (VC1_FieldInterlace != pContext->m_picLayerHeader->FCM ||
                0 == pContext->m_picLayerHeader->CurrField)
            {
                ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
            }
            else
            {
                if (1 == pContext->m_picLayerHeader->NUMREF)
                {
                    // first frame MAY_BE. Need to check
                    if (-1 != pContext->m_frmBuff.m_iPrevIndex)
                        ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
                    else
                        ptr->wForwardRefPictureIndex = 0;

                }
                else if (1 == pContext->m_picLayerHeader->REFFIELD)
                {
                    ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
                }
                else
                {
                    ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iCurrIndex);
                }
            }
        }
        else
        {
            ptr->wForwardRefPictureIndex  = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
            ptr->wBackwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iNextIndex);
        }

        //Range Mapping
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {
            if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG)
            {
                ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 7, 1, pContext->m_seqLayerHeader.RANGE_MAPY_FLAG);
                ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 4, 3, pContext->m_seqLayerHeader.RANGE_MAPY);
            }
            if (pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG)
            {
                ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 3, 1,pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG);
                ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 0, 3, pContext->m_seqLayerHeader.RANGE_MAPUV);
            }
        }
        else if (VC1_PROFILE_MAIN == pContext->m_seqLayerHeader.PROFILE && pContext->m_picLayerHeader->RANGEREDFRM)
        {
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 7, 1, pContext->m_picLayerHeader->RANGEREDFRM);
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 4, 3, 7);
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 3, 1, pContext->m_picLayerHeader->RANGEREDFRM);
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 0, 3, 7);
        }
        else
        {
            ptr->bPicOBMC = 0;
        }
        ptr->bPicBinPB = 0; // RESPIC - need to add
        ptr->bMV_RPS = 0;
        ptr->bReservedBits = BYTE(pContext->m_picLayerHeader->PQUANT);

        ptr->wBitstreamFcodes = 0;
        ptr->wBitstreamPCEelements = 0;

        if (pContext->m_bIntensityCompensation)
        {
            if (3 == ptr->bPicStructure)
            {
                ptr->wBitstreamFcodes = WORD(pContext->m_picLayerHeader->LUMSCALE + 32);
                ptr->wBitstreamPCEelements = WORD(pContext->m_picLayerHeader->LUMSHIFT);
            }
            else
            {
                if (0 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->m_picLayerHeader->LUMSCALE1 + 32);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->m_picLayerHeader->LUMSHIFT1);

                    //bottom field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 0, 8, pContext->m_picLayerHeader->LUMSCALE1 + 32);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 0, 8, pContext->m_picLayerHeader->LUMSHIFT1);
                }
                else if (1 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->m_picLayerHeader->LUMSCALE1 + 32);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->m_picLayerHeader->LUMSHIFT1);
                    // bottom field not compensated
                    ptr->wBitstreamFcodes |= 32;
                }
                else if (2 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->m_picLayerHeader->LUMSCALE1 + 32);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->m_picLayerHeader->LUMSHIFT1);
                    // bottom field not compensated
                    ptr->wBitstreamFcodes |= 32;
                }
            }
        }
        else
        {
            ptr->wBitstreamFcodes = 32;
            if (ptr->bPicStructure != 3)
            {
                ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, 32);
            }
        }
        ptr->bBitstreamConcealmentNeed = 0;
        ptr->bBitstreamConcealmentMethod = 0;
    }

    uint32_t VC1PackerDXVA::VC1PackBitStreamSM (uint32_t Size,
        uint8_t* pOriginalData,
        uint32_t ByteOffset) // offset of the first byte of pbs in buffer
    {
        UMCVACompBuffer* CompBuf;
        uint8_t* pBitstream = (uint8_t*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);
        uint32_t DrvBufferSize = CompBuf->GetBufferSize();
        uint32_t RemainBytes = 0;

        if (DrvBufferSize < (Size + ByteOffset)) // we don't have enough buffer
        {
            RemainBytes = Size + ByteOffset - DrvBufferSize;
        }
        MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData, Size - RemainBytes);
        CompBuf->SetDataSize(ByteOffset + Size - RemainBytes);
        return RemainBytes;
    }
    uint32_t VC1PackerDXVA::VC1PackBitStreamAdv (VC1Context* ,
        uint32_t Size,
        uint8_t* pOriginalData,
        uint32_t ByteOffset)
    {
        UMCVACompBuffer* CompBuf;

        uint8_t* pBitstream = (uint8_t*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);
        uint32_t DrvBufferSize = CompBuf->GetBufferSize();
        uint32_t RemainBytes = 0;

        if (DrvBufferSize < (Size + ByteOffset)) // we don't have enough buffer
        {
            RemainBytes = Size + ByteOffset - DrvBufferSize;
        }
        MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData, Size - RemainBytes);

        CompBuf->SetDataSize(ByteOffset + Size - RemainBytes );
        // for correct .WMV files support. By now the driver can process only the whole frame/field
        return RemainBytes;
    }

    void VC1PackerDXVA::VC1PackBitplaneBuffers(VC1Context* )
    {
    }

    void VC1PackerDXVA::VC1PackBitStreamForOneSlice (VC1Context* pContext, uint32_t Size)
    {
        UMCVACompBuffer* CompBuf;
        uint8_t* pSliceData = (uint8_t*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER,&CompBuf);
        MFX_INTERNAL_CPY(pSliceData, (uint8_t*)(pContext->m_bitstream.pBitstream - 1), Size + 4);
        CompBuf->SetDataSize(Size + 4);
        SwapData((uint8_t*)pSliceData, Size + 4);
    }

#endif

#if defined (UMC_VA_LINUX)

    enum
    {
        VC1_I_I_FRAME   = 0,
        VC1_I_P_FRAME   = 1,
        VC1_P_I_FRAME   = 2,
        VC1_P_P_FRAME   = 3,
        VC1_B_B_FRAME   = 4,
        VC1_B_BI_FRAME  = 5,
        VC1_BI_B_FRAME  = 6,
        VC1_BI_BI_FRAME = 7
    };

    void VC1PackerLVA::VC1SetSliceDataBuffer(int32_t size)
    {
        UMCVACompBuffer* pCompBuf;
        uint8_t* ptr = (uint8_t*)m_va->GetCompBuffer(VASliceDataBufferType, &pCompBuf, size);
        if (!pCompBuf || (pCompBuf->GetBufferSize() < size))
            throw vc1_exception(mem_allocation_er);
        memset(ptr, 0, size);
    }
    void VC1PackerLVA::VC1SetBitplaneBuffer(int32_t size)
    {
        UMCVACompBuffer* CompBuf;
        uint8_t* ptr = (uint8_t*)m_va->GetCompBuffer(VABitPlaneBufferType, &CompBuf, size);
        if (CompBuf->GetBufferSize() < size)
            throw vc1_exception(mem_allocation_er);
        memset(ptr, 0, size);
    }

    void VC1PackerLVA::VC1SetSliceParamBuffer(uint32_t* pOffsets, uint32_t* pValues)
    {
        const uint32_t Slice = 0x0B010000;
        const uint32_t Field = 0x0C010000;
        int32_t slice_counter = 1;

        pOffsets++;
        pValues++;
        while (*pOffsets)
        {
            if (pValues && (*pValues == Slice || *pValues == Field))
            {
                ++slice_counter;
            }
            ++pOffsets;
            ++pValues;
        }

        UMCVACompBuffer* pCompBuf;
        m_pSliceInfo = (VASliceParameterBufferVC1*)m_va->GetCompBuffer(VASliceParameterBufferType, &pCompBuf, slice_counter * sizeof(VASliceParameterBufferVC1));
        if (!pCompBuf || (static_cast<unsigned int>(pCompBuf->GetBufferSize()) < slice_counter * sizeof(VASliceParameterBufferVC1)))
            throw vc1_exception(mem_allocation_er);
        memset(m_pSliceInfo, 0, slice_counter * sizeof(VASliceParameterBufferVC1));
    }

    void VC1PackerLVA::VC1SetPictureBuffer()
    {
        UMCVACompBuffer* pCompBuf;
        m_pPicPtr = (VAPictureParameterBufferVC1*)m_va->GetCompBuffer(VAPictureParameterBufferType, &pCompBuf,sizeof(VAPictureParameterBufferVC1));
        if (!pCompBuf || (static_cast<unsigned int>(pCompBuf->GetBufferSize()) < sizeof(VAPictureParameterBufferVC1)))
            throw vc1_exception(mem_allocation_er);
        memset(m_pPicPtr, 0, sizeof(VAPictureParameterBufferType));
    }

    //to support multislice mode. Save number of slices value in reserved bits of first slice
    void VC1PackerLVA::VC1SetBuffersSize(uint32_t SliceBufIndex)
    {
        UMCVACompBuffer* CompBuf;
        m_va->GetCompBuffer(VASliceParameterBufferType,&CompBuf);
        CompBuf->SetDataSize(SliceBufIndex*sizeof(VASliceParameterBufferVC1));
        CompBuf->SetNumOfItem(SliceBufIndex);
        m_va->GetCompBuffer(VAPictureParameterBufferType,&CompBuf);
        CompBuf->SetDataSize(sizeof(VAPictureParameterBufferVC1));
    }

    void VC1PackerLVA::VC1PackBitplaneBuffers(VC1Context* pContext)
    {
        UMCVACompBuffer* CompBuf;
        int32_t i;
        int32_t bitplane_size = 0;
        int32_t real_bitplane_size = 0;
        VC1Bitplane* lut_bitplane[3];
        VC1Bitplane* check_bitplane = NULL;

        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
            lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
            lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
            break;
        case VC1_P_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
            break;
        case VC1_B_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
            break;
        default:
            return;
        }

        for (i = 0; i < 3; i++)
        {
            if ((!VC1_IS_BITPLANE_RAW_MODE(lut_bitplane[i])) && lut_bitplane[i]->m_databits)
                check_bitplane = lut_bitplane[i];

        }
        if(check_bitplane)
        {

            bitplane_size = ((pContext->m_seqLayerHeader.heightMB+1)/2)*pContext->m_seqLayerHeader.widthMB;
            real_bitplane_size = pContext->m_seqLayerHeader.heightMB * pContext->m_seqLayerHeader.widthMB;
            if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
                bitplane_size /= 2;

            uint8_t* ptr = (uint8_t*)m_va->GetCompBuffer(VABitPlaneBufferType, &CompBuf, bitplane_size);
            if (!ptr)
                throw vc1_exception(mem_allocation_er);
            memset(ptr, 0, bitplane_size);
            for (i = 0; i < 3; i++)
            {
                if (!lut_bitplane[i]->m_databits)
                    lut_bitplane[i] = check_bitplane;
            }

            for (i = 0; i < real_bitplane_size - (real_bitplane_size & 0x1);)
            {
                *ptr = (lut_bitplane[0]->m_databits[i] << 4) + (lut_bitplane[1]->m_databits[i] << 5) +
                    (lut_bitplane[2]->m_databits[i] << 6) + lut_bitplane[0]->m_databits[i+1] +
                    (lut_bitplane[1]->m_databits[i+1] << 1) + (lut_bitplane[2]->m_databits[i+1] << 2);
                i += 2;
                ++ptr;
            }

            // last macroblock case
            if (real_bitplane_size & 0x1)
                *ptr = (lut_bitplane[0]->m_databits[i] << 4) + (lut_bitplane[1]->m_databits[i] << 5) +
                (lut_bitplane[2]->m_databits[i] << 6);

            CompBuf->SetDataSize(bitplane_size);
        }
    }

    uint32_t VC1PackerLVA::VC1PackBitStreamAdv (VC1Context* pContext,
        uint32_t& Size,
        uint8_t* pOriginalData,
        uint32_t OriginalSize,
        uint32_t ByteOffset,
        uint8_t& Flag_03)
    {
        UMCVACompBuffer* CompBuf;
        uint8_t* pBitstream = (uint8_t*)m_va->GetCompBuffer(VASliceDataBufferType, &CompBuf, OriginalSize);

        uint32_t DrvBufferSize = CompBuf->GetBufferSize();
        uint8_t* pEnd = pBitstream + ByteOffset + OriginalSize;
        Size = OriginalSize;

        if (DrvBufferSize < (OriginalSize + ByteOffset)) // we don't have enough buffer
        {
            throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
        }

        if(Flag_03 == 1)
        {
            *(pBitstream + ByteOffset) = 0;
            MFX_INTERNAL_CPY(pBitstream + ByteOffset + 1, pOriginalData + 1, OriginalSize - 1);
            Flag_03 = 0;
        }
        else if(Flag_03 == 2)
        {
            if (pContext->m_bitstream.bitOffset < 24)
            {
                MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData + 1, OriginalSize + 1);
                pEnd++;

                *(pBitstream + ByteOffset + 1) = 0;
                Size--;
            }
            else
            {
                MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData, OriginalSize );
            }
            Flag_03 = 0;
        }
        else if(Flag_03 == 3)
        {
            if(pContext->m_bitstream.bitOffset < 16)
            {
                *(pBitstream + ByteOffset) = *pOriginalData;
                *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
                *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
                *(pBitstream + ByteOffset + 3) = *(pOriginalData + 4);
                MFX_INTERNAL_CPY(pBitstream + ByteOffset + 4, pOriginalData + 5, OriginalSize - 4);
                Size--;
            }
            else
            {
                MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData, OriginalSize);
            }

            Flag_03 = 0;
        }
        else if(Flag_03 == 4)
        {
            MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData, OriginalSize);
            Flag_03 = 0;

        }
        else if(Flag_03 == 5)
        {
            *(pBitstream + ByteOffset) = 0;
            *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
            *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
            MFX_INTERNAL_CPY(pBitstream + ByteOffset + 3, pOriginalData + 4, OriginalSize - 4);
            pEnd--;

            Size--;
            Flag_03 = 0;
        }
        else
        {
            MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData, OriginalSize);
        }

        CompBuf->SetDataSize(ByteOffset + Size);

        if(*(pEnd-1) == 0x03 && *(pEnd -2) == 0x0 && *(pEnd-3) == 0x0)
        {
            *(pEnd-1) = 0;
        }

        return 0;
    }

    static int ConvertMvModeVC1Mfx2VA(int mv_mode)
    {
        switch (mv_mode) {
        case VC1_MVMODE_HPELBI_1MV:     return VAMvMode1MvHalfPelBilinear;
        case VC1_MVMODE_1MV:            return VAMvMode1Mv;
        case VC1_MVMODE_HPEL_1MV:       return VAMvMode1MvHalfPel;
        case VC1_MVMODE_MIXED_MV:       return VAMvModeMixedMv;
        case VC1_MVMODE_INTENSCOMP:     return VAMvModeIntensityCompensation;
        }
        return 0;
    }

    void VC1PackerLVA::VC1PackPicParams (VC1Context* pContext, VAPictureParameterBufferVC1* ptr, VideoAccelerator* va)
    {
        memset(ptr, 0, sizeof(VAPictureParameterBufferVC1));

        ptr->forward_reference_picture                                 = VA_INVALID_ID;
        ptr->backward_reference_picture                                = VA_INVALID_ID;
        ptr->inloop_decoded_picture                                    = VA_INVALID_ID;

        if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ||
            pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG ||
            pContext->m_seqLayerHeader.RANGERED)
        {
            va->BeginFrame(pContext->m_frmBuff.m_iRangeMapIndex);
            ptr->inloop_decoded_picture = va->GetSurfaceID(pContext->m_frmBuff.m_iCurrIndex);
        }
        else
            va->BeginFrame(pContext->m_frmBuff.m_iCurrIndex);


        ptr->sequence_fields.bits.pulldown = pContext->m_seqLayerHeader.PULLDOWN;
        ptr->sequence_fields.bits.interlace = pContext->m_seqLayerHeader.INTERLACE;
        ptr->sequence_fields.bits.tfcntrflag = pContext->m_seqLayerHeader.TFCNTRFLAG;
        ptr->sequence_fields.bits.finterpflag = pContext->m_seqLayerHeader.FINTERPFLAG;
        ptr->sequence_fields.bits.psf = 0;
        ptr->sequence_fields.bits.multires = pContext->m_seqLayerHeader.MULTIRES;
        ptr->sequence_fields.bits.overlap = pContext->m_seqLayerHeader.OVERLAP;
        ptr->sequence_fields.bits.syncmarker = pContext->m_seqLayerHeader.SYNCMARKER;
        ptr->sequence_fields.bits.rangered = pContext->m_seqLayerHeader.RANGERED;
        ptr->sequence_fields.bits.max_b_frames = pContext->m_seqLayerHeader.MAXBFRAMES;
        ptr->sequence_fields.bits.profile = pContext->m_seqLayerHeader.PROFILE;

        ptr->coded_width = 2 * (pContext->m_seqLayerHeader.CODED_WIDTH +1);
        ptr->coded_height = 2 * (pContext->m_seqLayerHeader.CODED_HEIGHT + 1);

        ptr->entrypoint_fields.bits.broken_link  = pContext->m_seqLayerHeader.BROKEN_LINK;
        ptr->entrypoint_fields.bits.closed_entry = pContext->m_seqLayerHeader.CLOSED_ENTRY;
        ptr->entrypoint_fields.bits.panscan_flag = pContext->m_seqLayerHeader.PANSCAN_FLAG;
        ptr->entrypoint_fields.bits.loopfilter = pContext->m_seqLayerHeader.LOOPFILTER;

        switch(pContext->m_picLayerHeader->CONDOVER)
        {
        case 0:
            ptr->conditional_overlap_flag = 0; break;
        case 2:
            ptr->conditional_overlap_flag = 1; break;
        default:
            ptr->conditional_overlap_flag = 2; break;
        }
        ptr->fast_uvmc_flag = pContext->m_seqLayerHeader.FASTUVMC;

        ptr->range_mapping_fields.bits.luma_flag = pContext->m_seqLayerHeader.RANGE_MAPY_FLAG;
        ptr->range_mapping_fields.bits.luma = pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ? pContext->m_seqLayerHeader.RANGE_MAPY : 0;
        ptr->range_mapping_fields.bits.chroma_flag = pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG;
        ptr->range_mapping_fields.bits.chroma = pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG ? pContext->m_seqLayerHeader.RANGE_MAPUV : 0;

        ptr->b_picture_fraction       = pContext->m_picLayerHeader->BFRACTION_index;
        ptr->cbp_table                = pContext->m_picLayerHeader->CBPTAB;
        ptr->mb_mode_table            = pContext->m_picLayerHeader->MBMODETAB;
        ptr->range_reduction_frame = pContext->m_picLayerHeader->RANGEREDFRM;
        ptr->rounding_control =  pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED ? pContext->m_picLayerHeader->RNDCTRL : pContext->m_seqLayerHeader.RNDCTRL;
        ptr->post_processing          = pContext->m_picLayerHeader->POSTPROC;
        ptr->picture_resolution_index = 0;

        if (pContext->m_bIntensityCompensation)
        {
            if (VC1_FieldInterlace != pContext->m_picLayerHeader->FCM)
            {
                ptr->luma_scale = pContext->m_picLayerHeader->LUMSCALE;
                ptr->luma_shift = pContext->m_picLayerHeader->LUMSHIFT;
            }
            else
            {
                if (VC1_INTCOMP_BOTH_FIELD == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->luma_scale = pContext->m_picLayerHeader->LUMSCALE;
                    ptr->luma_shift = pContext->m_picLayerHeader->LUMSHIFT;

                    //bottom field
                    ptr->luma_scale2 = pContext->m_picLayerHeader->LUMSCALE1;
                    ptr->luma_shift2 = pContext->m_picLayerHeader->LUMSHIFT1;
                }
                else if (VC1_INTCOMP_BOTTOM_FIELD == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // bottom field
                    ptr->luma_scale2 = pContext->m_picLayerHeader->LUMSCALE1;
                    ptr->luma_shift2 = pContext->m_picLayerHeader->LUMSHIFT1;

                    // top field not compensated
                    ptr->luma_scale = 32;
                }
                else if (VC1_INTCOMP_TOP_FIELD == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->luma_scale = pContext->m_picLayerHeader->LUMSCALE;
                    ptr->luma_shift = pContext->m_picLayerHeader->LUMSHIFT;

                    // bottom field not compensated
                    ptr->luma_scale2 = 32;
                }
            }
        }
        else
        {
            ptr->luma_scale  = 32;
            ptr->luma_scale2 = 32;
        }

        /*picture_fields.bits.picture_type*/
        if(pContext->m_picLayerHeader->FCM == VC1_Progressive || pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
            ptr->picture_fields.bits.picture_type = pContext->m_picLayerHeader->PTYPE;
        else
        {
            //field picture
            switch(pContext->m_picLayerHeader->PTypeField1)
            {
            case VC1_I_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_I_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_I_I_FRAME;
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_P_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_I_P_FRAME;
                else
                    VM_ASSERT(0);
                break;
            case VC1_P_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_P_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_P_P_FRAME;
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_I_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_P_I_FRAME;
                else
                    VM_ASSERT(0);
                break;
            case VC1_B_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_B_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_B_B_FRAME;
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_BI_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_B_BI_FRAME;
                else
                    VM_ASSERT(0);
                break;
            case VC1_BI_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_B_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_BI_B_FRAME;
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_BI_FRAME)
                    ptr->picture_fields.bits.picture_type = VC1_BI_BI_FRAME;
                else
                    VM_ASSERT(0);
                break;
            default:
                VM_ASSERT(0);
                break;
            }
        }
        ptr->picture_fields.bits.frame_coding_mode =  pContext->m_picLayerHeader->FCM;
        ptr->picture_fields.bits.top_field_first = pContext->m_picLayerHeader->TFF;
        ptr->picture_fields.bits.is_first_field = ! pContext->m_picLayerHeader->CurrField;
        ptr->picture_fields.bits.intensity_compensation = pContext->m_bIntensityCompensation;

        ptr->bitplane_present.value = 0;
        VC1Bitplane* check_bitplane = 0;
        VC1Bitplane* lut_bitplane[3];

        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
            lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
            lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
            break;
        case VC1_P_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
            break;
        case VC1_B_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
            break;
        default:
            return;
        }

        for (uint32_t i = 0; i < 3; i++)
        {
            if ((!VC1_IS_BITPLANE_RAW_MODE(lut_bitplane[i])) && lut_bitplane[i]->m_databits)
                check_bitplane = lut_bitplane[i];
        }
        if (check_bitplane)
        {
            ptr->raw_coding.flags.mv_type_mb = VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->MVTYPEMB) && pContext->m_picLayerHeader->MVTYPEMB.m_databits ? 1 : 0;
            ptr->raw_coding.flags.direct_mb =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->m_DirectMB) && pContext->m_picLayerHeader->m_DirectMB.m_databits ? 1 : 0;
            ptr->raw_coding.flags.skip_mb =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->SKIPMB) && pContext->m_picLayerHeader->SKIPMB.m_databits ? 1 : 0;
            ptr->raw_coding.flags.field_tx =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FIELDTX) && pContext->m_picLayerHeader->FIELDTX.m_databits  ? 1 : 0;
            ptr->raw_coding.flags.forward_mb =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FORWARDMB) && pContext->m_picLayerHeader->FORWARDMB.m_databits  ? 1 : 0;
            ptr->raw_coding.flags.ac_pred =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->ACPRED) && pContext->m_picLayerHeader->ACPRED.m_databits  ? 1 : 0;
            ptr->raw_coding.flags.overflags =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->OVERFLAGS) && pContext->m_picLayerHeader->OVERFLAGS.m_databits  ? 1 : 0;

            ptr->bitplane_present.value = 1;
            ptr->bitplane_present.flags.bp_mv_type_mb = VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->MVTYPEMB) ? 0 : 1;
            ptr->bitplane_present.flags.bp_direct_mb =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->m_DirectMB) ? 0 : 1;
            ptr->bitplane_present.flags.bp_skip_mb =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->SKIPMB) ? 0 : 1;
            ptr->bitplane_present.flags.bp_field_tx =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FIELDTX) ? 0 : 1;
            ptr->bitplane_present.flags.bp_forward_mb =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FORWARDMB) ? 0 : 1;
            ptr->bitplane_present.flags.bp_ac_pred =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->ACPRED) ? 0 : 1;
            ptr->bitplane_present.flags.bp_overflags =  VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->OVERFLAGS) ? 0 : 1;
        }

        ptr->reference_fields.bits.reference_distance_flag = pContext->m_seqLayerHeader.REFDIST_FLAG;
        ptr->reference_fields.bits.reference_distance = pContext->m_picLayerHeader->REFDIST;
        ptr->reference_fields.bits.num_reference_pictures  = pContext->m_picLayerHeader->NUMREF;
        ptr->reference_fields.bits.reference_field_pic_indicator = pContext->m_picLayerHeader->REFFIELD;

        ptr->mv_fields.bits.mv_mode = (VAMvModeVC1)ConvertMvModeVC1Mfx2VA(pContext->m_picLayerHeader->MVMODE);
        ptr->mv_fields.bits.mv_mode2 = (VAMvModeVC1)ConvertMvModeVC1Mfx2VA(pContext->m_picLayerHeader->MVMODE2);
        ptr->mv_fields.bits.mv_table = pContext->m_picLayerHeader->MVTAB;
        ptr->mv_fields.bits.two_mv_block_pattern_table = pContext->m_picLayerHeader->MV2BPTAB;
        ptr->mv_fields.bits.four_mv_switch = pContext->m_picLayerHeader->MV4SWITCH;
        ptr->mv_fields.bits.four_mv_block_pattern_table = pContext->m_picLayerHeader->MV4BPTAB;
        ptr->mv_fields.bits.extended_mv_flag = pContext->m_seqLayerHeader.EXTENDED_MV;
        ptr->mv_fields.bits.extended_mv_range = pContext->m_picLayerHeader->MVRANGE;
        ptr->mv_fields.bits.extended_dmv_flag = pContext->m_seqLayerHeader.EXTENDED_DMV;
        ptr->mv_fields.bits.extended_dmv_range = pContext->m_picLayerHeader->DMVRANGE;

        ptr->pic_quantizer_fields.bits.dquant = pContext->m_seqLayerHeader.DQUANT;
        ptr->pic_quantizer_fields.bits.quantizer = pContext->m_seqLayerHeader.QUANTIZER;
        ptr->pic_quantizer_fields.bits.half_qp = pContext->m_picLayerHeader->HALFQP;
        ptr->pic_quantizer_fields.bits.pic_quantizer_scale = pContext->m_picLayerHeader->PQUANT;
        ptr->pic_quantizer_fields.bits.pic_quantizer_type = !pContext->m_picLayerHeader->QuantizationType;
        ptr->pic_quantizer_fields.bits.dq_frame = pContext->m_picLayerHeader->m_DQuantFRM;
        ptr->pic_quantizer_fields.bits.dq_profile = pContext->m_picLayerHeader->m_DQProfile;
        ptr->pic_quantizer_fields.bits.dq_sb_edge = pContext->m_picLayerHeader->m_DQProfile == VC1_DQPROFILE_SNGLEDGES  ? pContext->m_picLayerHeader->DQSBEdge : 0;
        ptr->pic_quantizer_fields.bits.dq_db_edge = pContext->m_picLayerHeader->m_DQProfile == VC1_DQPROFILE_DBLEDGES  ? pContext->m_picLayerHeader->DQSBEdge : 0;
        ptr->pic_quantizer_fields.bits.dq_binary_level = pContext->m_picLayerHeader->m_DQBILevel;
        ptr->pic_quantizer_fields.bits.alt_pic_quantizer = pContext->m_picLayerHeader->m_AltPQuant;

        ptr->transform_fields.bits.variable_sized_transform_flag = pContext->m_seqLayerHeader.VSTRANSFORM;
        ptr->transform_fields.bits.mb_level_transform_type_flag = pContext->m_picLayerHeader->TTMBF;
        ptr->transform_fields.bits.frame_level_transform_type = pContext->m_picLayerHeader->TTFRM_ORIG;
        ptr->transform_fields.bits.transform_ac_codingset_idx1 = pContext->m_picLayerHeader->TRANSACFRM;
        ptr->transform_fields.bits.transform_ac_codingset_idx2 = pContext->m_picLayerHeader->TRANSACFRM2;
        ptr->transform_fields.bits.intra_transform_dc_table = pContext->m_picLayerHeader->TRANSDCTAB;

        if (pContext->m_picLayerHeader->PTYPE == VC1_B_FRAME)
        {
            ptr->backward_reference_picture = va->GetSurfaceID(pContext->m_frmBuff.m_iNextIndex);
            ptr->forward_reference_picture =  va->GetSurfaceID(pContext->m_frmBuff.m_iPrevIndex);
        }
        else if (pContext->m_picLayerHeader->PTYPE == VC1_P_FRAME)
        {
            ptr->forward_reference_picture =  va->GetSurfaceID(pContext->m_frmBuff.m_iPrevIndex);
        }
    }
    void  VC1PackerLVA::VC1PackOneSlice (VC1Context* pContext,
        SliceParams* slparams,
        uint32_t SliceBufIndex,
        uint32_t MBOffset,
        uint32_t SliceDataSize,
        uint32_t StartCodeOffset,
        uint32_t ChoppingType) //compatibility with Windows code
    {
        (void)pContext;
        (void)ChoppingType;

        if (SliceBufIndex)
            ++m_pSliceInfo;

        m_pSliceInfo->macroblock_offset = ((31-slparams->m_bitOffset) + MBOffset*8);
        m_pSliceInfo->slice_data_offset = StartCodeOffset; // offset in bytes
        m_pSliceInfo->slice_vertical_position = slparams->MBStartRow;
        m_pSliceInfo->slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
        m_pSliceInfo->slice_data_size = SliceDataSize;
    }
    void VC1PackerLVA::VC1PackWholeSliceSM (VC1Context* pContext,
        uint32_t MBOffset,
        uint32_t SliceDataSize)
    {
        m_pSliceInfo->slice_vertical_position = 0;
        m_pSliceInfo->slice_data_size = SliceDataSize;
        m_pSliceInfo->macroblock_offset = ((31-pContext->m_bitstream.bitOffset) + MBOffset*8);
        m_pSliceInfo->slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
        m_pSliceInfo->slice_data_offset = 0;
    }


    void VC1PackerLVA::VC1PackBitStreamForOneSlice (VC1Context* pContext, uint32_t Size)
    {
        UMCVACompBuffer* CompBuf;
        uint8_t* pSliceData = (uint8_t*)m_va->GetCompBuffer(VASliceDataBufferType,&CompBuf);
        MFX_INTERNAL_CPY(pSliceData, (uint8_t*)(pContext->m_bitstream.pBitstream - 1), Size+4);
        CompBuf->SetDataSize(Size+4);
        SwapData((uint8_t*)pSliceData, Size + 4);
    }
#endif

#ifdef UMC_VA_DXVA
    void VC1PackerDXVA_EagleLake::VC1SetExtPictureBuffer()
    {
        UMCVACompBuffer* CompBuf;
        m_pExtPicInfo = (DXVA_ExtPicInfo*)m_va->GetCompBuffer(DXVA2_VC1PICTURE_PARAMS_EXT_BUFFER, &CompBuf,sizeof(DXVA_ExtPicInfo));
        if (CompBuf->GetBufferSize() < sizeof(DXVA_ExtPicInfo))
            throw vc1_exception(mem_allocation_er);
    }

    void VC1PackerDXVA_EagleLake::VC1SetBuffersSize(uint32_t SliceBufIndex)
    {
        //_MAY_BE_
        UMCVACompBuffer* CompBuf;
        m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER,&CompBuf);
        CompBuf->SetDataSize((int32_t)(sizeof(DXVA_SliceInfo)*SliceBufIndex));
        CompBuf->SetNumOfItem(SliceBufIndex);

        m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER,&CompBuf);
        CompBuf->SetDataSize(sizeof(DXVA_PictureParameters));

        m_va->GetCompBuffer(DXVA2_VC1PICTURE_PARAMS_EXT_BUFFER,&CompBuf);
        CompBuf->SetDataSize(sizeof(DXVA_ExtPicInfo));
    }

    void VC1PackerDXVA_EagleLake::VC1PackExtPicParams (VC1Context* pContext,
        DXVA_ExtPicInfo* ptr,
        VideoAccelerator* )
    {
        memset(ptr, 0, sizeof(DXVA_ExtPicInfo));

        //BFraction // ADVANCE
        ptr->bBScaleFactor = (uint8_t)pContext->m_picLayerHeader->ScaleFactor;

        //PQUANT
        if(pContext->m_seqLayerHeader.QUANTIZER == 0 
            && pContext->m_picLayerHeader->PQINDEX > 8)
        {
            ptr->bPQuant = (pContext->m_picLayerHeader->PQINDEX < 29)
                ? (uint8_t)(pContext->m_picLayerHeader->PQINDEX - 3) 
                : (uint8_t)(pContext->m_picLayerHeader->PQINDEX*2 - 31);
        }
        else
        {
            ptr->bPQuant = (uint8_t)pContext->m_picLayerHeader->PQINDEX;
        }

        //ALTPQUANT
        ptr->bAltPQuant = (uint8_t)pContext->m_picLayerHeader->m_AltPQuant;

        //PictureFlags
        if(pContext->m_picLayerHeader->FCM == VC1_Progressive)
        {
            ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 0);  //progressive
        }
        else if(pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
        {
            ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 1);  //interlace frame
        }
        else
        {
            if(pContext->m_picLayerHeader->TFF)
            {
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 2);  //interlace field, top field first
            }
            else
            {
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 3);  //interlace field, bottom field first
            }
        }

        if(pContext->m_picLayerHeader->FCM == VC1_Progressive || pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
        {
            //progressive or interlece frame
            switch(pContext->m_picLayerHeader->PTYPE)
            {
            case VC1_I_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 0);
                break;
            case VC1_P_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 1);
                break;
            case VC1_B_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 2);
                break;
            case VC1_BI_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 3);
                break;
            default:
                if (VC1_IS_SKIPPED(pContext->m_picLayerHeader->PTYPE))
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 4);
                else
                    VM_ASSERT(0);
                break;
            }
        }
        else
        {
            //field picture
            switch(pContext->m_picLayerHeader->PTypeField1)
            {
            case VC1_I_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_I_FRAME)
                {
                    //I/I field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 0);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_P_FRAME)
                {
                    //I/P field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 1);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            case VC1_P_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_P_FRAME)
                {
                    //P/P field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 3);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_I_FRAME)
                {
                    //P/I field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 2);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            case VC1_B_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_B_FRAME)
                {
                    //B/B field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 4);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_BI_FRAME)
                {
                    //B/BI field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 5);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            case VC1_BI_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_B_FRAME)
                {
                    //BI/B field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 6);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_BI_FRAME)
                {
                    //BI/BI field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 7);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            default:
                VM_ASSERT(0);
                break;
            }
        }

        ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 5, 2, pContext->m_picLayerHeader->CONDOVER);

        //PQuantFlags
        if(pContext->m_picLayerHeader->QuantizationType == VC1_QUANTIZER_UNIFORM)
        {
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 0, 1, 1);
        }
        else
        {
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 0, 1, 0);
        }

        ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 1, 1, pContext->m_picLayerHeader->HALFQP);

        switch(pContext->m_picLayerHeader->m_PQuant_mode)
        {
        case VC1_ALTPQUANT_NO:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 0);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 0);
            break;
        case VC1_ALTPQUANT_MB_LEVEL:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 3);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 0);
            break;
        case VC1_ALTPQUANT_ANY_VALUE:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 2);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 0);
            break;
        case VC1_ALTPQUANT_LEFT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 1);
            break;
        case VC1_ALTPQUANT_TOP:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 2);
            break;
        case VC1_ALTPQUANT_RIGTHT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 4);
            break;
        case VC1_ALTPQUANT_BOTTOM:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 8);
            break;
        case VC1_ALTPQUANT_LEFT_TOP:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 3);
            break;
        case VC1_ALTPQUANT_TOP_RIGTHT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 6);
            break;
        case VC1_ALTPQUANT_RIGTHT_BOTTOM:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 12);
            break;
        case VC1_ALTPQUANT_BOTTOM_LEFT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 9);
            break;
        case VC1_ALTPQUANT_EDGES:
        case VC1_ALTPQUANT_ALL:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 15);
            break;
        default:
            VM_ASSERT(0);
            break;
        }

        //MvRange

        if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_NONE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 0);
        }
        else if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_HORIZONTAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 1);
        }
        else if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 2);
        }
        else if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_HORIZONTAL_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 3);
        }
        else
        {
            VM_ASSERT(0);
        }

        if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_NONE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 0);
        }
        else if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_HORIZONTAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 1);
        }
        else if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 2);
        }
        else if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_HORIZONTAL_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 3);
        }
        else
        {
            VM_ASSERT(0);
        }

        //wMvReference

        uint32_t FREFDIST = ((pContext->m_picLayerHeader->ScaleFactor * pContext->m_picLayerHeader->REFDIST) >> 8);
        int32_t BREFDIST = pContext->m_picLayerHeader->REFDIST - FREFDIST - 1;

        if (BREFDIST < 0)
            BREFDIST = 0;

        if(pContext->m_picLayerHeader->FCM == VC1_FieldInterlace && VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
            ptr->wMvReference = bit_set(ptr->wMvReference, 0, 4, FREFDIST);
        else
            ptr->wMvReference = bit_set(ptr->wMvReference, 0, 4, pContext->m_picLayerHeader->REFDIST);

        ptr->wMvReference = bit_set(ptr->wMvReference, 4, 4, BREFDIST);

        ptr->wMvReference = bit_set(ptr->wMvReference, 8, 1, pContext->m_picLayerHeader->NUMREF);
        ptr->wMvReference = bit_set(ptr->wMvReference, 9, 1, pContext->m_picLayerHeader->REFFIELD);
        ptr->wMvReference = bit_set(ptr->wMvReference, 10, 1, pContext->m_seqLayerHeader.FASTUVMC);
        ptr->wMvReference = bit_set(ptr->wMvReference, 11, 1, pContext->m_picLayerHeader->MV4SWITCH);

        switch(pContext->m_picLayerHeader->MVMODE)
        {
        case VC1_MVMODE_HPELBI_1MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 3);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
        case VC1_MVMODE_1MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 1);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
        case VC1_MVMODE_MIXED_MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 0);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
        case VC1_MVMODE_HPEL_1MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 2);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
        default:
            VM_ASSERT(0);
            break;
        }

        if(pContext->m_bIntensityCompensation)
        {    
            switch(pContext->m_picLayerHeader->INTCOMFIELD)
            {
            case VC1_INTCOMP_TOP_FIELD:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 1);
                break;
            case VC1_INTCOMP_BOTTOM_FIELD:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 2);
                break;
            case VC1_INTCOMP_BOTH_FIELD:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 3);
                break;
            default:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
                break;
            }
        }

        //wTransformFlags
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 0, 3, pContext->m_picLayerHeader->CBPTAB);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 3, 1, pContext->m_picLayerHeader->TRANSDCTAB);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 4, 2, pContext->m_picLayerHeader->TRANSACFRM);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 6, 2, pContext->m_picLayerHeader->TRANSACFRM2);


        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 8, 3, pContext->m_picLayerHeader->MBMODETAB);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 11, 1, pContext->m_picLayerHeader->TTMBF);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 12, 2, pContext->m_picLayerHeader->TTFRM_ORIG);

        //MvTableFlags
        ptr->bMvTableFlags = bit_set(ptr->bMvTableFlags, 0, 2, pContext->m_picLayerHeader->MV2BPTAB);
        ptr->bMvTableFlags = bit_set(ptr->bMvTableFlags, 2, 2, pContext->m_picLayerHeader->MV4BPTAB);
        ptr->bMvTableFlags = bit_set(ptr->bMvTableFlags, 4, 3, pContext->m_picLayerHeader->MVTAB);

        //RawCodingFlag
        ptr->bRawCodingFlag = 0;
        VC1Bitplane* check_bitplane = 0;
        VC1Bitplane* lut_bitplane[3];


        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
            lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
            lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
            break;
        case VC1_P_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
            break;
        case VC1_B_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
            break;
        default:
            return;
        }

        for (uint32_t i = 0; i < 3; i++)
        {
            if ((!VC1_IS_BITPLANE_RAW_MODE(lut_bitplane[i])) && lut_bitplane[i]->m_databits)
                check_bitplane = lut_bitplane[i];

        }
        if (!check_bitplane) // no bitplanes can return
        {
            return;
        }
        else
            ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 7, 1, 1);


        if (VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE ||
            VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FIELDTX)&&
                pContext->m_picLayerHeader->FIELDTX.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 0, 1, 1);
            }
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->ACPRED) &&
                pContext->m_picLayerHeader->ACPRED.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 1, 1, 1);
            }
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->OVERFLAGS)&&
                pContext->m_picLayerHeader->OVERFLAGS.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 2, 1, 1);
            }
        }

        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE ||
            VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
        {

            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->m_DirectMB)&&
                pContext->m_picLayerHeader->m_DirectMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 3, 1, 1);
            }
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->SKIPMB)&& 
                pContext->m_picLayerHeader->SKIPMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 4, 1, 1);
            }
        }

        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->MVTYPEMB)&&
                pContext->m_picLayerHeader->MVTYPEMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 5, 1, 1);
            }
        }

        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FORWARDMB)&&
                pContext->m_picLayerHeader->FORWARDMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 6, 1, 1);
            }
        }
    }

    void VC1PackerDXVA_EagleLake::VC1PackPicParams (VC1Context* pContext,
        DXVA_PictureParameters* ptr,
        VideoAccelerator*              va)
    {
        memset(ptr, 0, sizeof(DXVA_PictureParameters));
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {
            ptr->wPicHeightInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_HEIGHT + 1) - 1);
            ptr->wPicWidthInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_WIDTH + 1) - 1);
        }
        else
        {
            ptr->wPicHeightInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_HEIGHT + 1) - 1)/16;
            ptr->wPicWidthInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_WIDTH + 1) - 1)/16;
        }

        ptr->bMacroblockWidthMinus1  = 15;
        ptr->bMacroblockHeightMinus1 = 15;
        ptr->bBlockWidthMinus1       = 7;
        ptr->bBlockHeightMinus1      = 7;
        ptr->bBPPminus1              = 7;

        // interlace fields
        if (VC1_FieldInterlace == pContext->m_picLayerHeader->FCM)
        {
            ptr->bPicExtrapolation = 0x2;
            if (pContext->m_picLayerHeader->CurrField == 0)
            {
                if (pContext->m_seqLayerHeader.PULLDOWN)
                {
                    if (pContext->m_picLayerHeader->TFF)
                        ptr->bPicStructure = 1;
                    else
                        ptr->bPicStructure = 2;
                }
                else
                    ptr->bPicStructure = 1;
            }
            else
            {
                if (pContext->m_picLayerHeader->TFF)
                    ptr->bPicStructure = 2;
                else
                    ptr->bPicStructure = 1;
            }
        }
        else  // frames
        {
            ptr->bPicStructure = 0x03;    //Frame
            if (VC1_Progressive == pContext->m_picLayerHeader->FCM)
                ptr->bPicExtrapolation = 1; //Progressive
            else
                ptr->bPicExtrapolation = 2; //Interlace
        }

        ptr->bSecondField = pContext->m_picLayerHeader->CurrField;
        ptr->bPicIntra = 0;
        ptr->bBidirectionalAveragingMode = 0x80;

        if (VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicIntra = 1;
        }
        else if  (VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicBackwardPrediction = 1;
            ptr->bPicIntra = 1;
        }
        else if (VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicBackwardPrediction = 1;
        }

        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
            ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,3,1,1);

        // intensity compensation
        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if (pContext->m_bIntensityCompensation && (pContext->m_picLayerHeader->INTCOMFIELD
                || (ptr->bPicStructure==0x3)))
                ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,4,1,1);

            if (pContext->m_bIntensityCompensation && 
                (VC1_PROFILE_ADVANCED != pContext->m_seqLayerHeader.PROFILE))
                ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,4,1,1);

        }

        //bicubic as default
        ptr->bMVprecisionAndChromaRelation = 0x04;
        if (VC1_IS_PRED(pContext->m_picLayerHeader->PTYPE)&&
            (pContext->m_picLayerHeader->FCM != VC1_FrameInterlace))
        {
            if (VC1_MVMODE_HPELBI_1MV == pContext->m_picLayerHeader->MVMODE)
                ptr->bMVprecisionAndChromaRelation |= 0x08;

        }

        if (pContext->m_seqLayerHeader.FASTUVMC)
            ptr->bMVprecisionAndChromaRelation |= 0x01;

        // 4:2:0
        ptr->bChromaFormat = 1;
        ptr->bPicScanFixed  = 1;
        ptr->bPicScanMethod = 0;
        ptr->bPicReadbackRequests = 1;

        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
            ptr->bRcontrol = (BYTE)(pContext->m_picLayerHeader->RNDCTRL);
        else
            ptr->bRcontrol = (BYTE)(pContext->m_seqLayerHeader.RNDCTRL);

        ptr->bPicDeblocked = 0;

        // should be zero

        if (pContext->m_seqLayerHeader.LOOPFILTER &&
            pContext->DeblockInfo.isNeedDbl)
        {
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 1, 1, 1);
        }

        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {

            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 2, 1, pContext->m_picLayerHeader->POSTPROC);
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 3, 1, pContext->m_picLayerHeader->POSTPROC);
            if (VC1_B_FRAME != pContext->m_picLayerHeader->PTYPE)
                ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 6, 1, pContext->m_seqLayerHeader.OVERLAP); // overlap
        }
        else
        {
            if(pContext->DeblockInfo.isNeedDbl)
                ptr->bPicDeblocked = 2;

            if ((pContext->m_picLayerHeader->PQUANT >= 9) && (VC1_B_FRAME != pContext->m_picLayerHeader->PTYPE))
                ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 6, 1, pContext->m_seqLayerHeader.OVERLAP); // overlap
        }


        //range reduction
        if (VC1_PROFILE_MAIN == pContext->m_seqLayerHeader.PROFILE &&
            pContext->m_seqLayerHeader.RANGERED)
        {
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked,5,1,(pContext->m_picLayerHeader->RANGEREDFRM>>3));
        }

        ptr->bPicDeblockConfined = 0;
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 7, 1, pContext->m_seqLayerHeader.POSTPROCFLAG);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 6, 1, pContext->m_seqLayerHeader.PULLDOWN);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 5, 1, pContext->m_seqLayerHeader.INTERLACE);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 4, 1, pContext->m_seqLayerHeader.TFCNTRFLAG);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 3, 1, pContext->m_seqLayerHeader.FINTERPFLAG);

            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 2, 1, 1);

            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 1, 1, 0); // PSF _MAY_BE need
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 0, 1, pContext->m_seqLayerHeader.EXTENDED_DMV);
        }

        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 7, 1, pContext->m_seqLayerHeader.PANSCAN_FLAG);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 6, 1, pContext->m_seqLayerHeader.REFDIST_FLAG);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 5, 1, pContext->m_seqLayerHeader.LOOPFILTER);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 4, 1, pContext->m_seqLayerHeader.FASTUVMC);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 3, 1, pContext->m_seqLayerHeader.EXTENDED_MV);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 1, 2, pContext->m_seqLayerHeader.DQUANT);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 0, 1, pContext->m_seqLayerHeader.VSTRANSFORM);

        ptr->bPicOverflowBlocks = bit_set(ptr->bPicOverflowBlocks, 6, 2, pContext->m_seqLayerHeader.QUANTIZER);

        ptr->bPic4MVallowed = 0;
        if (VC1_FrameInterlace == pContext->m_picLayerHeader->FCM &&
            1 == pContext->m_picLayerHeader->MV4SWITCH &&
            VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPic4MVallowed = 1;
        }
        else if (VC1_IS_PRED(pContext->m_picLayerHeader->PTYPE))
        {
            if (VC1_MVMODE_MIXED_MV == pContext->m_picLayerHeader->MVMODE)
                ptr->bPic4MVallowed = 1;
        }

        ptr->wDecodedPictureIndex = (WORD)(pContext->m_frmBuff.m_iCurrIndex);
        ptr->wDeblockedPictureIndex  = ptr->wDecodedPictureIndex;

        if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ||
            pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG || 
            pContext->m_seqLayerHeader.RANGERED)
        {

            if(VC1_IS_REFERENCE(pContext->m_picLayerHeader->PTYPE))
            {
                va->BeginFrame(pContext->m_frmBuff.m_iRangeMapIndex);
                ptr->wDeblockedPictureIndex = (WORD)(pContext->m_frmBuff.m_iRangeMapIndex);
            }
            else
            {
                va->BeginFrame(pContext->m_frmBuff.m_iRangeMapIndex);
                ptr->wDeblockedPictureIndex = (WORD)(pContext->m_frmBuff.m_iRangeMapIndexPrev);
            }
        }

        if (1 == ptr->bPicIntra)
        {
            ptr->wForwardRefPictureIndex =  0xFFFF;
            ptr->wBackwardRefPictureIndex = 0xFFFF;
        }
        else if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->wBackwardRefPictureIndex = 0xFFFF;
            if (VC1_FieldInterlace != pContext->m_picLayerHeader->FCM ||
                0 == pContext->m_picLayerHeader->CurrField)
            {
                ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
            }
            else
            {
                if (1 == pContext->m_picLayerHeader->NUMREF)
                {
                    // first frame MAY_BE. Need to check
                    if (-1 != pContext->m_frmBuff.m_iPrevIndex)
                        ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
                    else
                        ptr->wForwardRefPictureIndex = 0;

                }
                else if (1 == pContext->m_picLayerHeader->REFFIELD)
                {
                    ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
                }
                else
                {
                    ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iCurrIndex);
                }
            }
        }
        else
        {
            ptr->wForwardRefPictureIndex  = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
            ptr->wBackwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iNextIndex);
        }

        ptr->bPicOBMC = 0;
        if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG)
        {
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 7, 1, pContext->m_seqLayerHeader.RANGE_MAPY_FLAG);
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 4, 3, pContext->m_seqLayerHeader.RANGE_MAPY);

        }
        if (pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG)
        {
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 3, 1,pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG);
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 0, 3, pContext->m_seqLayerHeader.RANGE_MAPUV);
        }

        ptr->bPicBinPB = 0; // RESPIC - need to add
        ptr->bMV_RPS = 0;
        ptr->bReservedBits = BYTE(pContext->m_picLayerHeader->PQUANT);

        ptr->wBitstreamFcodes = 0;
        ptr->wBitstreamPCEelements = 0;

        if (pContext->m_bIntensityCompensation)
        {
            if (3 == ptr->bPicStructure)
            {
                ptr->wBitstreamFcodes = WORD(pContext->m_picLayerHeader->LUMSCALE);
                ptr->wBitstreamPCEelements = WORD(pContext->m_picLayerHeader->LUMSHIFT);
            }
            else
            {
                if (3 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->m_picLayerHeader->LUMSCALE);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->m_picLayerHeader->LUMSHIFT);


                    //bottom field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 0, 8, pContext->m_picLayerHeader->LUMSCALE1);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 0, 8, pContext->m_picLayerHeader->LUMSHIFT1);                    

                }
                else if (2 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // bottom field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 0, 8, pContext->m_picLayerHeader->LUMSCALE1);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 0, 8, pContext->m_picLayerHeader->LUMSHIFT1);
                    // top field not compensated
                    ptr->wBitstreamFcodes |= (32<<8);
                }
                else if (1 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->m_picLayerHeader->LUMSCALE);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->m_picLayerHeader->LUMSHIFT);
                    // bottom field not compensated
                    ptr->wBitstreamFcodes |= 32;
                }
            }
        }
        else
        {
            ptr->wBitstreamFcodes = 32;
            if (ptr->bPicStructure != 3)
            {
                ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, 32);
            }
        }
        ptr->bBitstreamConcealmentNeed = 0;
        ptr->bBitstreamConcealmentMethod = 0;
    }

    uint32_t VC1PackerDXVA_EagleLake::VC1PackBitStreamAdv (VC1Context* pContext,
        uint32_t& Size,
        uint8_t* pOriginalData,
        uint32_t OriginalSize,
        uint32_t ByteOffset,
        uint8_t& Flag_03)
    {
        UMCVACompBuffer* CompBuf;

        uint8_t* pBitstream = (uint8_t*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);

        uint32_t DrvBufferSize = CompBuf->GetBufferSize();
        uint32_t RemainBytes = 0;

        Size = OriginalSize;

        if (DrvBufferSize < (OriginalSize + ByteOffset)) // we don't have enough buffer
        {
            RemainBytes = OriginalSize + ByteOffset - DrvBufferSize;
            Size = DrvBufferSize;
        }

        if(Flag_03 == 1)
        {
            *(pBitstream + ByteOffset) = 0;
            MFX_INTERNAL_CPY(pBitstream + ByteOffset + 1, pOriginalData + 1, OriginalSize - 1 - RemainBytes);
            Flag_03 = 0;
        }
        else if(Flag_03 == 2)
        {
            if (pContext->m_bitstream.bitOffset < 24)
            {
                MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData + 1, OriginalSize - RemainBytes + 1);
                *(pBitstream + ByteOffset + 1) = 0;
                Size--;
            }
            else
            {
                MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData, OriginalSize - RemainBytes);
            }
            Flag_03 = 0;
        }
        else if(Flag_03 == 3)
        {
            if(pContext->m_bitstream.bitOffset < 16)
            {
                *(pBitstream + ByteOffset) = *pOriginalData;
                *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
                *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
                *(pBitstream + ByteOffset + 3) = *(pOriginalData + 4);
                MFX_INTERNAL_CPY(pBitstream + ByteOffset + 4, pOriginalData + 5, OriginalSize - 4 - RemainBytes);
                Size--;
            }
            else
            {
                MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData, OriginalSize - RemainBytes);
            }

            Flag_03 = 0;
        }
        else if(Flag_03 == 4)
        {
            MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData, OriginalSize - RemainBytes);

            Flag_03 = 0;

        }
        else if(Flag_03 == 5)
        {
            *(pBitstream + ByteOffset) = 0;
            *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
            *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
            MFX_INTERNAL_CPY(pBitstream + ByteOffset + 3, pOriginalData + 4, OriginalSize - 4 - RemainBytes);

            Size--;
            Flag_03 = 0;
        }
        else
        {
            MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData, OriginalSize - RemainBytes);
        }

        CompBuf->SetDataSize(ByteOffset + Size);
        return RemainBytes;
    }

    void VC1PackerDXVA_EagleLake::VC1PackBitplaneBuffers(VC1Context* pContext)
    {
        UMCVACompBuffer* CompBuf;

        uint8_t* ptr = 0;

        int32_t i = 0;
        int32_t j = 0;
        int32_t k = 0;

        int32_t h = pContext->m_seqLayerHeader.heightMB;

        VC1Bitplane* lut_bitplane[3];
        VC1Bitplane* check_bitplane = 0;

        int32_t bitplane_size = 0;

        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
            lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
            lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
            break;
        case VC1_P_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
            break;
        case VC1_B_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
            break;
        default: // VC1_SKIPPED_FRAME:
            return;
        }

        for (i = 0; i < 3; i++)
        {
            if ((!VC1_IS_BITPLANE_RAW_MODE(lut_bitplane[i])) && lut_bitplane[i]->m_databits)
                check_bitplane = lut_bitplane[i];

        }

        if (check_bitplane)
        {
            ptr = (uint8_t*)m_va->GetCompBuffer(DXVA2_VC1BITPLANE_EXT_BUFFER, &CompBuf);

            bitplane_size = ((pContext->m_seqLayerHeader.heightMB +1)/2)*(pContext->m_seqLayerHeader.MaxWidthMB); //need to update for fields
            if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
            {
                bitplane_size /= 2;
                h = (pContext->m_seqLayerHeader.heightMB+1)/2;
            }

            for (i = 0; i < 3; i++)
            {
                if (!lut_bitplane[i]->m_databits)
                    lut_bitplane[i] = check_bitplane;
            }

            for (i = 0; i < h; i++)
            {
                for (j = 0; j < pContext->m_seqLayerHeader.MaxWidthMB/2; j++)
                {
                    *ptr = lut_bitplane[0]->m_databits[k] + (lut_bitplane[1]->m_databits[k] << 1) +
                        (lut_bitplane[2]->m_databits[k] << 2) + (lut_bitplane[0]->m_databits[k+1] << 4) +
                        (lut_bitplane[1]->m_databits[k+1] << 5) + (lut_bitplane[2]->m_databits[k+1] << 6);
                    k += 2;

                    ++ptr;
                }
                if(2*(pContext->m_seqLayerHeader.MaxWidthMB/2) < pContext->m_seqLayerHeader.MaxWidthMB)
                {
                    *ptr = 0;
                    *ptr = lut_bitplane[0]->m_databits[k] + (lut_bitplane[1]->m_databits[k] << 1) +
                        (lut_bitplane[2]->m_databits[k] << 2);
                    k++;

                    ptr++;

                }
            }
            CompBuf->SetDataSize(bitplane_size);
        }


    }

    void VC1PackerDXVA_EagleLake::VC1PackOneSlice  (VC1Context* ,
        SliceParams* slparams,
        uint32_t BufIndex, // only in future realisations
        uint32_t MBOffset,
        uint32_t SliceDataSize,
        uint32_t StartCodeOffset,
        uint32_t ChoppingType)
    {
        if (BufIndex)
            ++m_pSliceInfo;

        m_pSliceInfo->wHorizontalPosition = 0;
        m_pSliceInfo->wVerticalPosition = slparams->MBStartRow;
        m_pSliceInfo->dwSliceBitsInBuffer = SliceDataSize*8;
        m_pSliceInfo->wMBbitOffset = (WORD)((31-(BYTE)slparams->m_bitOffset) + MBOffset*8);
        m_pSliceInfo->dwSliceDataLocation = StartCodeOffset;
        m_pSliceInfo->bStartCodeBitOffset = 0;
        m_pSliceInfo->bReservedBits = 0;
        m_pSliceInfo->wQuantizerScaleCode = 0;
        m_pSliceInfo->wNumberMBsInSlice = slparams->MBStartRow;

        m_pSliceInfo->wBadSliceChopping = (uint16_t)ChoppingType;
    }


#endif//UMC_VA_DXVA
#ifdef UMC_VA_DXVA
    void VC1PackerDXVA_Protected::VC1SetExtPictureBuffer()
    {
        UMCVACompBuffer* CompBuf;
        m_pExtPicInfo = (DXVA_ExtPicInfo*)m_va->GetCompBuffer(DXVA2_VC1PICTURE_PARAMS_EXT_BUFFER, &CompBuf,sizeof(DXVA_ExtPicInfo));
        if (CompBuf->GetBufferSize() < sizeof(DXVA_ExtPicInfo))
            throw vc1_exception(mem_allocation_er);
    }

    void VC1PackerDXVA_Protected::VC1SetBuffersSize(uint32_t SliceBufIndex)
    {
        //_MAY_BE_
        UMCVACompBuffer* CompBuf;
        m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER,&CompBuf);
        CompBuf->SetDataSize((int32_t)(sizeof(DXVA_SliceInfo)*SliceBufIndex));
        CompBuf->SetNumOfItem(SliceBufIndex);

        m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER,&CompBuf);
        CompBuf->SetDataSize(sizeof(DXVA_PictureParameters));

        m_va->GetCompBuffer(DXVA2_VC1PICTURE_PARAMS_EXT_BUFFER,&CompBuf);
        CompBuf->SetDataSize(sizeof(DXVA_ExtPicInfo));
    }

    void VC1PackerDXVA_Protected::VC1PackExtPicParams (VC1Context* pContext,
        DXVA_ExtPicInfo* ptr,
        VideoAccelerator* )
    {
        memset(ptr, 0, sizeof(DXVA_ExtPicInfo));

        //BFraction // ADVANCE
        ptr->bBScaleFactor = (uint8_t)pContext->m_picLayerHeader->ScaleFactor;

        //PQUANT
        if(pContext->m_seqLayerHeader.QUANTIZER == 0 
            && pContext->m_picLayerHeader->PQINDEX > 8)
        {
            ptr->bPQuant = (pContext->m_picLayerHeader->PQINDEX < 29)
                ? (uint8_t)(pContext->m_picLayerHeader->PQINDEX - 3) 
                : (uint8_t)(pContext->m_picLayerHeader->PQINDEX*2 - 31);
        }
        else
        {
            ptr->bPQuant = (uint8_t)pContext->m_picLayerHeader->PQINDEX;
        }

        //ALTPQUANT
        ptr->bAltPQuant = (uint8_t)pContext->m_picLayerHeader->m_AltPQuant;

        //PictureFlags
        if(pContext->m_picLayerHeader->FCM == VC1_Progressive)
        {
            ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 0);  //progressive
        }
        else if(pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
        {
            ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 1);  //interlace fram
        }
        else
        {
            if(pContext->m_picLayerHeader->TFF)
            {
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 2);  //interlace field, top field first
            }
            else
            {
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 0, 2, 3);  //interlace field,bottom field first
            }
        }

        if(pContext->m_picLayerHeader->FCM == VC1_Progressive || pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
        {
            //progressive or interlece frame
            switch(pContext->m_picLayerHeader->PTYPE)
            {
            case VC1_I_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 0);
                break;
            case VC1_P_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 1);
                break;
            case VC1_B_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 2);
                break;
            case VC1_BI_FRAME:
                ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 3);
                break;
            default:
                if (VC1_IS_SKIPPED(pContext->m_picLayerHeader->PTYPE))
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 4);
                else
                    VM_ASSERT(0);
                break;
            }
        }
        else
        {
            //field picture
            switch(pContext->m_picLayerHeader->PTypeField1)
            {
            case VC1_I_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_I_FRAME)
                {
                    //I/I field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 0);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_P_FRAME)
                {
                    //I/P field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 1);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            case VC1_P_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_P_FRAME)
                {
                    //P/P field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 3);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_I_FRAME)
                {
                    //P/I field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 2);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            case VC1_B_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_B_FRAME)
                {
                    //B/B field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 4);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_BI_FRAME)
                {
                    //B/BI field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 5);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            case VC1_BI_FRAME:
                if(pContext->m_picLayerHeader->PTypeField2 == VC1_B_FRAME)
                {
                    //BI/B field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 6);
                }
                else if(pContext->m_picLayerHeader->PTypeField2 == VC1_BI_FRAME)
                {
                    //BI/BI field
                    ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 2, 3, 7);
                }
                else
                {
                    VM_ASSERT(0);
                }
                break;
            default:
                VM_ASSERT(0);
                break;
            }
        }

        ptr->bPictureFlags = bit_set(ptr->bPictureFlags, 5, 2, pContext->m_picLayerHeader->CONDOVER);

        //PQuantFlags
        if(pContext->m_picLayerHeader->QuantizationType == VC1_QUANTIZER_UNIFORM)
        {
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 0, 1, 1);
        }
        else
        {
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 0, 1, 0);
        }

        ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 1, 1, pContext->m_picLayerHeader->HALFQP);

        switch(pContext->m_picLayerHeader->m_PQuant_mode)
        {
        case VC1_ALTPQUANT_NO:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 0);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 0);
            break;
        case VC1_ALTPQUANT_MB_LEVEL:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 3);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 0);
            break;
        case VC1_ALTPQUANT_ANY_VALUE:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 2);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 0);
            break;
        case VC1_ALTPQUANT_LEFT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 1);
            break;
        case VC1_ALTPQUANT_TOP:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 2);
            break;
        case VC1_ALTPQUANT_RIGTHT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 4);
            break;
        case VC1_ALTPQUANT_BOTTOM:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 8);
            break;
        case VC1_ALTPQUANT_LEFT_TOP:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 3);
            break;
        case VC1_ALTPQUANT_TOP_RIGTHT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 6);
            break;
        case VC1_ALTPQUANT_RIGTHT_BOTTOM:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 12);
            break;
        case VC1_ALTPQUANT_BOTTOM_LEFT:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 9);
            break;
        case VC1_ALTPQUANT_EDGES:
        case VC1_ALTPQUANT_ALL:
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 2, 2, 1);
            ptr->bPQuantFlags = bit_set(ptr->bPQuantFlags, 4, 4, 15);
            break;
        default:
            VM_ASSERT(0);
            break;
        }

        //MvRange

        if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_NONE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 0);
        }
        else if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_HORIZONTAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 1);
        }
        else if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 2);
        }
        else if(pContext->m_picLayerHeader->MVRANGE == VC1_DMVRANGE_HORIZONTAL_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 0, 2, 3);
        }
        else
        {
            VM_ASSERT(0);
        }

        if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_NONE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 0);
        }
        else if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_HORIZONTAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 1);
        }
        else if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 2);
        }
        else if(pContext->m_picLayerHeader->DMVRANGE == VC1_DMVRANGE_HORIZONTAL_VERTICAL_RANGE)
        {
            ptr->bMvRange = bit_set(ptr->bMvRange, 2, 2, 3);
        }
        else
        {
            VM_ASSERT(0);
        }

        //wMvReference

        uint32_t FREFDIST = ((pContext->m_picLayerHeader->ScaleFactor * pContext->m_picLayerHeader->REFDIST) >> 8);
        int32_t BREFDIST = pContext->m_picLayerHeader->REFDIST - FREFDIST - 1;

        if (BREFDIST < 0)
            BREFDIST = 0;

        if(pContext->m_picLayerHeader->FCM == VC1_FieldInterlace && VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
            ptr->wMvReference = bit_set(ptr->wMvReference, 0, 4, FREFDIST);
        else
            ptr->wMvReference = bit_set(ptr->wMvReference, 0, 4, pContext->m_picLayerHeader->REFDIST);

        ptr->wMvReference = bit_set(ptr->wMvReference, 4, 4, BREFDIST);

        ptr->wMvReference = bit_set(ptr->wMvReference, 8, 1, pContext->m_picLayerHeader->NUMREF);
        ptr->wMvReference = bit_set(ptr->wMvReference, 9, 1, pContext->m_picLayerHeader->REFFIELD);
        ptr->wMvReference = bit_set(ptr->wMvReference, 10, 1, pContext->m_seqLayerHeader.FASTUVMC);
        ptr->wMvReference = bit_set(ptr->wMvReference, 11, 1, pContext->m_picLayerHeader->MV4SWITCH);

        switch(pContext->m_picLayerHeader->MVMODE)
        {
        case VC1_MVMODE_HPELBI_1MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 3);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
        case VC1_MVMODE_1MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 1);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
        case VC1_MVMODE_MIXED_MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 0);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
        case VC1_MVMODE_HPEL_1MV:
            ptr->wMvReference = bit_set(ptr->wMvReference, 12, 2, 2);
            ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
            break;
        default:
            VM_ASSERT(0);
            break;
        }

        if(pContext->m_bIntensityCompensation)
        {    
            switch(pContext->m_picLayerHeader->INTCOMFIELD)
            {
            case VC1_INTCOMP_TOP_FIELD:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 1);
                break;
            case VC1_INTCOMP_BOTTOM_FIELD:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 2);
                break;
            case VC1_INTCOMP_BOTH_FIELD:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 3);
                break;
            default:
                ptr->wMvReference = bit_set(ptr->wMvReference, 14, 2, 0);
                break;
            }
        }

        //wTransformFlags
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 0, 3, pContext->m_picLayerHeader->CBPTAB);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 3, 1, pContext->m_picLayerHeader->TRANSDCTAB);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 4, 2, pContext->m_picLayerHeader->TRANSACFRM);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 6, 2, pContext->m_picLayerHeader->TRANSACFRM2);


        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 8, 3, pContext->m_picLayerHeader->MBMODETAB);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 11, 1, pContext->m_picLayerHeader->TTMBF);
        ptr->wTransformFlags = bit_set(ptr->wTransformFlags, 12, 2, pContext->m_picLayerHeader->TTFRM_ORIG);

        //MvTableFlags
        ptr->bMvTableFlags = bit_set(ptr->bMvTableFlags, 0, 2, pContext->m_picLayerHeader->MV2BPTAB);
        ptr->bMvTableFlags = bit_set(ptr->bMvTableFlags, 2, 2, pContext->m_picLayerHeader->MV4BPTAB);
        ptr->bMvTableFlags = bit_set(ptr->bMvTableFlags, 4, 3, pContext->m_picLayerHeader->MVTAB);

        //RawCodingFlag
        ptr->bRawCodingFlag = 0;
        VC1Bitplane* check_bitplane = 0;
        VC1Bitplane* lut_bitplane[3];


        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
            lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
            lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
            break;
        case VC1_P_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
            break;
        case VC1_B_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
            break;
        default:
            return;
        }

        for (uint32_t i = 0; i < 3; i++)
        {
            if ((!VC1_IS_BITPLANE_RAW_MODE(lut_bitplane[i])) && lut_bitplane[i]->m_databits)
                check_bitplane = lut_bitplane[i];

        }
        if (!check_bitplane) // no bitplanes can return
        {
            return;
        }
        else
            ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 7, 1, 1);


        if (VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE ||
            VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FIELDTX)&&
                pContext->m_picLayerHeader->FIELDTX.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 0, 1, 1);
            }
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->ACPRED) &&
                pContext->m_picLayerHeader->ACPRED.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 1, 1, 1);
            }
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->OVERFLAGS)&&
                pContext->m_picLayerHeader->OVERFLAGS.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 2, 1, 1);
            }
        }

        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE ||
            VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
        {

            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->m_DirectMB)&&
                pContext->m_picLayerHeader->m_DirectMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 3, 1, 1);
            }
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->SKIPMB)&& 
                pContext->m_picLayerHeader->SKIPMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 4, 1, 1);
            }
        }

        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->MVTYPEMB)&&
                pContext->m_picLayerHeader->MVTYPEMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 5, 1, 1);
            }
        }

        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if(VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FORWARDMB)&&
                pContext->m_picLayerHeader->FORWARDMB.m_databits)
            {
                ptr->bRawCodingFlag = bit_set(ptr->bRawCodingFlag, 6, 1, 1);
            }
        }

    }

    void VC1PackerDXVA_Protected::VC1PackPicParams (VC1Context* pContext,
        DXVA_PictureParameters* ptr,
        VideoAccelerator*              va)
    {
        memset(ptr, 0, sizeof(DXVA_PictureParameters));
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {
            ptr->wPicHeightInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_HEIGHT + 1) - 1);
            ptr->wPicWidthInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_WIDTH + 1) - 1);
        }
        else
        {
            ptr->wPicHeightInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_HEIGHT + 1) - 1)/16;
            ptr->wPicWidthInMBminus1 = (WORD)(2*(pContext->m_seqLayerHeader.CODED_WIDTH + 1) - 1)/16;
        }

        ptr->bMacroblockWidthMinus1  = 15;
        ptr->bMacroblockHeightMinus1 = 15;
        ptr->bBlockWidthMinus1       = 7;
        ptr->bBlockHeightMinus1      = 7;
        ptr->bBPPminus1              = 7;

        // interlace fields
        if (VC1_FieldInterlace == pContext->m_picLayerHeader->FCM)
        {
            ptr->bPicExtrapolation = 0x2;
            if (pContext->m_picLayerHeader->CurrField == 0)
            {
                if (pContext->m_seqLayerHeader.PULLDOWN)
                {
                    if (pContext->m_picLayerHeader->TFF)
                        ptr->bPicStructure = 1;
                    else
                        ptr->bPicStructure = 2;
                }
                else
                    ptr->bPicStructure = 1;
            }
            else
            {
                if (pContext->m_picLayerHeader->TFF)
                    ptr->bPicStructure = 2;
                else
                    ptr->bPicStructure = 1;
            }
        }
        else  // frames
        {
            ptr->bPicStructure = 0x03;    //Frame
            if (VC1_Progressive == pContext->m_picLayerHeader->FCM)
                ptr->bPicExtrapolation = 1; //Progressive
            else
                ptr->bPicExtrapolation = 2; //Interlace
        }

        ptr->bSecondField = pContext->m_picLayerHeader->CurrField;
        ptr->bPicIntra = 0;
        ptr->bBidirectionalAveragingMode = 0x80;

        if (VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicIntra = 1;
        }
        else if  (VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicBackwardPrediction = 1;
            ptr->bPicIntra = 1;
        }
        else if (VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPicBackwardPrediction = 1;
        }

        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
            ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,3,1,1);

        // intensity compensation
        if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            if (pContext->m_bIntensityCompensation && (pContext->m_picLayerHeader->INTCOMFIELD
                || (ptr->bPicStructure==0x3)))
                ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,4,1,1);

            if (pContext->m_bIntensityCompensation && 
                (VC1_PROFILE_ADVANCED != pContext->m_seqLayerHeader.PROFILE))
                ptr->bBidirectionalAveragingMode = bit_set(ptr->bBidirectionalAveragingMode,4,1,1);

        }

        //bicubic as default
        ptr->bMVprecisionAndChromaRelation = 0x04;
        if (VC1_IS_PRED(pContext->m_picLayerHeader->PTYPE)&&
            (pContext->m_picLayerHeader->FCM != VC1_FrameInterlace))
        {
            if (VC1_MVMODE_HPELBI_1MV == pContext->m_picLayerHeader->MVMODE)
                ptr->bMVprecisionAndChromaRelation |= 0x08;

        }

        if (pContext->m_seqLayerHeader.FASTUVMC)
            ptr->bMVprecisionAndChromaRelation |= 0x01;

        // 4:2:0
        ptr->bChromaFormat = 1;
        ptr->bPicScanFixed  = 1;
        ptr->bPicScanMethod = 0;
        ptr->bPicReadbackRequests = 1;

        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
            ptr->bRcontrol = (BYTE)(pContext->m_picLayerHeader->RNDCTRL);
        else
            ptr->bRcontrol = (BYTE)(pContext->m_seqLayerHeader.RNDCTRL);

        ptr->bPicDeblocked = 0;

        // should be zero

        if (pContext->m_seqLayerHeader.LOOPFILTER &&
            pContext->DeblockInfo.isNeedDbl)
        {
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 1, 1, 1);
        }

        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {

            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 2, 1, pContext->m_picLayerHeader->POSTPROC);
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 3, 1, pContext->m_picLayerHeader->POSTPROC);
            if (VC1_B_FRAME != pContext->m_picLayerHeader->PTYPE)
                ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 6, 1, pContext->m_seqLayerHeader.OVERLAP); // overlap
        }
        else
        {
            if(pContext->DeblockInfo.isNeedDbl)
                ptr->bPicDeblocked = 2;

            if ((pContext->m_picLayerHeader->PQUANT >= 9) && (VC1_B_FRAME != pContext->m_picLayerHeader->PTYPE))
                ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked, 6, 1, pContext->m_seqLayerHeader.OVERLAP); // overlap
        }


        //range reduction
        if (VC1_PROFILE_MAIN == pContext->m_seqLayerHeader.PROFILE &&
            pContext->m_seqLayerHeader.RANGERED)
        {
            ptr->bPicDeblocked = bit_set(ptr->bPicDeblocked,5,1,(pContext->m_picLayerHeader->RANGEREDFRM>>3));
        }

        ptr->bPicDeblockConfined = 0;
        if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
        {
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 7, 1, pContext->m_seqLayerHeader.POSTPROCFLAG);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 6, 1, pContext->m_seqLayerHeader.PULLDOWN);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 5, 1, pContext->m_seqLayerHeader.INTERLACE);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 4, 1, pContext->m_seqLayerHeader.TFCNTRFLAG);
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 3, 1, pContext->m_seqLayerHeader.FINTERPFLAG);

            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 2, 1, 1);

            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 1, 1, 0); // PSF _MAY_BE need
            ptr->bPicDeblockConfined = bit_set(ptr->bPicDeblockConfined, 0, 1, pContext->m_seqLayerHeader.EXTENDED_DMV);
        }

        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 7, 1, pContext->m_seqLayerHeader.PANSCAN_FLAG);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 6, 1, pContext->m_seqLayerHeader.REFDIST_FLAG);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 5, 1, pContext->m_seqLayerHeader.LOOPFILTER);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 4, 1, pContext->m_seqLayerHeader.FASTUVMC);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 3, 1, pContext->m_seqLayerHeader.EXTENDED_MV);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 1, 2, pContext->m_seqLayerHeader.DQUANT);
        ptr->bPicSpatialResid8 = bit_set(ptr->bPicSpatialResid8, 0, 1, pContext->m_seqLayerHeader.VSTRANSFORM);

        ptr->bPicOverflowBlocks = bit_set(ptr->bPicOverflowBlocks, 6, 2, pContext->m_seqLayerHeader.QUANTIZER);

        ptr->bPic4MVallowed = 0;
        if (VC1_FrameInterlace == pContext->m_picLayerHeader->FCM &&
            1 == pContext->m_picLayerHeader->MV4SWITCH &&
            VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->bPic4MVallowed = 1;
        }
        else if (VC1_IS_PRED(pContext->m_picLayerHeader->PTYPE))
        {
            if (VC1_MVMODE_MIXED_MV == pContext->m_picLayerHeader->MVMODE)
                ptr->bPic4MVallowed = 1;
        }

        ptr->wDecodedPictureIndex = (WORD)(pContext->m_frmBuff.m_iCurrIndex);
        ptr->wDeblockedPictureIndex  = ptr->wDecodedPictureIndex;

        if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ||
            pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG || 
            pContext->m_seqLayerHeader.RANGERED)
        {

            if(VC1_IS_REFERENCE(pContext->m_picLayerHeader->PTYPE))
            {
                va->BeginFrame(pContext->m_frmBuff.m_iRangeMapIndex);
                ptr->wDeblockedPictureIndex = (WORD)(pContext->m_frmBuff.m_iRangeMapIndex);
            }
            else
            {
                va->BeginFrame(pContext->m_frmBuff.m_iRangeMapIndex);
                ptr->wDeblockedPictureIndex = (WORD)(pContext->m_frmBuff.m_iRangeMapIndexPrev);
            }
        }

        if (1 == ptr->bPicIntra)
        {
            ptr->wForwardRefPictureIndex =  0xFFFF;
            ptr->wBackwardRefPictureIndex = 0xFFFF;
        }
        else if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            ptr->wBackwardRefPictureIndex = 0xFFFF;
            if (VC1_FieldInterlace != pContext->m_picLayerHeader->FCM ||
                0 == pContext->m_picLayerHeader->CurrField)
            {
                ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
            }
            else
            {
                if (1 == pContext->m_picLayerHeader->NUMREF)
                {
                    // first frame MAY_BE. Need to check
                    if (-1 != pContext->m_frmBuff.m_iPrevIndex)
                        ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
                    else
                        ptr->wForwardRefPictureIndex = 0;

                }
                else if (1 == pContext->m_picLayerHeader->REFFIELD)
                {
                    ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
                }
                else
                {
                    ptr->wForwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iCurrIndex);
                }
            }
        }
        else
        {
            ptr->wForwardRefPictureIndex  = (WORD)(pContext->m_frmBuff.m_iPrevIndex);
            ptr->wBackwardRefPictureIndex = (WORD)(pContext->m_frmBuff.m_iNextIndex);
        }

        ptr->bPicOBMC = 0;
        if (pContext->m_seqLayerHeader.RANGE_MAPY_FLAG)
        {
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 7, 1, pContext->m_seqLayerHeader.RANGE_MAPY_FLAG);
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 4, 3, pContext->m_seqLayerHeader.RANGE_MAPY);

        }
        if (pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG)
        {
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 3, 1,pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG);
            ptr->bPicOBMC = bit_set(ptr->bPicOBMC, 0, 3, pContext->m_seqLayerHeader.RANGE_MAPUV);
        }

        ptr->bPicBinPB = 0; // RESPIC - need to add
        ptr->bMV_RPS = 0;
        ptr->bReservedBits = BYTE(pContext->m_picLayerHeader->PQUANT);

        ptr->wBitstreamFcodes = 0;
        ptr->wBitstreamPCEelements = 0;

        if (pContext->m_bIntensityCompensation)
        {
            if (3 == ptr->bPicStructure)
            {
                ptr->wBitstreamFcodes = WORD(pContext->m_picLayerHeader->LUMSCALE);
                ptr->wBitstreamPCEelements = WORD(pContext->m_picLayerHeader->LUMSHIFT);
            }
            else
            {
                if (3 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->m_picLayerHeader->LUMSCALE);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->m_picLayerHeader->LUMSHIFT);

                    //bottom field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 0, 8, pContext->m_picLayerHeader->LUMSCALE1);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 0, 8, pContext->m_picLayerHeader->LUMSHIFT1);                    
                }
                else if (2 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // bottom field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 0, 8, pContext->m_picLayerHeader->LUMSCALE1);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 0, 8, pContext->m_picLayerHeader->LUMSHIFT1);
                    // top field not compensated
                    ptr->wBitstreamFcodes |= (32<<8);
                }
                else if (1 == pContext->m_picLayerHeader->INTCOMFIELD)
                {
                    // top field
                    ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, pContext->m_picLayerHeader->LUMSCALE);
                    ptr->wBitstreamPCEelements = bit_set(ptr->wBitstreamPCEelements, 8, 8, pContext->m_picLayerHeader->LUMSHIFT);
                    // bottom field not compensated
                    ptr->wBitstreamFcodes |= 32;
                }
            }
        }
        else
        {
            ptr->wBitstreamFcodes = 32;
            if (ptr->bPicStructure != 3)
            {
                ptr->wBitstreamFcodes = bit_set(ptr->wBitstreamFcodes, 8, 8, 32);
            }
        }
        ptr->bBitstreamConcealmentNeed = 0;
        ptr->bBitstreamConcealmentMethod = 0;
    }

    void VC1PackerDXVA_Protected::VC1PackBitplaneBuffers(VC1Context* pContext)
    {
        UMCVACompBuffer* CompBuf;

        uint8_t* ptr = (uint8_t*)m_va->GetCompBuffer(DXVA2_VC1BITPLANE_EXT_BUFFER, &CompBuf);

        int32_t i = 0;
        int32_t j = 0;
        int32_t k = 0;

        int32_t h = pContext->m_seqLayerHeader.heightMB;

        VC1Bitplane* lut_bitplane[3] = {0, 0, 0};
        VC1Bitplane* check_bitplane = 0;

        int32_t bitplane_size = 0;

        switch (pContext->m_picLayerHeader->PTYPE)
        {
        case VC1_I_FRAME:
        case VC1_BI_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->FIELDTX;
            lut_bitplane[1] = &pContext->m_picLayerHeader->ACPRED;
            lut_bitplane[2] = &pContext->m_picLayerHeader->OVERFLAGS;
            break;
        case VC1_P_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->MVTYPEMB;
            break;
        case VC1_B_FRAME:
            lut_bitplane[0] = &pContext->m_picLayerHeader->m_DirectMB;
            lut_bitplane[1] = &pContext->m_picLayerHeader->SKIPMB;
            lut_bitplane[2] = &pContext->m_picLayerHeader->FORWARDMB;
            break;
        }

        if (VC1_IS_SKIPPED(pContext->m_picLayerHeader->PTYPE))
            return;

        for (i = 0; i < 3; i++)
        {
            if ((!VC1_IS_BITPLANE_RAW_MODE(lut_bitplane[i])) && lut_bitplane[i]->m_databits)
                check_bitplane = lut_bitplane[i];

        }

        if (check_bitplane)
        {
            ptr = (uint8_t*)m_va->GetCompBuffer(DXVA2_VC1BITPLANE_EXT_BUFFER, &CompBuf);
            bitplane_size = (pContext->m_seqLayerHeader.heightMB+1)*(pContext->m_seqLayerHeader.widthMB)/2; //need to update for fields
            if (pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
            {
                bitplane_size /= 2;
                h = (pContext->m_seqLayerHeader.heightMB+1)/2;
            }

            for (i = 0; i < 3; i++)
            {
                if (!lut_bitplane[i]->m_databits)
                    lut_bitplane[i] = check_bitplane;
            }

            for (i = 0; i < h; i++)
            {
                for (j = 0; j < pContext->m_seqLayerHeader.widthMB/2; j++)
                {
                    *ptr = lut_bitplane[0]->m_databits[k] + (lut_bitplane[1]->m_databits[k] << 1) +
                        (lut_bitplane[2]->m_databits[k] << 2) + (lut_bitplane[0]->m_databits[k+1] << 4) +
                        (lut_bitplane[1]->m_databits[k+1] << 5) + (lut_bitplane[2]->m_databits[k+1] << 6);
                    k += 2;

                    ++ptr;
                }
                if(2*(pContext->m_seqLayerHeader.widthMB/2) < pContext->m_seqLayerHeader.widthMB)
                {
                    *ptr = 0;
                    *ptr = lut_bitplane[0]->m_databits[k] + (lut_bitplane[1]->m_databits[k] << 1) +
                        (lut_bitplane[2]->m_databits[k] << 2);
                    k++;

                    ptr++;

                }
            }
            CompBuf->SetDataSize(bitplane_size);
        }
    }

    void VC1PackerDXVA_Protected::VC1PackOneSlice  (VC1Context* ,
        SliceParams* slparams,
        uint32_t BufIndex, // only in future realisations
        uint32_t MBOffset,
        uint32_t SliceDataSize,
        uint32_t StartCodeOffset,
        uint32_t ChoppingType)
    {
        if (BufIndex)
            ++m_pSliceInfo;

        m_pSliceInfo->wHorizontalPosition = 0;
        m_pSliceInfo->wVerticalPosition = slparams->MBStartRow;
        m_pSliceInfo->dwSliceBitsInBuffer = SliceDataSize*8;
        m_pSliceInfo->wMBbitOffset = (WORD)((31-(BYTE)slparams->m_bitOffset) + MBOffset*8);
        m_pSliceInfo->dwSliceDataLocation = StartCodeOffset;
        m_pSliceInfo->bStartCodeBitOffset = 0;
        m_pSliceInfo->bReservedBits = 0;
        m_pSliceInfo->wQuantizerScaleCode = 0;
        m_pSliceInfo->wNumberMBsInSlice = slparams->MBStartRow;

        m_pSliceInfo->wBadSliceChopping = (uint16_t)ChoppingType;
    }


    uint32_t VC1PackerDXVA_Protected::VC1PackBitStreamAdv (VC1Context* pContext,
        uint32_t& Size,
        uint8_t* pOriginalData,
        uint32_t OriginalSize,
        uint32_t ByteOffset,
        uint8_t& Flag_03)
    {
        UMCVACompBuffer* CompBuf;

        uint8_t* pBitstream = (uint8_t*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);


        uint32_t DrvBufferSize = CompBuf->GetBufferSize();
        uint32_t RemainBytes = 0;

        Size = OriginalSize;

        if (DrvBufferSize < (OriginalSize + ByteOffset)) // we don't have enough buffer
        {
            RemainBytes = OriginalSize + ByteOffset - DrvBufferSize;
            Size = DrvBufferSize;
        }

        if(Flag_03 == 1)
        {
            *(pBitstream + ByteOffset) = 0;
            MFX_INTERNAL_CPY(pBitstream + ByteOffset + 1, pOriginalData + 1, OriginalSize - 1 - RemainBytes);
            Flag_03 = 0;
        }
        else if(Flag_03 == 2)
        {
            if (pContext->m_bitstream.bitOffset < 24)
            {
                MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData + 1, OriginalSize - RemainBytes + 1);
                *(pBitstream + ByteOffset + 1) = 0;
                Size--;
            }
            else
            {
                MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData, OriginalSize - RemainBytes);
            }
            Flag_03 = 0;
        }
        else if(Flag_03 == 3)
        {
            if(pContext->m_bitstream.bitOffset < 16)
            {
                *(pBitstream + ByteOffset) = *pOriginalData;
                *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
                *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
                *(pBitstream + ByteOffset + 3) = *(pOriginalData + 4);
                MFX_INTERNAL_CPY(pBitstream + ByteOffset + 4, pOriginalData + 5, OriginalSize - 4 - RemainBytes);
                Size--;
            }
            else
            {
                MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData, OriginalSize - RemainBytes);
            }

            Flag_03 = 0;
        }
        else if(Flag_03 == 4)
        {
            MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData, OriginalSize - RemainBytes);

            Flag_03 = 0;
        }
        else if(Flag_03 == 5)
        {
            *(pBitstream + ByteOffset) = 0;
            *(pBitstream + ByteOffset + 1) = *(pOriginalData + 1);
            *(pBitstream + ByteOffset + 2) = *(pOriginalData + 2);
            MFX_INTERNAL_CPY(pBitstream + ByteOffset + 3, pOriginalData + 4, OriginalSize - 4 - RemainBytes);

            Size--;
            Flag_03 = 0;
        }
        else
        {
            MFX_INTERNAL_CPY(pBitstream + ByteOffset, pOriginalData, OriginalSize - RemainBytes);
        }

        CompBuf->SetDataSize(ByteOffset + Size);
        return RemainBytes;
    }

#endif//UMC_VA_DXVA
}
#endif //MFX_ENABLE_VC1_VIDEO_DECODE


