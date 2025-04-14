/******************************************************************************* *\

Copyright (C) 2019-2022 Intel Corporation.  All rights reserved.

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

File Name: mfxenctools-int.h

*******************************************************************************/
#ifndef __MFXENCTOOLS_INT_H__
#define __MFXENCTOOLS_INT_H__

#include "mfx_config.h"
#include "mfxbrc.h"

#include "mfxenctools.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */



/* Extended Buffer Ids */
enum {
    MFX_EXTBUFF_ENCTOOLS = MFX_MAKEFOURCC('E', 'E', 'T', 'L'),
    MFX_EXTBUFF_ENCTOOLS_DEVICE = MFX_MAKEFOURCC('E', 'T', 'E', 'D'),
    MFX_EXTBUFF_ENCTOOLS_ALLOCATOR = MFX_MAKEFOURCC('E', 'T', 'E', 'A'),
    MFX_EXTBUFF_ENCTOOLS_FRAME_TO_ANALYZE = MFX_MAKEFOURCC('E', 'F', 'T', 'A'),
    MFX_EXTBUFF_ENCTOOLS_HINT_SCENE_CHANGE = MFX_MAKEFOURCC('E', 'H', 'S', 'C'),
    MFX_EXTBUFF_ENCTOOLS_HINT_GOP = MFX_MAKEFOURCC('E', 'H', 'G', 'O'),
    MFX_EXTBUFF_ENCTOOLS_HINT_AREF = MFX_MAKEFOURCC('E', 'H', 'A', 'R'),
    MFX_EXTBUFF_ENCTOOLS_BRC_BUFFER_HINT = MFX_MAKEFOURCC('E', 'B', 'B', 'H'),
    MFX_EXTBUFF_ENCTOOLS_BRC_FRAME_PARAM = MFX_MAKEFOURCC('E', 'B', 'F', 'P'),
    MFX_EXTBUFF_ENCTOOLS_BRC_QUANT_CONTROL = MFX_MAKEFOURCC('E', 'B', 'Q', 'C'),
    MFX_EXTBUFF_ENCTOOLS_BRC_HRD_POS = MFX_MAKEFOURCC('E', 'B', 'H', 'P'),
    MFX_EXTBUFF_ENCTOOLS_BRC_ENCODE_RESULT = MFX_MAKEFOURCC('E', 'B', 'E', 'R'),
    MFX_EXTBUFF_ENCTOOLS_BRC_STATUS = MFX_MAKEFOURCC('E', 'B', 'S', 'T'),
    MFX_EXTBUFF_ENCTOOLS_HINT_MATRIX = MFX_MAKEFOURCC('E', 'H', 'Q', 'M'),
    MFX_EXTBUFF_ENCTOOLS_HINT_QPMAP  = MFX_MAKEFOURCC('E', 'H', 'Q', 'P'),
    MFX_EXTBUFF_ENCTOOLS_HINT_SALIENCY_MAP  = MFX_MAKEFOURCC('E', 'H', 'S', 'M'),
    MFX_EXTBUFF_ENCTOOLS_PREFILTER_PARAM  = MFX_MAKEFOURCC('E', 'P', 'R', 'P')
};

