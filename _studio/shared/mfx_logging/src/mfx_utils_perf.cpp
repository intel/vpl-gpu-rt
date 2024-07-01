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

PerfUtility* g_perfutility = nullptr;
std::string PerfUtility::perfFilePath = "Initialize";
std::shared_ptr<PerfUtility> PerfUtility::instance = nullptr;
std::mutex PerfUtility::perfMutex;
std::mutex AutoPerfUtility::map_guard;
std::map<uint64_t, std::vector<uint32_t>> AutoPerfUtility::tid2taskIds;

void AutoPerfUtility::SetTaskId(uint32_t id)
{
    if (!g_perfutility)
    {
        return;
    }

    uint64_t tid = pthread_self();

    decltype(tid2taskIds)::iterator it;
    if ((it = tid2taskIds.find(tid)) == tid2taskIds.end())
    {
        std::lock_guard<std::mutex> lg(map_guard);
        tid2taskIds[tid] = {};
        it = tid2taskIds.find(tid);
    }
    it->second.push_back(id);
}

AutoPerfUtility::AutoPerfUtility(const std::string& tag, const std::string& level)
{
    if (!g_perfutility)
    {
        return;
    }

    std::string flag = MFX_FLAG_ENTER;
    try
    {
        g_perfutility->timeStampTick(tag, level, flag, std::vector<uint32_t>());
    }
    catch(...)
    {
        return;
    }

    autotag = tag;
    autolevel = level;
    if (level == PERF_LEVEL_API || level == PERF_LEVEL_ROUTINE)
    {
        bPrintTaskIds = true;
    }
}

AutoPerfUtility::~AutoPerfUtility()
{
    if (!g_perfutility)
    {
        return;
    }

    std::string flag = MFX_FLAG_EXIT;
    std::vector<uint32_t> ids;
    uint64_t tid = pthread_self();

    if (bPrintTaskIds && tid2taskIds.find(tid) != tid2taskIds.end())
    {
        tid2taskIds[tid].swap(ids);
    }

    try
    {
        g_perfutility->timeStampTick(autotag, autolevel, flag, ids);
    }
    catch (std::bad_array_new_length&)
    {

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

PerfUtility::~PerfUtility()
{
    // save perf data here
    if (instance)
    {
        try
        {
            instance->savePerfData();
        }
        catch (...)
        {

        }
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
        char sDetailsFileName[MFX_MAX_PERF_FILENAME_LEN + 1] = { '\0' };
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

void PerfUtility::timeStampTick(const std::string &tag, const std::string &level, const std::string &flag, const std::vector<uint32_t>& taskIds)
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

void PerfUtility::printPerfTimeStamp(Tick *newTick, const std::vector<uint32_t>& taskIds)
{
    int32_t current_tid = getTid();

    std::map<int32_t, std::string>::iterator it;

    it = log_buffer.find(current_tid);
    if (it == log_buffer.end()) 
    {
        std::lock_guard<std::mutex> lock(perfMutex);
        log_buffer[current_tid] = {};
        it = log_buffer.find(current_tid);
    }

    if (newTick->level == PERF_LEVEL_DDI || newTick->level == PERF_LEVEL_HW)
    {
        it->second.append("    ");
    }
    else if(newTick->level == PERF_LEVEL_ROUTINE)
    {
        it->second.append("  ");
    }
    else if (newTick->level == PERF_LEVEL_INTERNAL)
    {
        log_buffer[current_tid].append("   ");
    }
    
    it->second.append(newTick->tag);
    it->second.append(newTick->functionType);
    it->second.append("\tTimeStamp: ");
    it->second.append(std::to_string(newTick->timestamp));
    it->second.append("\tFreq: ");
    it->second.append(std::to_string(newTick->freq));
    if (!taskIds.empty())
    {
        it->second.append("\tAsync Task ID: ");
        for (auto id : taskIds)
        {
            it->second.append(std::to_string(id));
        }
    }
    it->second.append("\n");
}