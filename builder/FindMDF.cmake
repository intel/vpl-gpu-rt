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


if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set( mdf_arch x64 )
  set( mdf_lib_suffix 64 )
else()
  set( mdf_arch x86)
  set( mdf_lib_suffix 86 )
endif()

set( mdf_root $ENV{MFX_MDF_PATH} )
if( NOT EXISTS ${mdf_root} )
  if( Windows )
    message( STATUS "!!! There is no MFX_MDF_PATH, looking into $ENV{MEDIASDK_ROOT}/cmrt" )
    set( mdf_root $ENV{MEDIASDK_ROOT}/cmrt )
  else()
    message( STATUS "!!! There is no MFX_MDF_PATH, looking into $ENV{MEDIASDK_ROOT}/tools/linuxem64t/mdf" )
    set( mdf_root $ENV{MEDIASDK_ROOT}/tools/linuxem64t/mdf )
  endif()
endif()

find_path( CMRT_INCLUDE cm_rt.h PATHS ${mdf_root}/runtime/include )
find_path( CMDF_INCLUDE cm_def.h PATHS ${mdf_root}/compiler/include ${mdf_root}/compiler/include/cm )

if(NOT CMRT_INCLUDE MATCHES NOTFOUND AND
   NOT CMDF_INCLUDE MATCHES NOTFOUND)
  set( MDF_FOUND TRUE )

  add_library(MDF::cmrt STATIC IMPORTED)
  set_target_properties(MDF::cmrt PROPERTIES IMPORTED_LOCATION "${ipp_root}/lib/ippcoremt.lib")
  target_include_directories(MDF::cmrt 
    INTERFACE
      ${mdf_root}/runtime/include
      ${mdf_root}/compiler/include/cm
    )

  target_link_libraries(MDF::cmrt 
    INTERFACE
      ${mdf_root}/runtime/lib/${mdf_arch}
    )
endif()

if(NOT DEFINED MDF_FOUND)
  message( FATAL_ERROR "Intel(R) MDF was not found (required)! Set/check MFX_MDF_PATH environment variable!" )
else ()
  message( STATUS "Intel(R) MDF was found in ${mdf_root}" )
endif()

