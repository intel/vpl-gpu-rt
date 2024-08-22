// Copyright (c) 2008-2024 Intel Corporation
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

#include <assert.h>
#include <thread>
#include "mfx_common.h"
#include <mfx_session.h>

#include <libmfx_core_factory.h>
#include <libmfx_core.h>

#include <libmfx_core_vaapi.h>
#include "mfx_platform_caps.h"

#define TRY_GET_SESSION(verMax,verMin) MFXIPtr<MFXISession_##verMax##_##verMin> TryGetSession_##verMax##_##verMin(mfxSession session) \
{ \
    if (session == NULL)\
    { \
        return MFXIPtr<MFXISession_##verMax##_##verMin>(); \
    } \
    return MFXIPtr<MFXISession_##verMax##_##verMin>(static_cast<_mfxVersionedSessionImpl *>(session)->QueryInterface(MFXISession_##verMax##_##verMin##_GUID)); \
}

constexpr mfxU32 default_threads_limit = 16;

TRY_GET_SESSION(1,10)
TRY_GET_SESSION(2,1)

//////////////////////////////////////////////////////////////////////////
//  _mfxSession members
//////////////////////////////////////////////////////////////////////////

_mfxSession::_mfxSession(const mfxU32 adapterNum)
    : m_currentPlatform()
    , m_adapterNum(adapterNum)
    , m_implInterface()
    , m_pScheduler()
    , m_priority()
    , m_version()
    , m_versionToReport()
    , m_pOperatorCore()
    , m_bIsHWENCSupport()
    , m_bIsHWDECSupport()
{
    m_currentPlatform = MFX_PLATFORM_HARDWARE;

    m_versionToReport.Major = MFX_VERSION_MAJOR;
    m_versionToReport.Minor = MFX_VERSION_MINOR;

    Clear();
} // _mfxSession::_mfxSession(const mfxU32 adapterNum) :

_mfxSession::~_mfxSession(void)
{
    Cleanup();

} // _mfxSession::~_mfxSession(void)

void _mfxSession::Clear(void)
{
    m_pScheduler = NULL;
    m_pSchedulerAllocated = NULL;

    m_priority = MFX_PRIORITY_NORMAL;
    m_bIsHWENCSupport = false;

} // void _mfxSession::Clear(void)

void _mfxSession::Cleanup(void)
{
    // wait until all task are processed
    if (m_pScheduler)
    {
        if (m_pDECODE.get())
            m_pScheduler->WaitForAllTasksCompletion(m_pDECODE.get());
        if (m_pVPP.get())
            m_pScheduler->WaitForAllTasksCompletion(m_pVPP.get());
        if (m_pENCODE.get())
            m_pScheduler->WaitForAllTasksCompletion(m_pENCODE.get());
    }

    // release the components the excplicit way.
    // do not relay on default deallocation order,
    // somebody could change it.

    m_pVPP.reset();
    m_pDECODE.reset();
    m_pENCODE.reset();
    m_pDVP.reset();

    // release m_pScheduler and m_pSchedulerAllocated
    ReleaseScheduler();

    // release core
    m_pCORE.reset();

    //delete m_coreInt.ExternalSurfaceAllocator;
    Clear();

} // void _mfxSession::Release(void)

