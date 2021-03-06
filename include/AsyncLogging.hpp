#ifndef _ASYNC_LOGGING_HH
#define _ASYNC_LOGGING_HH

#include <memory>
#include <mutex>
#include <string>

#include "Thread.hpp"
#include "LogStream.hpp"
#include "ptr_vector.hpp"
#include "Condition.hpp"


class AsyncLogging
{
public:
        void start()
    {
        m_isRunning = true;
        m_thread.start();
    }
    static AsyncLogging &GetLogInstance(const std::string filePath, off_t rollSize, double flushInterval = 3.0)
    {
        static AsyncLogging log(filePath, rollSize, flushInterval);
        return log;
    }

    void stop()
    {
        m_isRunning = false;
        m_cond.notify();
        m_thread.join();
    }

    void append(const char *logline, std::size_t len);

private:
    AsyncLogging(const AsyncLogging &) = delete;
    AsyncLogging &operator=(const AsyncLogging &) = delete;
    AsyncLogging(const std::string filePath, off_t rollSize, double flushInterval = 3.0);
    ~AsyncLogging();
    void threadRoutine();

    typedef LogBuffer<kLargeBuffer> Buffer;          //Buffer大小4M
    typedef myself::ptr_vector<Buffer> BufferVector; //智能指针数组, 存放已经满了的前台buffer。
    typedef std::unique_ptr<Buffer> BufferPtr;       //智能指针，指向分配的缓冲区

    std::string m_filePath;
    off_t m_rollSize;             //预留日志大小，日志超过这个大小就会重新建一个新的日志文件
    const double m_flushInterval; //超时时间，在flushInterval_秒内，缓冲区没有写满，仍将缓冲区的数据写到文件中
    bool m_isRunning;
    Thread m_thread;
    std::mutex m_mutex;
    Condition m_cond;

    BufferPtr m_currentBuffer;
    BufferPtr m_nextBuffer;
    BufferVector m_buffers;
};

#endif
