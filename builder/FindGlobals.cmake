# Copyright (c) 2012-2021 Intel Corporation
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

set_property( GLOBAL PROPERTY USE_FOLDERS ON )

# the following options should disable rpath in both build and install cases
set( CMAKE_INSTALL_RPATH "" )
set( CMAKE_BUILD_WITH_INSTALL_RPATH TRUE )
set( CMAKE_SKIP_BUILD_RPATH TRUE )


set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


collect_oses()

add_library(mfx_common_properties INTERFACE)
target_link_libraries(mfx_common_properties
  INTERFACE ${MFX_API_TARGET}
)

target_compile_definitions(mfx_common_properties
  INTERFACE
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

target_compile_definitions(mfx_common_properties
  INTERFACE
    $<$<CONFIG:Debug>:MFX_ENABLE_LOG_UTILITY>
)

if( Linux )
  # If user did not override CMAKE_INSTALL_PREFIX, then set the default prefix
  # to /opt/intel/mediasdk instead of cmake's default
  if( CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT )
    set( CMAKE_INSTALL_PREFIX /opt/intel/mediasdk CACHE PATH "Install Path Prefix" FORCE )
  endif()

  include( GNUInstallDirs )

  target_compile_definitions(mfx_common_properties
    INTERFACE
      __USE_LARGEFILE64
      _FILE_OFFSET_BITS=64
      LINUX
      LINUX32
      $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:LINUX64>
  )

  execute_process(
    COMMAND echo
    COMMAND cut -f 1 -d.
    COMMAND date "+.%-y.%-m.%-d"
    OUTPUT_VARIABLE cur_date
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  string( SUBSTRING ${MEDIA_VERSION_STR} 0 1 ver )

  set( git_commit "" )
  git_describe( git_commit )

  target_compile_definitions(mfx_common_properties
    INTERFACE
      MSDK_BUILD=\"$ENV{BUILD_NUMBER}\"
  )

  if (CMAKE_C_COMPILER_ID MATCHES Intel)
    set(no_warnings "-Wno-deprecated -Wno-unknown-pragmas -Wno-unused -wd2304")
  else()
    set(no_warnings "-Wno-deprecated-declarations -Wno-unknown-pragmas -Wno-unused")
  endif()

# set(c_warnings "-Wall -Wformat -Wformat-security")
# set(cxx_warnings "${c_warnings} -Wnon-virtual-dtor")

# set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -pipe -fPIC ${c_warnings} ${no_warnings}")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe -fPIC ${cxx_warnings} ${no_warnings}")
# append("-fPIE -pie" CMAKE_EXE_LINKER_FLAGS)

  set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE})
  set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE})
  set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_LIB_DIR}/${CMAKE_BUILD_TYPE})

elseif( Windows )
  if( CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT )
    set( CMAKE_INSTALL_PREFIX "C:/Program Files/mediasdk/" CACHE PATH "Install Path Prefix" FORCE )
  endif()

  set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE})
  set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE})
  set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_LIB_DIR}/${CMAKE_BUILD_TYPE})
  
  foreach(config_type ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config_type} config_type_capital)
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_type_capital} ${CMAKE_BIN_DIR}/${config_type})
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_type_capital} ${CMAKE_BIN_DIR}/${config_type})
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_type_capital} ${CMAKE_LIB_DIR}/${config_type})
  endforeach()

  target_compile_definitions(mfx_common_properties
    INTERFACE
      MFX_DX9ON11
      MFX_D3D11_ENABLED
      UNICODE
      _UNICODE
      NOMINMAX
  )
  target_compile_options(mfx_common_properties
    INTERFACE
      $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<BOOL:${MFX_ENABLE_SPECTRE_MITIGATIONS}>>:/Qspectre>
      $<$<CXX_COMPILER_ID:MSVC>:/EHa>
      /guard:cf
      /fp:precise
      /Zc:wchar_t
      /Zc:inline
      /Zc:forScope
      /Gm-
      /Zi
      /GS
      /Gy
  )
  target_link_options(mfx_common_properties
    INTERFACE
      /guard:cf
  )

endif( )

if( NOT DEFINED MFX_APPS_DIR)
  set( MFX_APPS_DIR ${CMAKE_INSTALL_FULL_DATADIR}/mfx )
endif()

set( MFX_SAMPLES_INSTALL_BIN_DIR ${MFX_APPS_DIR}/samples )
set( MFX_SAMPLES_INSTALL_LIB_DIR ${MFX_APPS_DIR}/samples )

set( MFX_TOOLS_INSTALL_BIN_DIR ${MFX_APPS_DIR}/tools )
set( MFX_TOOLS_INSTALL_LIB_DIR ${MFX_APPS_DIR}/tools )

if( NOT DEFINED MFX_PLUGINS_DIR )
  set( MFX_PLUGINS_DIR ${CMAKE_INSTALL_FULL_LIBDIR}/mfx )
endif( )

if( NOT DEFINED MFX_PLUGINS_CONF_DIR )
  set( MFX_PLUGINS_CONF_DIR ${CMAKE_INSTALL_FULL_DATADIR}/mfx )
endif( )

if( NOT DEFINED MFX_MODULES_DIR )
  set( MFX_MODULES_DIR ${CMAKE_INSTALL_FULL_LIBDIR} )
endif( )

message( STATUS "CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" )

# Some font definitions: colors, bold text, etc.
if(NOT Windows)
  string(ASCII 27 Esc)
  set(EndColor   "${Esc}[m")
  set(BoldColor  "${Esc}[1m")
  set(Red        "${Esc}[31m")
  set(BoldRed    "${Esc}[1;31m")
  set(Green      "${Esc}[32m")
  set(BoldGreen  "${Esc}[1;32m")
