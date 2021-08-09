// Copyright (c) 2011-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_VPP_VAAPI_SIMULATION_H__
#define __MFX_VPP_VAAPI_SIMULATION_H__

#include "umc_va_base.h"
#include "mfx_vpp_driver.h"
#include "mfx_platform_headers.h"
//#include <va/va.h>

namespace MfxHwVideoProcessing
{
    class VAAPIVideoProcessing : public DriverVideoProcessing
    {
    public:

        VAAPIVideoProcessing();
        
        virtual ~VAAPIVideoProcessing();

        virtual mfxStatus CreateDevice(VideoCORE * core);

        virtual mfxStatus DestroyDevice( void );

        virtual mfxStatus Register(mfxHDL *pSurfaces, 
                                   mfxU32 num, 
                                   BOOL bRegister);

        virtual mfxStatus QueryTaskStatus(PREPROC_QUERY_STATUS *pQueryStatus, mfxU32 numStructures);
        
        virtual mfxStatus QueryCapabilities( mfxVppCaps& caps );

        virtual BOOL IsRunning() { return m_bRunning; }

        virtual mfxStatus Execute(mfxExecuteParams *pParams);

    private:

        BOOL m_bRunning;

        VideoCORE* m_core;
        
        //VADisplay  m_vaDisplay;

        std::vector<mfxU32> m_cachedReadyTaskIndex;

        UMC::Mutex m_guard;
        _mfxPlatformAccelerationService m_vaDisplay;

        mfxStatus Init( _mfxPlatformAccelerationService* pVADisplay );

        mfxStatus Close( void );
        
        mfxStatus SimulationExecute( 
            VASurfaceID in, 
            VASurfaceID out,
            mfxFrameInfo srcRect,
            mfxFrameInfo dstRect);
    };
    
}; // namespace

#endif //__MFX_VPP_VAAPI_SIMULATION_H__
#endif // MFX_ENABLE_VPP

/* EOF */
