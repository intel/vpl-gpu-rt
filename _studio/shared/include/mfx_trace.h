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

#ifndef __MFX_TRACE_H__
#define __MFX_TRACE_H__

#include <stdint.h>
#ifdef __cplusplus
#include <functional>
#include <tuple>
#include <utility>
#include <memory>
#endif

#include <stdarg.h>

#include "mfx_config.h"
#include "mfx_trace_dump.h"
#include "mfx_error.h"

    #define MAX_PATH 260

    #define __INT64   long long
    #define __UINT64  unsigned long long

    typedef char mfxTraceChar;

    #define MFX_TRACE_STRING(x) x

    #define DISABLE_WARN_HIDE_PREV_LOCAL_DECLARATION \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wshadow\"")
    #define ROLLBACK_WARN_HIDE_PREV_LOCAL_DECLARATION \
        _Pragma("GCC diagnostic pop")

#define VPLLOG_BUFFER_SIZE 256

typedef unsigned int mfxTraceU32;
typedef __UINT64 mfxTraceU64;

/*------------------------------------------------------------------------------*/
extern mfxTraceU64 EventCfg;
extern mfxTraceU32 LogConfig;
extern int32_t FrameIndex;
extern char VplLogPath[VPLLOG_BUFFER_SIZE];
// C section

