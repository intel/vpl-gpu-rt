#pragma once

#include <stdint.h>
#include <string>
#include <memory>
#include <mutex>
#include <fstream>
#include <sstream>

#define MFX_MAX_PERF_FILENAME_LEN 260
#define MFX_MAX_PATH_LENGTH       256

typedef struct _Tick
{
    std::string tag;
    int64_t timestamp;
    double freq;
    std::string functionType;
    std::string level;
}Tick;

class PerfUtility
{
public:
    static PerfUtility* getInstance();
    ~PerfUtility() {};
    PerfUtility() {};
    void timeStampTick(std::string tag, std::string level, std::string flag);
    bool setupFilePath(std::ofstream& pTimeStampFile);
    void closeFile() {};
    char sDetailsFileName[MFX_MAX_PERF_FILENAME_LEN + 1] = { '\0' };
    int32_t dwPerfUtilityIsEnabled = false;
    static std::mutex perfMutex;

private:
    void printPerfTimeStamp(Tick* newTick);

private:
    static std::shared_ptr<PerfUtility> instance;
    Tick newTick;
};


extern PerfUtility* g_perfutility;

#define PERF_LEVEL_API "API"
#define PERF_LEVEL_DDI "DDI"

#define MFX_FLAG_ENTER ": ENTER"
#define MFX_FLAG_EXIT  ": EXIT"

#define PERF_UTILITY_TIMESTAMP(TAG,LEVEL,FLAG)                             \
    do                                                                     \
    {                                                                      \
        if (g_perfutility->dwPerfUtilityIsEnabled)                         \
        {                                                                  \
            g_perfutility->timeStampTick(TAG, LEVEL, FLAG);                \
        }                                                                  \
    } while(0)

#define PERF_UTILITY_AUTO(TAG,LEVEL) AutoPerfUtility apu(TAG,LEVEL)

class AutoPerfUtility
{
public:
    AutoPerfUtility(std::string tag, std::string level);
    ~AutoPerfUtility();

private:
    bool bEnable = false;
    std::string autotag = "intialized";
    std::string autolevel = "intialized";
};