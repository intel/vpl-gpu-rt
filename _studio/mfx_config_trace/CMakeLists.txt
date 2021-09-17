add_library(mfx_config_trace STATIC mfx_config_trace.cpp)

target_link_libraries(mfx_config_trace
  PUBLIC 
    mfx_static_lib)