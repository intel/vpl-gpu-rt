# Copyright (c) 2017-2019 Intel Corporation
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

### UMC vm

add_subdirectory(core/vm)

### UMC vm plus

add_subdirectory(core/vm_plus)

### UMC core umc

add_subdirectory(core/umc)

### UMC VA (ex. umc_va_elk)
add_library(umc_va_hw STATIC)
set_property(TARGET umc_va_hw PROPERTY FOLDER "umc")

target_sources(umc_va_hw
  PRIVATE
    io/umc_va/include/umc_va.h
    io/umc_va/include/umc_va_linux.h
    io/umc_va/include/umc_va_video_processing.h
    io/umc_va/src/umc_va_linux.cpp
    io/umc_va/src/umc_va_video_processing.cpp
  )

target_compile_definitions(umc_va_hw
  PRIVATE
    ${API_FLAGS} ${WARNING_FLAGS}
  )
target_include_directories(umc_va_hw
  PUBLIC
    io/umc_va/include
    )
target_link_libraries(umc_va_hw
  PUBLIC
    umc
    $<$<PLATFORM_ID:Linux>:va>
  )

### UMC VA

### UMC codec brc

add_library(bitrate_control STATIC)
set_property(TARGET bitrate_control PROPERTY FOLDER "umc")

target_sources(bitrate_control
  PRIVATE
    codec/brc/include/umc_brc.h
    codec/brc/include/umc_h264_brc.h
    codec/brc/include/umc_mpeg2_brc.h

    codec/brc/include/umc_video_brc.h
    codec/brc/src/umc_brc.cpp

    codec/brc/src/umc_h264_brc.cpp
    codec/brc/src/umc_mpeg2_brc.cpp
    codec/brc/src/umc_video_brc.cpp
  )

target_include_directories(bitrate_control
  PUBLIC
    core/umc/include
    codec/brc/include
  PRIVATE
    codec/h264_enc/include
    )

target_link_libraries(bitrate_control PUBLIC umc)

### UMC codec brc

include(sources_ext.cmake OPTIONAL)
include(sources_tool_ext.cmake OPTIONAL)