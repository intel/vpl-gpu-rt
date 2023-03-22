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
std::mutex AutoPerfUtility::map_guard;
std::map<uint64_t, std::vector<uint32_t>> AutoPerfUtility::tid2taskIds;

void AutoPerfUtility::SetTaskId(uint32_t id)
{
    uint64_t tid = pthread_self();

    std::lock_guard<std::mutex> lg(map_guard);
    tid2taskIds[tid].push_back(id);
}

AutoPerfUtility::AutoPerfUtility(std::string tag, std::string level)
{
    if (g_perfutility->dwPerfUtilityIsEnabled)
    {
        std::string flag = MFX_FLAG_ENTER;
        g_perfutility->timeStampTick(tag, level, flag, std::vector<uint32_t>());
        autotag = tag;
        autolevel = level;
        bEnable = true;
        if (level == PERF_LEVEL_API || level == PERF_LEVEL_ROUTINE)
        {
            bPrintTaskIds = true;
        }
    }
}

AutoPerfUtility::~AutoPerfUtility()
{
    if (bEnable)
    {
        std::string flag = MFX_FLAG_EXIT;
        std::vector<uint32_t> ids;
        uint64_t tid = pthread_self();

        if (bPrintTaskIds)
        {
            std::lock_guard<std::mutex> lg(map_guard);
            if (tid2taskIds.find(tid) != tid2taskIds.end())
            {
                tid2taskIds[tid].swap(ids);
            }
        }

        g_perfutility->timeStampTick(autotag, autolevel, flag, ids);
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

bool PerfUtility::setupFilePath(std::fstream& pTimeStampFile)
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

void PerfUtility::timeStampTick(std::string tag, std::string level, std::string flag, const std::vector<uint32_t>& taskIds)
{
    std::lock_guard<std::mutex> lock(perfMutex);
    newTick.tag = tag;
    newTick.functionType = flag;
    newTick.level = level;
    printPerfTimeStamp(&newTick, taskIds);
}

void PerfUtility::printPerfTimeStamp(Tick *newTick, const std::vector<uint32_t>& taskIds)
{
    std::fstream pTimeStampFile;
    bool fileStatus = setupFilePath(pTimeStampFile);
    if (fileStatus == true)
    {
        if (newTick->level == PERF_LEVEL_DDI || newTick->level == PERF_LEVEL_HW)
        {
            pTimeStampFile << "    ";
        }
        else if (newTick->level == PERF_LEVEL_ROUTINE)
        {
            pTimeStampFile << "  ";
        }
        pTimeStampFile << newTick->tag << newTick->functionType << "\tTimeStamp: " << newTick->timestamp << "\tFreq: " << newTick->freq;
        if (!taskIds.empty())
        {
            pTimeStampFile << "\tAsync Task ID: ";
            for (auto id : taskIds)
            {
                pTimeStampFile << id << ",";
            }
            pTimeStampFile.unget();
        }
        pTimeStampFile << std::endl;
        pTimeStampFile.close();
    }
}