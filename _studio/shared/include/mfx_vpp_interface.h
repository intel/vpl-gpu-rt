// Copyright (c) 2018-2021 Intel Corporation
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
#include "mfxcamera.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_VPP_DDI_H
#define __MFX_VPP_DDI_H

#include <vector>
#include <memory>
#include "mfxdefs.h"
#include "libmfx_core.h"
#include "mfx_platform_headers.h"

    typedef unsigned int   UINT;

    typedef struct tagVPreP_StatusParams_V1_0
    {
        UINT StatusReportID;
        UINT Reserved1;

    } VPreP_StatusParams_V1_0;

    typedef struct tagFASTCOMP_QUERY_STATUS
    {
        UINT QueryStatusID;
        UINT Status;

        UINT Reserved0;
        UINT Reserved1;
        UINT Reserved2;
        UINT Reserved3;
        UINT Reserved4;

    } FASTCOMP_QUERY_STATUS;

    typedef enum _VPREP_ISTAB_MODE
    {
        ISTAB_MODE_NONE,
        ISTAB_MODE_BLACKEN,
        ISTAB_MODE_UPSCALE
    } VPREP_ISTAB_MODE;

namespace MfxHwVideoProcessing
{
    const mfxU32 g_TABLE_SUPPORTED_FOURCC [] =
    {
        MFX_FOURCC_NV12      ,
        MFX_FOURCC_YV12      ,
        MFX_FOURCC_NV16      ,
        MFX_FOURCC_YUY2      ,
        MFX_FOURCC_RGB3      ,
        MFX_FOURCC_RGB4      ,
        MFX_FOURCC_BGR4      ,
#if defined (MFX_ENABLE_FOURCC_RGB565)
        MFX_FOURCC_RGB565    ,
#endif
#ifdef MFX_ENABLE_RGBP
        MFX_FOURCC_RGBP      ,
#endif
        MFX_FOURCC_P8        ,
        MFX_FOURCC_P8_TEXTURE,
        MFX_FOURCC_P010      ,
        MFX_FOURCC_P210      ,
        MFX_FOURCC_A2RGB10   ,
        MFX_FOURCC_ARGB16    ,
        MFX_FOURCC_R16       ,
        MFX_FOURCC_AYUV      ,
        MFX_FOURCC_AYUV_RGB4 ,
        MFX_FOURCC_UYVY      ,
        MFX_FOURCC_Y210      ,
        MFX_FOURCC_Y410      ,
        MFX_FOURCC_P016      ,
        MFX_FOURCC_Y216      ,
        MFX_FOURCC_Y416      ,
        MFX_FOURCC_BGRP      ,
        MFX_FOURCC_ABGR16F   ,
        MFX_FOURCC_I420      ,
    };

    typedef enum mfxFormatSupport {
        MFX_FORMAT_SUPPORT_INPUT   = 1,
        MFX_FORMAT_SUPPORT_OUTPUT  = 2
    } mfxFormatSupport;

    struct RateRational
    {
        mfxU32  FrameRateExtN;
        mfxU32  FrameRateExtD;
    };

    struct CustomRateData
    {
        RateRational customRate;
        mfxU32       inputFramesOrFieldPerCycle;
        mfxU32       outputIndexCountPerCycle;
        mfxU32       bkwdRefCount;
        mfxU32       fwdRefCount;

        mfxU32       indexRateConversion;
    };

    struct FrcCaps
    {
        std::vector<CustomRateData> customRateData;
    };

    struct DstRect
    {
        mfxU32 DstX;
        mfxU32 DstY;
        mfxU32 DstW;
        mfxU32 DstH;
        mfxU16 LumaKeyEnable;
        mfxU16 LumaKeyMin;
        mfxU16 LumaKeyMax;
        mfxU16 GlobalAlphaEnable;
        mfxU16 GlobalAlpha;
        mfxU16 PixelAlphaEnable;
        mfxU16 IOPattern; //sys/video/opaque memory
        mfxU16 ChromaFormat;
        mfxU32 FourCC;
        mfxU32 TileId;
    };

