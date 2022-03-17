// Copyright (c) 2013-2019 Intel Corporation
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

#include "assert.h"
#include "cmrt_cross_platform.h"

#ifdef CM_WIN
#include <cfgmgr32.h>
#include <devguid.h>
#pragma comment(lib, "cfgmgr32")
#endif

const vm_char * DLL_NAME_LINUX = VM_STRING("libigfxcmrt.so.7");

#ifdef CMRT_EMU
    const char * FUNC_NAME_CREATE_CM_DEVICE  = "CreateCmDeviceEmu";
    const char * FUNC_NAME_CREATE_CM_DEVICE_EX  = "CreateCmDeviceEmu";
    const char * FUNC_NAME_DESTROY_CM_DEVICE = "DestroyCmDeviceEmu";
#else //CMRT_EMU
    const char * FUNC_NAME_CREATE_CM_DEVICE  = "CreateCmDevice";
    const char * FUNC_NAME_CREATE_CM_DEVICE_EX  = "CreateCmDeviceEx";
    const char * FUNC_NAME_DESTROY_CM_DEVICE = "DestroyCmDevice";
#endif //CMRT_EMU


#if defined(CM_WIN)
#define IMPL_FOR_ALL(RET, FUNC, PROTO, PARAM)   \
    RET FUNC PROTO {                            \
        switch (m_platform) {                   \
        case DX9:   return m_dx9->FUNC PARAM;   \
        case DX11:  return m_dx11->FUNC PARAM;  \
        default:    return CM_NOT_IMPLEMENTED;  \
        }                                       \
    }
#else // #if defined(CM_WIN)
#define IMPL_FOR_ALL(RET, FUNC, PROTO, PARAM)   \
    RET FUNC PROTO {                            \
        switch (m_platform) {                   \
        case VAAPI: return m_linux->FUNC PARAM; \
        default:    return CM_NOT_IMPLEMENTED;  \
        }                                       \
    }
#endif // #if defined(CM_WIN)

#if defined(CM_WIN)
#define IMPL_FOR_DX(RET, FUNC, PROTO, PARAM)    \
    RET FUNC PROTO {                            \
        switch (m_platform) {                   \
        case DX9:   return m_dx9->FUNC PARAM;   \
        case DX11:  return m_dx11->FUNC PARAM;  \
        default:    return CM_NOT_IMPLEMENTED;  \
        }                                       \
    }

#define IMPL_FOR_DX11(RET, FUNC, PROTO, PARAM)  \
    RET FUNC PROTO {                            \
        switch (m_platform) {                   \
        case DX11:  return m_dx11->FUNC PARAM;  \
        default:    return CM_NOT_IMPLEMENTED;  \
        }                                       \
    }

#include "dxgiformat.h"

#define CONVERT_FORMAT(FORMAT) \
    (m_platform == DX11 ? (D3DFORMAT)ConvertFormatToDx11(FORMAT) : (FORMAT))

namespace
{
    DXGI_FORMAT ConvertFormatToDx11(D3DFORMAT format)
    {
        switch ((UINT)format) {
        case CM_SURFACE_FORMAT_UNKNOWN:     return DXGI_FORMAT_UNKNOWN;
        case CM_SURFACE_FORMAT_A8R8G8B8:    return DXGI_FORMAT_B8G8R8A8_UNORM;
        case CM_SURFACE_FORMAT_X8R8G8B8:    return DXGI_FORMAT_B8G8R8X8_UNORM;
        case CM_SURFACE_FORMAT_A8:          return DXGI_FORMAT_A8_UNORM;
        case CM_SURFACE_FORMAT_P8:          return DXGI_FORMAT_P8;
        case CM_SURFACE_FORMAT_R32F:        return DXGI_FORMAT_R32_FLOAT;
        case CM_SURFACE_FORMAT_NV12:        return DXGI_FORMAT_NV12;
        case CM_SURFACE_FORMAT_UYVY:        return DXGI_FORMAT_R8G8_B8G8_UNORM;
        case CM_SURFACE_FORMAT_YUY2:        return DXGI_FORMAT_YUY2;
        case CM_SURFACE_FORMAT_V8U8:        return DXGI_FORMAT_R8G8_SNORM;
        default: assert(0);                 return DXGI_FORMAT_UNKNOWN;
        }
    }
};

#else /* #if defined(CM_WIN) */

#define CONVERT_FORMAT(FORMAT) (FORMAT)

#endif /* #if defined(CM_WIN) */

#if defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#else
#if defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif
#endif

enum { DX9 = 1, DX11 = 2, VAAPI = 3 };

#if defined(CM_WIN) 
typedef INT (* CreateCmDeviceDx9FuncTypeEx)(CmDx9::CmDevice *&, UINT &, IDirect3DDeviceManager9 *, UINT);
typedef INT (* CreateCmDeviceDx11FuncTypeEx)(CmDx11::CmDevice *&, UINT &, ID3D11Device *, UINT);
typedef INT (* CreateCmDeviceDx9FuncType)(CmDx9::CmDevice *&, UINT &, IDirect3DDeviceManager9 *);
typedef INT (* CreateCmDeviceDx11FuncType)(CmDx11::CmDevice *&, UINT &, ID3D11Device *);
typedef INT (* DestroyCmDeviceDx9FuncType)(CmDx9::CmDevice *&);
typedef INT (* DestroyCmDeviceDx11FuncType)(CmDx11::CmDevice *&);
#else // #if defined(CM_WIN) 
typedef INT (* CreateCmDeviceLinuxFuncTypeEx)(CmLinux::CmDevice *&, UINT &, VADisplay, UINT);
typedef INT (* CreateCmDeviceLinuxFuncType)(CmLinux::CmDevice *&, UINT &, VADisplay);
typedef INT (* DestroyCmDeviceVAAPIFuncType)(CmLinux::CmDevice *&);
#endif // #if defined(CM_WIN) 

class CmDeviceImpl : public CmDevice
{
public:
    void*           m_dll;
    INT             m_platform;

    union
    {
#if defined(CM_WIN)
        CmDx9::CmDevice *       m_dx9;
        CmDx11::CmDevice *      m_dx11;
#else
        CmLinux::CmDevice *     m_linux;
#endif
    };

    virtual ~CmDeviceImpl(){}

    INT GetPlatform(){return m_platform;};

    INT GetDevice(AbstractDeviceHandle & pDevice)
    {
#if !defined(CM_WIN)
        (void)pDevice;
#endif

        switch (m_platform) {
#if defined(CM_WIN)
        case DX9:   return m_dx9->GetD3DDeviceManager((IDirect3DDeviceManager9 *&)pDevice);
        case DX11:  return m_dx11->GetD3D11Device((ID3D11Device *&)pDevice);
#else
        case VAAPI: return CM_NOT_IMPLEMENTED;
#endif
        default:    return CM_NOT_IMPLEMENTED;
        }
    }

    INT CreateSurface2D(mfxHDLPair D3DSurfPair, CmSurface2D *& pSurface)
    {
        switch (m_platform) {
#if defined(CM_WIN)
        case DX9:   return m_dx9->CreateSurface2D((IDirect3DSurface9 *)D3DSurfPair.first, pSurface);
        case DX11:  return m_dx11->CreateSurface2DbySubresourceIndex((ID3D11Texture2D *)D3DSurfPair.first, static_cast<UINT>((size_t)D3DSurfPair.second), 0, pSurface);
#else
        case VAAPI: return m_linux->CreateSurface2D(*(VASurfaceID*)D3DSurfPair.first, pSurface);
#endif
        default:    return CM_NOT_IMPLEMENTED;
        }
    }

    INT CreateSurface2D(AbstractSurfaceHandle pD3DSurf, CmSurface2D *& pSurface)
    {
        switch (m_platform) {
#if defined(CM_WIN)
        case DX9:   return m_dx9->CreateSurface2D((IDirect3DSurface9 *)pD3DSurf, pSurface);
        case DX11:  return m_dx11->CreateSurface2D((ID3D11Texture2D *)pD3DSurf, pSurface);
#else
        case VAAPI: return m_linux->CreateSurface2D(*(VASurfaceID*)pD3DSurf, pSurface);
#endif
        default:    return CM_NOT_IMPLEMENTED;
        }
    }

    INT CreateSurface2DSubresource(AbstractSurfaceHandle pD3D11Texture2D, UINT subresourceCount, CmSurface2D ** ppSurfaces, UINT & createdSurfaceCount, UINT option)
    {
#if !defined(CM_WIN)
        (void)pD3D11Texture2D;
        (void)subresourceCount;
        (void)ppSurfaces;
        (void)createdSurfaceCount;
        (void)option;
#endif

        switch (m_platform) {
#if defined(CM_WIN)
        case DX9:   return CM_NOT_IMPLEMENTED;
        case DX11:  return m_dx11->CreateSurface2DSubresource((ID3D11Texture2D *)pD3D11Texture2D, subresourceCount, ppSurfaces, createdSurfaceCount, option);
#else
        case VAAPI: return CM_NOT_IMPLEMENTED;
#endif
        default:    return CM_NOT_IMPLEMENTED;
        }
    }

    INT CreateSurface2DbySubresourceIndex(AbstractSurfaceHandle pD3D11Texture2D, UINT FirstArraySlice, UINT FirstMipSlice, CmSurface2D *& pSurface)
    {
#if !defined(CM_WIN)
        (void)pD3D11Texture2D;
        (void)FirstArraySlice;
        (void)FirstMipSlice;
        (void)pSurface;
#endif

        switch (m_platform) {
#if defined(CM_WIN)
        case DX9:   return CM_NOT_IMPLEMENTED;
        case DX11:  return m_dx11->CreateSurface2DbySubresourceIndex((ID3D11Texture2D *)pD3D11Texture2D, FirstArraySlice, FirstMipSlice, pSurface);
#else
        case VAAPI: return CM_NOT_IMPLEMENTED;
#endif
        default:    return CM_NOT_IMPLEMENTED;
        }
    }

    IMPL_FOR_ALL(INT, CreateBuffer, (UINT size, CmBuffer *& pSurface), (size, pSurface));
    IMPL_FOR_ALL(INT, CreateSurface2D, (UINT width, UINT height, CM_SURFACE_FORMAT format, CmSurface2D* & pSurface), (width, height, CONVERT_FORMAT(format), pSurface));
    IMPL_FOR_ALL(INT, CreateSurface3D, (UINT width, UINT height, UINT depth, CM_SURFACE_FORMAT format, CmSurface3D* & pSurface), (width, height, depth, CONVERT_FORMAT(format), pSurface));
    IMPL_FOR_ALL(INT, DestroySurface, (CmSurface2D *& pSurface), (pSurface));
    IMPL_FOR_ALL(INT, DestroySurface, (CmBuffer *& pSurface), (pSurface));
    IMPL_FOR_ALL(INT, DestroySurface, (CmSurface3D *& pSurface), (pSurface));
    IMPL_FOR_ALL(INT, CreateQueue, (CmQueue *& pQueue), (pQueue));
    IMPL_FOR_ALL(INT, LoadProgram, (void * pCommonISACode, const UINT size, CmProgram *& pProgram, const char * options), (pCommonISACode, size, pProgram, options));
    IMPL_FOR_ALL(INT, CreateKernel, (CmProgram * pProgram, const char * kernelName, CmKernel *& pKernel, const char * options), (pProgram, kernelName, pKernel, options));
    IMPL_FOR_ALL(INT, CreateKernel, (CmProgram * pProgram, const char * kernelName, const void * fncPnt, CmKernel *& pKernel, const char * options), (pProgram, kernelName, fncPnt, pKernel, options));
    IMPL_FOR_ALL(INT, CreateSampler, (const CM_SAMPLER_STATE & sampleState, CmSampler *& pSampler), (sampleState, pSampler));
    IMPL_FOR_ALL(INT, DestroyKernel, (CmKernel *& pKernel), (pKernel));
    IMPL_FOR_ALL(INT, DestroySampler, (CmSampler *& pSampler), (pSampler));
    IMPL_FOR_ALL(INT, DestroyProgram, (CmProgram *& pProgram), (pProgram));
    IMPL_FOR_ALL(INT, DestroyThreadSpace, (CmThreadSpace *& pTS), (pTS));
    IMPL_FOR_ALL(INT, CreateTask, (CmTask *& pTask), (pTask));
    IMPL_FOR_ALL(INT, DestroyTask, (CmTask *& pTask), (pTask));
    IMPL_FOR_ALL(INT, GetCaps, (CM_DEVICE_CAP_NAME capName, size_t & capValueSize, void * pCapValue), (capName, capValueSize, pCapValue));
    IMPL_FOR_ALL(INT, CreateThreadSpace, (UINT width, UINT height, CmThreadSpace *& pTS), (width, height, pTS));
    IMPL_FOR_ALL(INT, CreateBufferUP, (UINT size, void * pSystMem, CmBufferUP *& pSurface), (size, pSystMem, pSurface));
    IMPL_FOR_ALL(INT, DestroyBufferUP, (CmBufferUP *& pSurface), (pSurface));
    IMPL_FOR_ALL(INT, GetSurface2DInfo, (UINT width, UINT height, CM_SURFACE_FORMAT format, UINT & pitch, UINT & physicalSize), (width, height, CONVERT_FORMAT(format), pitch, physicalSize));
    IMPL_FOR_ALL(INT, CreateSurface2DUP, (UINT width, UINT height, CM_SURFACE_FORMAT format, void * pSysMem, CmSurface2DUP *& pSurface), (width, height, CONVERT_FORMAT(format), pSysMem, pSurface));
    IMPL_FOR_ALL(INT, DestroySurface2DUP, (CmSurface2DUP *& pSurface), (pSurface));
    IMPL_FOR_ALL(INT, CreateVmeSurfaceG7_5 , (CmSurface2D * pCurSurface, CmSurface2D ** pForwardSurface, CmSurface2D ** pBackwardSurface, const UINT surfaceCountForward, const UINT surfaceCountBackward, SurfaceIndex *& pVmeIndex), (pCurSurface, pForwardSurface, pBackwardSurface, surfaceCountForward, surfaceCountBackward, pVmeIndex));
    IMPL_FOR_ALL(INT, DestroyVmeSurfaceG7_5, (SurfaceIndex *& pVmeIndex), (pVmeIndex));
    IMPL_FOR_ALL(INT, CreateSampler8x8, (const CM_SAMPLER_8X8_DESCR & smplDescr, CmSampler8x8 *& psmplrState), (smplDescr, psmplrState));
    IMPL_FOR_ALL(INT, DestroySampler8x8, (CmSampler8x8 *& pSampler), (pSampler));
    IMPL_FOR_ALL(INT, CreateSampler8x8Surface, (CmSurface2D * p2DSurface, SurfaceIndex *& pDIIndex, CM_SAMPLER8x8_SURFACE surf_type, CM_SURFACE_ADDRESS_CONTROL_MODE mode), (p2DSurface, pDIIndex, surf_type, mode));
    IMPL_FOR_ALL(INT, DestroySampler8x8Surface, (SurfaceIndex* & pDIIndex), (pDIIndex));
    IMPL_FOR_ALL(INT, CreateThreadGroupSpace, (UINT thrdSpaceWidth, UINT thrdSpaceHeight, UINT grpSpaceWidth, UINT grpSpaceHeight, CmThreadGroupSpace *& pTGS), (thrdSpaceWidth, thrdSpaceHeight, grpSpaceWidth, grpSpaceHeight, pTGS));
    IMPL_FOR_ALL(INT, DestroyThreadGroupSpace, (CmThreadGroupSpace *& pTGS), (pTGS));
    IMPL_FOR_ALL(INT, SetL3Config, (const L3_CONFIG_REGISTER_VALUES * l3_c), (l3_c));
    IMPL_FOR_ALL(INT, SetSuggestedL3Config, (L3_SUGGEST_CONFIG l3_s_c), (l3_s_c));
    IMPL_FOR_ALL(INT, SetCaps, (CM_DEVICE_CAP_NAME capName, size_t capValueSize, void * pCapValue), (capName, capValueSize, pCapValue));
    IMPL_FOR_ALL(INT, CreateSamplerSurface2D, (CmSurface2D * p2DSurface, SurfaceIndex *& pSamplerSurfaceIndex), (p2DSurface, pSamplerSurfaceIndex));
    IMPL_FOR_ALL(INT, CreateSamplerSurface3D, (CmSurface3D * p3DSurface, SurfaceIndex *& pSamplerSurfaceIndex), (p3DSurface, pSamplerSurfaceIndex));
    IMPL_FOR_ALL(INT, DestroySamplerSurface, (SurfaceIndex *& pSamplerSurfaceIndex), (pSamplerSurfaceIndex));
    IMPL_FOR_ALL(INT, InitPrintBuffer, (size_t printbufsize), (printbufsize));
    IMPL_FOR_ALL(INT, FlushPrintBuffer, (), ());
    IMPL_FOR_ALL(int32_t, CreateQueueEx, (CmQueue *&pQueue, CM_QUEUE_CREATE_OPTION QueueCreateOption = CM_DEFAULT_QUEUE_CREATE_OPTION), (pQueue, QueueCreateOption));
};

#ifdef CM_WIN

/*!
Function for converting list of "str1\0str2\0str3\0\0" to vector of single strings
\param input_str double-null string for parsing
\param input_str_len length of input_str, including double null
\param output_vec vector for storing found strings
*/
inline void ConvertDoubleNullTermStringToVector(const wchar_t *input_str, const size_t input_str_len, std::vector<std::wstring> &output_vec)
{
    const wchar_t *begin = input_str;
    const wchar_t *end = input_str + input_str_len;
    size_t len = 0;

    if (!input_str) return;

    while ((begin<end) && (len = wcslen(begin)) > 0)
    {
        output_vec.push_back(begin);
        begin += len + 1;
    }
}
/*!
Function for comparing for Intel Device ID
\param DeviceID string, containing Device ID for check
\return true if Intel Device ID found
*/
inline bool IsIntelDeviceInstanceID(std::wstring &DeviceID)
{
    if (DeviceID.find(L"VEN_8086") != std::wstring::npos || DeviceID.find(L"ven_8086") != std::wstring::npos)
        return true;
    else
        return false;
}
/*!
Function for looking for MDF in DriverStore
\param path allocated memory for storing path to MDF, not less than MAX_PATH
\return true if path found
*/
inline bool GetMDFDriverStorePath(TCHAR *path, DWORD *dwPathSize)
{
    // Obtain a PnP handle to the Intel graphics adapter
    CONFIGRET    result = CR_SUCCESS;
    std::wstring IntelDeviceID;
    std::vector<std::wstring> Devices;
    ULONG        DeviceIDListSize = 0;
    PWSTR        DeviceIDList = nullptr;
    wchar_t      DisplayGUID[40];
    DEVINST      DeviceInst;

    if(StringFromGUID2(GUID_DEVCLASS_DISPLAY, DisplayGUID, sizeof(DisplayGUID))==0)
    {
        return false;
    }

    do
    {
        result = CM_Get_Device_ID_List_SizeW(&DeviceIDListSize, DisplayGUID, CM_GETIDLIST_FILTER_CLASS | CM_GETIDLIST_FILTER_PRESENT);
        if (result != CR_SUCCESS)
        {
            break;
        }
        //This block should be executed once, but if we come here several times we should cleanup before new allocation
        if (DeviceIDList != nullptr)
        {
            delete[] DeviceIDList;
            DeviceIDList = nullptr;
        }
        try
        {
            DeviceIDList = new WCHAR[DeviceIDListSize * sizeof(PWSTR)]; /* free in this method */
        }
        catch (...)
        {
            return false;
        }
        result = CM_Get_Device_ID_ListW(DisplayGUID, DeviceIDList, DeviceIDListSize, CM_GETIDLIST_FILTER_CLASS | CM_GETIDLIST_FILTER_PRESENT);

    } while (result == CR_BUFFER_SMALL);

    if (result != CR_SUCCESS)
    {
        if(DeviceIDList!=nullptr)
        {
            delete[] DeviceIDList;
            DeviceIDList = nullptr;
        }
        return false;
    }

    ConvertDoubleNullTermStringToVector(DeviceIDList, DeviceIDListSize, Devices);
    delete[] DeviceIDList;
    DeviceIDList = nullptr;

    //Look for MDF record
    for (auto it = Devices.begin(); it != Devices.end(); ++it)
    {
        if (IsIntelDeviceInstanceID(*it))
        {
            result = CM_Locate_DevNodeW(&DeviceInst, const_cast<wchar_t *>((*it).c_str()), CM_LOCATE_DEVNODE_NORMAL);
            if (result != CR_SUCCESS)
            {
                continue;
            }

            HKEY hKey_sw;
            result = CM_Open_DevNode_Key(DeviceInst, KEY_READ, 0, RegDisposition_OpenExisting, &hKey_sw, CM_REGISTRY_SOFTWARE);
            if (result != CR_SUCCESS)
            {
                continue;
            }

            ULONG nError;

            nError = RegQueryValueEx(hKey_sw, _T("DriverStorePathForMDF"), 0, NULL, (LPBYTE)path, dwPathSize);

            RegCloseKey(hKey_sw);

            if (ERROR_SUCCESS == nError)
            {
                return true;
            }
        }
    }

    return false;
}
/*
On windows we are looking for CMRT DLL in driver store directory
Driver writes 'DriverStorePathForMDF' reg key during install.
Use path from driver to load igfx*cmrt*.dll
*/
void* cm_dll_load(const vm_char *so_file_name)
{
    //mfx_trace_get_reg_string
    void* handle = NULL;
    TCHAR path[MAX_PATH] = _T("");
    DWORD size = sizeof(path);

    /* check error(s) */
    if (NULL == so_file_name)
        return NULL;

    /* try to load from DriverStore, UWD/HKR path */
    if (GetMDFDriverStorePath(path, &size))
    {
        wcscat_s(path, MAX_PATH, _T("\\"));
        wcscat_s(path, MAX_PATH, so_file_name);
        handle = so_load(path);
    }

    return handle;

}

INT CreateCmDevice(CmDevice *& pD, UINT & version, IDirect3DDeviceManager9 * pD3DDeviceMgr, UINT mode )
{
    CmDeviceImpl * device = new CmDeviceImpl;

    device->m_platform = DX9;
    device->m_dll = cm_dll_load(DLL_NAME_DX9);
    if (device->m_dll == 0)
    {
        delete device;
        return CM_FAILURE;
    }

    CreateCmDeviceDx9FuncTypeEx createFunc = (CreateCmDeviceDx9FuncTypeEx)so_get_addr(device->m_dll, FUNC_NAME_CREATE_CM_DEVICE_EX);

    if (createFunc == 0)
    {
        delete device;
        return CM_FAILURE;
    }


    INT res = createFunc(device->m_dx9, version, pD3DDeviceMgr, mode);

    if (res != CM_SUCCESS)
    {
        delete device;
        return CM_FAILURE;
    }

    pD = device;
    return CM_SUCCESS;
}


INT CreateCmDevice(CmDevice* &pD, UINT& version, ID3D11Device * pD3D11Device, UINT mode )
{
    CmDeviceImpl * device = new CmDeviceImpl;

    device->m_platform = DX11;
    device->m_dll = cm_dll_load(DLL_NAME_DX11);
    if (device->m_dll == 0)
    {
        delete device;
        return CM_FAILURE;
    }


    CreateCmDeviceDx11FuncTypeEx createFunc = (CreateCmDeviceDx11FuncTypeEx)so_get_addr(device->m_dll, FUNC_NAME_CREATE_CM_DEVICE_EX);

    if (createFunc == 0)
    {
        delete device;
        return CM_FAILURE;
    }


    INT res = createFunc(device->m_dx11, version, pD3D11Device, mode);

    if (res != CM_SUCCESS)
    {
        delete device;
        return CM_FAILURE;
    }

    pD = device;
    return CM_SUCCESS;
}

#else /* #ifdef CM_WIN */

INT CreateCmDevice(CmDevice *& pD, UINT & version, VADisplay va_dpy, UINT mode)
{
    CmDeviceImpl * device = new CmDeviceImpl;

    device->m_platform = VAAPI;
    device->m_dll = so_load(DLL_NAME_LINUX);

    if (device->m_dll == 0)
    {
        delete device;
        return CM_FAILURE;
    }

    CreateCmDeviceLinuxFuncTypeEx createFunc = (CreateCmDeviceLinuxFuncTypeEx)so_get_addr(device->m_dll, FUNC_NAME_CREATE_CM_DEVICE_EX);
    if (createFunc == 0)
    {
        delete device;
        return CM_FAILURE;
    }

    INT res = createFunc(device->m_linux, version, va_dpy, mode);
    if (res != CM_SUCCESS)
    {
        delete device;
        return CM_FAILURE;
    }

    pD = device;
    return CM_SUCCESS;
}

#endif /* #ifdef CM_WIN */

INT DestroyCmDevice(CmDevice *& pD)
{
    CmDeviceImpl * device = (CmDeviceImpl *)pD;

    if (pD == 0 || device->m_dll == 0)
        return CM_SUCCESS;

    INT res = CM_SUCCESS;

    if (void* destroyFunc = so_get_addr(device->m_dll, FUNC_NAME_DESTROY_CM_DEVICE))
    {
        switch (device->GetPlatform())
        {
#if defined(CM_WIN)
        case DX9:
            res = ((DestroyCmDeviceDx9FuncType)destroyFunc)(device->m_dx9);
            break;
        case DX11:
            res = ((DestroyCmDeviceDx11FuncType)destroyFunc)(device->m_dx11);
            break;
#else
        case VAAPI:
            res = ((DestroyCmDeviceVAAPIFuncType)destroyFunc)(device->m_linux);
            break;
#endif
        }
    }

    so_free(device->m_dll);

    device->m_dll  = 0;
#if defined(CM_WIN)
    device->m_dx9  = 0;
    device->m_dx11 = 0;
#else
    device->m_linux = 0;
#endif

    delete device;
    pD = 0;
    return res;
}

int ReadProgram(CmDevice * device, CmProgram *& program, const unsigned char * buffer, unsigned int len)
{
#ifdef CMRT_EMU
    (void)buffer;
    (void)len;
    return device->LoadProgram(0, 0, program, "nojitter");
#else //CMRT_EMU
    return device->LoadProgram((void *)buffer, len, program, "nojitter");
#endif //CMRT_EMU
}

int ReadProgramJit(CmDevice * device, CmProgram *& program, const unsigned char * buffer, unsigned int len)
{
#ifdef CMRT_EMU
    (void)buffer;
    (void)len;
    return device->LoadProgram(0, 0, program);
#else //CMRT_EMU
    return device->LoadProgram((void *)buffer, len, program);
#endif //CMRT_EMU
}

int CreateKernel(CmDevice * device, CmProgram * program, const char * kernelName, const void * fncPnt, CmKernel *& kernel, const char * options)
{
#ifdef CMRT_EMU
    return device->CreateKernel(program, kernelName, fncPnt, kernel, options);
#else //CMRT_EMU
    (void)fncPnt;
    return device->CreateKernel(program, kernelName, kernel, options);
#endif //CMRT_EMU
}

CmEvent *CM_NO_EVENT = ((CmEvent *)(-1));

#if defined(__clang__)
  #pragma clang diagnostic pop
#else
#if defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif
#endif
