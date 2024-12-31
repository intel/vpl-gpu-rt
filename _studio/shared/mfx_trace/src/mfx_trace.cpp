// Copyright (c) 2010-2024 Intel Corporation
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

#include "mfxdefs.h"
#include "mfx_trace.h"
#include "mfx_utils_perf.h"
#include "mfx_decode_dpb_logging.h"

static mfx_reflect::AccessibleTypesCollection g_Reflection;

mfx_reflect::AccessibleTypesCollection GetReflection()
{
    return g_Reflection;
}

#ifdef MFX_TRACE_ENABLE

#ifdef _MSVC_LANG
#pragma warning(disable: 4127) // conditional expression is constant
#endif

extern "C"
{
#include "mfx_trace_utils.h"
#include "mfx_trace_textlog.h"
#include "mfx_trace_stat.h"
#include "mfx_trace_itt.h"
#include "mfx_trace_ftrace.h"
}
#include <stdlib.h>
#include <string.h>
#include "vm_interlocked.h"
#include "mfx_reflect.h"

/*------------------------------------------------------------------------------*/

typedef mfxTraceU32 (*MFXTrace_InitFn)();

typedef mfxTraceU32 (*MFXTrace_SetLevelFn)(mfxTraceChar* category,
                                      mfxTraceLevel level);

typedef mfxTraceU32 (*MFXTrace_DebugMessageFn)(mfxTraceStaticHandle *static_handle,
                                          const char *file_name, mfxTraceU32 line_num,
                                          const char *function_name,
                                          mfxTraceChar* category, mfxTraceLevel level,
                                          const char *message, const char *format, ...);

typedef mfxTraceU32 (*MFXTrace_vDebugMessageFn)(mfxTraceStaticHandle *static_handle,
                                           const char *file_name, mfxTraceU32 line_num,
                                           const char *function_name,
                                           mfxTraceChar* category, mfxTraceLevel level,
                                           const char *message,
                                           const char *format, va_list args);

typedef mfxTraceU32 (*MFXTrace_BeginTaskFn)(mfxTraceStaticHandle *static_handle,
                                       const char *file_name, mfxTraceU32 line_num,
                                       const char *function_name,
                                       mfxTraceChar* category, mfxTraceLevel level,
                                       const char *task_name, const mfxTraceTaskType task_type,
                                       mfxTraceTaskHandle *task_handle,
                                       const void *task_params);

typedef mfxTraceU32 (*MFXTrace_EndTaskFn)(mfxTraceStaticHandle *static_handle,
                                     mfxTraceTaskHandle *task_handle);

typedef mfxTraceU32 (*MFXTrace_CloseFn)(void);

struct mfxTraceAlgorithm
{
    mfxTraceU32              m_OutputInitilized;
    mfxTraceU32              m_OutputMask;
    MFXTrace_InitFn          m_InitFn;
    MFXTrace_SetLevelFn      m_SetLevelFn;
    MFXTrace_DebugMessageFn  m_DebugMessageFn;
    MFXTrace_vDebugMessageFn m_vDebugMessageFn;
    MFXTrace_BeginTaskFn     m_BeginTaskFn;
    MFXTrace_EndTaskFn       m_EndTaskFn;
    MFXTrace_CloseFn         m_CloseFn;
};

/*------------------------------------------------------------------------------*/

static mfxTraceU32      g_OutputMode = MFX_TRACE_OUTPUT_TRASH;
static mfxTraceU32      g_Level      = MFX_TRACE_LEVEL_DEFAULT;
mfxTraceU64      EventCfg = 0;
mfxTraceU32      LogConfig = 0;
int32_t FrameIndex = -1;
char VplLogPath[VPLLOG_BUFFER_SIZE] = "";
static volatile uint32_t  g_refCounter = 0;

static mfxTraceU32           g_mfxTraceCategoriesNum = 0;
static mfxTraceCategoryItem* g_mfxTraceCategoriesTable = NULL;