    typedef struct _CameraCaps
    {
        mfxU32 uBlackLevelCorrection;
        mfxU32 uHotPixelCheck;
        mfxU32 uWhiteBalance;
        mfxU32 uColorCorrectionMatrix;
        mfxU32 uGammaCorrection;
        mfxU32 uVignetteCorrection;
        mfxU32 u3DLUT;
        mfxU32 uCscYuvRgb;
        mfxU32 uTotalColorControl;
        mfxU32 uLensGeomDistCorrection;
        mfxU32 uPadding;
        mfxU32 uBayerDenoise;
    } CameraCaps;

    struct mfxVppCaps
    {
        mfxU32 uAdvancedDI;
        mfxU32 uSimpleDI;
        mfxU32 uInverseTC;
        mfxU32 uDenoiseFilter;
#ifdef MFX_ENABLE_MCTF
        mfxU32 uMCTF;
#endif
        mfxU32 uDetailFilter;
        mfxU32 uProcampFilter;
        mfxU32 uSceneChangeDetection;

        CameraCaps cameraCaps;

        mfxU32 uFrameRateConversion;
        mfxU32 uDeinterlacing;

        mfxU32 uVideoSignalInfoInOut;

        mfxU32 uHdr10ToneMapping;

        mfxU32 uVideoSignalInfo;
        FrcCaps frcCaps;

        mfxU32 uIStabFilter;
        mfxU32 uVariance;

        mfxI32 iNumBackwardSamples;
        mfxI32 iNumForwardSamples;

        mfxU32 uMaxWidth;
        mfxU32 uMaxHeight;
        mfxU32 uMinWidth;
        mfxU32 uMinHeight;

        mfxU32 uFieldWeavingControl;

        mfxU32 uRotation;

        mfxU32 uScaling;

        mfxU32 uChromaSiting;

        std::map <mfxU32, mfxU32> mFormatSupport;

        mfxU32 uMirroring;

        mfxU32 uFieldProcessing;

        mfxU32 u3DLut;
        mfxU32 uDenoise2Filter;   // mfxExtVPPDenoise2

        mfxU32 uSuperResolution;
        mfxU32 uSrMaxInWidth;
        mfxU32 uSrMaxInHeight;

        mfxVppCaps()
            : uAdvancedDI(0)
            , uSimpleDI(0)
            , uInverseTC(0)
            , uDenoiseFilter(0)
#ifdef MFX_ENABLE_MCTF
            , uMCTF(0)
#endif
            , uDetailFilter(0)
            , uProcampFilter(0)
            , uSceneChangeDetection(0)
            , uFrameRateConversion(0)
            , uDeinterlacing(0)

            , uVideoSignalInfoInOut(0)
            , uHdr10ToneMapping(0)
            , uVideoSignalInfo(0)
            , frcCaps()
            , uIStabFilter(0)
            , uVariance(0)
            , iNumBackwardSamples(0)
            , iNumForwardSamples(0)
            , uMaxWidth(0)
            , uMaxHeight(0)
            , uMinWidth(0)
            , uMinHeight(0)
            , uFieldWeavingControl(0)
            , uRotation(0)
            , uScaling(0)
            , uChromaSiting(0)
            , mFormatSupport()
            , uMirroring(0)
            , uFieldProcessing(0)
            , u3DLut(0)
            , uDenoise2Filter(0)
            , uSuperResolution(0)
            , uSrMaxInWidth(0)
            , uSrMaxInHeight(0)
        {
            memset(&cameraCaps, 0, sizeof(CameraCaps));
        };
    };


    typedef struct _mfxDrvSurface
    {
        mfxFrameInfo frameInfo;
        mfxHDLPair   hdl;

        mfxMemId     memId;
        bool         bExternal;

        mfxU64       startTimeStamp;
        mfxU64       endTimeStamp;

    } mfxDrvSurface;

    // Scene change detection
    typedef enum {
        VPP_NO_SCENE_CHANGE  = 0,
        VPP_SCENE_NEW        = 1,            // BOB display current field to generate output
        VPP_MORE_SCENE_CHANGE_DETECTED = 2 // BOB display only first field to avoid out of frame order
    } vppScene;

    // mfxVideoSignalInfo is for MFX_EXTBUFF_VIDEO_SIGNAL_INFO which supports the video signal info
    // in the video spec theoretically. See the mfxExtVideoSignalInfo structure for details.
    struct mfxVideoSignalInfo {
        bool            enabled;
        mfxU16          VideoFormat;
        mfxU16          VideoFullRange;
        mfxU16          ColourDescriptionPresent;
        mfxU16          ColourPrimaries;
        mfxU16          TransferCharacteristics;
        mfxU16          MatrixCoefficients;

