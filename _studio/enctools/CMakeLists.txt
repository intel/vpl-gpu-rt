set(sources
    src/mfx_enctools_brc.cpp
    src/mfx_enctools_common.cpp
    src/mfx_enctools_aenc.cpp
    src/mfx_enctools_utils.cpp
    src/mfx_enctools_lpla.cpp
    src/mfx_enctools_allocator.cpp
    src/mfxenctools_dl_int.cpp
    aenc/include/aenc.h
    include/mfx_ienctools.h
    include/mfx_enctools.h
    include/mfx_enctools_aenc.h
    include/mfx_enctools_brc.h
    include/mfx_enctools_defs.h
    include/mfx_enctools_lpla.h
    include/mfx_enctools_utils.h
    include/mfx_enctools_allocator.h
    include/mfxenctools_dl_int.h
    src/dll_main.cpp
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

function(make_enctools_name name)
  set (enctools_name enctools)
  if( CMAKE_SYSTEM_NAME MATCHES Windows )
    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
      set( enctools_name ${enctools_name}64 )
    else()
      set( enctools_name ${enctools_name}32 )
    endif()
  endif()
  set( ${name} ${enctools_name} PARENT_SCOPE)
endfunction()

make_enctools_name( enctools_name )

add_library(${enctools_name} SHARED ${sources})

set_target_properties(${enctools_name} PROPERTIES PREFIX "")

target_compile_definitions(${enctools_name}
  PRIVATE
    $<$<BOOL:${MFX_ENABLE_AENC}>:MFX_ENABLE_AENC>
  )

target_include_directories(${enctools_name}
  PUBLIC
    include
    aenc/include
    ${MSDK_STUDIO_ROOT}/shared/enctools/include
  )

target_link_libraries(${enctools_name}
  PUBLIC
    mfx_shared_lib
    fast_copy_sse4
    mfx_static_lib
    vm
  PRIVATE
    mfx_sdl_properties
    $<$<BOOL:${MFX_ENABLE_AENC}>:aenc>
  )

target_link_options(${enctools_name}
  PRIVATE
    $<$<PLATFORM_ID:Linux>:LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libenctools.map>
)
install(TARGETS ${enctools_name} LIBRARY DESTINATION ${MFX_MODULES_DIR}/libmfx-gen)

include(sources_ext.cmake OPTIONAL)

