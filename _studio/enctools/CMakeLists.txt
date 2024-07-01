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


target_include_directories(${enctools_name}
  PUBLIC
    include
    aenc/include
    ${MSDK_STUDIO_ROOT}/shared/enctools/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/vpp/include #for prefilter
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
    vpp_hw_avx2
  )


target_link_options(${enctools_name}
  PRIVATE
    $<$<PLATFORM_ID:Linux>:LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libenctools.map>
)
install(TARGETS ${enctools_name} LIBRARY DESTINATION ${MFX_MODULES_DIR}/libmfx-gen)


if (MFX_ENABLE_AENC)
  set(sources
      aenc/src/aenc.cpp
      aenc/src/av1_asc_agop_tree.cpp
      aenc/src/hevc_asc_agop_tree.cpp
      aenc/src/hevc_asc_apq_tree.cpp
      aenc/src/av1_asc_tree.cpp
      aenc/src/av1_asc.cpp
      aenc/include/aenc++.h
      aenc/include/asc_cpu_detect.h
      aenc/include/av1_scd.h
  )

  add_library(aenc STATIC ${sources})

  target_include_directories(aenc
    PUBLIC
      aenc/include
  )

  target_link_libraries(aenc
    PRIVATE
      mfx_require_sse4_properties
      mfx_static_lib
      mfx_sdl_properties
    )
endif()

