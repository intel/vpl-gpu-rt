// Copyright (c) 2022 Intel Corporation
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

#include "mfx_pxp_vvc_nal_spl.h"

#if defined(MFX_ENABLE_PXP)

namespace UMC_VVC_DECODER
{
    UMC::MediaDataEx* PXPNALUnitSplitter_VVC::GetNalUnits(UMC::MediaData* pSource)
    {
        UMC::MediaDataEx* nalUnit = Splitter_VVC::GetNalUnits(pSource);

        if (nalUnit && m_va && m_va->GetProtectedVA() && static_cast<UMC::PXPVA*>(m_va->GetProtectedVA())->GetPXPParams())
        {
            UMC::MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();
            NalUnitType unitType = (NalUnitType)pMediaDataEx->values[pMediaDataEx->index];
            if ( NAL_UNIT_CODED_SLICE_TRAIL == unitType
              || NAL_UNIT_CODED_SLICE_STSA == unitType
              || NAL_UNIT_CODED_SLICE_RADL == unitType
              || NAL_UNIT_CODED_SLICE_RASL == unitType
              || NAL_UNIT_CODED_SLICE_IDR_W_RADL == unitType
              || NAL_UNIT_CODED_SLICE_IDR_N_LP == unitType
              || NAL_UNIT_CODED_SLICE_CRA == unitType
              || NAL_UNIT_CODED_SLICE_GDR == unitType
            )
            {
                UMC_CHECK(UMC::UMC_ERR_INVALID_PARAMS != MergeEncryptedNalUnit(nalUnit, pSource), nullptr);
            }
        }

        return nalUnit;
    }

    UMC::Status PXPNALUnitSplitter_VVC::MergeEncryptedNalUnit(UMC::MediaDataEx* nalUnit, UMC::MediaData* pSource)
    {
        UMC_CHECK(nalUnit != nullptr, UMC::UMC_ERR_INVALID_PARAMS);
        UMC_CHECK(pSource != nullptr, UMC::UMC_ERR_INVALID_PARAMS);

        uint32_t cur_segment_length = 0;
        uint32_t start_code_length = 3;
        UMC::PXPVA* pxpva = static_cast<UMC::PXPVA*>(m_va->GetProtectedVA());

        cur_segment_length = (static_cast<VAEncryptionParameters*>(pxpva->GetPXPParams())->segment_info + pxpva->m_curSegment)->segment_length - start_code_length;

        while (cur_segment_length != nalUnit->GetBufferSize())
        {
            // middleclr mode has multi segments in one slice
            if (cur_segment_length < nalUnit->GetBufferSize())
            {
                pxpva->m_curSegment++;

                cur_segment_length += (static_cast<VAEncryptionParameters*>(pxpva->GetPXPParams())->segment_info + pxpva->m_curSegment)->segment_length;
            }
            // additional NAL after encryption
            else if (cur_segment_length > nalUnit->GetBufferSize())
            {
                // additional 00 at the end of NAL
                uint8_t* end_nal_ptr = (uint8_t*)nalUnit->GetBufferPointer() + nalUnit->GetBufferSize();
                uint32_t end_nal_index = 0;

                uint32_t currSegment = pxpva->m_curSegment;
                uint32_t ext_buf_size = cur_segment_length - nalUnit->GetBufferSize();
                uint8_t extEndByte = 0;
                uint32_t indexNewSeg = 0;
                while (end_nal_index < ext_buf_size)
                {
                    extEndByte = *end_nal_ptr;
                    if(extEndByte != 0)
                    {
                        break;
                    }
                    end_nal_ptr++;
                    end_nal_index++;
                    indexNewSeg++;
                    if(end_nal_index == ext_buf_size)
                    {
                        ++currSegment;
                        indexNewSeg = 0;
                        uint32_t numSegments = static_cast<VAEncryptionParameters*>(pxpva->GetPXPParams())->num_segments;
                        if(currSegment < numSegments)
                        {
                            ext_buf_size += (static_cast<VAEncryptionParameters*>(pxpva->GetPXPParams())->segment_info + currSegment)->segment_length;
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                if (extEndByte != 1 || (currSegment > pxpva->m_curSegment && indexNewSeg > 1))
                {
                    nalUnit->SetBufferPointer((uint8_t*)nalUnit->GetBufferPointer(), cur_segment_length);
                    nalUnit->SetDataSize(cur_segment_length);

                    pxpva->m_curSegment++;
                    return UMC::UMC_OK;
                }

                // additional start code, read next NAL
                uint8_t* prev_buffer_ptr = (uint8_t*)nalUnit->GetBufferPointer();
                uint32_t prev_buffer_size = nalUnit->GetBufferSize();
                UMC::MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();
                int32_t prev_iCode = pMediaDataEx->values[0];

                nalUnit = Splitter_VVC::GetNalUnits(pSource);
                if (!nalUnit)
                {
                    return UMC::UMC_ERR_NULL_PTR;
                }

                uint32_t newBufferSize = prev_buffer_size + 3 + nalUnit->GetBufferSize();
                nalUnit->SetBufferPointer(prev_buffer_ptr, newBufferSize);
                nalUnit->SetDataSize(newBufferSize);
                pMediaDataEx->values[0] = prev_iCode;
                pMediaDataEx->offsets[1] = newBufferSize;
            }
        }
        pxpva->m_curSegment++;

        return UMC::UMC_OK;
    }

}
#endif // MFX_ENABLE_PXP
#endif // MFX_ENABLE_VVC_VIDEO_DECODE
