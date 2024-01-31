#pragma once

#include <atomic>
#include <stdint.h>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <fstream>
#include <sstream>
#include <vector>

#define MFX_MAX_PERF_FILENAME_LEN 260
#define MFX_MAX_PATH_LENGTH       256
//For perf log
typedef struct _Tick
{
    std::string tag;
    int64_t timestamp;
    int64_t freq;
    std::string functionType;
    std::string level;
}Tick;

class PerfUtility
{
public:
    static PerfUtility* getInstance();
    ~PerfUtility();
    PerfUtility() = default;
    int32_t getPid();
    int32_t getTid();
    void timeStampTick(const std::string &tag, const std::string &level, const std::string &flag, const std::vector<uint32_t> &taskIds);
    void savePerfData();
    static std::string perfFilePath;
    static std::atomic<double> timeStamp;

private:
    void printPerfTimeStamp(Tick* newTick, const std::vector<uint32_t>& taskIds);

private:
    static std::shared_ptr<PerfUtility> instance;
    static std::mutex perfMutex;
    std::map<int32_t, std::string> log_buffer{};
};


extern PerfUtility* g_perfutility;

#define PERF_LEVEL_API "API"
#define PERF_LEVEL_DDI "DDI"
#define PERF_LEVEL_HW "HW"
#define PERF_LEVEL_ROUTINE "Routine"
#define PERF_LEVEL_INTERNAL "INTERNAL"

#define MFX_FLAG_ENTER ": ENTER"
#define MFX_FLAG_EXIT  ": EXIT"

#define PERF_UTILITY_TIMESTAMP(TAG,LEVEL,FLAG)                                       \
    do                                                                               \
    {                                                                                \
        if (g_perfutility)                                                           \
        {                                                                            \
            g_perfutility->timeStampTick(TAG, LEVEL, FLAG, std::vector<uint32_t>()); \
        }                                                                            \
    } while(0)

#define PERF_UTILITY_AUTO(TAG,LEVEL) AutoPerfUtility apu(TAG,LEVEL)
#define PERF_UTILITY_SET_ASYNC_TASK_ID(id) AutoPerfUtility::SetTaskId(id)

class AutoPerfUtility
{
public:
    static void SetTaskId(uint32_t id);
    AutoPerfUtility(const std::string &tag, const std::string &level);
    ~AutoPerfUtility();

private:
    static std::mutex map_guard;
    static std::map<uint64_t, std::vector<uint32_t>> tid2taskIds;
    bool bPrintTaskIds = false;
    std::string autotag = "intialized";
    std::string autolevel = "intialized";
};