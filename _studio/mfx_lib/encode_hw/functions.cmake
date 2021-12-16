function(custom_source_group GROUP_NAME)
  set(GROUP_FILES ${ARGN})

  if (WIN32)
    foreach(FILE ${GROUP_FILES})
      get_filename_component(PARENT_DIR "${FILE}" PATH)

      string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")
      string(REPLACE "/" "\\" GROUP "${GROUP}")

      source_group("${GROUP}" FILES "${FILE}")
    endforeach()
  else()
    source_group(${GROUP_NAME} FILES ${GROUP_FILES})
  endif()
endfunction()