// Copyright (c) 2007-2024 Intel Corporation
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

#include <mfxvideo.h>

#include <mfx_session.h>
#include <mfximplcaps.h>

#include <mfx_trace.h>

#include <ippcore.h>
#include <set>

#include <unistd.h>
#include <fcntl.h>
#include <xf86drm.h>
#include "va/va_drm.h"

#include "mediasdk_version.h"
#include "libmfx_core_factory.h"
#include "mfx_interface_scheduler.h"
#include "libmfx_core_interface.h"
#include "mfx_platform_caps.h"

#include "mfx_unified_decode_logging.h"

mfxStatus MFXInit(mfxIMPL implParam, mfxVersion *ver, mfxSession *session)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  implParam = ", MFX_TRACE_FORMAT_I, implParam);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  session = ", MFX_TRACE_FORMAT_P, session);

    mfxInitParam par = {};

    par.Implementation = implParam;
    if (ver)
    {
        par.Version = *ver;
    }
    else
    {
        par.Version.Major = 1;
        par.Version.Minor = 255;
    }
    par.ExternalThreads = 0;

    MFX_TRACE_2("Out:  MFX_API version = ", "%d.%d", par.Version.Major, par.Version.Minor);

    return MFXInitEx(par, session);

} // mfxStatus MFXInit(mfxIMPL impl, mfxVersion *ver, mfxHDL *session)

static inline mfxU32 MakeVersion(mfxU16 major, mfxU16 minor)
{
    return major * 1000 + minor;
}

static mfxStatus MFXInit_Internal(mfxInitParam par, mfxSession* session, mfxIMPL implInterface, mfxU32 adapterNum, bool isSingleThreadMode = false, bool bValidateHandle = false);

mfxStatus MFXInitEx(mfxInitParam par, mfxSession *session)
{
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  Implementation = ", MFX_TRACE_FORMAT_I, par.Implementation);
    MFX_TRACE_2("In:  MFX_API version = ", "%d.%d", par.Version.Major, par.Version.Minor);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  session = ", MFX_TRACE_FORMAT_P, session);
    mfxStatus mfxRes = MFX_ERR_NONE;
    int adapterNum = 0;
    mfxIMPL impl = par.Implementation & (MFX_IMPL_VIA_ANY - 1);
    mfxIMPL implInterface = par.Implementation & -MFX_IMPL_VIA_ANY;

    MFX_TRACE_INIT();
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "ThreadName=MSDK app");
    }
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);

    TRACE_EVENT(MFX_TRACE_API_MFX_INIT_EX_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data((mfxU32)par.Implementation, par.GPUCopy));

    // check the library version
    if (MakeVersion(par.Version.Major, par.Version.Minor) > MFX_VERSION)
    {
        mfxRes = MFX_ERR_UNSUPPORTED;
        MFX_RETURN(mfxRes);
    }

    // check error(s)
    if ((MFX_IMPL_AUTO != impl) &&
        (MFX_IMPL_AUTO_ANY != impl) &&
        (MFX_IMPL_HARDWARE_ANY != impl) &&
        (MFX_IMPL_HARDWARE != impl) &&
        (MFX_IMPL_HARDWARE2 != impl) &&
        (MFX_IMPL_HARDWARE3 != impl) &&
        (MFX_IMPL_HARDWARE4 != impl))
    {
        mfxRes = MFX_ERR_UNSUPPORTED;
        MFX_RETURN(mfxRes);
    }

    // if user did not specify MFX_IMPL_VIA_* treat it as MFX_IMPL_VIA_ANY
    if (!implInterface)
        implInterface = MFX_IMPL_VIA_ANY;

    if (
        (MFX_IMPL_VIA_VAAPI != implInterface) &&
        (MFX_IMPL_VIA_ANY != implInterface))
    {
        mfxRes = MFX_ERR_UNSUPPORTED;
        MFX_RETURN(mfxRes);
    }

    // set the adapter number
    switch (impl)
    {
    case MFX_IMPL_HARDWARE2:
        adapterNum = 1;
        break;

    case MFX_IMPL_HARDWARE3:
        adapterNum = 2;
        break;

    case MFX_IMPL_HARDWARE4:
        adapterNum = 3;
        break;

    default:
        adapterNum = 0;
        break;
    }

    // MFXInit / MFXInitEx in Intel VPL is for work in legacy (1.x) mode only
    // app. must use MFXInitialize for 2.x features
    if (par.Version.Major > 1)
    {
        mfxRes = MFX_ERR_UNSUPPORTED;
        MFX_RETURN(mfxRes);
    }

    mfxRes = MFXInit_Internal(par, session, implInterface, adapterNum);

    // Intel VPL should report 1.255 API version when it was initialized through MFXInit / MFXInitEx
    if (session && *session && mfxRes >= MFX_ERR_NONE)
    {
        (*session)->m_versionToReport.Major = 1;
        (*session)->m_versionToReport.Minor = 255;
    }

    TRACE_EVENT(MFX_TRACE_API_MFX_INIT_EX_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(mfxRes, session));
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;

} // mfxStatus MFXInitEx(mfxInitParam par, mfxSession *session)

