# Copyright (c) 2020 Intel Corporation
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

add_library(vc1_common STATIC)
set_property(TARGET vc1_common PROPERTY FOLDER "umc")

target_sources(vc1_common
  PRIVATE
    include/umc_vc1_common.h
    include/umc_vc1_common_defs.h
    include/umc_vc1_common_macros_defs.h
    include/umc_vc1_common_tables.h
    include/umc_vc1_common_tables_adv.h
    include/umc_vc1_spl_frame_constr.h
    include/umc_vc1_spl_tbl.h

    src/umc_vc1_common.cpp
    src/umc_vc1_common_tables.cpp
    src/umc_vc1_common_tables_adv.cpp
    src/umc_vc1_spl_frame_constr.cpp
    src/umc_vc1_spl_tbl.cpp
  )

target_include_directories(vc1_common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

target_link_libraries(vc1_common PUBLIC vm_plus umc_va_hw)
