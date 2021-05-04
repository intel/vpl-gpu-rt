// Copyright (c) 2008-2019 Intel Corporation
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

#ifndef __MFX_H264_ENC_COMMON_H__
#define __MFX_H264_ENC_COMMON_H__

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_PAK) || defined (MFX_ENABLE_H264_VIDEO_ENC) || defined (MFX_ENABLE_H264_VIDEO_ENCODE)
#define MFX_ENABLE_H264_VIDEO_ENCODE
#define MFX_ENABLE_H264_VIDEO_ENCODER_COMMON

#include "mfx_enc_common.h"
#include "umc_h264_core_enc.h"
#include "umc_h264_defs.h"

extern UMC_H264_ENCODER::SB_Type mapSBTypeMFXUMC_B[4][4];
extern UMC_H264_ENCODER::SB_Type mapSBTypeMFXUMC_P[4];
extern mfxU8 mfx_mbtype[32];

typedef struct _MFX_SBTYPE{
    mfxU8 shape;
    mfxU8 pred;
} MFX_SBTYPE;

typedef Ipp64u LimitsTableA1[4][H264_LIMIT_TABLE_LEVEL_MAX+1][6];

extern  MFX_SBTYPE mfx_sbtype[17];
struct AVCTemporalLayers;

class mfxVideoInternalParam : public mfxVideoParam
{
public:
    mfxVideoInternalParam() = default;

    mfxVideoInternalParam(mfxVideoParam const &);

    mfxVideoInternalParam & operator = (mfxVideoParam const &);

    void SetCalcParams( mfxVideoParam *parMFX);

    void GetCalcParams( mfxVideoParam *parMFX);

    struct CalculableParam
    {
        mfxU32 BufferSizeInKB   = 0;
        mfxU32 InitialDelayInKB = 0;
        mfxU32 TargetKbps       = 0;
        mfxU32 MaxKbps;
    } calcParam;
};

// map: SubMbShape[0..3] and SubMbPredMode[0..2] -> UMC::SB_Type
static const mfxU8 BSliceSubMbTypeMfx2Umc[4][3] =
{
    { SBTYPE_FORWARD_8x8,  SBTYPE_BACKWARD_8x8, SBTYPE_BIDIR_8x8 },
    { SBTYPE_FORWARD_8x4,  SBTYPE_BACKWARD_8x4, SBTYPE_BIDIR_8x4 },
    { SBTYPE_FORWARD_4x8,  SBTYPE_BACKWARD_4x8, SBTYPE_BIDIR_4x8 },
    { SBTYPE_FORWARD_4x4,  SBTYPE_BACKWARD_4x4, SBTYPE_BIDIR_4x4 }
};

