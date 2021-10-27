
set(sources
    src/mfx_pxp_video_accelerator_vaapi.cpp
    src/mfx_pxp_video_accelerator_d3d11.cpp
    src/mfx_pxp_video_accelerator.cpp
  )

add_library(pxp_hw STATIC ${sources})

target_include_directories(pxp_hw
  PUBLIC
    ${MSDK_STUDIO_ROOT}/shared/include
    ${MSDK_LIB_ROOT}/pxp/include
    ${MSDK_UMC_ROOT}/io/umc_va/include
)

target_link_libraries(pxp_hw
  PUBLIC
    mfx_static_lib
    umc_va_hw
)
