add_library(mfx_logging STATIC)

target_sources(mfx_logging 
  PRIVATE
    include/mfx_utils_logging.h
    src/mfx_utils_logging.cpp
  )

target_include_directories(mfx_logging 
  PUBLIC
    include
    ${MFX_API_HOME}/mediasdk_structures
  )

target_link_libraries(mfx_logging
  PUBLIC
    mfx_static_lib
    vm
  PRIVATE
    mfx_sdl_properties
  )
