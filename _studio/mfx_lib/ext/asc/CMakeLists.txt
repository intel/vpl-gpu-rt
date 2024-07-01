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

add_library(asc_ext STATIC
  include/asc_cm.h
  src/asc_cm.cpp
)

target_include_directories(asc_ext
  PUBLIC
    include
  PRIVATE
    ${MSDK_STUDIO_ROOT}/mfx_lib/ext/genx/asc/isa
)

target_sources(asc_ext
  PRIVATE
    ${MSDK_STUDIO_ROOT}/mfx_lib/ext/genx/asc/isa/genx_scd_gen12lp_isa.cpp
)

target_link_libraries(asc_ext
  PRIVATE
    cmrt_cross_platform_hw
    asc
)

include(sources_ext.cmake OPTIONAL)