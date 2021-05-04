add_library(jpeg_common STATIC)

target_include_directories(jpeg_common
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(jpeg_common
  PRIVATE
    include/basestream.h
    include/basestreamin.h
    include/basestreamout.h
    include/bitstreamin.h
    include/bitstreamout.h
    include/colorcomp.h
    include/jpegbase.h
    include/membuffin.h
    include/membuffout.h
    src/bitstreamin.cpp
    src/bitstreamout.cpp
    src/colorcomp.cpp
    src/jpegbase.cpp
    src/membuffin.cpp
    src/membuffout.cpp
)

target_link_libraries(jpeg_common
  PUBLIC
    mfx_static_lib
    umc_va_hw
  PRIVATE
    mfx_sdl_properties
  )

set_property(TARGET jpeg_common PROPERTY FOLDER "umc")
