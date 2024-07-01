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

#ifndef __UMC_VVC_HEAP_H
#define __UMC_VVC_HEAP_H

#include <memory>
#include "umc_mutex.h"
#include "umc_vvc_dec_defs.h"
#include "umc_media_data.h"

namespace UMC_VVC_DECODER
{

    // Data buffer container
    class MemoryPiece
    {
    public:

        MemoryPiece()
        {
            Reset();
        }

        ~MemoryPiece()
        {
            Release();
        }

        void Release()
        {
            if (m_pSourceBuffer)
            {
                delete[] m_pSourceBuffer;
            }
            Reset();
        }

        void SetData(UMC::MediaData *out)
        {
            Release();

            m_pDataPointer = (uint8_t*)out->GetDataPointer();
            m_nDataSize = out->GetDataSize();
            m_pts = out->GetTime();
        }

        // Allocate memory piece
        bool Allocate(size_t nSize)
        {
            Release();

            m_pSourceBuffer = new uint8_t[(int32_t)nSize];
            m_pDataPointer = m_pSourceBuffer;
            m_nSourceSize = nSize;
            return true;
        }

        uint8_t *GetPointer(){return m_pDataPointer;}
        size_t GetSize() const {return m_nSourceSize;}

        size_t GetDataSize() const {return m_nDataSize;}
        void SetDataSize(size_t dataSize) {m_nDataSize = dataSize;}

        double GetTime() const {return m_pts;}
        void SetTime(double pts) {m_pts = pts;}

    protected:

        uint8_t *m_pSourceBuffer;                           // pointer to source memory
        uint8_t *m_pDataPointer;                            // pointer to source memory
        size_t  m_nSourceSize;                              // allocated memory size
        size_t  m_nDataSize;                                // data memory size
        double  m_pts;

        void Reset()
        {
            m_pts = 0;
            m_pSourceBuffer = 0;
            m_pDataPointer = 0;
            m_nSourceSize = 0;
            m_nDataSize = 0;
        }
    };

    class Heap_Objects;

    class Item
    {
    public:

        Item(Heap_Objects *heap, void *ptr, size_t size, bool isTyped = false)
            : m_pNext(0)
            , m_ptr(ptr)
            , m_size(size)
            , m_isTyped(isTyped)
            , m_heap(heap)
        {
        }

        ~Item()
        {
        }

        Item   *m_pNext;
        void   *m_ptr;
        size_t m_size;
        bool   m_isTyped;
        Heap_Objects *m_heap;

        static Item *Allocate(Heap_Objects *heap, size_t size, bool isTyped = false)
        {
            uint8_t *ppp = new uint8_t[size + sizeof(Item)];
            if (!ppp)
            {
                throw vvc_exception(UMC::UMC_ERR_ALLOC);
            }
            Item *item = new (ppp)Item(heap, 0, size, isTyped);
            item->m_ptr = (uint8_t*)ppp + sizeof(Item);
            return item;
        }

        static void Free(Item *item)
        {
            if (item->m_isTyped)
            {
                HeapObject *obj = reinterpret_cast<HeapObject *>(item->m_ptr);
                obj->~HeapObject();
            }

            item->~Item();
            delete[] (uint8_t*)item;
        }
    };


    // Collection of heap objects
    class Heap_Objects
    {
    public:

        Heap_Objects()
            : m_firstFreeItem(0)
        {
        }

        virtual ~Heap_Objects()
        {
            Release();
        }

        Item *GetItemForAllocation(size_t size, bool typed = false)
        {
            UMC::AutomaticUMCMutex guard(m_guard);

            if (!m_firstFreeItem)
            {
                return 0;
            }

            if (m_firstFreeItem->m_size == size && m_firstFreeItem->m_isTyped == typed)
            {
                Item *ptr = m_firstFreeItem;
                m_firstFreeItem = m_firstFreeItem->m_pNext;
                assert(ptr->m_size == size);
                return ptr;
            }

            Item *temp = m_firstFreeItem;

            while (temp->m_pNext)
            {
                if (temp->m_pNext->m_size == size && temp->m_pNext->m_isTyped == typed)
                {
                    Item *ptr = temp->m_pNext;
                    temp->m_pNext = temp->m_pNext->m_pNext;
                    return ptr;
                }

                temp = temp->m_pNext;
            }

            return 0;
        }

        void *Allocate(size_t size, bool isTyped = false)
        {
            Item *item = GetItemForAllocation(size);
            if (!item)
            {
                item = Item::Allocate(this, size, isTyped);
            }

            return item->m_ptr;
        }

        template<typename T>
        T *Allocate(size_t size = sizeof(T), bool isTyped = false)
        {
            return (T*)Allocate(size, isTyped);
        }

        template <typename T>
        T *AllocateObject()
        {
            Item *item = GetItemForAllocation(sizeof(T), true);

            if (!item)
            {
                void *ptr = Allocate(sizeof(T), true);
                return new(ptr) T();
            }

            return (T*)(item->m_ptr);
        }

        void FreeObject(void *obj, bool forceFree = false)
        {
            Free(obj, forceFree);
        }

        void Free(void *obj, bool forceFree = false)
        {
            if (!obj)
                return;

            UMC::AutomaticUMCMutex guard(m_guard);
            Item *item = (Item *) ((uint8_t*)obj - sizeof(Item));

            Item *temp = m_firstFreeItem;

            while (temp)
            {
                if (temp == item) // already removed
                    return;

                temp = temp->m_pNext;
            }

            if (forceFree)
            {
                Item::Free(item);
                return;
            }
            else
            {
                if (item->m_isTyped)
                {
                    HeapObject * object = reinterpret_cast<HeapObject *>(item->m_ptr);
                    object->Reset();
                }
            }

            item->m_pNext = m_firstFreeItem;
            m_firstFreeItem = item;
        }

        void Release()
        {
            UMC::AutomaticUMCMutex guard(m_guard);

            while (m_firstFreeItem)
            {
                Item *pTemp = m_firstFreeItem->m_pNext;
                Item::Free(m_firstFreeItem);
                m_firstFreeItem = pTemp;
            }
        }

    private:

        Item *m_firstFreeItem;
        UMC::Mutex m_guard;
    };

} // namespace UMC_VVC_DECODER

#endif // __UMC_VVC_HEAP_H
#endif // MFX_ENABLE_VVC_VIDEO_DECODE
