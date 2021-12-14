
add_library(cmrt_cross_platform_hw STATIC
  include/cmrt_cross_platform.h
  include/cmrt_utility.h
  src/cmrt_cross_platform.cpp
  src/cmrt_utility.cpp)

target_include_directories(cmrt_cross_platform_hw
  PUBLIC
    include

    ${MSDK_STUDIO_ROOT}/shared/mfx_logging/include
)

target_link_libraries(cmrt_cross_platform_hw
  PUBLIC
    $<$<PLATFORM_ID:Windows>:cfgmgr32>
    mfx_static_lib
    vm
    mfx_logging
  PRIVATE
    mfx_sdl_properties)
