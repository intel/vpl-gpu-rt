# Copyright (c) 2022 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

set(sources
    include/mfxenctools-int.h
    include/mfx_enctools_loader.h
    include/mfx_ienctools.h
    include/mfx_enctools_stub.h
    include/mfx_loader_utils.h
    src/mfx_enctools.cpp
    src/mfx_enctools_loader.cpp
  )

add_library(enctools_base STATIC ${sources})

target_include_directories(enctools_base
  PUBLIC
    include
    ${OPENCL_INCLUDE}
    ${VULKAN_INCLUDE}
  )
target_link_libraries(enctools_base
  PRIVATE 
    mfx_static_lib
    umc
  )

include(sources_ext.cmake OPTIONAL)