#ifdef __cplusplus
extern "C"
{
#endif

// this is for RT events
enum mfxTraceTaskType
{
    MFX_TRACE_DEFAULT_TASK = 0,
    MFX_TRACE_API_DECODE_QUERY_TASK,
    MFX_TRACE_API_DECODE_QUERY_IOSURF_TASK,
    MFX_TRACE_API_DECODE_HEADER_TASK,
    MFX_TRACE_API_DECODE_INIT_TASK,
    MFX_TRACE_API_DECODE_CLOSE_TASK,
    MFX_TRACE_API_DECODE_FRAME_ASYNC_TASK,
    MFX_TRACE_API_ENCODE_QUERY_TASK,
    MFX_TRACE_API_ENCODE_QUERY_IOSURF_TASK,
    MFX_TRACE_API_ENCODE_INIT_TASK,
    MFX_TRACE_API_ENCODE_CLOSE_TASK,
    MFX_TRACE_API_ENCODE_FRAME_ASYNC_TASK,
    MFX_TRACE_API_VPP_QUERY_TASK,
    MFX_TRACE_API_VPP_QUERY_IOSURF_TASK,
    MFX_TRACE_API_VPP_INIT_TASK,
    MFX_TRACE_API_VPP_CLOSE_TASK,
    MFX_TRACE_API_VPP_LEGACY_ROUTINE_TASK,
    MFX_TRACE_API_VPP_RUN_FRAME_VPP_ASYNC_TASK,
    MFX_TRACE_API_VPP_RUN_FRAME_VPP_ASYNC_EX_TASK,
    MFX_TRACE_API_MFX_INIT_EX_TASK,
    MFX_TRACE_API_MFX_CLOSE_TASK,
    MFX_TRACE_API_DO_WORK_TASK,
    MFX_TRACE_API_SYNC_OPERATION_TASK,
    MFX_TRACE_HOTSPOT_SCHED_WAIT_GLOBAL_EVENT_TASK,
    MFX_TRACE_HOTSPOT_DDI_SUBMIT_TASK,
    MFX_TRACE_HOTSPOT_DDI_EXECUTE_TASK,
    MFX_TRACE_HOTSPOT_DDI_ENDFRAME_TASK,
    MFX_TRACE_HOTSPOT_DDI_WAIT_TASK_SYNC,
    MFX_TRACE_HOTSPOT_DDI_QUERY_TASK,
    MFX_TRACE_HOTSPOT_CM_COPY,
    MFX_TRACE_HOTSPOT_COPY_DX11_TO_DX9,
    MFX_TRACE_HOTSPOT_COPY_DX9_TO_DX11,
    MFX_TRACE_HOTSPOT_SCHED_ROUTINE,
    MFX_TRACE_API_MFXINITIALIZE_TASK,
    MFX_TRACE_API_MFXQUERYIMPLSDESCRIPTION_TASK,
    MFX_TRACE_API_HEVC_OUTPUTINFO_TASK,
    MFX_TRACE_API_AVC_OUTPUTINFO_TASK,
    MFX_TRACE_API_AV1_OUTPUTINFO_TASK,
    MFX_TRACE_API_VP9_OUTPUTINFO_TASK,
    MFX_TRACE_API_HEVC_SYNCINFO_TASK,
    MFX_TRACE_API_AVC_SYNCINFO_TASK,
    MFX_TRACE_API_AV1_SYNCINFO_TASK,
    MFX_TRACE_API_VP9_SYNCINFO_TASK,
    MFX_TRACE_API_HEVC_DISPLAYINFO_TASK,
    MFX_TRACE_API_AVC_DISPLAYINFO_TASK,
    MFX_TRACE_API_AV1_DISPLAYINFO_TASK,
    MFX_TRACE_API_VP9_DISPLAYINFO_TASK,
    MFX_TRACE_API_HEVC_PICTUREPARAMETER_TASK,   
    MFX_TRACE_API_HEVC_SLICEPARAMETER_TASK,
    MFX_TRACE_API_HEVC_QMATRIXARAMETER_TASK,
    MFX_TRACE_API_AVC_PICTUREPARAMETER_TASK,
    MFX_TRACE_API_AVC_SLICEPARAMETER_TASK,
    MFX_TRACE_API_AVC_QMATRIXARAMETER_TASK,
    MFX_TRACE_API_AV1_PICTUREPARAMETER_TASK,
    MFX_TRACE_API_AV1_TILECONTROLPARAMETER_TASK,      
    MFX_TRACE_API_VP9_PICTUREPARAMETER_TASK,
    MFX_TRACE_API_VP9_SEGMENTPARAMETER_TASK,   
    MFX_TRACE_API_BITSTREAM_TASK,
    MFX_TRACE_API_HEVC_DPBPARAMETER_TASK,
    MFX_TRACE_API_AVC_DPBPARAMETER_TASK,
    MFX_TRACE_API_AV1_DPBPARAMETER_TASK,
    MFX_TRACE_API_VP9_DPBPARAMETER_TASK,
    VA_TRACE_API_AV1_PICTUREPARAMETER_TASK,
    VA_TRACE_API_AV1_TILECONTROLPARAMETER_TASK,
    VA_TRACE_API_AVC_PICTUREPARAMETER_TASK,
    VA_TRACE_API_AVC_SLICEPARAMETER_TASK,
    VA_TRACE_API_AVC_QMATRIXARAMETER_TASK,
    VA_TRACE_API_HEVC_PICTUREPARAMETER_TASK,
    VA_TRACE_API_HEVC_SLICEPARAMETER_TASK,
    VA_TRACE_API_HEVC_QMATRIXARAMETER_TASK,
    VA_TRACE_API_VP9_PICTUREPARAMETER_TASK,
    VA_TRACE_API_VP9_SLICEPARAMETER_TASK,
    VA_TRACE_API_HEVC_DPBPARAMETER_TASK,
    MFX_TRACE_PIPELINE_STICKER_TASK,
    VPLMessage_TASK,
    MFX_TRACE_API_DECODE_RESET_TASK,
    MFX_TRACE_API_DECODE_GETVIDEOPARAM_TASK,
    MFX_TRACE_API_DECODE_GETSTAT_TASK,
    MFX_TRACE_API_DECODE_SETSKIPMODE_TASK,
    MFX_TRACE_API_DECODE_GETPAYLOAD_TASK,
    MFX_TRACE_API_GETSURFACE_TASK,
    MFX_TRACE_API_DECODE_VPP_INIT_TASK,
    MFX_TRACE_API_DECODE_VPP_CLOSE_TASK,
    MFX_TRACE_API_DECODE_VPP_RESET_TASK,
    MFX_TRACE_API_DECODE_VPP_FRAMEASYNC_TASK,
    MFX_TRACE_API_DOFASTCOPY_WRAPPER_TASK,
    MFX_TRACE_API_ADD_TASK,
    MFX_TRACE_API_GET_TASK,
    MFX_TRACE_API_QUERY_TASK,
    MFX_TRACE_API_MARK_TASK,
};

// list of output modes
enum
{
    MFX_TRACE_OUTPUT_TRASH  = 0x00,
    MFX_TRACE_OUTPUT_TEXTLOG = 0x01,
    MFX_TRACE_OUTPUT_STAT   = 0x02,
    MFX_TRACE_OUTPUT_ETW    = 0x04,
    MFX_TRACE_OUTPUT_TAL    = 0x08,

    MFX_TRACE_OUTPUT_ITT    = 0x10,
    MFX_TRACE_OUTPUT_FTRACE = 0x20,
    // special keys
    MFX_TRACE_OUTPUT_ALL     = 0xFFFFFFFF,
    MFX_TRACE_OUTPUT_REG     = MFX_TRACE_OUTPUT_ALL // output mode should be read from registry
};

// enumeration of the trace levels inside any category
typedef enum
{
    MFX_TRACE_LEVEL_0 = 0,
    MFX_TRACE_LEVEL_1 = 1,
    MFX_TRACE_LEVEL_2 = 2,
    MFX_TRACE_LEVEL_3 = 3,
    MFX_TRACE_LEVEL_4 = 4,
    MFX_TRACE_LEVEL_5 = 5,
    MFX_TRACE_LEVEL_6 = 6,
    MFX_TRACE_LEVEL_7 = 7,
    MFX_TRACE_LEVEL_8 = 8,
    MFX_TRACE_LEVEL_9 = 9,
    MFX_TRACE_LEVEL_10 = 10,
    MFX_TRACE_LEVEL_11 = 11,
    MFX_TRACE_LEVEL_12 = 12,
    MFX_TRACE_LEVEL_13 = 13,
    MFX_TRACE_LEVEL_14 = 14,
    MFX_TRACE_LEVEL_15 = 15,
    MFX_TRACE_LEVEL_16 = 16,

    MFX_TRACE_LEVEL_MAX = 0xFF
} mfxTraceLevel;

// enumeration of the TXT log levels
typedef enum
{
#ifndef NDEBUG
    MFX_TXTLOG_LEVEL_MAX = 1,   //include API Func, API PARAMS and internal Func
    MFX_TXTLOG_LEVEL_API_AND_INTERNAL = 2,  //include API Func and internal Func
    MFX_TXTLOG_LEVEL_API_AND_PARAMS = 3, //include API Func and API PARAMS
    MFX_TXTLOG_LEVEL_API = 4,   //include API Func
#else
    MFX_TXTLOG_LEVEL_API_AND_PARAMS = 1,    //include API Func, API PARAMS
    MFX_TXTLOG_LEVEL_API = 2,    //include API Func
    MFX_TXTLOG_LEVEL_MAX = 3   //include API Func, API PARAMS and internal Func
#endif
} mfxTxtLogLevel;

typedef enum _MEDIA_EVENT_TYPE
{
    EVENT_TYPE_INFO = 0,           //! function information event
    EVENT_TYPE_START = 1,           //! function entry event
    EVENT_TYPE_END = 2,           //! function exit event
    EVENT_TYPE_INFO2 = 3,           //! function extra information event
} MEDIA_EVENT_TYPE;

//!
//! \brief Keyword for ETW tracing, 1bit per keyworld, total 64bits
//!
typedef enum _MEDIA_EVENT_FILTER_KEYID
{
    //Common key
    TR_KEY_MFX_API = 0,
    TR_KEY_DDI_API,
    TR_KEY_INTERNAl,
    //Decode key
    TR_KEY_DECODE_PICPARAM,
    TR_KEY_DECODE_SLICEPARAM,
    TR_KEY_DECODE_TILEPARAM,
    TR_KEY_DECODE_QMATRIX,
    TR_KEY_DECODE_SEGMENT,
    TR_KEY_DECODE_BITSTREAM_INFO,
    TR_KEY_DECODE_DPB_INFO,
    TR_KEY_DECODE_BASIC_INFO,
    TR_KEY_PIPELINE_STICKER,
} MEDIA_EVENT_FILTER_KEYID;

#define MFX_ETWLOG_ENABLE 16

// delete the following levels completely
#define MFX_TRACE_LEVEL_SCHED       MFX_TRACE_LEVEL_10
#define MFX_TRACE_LEVEL_PRIVATE     MFX_TRACE_LEVEL_16

// the following levels should remain only

/** API level
 * - Media SDK library entry points exposed in Media SDK official API
 * - Media SDK library important internal entry points
 */
#define MFX_TRACE_LEVEL_API         MFX_TRACE_LEVEL_1

/** HOTSPOTS level
 * - Known Media SDK library internal hotspots (functions taking > ~50us on selected platforms/contents)
 */
#define MFX_TRACE_LEVEL_HOTSPOTS    MFX_TRACE_LEVEL_2

/** EXTCALL level
 * - Calls to external libaries (DXVA, LibVA, MDF/CM, etc.)
 */
#define MFX_TRACE_LEVEL_EXTCALL     MFX_TRACE_LEVEL_2 // should be MFX_TRACE_LEVEL_3

/** SCHEDULER level
 * - Media SDK internal scheduler functions calls
 */
#define MFX_TRACE_LEVEL_API_PARAMS   MFX_TRACE_LEVEL_4

/** INTERNAL level
 * - Media SDK components function calls. Use this level to get more deeper knowledge of
 * calling stack.
 */
#define MFX_TRACE_LEVEL_INTERNAL    MFX_TRACE_LEVEL_5

/** PARAMS level
 * - Tracing of function parameters, variables, etc. Note that not all tracing modules support this.
 */
#define MFX_TRACE_LEVEL_PARAMS      MFX_TRACE_LEVEL_6

 /** WARNING level
  * - Tracing of warning info
  */
#define MFX_TRACE_LEVEL_WARNING_INFO MFX_TRACE_LEVEL_7

 /** CRITICAL level
  * - Tracing of critical info
  */
#define MFX_TRACE_LEVEL_CRITICAL_INFO MFX_TRACE_LEVEL_8

// defines default trace category
#define MFX_TRACE_CATEGORY_DEFAULT  NULL

// defines category for the current module
#ifndef MFX_TRACE_CATEGORY
    #define MFX_TRACE_CATEGORY      MFX_TRACE_CATEGORY_DEFAULT
#endif

// defines default trace level
#define MFX_TRACE_LEVEL_DEFAULT     MFX_TRACE_LEVEL_MAX

// defines default trace level for the current module
#ifndef MFX_TRACE_LEVEL
    #define MFX_TRACE_LEVEL         MFX_TRACE_LEVEL_DEFAULT
#endif

// Perf traces
mfxTraceU32 MFXTrace_EventInit();
mfxTraceU32 MFXTrace_EventClose();

/*------------------------------------------------------------------------------*/

#ifdef MFX_TRACE_ENABLE

typedef union
{
    unsigned int  uint32;
    __UINT64      uint64;
    __INT64       tick;
    char*         str;
    void*         ptr;
    mfxTraceChar* category;
    mfxTraceLevel level;
} mfxTraceHandle;

typedef struct
{
    uint64_t callCount;
    double   totalTime;
}TimeStampInfo;

typedef struct
{
    mfxTraceChar* category;
    mfxTraceLevel level;
    // reserved for stat dump:
    mfxTraceHandle sd1;
    mfxTraceHandle sd2;
    mfxTraceHandle sd3;
    mfxTraceHandle sd4;
    mfxTraceHandle sd5;
    mfxTraceHandle sd6;
    mfxTraceHandle sd7;
    // reserved for itt
    mfxTraceHandle itt1;
    //timeStamp
    TimeStampInfo  tick;
} mfxTraceStaticHandle;

typedef struct
{
    // reserved for file dump:
    mfxTraceHandle fd1;
    mfxTraceHandle fd2;
    mfxTraceHandle fd3;
    mfxTraceHandle fd4;
    // reserved for stat dump:
    mfxTraceHandle sd1;
    // reserved for TAL:
    mfxTraceHandle tal1;
    // reserved for ETW:
    mfxTraceHandle etw1;
    mfxTraceHandle etw2;
    mfxTraceHandle etw3;
    // reserved for itt
    mfxTraceHandle itt1;
} mfxTraceTaskHandle;

/*------------------------------------------------------------------------------*/

// basic trace functions (macroses are recommended to use instead)

mfxTraceU32 MFXTrace_Init();

mfxTraceU32 MFXTrace_Close(void);

mfxTraceU32 MFXTrace_SetLevel(mfxTraceChar* category,
                              mfxTraceLevel level);

mfxTraceU32 MFXTrace_DebugMessage(mfxTraceStaticHandle *static_handle,
                             const char *file_name, mfxTraceU32 line_num,
                             const char *function_name,
                             mfxTraceChar* category, mfxTraceLevel level,
                             const char *message,
                             const char *format, ...);

mfxTraceU32 MFXTrace_vDebugMessage(mfxTraceStaticHandle *static_handle,
                              const char *file_name, mfxTraceU32 line_num,
                              const char *function_name,
                              mfxTraceChar* category, mfxTraceLevel level,
                              const char *message,
                              const char *format, va_list args);

mfxTraceU32 MFXTrace_BeginTask(mfxTraceStaticHandle *static_handle,
                          const char *file_name, mfxTraceU32 line_num,
                          const char *function_name,
                          mfxTraceChar* category, mfxTraceLevel level,
                          const char *task_name, const mfxTraceTaskType task_type,
                          mfxTraceTaskHandle *task_handle,
                          const void *task_params);

mfxTraceU32 MFXTrace_EndTask(mfxTraceStaticHandle *static_handle,
                             mfxTraceTaskHandle *task_handle);

/*------------------------------------------------------------------------------*/

// Perf traces
mfxTraceU32 MFXTraceEvent(uint16_t task, uint8_t opcode, uint8_t level, uint64_t size, const void *ptr);

#ifdef __cplusplus
}
#endif