        bool operator!=(const mfxVideoSignalInfo & other) const
        {
            return enabled                      != other.enabled
                || VideoFormat                  != other.VideoFormat
                || VideoFullRange               != other.VideoFullRange
                || ColourPrimaries              != other.ColourPrimaries
                || TransferCharacteristics      != other.TransferCharacteristics
                || MatrixCoefficients           != other.MatrixCoefficients;
        }
    };

    class mfxExecuteParams
    {
        // SignalInfo is for MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO which only
        // supports the signal info of SDR
        struct SignalInfo {
            bool   enabled;
            mfxU16 TransferMatrix;
            mfxU16 NominalRange;

            bool operator!=(const SignalInfo & other) const
            {
                return enabled        != other.enabled
                    || TransferMatrix != other.TransferMatrix
                    || NominalRange   != other.NominalRange;
            }
        };

        struct Lut3DInfo {
            bool                      Enabled;
            mfxMemId                  MemId;
            mfxDataType               DataType;
            mfxResourceType           BufferType;
            mfx3DLutMemoryLayout      MemLayout;
            mfx3DLutChannelMapping    ChannelMapping;
            mfxChannel                Channel[3];
#ifdef ONEVPL_EXPERIMENTAL
            mfx3DLutInterpolationMethod InterpolationMethod;
#endif
        };

        struct SRInfo {
            bool                                 Enabled;
            mfxAISuperResolutionMode             Mode;
            mfxAISuperResolutionAlgorithm        Algorithm;

            bool operator!=(const SRInfo& other) const
            {
                return Enabled        != other.Enabled
                       || Mode        != other.Mode
                       || Algorithm   != other.Algorithm;
            }
        };

    public:
            mfxExecuteParams():
                targetSurface()
               ,targetTimeStamp()
               ,pRefSurfaces(0)
               ,refCount(0)
               ,bkwdRefCount(0)
               ,fwdRefCount(0)
               ,iDeinterlacingAlgorithm(0)
               ,bFMDEnable(false)
               ,bDenoiseAutoAdjust(false)
               ,denoiseFactor(0)
               ,denoiseFactorOriginal(0)
               ,denoiseMode(MFX_DENOISE_MODE_DEFAULT)
               ,bdenoiseAdvanced(false)
               ,bDetailAutoAdjust(false)
               ,detailFactor(0)
               ,detailFactorOriginal(0)
               ,iTargetInterlacingMode(0)
               ,bEnableProcAmp(false)
               ,Brightness(0)
               ,Contrast(0)
               ,Hue(0)
               ,Saturation(0)
               ,bVarianceEnable(false)
               ,bImgStabilizationEnable(false)
               ,istabMode(0)
               ,bFRCEnable(false)
               ,frcModeOrig(0)
               ,bComposite(false)
               ,dstRects(0)
               ,bBackgroundRequired(true)
               ,iBackgroundColor(0)
               ,iTilesNum4Comp(0)
               ,execIdx(NO_INDEX)
               ,initialStreamNum(0)
               ,statusReportID(0)
               ,bFieldWeaving(false)
               ,bFieldWeavingExt(false)
               ,bFieldSplittingExt(false)
               ,iFieldProcessingMode(0)
               ,bCameraPipeEnabled(false)
               ,bCameraPipeControl(0)
               ,CameraPipeControl()
               ,bCameraBlackLevelCorrection(false)
               ,CameraBlackLevel()
               ,bCameraWhiteBalaceCorrection(false)
               ,CameraWhiteBalance()
               ,bCameraHotPixelRemoval(false)
               ,CameraHotPixel()
               ,bCCM(false)
               ,CCMParams()
               ,bCameraTCC(false)
               ,CameraTCC()
               ,bCameraRGBtoYUV(false)
               ,CameraRGBToYUV()               
               ,bCameraGammaCorrection(false)
               ,CameraForwardGammaCorrection()
               ,bCameraVignetteCorrection(false)
               ,CameraVignetteCorrection()
               ,bCameraLensCorrection(false)
               ,CameraLensCorrection()
               ,bCamera3DLUT(false)
               ,Camera3DLUT()
               ,bCameraPadding(false)
               ,CameraPadding()
               ,bCameraBayerDenoise(false)
               ,CameraBayerDenoise()
               ,rotation(0)
               ,scalingMode(MFX_SCALING_MODE_DEFAULT)
               ,interpolationMethod(MFX_INTERPOLATION_DEFAULT)
               ,chromaSiting(MFX_CHROMA_SITING_UNKNOWN)
               ,bEOS(false)
               ,mirroring(0)
               ,mirroringPosition(0)
               ,mirroringExt(false)
               ,scene(VPP_NO_SCENE_CHANGE)
               ,bDeinterlace30i60p(false)
               ,bAiVfi(false)
               ,m_aiFiMode(MFX_AI_FRAME_INTERPOLATION_MODE_DISABLE)
#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
               ,gpuHangTrigger(false)
#endif
#ifdef MFX_ENABLE_MCTF
               , bEnableMctf(false)
               , MctfFilterStrength(0)
#endif
#if defined (ONEVPL_EXPERIMENTAL)
               , bEnablePercEncFilter(false)
#endif
               , reset(0)
            {
                   memset(&targetSurface, 0, sizeof(mfxDrvSurface));
                   dstRects.clear();
                   memset(&customRateData, 0, sizeof(CustomRateData));
                   VideoSignalInfoIn.enabled         = false;
                   VideoSignalInfoIn.NominalRange    = MFX_NOMINALRANGE_16_235;
                   VideoSignalInfoIn.TransferMatrix  = MFX_TRANSFERMATRIX_BT601;
                   VideoSignalInfoOut.enabled        = false;
                   VideoSignalInfoOut.NominalRange   = MFX_NOMINALRANGE_16_235;
                   VideoSignalInfoOut.TransferMatrix = MFX_TRANSFERMATRIX_BT601;

                   VideoSignalInfo.clear();
                   VideoSignalInfo.assign(1, VideoSignalInfoIn);
                   lut3DInfo= {};

                   m_inVideoSignalInfo  = {};
                   m_outVideoSignalInfo = {};

                   inHDR10MetaData = {};
                   outHDR10MetaData = {};
                   denoiseStrength = 0;

                   srSetParams.Enabled = false;
                   srSetParams.Mode = MFX_AI_SUPER_RESOLUTION_MODE_DISABLED;
                   srSetParams.Algorithm = MFX_AI_SUPER_RESOLUTION_ALGORITHM_DEFAULT;
                   srGetParams = srSetParams;
            };

