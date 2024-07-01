// Copyright (c) 2019-2023 Intel Corporation
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

#include <assert.h>
#include <math.h>
#include <limits.h>
#include <malloc.h>
#include <string.h>
#include <sstream>
#include <algorithm>

#include "av1_scd.h"
#include "asc_cpu_detect.h"

#if defined(MFX_ENABLE_ADAPTIVE_ENCODE)

using std::min;
using std::max;

#define ASCTUNEDATA       0
#define OUT_BLOCK         16  // output pixels computed per thread
#define MVBLK_SIZE        8
#define BLOCK_SIZE        4
#define BLOCK_SIZE_SHIFT  2
#define NumTSC            10
#define NumSC             10
#define FLOAT_MAX         2241178.0
#define FRAMEMUL          16
#define CHROMASUBSAMPLE   4
#define MAXLTRHISTORY     120
#define ASC_SMALL_AREA    8192//13 bits
#define S_AREA_SHIFT      13
#define TSC_INT_SCALE     5
#define GAINDIFF_THR      20
#define RF_DECISION_LEVEL 10

#define TSCSTATBUFFER     3
#define ASCVIDEOSTATSBUF  2

#define SCD_BLOCK_PIXEL_WIDTH   32
#define SCD_BLOCK_HEIGHT        8

#if defined(ASC_DEBUG)
#define ASC_PRINTF(...)     printf(__VA_ARGS__)
#define ASC_FPRINTF(...)    fprintf(__VA_ARGS__)
#define ASC_FFLUSH(x)       fflush(x)
#else
#define ASC_PRINTF(...)
#define ASC_FPRINTF(...)
#define ASC_FFLUSH(x)
#endif

#define SCD_CHECK_MFX_ERR(STS) if ((STS) != MFX_ERR_NONE) { ASC_PRINTF("FAILED at file: %s, line: %d, mfxerr: %d\n", __FILE__, __LINE__, STS); return STS; }

namespace aenc {

    void GainOffset_C(mfxU8** pSrc, mfxU8** pDst, mfxU16 width, mfxU16 height, mfxU16 pitch, mfxI16 gainDiff)
    {
        mfxU8 *ss = *pSrc,
              *dd = *pDst;
        for (mfxU16 i = 0; i < height; i++) {
            for (mfxU16 j = 0; j < width; j++) {
                mfxI16
                    val = ss[j + i * pitch] - gainDiff;
                dd[j + i * pitch] = (mfxU8)std::min(std::max(val, mfxI16(0)), mfxI16(255));
            }
        }

        *pSrc = *pDst;
    }


    void RsCsCalc_4x4_C(mfxU8* pSrc, int srcPitch, int wblocks, int hblocks, mfxU16* pRs, mfxU16* pCs)
    {
        pSrc += (4 * srcPitch) + 4;
        for (mfxI16 i = 0; i < hblocks - 2; i++)
        {
            for (mfxI16 j = 0; j < wblocks - 2; j++)
            {
                mfxU16 accRs = 0;
                mfxU16 accCs = 0;

                for (mfxI32 k = 0; k < 4; k++)
                {
                    for (mfxI32 l = 0; l < 4; l++)
                    {
                        mfxU16 dRs = (mfxU16)abs(pSrc[l] - pSrc[l - srcPitch]) >> 2;
                        mfxU16 dCs = (mfxU16)abs(pSrc[l] - pSrc[l - 1]) >> 2;
                        accRs += (mfxU16)(dRs * dRs);
                        accCs += (mfxU16)(dCs * dCs);
                    }
                    pSrc += srcPitch;
                }
                pRs[i * wblocks + j] = accRs;
                pCs[i * wblocks + j] = accCs;

                pSrc -= 4 * srcPitch;
                pSrc += 4;
            }
            pSrc -= 4 * (wblocks - 2);
            pSrc += 4 * srcPitch;
        }
    }

    void RsCsCalc_4x4_NoBlkShift(mfxU8 *pSrc, int srcPitch, int wblocks, int hblocks, mfxU16 *pRs, mfxU16 *pCs)
    {
        pSrc += (1 * srcPitch) + 1;
        for (mfxI16 i = 0; i < hblocks - 1; i++)
        {
            for (mfxI16 j = 0; j < wblocks - 1; j++)
            {
                mfxU16 accRs = 0;
                mfxU16 accCs = 0;

                for (mfxI32 k = 0; k < 4; k++)
                {
                    for (mfxI32 l = 0; l < 4; l++)
                    {
                        mfxU16 dRs = (mfxU16)abs(pSrc[l] - pSrc[l - srcPitch]) >> 2;
                        mfxU16 dCs = (mfxU16)abs(pSrc[l] - pSrc[l - 1]) >> 2;
                        accRs += (mfxU16)(dRs * dRs);
                        accCs += (mfxU16)(dCs * dCs);
                    }
                    pSrc += srcPitch;
                }
                pRs[i * wblocks + j] = accRs;
                pCs[i * wblocks + j] = accCs;

                pSrc -= 4 * srcPitch;
                pSrc += 4;
            }
            mfxI32 j = wblocks - 1;
            {
                mfxU16 accRs = 0;
                mfxU16 accCs = 0;

                for (mfxI32 k = 0; k < 4; k++)
                {
                    for (mfxI32 l = 0; l < 3; l++)
                    {
                        mfxU16 dRs = (mfxU16)abs(pSrc[l] - pSrc[l - srcPitch]) >> 2;
                        mfxU16 dCs = (mfxU16)abs(pSrc[l] - pSrc[l - 1]) >> 2;
                        accRs += (mfxU16)(dRs * dRs);
                        accCs += (mfxU16)(dCs * dCs);
                    }
                    pSrc += srcPitch;
                }
                pRs[i * wblocks + j] = accRs;
                pCs[i * wblocks + j] = accCs;

                pSrc -= 4 * srcPitch;
                pSrc += 4;
            }
            pSrc -= 4 * (wblocks);
            pSrc += 4 * srcPitch;
        }
        {
            mfxI32 i = hblocks - 1;
            for (mfxI16 j = 0; j < wblocks - 1; j++)
            {
                mfxU16 accRs = 0;
                mfxU16 accCs = 0;

                for (mfxI32 k = 0; k < 3; k++)
                {
                    for (mfxI32 l = 0; l < 4; l++)
                    {
                        mfxU16 dRs = (mfxU16)abs(pSrc[l] - pSrc[l - srcPitch]) >> 2;
                        mfxU16 dCs = (mfxU16)abs(pSrc[l] - pSrc[l - 1]) >> 2;
                        accRs += (mfxU16)(dRs * dRs);
                        accCs += (mfxU16)(dCs * dCs);
                    }
                    pSrc += srcPitch;
                }
                pRs[i * wblocks + j] = accRs;
                pCs[i * wblocks + j] = accCs;

                pSrc -= 4 * srcPitch;
                pSrc += 4;
            }
            mfxI32 j = wblocks - 1;
            {
                mfxU16 accRs = 0;
                mfxU16 accCs = 0;

                for (mfxI32 k = 0; k < 3; k++)
                {
                    for (mfxI32 l = 0; l < 3; l++)
                    {
                        mfxU16 dRs = (mfxU16)abs(pSrc[l] - pSrc[l - srcPitch]) >> 2;
                        mfxU16 dCs = (mfxU16)abs(pSrc[l] - pSrc[l - 1]) >> 2;
                        accRs += (mfxU16)(dRs * dRs);
                        accCs += (mfxU16)(dCs * dCs);
                    }
                    pSrc += srcPitch;
                }
                pRs[i * wblocks + j] = accRs;
                pCs[i * wblocks + j] = accCs;

                pSrc -= 4 * srcPitch;
                pSrc += 4;
            }
            pSrc -= 4 * (wblocks);
            pSrc += 4 * srcPitch;
        }
    }

    void RsCsCalc_4x4_SSE4(mfxU8* pSrc, int srcPitch, int wblocks, int hblocks, mfxU16* pRs, mfxU16* pCs)
    {
        pSrc += (4 * srcPitch) + 4;
        for (mfxI16 i = 0; i < hblocks - 2; i++)
        {
            // 4 horizontal blocks at a time
            mfxI16 j;
            for (j = 0; j < wblocks - 5; j += 4)
            {
                __m128i rs0 = _mm_setzero_si128();
                __m128i cs0 = _mm_setzero_si128();
                __m128i rs1 = _mm_setzero_si128();
                __m128i cs1 = _mm_setzero_si128();
                __m128i a0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[-srcPitch + 0]));
                __m128i a1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[-srcPitch + 8]));

                for (mfxI32 k = 0; k < 4; k++)
                {
                    __m128i b0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[-1]));
                    __m128i b1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[7]));
                    __m128i c0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[0]));
                    __m128i c1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[8]));
                    pSrc += srcPitch;

                    // accRs += dRs * dRs
                    a0 = _mm_srai_epi16(_mm_abs_epi16(_mm_sub_epi16(c0, a0)), 2);
                    a1 = _mm_srai_epi16(_mm_abs_epi16(_mm_sub_epi16(c1, a1)), 2);
                    a0 = _mm_madd_epi16(a0, a0);
                    a1 = _mm_madd_epi16(a1, a1);
                    rs0 = _mm_add_epi32(rs0, a0);
                    rs1 = _mm_add_epi32(rs1, a1);

                    // accCs += dCs * dCs
                    b0 = _mm_srai_epi16(_mm_abs_epi16(_mm_sub_epi16(c0, b0)), 2);
                    b1 = _mm_srai_epi16(_mm_abs_epi16(_mm_sub_epi16(c1, b1)), 2);
                    b0 = _mm_madd_epi16(b0, b0);
                    b1 = _mm_madd_epi16(b1, b1);
                    cs0 = _mm_add_epi32(cs0, b0);
                    cs1 = _mm_add_epi32(cs1, b1);

                    // reuse next iteration
                    a0 = c0;
                    a1 = c1;
                }
                rs0 = _mm_hadd_epi32(rs0, rs1);
                cs0 = _mm_hadd_epi32(cs0, cs1);

                // store
                rs0 = _mm_packus_epi32(rs0, cs0);
#ifdef ARCH64
                pmfxU64 t = (pmfxU64)&(pRs[i * wblocks + j]);
                *t = _mm_extract_epi64(rs0, 0);
                t = (pmfxU64)&(pCs[i * wblocks + j]);
                *t = _mm_extract_epi64(rs0, 1);
#else
                mfxU32 *t = (mfxU32*)&(pRs[i * wblocks + j]);
                *t = _mm_extract_epi32(rs0, 0);
                t = (mfxU32*)&(pRs[i * wblocks + j + 2]);
                *t = _mm_extract_epi32(rs0, 1);
                t = (mfxU32*)&(pCs[i * wblocks + j]);
                *t = _mm_extract_epi32(rs0, 2);
                t = (mfxU32*)&(pCs[i * wblocks + j + 2]);
                *t = _mm_extract_epi32(rs0, 3);
