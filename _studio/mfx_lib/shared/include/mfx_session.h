// Copyright (c) 2008-2021 Intel Corporation
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

#if !defined(_MFX_SESSION_H)
#define _MFX_SESSION_H

#include <memory>

// base mfx headers
#include <mfxdefs.h>
#include <mfxstructures.h>
#include <mfxvideo++int.h>

#include "mfx_common.h"
#include "mfx_utils_logging.h"

// private headers
#include <mfx_interface_scheduler.h>
#include <libmfx_core_operation.h>

class SurfaceCache;

template <class T>
struct surface_cache_controller;

struct _mfxSession
{
    // Constructor
    _mfxSession(const mfxU32 adapterNum);
    // Destructor
    ~_mfxSession(void);

    // Clear state
    void Clear(void);

    // Initialize the session
    mfxStatus Init(mfxIMPL implInterface, mfxVersion *ver);

    // Attach to the original scheduler
    mfxStatus RestoreScheduler(void);
    // Release current scheduler
    mfxStatus ReleaseScheduler(void);

    // Declare session's components

    // It's important to keep VideoCORE object preceding components objects,
    // so it will guarantee correct destruction of surfaces: cache first, then allocator:
    // allocator is kept in CORE and each component have it's own SurfaceCache object
    std::shared_ptr<VideoCORE>   m_pCORE;

    std::unique_ptr<VideoENCODE> m_pENCODE;
    std::unique_ptr<VideoDECODE> m_pDECODE;
    std::unique_ptr<VideoVPP>    m_pVPP;

    class DVP_base
    {
    public:
        virtual ~DVP_base() {}
        virtual surface_cache_controller<SurfaceCache>* GetSurfacePool(mfxU16 /*channel_id*/) { return nullptr; }
        virtual void AssignPool(mfxU16 /*channel_id*/, SurfaceCache* /*cache*/) {}
        //channel ID / VPP config
        std::map<mfxU16, mfxVideoParam>                     VppParams;
        //channel ID / VPP component
        std::map<mfxU16, std::unique_ptr<VideoVPP>>         VPPs;
        //channel ID for VDBOX + SFC channel, 0 means VDBOX + SFC is not used
        mfxU16 sfcChannelID = 0;
        //true means keep only processed outputs
        bool   skipOriginalOutput;
    };
    std::unique_ptr<DVP_base>   m_pDVP;

    // Current implementation platform ID
    eMFXPlatform m_currentPlatform;
    // Current working HW adapter
    mfxU32 m_adapterNum;
    // Current working interface (D3D9 or so)
    mfxIMPL m_implInterface;

    // Pointer to the scheduler interface being used
    MFXIScheduler *m_pScheduler;
    // Priority of the given session instance
    mfxPriority m_priority;
    // API version requested by application
    mfxVersion  m_version;
    // API version to report from MFXQueryVersion
    mfxVersion  m_versionToReport;

    MFXIPtr<OperatorCORE> m_pOperatorCore;

    // if there are no Enc HW capabilities but HW library is used
    bool m_bIsHWENCSupport;

    // if there are no Dec HW capabilities but HW library is used
    bool m_bIsHWDECSupport;


    inline
    bool IsParentSession(void)
    {
        // if a session has m_pSchedulerAllocated != NULL
        // and the number of "Cores" > 1, then it's a parrent session.
        // and the number of "Cores" == 1, then it's a regular session.
        if (m_pSchedulerAllocated)
            return m_pOperatorCore->HaveJoinedSessions();
        else
            return false;
    }

    inline
    bool IsChildSession(void)
    {
        // child session has different references to active and allocated
        // scheduler. regular session has 2 references to the scheduler.
        // child session has only 1 reference to it.
        return (NULL == m_pSchedulerAllocated);
    }
    
    template<class T>
    T* Create(mfxVideoParam& par);

protected:
    // Release the object
    void Cleanup(void);

    // this variable is used to deteremine
    // if the object really owns the scheduler.
    MFXIUnknown *m_pSchedulerAllocated;                         // (MFXIUnknown *) pointer to the scheduler allocated

private:
    // Assignment operator is forbidden
    _mfxSession & operator = (const _mfxSession &);
};

// {90567606-C57A-447F-8941-1F14597DA475}
static const 
    MFX_GUID  MFXISession_1_10_GUID = {0x90567606, 0xc57a, 0x447f, {0x89, 0x41, 0x1f, 0x14, 0x59, 0x7d, 0xa4, 0x75}};

