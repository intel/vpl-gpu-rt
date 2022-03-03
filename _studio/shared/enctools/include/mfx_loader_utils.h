// Copyright (c) 2022 Intel Corporation
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

#ifndef __MFX_LOADER_UTILS_H__
#define __MFX_LOADER_UTILS_H__

#include "mfx_config.h"


#include <dlfcn.h>

#include <string.h>
#include <string>
inline std::string GetSelfModuleFullPath()
{
    Dl_info dl_info;
    dladdr((void*)GetSelfModuleFullPath, &dl_info);
    return std::string(dl_info.dli_fname);
}

inline std::string GenerateEnctoolPath(const char* fileName)
{
    std::string wdir;
    std::string path = GetSelfModuleFullPath();
    wdir = path.substr(0, path.find_last_of('/') + 1);
    wdir += "libmfx-gen/"; // enctools.so are inside dir "libmfx-gen" in the path of RT in linux

    std::string fileNameStr(fileName);
    std::string enctoolPath = wdir + fileNameStr;

    return enctoolPath;
}

inline void* LoadModule(const char* modulePath)
{
    void* hModule = nullptr;
    hModule = dlopen(modulePath, RTLD_LAZY);
    return hModule;
}

inline void UnLoadModule(void* hModule)
{
    if (hModule)
    {
        dlclose(hModule);
    }
}

inline void* LoadDependency(const char* dependencyFileName)
{
    std::string dllPath = GenerateEnctoolPath(dependencyFileName);
    return LoadModule(dllPath.c_str());
}

inline void* GetModuleFuncAddress(void* hModule, const char* funcName)
{
    return dlsym(hModule, funcName);
}

#endif