static mfxStatus MFXInit_Internal(mfxInitParam par, mfxSession *session, mfxIMPL implInterface, mfxU32 adapterNum, bool isSingleThreadMode, bool bValidateHandle)
{

    MFX_CHECK_HDL(session);
    _mfxVersionedSessionImpl* pSession = nullptr;
    mfxStatus                 mfxRes   = MFX_ERR_NONE;

    try
    {
        // reset output variable
        *session = 0;
        // prepare initialization parameters

        // create new session instance
        pSession = new _mfxVersionedSessionImpl(adapterNum);

        mfxInitParam init_param = par;
        init_param.Implementation = implInterface;

        mfxRes = pSession->InitEx(init_param, isSingleThreadMode, bValidateHandle);
    }
    catch(...)
    {
        mfxRes = MFX_ERR_MEMORY_ALLOC;
    }
    if (MFX_ERR_NONE != mfxRes &&
        MFX_WRN_PARTIAL_ACCELERATION != mfxRes)
    {
        if (pSession)
        {
            delete pSession;
        }

        return mfxRes;
    }

    // save the handle
    *session = dynamic_cast<_mfxSession *>(pSession);

    return mfxRes;

} // mfxStatus MFXInit_Internal(mfxInitParam par, mfxSession *session, mfxIMPL implInterface, mfxU32 adapterNum)

mfxStatus MFXDoWork(mfxSession session)
{
    mfxStatus res;

    try
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
        TRACE_EVENT(MFX_TRACE_API_DO_WORK_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data(session));

        // check error(s)
        if (0 == session)
        {
            MFX_RETURN(MFX_ERR_INVALID_HANDLE);
        }

        MFXIUnknown* pInt = session->m_pScheduler;
        MFXIScheduler2* newScheduler =
            ::QueryInterface<MFXIScheduler2>(pInt, MFXIScheduler2_GUID);

        if (!newScheduler)
        {
            if (!session->m_pScheduler && pInt) // if created in QueryInterface
            {
                pInt->Release();
                pInt = NULL;
            }
            MFX_RETURN(MFX_ERR_UNSUPPORTED);
        }

        res = newScheduler->DoWork();

        TRACE_EVENT(MFX_TRACE_API_DO_WORK_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(res));

        newScheduler->Release();
    }
    catch (...)
    {
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }

    return res;
} // mfxStatus MFXDoWork(mfxSession *session)

struct MFXTrace_EventCloseOnExit
{
    ~MFXTrace_EventCloseOnExit()
    {
        MFXTrace_EventClose();
    }
};

