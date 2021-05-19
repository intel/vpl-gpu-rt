
#==============MCTF========================================================

set(sources
    src/mfx_denoise_vpp.cpp
    src/mfx_detail_enhancement_vpp.cpp
    src/mfx_frame_rate_conversion_vpp.cpp
    src/mfx_procamp_vpp.cpp
    src/mfx_vpp_factory.cpp
    src/mfx_vpp_hw.cpp
    src/mfx_vpp_main.cpp
    src/mfx_vpp_mvc.cpp
    src/mfx_vpp_sw_core.cpp
    src/mfx_vpp_sw_internal.cpp
    src/mfx_vpp_utils.cpp
    src/mfx_vpp_jpeg_d3d.cpp
    )

add_library(vpp_hw STATIC ${sources})

target_compile_definitions(vpp_hw PRIVATE MFX_VA)

target_include_directories(vpp_hw
  PUBLIC
    include)

target_link_libraries(vpp_hw
  PUBLIC
    mfx_static_lib umc_va_hw cmrt_cross_platform_hw asc mctf_hw
  PRIVATE
    mfx_sdl_properties
    genx
    ipp
  )

include(sources_ext.cmake OPTIONAL)