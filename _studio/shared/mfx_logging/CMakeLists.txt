add_library(mfx_logging STATIC)

target_sources(mfx_logging 
  PRIVATE
    include/mfx_utils_logging.h
    include/mfx_utils_perf.h
    include/mfx_unified_decode_logging.h
    include/mfx_unified_h265d_logging.h
    include/mfx_unified_h264d_logging.h
    include/mfx_unified_av1d_logging.h
    include/mfx_unified_vp9d_logging.h
    include/mfx_decode_dpb_logging.h
    src/mfx_utils_logging.cpp
    src/mfx_utils_perf.cpp
    src/mfx_unified_decode_logging.cpp
    src/mfx_unified_h265d_logging.cpp
    src/mfx_unified_h264d_logging.cpp
    src/mfx_unified_av1d_logging.cpp
    src/mfx_unified_vp9d_logging.cpp
    src/mfx_decode_dpb_logging.cpp
  )

target_include_directories(mfx_logging 
  PUBLIC
    include
    ${MFX_API_HOME}/mediasdk_structures
    ${MSDK_STUDIO_ROOT}/shared/umc/codec/h265_dec/include
    ${MSDK_STUDIO_ROOT}/shared/umc/codec/h264_dec/include
    ${MSDK_STUDIO_ROOT}/shared/umc/codec/av1_dec/include
    ${MSDK_STUDIO_ROOT}/shared/umc/codec/vp9_dec/include
    ${MSDK_STUDIO_ROOT}/shared/umc/io/umc_va/include
  )

target_link_libraries(mfx_logging
  PUBLIC
    mfx_static_lib
    vm
  )