mfxStatus MFXClose(mfxSession session)
{
    PERF_UTILITY_AUTO("MFXClose", PERF_LEVEL_API);
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFXTrace_EventCloseOnExit MFXTrace_EventClose;

    TRACE_EVENT(MFX_TRACE_API_MFX_CLOSE_TASK, EVENT_TYPE_START, 0, make_event_data(session));
    // check error(s)
    if (0 == session)
    {
        MFX_RETURN(MFX_ERR_INVALID_HANDLE);
    }

    try
    {
        // NOTE MFXClose function calls MFX_TRACE_CLOSE, so no tracing points should be
        // used after it. special care should be taken with MFX_AUTO_TRACE macro
        // since it inserts class variable on stack which calls to trace library in the
        // destructor.
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
        MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  session = ", MFX_TRACE_FORMAT_P, session);
        // parent session can't be closed,
        // because there is no way to let children know about parent's death.

        // child session should be uncoupled from the parent before closing.
        if (session->IsChildSession())
        {
            mfxRes = MFXDisjoinSession(session);

            if (MFX_ERR_NONE != mfxRes)
            {
                return mfxRes;
            }
        }

        if (session->IsParentSession())
        {
            MFX_RETURN(MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        // deallocate the object
        _mfxVersionedSessionImpl *newSession  = (_mfxVersionedSessionImpl *)session;
        delete newSession;
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }
#if defined(MFX_TRACE_ENABLE)
    MFX_TRACE_CLOSE();
#endif
    TRACE_EVENT(MFX_TRACE_API_MFX_CLOSE_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(mfxRes));

    try
    {
        MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    }
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    return mfxRes;

} // mfxStatus MFXClose(mfxHDL session)

mfxStatus MFX_CDECL MFXInitialize(mfxInitializationParam param, mfxSession* session)
{

    mfxStatus mfxRes = MFX_ERR_NONE;

    try
    {
        MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  session = ", MFX_TRACE_FORMAT_P, session);

        MFX_TRACE_INIT();
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
        TRACE_EVENT(MFX_TRACE_API_MFXINITIALIZE_TASK, EVENT_TYPE_START, TR_KEY_MFX_API, make_event_data((mfxU32)param.AccelerationMode, param.VendorImplID));

        mfxInitParam par = {};

        par.Implementation = MFX_IMPL_HARDWARE;
        switch (param.AccelerationMode)
        {
        case MFX_ACCEL_MODE_VIA_D3D9:
            par.Implementation |= MFX_IMPL_VIA_D3D9;
            break;
        case MFX_ACCEL_MODE_VIA_D3D11:
            par.Implementation |= MFX_IMPL_VIA_D3D11;
            break;
        case MFX_ACCEL_MODE_VIA_VAAPI:
            par.Implementation |= MFX_IMPL_VIA_VAAPI;
            break;
        default:
            MFX_RETURN(MFX_ERR_UNSUPPORTED);
        }

        par.Version.Major = MFX_VERSION_MAJOR;
        par.Version.Minor = MFX_VERSION_MINOR;
        par.ExternalThreads = 0;
        par.NumExtParam = param.NumExtParam;
        par.ExtParam = param.ExtParam;

        bool isSingleThreadMode = false;

#if defined(MFX_ENABLE_SINGLE_THREAD)
        std::vector<mfxExtBuffer*> buffers;

        if (par.NumExtParam > 0)
        {
            MFX_CHECK_NULL_PTR1(par.ExtParam);

            buffers.reserve(par.NumExtParam);

            std::remove_copy_if(par.ExtParam, par.ExtParam + par.NumExtParam, std::back_inserter(buffers),
                [](const mfxExtBuffer* buf)
                {
                    return buf && buf->BufferId == MFX_EXTBUFF_THREADS_PARAM && (reinterpret_cast<const mfxExtThreadsParam*>(buf))->NumThread == 0;
                }
            );

            //here in 'buffers' we have all ext. buffers excepting MFX_EXTBUFF_THREADS_PARAM
            isSingleThreadMode = buffers.size() != par.NumExtParam;

            if (isSingleThreadMode)
            {
                par.NumExtParam = static_cast<mfxU16>(buffers.size());
                par.ExtParam = par.NumExtParam > 0 ? buffers.data() : nullptr;
            }
        }
#endif

#ifdef ONEVPL_EXPERIMENTAL
        par.GPUCopy = param.DeviceCopy;
#endif

        // VendorImplID is used as adapterNum in current implementation - see MFXQueryImplsDescription
        // app. supposed just to copy VendorImplID from mfxImplDescription (returned by MFXQueryImplsDescription) to mfxInitializationParam
        mfxRes = MFXInit_Internal(par, session, par.Implementation, param.VendorImplID, isSingleThreadMode);

        TRACE_EVENT(MFX_TRACE_API_MFXINITIALIZE_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(par.Implementation, isSingleThreadMode));
        MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    }
    catch (...)
    {
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }

    return mfxRes;
}

namespace mfx
{
    struct ImplCapsCommon
    {
        virtual ~ImplCapsCommon() {};
        static void Release(mfxHDL hdl)
        {
            auto p = (ImplCapsCommon*)((mfxU8*)hdl - sizeof(ImplCapsCommon));
            delete p;
        }
        template<class T>
        static mfxHDL GetHDL(T& caps)
        {
             return mfxHDL((mfxU8*)&caps + sizeof(ImplCapsCommon));
        }
    };

    template <class T>
    class DescriptionHolder
    {
    public:

        friend T;
        T& PushBack()
        {
            m_impls.emplace_back(new T(this));
            m_implsArray.push_back(ImplCapsCommon::GetHDL(*m_impls.back()));
            return *m_impls.back();
        }

        mfxHDL* GetArray() { return m_implsArray.data(); }

        size_t  GetSize() { return m_implsArray.size(); }

        void Detach()
        {
            m_ref = m_impls.size();
            for (auto& up : m_impls)
                up.release();
            m_impls.clear();
        }

    protected:
        std::vector<mfxHDL> m_implsArray;
        std::list<std::unique_ptr<T>> m_impls;
        size_t m_ref = 0;

        void Release()
        {
            if (m_impls.empty() && m_ref-- <= 1)
                delete this;
        }
    };

    class ImplDescription;
    using ImplDescriptionHolder = DescriptionHolder<ImplDescription>;

    class ImplDescription
        : public ImplCapsCommon
        , public mfxImplDescription
        , public PODArraysHolder
    {
    public:
        ImplDescription(ImplDescriptionHolder* h)
            : mfxImplDescription()
            , m_pHolder(h)
        {
        }
        ~ImplDescription();
    protected:
        ImplDescriptionHolder* m_pHolder;
    };

    ImplDescription::~ImplDescription()
    {
        if (m_pHolder)
            m_pHolder->Release();
    }

#ifdef ONEVPL_EXPERIMENTAL
    class ExtendedDeviceID;
    using ExtendedDeviceIDHolder = DescriptionHolder<ExtendedDeviceID>;

    class ExtendedDeviceID
        : public ImplCapsCommon
        , public mfxExtendedDeviceId
        , public PODArraysHolder
    {
    public:
        ExtendedDeviceID(ExtendedDeviceIDHolder* h)
            : mfxExtendedDeviceId()
            , m_pHolder(h)
        {
        }
        ~ExtendedDeviceID();
    protected:
        ExtendedDeviceIDHolder* m_pHolder;
    };

    ExtendedDeviceID::~ExtendedDeviceID()
    {
        if (m_pHolder)
            m_pHolder->Release();
    }

    class SurfaceTypesSupported
        : public ImplCapsCommon
        , public mfxSurfaceTypesSupported
        , public PODArraysHolder
    {
    public:
        SurfaceTypesSupported()
            : mfxSurfaceTypesSupported()
        {
            Version.Version = MFX_SURFACETYPESSUPPORTED_VERSION;

            m_pthis = ImplCapsCommon::GetHDL(*this);
        }

        mfxHDL* GetHDL()
        {
            return &m_pthis;
        }

    private:
        mfxHDL m_pthis = nullptr;
    };
#endif // ONEVPL_EXPERIMENTAL

    class ImplFunctions
        : public ImplCapsCommon
        , public mfxImplementedFunctions
    {
    public:
        template<typename T>
        void PushFuncName(T&& name)
        {
            m_funcNames.emplace_back(std::vector<mfxChar>(std::begin(name), std::end(name)));
        }

        ImplFunctions()
        {
#define MFX_API_FUNCTION_IMPL(NAME, RTYPE, ARGS_DECL, ARGS) PushFuncName(#NAME);
#undef _MFX_FUNCTIONS_H_
#include "mfx_functions.h"

            m_funcArray.resize(m_funcNames.size(), nullptr);
            std::transform(m_funcNames.begin(), m_funcNames.end(), m_funcArray.begin()
                , [](std::vector<mfxChar>& name) {return name.data(); });

            NumFunctions = mfxU16(m_funcArray.size());
            FunctionsName = m_funcArray.data();

            m_implArray[0] = ImplCapsCommon::GetHDL(*this);
        }

        mfxHDL* GetArray() { return m_implArray; }

        size_t  GetSize() { return 1; }

        void Remove(const mfxChar* substr, bool bExact = false)
        {
            auto it = std::remove_if(m_funcArray.begin(), m_funcArray.end()
                , [=](mfxChar* str) { return !str || (substr && (str == strstr(str, substr)) && (!bExact || strlen(str) == strlen(substr))); });

            if (it != m_funcArray.end())
            {
                m_funcArray.erase(it, m_funcArray.end());

                NumFunctions = mfxU16(m_funcArray.size());
                FunctionsName = m_funcArray.data();
            }
        }

    protected:
        std::list<std::vector<mfxChar>> m_funcNames;
        std::vector<mfxChar*>  m_funcArray;
        mfxHDL m_implArray[1] = {};
    };
};

mfxStatus QueryImplsDescription(VideoCORE&, mfxEncoderDescription&, mfx::PODArraysHolder&);
mfxStatus QueryImplsDescription(VideoCORE&, mfxDecoderDescription&, mfx::PODArraysHolder&);
mfxStatus QueryImplsDescription(VideoCORE&, mfxVPPDescription&, mfx::PODArraysHolder&);

inline
std::tuple<mfxU32 /*Domain*/, mfxU32 /*Bus*/, mfxU32 /*Device*/, mfxU32 /*Function*/, mfxU16 /*RevisionID*/>
GetAdapterInfo(mfxU64 adapterId)
{
    auto result = std::make_tuple(0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, mfxU16(0xffff));

    auto fd = static_cast<int>(adapterId);
    drmDevicePtr pd;
    int sts = drmGetDevice(fd, &pd);
    if (!(!sts && pd))
        return result;
    std::unique_ptr<
        drmDevicePtr, void(*)(drmDevicePtr*)
    > dev(&pd, drmFreeDevice);

    if ((*dev)->bustype != DRM_BUS_PCI)
        return result;

    result = std::make_tuple(
        (*dev)->businfo.pci->domain,
        (*dev)->businfo.pci->bus,
        (*dev)->businfo.pci->dev,
        (*dev)->businfo.pci->func,
        (*dev)->deviceinfo.pci->revision_id
    );

    return result;
}

static bool QueryImplCaps(std::function < bool (VideoCORE&, mfxU32, mfxU32 , mfxU64, const std::vector<bool>& ) > QueryImpls)
{
    for (int i = 0; i < 64; ++i)
    {
        std::string path;

        {
            mfxU32 vendorId = 0;

            path = std::string("/sys/class/drm/renderD") + std::to_string(128 + i) + "/device/vendor";
            FILE* file = fopen(path.c_str(), "r");

            if (!file)
                break;

            int nread = fscanf(file, "%x", &vendorId);
            fclose(file);

            if (nread != 1 || vendorId != 0x8086)
                continue;
        }

        mfxU32 deviceId = 0;
        {
            path = std::string("/sys/class/drm/renderD") + std::to_string(128 + i) + "/device/device";

            FILE* file = fopen(path.c_str(), "r");
            if (!file)
                break;

            int nread = fscanf(file, "%x", &deviceId);
            fclose(file);

            if (nread != 1)
                break;
        }

        path = std::string("/dev/dri/renderD") + std::to_string(128 + i);

        int fd = open(path.c_str(), O_RDWR);
        if (fd < 0)
            continue;

        std::shared_ptr<int> closeFile(&fd, [fd](int*) { close(fd); });

        {
            auto displ = vaGetDisplayDRM(fd);

            int vamajor = 0, vaminor = 0;
            if (VA_STATUS_SUCCESS != vaInitialize(displ, &vamajor, &vaminor))
                continue;

            std::shared_ptr<VADisplay> closeVA(&displ, [displ](VADisplay*) { vaTerminate(displ); });

            VADisplayAttribute attr = {};
            attr.type = VADisplayAttribSubDevice;
            auto sts = vaGetDisplayAttributes(displ, &attr, 1);
            std::ignore = MFX_STS_TRACE(sts);

            VADisplayAttribValSubDevice out = {};
            out.value = attr.value;

            std::vector<bool> subDevMask(VA_STATUS_SUCCESS == sts ? out.bits.sub_device_count : 0);
            for (std::size_t id = 0; id < subDevMask.size(); ++id)
            {
                subDevMask[id] = !!((1 << id) & out.bits.sub_device_mask);
            }
            {
                std::unique_ptr<VideoCORE> pCore(FactoryCORE::CreateCORE(MFX_HW_VAAPI, 0, {}, 0));

                if (pCore->SetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL)displ))
                    continue;

                if (!QueryImpls(*pCore, deviceId, i, fd, subDevMask))
                    return false;
            }
        }
    }
    return true;
};

