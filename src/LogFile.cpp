#include "LogFile.hpp"
#include "FileUtil.hpp"
#include <time.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <assert.h>

LogFile::LogFile(const std::string &filePath, off_t rollSize, bool threadSafe, int flushInterval)
    : m_filePath(filePath),
      m_flushInterval(flushInterval),
      m_rollCnt(0),
      m_roolSize(rollSize),
      m_mutex(threadSafe ? new std::mutex : NULL),
      m_file(/*new FileUtil::AppendFile(m_filePath)*/ NULL),
      m_startOfPeriod(0),
      m_lastFlush(0),
      m_lastRoll(0)
{
    assert(filePath.find('/') == std::string::npos);
    rollFile();
}

LogFile::~LogFile()
{
}

void LogFile::append(const char *logline, int len)
{
    if (m_mutex) //线程安全版本
    {
        std::lock_guard<std::mutex> lock(*m_mutex);
        append_unlocked(logline, len);
    }
    else //不考虑线程安全版本（use this version）
    {
        append_unlocked(logline, len);
    }
}

void LogFile::append_unlocked(const char *logline, int len)
{
    m_file->append(logline, len);

    //写入的数据已经超过日志滚动大小了。这里是根据文件大小进行滚动
    if (m_file->writtenBytes() > m_roolSize)
    {
        rollFile();
    }
    else
    {
        if (m_rollCnt > kCheckTimeRoll_) //如果追加次数大于1024
        {
            m_rollCnt = 0;
            time_t now = ::time(NULL);
            time_t thisPeriod_ = now / kRollPerSeconds_ * kRollPerSeconds_;
            if (thisPeriod_ != m_startOfPeriod) //文件创建的日期不是现在的日期。就是创建文件的日期不是今天，就重新建文件
            {
                rollFile();
            }
            else if (now - m_lastFlush > m_flushInterval) //如果距离上次写入文件的时间大于刷新时间间隔3s，就刷新一下内存
            {
                m_lastFlush = now;
                m_file->flush();
            }
        }
        else
        {
            ++m_rollCnt; //每追加一次，就加1
        }
    }
}

void LogFile::flush()
{
    if (m_mutex) //有锁
    {
        std::lock_guard<std::mutex> lock(*m_mutex);
        m_file->flush();
    }
    else //无锁（use this version）
    {
        m_file->flush();
    }
}

bool LogFile::rollFile()
{
    time_t now = ::time(NULL);
    std::string filename = getlogFileName(m_filePath);
    // 注意，这里先除kRollPerSeconds_ 后乘kRollPerSeconds_表示
    // 对齐至kRollPerSeconds_整数倍，也就是时间调整到当天零点。
    time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;
    if (now > m_lastRoll)
    {
        m_lastRoll = now;
        m_lastFlush = now;
        m_startOfPeriod = start;
        m_file.reset(new FileUtil::AppendFile(filename));
    }
    // if (m_file->writtenBytes() < m_roolSize)
    //     m_file.reset(new FileUtil::AppendFile(m_filePath));
    // else
    // {
    //     assert(remove(m_filePath.c_str()) == 0);
    //     m_file.reset(new FileUtil::AppendFile(m_filePath));
    // }
    //checkLogNum();

    return true;
}

std::string LogFile::getlogFileName(const std::string &baseName)
{
    std::string fileName;
    fileName.reserve(baseName.size() + 32);
    fileName = baseName.substr(0, baseName.rfind("."));

    char timebuf[24];
    struct tm tm;
    time_t now = time(NULL);
    gmtime_r(&now, &tm);
    strftime(timebuf, sizeof(timebuf), ".%Y%m%d-%H%M%S", &tm);
    fileName += timebuf;
    fileName += ".log";

    return fileName;
}
