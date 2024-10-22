add_library(decode_hw STATIC)

set(UMC_CODECS ${MSDK_UMC_ROOT}/codec)

if (MFX_ENABLE_H264_VIDEO_DECODE)
  target_include_directories(decode_hw PUBLIC
    ${UMC_CODECS}/h264_dec/include
    $<$<BOOL:${MFX_ENABLE_PXP}>:${MSDK_LIB_ROOT}/pxp/include>
    h264/include
  )

  set(AVC_VIDEO_DECODE_SRC
    h264/include/mfx_h264_dec_decode.h

    h264/src/mfx_h264_dec_decode.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_au_splitter.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_dec_bitstream_headers.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_dec_debug.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_dec_defs_yuv.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_dec_mfx.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_dec_slice_decoder_decode_pic.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_frame.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_frame_list.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_heap.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_mfx_supplier.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_nal_spl.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_segment_decoder_dxva.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_slice_decoding.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_task_broker.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_task_supplier.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_va_packer.cpp
    ${UMC_CODECS}/h264_dec/src/umc_h264_va_supplier.cpp
  )

  source_group("avc" FILES ${AVC_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_H265_VIDEO_DECODE)
  target_include_directories(decode_hw PUBLIC
    ${UMC_CODECS}/h265_dec/include
    h265/include
  )

  set(HEVC_VIDEO_DECODE_SRC
    h265/include/mfx_h265_dec_decode.h

    h265/src/mfx_h265_dec_decode.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_au_splitter.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_bitstream_headers.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_debug.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_frame.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_frame_info.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_frame_list.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_heap.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_mfx_supplier.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_nal_spl.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_scaling_list.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_segment_decoder_dxva.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_sei.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_slice_decoding.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_tables.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_task_broker.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_task_supplier.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_va_packer.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_va_packer_vaapi.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_va_packer_ms.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_va_packer_intel.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_va_supplier.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_yuv.cpp
    ${UMC_CODECS}/h265_dec/src/umc_h265_mfx_utils.cpp
  )

  source_group("hevc" FILES ${HEVC_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_MJPEG_VIDEO_DECODE)
  target_include_directories(decode_hw PUBLIC
    ${UMC_CODECS}/jpeg_dec/include
    ${UMC_CODECS}/jpeg_common/include
    ${MSDK_LIB_ROOT}/vpp/include
    ${MSDK_UMC_ROOT}/codec/color_space_converter/include
    mjpeg/include
    ${OPENCL_INCLUDE}
    ${VULKAN_INCLUDE}
  )

  set(MJPEG_VIDEO_DECODE_SRC
    mjpeg/include/mfx_mjpeg_dec_decode.h
    mjpeg/src/mfx_mjpeg_dec_decode.cpp
    ${UMC_CODECS}/jpeg_common/src/bitstreamin.cpp
    ${UMC_CODECS}/jpeg_common/src/bitstreamout.cpp
    ${UMC_CODECS}/jpeg_common/src/colorcomp.cpp
    ${UMC_CODECS}/jpeg_common/src/jpegbase.cpp
    ${UMC_CODECS}/jpeg_common/src/membuffin.cpp
    ${UMC_CODECS}/jpeg_common/src/membuffout.cpp
    ${UMC_CODECS}/jpeg_dec/src/dechtbl.cpp
    ${UMC_CODECS}/jpeg_dec/src/decqtbl.cpp
    ${UMC_CODECS}/jpeg_dec/src/jpegdec.cpp
    ${UMC_CODECS}/jpeg_dec/src/jpegdec_base.cpp
    ${UMC_CODECS}/jpeg_dec/src/mfx_mjpeg_task.cpp
    ${UMC_CODECS}/jpeg_dec/src/umc_jpeg_frame_constructor.cpp
    ${UMC_CODECS}/jpeg_dec/src/umc_mjpeg_mfx_decode.cpp
    ${UMC_CODECS}/jpeg_dec/src/umc_mjpeg_mfx_decode_base.cpp
    ${UMC_CODECS}/jpeg_dec/src/umc_mjpeg_mfx_decode_hw.cpp

    ${UMC_CODECS}/color_space_converter/include/umc_color_space_conversion.h
    ${UMC_CODECS}/color_space_converter/include/umc_data_pointers_copy.h
    ${UMC_CODECS}/color_space_converter/include/umc_deinterlacing.h
    ${UMC_CODECS}/color_space_converter/include/umc_video_processing.h
    ${UMC_CODECS}/color_space_converter/include/umc_video_resizing.h
    ${UMC_CODECS}/color_space_converter/src/umc_video_processing.cpp
    ${UMC_CODECS}/color_space_converter/src/umc_video_resizing.cpp
    ${UMC_CODECS}/color_space_converter/src/umc_color_space_conversion.cpp
    ${UMC_CODECS}/color_space_converter/src/umc_deinterlacing.cpp
  )

  source_group("mjpeg" FILES ${MJPEG_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_MPEG2_VIDEO_DECODE)
  target_include_directories(decode_hw PUBLIC
    ${UMC_CODECS}/mpeg2_dec/include
    mpeg2/hw/include
  )

  set(MPEG2_VIDEO_DECODE_SRC
    mpeg2/hw/include/mfx_mpeg2_decode.h
    mpeg2/hw/src/mfx_mpeg2_decode.cpp
    ${UMC_CODECS}/mpeg2_dec/src/umc_mpeg2_bitstream.cpp
    ${UMC_CODECS}/mpeg2_dec/src/umc_mpeg2_decoder.cpp
    ${UMC_CODECS}/mpeg2_dec/src/umc_mpeg2_decoder_va.cpp
    ${UMC_CODECS}/mpeg2_dec/src/umc_mpeg2_frame.cpp
    ${UMC_CODECS}/mpeg2_dec/src/umc_mpeg2_slice.cpp
    ${UMC_CODECS}/mpeg2_dec/src/umc_mpeg2_splitter.cpp
    ${UMC_CODECS}/mpeg2_dec/src/umc_mpeg2_utils.cpp
    ${UMC_CODECS}/mpeg2_dec/src/umc_mpeg2_va_packer.cpp
  )

  source_group("mpeg2" FILES ${MPEG2_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_VC1_VIDEO_DECODE)
  target_include_directories(decode_hw PUBLIC
    ${UMC_CODECS}/vc1_common/include
    ${UMC_CODECS}/vc1_dec/include
    vc1/include
  )

  set(VC1_VIDEO_DECODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/vc1/src/mfx_vc1_decode.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common_tables.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_common_tables_adv.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_spl_frame_constr.cpp
    ${UMC_CODECS}/vc1_common/src/umc_vc1_spl_tbl.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_huffman.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_bitplane.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_bpic.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_bpic_adv.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_frame_descr.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_frame_va.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_ipic.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_ipic_adv.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_pic_com.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_pic_com_adv.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_ppic.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_ppic_adv.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_seq.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_skipping.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_task_store.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_dec_vopdq.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_video_decoder.cpp
    ${UMC_CODECS}/vc1_dec/src/umc_vc1_video_decoder_hw.cpp
  )

  source_group("vc1" FILES ${VC1_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_VP8_VIDEO_DECODE)
  target_include_directories(decode_hw PUBLIC
    vp8/include
  )

  set(VP8_VIDEO_DECODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/vp8/src/mfx_vp8_dec_decode_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vp8/src/mfx_vp8_dec_decode_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vp8/src/mfx_vp8_dec_decode_hw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vp8/src/mfx_vp8_dec_decode_hw_vaapi.cpp
  )

  source_group("vp8" FILES ${VP8_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_VP9_VIDEO_DECODE)
  target_include_directories(decode_hw PUBLIC
    ${UMC_CODECS}/vp9_dec/include
    vp9/include
  )

  set(VP9_VIDEO_DECODE_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/vp9/src/mfx_vp9_dec_decode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vp9/src/mfx_vp9_dec_decode_hw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vp9/src/mfx_vp9_dec_decode_utils.cpp
    ${UMC_CODECS}/vp9_dec/src/umc_vp9_bitstream.cpp
    ${UMC_CODECS}/vp9_dec/src/umc_vp9_utils.cpp
    ${UMC_CODECS}/vp9_dec/src/umc_vp9_va_packer.cpp
  )

  source_group("vp9" FILES ${VP9_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_AV1_VIDEO_DECODE)
  target_include_directories(decode_hw PUBLIC
    ${UMC_CODECS}/vp9_dec/include
    ${UMC_CODECS}/av1_dec/include
    av1/include
  )

  set(AV1_VIDEO_DECODE_SRC
    av1/src/mfx_av1_dec_decode.cpp
    ${UMC_CODECS}/av1_dec/src/umc_av1_bitstream.cpp
    ${UMC_CODECS}/av1_dec/src/umc_av1_decoder.cpp
    ${UMC_CODECS}/av1_dec/src/umc_av1_decoder_va.cpp
    ${UMC_CODECS}/av1_dec/src/umc_av1_frame.cpp
    ${UMC_CODECS}/av1_dec/src/umc_av1_utils.cpp
    ${UMC_CODECS}/av1_dec/src/umc_av1_va_packer_vaapi.cpp

    ${UMC_CODECS}/vp9_dec/src/umc_vp9_bitstream.cpp
  )

  source_group("av1" FILES ${AV1_VIDEO_DECODE_SRC})
endif()

if (MFX_ENABLE_VVC_VIDEO_DECODE)
  target_include_directories(decode_hw PUBLIC
    ${UMC_CODECS}/vvc_dec/include
    vvc/include
  )

  set(VVC_VIDEO_DECODE_SRC
    vvc/src/mfx_vvc_dec_decode.cpp
    ${UMC_CODECS}/vvc_dec/src/umc_vvc_bitstream_headers.cpp
    ${UMC_CODECS}/vvc_dec/src/umc_vvc_decoder.cpp
    ${UMC_CODECS}/vvc_dec/src/umc_vvc_decoder_va.cpp
    ${UMC_CODECS}/vvc_dec/src/umc_vvc_frame.cpp
    ${UMC_CODECS}/vvc_dec/src/umc_vvc_heap.cpp
    ${UMC_CODECS}/vvc_dec/src/umc_vvc_slice_decoding.cpp
    ${UMC_CODECS}/vvc_dec/src/umc_vvc_mfx_utils.cpp
    ${UMC_CODECS}/vvc_dec/src/umc_vvc_au_splitter.cpp
    ${UMC_CODECS}/vvc_dec/src/umc_vvc_va_packer_vaapi.cpp
  )

  source_group("vvc" FILES ${VVC_VIDEO_DECODE_SRC})
endif()

target_sources(decode_hw PRIVATE
  ${AVC_VIDEO_DECODE_SRC}
  ${HEVC_VIDEO_DECODE_SRC}
  ${MJPEG_VIDEO_DECODE_SRC}
  ${MPEG2_VIDEO_DECODE_SRC}
  ${VC1_VIDEO_DECODE_SRC}
  ${VP8_VIDEO_DECODE_SRC}
  ${VP9_VIDEO_DECODE_SRC}
  ${AV1_VIDEO_DECODE_SRC}
  ${VVC_VIDEO_DECODE_SRC}
)

target_include_directories(decode_hw
  PUBLIC
    ${MSDK_UMC_ROOT}/io/umc_va/include
)

target_link_libraries(decode_hw
  PUBLIC
    mfx_static_lib
    umc_va_hw
    $<$<BOOL:${MFX_ENABLE_PXP}>:pxp_hw>
  PRIVATE
    mfx_sdl_properties
    ${IPP_LIBS}
)

include(sources_ext.cmake OPTIONAL)