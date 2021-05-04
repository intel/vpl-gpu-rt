
add_library(cmrt_cross_platform_hw STATIC
  include/cmrt_cross_platform.h
  src/cmrt_cross_platform.cpp)

target_include_directories(cmrt_cross_platform_hw PUBLIC include)

target_link_libraries(cmrt_cross_platform_hw
  PUBLIC
    mfx_static_lib
    vm
  PRIVATE
    mfx_sdl_properties)
