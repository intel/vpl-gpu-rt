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

#include "mfx_mpeg2_encode_hw.h"
#if defined (MPEG2_ENCODE_DEBUG_HW)

#include "mfx_mpeg2_enc_defs_hw.h"
#include "ippi.h"
#include <direct.h>

enum MPEG2FrameType
{
  MPEG2_I_PICTURE = 1,
  MPEG2_P_PICTURE = 2,
  MPEG2_B_PICTURE = 3
};

typedef struct _IppMotionVector2
{
  int32_t x;
  int32_t y;
  int32_t mctype_l;
  int32_t offset_l;
  int32_t mctype_c;
  int32_t offset_c;
} IppMotionVector2;

int ImgCopy(uint8_t* pSrc, int srcStep, uint8_t* pDst, int dstStep,mfxSize roiSize, int cc_flag)
{
  int x,y;
  for(y = 0; y < roiSize.height; y++)
  {
    for(x = 0; x < roiSize.width; x++)
    {
      pDst[x] = pSrc[cc_flag*x];//cc_flag is used for conversion from NV12 to U&V planes
    }
    pSrc+=srcStep;
    pDst+=dstStep;
  }

  return 0;
}

#define COL_Y_STEP_Y 1
#define COL_U_STEP_UV 1
#define COL_V_STEP_UV 1
#define COL_U_STEP_Y 2
#define COL_V_STEP_Y 2