#pragma pack(push, 2)

// struct event_data contains arguments we need to trace in perf events
// It contains variadic amount of arguments
template <typename ...>
struct event_data;

template <>
struct event_data<>
{
    event_data() = default;
};

template <typename T, typename ...Rest>
struct event_data<T, Rest...>
    : event_data<Rest...>
{
    typename std::decay<T>::type value;

    template <typename A, typename ...Args>
    event_data(A&& a, Args&&... args)
        : event_data<Rest...>(std::forward<Args>(args)...)
        , value(std::forward<A>(a))
    {}
};


#pragma pack(pop)

// Utility function for maintaining correct arguments order in event_data<...>()
template <typename Tuple, std::size_t... I>
auto make_event_data(Tuple&& t, std::index_sequence<I...>)
{
    return event_data<
        typename std::tuple_element<sizeof...(I) - I - 1, Tuple>::type...
    >(std::get<sizeof...(I) - I - 1>(std::forward<Tuple>(t))...);
}

// Helper function for creating event_data<...>() object
template <typename ...Args>
auto make_event_data(Args&&... args)
{
    return make_event_data(
        std::forward_as_tuple(std::forward<Args>(args)...),
        std::make_index_sequence<sizeof...(args)>{}
    );
}

// PerfScopedTrace objects are created using PERF_EVENT macro
class PerfScopedTrace
{
public:
    template <typename ...Args>
    PerfScopedTrace(uint16_t task, uint8_t ucType, uint8_t level, event_data<Args...> data)
    {
        // Send event data on PerfScopedTrace creation
        write(task, ucType, level, data);
    }

