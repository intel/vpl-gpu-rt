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

#ifndef __AV1SCD_H
#define __AV1SCD_H

#include "aenc.h"
#include <stdint.h>

#if defined(MFX_ENABLE_ADAPTIVE_ENCODE)

#include <list>
#include <map>
#include <functional>
#include <set>

#include <immintrin.h>

#define ASC_SMALL_WIDTH         128
#define ASC_SMALL_HEIGHT        64
#define MAX_BSTRUCTRUE_GOP_SIZE 16
#define ASC_MAP_SIZE            ((ASC_SMALL_WIDTH * ASC_SMALL_HEIGHT) >> 6)

namespace aenc {

    enum  ASC_LTR_DEC {
        NO_LTR = false,
        YES_LTR = true,
        FORCE_LTR
    };
    enum ASCSimil {
        Not_same,
        Same
    };
    enum ASCLayers {
        ASCFull_Size,
        ASCSmall_Size
    };
    enum ASCDFD {
        ASCReference_Frame,
        ASCCurrent_Frame,
        ASCScene_Diff_Frame
    };
    enum ASCGOP {
        Forbidden_GoP,
        Immediate_GoP,
        QuarterHEVC_GoP,
        Three_GoP,
        HalfHEVC_GoP,
        Five_GoP,
        Six_GoP,
        Seven_Gop,
        HEVC_Gop,
        Nine_Gop,
        Ten_Gop,
        Eleven_Gop,
        Twelve_Gop,
        Thirteen_Gop,
        Fourteen_Gop,
        Fifteen_Gop,
        Double_HEVC_Gop
    };
    enum ASCBP {
        ASCcurrent_frame_data,
        ASCprevious_frame_data,
        ASCprevious_previous_frame_data,
        ASCSceneVariation_frame_data
    };
    enum ASCFTS {
        ASC_UNKNOWN,
        ASCprogressive_frame,
        ASCtopfieldfirst_frame,
        ASCbotfieldFirst_frame
    };
    enum ASCFF {
        ASCTopField,
        ASCBottomField
    };

    typedef struct ASCcoordinates {
        mfxI16
            x;
        mfxI16
            y;
    }ASCMVector;

    typedef struct ASCBaseline {
        mfxI32
            start;
        mfxI32
            end;
    }ASCLine;

    typedef struct ASCYUV_layer_store {
        mfxU8
            data[ASC_SMALL_WIDTH * ASC_SMALL_HEIGHT],
            *Y,
            *U,
            *V;
        mfxU32
            width,
            height,
            pitch,
            hBorder,
            wBorder,
            extWidth,
            extHeight;
    }ASCYUV;

    typedef struct ASCSAD_range {
        mfxU16
            SAD,
            distance;
        ASCMVector
            BestMV;
    }ASCRsad;

    typedef struct ASCImage_details {
        mfxI32
            Original_Width,             //User input
            Original_Height,            //User input
            horizontal_pad,             //User input for original resolution in multiples of FRAMEMUL, derived for the other two layers
            vertical_pad,               //User input for original resolution in multiples of FRAMEMUL, derived for the other two layers
            _cwidth,                    //corrected size if width not multiple of FRAMEMUL
            _cheight,                   //corrected size if height not multiple of FRAMEMUL
            pitch,                      //horizontal_pad + _cwidth + horizontal_pad
            Extended_Width,             //horizontal_pad + _cwidth + horizontal_pad
            Extended_Height,            //vertical_pad + _cheight + vertical_pad
            block_width,                //User input
            block_height,               //User input
            Width_in_blocks,            //_cwidth / block_width
            Height_in_blocks,           //_cheight / block_height
            initial_point,              //(Extended_Width * vertical_pad) + horizontal_pad
            sidesize,                   //_cheight + (1 * vertical_pad)
            MVspaceSize;                //Pixels_in_Y_layer / block_width / block_height
    }ASCImDetails;

    typedef struct ASCVideo_characteristics {
        ASCImDetails
            *layer;
        mfxI32
            starting_frame,              //Frame number where the video is going to be accessed
            total_number_of_frames,      //Total number of frames in video file
            processed_frames,            //Number of frames that are going to be processed, taking into account the starting frame
            accuracy,
            key_frame_frequency,
            limitRange,
            maxXrange,
            maxYrange,
            interlaceMode,
            StartingField,
            currentField;
       /* ASCTime
            timer;*/
    }ASCVidData;

