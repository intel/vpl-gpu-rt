##******************************************************************************
##  Copyright(C) 2014 Intel Corporation. All Rights Reserved.
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
##  Content: Intel(R) Media SDK Samples projects creation and build
##******************************************************************************

find_path( GTEST_INCLUDE gtest/gtest.h PATHS "$ENV{GTEST_INCLUDE_PATH}" "/usr/local/include" )
find_library( GTEST_LIBRARY gtest PATHS "$ENV{GTEST_LIBRARY_PATH}" "/usr/local/lib" )
find_library( GTEST_MAIN_LIBRARY gtest_main PATHS "$ENV{GTEST_LIBRARY_PATH}" "/usr/local/lib" )

if(NOT GTEST_INCLUDE MATCHES NOTFOUND)
  if(NOT GTEST_LIBRARY MATCHES NOTFOUND AND
        NOT GTEST_MAIN_LIBRARY MATCHES NOTFOUND)
    set( GTEST_FOUND TRUE )

    get_filename_component(GTEST_LIB_DIR ${GTEST_LIBRARY} PATH)
    get_filename_component(GTEST_MAIN_LIB_DIR ${GTEST_MAIN_LIBRARY} PATH)

    include_directories( "${GTEST_INCLUDE}" )
    link_directories( "${GTEST_LIB_DIR}" "${GTEST_MAIN_LIB_DIR}" )
  endif()
endif()

if(NOT DEFINED GTEST_FOUND)
  message( STATUS "Google tests libraries and headers were not found! Build GTest and install to /usr/local." )
else ()
  message( STATUS "Google tests libraries were found in ${GTEST_LIB_DIR}" )
  message( STATUS "Google tests headers were found in ${GTEST_INCLUDE}" )
endif()

