# Copyright (c) 2017-2021 Intel Corporation
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

add_library(ipp_sse4 OBJECT)

target_compile_definitions(ipp_sse4
  PRIVATE
    $<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,_Y8,_P8>
    $<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,_ARCH_EM64T,_ARCH_IA32>
  )

target_include_directories(ipp_sse4 PRIVATE include)

target_link_libraries(ipp_sse4 PRIVATE mfx_common_properties mfx_require_sse4_properties)

target_sources(ipp_sse4
  PRIVATE
    src/dispatcher.h
    src/ippcachesize.c
    src/ippmalloc.c
    src/ippver.h
    src/ippversion.h
    src/ippverstr.gen
    src/owncc.h
    src/owncore.h
    src/owncpufeatures.c
    src/owni.h
    src/ownj.h
    src/owns.h
    src/ownvc.h
    src/pccjoin422pxca.c
    src/pccycbcr44deint.c
    src/pccycbcr44deintw7cn.c
    src/pccyuvms.c
    src/pccyuvmsw7.c
    src/piconvert02ca.c
    src/piconvertca.c
    src/picopyca.c
    src/picopycpp8y8cn.c
    src/pilogic.c
    src/pizigzag.c
    src/pjdecdct0.c
    src/pjdecdct0w7cn.c
    src/pjdecdct1.c
    src/pjdecdct1w7cn.c
    src/pjdecdct32f.c
    src/pjdecdct32fw7cn.c
    src/pjdecdctcn.c
    src/pjdechuff.c
    src/pjdechuff.h
    src/pjdechuffls.c
    src/pjdechuffp.c
    src/pjdecpred.c
    src/pjdecqnt.c
    src/pjdecss0.c
    src/pjdecss.h
    src/pjenccc0.c
    src/pjenccc1.c
    src/pjenccc.c
    src/pjenccc.h
    src/pjencccps.c
    src/pjencccpsy8.c
    src/pjenccctbl.c
    src/pjenccctbl.h
    src/pjencdct32f.c
    src/pjencdct32fw7cn.c
    src/pjencdct.c
    src/pjenchuff.c
    src/pjenchuff.h
    src/pjenchuffls.c
    src/pjenchuffp.c
    src/pjencqnt.c
    src/pjencss0.c
    src/pjencss.c
    src/pjencss.h
    src/pjhufftbl.c
    src/pjhufftbl.h
    src/pjname.h
    src/pjquant.h
    src/pjver.h
    src/pjzigzag.c
    src/pjzigzag.h
    src/precomp.h
    src/ps_anarith.h
    src/pscopyca.c
    src/pscopy.h
    src/psdiv.c
    src/psmul.c
    src/psmulm6a6w7cn.c
    src/psshift.c
    src/pvccommonfunctions.h
    src/pvcdeinterlacevideoplane.c
    src/pvcmacro.h
    src/pvcown.h
    src/pvcvc1functions.h
    src/pvcvc1rangemap.c

    src/asm_intel64/pcccopym7as.s
    src/asm_intel64/piconvert01m7as.s
    src/asm_intel64/piconvert02m7as.s
    src/asm_intel64/picopyu8as.s
    src/asm_intel64/pidct8844im7as.s
    src/asm_intel64/pidct88fm7as.s
    src/asm_intel64/pidct88i_3_m7as.s
    src/asm_intel64/pidct88im7as.s
    src/asm_intel64/pilogic16um7as.s
    src/asm_intel64/pjarithm7as.s
    src/asm_intel64/pjdechuffpm7as.s
    src/asm_intel64/pjdechuffsu8as.s
    src/asm_intel64/pjdechuffu8as.s
    src/asm_intel64/pjdecpredm7as.s
    src/asm_intel64/pjdecqntm7as.s
    src/asm_intel64/pjdecssu8as.s
    src/asm_intel64/pjenchuffm7as.s
    src/asm_intel64/pjenchuffpm7as.s
    src/asm_intel64/pjencssm7as.s
    src/asm_intel64/pscopyu8as.s
    src/asm_intel64/pscpudef32eas.s
    src/asm_intel64/pscpuinfo32eas.s
    src/asm_intel64/psdivu8as.s
    src/asm_intel64/pssetm7as.s
    src/asm_intel64/psshiftm7as.s
    src/asm_intel64/pszerom7as.s
    src/asm_intel64/pvcvc1rangemapm7as.s
  )

enable_language(C ASM)
set( CMAKE_ASM_SOURCE_FILE_EXTENSIONS s )

## ipp
add_library(ipp STATIC 
  src/ippinit.c
  $<TARGET_OBJECTS:ipp_sse4>
)

target_include_directories(ipp PUBLIC include)

target_link_libraries(ipp PRIVATE mfx_common_properties)

set(IPP_LIBS ipp)