enum
{
    MFX_BRC_NO_HRD = 0,
    MFX_BRC_HRD_WEAK,  /* IF HRD CALCULATION IS REQUIRED, BUT NOT WRITTEN TO THE STREAM */
    MFX_BRC_HRD_STRONG,
};

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct
{
    mfxStructVersion Version;
    mfxU8   GopOptFlag;
    mfxU8   reserved8b;
    mfxU16  AsyncDepth;
    mfxU16  reserved[1];

    /* info about codec */
    struct  /* coding info*/
    {
        mfxU32  CodecId;
        mfxU16  CodecProfile;
        mfxU16  CodecLevel;
        mfxU16  LowPower;
        mfxU16  reserved2[63];
    };
    struct      /* input frames info */
    {
        /* info about input frames */
        mfxFrameInfo FrameInfo;
        mfxU16  IOPattern;
        mfxU16 MaxDelayInFrames;
        mfxU16  reserved3[64];
    };
    struct
    {
        /* Gop limitations */
        mfxU16  MaxGopSize;
        mfxU16  MaxGopRefDist;
        mfxU32  MaxIDRDist;
        mfxU16  BRefType;
        mfxU16  reserved4[63];
    };
    mfxU16 ScenarioInfo;

    struct  /* Rate control info */
    {
        mfxU16  RateControlMethod;          /* CBR, VBR, CRF, CQP */
        mfxU32  TargetKbps;                 /* ignored for CRF, CQP */
        mfxU32  MaxKbps;                    /* ignored for CRF, CQP */
        mfxU16  QPLevel[3];                 /* for CRF, CQP */

        mfxU16  HRDConformance;             /* for CBR & VBR */
        mfxU32  BufferSizeInKB;             /* if HRDConformance is ON */
        mfxU32  InitialDelayInKB;           /* if HRDConformance is ON */

        mfxU32  ConvergencePeriod;          /* if HRDConformance is OFF, 0 - the period is whole stream */
        mfxU32  Accuracy;                   /* if HRDConformance is OFF */

        mfxU32  WinBRCMaxAvgKbps;           /* sliding window restriction */
        mfxU16  WinBRCSize;                 /* sliding window restriction */

        mfxU32  MaxFrameSizeInBytes[3];     /* MaxFrameSize limitation */

        mfxU16  MinQPLevel[3];              /* QP range  limitations */
        mfxU16  MaxQPLevel[3];              /* QP range  limitations */

        mfxU32  PanicMode;

        mfxU16  LaQp;                       /* QP used for LookAhead encode */
        mfxU16  LaScale;                    /* Downscale Factor for LookAhead encode */


        mfxU16  NumRefP;                     /* Number for Reference allowed for P frames to handle reflists*/

        mfxU16  reserved5[60];
    };
    mfxU16 LAMode;
    mfxU16 NumExtParam;
    mfxExtBuffer** ExtParam;
} mfxEncToolsCtrl;
MFX_PACK_END()

#define MFX_ENCTOOLS_CTRL_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxExtBuffer      Header;
    mfxStructVersion  Version;
    mfxU16            reserved[3];
    mfxHDL            DeviceHdl;
    mfxU32            HdlType;
    mfxU32            reserved2[3];
} mfxEncToolsCtrlExtDevice;
MFX_PACK_END()

#define MFX_ENCTOOLS_CTRL_EXTDEVICE_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxExtBuffer       Header;
    mfxStructVersion   Version;
    mfxU16             reserved[3];
    mfxFrameAllocator *pAllocator;
    mfxU32             reserved2[4];
} mfxEncToolsCtrlExtAllocator;
MFX_PACK_END()

#define MFX_ENCTOOLS_CTRL_EXTALLOCATOR_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxStructVersion  Version;
    mfxU16            reserved;
    mfxU32            DisplayOrder;
    mfxExtBuffer    **ExtParam;
    mfxU16            NumExtParam;
    mfxU16            reserved2;
    mfxU32            reserved3[3];
} mfxEncToolsTaskParam;
MFX_PACK_END()

#define MFX_ENCTOOLS_TASKPARAM_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct
{
    mfxExtBuffer      Header;
    mfxStructVersion  Version;
    mfxU16            reserved[3];
    mfxFrameSurface1 *Surface;        /* Input surface */
    mfxU32            reserved2[2];
} mfxEncToolsFrameToAnalyze;
MFX_PACK_END()

#define MFX_ENCTOOLS_FRAMETOANALYZE_VERSION MFX_STRUCT_VERSION(1, 0)

#define MFX_ENCTOOLS_PREENC_MAP_WIDTH 16
#define MFX_ENCTOOLS_PREENC_MAP_SIZE 128

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer      Header;
    mfxStructVersion  Version;
    mfxU16            reserved[3];
    mfxU16            SceneChangeFlag;
    mfxU16            RepeatedFrameFlag;
    mfxU32            TemporalComplexity;
    mfxU16            SpatialComplexity;    // Frame Spatial Complexity (RsCs) computed at low res
    /* Persistence Parameters */
    /* Persistence of a block = number of frames the blk persists without much change */
    mfxU16            PersistenceMapNZ; // If !0, Peristence Map has some Non Zero values
    mfxU8             PersistenceMap[MFX_ENCTOOLS_PREENC_MAP_SIZE];
    mfxU32            reserved2[2];
} mfxEncToolsHintPreEncodeSceneChange;
MFX_PACK_END()

