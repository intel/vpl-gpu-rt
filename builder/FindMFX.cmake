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

if( Linux )
  set( os_arch "lin" )
elseif( Darwin )
  set( os_arch "darwin" )
elseif( Windows )
  set( os_arch "win" )
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set( os_arch "${os_arch}_x64" )
else()
  set( os_arch "${os_arch}_ia32" )
endif()

set( MFX_API_HOME ${MFX_API_HOME}/vpl)

unset( MFX_INCLUDE CACHE )
find_path( MFX_INCLUDE
  NAMES mfxdefs.h
  PATHS "${MFX_API_HOME}"
  PATH_SUFFIXES include
  NO_CMAKE_FIND_ROOT_PATH
)

include (${CMAKE_ROOT}/Modules/FindPackageHandleStandardArgs.cmake)
find_package_handle_standard_args(MFX REQUIRED_VARS MFX_INCLUDE)

set( MFX_API_HOME ${MFX_INCLUDE} )
if (NOT MFX_FOUND)
  message( FATAL_ERROR "Unknown API = ${API}")
endif()

function( get_mfx_version mfx_version_major mfx_version_minor )
  file(STRINGS ${MFX_API_HOME}/mfxdefs.h major REGEX "#define MFX_VERSION_MAJOR" LIMIT_COUNT 1)
  if(major STREQUAL "") # old style version
     file(STRINGS ${MFX_API_HOME}/mfxvideo.h major REGEX "#define MFX_VERSION_MAJOR")
  endif()
  file(STRINGS ${MFX_API_HOME}/mfxdefs.h minor REGEX "#define MFX_VERSION_MINOR" LIMIT_COUNT 1)
  if(minor STREQUAL "") # old style version
     file(STRINGS ${MFX_API_HOME}/mfxvideo.h minor REGEX "#define MFX_VERSION_MINOR")
  endif()
  string(REPLACE "#define MFX_VERSION_MAJOR " "" major ${major})
  string(REPLACE "#define MFX_VERSION_MINOR " "" minor ${minor})
  set(${mfx_version_major} ${major} PARENT_SCOPE)
  set(${mfx_version_minor} ${minor} PARENT_SCOPE)
endfunction()

# Potential source of confusion here. MFX_VERSION should contain API version i.e. 1025 for API 1.25, 
# Product version stored in MEDIA_VERSION_STR
get_mfx_version(major_vers minor_vers)

set( API_VERSION "${major_vers}.${minor_vers}")
set( MFX_VERSION_MAJOR ${major_vers})
set( MFX_VERSION_MINOR ${minor_vers})

set( API_USE_LATEST TRUE )

add_library( onevpl-api INTERFACE )
target_include_directories(onevpl-api
  INTERFACE
  ${MFX_API_HOME}
  ${MFX_API_HOME}/../mediasdk_structures
  ${MFX_API_HOME}/private
)
target_compile_definitions(onevpl-api
  INTERFACE MFX_ONEVPL
)

add_library( onevpl::api ALIAS onevpl-api )
set (MFX_API_TARGET onevpl::api)

message(STATUS "Enabling API ${major_vers}.${minor_vers} feature set with flags ${API_FLAGS}")
