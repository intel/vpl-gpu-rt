// Copyright (c) 2010-2022 Intel Corporation
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

#ifdef MFX_TRACE_ENABLE_TEXTLOG
extern "C"
{

#define MFT_TRACE_PATH_TO_TEMP_LIBLOG MFX_TRACE_STRING("/tmp/mfxlib.log")

#include <stdio.h>
#include "mfx_trace_utils.h"
#include "mfx_trace_textlog.h"
#include "unistd.h"

/*------------------------------------------------------------------------------*/
static FILE* g_mfxTracePrintfFile = NULL;
static mfxTraceChar g_mfxTracePrintfFileName[MAX_PATH] =
    MFT_TRACE_PATH_TO_TEMP_LIBLOG;
static mfxTraceU32 g_PrintfSuppress =
    MFX_TRACE_TEXTLOG_SUPPRESS_FILE_NAME |
    MFX_TRACE_TEXTLOG_SUPPRESS_LINE_NUM |
    MFX_TRACE_TEXTLOG_SUPPRESS_LEVEL;

bool trace_bEnable = false;
double timeStamp = 0;
std::string trace_autotag = "intialized";
static std::map<std::pair<uint64_t, std::string>, std::vector<TickTime>*> records;
/*------------------------------------------------------------------------------*/
void startTick(const std::string& tag)
{
}

void stopTick(const std::string& tag)
{
}


mfxTraceU32 MFXTraceTextLog_GetRegistryParams(void)
{
    FILE* conf_file = mfx_trace_open_conf_file(MFX_TRACE_CONFIG);
    mfxTraceU32 value = 0;

    if (!conf_file) return 0;
    mfx_trace_get_conf_string(conf_file,
                              MFX_TRACE_TEXTLOG_REG_FILE_NAME,
                              g_mfxTracePrintfFileName,
                              sizeof(g_mfxTracePrintfFileName));

    if (!mfx_trace_get_conf_dword(conf_file,
                                  MFX_TRACE_TEXTLOG_REG_SUPPRESS,
                                  &value))
    {
        g_PrintfSuppress = value;
    }
    if (!mfx_trace_get_conf_dword(conf_file,
                                  MFX_TRACE_TEXTLOG_REG_PERMIT,
                                  &value))
    {
        g_PrintfSuppress &= ~value;
    }
    fclose(conf_file);
    return 0;
}


/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTextLog_Init()
{
    mfxTraceU32 sts = 0;

    sts = MFXTraceTextLog_Close();
    if (!sts) sts = MFXTraceTextLog_GetRegistryParams();
    std::string StrPid = "/mfxlib_Pid";
    std::string filename_path = VplLogPath + StrPid + std::to_string(getpid()) + "_Tid" + std::to_string(pthread_self()) + ".log";
    strncpy(g_mfxTracePrintfFileName,filename_path.c_str(), MAX_PATH - 1);
    g_mfxTracePrintfFileName[MAX_PATH - 1] = 0;

    if (!sts)
    {
        if (!g_mfxTracePrintfFile)
        {
            if (!mfx_trace_tcmp(g_mfxTracePrintfFileName, MFX_TRACE_STRING("stdout"))) g_mfxTracePrintfFile = stdout;
            else g_mfxTracePrintfFile = mfx_trace_tfopen(g_mfxTracePrintfFileName, MFX_TRACE_STRING("a"));
        }
        if (!g_mfxTracePrintfFile) return 1;
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTextLog_Close(void)
{
    g_PrintfSuppress = //MFX_TRACE_TEXTLOG_SUPPRESS_FILE_NAME |
                       //MFX_TRACE_TEXTLOG_SUPPRESS_LINE_NUM |
                       MFX_TRACE_TEXTLOG_SUPPRESS_LEVEL;
    if (g_mfxTracePrintfFile)
    {
        fclose(g_mfxTracePrintfFile);
        g_mfxTracePrintfFile = NULL;
    }
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTextLog_SetLevel(mfxTraceChar* /*category*/, mfxTraceLevel /*level*/)
{
    return 1;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTextLog_DebugMessage(mfxTraceStaticHandle* static_handle,
                                   const char *file_name, mfxTraceU32 line_num,
                                   const char *function_name,
                                   mfxTraceChar* category, mfxTraceLevel level,
                                   const char *message, const char *format, ...)
{
    mfxTraceU32 res = 0;
    va_list args;

    va_start(args, format);
    res = MFXTraceTextLog_vDebugMessage(static_handle,
                                       file_name , line_num,
                                       function_name,
                                       category, level,
                                       message, format, args);
    va_end(args);
    return res;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTextLog_vDebugMessage(mfxTraceStaticHandle* static_handle,
                                    const char *file_name, mfxTraceU32 line_num,
                                    const char *function_name,
                                    mfxTraceChar* category, mfxTraceLevel level,
                                    const char *message,
                                    const char *format, va_list args)
{
    if (!g_mfxTracePrintfFile) return 1;

    size_t len = MFX_TRACE_MAX_LINE_LENGTH;
    char str[MFX_TRACE_MAX_LINE_LENGTH] = {0}, *p_str = str;
    char strfile_name[MFX_TRACE_MAX_LINE_LENGTH] = {0};
    char enterChr[] = ": ENTER";
    char exitChr[] = ": EXIT";
    strncpy(strfile_name, file_name, MFX_TRACE_MAX_LINE_LENGTH-1);
    strfile_name[MFX_TRACE_MAX_LINE_LENGTH - 1] = 0;
    char* g_fimeName = strrchr(strfile_name, '/') + 1;
    if (g_fimeName && !(g_PrintfSuppress & MFX_TRACE_TEXTLOG_SUPPRESS_FILE_NAME))
    {
        p_str = mfx_trace_sprintf(p_str, len, "=====>%-40s: ", g_fimeName);
    }
    if (line_num && !(g_PrintfSuppress & MFX_TRACE_TEXTLOG_SUPPRESS_LINE_NUM))
    {
        p_str = mfx_trace_sprintf(p_str, len, "%-10d: ", line_num);
    }
    if (category && !(g_PrintfSuppress & MFX_TRACE_TEXTLOG_SUPPRESS_CATEGORY))
    {
        p_str = mfx_trace_sprintf(p_str, len, "%s: ", category);
    }
    if (function_name && !(g_PrintfSuppress & MFX_TRACE_TEXTLOG_SUPPRESS_FUNCTION_NAME) && !(format && ((strcmp(format, exitChr) == 0) || (strcmp(format, enterChr) == 0))))
    {
        p_str = mfx_trace_sprintf(p_str, len, "%-60s: ", function_name);
    }
    if (level == MFX_TRACE_LEVEL_CRITICAL_INFO)
    {
        p_str = mfx_trace_sprintf(p_str, len, "%s", "[Critical] ");
    }
    else if (level == MFX_TRACE_LEVEL_WARNING_INFO)
    {
        p_str = mfx_trace_sprintf(p_str, len, "%s", "[Warning] ");
    }
    if (message && strlen(message) != 0)
    {
        if (format && ((strcmp(format, exitChr) == 0) || (strcmp(format, enterChr) == 0)))
        {
            p_str = mfx_trace_sprintf(p_str, len, "%-40s", message);
        }
        else
        {
            p_str = mfx_trace_sprintf(p_str, len, "%s", message);
        }
    }
    if (format)
    {
        p_str = mfx_trace_vsprintf(p_str, len, format, args);
        if (strcmp(format, exitChr) == 0)
        {
            if (static_handle)
            {
                if (static_handle->tick.totalTime)
                {
                    p_str = mfx_trace_sprintf(p_str, len, "\t\tExec Time: %5.4fms\t\t", static_handle->tick.totalTime);
                }

                if (static_handle->tick.callCount)
                {
                    p_str = mfx_trace_sprintf(p_str, len, "\tCall Count: %I64d", static_handle->tick.callCount);
                }
            }

            p_str = mfx_trace_sprintf(p_str, len, "\n");
        }
    }
    {
        p_str = mfx_trace_sprintf(p_str, len, "\n");
    }
    fprintf(g_mfxTracePrintfFile, "%s", str);
    fflush(g_mfxTracePrintfFile);
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTextLog_BeginTask(mfxTraceStaticHandle *static_handle,
                                const char *file_name, mfxTraceU32 line_num,
                                const char *function_name,
                                mfxTraceChar* category, mfxTraceLevel level,
                                const char *task_name, const mfxTraceTaskType /*task_type*/,
                                mfxTraceTaskHandle* handle, const void * /*task_params*/)
{
    if (handle)
    {
        handle->fd1.str    = (char*)file_name;
        handle->fd2.uint32 = line_num;
        handle->fd3.str    = (char*)function_name;
        handle->fd4.str    = (char*)task_name;
    }


    return MFXTraceTextLog_DebugMessage(static_handle,
                                       file_name, line_num,
                                       function_name,
                                       category, level,
                                       task_name, (task_name)? ": ENTER": "ENTER");
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceTextLog_EndTask(mfxTraceStaticHandle *static_handle,
                              mfxTraceTaskHandle *handle)
{
    if (!handle) return 1;

    char *file_name = NULL, *function_name = NULL, *task_name = NULL;
    mfxTraceU32 line_num = 0;
    mfxTraceChar* category = NULL;
    mfxTraceLevel level = MFX_TRACE_LEVEL_DEFAULT;

    if (handle)
    {
        category      = static_handle->category;
        level         = static_handle->level;

        file_name     = handle->fd1.str;
        line_num      = handle->fd2.uint32;
        function_name = handle->fd3.str;
        task_name     = handle->fd4.str;
    }


    return MFXTraceTextLog_DebugMessage(static_handle,
                                       file_name, line_num,
                                       function_name,
                                       category, level,
                                       task_name, (task_name)? ": EXIT": "EXIT");
}

} // extern "C"
#endif // #ifdef MFX_TRACE_ENABLE_TEXTLOG
