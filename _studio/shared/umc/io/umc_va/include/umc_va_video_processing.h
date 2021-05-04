// Copyright (c) 2014-2018 Intel Corporation
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

#ifndef __UMC_VA_VIDEO_PROCESSING_H
#define __UMC_VA_VIDEO_PROCESSING_H

#include "umc_va_base.h"
#include "mfxstructures.h"

namespace UMC
{

class VideoProcessingVA
{
public:

    VideoProcessingVA();

    virtual ~VideoProcessingVA();

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    virtual Status Init(mfxVideoParam * vpParams, mfxExtDecVideoProcessing * videoProcessing);

    virtual void SetOutputSurface(mfxHDL surfHDL);

    mfxHDL GetCurrentOutputSurface() const;

#ifdef UMC_VA_LINUX
    VAProcPipelineParameterBuffer m_pipelineParams;
#endif

protected:

#ifdef UMC_VA_DXVA

#elif defined(UMC_VA_LINUX)

    VARectangle m_surf_region;
    VARectangle m_output_surf_region;
    VASurfaceID output_surface_array[1];
#endif

    mfxHDL m_currentOutputSurface;
#endif // #ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
};

}
#endif // __UMC_VA_VIDEO_PROCESSING_H
