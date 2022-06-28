if (NOT CMAKE_SYSTEM_NAME MATCHES Windows)
    return()
endif()

cmake_minimum_required(VERSION 3.5)
project(IntelMediaSDK)
set(DLL_NAME IntelMediaSDK)

# MediaEtwSetLinkerFlags: apply linker flags for given configuration
# linkerFlags: linker specific options
# linkerTarget: optional parameter - apply linker flags for specfied target
macro (MediaEtwSetLinkerFlags linkerFlags linkerTarget)
    foreach (opt ${linkerFlags})
        if ("${linkerTarget}" STREQUAL "")
            set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${opt}")
            set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${opt}")
        else()
            set (CMAKE_SHARED_LINKER_FLAGS_${linkerTarget} "${CMAKE_SHARED_LINKER_FLAGS_${linkerTarget}} ${opt}")
            set (CMAKE_EXE_LINKER_FLAGS_${linkerTarget} "${CMAKE_SHARED_LINKER_FLAGS_${linkerTarget}} ${opt}")
        endif()
    endforeach()
endmacro()

set(SOURCES_
    ${MSDK_STUDIO_ROOT}/shared/mfx_trace/IntelMediaSDK.man
    )

source_group("Resource Files" FILES
    ${MSDK_STUDIO_ROOT}/shared/mfx_trace/IntelMediaSDK.man
    ${CMAKE_BINARY_DIR}/IntelMediaSDK.rc
    )

# set compiler options
include(sources_ext.cmake)	

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_UNICODE /DUNICODE")
	
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_library(${DLL_NAME} SHARED ${SOURCES_} ${CMAKE_BINARY_DIR}/IntelMediaSDK.rc)

set_target_properties(${DLL_NAME} PROPERTIES OUTPUT_NAME IntelMediaSDK)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/IntelMediaSDK.rc 
    COMMENT "Running mc.exe to compile Media ETW manifest"
    COMMAND mc -h ${CMAKE_BINARY_DIR} -r ${CMAKE_BINARY_DIR} -z IntelMediaSDK -um ${MSDK_STUDIO_ROOT}/shared/mfx_trace/IntelMediaSDK.man
)