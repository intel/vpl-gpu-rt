##******************************************************************************
##  Copyright(C) 2015 Intel Corporation. All Rights Reserved.
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
##  Content: Intel(R) Media SDK Global Configuration of Targets Cmake module
##******************************************************************************

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