#define MFX_ENCTOOLS_HINTPREENCODE_SCENECHANGE_VERSION MFX_STRUCT_VERSION(1, 0)

enum
{
    MFX_QUANT_MATRIX_DEFAULT = 0,
    MFX_QUANT_MATRIX_FLAT,
    MFX_QUANT_MATRIX_WEAK,
    MFX_QUANT_MATRIX_MEDIUM,
    MFX_QUANT_MATRIX_STRONG,
    MFX_QUANT_MATRIX_EXTREME
};

enum
{
    MFX_VPP_LOOKAHEAD = 0,
    MFX_FASTPASS_LOOKAHEAD = 1
};

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer     Header;
    mfxStructVersion Version;
    mfxU16           reserved[2];
    mfxU16           MatrixType; /* enum */
} mfxEncToolsHintQuantMatrix;
MFX_PACK_END()

#define MFX_ENCTOOLS_HINT_QUANTMATRIX_VERSION MFX_STRUCT_VERSION(1, 0)

#define MFX_QP_UNDEFINED 0x1111

#define MAX_QP_MODULATION 5

enum
{
    MFX_QP_MODULATION_NOT_DEFINED = 0,  /* QPModulation type for the frame is not defined. */
    MFX_QP_MODULATION_LOW,              /* Use low pyramid delta QP for this frame. This type of content prefers low delta QP between P/B Layers. */
    MFX_QP_MODULATION_MEDIUM,           /* Use medium pyramid delta QP for this frame. This type of content prefers medium delta QP between P/B Layers. */
    MFX_QP_MODULATION_HIGH,             /* Use high pyramid delta QP for this frame. This type of content prefers high delta QP between P/B Layers. */
    MFX_QP_MODULATION_MIXED,            /* Use pyramid delta QP appropriate for mixed content. */
#if defined(MFX_ENABLE_ENCTOOLS)
    MFX_QP_MODULATION_EXPLICIT,         /* Use explicit pyramid delta QP. */
#endif
    MFX_QP_MODULATION_RESERVED0
};
MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer      Header;
    mfxStructVersion  Version;
    mfxU16            reserved[2];
    mfxU16            QpMapFilled;     /* If !0, QP Map is filled */
    mfxExtMBQP        ExtQpMap;        /* Per Block QP Map for current frame */
    mfxU16            QpMapPitch;      /* Additional parameter for ExtQpMap, number QPs per MB line */
    mfxU16            reserved2[9];
} mfxEncToolsHintQPMap;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
/*
This buffer can be used by encoder to get saliency map from enctools. Encoder should allocate 
sufficient "SaliencyMap" buffer and set its size in "AllocatedSize" before calling enctools. 
Enctools fill this buffer with saliency map and set "Width", "Height" and "BlockSize". If buffer
size is not sufficient, enctools do not modify "SaliencyMap", but set "Width", "Height", 
"BlockSize" and return MFX_ERR_NOT_ENOUGH_BUFFER. Encoder should reallocate buffer and 
call enctools one more time.
*/ 
typedef struct {
    mfxExtBuffer      Header;        /* MFX_EXTBUFF_ENCTOOLS_HINT_SALIENCY_MAP */
    mfxStructVersion  Version;
    mfxU32            Width;         /* width of the map in blocks, set by enctools */
    mfxU32            Height;        /* height of the map in blocks, set by enctools */
    mfxU32            BlockSize;     /* block size of the saliency map in pixels, set by enctools */
    mfxU32            AllocatedSize; /* number of elements (blocks) allocated in "SaliencyMap" array, set by encoder */
    mfxF32*           SaliencyMap;   /* saliency map, in 0..1 range, allocated by encoder */
    mfxU32            reserved[64];
} mfxEncToolsHintSaliencyMap;
MFX_PACK_END()

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer      Header;
    mfxStructVersion  Version;
    mfxU16            reserved[2];
    mfxU16            FrameType;
    mfxI16            QPDelta;
    mfxU16            QPModulation;    /* enum */
    mfxU16            MiniGopSize;     /* Adaptive GOP decision for the frame */