mfxTraceAlgorithm g_TraceAlgorithms[] =
{
#ifdef MFX_TRACE_ENABLE_TEXTLOG
    {
        0,
        MFX_TRACE_OUTPUT_TEXTLOG,
        MFXTraceTextLog_Init,
        MFXTraceTextLog_SetLevel,
        MFXTraceTextLog_DebugMessage,
        MFXTraceTextLog_vDebugMessage,
        MFXTraceTextLog_BeginTask,
        MFXTraceTextLog_EndTask,
        MFXTraceTextLog_Close
    },
#endif
#ifdef MFX_TRACE_ENABLE_STAT
    {
        0,
        MFX_TRACE_OUTPUT_STAT,
        MFXTraceStat_Init,
        MFXTraceStat_SetLevel,
        MFXTraceStat_DebugMessage,
        MFXTraceStat_vDebugMessage,
        MFXTraceStat_BeginTask,
        MFXTraceStat_EndTask,
        MFXTraceStat_Close
    },
#endif
#ifdef MFX_TRACE_ENABLE_TAL
    {
        0,
        MFX_TRACE_OUTPUT_TAL,
        MFXTraceTAL_Init,
        MFXTraceTAL_SetLevel,
        MFXTraceTAL_DebugMessage,
        MFXTraceTAL_vDebugMessage,
        MFXTraceTAL_BeginTask,
        MFXTraceTAL_EndTask,
        MFXTraceTAL_Close
    },
#endif
#ifdef MFX_TRACE_ENABLE_ITT
    {
        0,
        MFX_TRACE_OUTPUT_ITT,
        MFXTraceITT_Init,
        MFXTraceITT_SetLevel,
        MFXTraceITT_DebugMessage,
        MFXTraceITT_vDebugMessage,
        MFXTraceITT_BeginTask,
        MFXTraceITT_EndTask,
        MFXTraceITT_Close
    },
#endif
#ifdef MFX_TRACE_ENABLE_FTRACE
    {
        0,
        MFX_TRACE_OUTPUT_FTRACE,
        MFXTraceFtrace_Init,
        MFXTraceFtrace_SetLevel,
        MFXTraceFtrace_DebugMessage,
        MFXTraceFtrace_vDebugMessage,
        MFXTraceFtrace_BeginTask,
        MFXTraceFtrace_EndTask,
        MFXTraceFtrace_Close
    },
#endif
};

/*------------------------------------------------------------------------------*/


#define CATEGORIES_BUFFER_SIZE 1024
#define USER_FEATURE_FILE_NEXT               "/etc/igfx_user_feature_next.txt"
#define USER_SETTING_CONFIG_PATH             "[config]"
#define USER_SETTING_REPORT_PATH             "[report]"

mfxTraceU32 MFXTrace_GetRegistryParams(void)
{
    std::ifstream regStream;
    std::map<std::string, std::map<std::string, std::string>> RegBufferMap;

    regStream.open(USER_FEATURE_FILE_NEXT);
    if (regStream.good())
    {
        std::string id = "";
        while (!regStream.eof())
        {
            std::string line = "";
            std::getline(regStream, line);
            auto endIndex = line.find("\r");
            if (endIndex != std::string::npos)
            {
                line = line.substr(0, endIndex);
            }
            if (std::string::npos != line.find(USER_SETTING_CONFIG_PATH))
            {
                id = USER_SETTING_CONFIG_PATH;
            }
            else if (std::string::npos != line.find(USER_SETTING_REPORT_PATH))
            {
                id = USER_SETTING_REPORT_PATH;
            }
            else if (line.find("]") != std::string::npos)
            {
                auto mkPos = line.find_last_of("]");
                id = line.substr(0, mkPos + 1);
            }
            else
            {
                if (id == USER_SETTING_REPORT_PATH)
                {
                    continue;
                }
                std::size_t pos = line.find("=");
                if (std::string::npos != pos && !id.empty())
                {
                    std::string name = line.substr(0, pos);
                    std::string value = line.substr(pos + 1);
                    auto& keys = RegBufferMap[id];
                    keys[name] = value;
                }
            }
        }
    }
    regStream.close();

    for (auto multiter = RegBufferMap.begin(); multiter != RegBufferMap.end(); multiter++)
    {
        if (multiter->first == "[config]")
        {
            for (auto iter = multiter->second.begin(); iter != multiter->second.end(); iter++)
            {
                if (iter->first == "VPL TXT LOG")
                {
                    if (stoi(iter->second) > 0 && stoi(iter->second) < MFX_TXTLOG_LEVEL_API)
                    {
                        g_OutputMode |= MFX_TRACE_OUTPUT_TEXTLOG;
                        g_Level = stoi(iter->second);
                    }
                }
                else if (iter->first == "VPL LOG PATH")
                {
                    if (!iter->second.empty())
                    {
                        strncpy(VplLogPath, iter->second.c_str(), VPLLOG_BUFFER_SIZE - 1);
                        VplLogPath[VPLLOG_BUFFER_SIZE - 1] = 0;
                    }
                    else
                    {
                        strcpy(VplLogPath, "/tmp");
                    }
                }
                else if (iter->first == "VPL PERF LOG" && stoi(iter->second))
                {
                    g_perfutility = PerfUtility::getInstance();
                }
                else if (iter->first == "VPL PERF PATH")
                {
                    if (!iter->second.empty())
                    {
                        PerfUtility::perfFilePath = iter->second;
                    }
                }
                else if (iter->first == "VPL DPB LOG" && stoi(iter->second))
                {
                    dpb_logger = DPBLog::getInstance();
                }
                else if (iter->first == "VPL DPB PATH")
                {
                    if (!iter->second.empty())
                    {
                        DPBLog::dpbFilePath = iter->second;
                    }
                }
            }
        }
    }
    return 0;
}


