# Copyright (c) 2018-2019 Intel Corporation
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

set(CMAKE_VERBOSE_MAKEFILE ON)


function( make_runtime_name variant name )
  if( CMAKE_SYSTEM_NAME MATCHES Windows)
    set(prefix "lib")
  else()
    set(prefix "")
  endif()

  set (runtime_name ${prefix}mfx )
  if( CMAKE_SYSTEM_NAME MATCHES Windows )
    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
      set( runtime_name ${runtime_name}64 )
    else()
      set( runtime_name ${runtime_name}32 )
    endif()
  endif()
  set (runtime_name ${runtime_name}-gen)
  set( ${name} ${runtime_name} PARENT_SCOPE)
endfunction()