    class ASCTSCstat {
    public:
        ASCTSCstat();
        mfxI64
            ssDCint,
            refDCint;
        mfxU64
            m_distance;
        mfxI32
            frameNum,
            scVal,
            tscVal,
            pdist,
            histogram[5],
            Schg,
            last_shot_distance;
        mfxI32
            ssDCval,
            refDCval,
            diffAFD,
            diffTSC,
            diffRsCsDiff,
            diffMVdiffVal;
        mfxI32
            RecentHighMvCount;
        mfxU32
            TSC0,
            RecentHighMvMap[8];
        mfxU32
            SCindex,
            TSCindex,
            Rs,
            Cs,
            Contrast,
            SC,
            TSC,
            RsDiff,
            CsDiff,
            RsCsDiff,
            MVdiffVal,
            AbsMVSize,
            AbsMVHSize,
            AbsMVVSize;
        mfxU32
            gchDC,
            posBalance,
            negBalance,
            avgVal;
        mfxU32
            mu_mv_mag_sq;
        mfxF32
            MV0,
            m_ltrMCFD;
        mfxI16
            tcor,
            mcTcor;
        mfxU16
            AFD,
            gop_size;
        mfxU8
            picType,
            lastFrameInShot;
        bool
            Gchg,
            repeatedFrame,
            firstFrame,
            copyFrameDelay,
            fadeIn,
            ltr_flag;
    };

    typedef void(*t_GainOffset)(mfxU8** pSrc, mfxU8** pDst, mfxU16 width, mfxU16 height, mfxU16 pitch, mfxI16 gainDiff);
    typedef void(*t_RsCsCalc)(mfxU8* pSrc, int srcPitch, int wblocks, int hblocks, mfxU16* pRs, mfxU16* pCs);
    typedef void(*t_RsCsCalc_bound)(mfxU16* pRs, mfxU16* pCs, mfxU16* pRsCs, mfxU32* pRsFrame, mfxU32* pCsFrame, mfxU32* pContrast, int wblocks, int hblocks);
    typedef void(*t_RsCsCalc_diff)(mfxU16* pRs0, mfxU16* pCs0, mfxU16* pRs1, mfxU16* pCs1, int wblocks, int hblocks, mfxU32* pRsDiff, mfxU32* pCsDiff);
    typedef void(*t_ImageDiffHistogram)(mfxU8* pSrc, mfxU8* pRef, mfxU32 pitch, mfxU32 width, mfxU32 height, mfxI32 histogram[5], mfxI64 *pSrcDC, mfxI64 *pRefDC);
    typedef mfxI16(*t_AvgLumaCalc)(mfxU32* pAvgLineVal, int len);
    typedef void(*t_ME_SAD_8x8_Block_Search)(mfxU8 *pSrc, mfxU8 *pRef, int pitch, int xrange, int yrange, mfxU16 *bestSAD, int *bestX, int *bestY);
    typedef void(*t_ME_SAD_8x8_Block_FSearch)(mfxU8 *pSrc, mfxU8 *pRef, int pitch, int xrange, int yrange, mfxU32 *bestSAD, int *bestX, int *bestY);
    typedef mfxStatus(*t_Calc_RaCa_pic)(mfxU8 *pPicY, mfxI32 width, mfxI32 height, mfxI32 pitch, mfxF64 &RsCs);
    typedef mfxU16(*t_Agop_selection)(mfxI32 diffMVdiffVal, mfxU32 RsCsDiff, mfxU32 MVDiff, mfxU32 Rs, mfxU32 AFD,
        mfxU32 CsDiff, mfxI32 diffTSC, mfxU32 TSC, mfxU32 gchDC, mfxI32 diffRsCsdiff,
        mfxU32 posBalance, mfxU32 SC, mfxU32 TSCindex, mfxU32 Scindex, mfxU32 Cs,
        mfxI32 diffAFD, mfxU32 negBalance, mfxU32 ssDCval, mfxU32 refDCval, mfxU32 RsDiff,
        mfxU32 mu_mv_mag_sq, mfxI16 mcTcor, mfxI16 tcor);

