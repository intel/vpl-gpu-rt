#pragma once

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
//For TXT trace
typedef struct _TickTime
{
    double freq;
    int64_t start;
    int64_t stop;
}TickTime;

class PerfUtility
{
public:
    static PerfUtility* getInstance();
    ~PerfUtility();
    PerfUtility();
    int32_t getPid();
    int32_t getTid();
    void timeStampTick(std::string tag, std::string level, std::string flag, const std::vector<uint32_t> &taskIds);
    void startTick(std::string tag);
    void stopTick(std::string tag);
    void savePerfData();
    int32_t dwPerfUtilityIsEnabled;
    std::string perfFilePath;
    double timeStamp;

private:
    void printPerfTimeStamp(Tick* newTick, const std::vector<uint32_t>& taskIds);

private:
    static std::shared_ptr<PerfUtility> instance;
    char sDetailsFileName[MFX_MAX_PERF_FILENAME_LEN + 1] = { '\0' };
    static std::mutex perfMutex;
    std::map<int32_t, std::string> log_buffer{};
    std::map<std::string, std::vector<TickTime>*> records{};
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
        if (g_perfutility->dwPerfUtilityIsEnabled)                                   \
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
    AutoPerfUtility(std::string tag, std::string level);
    ~AutoPerfUtility();

private:
    static std::mutex map_guard;
    static std::map<uint64_t, std::vector<uint32_t>> tid2taskIds;
    bool bEnable = false;
    bool bPrintTaskIds = false;
    std::string autotag = "intialized";
    std::string autolevel = "intialized";
};