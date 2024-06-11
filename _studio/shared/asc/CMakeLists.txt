

add_library(asc_sse4 OBJECT
    include/asc_sse4_impl.h
    include/motion_estimation_engine.h
    src/asc_sse4_impl.cpp
    src/motion_estimation_engine.cpp
)
set_property(TARGET asc_sse4 PROPERTY FOLDER "optimization/asc")

target_include_directories(asc_sse4 PRIVATE include)

target_link_libraries(asc_sse4 PRIVATE 
    mfx_require_sse4_properties 
    mfx_static_lib
    mfx_sdl_properties)

#=====================================================================

add_library(asc_avx2 OBJECT
    include/asc_avx2_impl.h
    src/asc_avx2_impl.cpp
    )
set_property(TARGET asc_avx2 PROPERTY FOLDER "optimization/asc")

target_include_directories(asc_avx2 PRIVATE include)

target_link_libraries(asc_avx2 PRIVATE
    mfx_require_avx2_properties
    mfx_static_lib
    mfx_sdl_properties)

#=====================================================================

list( APPEND sources

    include/asc_c_impl.h
    include/asc_common_impl.h
    include/asc_cpu_dispatcher.h
    include/asc_defs.h
    include/asc.h
    include/asc_structures.h
    include/cpu_detect.h
    include/iofunctions.h
    include/me_asc.h
    include/tree.h

    src/asc.cpp
    src/asc_c_impl.cpp
    src/iofunctions.cpp
    src/tree.cpp

    $<TARGET_OBJECTS:asc_sse4>
    $<TARGET_OBJECTS:asc_avx2>
  )

add_library(asc STATIC ${sources})

target_include_directories(asc PUBLIC include)

target_link_libraries(asc
  PUBLIC
    mfx_static_lib
  PRIVATE
    mfx_sdl_properties
    umc
  )
