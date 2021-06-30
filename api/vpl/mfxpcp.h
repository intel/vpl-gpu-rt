/*############################################################################
  # Copyright (C) 2019-2020 Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MFXPCP_H__
#define __MFXPCP_H__
#include "mfxstructures.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/*! The Protected enumerator describes the protection schemes. */
enum {
    MFX_PROTECTION_CENC_ANDROID_WV_CLASSIC      = 0x0004, /*!< The protection scheme is based on the Hardware DRM*. */
    MFX_PROTECTION_CENC_ANDROID_WV_DASH         = 0x0005, /*!< The protection scheme is based on the Modular DRM*. */
};

/* Extended Buffer Ids */
enum {
    MFX_EXTBUFF_CENC_PARAM          = MFX_MAKEFOURCC('C','E','N','P') /*!< This structure is used to pass CENC status report index for Common
                                                                           Encryption usage model. See the mfxExtCencParam structure for more details. */
};

MFX_PACK_BEGIN_USUAL_STRUCT()
/*!
   Used to pass the CENC status report index for the Common Encryption usage model. The application can
   attach this extended buffer to the mfxBitstream structure at runtime.
*/
typedef struct _mfxExtCencParam{
    mfxExtBuffer Header;      /*!< Extension buffer header. Header.BufferId must be equal to MFX_EXTBUFF_CENC_PARAM. */

    mfxU32 StatusReportIndex; /*!< CENC status report index. */
    mfxU32 reserved[15];
} mfxExtCencParam;
MFX_PACK_END()

#ifdef __cplusplus
} // extern "C"
#endif /* __cplusplus */

#endif