#define COPY_FRAME_BLOCK(X, CC, C, DIR) \
    ImgCopy( \
      state->X##RecFrame[mbinfo->mv_field_sel[2][DIR]][DIR] + vector[2][DIR].offset_##C, \
      state->CC##FrameHSize, \
      mb_debug_info[k].X##BlockRec[DIR][0], \
      roi_##C.width, \
      roi_##C, \
      COL_##X##_STEP_##CC)

#define COPY_FIELD_BLOCKS(X, CC, C, DIR) \
  if(!pPAK->m_cuc->FrameParam->MPEG2.FieldPicFlag) \
  { \
    ImgCopy( \
      state->X##RecFrame[mbinfo->mv_field_sel[0][DIR]][DIR] + vector[0][DIR].offset_##C, \
      2*(state->CC##FrameHSize), \
      mb_debug_info[k].X##BlockRec[DIR][0], \
      roi_##C.width, \
      roi_##C, \
      COL_##X##_STEP_##CC); \
    ImgCopy( \
      state->X##RecFrame[mbinfo->mv_field_sel[1][DIR]][DIR] + vector[1][DIR].offset_##C, \
      2*(state->CC##FrameHSize), \
      mb_debug_info[k].X##BlockRec[DIR][1], \
      roi_##C.width, \
      roi_##C, \
      COL_##X##_STEP_##CC); \
  }else{ \
    ImgCopy( \
      state->X##RecFrame[mbinfo->mv_field_sel[0][DIR]][DIR] + vector[0][DIR].offset_##C, \
      state->CC##FrameHSize, \
      mb_debug_info[k].X##BlockRec[DIR][0], \
      roi_##C.width, \
      roi_##C, \
      COL_##X##_STEP_##CC); \
    ImgCopy( \
      state->X##RecFrame[mbinfo->mv_field_sel[1][DIR]][DIR] + vector[1][DIR].offset_##C + roi_##C.height*(state->CC##FrameHSize), \
      state->CC##FrameHSize, \
      mb_debug_info[k].X##BlockRec[DIR][1], \
      roi_##C.width, \
      roi_##C, \
      COL_##X##_STEP_##CC); \
  }
#define COPY_FIELD_BLOCKS_DP(X, CC, C) \
  if(!pPAK->m_cuc->FrameParam->MPEG2.FieldPicFlag) \
  { \
    ImgCopy( \
      state->X##RecFrame[0][0] + vector[0][0].offset_##C, \
      2*(state->CC##FrameHSize), \
      mb_debug_info[k].X##BlockRec[0][0], \
      roi_##C.width, \
      roi_##C, \
      COL_##X##_STEP_##CC); \
    ImgCopy( \
      state->X##RecFrame[1][0] + vector[0][0].offset_##C, \
      2*(state->CC##FrameHSize), \
      mb_debug_info[k].X##BlockRec[0][1], \
      roi_##C.width, \
      roi_##C, \
      COL_##X##_STEP_##CC); \
  }else{ \
    ImgCopy( \
      state->X##RecFrame[pPAK->m_cuc->FrameParam->MPEG2.BottomFieldFlag][0] + vector[0][0].offset_##C, \
      state->CC##FrameHSize, \
      mb_debug_info[k].X##BlockRec[0][0], \
      roi_##C.width, \
      roi_##C, \
      COL_##X##_STEP_##CC); \
  }

void MPEG2EncodeDebug_HW::GatherInterRefMBlocksData(int k,void *vector_in,void *pState,void *pMbinfo)
{
  int numEncodedFrames = encode_hw->m_OutputFrameOrder + 1;

  if(use_reference_log != 1 || numEncodedFrames < minFrameIndex || numEncodedFrames > maxFrameIndex)
    return;

  int32_t    curr_field = encode_hw->m_pPAK->m_cuc->FrameParam->MPEG2.SecondFieldFlag;
  mfxI32      MBcountV = encode_hw->m_pPAK->m_cuc->FrameParam->MPEG2.FrameHinMbMinus1+1;
  mfxI32      MBcountH = encode_hw->m_pPAK->m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1;
  mfxI32       mbstart = (curr_field == 0)?0:(MBcountV*MBcountH / 2);// offset to switch between top and bottom

  k += mbstart;

  MPEG2FrameState *state = (MPEG2FrameState *)pState;
  MBInfo *mbinfo =(MBInfo *) pMbinfo;

  MFXVideoPAKMPEG2 *pPAK = encode_hw->m_pPAK;
  IppMotionVector2 vector[3][2];


  MFX_INTERNAL_CPY(mb_debug_info[k].ref_vfield_select,mbinfo->mv_field_sel,sizeof(int[3][2]));

  MFX_INTERNAL_CPY(vector,vector_in,sizeof(IppMotionVector2[3][2]));
  mfxSize roi_l = {16,16},
    roi_c = {pPAK->BlkWidth_c,pPAK->BlkHeight_c};

  if(mbinfo->prediction_type == MC_FRAME)
  {
    if(mbinfo->mb_type & MB_FORWARD)
    {
      COPY_FRAME_BLOCK(Y,Y,l,0);
      if(pPAK->m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
        COPY_FRAME_BLOCK(U,Y,c,0);
        COPY_FRAME_BLOCK(V,Y,c,0);
      } else {
        COPY_FRAME_BLOCK(U,UV,c,0);
        COPY_FRAME_BLOCK(V,UV,c,0);
      }
    }
    if(mbinfo->mb_type & MB_BACKWARD)
    {
      COPY_FRAME_BLOCK(Y,Y,l,1);
      if(pPAK->m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
        COPY_FRAME_BLOCK(U,Y,c,1);
        COPY_FRAME_BLOCK(V,Y,c,1);
      } else {
        COPY_FRAME_BLOCK(U,UV,c,1);
        COPY_FRAME_BLOCK(V,UV,c,1);
      }
    }
  }else if(mbinfo->prediction_type == MC_FIELD)
  {
    roi_l.height /= 2;
    roi_c.height /= 2;

    if(mbinfo->mb_type & MB_FORWARD)
    {
      COPY_FIELD_BLOCKS(Y,Y,l,0);
      if(pPAK->m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
        COPY_FIELD_BLOCKS(U,Y,c,0);
        COPY_FIELD_BLOCKS(V,Y,c,0);
      } else {
        COPY_FIELD_BLOCKS(U,UV,c,0);
        COPY_FIELD_BLOCKS(V,UV,c,0);
      }
    }
    if(mbinfo->mb_type & MB_BACKWARD)
    {
      COPY_FIELD_BLOCKS(Y,Y,l,1);
      if(pPAK->m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
        COPY_FIELD_BLOCKS(U,Y,c,1);
        COPY_FIELD_BLOCKS(V,Y,c,1);
      } else {
        COPY_FIELD_BLOCKS(U,UV,c,1);
        COPY_FIELD_BLOCKS(V,UV,c,1);
      }
    }
  }else if(mbinfo->prediction_type == MC_DMV)
  {
    if(!pPAK->m_cuc->FrameParam->MPEG2.FieldPicFlag)
    {
      roi_l.height /= 2;
      roi_c.height /= 2;
    }
    COPY_FIELD_BLOCKS_DP(Y,Y,l);
    if(pPAK->m_info.FrameInfo.FourCC == MFX_FOURCC_NV12) {
      COPY_FIELD_BLOCKS_DP(U,Y,c);
      COPY_FIELD_BLOCKS_DP(V,Y,c);
    } else {
      COPY_FIELD_BLOCKS_DP(U,UV,c);
      COPY_FIELD_BLOCKS_DP(V,UV,c);
    }
  }
}

void MPEG2EncodeDebug_HW::SetNoMVMb(int k)
{
  int numEncodedFrames = encode_hw->m_OutputFrameOrder + 1;

  if(numEncodedFrames < minFrameIndex || numEncodedFrames > maxFrameIndex)
    return;

  int32_t    curr_field = encode_hw->m_pPAK->m_cuc->FrameParam->MPEG2.SecondFieldFlag;
  mfxI32      MBcountV = encode_hw->m_pPAK->m_cuc->FrameParam->MPEG2.FrameHinMbMinus1+1;
  mfxI32      MBcountH = encode_hw->m_pPAK->m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1;
  mfxI32       mbstart = (curr_field == 0)?0:(MBcountV*MBcountH / 2);// offset to switch between top and bottom

  mb_debug_info[k + mbstart].no_motion_flag = 1;
}

void MPEG2EncodeDebug_HW::SetSkippedMb(int k)
{
  int numEncodedFrames = encode_hw->m_OutputFrameOrder + 1;

  if(numEncodedFrames < minFrameIndex || numEncodedFrames > maxFrameIndex)
    return;

  int32_t    curr_field = encode_hw->m_pPAK->m_cuc->FrameParam->MPEG2.SecondFieldFlag;
  mfxI32      MBcountV = encode_hw->m_pPAK->m_cuc->FrameParam->MPEG2.FrameHinMbMinus1+1;
  mfxI32      MBcountH = encode_hw->m_pPAK->m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1;
  mfxI32       mbstart = (curr_field == 0)?0:(MBcountV*MBcountH / 2);// offset to switch between top and bottom

  mb_debug_info[k + mbstart].skipped = 1;
}

void MPEG2EncodeDebug_HW::CreateDEBUGframeLog()
{
  int numEncodedFrames = encode_hw->m_OutputFrameOrder + 1;

  if(numEncodedFrames < minFrameIndex || numEncodedFrames > maxFrameIndex)
    return;

  MFXVideoENCMPEG2_HW *enc_hw = encode_hw->m_pENC;

  mfxU8     mFrameType = enc_hw->m_cuc->FrameParam->MPEG2.FrameType;
  int32_t    curr_field = enc_hw->m_cuc->FrameParam->MPEG2.SecondFieldFlag;
  mfxI32      MBcountV = enc_hw->m_cuc->FrameParam->MPEG2.FrameHinMbMinus1+1;
  mfxI32      MBcountH = enc_hw->m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1;
  mfxI32       mbstart = (curr_field == 0)?0:(MBcountV*MBcountH / 2);

  int i,j,k,is_skipped;
  char frameLogName[2048],mb_type_log[128],mv_log[128],quantizer_log[128],*fieldName;
  char frame_type = (mFrameType & MFX_FRAMETYPE_I)? 'I' : ((mFrameType & MFX_FRAMETYPE_P)? 'P':'B');
  fieldName = (!enc_hw->m_cuc->FrameParam->MPEG2.FieldPicFlag)? "" :((enc_hw->m_cuc->FrameParam->MPEG2.BottomFieldFlag)? "(bottom)":"(top)");
  sprintf(frameLogName,"%s\\%d_%c%s.log",debug_logs_folder,numEncodedFrames,frame_type,fieldName);
  FILE *frameLogFile = fopen(frameLogName,"w");

  if(frameLogFile == NULL)
  {
    printf("Couldn't write to %s file\n",frameLogName);
    return ;
  }
  for(k = mbstart, j=0; j < MBcountV*BlkWidth_l; j += 16)
  {
    for(i=0; i < MBcountH*BlkWidth_l; i += 16, k++)
    {
      is_skipped       = 0;
      mv_log[0]        = 0;
      quantizer_log[0] = 0;

      if(enc_hw->pMBInfo0[k].mb_type & MB_INTRA){
        strcpy(mb_type_log,"mb_type Intra;");
        sprintf(quantizer_log,"quant: %d;",mb_debug_info[k].quantizer);
      }
      else if(enc_hw->pMBInfo0[k].skipped == 1 ||  enc_hw->pMBInfo0[k].mb_type == 0 || mb_debug_info[k].skipped == 1){
        strcpy(mb_type_log,"mb_type Inter skip;");
        is_skipped = 1;
      }
      else if(mb_debug_info[k].no_motion_flag == 1 && enc_hw->pMBInfo0[k].prediction_type == MC_FRAME && (mFrameType & MFX_FRAMETYPE_P)){
        strcpy(mb_type_log,"mb_type Inter (MC_ZERO);");
        sprintf(quantizer_log,"quant: %d;",mb_debug_info[k].quantizer);
      }
      else if((enc_hw->pMBInfo0[k].mb_type & (MB_FORWARD | MB_BACKWARD)) == (MB_FORWARD | MB_BACKWARD)){
        strcpy(mb_type_log,"mb_type Inter F/B");
        sprintf(quantizer_log,"quant: %d;",mb_debug_info[k].quantizer);
        sprintf(mv_log,"MV_F x:%+2d y:%+2d; MV_B x:%+2d y:%+2d",enc_hw->pMBInfo0[k].MV[0][0].x,
                                                                enc_hw->pMBInfo0[k].MV[0][0].y,
                                                                enc_hw->pMBInfo0[k].MV[0][1].x,
                                                                enc_hw->pMBInfo0[k].MV[0][1].y);
      }
      else if(enc_hw->pMBInfo0[k].mb_type & MB_FORWARD){
        strcpy(mb_type_log,"mb_type Inter F");
        sprintf(quantizer_log,"quant: %d;",mb_debug_info[k].quantizer);

        if(enc_hw->pMBInfo0[k].prediction_type == MC_DMV)
        {
          //sprintf(mv_log,"MV_F x:%+2d y:%+2d, DMV x:%+2d y:%+2d",enc_hw->pMBInfo0[k].dpMV[curr_field][0].x,
          //                                                               enc_hw->pMBInfo0[k].dpMV[curr_field][0].y << (enc_hw->picture_structure == FRAME_PICTURE),
          //                                                               enc_hw->pMBInfo0[k].dpDMV[curr_field].x,
          //                                                               enc_hw->pMBInfo0[k].dpDMV[curr_field].y);
          sprintf(mv_log,"Dual prime not supported\n");
        }
        else
          sprintf(mv_log,"MV_F x:%+2d y:%+2d",enc_hw->pMBInfo0[k].MV[0][0].x,
                                              enc_hw->pMBInfo0[k].MV[0][0].y);
      }
      else //if(enc_hw->pMBInfo0[k].mb_type & MB_BACKWARD)
      {
        strcpy(mb_type_log,"mb_type Inter B");
        sprintf(quantizer_log,"quant: %d;",mb_debug_info[k].quantizer);

        sprintf(mv_log,"MV_B x:%+2d y:%+2d",enc_hw->pMBInfo0[k].MV[0][1].x,
                                            enc_hw->pMBInfo0[k].MV[0][1].y);
      }

      if(mv_log[0] != 0)
      {
        if(enc_hw->pMBInfo0[k].prediction_type == MC_FIELD)
        {
          char tmp_buf[128];

          if((enc_hw->pMBInfo0[k].mb_type & (MB_FORWARD | MB_BACKWARD)) == (MB_FORWARD | MB_BACKWARD))
            sprintf(tmp_buf," (field1); MV_F x:%+2d y:%+2d; MV_B x:%+2d y:%+2d (field2)",enc_hw->pMBInfo0[k].MV[1][0].x,enc_hw->pMBInfo0[k].MV[1][0].y,enc_hw->pMBInfo0[k].MV[1][1].x,enc_hw->pMBInfo0[k].MV[1][1].y);
          else if(enc_hw->pMBInfo0[k].mb_type & MB_FORWARD)
            sprintf(tmp_buf," (field1); MV_F x:%+2d y:%+2d (field2)",enc_hw->pMBInfo0[k].MV[1][0].x,enc_hw->pMBInfo0[k].MV[1][0].y);
          else //if(enc_hw->pMBInfo0[k].mb_type & MB_BACKWARD)
            sprintf(tmp_buf," (field1); MV_B x:%+2d y:%+2d (field2)",enc_hw->pMBInfo0[k].MV[1][1].x,enc_hw->pMBInfo0[k].MV[1][1].y);

          strcat(mb_type_log," (MC_FIELD);");
          strcat(mv_log,tmp_buf);
        }else if(enc_hw->pMBInfo0[k].prediction_type == MC_FRAME)
        {
          strcat(mb_type_log," (MC_FRAME);");
        }
        else
        {
          strcat(mb_type_log," (MC_DMV);");
        }
      }
      if(!use_mb_type)
        mb_type_log[0] = 0;

      if(!use_mb_MVinfo)
        mv_log[0] = 0;

      if(!use_quantizer_info)
        quantizer_log[0] = 0;

      int mb_y = (!enc_hw->m_cuc->FrameParam->MPEG2.FieldPicFlag)? j/16 :((enc_hw->m_cuc->FrameParam->MPEG2.BottomFieldFlag)? j/16 + MBcountV : j/16);
      fprintf(frameLogFile,"MB(%dx%d) %s %s %s\n",i/16,mb_y,mb_type_log,quantizer_log,mv_log);

      if(use_extended_log == 1 && !is_skipped)
      {
        fprintf(frameLogFile,"MB data:");
        for (int blk = 0; blk < enc_hw->m_block_count; blk++) {
          if(mb_debug_info[k].Count[blk])
          {
            fprintf(frameLogFile,"\n            blk %d:",blk);
            for(int i = 0; i < 64; i++)
              fprintf(frameLogFile," %d",mb_debug_info[k].encMbData[blk][i]);
          }else if(enc_hw->pMBInfo0[k].mb_type & MB_INTRA)
          {
            fprintf(frameLogFile,"\n            blk %d: %d",blk,mb_debug_info[k].encMbData[blk][0]);
          }else
          {
            fprintf(frameLogFile,"\n            blk %d: ---",blk);
          }
        }
        fprintf(frameLogFile,"\n");
      }

      if(use_reference_log == 1 && !(mFrameType & MFX_FRAMETYPE_I) && !is_skipped)
      {
        int ref_pos[2][2][2];//fld1|fld2, fwd|bwd, x|y
        int print_size[2][2][2],size_l,size_c;//l|c,fwd|bwd,fld1|fld2
        int fld_offset;

        for(int dir_i = 0; dir_i < 2; dir_i++)
        {
          for(int fld_i = 0; fld_i < 2; fld_i++)
          {
            if(enc_hw->pMBInfo0[k].prediction_type == MC_FRAME)
              mb_debug_info[k].ref_vfield_select[fld_i][dir_i] = mb_debug_info[k].ref_vfield_select[2][dir_i];

            fld_offset = (enc_hw->m_cuc->FrameParam->MPEG2.FieldPicFlag && mb_debug_info[k].ref_vfield_select[fld_i][dir_i] != 0)? MBcountV*BlkWidth_l : 0;

            ref_pos[fld_i][dir_i][0] = ((enc_hw->pMBInfo0[k].MV[fld_i][dir_i].x>>1) + i);
            ref_pos[fld_i][dir_i][1] = ((enc_hw->pMBInfo0[k].MV[fld_i][dir_i].y>>1) + j + fld_offset);
          }
        }

        size_l = BlkHeight_l * BlkWidth_l;
        size_c = enc_hw->BlkHeight_c * enc_hw->BlkWidth_c;
        if(enc_hw->pMBInfo0[k].prediction_type == MC_FIELD || (enc_hw->pMBInfo0[k].prediction_type == MC_DMV && (!enc_hw->m_cuc->FrameParam->MPEG2.FieldPicFlag)))
        {
          size_l /= 2;
          size_c /= 2;
        }

        memset(print_size,0,sizeof(print_size));

        if(enc_hw->pMBInfo0[k].mb_type & MB_FORWARD)
        {
          print_size[0][0][0] = size_l;
          print_size[1][0][0] = size_c;
          if(enc_hw->pMBInfo0[k].prediction_type == MC_FIELD || (enc_hw->pMBInfo0[k].prediction_type == MC_DMV && (!enc_hw->m_cuc->FrameParam->MPEG2.FieldPicFlag)))
          {
            print_size[0][0][1] = size_l;
            print_size[1][0][1] = size_c;
          }
        }
        if(enc_hw->pMBInfo0[k].mb_type & MB_BACKWARD)
        {
          print_size[0][1][0] = size_l;
          print_size[1][1][0] = size_c;
          if(enc_hw->pMBInfo0[k].prediction_type == MC_FIELD || (enc_hw->pMBInfo0[k].prediction_type == MC_DMV && (!enc_hw->m_cuc->FrameParam->MPEG2.FieldPicFlag)))
          {
            print_size[0][1][1] = size_l;
            print_size[1][1][1] = size_c;
          }
        }

        for(int dir_i = 0; dir_i < 2; dir_i++)
        {
          for(int fld_i = 0; fld_i < 2 && print_size[0][dir_i][fld_i] > 0; fld_i++)
          {

            fprintf(frameLogFile,"\n            reference MB(%dx%d)",ref_pos[fld_i][dir_i][0],ref_pos[fld_i][dir_i][1]);

            if(dir_i == 0)
              fprintf(frameLogFile,"  FWD");
            else
              fprintf(frameLogFile,"  BWD");

            if(enc_hw->pMBInfo0[k].prediction_type == MC_FIELD || enc_hw->m_cuc->FrameParam->MPEG2.FieldPicFlag)
              fprintf(frameLogFile,"  %s (field %d)",mb_debug_info[k].ref_vfield_select[fld_i][dir_i]?"BOTTOM":"TOP",fld_i+1);
            fprintf(frameLogFile,"\n");

            if(1)
            {
              fprintf(frameLogFile,"\n            Y:");
              for(int i = 0; i < print_size[0][dir_i][fld_i]; i++)fprintf(frameLogFile," %d",mb_debug_info[k].YBlockRec[dir_i][fld_i][i]);

              fprintf(frameLogFile,"\n            U:");
              for(int i = 0; i < print_size[1][dir_i][fld_i]; i++)fprintf(frameLogFile," %d",mb_debug_info[k].UBlockRec[dir_i][fld_i][i]);

              fprintf(frameLogFile,"\n            V:");
              for(int i = 0; i < print_size[1][dir_i][fld_i]; i++)fprintf(frameLogFile," %d",mb_debug_info[k].VBlockRec[dir_i][fld_i][i]);
            }
          }
        }
        fprintf(frameLogFile,"\n");
      }

      fprintf(frameLogFile,"\n");

      memset(&mb_debug_info[k],0,sizeof(MbDebugInfo));
    }
  }

  if(frameLogFile)fclose(frameLogFile);
}


void MPEG2EncodeDebug_HW::GatherBlockData(int k,int blk,int picture_coding_type,int quantiser_scale_value,int16_t *quantMatrix,int16_t *pBlock,int Count,int intra_flag,int intra_dc_shift)
{
  int numEncodedFrames = encode_hw->m_OutputFrameOrder + 1;

  if(use_extended_log == 0 || numEncodedFrames < minFrameIndex || numEncodedFrames > maxFrameIndex)
    return;

  int32_t    curr_field = encode_hw->m_pPAK->m_cuc->FrameParam->MPEG2.SecondFieldFlag;
  mfxI32      MBcountV = encode_hw->m_pPAK->m_cuc->FrameParam->MPEG2.FrameHinMbMinus1+1;
  mfxI32      MBcountH = encode_hw->m_pPAK->m_cuc->FrameParam->MPEG2.FrameWinMbMinus1+1;
  mfxI32       mbstart = (curr_field == 0)?0:(MBcountV*MBcountH / 2);// offset to switch between top and bottom

  k += mbstart;

  mb_debug_info[k].quantizer = quantiser_scale_value;

  if(use_extended_log == 1)
  {
    mb_debug_info[k].Count[blk] = Count;
    if(Count)
    {
      //debug for non reference frames should prepare data by itself
      if(picture_coding_type == MPEG2_B_PICTURE)
      {
        if(intra_flag) 
        {
          pBlock[0] <<= intra_dc_shift;
          ippiQuantInvIntra_MPEG2_16s_C1I(pBlock, quantiser_scale_value,quantMatrix);
        } else {
          ippiQuantInv_MPEG2_16s_C1I(pBlock, quantiser_scale_value, quantMatrix);
        }
      }
      MFX_INTERNAL_CPY(mb_debug_info[k].encMbData[blk],pBlock,8*8*sizeof(int16_t));
    }
    else if(intra_flag)
    {
      mb_debug_info[k].encMbData[blk][0] = pBlock[0];

      if(picture_coding_type == MPEG2_B_PICTURE)
        mb_debug_info[k].encMbData[blk][0] <<= intra_dc_shift;
    }
  }
}

void MPEG2EncodeDebug_HW::Init(MFXVideoENCODEMPEG2_HW *pEncode_hw)
{
  encode_hw          = pEncode_hw;
  use_manual_setup   = MP2_DBG_MANUAL_SETUP;
  use_reference_log  = MP2_DBG_ADD_REF_DATA;

  use_mb_type        = MP2_DBG_ADD_MB_TYPE;
  use_mb_MVinfo      = MP2_DBG_ADD_MB_MVINFO;
  use_quantizer_info = MP2_DBG_ADD_QUANT;

  minFrameIndex      = MP2_DBG_FRAME_MIN;
  maxFrameIndex      = MP2_DBG_FRAME_MAX;

  debug_logs_folder = (char*)malloc(1024);
  if(use_manual_setup)
  {
    printf("Debug mode.\nEnter folder to put logs in it:");
    scanf("%s",debug_logs_folder);

    printf("\nEnter mode (1 - with MB data, 0 - without):");
    scanf("%d",&use_extended_log);
  }
  else
  {
    use_extended_log  = MP2_DBG_EXTENDED_MODE;
    strcpy(debug_logs_folder,MP2_DBG_DEBUG_LOG_FOLDER);
  }

  system("rd /S /Q " MP2_DBG_DEBUG_LOG_FOLDER "");
  _mkdir(debug_logs_folder);

//allocating memory
  mb_num = (encode_hw->m_pENC->m_info.FrameInfo.Height * encode_hw->m_pENC->m_info.FrameInfo.Width/256);
  //if(encode_hw->m_pFrameCUC->FrameParam->MPEG2.FieldPicFlag) mb_num /=2;
  mb_debug_info = (MbDebugInfo*)malloc(mb_num*sizeof(MbDebugInfo));

  memset(mb_debug_info,0,mb_num*sizeof(MbDebugInfo));
}

void MPEG2EncodeDebug_HW::Free()
{
  if(debug_logs_folder != NULL)
  {
    free(debug_logs_folder);
  }
  if(mb_debug_info != NULL)
  {
    free(mb_debug_info);
  }
}
#endif // MPEG2_ENCODE_DEBUG_HW && __SW_ENC
