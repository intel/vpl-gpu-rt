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

add_subdirectory(shared/mfx_trace)
add_subdirectory(shared/IntelMediaSDK)
add_subdirectory(shared/mfx_logging)
add_subdirectory(shared/umc)
add_subdirectory(shared/asc)
add_subdirectory(mfx_lib/decode)
add_subdirectory(mfx_lib/encode_hw)
if (MFX_ENABLE_EXT)
  add_subdirectory(mfx_lib/ext)
endif()
add_subdirectory(mfx_lib/vpp)
add_subdirectory(mfx_lib/shared)
if (MFX_ENABLE_PXP)
  add_subdirectory(mfx_lib/pxp)
endif()
add_subdirectory(mfx_lib)
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/mfx_config_trace")
add_subdirectory(mfx_config_trace)
endif()

if (MFX_ENABLE_ENCTOOLS OR MFX_ENABLE_HW_LPLA)
  add_subdirectory(shared/enctools)
endif()

if (MFX_ENABLE_ENCTOOLS)
  add_subdirectory(enctools)
endif()

include(sources_ext.cmake OPTIONAL)
