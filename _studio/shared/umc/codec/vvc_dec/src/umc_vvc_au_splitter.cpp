// Copyright (c) 2022-2023 Intel Corporation
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

#include "umc_media_data.h"
#include "umc_vvc_au_splitter.h"

namespace UMC_VVC_DECODER
{
    // NAL unit definitions
    enum
    {
        NAL_UNITTYPE_SHIFT     = 3,
    };

    StartCodeSearcher::StartCodeSearcher()
        : m_pts(-1)
        , m_code(-1)
        , m_suggestedSize(10 * 1024)
    {
        m_mediaData.SetExData(&m_mediaDataEx);
        Reset();
    }

    UMC::MediaDataEx *StartCodeSearcher::LoadData(UMC::MediaData *source)
    {
        UMC::MediaDataEx * dst = &m_mediaData;
        UMC::MediaDataEx::_MediaDataEx* mediaDataEx = &m_mediaDataEx;

        int32_t nut = FindNalUnit(source, dst);

        if (nut == -1)
        {
            mediaDataEx->count = 0;
            return 0;
        }

        mediaDataEx->values[0]  = nut;
        mediaDataEx->offsets[0] = 0;
        mediaDataEx->offsets[1] = (int32_t)dst->GetDataSize();
        mediaDataEx->count      = 1;
        mediaDataEx->index      = 0;

        return dst;
    }

    int32_t StartCodeSearcher::FindNalUnit(UMC::MediaData *pSource, UMC::MediaData *pDst)
    {
        if (!pSource)
        {
            return OnEOS(pDst);
        }

        int32_t naluType = FindNalUnitInternal(pSource, pDst);
        if (naluType == -1)
        {
            bool endOfStream = pSource && ((pSource->GetFlags() & UMC::MediaData::FLAG_VIDEO_DATA_END_OF_STREAM) != 0);
            if (endOfStream)
            {
                naluType = OnEOS(pDst);
            }
        }

        return naluType;
    }

    int32_t StartCodeSearcher::MoveToStartCode(UMC::MediaData *pSource)
    {
        if (!pSource)
            return -1;

        if (m_code == -1)
        {
            m_cache.clear();
        }

        uint8_t * source = (uint8_t *)pSource->GetDataPointer();
        size_t  size = pSource->GetDataSize();

        int32_t startCodeSize;
        int32_t nut = FindStartCode(source, size, startCodeSize);
        pSource->MoveDataPointer((int32_t)(source - (uint8_t *)pSource->GetDataPointer()));

        if (nut != -1)
        {
             pSource->MoveDataPointer(-startCodeSize);
        }

        return nut;
    }

    int32_t StartCodeSearcher::OnEOS(UMC::MediaData *pDst)
    {
        if (m_code == -1)
        {
            m_cache.clear();
            return -1;
        }

        if (m_cache.size())
        {
            pDst->SetBufferPointer(&(m_cache[3]), m_cache.size() - 3);
            pDst->SetDataSize(m_cache.size() - 3);
            pDst->SetTime(m_pts);
            int32_t code = m_code;
            m_code = -1;
            m_pts = -1;
            return code;
        }

        m_code = -1;
        return -1;
    }

    int32_t StartCodeSearcher::FindStartCode(uint8_t *(&pBuf), size_t &size, int32_t &startCodeSize)
    {
        uint32_t numZeroBytes = 0;

        int32_t i = 0;
        for (; i < (int32_t)size - 2; )
        {
            if (pBuf[1])
            {
                pBuf += 2;
                i += 2;
                continue;
            }

            numZeroBytes = 0;
            if (!pBuf[0])
                numZeroBytes++;

            uint32_t j;
            for (j = 1; j < (uint32_t)size - i; j++)
            {
                if (pBuf[j])
                    break;
            }

            numZeroBytes = numZeroBytes ? j: j - 1;

            pBuf += j;
            i += j;

            if (i >= (int32_t)size)
            {
                break;
            }

            if (numZeroBytes >= 2 && pBuf[0] == 1)
            {
                startCodeSize = std::min(numZeroBytes + 1, 4u);
                size -= i + 1;
                pBuf++;    // remove 0x01 symbol
                if (size >= 1)
                {
                    return (pBuf[1] >> NAL_UNITTYPE_SHIFT); // get nal_unit_type
                }
                else
                {
                    pBuf -= startCodeSize;
                    size += startCodeSize;
                    startCodeSize = 0;
                    return -1;
                }
            }

            numZeroBytes = 0;
        }

        if (!numZeroBytes)
        {
            for (uint32_t k = 0; k < size - i; k++, pBuf++)
            {
                if (pBuf[0])
                {
                    numZeroBytes = 0;
                    continue;
                }

                numZeroBytes++;
            }
        }

        numZeroBytes = std::min(numZeroBytes, 3u);
        pBuf -= numZeroBytes;
        size = numZeroBytes;
        startCodeSize = numZeroBytes;
        return -1;
    }