// {701A88BB-E482-4374-A08D-621641EC98B2}
//DEFINE_GUID(<<name>>, 
//            {0x701a88bb, 0xe482, 0x4374, {0xa0, 0x8d, 0x62, 0x16, 0x41, 0xec, 0x98, 0xb2}};

class MFXISession_1_10: public MFXIUnknown
{
public:
    virtual ~MFXISession_1_10() {}

    // Finish initialization. Should be called before Init().
    virtual void SetAdapterNum(const mfxU32 adapterNum) = 0;

};

MFXIPtr<MFXISession_1_10> TryGetSession_1_10(mfxSession session);

//--- MFX SESIION 1.33 ---------------------------------------------------------------------------

// {A42A8B5B-162F-4ACC-B51D-F7B24352BDCD}
static const MFX_GUID MFXISession_2_1_GUID =
{ 0xa42a8b5b, 0x162f, 0x4acc, { 0xb5, 0x1d, 0xf7, 0xb2, 0x43, 0x52, 0xbd, 0xcd } };

class MFXISession_2_1 : public MFXISession_1_10
{
public:
    virtual ~MFXISession_2_1() {}

};

MFXIPtr<MFXISession_2_1> TryGetSession_2_1(mfxSession session);

class _mfxVersionedSessionImpl: public _mfxSession, public MFXISession_2_1
{
public:
    _mfxVersionedSessionImpl(mfxU32 adapterNum);

    // Destructor
    virtual ~_mfxVersionedSessionImpl(void);

    //--- MFXIUnknown interface -----------------------------------------------------------------------

    // Query another interface from the object. If the pointer returned is not NULL,
    // the reference counter is incremented automatically.
    virtual void *QueryInterface(const MFX_GUID &guid) override;

    // Increment reference counter of the object.
    virtual void AddRef(void) override;
    // Decrement reference counter of the object.
    // If the counter is equal to zero, destructor is called and
    // object is removed from the memory.
    virtual void Release(void) override;
    // Get the current reference counter value
    virtual mfxU32 GetNumRef(void) const override;

    /// Initialize session
    /// @param par - initialization parameters
    /// @return MFX_ERROR_NONE if completed successfully, error code otherwise.
    virtual mfxStatus InitEx(mfxInitParam& par, bool isSingleThreadMode, bool bValidateHandle);

    //--- MFXISession_1_9 interface -------------------------------------------------------------------

    void SetAdapterNum(const mfxU32 adapterNum) override;

protected:
    // Reference counters
    mfxU32 m_refCounter;
    mfxU16 m_externalThreads;
};

//
// DEFINES FOR IMPLICIT FUNCTIONS IMPLEMENTATION
//

#undef FUNCTION_IMPL
#define FUNCTION_IMPL(component, func_name, formal_param_list, actual_param_list) \
mfxStatus APIImpl_MFXVideo##component##_##func_name formal_param_list \
{ \
    PERF_UTILITY_AUTO(__FUNCTION__, PERF_LEVEL_API); \
    MFX_LOG_API_TRACE("----------------MFXVideo" #component "_" #func_name "----------------\n"); \
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE); \
    MFX_CHECK(session->m_p##component.get(), MFX_ERR_NOT_INITIALIZED); \
    try { \
        /* call the codec's method */ \
        return session->m_p##component->func_name actual_param_list; \
    } catch(...) { \
        return MFX_ERR_NULL_PTR; \
    } \
}

#undef FUNCTION_RESET_IMPL
#define FUNCTION_RESET_IMPL(component, func_name, formal_param_list, actual_param_list) \
mfxStatus APIImpl_MFXVideo##component##_##func_name formal_param_list \
{ \
    PERF_UTILITY_AUTO(__FUNCTION__, PERF_LEVEL_API); \
    MFX_LOG_API_TRACE("----------------MFXVideo" #component "_" #func_name "----------------\n"); \
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE); \
    MFX_CHECK(session->m_p##component.get(), MFX_ERR_NOT_INITIALIZED); \
                                                                                         \
    try { \
        /* wait until all tasks are processed */ \
        MFX_SAFE_CALL(session->m_p##component ->ResetCache actual_param_list ); \
        session->m_pScheduler->WaitForAllTasksCompletion(session->m_p##component.get()); \
        /* call the codec's method */ \
        return session->m_p##component->func_name actual_param_list; \
    } catch(...) { \
        return MFX_ERR_NULL_PTR; \
    } \
}

mfxStatus MFXInternalPseudoJoinSession(mfxSession session, mfxSession child_session);
mfxStatus MFXInternalPseudoDisjoinSession(mfxSession session);

#endif // _MFX_SESSION_H

