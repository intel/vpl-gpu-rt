#include "mfx_config.h"
#include <iostream>
#include <stdarg.h>
#include <iomanip> 
#include <pthread.h>
#include "unistd.h"
#include <sys/stat.h>

#include "mfx_decode_dpb_logging.h"


DPBLog* dpb_logger = nullptr;
std::string DPBLog::dpbFilePath = "Initialize";
std::shared_ptr<DPBLog> DPBLog::instance = nullptr;


int32_t DpbSecureStringPrint(char* buffer, size_t bufSize, size_t length, const char* const format, ...)
{
    int32_t   iRet = 0;
    va_list   var_args;

    va_start(var_args, format);
    iRet = vsnprintf(buffer, bufSize, format, var_args);
    va_end(var_args);

    return iRet;
}

#define Dpb_SecureStringPrint(buffer, bufSize, length, format, ...)                                 \
    DpbSecureStringPrint(buffer, bufSize, length, format, ##__VA_ARGS__)

DPBLog::DPBLog()
{
    instance = nullptr;
}


DPBLog::~DPBLog()
{
    if (instance)
    {
        try
        {
            Flushlog();
        }
        catch (...)
        {

        }
    }
}


std::string GetFunctionName(const std::string& input)
{
    // find last ':'
    size_t pos = input.rfind(':');
    if (pos == std::string::npos)
    {
        // if not find ':', return empty
        return "";
    }
    // extract ':' 
    return input.substr(pos + 1);
}

void DPBLog::Addlog(const std::string& funtion, int line, std::string change, uintptr_t frameaddress, uint32_t refoucnter)
{
    //get function name
    std::string function_name = GetFunctionName(funtion);
    if (!function_name.empty())
    {
        Logbuffer << std::left
                  << std::setw(25) << function_name << ":"
                  << std::setw(10) << std::dec << line 
                  << std::setw(16) << std::hex<< std::uppercase << frameaddress << "refcounter "
                  << std::setw(5)  << std::dec << change
                  << std::setw(5)  << std::dec << refoucnter << std::endl;
    }
    else
    {
        Logbuffer << "Not find function name" << std::endl;
    }
}

void DPBLog::Flushlog() 
{

    int32_t pid = getPid();
    int32_t tid = getTid();

    const char* const dpb_log_path_fmt = "%s/dpb_pid%d_tid%d.txt"; 


    char logfilename[MFX_MAX_DPBLOG_FILENAME_LEN + 1] = { '\0' };
    Dpb_SecureStringPrint(logfilename, MFX_MAX_PATH_LENGTH + 1, MFX_MAX_PATH_LENGTH + 1,
        dpb_log_path_fmt, dpbFilePath.c_str(), pid, tid);


    if (access(dpbFilePath.c_str(), 0) == -1)
    {
        int folder_exist_status = mkdir(dpbFilePath.c_str(), S_IRWXU);
        if (folder_exist_status == -1)
        {
            return;
        }
    }

    std::ofstream file(logfilename);
    if (file.is_open()) {
        file << Logbuffer.str(); // write into file
        Logbuffer.str("");       // reset buffer
        Logbuffer.clear();       // clear buffer 
        file.close();
    }
    else {
        std::cerr << "Failed to open log file." << std::endl;
    }
}


DPBLog* DPBLog::getInstance()
{
    if (instance == nullptr)
    {
        instance = std::make_shared<DPBLog>();
        if (!instance)
        {
            return nullptr;
        }
    }

    return instance.get();
}

int32_t DPBLog::getPid()
{
    int32_t pid = getpid();
    return  pid;
}

int32_t DPBLog::getTid()
{
    int32_t tid = pthread_self();
    return tid;
}