            bool IsDoNothing()
            {
                CustomRateData refCustomRateData;
                memset(&refCustomRateData, 0, sizeof(CustomRateData));
                if (memcmp(&refCustomRateData, &customRateData, sizeof(CustomRateData)))
                    return false;
                if (iDeinterlacingAlgorithm != 0 ||
                    bFMDEnable != 0 ||
                    bDenoiseAutoAdjust != 0 ||
                    bDetailAutoAdjust != 0 ||
                    denoiseFactor != 0 ||
                    bdenoiseAdvanced != false ||
                    detailFactor != 0 ||
                    iTargetInterlacingMode != 0 ||
                    bEnableProcAmp != false ||
                    bVarianceEnable != false ||
                    bImgStabilizationEnable != false ||
                    bFRCEnable != false ||
                    bComposite != false ||
                    bFieldWeaving != false ||
                    bFieldSplittingExt != false ||
                    iFieldProcessingMode != 0 ||
                    bCameraPipeEnabled != false ||
                    bCameraBlackLevelCorrection != false ||
                    bCameraGammaCorrection != false ||
                    bCameraHotPixelRemoval != false ||
                    bCameraWhiteBalaceCorrection != false ||
                    bCCM != false ||
                    bCameraLensCorrection != false ||
                    rotation != 0 ||
                    scalingMode != MFX_SCALING_MODE_DEFAULT ||
                    mirroring != 0 ||
                    mirroringExt != false ||
                    scene != VPP_NO_SCENE_CHANGE ||
                    bDeinterlace30i60p != false  ||
                    chromaSiting != MFX_CHROMA_SITING_UNKNOWN
#ifdef MFX_ENABLE_MCTF
                    || bEnableMctf != false
#endif
#if defined (ONEVPL_EXPERIMENTAL)
                    || bEnablePercEncFilter
#endif
                    || lut3DInfo.Enabled != false
                )
                    return false;
                if (VideoSignalInfoIn != VideoSignalInfoOut)
                    return false;
                if (m_inVideoSignalInfo != m_outVideoSignalInfo)
                    return false;
                return true;
            };

