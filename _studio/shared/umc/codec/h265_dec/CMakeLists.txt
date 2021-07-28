if( MFX_LIB_ONLY )
  return()
endif()

# ==================================== umc_h265_hw ====================================

add_library(umc_h265_hw STATIC)

target_include_directories(umc_h265_hw
  PRIVATE
    include
    ${MSDK_LIB_ROOT}/optimization/h264/include
    ${MSDK_LIB_ROOT}/optimization/h265/include
)

target_sources(umc_h265_hw
  PRIVATE
    src/umc_h265_au_splitter.cpp
    src/umc_h265_bitstream_headers.cpp
    src/umc_h265_debug.cpp
    src/umc_h265_frame.cpp
    src/umc_h265_frame_info.cpp
    src/umc_h265_frame_list.cpp
    src/umc_h265_heap.cpp
    src/umc_h265_mfx_supplier.cpp
    src/umc_h265_mfx_utils.cpp
    src/umc_h265_nal_spl.cpp
    src/umc_h265_scaling_list.cpp
    src/umc_h265_sei.cpp
    src/umc_h265_slice_decoding.cpp
    src/umc_h265_tables.cpp
    src/umc_h265_task_broker.cpp
    src/umc_h265_task_supplier.cpp
    src/umc_h265_va_packer.cpp
    src/umc_h265_va_packer_dxva.cpp
    src/umc_h265_va_packer_intel.cpp
    src/umc_h265_va_packer_ms.cpp
    src/umc_h265_va_packer_vaapi.cpp
    src/umc_h265_va_supplier.cpp
    src/umc_h265_yuv.cpp
  )

target_link_libraries(umc_h265_hw
  PUBLIC
    mfx_static_lib
    umc_va_hw
    vm
    vm_plus
  PRIVATE
    mfx_sdl_properties
  )
set_property(TARGET umc_h265_hw PROPERTY FOLDER "umc")

include(sources_ext.cmake OPTIONAL)