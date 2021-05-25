// Copyright (c) 2011-2020 Intel Corporation
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


#ifndef __LIBMFX_CORE__VAAPI_H__
#define __LIBMFX_CORE__VAAPI_H__

#include <memory>
#include "umc_structures.h"
#include "libmfx_core.h"
#include "libmfx_allocator_vaapi.h"
#include "libmfx_core_interface.h"

#include "mfx_platform_headers.h"
#include "mfx_session.h"

#include "va/va.h"
#include "vaapi_ext_interface.h"

#if defined (MFX_ENABLE_VPP)
#include "mfx_vpp_interface.h"
#endif

#include <memory>

// helper struct, it is helper for conversion from between [linux/android GUIDs] and [VAProfile + VAEntryPoint]
// linux/android GUIDs is defined in _studio/shared/include/mfxvideo++int.h
//
// GUID.Data4 is unused in current implementation, but struct stores it for correct GUID comparison
// Original mapping between GUID and VAProfile:
// {00000000 - 0000-0000  - 0000-000000000000}
// VAProfile |VAEntryPoint|       UNUSED
struct VaGuidMapper
{
    // VAEntrypoint and VAProfile is libVA enum
    VAProfile m_profile;
    VAEntrypoint m_entrypoint;
    unsigned char  m_Data4[8] = {};

    VaGuidMapper(VAProfile profile, VAEntrypoint entrypoint) :
        m_profile    (profile),
        m_entrypoint (entrypoint)
    {}

    VaGuidMapper(int profile, int entrypoint) :
        m_profile    (static_cast<VAProfile>    (profile)),
        m_entrypoint (static_cast<VAEntrypoint> (entrypoint))
    {}

    // VaGuidMapper unpacking GUIDs
    VaGuidMapper(GUID guid)
    {
        m_profile    = static_cast<VAProfile>    (guid.Data1);
        m_entrypoint = static_cast<VAEntrypoint> ((guid.Data2 << 16) + guid.Data3);

        //check to correct copy string
        static_assert( sizeof(guid.Data4) == sizeof(m_Data4),
            "Error! Can't store guid.Data4 in m_Data4.");
        static_assert( sizeof(*guid.Data4) == sizeof(*m_Data4),
            "Error! Can't store guid.Data4 in m_Data4.");

        std::copy(std::begin(guid.Data4), std::end(guid.Data4), std::begin(m_Data4));
    }

    operator GUID() const
    {
        // packing enum VAProfile and VAEntrypoint to Data1 and Data2 and Data3
        GUID res = { (unsigned long)  m_profile,
                     (unsigned short) (m_entrypoint >> 16),
                     (unsigned short) (m_entrypoint & 0xffff),
                     { m_Data4[0], m_Data4[1], m_Data4[2], m_Data4[3],
                     m_Data4[4], m_Data4[5], m_Data4[6], m_Data4[7] }};

        // check size of Data1 and Data2 and Data3 for enum VAProfile and VAEntrypoint packing
        static_assert( sizeof(res.Data1) >= sizeof(VAProfile),
            "Error! Can't store data profile in guid.data1 (unsigned long).");
        static_assert((sizeof(res.Data2) + sizeof(res.Data3)) >= sizeof(VAEntrypoint),
            "Error! Can't store data entrypoint in guid.data2 (unsigned short) and guid.data3 (unsigned short).");

        return res;
    }
};

class CmCopyWrapper;

// disable the "conditional expression is constant" warning
#ifdef _MSVC_LANG
#pragma warning(disable: 4127)
#endif

namespace UMC
{
    class DXVA2Accelerator;
    class LinuxVideoAccelerator;
};

template <class Base>
class VAAPIVideoCORE_T : public Base
{
public:
    friend class FactoryCORE;
    friend class VAAPIVideoCORE20;
    friend class VAAPIVideoCORE_T<CommonCORE20>;
    class VAAPIAdapter : public VAAPIInterface
    {
    public:
        VAAPIAdapter(VAAPIVideoCORE_T *pVAAPICore):m_pVAAPICore(pVAAPICore)
        {
        };

    protected:
        VAAPIVideoCORE_T *m_pVAAPICore;
    };

    class CMEnabledCoreAdapter : public CMEnabledCoreInterface
    {
    public:
        CMEnabledCoreAdapter(VAAPIVideoCORE_T *pVAAPICore): m_pVAAPICore(pVAAPICore)
        {
        };
        virtual mfxStatus SetCmCopyStatus(bool enable) override
        {
            return m_pVAAPICore->SetCmCopyStatus(enable);
        };
    protected:
        VAAPIVideoCORE_T *m_pVAAPICore;
    };

    virtual ~VAAPIVideoCORE_T();

    virtual mfxStatus    GetHandle(mfxHandleType type, mfxHDL *handle)                                                           override;
    virtual mfxStatus    SetHandle(mfxHandleType type, mfxHDL handle)                                                            override;

