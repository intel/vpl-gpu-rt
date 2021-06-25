// Copyright (c) 2007-2021 Intel Corporation
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
#include "va/va_drm.h"

#include "mediasdk_version.h"
#include "libmfx_core_factory.h"
#include "mfx_interface_scheduler.h"


// static section of the file
namespace
{

void* g_hModule = NULL; // DLL handle received in DllMain

} // namespace

mfxStatus MFXInit(mfxIMPL implParam, mfxVersion *ver, mfxSession *session)
{
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

    return MFXInitEx(par, session);

} // mfxStatus MFXInit(mfxIMPL impl, mfxVersion *ver, mfxHDL *session)

static inline mfxU32 MakeVersion(mfxU16 major, mfxU16 minor)
{
    return major * 1000 + minor;
}

static mfxStatus MFXInit_Internal(mfxInitParam par, mfxSession* session, mfxIMPL implInterface, mfxU32 adapterNum);

mfxStatus MFXInitEx(mfxInitParam par, mfxSession *session)
{
    (void)g_hModule;
    mfxStatus mfxRes;
    int adapterNum = 0;
    mfxIMPL impl = par.Implementation & (MFX_IMPL_VIA_ANY - 1);
    mfxIMPL implInterface = par.Implementation & -MFX_IMPL_VIA_ANY;

    MFX_TRACE_INIT();
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "ThreadName=MSDK app");
    }
    MFX_AUTO_TRACE("MFXInitEx");
    ETW_NEW_EVENT(MFX_TRACE_API_MFX_INIT_EX_TASK, 0, make_event_data((mfxU32) par.Implementation, par.GPUCopy), [&](){ return make_event_data(mfxRes, session ? *session : nullptr);});
    MFX_LTRACE_1(MFX_TRACE_LEVEL_API, "^ModuleHandle^libmfx=", "%p", g_hModule);

    // check the library version
    if (MakeVersion(par.Version.Major, par.Version.Minor) > MFX_VERSION)
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
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
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    // if user did not specify MFX_IMPL_VIA_* treat it as MFX_IMPL_VIA_ANY
    if (!implInterface)
        implInterface = MFX_IMPL_VIA_ANY;

    if (
        (MFX_IMPL_VIA_VAAPI != implInterface) &&
        (MFX_IMPL_VIA_ANY != implInterface))
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
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

    // MFXInit / MFXInitEx in oneVPL is for work in legacy (1.x) mode only
    // app. must use MFXInitialize for 2.x features
    if (par.Version.Major > 1)
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }

    mfxStatus sts = MFXInit_Internal(par, session, implInterface, adapterNum);

    // oneVPL should report 1.255 API version when it was initialized through MFXInit / MFXInitEx
    if (session && *session && sts >= MFX_ERR_NONE)
    {
        (*session)->m_versionToReport.Major = 1;
        (*session)->m_versionToReport.Minor = 255;
    }

    return sts;

} // mfxStatus MFXInitEx(mfxInitParam par, mfxSession *session)

static mfxStatus MFXInit_Internal(mfxInitParam par, mfxSession *session, mfxIMPL implInterface, mfxU32 adapterNum)
{
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

        mfxRes = pSession->InitEx(init_param);
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
    MFX_AUTO_TRACE("MFXDoWork");
    ETW_NEW_EVENT(MFX_TRACE_API_DO_WORK_TASK, 0, make_event_data(session), [&](){ return make_event_data(res);});

    // check error(s)
    if (0 == session)
    {
        MFX_RETURN(MFX_ERR_INVALID_HANDLE);
    }

    MFXIUnknown * pInt = session->m_pScheduler;
    MFXIScheduler2 *newScheduler = 
        ::QueryInterface<MFXIScheduler2>(pInt, MFXIScheduler2_GUID);

    if (!newScheduler)
    {
        MFX_RETURN(MFX_ERR_UNSUPPORTED);
    }
    newScheduler->Release();

    res = newScheduler->DoWork();

    return res;
} // mfxStatus MFXDoWork(mfxSession *session)

mfxStatus MFXClose(mfxSession session)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

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
        MFX_AUTO_TRACE("MFXClose");
        ETW_NEW_EVENT(MFX_TRACE_API_MFX_CLOSE_TASK, 0, make_event_data(session), [&](){ return make_event_data(mfxRes);});

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
    return mfxRes;

} // mfxStatus MFXClose(mfxHDL session)

mfxStatus MFX_CDECL MFXInitialize(mfxInitializationParam param, mfxSession* session)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_TRACE_INIT();
    ETW_NEW_EVENT(MFX_TRACE_API_MFXINITIALIZE_TASK_ETW, 0, make_event_data((mfxU32)param.AccelerationMode, param.VendorImplID),
        [&](){ return make_event_data(mfxRes, session ? *session : nullptr); }
    );

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

    mfxRes = MFXInit_Internal(par, session, par.Implementation, 0);

    return mfxRes;
}

