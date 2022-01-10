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

#ifndef __UMC_VC1_DEC_DEBUG_H__
#define __UMC_VC1_DEC_DEBUG_H__


#include "umc_vc1_dec_seq.h"
#include "vm_types.h"



    extern const uint32_t  VC1_POSITION; // MB, Block positions, skip info
    extern const uint32_t  VC1_CBP; // coded block patern info
    extern const uint32_t  VC1_BITBLANES; // bitplane information
    extern const uint32_t  VC1_QUANT; // transform types decoded info
    extern const uint32_t  VC1_TT; // transform types decoded info
    extern const uint32_t  VC1_MV; // motion vectors info
    extern const uint32_t  VC1_PRED; // predicted blocks
    extern const uint32_t  VC1_COEFFS; // DC, AC coefficiens
    extern const uint32_t  VC1_RESPEL; // pixels befor filtering
    extern const uint32_t  VC1_SMOOTHINT; // smoothing
    extern const uint32_t  VC1_BFRAMES; // B frames log
    extern const uint32_t  VC1_INTENS; // intesity compensation tables
    extern const uint32_t  VC1_MV_BBL; // deblocking
    extern const uint32_t  VC1_MV_FIELD; // motion vectors info for field pic

    extern const uint32_t  VC1_DEBUG; //current debug output
    extern const uint32_t  VC1_FRAME_DEBUG; //on/off frame debug
    extern const uint32_t  VC1_FRAME_MIN; //first frame to debug
    extern const uint32_t  VC1_FRAME_MAX; //last frame to debug
    extern const uint32_t  VC1_TABLES; //VLC tables

    typedef enum
    {
        VC1DebugAlloc,
        VC1DebugFree,
        VC1DebugRoutine
    } VC1DebugWork;

class VM_Debug
{
public:

    void vm_debug_frame(int32_t _cur_frame, int32_t level, const vm_char *format, ...);
    void print_bitplane(VC1Bitplane* pBitplane, int32_t width, int32_t height);
    static VM_Debug& GetInstance(VC1DebugWork typeWork)
    {
        static VM_Debug* singleton;
        switch (typeWork)
        {
        case VC1DebugAlloc:
            singleton = new VM_Debug;
            break;
        case VC1DebugRoutine:
            break;
        default:
            break;
        }
        return *singleton;
    }

    void Release()
    {
        delete this;
    }
    VM_Debug()
    {
#ifdef VC1_DEBUG_ON
        Logthread0 = fopen("_Log0.txt","w");
        Logthread1 = fopen("_Log1.txt","w");

        assert(Logthread0 != NULL);
        assert(Logthread1 != NULL);
#endif

    }; //only for Win debug

    ~VM_Debug()
    {
#ifdef VC1_DEBUG_ON
        fclose(Logthread0);
        Logthread0 = NULL;
        fclose(Logthread1);
        Logthread1 = NULL;
#endif
    };

private:

#ifdef VC1_DEBUG_ON
    FILE* Logthread0;
    FILE* Logthread1;
#endif
};

#endif
#endif //MFX_ENABLE_VC1_VIDEO_DECODE
