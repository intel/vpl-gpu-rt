add_library(mfx_common_hw STATIC)

target_sources(mfx_common_hw
  PRIVATE
    include/libmfxsw.h
    include/mediasdk_version.h
    include/mfx_brc_common.h
    include/mfx_common_decode_int.h
    include/mfx_common_int.h
    include/mfx_critical_error_handler.h
    include/mfx_ddi_enc_dump.h
    include/mfx_enc_common.h
    include/mfx_frames.h
    include/mfx_h264_enc_common_hw.h
    include/mfx_h264_encode_interface.h
    include/mfx_h264_encode_struct_vaapi.h
    include/mfx_h264_encode_vaapi.h
    include/mfx_interface.h
    include/mfx_interface_scheduler.h
    include/mfx_log.h
    include/mfx_mpeg2_dec_common.h
    include/mfx_mpeg2_enc_common.h
    include/mfx_mpeg2_enc_common_hw.h
    include/mfx_mpeg2_encode_interface.h
    include/mfx_mpeg2_encode_vaapi.h
    include/mfx_msg.h
    include/mfx_platform_headers.h
    include/mfx_session.h
    include/mfx_task.h
    include/mfx_task_threading_policy.h
    include/mfx_thread_task.h
    include/mfx_tools.h
    include/mfx_vc1_dec_common.h
    include/mfx_vpx_dec_common.h
    include/feature_blocks/mfx_feature_blocks_base.h

    src/mfx_feature_blocks_base.cpp
    src/mfx_brc_common.cpp
    src/mfx_common_decode_int.cpp
    src/mfx_common_int.cpp
    src/mfx_enc_common.cpp
    src/mfx_mpeg2_dec_common.cpp
    src/mfx_vc1_dec_common.cpp
    src/mfx_critical_error_handler.cpp
    src/mfx_ddi_enc_dump.cpp
    src/mfx_h264_enc_common_hw.cpp
    src/mfx_h264_encode_factory.cpp
    src/mfx_h264_encode_vaapi.cpp

    src/mfx_mpeg2_encode_vaapi.cpp
    src/mfx_mpeg2_encode_factory.cpp
    src/mfx_mpeg2_enc_common_hw.cpp
    src/mfx_vpx_dec_common.cpp
  )

target_include_directories(mfx_common_hw
  PUBLIC
    include
    include/feature_blocks
    ${MSDK_LIB_ROOT}/cmrt_cross_platform/include
    ${MSDK_LIB_ROOT}/encode_hw/h264/include
    ${MSDK_UMC_ROOT}/codec/brc/include
    ${MSDK_UMC_ROOT}/codec/vc1_common/include
    ${MSDK_UMC_ROOT}/codec/vc1_dec/include
    ${MSDK_UMC_ROOT}/codec/h264_enc/include
    ${MSDK_STUDIO_ROOT}/shared/asc/include
    ${MSDK_STUDIO_ROOT}/shared/include
  )

target_link_libraries(mfx_common_hw
  PUBLIC
    mfx_static_lib
    umc_va_hw
    mctf_hw
  PRIVATE
    mfx_sdl_properties
  )

include(sources_ext.cmake OPTIONAL)