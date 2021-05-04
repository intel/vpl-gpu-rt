add_library(vc1_dec STATIC)

target_include_directories(vc1_dec
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(vc1_dec
  PRIVATE
    src/umc_vc1_dec_bitplane.cpp
    src/umc_vc1_dec_bpic.cpp
    src/umc_vc1_dec_bpic_adv.cpp
    src/umc_vc1_dec_frame_descr.cpp
    src/umc_vc1_dec_frame_va.cpp
    src/umc_vc1_dec_ipic.cpp
    src/umc_vc1_dec_ipic_adv.cpp
    src/umc_vc1_dec_pic_com.cpp
    src/umc_vc1_dec_pic_com_adv.cpp
    src/umc_vc1_dec_ppic.cpp
    src/umc_vc1_dec_ppic_adv.cpp
    src/umc_vc1_dec_seq.cpp
    src/umc_vc1_dec_skipping.cpp
    src/umc_vc1_dec_task_store.cpp
    src/umc_vc1_dec_vopdq.cpp
    src/umc_vc1_huffman.cpp
    src/umc_vc1_video_decoder.cpp

    include/umc_vc1_dec_debug.h
    include/umc_vc1_dec_exception.h
    include/umc_vc1_dec_frame_descr.h
    include/umc_vc1_dec_frame_descr_va.h
    include/umc_vc1_dec_seq.h
    include/umc_vc1_dec_skipping.h
    include/umc_vc1_dec_task.h
    include/umc_vc1_dec_task_store.h
    include/umc_vc1_dec_va_defs.h
    include/umc_vc1_huffman.h
    include/umc_vc1_video_decoder.h
)

target_link_libraries(vc1_dec
  PUBLIC
    mfx_static_lib
    color_space_converter
    vc1_common
  PRIVATE
    mfx_sdl_properties
  )

set_property(TARGET vc1_dec PROPERTY FOLDER "umc")
