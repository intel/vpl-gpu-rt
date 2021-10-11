# Copyright (c) 2015-2021 Intel Corporation
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

if(__ITT OR ENABLE_ITT)
  if( Linux )
    if( CMAKE_VTUNE_HOME )
      set( VTUNE_HOME ${CMAKE_VTUNE_HOME} )
    elseif( DEFINED ENV{CMAKE_VTUNE_HOME} )
      set( VTUNE_HOME $ENV{CMAKE_VTUNE_HOME} )
    else()
      set( VTUNE_HOME /opt/intel/vtune_amplifier_xe )
    endif()

    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
      set( arch "64" )
    elseif()
      set( arch "32" )
    endif()

    find_path( VTUNE_INCLUDE ittnotify.h PATHS ${VTUNE_HOME}/include )
    find_library( VTUNE_LIBRARY libittnotify.a PATHS ${VTUNE_HOME}/lib${arch}/ )

    if( NOT VTUNE_INCLUDE MATCHES NOTFOUND )
      if( NOT VTUNE_LIBRARY MATCHES NOTFOUND )
        set( VTUNE_FOUND TRUE )
        message( STATUS "ITT was found here ${VTUNE_HOME}" )

        get_filename_component( VTUNE_LIBRARY_PATH ${VTUNE_LIBRARY} PATH )

        include_directories( ${VTUNE_INCLUDE} )
        link_directories( ${VTUNE_LIBRARY_PATH} )

        append( "-DMFX_TRACE_ENABLE_ITT" CMAKE_C_FLAGS )
        append( "-DMFX_TRACE_ENABLE_ITT" CMAKE_CXX_FLAGS )

        set( ITT_CFLAGS "-I${VTUNE_INCLUDE} -DITT_SUPPORT" )
        set( ITT_LIBRARY_DIRS "${VTUNE_LIBRARY_PATH}" )

        set( ITT_LIBRARIES "ittnotify" )
      endif()
    endif()
  else()
    message( STATUS "MFX tracing is supported only for linux!" )
  endif()

endif()