    ~PerfScopedTrace()
    {
    }

    template <typename T>
    static void write(uint16_t task, uint8_t  opcode, uint8_t level, T&& data)
    {
        MFXTraceEvent(task, opcode, level, sizeof(data), &data);
    }

    static void write(uint16_t task, uint8_t  opcode, uint8_t level, event_data<>& /*data*/)
    {
        MFXTraceEvent(task, opcode, level, 0, nullptr);
    }

    template <typename T>
    static void thunk(void* f, uint16_t task, uint8_t level)
    {
        std::unique_ptr<T> p { static_cast<T*>(f) };

        auto e = (*p)();

        // Send event data at function exit
        write(task, 2, level, e);
    }

    template <typename T>
    static constexpr auto get_thunk() noexcept {
        return &thunk<T>;
    }
};

#define TRACE_CHECK(keyWord)   \
    (EventCfg & (1 << keyWord)) || keyWord == TR_KEY_MFX_API || keyWord == TR_KEY_DDI_API || keyWord == TR_KEY_INTERNAl

// This macro is recommended to use instead creating RT Info ScopedTrace object directly
#define TRACE_BUFFER_EVENT(task, level, keyWord, pData, funcName, structType)   \
    if (TRACE_CHECK(keyWord))                           \
    {                                                   \
        EVENTDATA_##structType eventData;               \
        Event##funcName(&eventData, pData);             \
        PerfScopedTrace _info_scoped_trace##__LINE__(task, level, 0, make_event_data(eventData));   \
    }

