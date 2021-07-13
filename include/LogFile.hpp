#ifndef _LOG_FILE_HH
#define _LOG_FILE_HH
#include <string>
#include <mutex>
#include "scoped_ptr.hpp"

namespace FileUtil
{
    class AppendFile;
}

class LogFile
{
public:
    LogFile(const std::string &filePath, off_t rollSize, bool threadSafe, int flushInterval = 3);
    ~LogFile();

    void append(const char *logline, int len);
    void flush();

    std::string getlogFileName(const std::string &baseName);

private:
    void append_unlocked(const char *logline, int len);

    //static std::string getlogFileName(const std::string& filePath);
    bool rollFile();

    const std::string m_filePath;
    const int m_flushInterval;
    time_t m_startOfPeriod; // 开始记录日志时间（调整至零点的时间）
    time_t m_lastRoll;      // 上一次滚动日志文件时间
    time_t m_lastFlush;     // 上一次日志写入文件时间
    int m_rollCnt;
    off_t m_roolSize;
    const static int kCheckTimeRoll_ = 1024;
    const static int kRollPerSeconds_ = 60 * 60 * 24; //一天的秒数
    scoped_ptr<std::mutex> m_mutex;
    scoped_ptr<FileUtil::AppendFile> m_file;
};

#endif