    int32_t StartCodeSearcher::FindNalUnitInternal(UMC::MediaData *pSource, UMC::MediaData *pDst)
    {
        if (m_code == -1)
        {
            m_cache.clear();
        }

        uint8_t * source = (uint8_t *)pSource->GetDataPointer();
        size_t  size = pSource->GetDataSize();

        if (!size)
        {
            return -1;
        }

        int32_t startCodeSize;
        int32_t nut = FindStartCode(source, size, startCodeSize); // nut: nal_unit_type

        // Use start code which is saved from previous call because start code could be split between bs buffers from application
        if (!m_cache.empty())
        {
            if (nut == -1)
            {
                size_t szToAdd = source - (uint8_t *)pSource->GetDataPointer();
                size_t szToMove = szToAdd;
                if (m_cache.size() + szToAdd >  m_suggestedSize)
                {
                    szToAdd = (m_suggestedSize > m_cache.size()) ? m_suggestedSize - m_cache.size() : 0;
                }

                m_cache.insert(m_cache.end(), (uint8_t *)pSource->GetDataPointer(), (uint8_t *)pSource->GetDataPointer() + szToAdd);
                pSource->MoveDataPointer((int32_t)szToMove);
                return -1;
            }

            source -= startCodeSize;
            m_cache.insert(m_cache.end(), (uint8_t *)pSource->GetDataPointer(), source);
            pSource->MoveDataPointer((int32_t)(source - (uint8_t *)pSource->GetDataPointer()));

            pDst->SetFlags(UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME);
            pDst->SetBufferPointer(&(m_cache[3]), m_cache.size() - 3);
            pDst->SetDataSize(m_cache.size() - 3);
            pDst->SetTime(m_pts);
            int32_t code = m_code;
            m_code = -1;
            m_pts = -1;
            return code;
        }

        if (nut == -1)
        {
            pSource->MoveDataPointer((int32_t)(source - (uint8_t *)pSource->GetDataPointer()));
            return -1;
        }

        m_pts = pSource->GetTime();
        m_code = nut;

        // move data pointer to the front of start code
        pSource->MoveDataPointer((int32_t)(source - (uint8_t *)pSource->GetDataPointer() - startCodeSize));

        int32_t startCodeSizeNext;
        int32_t nutNext = FindStartCode(source, size, startCodeSizeNext);

        pSource->MoveDataPointer(startCodeSize); //TODO, double check if this operation is really needed. 
        uint32_t flags = pSource->GetFlags();

        if (nutNext == -1 && !(flags & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_UNIT))
        {
            nutNext = 1;
            startCodeSizeNext = 0;
            source += size;
            size = 0;
            if (!flags) // completeframe mode
            {
                MFX_LTRACE_MSG(MFX_TRACE_LEVEL_CRITICAL_INFO, "Incomplete bitstream will be sent to driver in completeframe mode");
            }
        }

        if (nutNext == -1)
        {
            if (m_code == NAL_UNIT_SPS)
            {
                pSource->MoveDataPointer(-startCodeSize); // leave start code for SPS
                return -1;
            }

            assert(!m_cache.size());

            size_t sz = source - (uint8_t *)pSource->GetDataPointer();
            size_t szToMove = sz;
            if (sz >  m_suggestedSize)
            {
                sz = m_suggestedSize;
            }

            if (!m_cache.size())
            {
                m_cache.insert(m_cache.end(), (uint8_t *)start_code_prefix, (uint8_t*)start_code_prefix + prefix_size);
            }
            m_cache.insert(m_cache.end(), (uint8_t *)pSource->GetDataPointer(), (uint8_t *)pSource->GetDataPointer() + sz);
            pSource->MoveDataPointer((int32_t)szToMove);
            return -1;
        }

        // set destination buffer pointer & data size
        size_t nal_size = source - (uint8_t *)pSource->GetDataPointer() - startCodeSizeNext;
        pDst->SetBufferPointer((uint8_t*)pSource->GetDataPointer(), nal_size);
        pDst->SetDataSize(nal_size);
        pDst->SetFlags(pSource->GetFlags());
        pSource->MoveDataPointer((int32_t)nal_size);

        int32_t code = m_code;
        m_code = -1;

        pDst->SetTime(m_pts);
        m_pts = -1;
        return code;
    }

