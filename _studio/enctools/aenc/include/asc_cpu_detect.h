// Copyright (c) 2021-2023 Intel Corporation
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
#ifndef _CPUDETECT_H_
#define _CPUDETECT_H_

#if defined(MFX_ENABLE_ADAPTIVE_ENCODE)

#include "av1_scd.h"
    #include <cpuid.h>

#define ET_ASC_ALIGN_DECL(X) __attribute__ ((aligned(X)))

//
// CPU Dispatcher
// 1) global CPU flags are initialized at startup
// 2) each stub configures a function pointer on first call
//

static inline mfxI32 CpuFeature_SSE41() {
    return((__builtin_cpu_supports("sse4.1")));
}

//
// end Dispatcher
//


static const int ET_HIST_THRESH_LO = 1;
static const int ET_HIST_THRESH_HI = 12;

#define ET_SAD_SEARCH_VSTEP 2  // 1=FS 2=FHS

ET_ASC_ALIGN_DECL(16) static const mfxU16 et_tab_twostep[8] = {
    0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff, 0x0000, 0xffff,
};

ET_ASC_ALIGN_DECL(16) static const mfxU16 et_tab_killmask[8][8] = {
    { 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff },
    { 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff },
    { 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff },
    { 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff },
    { 0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff },
    { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff },
    { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff },
    { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xffff },
};

#define ET_ASC_CPU_DISP_INIT_C(func)           (func = (func ## _C))
#define ET_ASC_CPU_DISP_INIT_SSE4(func)        (func = (func ## _SSE4))
#define ET_ASC_CPU_DISP_INIT_SSE4_C(func)      (m_SSE4_available ? ET_ASC_CPU_DISP_INIT_SSE4(func) : ET_ASC_CPU_DISP_INIT_C(func))

#define _mm_loadh_epi64(a, ptr) _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(a), (__m64 *)(ptr)))
#define _mm_movehl_epi64(a, b) _mm_castps_si128(_mm_movehl_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b)))

// Load 0..3 floats to XMM register from memory
// NOTE: elements of XMM are permuted [ 2 - 1 ]
static inline __m128 LoadPartialXmm(float *pSrc, mfxI32 len)
{
    __m128 xmm = _mm_setzero_ps();
    if (len & 2)
    {
        xmm = _mm_loadh_pi(xmm, (__m64 *)pSrc);
        pSrc += 2;
    }
    if (len & 1)
    {
        xmm = _mm_move_ss(xmm, _mm_load_ss(pSrc));
    }
    return xmm;
}

// Store 0..3 floats from XMM register to memory
// NOTE: elements of XMM are permuted [ 2 - 1 ]
static inline void StorePartialXmm(float *pDst, __m128 xmm, mfxI32 len)
{
    if (len & 2)
    {
        _mm_storeh_pi((__m64 *)pDst, xmm);
        pDst += 2;
    }
    if (len & 1)
    {
        _mm_store_ss(pDst, xmm);
    }
}

// Load 0..15 bytes to XMM register from memory
// NOTE: elements of XMM are permuted [ 8 4 2 - 1 ]
template <char init>
static inline __m128i LoadPartialXmm(unsigned char *pSrc, mfxI32 len)
{
    __m128i xmm = _mm_set1_epi8(init);
    if (len & 8) {
        xmm = _mm_loadh_epi64(xmm, (__m64 *)pSrc);
        pSrc += 8;
    }
    if (len & 4) {
        xmm = _mm_insert_epi32(xmm, *((int *)pSrc), 1);
        pSrc += 4;
    }
    if (len & 2) {
        xmm = _mm_insert_epi16(xmm, *((short *)pSrc), 1);
        pSrc += 2;
    }
    if (len & 1) {
        xmm = _mm_insert_epi8(xmm, *pSrc, 0);
    }
    return xmm;
}

#endif // MFX_ENABLE_ADAPTIVE_ENCODE
#endif // _CPUDETECT_H_
