# Copyright (c) 2017-2021 Intel Corporation
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

cmake_minimum_required( VERSION 3.14 )

cmake_policy(SET CMP0048 NEW)   # version handled by project()
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.15)
cmake_policy(SET CMP0092 NEW)   # we handle warning level programmaticaly
endif()


# Througout this project MFX_VERSION refers to uAPI version and MEDIA_VERSION refers to product version
set(MEDIA_VERSION 25.2.2) # auto-update
set(MEDIA_VERSION_STR "${MEDIA_VERSION}${MEDIA_VERSION_EXTRA}" CACHE STRING "" FORCE)

set(LANG CXX C ASM)
project(mfxgen1
  VERSION ${MEDIA_VERSION}
  LANGUAGES ${LANG})

include(CMakeDependentOption)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to `release` build if nothing was specified
if((NOT CMAKE_BUILD_TYPE) AND (CMAKE_SYSTEM_NAME MATCHES Linux))
  message(STATUS "No build type selected, default to release")
  set(CMAKE_BUILD_TYPE release)
endif()

set( BUILDER_ROOT "${CMAKE_HOME_DIRECTORY}/builder" )

if(DEFINED API)
  message(STATUS "Ignoring specified API: $API in favor or version in mfxdefs.h")
  set( API "" )
  set( MFX_API_HOME "${MFX_HOME}/api")
endif()

if(NOT DEFINED ENV{MFX_HOME} )
  set( MFX_HOME "${CMAKE_HOME_DIRECTORY}" )
  set( MFX_API_HOME "${CMAKE_HOME_DIRECTORY}/api" )
else()
  set( MFX_HOME $ENV{MFX_HOME} )
  set( MFX_API_HOME "${MFX_HOME}/api")
endif()

include( ${BUILDER_ROOT}/FindMFX.cmake )

include( ${BUILDER_ROOT}/FindInternals.cmake )
include( ${BUILDER_ROOT}/FindOpenCL.cmake )
include( ${BUILDER_ROOT}/FindFunctions.cmake )
include( ${BUILDER_ROOT}/BuildOptions.cmake )
include( ${BUILDER_ROOT}/FindGlobals.cmake )
include( ${BUILDER_ROOT}/FindITT.cmake )
include( ${BUILDER_ROOT}/FindIPP.cmake OPTIONAL)
include( ${BUILDER_ROOT}/FindPackages.cmake )

if (MFX_ENABLE_PXP)
  add_definitions(-DMFX_ENABLE_PXP_EXT)
endif()

if (BUILD_RUNTIME)
  add_subdirectory(${CMAKE_HOME_DIRECTORY}/contrib/ipp)
  add_subdirectory(${CMAKE_HOME_DIRECTORY}/_studio)
endif()


if (ENABLE_ITT)
  target_link_libraries(mfx_common_properties INTERFACE itt)
endif()

message("")
message("********************** Intel VPL GPU Runtime Configuration Summary **********************")
message("Intel VPL:")
message("  API (request to use this version)       : ${API}")
message("  API_VERSION                             : ${API_VERSION}")
message("  API_FLAGS                               : ${API_FLAGS}")
message("  MFX_CONFIG_FILE                         : ${MFX_CONFIG_FILE}")
message("  See detailed per-component mediasdk configuration in the auto-generated file:")
message("    ${MSDK_CMAKE_BINARY_ROOT}/mfx_features.h")
message("Cmake:")
message("  CMAKE_VERSION                           : ${CMAKE_VERSION}")
message("  CMAKE_GENERATOR                         : ${CMAKE_GENERATOR}")
message("  CMAKE_BUILD_TOOL                        : ${CMAKE_BUILD_TOOL}")
message("Target:")
message("  CMAKE_SYSTEM_NAME                       : ${CMAKE_SYSTEM_NAME}")
message("  CMAKE_SYSTEM_VERSION                    : ${CMAKE_SYSTEM_VERSION}")
message("  CMAKE_SYSTEM_PROCESSOR                  : ${CMAKE_SYSTEM_PROCESSOR}")
message("General:")
message("  CMAKE_BUILD_TYPE                        : ${CMAKE_BUILD_TYPE}")
message("  CMAKE_TOOLCHAIN_FILE                    : ${CMAKE_TOOLCHAIN_FILE}")
message("  CMAKE_C_COMPILER                        : ${CMAKE_C_COMPILER}")
message("  CMAKE_CXX_COMPILER                      : ${CMAKE_CXX_COMPILER}")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  message("  Building architecture                   : 64-bit")
else()
  message("  Building architecture                   : 32-bit")
endif()
message("Flags:")
message("  CMAKE_C_FLAGS                           : ${CMAKE_C_FLAGS}")
message("  CMAKE_CXX_FLAGS                         : ${CMAKE_CXX_FLAGS}")
message("Release flags:")
message("  CMAKE_C_FLAGS_RELEASE                   : ${CMAKE_C_FLAGS_RELEASE}")
message("  CMAKE_CXX_FLAGS_RELEASE                 : ${CMAKE_CXX_FLAGS_RELEASE}")
message("Debug flags:")
message("  CMAKE_C_FLAGS_DEBUG                     : ${CMAKE_C_FLAGS_DEBUG}")
message("  CMAKE_CXX_FLAGS_DEBUG                   : ${CMAKE_CXX_FLAGS_DEBUG}")
message("Install:")
message("  CMAKE_INSTALL_PREFIX                    : ${CMAKE_INSTALL_PREFIX}")
if (BUILD_TOOLS)
  message("  CMAKE_INSTALL_FULL_BINDIR               : ${CMAKE_INSTALL_FULL_BINDIR}")
endif()
message("  CMAKE_INSTALL_FULL_INCLUDEDIR           : ${CMAKE_INSTALL_FULL_INCLUDEDIR}")
message("  CMAKE_INSTALL_FULL_LIBDIR               : ${CMAKE_INSTALL_FULL_LIBDIR}")
message("  CMAKE_INSTALL_FULL_DATADIR              : ${CMAKE_INSTALL_FULL_DATADIR}")
message("Enable:")
message("  ENABLE_ITT                              : ${ENABLE_ITT}")
message("  ENABLE_TEXTLOG                          : ${ENABLE_TEXTLOG}")
message("  ENABLE_STAT                             : ${ENABLE_STAT}")
message("  ENABLE_PXP                              : ${MFX_ENABLE_PXP}")
message("Build:")
message("  BUILD_RUNTIME                           : ${BUILD_RUNTIME}")
message("  BUILD_TESTS                             : ${BUILD_TESTS}")
message("  BUILD_MOCK_TESTS                        : ${BUILD_MOCK_TESTS}")
message("  BUILD_TOOLS                             : ${BUILD_TOOLS}")
message("  BUILD_KERNELS                           : ${BUILD_KERNELS}")
message("*****************************************************************************")

include(sources_ext.cmake OPTIONAL)
