add_library(vm STATIC)
set_property(TARGET vm PROPERTY FOLDER "umc")

target_sources(vm
  PRIVATE
    include/sys/vm_types_linux32.h

    include/vm_debug.h
    include/vm_event.h
    include/vm_interlocked.h
    include/vm_semaphore.h
    include/vm_shared_object.h
    include/vm_strings.h
    include/vm_sys_info.h
    include/vm_time.h
    include/vm_types.h

    src/vm_event_linux32.c
    src/vm_interlocked_unix.c
    src/vm_semaphore_linux32.c
    src/vm_shared_object_linux32.c
    src/vm_strings_unix.c
    src/vm_sys_info_linux32.c
    src/vm_time_linux32.c
)

target_include_directories(vm
  PUBLIC
    include
)
target_link_libraries(vm
  PUBLIC
    umc
    mfx_static_lib
    ${IPP_LIBS}
  PRIVATE
    mfx_sdl_properties
    ${CMAKE_DL_LIBS}
  )

include(sources_ext.cmake OPTIONAL)
