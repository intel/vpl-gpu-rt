// Copyright (c) 2007-2023 Intel Corporation
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

#ifndef __MFXSTRUCTURES_INT_H__
#define __MFXSTRUCTURES_INT_H__
#include "mfxstructures.h"
#include "mfx_config.h"

#include "mfxdeprecated.h"

#ifdef __cplusplus
extern "C" {
#endif

enum eMFXPlatform
{
    MFX_PLATFORM_SOFTWARE      = 0,
    MFX_PLATFORM_HARDWARE      = 1,
};

enum eMFXVAType
{
    MFX_HW_NO       = 0,
    MFX_HW_D3D9     = 1,
    MFX_HW_D3D11    = 2,
    MFX_HW_VAAPI    = 4,// Linux VA-API
};

enum eMFXHWType
{
    MFX_HW_UNKNOWN   = 0,
    MFX_HW_SNB       = 0x300000,

    MFX_HW_IVB       = 0x400000,

    MFX_HW_HSW       = 0x500000,
    MFX_HW_HSW_ULT   = 0x500001,

    MFX_HW_VLV       = 0x600000,

    MFX_HW_BDW       = 0x700000,

    MFX_HW_CHT       = 0x800000,

    MFX_HW_SCL       = 0x900000,

    MFX_HW_APL       = 0x1000000,

    MFX_HW_KBL       = 0x1100000,
    MFX_HW_GLK       = MFX_HW_KBL + 1,
    MFX_HW_CFL       = MFX_HW_KBL + 2,

    MFX_HW_CNL       = 0x1200000,

    MFX_HW_ICL       = 0x1400000,
    MFX_HW_ICL_LP    = MFX_HW_ICL + 1,
    MFX_HW_JSL       = 0x1500001,
    MFX_HW_EHL       = 0x1500002,

    MFX_HW_TGL_LP    = 0x1600000,
    MFX_HW_RKL       = MFX_HW_TGL_LP + 2,
    MFX_HW_DG1       = MFX_HW_TGL_LP + 3,
    MFX_HW_ADL_S     = MFX_HW_TGL_LP + 4,
    MFX_HW_ADL_P     = MFX_HW_TGL_LP + 5,
    MFX_HW_ADL_N     = MFX_HW_TGL_LP + 6,
    MFX_HW_XE_HP_SDV = MFX_HW_TGL_LP + 7,
    MFX_HW_DG2       = MFX_HW_TGL_LP + 8,

    MFX_HW_PVC       = MFX_HW_XE_HP_SDV + 2,

    MFX_HW_MTL       = 0x1700000,
    MFX_HW_ARL       = MFX_HW_MTL + 1,
    MFX_HW_LNL       = MFX_HW_MTL + 3,

};

enum eMFXGTConfig
{
    MFX_GT_UNKNOWN = 0,
    MFX_GT1     = 1,
    MFX_GT2     = 2,
    MFX_GT3     = 3,
    MFX_GT4     = 4
};

typedef struct {
    int          device_id;
    eMFXHWType   platform;
    eMFXGTConfig config;
} mfx_device_item;

// list of legal dev ID for Intel's graphics
 const mfx_device_item listLegalDevIDs[] = {
    /*IVB*/
    { 0x0156, MFX_HW_IVB, MFX_GT1 },   /* GT1 mobile */
    { 0x0166, MFX_HW_IVB, MFX_GT2 },   /* GT2 mobile */
    { 0x0152, MFX_HW_IVB, MFX_GT1 },   /* GT1 desktop */
    { 0x0162, MFX_HW_IVB, MFX_GT2 },   /* GT2 desktop */
    { 0x015a, MFX_HW_IVB, MFX_GT1 },   /* GT1 server */
    { 0x016a, MFX_HW_IVB, MFX_GT2 },   /* GT2 server */
    /*HSW*/
    { 0x0402, MFX_HW_HSW, MFX_GT1 },   /* GT1 desktop */
    { 0x0412, MFX_HW_HSW, MFX_GT2 },   /* GT2 desktop */
    { 0x0422, MFX_HW_HSW, MFX_GT2 },   /* GT2 desktop */
    { 0x041e, MFX_HW_HSW, MFX_GT2 },   /* Core i3-4130 */
    { 0x040a, MFX_HW_HSW, MFX_GT1 },   /* GT1 server */
    { 0x041a, MFX_HW_HSW, MFX_GT2 },   /* GT2 server */
    { 0x042a, MFX_HW_HSW, MFX_GT2 },   /* GT2 server */
    { 0x0406, MFX_HW_HSW, MFX_GT1 },   /* GT1 mobile */
    { 0x0416, MFX_HW_HSW, MFX_GT2 },   /* GT2 mobile */
    { 0x0426, MFX_HW_HSW, MFX_GT2 },   /* GT2 mobile */
    { 0x0C02, MFX_HW_HSW, MFX_GT1 },   /* SDV GT1 desktop */
    { 0x0C12, MFX_HW_HSW, MFX_GT2 },   /* SDV GT2 desktop */
    { 0x0C22, MFX_HW_HSW, MFX_GT2 },   /* SDV GT2 desktop */
    { 0x0C0A, MFX_HW_HSW, MFX_GT1 },   /* SDV GT1 server */
    { 0x0C1A, MFX_HW_HSW, MFX_GT2 },   /* SDV GT2 server */
    { 0x0C2A, MFX_HW_HSW, MFX_GT2 },   /* SDV GT2 server */
    { 0x0C06, MFX_HW_HSW, MFX_GT1 },   /* SDV GT1 mobile */
    { 0x0C16, MFX_HW_HSW, MFX_GT2 },   /* SDV GT2 mobile */
    { 0x0C26, MFX_HW_HSW, MFX_GT2 },   /* SDV GT2 mobile */
    { 0x0A02, MFX_HW_HSW, MFX_GT1 },   /* ULT GT1 desktop */
    { 0x0A12, MFX_HW_HSW, MFX_GT2 },   /* ULT GT2 desktop */
    { 0x0A22, MFX_HW_HSW, MFX_GT2 },   /* ULT GT2 desktop */
    { 0x0A0A, MFX_HW_HSW, MFX_GT1 },   /* ULT GT1 server */
    { 0x0A1A, MFX_HW_HSW, MFX_GT2 },   /* ULT GT2 server */
    { 0x0A2A, MFX_HW_HSW, MFX_GT2 },   /* ULT GT2 server */
    { 0x0A06, MFX_HW_HSW, MFX_GT1 },   /* ULT GT1 mobile */
    { 0x0A16, MFX_HW_HSW, MFX_GT2 },   /* ULT GT2 mobile */
    { 0x0A26, MFX_HW_HSW, MFX_GT2 },   /* ULT GT2 mobile */
    { 0x0D02, MFX_HW_HSW, MFX_GT1 },   /* CRW GT1 desktop */
    { 0x0D12, MFX_HW_HSW, MFX_GT2 },   /* CRW GT2 desktop */
    { 0x0D22, MFX_HW_HSW, MFX_GT2 },   /* CRW GT2 desktop */
    { 0x0D0A, MFX_HW_HSW, MFX_GT1 },   /* CRW GT1 server */
    { 0x0D1A, MFX_HW_HSW, MFX_GT2 },   /* CRW GT2 server */
    { 0x0D2A, MFX_HW_HSW, MFX_GT2 },   /* CRW GT2 server */
    { 0x0D06, MFX_HW_HSW, MFX_GT1 },   /* CRW GT1 mobile */
    { 0x0D16, MFX_HW_HSW, MFX_GT2 },   /* CRW GT2 mobile */
    { 0x0D26, MFX_HW_HSW, MFX_GT2 },   /* CRW GT2 mobile */
    { 0x040B, MFX_HW_HSW, MFX_GT1 }, /*HASWELL_B_GT1 *//* Reserved */
    { 0x041B, MFX_HW_HSW, MFX_GT2 }, /*HASWELL_B_GT2*/
    { 0x042B, MFX_HW_HSW, MFX_GT3 }, /*HASWELL_B_GT3*/
    { 0x040E, MFX_HW_HSW, MFX_GT1 }, /*HASWELL_E_GT1*//* Reserved */
    { 0x041E, MFX_HW_HSW, MFX_GT2 }, /*HASWELL_E_GT2*/
    { 0x042E, MFX_HW_HSW, MFX_GT3 }, /*HASWELL_E_GT3*/

    { 0x0C0B, MFX_HW_HSW, MFX_GT1 }, /*HASWELL_SDV_B_GT1*/ /* Reserved */
    { 0x0C1B, MFX_HW_HSW, MFX_GT2 }, /*HASWELL_SDV_B_GT2*/
    { 0x0C2B, MFX_HW_HSW, MFX_GT3 }, /*HASWELL_SDV_B_GT3*/
    { 0x0C0E, MFX_HW_HSW, MFX_GT1 }, /*HASWELL_SDV_B_GT1*//* Reserved */
    { 0x0C1E, MFX_HW_HSW, MFX_GT2 }, /*HASWELL_SDV_B_GT2*/
    { 0x0C2E, MFX_HW_HSW, MFX_GT3 }, /*HASWELL_SDV_B_GT3*/

    { 0x0A0B, MFX_HW_HSW, MFX_GT1 }, /*HASWELL_ULT_B_GT1*/ /* Reserved */
    { 0x0A1B, MFX_HW_HSW, MFX_GT2 }, /*HASWELL_ULT_B_GT2*/
    { 0x0A2B, MFX_HW_HSW, MFX_GT3 }, /*HASWELL_ULT_B_GT3*/
    { 0x0A0E, MFX_HW_HSW, MFX_GT1 }, /*HASWELL_ULT_E_GT1*/ /* Reserved */
    { 0x0A1E, MFX_HW_HSW, MFX_GT2 }, /*HASWELL_ULT_E_GT2*/
    { 0x0A2E, MFX_HW_HSW, MFX_GT3 }, /*HASWELL_ULT_E_GT3*/

    { 0x0D0B, MFX_HW_HSW, MFX_GT1 }, /*HASWELL_CRW_B_GT1*/ /* Reserved */
    { 0x0D1B, MFX_HW_HSW, MFX_GT2 }, /*HASWELL_CRW_B_GT2*/
    { 0x0D2B, MFX_HW_HSW, MFX_GT3 }, /*HASWELL_CRW_B_GT3*/
    { 0x0D0E, MFX_HW_HSW, MFX_GT1 }, /*HASWELL_CRW_E_GT1*/ /* Reserved */
    { 0x0D1E, MFX_HW_HSW, MFX_GT2 }, /*HASWELL_CRW_E_GT2*/
    { 0x0D2E, MFX_HW_HSW, MFX_GT3 }, /*HASWELL_CRW_E_GT3*/

    /* VLV */
    { 0x0f30, MFX_HW_VLV, MFX_GT1 },   /* VLV mobile */
    { 0x0f31, MFX_HW_VLV, MFX_GT1 },   /* VLV mobile */
    { 0x0f32, MFX_HW_VLV, MFX_GT1 },   /* VLV mobile */
    { 0x0f33, MFX_HW_VLV, MFX_GT1 },   /* VLV mobile */
    { 0x0157, MFX_HW_VLV, MFX_GT1 },
    { 0x0155, MFX_HW_VLV, MFX_GT1 },

    /* BDW */
    /*GT3: */
    { 0x162D, MFX_HW_BDW, MFX_GT3 },
    { 0x162A, MFX_HW_BDW, MFX_GT3 },
    /*GT2: */
    { 0x161D, MFX_HW_BDW, MFX_GT2 },
    { 0x161A, MFX_HW_BDW, MFX_GT2 },
    /* GT1: */
    { 0x160D, MFX_HW_BDW, MFX_GT1 },
    { 0x160A, MFX_HW_BDW, MFX_GT1 },
    /* BDW-ULT */
    /* (16x2 - ULT, 16x6 - ULT, 16xB - Iris, 16xE - ULX) */
    /*GT3: */
    { 0x162E, MFX_HW_BDW, MFX_GT3 },
    { 0x162B, MFX_HW_BDW, MFX_GT3 },
    { 0x1626, MFX_HW_BDW, MFX_GT3 },
    { 0x1622, MFX_HW_BDW, MFX_GT3 },
    { 0x1636, MFX_HW_BDW, MFX_GT3 }, /* ULT */
    { 0x163B, MFX_HW_BDW, MFX_GT3 }, /* Iris */
    { 0x163E, MFX_HW_BDW, MFX_GT3 }, /* ULX */
    { 0x1632, MFX_HW_BDW, MFX_GT3 }, /* ULT */
    { 0x163A, MFX_HW_BDW, MFX_GT3 }, /* Server */
    { 0x163D, MFX_HW_BDW, MFX_GT3 }, /* Workstation */

    /* GT2: */
    { 0x161E, MFX_HW_BDW, MFX_GT2 },
    { 0x161B, MFX_HW_BDW, MFX_GT2 },
    { 0x1616, MFX_HW_BDW, MFX_GT2 },
    { 0x1612, MFX_HW_BDW, MFX_GT2 },
    /* GT1: */
    { 0x160E, MFX_HW_BDW, MFX_GT1 },
    { 0x160B, MFX_HW_BDW, MFX_GT1 },
    { 0x1606, MFX_HW_BDW, MFX_GT1 },
    { 0x1602, MFX_HW_BDW, MFX_GT1 },

    /* CHT */
    { 0x22b0, MFX_HW_CHT, MFX_GT1 },
    { 0x22b1, MFX_HW_CHT, MFX_GT1 },
    { 0x22b2, MFX_HW_CHT, MFX_GT1 },
    { 0x22b3, MFX_HW_CHT, MFX_GT1 },

    /* SCL */
    /* GT1F */
    { 0x1902, MFX_HW_SCL, MFX_GT1 }, // DT, 2x1F, 510
    { 0x1906, MFX_HW_SCL, MFX_GT1 }, // U-ULT, 2x1F, 510
    { 0x190A, MFX_HW_SCL, MFX_GT1 }, // Server, 4x1F
    { 0x190B, MFX_HW_SCL, MFX_GT1 },
    { 0x190E, MFX_HW_SCL, MFX_GT1 }, // Y-ULX 2x1F
    /*GT1.5*/
    { 0x1913, MFX_HW_SCL, MFX_GT1 }, // U-ULT, 2x1.5
    { 0x1915, MFX_HW_SCL, MFX_GT1 }, // Y-ULX, 2x1.5
    { 0x1917, MFX_HW_SCL, MFX_GT1 }, // DT, 2x1.5
    /* GT2 */
    { 0x1912, MFX_HW_SCL, MFX_GT2 }, // DT, 2x2, 530
    { 0x1916, MFX_HW_SCL, MFX_GT2 }, // U-ULD 2x2, 520
    { 0x191A, MFX_HW_SCL, MFX_GT2 }, // 2x2,4x2, Server
    { 0x191B, MFX_HW_SCL, MFX_GT2 }, // DT, 2x2, 530
    { 0x191D, MFX_HW_SCL, MFX_GT2 }, // 4x2, WKS, P530
    { 0x191E, MFX_HW_SCL, MFX_GT2 }, // Y-ULX, 2x2, P510,515
    { 0x1921, MFX_HW_SCL, MFX_GT2 }, // U-ULT, 2x2F, 540
    /* GT3 */
    { 0x1923, MFX_HW_SCL, MFX_GT3 }, // U-ULT, 2x3, 535
    { 0x1926, MFX_HW_SCL, MFX_GT3 }, // U-ULT, 2x3, 540 (15W)
    { 0x1927, MFX_HW_SCL, MFX_GT3 }, // U-ULT, 2x3e, 550 (28W)
    { 0x192A, MFX_HW_SCL, MFX_GT3 }, // Server, 2x3
    { 0x192B, MFX_HW_SCL, MFX_GT3 }, // Halo 3e
    { 0x192D, MFX_HW_SCL, MFX_GT3 },
    /* GT4e*/
    { 0x1932, MFX_HW_SCL, MFX_GT4 }, // DT
    { 0x193A, MFX_HW_SCL, MFX_GT4 }, // SRV
    { 0x193B, MFX_HW_SCL, MFX_GT4 }, // Halo
    { 0x193D, MFX_HW_SCL, MFX_GT4 }, // WKS

    /* APL */
    { 0x0A84, MFX_HW_APL, MFX_GT1 },
    { 0x0A85, MFX_HW_APL, MFX_GT1 },
    { 0x0A86, MFX_HW_APL, MFX_GT1 },
    { 0x0A87, MFX_HW_APL, MFX_GT1 },
    { 0x1A84, MFX_HW_APL, MFX_GT1 },
    { 0x1A85, MFX_HW_APL, MFX_GT1 },
    { 0x5A84, MFX_HW_APL, MFX_GT1 },
    { 0x5A85, MFX_HW_APL, MFX_GT1 },

    /* KBL */
    { 0x5902, MFX_HW_KBL, MFX_GT1 }, // DT GT1
    { 0x5906, MFX_HW_KBL, MFX_GT1 }, // ULT GT1
    { 0x5908, MFX_HW_KBL, MFX_GT1 }, // HALO GT1F
    { 0x590A, MFX_HW_KBL, MFX_GT1 }, // SERV GT1
    { 0x590B, MFX_HW_KBL, MFX_GT1 }, // HALO GT1
    { 0x590E, MFX_HW_KBL, MFX_GT1 }, // ULX GT1
    { 0x5912, MFX_HW_KBL, MFX_GT2 }, // DT GT2
    { 0x5913, MFX_HW_KBL, MFX_GT1 }, // ULT GT1 5
    { 0x5915, MFX_HW_KBL, MFX_GT1 }, // ULX GT1 5
    { 0x5916, MFX_HW_KBL, MFX_GT2 }, // ULT GT2
    { 0x5917, MFX_HW_KBL, MFX_GT2 }, // ULT GT2 R
    { 0x591A, MFX_HW_KBL, MFX_GT2 }, // SERV GT2
    { 0x591B, MFX_HW_KBL, MFX_GT2 }, // HALO GT2
    { 0x591C, MFX_HW_KBL, MFX_GT2 }, // ULX GT2
    { 0x591D, MFX_HW_KBL, MFX_GT2 }, // WRK GT2
    { 0x591E, MFX_HW_KBL, MFX_GT2 }, // ULX GT2
    { 0x5921, MFX_HW_KBL, MFX_GT2 }, // ULT GT2F
    { 0x5923, MFX_HW_KBL, MFX_GT3 }, // ULT GT3
    { 0x5926, MFX_HW_KBL, MFX_GT3 }, // ULT GT3 15W
    { 0x5927, MFX_HW_KBL, MFX_GT3 }, // ULT GT3 28W
    { 0x592A, MFX_HW_KBL, MFX_GT3 }, // SERV GT3
    { 0x592B, MFX_HW_KBL, MFX_GT3 }, // HALO GT3
    { 0x5932, MFX_HW_KBL, MFX_GT4 }, // DT GT4
    { 0x593A, MFX_HW_KBL, MFX_GT4 }, // SERV GT4
    { 0x593B, MFX_HW_KBL, MFX_GT4 }, // HALO GT4
    { 0x593D, MFX_HW_KBL, MFX_GT4 }, // WRK GT4
    { 0x87C0, MFX_HW_KBL, MFX_GT2 }, // ULX GT2

    /* GLK */
    { 0x3184, MFX_HW_GLK, MFX_GT1 },
    { 0x3185, MFX_HW_GLK, MFX_GT1 },

    /* CFL */
    { 0x3E90, MFX_HW_CFL, MFX_GT1 },
    { 0x3E91, MFX_HW_CFL, MFX_GT2 },
    { 0x3E92, MFX_HW_CFL, MFX_GT2 },
    { 0x3E93, MFX_HW_CFL, MFX_GT1 },
    { 0x3E94, MFX_HW_CFL, MFX_GT2 },
    { 0x3E96, MFX_HW_CFL, MFX_GT2 },
    { 0x3E98, MFX_HW_CFL, MFX_GT2 },
    { 0x3E99, MFX_HW_CFL, MFX_GT1 },
    { 0x3E9A, MFX_HW_CFL, MFX_GT2 },
    { 0x3E9C, MFX_HW_CFL, MFX_GT1 },
    { 0x3E9B, MFX_HW_CFL, MFX_GT2 },
    { 0x3EA5, MFX_HW_CFL, MFX_GT3 },
    { 0x3EA6, MFX_HW_CFL, MFX_GT3 },
    { 0x3EA7, MFX_HW_CFL, MFX_GT3 },
    { 0x3EA8, MFX_HW_CFL, MFX_GT3 },
    { 0x3EA9, MFX_HW_CFL, MFX_GT2 },
    { 0x87CA, MFX_HW_CFL, MFX_GT2 },

    /* WHL */
    { 0x3EA0, MFX_HW_CFL, MFX_GT2 },
    { 0x3EA1, MFX_HW_CFL, MFX_GT1 },
    { 0x3EA2, MFX_HW_CFL, MFX_GT3 },
    { 0x3EA3, MFX_HW_CFL, MFX_GT2 },
    { 0x3EA4, MFX_HW_CFL, MFX_GT1 },


    /* CML GT1 */
    { 0x9b21, MFX_HW_CFL, MFX_GT1 },
    { 0x9baa, MFX_HW_CFL, MFX_GT1 },
    { 0x9bab, MFX_HW_CFL, MFX_GT1 },
    { 0x9bac, MFX_HW_CFL, MFX_GT1 },
    { 0x9ba0, MFX_HW_CFL, MFX_GT1 },
    { 0x9ba5, MFX_HW_CFL, MFX_GT1 },
    { 0x9ba8, MFX_HW_CFL, MFX_GT1 },
    { 0x9ba4, MFX_HW_CFL, MFX_GT1 },
    { 0x9ba2, MFX_HW_CFL, MFX_GT1 },

    /* CML GT2 */
    { 0x9b41, MFX_HW_CFL, MFX_GT2 },
    { 0x9bca, MFX_HW_CFL, MFX_GT2 },
    { 0x9bcb, MFX_HW_CFL, MFX_GT2 },
    { 0x9bcc, MFX_HW_CFL, MFX_GT2 },
    { 0x9bc0, MFX_HW_CFL, MFX_GT2 },
    { 0x9bc5, MFX_HW_CFL, MFX_GT2 },
    { 0x9bc8, MFX_HW_CFL, MFX_GT2 },
    { 0x9bc4, MFX_HW_CFL, MFX_GT2 },
    { 0x9bc2, MFX_HW_CFL, MFX_GT2 },
    { 0x9bc6, MFX_HW_CFL, MFX_GT2 },
    { 0x9be6, MFX_HW_CFL, MFX_GT2 },
    { 0x9bf6, MFX_HW_CFL, MFX_GT2 },


    /* CNL */
    { 0x5A51, MFX_HW_CNL, MFX_GT2 },
    { 0x5A52, MFX_HW_CNL, MFX_GT2 },
    { 0x5A5A, MFX_HW_CNL, MFX_GT2 },
    { 0x5A40, MFX_HW_CNL, MFX_GT2 },
    { 0x5A42, MFX_HW_CNL, MFX_GT2 },
    { 0x5A4A, MFX_HW_CNL, MFX_GT2 },
    { 0x5A4C, MFX_HW_CNL, MFX_GT1 },
    { 0x5A50, MFX_HW_CNL, MFX_GT2 },
    { 0x5A54, MFX_HW_CNL, MFX_GT1 },
    { 0x5A59, MFX_HW_CNL, MFX_GT2 },
    { 0x5A5C, MFX_HW_CNL, MFX_GT1 },
    { 0x5A41, MFX_HW_CNL, MFX_GT2 },
    { 0x5A44, MFX_HW_CNL, MFX_GT1 },
    { 0x5A49, MFX_HW_CNL, MFX_GT2 },

    /* ICL LP */
    { 0xFF05, MFX_HW_ICL_LP, MFX_GT1 },
    { 0x8A50, MFX_HW_ICL_LP, MFX_GT2 },
    { 0x8A51, MFX_HW_ICL_LP, MFX_GT2 },
    { 0x8A52, MFX_HW_ICL_LP, MFX_GT2 },
    { 0x8A53, MFX_HW_ICL_LP, MFX_GT2 },
    { 0x8A54, MFX_HW_ICL_LP, MFX_GT1 },
    { 0x8A56, MFX_HW_ICL_LP, MFX_GT1 },
    { 0x8A57, MFX_HW_ICL_LP, MFX_GT1 },
    { 0x8A58, MFX_HW_ICL_LP, MFX_GT1 },
    { 0x8A59, MFX_HW_ICL_LP, MFX_GT1 },
    { 0x8A5A, MFX_HW_ICL_LP, MFX_GT1 },
    { 0x8A5B, MFX_HW_ICL_LP, MFX_GT1 },
    { 0x8A5C, MFX_HW_ICL_LP, MFX_GT1 },
    { 0x8A5D, MFX_HW_ICL_LP, MFX_GT1 },
    { 0x8A70, MFX_HW_ICL_LP, MFX_GT1 },
    { 0x8A71, MFX_HW_ICL_LP, MFX_GT1 },  // GT05, but 1 ok in this context

    /* JSL */
    { 0x4E51, MFX_HW_JSL, MFX_GT2 },
    { 0x4E55, MFX_HW_JSL, MFX_GT2 },
    { 0x4E61, MFX_HW_JSL, MFX_GT2 },
    { 0x4E71, MFX_HW_JSL, MFX_GT2 },

    /* EHL */
    { 0x4500, MFX_HW_EHL, MFX_GT2 },
    { 0x4541, MFX_HW_EHL, MFX_GT2 },
    { 0x4551, MFX_HW_EHL, MFX_GT2 },
    { 0x4555, MFX_HW_EHL, MFX_GT2 },
    { 0x4569, MFX_HW_EHL, MFX_GT2 },
    { 0x4571, MFX_HW_EHL, MFX_GT2 },

    /* TGL */
    { 0x9A40, MFX_HW_TGL_LP, MFX_GT2 },
    { 0x9A49, MFX_HW_TGL_LP, MFX_GT2 },
    { 0x9A59, MFX_HW_TGL_LP, MFX_GT2 },
    { 0x9A60, MFX_HW_TGL_LP, MFX_GT2 },
    { 0x9A68, MFX_HW_TGL_LP, MFX_GT2 },
    { 0x9A70, MFX_HW_TGL_LP, MFX_GT2 },
    { 0x9A78, MFX_HW_TGL_LP, MFX_GT2 },

    /* DG1/SG1 */
    { 0x4905, MFX_HW_DG1, MFX_GT2 },
    { 0x4906, MFX_HW_DG1, MFX_GT2 },
    { 0x4907, MFX_HW_DG1, MFX_GT2 }, // SG1
    { 0x4908, MFX_HW_DG1, MFX_GT2 },

    /* RKL */
    { 0x4C80, MFX_HW_RKL, MFX_GT1 }, // RKL-S
    { 0x4C8A, MFX_HW_RKL, MFX_GT1 }, // RKL-S
    { 0x4C81, MFX_HW_RKL, MFX_GT1 }, // RKL-S
    { 0x4C8B, MFX_HW_RKL, MFX_GT1 }, // RKL-S
    { 0x4C90, MFX_HW_RKL, MFX_GT1 }, // RKL-S
    { 0x4C9A, MFX_HW_RKL, MFX_GT1 }, // RKL-S

    /* ADL-S */
    { 0x4680, MFX_HW_ADL_S, MFX_GT1 },//ADL-S
    { 0x4681, MFX_HW_ADL_S, MFX_GT1 },//ADL-S
    { 0x4682, MFX_HW_ADL_S, MFX_GT1 },//ADL-S
    { 0x4683, MFX_HW_ADL_S, MFX_GT1 },//ADL-S
    { 0x4688, MFX_HW_ADL_S, MFX_GT1 },//ADL-S
    { 0x468A, MFX_HW_ADL_S, MFX_GT1 },//ADL-S
    { 0x468B, MFX_HW_ADL_S, MFX_GT1 },//ADL-S
    { 0x4690, MFX_HW_ADL_S, MFX_GT1 },//ADL-S
    { 0x4691, MFX_HW_ADL_S, MFX_GT1 },//ADL-S
    { 0x4692, MFX_HW_ADL_S, MFX_GT1 },//ADL-S
    { 0x4693, MFX_HW_ADL_S, MFX_GT1 },//ADL-S
    { 0x4698, MFX_HW_ADL_S, MFX_GT1 },//ADL-S
    { 0x4699, MFX_HW_ADL_S, MFX_GT1 },//ADL-S

    /* RPL-S */
    { 0xA780, MFX_HW_ADL_S, MFX_GT1 },//RPL-S
    { 0xA781, MFX_HW_ADL_S, MFX_GT1 },//RPL-S
    { 0xA782, MFX_HW_ADL_S, MFX_GT1 },//RPL-S
    { 0xA783, MFX_HW_ADL_S, MFX_GT1 },//RPL-S
    { 0xA784, MFX_HW_ADL_S, MFX_GT1 },//RPL-S
    { 0xA785, MFX_HW_ADL_S, MFX_GT1 },//RPL-S
    { 0xA786, MFX_HW_ADL_S, MFX_GT1 },//RPL-S
    { 0xA787, MFX_HW_ADL_S, MFX_GT1 },//RPL-S
    { 0xA788, MFX_HW_ADL_S, MFX_GT1 },//RPL-S
    { 0xA789, MFX_HW_ADL_S, MFX_GT1 },//RPL-S
    { 0xA78A, MFX_HW_ADL_S, MFX_GT1 },//RPL-S
    { 0xA78B, MFX_HW_ADL_S, MFX_GT1 },//RPL-S
    { 0xA78C, MFX_HW_ADL_S, MFX_GT1 },//RPL-S
    { 0xA78D, MFX_HW_ADL_S, MFX_GT1 },//RPL-S
    { 0xA78E, MFX_HW_ADL_S, MFX_GT1 },//RPL-S

    { 0xA7AA, MFX_HW_ADL_S, MFX_GT1 },//RPL 
    { 0xA7AB, MFX_HW_ADL_S, MFX_GT1 },//RPL 
    { 0xA7AC, MFX_HW_ADL_S, MFX_GT1 },//RPL 
    { 0xA7AD, MFX_HW_ADL_S, MFX_GT1 },//RPL

    /* ADL-P */
    { 0x46A0, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x46A1, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x46A2, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x46A3, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x46A6, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x4626, MFX_HW_ADL_P, MFX_GT2 },//ADL-P

    { 0x46B0, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x46B1, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x46B2, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x46B3, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x46A8, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x4628, MFX_HW_ADL_P, MFX_GT2 },//ADL-P

    { 0x46C0, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x46C1, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x46C2, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x46C3, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x46AA, MFX_HW_ADL_P, MFX_GT2 },//ADL-P
    { 0x462A, MFX_HW_ADL_P, MFX_GT2 },//ADL-P

    /* RPL-P */
    { 0xA7A0, MFX_HW_ADL_P, MFX_GT2 },
    { 0xA720, MFX_HW_ADL_P, MFX_GT2 },
    { 0xA7A8, MFX_HW_ADL_P, MFX_GT2 },
    { 0xA7A1, MFX_HW_ADL_P, MFX_GT2 },
    { 0xA721, MFX_HW_ADL_P, MFX_GT2 },
    { 0xA7A9, MFX_HW_ADL_P, MFX_GT2 },

    /* ADL-N */
    { 0x46D0, MFX_HW_ADL_N, MFX_GT1 },//ADL-N
    { 0x46D1, MFX_HW_ADL_N, MFX_GT1 },//ADL-N
    { 0x46D2, MFX_HW_ADL_N, MFX_GT1 },//ADL-N

    /* TWL */
    { 0x46D3, MFX_HW_ADL_N, MFX_GT1 },//TWL
    { 0x46D4, MFX_HW_ADL_N, MFX_GT1 },//TWL

    /* DG2 */
    { 0x4F80, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x4F81, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x4F82, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x4F83, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x4F84, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x4F85, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x4F86, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x4F87, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x4F88, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x5690, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x5691, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x5692, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x5693, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x5694, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x5695, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x5696, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x5697, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56A0, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56A1, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56A2, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56A3, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56A4, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56A5, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56A6, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56B0, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56B1, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56B2, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56B3, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56C0, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56C1, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56BA, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56BB, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56BC, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56BD, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56BE, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56BF, MFX_HW_DG2, MFX_GT4 }, // DG2
    { 0x56C2, MFX_HW_DG2, MFX_GT4 }, // DG2

    /* PVC */
    { 0x0BD0, MFX_HW_PVC, MFX_GT4 },
    { 0x0BD5, MFX_HW_PVC, MFX_GT4 },
    { 0x0BD6, MFX_HW_PVC, MFX_GT4 },
    { 0x0BD7, MFX_HW_PVC, MFX_GT4 },
    { 0x0BD8, MFX_HW_PVC, MFX_GT4 },
    { 0x0BD9, MFX_HW_PVC, MFX_GT4 },
    { 0x0BDA, MFX_HW_PVC, MFX_GT4 },
    { 0x0BDB, MFX_HW_PVC, MFX_GT4 },
    { 0x0BE0, MFX_HW_PVC, MFX_GT4 },
    { 0x0BE1, MFX_HW_PVC, MFX_GT4 },
    { 0x0BE5, MFX_HW_PVC, MFX_GT4 },


    /* MTL */
    { 0x7D40, MFX_HW_MTL, MFX_GT2 },
    { 0x7D50, MFX_HW_MTL, MFX_GT2 },
    { 0x7D55, MFX_HW_MTL, MFX_GT2 },
    { 0x7D57, MFX_HW_MTL, MFX_GT2 },
    { 0x7D60, MFX_HW_MTL, MFX_GT2 },
    { 0x7D70, MFX_HW_MTL, MFX_GT2 },
    { 0x7D75, MFX_HW_MTL, MFX_GT2 },
    { 0x7D79, MFX_HW_MTL, MFX_GT2 },
    { 0x7D76, MFX_HW_MTL, MFX_GT2 },
    { 0x7D66, MFX_HW_MTL, MFX_GT2 },
    { 0x7DD5, MFX_HW_MTL, MFX_GT2 },
    { 0x7DD7, MFX_HW_MTL, MFX_GT2 },
    { 0x7D45, MFX_HW_MTL, MFX_GT2 },
    { 0x7DE0, MFX_HW_MTL, MFX_GT2 },

    /* ARL S */
    { 0x7D67, MFX_HW_ARL, MFX_GT2 },

    /* ARL H*/
    { 0x7D51, MFX_HW_ARL, MFX_GT2 },
    { 0x7DD1, MFX_HW_ARL, MFX_GT2 },
    { 0x7D41, MFX_HW_ARL, MFX_GT2 },


    /* LNL */
    { 0x6420, MFX_HW_LNL, MFX_GT2 },
    { 0x6480, MFX_HW_LNL, MFX_GT2 },
    { 0x64A0, MFX_HW_LNL, MFX_GT2 },
    { 0x64B0, MFX_HW_LNL, MFX_GT2 },

};

enum
{
    MFX_FOURCC_IMC3         = MFX_MAKEFOURCC('I','M','C','3'),
    MFX_FOURCC_YUV400       = MFX_MAKEFOURCC('4','0','0','P'),
    MFX_FOURCC_YUV411       = MFX_MAKEFOURCC('4','1','1','P'),
    MFX_FOURCC_YUV422H      = MFX_MAKEFOURCC('4','2','2','H'),
    MFX_FOURCC_YUV422V      = MFX_MAKEFOURCC('4','2','2','V'),
    MFX_FOURCC_YUV444       = MFX_MAKEFOURCC('4','4','4','P'),
};

#ifdef __cplusplus
};
#endif

#endif // __MFXSTRUCTURES_INT_H__