        //surfaces
        mfxDrvSurface  targetSurface;
        mfxU64         targetTimeStamp;

        mfxDrvSurface* pRefSurfaces;
        int            refCount; // refCount == bkwdRefCount + 1 + fwdRefCount

        int            bkwdRefCount; // filled from DdiTask
        int            fwdRefCount;  //

        // params
        mfxI32         iDeinterlacingAlgorithm; //0 - none, 1 - BOB, 2 - advanced (means reference need)
        bool           bFMDEnable;

        bool           bDenoiseAutoAdjust;    // MFX_EXTBUFF_VPP_DENOISE
        mfxU16         denoiseFactor;         // MFX_EXTBUFF_VPP_DENOISE
        mfxU16         denoiseFactorOriginal; // MFX_EXTBUFF_VPP_DENOISE, Original denoise factor or strength provided by app.
        mfxDenoiseMode denoiseMode;           // MFX_EXTBUFF_VPP_DENOISE2 Denoise mode
        mfxU16         denoiseStrength;       // MFX_EXTBUFF_VPP_DENOISE2 denoiseStrength
        bool           bdenoiseAdvanced;      // Indicate MFX_EXTBUFF_VPP_DENOISE2
        bool           bDetailAutoAdjust;
        mfxU16         detailFactor;
        mfxU16         detailFactorOriginal;  // Original detail factor provided by app.

        mfxU32         iTargetInterlacingMode;

        bool           bEnableProcAmp;
        mfxF64         Brightness;
        mfxF64         Contrast;
        mfxF64         Hue;
        mfxF64         Saturation;

        bool           bVarianceEnable;
        bool           bImgStabilizationEnable;
        mfxU32         istabMode;

        bool           bFRCEnable;
        CustomRateData customRateData;
        mfxU16         frcModeOrig; // Original mode provided by app

        bool           bComposite;
        std::vector<DstRect> dstRects;
        bool           bBackgroundRequired;
        mfxU64         iBackgroundColor;
        mfxU32         iTilesNum4Comp;
        mfxU32         execIdx; //index call of execute for current frame, actual for composition
        mfxU32         initialStreamNum;

        mfxU32         statusReportID;

        bool           bFieldWeaving;
        bool           bFieldWeavingExt;
        bool           bFieldSplittingExt;

        mfxU32         iFieldProcessingMode;

        //  Camera Pipe specific params
        bool                            bCameraPipeEnabled;
        bool                            bCameraPipeControl;
        mfxExtCamPipeControl            CameraPipeControl;
        bool                            bCameraBlackLevelCorrection;
        mfxExtCamBlackLevelCorrection   CameraBlackLevel;
        bool                            bCameraWhiteBalaceCorrection;
        mfxExtCamWhiteBalance           CameraWhiteBalance;
        bool                            bCameraHotPixelRemoval;
        mfxExtCamHotPixelRemoval        CameraHotPixel;
        bool                            bCCM;
        mfxExtCamColorCorrection3x3     CCMParams;
        bool                            bCameraTCC;
        mfxExtCamTotalColorControl      CameraTCC;
        bool                            bCameraRGBtoYUV;
        mfxExtCamCscYuvRgb              CameraRGBToYUV;
        bool                            bCameraGammaCorrection;
        mfxExtCamFwdGamma               CameraForwardGammaCorrection;
        bool                            bCameraVignetteCorrection;
        mfxExtCamVignetteCorrection     CameraVignetteCorrection;
        bool                            bCameraLensCorrection;
        mfxExtCamLensGeomDistCorrection CameraLensCorrection;
        bool                            bCamera3DLUT;
        mfxExtCam3DLut                  Camera3DLUT;
        bool                            bCameraPadding;
        mfxExtCamPadding                CameraPadding;
        bool                            bCameraBayerDenoise;
        mfxExtCamBayerDenoise           CameraBayerDenoise;

        int         rotation;

        mfxU16      scalingMode;

        mfxU16      interpolationMethod;

        mfxU16      chromaSiting;

        bool        bEOS;

        SignalInfo VideoSignalInfoIn;   // Common video signal info set on Init
        SignalInfo VideoSignalInfoOut;  // Video signal info for output

