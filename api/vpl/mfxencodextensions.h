/*############################################################################
  # Copyright Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef STRIP_EMBARGO

#ifndef __MFXENCODEXTENSIONS_H__
#define __MFXENCODEXTENSIONS_H__
#include "mfxcommon.h"
#include "mfxstructures.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/*! Private extension of ExtendedBufferID enumerator to specify ext buffers for CG encoding. */
enum {
    /*!
       See the mfxExtTemporalLayerBRC structure for details.
    */
    MFX_EXTBUFF_TEMPORALLAYER_BRC         = MFX_MAKEFOURCC('T','M','P','B'),
    /*!
       See the mfxExtContentAnalyzer structure for details.
    */
    MFX_EXTBUFF_CONTENT_ANALYSER          = MFX_MAKEFOURCC('C','O','N','A'),
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   The structure can be used by application to enable Bit-rate control for per each temporal layer.
   See mfxExtTemporalLayers for details. 
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_TEMPORALLAYER_BRC. */
    mfxU16       EnableTemporalLayerBRC;      /*! Tri-state option to enable temporal layer BRC. 
                                                  Temporal Layered BRC is enabled when EnableTemporalLayerBRC 
                                                  equals to MFX_CODINGOPTION_ON or MFX_CODINGOPTION_UNKNOWN.*/ 
    mfxU16       reserved[11];
} mfxExtTemporalLayerBRC;
MFX_PACK_END()

/*! Modes of content analysis. */
enum {
     MFX_CONTENTANALYSIS_ROM        = 0, /*!< In this mode the content analyzer detects regions of motion including scene changes. */
     MFX_CONTENTANALYSIS_SCD        = 1, /*!< In this mode the content analyzer detects scene changes. */
     MFX_CONTENTANALYSIS_DISABLED   = 2, /*!< The content analyzer is disabled. */
};


MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   The structure can be used by application to specify diffrent modes of content analysis which are used
   by BRC. The control is a tradeoff between encoding latency and efficiency of how BRC 
   distributes bits between compressed frames. 
*/
typedef struct {
    mfxExtBuffer Header; /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_CONTENT_ANALYSER. */
    mfxU16       ContentAnalysisMode;      /*! enum to specify current mode of content analyzer block.*/ 
    mfxU16       reserved[11];
} mfxExtContentAnalysisMode;
MFX_PACK_END()


/*! Special ScenarioInfo mode for High Quality cloug gaming. */
enum {
     MFX_SCENARIO_REMOTE_GAMING_HQ         = MFX_SCENARIO_REMOTE_GAMING + 1, /*!< High quality encoding mode for cloud gaming. */
};

#ifdef __cplusplus
} // extern "C"
#endif

#endif

#endif
