if( NOT MFX_LIB_ONLY )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../core/umc/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../core/vm/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../core/vm_plus/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../io/media_buffers/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../../include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../api/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../../../mfx_lib/shared/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../io/umc_va/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../codec/vp9_dec/include )

set( sources "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources ${srcs})
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../../codec/vp9_dec/src/umc_vp9_bitstream.cpp )

set( defs "${API_FLAGS} ${WARNING_FLAGS} -DUMC_AV1_DECODER_REV=8500" )
make_library( umc_av1 hw static )

set( defs "" )
endif(0)