//Functions
UMC_H264_ENCODER::MBTypeValue ConvertMBTypeToUMC(const mfxMbCodeAVC& mbCode, bool biDir = true);
Ipp32s           ConvertColorFormat_H264enc(mfxU16 ColorFormat);
UMC::ColorFormat ConvertColorFormatToUMC(mfxU16 ColorFormat);
EnumPicCodType   ConvertPicType_H264enc(mfxU16 FrameType);
EnumPicClass     ConvertPicClass_H264enc(mfxU16 FrameType);
Ipp32s ConvertPicStruct_H264enc(mfxU16 PicStruct);
Ipp32u ConvertBitrate_H264enc(mfxU16 TargetKbpf, Ipp64f framerate);
mfxU32 ConvertSARtoIDC_H264enc(mfxU32 sarw, mfxU32 sarh);
mfxStatus ConvertStatus_H264enc(UMC::Status status);
mfxU16 ConvertCUCProfileToUMC( mfxU16 profile );
mfxU16 ConvertCUCLevelToUMC( mfxU16 level );
mfxU16 ConvertUMCProfileToCUC( mfxU16 profile );
mfxU16 ConvertUMCLevelToCUC( mfxU16 level );
mfxStatus ConvertVideoParamFromSPSPPS_H264enc( mfxVideoInternalParam *parMFX, UMC::H264SeqParamSet *seq_param, UMC::H264PicParamSet *pic_param);
mfxStatus ConvertVideoParam_H264enc( mfxVideoInternalParam *parMFX, UMC::H264EncoderParams *parUMC);
mfxStatus ConvertVideoParamBack_H264enc(mfxVideoInternalParam *parMFX, const UMC_H264_ENCODER::H264CoreEncoder_8u16s *enc);
mfxStatus CheckProfileLevelLimits_H264enc(mfxVideoInternalParam *parMFX, bool queryMode, mfxVideoInternalParam *parMFXSetByTU = 0);
mfxStatus CheckExtBuffers_H264enc(mfxExtBuffer** ebuffers, mfxU32 nbuffers);
Status SetConstQP(mfxU16 isEncodedOrder, UMC_H264_ENCODER::H264EncoderFrame_8u16s* currFrame, const UMC::H264EncoderParams *encParam);
void SetUMCFrame(H264EncoderFrame_8u16s* umcFrame, mfxExtAvcRefFrameParam* cucFrame);
void InitSliceFromCUC( int slice_num, mfxFrameCUC *cuc, H264Slice_8u16s *curr_slice );
void SetPlanePointers(mfxU32 fourCC, const mfxFrameData& data, H264EncoderFrame_8u16s& umcFrame);
mfxStatus LoadSPSPPS(const mfxVideoParam* in, H264SeqParamSet& seq_parms, H264PicParamSet& pic_parms);
Status H264CoreEncoder_PrepareRefPicMarking(H264CoreEncoder_8u16s* core_enc, mfxEncodeCtrl *ctrl, EnumPicClass& ePic_Class, AVCTemporalLayers *tempLayers);
mfxI64 CalculateDTSFromPTS_H264enc(mfxFrameInfo info, mfxU16 dpb_output_delay, mfxU64 TimeStamp);
void SetDefaultParamForReset(mfxVideoParam& parNew, const mfxVideoParam& parOld);

Status H264CoreEncoder_ReorderRefPicList( 
    EncoderRefPicListStruct_8u16s       *pRefListIn,
    EncoderRefPicListStruct_8u16s       *pRefListOut,
    mfxExtAVCRefListCtrl                *pRefPicListCtrl,
    Ipp16u                               numRefInList,
    Ipp16u                               &numRefAllowedInList,
    bool                                isFieldRefPicList);

Status H264CoreEncoder_BuildRefPicListModSyntax(
    H264CoreEncoder_8u16s*              core_enc,
    EncoderRefPicListStruct_8u16s       *pRefListDefault,
    EncoderRefPicListStruct_8u16s       *pRefListReordered,
    RefPicListReorderInfo               *pReorderInfo,
    Ipp16u                              numRefActiveInList);

void RejectPrevRefsEncOrder(
    mfxExtAVCRefListCtrl *pRefPicListCtrl,
    EncoderRefPicListStruct_8u16s *pRefList,
    RefPicListInfo *pRefPicListInfo,
    Ipp8u listID,
    H264EncoderFrame_8u16s *curFrm);

void RejectPastRefs(
    mfxExtAVCRefListCtrl *pRefPicListCtrl,
    EncoderRefPicListStruct_8u16s *pRefList,
    Ipp32s numRefsInList,
    H264EncoderFrame_8u16s *curFrm);

void RejectFutureRefs(
    mfxExtAVCRefListCtrl *pRefPicListCtrl,
    EncoderRefPicListStruct_8u16s *pRefList,
    Ipp32s numRefsInList,
    H264EncoderFrame_8u16s *curFrm);

mfxU16 GetTemporalLayer( mfxU32 currFrameNum, mfxU16 numLayers);
mfxU32 GetRefFrameNum( mfxU32 currFrameNum, mfxU16 numLayers);
mfxU16 GetMaxVmvR(mfxU16 CodecLevel);
void ClearRefListCtrl(mfxExtAVCRefListCtrl *pRefPicListCtrl);

//Inline functions
inline mfxU16 GetWidth (mfxVideoParam * pPar)
{
    return (pPar->mfx.FrameInfo.CropW!=0) ? pPar->mfx.FrameInfo.CropW : pPar->mfx.FrameInfo.Width;
}

inline mfxU16 GetHeight (mfxVideoParam * pPar)
{
    return (pPar->mfx.FrameInfo.CropH!=0) ? pPar->mfx.FrameInfo.CropH : pPar->mfx.FrameInfo.Height;
}

