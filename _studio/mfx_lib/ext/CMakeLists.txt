# Copyright (c) 2018-2021 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

add_subdirectory(cmrt_cross_platform)
add_subdirectory(asc)

add_library(mfx_ext STATIC)

if(MFX_ENABLE_KERNELS)

  set( MSDK_GENX_ROOT ${MSDK_LIB_ROOT}/ext/genx )

  if (BUILD_KERNELS)
    # Here we define build steps to generate c-array binary kernels. If you
    # don't use BUILD_KERNELS options these kernels will just be used from
    # pre-built form. If you will regenerate kernels you may notice the
    # difference from the per-built kernels in git-diff.

    # Helper application to generate c-array data representation of .isa binary
    # kernels.
    add_executable(embed_isa ${MSDK_GENX_ROOT}/tools/embed_isa.c)
    set_target_properties(embed_isa PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # Generates CM kernel suitable for usage in C/C++ application sources
    #   ${src} - input CM kernel source file
    #   ${genx} - platform generation level
    #   ${out_dir} - directory to generate output
    # Generated outputs for ${src}=path/file.cpp, ${genx}=gen9:
    #   ${out_dir}/file_gen9_isa.h
    #   ${out_dir}/file_gen9_isa.cpp
    function(add_cmc_target src genx out_dir)
      get_filename_component(tgt ${src} NAME_WE)
      add_custom_command(
        OUTPUT ${tgt}_${genx}.isa
        DEPENDS embed_isa ${src}
        COMMAND ${CMC}
          -c -Qxcm -Qxcm_release -Qxcm_print_asm_count
          -menableiga -mCM_init_global -mCM_printregusage
          -Dtarget_${genx} -Qxcm_jit_target=${genx}
          ${src} -o ${tgt}_${genx}.isa)

      add_custom_command(
        OUTPUT ${tgt}_${genx}_isa.cpp ${tgt}_${genx}_isa.h
        DEPENDS embed_isa ${tgt}_${genx}.isa
        COMMAND embed_isa ${tgt}_${genx}.isa)
      add_custom_command(
        OUTPUT ${out_dir}/${tgt}_${genx}_isa.h ${out_dir}/${tgt}_${genx}_isa.cpp
        DEPENDS ${tgt}_${genx}_isa.cpp ${tgt}_${genx}_isa.h
        COMMAND ${CMAKE_COMMAND} -E copy ${tgt}_${genx}_isa.h ${out_dir}
        COMMAND ${CMAKE_COMMAND} -E copy ${tgt}_${genx}_isa.cpp ${out_dir})
    endfunction()
  endif()

  ### ASC
  if( MFX_ENABLE_ASC )
    if(BUILD_KERNELS)
      foreach(platform gen12lp)
        add_cmc_target(${MSDK_GENX_ROOT}/asc/src/genx_scd.cpp ${platform} ${MSDK_GENX_ROOT}/asc/isa)
      endforeach()
    endif()

    set( prefix ${MSDK_GENX_ROOT}/asc/isa )
    list( APPEND genx_sources
      ${prefix}/genx_scd_gen12lp_isa.cpp
    )
    list( APPEND genx_include_dirs
      ${prefix}
    )
  endif()

  ### copy
  if(BUILD_KERNELS)
    foreach(platform gen12lp)
      add_cmc_target(${MSDK_GENX_ROOT}/copy_kernels/src/genx_copy_kernel.cpp ${platform} ${MSDK_GENX_ROOT}/copy_kernels/isa)
    endforeach()
  endif()

  set( prefix ${MSDK_GENX_ROOT}/copy_kernels/isa )
  list( APPEND genx_sources
    ${prefix}/genx_copy_kernel_gen12lp_isa.cpp
  )
  list( APPEND genx_include_dirs
    ${prefix}
  )

  ### field copy
  if(BUILD_KERNELS)
    foreach(platform gen12lp)
      add_cmc_target(${MSDK_GENX_ROOT}/field_copy/src/genx_fcopy.cpp ${platform} ${MSDK_GENX_ROOT}/field_copy/isa)
    endforeach()
  endif()

  set( prefix ${MSDK_GENX_ROOT}/field_copy/isa )
  list( APPEND genx_sources
    ${prefix}/genx_fcopy_gen12lp_isa.cpp
  )
  list( APPEND genx_include_dirs
    ${prefix}
  )

  ### h264_encode
  if(MFX_ENABLE_H264_VIDEO_ENCODE)
    if(BUILD_KERNELS)
      foreach(platform gen12lp)
        add_cmc_target(${MSDK_GENX_ROOT}/h264_encode/src/genx_histogram.cpp ${platform} ${MSDK_GENX_ROOT}/h264_encode/isa)
        add_cmc_target(${MSDK_GENX_ROOT}/h264_encode/src/genx_simple_me.cpp ${platform} ${MSDK_GENX_ROOT}/h264_encode/isa)
      endforeach()
    endif()

    set( prefix ${MSDK_GENX_ROOT}/h264_encode/isa )
    list( APPEND genx_sources
      ${MSDK_GENX_ROOT}/h264_encode/src/genx_simple_me_proto.cpp
      ${prefix}/genx_simple_me_gen12lp_isa.cpp
      ${prefix}/genx_histogram_gen12lp_isa.cpp
    )
    list( APPEND genx_include_dirs
      ${prefix}
    )

  endif()

  ### MCTF
  if(MFX_ENABLE_MCTF)
    if(BUILD_KERNELS)
      foreach(platform gen12lp)
        add_cmc_target(${MSDK_GENX_ROOT}/mctf/src/genx_me.cpp ${platform} ${MSDK_GENX_ROOT}/mctf/isa)
        add_cmc_target(${MSDK_GENX_ROOT}/mctf/src/genx_mc.cpp ${platform} ${MSDK_GENX_ROOT}/mctf/isa)
        add_cmc_target(${MSDK_GENX_ROOT}/mctf/src/genx_sd.cpp ${platform} ${MSDK_GENX_ROOT}/mctf/isa)
      endforeach()
    endif()

    set( prefix ${MSDK_GENX_ROOT}/mctf/isa )
    list( APPEND genx_sources
      ${prefix}/genx_mc_gen12lp_isa.cpp
      ${prefix}/genx_me_gen12lp_isa.cpp
      ${prefix}/genx_sd_gen12lp_isa.cpp
    )
    list( APPEND mctf_package
      mctf_package/mctf/src/mctf_common.cpp
      mctf_package/mctf/include/mctf_common.h
    )
    list( APPEND genx_include_dirs
      ${prefix}
    )
    list ( APPEND mctf_include_dir
      mctf_package/mctf/include
    )
  endif()

  source_group("genx" FILES ${genx_sources})
  source_group("mctf_hw" FILES ${mctf_package})

  target_include_directories(mfx_ext
    PUBLIC
      ${genx_include_dirs}
      ${mctf_include_dir}
      ${VULKAN_INCLUDE}
  )

  target_sources(mfx_ext PRIVATE
    ${genx_sources}
    ${mctf_package}
  )

  if (MFX_ENABLE_H264_VIDEO_ENCODE)
    target_include_directories(mfx_ext PUBLIC
      h264/include
      ${MSDK_UMC_ROOT}/codec/brc/include
      ${MSDK_LIB_ROOT}/encode_hw/h264/include
    )
    set(avc_video_encode_src
      h264/include/mfx_h264_encode_cm.h
      h264/include/mfx_h264_encode_cm_defs.h
      h264/src/mfx_h264_encode_cm.cpp
    )

    source_group("avce" FILES ${avc_video_encode_src})

    target_sources(mfx_ext PRIVATE
      ${avc_video_encode_src}
    )
  endif()
endif()

if (MFX_ENABLE_MPEG2_VIDEO_ENCODE)
  target_include_directories(mfx_ext PUBLIC
    mpeg2/include
    ${MSDK_UMC_ROOT}/codec/brc/include
    ${MSDK_LIB_ROOT}/shared/include
    ${MSDK_STUDIO_ROOT}/shared/enctools/include
  )

  set(mpeg2_video_encode_src
    mpeg2/include/mfx_mpeg2_encode_debug_hw.h
    mpeg2/include/mfx_mpeg2_encode_full_hw.h
    mpeg2/include/mfx_mpeg2_encode_hw.h
    mpeg2/include/mfx_mpeg2_encode_utils_hw.h
    mpeg2/include/mfx_mpeg2_enc_common.h
    mpeg2/include/mfx_mpeg2_enc_common_hw.h
    mpeg2/include/mfx_mpeg2_encode_interface.h
    mpeg2/include/mfx_mpeg2_encode_vaapi.h

    mpeg2/src/mfx_mpeg2_encode_debug_hw.cpp
    mpeg2/src/mfx_mpeg2_encode_full_hw.cpp
    mpeg2/src/mfx_mpeg2_encode_hw.cpp
    mpeg2/src/mfx_mpeg2_encode_utils_hw.cpp
    mpeg2/src/mfx_mpeg2_encode_vaapi.cpp
    mpeg2/src/mfx_mpeg2_encode_factory.cpp
    mpeg2/src/mfx_mpeg2_enc_common_hw.cpp

  )

  source_group("mpeg2e" FILES ${mpeg2_video_encode_src})

  target_sources(mfx_ext PRIVATE
    ${mpeg2_video_encode_src}
  )
endif()

target_link_libraries(mfx_ext
  PUBLIC
    mfx_static_lib
    asc_ext
    cmrt_cross_platform_hw
  PRIVATE
    bitrate_control
)

include(sources_ext.cmake OPTIONAL)