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

if(NOT DEFINED BUILDER_ROOT)
  set( BUILDER_ROOT ${CMAKE_HOME_DIRECTORY}/builder )
endif()

get_filename_component( MSDK_BUILD_ROOT_MINUS_ONE "${BUILDER_ROOT}/.." ABSOLUTE )

set( MSDK_STUDIO_ROOT  ${MSDK_BUILD_ROOT_MINUS_ONE}/_studio )
set( MSDK_TSUITE_ROOT  ${MSDK_BUILD_ROOT_MINUS_ONE}/_testsuite )
set( MSDK_LIB_ROOT     ${MSDK_STUDIO_ROOT}/mfx_lib )
set( MSDK_UMC_ROOT     ${MSDK_STUDIO_ROOT}/shared/umc )
set( MSDK_SAMPLES_ROOT ${MSDK_BUILD_ROOT_MINUS_ONE}/samples )
set( MSDK_TOOLS_ROOT   ${MSDK_BUILD_ROOT_MINUS_ONE}/tools )
set( MSDK_BUILDER_ROOT ${BUILDER_ROOT} )
set( MSDK_CMAKE_BINARY_ROOT ${CMAKE_CURRENT_BINARY_DIR} )

#================================================

add_library(mfx_static_lib INTERFACE)
target_include_directories(mfx_static_lib
  INTERFACE
    ${MFX_API_HOME}/include
    ${MSDK_STUDIO_ROOT}/shared/include
    ${MSDK_LIB_ROOT}/shared/include
    )

target_compile_definitions(mfx_static_lib
  INTERFACE
    ${API_FLAGS}
    MFX_DEPRECATED_OFF
    ${WARNING_FLAGS}
  )

target_link_libraries(mfx_static_lib
  INTERFACE
    mfx_common_properties
    $<$<PLATFORM_ID:Linux>:va>  #fixme: break down to real dependencies
  )

target_link_libraries(mfx_static_lib
  INTERFACE
    ipp
)

#================================================

add_library(mfx_shared_lib INTERFACE)
target_link_options(mfx_shared_lib
  INTERFACE
    # $<$<PLATFORM_ID:Windows>:/NODEFAULTLIB:libcmtd.lib>
    $<$<AND:$<PLATFORM_ID:Windows>,$<CONFIG:Debug>>:/NODEFAULTLIB:libcpmt.lib>
    $<$<PLATFORM_ID:Windows>:
      /DEBUG
      /PDB:$<TARGET_PDB_FILE_DIR:$<TARGET_PROPERTY:NAME>>/$<TARGET_PROPERTY:NAME>.pdb
    >
  )

  target_link_options(mfx_shared_lib
    INTERFACE
      $<$<PLATFORM_ID:Linux>:LINKER:--no-undefined,-z,relro,-z,now,-z,noexecstack>
    )

target_link_libraries(mfx_shared_lib
  INTERFACE
    $<$<PLATFORM_ID:Windows>:
      dxva2.lib
      D3D11.lib
      DXGI.lib
      d3d9.lib
      d3d12.lib
      >
    mfx_common_properties
    ${CMAKE_DL_LIBS}
  )

target_compile_definitions(mfx_shared_lib
  INTERFACE
    ${API_FLAGS}
  )

add_library(mfx_sdl_properties INTERFACE)

target_compile_options(mfx_sdl_properties
  INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:
      /WX
      /W4
      /sdl
      /wd5105  # macro expands to defined(smth)
      /wd4061  # not all enum members listed in case, but default found
      /wd4710  # no inline for inline function
      /wd4668  # replacing undefined macro with 0
      /wd4201  # unnamed unions in headers
      /wd4711  # inlining of non-marked function
      >
  )
#================================================

add_library(mfx_plugin_properties INTERFACE)

target_link_options(mfx_plugin_properties
  INTERFACE
    $<$<PLATFORM_ID:Windows>: LINKER:/DEF:${MSDK_STUDIO_ROOT}/mfx_lib/plugin/libmfxsw_plugin.def>
    $<$<PLATFORM_ID:Linux>: LINKER:--version-script=${MSDK_STUDIO_ROOT}/mfx_lib/plugin/libmfxsw_plugin.map>
  )

target_link_libraries(mfx_plugin_properties
  INTERFACE
    umc_va_hw
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

#================================================

add_library(mfx_va_properties INTERFACE) # va stands for video acceleration 

  target_link_options(mfx_va_properties
    INTERFACE
      $<$<PLATFORM_ID:Linux>:LINKER:--no-undefined,-z,relro,-z,now,-z,noexecstack>
    )

target_link_libraries(mfx_va_properties
  INTERFACE
    $<$<PLATFORM_ID:Linux>:va>
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

target_compile_definitions(mfx_va_properties
  INTERFACE
    MFX_VA
    $<$<PLATFORM_ID:Linux>:
      LIBVA_SUPPORT
      LIBVA_DRM_SUPPORT
    >
)