#define TRACE_EVENT(task, level, keyWord, at_exit_func)         \
    if (TRACE_CHECK(keyWord))                                   \
    {                                                           \
        PerfScopedTrace _info_scoped_trace##__LINE__(task, level, 0, at_exit_func);    \
    }

#ifdef __cplusplus
extern "C" {
#endif

/*------------------------------------------------------------------------------*/

// basic macroses

#define MFX_TRACE_PARAMS \
    &_trace_static_handle, __FILE__, __LINE__, __FUNCTION__, MFX_TRACE_CATEGORY


#define MFX_TRACE_INIT() \
    MFXTrace_Init();

#define MFX_TRACE_INIT_RES(_res) \
    _res = MFXTrace_Init();

#define MFX_TRACE_CLOSE() \
    MFXTrace_Close();

#define MFX_TRACE_CLOSE_RES(_res) \
    _res = MFXTrace_Close();

#define MFX_LTRACE(_trace_all_params)                       \
{                                                           \
    DISABLE_WARN_HIDE_PREV_LOCAL_DECLARATION                \
    static mfxTraceStaticHandle _trace_static_handle = {};  \
    MFXTrace_DebugMessage _trace_all_params;                \
    ROLLBACK_WARN_HIDE_PREV_LOCAL_DECLARATION               \
}
#else
#define MFX_TRACE_INIT()
#define MFX_TRACE_INIT_RES(res)
#define MFX_TRACE_CLOSE()
#define MFX_TRACE_CLOSE_RES(res)
#define MFX_LTRACE(_trace_all_params)
#define PERF_EVENT(task, level, ...)
#define TRACE_EVENT(task, level, ...)
#endif

/*------------------------------------------------------------------------------*/
// standard formats

#define MFX_TRACE_FORMAT_S    "%s"
#define MFX_TRACE_FORMAT_WS   "%S"
#define MFX_TRACE_FORMAT_P    "%p"
#define MFX_TRACE_FORMAT_I    "%d"
#define MFX_TRACE_FORMAT_X    "%x"
#define MFX_TRACE_FORMAT_D    "%d (0x%x)"
#define MFX_TRACE_FORMAT_F    "%g"
#define MFX_TRACE_FORMAT_GUID "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}"

/*------------------------------------------------------------------------------*/
// these macroses permit to set trace level

#define MFX_LTRACE_1(_level, _message, _format, _arg1) \
    MFX_LTRACE((MFX_TRACE_PARAMS, _level, _message, _format, _arg1))

#define MFX_LTRACE_2(_level, _message, _format, _arg1, _arg2) \
    MFX_LTRACE((MFX_TRACE_PARAMS, _level, _message, _format, _arg1, _arg2))

#define MFX_LTRACE_3(_level, _message, _format, _arg1, _arg2, _arg3) \
    MFX_LTRACE((MFX_TRACE_PARAMS, _level, _message, _format, _arg1, _arg2, _arg3))

#define MFX_LTRACE_MSG(_level, _message) \
    MFX_LTRACE_1(_level, _message, NULL, 0)

#define MFX_LTRACE_MSG_1(_level, ...) \
    char str[256]; \
    sprintf(str, __VA_ARGS__); \
    MFX_LTRACE_MSG(_level, str); \

#define MFX_LTRACE_S(_level, _string) \
    MFX_LTRACE_1(_level, #_string " = ", MFX_TRACE_FORMAT_S, _string)

#define MFX_LTRACE_WS(_level, _string) \
    MFX_LTRACE_1(_level, #_string " = ", MFX_TRACE_FORMAT_WS, _string)

#define MFX_LTRACE_P(_level, _arg1) \
    MFX_LTRACE_1(_level, #_arg1 " = ", MFX_TRACE_FORMAT_P, _arg1)

#define MFX_LTRACE_X(_level, _arg1) \
    MFX_LTRACE_1(_level, #_arg1 " = ", MFX_TRACE_FORMAT_X, _arg1)

#define MFX_LTRACE_D(_level, _arg1) \
    MFX_LTRACE_2(_level, #_arg1 " = ", MFX_TRACE_FORMAT_D, _arg1, _arg1)

#define MFX_LTRACE_F(_level, _arg1) \
    MFX_LTRACE_1(_level, #_arg1 " = ", MFX_TRACE_FORMAT_F, _arg1)

#ifdef MFX_TRACE_ENABLE
#define MFX_LTRACE_BUFFER_S(_level, _name, _buffer, _size)  \
    if (_buffer)                                            \
    MFX_LTRACE_2(_level, _name, "%p[%lu]", _buffer, _size)
#else
#define MFX_LTRACE_BUFFER_S(_level, _name, _buffer, _size)
#endif

#ifdef MFX_TRACE_ENABLE
#define MFX_LTRACE_BUFFER(_level, _message, _buffer)                    \
{                                                                       \
    if (0 != LogConfig && _buffer)                                      \
    {                                                                   \
        DumpContext context;                                            \
        std::string _str;                                               \
        try                                                             \
        {                                                               \
            _str = context.dump(#_buffer, *_buffer);                    \
        }                                                               \
        catch(...)                                                      \
        {                                                               \
            return MFX_ERR_UNKNOWN;                                     \
        }                                                               \
        MFX_LTRACE_1(_level, "\n" _message, "%s", _str.c_str())         \
    }                                                                   \
else {                                                                  \
        MFX_LTRACE_BUFFER_S(_level, #_buffer, _buffer, sizeof(*_buffer))\
    }                                                                   \
}
#else
#define MFX_LTRACE_BUFFER(_level, _buffer) \
    MFX_LTRACE_BUFFER_S(_level, #_buffer, _buffer, sizeof(*_buffer)) 
#endif

#define MFX_LTRACE_GUID(_level, _guid) \
    MFX_LTRACE((MFX_TRACE_PARAMS, _level, #_guid " = ", \
               MFX_TRACE_FORMAT_GUID, \
               (_guid).Data1, (_guid).Data2, (_guid).Data3, \
               (_guid).Data4[0], (_guid).Data4[1], (_guid).Data4[2], (_guid).Data4[3], \
               (_guid).Data4[4], (_guid).Data4[5], (_guid).Data4[6], (_guid).Data4[7]))

/*------------------------------------------------------------------------------*/
// these macroses uses default trace level

#define MFX_TRACE_1(_message, _format, _arg1) \
    MFX_LTRACE_1(MFX_TRACE_LEVEL, _message, _format, _arg1)

#define MFX_TRACE_2(_message, _format, _arg1, _arg2) \
    MFX_LTRACE_2(MFX_TRACE_LEVEL, _message, _format, _arg1, _arg2)

#define MFX_TRACE_3(_message, _format, _arg1, _arg2, _arg3) \
    MFX_LTRACE_3(MFX_TRACE_LEVEL, _message, _format, _arg1, _arg2, _arg3)

#define MFX_TRACE_S(_arg1) \
    MFX_LTRACE_S(MFX_TRACE_LEVEL, _arg1)

#define MFX_TRACE_WS(_message) \
    MFX_LTRACE_WS(MFX_TRACE_LEVEL, _message)

#define MFX_TRACE_P(_arg1) \
    MFX_LTRACE_P(MFX_TRACE_LEVEL, _arg1)

#define MFX_TRACE_I(_arg1) \
    MFX_LTRACE_I(MFX_TRACE_LEVEL, _arg1)

#define MFX_TRACE_X(_arg1) \
    MFX_LTRACE_X(MFX_TRACE_LEVEL, _arg1)

#define MFX_TRACE_D(_arg1) \
    MFX_LTRACE_D(MFX_TRACE_LEVEL, _arg1)

#define MFX_TRACE_F(_arg1) \
    MFX_LTRACE_F(MFX_TRACE_LEVEL, _arg1)

#define MFX_TRACE_GUID(_guid) \
    MFX_LTRACE_GUID(MFX_TRACE_LEVEL, _guid)

#define MFX_TRACE_BUFFER(_name, _buffer, _size) \
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL, _name, _buffer, _size)

/*------------------------------------------------------------------------------*/

#ifdef __cplusplus
} // extern "C"
#endif

/*------------------------------------------------------------------------------*/
// C++ section

#ifdef __cplusplus


#include "mfx_reflect.h"

mfx_reflect::AccessibleTypesCollection GetReflection();


#ifdef MFX_TRACE_ENABLE
// C++ class for BeginTask/EndTask
class MFXTraceTask
{
public:
    MFXTraceTask(mfxTraceStaticHandle *static_handle,
                 const char *file_name, mfxTraceU32 line_num,
                 const char *function_name,
                 mfxTraceChar* category, mfxTraceLevel level,
                 const char *task_name,
                 const mfxTraceTaskType task_type,
                 const bool bCreateID = false);
    mfxTraceU32 GetID();
    void        Stop();
    ~MFXTraceTask();

private:
    bool                    m_bStarted;
    mfxTraceU32             m_TaskID;
    mfxTraceStaticHandle    *m_pStaticHandle;
    mfxTraceTaskHandle      m_TraceTaskHandle;
};
#endif // #ifdef MFX_TRACE_ENABLE

/*------------------------------------------------------------------------------*/
// auto tracing of the functions

#ifdef MFX_TRACE_ENABLE
    #define _MFX_AUTO_LTRACE_(_level, _task_name, _task_type, _bCreateID)       \
        DISABLE_WARN_HIDE_PREV_LOCAL_DECLARATION                    \
        static mfxTraceStaticHandle _trace_static_handle;           \
        MFXTraceTask                _mfx_trace_task(MFX_TRACE_PARAMS, _level, _task_name, _task_type, _bCreateID); \
        ROLLBACK_WARN_HIDE_PREV_LOCAL_DECLARATION

    #define MFX_AUTO_TRACE_STOP()   _mfx_trace_task.Stop()
    #define MFX_AUTO_TRACE_GETID()  _mfx_trace_task.GetID()
#else
    #define _MFX_AUTO_LTRACE_(_level, _task_name, _task_type, _bCreateID)
    #define MFX_AUTO_TRACE_STOP()
    #define MFX_AUTO_TRACE_GETID()  0
#endif

#define MFX_AUTO_LTRACE(_level, _task_name)     \
    _MFX_AUTO_LTRACE_(_level, _task_name, MFX_TRACE_DEFAULT_TASK, false)

#define MFX_AUTO_TRACE(_task_name) \
    _MFX_AUTO_LTRACE_(MFX_TRACE_LEVEL, _task_name, MFX_TRACE_DEFAULT_TASK, false)

#define MFX_AUTO_LTRACE_FUNC(_level) \
    _MFX_AUTO_LTRACE_(_level, __FUNCTION__, MFX_TRACE_DEFAULT_TASK, false)

#define MFX_AUTO_LTRACE_WITHID(_level, _task_name)     \
    _MFX_AUTO_LTRACE_(_level, _task_name, MFX_TRACE_DEFAULT_TASK, true)

#define MFX_AUTO_TRACE_WITHID(_task_name) \
    _MFX_AUTO_LTRACE_(MFX_TRACE_LEVEL, _task_name, MFX_TRACE_DEFAULT_TASK, true)

#ifdef MFX_TRACE_ENABLE
class MFXLTraceI 
{
public:
    template <typename T>
    void mfx_ltrace_i(mfxTraceLevel _level, const char* _mesg, const char* _function, const char* _filename, mfxTraceU32 _line, T _arg1)
    {
        std::stringstream ss;
        if (_arg1 > 0)
        {
            ss << "[Warning]  ";
        }
        else if (_arg1 < 0)
        {
            ss << "[Critical]  ";
        }
        ss << _mesg << " = ";
        MFX_LTRACE((&_trace_static_handle, _filename, _line, _function, MFX_TRACE_CATEGORY, _level, ss.str().c_str(), MFX_TRACE_FORMAT_I, _arg1))
    }

    void mfx_ltrace_i(mfxTraceLevel _level, const char* _mesg, const char* _function, const char* _filename, mfxTraceU32 _line, mfxStatus _arg1)
    {
        std::error_code code = mfx::make_error_code(_arg1);
        std::stringstream ss;
        if (_arg1 > 0) 
        {
            ss << "[Warning]  ";
        }
        else if (_arg1 < 0) 
        {
            ss << "[Critical]  ";
        }
        ss << _mesg << " = ";
        MFX_LTRACE((&_trace_static_handle, _filename, _line, _function, MFX_TRACE_CATEGORY, _level, ss.str().c_str(), MFX_TRACE_FORMAT_S, code.message().c_str()))
    }
};

#define MFX_LTRACE_I(_level, _arg1)                                                         \
{                                                                                           \
    MFXLTraceI mFXLTraceI;                                                                  \
    try                                                                                     \
    {                                                                                       \
        mFXLTraceI.mfx_ltrace_i(_level, #_arg1, __FUNCTION__, __FILE__, __LINE__,_arg1);    \
    }                                                                                       \
    catch(...)                                                                              \
    {                                                                                       \
        std::cerr << "Trace failed!" << '\n';                                               \
    }                                                                                       \
}
#else
#define MFX_LTRACE_I(_level, _arg1) \
    MFX_LTRACE_1(_level, #_arg1 " = ", MFX_TRACE_FORMAT_I, _arg1)
#endif

#endif // ifdef __cplusplus

template <int TYPE>
struct TraceTaskType2TraceLevel;

template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_DEFAULT_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_DECODE_QUERY_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_DECODE_QUERY_IOSURF_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_DECODE_HEADER_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_DECODE_INIT_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_DECODE_CLOSE_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_DECODE_FRAME_ASYNC_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_ENCODE_QUERY_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_ENCODE_QUERY_IOSURF_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_ENCODE_INIT_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_ENCODE_CLOSE_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_ENCODE_FRAME_ASYNC_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_VPP_QUERY_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_VPP_QUERY_IOSURF_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_VPP_INIT_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_VPP_CLOSE_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_VPP_LEGACY_ROUTINE_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_VPP_RUN_FRAME_VPP_ASYNC_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_VPP_RUN_FRAME_VPP_ASYNC_EX_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_MFX_INIT_EX_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_MFX_CLOSE_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_DO_WORK_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_SYNC_OPERATION_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_HOTSPOT_SCHED_WAIT_GLOBAL_EVENT_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_HOTSPOTS> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_HOTSPOT_DDI_SUBMIT_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_HOTSPOTS> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_HOTSPOT_DDI_EXECUTE_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_HOTSPOTS> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_HOTSPOT_DDI_ENDFRAME_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_HOTSPOTS> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_HOTSPOT_DDI_WAIT_TASK_SYNC> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_HOTSPOTS> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_HOTSPOT_DDI_QUERY_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_HOTSPOTS> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_HOTSPOT_CM_COPY> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_HOTSPOTS> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_HOTSPOT_COPY_DX11_TO_DX9> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_HOTSPOTS> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_HOTSPOT_COPY_DX9_TO_DX11> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_HOTSPOTS> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_HOTSPOT_SCHED_ROUTINE> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_HOTSPOTS> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_MFXINITIALIZE_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};
template <>
struct TraceTaskType2TraceLevel<MFX_TRACE_API_MFXQUERYIMPLSDESCRIPTION_TASK> : std::integral_constant<mfxTraceLevel, MFX_TRACE_LEVEL_API> {};

#endif // #ifndef __MFX_TRACE_H__