    virtual mfxStatus    AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool isNeedCopy = true)     override;
            mfxStatus    ReallocFrame(mfxFrameSurface1 *surf);
    virtual void         GetVA(mfxHDL* phdl, mfxU16 type)                                                                        override
    {
        if (!phdl) return;

        (type & MFX_MEMTYPE_FROM_DECODE)?(*phdl = m_pVA.get()):(*phdl = 0);
    }
    // Get the current working adapter's number
    virtual mfxU32       GetAdapterNumber() override { return m_adapterNum; }
    virtual eMFXPlatform GetPlatformType()  override { return MFX_PLATFORM_HARDWARE; }

    virtual mfxStatus    DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)                                      override;
    virtual mfxStatus    DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType) override;

    mfxHDL * GetFastCompositingService();
    void SetOnFastCompositingService(void);
    bool IsFastCompositingEnabled(void) const;


    virtual eMFXHWType   GetHWType() override;

    virtual mfxStatus    CreateVA(mfxVideoParam * param, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, UMC::FrameAllocator *allocator) override;
    // to check HW capabilities
    virtual mfxStatus    IsGuidSupported(const GUID guid, mfxVideoParam *par, bool isEncoder = false) override;

    virtual eMFXVAType   GetVAType() const                                                            override { return MFX_HW_VAAPI; }
    virtual void*        QueryCoreInterface(const MFX_GUID &guid)                                     override;

#if defined (MFX_ENABLE_VPP)
    virtual void         GetVideoProcessing(mfxHDL* phdl)                                             override
    {
        if (!phdl) return;

        *phdl = &m_vpp_hw_resmng;
    }
#endif
    virtual mfxStatus    CreateVideoProcessing(mfxVideoParam * param)                                 override;

    mfxStatus            GetVAService(VADisplay *pVADisplay);

    // this function should not be virtual
    mfxStatus            SetCmCopyStatus(bool enable);

protected:
    VAAPIVideoCORE_T(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session = nullptr);
    virtual void           Close()                                                                            override;
    virtual mfxStatus      DefaultAllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response) override;

    mfxStatus              CreateVideoAccelerator(mfxVideoParam * param, int profile, int NumOfRenderTarget, VASurfaceID *RenderTargets, UMC::FrameAllocator *allocator);
    mfxStatus              ProcessRenderTargets(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxBaseWideFrameAllocator* pAlloc);
    mfxStatus              TraceFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxStatus sts);
    mfxStatus              OnDeblockingInWinRegistry(mfxU32 codecId);
    mfxStatus              CheckOrInitDisplay();

    void                   ReleaseHandle();

    std::unique_ptr<UMC::LinuxVideoAccelerator> m_pVA;
    VADisplay                                   m_Display;
    mfxHDL                                      m_VAConfigHandle;
    mfxHDL                                      m_VAContextHandle;
    bool                                        m_KeepVAState;
    int                                         m_intDRM = -1;

    const mfxU32                                m_adapterNum; // Ordinal number of adapter to work
    bool                                        m_bUseExtAllocForHWFrames;
    std::unique_ptr<mfxDefaultAllocatorVAAPI::mfxWideHWFrameAllocator>
                                                m_pcHWAlloc;
    eMFXHWType                                  m_HWType;
    eMFXGTConfig                                m_GTConfig;

    bool                                        m_bCmCopy;
    bool                                        m_bCmCopyAllowed;
    std::unique_ptr<CmCopyWrapper>              m_pCmCopy;
#if defined (MFX_ENABLE_VPP)
    VPPHWResMng                                 m_vpp_hw_resmng;
#endif

private:

    std::unique_ptr<VAAPIAdapter>               m_pAdapter;
    std::unique_ptr<CMEnabledCoreAdapter>       m_pCmAdapter;
    //required to WA FEI enabling after move it from plugin to library
    bool                                        m_bHEVCFEIEnabled;
};

using VAAPIVideoCORE = VAAPIVideoCORE_T<CommonCORE>;


// Refactored MSDK 2.0 core

using VAAPIVideoCORE20_base = deprecate_from_base < VAAPIVideoCORE_T<CommonCORE20> >;

class VAAPIVideoCORE20 : public VAAPIVideoCORE20_base
{
public:
    friend class FactoryCORE;

    virtual ~VAAPIVideoCORE20();

    virtual mfxStatus SetHandle(mfxHandleType type, mfxHDL handle)                                                                                    override;

    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool isNeedCopy = true)                             override;
            mfxStatus ReallocFrame(mfxFrameSurface1 *surf);

    virtual mfxStatus DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)                                                              override;
    virtual mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType)                         override;

    virtual mfxStatus CreateSurface(mfxU16 type, const mfxFrameInfo& info, mfxFrameSurface1*& surf)                                                   override;

protected:
    VAAPIVideoCORE20(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session = nullptr);
};


#endif // __LIBMFX_CORE__VAAPI_H__
/* EOF */
