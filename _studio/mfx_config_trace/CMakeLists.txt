add_library(mfx_config_trace STATIC ${CMAKE_BINARY_DIR}/mfx_config_trace.cpp)

if( DEFINED MFX_LIBNAME )
  set( mfxlibname "${MFX_LIBNAME}")
else()
  make_runtime_name( hw mfxlibname )
endif()

target_compile_definitions(mfx_config_trace
  PRIVATE
     $<TARGET_PROPERTY:${mfxlibname},COMPILE_DEFINITIONS>)

target_link_libraries(mfx_config_trace
  PUBLIC 
    mfx_static_lib)

file(STRINGS ${CMAKE_BINARY_DIR}/mfx_features.h mfx_features_contents)
file(STRINGS ../shared/include/mfx_config.h mfx_config_contents)
file(STRINGS ../shared/include/mfx_config_decode.h mfx_config_decode_contents)
file(STRINGS ../shared/include/mfx_config_encode.h mfx_config_encode_contents)
file(STRINGS ../shared/include/mfx_config_vpp.h mfx_config_vpp_contents)
string(REGEX MATCHALL " *((#define)|(#undef)) +[A-Z_a-z0-9]+" macro_list
 ${mfx_features_contents}
 ${mfx_features_ext_contents}
 ${mfx_config_contents}
 ${mfx_config_decode_contents}
 ${mfx_config_encode_contents}
 ${mfx_config_vpp_contents})

file(REMOVE ${CMAKE_BINARY_DIR}/mfx_config_trace.cpp)
file(APPEND ${CMAKE_BINARY_DIR}/mfx_config_trace.cpp "#include \"mfx_config.h\"\n")
list(APPEND history_list "")
foreach(macro IN LISTS macro_list)
  string(REPLACE "#define" "" macro_tmp1 ${macro})
  string(REPLACE "#undef" "" macro_tmp2 ${macro_tmp1})
  string(REPLACE " " "" macro_out ${macro_tmp2})
  
  string(REGEX MATCHALL ".*_H_.*" tmp_list ${macro_out})
  list(LENGTH tmp_list length)
  if (length EQUAL 1)
      continue()
  endif()
  
  set(is_dup 0)
  foreach(history_macro IN LISTS history_list)
      string(COMPARE EQUAL ${macro_out} ${history_macro} is_dup)
  endforeach()
  if (is_dup EQUAL 1)
      continue()
  endif()
  list(APPEND history_list ${macro_out})
  
  file(APPEND ${CMAKE_BINARY_DIR}/mfx_config_trace.cpp
  "#ifdef ${macro_out}\n"
  "#pragma message(\"${macro_out}    ON\")\n"
  "#else\n"
  "#pragma message(\"${macro_out}    OFF\")\n"
  "#endif\n")
endforeach()
