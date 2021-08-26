add_library(vm_plus STATIC)
set_property(TARGET vm_plus PROPERTY FOLDER "umc")

target_sources(vm_plus
  PRIVATE
    include/sys/mfx_vm++_pthread_unix.h
    include/umc_event.h
    include/umc_mutex.h
    include/umc_semaphore.h
    include/mfx_vm++_pthread.h

    src/mfx_vm++_pthread.cpp
    src/mfx_vm++_pthread_unix.cpp
    src/umc_event.cpp
    src/umc_semaphore.cpp
  )


target_include_directories(vm_plus
  PUBLIC
    include
)
target_link_libraries(vm_plus
  PUBLIC
    vm
    umc
    mfx_static_lib
  PRIVATE
    mfx_sdl_properties
  )

include(sources_ext.cmake OPTIONAL)