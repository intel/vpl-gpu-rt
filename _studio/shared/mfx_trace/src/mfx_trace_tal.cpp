// Copyright (c) 2010-2018 Intel Corporation
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

#include "mfx_trace.h"

#ifdef MFX_TRACE_ENABLE_TAL
extern "C"
{

#include "mfx_trace_utils.h"
#include "mfx_trace_tal.h"

/*------------------------------------------------------------------------------*/

UINT32 MFXTraceTAL_GetRegistryParams(void)
{
    return 0;
}

/*------------------------------------------------------------------------------*/

#ifdef WIN64
#define DLL_FILENAME    _T("mfx_tal64.dll")
#else
#define DLL_FILENAME    _T("mfx_tal32.dll")
#endif

typedef LONGLONG mfxTimerId;

static HMODULE pDLL;
static int (*pMFXTiming_Init)(const WCHAR *filename, int level);
static int (*pMFXTiming_StartTimer)(const char *name, mfxTimerId *id);
static int (*pMFXTiming_AddParam )(const char *name, mfxTimerId id, const char *param_name, const char *param_value);
static int (*pMFXTiming_AddParami)(const char *name, mfxTimerId id, const char *param_name, int param_value);
static int (*pMFXTiming_AddParamf)(const char *name, mfxTimerId id, const char *param_name, double param_value);
static int (*pMFXTiming_StopTimer)(const char *name, mfxTimerId id);
static int (*pMFXTiming_Close)();

#define LOAD_FUNC(FUNC)                                         \
    *(FARPROC*)&p##FUNC = GetProcAddress(pDLL, #FUNC);          \

mfxTraceU32 MFXTraceTAL_Init()
{
    TCHAR reg_filename[MAX_PATH];
    mfxTraceU32 sts = 0;

    reg_filename[0] = 0;
    mfx_trace_get_reg_string(MFX_TRACE_REG_ROOT, MFX_TRACE_REG_PARAMS_PATH, _T("TAL"), reg_filename, sizeof(reg_filename));
    if (!reg_filename[0]) return 1;


    if (pDLL && pMFXTiming_Init) return 0;

    //sts = MFXTraceTAL_Close();

    if (!sts)
    {
        sts = MFXTraceTAL_GetRegistryParams();
    }
    if (!sts)
    {
        HMODULE pDLL = LoadLibrary(DLL_FILENAME);
        LOAD_FUNC(MFXTiming_Init);
        LOAD_FUNC(MFXTiming_StartTimer);
        LOAD_FUNC(MFXTiming_AddParam);
        LOAD_FUNC(MFXTiming_AddParami);
        LOAD_FUNC(MFXTiming_AddParamf);
        LOAD_FUNC(MFXTiming_StopTimer);
        LOAD_FUNC(MFXTiming_Close);

        if (!pMFXTiming_Init)
        {
            return 1;
        }
        sts = pMFXTiming_Init(reg_filename, 0);
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTAL_Close(void)
{
    if (pMFXTiming_Close) pMFXTiming_Close();
    FreeLibrary(pDLL);
    pDLL = 0;
    pMFXTiming_Init = 0;
    pMFXTiming_StartTimer = 0;
    pMFXTiming_AddParam = 0;
    pMFXTiming_AddParami = 0;
    pMFXTiming_AddParamf = 0;
    pMFXTiming_StopTimer = 0;
    pMFXTiming_Close = 0;
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTAL_SetLevel(mfxTraceChar* /*category*/, mfxTraceLevel /*level*/)
{
    return 1;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTAL_DebugMessage(mfxTraceStaticHandle* handle,
                                 const char *file_name, mfxTraceU32 line_num,
                                 const char *function_name,
                                 mfxTraceChar* category, mfxTraceLevel level,
                                 const char *message, const char *format, ...)
{
    mfxTraceU32 res = 0;
    va_list args;

    va_start(args, format);
    res = MFXTraceTAL_vDebugMessage(handle,
                                    file_name , line_num,
                                    function_name,
                                    category, level,
                                    message, format, args);
    va_end(args);
    return res;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTAL_vDebugMessage(mfxTraceStaticHandle* /*handle*/,
                                 const char* /*file_name*/, mfxTraceU32 /*line_num*/,
                                 const char* /*function_name*/,
                                 mfxTraceChar* /*category*/, mfxTraceLevel /*level*/,
                                 const char* message,
                                 const char* format, va_list args)
{
    if (!format)
    {
        if (!message) return 1;
        if (pMFXTiming_AddParam) pMFXTiming_AddParam(0, 0, message, 0);
    }
    else if (!strcmp(format, MFX_TRACE_FORMAT_I) ||
             !strcmp(format, MFX_TRACE_FORMAT_X) ||
             !strcmp(format, MFX_TRACE_FORMAT_D))
    {
        if (pMFXTiming_AddParami) pMFXTiming_AddParami(0, 0, message, va_arg(args, int));
    }
    else if (!strcmp(format, MFX_TRACE_FORMAT_P))
    {
        if (pMFXTiming_AddParami) pMFXTiming_AddParami(0, 0, message, (int)va_arg(args, void*));
    }
    else if (!strcmp(format, MFX_TRACE_FORMAT_F))
    {
        if (pMFXTiming_AddParamf) pMFXTiming_AddParamf(0, 0, message, va_arg(args, double));
    }
    else if (pMFXTiming_AddParam)
    {
        char format_UNK[MFX_TRACE_MAX_LINE_LENGTH] = {0};
        size_t format_UNK_len = MFX_TRACE_MAX_LINE_LENGTH;
        mfx_trace_vsprintf(format_UNK, format_UNK_len, format, args);
        pMFXTiming_AddParam(0, 0, message, format_UNK);
    }

    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTAL_BeginTask(mfxTraceStaticHandle * /*static_handle*/,
                             const char * /*file_name*/, mfxTraceU32 /*line_num*/,
                             const char *function_name,
                             mfxTraceChar* /*category*/, mfxTraceLevel /*level*/,
                             const char *task_name, mfxTraceTaskHandle* /*task_handle*/,
                             const void * /*task_params*/)
{
    if (pMFXTiming_StartTimer)
    {
        const char* name = (task_name) ? task_name : function_name;
        return pMFXTiming_StartTimer(name, 0);
    }
    return 1;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTAL_EndTask(mfxTraceStaticHandle* /*static_handle*/,
                                mfxTraceTaskHandle* /*task_handle*/)
{
    if (pMFXTiming_StopTimer)
    {
        return pMFXTiming_StopTimer(0, 0);
    }
    return 1;
}

} // extern "C"
#endif // #ifdef MFX_TRACE_ENABLE_TAL
