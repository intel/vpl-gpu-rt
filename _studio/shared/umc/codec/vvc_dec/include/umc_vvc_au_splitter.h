// Copyright (c) 2020-2024 Intel Corporation
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

#pragma once

#include "umc_defs.h"
#ifdef MFX_ENABLE_VVC_VIDEO_DECODE

#include "umc_vvc_dec_defs.h"
#include "umc_media_data_ex.h"
#include "umc_vvc_heap.h"

namespace UMC
{ class MediaData; }

namespace UMC_VVC_DECODER
{
    // The purpose of this class is to search nalu start code in provided bistream.
    class StartCodeSearcher
    {
    public:

        StartCodeSearcher();

        void Reset()
        {
            m_cache.resize(0);
            m_pts = -1;
            m_code = -1;
        }

        UMC::MediaDataEx *LoadData(UMC::MediaData *);       // Load new data from source and searches a nal unit
        static int32_t FindStartCode(uint8_t *(&pBuf), size_t &size, int32_t &startCodeSize);
        int32_t MoveToStartCode(UMC::MediaData *pSource); // Move bitstream pointer to start code address

        void SetSuggestedSize(size_t size)
        {
            if (size > m_suggestedSize)
                m_suggestedSize = size;
        }

    private:

        int32_t FindNalUnit(UMC::MediaData *pSource, UMC::MediaData *pDst); // Find nal unit in provided bitstream. Save bs to cache if needed
        int32_t OnEOS(UMC::MediaData *pDst);       // Action on end of stream
        int32_t FindNalUnitInternal(UMC::MediaData *pSource, UMC::MediaData *pDst); // Find nal unit by start code

        UMC::MediaDataEx               m_mediaData;
        UMC::MediaDataEx::_MediaDataEx m_mediaDataEx;

        std::vector<uint8_t>  m_cache;  // Started but not completed nal unit
        double                m_pts;
        int32_t               m_code;
        size_t                m_suggestedSize; // Actual size is calculated in CalculateSuggestedSize
    };

    // This is a wrapper under StartCodeSearcher class.
    // The additional functionality introduced by the class is to return nalu type.
    class Splitter_VVC
    {
    public:

        virtual ~Splitter_VVC() = default;

        void Reset()
        {
            m_iCodeSearcher.Reset();
        }

        virtual UMC::MediaDataEx *GetNalUnits(UMC::MediaData *);

        int32_t MoveToStartCode(UMC::MediaData *pSource)
        {
            return m_iCodeSearcher.MoveToStartCode(pSource);
        }

        // Set maximum NAL unit size
        void SetSuggestedSize(size_t size)
        {
            m_iCodeSearcher.SetSuggestedSize(size);
        }

        void SwapMemory(MemoryPiece *pMemDst, MemoryPiece *pMemSrc, std::vector<uint32_t> *pRemovedOffsets);

    private:

        StartCodeSearcher     m_iCodeSearcher;    // NAL unit start code searcher
    };

    // Utility class for writing 32-bit little endian integers
    class VVCDstDwordPointer
    {
    public:
        VVCDstDwordPointer(void)
        {
            m_pDest = NULL;
            m_iCur = 0;
            m_nByteNum = 0;
        }

        VVCDstDwordPointer operator = (void *pDest)
        {
            m_pDest = (uint32_t *) pDest;
            m_nByteNum = 0;
            m_iCur = 0;

            return *this;
        }

        VVCDstDwordPointer &operator ++ (void)
        {
            if (4 == ++m_nByteNum)
            {
                *m_pDest = m_iCur;
                m_pDest += 1;
                m_nByteNum = 0;
                m_iCur = 0;
            }
            else
            {
                m_iCur <<= 8;
            }

            return *this;
        }

        uint8_t operator = (uint8_t nByte)
        {
            m_iCur = (m_iCur & ~0x0ff) | ((uint32_t) nByte);
            return nByte;
        }

    protected:
        uint32_t *m_pDest;                              // pointer to destination buffer
        uint32_t m_nByteNum;                            // number of current byte in dword
        uint32_t m_iCur;                                // current dword
    };

    // Utility class for reading big endian bitstream
    class VVCSourcePointer
    {
    public:
        VVCSourcePointer(void)
        {
            m_nZeros = 0;
            m_pSource = NULL;
            m_nRemovedBytes = 0;
        }

        VVCSourcePointer &operator = (void *pSource)
        {
            m_pSource = (uint8_t *) pSource;
            m_nZeros = 0;
            m_nRemovedBytes = 0;

            return *this;
        }

        VVCSourcePointer &operator ++ (void)
        {
            uint8_t bCurByte = m_pSource[0];

            if (0 == bCurByte)
            {
                m_nZeros += 1;
            }
            else
            {
                if ((3 == bCurByte) && (2 <= m_nZeros))
                {
                    m_nRemovedBytes += 1;
                }
                m_nZeros = 0;
            }

            m_pSource += 1;

            return *this;
        }

        bool IsPrevent(void)
        {
            if ((3 == m_pSource[0]) && (2 <= m_nZeros))
                return true;
            else
                return false;
        }

        operator uint8_t (void)
        {
            return m_pSource[0];
        }

        uint32_t GetRemovedBytes(void)
        {
            return m_nRemovedBytes;
        }

    protected:
        uint8_t *m_pSource;                       // pointer to destination buffer
        uint32_t m_nZeros;                        // number of preceding zeros
        uint32_t m_nRemovedBytes;                 // number of removed bytes
    };

} // namespace UMC_VVC_DECODER
#endif // MFX_ENABLE_VVC_VIDEO_DECODE