#if defined(MFX_ENABLE_ENCTOOLS)
    mfxI8             QPDeltaExplicitModulation;     /* Explicit adaptive QP offset */
    mfxU8             reserved8bits;
#endif
    mfxU16            reserved2[5];
} mfxEncToolsHintPreEncodeGOP;
MFX_PACK_END()

#define MFX_ENCTOOLS_HINTPREENCODE_GOP_VERSION MFX_STRUCT_VERSION(1, 0)

enum
{
    MFX_REF_FRAME_NORMAL = 0,
    MFX_REF_FRAME_TYPE_LTR,
    MFX_REF_FRAME_TYPE_KEY
};

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {                               /* only for progressive. Need to think about interlace for future support */
    mfxExtBuffer      Header;
    mfxStructVersion  Version;
    mfxU16            reserved[2];
    mfxU16            CurrFrameType;           /* enum */
    mfxU32            PreferredRefList[16];
    mfxU32            RejectedRefList[16];
    mfxU16            PreferredRefListSize;
    mfxU16            RejectedRefListSize;
    mfxU32            LongTermRefList[16];
    mfxU16            LongTermRefListSize;
    mfxU16            reserved2[6];
} mfxEncToolsHintPreEncodeARefFrames;         /* Output structure */
MFX_PACK_END()

#define MFX_ENCTOOLS_HINTPREENCODE_AREFFRAMES_VERSION MFX_STRUCT_VERSION(1, 0)

enum
{
    MFX_BUFFERHINT_OUTPUT_ENCORDER = 0,
    MFX_BUFFERHINT_OUTPUT_DISPORDER
};

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer      Header;
    mfxStructVersion  Version;
    mfxU16            reserved[3];
    mfxU32            OptimalFrameSizeInBytes;
    mfxU32            AvgEncodedSizeInBits;         /* Average size of encoded Lookahead frames in bits */
    mfxU32            CurEncodedSizeInBits;         /* Size of encoded Lookahead frame at current frame location in bits */
    mfxU16            DistToNextI;                  /* Distance to next I Frame in Lookahead frames (0 if not found) */
    mfxU16            OutputMode;                   /* enum */
    mfxU32            reserved2[4];
} mfxEncToolsBRCBufferHint;
MFX_PACK_END()

#define MFX_ENCTOOLS_BRCBUFFER_HINT_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer      Header;
    mfxStructVersion  Version;
    mfxU16            reserved[3];
    mfxU16            FrameType;       /* See FrameType enumerator */
    mfxU16            PyramidLayer;    /* B-pyramid or P-pyramid layer frame belongs to */
    mfxU32            EncodeOrder;     /* Frame number in a sequence of reordered frames starting from encoder Init() */
    mfxU16            SceneChange;     // Frame is Scene Chg frame
    mfxU16            LongTerm;        // Frame is long term refrence
    mfxU16            SpatialComplexity; // Frame Spatial Complexity (RsCs) computed at low res
    /* Persistence Parameters */
    /* Persistence of a block = number of frames the blk persists without much change */
    mfxU16            PersistenceMapNZ; // If !0, Peristence Map has some Non Zero values
    mfxU8             PersistenceMap[MFX_ENCTOOLS_PREENC_MAP_SIZE];
    mfxU32            reserved1[1];
} mfxEncToolsBRCFrameParams;
MFX_PACK_END()

#define MFX_ENCTOOLS_BRCFRAMEPARAMS_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer      Header;
    mfxStructVersion  Version;
    mfxU16            reserved[3];
    mfxU32            QpY;             /* Frame-level Luma QP. Mandatory */
    mfxU32            MaxFrameSize;    /* Max frame size in bytes (used for rePak). Optional */
    mfxU8             DeltaQP[8];      /* deltaQP[i] is added to QP value while ith-rePakOptional */
    mfxU16            NumDeltaQP;      /* Max number of rePaks to provide MaxFrameSize (from 0 to 8) */
    mfxU16            reserved2[5];
} mfxEncToolsBRCQuantControl;
MFX_PACK_END()

#define MFX_ENCTOOLS_BRCQUANTCONTROL_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer      Header;
    mfxStructVersion  Version;
    mfxU16            reserved[3];
    mfxU32            InitialCpbRemovalDelay;
    mfxU32            InitialCpbRemovalDelayOffset;
    mfxU32            reserved2[2];
} mfxEncToolsBRCHRDPos;
MFX_PACK_END()

