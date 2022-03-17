
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
    src/mfx_vpp_jpeg.cpp
    )

add_library(vpp_hw STATIC ${sources})

target_include_directories(vpp_hw
  PUBLIC
    include)

target_link_libraries(vpp_hw
  PUBLIC
    mfx_static_lib umc_va_hw asc $<$<BOOL:${MFX_ENABLE_PXP}>:pxp_hw>
  PRIVATE
    mfx_sdl_properties
    $<$<BOOL:${MFX_ENABLE_EXT}>:mfx_ext>
  )

include(sources_ext.cmake OPTIONAL)