mfxHDL* MFX_CDECL MFXQueryImplsDescription(mfxImplCapsDeliveryFormat format, mfxU32* num_impls)
{
    PERF_UTILITY_AUTO(__FUNCTION__, PERF_LEVEL_API);
    mfxHDL* impl = nullptr;
    if (!num_impls)
        return impl;

    MFX_TRACE_INIT();
    MFXTrace_EventInit();
    TRACE_EVENT(MFX_TRACE_API_MFXQUERYIMPLSDESCRIPTION_TASK, EVENT_TYPE_START, 0, make_event_data((mfxU32)format));

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "In:  format = ", MFX_TRACE_FORMAT_I, format);
    try
    {
        switch (format)
        {
        case MFX_IMPLCAPS_IMPLEMENTEDFUNCTIONS:
        {
            std::unique_ptr<mfx::ImplFunctions> holder(new mfx::ImplFunctions);

            //remove non-VPL functions
            holder->Remove("MFXInit", true);
            holder->Remove("MFXInitEx", true);
            holder->Remove("MFXDoWork", true);
            holder->Remove("MFXVideoCORE_SetBufferAllocator", true);
            holder->Remove("MFXVideoVPP_RunFrameVPPAsyncEx", true);
            holder->Remove("MFXVideoUSER_");
            holder->Remove("MFXVideoENC_");
            holder->Remove("MFXVideoPAK_");

            *num_impls = mfxU32(holder->GetSize());

            TRACE_EVENT(MFX_TRACE_API_MFXQUERYIMPLSDESCRIPTION_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(*num_impls));
            MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "Out:  num_impls = ", MFX_TRACE_FORMAT_I, *num_impls);
            return holder.release()->GetArray();
        }
#if defined(ONEVPL_EXPERIMENTAL)
        case MFX_IMPLCAPS_DEVICE_ID_EXTENDED:
        {
            std::unique_ptr<mfx::ExtendedDeviceIDHolder> holder(new mfx::ExtendedDeviceIDHolder);

            auto QueryDevExtended = [&](VideoCORE& core, mfxU32 deviceId, mfxU32 adapterNum, mfxU64 adapterId, const std::vector<bool>&)-> bool
            {
                if (!CommonCaps::IsVplHW(core.GetHWType(), deviceId))
                    return true;

                auto& device = holder->PushBack();

                device.Version.Version = MFX_STRUCT_VERSION(1, 0);
                device.VendorID = 0x8086;
                device.DeviceID = mfxU16(deviceId);

                device.LUIDValid = 0;
                device.LUIDDeviceNodeMask = 0;

                for (int i = 0; i < 8; i++)
                    device.DeviceLUID[i] = 0;

                device.DRMPrimaryNodeNum = adapterNum;
                device.DRMRenderNodeNum = 128 + adapterNum;

                std::tie(device.PCIDomain, device.PCIBus, device.PCIDevice, device.PCIFunction, device.RevisionID)
                    = GetAdapterInfo(adapterId);

                snprintf(device.DeviceName, sizeof(device.DeviceName), "mfx-gen");

                return true;
            };

            if (!QueryImplCaps(QueryDevExtended))
                return impl;

            if (!holder->GetSize())
                return impl;

            *num_impls = mfxU32(holder->GetSize());

            TRACE_EVENT(MFX_TRACE_API_MFXQUERYIMPLSDESCRIPTION_TASK, EVENT_TYPE_END, TR_KEY_MFX_API, make_event_data(*num_impls));
            MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "Out:  num_impls = ", MFX_TRACE_FORMAT_I, *num_impls);

            holder->Detach();
            impl = holder.release()->GetArray();

            return impl;
        }

        case MFX_IMPLCAPS_SURFACE_TYPES:
        {
            std::unique_ptr<mfx::SurfaceTypesSupported> holder(new mfx::SurfaceTypesSupported);

            auto get_sharing_mode_flags = [](mfxSurfaceType type) -> mfxU32
            {
                switch (type)
                {
                case MFX_SURFACE_TYPE_VAAPI:
                    return MFX_SURFACE_FLAG_IMPORT_SHARED | MFX_SURFACE_FLAG_IMPORT_COPY | MFX_SURFACE_FLAG_EXPORT_SHARED | MFX_SURFACE_FLAG_EXPORT_COPY;
                default:
                    return MFX_SURFACE_FLAG_DEFAULT;
                }
            };

            for (auto type : { MFX_SURFACE_TYPE_VAAPI })
            {
                auto& surface_type = holder->PushBack(holder->SurfaceTypes);
                holder->NumSurfaceTypes++;

                surface_type = {};
                surface_type.SurfaceType = type;

                for (auto comp : { MFX_SURFACE_COMPONENT_ENCODE, MFX_SURFACE_COMPONENT_DECODE, MFX_SURFACE_COMPONENT_VPP_INPUT, MFX_SURFACE_COMPONENT_VPP_OUTPUT })
                {
                    auto& surface_component = holder->PushBack(surface_type.SurfaceComponents);
                    surface_type.NumSurfaceComponents++;

                    surface_component = {};
                    surface_component.SurfaceComponent = comp;
                    surface_component.SurfaceFlags     = get_sharing_mode_flags(type);
                }
            }

            *num_impls = 1;
            impl = holder.release()->GetHDL();

            return impl;
        }
