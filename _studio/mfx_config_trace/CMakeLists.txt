add_library(mfx_config_trace STATIC mfx_config_trace.cpp)

target_link_libraries(mfx_config_trace
  PUBLIC 
    mfx_static_lib)

if (ENABLE_PRE_PROCESS)
    add_custom_target(
      pre-process
      COMMAND pip3 install regex
      COMMAND python3 ${MSDK_STUDIO_ROOT}/../config/scripts/sdm/traceFeature.py ${MSDK_STUDIO_ROOT}/.. ${CMAKE_BINARY_DIR}
    )

  add_dependencies(mfx_config_trace pre-process)
endif()
  