# Copyright (c) 2012-2021 Intel Corporation
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
  set(component "${CMAKE_SOURCE_DIR}/${current_repo}/CMakeLists.txt")
  if(EXISTS ${component})
    add_subdirectory(${CMAKE_SOURCE_DIR}/${current_repo} ./${current_repo})
    message(STATUS "Adding subdir: ${CMAKE_SOURCE_DIR}/${current_repo} ./${current_repo}")
  endif()
endfunction()
# .....................................................

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


macro (PushOption)
  foreach(arg IN ITEMS ${ARGN})
    if (NOT DEFINED ${arg})
      continue()
    endif()

    get_property(_save_help_${arg} CACHE ${arg} PROPERTY HELPSTRING)
    set(_save_value_${arg} ${${arg}})
  endforeach()
endmacro ()

macro (PopOption)
  foreach(arg IN ITEMS ${ARGN})
    if (NOT DEFINED ${arg})
      continue()
    endif()

    if (DEFINED _save_help_${arg})
      set(${arg} ${_save_value_${arg}} CACHE BOOL ${_save_help_${arg}} FORCE)
    else()
      set(${arg} ${_save_value_${arg}})
    endif()
  endforeach()
endmacro ()