#include "mfx_config.h"
#include <iostream>
#include <stdarg.h>
#include <pthread.h>
#include "unistd.h"
#include <sys/stat.h> 

#include "mfx_utils_perf.h"

int32_t QueryPerformanceFrequency(int64_t & frequency)
{
    struct timespec  res;

    if ( clock_getres(CLOCK_MONOTONIC, &res) != 0 )
    {
        return -1;
    }

    // resolution (precision) can't be in seconds for current machine and OS
    if (res.tv_sec != 0)
    {
        return -1;
    }
    frequency = (1000000000LL) / res.tv_nsec;

    return 0;
}

int32_t QueryPerformanceCounter(int64_t & performanceCount)
{
    struct timespec     res;
    struct timespec     t;

    if ( clock_getres (CLOCK_MONOTONIC, &res) != 0 )
    {
        return -1;
    }
    if (res.tv_sec != 0)
    { // resolution (precision) can't be in seconds for current machine and OS
        return -1;
    }
    if( clock_gettime(CLOCK_MONOTONIC, &t) != 0)
    {
        return -1;
    }
    performanceCount = (1000000000LL * t.tv_sec + t.tv_nsec) / res.tv_nsec;

    return 0;
}

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

PerfUtility::PerfUtility()
{
    int32_t dwPerfUtilityIsEnabled = 0;
    double timeStamp = 0;
}

PerfUtility::~PerfUtility()
{
    // save perf data here
    if (instance->dwPerfUtilityIsEnabled)
    {
        instance->savePerfData();
    }
}

int32_t PerfUtility::getPid()
{
    int32_t pid = getpid();
    return pid;
}

int32_t PerfUtility::getTid()
{
    int32_t tid = pthread_self();
    return tid;
}

void PerfUtility::savePerfData()
{
    std::fstream pTimeStampFile;
    int32_t pid = getPid();

        const char* const perf_log_path_fmt = "%s/perf_details_pid%d_tid%d.txt";
    for (auto it : log_buffer)
    {

        MFX_SecureStringPrint(sDetailsFileName, MFX_MAX_PATH_LENGTH + 1, MFX_MAX_PATH_LENGTH + 1,
                              perf_log_path_fmt, perfFilePath.c_str(), pid, it.first);

        if (access(perfFilePath.c_str(), 0) == -1)
        {
            int folder_exist_status = mkdir(perfFilePath.c_str(), S_IRWXU);
            if (folder_exist_status == -1)
            {
                return;
            }
        }
        pTimeStampFile.open(sDetailsFileName, std::ios::app);
        if (pTimeStampFile.good() == false)
        {
            pTimeStampFile.close();
            return;
        }
        pTimeStampFile << it.second;
        pTimeStampFile.close();
        pTimeStampFile.clear();
    }
}

void PerfUtility::timeStampTick(std::string tag, std::string level, std::string flag, const std::vector<uint32_t>& taskIds)
{
    Tick newTick;
    newTick.tag = tag;
    int64_t _freq = 0;
    QueryPerformanceFrequency(_freq);
    newTick.freq = _freq / 1000;     // ms
    QueryPerformanceCounter(newTick.timestamp);
    newTick.functionType = flag;
    newTick.level = level;
    printPerfTimeStamp(&newTick, taskIds);
}

void PerfUtility::startTick(std::string tag)
{
}

void PerfUtility::stopTick(std::string tag)
{
}

void PerfUtility::printPerfTimeStamp(Tick *newTick, const std::vector<uint32_t>& taskIds)
{
    int32_t current_tid = getTid();

    std::map<int32_t, std::string>::iterator it;

    it = log_buffer.find(current_tid);
    if (it == log_buffer.end()) 
    {
        std::string ss;
        log_buffer[current_tid] = ss;
    }

    if (newTick->level == PERF_LEVEL_DDI || newTick->level == PERF_LEVEL_HW)
    {
        log_buffer[current_tid].append("    ");
    }
    else if(newTick->level == PERF_LEVEL_ROUTINE)
    {
        log_buffer[current_tid].append("  ");
    }
    else if (newTick->level == PERF_LEVEL_INTERNAL)
    {
        log_buffer[current_tid].append("   ");
    }
    
    log_buffer[current_tid].append(newTick->tag);
    log_buffer[current_tid].append(newTick->functionType);
    log_buffer[current_tid].append("\tTimeStamp: ");
    log_buffer[current_tid].append(std::to_string(newTick->timestamp));
    log_buffer[current_tid].append("\tFreq: ");
    log_buffer[current_tid].append(std::to_string(newTick->freq));
    if (!taskIds.empty())
    {
        log_buffer[current_tid].append("\tAsync Task ID: ");
        for (auto id : taskIds)
        {
            log_buffer[current_tid].append(std::to_string(id));
        }
    }
    log_buffer[current_tid].append("\n");
}