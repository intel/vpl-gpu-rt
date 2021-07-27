set(sources
    enctools.cpp
    src/mfx_enctools_brc.cpp
    src/mfx_enctools_common.cpp
    src/mfx_enctools_aenc.cpp
    src/mfx_enctools_utils.cpp
    src/mfx_enctools_lpla.cpp
    src/mfx_enctools_allocator.cpp
    aenc/include/aenc.h
    include/mfx_enctools.h
    include/mfx_enctools_aenc.h
    include/mfx_enctools_brc.h
    include/mfx_enctools_defs.h
    include/mfx_enctools_lpla.h
    include/mfx_enctools_utils.h
    include/mfx_enctools_allocator.h
  )

if (MFX_ENABLE_AENC)
  find_library( AENC_LIBRARY libaenc.a PATHS ${CMAKE_HOME_DIRECTORY}/contrib/aenc/ )
  if(NOT ${AENC_LIBRARY} MATCHES NOTFOUND)
    message(STATUS "aenc library was found here ${AENC_LIBRARY}")

    add_library(aenc STATIC IMPORTED GLOBAL)
    set_target_properties(aenc PROPERTIES IMPORTED_LOCATION "${CMAKE_HOME_DIRECTORY}/contrib/aenc/libaenc.a")
    target_include_directories(aenc INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  else()
    message(STATUS "aenc library was not found (optional)! mfxlib will be built without aenc")
    set(MFX_ENABLE_AENC OFF)
  endif()
endif()

add_library(enctools_hw STATIC ${sources})

target_compile_definitions(enctools_hw
  PRIVATE 
    $<$<BOOL:${MFX_ENABLE_AENC}>:MFX_ENABLE_AENC>
  )

target_include_directories(enctools_hw
  PUBLIC
    include
    aenc/include
  )
target_link_libraries(enctools_hw
  PUBLIC 
    mfx_static_lib
    vm
  PRIVATE
    mfx_sdl_properties    
    $<$<BOOL:${MFX_ENABLE_AENC}>:aenc> 
  )

include(sources_ext.cmake OPTIONAL)
