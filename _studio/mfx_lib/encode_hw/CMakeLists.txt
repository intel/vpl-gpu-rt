add_library(encode_hw STATIC)

if (MFX_ENABLE_H264_VIDEO_ENCODE)
  target_include_directories(encode_hw PUBLIC
    h264/include
  )

  set(AVC_VIDEO_ENCODE_SRC
    h264/include/mfx_h264_encode_cm.h
    h264/include/mfx_h264_encode_cm_defs.h
    h264/include/mfx_h264_encode_hw.h
    h264/include/mfx_h264_encode_hw_utils.h

    h264/src/mfx_h264_encode_cm.cpp
    h264/src/mfx_h264_encode_hw.cpp
    h264/src/mfx_h264_encode_hw_utils.cpp
    h264/src/mfx_h264_encode_hw_utils_new.cpp



  )

  source_group("avc" FILES ${AVC_VIDEO_ENCODE_SRC})
endif()

if (MFX_ENABLE_H265_VIDEO_ENCODE)
  target_include_directories(encode_hw PUBLIC
    hevc
    hevc/agnostic
    hevc/agnostic/base
    hevc/agnostic/g12
    hevc/linux
    hevc/linux/g9
    hevc/linux/g12
    hevc/linux/base
  )

  set(HEVC_VIDEO_ENCODE_SRC
    hevc/hevcehw_disp.h
    hevc/agnostic/hevcehw_base.h
    hevc/agnostic/hevcehw_block_queues.h
    hevc/agnostic/hevcehw_ddi.h
    hevc/agnostic/hevcehw_decl_blocks.h
    hevc/agnostic/hevcehw_utils.h
    hevc/agnostic/base/hevcehw_base_data.h
    hevc/agnostic/base/hevcehw_base_dirty_rect.h
    hevc/agnostic/base/hevcehw_base_encoded_frame_info.h
    hevc/agnostic/base/hevcehw_base_enctools.cpp
    hevc/agnostic/base/hevcehw_base_enctools.h
    hevc/agnostic/base/hevcehw_base_ext_brc.h
    hevc/agnostic/base/hevcehw_base_hdr_sei.h
    hevc/agnostic/base/hevcehw_base_hrd.h
    hevc/agnostic/base/hevcehw_base_iddi.h
    hevc/agnostic/base/hevcehw_base_iddi_packer.h
    hevc/agnostic/base/hevcehw_base_interlace.h
    hevc/agnostic/base/hevcehw_base_legacy.h
    hevc/agnostic/base/hevcehw_base_max_frame_size.h
    hevc/agnostic/base/hevcehw_base_packer.h
    hevc/agnostic/base/hevcehw_base_parser.h
    hevc/agnostic/base/hevcehw_base_query_impl_desc.h
    hevc/agnostic/base/hevcehw_base_roi.h
    hevc/agnostic/base/hevcehw_base_task.h
    hevc/agnostic/base/hevcehw_base_weighted_prediction.h
    hevc/agnostic/base/hevcehw_base_alloc.h
    hevc/agnostic/base/hevcehw_base_constraints.h
    hevc/agnostic/base/hevcehw_base_scc.h
    hevc/agnostic/base/hevcehw_base_sao.h
    hevc/agnostic/base/hevcehw_base_rext.h
    hevc/agnostic/g12/hevcehw_g12_caps.h
    hevc/agnostic/g12/hevcehw_g12_data.h

    hevc/linux/base/hevcehw_base_encoded_frame_info_lin.h
    hevc/linux/base/hevcehw_base_interlace_lin.h
    hevc/linux/base/hevcehw_base_lin.h
    hevc/linux/base/hevcehw_base_roi_lin.h
    hevc/linux/base/hevcehw_base_va_lin.h
    hevc/linux/base/hevcehw_base_va_packer_lin.h
    hevc/linux/base/hevcehw_base_weighted_prediction_lin.h
    hevc/linux/base/hevcehw_base_max_frame_size_lin.h
    hevc/linux/base/hevcehw_base_dirty_rect_lin.h
    hevc/linux/base/hevcehw_base_rext_lin.h
    hevc/linux/base/hevcehw_base_scc_lin.h
    hevc/linux/base/hevcehw_base_qp_modulation_lin.h
    hevc/linux/g12/hevcehw_g12_caps_lin.h
    hevc/linux/g12/hevcehw_g12_lin.h

    hevc/hevcehw_disp.cpp
    hevc/agnostic/hevcehw_base.cpp
    hevc/agnostic/base/hevcehw_base_alloc.cpp
    hevc/agnostic/base/hevcehw_base_constraints.cpp
    hevc/agnostic/base/hevcehw_base_dirty_rect.cpp
    hevc/agnostic/base/hevcehw_base_encoded_frame_info.cpp
    hevc/agnostic/base/hevcehw_base_ext_brc.cpp
    hevc/agnostic/base/hevcehw_base_hdr_sei.cpp
    hevc/agnostic/base/hevcehw_base_hrd.cpp
    hevc/agnostic/base/hevcehw_base_interlace.cpp
    hevc/agnostic/base/hevcehw_base_impl.cpp
    hevc/agnostic/base/hevcehw_base_legacy.cpp
    hevc/agnostic/base/hevcehw_base_legacy_defaults.cpp
    hevc/agnostic/base/hevcehw_base_max_frame_size.cpp
    hevc/agnostic/base/hevcehw_base_packer.cpp
    hevc/agnostic/base/hevcehw_base_parser.cpp
    hevc/agnostic/base/hevcehw_base_recon_info.cpp
    hevc/agnostic/base/hevcehw_base_query_impl_desc.cpp
    hevc/agnostic/base/hevcehw_base_roi.cpp
    hevc/agnostic/base/hevcehw_base_task.cpp
    hevc/agnostic/base/hevcehw_base_weighted_prediction.cpp
    hevc/agnostic/base/hevcehw_base_rext.cpp
    hevc/agnostic/base/hevcehw_base_scc.cpp
    hevc/agnostic/g12/hevcehw_g12_caps.cpp

    hevc/linux/base/hevcehw_base_interlace_lin.cpp
    hevc/linux/base/hevcehw_base_lin.cpp
    hevc/linux/base/hevcehw_base_roi_lin.cpp
    hevc/linux/base/hevcehw_base_va_lin.cpp
    hevc/linux/base/hevcehw_base_va_packer_lin.cpp
    hevc/linux/base/hevcehw_base_weighted_prediction_lin.cpp
    hevc/linux/base/hevcehw_base_max_frame_size_lin.cpp
    hevc/linux/base/hevcehw_base_dirty_rect_lin.cpp
    hevc/linux/base/hevcehw_base_rext_lin.cpp
    hevc/linux/base/hevcehw_base_qp_modulation_lin.cpp
    hevc/linux/g12/hevcehw_g12_lin.cpp
  )

  source_group("hevc" FILES ${HEVC_VIDEO_ENCODE_SRC})
