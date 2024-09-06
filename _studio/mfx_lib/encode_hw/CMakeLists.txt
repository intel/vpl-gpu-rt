add_library(encode_hw STATIC)

include(functions.cmake)

if (MFX_ENABLE_H264_VIDEO_ENCODE)
  target_include_directories(encode_hw PUBLIC
    h264/include
  )

  set(AVC_VIDEO_ENCODE_SRC
    h264/include/mfx_h264_encode_hw.h
    h264/include/mfx_h264_encode_hw_utils.h
    h264/include/mfx_h264_enc_common_hw.h
    h264/include/mfx_h264_encode_interface.h
    h264/include/mfx_h264_encode_struct_vaapi.h
    h264/include/mfx_h264_encode_vaapi.h

    h264/src/mfx_h264_encode_hw.cpp
    h264/src/mfx_h264_encode_hw_utils.cpp
    h264/src/mfx_h264_encode_hw_utils_new.cpp
    h264/src/mfx_h264_enc_common_hw.cpp
    h264/src/mfx_h264_encode_factory.cpp
    h264/src/mfx_h264_encode_vaapi.cpp

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

    target_include_directories(encode_hw PUBLIC
      hevc/linux/xe_hpm
      hevc/linux/xe_lpm_plus
      hevc/linux/xe2
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
    hevc/agnostic/base/hevcehw_base_encode_stats.h
    hevc/agnostic/base/hevcehw_base_encoded_frame_info.h
    hevc/agnostic/base/hevcehw_base_enctools.cpp
    hevc/agnostic/base/hevcehw_base_enctools.h
    hevc/agnostic/base/hevcehw_base_enctools_com.cpp
    hevc/agnostic/base/hevcehw_base_enctools_com.h
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
    hevc/agnostic/base/hevcehw_base_extddi.h
    hevc/agnostic/base/hevcehw_base_caps.h
    hevc/agnostic/base/hevcehw_base_recon422.h
    hevc/agnostic/base/hevcehw_base_recon422_ext.h
    hevc/agnostic/g12/hevcehw_g12_caps.h
    hevc/agnostic/g12/hevcehw_g12_data.h

    hevc/linux/base/hevcehw_base_encoded_frame_info_lin.h
    hevc/linux/base/hevcehw_base_interlace_lin.h
    hevc/linux/base/hevcehw_base_lin.h
    hevc/linux/base/hevcehw_base_roi_lin.h
    hevc/linux/base/hevcehw_base_va_lin.h
    hevc/linux/base/hevcehw_base_va_packer_lin.h
    hevc/linux/base/hevcehw_base_enctools_qmatrix_lin.h
    hevc/linux/base/hevcehw_base_weighted_prediction_lin.h
    hevc/linux/base/hevcehw_base_max_frame_size_lin.h
    hevc/linux/base/hevcehw_base_dirty_rect_lin.h
    hevc/linux/base/hevcehw_base_rext_lin.h
    hevc/linux/base/hevcehw_base_scc_lin.h
    hevc/linux/base/hevcehw_base_qp_modulation_lin.h
    hevc/linux/base/hevcehw_base_caps_lin.h
    hevc/linux/g12/hevcehw_g12_caps_lin.h
    hevc/linux/g12/hevcehw_g12_lin.h

    hevc/hevcehw_disp.cpp
    hevc/agnostic/hevcehw_base.cpp
    hevc/agnostic/base/hevcehw_base_alloc.cpp
    hevc/agnostic/base/hevcehw_base_constraints.cpp
    hevc/agnostic/base/hevcehw_base_dirty_rect.cpp
    hevc/agnostic/base/hevcehw_base_encode_stats.cpp
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
    hevc/agnostic/base/hevcehw_base_extddi.cpp
    hevc/agnostic/base/hevcehw_base_caps.cpp
    hevc/agnostic/g12/hevcehw_g12_caps.cpp

    hevc/linux/base/hevcehw_base_interlace_lin.cpp
    hevc/linux/base/hevcehw_base_lin.cpp
    hevc/linux/base/hevcehw_base_roi_lin.cpp
    hevc/linux/base/hevcehw_base_va_lin.cpp
    hevc/linux/base/hevcehw_base_va_packer_lin.cpp
    hevc/linux/base/hevcehw_base_weighted_prediction_lin.cpp
    hevc/linux/base/hevcehw_base_enctools_qmatrix_lin.cpp
    hevc/linux/base/hevcehw_base_max_frame_size_lin.cpp
    hevc/linux/base/hevcehw_base_dirty_rect_lin.cpp
    hevc/linux/base/hevcehw_base_rext_lin.cpp
    hevc/linux/base/hevcehw_base_qp_modulation_lin.cpp
    hevc/linux/g12/hevcehw_g12_lin.cpp
  )

    set(HEVC_VIDEO_ENCODE_SRC
      ${HEVC_VIDEO_ENCODE_SRC}
      hevc/linux/xe_hpm/hevcehw_xe_hpm_lin.h
      hevc/linux/xe_hpm/hevcehw_xe_hpm_lin.cpp
      hevc/linux/xe_lpm_plus/hevcehw_xe_lpm_plus_lin.h
      hevc/linux/xe_lpm_plus/hevcehw_xe_lpm_plus_lin.cpp
      hevc/linux/xe2/hevcehw_xe2_lin.h
      hevc/linux/xe2/hevcehw_xe2_lin.cpp
    )

  custom_source_group("hevc" ${HEVC_VIDEO_ENCODE_SRC})
endif()

if (MFX_ENABLE_AV1_VIDEO_ENCODE)
  target_include_directories(encode_hw PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base
  )

    target_include_directories(encode_hw PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/Xe_HPM
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/Xe_HPM
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/Xe_LPM_plus
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/Xe_LPM_plus
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/Xe2
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/Xe2
    )

  set(AV1_VIDEO_ENCODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/av1ehw_disp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/av1eimplbase.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/av1ehw_disp.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/av1ehw_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/av1ehw_block_queues.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/av1ehw_ddi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/av1ehw_decl_blocks.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/av1ehw_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/av1ehw_base.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/av1ehw_struct_vaapi.h

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_lin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_va_lin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_va_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_va_packer_lin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_va_packer_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_qmatrix_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_max_frame_size_lin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_max_frame_size_lin.h

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_alloc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_alloc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_constraints.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_constraints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_data.h

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_qmatrix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_qmatrix.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_general.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_general.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_general_defaults.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_iddi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_iddi_packer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_packer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_packer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_query_impl_desc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_query_impl_desc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_segmentation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_segmentation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_task.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_tile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_tile.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_encoded_frame_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_encoded_frame_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_max_frame_size.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_max_frame_size.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_enctools.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_enctools.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_hdr.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_hdr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_enctools_com.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_enctools_com.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_scc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_scc.h
  )

    set(AV1_VIDEO_ENCODE_SRC
      ${AV1_VIDEO_ENCODE_SRC}
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/Xe_HPM/av1ehw_xe_hpm.h
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/Xe_HPM/av1ehw_xe_hpm_segmentation.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/Xe_HPM/av1ehw_xe_hpm_segmentation.h

      ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/Xe_HPM/av1ehw_xe_hpm_lin.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/Xe_HPM/av1ehw_xe_hpm_lin.h

      ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/Xe_LPM_plus/av1ehw_xe_lpm_plus_lin.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/Xe_LPM_plus/av1ehw_xe_lpm_plus_lin.h

      ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/Xe_LPM_plus/av1ehw_xe_lpm_plus.h

      ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/Xe2/av1ehw_xe2_lin.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/Xe2/av1ehw_xe2_lin.h

      ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/Xe2/av1ehw_xe2_scc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/Xe2/av1ehw_xe2_scc.h
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/Xe2/av1ehw_xe2_cdef.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/Xe2/av1ehw_xe2_cdef.h
    )

  custom_source_group("av1" ${AV1_VIDEO_ENCODE_SRC})
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
  ${AV1_VIDEO_ENCODE_SRC}
  ${MJPEG_VIDEO_ENCODE_SRC}
  ${VP9_VIDEO_ENCODE_SRC}

  shared/ehw_utils.h
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
    ${MSDK_STUDIO_ROOT}/shared/mfx_logging/include

    ${MSDK_UMC_ROOT}/codec/brc/include
    ${MSDK_UMC_ROOT}/codec/color_space_converter/include
    ${OPENCL_INCLUDE}
)

target_link_libraries(encode_hw
  PUBLIC
    mfx_static_lib
    umc_va_hw
    asc
    mfx_logging
    $<$<BOOL:${MFX_ENABLE_EXT}>:mfx_ext>
  PRIVATE
    mfx_sdl_properties
    bitrate_control
  )

if (MFX_ENABLE_ENCTOOLS OR MFX_ENABLE_HW_LPLA)
  target_include_directories(encode_hw
    PUBLIC
      ${MSDK_STUDIO_ROOT}/shared/enctools/include
    )
  target_link_libraries(encode_hw
    PRIVATE
      enctools_base
    )
endif()

include(sources_ext.cmake OPTIONAL)
