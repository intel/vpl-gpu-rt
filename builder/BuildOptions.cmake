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

message( STATUS "Global Configuration of Targets" )

include(CMakeDependentOption)

option( MFX_ENABLE_KERNELS "Build with advanced media kernels support?" ON )
option( MFX_ENABLE_JPEG_SW_FALLBACK "Enabled software fallback for JPEG ?" ON )


option( ENABLE_OPENCL "Build targets dependent on OpenCL?" ON )

# -DENABLE_ALL will enable all the dependencies and features unless user did not
# explicitly switched some of them OFF, i.e. configuring in the following way
# is possible:
#   cmake -DENABLE_ALL=ON -DENABLE_TEXTLOG=OFF
# and it will configure all targets except samples.
#
# TODO: As of now ENABLE_ALL is not fully implemented, it will not affect all
#   of the ENABLE_* options. Those options it don't affect are placed above
#   ENABLE_ALL definition and require some rework and/or pending CI adoption.
#
option( ENABLE_ALL "Enable all dependencies and features?" OFF )

option( ENABLE_ITT "Build targets with ITT instrumentation support (requires VTune)?" ${ENABLE_ALL} )

option( ENABLE_TEXTLOG "Enable textlog tracing?" "${ENABLE_ALL}")
option( ENABLE_STAT "Enable stat tracing?" "${ENABLE_ALL}")

# -DBUILD_ALL will enable all the build targets unless user did not explicitly
# switched some targets OFF, i.e. configuring in the following way is possible:
#   cmake -DBUILD_ALL=ON -DBUILD_SAMPLES=OFF
# and it will configure all targets except samples.
option( BUILD_ALL "Build all the targets?" OFF )

option( BUILD_RUNTIME "Build mediasdk runtime (library, plugins, etc.)?" ON )
# Tools depend on samples (sample_common) and can't be built without it. The
# following BUILD_TOOLS option declaration assures that.
cmake_dependent_option( BUILD_TOOLS "Build tools?" ON "${BUILD_ALL}" OFF)

option(BUILD_TESTS "Build tests?" "${BUILD_ALL}")
option(USE_SYSTEM_GTEST "Use system installed gtest?" OFF)

option(BUILD_MOCK_TESTS "Build all mocks' self tests?" ${BUILD_TESTS} )

option(BUILD_TUTORIALS "Build tutorials?" "${BUILD_ALL}")

cmake_dependent_option(
  BUILD_KERNELS "Rebuild kernels (shaders)?" OFF
  "MFX_ENABLE_KERNELS" OFF)

if (BUILD_KERNELS)
  find_program(CMC cmc)
  if(NOT CMC)
    message(FATAL_ERROR "Failed to find cm compiler")
  endif()
  find_program(GENX_IR GenX_IR)
  if(NOT GENX_IR)
    message(FATAL_ERROR "Failed to find GenX_IR compiler (part of IGC)")
  endif()
endif()

option( MFX_ENABLE_AENC "Enabled AENC extension?" ON)

cmake_dependent_option(
  MFX_ENABLE_USER_DECODE "Enable H.264 (AVC) FEI?" ON
  "${MFX_ENABLE_H264_VIDEO_ENCODE};NOT ${API_USE_VPL}" OFF)

option( MFX_ENABLE_USER_DECODE "Enabled user decode plugins?" OFF)
option( MFX_ENABLE_USER_ENCODE "Enabled user encode plugins?" OFF)
option( MFX_ENABLE_USER_ENC "Enabled user ENC plugins?" OFF)
option( MFX_ENABLE_USER_VPP "Enabled user VPP plugins?" OFF)

option( MFX_ENABLE_AV1_VIDEO_DECODE "Enabled AV1 decoder?" ON)
option( MFX_ENABLE_VP8_VIDEO_DECODE "Enabled VP8 decoder?" ON)
option( MFX_ENABLE_VP9_VIDEO_DECODE "Enabled VP9 decoder?" ON)
option( MFX_ENABLE_H264_VIDEO_DECODE "Enabled AVC decoder?" ON)
option( MFX_ENABLE_H265_VIDEO_DECODE "Enabled HEVC decoder?" ON)
option( MFX_ENABLE_MPEG2_VIDEO_DECODE "Enabled MPEG2 decoder?" ON)
option( MFX_ENABLE_MPEG2_VIDEO_ENCODE "Enabled MPEG2 encoder?" ON)
option( MFX_ENABLE_MJPEG_VIDEO_DECODE "Enabled MJPEG decoder?" ON)
option( MFX_ENABLE_MJPEG_VIDEO_ENCODE "Enabled MJPEG encoder?" ON)
option( MFX_ENABLE_VC1_VIDEO_DECODE "Enabled VC1 decoder?" ON)

option( MFX_ENABLE_H264_VIDEO_ENCODE "Enable H.264 (AVC) encoder?" ON)
option( MFX_ENABLE_H264_VIDEO_FEI_ENCODE "Enable H.264 (AVC) FEI?" OFF)

option( MFX_ENABLE_H265_VIDEO_ENCODE "Enable H.265 (HEVC) encoder?" ON)
option( MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE "Enable H.265 (HEVC) FEI?" OFF)

option( MFX_ENABLE_VP9_VIDEO_ENCODE "Enable VP9 encoder?" ON)

option( MFX_ENABLE_ASC "Enable ASC support?"  ON )
option( MFX_ENABLE_VPP "Enabled Video Processing?" ON)

cmake_dependent_option(
  MFX_ENABLE_MCTF "Build with MCTF support?" ON
  "${MFX_ENABLE_ASC};${MFX_ENABLE_KERNELS};${MFX_ENABLE_VPP}" OFF)
cmake_dependent_option(
  MFX_ENABLE_ENCODE_MCTF "Build encoders with MCTF support?" ON
  "${MFX_ENABLE_ASC};${MFX_ENABLE_KERNELS}" OFF)

option( MFX_ENABLE_MVC_VIDEO_ENCODE "Enable MVC encoder?" OFF)

option( MFX_ENABLE_MFE             "Enable MFE?"  OFF )
option( MFX_ENABLE_OPAQUE_MEMORY   "Enable opaque memory?"  OFF )
option( MFX_ENABLE_USER_ENCTOOLS   "Enable encoding tools?" OFF )

option( MFX_ENABLE_H264_VIDEO_DECODE_STREAMOUT "Enable H.264 (AVC) Decode Streamout?" OFF)

# Now we will include config file which may overwrite default values of the
# options and options which user provided in a command line.
# It is critically important to include config file _after_ definition of
# all options. Otherwise rewrite of options in a config file will not take
# effect!
if (DEFINED MFX_CONFIG_FILE)
  # Include user provided cmake config file of the format:
  # set( VARIABLE VALUE )
  message ( "Loading user-supplied config file ${MFX_CONFIG_FILE}" )
  include(${MFX_CONFIG_FILE})
else()
  message ( "Loading VPL profile" )
  include(${BUILDER_ROOT}/profiles/onevpl.cmake)
endif()

configure_file(${BUILDER_ROOT}/mfxconfig.h.in ${MSDK_CMAKE_BINARY_ROOT}/mfxconfig.h)
add_definitions( -DMFX_HAVE_EXTERNAL_CONFIG )