#endif // defined(ONEVPL_EXPERIMENTAL)
        case MFX_IMPLCAPS_IMPLDESCSTRUCTURE:
        {
            std::unique_ptr<mfx::ImplDescriptionHolder> holder(new mfx::ImplDescriptionHolder);

            auto QueryImplDesc = [&](VideoCORE& core, mfxU32 deviceId, mfxU32 adapterNum, mfxU64, const std::vector<bool>& subDevMask) -> bool
            {
                if (!CommonCaps::IsVplHW(core.GetHWType(), deviceId))
                    return true;

                auto& impl = holder->PushBack();

                impl.Version.Version = MFX_STRUCT_VERSION(1, 2);
                impl.Impl = MFX_IMPL_TYPE_HARDWARE;
                impl.ApiVersion = { { MFX_VERSION_MINOR, MFX_VERSION_MAJOR } };
                impl.VendorID = 0x8086;
                // use adapterNum as VendorImplID, app. supposed just to copy it from mfxImplDescription to mfxInitializationParam
                impl.VendorImplID = adapterNum;
                impl.AccelerationMode = core.GetVAType() == MFX_HW_VAAPI ? MFX_ACCEL_MODE_VIA_VAAPI : MFX_ACCEL_MODE_VIA_D3D11;

                impl.AccelerationModeDescription.Version.Version = MFX_STRUCT_VERSION(1, 0);
                mfx::PODArraysHolder& ah = impl;
                ah.PushBack(impl.AccelerationModeDescription.Mode) = impl.AccelerationMode;
                impl.AccelerationModeDescription.NumAccelerationModes++;
                impl.PoolPolicies.Version.Version = MFX_STRUCT_VERSION(1, 0);
                impl.PoolPolicies.NumPoolPolicies = 3;
                ah.PushBack(impl.PoolPolicies.Policy) = MFX_ALLOCATION_OPTIMAL;
                ah.PushBack(impl.PoolPolicies.Policy) = MFX_ALLOCATION_UNLIMITED;
                ah.PushBack(impl.PoolPolicies.Policy) = MFX_ALLOCATION_LIMITED;

                impl.Dev.Version.Version = MFX_STRUCT_VERSION(1, 1);
                impl.Dev.MediaAdapterType = MFX_MEDIA_UNKNOWN;

                if (auto pCore1_19 = QueryCoreInterface<IVideoCore_API_1_19>(&core, MFXICORE_API_1_19_GUID))
                {
                    mfxPlatform platform = {};
                    if (MFX_ERR_NONE == pCore1_19->QueryPlatform(&platform))
                        impl.Dev.MediaAdapterType = platform.MediaAdapterType;
                }

                snprintf(impl.Dev.DeviceID, sizeof(impl.Dev.DeviceID), "%x/%d", deviceId, adapterNum);
                snprintf(impl.ImplName, sizeof(impl.ImplName), "mfx-gen");
                snprintf(impl.License, sizeof(impl.License),
                    "MIT License"
                    );

                for (std::size_t i = 0, e = subDevMask.size(); i != e; ++i)
                {
                    if (subDevMask[i])
                    {
                        auto & subDevices = ah.PushBack(impl.Dev.SubDevices);
                        subDevices.Index = impl.Dev.NumSubDevices;
                        snprintf(subDevices.SubDeviceID, sizeof(subDevices.SubDeviceID), "%d", static_cast<mfxU32>(i));
                        impl.Dev.NumSubDevices++;
                    }
                }

                impl.AccelerationModeDescription.Version.Version = MFX_STRUCT_VERSION(1, 0);
                impl.PoolPolicies.Version.Version = MFX_STRUCT_VERSION(1, 0);
                impl.Dec.Version.Version = MFX_STRUCT_VERSION(1, 0);
                impl.Enc.Version.Version = MFX_STRUCT_VERSION(1, 0);
                impl.VPP.Version.Version = MFX_STRUCT_VERSION(1, 0);

                return (MFX_ERR_NONE == QueryImplsDescription(core, impl.Enc, impl) &&
                    MFX_ERR_NONE == QueryImplsDescription(core, impl.Dec, impl) &&
                    MFX_ERR_NONE == QueryImplsDescription(core, impl.VPP, impl));
            };

            {
                auto logSkip = GetMfxLogSkip();
                std::ignore  = logSkip;

                InitMfxLogging();
                MFX_LOG_API_TRACE("----------------MFXQueryImplsDescription----------------\n");

                if (!QueryImplCaps(QueryImplDesc))
                    return impl;
            }

            if (!holder->GetSize())
                return impl;

            *num_impls = mfxU32(holder->GetSize());

            MFX_LTRACE_1(MFX_TRACE_LEVEL_API_PARAMS, "Out:  num_impls = ", MFX_TRACE_FORMAT_I, *num_impls);

            holder->Detach();
            impl = holder.release()->GetArray();

            return impl;
        }
        default:
            assert(!"Unknown format");
            return impl;
        }
    }
    catch (...)
    {
        return impl;
    }
}

mfxStatus MFX_CDECL MFXReleaseImplDescription(mfxHDL hdl)
{
    PERF_UTILITY_AUTO(__FUNCTION__, PERF_LEVEL_API);
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, __FUNCTION__);
    MFX_CHECK_HDL(hdl);

    try
    {
        mfx::ImplCapsCommon::Release(hdl);
    }
    catch (...)
    {
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }

    return MFX_ERR_NONE;
}

