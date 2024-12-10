// Copyright (c) 2008-2021 Intel Corporation
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

#include "mfx_pxp_h264_nal_spl.h"

#if defined(MFX_ENABLE_PXP)

namespace UMC
{
    NalUnit* PXPNALUnitSplitter::GetNalUnits(MediaData* pSource)
    {
        NalUnit* nalUnit = NALUnitSplitter::GetNalUnits(pSource);

        if (nalUnit && m_va && m_va->GetProtectedVA() && static_cast<PXPVA*>(m_va->GetProtectedVA())->GetPXPParams())
        {
            NAL_Unit_Type nalUnitType = (NAL_Unit_Type)nalUnit->GetNalUnitType();
            if (NAL_UT_IDR_SLICE == nalUnitType
                || NAL_UT_SLICE == nalUnitType
                || NAL_UT_AUXILIARY == nalUnitType
                || NAL_UT_CODED_SLICE_EXTENSION == nalUnitType)
            {
                UMC_CHECK(UMC_ERR_INVALID_PARAMS != MergeEncryptedNalUnit(nalUnit, pSource), nullptr);
            }
        }

        return nalUnit;
    }

    Status PXPNALUnitSplitter::MergeEncryptedNalUnit(NalUnit* nalUnit, MediaData* pSource)
    {
        UMC_CHECK(nalUnit != nullptr, UMC_ERR_INVALID_PARAMS);
        UMC_CHECK(pSource != nullptr, UMC_ERR_INVALID_PARAMS);

        uint32_t cur_segment_length = 0;
        uint32_t start_code_length = 3;
        PXPVA* pxpva = static_cast<PXPVA*>(m_va->GetProtectedVA());

        cur_segment_length = (static_cast<VAEncryptionParameters*>(pxpva->GetPXPParams())->segment_info + pxpva->m_curSegment)->segment_length;

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
                    return UMC_OK;
                }

                // additional start code, read next NAL
                uint8_t* prev_buffer_ptr = (uint8_t*)nalUnit->GetBufferPointer();
                uint32_t prev_buffer_size = nalUnit->GetBufferSize();
                int prev_nal_unit_type = nalUnit->m_nal_unit_type;

                nalUnit = NALUnitSplitter::GetNalUnits(pSource);
                if (!nalUnit)
                {
                    return UMC_ERR_NULL_PTR;
                }

                uint32_t newBufferSize = prev_buffer_size + 3 + nalUnit->GetBufferSize();
                nalUnit->SetBufferPointer(prev_buffer_ptr, newBufferSize);
                nalUnit->SetDataSize(newBufferSize);
                nalUnit->m_nal_unit_type = prev_nal_unit_type;
            }
        }
        pxpva->m_curSegment++;

        return UMC_OK;
    }

}
#endif // MFX_ENABLE_PXP