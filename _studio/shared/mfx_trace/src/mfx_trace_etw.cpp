// Copyright (c) 2010-2020 Intel Corporation
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

#ifdef MFX_TRACE_ENABLE_ETW
extern "C"
{

#include "mfx_trace_utils.h"
#include "mfx_trace_etw.h"

#include <stdio.h>
#include <math.h>
#include <evntprov.h>
#include <evntrace.h>

// This header is an on fly generated file
// Windows Message Compiler generates it from manifest file during mfx_trace build
#include "media_sdk_etw.h"
#include "mfx_trace_utils.h"
#include "mfx_trace_etw.h"

/*------------------------------------------------------------------------------*/
// {2D6B112A-D21C-4A40-9BF2-A3EDF212F624}
static const GUID MFX_TRACE_ETW_GUID =
    {0x2d6b112a, 0xd21C, 0x4a40, { 0x9b, 0xf2, 0xa3, 0xed, 0xf2, 0x12, 0xf6, 0x24 } };

/*------------------------------------------------------------------------------*/

#define MFX_2_ETW_TASK(MFXTASK) MFXTASK##_ETW

#define ERROR_MSG "MFX trace task declared in agnostric mfx_trace.h doesn't match ETW task from an auto-generated header file compile from the manifest file!"

#define CHECK_TYPE_MATCH(MFXTASK) \
    static_assert(MFXTASK == MFX_2_ETW_TASK(MFXTASK), ERROR_MSG);

static_assert(MFX_TRACE_DEFAULT_TASK == 0, ERROR_MSG);
CHECK_TYPE_MATCH(MFX_TRACE_API_DECODE_QUERY_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_DECODE_QUERY_IOSURF_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_DECODE_HEADER_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_DECODE_INIT_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_DECODE_CLOSE_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_DECODE_FRAME_ASYNC_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_ENCODE_QUERY_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_ENCODE_QUERY_IOSURF_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_ENCODE_INIT_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_ENCODE_CLOSE_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_ENCODE_FRAME_ASYNC_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_VPP_QUERY_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_VPP_QUERY_IOSURF_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_VPP_INIT_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_VPP_CLOSE_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_VPP_LEGACY_ROUTINE_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_VPP_RUN_FRAME_VPP_ASYNC_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_VPP_RUN_FRAME_VPP_ASYNC_EX_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_MFX_INIT_EX_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_MFX_CLOSE_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_DO_WORK_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_API_SYNC_OPERATION_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_HOTSPOT_SCHED_WAIT_GLOBAL_EVENT_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_HOTSPOT_DDI_EXECUTE_D3DX_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_HOTSPOT_DDI_QUERY_D3DX_TASK);
CHECK_TYPE_MATCH(MFX_TRACE_HOTSPOT_CM_COPY);

#undef MFX_2_ETW_TASK
#undef ERROR_MSG
#undef CHECK_TYPE

/*------------------------------------------------------------------------------*/

class mfxETWGlobalHandle
{
public:
    ~mfxETWGlobalHandle(void)
    {
        Close();
    };

    void Close(void)
    {
        EventUnregisterIntel_MediaSDK();
    };
};

static mfxETWGlobalHandle g_ETWGlobalHandle;

/*------------------------------------------------------------------------------*/
enum
{
    MPA_ETW_OPCODE_MESSAGE = 0x03, // String message
    MPA_ETW_OPCODE_PARAM_INTEGER = 0x10, // Param: int
    MPA_ETW_OPCODE_PARAM_DOUBLE = 0x11, // Param: double
    MPA_ETW_OPCODE_PARAM_POINTER = 0x12, // Param: void*
    MPA_ETW_OPCODE_PARAM_STRING = 0x13, // Param: char*
    MPA_ETW_OPCODE_BUFFER = 0x14, // Param: buffer
};

/*------------------------------------------------------------------------------*/
UINT32 MFXTraceETW_GetRegistryParams(void)
{
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_Init()
{
    if (IntelMediaSDK_Context.RegistrationHandle) return 0;

    TCHAR reg_filename[MAX_PATH];
    reg_filename[0] = 0;
    mfx_trace_get_reg_string(HKEY_CURRENT_USER, MFX_TRACE_REG_PARAMS_PATH, _T("ETW"), reg_filename, sizeof(reg_filename));
    if (!reg_filename[0])
    {
        mfx_trace_get_reg_string(HKEY_LOCAL_MACHINE, MFX_TRACE_REG_PARAMS_PATH, _T("ETW"), reg_filename, sizeof(reg_filename));
    }
    if (!reg_filename[0])
    {
        return 1;
    }

    return ERROR_SUCCESS != EventRegisterIntel_MediaSDK() ? 1 : 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_Close(void)
{
    g_ETWGlobalHandle.Close();
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_SetLevel(mfxTraceChar* /*category*/, mfxTraceLevel /*level*/)
{
    return 1;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_DebugMessage(mfxTraceStaticHandle* handle,
                                 const char *file_name, mfxTraceU32 line_num,
                                 const char *function_name,
                                 mfxTraceChar* category, mfxTraceLevel level,
                                 const char *message, const char *format, ...)
{
    mfxTraceU32 res = 0;
    va_list args;

    va_start(args, format);
    res = MFXTraceETW_vDebugMessage(handle,
                                    file_name , line_num,
                                    function_name,
                                    category, level,
                                    message, format, args);
    va_end(args);
    return res;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_vDebugMessage(mfxTraceStaticHandle* /*handle*/,
                                 const char* /*file_name*/, mfxTraceU32 /*line_num*/,
                                 const char* /*function_name*/,
                                 mfxTraceChar* /*category*/, mfxTraceLevel level,
                                 const char* message,
                                 const char* format, va_list args)
{
    EVENT_DESCRIPTOR descriptor = {};
    EVENT_DATA_DESCRIPTOR data_descriptor[4];
    char format_UNK[MFX_TRACE_MAX_LINE_LENGTH] = {0};
    ULONG count = 0;

    if (!message) return 1;

    descriptor.Level = (UCHAR)level;
    descriptor.Keyword = MFX_ETW_KEYWORD_NON_TYPED_EVENT;
    if (!EventEnabled(Intel_MediaSDKHandle, &descriptor))
    {
        return 0; // no ETW consumer for given GUID, Level, Keyword
    }

    // Purpose of this is to keep legacy code below (which actually doesn't match manifest based scheme) working
    descriptor.Keyword = 0;

    EventDataDescCreate(&data_descriptor[count++], message, (ULONG)strlen(message) + 1);

    if (format == nullptr)
    {
        descriptor.Opcode = MPA_ETW_OPCODE_MESSAGE;
    } else if (message[0] == '^')
    {
        if (!strncmp(message, "^ModuleHandle^", 14))
        {
            HMODULE hModule = (HMODULE)va_arg(args, void*);
            if (hModule)
            {
                if (SUCCEEDED(GetModuleFileNameA(hModule, format_UNK, sizeof(format_UNK) - 1)))
                {
                    descriptor.Opcode = MPA_ETW_OPCODE_PARAM_STRING;
                    EventDataDescCreate(&data_descriptor[count++], format_UNK, (ULONG)strlen(format_UNK) + 1);
                }
            }
        }
    }

    if (!descriptor.Opcode && format != nullptr && format[0] == '%')
    {
        if (format[2] == 0)
        {
            switch (format[1])
            {
            case 'd':
            case 'x':
                descriptor.Opcode = MPA_ETW_OPCODE_PARAM_INTEGER;
                descriptor.Keyword = va_arg(args, int);
                break;
            case 'p':
                descriptor.Opcode = MPA_ETW_OPCODE_PARAM_POINTER;
                descriptor.Keyword = (UINT64)va_arg(args, void*);
                break;
            case 'f':
            case 'g':
                descriptor.Opcode = MPA_ETW_OPCODE_PARAM_DOUBLE;
                *(double*)&descriptor.Keyword = va_arg(args, double);
                break;
            case 's':
                descriptor.Opcode = MPA_ETW_OPCODE_PARAM_STRING;
                char *str = va_arg(args, char*);
                EventDataDescCreate(&data_descriptor[count++], str, (ULONG)strlen(str) + 1);
                break;
            }
        } else
        {
            if (!strcmp(format, "%p[%d]"))
            {
                descriptor.Opcode = MPA_ETW_OPCODE_BUFFER;
                char *buffer = va_arg(args, char*);
                int size = va_arg(args, int);
                EventDataDescCreate(&data_descriptor[count++], buffer, size);
            }
        }
    }
    if (!descriptor.Opcode)
    {
        descriptor.Opcode = MPA_ETW_OPCODE_PARAM_STRING;
        size_t format_UNK_len = MFX_TRACE_MAX_LINE_LENGTH;
        mfx_trace_vsprintf(format_UNK, format_UNK_len, format, args);
        EventDataDescCreate(&data_descriptor[count++], format_UNK, (ULONG)strlen(format_UNK) + 1);
    }

    if (!count ||
        (ERROR_SUCCESS != EventWrite(Intel_MediaSDKHandle, &descriptor, count, data_descriptor)))
    {
        return 1;
    }

    return 0;
}

/*------------------------------------------------------------------------------*/

enum {
    MPA_ETW_OPCODE_START = 1,
    MPA_ETW_OPCODE_STOP = 2,
};

mfxTraceU32 MFXTraceETW_SendNamedEvent(mfxTraceStaticHandle *static_handle,
                                       mfxTraceTaskHandle *handle,
                                       UCHAR opcode,
                                       const mfxTraceU32 *task_params = nullptr)
{
    EVENT_DESCRIPTOR descriptor;
    EVENT_DATA_DESCRIPTOR data_descriptor;
    char* task_name = nullptr;

    if (!handle) return 1;

    task_name = handle->etw1.str;
    if (!task_name) return 1;

    memset(&descriptor, 0, sizeof(EVENT_DESCRIPTOR));
    memset(&data_descriptor, 0, sizeof(EVENT_DATA_DESCRIPTOR));

    descriptor.Opcode = opcode;
    descriptor.Level = (static_handle) ? (UCHAR)static_handle->level : 0;
    descriptor.Task = (USHORT)handle->etw2.uint32;
    if (task_params)
    {
        descriptor.Id = 1;
        descriptor.Keyword = *task_params;
    }

    EventDataDescCreate(&data_descriptor, task_name, (ULONG)(strlen(task_name) + 1));

    EventWrite(Intel_MediaSDKHandle, &descriptor, 1, &data_descriptor);
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_BeginTask(mfxTraceStaticHandle * static_handle,
                             const char * /*file_name*/, mfxTraceU32 line_num,
                             const char * function_name,
                             mfxTraceChar* /*category*/, mfxTraceLevel /*level*/,
                             const char * task_name, const mfxTraceTaskType /*task_type*/,
                             mfxTraceTaskHandle* task_handle, const void * task_params)
{
    if (!task_handle) return 1;
    auto function_or_task_name = ((task_name) ? task_name : function_name);
    task_handle->etw1.str    = (char*)function_or_task_name;
    task_handle->etw2.uint32 = line_num;

    return MFXTraceETW_SendNamedEvent(static_handle, task_handle, MPA_ETW_OPCODE_START, (const UINT32*)task_params);
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceETW_EndTask(mfxTraceStaticHandle * static_handle,
                           mfxTraceTaskHandle* task_handle)
{
    if (!task_handle) return 1;

    return MFXTraceETW_SendNamedEvent(static_handle, task_handle, MPA_ETW_OPCODE_STOP);
}

mfxTraceU32 MFXTrace_ETWEvent(uint16_t task, uint8_t opcode, uint8_t level, uint64_t size, void *ptr)
{
    EVENT_DESCRIPTOR descriptor = {};
    descriptor.Opcode = opcode;
    descriptor.Level = level;
    descriptor.Task = task;
    // VERY IMPORTANT !!!
    // MANIFEST FILE SHALL FOLLOW THE SAME RULE DEFINING ID FOR EVENTS:
    descriptor.Keyword = task ? MFX_ETW_KEYWORD_TYPED_EVENT : MFX_ETW_KEYWORD_NON_TYPED_EVENT;
    descriptor.Id = (descriptor.Task << 2) + descriptor.Opcode; // Since ONLY Info/Start/Stop opcodes are used
                                                                // we reserve 2 lowest bits for opcode and the rest for task Id.

    EVENT_DATA_DESCRIPTOR EventData[1];
    int count = 0;
    if (size && ptr) {
        EventDataDescCreate(&EventData[count++], ptr, (ULONG)size);
    }

    EventWrite(Intel_MediaSDKHandle, &descriptor, count, count ? EventData : nullptr);
    return 0;
}

} // extern "C"
#endif // #ifdef MFX_TRACE_ENABLE_ETW
