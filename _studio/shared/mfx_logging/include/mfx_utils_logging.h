// Copyright (c) 2021-2022 Intel Corporation
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

#pragma once

#include "mfx_ext_buffers.h"


#include <iostream>
#include <thread>
#include <memory>
#include <mutex>
#include <string>
#include <map>
#include <sstream>

typedef enum
{
    LEVEL_UNKNOWN  = 0,
    LEVEL_DISABLED = 1,
    LEVEL_TRACE    = 2,
    LEVEL_DEBUG    = 3,
    LEVEL_INFO     = 4,
    LEVEL_WARN     = 5,
    LEVEL_ERROR    = 6,
    LEVEL_FATAL    = 7,
} mfxLogLevel;

extern bool gMfxLogSkipped;
extern mfxLogLevel gMfxLogLevel;
extern std::shared_ptr<std::FILE> gMfxLogFile;
extern std::shared_ptr<std::FILE> gMfxAPIDumpFile;
extern std::mutex gMfxLogMutex;

#define COLOR_BLACK  0x0000
#define COLOR_GREEN  0x0002
#define COLOR_RED    0x0004
#define COLOR_GRAY   0x0008
#define COLOR_YELLOW 0x000E
#define COLOR_WHITE  0x000F

class MfxLogSkip
{
public:
    MfxLogSkip()
    {
        gMfxLogSkipped = true;
    };

    ~MfxLogSkip()
    {
        gMfxLogSkipped = false;
    }
};

std::unique_ptr<MfxLogSkip> GetMfxLogSkip();
void InitMfxLogging();


const std::string GetMFXStatusInString(mfxStatus mfxSts);

#if defined(MFX_ENABLE_LOG_UTILITY)
    #define SET_LOG_GUARD() std::unique_lock<std::mutex> closeGuard(gMfxLogMutex)
#else
    #define SET_LOG_GUARD()
#endif

#if defined(MFX_ENABLE_LOG_UTILITY)
    #define CHECK_LOG_LEVEL(level) \
        if (gMfxLogSkipped || gMfxLogLevel == LEVEL_DISABLED || level < gMfxLogLevel) break
#else
    #define CHECK_LOG_LEVEL(level)
#endif

inline void SetLogColor(uint16_t backColor, uint16_t frontColor)
{
#if defined(MFX_ENABLE_LOG_UTILITY)
    std::ignore = backColor;
    std::ignore = frontColor;
#else
    std::ignore = backColor;
    std::ignore = frontColor;
#endif
}

#if defined(_MSVC_LANG)
#pragma warning(push)
#pragma warning(disable: 4100)
#endif

// Fix GCC warning: format not a string literal and no format arguments
template<class... Args,
    typename std::enable_if<sizeof...(Args) == 1, bool>::type = true>
inline void MfxFprintf(FILE* file, Args&&... args)
{
    fprintf(file, "%s", std::forward<Args>(args)...);
}

template<class... Args,
    typename std::enable_if<sizeof...(Args) >= 2, bool>::type = true>
inline void MfxFprintf(FILE* file, Args&&... args)
{
    fprintf(file, std::forward<Args>(args)...);
}

template<class... Args>
inline void MfxLog(FILE* file, const char* levelName, const char* fileName, const uint32_t lineNum, Args&&... args)
{
#if defined(MFX_ENABLE_LOG_UTILITY)
    std::stringstream threadID;
    threadID << std::this_thread::get_id();
    MfxFprintf(file, "TH#%s %s %s[Line: %d]", threadID.str().c_str(), levelName, fileName, lineNum);
    MfxFprintf(file, std::forward<Args>(args)...);
    fflush(file);
#endif
}

template<class... Args>
inline void MfxLog(const mfxLogLevel level, const char* levelName,
    const char* fileName, const uint32_t lineNum, Args&&... args)
{
#if defined(MFX_ENABLE_LOG_UTILITY)
    FILE* file     = gMfxLogFile != nullptr ? gMfxLogFile.get() : stdout;
    bool bSetColor = gMfxLogFile == nullptr;

    if (bSetColor)
    {
        if (level == LEVEL_INFO)
        {
            SetLogColor(COLOR_BLACK, COLOR_GREEN);
        }
        else if (level == LEVEL_WARN)
        {
            SetLogColor(COLOR_BLACK, COLOR_GRAY);
        }
        else if (level == LEVEL_ERROR)
        {
            SetLogColor(COLOR_BLACK, COLOR_YELLOW);
        }
        else if (level == LEVEL_FATAL)
        {
            SetLogColor(COLOR_BLACK, COLOR_RED);
        }
        else
        {
            bSetColor = false;
        }
    }

    MfxLog(file, levelName, fileName, lineNum, std::forward<Args>(args)...);

    if (bSetColor)
    {
        SetLogColor(COLOR_BLACK, COLOR_WHITE);
    }
#endif
}

#if defined(_MSVC_LANG)
#pragma warning(pop)
#endif

template<class... Args>
inline void MfxLogPrint(FILE* file, Args&&... args)
{
    MfxFprintf(file, std::forward<Args>(args)...);
    fflush(file);
}

template<class... Args>
inline void MfxLogPrint(Args&&... args)
{
    FILE* file = gMfxLogFile != nullptr ? gMfxLogFile.get() : stdout;
    MfxLogPrint(file, std::forward<Args>(args)...);
}

#define MFX_LOG(level, ...) do { \
    CHECK_LOG_LEVEL(level); \
    SET_LOG_GUARD(); \
    MfxLog(level, #level, __VA_ARGS__); } \
while(0)

#define MFX_LOG_TRACE(...) MFX_LOG(LEVEL_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define MFX_LOG_DEBUG(...) MFX_LOG(LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define MFX_LOG_INFO(...) MFX_LOG(LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define MFX_LOG_WARN(...) MFX_LOG(LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define MFX_LOG_ERROR(...) MFX_LOG(LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#if defined(MFX_ENABLE_LOG_UTILITY)
    #define MFX_LOG_FATAL(...) MFX_LOG(LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__)
#else
    #define MFX_LOG_FATAL(...) MfxLogPrint(stdout, ##__VA_ARGS__)
#endif

#if defined(MFX_ENABLE_LOG_UTILITY)
    #define MFX_LOG_DDI_TRACE(file, ...) do { \
        CHECK_LOG_LEVEL(LEVEL_TRACE); \
        MfxLogPrint(file, ##__VA_ARGS__); } \
    while (0)
#endif

#if defined(MFX_ENABLE_LOG_UTILITY)
    template <typename T>
    const std::string GetNumberFormat(T&)
    {
        if (std::is_signed<T>::value)
            return "%d";
        else
            return "%u";
    }
#endif

#if defined(MFX_ENABLE_LOG_UTILITY)
    #define MFX_LOG_API_TRACE(...) do { \
        CHECK_LOG_LEVEL(LEVEL_TRACE); \
        if (gMfxAPIDumpFile != nullptr) MfxLogPrint(gMfxAPIDumpFile.get(), ##__VA_ARGS__); } \
    while (0)
#else
    #define MFX_LOG_API_TRACE(...)
#endif
