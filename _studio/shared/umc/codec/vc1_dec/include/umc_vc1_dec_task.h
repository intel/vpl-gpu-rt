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

#ifndef __UMC_VC1_DEC_TASK_H_
#define __UMC_VC1_DEC_TASK_H_

#include "vm_types.h"
#include "umc_structures.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_dec_seq.h"


namespace UMC
{
    enum SelfUMCStatus
    {
        UMC_LAST_FRAME = 10
    };

    class VC1TaskProcessorUMC;

    typedef enum
    {
        VC1Decode        = 1,
        //VC1Dequant       = 2,
        VC1Reconstruct   = 2,
        VC1MVCalculate   = 4,
        VC1MC            = 8,
        VC1PreparePlane  = 16,
        VC1Deblock       = 32,
        VC1Complete      = 64
    } VC1TaskTypes;

#pragma pack(16)

    class VC1Task
    {
    public:
        // Default constructor
        VC1Task(int32_t iThreadNumber,int32_t TaskID):m_pSlice(NULL),
                                                    m_iThreadNumber(iThreadNumber),
                                                    m_iTaskID(TaskID),
                                                    m_eTasktype(VC1Decode),
                                                    m_pBlock(NULL),
                                                    m_pPredBlock(NULL),
                                                    m_pSrcToSwap(NULL),
                                                    m_uDataSizeToSwap(0),
                                                    m_isFirstInSecondSlice(false),
                                                    m_isFieldReady(false),
                                                    pMulti(NULL)
          {
          };
          VC1Task(int32_t iThreadNumber):m_pSlice(NULL),
                                        m_iThreadNumber(iThreadNumber),
                                        m_iTaskID(0),
                                        m_eTasktype(VC1Decode),
                                        m_pBlock(NULL),
                                        m_pPredBlock(NULL),
                                        m_pSrcToSwap(NULL),
                                        m_uDataSizeToSwap(0),
                                        m_isFirstInSecondSlice(false),
                                        m_isFieldReady(false),
                                        pMulti(NULL)

          {
          };

          uint32_t IsDecoding (uint32_t _task_settings) {return _task_settings&VC1Decode;}
          uint32_t IsDeblocking(uint32_t _task_settings) {return _task_settings&VC1Deblock;}
          void setSliceParams(VC1Context* pContext);

          SliceParams* m_pSlice;                                        //

          int32_t m_iThreadNumber;                                     // (int32_t) owning thread number
          int32_t m_iTaskID;                                           // (int32_t) task identificator
          VC1TaskTypes m_eTasktype;
          int16_t*      m_pBlock;
          uint8_t*       m_pPredBlock;
          uint8_t*       m_pSrcToSwap;
          uint32_t       m_uDataSizeToSwap;
          bool         m_isFirstInSecondSlice;
          bool         m_isFieldReady;
          VC1Status (VC1TaskProcessorUMC::*pMulti)(VC1Context* pContext, VC1Task* pTask);
    };

#pragma pack()

}
#endif //__umc_vc1_dec_task_H__
#endif //MFX_ENABLE_VC1_VIDEO_DECODE
