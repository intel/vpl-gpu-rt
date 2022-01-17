// Copyright (c) 2003-2018 Intel Corporation
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

#include "umc_defs.h"

#if defined (UMC_ENABLE_VPP_RESIZE)

#include "umc_video_resizing.h"
#include "umc_video_data.h"
#include "ippi.h"

using namespace UMC;

VideoResizing::VideoResizing()
{
  mInterpolation = IPPI_INTER_NN;
}

Status VideoResizing::SetMethod(int lInterpolation)
{
  mInterpolation = lInterpolation;
  return UMC_OK;
}

Status VideoResizing::GetFrame(MediaData *input, MediaData *output)
{
  VideoData *in = DynamicCast<VideoData>(input);
  VideoData *out = DynamicCast<VideoData>(output);
  VideoData::PlaneInfo srcPlane;
  VideoData::PlaneInfo dstPlane;
  int k;

  if (NULL == in || NULL == out) {
    return UMC_ERR_NULL_PTR;
  }

  ColorFormat cFormat = in->GetColorFormat();
  if (out->GetColorFormat() != cFormat) {
    return UMC_ERR_INVALID_PARAMS;
  }
  int32_t in_Width = in->GetWidth();
  int32_t in_Height = in->GetHeight();
  int32_t out_Width = out->GetWidth();
  int32_t out_Height = out->GetHeight();

  double xRatio = static_cast<double> (out_Width)  / static_cast<double> (in_Width);
  double yRatio = static_cast<double> (out_Height) / static_cast<double> (in_Height);

  for (k = 0; k < in->GetNumPlanes(); k++) {
    in->GetPlaneInfo(&srcPlane, k);
    out->GetPlaneInfo(&dstPlane, k);

    UMC_CHECK(srcPlane.m_iSampleSize == dstPlane.m_iSampleSize, UMC_ERR_INVALID_PARAMS);
    UMC_CHECK(srcPlane.m_iSamples == dstPlane.m_iSamples, UMC_ERR_INVALID_PARAMS);

    IppiRect RectSrc = {0, 0, srcPlane.m_ippSize.width, srcPlane.m_ippSize.height};

    if (cFormat == YUY2) {
      // YUY2 format defined in VideoData with WidthDiv = 2
      srcPlane.m_ippSize.width *= 2;
      dstPlane.m_ippSize.width *= 2;
      RectSrc.width *= 2;
      ippiResizeYUV422_8u_C2R((const uint8_t *)srcPlane.m_pPlane,
        srcPlane.m_ippSize,
        (int32_t)srcPlane.m_nPitch,
        RectSrc,
        (uint8_t *)dstPlane.m_pPlane,
        (int32_t)dstPlane.m_nPitch,
        dstPlane.m_ippSize,
        xRatio,
        yRatio,
        mInterpolation);
      return UMC_OK;
    }

    if (srcPlane.m_iSampleSize == sizeof(uint8_t)) {
      switch (srcPlane.m_iSamples) {
      case 1:
        ippiResize_8u_C1R((const uint8_t *)srcPlane.m_pPlane,
                          srcPlane.m_ippSize,
                          (int32_t)srcPlane.m_nPitch,
                          RectSrc,
                          (uint8_t *)dstPlane.m_pPlane,
                          (int32_t)dstPlane.m_nPitch,
                          dstPlane.m_ippSize,
                          xRatio,
                          yRatio,
                          mInterpolation);
        break;
      case 3:
        ippiResize_8u_C3R((const uint8_t *)srcPlane.m_pPlane,
                          srcPlane.m_ippSize,
                          (int32_t)srcPlane.m_nPitch,
                          RectSrc,
                          (uint8_t *)dstPlane.m_pPlane,
                          (int32_t)dstPlane.m_nPitch,
                          dstPlane.m_ippSize,
                          xRatio,
                          yRatio,
                          mInterpolation);
        break;
      case 4:
        ippiResize_8u_C4R((const uint8_t *)srcPlane.m_pPlane,
                          srcPlane.m_ippSize,
                          (int32_t)srcPlane.m_nPitch,
                          RectSrc,
                          (uint8_t *)dstPlane.m_pPlane,
                          (int32_t)dstPlane.m_nPitch,
                          dstPlane.m_ippSize,
                          xRatio,
                          yRatio,
                          mInterpolation);
        break;
      default:
        return UMC_ERR_UNSUPPORTED;
      }
    } else if (srcPlane.m_iSampleSize == sizeof(uint16_t)) {
      switch (srcPlane.m_iSamples) {
      case 1:
        ippiResize_16u_C1R((const uint16_t *)srcPlane.m_pPlane,
                           srcPlane.m_ippSize,
                           (int32_t)srcPlane.m_nPitch,
                           RectSrc,
                           (uint16_t *)dstPlane.m_pPlane,
                           (int32_t)dstPlane.m_nPitch,
                           dstPlane.m_ippSize,
                           xRatio,
                           yRatio,
                           mInterpolation);
        break;
      case 3:
        ippiResize_16u_C3R((const uint16_t *)srcPlane.m_pPlane,
                           srcPlane.m_ippSize,
                           (int32_t)srcPlane.m_nPitch,
                           RectSrc,
                           (uint16_t *)dstPlane.m_pPlane,
                           (int32_t)dstPlane.m_nPitch,
                           dstPlane.m_ippSize,
                           xRatio,
                           yRatio,
                           mInterpolation);
        break;
      case 4:
        ippiResize_16u_C4R((const uint16_t *)srcPlane.m_pPlane,
                           srcPlane.m_ippSize,
                           (int32_t)srcPlane.m_nPitch,
                           RectSrc,
                           (uint16_t *)dstPlane.m_pPlane,
                           (int32_t)dstPlane.m_nPitch,
                           dstPlane.m_ippSize,
                           xRatio,
                           yRatio,
                           mInterpolation);
        break;
      default:
        return UMC_ERR_UNSUPPORTED;
      }
    } else {
      return UMC_ERR_UNSUPPORTED;
    }
  }
  return UMC_OK;
}

#endif // #if defined (UMC_ENABLE_VPP_RESIZE)
/* EOF */
