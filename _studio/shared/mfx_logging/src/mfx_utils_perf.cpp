#include "mfx_config.h"
#include <iostream>
#include <stdarg.h>
#include <pthread.h>
#include "unistd.h"

#include "mfx_utils_perf.h"

std::string perfFilePath = "C:\\Temp\\";

int32_t MfxSecureStringPrint(char* buffer, size_t bufSize, size_t length, const char* const format, ...)
{
    int32_t   iRet = 0;
    va_list var_args;

    va_start(var_args, format);
    iRet = vsnprintf(buffer, bufSize, format, var_args);
    va_end(var_args);

    return iRet;
}

#define MFX_SecureStringPrint(buffer, bufSize, length, format, ...)                                 \
    MfxSecureStringPrint(buffer, bufSize, length, format, ##__VA_ARGS__)

PerfUtility* g_perfutility = PerfUtility::getInstance();
std::shared_ptr<PerfUtility> PerfUtility::instance = nullptr;
std::mutex PerfUtility::perfMutex;


AutoPerfUtility::AutoPerfUtility(std::string tag, std::string level)
{
    if (g_perfutility->dwPerfUtilityIsEnabled)
    {
        std::string flag = MFX_FLAG_ENTER;
        g_perfutility->timeStampTick(tag, level, flag);
        autotag = tag;
        autolevel = level;
        bEnable = true;
    }
}

AutoPerfUtility::~AutoPerfUtility()
{
    if (bEnable)
    {
        std::string flag = MFX_FLAG_EXIT;
        g_perfutility->timeStampTick(autotag, autolevel, flag);
    }
}

PerfUtility* PerfUtility::getInstance()
{
    if (instance == nullptr)
    {
        instance = std::make_shared<PerfUtility>();
    }

    return instance.get();
}

bool PerfUtility::setupFilePath(std::ofstream& pTimeStampFile)
{
    int32_t pid = getpid();
    int32_t tid = pthread_self();
    MFX_SecureStringPrint(sDetailsFileName, MFX_MAX_PATH_LENGTH + 1, MFX_MAX_PATH_LENGTH + 1,
        "%sperf_details_pid%d_tid%d.txt", perfFilePath.c_str(), pid, tid);
    pTimeStampFile.open(sDetailsFileName, std::ios::app);
    if (pTimeStampFile.good() == false)
    {
        pTimeStampFile.close();
        return false;
    }
    return true;
}

void PerfUtility::timeStampTick(std::string tag, std::string level, std::string flag)
{
    std::lock_guard<std::mutex> lock(perfMutex);
    newTick.tag = tag;
    newTick.functionType = flag;
    newTick.level = level;
    printPerfTimeStamp(&newTick);
}

void PerfUtility::printPerfTimeStamp(Tick *newTick)
{
    std::ofstream pTimeStampFile;
    bool fileStatus = setupFilePath(pTimeStampFile);
    if (fileStatus == true)
    {
        if (newTick->level == PERF_LEVEL_DDI)
        {
            pTimeStampFile << "\t";
        }
        pTimeStampFile << newTick->tag << newTick->functionType << "\tTimeStamp: " << newTick->timestamp << "\tFreq: " << newTick->freq << std::endl;
        pTimeStampFile.close();
    }
}