    class ASCimageData {
    public:
        ASCimageData();
        ~ASCimageData();
        //Copy assignment constructor
        ASCimageData & operator=(const ASCimageData & iData);
        ASCYUV Image;
        ASCMVector
            pInteger[(ASC_SMALL_WIDTH * ASC_SMALL_HEIGHT) >> 6];
        mfxI32
            var,
            jtvar,
            mcjtvar;
        mfxU32
            Contrast,
            CsVal,
            RsVal;
        mfxI16
            tcor,
            mcTcor;
        mfxI16
            avgval;
        mfxU16
            Cs[(ASC_SMALL_WIDTH * ASC_SMALL_HEIGHT) >> 4],
            Rs[(ASC_SMALL_WIDTH * ASC_SMALL_HEIGHT) >> 4],
            Cs1[(ASC_SMALL_WIDTH * ASC_SMALL_HEIGHT) >> 4],
            Rs1[(ASC_SMALL_WIDTH * ASC_SMALL_HEIGHT) >> 4],
            RsCs[(ASC_SMALL_WIDTH * ASC_SMALL_HEIGHT) >> 4],
            SAD[(ASC_SMALL_WIDTH * ASC_SMALL_HEIGHT) >> 6];
        mfxU8 
            PAQ[(ASC_SMALL_WIDTH * ASC_SMALL_HEIGHT) >> 6];
        mfxStatus
            InitFrame();
    };

    typedef struct ASCvideoBuffer {
        ASCimageData
            layer;
        mfxI32
            frame_number,
            forward_reference,
            backward_reference;
        mfxU32 is_ltr_frame;
    } ASCVidSample;

    typedef struct ASCextended_storage {
        mfxI32
            average;
        mfxI32
            avgSAD;
        mfxU32
            gopSize,
            lastSCdetectionDistance,
            detectedSch,
            pendingSch;
        ASCTSCstat
            **logic;
        // For Pdistance table selection
        mfxI8*
            PDistanceTable;
        ASCLayers
            size;
        mfxU32
            mu_mv_mag_sq;
        bool
            firstFrame;
        ASCimageData
            *gainCorrection;
        mfxU8
            control;
        mfxU32
            frameOrder;
    }ASCVidRead;

    class ASC {
    public:
        ASC();
    //private:
        mfxU32
            m_gpuImPitch,
            m_threadsWidth,
            m_threadsHeight;
        mfxI32
            m_gpuwidth,
            m_gpuheight;

        static const int subWidth = 128;
        static const int subHeight = 64;

        ASCVidRead *m_support;
        ASCVidData *m_dataIn;
        ASCVidSample **m_videoData;
        bool
            m_dataReady,
            m_cmDeviceAssigned,
            m_is_LTR_on,
            m_ASCinitialized;
        mfxI32
            m_width,
            m_height,
            m_pitch;
        bool ColorFormatYUV; //true - YUV, false - ARGB
        /**
        ****************************************************************
        * \Brief List of long term reference friendly frame detection
        *
        * The list keeps the history of ltr friendly frame detections,
        * each element in the listis made of a frame number <mfxI32> and
        * its detection <bool> as a pair.
        *
        */
        std::list<std::pair<mfxI32, bool> > ltr_check_history;

        int m_AVX2_available;
        int m_SSE4_available;
        t_GainOffset               GainOffset;
        t_RsCsCalc                 RsCsCalc_4x4;
        t_RsCsCalc_bound           RsCsCalc_bound;
        t_RsCsCalc_diff            RsCsCalc_diff;
        t_ImageDiffHistogram       ImageDiffHistogram;
        t_ME_SAD_8x8_Block_Search  ME_SAD_8x8_Block_Search;
        t_Calc_RaCa_pic            Calc_RaCa_pic;
        t_Agop_selection           AGOP_RF;

        void SubSample_Point(
            mfxU8 *pSrc, mfxU32 srcWidth, mfxU32 srcHeight, mfxU32 srcPitch,
            mfxU8 *pDst, mfxU32 dstWidth, mfxU32 dstHeight, mfxU32 dstPitch,
            mfxI16 &avgLuma);

