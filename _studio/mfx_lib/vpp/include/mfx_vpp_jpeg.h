// Copyright (c) 2011-2019 Intel Corporation
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

#if defined (MFX_ENABLE_VPP)
#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)

#ifndef __MFX_VPP_JPEG_H
#define __MFX_VPP_JPEG_H

#include "umc_va_base.h"
#include "mfx_vpp_interface.h"
#include "umc_mutex.h"

#include <map>

class VideoVppJpeg
{
public:

    VideoVppJpeg(VideoCORE *core, bool useInternalMem);
    virtual ~VideoVppJpeg(void);

    mfxStatus Init(const mfxVideoParam *par);
    mfxStatus Close(void);

    mfxStatus BeginHwJpegProcessing(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxU16* taskId);
    mfxStatus BeginHwJpegProcessing(mfxFrameSurface1 *inTop, mfxFrameSurface1 *inBottom, mfxFrameSurface1 *out, mfxU16* taskId);
    mfxStatus QueryTaskRoutine(mfxU16 taskId);
    mfxStatus EndHwJpegProcessing(mfxFrameSurface1 *in, mfxFrameSurface1 *out);
    mfxStatus EndHwJpegProcessing(mfxFrameSurface1 *inTop, mfxFrameSurface1 *inBottom, mfxFrameSurface1 *out);

protected:

    VideoCORE *m_pCore;
    mfxU16 m_IOPattern;
    bool   m_bUseInternalMem;
    mfxU16 m_taskId;
#ifdef MFX_ENABLE_MJPEG_ROTATE_VPP
    mfxI32 m_rotation;
#endif
    std::vector<mfxFrameSurface1>  m_surfaces;
    mfxFrameAllocResponse m_response;
    UMC::Mutex m_guard;

    std::map<mfxMemId, mfxI32> AssocIdx;

    VPPHWResMng  m_ddi;
};



#endif // __MFX_VPP_JPEG_H

#endif // MFX_ENABLE_MJPEG_VIDEO_DECODE
#endif // MFX_ENABLE_VPP