mfxStatus _mfxSession::Init(mfxIMPL implInterface, mfxVersion *ver)
{
    mfxStatus mfxRes;
    MFX_SCHEDULER_PARAM schedParam;
    mfxU32 maxNumThreads;

    // release the object before initialization
    Cleanup();

    if (ver)
    {
        m_version = *ver;
    }
    else
    {
        mfxStatus sts = MFXQueryVersion(this, &m_version);
        if (sts != MFX_ERR_NONE)
            return sts;
    }

    // save working HW interface
    switch (implInterface&-MFX_IMPL_VIA_ANY)
    {
    case MFX_IMPL_UNSUPPORTED:
        assert(!"MFXInit(Ex) was supposed to correct zero-impl to MFX_IMPL_VIA_ANY");
        return MFX_ERR_UNDEFINED_BEHAVIOR;
        // VAAPI is only one supported interface
    case MFX_IMPL_VIA_ANY:
    case MFX_IMPL_VIA_VAAPI:
        m_implInterface = MFX_IMPL_VIA_VAAPI;
        break;

    // unknown hardware interface
    default:
        if (MFX_PLATFORM_HARDWARE == m_currentPlatform)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    // get the number of available threads
    maxNumThreads = std::min((mfxU32)default_threads_limit, (mfxU32)std::thread::hardware_concurrency());
    if (maxNumThreads == 1) {
        maxNumThreads = 2;
    }

    // allocate video core
    if (MFX_PLATFORM_SOFTWARE == m_currentPlatform)
    {
        m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_NO, 0, {}, maxNumThreads, this));
    }

    else
    {
        m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_VAAPI, m_adapterNum, {}, maxNumThreads, this));
    }


    // query the scheduler interface
    m_pScheduler = QueryInterface<MFXIScheduler> (m_pSchedulerAllocated,
                                                  MFXIScheduler_GUID);
    if (NULL == m_pScheduler)
    {
        return MFX_ERR_UNKNOWN;
    }
    memset(&schedParam, 0, sizeof(schedParam));
    schedParam.flags = MFX_SCHEDULER_DEFAULT;
    schedParam.numberOfThreads = maxNumThreads;
    schedParam.pCore = m_pCORE.get();
    mfxRes = m_pScheduler->Initialize(&schedParam);
    if (MFX_ERR_NONE != mfxRes)
    {
        return mfxRes;
    }

    m_pOperatorCore = new OperatorCORE(m_pCORE.get());

    return MFX_ERR_NONE;

} // mfxStatus _mfxSession::Init(mfxIMPL implInterface)