        mfxStatus RsCsCalc();
        mfxI32 ShotDetect(ASCimageData& Data, ASCimageData& DataRef, ASCImDetails& imageInfo, ASCTSCstat *current, ASCTSCstat *reference, mfxU8 controlLevel);
        void MotionAnalysis(ASCVidSample *videoIn, ASCVidSample *videoRef, mfxU32 *TSC, mfxU16 *AFD, mfxU32 *MVdiffVal, mfxU32 *AbsMVSize, mfxU32 *AbsMVHSize, mfxU32 *AbsMVVSize, ASCLayers lyrIdx, float *mvSum);

        typedef void(ASC::*t_resizeImg)(mfxU8 *frame, mfxI32 srcWidth, mfxI32 srcHeight, mfxI32 inputPitch, aenc::ASCLayers dstIdx, mfxU32 parity);
        t_resizeImg resizeFunc;
        mfxStatus VidSample_Alloc();
        void VidSample_dispose();
        void VidRead_dispose();
        mfxStatus SetWidth(mfxI32 Width);
        mfxStatus SetHeight(mfxI32 Height);
        mfxStatus SetPitch(mfxI32 Pitch);
        void SetNextField();
        mfxStatus SetDimensions(mfxI32 Width, mfxI32 Height, mfxI32 Pitch);
        void Setup_Environment();
        void SetUltraFastDetection();
        mfxStatus Params_Init();
        void InitStruct();
        mfxStatus VidRead_Init();
        mfxStatus VidSample_Init();
        void SubSampleASC_ImagePro(mfxU8 *frame, mfxI32 srcWidth, mfxI32 srcHeight, mfxI32 inputPitch, ASCLayers dstIdx, mfxU32 parity);
        void SubSampleASC_ImageInt(mfxU8 *frame, mfxI32 srcWidth, mfxI32 srcHeight, mfxI32 inputPitch, ASCLayers dstIdx, mfxU32 parity);
        bool CompareStats(mfxU8 current, mfxU8 reference);
        bool FrameRepeatCheck();
        void DetectShotChangeFrame(bool hasLTR = false);
        void GeneralBufferRotation();
        void Put_LTR_Hint();
        ASC_LTR_DEC Continue_LTR_Mode(mfxU16 goodLTRLimit, mfxU16 badLTRLimit);
        void AscFrameAnalysis();
        mfxStatus RunFrame(mfxU8 *frame, mfxU32 parity, bool hasLTR = false);

        mfxStatus SetInterlaceMode(ASCFTS interlaceMode);
    public:

        mfxU16 ML_SelectGoPSize();
        mfxU16 GetFrameGopSize();
        mfxStatus Init(mfxI32 Width, mfxI32 Height, mfxI32 Pitch, mfxU32 /*PicStruct*/, bool IsYUV, mfxU32 CodecId);
        void Close();
        bool IsASCinitialized();

        mfxStatus AssignResources(mfxU8 position, mfxU8 *pixelData);
        void SetControlLevel(mfxU8 level);
        mfxStatus SetGoPSize(mfxU32 GoPSize);
        void ResetGoPSize();

        mfxStatus PutFrameProgressive(mfxU8 *frame, mfxI32 Pitch, bool hasLTR);
        bool   Get_Last_frame_Data();
        mfxU16 Get_asc_subsampling_width();
        mfxU16 Get_asc_subsampling_height();
        mfxU32 Get_starting_frame_number();
        mfxU32 Get_frame_number();

