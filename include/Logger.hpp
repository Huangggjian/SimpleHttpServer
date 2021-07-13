#ifndef _LOGGER_HH
#define _LOGGER_HH

#include <string.h>
#include "LogStream.hpp"
#include "TimeStamp.hpp"

/* Logger类是一个生命周期非常短的类，在使用的过程中基本就是使用类的临时变量，在类创建时将日志信息输出到buffer区，然后在类析构时将Buffer区
 * 的内容调用输出函数去运行，所以一般都是使用宏定义来创建类的临时变量
 * */

//如果全局变量日志等级为TRACE，就定义LOG_TRACE宏，也就是创建一个临时变量来使用，可以实现LOG_TRACE<<"要存入的字符串"这样一个形式。
#define LOG_TRACE                            \
    if (Logger::logLevel() <= Logger::TRACE) \
    Logger(__FILE__, __LINE__, Logger::TRACE, __func__).stream()
#define LOG_DEBUG                            \
    if (Logger::logLevel() <= Logger::DEBUG) \
    Logger(__FILE__, __LINE__, Logger::DEBUG, __func__).stream()
#define LOG_INFO                            \
    if (Logger::logLevel() <= Logger::INFO) \
    Logger(__FILE__, __LINE__).stream()
#define LOG_WARN                            \
    if (Logger::logLevel() <= Logger::WARN) \
    Logger(__FILE__, __LINE__, Logger::WARN).stream()
#define LOG_ERROR                            \
    if (Logger::logLevel() <= Logger::ERROR) \
    Logger(__FILE__, __LINE__, Logger::ERROR).stream()
#define LOG_FATAL                            \
    if (Logger::logLevel() <= Logger::FATAL) \
    Logger(__FILE__, __LINE__, Logger::FATAL).stream()
#define LOG_SYSERR                           \
    if (Logger::logLevel() <= Logger::ERROR) \
    Logger(__FILE__, __LINE__, false).stream()
#define LOG_SYSFATAL                         \
    if (Logger::logLevel() <= Logger::FATAL) \
    Logger(__FILE__, __LINE__, true).stream()

class Logger
{
public:
    enum LogLevel
    {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        NUM_LOG_LEVELS,
    };

    //compile time calculation of basename of source file
    class SourceFile
    {
    public:
        template <int N>
        inline SourceFile(const char (&arr)[N])
            : m_data(arr),
              m_size(N - 1)
        {
            const char *slash = strrchr(m_data, '/'); // strrchr：查找到/符号最后一次出现的位置，并且将该符号之后的位置给data_指针
            if (slash)
            {
                m_data = slash + 1;
                m_size -= static_cast<int>(m_data - arr);
            }
        }

        explicit SourceFile(const char *filename)
            : m_data(filename)
        {
            const char *slash = strrchr(filename, '/');
            if (slash)
            {
                m_data = slash + 1;
            }
            m_size = static_cast<int>(strlen(m_data));
        }

        const char *m_data;
        int m_size;
    };

    Logger(SourceFile file, int line);
    Logger(SourceFile file, int line, LogLevel level);
    Logger(SourceFile file, int line, LogLevel level, const char *func);
    Logger(SourceFile file, int line, bool toAbort);
    ~Logger();

    static void setLogLevel(LogLevel level);
    static LogLevel logLevel();

    LogStream &stream() { return m_impl.m_stream; }

    typedef void (*outputFunc)(const char *msg, int len);
    typedef void (*flushFunc)();

    static void setOutput(outputFunc);
    static void setFlush(flushFunc);

private:
    Logger(const Logger &lg); //no copyable
    Logger &operator=(const Logger &lg);

    class Impl
    {
    public:
        typedef Logger::LogLevel LogLevel;
        Impl(LogLevel level, int old_errno, const SourceFile &file, int line);
        void formatTime();
        void finish();

        TimeStamp m_time;          //当前的时间
        LogStream m_stream;        //日志缓冲区，及通过流的方式，也就是使用<<符号，对缓冲区进行操作
        LogLevel m_level;          //日志等级
        int m_line;                //logger类所在行数，一般为__LINE__
        SourceFile m_fileBaseName; //logger类源文件名称，一般为__FILE__
    };

    Impl m_impl;
};

const char *strerror_tl(int savedErrno);

#endif