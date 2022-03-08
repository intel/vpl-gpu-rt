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

#ifndef __MFX_COMMON_DECODE_INT_H__
#define __MFX_COMMON_DECODE_INT_H__

#include <vector>

#include "mfx_common.h"
#include "mfx_common_int.h"
#include "umc_video_decoder.h"
#include "mfx_platform_caps.h"

class MFXMediaDataAdapter : public UMC::MediaData
{
public:
    MFXMediaDataAdapter(mfxBitstream *pBitstream = 0);

    void Load(mfxBitstream *pBitstream);
    void Save(mfxBitstream *pBitstream);

    void SetExtBuffer(mfxExtBuffer*);
};

mfxStatus ConvertUMCStatusToMfx(UMC::Status status);

void ConvertMFXParamsToUMC(mfxVideoParam const*, UMC::VideoStreamInfo*);
void ConvertMFXParamsToUMC(mfxVideoParam const*, UMC::VideoDecoderParams*);

UMC::ColorFormat ConvertFOURCCToUMCColorFormat(mfxU32);
mfxU32 ConvertUMCColorFormatToFOURCC(UMC::ColorFormat);

void ConvertUMCParamsToMFX(UMC::VideoStreamInfo const*, mfxVideoParam*);
void ConvertUMCParamsToMFX(UMC::VideoDecoderParams const*, mfxVideoParam*);

bool IsNeedChangeVideoParam(mfxVideoParam *par);

mfxU16 FourCcBitDepth(mfxU32 fourCC);
bool InitBitDepthFields(mfxFrameInfo *info);

inline mfxU32 ExtractProfile(mfxU32 profile)
{
    return profile & 0xFF;
}

inline bool IsMVCProfile(mfxU32 profile)
{
    profile = ExtractProfile(profile);
    return (profile == MFX_PROFILE_AVC_MULTIVIEW_HIGH || profile == MFX_PROFILE_AVC_STEREO_HIGH);
}

#if defined(MFX_ENABLE_SVC_VIDEO_DECODE)
inline bool IsSVCProfile(mfxU32 profile)
{
    profile = ExtractProfile(profile);
    return (profile == MFX_PROFILE_AVC_SCALABLE_BASELINE || profile == MFX_PROFILE_AVC_SCALABLE_HIGH);
}
#endif


inline
void MoveBitstreamData(mfxBitstream& bs, mfxU32 offset)
{
    assert(offset <= bs.DataLength);
    bs.DataOffset += offset;
    bs.DataLength -= offset;
}

// Memory reference counting base class
class RefCounter
{
public:

    RefCounter() : m_refCounter(0)
    {
    }

    void IncrementReference() const;

    void DecrementReference();

    void ResetRefCounter() { m_refCounter = 0; }

    uint32_t GetRefCounter() const { return m_refCounter; }

protected:
    mutable int32_t m_refCounter;

    virtual ~RefCounter()
    {
    }

    virtual void Free()
    {
    }
};

inline
mfxU16 CalculateNumThread(mfxVideoParam *par)
{
    (void)par;
    return 1;
}

inline
mfxU32 CalculateAsyncDepth(mfxVideoParam *par)
{
    mfxU32 asyncDepth = par ? par->AsyncDepth : 0;
    if (!asyncDepth)
    {
        asyncDepth = MFX_AUTO_ASYNC_DEPTH_VALUE;
    }

    return asyncDepth;
}

#endif
