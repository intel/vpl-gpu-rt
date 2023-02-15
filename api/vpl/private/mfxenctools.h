/******************************************************************************* *\

Copyright (C) 2019-2020 Intel Corporation.  All rights reserved.

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

File Name: mfxenctools.h

*******************************************************************************/
#ifndef __MFXENCTOOLS_H__
#define __MFXENCTOOLS_H__

#include "mfxvideo++.h"
#include "mfxbrc.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* Extended Buffer Ids */
enum {
    MFX_EXTBUFF_ENCTOOLS_CONFIG = MFX_MAKEFOURCC('E', 'T', 'C', 'F'), 
};

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct
{
    mfxExtBuffer      Header;
    mfxStructVersion  Version;
    mfxU16            AdaptiveI;
    mfxU16            AdaptiveB;
    mfxU16            AdaptiveRefP;
    mfxU16            AdaptiveRefB;
    mfxU16            SceneChange;
    mfxU16            AdaptiveLTR;
    mfxU16            AdaptivePyramidQuantP;
    mfxU16            AdaptivePyramidQuantB;
    mfxU16            AdaptiveQuantMatrices;
    mfxU16            AdaptiveMBQP;
    mfxU16            BRCBufferHints;
    mfxU16            BRC;
    mfxU16            SaliencyMapHint;
    mfxU16            reserved[19];
} mfxExtEncToolsConfig;
MFX_PACK_END()

#define MFX_ENCTOOLS_CONFIG_VERSION MFX_STRUCT_VERSION(1, 0)

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif

