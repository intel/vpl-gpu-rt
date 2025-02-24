set(sources
    src/mfx_denoise_vpp.cpp
    src/mfx_detail_enhancement_vpp.cpp
    src/mfx_frame_rate_conversion_vpp.cpp
    src/mfx_perc_enc_vpp.cpp
    src/mfx_procamp_vpp.cpp
    src/mfx_vpp_factory.cpp
    src/mfx_vpp_hw.cpp
    src/mfx_vpp_main.cpp
    src/mfx_vpp_mvc.cpp
    src/mfx_vpp_sw_core.cpp
    src/mfx_vpp_sw_internal.cpp
    src/mfx_vpp_utils.cpp
    src/mfx_vpp_jpeg.cpp
    src/mfx_vpp_ai_frame_interpolation.cpp
    )

add_library(vpp_hw STATIC ${sources})

target_include_directories(vpp_hw
  PUBLIC
    include
    ${OPENCL_INCLUDE}
    ${VULKAN_INCLUDE}
    ${MSDK_STUDIO_ROOT}/mfx_lib/ext/ai_vfi
    )

target_link_libraries(vpp_hw
  PUBLIC
    mfx_static_lib umc_va_hw asc $<$<BOOL:${MFX_ENABLE_PXP}>:pxp_hw>
  PRIVATE
    vpp_hw_avx2
    mfx_sdl_properties
    $<$<BOOL:${MFX_ENABLE_EXT}>:mfx_ext>
    $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${MFX_ENABLE_EXT}>>:ai_vfi_ext>
  )

add_library(vpp_hw_avx2
  STATIC
    src/mfx_perc_enc_vpp_avx2.cpp)

target_include_directories(vpp_hw_avx2
  PUBLIC
    include)

target_link_libraries(vpp_hw_avx2
  PRIVATE
    mfx_require_avx2_properties
    mfx_static_lib
    mfx_sdl_properties
  )

include(sources_ext.cmake OPTIONAL)