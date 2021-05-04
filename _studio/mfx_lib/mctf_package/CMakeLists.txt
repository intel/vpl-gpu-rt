if( MFX_ENABLE_KERNELS )

  set(sources
    mctf/include/mctf_common.h
    mctf/src/mctf_common.cpp
    )

  add_library(mctf_hw STATIC ${sources})

  target_include_directories(mctf_hw PUBLIC mctf/include)

  target_compile_definitions(mctf_hw PRIVATE MFX_VA)

  target_link_libraries(mctf_hw
    PUBLIC 
      mfx_static_lib umc_va_hw cmrt_cross_platform_hw asc
    PRIVATE
      mfx_sdl_properties
      genx)

endif()
