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

#ifndef __UMC_DATA_POINTERS_COPY_H__
#define __UMC_DATA_POINTERS_COPY_H__

#include "vm_types.h"
#include "ippdefs.h"
#include "umc_video_data.h"
#include "umc_base_codec.h"

namespace UMC
{

// This class is designed as special kind of VideoProcessing which just
// copies pointers and therefore gives access to internal buffers of decoders.

class DataPointersCopy : public BaseCodec
{
  DYNAMIC_CAST_DECL(DataPointersCopy, BaseCodec)
public:
  virtual Status Init(BaseCodecParams* /*init*/)
  {
    return UMC_OK;
  }

  virtual Status GetFrame(MediaData *input, MediaData *output)
  {
    VideoData *video_input = DynamicCast<VideoData>(input);
    VideoData *video_output = DynamicCast<VideoData>(output);

    if (video_input && video_output) {
      *video_output = *video_input;
    } else {
      *output = *input;
    }

    return UMC_OK;
  }

  virtual Status GetInfo(BaseCodecParams* /*info*/)
  {
    return UMC_OK;
  }

  virtual Status Close(void)
  {
    return UMC_OK;
  }

  virtual Status Reset(void)
  {
    return UMC_OK;
  }
};

} // namespace UMC

#endif /* __UMC_DATA_POINTERS_COPY_H__ */