#define MFX_ENCTOOLS_BRCHRDPOS_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer      Header;
    mfxStructVersion  Version;
    mfxU16            reserved[3];
    mfxU16            NumRecodesDone;  /* Number of recodings performed for this frame. Optional */
    mfxU16            QpY;             /* Luma QP the frame was encoded with */
    mfxU32            CodedFrameSize;  /* Size of frame in bytes after encoding */
    mfxU32            reserved2[2];
} mfxEncToolsBRCEncodeResult;
MFX_PACK_END()

#define MFX_ENCTOOLS_BRCENCODERESULT_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {
    mfxExtBuffer       Header;
    mfxStructVersion   Version;
    mfxU16             reserved[3];
    mfxBRCFrameStatus  FrameStatus;
} mfxEncToolsBRCStatus;
MFX_PACK_END()

#define MFX_ENCTOOLS_BRCSTATUS_VERSION MFX_STRUCT_VERSION(1, 0)


MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct
{
    mfxExtBuffer      Header;
    mfxFrameSurface1 *InSurface;
    mfxFrameSurface1 *OutSurface;
    mfxU32            QpY;             /* Frame-level Luma QP*/
    mfxU32            reserved[58];
} mfxEncToolsPrefilterParam;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxExtBuffer      Header;
    mfxStructVersion  Version;           /* what about to return version of EncTools containing commit_id - return through GetVersion? */
    mfxU16            reserved[3];       /* to align with Version */
    mfxU32            reserved2[14];
    mfxHDL            Context;
    mfxStatus(MFX_CDECL *Init)   (mfxHDL pthis, mfxExtEncToolsConfig*  config, mfxEncToolsCtrl* ctrl);
    mfxStatus(MFX_CDECL *GetSupportedConfig)     (mfxHDL pthis, mfxExtEncToolsConfig*  config, mfxEncToolsCtrl* ctrl);
    mfxStatus(MFX_CDECL *GetActiveConfig)  (mfxHDL pthis, mfxExtEncToolsConfig* pEncToolConfig);
    mfxStatus(MFX_CDECL *GetDelayInFrames)(mfxHDL pthis, mfxExtEncToolsConfig* pEncToolConfig, mfxEncToolsCtrl* ctrl, mfxU32 *numFrames);
    mfxStatus(MFX_CDECL *Reset)  (mfxHDL pthis, mfxExtEncToolsConfig* pEncToolConfig, mfxEncToolsCtrl* ctrl); /* asynchronous reset */
    mfxStatus(MFX_CDECL *Close)  (mfxHDL pthis);
    mfxStatus(MFX_CDECL *Submit) (mfxHDL pthis, mfxEncToolsTaskParam* sparam); /* Encoder knows how many buffers should be provided by app/encode and it waits for all of them */
    mfxStatus(MFX_CDECL *Query)  (mfxHDL pthis, mfxEncToolsTaskParam* param, mfxU32 timeout);
    mfxStatus(MFX_CDECL *Discard) (mfxHDL pthis, mfxU32 DisplayOrder);
    mfxHDL            reserved3[8];
} mfxEncTools;
MFX_PACK_END()

#define MFX_ENCTOOLS_VERSION MFX_STRUCT_VERSION(1, 0)

MFX_PACK_BEGIN_USUAL_STRUCT()
struct mfxLplastatus
{
#if defined(MFX_ENABLE_LPLA_BASE)
    mfxU8 ValidInfo = 0;
    mfxU8 CqmHint = 0xFF;
    mfxU32 TargetFrameSize = 0;
    mfxU8 MiniGopSize = 0;
    mfxU8 QpModulation = 0;
#endif
    mfxU32 AvgEncodedBits = 0;
    mfxU32 CurEncodedBits = 0;
    mfxU16 DistToNextI = 0;
};
MFX_PACK_END()

mfxEncTools*  MFX_CDECL MFXVideoENCODE_CreateEncTools(const mfxVideoParam& par);
void  MFX_CDECL MFXVideoENCODE_DestroyEncTools(mfxEncTools *et);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
#endif





