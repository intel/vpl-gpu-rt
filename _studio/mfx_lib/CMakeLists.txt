set(sources_common
    shared/src/libmfxsw.cpp
    shared/src/libmfxsw_async.cpp
    shared/src/libmfxsw_decode.cpp
    shared/src/libmfxsw_encode.cpp
    shared/src/libmfxsw_functions.cpp
    shared/src/libmfxsw_enc.cpp
    shared/src/libmfxsw_pak.cpp
    shared/src/libmfxsw_plugin.cpp
    shared/src/libmfxsw_query.cpp
    shared/src/libmfxsw_session.cpp
    shared/src/libmfxsw_vpp.cpp
    shared/src/libmfxsw_decode_vp.cpp
    shared/src/mfx_session.cpp

    shared/include/feature_blocks/mfx_feature_blocks_base.h
    shared/include/feature_blocks/mfx_feature_blocks_decl_blocks.h
    shared/include/feature_blocks/mfx_feature_blocks_init_macros.h
    shared/include/feature_blocks/mfx_feature_blocks_utils.h
    shared/include/mfx_utils_extbuf.h
    ${MSDK_CMAKE_BINARY_ROOT}/mfx_features.h  # Cmake-generated configuration derived from BuildOptions
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_config.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_common.h

    ${MSDK_STUDIO_ROOT}/shared/include/dispatch_session.h
    ${MSDK_STUDIO_ROOT}/shared/include/fast_copy_c_impl.h
    ${MSDK_STUDIO_ROOT}/shared/include/fast_copy.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_allocator.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_factory.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_interface.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_operation.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfxstructures-int.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfxstructurespro.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfxvideo++int.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_ext_buffers.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_reflect.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_timing.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_trace.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_umc_alloc_wrapper.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_utils.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_vpp_interface.h

    ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_c_impl.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/fast_copy.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_factory.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_umc_alloc_wrapper.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_umc_mjpeg_vpp.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_static_assert_structs.cpp
  )

set(sources_hw
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_allocator_vaapi.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_hw.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_vaapi.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_vpp_vaapi.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_vpp_helper.h

    ${MSDK_STUDIO_ROOT}/shared/src/mfx_vpp_vaapi.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_vpp_helper.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_vaapi.cpp
    
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_hw.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_vaapi.cpp
  )

add_library(mfxcore OBJECT)

target_sources(mfxcore PRIVATE ${sources_common})
target_sources(mfxcore PRIVATE ${sources_hw})

if (CMAKE_SYSTEM_NAME MATCHES Linux)
  target_sources(mfxcore
    PRIVATE
      scheduler/linux/src/mfx_scheduler_core.cpp
      scheduler/linux/src/mfx_scheduler_core_ischeduler.cpp
      scheduler/linux/src/mfx_scheduler_core_iunknown.cpp
      scheduler/linux/src/mfx_scheduler_core_task.cpp
      scheduler/linux/src/mfx_scheduler_core_task_management.cpp
      scheduler/linux/src/mfx_scheduler_core_thread.cpp
  )
endif()

set(HW_ONLY_LIBS
  asc
  $<$<BOOL:${MFX_ENABLE_EXT}>:mfx_ext>
)

set(LIBS
  bitrate_control
  decode_hw
  encode_hw
  vpp_hw
  mfx_common_hw
  mfx_trace
  mfx_logging
  ${HW_ONLY_LIBS}
  $<$<BOOL:${ITT_FOUND}>:itt>
  Threads::Threads
  ${CMAKE_DL_LIBS}
  $<$<PLATFORM_ID:Windows>:cfgmgr32>
)

if (MFX_ENABLE_ENCTOOLS OR MFX_ENABLE_HW_LPLA)
  set(LIBS
      ${LIBS}
      enctools_base
    )
endif()

target_compile_definitions(mfxcore
  PUBLIC
    MFX_VA
)

target_include_directories(mfxcore
  PUBLIC
    scheduler/include
    ${OPENCL_INCLUDE}
    ${VULKAN_INCLUDE}
  )
  
if (CMAKE_SYSTEM_NAME MATCHES Linux)
  target_include_directories(mfxcore
    PUBLIC
      scheduler/linux/include
      ${VULKAN_INCLUDE}
  )
endif()

target_link_libraries(mfxcore
  PUBLIC
    ${LIBS}
  PRIVATE
    mfx_static_lib
    mfx_sdl_properties
)

add_library(fast_copy_sse4 OBJECT
  ${MSDK_STUDIO_ROOT}/shared/include/fast_copy_sse4_impl.h
  ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_sse4_impl.cpp
)
target_include_directories(fast_copy_sse4
  PRIVATE
    ${MSDK_STUDIO_ROOT}/shared/include
)
target_link_libraries(fast_copy_sse4
  PRIVATE
    bitrate_control
    mfx_require_sse4_properties
)

if( DEFINED MFX_LIBNAME )
  set( mfxlibname "${MFX_LIBNAME}")
else()
  make_runtime_name( hw mfxlibname )
endif()

message( DEBUG "Runtime name: ${mfxlibname}")

add_library(${mfxlibname} SHARED)

target_sources(${mfxlibname}
  PRIVATE
    $<TARGET_OBJECTS:fast_copy_sse4>
)

target_link_libraries(${mfxlibname}
  PRIVATE
    mfxcore
    mfx_shared_lib
    mfx_sdl_properties
)

target_link_options(${mfxlibname}
  PRIVATE
    $<$<PLATFORM_ID:Linux>:LINKER:--version-script=${MSDK_LIB_ROOT}/libmfx-gen.map>
)

get_mfx_version(mfx_version_major mfx_version_minor)

set(libversion   1.${mfx_version_major}.${mfx_version_minor})
set(libsoversion 1.${mfx_version_major})

set_target_properties(${mfxlibname} PROPERTIES
    VERSION ${libversion}
    SOVERSION ${libsoversion}
)

install(TARGETS ${mfxlibname} LIBRARY DESTINATION ${MFX_MODULES_DIR})

if (CMAKE_SYSTEM_NAME MATCHES Linux)
set( PKG_CONFIG_FNAME "${CMAKE_LIB_DIR}/${CMAKE_BUILD_TYPE}/lib${mfxlibname}.pc")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/pkg-config.pc.cmake" ${PKG_CONFIG_FNAME} @ONLY)
install( FILES ${PKG_CONFIG_FNAME} DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig )
endif()

include(sources_ext.cmake OPTIONAL)
