/******************************************************************************* *\

Copyright (C) 2021-2022 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfxav1.h

*******************************************************************************/
#if !defined(__MFXAV1_H__)
#define __MFXAV1_H__

#include "mfxdefs.h"
#include "mfxstructures.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* Extended Buffer Ids */
enum {
    MFX_EXTBUFF_AV1_AUXDATA                     = MFX_MAKEFOURCC('1', 'A', 'U', 'X'),
};

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer Header;

    mfxU8   StillPictureMode;         /* tri-state option */
    mfxU8   UseAnnexB;                /* tri-state option */
    mfxU8   PackOBUFrame;             /* tri-state option */
    mfxU8   InsertTemporalDelimiter;  /* tri-state option */

    mfxU8   EnableCdef;               /* tri-state option */
    mfxU8   EnableRestoration;        /* tri-state option */

    mfxU8   EnableLoopFilter;         /* tri-state option */
    mfxU8   LoopFilterSharpness;      /* 0..8, 0 = default, map to bitstream: [1..8] => [0..7] */

    mfxU8   EnableSuperres;           /* tri-state option */
    mfxU8   SuperresScaleDenominator; /* 9..16, 0 = default */

    mfxU8   SegmentationMode;         /* see enum AV1SegmentMode*/
    mfxU8   InterpFilter;             /* see enum AV1InterpolationMode */

    mfxU8   DisableCdfUpdate;         /* tri-state option */
    mfxU8   DisableFrameEndUpdateCdf; /* tri-state option */

    mfxU8   UniformTileSpacing;       /* tri-state option */
    mfxU8   ContextUpdateTileIdPlus1; /* Minus 1 specifies context_update_tile_id */

    mfxU16  SwitchInterval;           /* interval, 0 - disabled */

    mfxU16  NumTilesPerTileGroup[256];
    mfxU16  TileWidthInSB[128];
    mfxU16  TileHeightInSB[128];

    struct {
        mfxU8  CdefDampingMinus3;   /* 0..3 */
        mfxU8  CdefBits;            /* 0..3 */
        mfxU8  CdefYStrengths[8];   /* 0..63 */
        mfxU8  CdefUVStrengths[8];  /* 0..63 */
        mfxU8  reserved[14];
    } Cdef;

    struct {
        mfxU8  LFLevelYVert;        /* 0..63 */
        mfxU8  LFLevelYHorz;        /* 0..63 */
        mfxU8  LFLevelU;            /* 0..63 */
        mfxU8  LFLevelV;            /* 0..63 */
        mfxU8  ModeRefDeltaEnabled; /* 0, 1 */
        mfxU8  ModeRefDeltaUpdate;  /* 0, 1 */
        mfxI8  RefDeltas[8];        /* -63..63 */
        mfxI8  ModeDeltas[2];       /* -63..63 */
        mfxU8  reserved[16];
    } LoopFilter;

    struct {
        mfxI8  YDcDeltaQ;           /* -63..63 */
        mfxI8  UDcDeltaQ;           /* -63..63 */
        mfxI8  VDcDeltaQ;           /* -63..63 */
        mfxI8  UAcDeltaQ;           /* -63..63 */
        mfxI8  VAcDeltaQ;           /* -63..63 */
        mfxU8  MinBaseQIndex;
        mfxU8  MaxBaseQIndex;
        mfxU8  reserved[25];
    } QP;

    mfxU8  ErrorResilientMode;          /* tri-state option */
    mfxU8  EnableOrderHint;             /* tri-state option */
    mfxU8  OrderHintBits;               /* 0..8, 0 = default */
    mfxU8  DisplayFormatSwizzle;        /* 0, 1 */
    mfxU16 Palette;                     /* tri-state option */
    mfxU16 IBC;                         /* tri-state option */
    mfxU16 SegmentTemporalUpdate;       /* tri-state option */

    mfxU8  reserved[52];
} mfxExtAV1AuxData;
MFX_PACK_END()

/* AV1InterpolationMode */
enum {
    MFX_AV1_INTERP_DEFAULT         = 0,
    MFX_AV1_INTERP_EIGHTTAP        = 1,
    MFX_AV1_INTERP_EIGHTTAP_SMOOTH = 2,
    MFX_AV1_INTERP_EIGHTTAP_SHARP  = 3,
    MFX_AV1_INTERP_BILINEAR        = 4,
    MFX_AV1_INTERP_SWITCHABLE      = 5
};

#if defined(__cplusplus)
} // extern "C"
#endif

#endif //!defined(__MFXAV1_H__)
