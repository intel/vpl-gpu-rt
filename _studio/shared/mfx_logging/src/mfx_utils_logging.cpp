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

mfxLogLevel gMfxLogLevel               = LEVEL_WARN;
std::shared_ptr<std::FILE> gMfxLogFile = {};
std::mutex gMfxLogMutex                = {};

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
    const char* logFileName = std::getenv("VPL_RUNTIME_LOG_FILE");
    if (logFileName)
    {
        gMfxLogFile = std::shared_ptr<std::FILE>(fopen(logFileName, "w"), fileDeleter);
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

