// Copyright (c) 2019-2021 Intel Corporation
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

#if !defined(DECL_BLOCK_LIST)
    #error "Invalid usage of " __FILE__ ": DECL_BLOCK_LIST  must be defined"
#endif
#if !defined(DECL_FEATURE_NAME)
    #error "Invalid usage of " __FILE__ ": DECL_FEATURE_NAME must be defined"
#endif

    enum eBlockId
    {
/*
* BLOCK_ID_OFFSET is used when there are feature classes inheritance and feature blocks are defined in both derived class and base class.
* BLOCK_ID_OFFSET should be defind in derived class as Base::eBlockId::NUM_BLOCKS before "#define DECL_BLOCK_LIST".
*/
#if defined(BLOCK_ID_OFFSET)
        __FIRST_MINUS1 = BLOCK_ID_OFFSET - 1
#else
        __FIRST_MINUS1 = -1
#endif
    #define DECL_BLOCK(NAME) , BLK_##NAME
        DECL_BLOCK_LIST
    #undef DECL_BLOCK
        , NUM_BLOCKS
    };

#if defined(BLOCK_ID_OFFSET)
    #undef BLOCK_ID_OFFSET
#endif

#if defined(MFX_ENABLE_LOG_UTILITY)
    #if !defined(CODEC_NAME_PREFIX)
        #error "Invalid usage of " __FILE__ ": CODEC_NAME_PREFIX must be defined"
    #endif

    BlockTracer::TFeatureTrace m_trace =
    {
        CODEC_NAME_PREFIX + std::string(DECL_FEATURE_NAME),
        {
        #define DECL_BLOCK(NAME) {BLK_##NAME, #NAME},
                    DECL_BLOCK_LIST
        #undef DECL_BLOCK
        }
    };

    virtual const BlockTracer::TFeatureTrace* GetTrace() override { return &m_trace; }
    virtual void SetTraceName(std::string&& name) override { m_trace.first = CODEC_NAME_PREFIX + std::move(name); }
#endif // defined(MFX_ENABLE_LOG_UTILITY)

#if !defined DECL_BLOCK_CLEANUP_DISABLE
    #undef DECL_BLOCK_LIST
    #undef DECL_FEATURE_NAME
#endif