mfxTraceU32 MFXTrace_GetEnvParams(void)
{
    //Capture different info according to VPL_EVENT_TRACE_CFG
    const char* g_eventCfg = std::getenv("VPL_EVENT_TRACE_CFG");
    char* endEventCfg = nullptr;
    if (g_eventCfg != nullptr)
    {
        EventCfg = std::strtol(g_eventCfg, &endEventCfg, 16);
    }
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 mfx_trace_get_category_index(mfxTraceChar* category, mfxTraceU32& index)
{
    if (category && g_mfxTraceCategoriesTable)
    {
        mfxTraceU32 i = 0;
        for (i = 0; i < g_mfxTraceCategoriesNum; ++i)
        {
            if (!mfx_trace_tcmp(g_mfxTraceCategoriesTable[i].m_name, category))
            {
                index = i;
                return 0;
            }
        }
    }
    return 1;
}

/*------------------------------------------------------------------------------*/

inline bool MFXTrace_IsPrintableCategoryAndLevel(mfxTraceU32 m_OutputInitilized, mfxTraceU32 level)
{
    bool logFlag = false;
    if (m_OutputInitilized == MFX_TRACE_OUTPUT_TEXTLOG) {

        if (g_Level == MFX_TXTLOG_LEVEL_MAX)
        {
            logFlag = true;
        }
#ifndef NDEBUG
        else if (g_Level == MFX_TXTLOG_LEVEL_API_AND_INTERNAL)
        {
            if (level != MFX_TRACE_LEVEL_API_PARAMS)
            {
                logFlag = true;
            }
        }
        else if (g_Level == MFX_TXTLOG_LEVEL_API_AND_PARAMS)
#else
        else if (g_Level == MFX_TXTLOG_LEVEL_API_AND_PARAMS)
#endif
        {
            if (level == MFX_TRACE_LEVEL_API_PARAMS || level == MFX_TRACE_LEVEL_API)
            {
                logFlag = true;
            }
        }
        else if (g_Level == MFX_TXTLOG_LEVEL_API)
        {
            if (level == MFX_TRACE_LEVEL_API)
            {
                logFlag = true;
            }
        }
    }
    else if(m_OutputInitilized == MFX_TRACE_OUTPUT_ETW)
    {
        if (EventCfg & (1 << MFX_ETWLOG_ENABLE))
        {
            logFlag = true;
        }
    }

    return logFlag;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTrace_Init()
{
    mfxTraceU32 sts = 0;
    mfxTraceU32 i = 0;
    mfxTraceU32 output_mode = 0;

#if defined(MFX_TRACE_ENABLE_TRASH)
    g_OutputMode |= MFX_TRACE_OUTPUT_TRASH;
#endif
#if defined(MFX_TRACE_ENABLE_STAT)
    g_OutputMode |= MFX_TRACE_OUTPUT_STAT;
#endif
#if defined(MFX_TRACE_ENABLE_TAL)
    g_OutputMode |= MFX_TRACE_OUTPUT_TAL;
#endif
#if defined(MFX_TRACE_ENABLE_ITT)
    g_OutputMode |= MFX_TRACE_OUTPUT_ITT;
#endif
#if defined(MFX_TRACE_ENABLE_FTRACE)
    g_OutputMode |= MFX_TRACE_OUTPUT_FTRACE;
#endif

    if (vm_interlocked_inc32(&g_refCounter) != 1)
    {
        return sts;
    }

    if (!g_Reflection.m_bIsInitialized &&
        g_OutputMode & (MFX_TRACE_OUTPUT_ETW | MFX_TRACE_OUTPUT_TEXTLOG))
    {
        g_Reflection.DeclareMsdkStructs();
        g_Reflection.m_bIsInitialized = true;
    }

    sts = MFXTrace_GetRegistryParams();
    sts = MFXTrace_GetEnvParams();
    if (!sts)
    {
        output_mode = g_OutputMode;
        g_OutputMode = 0;
        for (i = 0; i < sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm); ++i)
        {
            if (output_mode & g_TraceAlgorithms[i].m_OutputMask)
            {
                sts = g_TraceAlgorithms[i].m_InitFn();
                if (sts == 0)
                {
                    g_OutputMode |= output_mode;
                    g_TraceAlgorithms[i].m_OutputInitilized = g_TraceAlgorithms[i].m_OutputMask;
                }
            }
        }
        // checking search failure
        if (g_OutputMode == MFX_TRACE_OUTPUT_TRASH)
        {
            sts = 1;
        }
    }

    return sts;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTrace_Close(void)
{
    mfxTraceU32 sts = 0, res = 0;
    mfxTraceU32 i = 0;

    if (vm_interlocked_dec32(&g_refCounter) != 0)
    {
        return sts;
    }

    for (i = 0; i < sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm); ++i)
    {
        if (g_OutputMode & g_TraceAlgorithms[i].m_OutputInitilized)
        {
            res = g_TraceAlgorithms[i].m_CloseFn();
            if (!sts && res) sts = res;
        }
    }
    g_OutputMode = 0;
    g_Level = MFX_TRACE_LEVEL_DEFAULT;
    if (g_mfxTraceCategoriesTable)
    {
        free(g_mfxTraceCategoriesTable);
        g_mfxTraceCategoriesTable = NULL;
    }
    g_mfxTraceCategoriesNum = 0;
    return res;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTrace_SetLevel(mfxTraceChar* category, mfxTraceLevel level)
{
    mfxTraceU32 sts = 0, res = 0;
    mfxTraceU32 i = 0;

    for (i = 0; i < sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm); ++i)
    {
        if (g_OutputMode & g_TraceAlgorithms[i].m_OutputInitilized)
        {
            res = g_TraceAlgorithms[i].m_SetLevelFn(category, level);
            if (!sts && res) sts = res;
        }
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTrace_DebugMessage(mfxTraceStaticHandle *static_handle,
                             const char *file_name, mfxTraceU32 line_num,
                             const char *function_name,
                             mfxTraceChar* category, mfxTraceLevel level,
                             const char *message, const char *format, ...)
{
    mfxTraceU32 sts = 0, res = 0;
    mfxTraceU32 i = 0;
    va_list args;

    va_start(args, format);
    for (i = 0; i < sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm); ++i)
    {
        if (g_OutputMode & g_TraceAlgorithms[i].m_OutputInitilized)
        {
            if (!MFXTrace_IsPrintableCategoryAndLevel(g_TraceAlgorithms[i].m_OutputInitilized, level)) continue;

            res = g_TraceAlgorithms[i].m_vDebugMessageFn(static_handle,
                                                         file_name, line_num,
                                                         function_name,
                                                         category, level,
                                                         message, format, args);
            if (!sts && res) sts = res;
        }
    }
    va_end(args);
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTrace_vDebugMessage(mfxTraceStaticHandle *static_handle,
                              const char *file_name, mfxTraceU32 line_num,
                              const char *function_name,
                              mfxTraceChar* category, mfxTraceLevel level,
                              const char *message,
                              const char *format, va_list args)
{

    mfxTraceU32 sts = 0, res = 0;
    mfxTraceU32 i = 0;

    for (i = 0; i < sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm); ++i)
    {
        if (g_OutputMode & g_TraceAlgorithms[i].m_OutputInitilized)
        {
            if (!MFXTrace_IsPrintableCategoryAndLevel(g_TraceAlgorithms[i].m_OutputInitilized, level)) continue;

            res = g_TraceAlgorithms[i].m_vDebugMessageFn(static_handle,
                                                         file_name, line_num,
                                                         function_name,
                                                         category, level,
                                                         message, format, args);
            if (!sts && res) sts = res;
        }
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTrace_BeginTask(mfxTraceStaticHandle *static_handle,
                          const char *file_name, mfxTraceU32 line_num,
                          const char *function_name,
                          mfxTraceChar* category, mfxTraceLevel level,
                          const char *task_name, const mfxTraceTaskType task_type,
                          mfxTraceTaskHandle *task_handle,
                          const void *task_params)
{
    // store category and level to check for MFXTrace_IsPrintableCategoryAndLevel in MFXTrace_EndTask
    if (static_handle)
    {
        static_handle->category = category;
        static_handle->level    = level;
    }

    mfxTraceU32 sts = 0, res = 0;
    mfxTraceU32 i = 0;

    for (i = 0; i < sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm); ++i)
    {
        if (g_OutputMode & g_TraceAlgorithms[i].m_OutputInitilized)
        {
            if (!MFXTrace_IsPrintableCategoryAndLevel(g_TraceAlgorithms[i].m_OutputInitilized, level)) continue;

            res = g_TraceAlgorithms[i].m_BeginTaskFn(static_handle,
                                                     file_name, line_num,
                                                     function_name,
                                                     category, level,
                                                     task_name, task_type,
                                                     task_handle, task_params);
            if (!sts && res) sts = res;
        }
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTrace_EndTask(mfxTraceStaticHandle *static_handle,
                        mfxTraceTaskHandle *task_handle)
{
    mfxTraceChar* category = NULL;
    mfxTraceLevel level = MFX_TRACE_LEVEL_DEFAULT;

    // use category and level stored in MFXTrace_BeginTask values to check for MFXTrace_IsPrintableCategoryAndLevel
    // preserve previous behaviour if static_handle is null that should never happend in normal usage model
    if (static_handle)
    {
        category = static_handle->category;
        level    = static_handle->level;
    }
    else
    {
        return 1;
    }

    mfxTraceU32 sts = 0, res = 0;
    mfxTraceU32 i = 0;

    for (i = 0; i < sizeof(g_TraceAlgorithms)/sizeof(mfxTraceAlgorithm); ++i)
    {
        if (g_OutputMode & g_TraceAlgorithms[i].m_OutputInitilized)
        {
            if (!MFXTrace_IsPrintableCategoryAndLevel(g_TraceAlgorithms[i].m_OutputInitilized, level)) continue;

            res = (g_TraceAlgorithms[i].m_EndTaskFn) ? g_TraceAlgorithms[i].m_EndTaskFn(static_handle, task_handle) : 0;
            if (!sts && res) sts = res;
        }
    }
    return sts;
}

/*------------------------------------------------------------------------------*/
// C++ class MFXTraceTask

static unsigned int CreateUniqTaskId()
{
    static unsigned int g_tasksId = 0;
    unsigned int add_val = 1;
    asm volatile ("lock; xaddl %0,%1"
                  : "=r" (add_val), "=m" (g_tasksId)
                  : "0" (add_val), "m" (g_tasksId)
                  : "memory", "cc");
    return add_val; // incremented result will be stored in add_val
}

MFXTraceTask::MFXTraceTask(mfxTraceStaticHandle *static_handle,
                 const char *file_name, mfxTraceU32 line_num,
                 const char *function_name,
                 mfxTraceChar* category, mfxTraceLevel level,
                 const char *task_name,
                 const mfxTraceTaskType task_type,
                 const bool bCreateID)
{
    mfxTraceU32 sts;
    m_pStaticHandle = static_handle;
    memset(&m_TraceTaskHandle, 0, sizeof(m_TraceTaskHandle));
    m_TaskID = (bCreateID) ? CreateUniqTaskId() : 0;
    sts = MFXTrace_BeginTask(static_handle,
                       file_name, line_num,
                       function_name,
                       category, level,
                       task_name, task_type,
                       &m_TraceTaskHandle,
                       (bCreateID) ? &m_TaskID : 0);
    m_bStarted = (sts == 0);
}

mfxTraceU32 MFXTraceTask::GetID()
{
    return m_TaskID;
}

void MFXTraceTask::Stop()
{
    if (m_bStarted)
    {
        MFXTrace_EndTask(m_pStaticHandle, &m_TraceTaskHandle);
        m_bStarted = false;
    }
}

MFXTraceTask::~MFXTraceTask()
{
    Stop();
}

#endif //#ifdef MFX_TRACE_ENABLE