endif()

if (MFX_ENABLE_MPEG2_VIDEO_ENCODE)
  target_include_directories(encode_hw PUBLIC
    mpeg2/include
    ${MSDK_UMC_ROOT}/codec/mpeg2_enc/include
  )

  set(MPEG2_VIDEO_ENCODE_SRC
    mpeg2/include/mfx_mpeg2_encode_debug_hw.h
    mpeg2/include/mfx_mpeg2_encode_full_hw.h
    mpeg2/include/mfx_mpeg2_encode_hw.h
    mpeg2/include/mfx_mpeg2_encode_utils_hw.h

    mpeg2/src/mfx_mpeg2_encode_debug_hw.cpp
    mpeg2/src/mfx_mpeg2_encode_full_hw.cpp
    mpeg2/src/mfx_mpeg2_encode_hw.cpp
    mpeg2/src/mfx_mpeg2_encode_utils_hw.cpp
  )

  source_group("mpeg2" FILES ${MPEG2_VIDEO_ENCODE_SRC})
endif()

if (MFX_ENABLE_MJPEG_VIDEO_ENCODE)
  target_include_directories(encode_hw PUBLIC
    mjpeg/include
    ${MSDK_UMC_ROOT}/codec/jpeg_common/include
  )

  set(MJPEG_VIDEO_ENCODE_SRC
    mjpeg/include/mfx_mjpeg_encode_hw.h
    mjpeg/include/mfx_mjpeg_encode_hw_utils.h
    mjpeg/include/mfx_mjpeg_encode_interface.h
    mjpeg/include/mfx_mjpeg_encode_vaapi.h

    mjpeg/src/mfx_mjpeg_encode_factory.cpp
    mjpeg/src/mfx_mjpeg_encode_hw.cpp
    mjpeg/src/mfx_mjpeg_encode_hw_utils.cpp
    mjpeg/src/mfx_mjpeg_encode_vaapi.cpp
  )

  source_group("mjpeg" FILES ${MJPEG_VIDEO_ENCODE_SRC})
endif()

if (MFX_ENABLE_VP9_VIDEO_ENCODE)
  target_include_directories(encode_hw PUBLIC
    vp9/include
  )

  set(VP9_VIDEO_ENCODE_SRC
    vp9/include/mfx_platform_defs.h
    vp9/include/mfx_vp9_encode_hw.h
    vp9/include/mfx_vp9_encode_hw_par.h
    vp9/include/mfx_vp9_encode_hw_utils.h
    vp9/include/mfx_vp9_encode_hw_vaapi.h
    vp9/include/mfx_vp9_encode_hw_ddi.h

    vp9/src/mfx_vp9_encode_hw.cpp
    vp9/src/mfx_vp9_encode_hw_ddi.cpp
    vp9/src/mfx_vp9_encode_hw_par.cpp
    vp9/src/mfx_vp9_encode_hw_utils.cpp
    vp9/src/mfx_vp9_encode_hw_vaapi.cpp
  )

  source_group("vp9" FILES ${VP9_VIDEO_ENCODE_SRC})
endif()

target_sources(encode_hw PRIVATE
  ${AVC_VIDEO_ENCODE_SRC}
  ${HEVC_VIDEO_ENCODE_SRC}
  ${MPEG2_VIDEO_ENCODE_SRC}
  ${MJPEG_VIDEO_ENCODE_SRC}
  ${VP9_VIDEO_ENCODE_SRC}

  shared/ehw_resources_pool.h
  shared/ehw_task_manager.h

  shared/ehw_resources_pool.cpp
  shared/ehw_task_manager.cpp
  shared/ehw_device_vaapi.cpp
  shared/ehw_utils_vaapi.cpp
)

target_include_directories(encode_hw
  PUBLIC
    ${MSDK_LIB_ROOT}/encode_hw/shared

    ${MSDK_STUDIO_ROOT}/shared/include

    ${MSDK_UMC_ROOT}/codec/brc/include
    ${MSDK_UMC_ROOT}/codec/color_space_converter/include
)

set(ENCTOOLS_LIB $<$<BOOL:${MFX_ENABLE_USER_ENCTOOLS}>:enctools_hw>)

target_link_libraries(encode_hw
  PUBLIC
    mfx_static_lib
    umc_va_hw
    asc
    mctf_hw
    mfx_ext
  PRIVATE
    mfx_sdl_properties
    bitrate_control
    ${ENCTOOLS_LIB}
  )


include(sources_ext.cmake OPTIONAL)
