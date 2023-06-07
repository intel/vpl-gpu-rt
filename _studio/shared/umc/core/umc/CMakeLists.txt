add_library(umc STATIC)
set_property(TARGET umc PROPERTY FOLDER "umc")

target_sources(umc
  PRIVATE
    include/umc_base_codec.h
    include/umc_base_color_space_converter.h
    include/umc_defs.h
    include/umc_dynamic_cast.h
    include/umc_frame_allocator.h
    include/umc_frame_data.h
    include/umc_media_data.h
    include/umc_memory_allocator.h
    include/umc_structures.h
    include/umc_va_base.h
    include/umc_video_data.h
    include/umc_video_decoder.h
    include/umc_video_encoder.h

    src/umc_base_codec.cpp
    src/umc_frame_data.cpp
    src/umc_media_data.cpp
    src/umc_va_base.cpp
    src/umc_video_data.cpp
    src/umc_video_decoder.cpp
    src/umc_video_encoder.cpp
  )

target_include_directories(umc
  PUBLIC
    include

    ${MSDK_STUDIO_ROOT}/shared/mfx_logging/include
  )

target_link_libraries(umc
  PUBLIC
    mfx_static_lib
    vm
    vm_plus
    ${IPP_LIBS}
  PRIVATE
    mfx_sdl_properties
  )

include(sources_ext.cmake OPTIONAL)
