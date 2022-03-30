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

add_library(cmrt_cross_platform_hw STATIC
  include/cmrt_cross_platform.h
  include/cmrt_utility.h
  include/cm_mem_copy.h
  include/cm_gpu_copy_code.h
  include/cmrt_vpp.h
  src/cmrt_cross_platform.cpp
  src/cmrt_utility.cpp
  src/cm_mem_copy.cpp
)

target_include_directories(cmrt_cross_platform_hw
  PUBLIC
    include
    ${MSDK_STUDIO_ROOT}/shared/asc/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/ext/genx/copy_kernels/isa
)

target_sources(cmrt_cross_platform_hw
  PRIVATE
    ${MSDK_STUDIO_ROOT}/mfx_lib/ext/genx/copy_kernels/isa/genx_copy_kernel_gen12lp_isa.cpp
)

target_link_libraries(cmrt_cross_platform_hw
  PUBLIC
    $<$<PLATFORM_ID:Windows>:cfgmgr32>
    mfx_static_lib
    vm
    mfx_logging
  PRIVATE
    mfx_sdl_properties
)

include(sources_ext.cmake OPTIONAL)