#endif

                pSrc -= 4 * srcPitch;
                pSrc += 16;
            }
            //2 horizontal blocks
            for (; j < wblocks - 3; j += 2)
            {
                __m128i rs0 = _mm_setzero_si128();
                __m128i cs0 = _mm_setzero_si128();
                __m128i rs1 = _mm_setzero_si128();
                __m128i cs1 = _mm_setzero_si128();
                __m128i a0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[-srcPitch + 0]));
                __m128i a1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[-srcPitch + 8]));

                for (mfxI32 k = 0; k < 4; k++)
                {
                    __m128i b0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[-1]));
                    __m128i b1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[7]));
                    __m128i c0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[0]));
                    __m128i c1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pSrc[8]));
                    pSrc += srcPitch;

                    // accRs += dRs * dRs
                    a0 = _mm_srai_epi16(_mm_abs_epi16(_mm_sub_epi16(c0, a0)), 2);
                    a1 = _mm_srai_epi16(_mm_abs_epi16(_mm_sub_epi16(c1, a1)), 2);
                    a0 = _mm_madd_epi16(a0, a0);
                    a1 = _mm_madd_epi16(a1, a1);
                    rs0 = _mm_add_epi32(rs0, a0);
                    rs1 = _mm_add_epi32(rs1, a1);

                    // accCs += dCs * dCs
                    b0 = _mm_srai_epi16(_mm_abs_epi16(_mm_sub_epi16(c0, b0)), 2);
                    b1 = _mm_srai_epi16(_mm_abs_epi16(_mm_sub_epi16(c1, b1)), 2);
                    b0 = _mm_madd_epi16(b0, b0);
                    b1 = _mm_madd_epi16(b1, b1);
                    cs0 = _mm_add_epi32(cs0, b0);
                    cs1 = _mm_add_epi32(cs1, b1);

                    // reuse next iteration
                    a0 = c0;
                    a1 = c1;
                }
                rs0 = _mm_hadd_epi32(rs0, rs1);
                cs0 = _mm_hadd_epi32(cs0, cs1);

                // store
                rs0 = _mm_packus_epi32(rs0, cs0);
                mfxU32 *t = (mfxU32*)&(pRs[i * wblocks + j]);
                *t = _mm_extract_epi32(rs0, 0);
                t = (mfxU32*)&(pCs[i * wblocks + j]);
                *t = _mm_extract_epi32(rs0, 2);

                pSrc -= 4 * srcPitch;
                pSrc += 8;
            }

            // remaining blocks
            for (; j < wblocks - 2; j++)
            {
                mfxU16 accRs = 0;
                mfxU16 accCs = 0;

                for (mfxU16 k = 0; k < 4; k++)
                {
                    for (mfxI16 l = 0; l < 4; l++)
                    {
                        mfxU16 dRs = (mfxU16)abs(pSrc[l] - pSrc[l - srcPitch]) >> 2;
                        mfxU16 dCs = (mfxU16)abs(pSrc[l] - pSrc[l - 1]) >> 2;
                        accRs += dRs * dRs;
                        accCs += dCs * dCs;
                    }
                    pSrc += srcPitch;
                }
                pRs[i * wblocks + j] = accRs;
                pCs[i * wblocks + j] = accCs;

                pSrc -= 4 * srcPitch;
                pSrc += 4;
            }
            pSrc -= 4 * (wblocks - 2);
            pSrc += 4 * srcPitch;
        }
    }


    void RsCsCalc_bound_C(mfxU16* pRs, mfxU16* pCs, mfxU16* pRsCs, mfxU32* pRsFrame, mfxU32* pCsFrame, mfxU32* pContrast, int wblocks, int hblocks)
    {
        //mfxI32 len = wblocks * hblocks;
        mfxU16 accRs = 0;
        mfxU16 accCs = 0;
        mfxI32 sumRsCs00 = 0;
        mfxI32 sumRsCs01 = 0;
        mfxI32 sumRsCs10 = 0;
        mfxI32 sumRsCs11 = 0;
        mfxI32 maxRsCs, minRsCs;
        //ASC_PRINTF("\n");
        for (mfxI32 i = 0; i < hblocks / 2; i++)
        {
            for (mfxI32 j = 0; j < wblocks / 2; j++)
            {
                sumRsCs00 += pRs[i*wblocks + j];
                sumRsCs00 += pCs[i*wblocks + j];
                //    ASC_PRINTF("%i\t", pRs[i*wblocks + j]);
                accRs += pRs[i*wblocks + j] >> 7;
                accCs += pCs[i*wblocks + j] >> 7;
                pRsCs[i*wblocks + j] = (pRs[i*wblocks + j] + pCs[i*wblocks + j]) >> 1;
            }
            for (mfxI32 j = wblocks / 2; j < wblocks; j++)
            {
                sumRsCs01 += pRs[i*wblocks + j];
                sumRsCs01 += pCs[i*wblocks + j];
                //    ASC_PRINTF("%i\t", pRs[i*wblocks + j]);
                accRs += pRs[i*wblocks + j] >> 7;
                accCs += pCs[i*wblocks + j] >> 7;
                pRsCs[i*wblocks + j] = (pRs[i*wblocks + j] + pCs[i*wblocks + j]) >> 1;
            }
        }
        for (mfxI32 i = hblocks / 2; i < hblocks; i++)
        {
            for (mfxI32 j = 0; j < wblocks / 2; j++)
            {
                sumRsCs10 += pRs[i*wblocks + j];
                sumRsCs10 += pCs[i*wblocks + j];
                //    ASC_PRINTF("%i\t", pRs[i*wblocks + j]);
                accRs += pRs[i*wblocks + j] >> 7;
                accCs += pCs[i*wblocks + j] >> 7;
                pRsCs[i*wblocks + j] = (pRs[i*wblocks + j] + pCs[i*wblocks + j]) >> 1;
            }
            for (mfxI32 j = wblocks / 2; j < wblocks; j++)
            {
                sumRsCs11 += pRs[i*wblocks + j];
                sumRsCs11 += pCs[i*wblocks + j];
                //    ASC_PRINTF("%i\t", pRs[i*wblocks + j]);
                accRs += pRs[i*wblocks + j] >> 7;
                accCs += pCs[i*wblocks + j] >> 7;
                pRsCs[i*wblocks + j] = (pRs[i*wblocks + j] + pCs[i*wblocks + j]) >> 1;
            }
        }
        //ASC_PRINTF("\n");

        *pRsFrame = accRs;
        *pCsFrame = accCs;

        maxRsCs = std::max(1, std::max(std::max(sumRsCs00, sumRsCs01), std::max(sumRsCs10, sumRsCs11)));
        minRsCs = std::min(std::min(sumRsCs00, sumRsCs01), std::min(sumRsCs10, sumRsCs11));
        *pContrast = ((maxRsCs - minRsCs) * 100) / (maxRsCs + minRsCs);
    }

    void RsCsCalc_diff_C(mfxU16* pRs0, mfxU16* pCs0, mfxU16* pRs1, mfxU16* pCs1, int wblocks, int hblocks,
        mfxU32* pRsDiff, mfxU32* pCsDiff)
    {
        mfxU32 len = wblocks * hblocks;
        mfxU16 accRs = 0;
        mfxU16 accCs = 0;

        for (mfxU32 i = 0; i < len; i++)
        {
            accRs += (mfxU16)abs((pRs0[i] >> 5) - (pRs1[i] >> 5));
            accCs += (mfxU16)abs((pCs0[i] >> 5) - (pCs1[i] >> 5));
        }
        *pRsDiff = accRs;
        *pCsDiff = accCs;
    }


    void ImageDiffHistogram_C(mfxU8* pSrc, mfxU8* pRef, mfxU32 pitch, mfxU32 width, mfxU32 height, mfxI32 histogram[5], mfxI64 *pSrcDC, mfxI64 *pRefDC)
    {
        static const int HIST_THRESH_LO = 1;
        static const int HIST_THRESH_HI = 12;

        mfxI64 srcDC = 0;
        mfxI64 refDC = 0;

        histogram[0] = 0;
        histogram[1] = 0;
        histogram[2] = 0;
        histogram[3] = 0;
        histogram[4] = 0;

        for (mfxU32 i = 0; i < height; i++)
        {
            for (mfxU32 j = 0; j < width; j++)
            {
                int s = pSrc[j];
                int r = pRef[j];
                int d = s - r;

                srcDC += s;
                refDC += r;

                if (d < -HIST_THRESH_HI)
                    histogram[0]++;
                else if (d < -HIST_THRESH_LO)
                    histogram[1]++;
                else if (d < HIST_THRESH_LO)
                    histogram[2]++;
                else if (d < HIST_THRESH_HI)
                    histogram[3]++;
                else
                    histogram[4]++;
            }
            pSrc += pitch;
            pRef += pitch;
        }
        *pSrcDC = srcDC;
        *pRefDC = refDC;
    }

    void ImageDiffHistogram_SSE4(mfxU8* pSrc, mfxU8* pRef, mfxU32 pitch, mfxU32 width, mfxU32 height, mfxI32 histogram[5], mfxI64 *pSrcDC, mfxI64 *pRefDC) {
        __m128i sDC = _mm_setzero_si128();
        __m128i rDC = _mm_setzero_si128();

        __m128i h0 = _mm_setzero_si128();
        __m128i h1 = _mm_setzero_si128();
        __m128i h2 = _mm_setzero_si128();
        __m128i h3 = _mm_setzero_si128();

        __m128i zero = _mm_setzero_si128();

        for (mfxU32 i = 0; i < height; i++)
        {
            // process 16 pixels per iteration
            mfxU32 j;
            for (j = 0; j < width - 15; j += 16)
            {
                __m128i s = _mm_loadu_si128((__m128i *)(&pSrc[j]));
                __m128i r = _mm_loadu_si128((__m128i *)(&pRef[j]));

                sDC = _mm_add_epi64(sDC, _mm_sad_epu8(s, zero));    //accumulate horizontal sums
                rDC = _mm_add_epi64(rDC, _mm_sad_epu8(r, zero));

                r = _mm_sub_epi8(r, _mm_set1_epi8(-128));   // convert to signed
                s = _mm_sub_epi8(s, _mm_set1_epi8(-128));

                __m128i dn = _mm_subs_epi8(r, s);   // -d saturated to [-128,127]
                __m128i dp = _mm_subs_epi8(s, r);   // +d saturated to [-128,127]

                __m128i m0 = _mm_cmpgt_epi8(dn, _mm_set1_epi8(ET_HIST_THRESH_HI)); // d < -12
                __m128i m1 = _mm_cmpgt_epi8(dn, _mm_set1_epi8(ET_HIST_THRESH_LO)); // d < -4
                __m128i m2 = _mm_cmpgt_epi8(_mm_set1_epi8(ET_HIST_THRESH_LO), dp); // d < +4
                __m128i m3 = _mm_cmpgt_epi8(_mm_set1_epi8(ET_HIST_THRESH_HI), dp); // d < +12

                m0 = _mm_sub_epi8(zero, m0);    // negate masks from 0xff to 1
                m1 = _mm_sub_epi8(zero, m1);
                m2 = _mm_sub_epi8(zero, m2);
                m3 = _mm_sub_epi8(zero, m3);

                h0 = _mm_add_epi32(h0, _mm_sad_epu8(m0, zero)); // accumulate horizontal sums
                h1 = _mm_add_epi32(h1, _mm_sad_epu8(m1, zero));
                h2 = _mm_add_epi32(h2, _mm_sad_epu8(m2, zero));
                h3 = _mm_add_epi32(h3, _mm_sad_epu8(m3, zero));
            }

            // process remaining 1..15 pixels
            if (j < width)
            {
                __m128i s = LoadPartialXmm<0>(&pSrc[j], width & 0xf);
                __m128i r = LoadPartialXmm<0>(&pRef[j], width & 0xf);

                sDC = _mm_add_epi64(sDC, _mm_sad_epu8(s, zero));    //accumulate horizontal sums
                rDC = _mm_add_epi64(rDC, _mm_sad_epu8(r, zero));

                s = LoadPartialXmm<-1>(&pSrc[j], width & 0xf);      // ensure unused elements not counted

                r = _mm_sub_epi8(r, _mm_set1_epi8(-128));   // convert to signed
                s = _mm_sub_epi8(s, _mm_set1_epi8(-128));

                __m128i dn = _mm_subs_epi8(r, s);   // -d saturated to [-128,127]
                __m128i dp = _mm_subs_epi8(s, r);   // +d saturated to [-128,127]

                __m128i m0 = _mm_cmpgt_epi8(dn, _mm_set1_epi8(ET_HIST_THRESH_HI)); // d < -12
                __m128i m1 = _mm_cmpgt_epi8(dn, _mm_set1_epi8(ET_HIST_THRESH_LO)); // d < -4
                __m128i m2 = _mm_cmpgt_epi8(_mm_set1_epi8(ET_HIST_THRESH_LO), dp); // d < +4
                __m128i m3 = _mm_cmpgt_epi8(_mm_set1_epi8(ET_HIST_THRESH_HI), dp); // d < +12

                m0 = _mm_sub_epi8(zero, m0);    // negate masks from 0xff to 1
                m1 = _mm_sub_epi8(zero, m1);
                m2 = _mm_sub_epi8(zero, m2);
                m3 = _mm_sub_epi8(zero, m3);

                h0 = _mm_add_epi32(h0, _mm_sad_epu8(m0, zero)); // accumulate horizontal sums
                h1 = _mm_add_epi32(h1, _mm_sad_epu8(m1, zero));
                h2 = _mm_add_epi32(h2, _mm_sad_epu8(m2, zero));
                h3 = _mm_add_epi32(h3, _mm_sad_epu8(m3, zero));
            }
            pSrc += pitch;
            pRef += pitch;
        }

        // finish horizontal sums
        sDC = _mm_add_epi64(sDC, _mm_movehl_epi64(sDC, sDC));
        rDC = _mm_add_epi64(rDC, _mm_movehl_epi64(rDC, rDC));

        h0 = _mm_add_epi32(h0, _mm_movehl_epi64(h0, h0));
        h1 = _mm_add_epi32(h1, _mm_movehl_epi64(h1, h1));
        h2 = _mm_add_epi32(h2, _mm_movehl_epi64(h2, h2));
        h3 = _mm_add_epi32(h3, _mm_movehl_epi64(h3, h3));

        _mm_storel_epi64((__m128i *)pSrcDC, sDC);
        _mm_storel_epi64((__m128i *)pRefDC, rDC);

        histogram[0] = _mm_cvtsi128_si32(h0);
        histogram[1] = _mm_cvtsi128_si32(h1);
        histogram[2] = _mm_cvtsi128_si32(h2);
        histogram[3] = _mm_cvtsi128_si32(h3);
        histogram[4] = width * height;

        // undo cumulative counts, by differencing
        histogram[4] -= histogram[3];
        histogram[3] -= histogram[2];
        histogram[2] -= histogram[1];
        histogram[1] -= histogram[0];
    }

    void ME_SAD_8x8_Block_Search_C(mfxU8 *pSrc, mfxU8 *pRef, int pitch, int xrange, int yrange,  mfxU16 *bestSAD, int *bestX, int *bestY)
    {
        const int SAD_SEARCH_VSTEP = 2;  // 1=FS 2=FHS
        for (int y = 0; y < yrange; y += SAD_SEARCH_VSTEP) {
            for (int x = 0; x < xrange; x += SAD_SEARCH_VSTEP) {/*x++) {*/
                mfxU8
                    *pr = pRef + (y * pitch) + x,
                    *ps = pSrc;
                mfxU16
                    SAD = 0;
                for (int i = 0; i < 8; i++) {
                    SAD += (mfxU16)abs(pr[0] - ps[0]);
                    SAD += (mfxU16)abs(pr[1] - ps[1]);
                    SAD += (mfxU16)abs(pr[2] - ps[2]);
                    SAD += (mfxU16)abs(pr[3] - ps[3]);
                    SAD += (mfxU16)abs(pr[4] - ps[4]);
                    SAD += (mfxU16)abs(pr[5] - ps[5]);
                    SAD += (mfxU16)abs(pr[6] - ps[6]);
                    SAD += (mfxU16)abs(pr[7] - ps[7]);
                    pr += pitch;
                    ps += pitch;
                }
                if (SAD < *bestSAD) {
                    *bestSAD = SAD;
                    *bestX = x;
                    *bestY = y;
                }
            }
        }
    }

    void ME_SAD_8x8_Block_Search_SSE4(mfxU8 *pSrc, mfxU8 *pRef, int pitch, int xrange, int yrange, mfxU16 *bestSAD, int *bestX, int *bestY)
    {
        __m128i
            s0 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[0 * pitch]), (__m128i *)&pSrc[1 * pitch]),
            s1 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[2 * pitch]), (__m128i *)&pSrc[3 * pitch]),
            s2 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[4 * pitch]), (__m128i *)&pSrc[5 * pitch]),
            s3 = _mm_loadh_epi64(_mm_loadl_epi64((__m128i *)&pSrc[6 * pitch]), (__m128i *)&pSrc[7 * pitch]);
        for (int y = 0; y < yrange; y += ET_SAD_SEARCH_VSTEP) {
            for (int x = 0; x < xrange; x += 8) {
                mfxU8
                    *pr = pRef + (y * pitch) + x;
                __m128i
                    r0 = _mm_loadu_si128((__m128i *)&pr[0 * pitch]),
                    r1 = _mm_loadu_si128((__m128i *)&pr[1 * pitch]),
                    r2 = _mm_loadu_si128((__m128i *)&pr[2 * pitch]),
                    r3 = _mm_loadu_si128((__m128i *)&pr[3 * pitch]),
                    r4 = _mm_loadu_si128((__m128i *)&pr[4 * pitch]),
                    r5 = _mm_loadu_si128((__m128i *)&pr[5 * pitch]),
                    r6 = _mm_loadu_si128((__m128i *)&pr[6 * pitch]),
                    r7 = _mm_loadu_si128((__m128i *)&pr[7 * pitch]);
                r0 = _mm_add_epi16(_mm_mpsadbw_epu8(r0, s0, 0), _mm_mpsadbw_epu8(r0, s0, 5));
                r1 = _mm_add_epi16(_mm_mpsadbw_epu8(r1, s0, 2), _mm_mpsadbw_epu8(r1, s0, 7));
                r2 = _mm_add_epi16(_mm_mpsadbw_epu8(r2, s1, 0), _mm_mpsadbw_epu8(r2, s1, 5));
                r3 = _mm_add_epi16(_mm_mpsadbw_epu8(r3, s1, 2), _mm_mpsadbw_epu8(r3, s1, 7));
                r4 = _mm_add_epi16(_mm_mpsadbw_epu8(r4, s2, 0), _mm_mpsadbw_epu8(r4, s2, 5));
                r5 = _mm_add_epi16(_mm_mpsadbw_epu8(r5, s2, 2), _mm_mpsadbw_epu8(r5, s2, 7));
                r6 = _mm_add_epi16(_mm_mpsadbw_epu8(r6, s3, 0), _mm_mpsadbw_epu8(r6, s3, 5));
                r7 = _mm_add_epi16(_mm_mpsadbw_epu8(r7, s3, 2), _mm_mpsadbw_epu8(r7, s3, 7));
                r0 = _mm_add_epi16(r0, r1);
                r2 = _mm_add_epi16(r2, r3);
                r4 = _mm_add_epi16(r4, r5);
                r6 = _mm_add_epi16(r6, r7);
                r0 = _mm_add_epi16(r0, r2);
                r4 = _mm_add_epi16(r4, r6);
                r0 = _mm_add_epi16(r0, r4);
                // kill every other SAD results, simulating search every two in X dimension
                r0 = _mm_or_si128(r0, _mm_load_si128((__m128i *)et_tab_twostep));
                // kill out-of-bound values
                if (xrange - x < 8)
                    r0 = _mm_or_si128(r0, _mm_load_si128((__m128i *)et_tab_killmask[xrange - x]));
                r0 = _mm_minpos_epu16(r0);
                mfxU16
                    SAD = (mfxU16)_mm_extract_epi16(r0, 0);
                if (SAD < *bestSAD) {
                    *bestSAD = SAD;
                    *bestX = x + _mm_extract_epi16(r0, 1);
                    *bestY = y;
                }
            }
        }
    }

    static inline void calc_RACA_4x4_C(mfxU8 *pSrc, mfxI32 pitch, mfxI32 *RS, mfxI32 *CS) {
        mfxI32 i, j;
        mfxU8 *pS = pSrc;
        mfxU8 *pS2 = pSrc + pitch;
        mfxI32 Rs, Cs;

        Cs = 0;
        Rs = 0;
        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 4; j++)
            {
                Cs += (pS[j] > pS[j + 1]) ? (pS[j] - pS[j + 1]) : (pS[j + 1] - pS[j]);
                Rs += (pS[j] > pS2[j]) ? (pS[j] - pS2[j]) : (pS2[j] - pS[j]);
            }
            pS += pitch;
            pS2 += pitch;
        }

        *CS += Cs >> 4;
        *RS += Rs >> 4;
    }

    mfxStatus Calc_RaCa_pic_C(mfxU8 *pPicY, mfxI32 width, mfxI32 height, mfxI32 pitch, mfxF64 &RsCs) {
        mfxI32 i, j;
        mfxI32 Rs, Cs;

        Rs = Cs = 0;
        for (i = 4; i < (height - 4); i += 4) {
            for (j = 4; j < (width - 4); j += 4) {
                calc_RACA_4x4_C(pPicY + i * pitch + j, pitch, &Rs, &Cs);
            }
        }

        mfxI32 w4 = (width - 8) >> 2;
        mfxI32 h4 = (height - 8) >> 2;
        mfxF64 d1 = 1.0 / (mfxF64)(w4*h4);
        mfxF64 drs = (mfxF64)Rs * d1;
        mfxF64 dcs = (mfxF64)Cs * d1;

        RsCs = sqrt(drs * drs + dcs * dcs);
        return MFX_ERR_NONE;
    }

    mfxStatus Calc_RaCa_pic_SSE4(mfxU8 *pPicY, mfxI32 width, mfxI32 height, mfxI32 pitch, mfxF64 &RsCs) {
        mfxU32
            count = 0;
        mfxU8*
            pY = pPicY + (4 * pitch) + 4;
        mfxI32
            RS = 0,
            CS = 0,
            i;
        for (i = 0; i < height - 8; i += 4)
        {
            // 4 horizontal blocks at a time
            mfxI32 j;
            for (j = 0; j < width - 20; j += 16)
            {
                __m128i rs = _mm_setzero_si128();
                __m128i cs = _mm_setzero_si128();
                __m128i c0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pY[0]));
                __m128i c1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pY[8]));

                for (mfxI32 k = 0; k < 4; k++)
                {
                    __m128i b0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pY[1]));
                    __m128i b1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pY[9]));
                    __m128i a0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pY[pitch + 0]));
                    __m128i a1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)&pY[pitch + 8]));
                    pY += pitch;

                    // Cs += (pS[j] > pS[j + 1]) ? (pS[j] - pS[j + 1]) : (pS[j + 1] - pS[j]);
                    b0 = _mm_abs_epi16(_mm_sub_epi16(c0, b0));
                    b1 = _mm_abs_epi16(_mm_sub_epi16(c1, b1));
                    b0 = _mm_hadd_epi16(b0, b1);
                    cs = _mm_add_epi16(cs, b0);

                    // Rs += (pS[j] > pS2[j]) ? (pS[j] - pS2[j]) : (pS2[j] - pS[j]);
                    c0 = _mm_abs_epi16(_mm_sub_epi16(c0, a0));
                    c1 = _mm_abs_epi16(_mm_sub_epi16(c1, a1));
                    c0 = _mm_hadd_epi16(c0, c1);
                    rs = _mm_add_epi16(rs, c0);

                    // reuse next iteration
                    c0 = a0;
                    c1 = a1;
                }

                // horizontal sum
                rs = _mm_hadd_epi16(rs, cs);
                //Cs >> 4; Rs >> 4;
                rs = _mm_srai_epi16(rs, 4);
                //*CS += Cs; *RS += Rs;
                rs = _mm_hadd_epi16(rs, rs);
                rs = _mm_hadd_epi16(rs, rs);
                RS += _mm_extract_epi16(rs, 0);
                CS += _mm_extract_epi16(rs, 1);

                pY -= 4 * pitch;
                pY += 16;
                count += 4;
            }

            // remaining blocks
            for (; j < width - 8; j += 4)
            {
                calc_RACA_4x4_C(pY, pitch, &RS, &CS);
                pY += 4;
                count++;
            }

            pY -= width - 8;
            pY += 4 * pitch;
        }
        mfxI32 w4 = (width - 8) >> 2;
        mfxI32 h4 = (height - 8) >> 2;
        mfxF64 d1 = 1.0 / (mfxF64)(w4*h4);
        mfxF64 drs = (mfxF64)RS * d1;
        mfxF64 dcs = (mfxF64)CS * d1;

        RsCs = sqrt(drs * drs + dcs * dcs);
        return MFX_ERR_NONE;
    }

    mfxStatus imageInit(ASCYUV *buffer) {
        if (!buffer)
            return MFX_ERR_NULL_PTR;
        memset(buffer, 0, sizeof(ASCYUV));
        return MFX_ERR_NONE;
    }

    mfxStatus nullifier(ASCimageData *Buffer) {
        mfxStatus
            sts = MFX_ERR_NONE;
        sts = imageInit(&Buffer->Image);
        SCD_CHECK_MFX_ERR(sts);
        memset(&Buffer->pInteger, 0, sizeof(ASCMVector));
        memset(&Buffer->Cs, 0, sizeof(Buffer->Cs));
        memset(&Buffer->Rs, 0, sizeof(Buffer->Rs));
        memset(&Buffer->Cs1, 0, sizeof(Buffer->Cs1));
        memset(&Buffer->Rs1, 0, sizeof(Buffer->Rs1));
        memset(&Buffer->RsCs, 0, sizeof(Buffer->RsCs));
        memset(&Buffer->SAD, 0, sizeof(Buffer->SAD));
        memset(&Buffer->PAQ, 0, sizeof(Buffer->PAQ));
        Buffer->Contrast = 0;
        Buffer->CsVal = 0;
        Buffer->RsVal = 0;
        return sts;
    }

    mfxStatus ImDetails_Init(ASCImDetails *Rdata) {
        if (!Rdata)
            return MFX_ERR_NULL_PTR;
        memset(Rdata, 0, sizeof(ASCImDetails));
        return MFX_ERR_NONE;
    }

    mfxStatus ASCTSCstat_Init(ASCTSCstat **logic) {
        for (int i = 0; i < TSCSTATBUFFER; i++)
        {
            try
            {
                logic[i] = new ASCTSCstat;
            }
            catch (...)
            {
                return MFX_ERR_MEMORY_ALLOC;
            }
        }
        return MFX_ERR_NONE;
    }

    static mfxI8
        PDISTTbl2[NumTSC*NumSC] =
    {
        2, 3, 3, 4, 4, 5, 5, 5, 5, 5,
        2, 2, 3, 3, 4, 4, 5, 5, 5, 5,
        1, 2, 2, 3, 3, 3, 4, 4, 5, 5,
        1, 1, 2, 2, 3, 3, 3, 4, 4, 5,
        1, 1, 2, 2, 3, 3, 3, 3, 3, 4,
        1, 1, 1, 2, 2, 3, 3, 3, 3, 3,
        1, 1, 1, 1, 2, 2, 3, 3, 3, 3,
        1, 1, 1, 1, 2, 2, 2, 3, 3, 3,
        1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };
    static mfxU32 lmt_sc2[NumSC] = { 112, 255, 512, 1536, 4096, 6144, 10752, 16384, 23040, UINT_MAX };
    static mfxU32 lmt_tsc2[NumTSC] = { 24, 48, 72, 96, 128, 160, 192, 224, 256, UINT_MAX };

    ASCTSCstat::ASCTSCstat():
        ssDCint(0),
        refDCint(0),
        m_distance(0),
        frameNum(0),
        scVal(0),
        tscVal(0),
        pdist(0),
        histogram(),
        Schg(0),
        last_shot_distance(0),
        ssDCval(0),
        refDCval(0),
        diffAFD(0),
        diffTSC(0),
        diffRsCsDiff(0),
        diffMVdiffVal(0),
        RecentHighMvCount(0),
        TSC0(0),
        RecentHighMvMap(),
        SCindex(0),
        TSCindex(0),
        Rs(0),
        Cs(0),
        Contrast(0),
        SC(0),
        TSC(0),
        RsDiff(0),
        CsDiff(0),
        RsCsDiff(0),
        MVdiffVal(0),
        AbsMVSize(0),
        AbsMVHSize(0),
        AbsMVVSize(0),
        gchDC(0),
        posBalance(0),
        negBalance(0),
        avgVal(0),
        mu_mv_mag_sq(0),
        MV0(0.0f),
        m_ltrMCFD(0.0f),
        tcor(0),
        mcTcor(0),
        AFD(0),
        gop_size(0),
        picType(0),
        lastFrameInShot(0),
        Gchg(0),
        repeatedFrame(0),
        firstFrame(0),
        copyFrameDelay(0),
        fadeIn(0),
        ltr_flag(0)
    {
    }

    ASCimageData::ASCimageData() :
        Image(),
        pInteger(),
        var(0),
        jtvar(0),
        mcjtvar(0),
        Contrast(0),
        CsVal(0),
        RsVal(0),
        tcor(0),
        mcTcor(0),
        avgval(0),
        Cs(),
        Rs(),
        Cs1(),
        Rs1(),
        RsCs(),
        SAD(),
        PAQ()
    {}

    ASCimageData::~ASCimageData()
    {
    }

    ASCimageData & ASCimageData::operator=(const ASCimageData & iData)
    {
        mfxU32
            imageSpaceSize  = iData.Image.extHeight * iData.Image.extWidth,
            mvSpaceSize     = (iData.Image.height * iData.Image.width) >> 6,
            texSpaceSize    = (iData.Image.height * iData.Image.width) >> 4;

        Image.extHeight     = iData.Image.extHeight;
        Image.extWidth      = iData.Image.extWidth;
        Image.hBorder       = iData.Image.hBorder;
        Image.wBorder       = iData.Image.wBorder;
        Image.height        = iData.Image.height;
        Image.width         = iData.Image.width;
        Image.pitch         = iData.Image.pitch;

        Contrast            = iData.Contrast;
        CsVal               = iData.CsVal;
        RsVal               = iData.RsVal;
        avgval              = iData.avgval;
        var                 = iData.var;
        jtvar               = iData.jtvar;
        mcjtvar             = iData.mcjtvar;
        tcor                = iData.tcor;
        mcTcor              = iData.mcTcor;

        std::copy(iData.Image.data, iData.Image.data + imageSpaceSize, Image.data);
        Image.Y = Image.data + ((Image.extWidth * Image.hBorder) + Image.wBorder);
        Image.U = nullptr;
        Image.V = nullptr;

        std::copy(iData.pInteger, iData.pInteger + mvSpaceSize, pInteger);

        std::copy(iData.Cs, iData.Cs + texSpaceSize, Cs);
        std::copy(iData.Rs, iData.Rs + texSpaceSize, Rs);
        std::copy(iData.Cs1, iData.Cs1 + texSpaceSize, Cs1);
        std::copy(iData.Rs1, iData.Rs1 + texSpaceSize, Rs1);
        std::copy(iData.RsCs, iData.RsCs + texSpaceSize, RsCs);
        std::copy(iData.SAD, iData.SAD + mvSpaceSize, SAD);
        std::copy(iData.PAQ, iData.PAQ + mvSpaceSize, PAQ);
        return *this;
    }

    mfxStatus ASCimageData::InitFrame() {
        Image.extHeight = ASC_SMALL_HEIGHT;
        Image.extWidth  = ASC_SMALL_WIDTH;
        Image.pitch     = ASC_SMALL_WIDTH;
        Image.height    = ASC_SMALL_HEIGHT;
        Image.width     = ASC_SMALL_WIDTH;
        Image.hBorder   = 0;
        Image.wBorder   = 0;
        Image.Y         = nullptr;
        Image.U         = nullptr;
        Image.V         = nullptr;

        //Pointer conf.
        Image.Y = Image.data + ((Image.pitch * Image.hBorder) + Image.wBorder);
        return MFX_ERR_NONE;
    }

    ASC::ASC() :
        m_gpuImPitch(0),
        m_threadsWidth(0),
        m_threadsHeight(0),

        m_gpuwidth(0),
        m_gpuheight(0),

        m_support(),
        m_dataIn(),
        m_videoData(),

        m_dataReady(0),
        m_cmDeviceAssigned(0),
        m_is_LTR_on(0),
        m_ASCinitialized(0),

        m_width(0),
        m_height(0),
        m_pitch(0),

        ltr_check_history(),
        m_AVX2_available(0),
        m_SSE4_available(0),
        GainOffset(),
        RsCsCalc_4x4(),
        RsCsCalc_bound(),
        RsCsCalc_diff(),
        ImageDiffHistogram(),
        ME_SAD_8x8_Block_Search(),
        Calc_RaCa_pic(),
        AGOP_RF(),
        resizeFunc(),
        m_TSC0(0),
        m_TSC_APQ(0)
    {}

    void ASC::Setup_Environment() {
        m_dataIn->accuracy = 1;

        m_dataIn->layer->Original_Width = ASC_SMALL_WIDTH;
        m_dataIn->layer->Original_Height = ASC_SMALL_HEIGHT;
        m_dataIn->layer->_cwidth = ASC_SMALL_WIDTH;
        m_dataIn->layer->_cheight = ASC_SMALL_HEIGHT;

        m_dataIn->layer->block_width = 8;
        m_dataIn->layer->block_height = 8;
        m_dataIn->layer->vertical_pad = 0;
        m_dataIn->layer->horizontal_pad = 0;
        m_dataIn->layer->Extended_Height = m_dataIn->layer->vertical_pad + ASC_SMALL_HEIGHT + m_dataIn->layer->vertical_pad;
        m_dataIn->layer->Extended_Width = m_dataIn->layer->horizontal_pad + ASC_SMALL_WIDTH + m_dataIn->layer->horizontal_pad;
        m_dataIn->layer->pitch = m_dataIn->layer->Extended_Width;
        m_dataIn->layer->Height_in_blocks = m_dataIn->layer->_cheight / m_dataIn->layer->block_height;
        m_dataIn->layer->Width_in_blocks = m_dataIn->layer->_cwidth / m_dataIn->layer->block_width;
        m_dataIn->layer->sidesize = m_dataIn->layer->_cheight + (1 * m_dataIn->layer->vertical_pad);
        m_dataIn->layer->initial_point = (m_dataIn->layer->Extended_Width * m_dataIn->layer->vertical_pad) + m_dataIn->layer->horizontal_pad;
        m_dataIn->layer->MVspaceSize = (m_dataIn->layer->_cheight / m_dataIn->layer->block_height) * (m_dataIn->layer->_cwidth / m_dataIn->layer->block_width);
    }

    mfxStatus ASC::Params_Init() {
        m_dataIn->accuracy = 1;
        m_dataIn->processed_frames = 0;
        m_dataIn->total_number_of_frames = -1;
        m_dataIn->starting_frame = 0;
        m_dataIn->key_frame_frequency = INT_MAX;
        m_dataIn->limitRange = 0;
        m_dataIn->maxXrange = 32;
        m_dataIn->maxYrange = 32;
        m_dataIn->interlaceMode = 0;
        m_dataIn->StartingField = ASCTopField;
        m_dataIn->currentField = ASCTopField;
        SCD_CHECK_MFX_ERR(ImDetails_Init(m_dataIn->layer));
        return MFX_ERR_NONE;
    }

    mfxStatus ASC::VidSample_Alloc()
    {
        for (mfxI32 i = 0; i < ASCVIDEOSTATSBUF; i++)
            SCD_CHECK_MFX_ERR(m_videoData[i]->layer.InitFrame());
        return MFX_ERR_NONE;
    }

    mfxStatus ASC::AssignResources(mfxU8 position, mfxU8 *pixelData)
    {
        if (!IsASCinitialized())
            return MFX_ERR_DEVICE_FAILED;
        if (pixelData == nullptr)
            return MFX_ERR_DEVICE_FAILED;
        m_videoData[position]->layer.Image.Y = pixelData;
        return MFX_ERR_NONE;
    }

    void ASC::VidSample_dispose()
    {
        for (mfxI32 i = ASCVIDEOSTATSBUF - 1; i >= 0; i--)
        {
            if (m_videoData[i] != nullptr)
                delete m_videoData[i];
        }
    }
    void ASC::VidRead_dispose()
    {
        if (m_support->logic != nullptr)
        {
            for (mfxI32 i = 0; i < TSCSTATBUFFER; i++)
                delete m_support->logic[i];
            delete[] m_support->logic;
        }
        if (m_support->gainCorrection != nullptr)
            delete m_support->gainCorrection;
    }

    void ASC::InitStruct() {
        m_dataIn = nullptr;
        m_support = nullptr;
        m_videoData = nullptr;
        resizeFunc = nullptr;
    }

    mfxStatus ASC::VidRead_Init() {
        m_support->control                  = 0;
        m_support->average                  = 0;
        m_support->avgSAD                   = 0;
        m_support->gopSize                  = 1;
        m_support->pendingSch               = 0;
        m_support->lastSCdetectionDistance  = 0;
        m_support->detectedSch              = 0;
        m_support->frameOrder               = 0;
        m_support->PDistanceTable           = PDISTTbl2;
        m_support->size                     = ASCSmall_Size;
        m_support->firstFrame               = true;
        try
        {
            m_support->logic            = new ASCTSCstat *[TSCSTATBUFFER];
            m_support->gainCorrection   = new ASCimageData;
        }
        catch (...)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
        SCD_CHECK_MFX_ERR(ASCTSCstat_Init(m_support->logic));
        SCD_CHECK_MFX_ERR(m_support->gainCorrection->InitFrame());
        return MFX_ERR_NONE;
    }

    mfxStatus ASC::VidSample_Init() {
        for (mfxI32 i = 0; i < ASCVIDEOSTATSBUF; i++) {
            SCD_CHECK_MFX_ERR(nullifier(&m_videoData[i]->layer));
            imageInit(&m_videoData[i]->layer.Image);
            m_videoData[i]->frame_number = -1;
            m_videoData[i]->forward_reference = -1;
            m_videoData[i]->backward_reference = -1;
        }
        return MFX_ERR_NONE;
    }

    void ASC::SetUltraFastDetection() {
        m_support->size = ASCSmall_Size;
        resizeFunc = &ASC::SubSampleASC_ImagePro;
    }

    mfxStatus ASC::SetWidth(mfxI32 Width) {
        if (Width < ASC_SMALL_WIDTH) {
            ASC_PRINTF("\nError: Width value is too small, it needs to be bigger than %i\n", ASC_SMALL_WIDTH);
            return MFX_ERR_UNSUPPORTED;
        }
        else
            m_width = Width;

        return MFX_ERR_NONE;
    }

    mfxStatus ASC::SetHeight(mfxI32 Height) {
        if (Height < ASC_SMALL_HEIGHT) {
            ASC_PRINTF("\nError: Height value is too small, it needs to be bigger than %i\n", ASC_SMALL_HEIGHT);
            return MFX_ERR_UNSUPPORTED;
        }
        else
            m_height = Height;

        return MFX_ERR_NONE;
    }

    mfxStatus ASC::SetPitch(mfxI32 Pitch) {
        if (m_width < ASC_SMALL_WIDTH) {
            ASC_PRINTF("\nError: Width value has not been set, init the variables first\n");
            return MFX_ERR_UNSUPPORTED;
        }

        if (Pitch < m_width) {
            ASC_PRINTF("\nError: Pitch value is too small, it needs to be bigger than %i\n", m_width);
            return MFX_ERR_UNSUPPORTED;
        }
        else
            m_pitch = Pitch;

        return MFX_ERR_NONE;
    }

    void ASC::SetNextField() {
        if (m_dataIn->interlaceMode != ASCprogressive_frame)
            m_dataIn->currentField = !m_dataIn->currentField;
    }

    mfxStatus ASC::SetDimensions(mfxI32 Width, mfxI32 Height, mfxI32 Pitch) {
        mfxStatus sts;
        sts = SetWidth(Width);
        SCD_CHECK_MFX_ERR(sts);
        sts = SetHeight(Height);
        SCD_CHECK_MFX_ERR(sts);
        sts = SetPitch(Pitch);
        SCD_CHECK_MFX_ERR(sts);
        return sts;
    }

    mfxStatus ASC::SetInterlaceMode(ASCFTS interlaceMode) {
        if (interlaceMode > ASCbotfieldFirst_frame) {
            ASC_PRINTF("\nError: Interlace Mode invalid, valid values are: 1 (progressive), 2 (TFF), 3 (BFF)\n");
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        else
            m_dataIn->interlaceMode = interlaceMode;

        m_dataIn->StartingField = ASCTopField;
        if (m_dataIn->interlaceMode != ASCprogressive_frame) {
            if (m_dataIn->interlaceMode == ASCbotfieldFirst_frame)
                m_dataIn->StartingField = ASCBottomField;
            resizeFunc = &ASC::SubSampleASC_ImageInt;
        }
        else {
            resizeFunc = &ASC::SubSampleASC_ImagePro;
        }
        m_dataIn->currentField = m_dataIn->StartingField;
        return MFX_ERR_NONE;
    }

#define ET_ASC_CPU_DISP_INIT_C(func)           (func = (func ## _C))
#define ET_ASC_CPU_DISP_INIT_SSE4(func)        (func = (func ## _SSE4))
#define ET_ASC_CPU_DISP_INIT_SSE4_C(func)      (m_SSE4_available ? ET_ASC_CPU_DISP_INIT_SSE4(func) : ET_ASC_CPU_DISP_INIT_C(func))

    mfxStatus ASC::Init(mfxI32 Width, mfxI32 Height, mfxI32 Pitch, mfxU32 /*PicStruct*/, bool IsYUV, mfxU32 CodecId)
    {
        m_AVX2_available = 0;// CpuFeature_AVX2();
        m_SSE4_available = CpuFeature_SSE41();
        ET_ASC_CPU_DISP_INIT_C(GainOffset);
        ET_ASC_CPU_DISP_INIT_SSE4_C(RsCsCalc_4x4);
        ET_ASC_CPU_DISP_INIT_C(RsCsCalc_bound);
        ET_ASC_CPU_DISP_INIT_C(RsCsCalc_diff);
        ET_ASC_CPU_DISP_INIT_SSE4_C(ImageDiffHistogram);
        ET_ASC_CPU_DISP_INIT_SSE4_C(ME_SAD_8x8_Block_Search);
        ET_ASC_CPU_DISP_INIT_SSE4_C(Calc_RaCa_pic);

        InitStruct();
        try
        {
            m_dataIn = new ASCVidData;
        }
        catch (...)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
        m_dataIn->layer = nullptr;
        try
        {
            m_dataIn->layer = new ASCImDetails;
            m_videoData     = new ASCVidSample *[ASCVIDEOSTATSBUF];
            for (mfxU8 i = 0; i < ASCVIDEOSTATSBUF; i++)
            {
                try
                {
                    m_videoData[i] = new ASCVidSample;
                }
                catch (...)
                {
                    return MFX_ERR_MEMORY_ALLOC;
                }
            }
            m_support = new ASCVidRead;
        }
        catch (...)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }

        Params_Init();

        SCD_CHECK_MFX_ERR(SetDimensions(Width, Height, Pitch));
        ColorFormatYUV   = IsYUV;
        m_gpuwidth       = Width;
        m_gpuheight      = Height;
        SCD_CHECK_MFX_ERR(VidSample_Init());
        Setup_Environment();
        SCD_CHECK_MFX_ERR(VidSample_Alloc());
        SCD_CHECK_MFX_ERR(VidRead_Init());
        SCD_CHECK_MFX_ERR(SetInterlaceMode(ASCprogressive_frame));
        m_dataReady      = false;
        m_ASCinitialized = true;
        if (CodecId == MFX_CODEC_HEVC || CodecId == MFX_CODEC_AV1)
            AGOP_RF = AGOPHEVCSelectRF;
        else if (CodecId == MFX_CODEC_AVC)
            AGOP_RF = AGOPSelectRF;
        else
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        return MFX_ERR_NONE;
    }

    bool ASC::IsASCinitialized() {
        return m_ASCinitialized;
    }

    void ASC::SetControlLevel(mfxU8 level) {
        if (level >= RF_DECISION_LEVEL) {
            ASC_PRINTF("\nWarning: Control level too high, shot change detection disabled! (%i)\n", level);
            ASC_PRINTF("Control levels 0 to %i, smaller value means more sensitive detection\n", RF_DECISION_LEVEL);
        }
        m_support->control = level;
    }

    mfxStatus ASC::SetGoPSize(mfxU32 GoPSize) {
        if (GoPSize > Double_HEVC_Gop) {
            ASC_PRINTF("\nError: GoPSize is too big! (%i)\n", GoPSize);
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        else if (GoPSize == Forbidden_GoP) {
            ASC_PRINTF("\nError: GoPSize value cannot be zero!\n");
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        else if (GoPSize > HEVC_Gop && GoPSize <= Double_HEVC_Gop) {
            ASC_PRINTF("\nWarning: Your GoPSize is larger than usual! (%i)\n", GoPSize);
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        m_support->gopSize = GoPSize;
        m_support->pendingSch = 0;

        return MFX_ERR_NONE;
    }

    void ASC::ResetGoPSize() {
        SetGoPSize(Immediate_GoP);
    }

    void ASC::Close() {
        if (m_videoData != nullptr) {
            VidSample_dispose();
            delete[] m_videoData;
            m_videoData = nullptr;
        }

        if (m_support != nullptr) {
            VidRead_dispose();
            delete m_support;
            m_support = nullptr;
        }

        if (m_dataIn != nullptr) {
            delete m_dataIn->layer;
            delete m_dataIn;
            m_dataIn = nullptr;
        }
    }

    void ASC::SubSampleASC_ImagePro(mfxU8 *frame, mfxI32 srcWidth, mfxI32 srcHeight, mfxI32 inputPitch, ASCLayers dstIdx, mfxU32 /*parity*/) {

        ASCImDetails *pIDetDst = &m_dataIn->layer[dstIdx];
        mfxU8 *pDst = m_videoData[ASCCurrent_Frame]->layer.Image.Y;
        mfxI16& avgLuma = m_videoData[ASCCurrent_Frame]->layer.avgval;

        mfxI32 dstWidth = pIDetDst->Original_Width;
        mfxI32 dstHeight = pIDetDst->Original_Height;
        mfxI32 dstPitch = pIDetDst->pitch;

        SubSample_Point(frame, srcWidth, srcHeight, inputPitch, pDst, dstWidth, dstHeight, dstPitch, avgLuma);
    }

    void ASC::SubSampleASC_ImageInt(mfxU8 *frame, mfxI32 srcWidth, mfxI32 srcHeight, mfxI32 inputPitch, ASCLayers dstIdx, mfxU32 parity) {

        ASCImDetails *pIDetDst = &m_dataIn->layer[dstIdx];
        mfxU8 *pDst = m_videoData[ASCCurrent_Frame]->layer.Image.Y;
        mfxI16 &avgLuma = m_videoData[ASCCurrent_Frame]->layer.avgval;

        mfxI32 dstWidth = pIDetDst->Original_Width;
        mfxI32 dstHeight = pIDetDst->Original_Height;
        mfxI32 dstPitch = pIDetDst->pitch;

        SubSample_Point(frame + (parity * inputPitch), srcWidth, srcHeight / 2, inputPitch * 2, pDst, dstWidth, dstHeight, dstPitch, avgLuma);
    }

    //
    // SubSample pSrc into pDst, using point-sampling of source pixels
    // Corrects the position on odd lines in case the input video is
    // interlaced
    //
    void ASC::SubSample_Point(
        mfxU8* pSrc, mfxU32 srcWidth, mfxU32 srcHeight, mfxU32 srcPitch,
        mfxU8* pDst, mfxU32 dstWidth, mfxU32 dstHeight, mfxU32 dstPitch,
        mfxI16 &avgLuma) {
        mfxI32 step_w = srcWidth / dstWidth;
        mfxI32 offset = 0;
        if (!ColorFormatYUV) {
            step_w *= 4;
            offset = 2; //Green channel
        }

        mfxI32 step_h = srcHeight / dstHeight;

        mfxI32 need_correction = !(step_h % 2);
        mfxI32 correction = 0;
        mfxU32 sumAll = 0;
        mfxI32 y = 0;

        for (y = 0; y < (mfxI32)dstHeight; y++) {
            correction = (y % 2) & need_correction;
            for (mfxI32 x = 0; x < (mfxI32)dstWidth; x++) {

                mfxU8* ps = pSrc + ((y * step_h + correction) * srcPitch) + (x * step_w) + offset;
                mfxU8* pd = pDst + (y * dstPitch) + x;

                pd[0] = ps[0];
                sumAll += ps[0];
            }
        }
        avgLuma = (mfxI16)(sumAll >> 13);
    }

    mfxStatus ASC::RsCsCalc() {
        ASCYUV
            *pFrame = &m_videoData[ASCCurrent_Frame]->layer.Image;
        ASCImDetails
            vidCar = m_dataIn->layer[0];
        mfxU8*
            ss = pFrame->Y;
        mfxU32
            hblocks = (pFrame->height >> BLOCK_SIZE_SHIFT) /*- 2*/,
            wblocks = (pFrame->width >> BLOCK_SIZE_SHIFT) /*- 2*/;

        mfxI16
            diff = m_videoData[ASCReference_Frame]->layer.avgval - m_videoData[ASCCurrent_Frame]->layer.avgval;
        ss = m_videoData[ASCReference_Frame]->layer.Image.Y;
        if (!m_support->firstFrame && abs(diff) >= GAINDIFF_THR) {
            if (m_support->gainCorrection->Image.Y == nullptr)
                return MFX_ERR_MEMORY_ALLOC;
            GainOffset(&ss, &m_support->gainCorrection->Image.Y, (mfxU16)vidCar._cwidth, (mfxU16)vidCar._cheight, (mfxU16)vidCar.Extended_Width, diff);
        }
        ss = m_videoData[ASCCurrent_Frame]->layer.Image.Y;

        RsCsCalc_4x4(ss, pFrame->pitch, wblocks, hblocks, m_videoData[ASCCurrent_Frame]->layer.Rs, m_videoData[ASCCurrent_Frame]->layer.Cs);
        RsCsCalc_bound(m_videoData[ASCCurrent_Frame]->layer.Rs, m_videoData[ASCCurrent_Frame]->layer.Cs, m_videoData[ASCCurrent_Frame]->layer.RsCs, &m_videoData[ASCCurrent_Frame]->layer.RsVal, &m_videoData[ASCCurrent_Frame]->layer.CsVal, &m_videoData[ASCCurrent_Frame]->layer.Contrast, wblocks, hblocks);
        RsCsCalc_4x4_NoBlkShift(ss, pFrame->pitch, wblocks, hblocks, m_videoData[ASCCurrent_Frame]->layer.Rs1, m_videoData[ASCCurrent_Frame]->layer.Cs1);
        return MFX_ERR_NONE;
    }

    bool Hint_LTR_op_on(mfxU32 SC, mfxU32 TSC) {
        bool ltr = TSC *TSC < (std::max(SC, (mfxU32)64) / 12);
        return ltr;
    }

    mfxI32 ASC::ShotDetect(ASCimageData& Data, ASCimageData& DataRef, ASCImDetails& imageInfo, ASCTSCstat *current, ASCTSCstat *reference, mfxU8 controlLevel) {
        mfxU8
            *ssFrame = Data.Image.Y,
            *refFrame = DataRef.Image.Y;
        mfxU16
            *objRs = Data.Rs,
            *objCs = Data.Cs,
            *refRs = DataRef.Rs,
            *refCs = DataRef.Cs;

        current->RsCsDiff = 0;
        current->Schg = -1;
        current->Gchg = 0;

        RsCsCalc_diff(objRs, objCs, refRs, refCs, 2 * imageInfo.Width_in_blocks, 2 * imageInfo.Height_in_blocks, &current->RsDiff, &current->CsDiff);
        ImageDiffHistogram(ssFrame, refFrame, imageInfo.Extended_Width, imageInfo._cwidth, imageInfo._cheight, current->histogram, &current->ssDCint, &current->refDCint);

        if (reference->Schg)
            current->last_shot_distance = 1;
        else
            current->last_shot_distance++;

        current->RsDiff >>= 9;
        current->CsDiff >>= 9;
        current->RsCsDiff = (current->RsDiff*current->RsDiff) + (current->CsDiff*current->CsDiff);
        current->ssDCval = (mfxI32)current->ssDCint >> 13;
        current->refDCval = (mfxI32)current->refDCint >> 13;
        current->gchDC = std::abs(current->ssDCval - current->refDCval);
        current->posBalance = (current->histogram[3] + current->histogram[4]) >> 6;
        current->negBalance = (current->histogram[0] + current->histogram[1]) >> 6;
        current->diffAFD = current->AFD - reference->AFD;
        current->diffTSC = current->TSC - reference->TSC;
        current->diffRsCsDiff = current->RsCsDiff - reference->RsCsDiff;
        current->diffMVdiffVal = current->MVdiffVal - reference->MVdiffVal;
        mfxI32
            SChange = SCDetectRF(
                current->diffMVdiffVal, current->RsCsDiff, current->MVdiffVal,
                current->Rs, current->AFD, current->CsDiff,
                current->diffTSC, current->TSC, current->gchDC,
                current->diffRsCsDiff, current->posBalance, current->SC,
                current->TSCindex, current->SCindex, current->Cs,
                current->diffAFD, current->negBalance, current->ssDCval,
                current->refDCval, current->RsDiff, controlLevel);

#if ASCTUNEDATA
        {
            FILE *dataFile = NULL;
            fopen_s(&dataFile, "stats_shotdetect.txt", "a+");
            fprintf(dataFile, "%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\t%i\n",
                current->frameNum,
                current->Rs, current->Cs, current->SC,
                current->AFD, current->TSC, current->RsDiff,
                current->CsDiff, current->RsCsDiff, current->MVdiffVal,
                current->avgVal, current->ssDCval, current->refDCval,
                current->gchDC, current->posBalance, current->negBalance,
                current->diffAFD, current->diffTSC, current->diffRsCsDiff,
                current->diffMVdiffVal, current->SCindex, current->TSCindex,
                current->tcor, current->mcTcor, current->mu_mv_mag_sq, SChange);
            fclose(dataFile);
        }
#endif
        current->ltr_flag = Hint_LTR_op_on(current->SC, current->TSC);
        return SChange;
    }

    mfxU16  ME_simple(ASCVidRead *videoIn, mfxI32 fPos, ASCImDetails *dataIn, ASCimageData *scale, ASCimageData *scaleRef, bool /*first*/, ASCVidData *limits, t_ME_SAD_8x8_Block_Search ME_SAD_8x8_Block_Search);

    typedef std::function<bool(
        std::pair<mfxU32, mfxU32>,
        std::pair<mfxU32, mfxU32>)>
        MV_Comparator;

    void ASC::MotionAnalysis(ASCVidSample *videoIn, ASCVidSample *videoRef, mfxU32 *TSC, mfxU16 *AFD, mfxU32 *MVdiffVal, mfxU32 *AbsMVSize, mfxU32 *AbsMVHSize, mfxU32 *AbsMVVSize, ASCLayers lyrIdx, float *mvSum)
    {
        MV_Comparator Comparison = [](
            std::pair<mfxU32, mfxU32> mv_size1,
            std::pair<mfxU32, mfxU32> mv_size2
            )
        {
            return mv_size1.second != mv_size2.second ? mv_size1.second > mv_size2.second : mv_size1.first < mv_size2.first;
        };
        mfxU32//24bit is enough
            valb = 0;
        mfxU32
            acc = 0;
        /*--Motion Estimation--*/
        *MVdiffVal = 0;
        *AbsMVSize = 0;
        *AbsMVHSize = 0;
        *AbsMVVSize = 0;
        std::map<mfxU32, mfxU32>
            mv_freq;
        mfxI16
            diff = (int)videoIn->layer.avgval - (int)videoRef->layer.avgval;

        ASCimageData
            *referenceImageIn = &videoRef->layer;
        mfxU32
            wblocks = (videoIn->layer.Image.width >> BLOCK_SIZE_SHIFT);

        if (abs(diff) >= GAINDIFF_THR) {
            referenceImageIn = m_support->gainCorrection;
        }
        m_support->average = 0;
        videoIn->layer.var = 0;
        videoIn->layer.jtvar = 0;
        videoIn->layer.mcjtvar = 0;

        uint32_t mv2 = 0;
        for (mfxU16 i = 0; i < m_dataIn->layer[lyrIdx].Height_in_blocks; i++)
        {
            mfxU16 prevFPos = i << 4;
            for (mfxU16 j = 0; j < m_dataIn->layer[lyrIdx].Width_in_blocks; j++)
            {
                mfxU16 fPos = prevFPos + j;
                acc += ME_simple(m_support, fPos, m_dataIn->layer, &videoIn->layer, referenceImageIn, true, m_dataIn, ME_SAD_8x8_Block_Search);
                mfxF32 Rs2 = (mfxF32)(videoIn->layer.Rs1[i * 2 * wblocks + j * 2] 
                                    + videoIn->layer.Rs1[i * 2 * wblocks + j * 2 + 1] 
                                    + videoIn->layer.Rs1[(i * 2 + 1) * wblocks + j * 2] 
                                    + videoIn->layer.Rs1[(i * 2 + 1) * wblocks + j * 2 + 1]);
                mfxF32 Cs2 = (mfxF32)(videoIn->layer.Cs1[i * 2 * wblocks + j * 2] 
                                    + videoIn->layer.Cs1[i * 2 * wblocks + j * 2 + 1] 
                                    + videoIn->layer.Cs1[(i * 2 + 1) * wblocks + j * 2] 
                                    + videoIn->layer.Cs1[(i * 2 + 1) * wblocks + j * 2 + 1]);
                mfxF32 SC = sqrtf(Rs2 + Cs2) / 1.414f;

                if (SC > 4 && videoIn->layer.SAD[fPos] < SC) {
                    videoIn->layer.PAQ[fPos] = 1;
                }
                else {
                    videoIn->layer.PAQ[fPos] = 0;
                }
                valb += videoIn->layer.SAD[fPos];
                *MVdiffVal += (videoIn->layer.pInteger[fPos].x - videoRef->layer.pInteger[fPos].x) * (videoIn->layer.pInteger[fPos].x - videoRef->layer.pInteger[fPos].x);
                *MVdiffVal += (videoIn->layer.pInteger[fPos].y - videoRef->layer.pInteger[fPos].y) * (videoIn->layer.pInteger[fPos].y - videoRef->layer.pInteger[fPos].y);
                *AbsMVHSize += (videoIn->layer.pInteger[fPos].x * videoIn->layer.pInteger[fPos].x);
                *AbsMVVSize += (videoIn->layer.pInteger[fPos].y * videoIn->layer.pInteger[fPos].y);
                mfxU32
                    mv_size = (videoIn->layer.pInteger[fPos].x * videoIn->layer.pInteger[fPos].x) + (videoIn->layer.pInteger[fPos].y * videoIn->layer.pInteger[fPos].y);
                *AbsMVSize += mv_size;
                mv_freq[mv_size]++;
                // metric is used for ALTR
                mv2 += mv_size;
                //printf("pos=%d mv=%d\n", fPos, mv2);
            }
        }

        std::set<std::pair<mfxU32, mfxU32>, MV_Comparator>
            mv_size_sorted(mv_freq.begin(), mv_freq.end(), Comparison);
        videoIn->layer.var = videoIn->layer.var * 10 / 128 / 64;
        videoIn->layer.jtvar = videoIn->layer.jtvar * 10 / 128 / 64;
        videoIn->layer.mcjtvar = videoIn->layer.mcjtvar * 10 / 128 / 64;
        m_support->mu_mv_mag_sq = mv_size_sorted.begin()->first;
        if (!m_support->mu_mv_mag_sq)
            m_support->mu_mv_mag_sq = mv_size_sorted.begin()->second;

        m_support->average >>= 7;
        *AbsMVHSize >>= 7;
        *AbsMVVSize >>= 7;
        //*AbsMVSize >>= 7;

        if (videoIn->layer.var == 0)
        {
            if (videoIn->layer.jtvar == 0)
                videoIn->layer.tcor = 100;
            else
                videoIn->layer.tcor = (mfxI16)std::min(1000 * videoIn->layer.jtvar, 2000);

            if (videoIn->layer.mcjtvar == 0)
                videoIn->layer.mcTcor = 100;
            else
                videoIn->layer.mcTcor = (mfxI16)std::min(1000 * videoIn->layer.mcjtvar, 2000);
        }
        else
        {
            videoIn->layer.tcor = (mfxI16)(100 * videoIn->layer.jtvar / videoIn->layer.var);
            videoIn->layer.mcTcor = (mfxI16)(100 * videoIn->layer.mcjtvar / videoIn->layer.var);
        }
        m_TSC0 = valb;
        m_TSC_APQ = valb;
        *TSC = valb >> 8;
        *AFD = (mfxU16)(acc >> 13);//Picture area is 2^13, and 10 have been done before so it needs to shift 3 more.
        *MVdiffVal = *MVdiffVal >> 7;
        *mvSum = (float)mv2;
    }

    mfxU32 TableLookUp(mfxU32 limit, mfxU32 *table, mfxU32 comparisonValue) {
        for (mfxU32 pos = 0; pos < limit; pos++) {
            if (comparisonValue < table[pos])
                return pos;
        }
        return limit;
    }

    void CorrectionForGoPSize(ASCVidRead *m_support, mfxU32 PdIndex) {
        m_support->detectedSch = 0;
        if (m_support->logic[PdIndex]->Schg) {
            if (m_support->lastSCdetectionDistance % m_support->gopSize)
                m_support->pendingSch = 1;
            else {
                m_support->lastSCdetectionDistance = 0;
                m_support->pendingSch = 0;
                m_support->detectedSch = 1;
            }
        }
        else if (m_support->pendingSch) {
            if (!(m_support->lastSCdetectionDistance % m_support->gopSize)) {
                m_support->lastSCdetectionDistance = 0;
                m_support->pendingSch = 0;
                m_support->detectedSch = 1;
            }
        }
        m_support->lastSCdetectionDistance++;
    }

    bool ASC::CompareStats(mfxU8 current, mfxU8 reference) {
        if (current > 2 || reference > 2 || current == reference) {
            ASC_PRINTF("Error: Invalid stats comparison\n");
            assert(!"Error: Invalid stats comparison");
        }
        mfxU8 comparison = 0;
        if (m_dataIn->interlaceMode == ASCprogressive_frame) {
            comparison += m_support->logic[current]->AFD == 0;
            comparison += m_support->logic[current]->RsCsDiff == 0;
            comparison += m_support->logic[current]->TSCindex == 0;
            comparison += m_support->logic[current]->negBalance <= 3;
            comparison += m_support->logic[current]->posBalance <= 20;
            comparison += ((m_support->logic[current]->diffAFD <= 0) && (m_support->logic[current]->diffTSC <= 0));
            comparison += (m_support->logic[current]->diffAFD <= m_support->logic[current]->diffTSC);

            if (comparison == 7)
                return Same;
        }
        else if ((m_dataIn->interlaceMode == ASCbotfieldFirst_frame) || (m_dataIn->interlaceMode == ASCtopfieldfirst_frame)) {
            comparison += m_support->logic[current]->AFD == m_support->logic[current]->TSC;
            comparison += m_support->logic[current]->AFD <= 9;
            comparison += m_support->logic[current]->gchDC <= 1;
            comparison += m_support->logic[current]->RsCsDiff <= 9;
            comparison += ((m_support->logic[current]->diffAFD <= 1) && (m_support->logic[current]->diffTSC <= 1));
            comparison += (m_support->logic[current]->diffAFD <= m_support->logic[current]->diffTSC);

            if (comparison == 6)
                return Same;
        }
        else {
            ASC_PRINTF("Error: Invalid interlace mode for stats comparison\n");
            assert(!"Error: Invalid interlace mode for stats comparison\n");
        }

        return Not_same;
    }

    bool ASC::FrameRepeatCheck() {
        mfxU8 reference = ASCprevious_frame_data;
        if (m_dataIn->interlaceMode > ASCprogressive_frame)
            reference = ASCprevious_previous_frame_data;
        return(CompareStats(ASCcurrent_frame_data, reference));
    }

    mfxU16 ASC::ML_SelectGoPSize()
    {
        mfxU16
            result = 8;
        /*std::pair<mfxU16, mfxU16>
            gopselection =*/
        if (m_support->logic[ASCcurrent_frame_data]->repeatedFrame)
            result = MAX_BSTRUCTRUE_GOP_SIZE;
        else
            result = (mfxU16)AGOP_RF(
                m_support->logic[ASCcurrent_frame_data]->diffMVdiffVal,
                m_support->logic[ASCcurrent_frame_data]->RsCsDiff,
                m_support->logic[ASCcurrent_frame_data]->MVdiffVal,
                m_support->logic[ASCcurrent_frame_data]->Rs,
                m_support->logic[ASCcurrent_frame_data]->AFD,
                m_support->logic[ASCcurrent_frame_data]->CsDiff,
                m_support->logic[ASCcurrent_frame_data]->diffTSC,
                m_support->logic[ASCcurrent_frame_data]->TSC,
                m_support->logic[ASCcurrent_frame_data]->gchDC,
                m_support->logic[ASCcurrent_frame_data]->diffRsCsDiff,
                m_support->logic[ASCcurrent_frame_data]->posBalance,
                m_support->logic[ASCcurrent_frame_data]->SC,
                m_support->logic[ASCcurrent_frame_data]->TSCindex,
                m_support->logic[ASCcurrent_frame_data]->SCindex,
                m_support->logic[ASCcurrent_frame_data]->Cs,
                m_support->logic[ASCcurrent_frame_data]->diffAFD,
                m_support->logic[ASCcurrent_frame_data]->negBalance,
                m_support->logic[ASCcurrent_frame_data]->ssDCval,
                m_support->logic[ASCcurrent_frame_data]->refDCval,
                m_support->logic[ASCcurrent_frame_data]->RsDiff,
                m_support->logic[ASCcurrent_frame_data]->mu_mv_mag_sq,
                m_support->logic[ASCcurrent_frame_data]->mcTcor,
                m_support->logic[ASCcurrent_frame_data]->tcor);

        mfxI64
            val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->diffMVdiffVal - (mfxI32)m_support->logic[ASCprevious_frame_data]->diffMVdiffVal);
        val *= val;
        mfxU64
            distance = (mfxU64)val;
        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->RsCsDiff - (mfxI32)m_support->logic[ASCprevious_frame_data]->RsCsDiff);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->MVdiffVal - (mfxI32)m_support->logic[ASCprevious_frame_data]->MVdiffVal);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->Rs - (mfxI32)m_support->logic[ASCprevious_frame_data]->Rs);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->AFD - (mfxI32)m_support->logic[ASCprevious_frame_data]->AFD);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->CsDiff - (mfxI32)m_support->logic[ASCprevious_frame_data]->CsDiff);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->diffTSC - (mfxI32)m_support->logic[ASCprevious_frame_data]->diffTSC);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->TSC - (mfxI32)m_support->logic[ASCprevious_frame_data]->TSC);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->gchDC - (mfxI32)m_support->logic[ASCprevious_frame_data]->gchDC);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->diffRsCsDiff - (mfxI32)m_support->logic[ASCprevious_frame_data]->diffRsCsDiff);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->posBalance - (mfxI32)m_support->logic[ASCprevious_frame_data]->posBalance);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->SC - (mfxI32)m_support->logic[ASCprevious_frame_data]->SC);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->diffAFD - (mfxI32)m_support->logic[ASCprevious_frame_data]->diffAFD);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->negBalance - (mfxI32)m_support->logic[ASCprevious_frame_data]->negBalance);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->ssDCval - (mfxI32)m_support->logic[ASCprevious_frame_data]->ssDCval);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->refDCval - (mfxI32)m_support->logic[ASCprevious_frame_data]->refDCval);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->RsDiff - (mfxI32)m_support->logic[ASCprevious_frame_data]->RsDiff);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->mu_mv_mag_sq - (mfxI32)m_support->logic[ASCprevious_frame_data]->mu_mv_mag_sq);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->mcTcor - (mfxI32)m_support->logic[ASCprevious_frame_data]->mcTcor);
        val *= val;
        distance += (mfxU64)val;

        val = ((mfxI32)m_support->logic[ASCcurrent_frame_data]->tcor - (mfxI32)m_support->logic[ASCprevious_frame_data]->tcor);
        val *= val;
        distance += (mfxU64)val;

        m_support->logic[ASCcurrent_frame_data]->m_distance = distance;

        return (result);
    }

    mfxU16 ASC::GetFrameGopSize()
    {
        return m_support->logic[ASCprevious_frame_data]->gop_size; //this is done after buffer rotation
    }

    void ASC::DetectShotChangeFrame(bool hasLTR) {
        m_support->logic[ASCcurrent_frame_data]->frameNum = m_videoData[ASCCurrent_Frame]->frame_number;
        m_support->logic[ASCcurrent_frame_data]->firstFrame = m_support->firstFrame;
        m_support->logic[ASCcurrent_frame_data]->avgVal = m_videoData[ASCCurrent_Frame]->layer.avgval;
        /*---------RsCs data--------*/
        m_support->logic[ASCcurrent_frame_data]->Rs = m_videoData[ASCCurrent_Frame]->layer.RsVal;
        m_support->logic[ASCcurrent_frame_data]->Cs = m_videoData[ASCCurrent_Frame]->layer.CsVal;
        m_support->logic[ASCcurrent_frame_data]->Contrast = m_videoData[ASCCurrent_Frame]->layer.Contrast;
        m_support->logic[ASCcurrent_frame_data]->SC = m_videoData[ASCCurrent_Frame]->layer.RsVal + m_videoData[ASCCurrent_Frame]->layer.CsVal;
        if (m_support->firstFrame) {
            m_support->logic[ASCcurrent_frame_data]->TSC                = 0;
            m_support->logic[ASCcurrent_frame_data]->TSC0               = 0;
            m_support->logic[ASCcurrent_frame_data]->RecentHighMvCount  = 0;
            for (mfxI32 i = 0; i < 8; i++)
            {
                m_support->logic[ASCcurrent_frame_data]->RecentHighMvMap[i] = 0;
            }
            m_support->logic[ASCcurrent_frame_data]->AFD                = 0;
            m_support->logic[ASCcurrent_frame_data]->TSCindex           = 0;
            m_support->logic[ASCcurrent_frame_data]->SCindex            = 0;
            m_support->logic[ASCcurrent_frame_data]->Schg               = 0;
            m_support->logic[ASCcurrent_frame_data]->Gchg               = 0;
            m_support->logic[ASCcurrent_frame_data]->picType            = 0;
            m_support->logic[ASCcurrent_frame_data]->lastFrameInShot    = 0;
            m_support->logic[ASCcurrent_frame_data]->pdist              = 0;
            m_support->logic[ASCcurrent_frame_data]->MVdiffVal          = 0;
            m_support->logic[ASCcurrent_frame_data]->RsCsDiff           = 0;
            m_support->logic[ASCcurrent_frame_data]->last_shot_distance = 0;
            m_support->logic[ASCcurrent_frame_data]->tcor               = 0;
            m_support->logic[ASCcurrent_frame_data]->mcTcor             = 0;
            m_support->firstFrame = false;
        }
        else {
            /*--------Motion data-------*/
            mfxF32 mv0 = 0;
            MotionAnalysis(
                m_videoData[ASCCurrent_Frame],
                m_videoData[ASCReference_Frame],
                &m_support->logic[ASCcurrent_frame_data]->TSC,
                &m_support->logic[ASCcurrent_frame_data]->AFD,
                &m_support->logic[ASCcurrent_frame_data]->MVdiffVal,
                &m_support->logic[ASCcurrent_frame_data]->AbsMVSize,
                &m_support->logic[ASCcurrent_frame_data]->AbsMVHSize,
                &m_support->logic[ASCcurrent_frame_data]->AbsMVVSize,
                (ASCLayers)0,
                &mv0);

            //int hasLTR = 0;
            m_support->logic[ASCcurrent_frame_data]->MV0 = mv0;

            mfxI32 countMvMap = 0;
            for (mfxI32 i = 0; i < 8; i++)
            {
                if (m_support->logic[ASCcurrent_frame_data]->RecentHighMvMap[i])
                    countMvMap++;
            }
            m_support->logic[ASCcurrent_frame_data]->RecentHighMvCount = countMvMap;
            mfxI32 idx8 = m_dataIn->processed_frames % 8;
            m_support->logic[ASCcurrent_frame_data]->RecentHighMvMap[idx8] = (mv0 > 1400) ? 1 : 0;
            if (hasLTR) {
                if (mv0 > 2500) {
                    //m_support->logic[ASCcurrent_frame_data]->m_ltrMCFD = Compute_MCFD(src, srcMv, input->m_sceneStatsLtr->Y);
                    m_support->logic[ASCcurrent_frame_data]->m_ltrMCFD = (float)(m_support->logic[ASCcurrent_frame_data]->AFD) / (128 * 64);
                }
                else {
                    m_support->logic[ASCcurrent_frame_data]->m_ltrMCFD = m_support->logic[ASCprevious_frame_data]->m_ltrMCFD;
                }
            }

            m_support->logic[ASCcurrent_frame_data]->TSCindex   = TableLookUp(NumTSC, lmt_tsc2, m_support->logic[ASCcurrent_frame_data]->TSC);
            m_support->logic[ASCcurrent_frame_data]->SCindex    = TableLookUp(NumSC, lmt_sc2, m_support->logic[ASCcurrent_frame_data]->SC);
            m_support->logic[ASCcurrent_frame_data]->pdist      = m_support->PDistanceTable[(m_support->logic[ASCcurrent_frame_data]->TSCindex * NumSC) +
                                                                  m_support->logic[ASCcurrent_frame_data]->SCindex];

            m_support->logic[ASCcurrent_frame_data]->TSC        >>= 5;
            m_support->logic[ASCcurrent_frame_data]->tcor         = m_videoData[ASCCurrent_Frame]->layer.tcor;
            m_support->logic[ASCcurrent_frame_data]->mcTcor       = m_videoData[ASCCurrent_Frame]->layer.mcTcor;
            m_support->logic[ASCcurrent_frame_data]->mu_mv_mag_sq = m_support->mu_mv_mag_sq;
            /*------Shot Detection------*/
            m_support->logic[ASCcurrent_frame_data]->Schg               = ShotDetect(m_videoData[ASCCurrent_Frame]->layer, m_videoData[ASCReference_Frame]->layer, *m_dataIn->layer, m_support->logic[ASCcurrent_frame_data], m_support->logic[ASCprevious_frame_data], m_support->control);
            m_support->logic[ASCprevious_frame_data]->lastFrameInShot   = (mfxU8)m_support->logic[ASCcurrent_frame_data]->Schg;
            m_support->logic[ASCcurrent_frame_data]->repeatedFrame      = FrameRepeatCheck();
        }
        m_support->logic[ASCcurrent_frame_data]->gop_size = ML_SelectGoPSize();
        m_dataIn->processed_frames++;

        if (m_support->logic[ASCcurrent_frame_data]->Schg)
        {
            m_TSC0 = 0;
        }
        else
        {
            if (m_support->logic[ASCcurrent_frame_data]->repeatedFrame == 0)
            {
                m_support->logic[ASCcurrent_frame_data]->TSC0 = m_support->logic[ASCcurrent_frame_data]->SC ? ((m_TSC0 * m_TSC0) / m_support->logic[ASCcurrent_frame_data]->SC) : 0;
            }
            else
            {
                m_support->logic[ASCcurrent_frame_data]->TSC0 = 0;
            }
            m_TSC0 = m_support->logic[ASCcurrent_frame_data]->TSC0 >> 10;
        }
        mfxI32 tsc0 = m_support->logic[ASCcurrent_frame_data]->SC ? ((m_TSC_APQ * m_TSC_APQ) / m_support->logic[ASCcurrent_frame_data]->SC) : 0;
        m_TSC_APQ = (m_support->logic[ASCcurrent_frame_data]->repeatedFrame == 0) ? (tsc0 >> 10) : 0;
    }

    /**
    ***********************************************************************
    * \Brief Adds LTR friendly frame decision to list
    *
    * Adds frame number and ltr friendly frame decision pair to list, but
    * first checks if the size of the list is same or less to MAXLTRHISTORY,
    * if the list is longer, then it removes the top elements of the list
    * until it is MAXLTRHISTORY - 1, then it adds the new pair to the bottom
    * of the list.
    *
    * \return none
    */
    void ASC::Put_LTR_Hint() {
        mfxI16
            list_size = (mfxI16)ltr_check_history.size();
        for (mfxI16 i = 0; i < list_size - (MAXLTRHISTORY - 1); i++)
            ltr_check_history.pop_front();
        ltr_check_history.push_back(std::make_pair(m_videoData[ASCCurrent_Frame]->frame_number, m_support->logic[ASCcurrent_frame_data]->ltr_flag));
    }

    /**
    ***********************************************************************
    * \Brief Checks LTR friendly decision history per frame and returns if
    *        LTR operation should be turn on or off.
    *
    * Travels the LTR friendly decision list backwards checking for frequency
    * and amount of true/false LTR friendly frame decision, based on the
    * good and bad limit inputs, if bad limit condition is reached first,
    * then it inmediately returns 0 (zero) which means to stop LTR operation
    *
    * \param goodLTRLimit      [IN] - Amount of true values to determine
    *                                 if the sequence should run in LTR mode.
    * \param badLTRLimit       [IN] - Amount of consecutive false values to
    *                                 stop LTR mode.
    *
    * \return ASC_LTR_DEC to flag stop(false)/continue(true) or FORCE LTR operation
    */
    ASC_LTR_DEC ASC::Continue_LTR_Mode(mfxU16 goodLTRLimit, mfxU16 badLTRLimit) {
        size_t
            goodLTRCounter = 0,
            goodLTRRelativeCount = 0,
            badLTRCounter = 0,
            list_size = ltr_check_history.size();
        std::list<std::pair<mfxI32, bool> >::iterator
            ltr_list_it = std::prev(ltr_check_history.end());
        goodLTRLimit = goodLTRLimit > MAXLTRHISTORY ? MAXLTRHISTORY : goodLTRLimit;
        //When a scene change happens, all history is discarded
        if (Get_frame_shot_Decision()) {
            ltr_check_history.resize(0);
            list_size = 0;
        }
        //If not enough history then let it be LTR
        if (list_size < badLTRLimit)
            return YES_LTR;
        //Travel trhough the list to determine if LTR operation should be kept on
        mfxU16
            bkp_size = (mfxU16)list_size;
        while ((bkp_size > 1) && (goodLTRCounter < goodLTRLimit)) {
            auto scd = ltr_list_it->second;
            if (!scd) {
                badLTRCounter++;
                goodLTRRelativeCount = 0;
            }
            if (badLTRCounter >= badLTRLimit)
                return NO_LTR;
            goodLTRCounter += (mfxU16)ltr_list_it->second;
            goodLTRRelativeCount += (mfxU16)ltr_list_it->second;
            if (goodLTRRelativeCount >= badLTRLimit)
                badLTRCounter = 0;
            ltr_list_it = std::prev(ltr_list_it);
            bkp_size--;
        }
        if (goodLTRCounter >= goodLTRLimit)
            return FORCE_LTR;
        else if (goodLTRRelativeCount >= (size_t)std::min(badLTRLimit, mfxU16(list_size - 1)) && badLTRCounter < goodLTRRelativeCount)
            return YES_LTR;
        else
            return NO_LTR;
    }

    void ASC::AscFrameAnalysis() {
        mfxU8
            *ss = m_videoData[ASCCurrent_Frame]->layer.Image.Y;

        mfxU32
            sumAll = 0;

        for (mfxU16 i = 0; i < m_dataIn->layer->_cheight; i++) {
            for (mfxU16 j = 0; j < m_dataIn->layer->_cwidth; j++)
                sumAll += ss[j];
            ss += m_dataIn->layer->Extended_Width;
        }
        sumAll >>= 13;
        m_videoData[ASCCurrent_Frame]->layer.avgval = (mfxU16)sumAll;
        RsCsCalc();
        DetectShotChangeFrame();
        Put_LTR_Hint();
        GeneralBufferRotation();
    }

    mfxStatus ASC::RunFrame(mfxU8 *frame, mfxU32 parity, bool hasLTR) {
        if (!m_ASCinitialized)
            return MFX_ERR_NOT_INITIALIZED;

        m_videoData[ASCCurrent_Frame]->frame_number = m_videoData[ASCReference_Frame]->frame_number + 1;

        (this->*(resizeFunc))(frame, m_width, m_height, m_pitch, (ASCLayers)0, parity);
        RsCsCalc();
        DetectShotChangeFrame(hasLTR);
        Put_LTR_Hint();
        GeneralBufferRotation();
        return MFX_ERR_NONE;
    }

    mfxStatus ASC::RunFrame_LTR() {
        if (!m_ASCinitialized)
            return MFX_ERR_NOT_INITIALIZED;

        RsCsCalc();
        DetectShotChangeFrame(true);
        Put_LTR_Hint();

        m_dataReady = true;
        return MFX_ERR_NONE;
    }

    mfxStatus ASC::PutFrameProgressive(mfxU8 *frame, mfxI32 Pitch, bool hasLTR) {
        mfxStatus sts;
        if (Pitch > 0) {
            sts = SetPitch(Pitch);
            SCD_CHECK_MFX_ERR(sts);
        }

        sts = RunFrame(frame, ASCTopField, hasLTR);
        SCD_CHECK_MFX_ERR(sts);
        m_dataReady = (sts == MFX_ERR_NONE);
        return sts;
    }

    mfxStatus ASC::calc_RaCa_pic(mfxU8 *pSrc, mfxI32 width, mfxI32 height, mfxI32 pitch, mfxF64 &RsCs) {
        if (!m_ASCinitialized)
            return MFX_ERR_NOT_INITIALIZED;
        return Calc_RaCa_pic(pSrc, width, height, pitch, RsCs);
    }

    bool ASC::Get_Last_frame_Data() {
        if (m_dataReady)
            GeneralBufferRotation();
        else
            ASC_PRINTF("Warning: Trying to grab data not ready\n");
        return(m_dataReady);
    }

    mfxU16 ASC::Get_asc_subsampling_width()
    {
        return mfxU16(subWidth);
    }

    mfxU16 ASC::Get_asc_subsampling_height()
    {
        return mfxU16(subHeight);
    }

    mfxU32 ASC::Get_starting_frame_number() {
        return m_dataIn->starting_frame;
    }

    mfxU32 ASC::Get_frame_number() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->frameNum;
        else
            return 0;
    }

    mfxU32 ASC::Get_frame_shot_Decision() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->Schg;
        else
            return 0;
    }

    bool ASC::Get_Repeated_Frame_Flag() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->repeatedFrame;
        else
            return 0;
    }
    mfxU32 ASC::Get_scene_transition_Decision() {
        if (m_dataReady)
            return m_support->logic[ASCcurrent_frame_data]->Schg;
        else
            return 0;
    }
    mfxU32 ASC::Get_frame_SC() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->SC;
        else
            return 0;
    }

    mfxU32 ASC::Get_frame_TSC() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->TSC;
        else
            return 0;
    }

    mfxF32 ASC::Get_frame_MV0() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->MV0;
        else
            return 0.f;
    }

    mfxU32 ASC::Get_frame_MVSize() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->AbsMVSize;
        else
            return 0;
    }

    mfxU32 ASC::Get_frame_Contrast() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->Contrast;
        else
            return 0;
    }
    mfxI32 ASC::Get_frame_RecentHighMvCount() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->RecentHighMvCount;
        else
            return 0;
    }
    mfxI32 ASC::Get_frame_mcTCorr() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->mcTcor;
        else
            return 0;
    }
    mfxI32 ASC::Get_frame_mcTCorr_APQ() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->mcTcor;
        else
            return 0;
    }

    mfxF32 ASC::Get_frame_ltrMCFD() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->m_ltrMCFD;
        else
            return 0.f;
    }

    mfxU32 ASC::Get_frame_last_in_scene() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->lastFrameInShot;
        else
            return 0;
    }

    bool ASC::Get_GoPcorrected_frame_shot_Decision() {
        if (m_dataReady)
            return (m_support->detectedSch > 0);
        else
            return 0;
    }
    mfxI32 ASC::Get_frame_Spatial_complexity() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->SCindex;
        else
            return 0;
    }

    mfxI32 ASC::Get_frame_Temporal_complexity() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->TSCindex;
        else
            return 0;
    }

    mfxI32 ASC::Get_frame_SpatioTemporal_complexity() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->TSC0;
        else
            return 0;
    }

    mfxU32 ASC::Get_PDist_advice() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->pdist;
        else
            return 0;
    }

    bool ASC::Get_LTR_advice() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->ltr_flag;
        else
            return 0;
    }

    bool ASC::Get_RepeatedFrame_advice() {
        if (m_dataReady)
            return m_support->logic[ASCprevious_frame_data]->repeatedFrame;
        else
            return 0;
    }

    void ASC::get_PersistenceMap(mfxU8 PMap[ASC_MAP_SIZE], bool add) 
    {
        if (m_dataReady) 
        {
            for (int i = 0; i < ASC_MAP_SIZE; i++) 
            {
                if (m_videoData[ASCReference_Frame]->layer.PAQ[i] 
                      && !m_support->logic[ASCprevious_frame_data]->Schg 
                      && !m_support->logic[ASCprevious_frame_data]->firstFrame
                      && !m_support->logic[ASCprevious_frame_data]->repeatedFrame) 
                {
                    if (add) 
                    {
                        if (PMap[i] < UCHAR_MAX) PMap[i] += m_videoData[ASCReference_Frame]->layer.PAQ[i];
                    }
                    else
                    {
                        PMap[i] = m_videoData[ASCReference_Frame]->layer.PAQ[i];
                    }
                }
                else if(m_support->logic[ASCprevious_frame_data]->repeatedFrame)
                {
                    if (add)
                    {
                        if (PMap[i] < UCHAR_MAX) PMap[i] += 1;
                    }
                    else
                    {
                        PMap[i] = 1;
                    }
                }
                else
                {
                    PMap[i] = 0;
                }
            }
        }
    }

    uint32_t ASC::CorrectScdMiniGopDecision() {
        int32_t  sc = Get_frame_SC();
        int32_t qsc = (sc < 2048) ? (sc >> 9) : (4 + ((sc - 2048) >> 10));
        qsc = (qsc < 0) ? 0 : ((qsc > 9) ? 9 : qsc);
        int32_t  mv = Get_frame_MVSize();
        int32_t qmv = 0;
        if (mv < 1024) {
            qmv = (mv < 256) ? 0 : ((mv < 512) ? 1 : 2);
        }
        else {
            qmv = 3 + ((mv - 1024) >> 10);
            qmv = (qmv < 0) ? 0 : ((qmv > 9) ? 9 : qmv);
        }
        int32_t  MV_TH[10] = { 2,4,4,4,4,4,4,4,4,6 };
        uint32_t miniGopSize = (qmv < MV_TH[qsc]) ? 2 : 1;

        return miniGopSize;
    }
    /**
    ***********************************************************************
    * \Brief Tells if LTR mode should be on/off or forced on.
    *
    * \return  ASC_LTR_DEC& to flag stop(false)/continue(true) or force (2)
    *          LTR operation
    */
    mfxStatus ASC::get_LTR_op_hint(ASC_LTR_DEC& scd_LTR_hint) {
        if (!m_ASCinitialized)
            return MFX_ERR_NOT_INITIALIZED;
        scd_LTR_hint = Continue_LTR_Mode(50, 5);
        return MFX_ERR_NONE;
    }

    bool ASC::Check_last_frame_processed(mfxU32 frameOrder) {
        if (m_support->frameOrder <= frameOrder && (m_support->frameOrder == frameOrder && frameOrder > 0))
            return 0;
        else
            m_support->frameOrder = frameOrder;
        return 1;
    }

    void ASC::Reset_last_frame_processed() {
        m_support->frameOrder = 0;
    }

    void bufferRotation(void *Buffer1, void *Buffer2) {
        void
            *transfer;
        transfer = Buffer2;
        Buffer2 = Buffer1;
        Buffer1 = transfer;
    }

    void ASC::GeneralBufferRotation() {
        ASCVidSample
            *videoTransfer;
        ASCTSCstat
            *metaTransfer;

        if (m_support->logic[ASCcurrent_frame_data]->repeatedFrame) {
            m_videoData[ASCReference_Frame]->frame_number = m_videoData[ASCCurrent_Frame]->frame_number;
            m_support->logic[ASCprevious_frame_data]->frameNum = m_support->logic[ASCcurrent_frame_data]->frameNum;
            m_support->logic[ASCcurrent_frame_data]->Schg = 0;
            m_support->logic[ASCprevious_frame_data]->Schg = 0;
            m_support->logic[ASCprevious_frame_data]->repeatedFrame = true;
            m_support->logic[ASCprevious_previous_frame_data]->Schg = 0;
        }
        else {
            videoTransfer = m_videoData[0];
            m_videoData[0] = m_videoData[1];
            m_videoData[1] = videoTransfer;

            metaTransfer = m_support->logic[ASCprevious_previous_frame_data];
            m_support->logic[ASCprevious_previous_frame_data] = m_support->logic[ASCprevious_frame_data];
            m_support->logic[ASCprevious_frame_data] = m_support->logic[ASCcurrent_frame_data];
            m_support->logic[ASCcurrent_frame_data] = metaTransfer;
        }
    }

    void ASC::GetImageAndStat(ASCVidSample& img, ASCTSCstat& stat, uint32_t ImgIdx, uint32_t StatIdx) {
        img = *(m_videoData[ImgIdx]);
        stat = *(m_support->logic[StatIdx]);
    }

    void ASC::SetImageAndStat(ASCVidSample& img, ASCTSCstat& stat, uint32_t ImgIdx, uint32_t StatIdx) {
        *(m_videoData[ImgIdx]) = img;
        *(m_support->logic[StatIdx]) = stat;
    }

    /**
    ***********************************************************************
    * \Brief ME part of SCD.
    ***********************************************************************
    */

    mfxI32 DistInt(ASCMVector vector) {
        return (vector.x*vector.x) + (vector.y*vector.y);
    }

    mfxU16 ME_SAD_8x8_Block(mfxU8 *pSrc, mfxU8 *pRef, mfxU32 srcPitch, mfxU32 refPitch)
    {
        int sum = 0;
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                sum += std::abs(pSrc[y * srcPitch + x] - pRef[y * refPitch + x]);
            }
        }

        return (mfxU16)sum;
    }

    bool MVcalcSAD8x8(ASCMVector MV, mfxU8* curY, mfxU8* refY, ASCImDetails *dataIn, mfxU16 *bestSAD, mfxI32 *distance) {
        mfxI32
            preDist = (MV.x * MV.x) + (MV.y * MV.y),
            _fPos = MV.x + (MV.y * dataIn->Extended_Width);
        mfxU8*
            fRef = &refY[_fPos];
        mfxU16
            SAD = ME_SAD_8x8_Block(curY, fRef, dataIn->Extended_Width, dataIn->Extended_Width);
        if ((SAD < *bestSAD) || ((SAD == *(bestSAD)) && *distance > preDist)) {
            *distance = preDist;
            *(bestSAD) = SAD;
            return true;
        }
        return false;
    }

    void SearchLimitsCalcSqr(mfxI16 xLoc, mfxI16 yLoc, mfxI16 *limitXleft, mfxI16 *limitXright, mfxI16 *limitYup, mfxI16 *limitYdown, ASCImDetails *dataIn, mfxI32 range, ASCMVector mv, ASCVidData *limits) {
        mfxI16
            locX = (mfxI16)((xLoc * dataIn->block_width) + dataIn->horizontal_pad + mv.x),
            locY = (mfxI16)((yLoc * dataIn->block_height) + dataIn->vertical_pad + mv.y);
        *limitXleft = (mfxI16)std::max(-locX, -range);
        *limitXright = (mfxI16)std::min(dataIn->Extended_Width - ((xLoc + 1) * dataIn->block_width) - dataIn->horizontal_pad - mv.x, range - 1);
        *limitYup = (mfxI16)std::max(-locY, -range);
        *limitYdown = (mfxI16)std::min(dataIn->Extended_Height - ((yLoc + 1) * dataIn->block_height) - dataIn->vertical_pad - mv.y, range - 1);
        if (limits->limitRange) {
            *limitXleft = (mfxI16)std::max(*limitXleft, (mfxI16)-limits->maxXrange);
            *limitXright = (mfxI16)std::min(*limitXright, (mfxI16)limits->maxXrange);
            *limitYup = (mfxI16)std::max(*limitYup, (mfxI16)-limits->maxYrange);
            *limitYdown = (mfxI16)std::min(*limitYdown, (mfxI16) limits->maxYrange);
        }
    }

    void SearchLimitsCalc(mfxI16 xLoc, mfxI16 yLoc, mfxI16 *limitXleft, mfxI16 *limitXright, mfxI16 *limitYup, mfxI16 *limitYdown, ASCImDetails *dataIn, mfxI32 range, ASCMVector mv, ASCVidData *limits)
    {
        mfxI16
            locX = (mfxI16)((xLoc * dataIn->block_width) + dataIn->horizontal_pad + mv.x),
            locY = (mfxI16)((yLoc * dataIn->block_height) + dataIn->vertical_pad + mv.y);
        *limitXleft = (mfxI16)std::max(-locX, -range);
        *limitXright = (mfxI16)std::min(dataIn->Extended_Width - ((xLoc + 1) * dataIn->block_width) - dataIn->horizontal_pad - mv.x, range);
        *limitYup = (mfxI16)std::max(-locY, -range);
        *limitYdown = (mfxI16)std::min(dataIn->Extended_Height - ((yLoc + 1) * dataIn->block_height) - dataIn->vertical_pad - mv.y, range);
        if (limits->limitRange) {
            *limitXleft = (mfxI16)std::max(*limitXleft, (mfxI16) -limits->maxXrange);
            *limitXright = (mfxI16)std::min(*limitXright, (mfxI16) limits->maxXrange);
            *limitYup = (mfxI16)std::max(*limitYup, (mfxI16) -limits->maxYrange);
            *limitYdown = (mfxI16)std::min(*limitYdown, (mfxI16) limits->maxYrange);
        }
    }

    void ME_VAR_8x8_Block(mfxU8 *pSrc, mfxU8 *pRef, mfxU8 *pMCref, mfxI16 srcAvgVal, mfxI16 refAvgVal, mfxU32 srcPitch, mfxU32 refPitch, mfxI32 &var, mfxI32 &jtvar, mfxI32 &jtMCvar)
    {
        int accuVar = 0;
        int accuJtvar = 0;
        int accuMcJtvar = 0;

        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {

                int diffSrc = (pSrc[y * srcPitch + x] - srcAvgVal);
                accuVar += diffSrc * diffSrc;

                int diffRef = (pRef[y * refPitch + x] - refAvgVal);
                accuJtvar += diffSrc * diffRef;

                int diffMCref = (pMCref[y * refPitch + x] - refAvgVal);
                accuMcJtvar += diffSrc * diffMCref;
            }
        }
        var += accuVar;
        jtvar += accuJtvar;
        jtMCvar += accuMcJtvar;
    }

    void MVcalcVar8x8(ASCMVector MV, mfxU8* curY, mfxU8* refY, mfxI16 curAvg, mfxI16 refAvg, mfxI32 &var, mfxI32 &jtvar, mfxI32 &jtMCvar, ASCImDetails *dataIn)
    {
        mfxI32 _fPos = MV.x + (MV.y * dataIn->Extended_Width);
        mfxU8* fRef = &refY[_fPos];

        ME_VAR_8x8_Block(curY, refY, fRef, curAvg, refAvg, dataIn->Extended_Width, dataIn->Extended_Width, var, jtvar, jtMCvar);
    }

    mfxU16  ME_simple(ASCVidRead *videoIn, mfxI32 fPos, ASCImDetails *dataIn, ASCimageData *scale, ASCimageData *scaleRef, bool /*first*/, ASCVidData *limits, t_ME_SAD_8x8_Block_Search ME_SAD_8x8_Block_Search)
    {
        const ASCMVector zero = { 0,0 };

        ASCMVector
            tMV,
            ttMV,
            *current,
            predMV = zero,
            Nmv = zero;
        mfxU8
            *objFrame = NULL,
            *refFrame = NULL;
        mfxI16
            limitXleft = 0,
            limitXright = 0,
            limitYup = 0,
            limitYdown = 0,
            xLoc = ((mfxI16)fPos % (mfxI16)dataIn->Width_in_blocks),
            yLoc = ((mfxI16)fPos / (mfxI16)dataIn->Width_in_blocks);
        mfxI32
            distance = 0,
            mainDistance = 0,
            offset = (yLoc * (mfxI16)dataIn->Extended_Width * (mfxI16)dataIn->block_height) + (xLoc * (mfxI16)dataIn->block_width);
        mfxU16
            *outSAD,
            zeroSAD = USHRT_MAX,
            bestSAD = USHRT_MAX;
        mfxU8
            neighbor_count = 0;
        bool
            foundBetter = false;


        objFrame = &scale->Image.Y[offset];
        refFrame = &scaleRef->Image.Y[offset];

        current = scale->pInteger;
        outSAD = scale->SAD;

        outSAD[fPos] = USHRT_MAX;

        MVcalcSAD8x8(zero, objFrame, refFrame, dataIn, &bestSAD, &distance);
        current[fPos] = zero;
        outSAD[fPos] = bestSAD;
        zeroSAD = bestSAD;
        mainDistance = distance;
        if (bestSAD == 0)
            return bestSAD;

        if ((fPos > (mfxI32)dataIn->Width_in_blocks) && (xLoc > 0)) { //Top Left
            neighbor_count++;
            Nmv.x += current[fPos - dataIn->Width_in_blocks - 1].x;
            Nmv.y += current[fPos - dataIn->Width_in_blocks - 1].y;
        }
        if (fPos > (mfxI32)dataIn->Width_in_blocks) { // Top
            neighbor_count++;
            Nmv.x += current[fPos - dataIn->Width_in_blocks].x;
            Nmv.y += current[fPos - dataIn->Width_in_blocks].y;
        }
        if (xLoc > 0) {//Left
            neighbor_count++;
            Nmv.x += current[fPos - 1].x;
            Nmv.y += current[fPos - 1].y;
        }
        if (neighbor_count) {
            Nmv.x /= neighbor_count;
            Nmv.y /= neighbor_count;
            if ((Nmv.x + ((xLoc + 1) * MVBLK_SIZE)) > ASC_SMALL_WIDTH)
                Nmv.x -= (Nmv.x + ((xLoc + 1) * MVBLK_SIZE)) - ASC_SMALL_WIDTH;
            else if (((xLoc * MVBLK_SIZE) + Nmv.x) < 0)
                Nmv.x -= ((xLoc * MVBLK_SIZE) + Nmv.x);

            if ((Nmv.y + ((yLoc + 1) * MVBLK_SIZE)) > ASC_SMALL_HEIGHT)
                Nmv.y -= (Nmv.y + ((yLoc + 1) * MVBLK_SIZE)) - ASC_SMALL_HEIGHT;
            else if (((yLoc * MVBLK_SIZE) + Nmv.y) < 0)
                Nmv.y -= ((yLoc * MVBLK_SIZE) + Nmv.y);

            distance = mainDistance;
            if (Nmv.x != zero.x || Nmv.y != zero.y) {
                foundBetter = MVcalcSAD8x8(Nmv, objFrame, refFrame, dataIn, &bestSAD, &distance);
                if (foundBetter) {
                    current[fPos] = Nmv;
                    outSAD[fPos] = bestSAD;
                    mainDistance = distance;
                }
            }
        }

        //Search around the best predictor (zero or Neighbor)
        SearchLimitsCalcSqr(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown, dataIn, 8, current[fPos], limits);//Checks limits for +-8
        ttMV = current[fPos];
        bestSAD = outSAD[fPos];
        distance = mainDistance;

        {//Search area in steps of 2 for x and y
            mfxI32 _fPos = (limitYup + ttMV.y) * dataIn->Extended_Width + limitXleft + ttMV.x;
            mfxU8
                *ps = objFrame,
                *pr = &refFrame[_fPos];
            int xrange = limitXright - limitXleft/* + 1*/,
                yrange = limitYdown - limitYup/* + 1*/,
                bX = 0,
                bY = 0;
            ME_SAD_8x8_Block_Search(ps, pr, dataIn->Extended_Width, xrange, yrange, &bestSAD, &bX, &bY);
            if (bestSAD < outSAD[fPos]) {
                outSAD[fPos] = bestSAD;
                current[fPos].x = (mfxI16)bX + limitXleft + ttMV.x;
                current[fPos].y = (mfxI16)bY + limitYup + ttMV.y;
                mainDistance = DistInt(current[fPos]);
            }
        }
        //Final refinement +-1 search
        ttMV = current[fPos];
        bestSAD = outSAD[fPos];
        distance = mainDistance;
        SearchLimitsCalc(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown, dataIn, 1, ttMV, limits);
        for (tMV.y = limitYup; tMV.y <= limitYdown; tMV.y++) {
            for (tMV.x = limitXleft; tMV.x <= limitXright; tMV.x++) {
                if (tMV.x != 0 || tMV.y != 0) {// don't search on center position
                    predMV.x = tMV.x + ttMV.x;
                    predMV.y = tMV.y + ttMV.y;
                    foundBetter = MVcalcSAD8x8(predMV, objFrame, refFrame, dataIn, &bestSAD, &distance);
                    if (foundBetter) {
                        current[fPos] = predMV;
                        outSAD[fPos] = bestSAD;
                        mainDistance = distance;
                        foundBetter = false;
                    }
                }
            }
        }
        videoIn->average += (current[fPos].x * current[fPos].x) + (current[fPos].y * current[fPos].y);
        MVcalcVar8x8(current[fPos], objFrame, refFrame, scale->avgval, scaleRef->avgval, scale->var, scale->jtvar, scale->mcjtvar, dataIn);
        return(zeroSAD);
    }
}

#endif // MFX_ENABLE_ADAPTIVE_ENCODE
