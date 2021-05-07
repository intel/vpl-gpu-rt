set(common_sources
  include/dechtbl.h
  include/decqtbl.h
  include/jpegdec_base.h
  include/umc_jpeg_frame_constructor.h
  include/umc_mjpeg_mfx_decode_base.h
  src/dechtbl.cpp
  src/decqtbl.cpp
  src/jpegdec.cpp
  src/jpegdec_base.cpp
  src/mfx_mjpeg_task.cpp
  src/umc_jpeg_frame_constructor.cpp
  src/umc_mjpeg_mfx_decode.cpp
  src/umc_mjpeg_mfx_decode_base.cpp
  src/umc_mjpeg_mfx_decode_hw.cpp
)

add_library(jpeg_dec_hw STATIC)

target_include_directories(jpeg_dec_hw
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(jpeg_dec_hw
  PRIVATE
    include/umc_mjpeg_mfx_decode_hw.h
    src/umc_mjpeg_mfx_decode_hw.cpp
    ${common_sources}
)

target_link_libraries(jpeg_dec_hw
  PUBLIC
    mfx_static_lib
    jpeg_common
  PRIVATE
    mfx_sdl_properties
  )

set_property(TARGET jpeg_dec_hw PROPERTY FOLDER "umc")


add_library(jpeg_dec_sw STATIC)

target_include_directories(jpeg_dec_sw
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(jpeg_dec_sw
  PRIVATE
    ${common_sources}
)

target_link_libraries(jpeg_dec_sw
  PUBLIC
    mfx_static_lib
    jpeg_common
  PRIVATE
    mfx_sdl_properties
  )

set_property(TARGET jpeg_dec_sw PROPERTY FOLDER "umc")