inline void MemorySet(Ipp8u *dst, Ipp32s val, Ipp32s length)
{
    memset(dst, val, length);
}

inline void MemorySet(Ipp16u *dst, Ipp32s val, Ipp32s length)
{
    ippsSet_16s((Ipp16s)val, (Ipp16s*)dst, length);
}

inline mfxExtBuffer* GetExtBuffer(mfxFrameCUC& cuc, mfxU32 extensionId)
{
    return GetExtBuffer(cuc.ExtBuffer, cuc.NumExtBuffer, extensionId);
}

inline bool IsPreferred(mfxU32 FrameOrder, mfxExtAVCRefListCtrl *pRefPicListCtrl)
{
    for (Ipp32s k = 0; k < 32; k ++)
        if (FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN) && pRefPicListCtrl->PreferredRefList[k].FrameOrder == FrameOrder &&
            (pRefPicListCtrl->PreferredRefList[k].PicStruct == MFX_PICSTRUCT_PROGRESSIVE || pRefPicListCtrl->PreferredRefList[k].PicStruct == MFX_PICSTRUCT_UNKNOWN))
            return true;

    return false;
}

inline bool IsRejected(mfxU32 FrameOrder, mfxExtAVCRefListCtrl *pRefPicListCtrl)
{
    for (Ipp32s k = 0; k < 16; k ++)
        if (FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN) && pRefPicListCtrl->RejectedRefList[k].FrameOrder == FrameOrder &&
            (pRefPicListCtrl->RejectedRefList[k].PicStruct == MFX_PICSTRUCT_PROGRESSIVE || pRefPicListCtrl->RejectedRefList[k].PicStruct == MFX_PICSTRUCT_UNKNOWN))
            return true;

    return false;
}

inline bool IsLongTerm(mfxU32 FrameOrder, mfxExtAVCRefListCtrl *pRefPicListCtrl)
{
    for (Ipp32s k = 0; k < 16; k ++)
        if (FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN) && pRefPicListCtrl->LongTermRefList[k].FrameOrder == FrameOrder &&
            (pRefPicListCtrl->LongTermRefList[k].PicStruct == MFX_PICSTRUCT_PROGRESSIVE || pRefPicListCtrl->LongTermRefList[k].PicStruct == MFX_PICSTRUCT_UNKNOWN))
            return true;

    return false;
}

mfxI16Pair* PackMVs(const H264MacroblockGlobalInfo* m_cur_mb, mfxI16Pair* pMV, H264MotionVector* mvL0, H264MotionVector* mvL1 );
void UnPackMVs(const H264MacroblockGlobalInfo* m_cur_mb, mfxI16Pair* pMV, H264MotionVector* mvL0, H264MotionVector* mvL1 );
void PackRefIdxs(const H264MacroblockGlobalInfo* mbinfo, mfxMbCode* mb, T_RefIdx* refL0, T_RefIdx* refL1);
void UnPackRefIdxs(const H264MacroblockGlobalInfo* mbinfo, mfxMbCodeAVC* mb, T_RefIdx* refL0, T_RefIdx* refL1);

class IncrementLocked
{
public:
    explicit IncrementLocked(mfxFrameData& fd) : m_fd(fd) { m_fd.Locked++; }
    ~IncrementLocked() { m_fd.Locked--; }
private:
    IncrementLocked(const IncrementLocked&);
    IncrementLocked& operator=(const IncrementLocked&);
    mfxFrameData& m_fd;
};

class MultiFrameLocker
{
public:
    MultiFrameLocker(VideoCORE& core, mfxFrameSurface& surface);
    ~MultiFrameLocker();
    mfxStatus Lock(mfxU32 label);
    mfxStatus Unlock(mfxU32 label);

private:
    MultiFrameLocker(const MultiFrameLocker&);
    MultiFrameLocker& operator=(const MultiFrameLocker&);

    enum { MAX_NUM_FRAME_DATA = 256 };
    VideoCORE& m_core;
    mfxFrameSurface& m_surface;
    mfxU8 m_locked[MAX_NUM_FRAME_DATA];
};

#endif
#endif