mfxStatus _mfxSession::RestoreScheduler(void)
{
    if(m_pSchedulerAllocated)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    // leave the current scheduler
    if (m_pScheduler)
    {
        m_pScheduler->Release();
        m_pScheduler = NULL;
    }

    // query the scheduler interface
    m_pScheduler = QueryInterface<MFXIScheduler> (m_pSchedulerAllocated,
                                                  MFXIScheduler_GUID);
    if (NULL == m_pScheduler)
    {
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;

} // mfxStatus _mfxSession::RestoreScheduler(void)

mfxStatus _mfxSession::ReleaseScheduler(void)
{
    if(m_pScheduler)
        m_pScheduler->Release();

    if(m_pSchedulerAllocated)
        m_pSchedulerAllocated->Release();

    m_pScheduler = nullptr;
    m_pSchedulerAllocated = nullptr;

    return MFX_ERR_NONE;

} // mfxStatus _mfxSession::RestoreScheduler(void)

//////////////////////////////////////////////////////////////////////////
// _mfxVersionedSessionImpl own members
//////////////////////////////////////////////////////////////////////////

_mfxVersionedSessionImpl::_mfxVersionedSessionImpl(mfxU32 adapterNum)
    : _mfxSession(adapterNum)
    , m_refCounter(1)
    , m_externalThreads(0)
{
}

_mfxVersionedSessionImpl::~_mfxVersionedSessionImpl(void)
{
}

//////////////////////////////////////////////////////////////////////////
// _mfxVersionedSessionImpl::MFXISession_1_10 members
//////////////////////////////////////////////////////////////////////////


void _mfxVersionedSessionImpl::SetAdapterNum(const mfxU32 adapterNum)
{
    m_adapterNum = adapterNum;
}

//////////////////////////////////////////////////////////////////////////
// _mfxVersionedSessionImpl::MFXIUnknown members
//////////////////////////////////////////////////////////////////////////

void *_mfxVersionedSessionImpl::QueryInterface(const MFX_GUID &guid)
{
    // Specific interface is required
    if (MFXISession_1_10_GUID == guid)
    {
        // increment reference counter
        vm_interlocked_inc32(&m_refCounter);

        return (MFXISession_1_10 *) this;
    }

    // Specific interface is required
    if (MFXISession_2_1_GUID == guid)
    {
        // increment reference counter
        vm_interlocked_inc32(&m_refCounter);

        return (MFXISession_2_1 *)this;
    }

    // it is unsupported interface
    return NULL;
} // void *_mfxVersionedSessionImpl::QueryInterface(const MFX_GUID &guid)

void _mfxVersionedSessionImpl::AddRef(void)
{
    // increment reference counter
    vm_interlocked_inc32(&m_refCounter);

} // void mfxSchedulerCore::AddRef(void)

void _mfxVersionedSessionImpl::Release(void)
{
    // decrement reference counter
    vm_interlocked_dec32(&m_refCounter);

    if (0 == m_refCounter)
    {
        delete this;
    }

} // void _mfxVersionedSessionImpl::Release(void)

mfxU32 _mfxVersionedSessionImpl::GetNumRef(void) const
{
    return m_refCounter;

} // mfxU32 _mfxVersionedSessionImpl::GetNumRef(void) const


mfxStatus _mfxVersionedSessionImpl::InitEx(mfxInitParam& par, bool isSingleThreadMode = false, bool bValidateHandle = false)
{
    mfxStatus mfxRes;
    mfxU32 maxNumThreads;

    // release the object before initialization
    Cleanup();

    m_version = par.Version;

    // save working HW interface
    switch (par.Implementation&-MFX_IMPL_VIA_ANY)
    {
    case MFX_IMPL_UNSUPPORTED:
        assert(!"MFXInit(Ex) was supposed to correct zero-impl to MFX_IMPL_VIA_ANY");
        return MFX_ERR_UNDEFINED_BEHAVIOR;
        // VAAPI is only one supported interface
    case MFX_IMPL_VIA_ANY:
    case MFX_IMPL_VIA_VAAPI:
        m_implInterface = MFX_IMPL_VIA_VAAPI;
        break;

    // unknown hardware interface
    default:
        if (MFX_PLATFORM_HARDWARE == m_currentPlatform)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (par.NumExtParam)
    {
        MFX_CHECK(par.ExtParam, MFX_ERR_UNSUPPORTED);
    }

    // get the number of available threads
    maxNumThreads = 0;
    if (par.ExternalThreads == 0)
    {
        maxNumThreads = std::min((mfxU32)default_threads_limit, (mfxU32)std::thread::hardware_concurrency());
        if (maxNumThreads == 1)
        {
            maxNumThreads = 2;
        }
    }

    auto IsMaskValid = [](const std::pair<mfxU32, std::vector<mfxU8>> & affinityMask) -> bool
    {
        if (!affinityMask.first || affinityMask.first > 15) // va supports 16 sub-devices
            return false;

        mfxU8 byteIdx = 0;
        mfxU32 numPowerOfTwo = 0;
        for (auto val : affinityMask.second)
        {
            const auto length = affinityMask.first - (8*byteIdx);
            val &= (1<<length) - 1;
            numPowerOfTwo += !!(val && (!(val & (val - 1))));
            ++byteIdx;
        }
        return (numPowerOfTwo == 1);

    };

    auto extAffinityMask = (mfxExtDeviceAffinityMask*) mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_DEVICE_AFFINITY_MASK);
    std::pair<mfxU32, std::vector<mfxU8>> affinityMask;
    if (extAffinityMask)
    {
        std::vector<mfxU8> mask(extAffinityMask->Mask, extAffinityMask->Mask + (extAffinityMask->NumSubDevices + 7) / 8);
        affinityMask = {extAffinityMask->NumSubDevices, std::move(mask)};
        MFX_CHECK(IsMaskValid(affinityMask), MFX_ERR_UNSUPPORTED);
    }

    // allocate video core
    if (MFX_PLATFORM_SOFTWARE == m_currentPlatform)
    {
        m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_NO, 0, {}, maxNumThreads, this));
    }
    else
    {
        m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_VAAPI, m_adapterNum, affinityMask, maxNumThreads, this));
    }

    // query the scheduler interface
    m_pScheduler = ::QueryInterface<MFXIScheduler>(m_pSchedulerAllocated, MFXIScheduler_GUID);
    if (NULL == m_pScheduler)
    {
        return MFX_ERR_UNKNOWN;
    }

    MFXIScheduler2* pScheduler2 = ::QueryInterface<MFXIScheduler2>(m_pSchedulerAllocated, MFXIScheduler2_GUID);

    auto threadsParam = (mfxExtThreadsParam*) mfx::GetExtBuffer(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_THREADS_PARAM);
    if (threadsParam && !pScheduler2)
    {
        return MFX_ERR_UNKNOWN;
    }

    if (pScheduler2) {
        MFX_SCHEDULER_PARAM2 schedParam;
        memset(&schedParam, 0, sizeof(schedParam));
        schedParam.flags = MFX_SCHEDULER_DEFAULT;
#if defined(MFX_ENABLE_SINGLE_THREAD)
        if (isSingleThreadMode)
            schedParam.flags = MFX_SINGLE_THREAD;
#endif
        schedParam.numberOfThreads = maxNumThreads;
        schedParam.pCore = m_pCORE.get();
        if (threadsParam)
        {
            schedParam.params = *threadsParam;
        }
        mfxRes = pScheduler2->Initialize2(&schedParam);

        m_pScheduler->Release();
    }
    else {
        MFX_SCHEDULER_PARAM schedParam;
        memset(&schedParam, 0, sizeof(schedParam));
        schedParam.flags = MFX_SCHEDULER_DEFAULT;
#if defined(MFX_ENABLE_SINGLE_THREAD)
        if (isSingleThreadMode)
            schedParam.flags = MFX_SINGLE_THREAD;
#endif
        schedParam.numberOfThreads = maxNumThreads;
        schedParam.pCore = m_pCORE.get();
        mfxRes = m_pScheduler->Initialize(&schedParam);
    }

    if (MFX_ERR_NONE != mfxRes) {
        return mfxRes;
    }

    m_pOperatorCore = new OperatorCORE(m_pCORE.get());

    auto IsGPUcopyValid = [](mfxU16 gpu_copy) -> bool
    {
        return gpu_copy == MFX_GPUCOPY_DEFAULT
            || gpu_copy == MFX_GPUCOPY_ON
            || gpu_copy == MFX_GPUCOPY_OFF
            || gpu_copy == MFX_GPUCOPY_SAFE
#ifdef ONEVPL_EXPERIMENTAL
            || gpu_copy == MFX_GPUCOPY_FAST
#endif
            ;
    };

    MFX_CHECK(IsGPUcopyValid(par.GPUCopy), MFX_ERR_UNSUPPORTED);

    if (MFX_PLATFORM_SOFTWARE == m_currentPlatform && (MFX_GPUCOPY_ON   == par.GPUCopy
                                                    || MFX_GPUCOPY_SAFE == par.GPUCopy
#ifdef ONEVPL_EXPERIMENTAL
                                                    || MFX_GPUCOPY_FAST == par.GPUCopy
#endif
    ))
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    if (par.GPUCopy != MFX_GPUCOPY_DEFAULT)
    {
        CMEnabledCoreInterface* pCmCore = QueryCoreInterface<CMEnabledCoreInterface>(m_pCORE.get());
        if (pCmCore)
        {
            pCmCore->SetCmCopyMode(par.GPUCopy);
        }
    }

    return MFX_ERR_NONE;
} // mfxStatus _mfxVersionedSessionImpl::InitEx(mfxInitParam& par);

//explicit specification of interface creation
template<> MFXISession_1_10*  CreateInterfaceInstance<MFXISession_1_10>(const MFX_GUID &guid)
{
    if (MFXISession_1_10_GUID == guid)
        return (MFXISession_1_10*) (new _mfxVersionedSessionImpl(0));

    return NULL;
}

template<> MFXISession_2_1*  CreateInterfaceInstance<MFXISession_2_1>(const MFX_GUID &guid)
{
    if (MFXISession_2_1_GUID == guid)
        return (MFXISession_2_1*)(new _mfxVersionedSessionImpl(0));

    return NULL;
}

namespace MFX
{
    unsigned int CreateUniqId()
    {
        static volatile mfxU32 g_tasksId = 0;
        return (unsigned int)vm_interlocked_inc32(&g_tasksId);
    }
}
