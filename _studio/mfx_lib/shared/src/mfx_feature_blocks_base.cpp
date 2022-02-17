// Copyright (c) 2021 Intel Corporation
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

#include "mfx_utils_logging.h"
#include "mfx_feature_blocks_base.h"

namespace MfxFeatureBlocks
{
BlockTracer::BlockTracer(
    ID id
    , const char* fName
    , const char* bName)
    : ID(id)
    , m_featureName(fName)
    , m_blockName(bName)
{
#if defined(MFX_ENABLE_LOG_UTILITY)
    MFX_LOG_TRACE("%s(%d)::%s(%d): Enter\n", m_featureName, FeatureID, m_blockName, BlockID);
#endif // MFX_ENABLE_LOG_UTILITY
}

BlockTracer::~BlockTracer()
{
#if defined(MFX_ENABLE_LOG_UTILITY)
    MFX_LOG_TRACE("%s(%d)::%s(%d): Exit\n", m_featureName, FeatureID, m_blockName, BlockID);
#endif // MFX_ENABLE_LOG_UTILITY
}

}; //namespace MfxFeatureBlocks
