# Copyright (c) 2018-2021 Intel Corporation
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

if(MFX_ENABLE_KERNELS)

  list( APPEND sources
    asc/isa/genx_scd_gen12lp_isa.cpp
  )

  list( APPEND sources
    copy_kernels/isa/genx_copy_kernel_gen12lp_isa.cpp
  )

  list( APPEND sources
    field_copy/isa/genx_fcopy_gen12lp_isa.cpp
  )

  list( APPEND sources
    h264_encode/isa/genx_histogram_gen12lp_isa.cpp
    h264_encode/isa/genx_simple_me_gen12lp_isa.cpp
    h264_encode/src/genx_simple_me_proto.cpp
  )

  list( APPEND sources
    mctf/isa/genx_mc_gen12lp_isa.cpp
    mctf/isa/genx_me_gen12lp_isa.cpp
    mctf/isa/genx_sd_gen12lp_isa.cpp
  )

  add_library(genx STATIC ${sources})
  set_property(TARGET genx PROPERTY FOLDER "kernels")

  target_include_directories(genx
    PUBLIC
      asc/isa
      copy_kernels/isa
      field_copy/isa
      h264_encode/isa
      mctf/isa
    )

  target_link_libraries(genx
    PUBLIC
      mfx_static_lib
    PRIVATE
      mfx_sdl_properties
    )
endif()

include(sources_ext.cmake OPTIONAL)