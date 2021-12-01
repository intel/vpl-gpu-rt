
set(sources
    src/mfx_pxp_video_accelerator_vaapi.cpp
    src/mfx_pxp_video_accelerator_d3d11.cpp
    src/mfx_pxp_video_accelerator.cpp
    src/mfx_pxp_vpp_vaapi.cpp
    src/mfx_pxp_h264_supplier.cpp
    src/mfx_pxp_h264_nal_spl.cpp
  )

add_library(pxp_hw STATIC ${sources})

target_include_directories(pxp_hw
  PUBLIC
    ${MSDK_STUDIO_ROOT}/shared/include
    ${MSDK_LIB_ROOT}/pxp/include
    ${MSDK_UMC_ROOT}/io/umc_va/include
    ${MSDK_UMC_ROOT}/codec/h264_dec/include
)

target_link_libraries(pxp_hw
  PUBLIC
    mfx_static_lib
    umc_va_hw
)