namespace mfx
{
    class ImplDescriptionHolder;

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

    class ImplDescriptionHolder
    {
    public:
        friend class ImplDescription;

        ImplDescription& PushBack()
        {
            m_impls.emplace_back(new ImplDescription(this));
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
        std::list<std::unique_ptr<ImplDescription>> m_impls;
        size_t m_ref = 0;

        void Release()
        {
            if (m_impls.empty() && m_ref-- <= 1)
                delete this;
        }
    };

    ImplDescription::~ImplDescription()
    {
        if (m_pHolder)
            m_pHolder->Release();
    }

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

mfxHDL* MFX_CDECL MFXQueryImplsDescription(mfxImplCapsDeliveryFormat format, mfxU32* num_impls)
{
    mfxHDL* impl = nullptr;
    if (!num_impls)
        return impl;

    MFX_TRACE_INIT();

    ETW_NEW_EVENT(MFX_TRACE_API_MFXQUERYIMPLSDESCRIPTION_TASK_ETW, 0, make_event_data((mfxU32)format),
        [&]() { return make_event_data(*num_impls); }
    );

    if (format == MFX_IMPLCAPS_IMPLEMENTEDFUNCTIONS)
    {
        try
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
            return holder.release()->GetArray();
        }
        catch (...)
        {
            return impl;
        }
    }

    if (format != MFX_IMPLCAPS_IMPLDESCSTRUCTURE)
        return impl;

    try
    {
        std::unique_ptr<mfx::ImplDescriptionHolder> holder(new mfx::ImplDescriptionHolder);
        std::set<mfxU32> deviceIds;

        auto IsVplHW = [](eMFXHWType hw) -> bool
        {
            return hw >= MFX_HW_TGL_LP;
        };
        auto QueryImplDesc = [&](VideoCORE& core, mfxU32 deviceId, mfxU32 adapterNum) -> bool
        {
            if (   !IsVplHW(core.GetHWType())
                || deviceIds.end() != deviceIds.find(deviceId))
                return true;

            deviceIds.insert(deviceId);

            auto& impl = holder->PushBack();

            impl.Impl             = MFX_IMPL_TYPE_HARDWARE;
            impl.ApiVersion       = { { MFX_VERSION_MINOR, MFX_VERSION_MAJOR } };
            impl.VendorID         = 0x8086;
            // use adapterNum as VendorImplID, app. supposed just to copy it from mfxImplDescription to mfxInitializationParam
            impl.VendorImplID     = adapterNum;
            impl.AccelerationMode = core.GetVAType() == MFX_HW_VAAPI ? MFX_ACCEL_MODE_VIA_VAAPI : MFX_ACCEL_MODE_VIA_D3D11;

            snprintf(impl.Dev.DeviceID, sizeof(impl.Dev.DeviceID), "%x/%d", deviceId, adapterNum);
            snprintf(impl.ImplName, sizeof(impl.ImplName), "mfx-gen");

            //TODO:
            impl.License;
            impl.Keywords;

            return (MFX_ERR_NONE == QueryImplsDescription(core, impl.Enc, impl) &&
                    MFX_ERR_NONE == QueryImplsDescription(core, impl.Dec, impl) &&
                    MFX_ERR_NONE == QueryImplsDescription(core, impl.VPP, impl));
        };

        for (int i = 0; i < 64; ++i)
        {
            std::string path;

            {
                mfxU32 vendorId = 0;

                path = std::string("/sys/class/drm/renderD") + std::to_string(128 + i) + "/device/vendor";
                FILE* file = fopen(path.c_str(), "r");

                if (!file)
                    break;

                int res = fscanf(file, "%x", &vendorId);                
                fclose(file);

                if (res == EOF || vendorId != 0x8086)
                    continue;
            }

            mfxU32 deviceId = 0;
            {
                path = std::string("/sys/class/drm/renderD") + std::to_string(128 + i) + "/device/device";

                FILE* file = fopen(path.c_str(), "r");
                if (!file)
                    break;

                int res = fscanf(file, "%x", &deviceId);
                fclose(file);

                if (res == EOF)
                    continue;
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

                {
                    std::unique_ptr<VideoCORE> pCore(FactoryCORE::CreateCORE(MFX_HW_VAAPI, 0, 0));

                    if (pCore->SetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL)displ))
                        continue;

                    if (!QueryImplDesc(*pCore, deviceId, i))
                        return nullptr;
                }
            }
        }

        if (!holder->GetSize())
            return impl;

        *num_impls = mfxU32(holder->GetSize());

        holder->Detach();
        impl = holder.release()->GetArray();

        return impl;
    }
    catch (...)
    {
        return impl;
    }
}

mfxStatus MFX_CDECL MFXReleaseImplDescription(mfxHDL hdl)
{
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

void __attribute__ ((constructor)) dll_init(void)
{

}
