// Copyright (c) 2002-2019 Intel Corporation
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

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)

#ifndef __MFX_MPEG2_ENCODE_DEBUG_HW_H
#define __MFX_MPEG2_ENCODE_DEBUG_HW_H

//modify here
#define MP2_DBG_MANUAL_SETUP  0

#define MP2_DBG_DEBUG_LOG_FOLDER "C:\\tv\\enc_data1\\tmp\\enc_hw_log"
#define MP2_DBG_EXTENDED_MODE 1//add encoded DCT values
#define MP2_DBG_ADD_REF_DATA  1//add referece data for Inter MBs

#define MP2_DBG_ADD_QUANT     1
#define MP2_DBG_ADD_MB_TYPE   1
#define MP2_DBG_ADD_MB_MVINFO 1
#define MP2_DBG_FRAME_MIN     0 //frame position in stream (not display order)
#define MP2_DBG_FRAME_MAX     1000

class MFXVideoENCODEMPEG2_HW;

typedef struct 
{
  int ref_vfield_select[3][2];//fld1|fld2|frame, fwd|bwd 

  uint8_t YBlockRec[2][2][128];//fwd|bwd, fld1|fld2, fld data
  uint8_t UBlockRec[2][2][128];
  uint8_t VBlockRec[2][2][128];

  int16_t encMbData[12][64];//reconstructed blocks
  int32_t Count[12];//coded_block_pattern
  int32_t no_motion_flag;
  int32_t quant_skip_flag;
  int32_t quantizer;
  int32_t skipped;

} MbDebugInfo;


class MPEG2EncodeDebug_HW
{

  typedef struct _MBInfo  // macroblock information
  {
    mfxI32 mb_type;               // intra/forward/backward/interpolated
    mfxI32 dct_type;              // field/frame DCT
    mfxI32 prediction_type;       // MC_FRAME/MC_FIELD
    IppiPoint MV[2][2];           // motion vectors [vecnum][F/B]
    IppiPoint MV_P[2];            // motion vectors from P frame [vecnum]
    mfxI32 mv_field_sel[3][2];    // motion vertical field select:
    // the first index: 0-top field, 1-bottom field, 2-frame;
    // the second index:0-forward, 1-backward
    mfxI32 skipped;

  } MBInfo;


  typedef struct _MPEG2FrameState
  {
    // Input frame
    mfxU8       *Y_src;
    mfxU8       *U_src;
    mfxU8       *V_src;
    // Input reference reconstructed frames
    uint8_t       *YRecFrame[2][2];   // [top/bottom][fwd/bwd]
    uint8_t       *URecFrame[2][2];
    uint8_t       *VRecFrame[2][2];
    // Output reconstructed frame
    mfxU8       *Y_out;
    mfxU8       *U_out;
    mfxU8       *V_out;
    mfxI32      YFrameHSize;
    mfxI32      UVFrameHSize;
    mfxU32      needFrameUnlock[4]; // 0-src, 1,2-Rec, 3-out

  } MPEG2FrameState;


private:
//additional MBinfo (debug data for each macroblock)
  MFXVideoENCODEMPEG2_HW *encode_hw;
  MbDebugInfo *mb_debug_info;

//debug parameters
  char *debug_logs_folder;
  int  use_extended_log;
  int  use_reference_log;
  int  use_manual_setup;
  int  use_mb_type;
  int  use_mb_MVinfo;
  int  use_quantizer_info;
  int  minFrameIndex;
  int  maxFrameIndex;
  int  mb_num;

public:
  MPEG2EncodeDebug_HW()
  {
    debug_logs_folder = NULL;
    mb_debug_info = NULL;
  }


  ~MPEG2EncodeDebug_HW()
  {
    Free();
  }


  void CreateDEBUGframeLog();
  void Init(MFXVideoENCODEMPEG2_HW *pEncode_hw);
  void Free();

  void GatherInterRefMBlocksData(int k,void *vector_in,void* state,void *mbinfo);
  void GatherBlockData(int k,int blk,int picture_coding_type,int quantiser_scale_value,int16_t *quantMatrix,int16_t *pMBlock,int Count,int intra_flag,int intra_dc_shift);
  void SetSkippedMb(int k);
  void SetNoMVMb(int k);
  void SetMotionFlag(int k,int value);
};

#endif //__MFX_MPEG2_ENCODE_DEBUG_HW_H
#endif //MFX_ENABLE_MPEG2_VIDEO_PAK
/* EOF */
