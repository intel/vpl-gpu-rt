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

#ifndef __UMC_VVC_HEADERS_MANAGER_H
#define __UMC_VVC_HEADERS_MANAGER_H

#include "umc_vvc_dec_defs.h"
#include "umc_media_data_ex.h"
#include "umc_vvc_heap.h"

namespace UMC_VVC_DECODER
{
    // Header set container
    template <typename T>
    class HeaderSet
    {
    public:

        HeaderSet(Heap_Objects *pObjHeap)
            : m_header()
            , m_objHeap(pObjHeap)
            , m_currentID(-1)
        {
        }

        virtual ~HeaderSet()
        {
            Reset(false);
        }

        T * AddHeader(T* hdr)
        {
            uint32_t id = hdr->GetID();

            if (id >= m_header.size())
            {
                m_header.resize(id + 1, nullptr);
            }

            m_currentID = id;

            if (m_header[id])
            {
                m_header[id]->DecrementReference();
            }

            T * header = m_objHeap->AllocateObject<T>();
            *header = *hdr;

            //ref counter may not be 0 here since it can be copied from given [hdr] object
            header->ResetRefCounter();
            header->IncrementReference();

            m_header[id] = header;
            return header;
        }

        T * GetHeader(int32_t id)
        {
            if ((uint32_t)id >= m_header.size())
            {
                return 0;
            }

            return m_header[id];
        }

        const T * GetHeader(int32_t id) const
        {
            if ((uint32_t)id >= m_header.size())
            {
                return 0;
            }

            return m_header[id];
        }

        uint8_t GetHeaderNum()
        {
            return (uint8_t)m_header.size();
        }

        void RemoveHeader(void * hdr)
        {
            T * tmp = (T *)hdr;
            if (!tmp)
            {
                assert(false);
                return;
            }

            uint32_t id = tmp->GetID();

            if (id >= m_header.size())
            {
                assert(false);
                return;
            }

            if (!m_header[id])
            {
                assert(false);
                return;
            }

            assert(m_header[id] == hdr);
            m_header[id]->DecrementReference();
            m_header[id] = 0;
        }

        void Reset(bool isPartialReset = false)
        {
            if (!isPartialReset)
            {
                for (uint32_t i = 0; i < m_header.size(); i++)
                {
                    m_objHeap->FreeObject(m_header[i]);
                }

                m_header.clear();
                m_currentID = -1;
            }
        }

        void SetCurrentID(int32_t id)
        {
            if (GetHeader(id))
                m_currentID = id;
        }

        int32_t GetCurrentID() const
        {
            return m_currentID;
        }

        T * GetCurrentHeader()
        {
            if (m_currentID == -1)
                return 0;

            return GetHeader(m_currentID);
        }

        const T * GetCurrentHeader() const
        {
            if (m_currentID == -1)
                return 0;

            return GetHeader(m_currentID);
        }

    private:

        std::vector<T*>           m_header;
        Heap_Objects              *m_objHeap;
        int32_t                   m_currentID;
    };

    // VPS/SPS/PPS/PH etc. headers manager
    class ParameterSetManager
    {
    public:

        ParameterSetManager(Heap_Objects *pObjHeap)
            : m_videoParams(pObjHeap)
            , m_seqParams(pObjHeap)
            , m_picParams(pObjHeap)
            , m_adaptionParams(pObjHeap)
            , m_SEIParams(pObjHeap)
            , m_opiParams(pObjHeap)
        {
        }

        void Reset(bool isPartialReset = false)
        {
            m_videoParams.Reset(isPartialReset);
            m_seqParams.Reset(isPartialReset);
            for (uint32_t id = 0; id < m_picParams.GetHeaderNum(); id++)
            {
                VVCPicParamSet* pPps = m_picParams.GetHeader(id);
                if (pPps != nullptr)
                {
                    for (uint32_t i = 0; i < pPps->pps_rect_slices.size(); i++)
                    {
                        delete pPps->pps_rect_slices[i];
                    }
                    pPps->pps_rect_slices.clear();
                    for (uint32_t j = 0; j < pPps->pps_slice_map.size(); j++)
                    {
                        delete pPps->pps_slice_map[j];
                    }
                    pPps->pps_slice_map.clear();
                    for (uint32_t k = 0; k < pPps->pps_sub_pics.size(); k++)
                    {
                        delete pPps->pps_sub_pics[k];
                    }
                    pPps->pps_sub_pics.clear();
                }
            }
            m_picParams.Reset(isPartialReset);
            m_phParams.Reset();
            m_adaptionParams.Reset(isPartialReset);
            m_SEIParams.Reset(isPartialReset);
            m_opiParams.Reset();
        }

        HeaderSet<VVCVideoParamSet>           m_videoParams;
        HeaderSet<VVCSeqParamSet>             m_seqParams;
        HeaderSet<VVCPicParamSet>             m_picParams;
        VVCPicHeader                          m_phParams;
        HeaderSet<VVCAPS>                     m_adaptionParams;
        HeaderSet<VVCSEIPayLoad>              m_SEIParams;
        HeaderSet<VVCOPI>                     m_opiParams;
    };
} // namespace UMC_VVC_DECODER

#endif // __UMC_VVC_HEADERS_MANAGER_H
#endif // MFX_ENABLE_VVC_VIDEO_DECODE
