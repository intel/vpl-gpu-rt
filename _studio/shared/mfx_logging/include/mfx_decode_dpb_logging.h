#pragma once

#include <stdint.h>
#include <string>
#include <fstream>
#include <sstream>
#include <memory>

#define MFX_MAX_DPBLOG_FILENAME_LEN    260
#define MFX_MAX_PATH_LENGTH            256


#define DPBLOG_PRINT(funtion,line, change, frame, count)               \
        if(dpb_logger)                                                  \
        {                                                               \
            uintptr_t address = reinterpret_cast<uintptr_t>(frame);     \
            dpb_logger->Addlog(funtion, line, change, address, count);  \
        } 

class DPBLog
{
public:

    DPBLog();
    ~DPBLog();
    void  Addlog(const std::string& funtion, int line, std::string change, uintptr_t frameaddress, uint32_t refoucnter);
    int32_t getPid();
    int32_t getTid();
    static  DPBLog* getInstance();

    //flush
    void  Flushlog();
    static std::string dpbFilePath;

private:
    std::ostringstream Logbuffer;
    static std::shared_ptr<DPBLog> instance; 
};

extern DPBLog* dpb_logger;