        std::vector<SignalInfo> VideoSignalInfo; // Video signal info for each frame in a single run

        int        mirroring;
        int        mirroringPosition;
        bool       mirroringExt;

        vppScene    scene;     // Keep information about scene change
        bool        bDeinterlace30i60p;

        SRInfo      srSetParams;
        SRInfo      srGetParams;

        bool                        bAiVfi;
        mfxAIFrameInterpolationMode m_aiFiMode;

#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
        bool       gpuHangTrigger;
#endif

#ifdef MFX_ENABLE_MCTF
        bool         bEnableMctf;
        mfxU16       MctfFilterStrength;
#endif
#if defined (ONEVPL_EXPERIMENTAL)
        bool         bEnablePercEncFilter;
#endif

        bool reset;
        Lut3DInfo    lut3DInfo;

        mfxVideoSignalInfo    m_inVideoSignalInfo;
        mfxVideoSignalInfo    m_outVideoSignalInfo;

        struct HDR10MetaData
        {
            bool   enabled;
            mfxU16 displayPrimariesX[3];
            mfxU16 displayPrimariesY[3];
            mfxU16 whitePoint[2];
            mfxU32 maxMasteringLuminance;
            mfxU32 minMasteringLuminance;
            mfxU16 maxContentLightLevel;
            mfxU16 maxFrameAverageLightLevel;
        };
        HDR10MetaData inHDR10MetaData;
        HDR10MetaData outHDR10MetaData;
    };

    class DriverVideoProcessing
    {
    public:

        virtual ~DriverVideoProcessing(void){}

        virtual mfxStatus CreateDevice(VideoCORE * core, mfxVideoParam* par, bool isTemporal = false) = 0;

        virtual mfxStatus ReconfigDevice(mfxU32 indx) = 0;

        virtual mfxStatus DestroyDevice( void ) = 0;

        virtual mfxStatus Register(mfxHDLPair* pSurfaces, mfxU32 num, BOOL bRegister) = 0;

        virtual mfxStatus QueryTaskStatus(SynchronizedTask* pSyncTask) = 0;

        virtual mfxStatus Execute(mfxExecuteParams *pParams) = 0;

        virtual mfxStatus QueryCapabilities( mfxVppCaps& caps ) = 0;

        virtual mfxStatus QueryVariance(
            mfxU32 frameIndex,
            std::vector<UINT> &variance) = 0;
    }; // DriverVideoProcessing

    DriverVideoProcessing* CreateVideoProcessing( VideoCORE* core );

}; // namespace


/*
 * Simple proxy for VPP device/caps create. It simplifies having a single
 * cached vpp processing device accessible thru VideoCORE
 */
class VPPHWResMng
{
public:
    VPPHWResMng(): m_ddi(nullptr), m_caps() {};

    ~VPPHWResMng() { Close(); };

    mfxStatus Close(void){
        m_ddi.reset(0);
        return MFX_ERR_NONE;
    }

    MfxHwVideoProcessing::DriverVideoProcessing *GetDevice(void) const {
        return m_ddi.get();
    };

    mfxStatus SetDevice(MfxHwVideoProcessing::DriverVideoProcessing *ddi){
        MFX_CHECK_NULL_PTR1(ddi);
        MFX_CHECK_STS(Close());
        m_ddi.reset(ddi);
        return MFX_ERR_NONE;
    }

    MfxHwVideoProcessing::mfxVppCaps GetCaps(void) const {
        return m_caps;
    }

    mfxStatus  SetCaps(const MfxHwVideoProcessing::mfxVppCaps &caps){
        m_caps = caps;
        return MFX_ERR_NONE;
    }

    mfxStatus CreateDevice(VideoCORE * core);

    // Just to make ResMang easier to use with existing code of DriverVideoProcessing
    MfxHwVideoProcessing::DriverVideoProcessing *operator->() const {
        return m_ddi.get();
    }

private:
    VPPHWResMng(const VPPHWResMng &);
    VPPHWResMng &operator=(const VPPHWResMng &);

    std::unique_ptr<MfxHwVideoProcessing::DriverVideoProcessing> m_ddi;
    MfxHwVideoProcessing::mfxVppCaps                           m_caps;
};


#endif // __MFX_VPP_BASE_DDI_H
#endif // MFX_ENABLE_VPP
/* EOF */
