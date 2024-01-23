// Copyright (c) 2021 Intel Corporation
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

#include "mfx_utils_logging.h"

auto fileDeleter = [](FILE* file)->void { if (file) fclose(file); };

bool gMfxLogSkipped                        = false;
mfxLogLevel gMfxLogLevel                   = LEVEL_WARN;
std::shared_ptr<std::FILE> gMfxLogFile     = {};
std::mutex gMfxLogMutex                    = {};
std::shared_ptr<std::FILE> gMfxAPIDumpFile = {};

#define DEFINE_ERR_CODE(code)\
    {code, #code}

static const std::map<mfxStatus, const char*> StringsOfStatus =
{
    DEFINE_ERR_CODE(MFX_ERR_NONE                    ),
    DEFINE_ERR_CODE(MFX_ERR_UNKNOWN                 ),
    DEFINE_ERR_CODE(MFX_ERR_NULL_PTR                ),
    DEFINE_ERR_CODE(MFX_ERR_UNSUPPORTED             ),
    DEFINE_ERR_CODE(MFX_ERR_MEMORY_ALLOC            ),
    DEFINE_ERR_CODE(MFX_ERR_NOT_ENOUGH_BUFFER       ),
    DEFINE_ERR_CODE(MFX_ERR_INVALID_HANDLE          ),
    DEFINE_ERR_CODE(MFX_ERR_LOCK_MEMORY             ),
    DEFINE_ERR_CODE(MFX_ERR_NOT_INITIALIZED         ),
    DEFINE_ERR_CODE(MFX_ERR_NOT_FOUND               ),
    DEFINE_ERR_CODE(MFX_ERR_MORE_DATA               ),
    DEFINE_ERR_CODE(MFX_ERR_MORE_SURFACE            ),
    DEFINE_ERR_CODE(MFX_ERR_ABORTED                 ),
    DEFINE_ERR_CODE(MFX_ERR_DEVICE_LOST             ),
    DEFINE_ERR_CODE(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM),
    DEFINE_ERR_CODE(MFX_ERR_INVALID_VIDEO_PARAM     ),
    DEFINE_ERR_CODE(MFX_ERR_UNDEFINED_BEHAVIOR      ),
    DEFINE_ERR_CODE(MFX_ERR_DEVICE_FAILED           ),
    DEFINE_ERR_CODE(MFX_ERR_MORE_BITSTREAM          ),
    DEFINE_ERR_CODE(MFX_ERR_GPU_HANG                ),
    DEFINE_ERR_CODE(MFX_ERR_REALLOC_SURFACE         ),
    DEFINE_ERR_CODE(MFX_ERR_RESOURCE_MAPPED         ),
    DEFINE_ERR_CODE(MFX_ERR_NOT_IMPLEMENTED         ),

    DEFINE_ERR_CODE(MFX_WRN_IN_EXECUTION            ),
    DEFINE_ERR_CODE(MFX_WRN_DEVICE_BUSY             ),
    DEFINE_ERR_CODE(MFX_WRN_VIDEO_PARAM_CHANGED     ),
    DEFINE_ERR_CODE(MFX_WRN_PARTIAL_ACCELERATION    ),
    DEFINE_ERR_CODE(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM),
    DEFINE_ERR_CODE(MFX_WRN_VALUE_NOT_CHANGED       ),
    DEFINE_ERR_CODE(MFX_WRN_OUT_OF_RANGE            ),
    DEFINE_ERR_CODE(MFX_WRN_FILTER_SKIPPED          ),
    DEFINE_ERR_CODE(MFX_ERR_NONE_PARTIAL_OUTPUT     ),
    DEFINE_ERR_CODE(MFX_WRN_ALLOC_TIMEOUT_EXPIRED   ),
    DEFINE_ERR_CODE(MFX_TASK_WORKING                ),
    DEFINE_ERR_CODE(MFX_TASK_BUSY                   ),
    DEFINE_ERR_CODE(MFX_ERR_MORE_DATA_SUBMIT_TASK   )
};

const std::string GetMFXStatusInString(mfxStatus mfxSts)
{
    auto it = StringsOfStatus.find(mfxSts);
    if (it != StringsOfStatus.end())
    {
        return it->second;
    }
    
    return std::to_string(mfxSts);
}

inline bool SetLogLevelFromEnv()
{
    const char* logLevelChar = std::getenv("VPL_RUNTIME_LOG_LEVEL");
    if (logLevelChar)
    {
        char** endPtr = nullptr;
        auto logLevel = std::strtol(logLevelChar, endPtr, 10);
        if (logLevel != LEVEL_UNKNOWN)
        {
            gMfxLogLevel = static_cast<mfxLogLevel>(logLevel);
            return true;
        }
    }

    return false;
}

inline void SetLogFileFromEnv()
{

    if (gMfxLogLevel == LEVEL_DISABLED)
    {
        return;
    }

    const char* logFileName = std::getenv("VPL_RUNTIME_LOG_FILE");
    if (logFileName && gMfxLogFile == nullptr)
    {
        gMfxLogFile = std::shared_ptr<std::FILE>(fopen(logFileName, "a"), fileDeleter);
    }

    std::stringstream trace_name_stream;
    trace_name_stream << "VPL_API_LOG_THREAD_" << std::this_thread::get_id() << ".txt";
    if (logFileName && gMfxAPIDumpFile == nullptr)
    {
        gMfxAPIDumpFile = std::shared_ptr<std::FILE>(fopen(trace_name_stream.str().c_str(), "a"), fileDeleter);
    }
}

void InitMfxLogging()
{
#if defined(MFX_ENABLE_LOG_UTILITY)
    std::unique_lock<std::mutex> closeGuard(gMfxLogMutex);

    SetLogLevelFromEnv();
    SetLogFileFromEnv();
#endif
}


std::unique_ptr<MfxLogSkip> GetMfxLogSkip()
{
#if defined(MFX_ENABLE_LOG_UTILITY)
    return std::make_unique<MfxLogSkip>();
#else
    return nullptr;
#endif
}
