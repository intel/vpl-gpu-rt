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

#include "mfxenctools_dl_int.h"
#include "mfx_loader_utils.h"

MFXDLVideoSession::MFXDLVideoSession(void* module)
{
    m_session = (mfxSession)0;

    InitExPtr = reinterpret_cast<decltype(InitExPtr)>(
        GetModuleFuncAddress(module, "MFXInitEx"));
    ClosePtr = reinterpret_cast<decltype(ClosePtr)>(
        GetModuleFuncAddress(module, "MFXClose"));
    QueryVersionPtr = reinterpret_cast<decltype(QueryVersionPtr)>(
        GetModuleFuncAddress(module, "MFXQueryVersion"));
    SetFrameAllocatorPtr = reinterpret_cast<decltype(SetFrameAllocatorPtr)>(
        GetModuleFuncAddress(module, "MFXVideoCORE_SetFrameAllocator"));
    SetHandlePtr = reinterpret_cast<decltype(SetHandlePtr)>(
        GetModuleFuncAddress(module, "MFXVideoCORE_SetHandle"));
    SyncOperationPtr = reinterpret_cast<decltype(SyncOperationPtr)>(
        GetModuleFuncAddress(module, "MFXVideoCORE_SyncOperation"));
    GetSurfaceForEncodePtr = reinterpret_cast<decltype(GetSurfaceForEncodePtr)>(
        GetModuleFuncAddress(module, "MFXMemory_GetSurfaceForEncode"));
    JoinSessionPtr = reinterpret_cast<decltype(JoinSessionPtr)>(
        GetModuleFuncAddress(module, "MFXJoinSession"));
    DisjoinSessionPtr = reinterpret_cast<decltype(DisjoinSessionPtr)>(
        GetModuleFuncAddress(module, "MFXDisjoinSession"));
}


MFXDLVideoENCODE::MFXDLVideoENCODE(mfxSession session, void* module)
{
    m_session = session;
    
    InitPtr = reinterpret_cast<decltype(InitPtr)>(
        GetModuleFuncAddress(module, "MFXVideoENCODE_Init"));
    ClosePtr = reinterpret_cast<decltype(ClosePtr)>(
        GetModuleFuncAddress(module, "MFXVideoENCODE_Close"));
    EncodeFrameAsyncPtr = reinterpret_cast<decltype(EncodeFrameAsyncPtr)>(
        GetModuleFuncAddress(module, "MFXVideoENCODE_EncodeFrameAsync"));
}
 
MFXDLVideoVPP::MFXDLVideoVPP(mfxSession session, void* module)
{
    m_session = session;

    QueryIOSurfPtr = reinterpret_cast<decltype(QueryIOSurfPtr)>(
        GetModuleFuncAddress(module, "MFXVideoVPP_QueryIOSurf"));
    InitPtr = reinterpret_cast<decltype(InitPtr)>(
        GetModuleFuncAddress(module, "MFXVideoVPP_Init"));
    ClosePtr = reinterpret_cast<decltype(ClosePtr)>(
        GetModuleFuncAddress(module, "MFXVideoVPP_Close"));
    RunFrameVPPAsyncPtr = reinterpret_cast<decltype(RunFrameVPPAsyncPtr)>(
        GetModuleFuncAddress(module, "MFXVideoVPP_RunFrameVPPAsync"));
}