endif()

# Usage: report_targets( "Description for the following targets:" [targets] )
# Note: targets list is optional
function(report_targets description )
  message("")
  message("${ARGV0}")
  foreach(target ${ARGV1})
    message("  ${target}")
  endforeach()
  message("")
endfunction()

# Permits to accumulate strings in some variable for the delayed output
function(report_add_target var target)
  set(${ARGV0} ${${ARGV0}} ${ARGV1} CACHE INTERNAL "" FORCE)
endfunction()

# Defined OS name and version and build info and build commit
if( Linux )
  execute_process(
    COMMAND lsb_release -i
    OUTPUT_VARIABLE OUTPUT
    ERROR_VARIABLE ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT ERROR)
    string(REPLACE  "Distributor ID:	" ""  MFX_LINUX_NAME "${OUTPUT}")
    if(NOT "${OUTPUT}" STREQUAL "${MFX_LINUX_NAME}")
      set(MFX_SYSTEM "${MFX_LINUX_NAME}")
    endif()
  endif()

  execute_process(
    COMMAND getconf GNU_LIBC_VERSION
    OUTPUT_VARIABLE OUTPUT
    ERROR_VARIABLE ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT ERROR AND Linux)
    set(MFX_GLIBC ${OUTPUT})
  endif()
elseif( Windows )
  set(MFX_SYSTEM "Windows")
endif()

if( API_USE_LATEST )
  set(API_VER_MODIF "${API_VERSION}+")
else()
  set(API_VER_MODIF "${API_VERSION}")
endif()

if( MFX_SYSTEM )
  set( BUILD_INFO "${MFX_SYSTEM} | ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}" )
else()
  set( BUILD_INFO "${CMAKE_SYSTEM} ${CMAKE_SYSTEM_VERSION} | ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}" )
endif()

if(Linux AND MFX_GLIBC)
  set( BUILD_INFO "${BUILD_INFO} | ${MFX_GLIBC}")
endif()

git_describe( git_commit )

target_compile_definitions(mfx_common_properties
  INTERFACE
    ${API_FLAGS}
    ${WARNING_FLAGS}
    MFX_BUILD_INFO=\"${BUILD_INFO}\"
    MFX_API_VERSION=\"${API_VER_MODIF}\"
    MFX_GIT_COMMIT=\"${git_commit}\"
    MEDIA_VERSION_STR=\"${MEDIA_VERSION_STR}\"
    ONEVPL_EXPERIMENTAL
  )

if(NOT CMAKE_SYSTEM_NAME MATCHES Windows)
  target_compile_options(mfx_common_properties
    INTERFACE
      $<$<CXX_COMPILER_ID:Intel>: -static-intel
        -Wno-deprecated
        -Wno-unknown-pragmas
        -Wno-unused
      >
      $<$<NOT:$<CXX_COMPILER_ID:Intel>>:
        -Wno-deprecated-declarations
        -Wno-unknown-pragmas
        -Wno-unused
      >
  )
endif()


target_compile_definitions(mfx_common_properties
  INTERFACE
    $<$<BOOL:${ENABLE_TEXTLOG}>:MFX_TRACE_ENABLE_TEXTLOG>
    $<$<BOOL:${ENABLE_STAT}>:MFX_TRACE_ENABLE_STAT>
    $<$<BOOL:${ENABLE_ITT}>:MFX_TRACE_ENABLE_ITT>
  )

target_compile_definitions(mfx_common_properties
  INTERFACE
    SYNCHRONIZATION_BY_VA_SYNC_SURFACE
  )

target_include_directories(mfx_common_properties
  INTERFACE
    ${CMAKE_CURRENT_BINARY_DIR}
  )

add_library(mfx_require_ssse3_properties INTERFACE)

if (CMAKE_C_COMPILER_ID MATCHES Intel)
  target_compile_options(mfx_require_ssse3_properties
    INTERFACE
      $<$<PLATFORM_ID:Windows>: /arch:ssse3>
      $<$<PLATFORM_ID:Linux>:   -xssse3>
  )
else()
  target_compile_options(mfx_require_ssse3_properties
    INTERFACE
      # on Windows MSVC AVX includes SSSE3
      $<$<PLATFORM_ID:Windows>: /arch:AVX>
      $<$<PLATFORM_ID:Linux>:   -mssse3>
  )
endif()


add_library(mfx_require_sse4_properties INTERFACE)

if (CMAKE_C_COMPILER_ID MATCHES Intel)
  target_compile_options(mfx_require_sse4_properties
    INTERFACE
      $<$<PLATFORM_ID:Windows>: /arch:sse4.2>
      $<$<PLATFORM_ID:Linux>:   -xsse4.2>
    )
else()
  target_compile_options(mfx_require_sse4_properties
    INTERFACE
      # on Windows MSVC SSE4.2 supported by default
      $<$<PLATFORM_ID:Linux>:   -msse4.2>
    )
endif()

add_library(mfx_require_avx2_properties INTERFACE)

if (CMAKE_C_COMPILER_ID MATCHES Intel)
  target_compile_options(mfx_require_avx2_properties
    INTERFACE
      $<$<PLATFORM_ID:Windows>: /QxCORE-AVX2>
      $<$<PLATFORM_ID:Linux>:   -xCORE-AVX2>
    )
else()
  target_compile_options(mfx_require_avx2_properties
    INTERFACE
      $<$<PLATFORM_ID:Windows>: /arch:AVX2>
      $<$<PLATFORM_ID:Linux>:   -mavx2>
    )
endif()