        mfxU32 Get_frame_shot_Decision();
        bool Get_Repeated_Frame_Flag();
        mfxU32 Get_frame_SC();
        mfxU32 Get_frame_TSC();
        mfxF32 Get_frame_MV0();
        mfxU32 Get_frame_MVSize();
        mfxU32 Get_frame_Contrast();
        mfxF32 Get_frame_ltrMCFD();
        mfxI32 m_TSC0;
        mfxI32 Get_frame_RecentHighMvCount();
        mfxU32 Get_scene_transition_Decision();
        mfxI32 Get_frame_mcTCorr();
        void GetImageAndStat(ASCVidSample& img, ASCTSCstat& stat, uint32_t ImgIdx, uint32_t StatIdx);
        void SetImageAndStat(ASCVidSample& img, ASCTSCstat& stat, uint32_t ImgIdx, uint32_t StatIdx);
        mfxStatus RunFrame_LTR();
        void setLtrFlag(mfxU32 ltrFlag) { m_videoData[ASCReference_Frame]->is_ltr_frame = ltrFlag; }   // already rotated
        mfxU32 getLtrFlag() { return m_videoData[ASCReference_Frame]->is_ltr_frame; }
        mfxI32 m_TSC_APQ;
        mfxI32 Get_frame_mcTCorr_APQ();
        mfxU32 Get_frame_last_in_scene();
        bool   Get_GoPcorrected_frame_shot_Decision();
        mfxI32 Get_frame_Spatial_complexity();
        mfxI32 Get_frame_Temporal_complexity();
        mfxI32 Get_frame_SpatioTemporal_complexity();
        mfxU32 Get_PDist_advice();
        bool   Get_LTR_advice();
        bool   Get_RepeatedFrame_advice();
        mfxStatus get_LTR_op_hint(ASC_LTR_DEC& scd_LTR_hint);
        void get_PersistenceMap(mfxU8 PMap[ASC_MAP_SIZE], bool add);
        uint32_t CorrectScdMiniGopDecision();

        mfxStatus calc_RaCa_pic(mfxU8 *pSrc, mfxI32 width, mfxI32 height, mfxI32 pitch, mfxF64 &RsCs);
        bool Check_last_frame_processed(mfxU32 frameOrder);
        void Reset_last_frame_processed();
    };

    bool SCDetectRF(mfxI32 diffMVdiffVal, mfxU32 RsCsDiff, mfxU32 MVDiff, mfxU32 Rs, mfxU32 AFD,
        mfxU32 CsDiff, mfxI32 diffTSC, mfxU32 TSC, mfxU32 gchDC, mfxI32 diffRsCsdiff,
        mfxU32 posBalance, mfxU32 SC, mfxU32 TSCindex, mfxU32 Scindex, mfxU32 Cs,
        mfxI32 diffAFD, mfxU32 negBalance, mfxU32 ssDCval, mfxU32 refDCval, mfxU32 RsDiff,
        mfxU8 control);

    mfxU16 AGOPSelectRF(mfxI32 diffMVdiffVal, mfxU32 RsCsDiff, mfxU32 MVDiff, mfxU32 Rs, mfxU32 AFD,
        mfxU32 CsDiff, mfxI32 diffTSC, mfxU32 TSC, mfxU32 gchDC, mfxI32 diffRsCsdiff,
        mfxU32 posBalance, mfxU32 SC, mfxU32 TSCindex, mfxU32 Scindex, mfxU32 Cs,
        mfxI32 diffAFD, mfxU32 negBalance, mfxU32 ssDCval, mfxU32 refDCval, mfxU32 RsDiff,
        mfxU32 mu_mv_mag_sq, mfxI16 mcTcor, mfxI16 tcor);

    mfxU16 AGOPHEVCSelectRF(mfxI32 diffMVdiffVal, mfxU32 RsCsDiff, mfxU32 MVDiff, mfxU32 Rs, mfxU32 AFD,
        mfxU32 CsDiff, mfxI32 diffTSC, mfxU32 TSC, mfxU32 gchDC, mfxI32 diffRsCsdiff,
        mfxU32 posBalance, mfxU32 SC, mfxU32 TSCindex, mfxU32 Scindex, mfxU32 Cs,
        mfxI32 diffAFD, mfxU32 negBalance, mfxU32 ssDCval, mfxU32 refDCval, mfxU32 RsDiff,
        mfxU32 mu_mv_mag_sq, mfxI16 mcTcor, mfxI16 tcor);

    mfxI8 APQSelect(mfxU32 SC, mfxU32 TSC, mfxU32 MVSize, mfxU32 Contrast, mfxU32 PyramidLayer, mfxU32 BaseQp);

}; //namespace EncToolsAdaptiveGop

#endif // MFX_ENABLE_ADAPTIVE_ENCODE
#endif //__AV1SCD_H