    // Change memory region to little endian for reading with 32-bit DWORDs and remove start code emulation prevention bytes
    inline void SwapMemoryAndRemovePreventionBytes(void *pDestination, size_t &nDstSize, void *pSource, size_t nSrcSize, std::vector<uint32_t> *pRemovedOffsets)
    {
        VVCDstDwordPointer pDst;
        VVCSourcePointer pSrc;
        size_t i;

        // VVCDstDwordPointer object is swapping written bytes
        // VVCSourcePointer removes emu-prevention start-code bytes

        // reset pointer(s)
        pSrc = pSource;
        pDst = pDestination;

        // first two bytes
        i = 0;
        while (i < std::min(size_t(2), nSrcSize))
        {
            pDst = (uint8_t) pSrc;
            ++pDst;
            ++pSrc;
            ++i;
        }

        // do swapping
        if (NULL != pRemovedOffsets)
        {
            while (i < (uint32_t) nSrcSize)
            {
                if (false == pSrc.IsPrevent())
                {
                    pDst = (uint8_t) pSrc;
                    ++pDst;
                }
                else
                    pRemovedOffsets->push_back(uint32_t(i));

                ++pSrc;
                ++i;
            }
        }
        else
        {
            while (i < (uint32_t) nSrcSize)
            {
                if (false == pSrc.IsPrevent())
                {
                    pDst = (uint8_t) pSrc;
                    ++pDst;
                }

                ++pSrc;
                ++i;
            }
        }

        // write padding bytes
        nDstSize = nSrcSize - pSrc.GetRemovedBytes();
        while (nDstSize & 3)
        {
            pDst = (uint8_t) (0);
            ++nDstSize;
            ++pDst;
        }
    }

    // Memory big-to-little endian converter
    void Splitter_VVC::SwapMemory(MemoryPiece *pMemDst, MemoryPiece *pMemSrc, std::vector<uint32_t> *pRemovedOffsets)
    {
        size_t dstSize = pMemSrc->GetDataSize();
        SwapMemoryAndRemovePreventionBytes(pMemDst->GetPointer(),
                    dstSize,
                    pMemSrc->GetPointer(),
                    pMemSrc->GetDataSize(),
                    pRemovedOffsets);

        assert(pMemDst->GetSize() >= dstSize);
        size_t tail_size = std::min<size_t>(pMemDst->GetSize() - dstSize, DEFAULT_UNIT_TAIL_SIZE);
        memset(pMemDst->GetPointer() + dstSize, DEFAULT_UNIT_TAIL_VALUE, tail_size);
        pMemDst->SetDataSize(dstSize);
        pMemDst->SetTime(pMemSrc->GetTime());
    }

    // Main API call
    UMC::MediaDataEx *Splitter_VVC::GetNalUnits(UMC::MediaData *source)
    {
        return m_iCodeSearcher.LoadData(source); // Load new data from source and search a new nal unit
    }

} // namespace UMC_VVC_DECODER
#endif // MFX_ENABLE_VVC_VIDEO_DECODE
