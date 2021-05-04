##******************************************************************************
##  Copyright(C) 2012-2020 Intel Corporation. All Rights Reserved.
##
##  The source code, information  and  material ("Material") contained herein is
##  owned  by Intel Corporation or its suppliers or licensors, and title to such
##  Material remains  with Intel Corporation  or its suppliers or licensors. The
##  Material  contains proprietary information  of  Intel or  its  suppliers and
##  licensors. The  Material is protected by worldwide copyright laws and treaty
##  provisions. No  part  of  the  Material  may  be  used,  copied, reproduced,
##  modified, published, uploaded, posted, transmitted, distributed or disclosed
##  in any way  without Intel's  prior  express written  permission. No  license
##  under  any patent, copyright  or  other intellectual property rights  in the
##  Material  is  granted  to  or  conferred  upon  you,  either  expressly,  by
##  implication, inducement,  estoppel or  otherwise.  Any  license  under  such
##  intellectual  property  rights must  be express  and  approved  by  Intel in
##  writing.
##
##  *Third Party trademarks are the property of their respective owners.
##
##  Unless otherwise  agreed  by Intel  in writing, you may not remove  or alter
##  this  notice or  any other notice embedded  in Materials by Intel or Intel's
##  suppliers or licensors in any way.
##
##******************************************************************************
##  Content: Intel(R) Media SDK projects creation and build
##******************************************************************************

set( CMAKE_LIB_DIR ${CMAKE_BINARY_DIR}/__lib )
set( CMAKE_BIN_DIR ${CMAKE_BINARY_DIR}/__bin )
set_property( GLOBAL PROPERTY PROP_PLUGINS_CFG "" )
set_property( GLOBAL PROPERTY PROP_PLUGINS_EVAL_CFG "" )

# .....................................................
function( collect_oses )
  if( CMAKE_SYSTEM_NAME MATCHES Windows )
    set( Windows    true PARENT_SCOPE )
    set( NotLinux   true PARENT_SCOPE )
    set( NotDarwin  true PARENT_SCOPE )

  elseif( CMAKE_SYSTEM_NAME MATCHES Linux )
    set( Linux      true PARENT_SCOPE )
    set( NotDarwin  true PARENT_SCOPE )
    set( NotWindows true PARENT_SCOPE )

  elseif( CMAKE_SYSTEM_NAME MATCHES Darwin )
    set( Darwin     true PARENT_SCOPE )
    set( NotLinux   true PARENT_SCOPE )
    set( NotWindows true PARENT_SCOPE )

  endif()
endfunction()

# .....................................................
function( append what where )
  set(${ARGV1} "${ARGV0} ${${ARGV1}}" PARENT_SCOPE)
endfunction()

# .....................................................
function( create_build )
  file( GLOB_RECURSE components "${CMAKE_SOURCE_DIR}/*/CMakeLists.txt" )
  foreach( component ${components} )
    get_filename_component( path ${component} PATH )
    if(NOT path MATCHES ".*/deprecated/.*")
      message(STATUS "Adding subdir: ${path}")
      add_subdirectory( ${path} )
    endif()
  endforeach()
endfunction()

# .....................................................
# Usage:
#  create_build_outside_tree(current_repo|<name>)
#    - current_repo|<name>: the alias of the folder for which the built components will be placed
#
function( create_build_outside_tree current_repo)

  file( GLOB_RECURSE components "${CMAKE_SOURCE_DIR}/*/CMakeLists.txt" )
  foreach( component ${components} )
    get_filename_component(path ${component} PATH)
    get_filename_component(folder_name ${path} NAME)
    if(NOT path MATCHES ".*/deprecated/.*")
      message(STATUS "Adding subdir: ${path} ./${current_repo}/${folder_name}")
      add_subdirectory(${path} ./${current_repo}/${folder_name})
    endif()
  endforeach()
endfunction()

# .....................................................
function( get_source include sources)
  file( GLOB_RECURSE include "[^.]*.h" )
  file( GLOB_RECURSE sources "[^.]*.c" "[^.]*.cpp" )

  set( ${ARGV0} ${include} PARENT_SCOPE )
  set( ${ARGV1} ${sources} PARENT_SCOPE )
endfunction()


function( git_describe git_commit )
  execute_process(
    COMMAND git rev-parse --short HEAD
    OUTPUT_VARIABLE git_commit
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  )
  if( NOT ${git_commit} MATCHES "^$" )
    set( git_commit "${git_commit}" PARENT_SCOPE )
  endif()
endfunction()

function(msdk_install target dir)
  if( Windows )
    install( TARGETS ${target} RUNTIME DESTINATION ${dir} )
  else()
    install( TARGETS ${target} LIBRARY DESTINATION ${dir} )
  endif()
endfunction()

function(disable_werror)

  foreach(var
    CMAKE_C_FLAGS CMAKE_CXX_FLAGS
    CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELWITHDEBINFO
    CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELWITHDEBINFO)


    string(REPLACE "-Werror" ""  ${var} "${${var}}")
  endforeach()

    string(REPLACE " -Werror" "" TMP_C_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REPLACE " -Werror" "" TMP_CXX_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE " -Werror" "" TMP_C_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REPLACE " -Werror" "" TMP_CXX_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS_DEBUG "${TMP_C_DEBUG}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS_DEBUG "${TMP_CXX_DEBUG}" PARENT_SCOPE)
    set(CMAKE_C_FLAGS_RELEASE "${TMP_C_RELEASE}" PARENT_SCOPE)
    set(CMAKE_CXX_FLAGS_RELEASE "${TMP_CXX_RELEASE}" PARENT_SCOPE)
endfunction()
