#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "TimeStamp.hpp"
#include "Logger.hpp"

__thread char t_time[64];
__thread time_t t_lastSecond;
__thread char t_errnobuf[512];

static const char black[] = {0x1b, '[', '1', ';', '3', '0', 'm', 0};
static const char red[] = {0x1b, '[', '1', ';', '3', '1', 'm', 0};
static const char green[] = {0x1b, '[', '1', ';', '3', '2', 'm', 0};
static const char yellow[] = {0x1b, '[', '1', ';', '3', '3', 'm', 0};
static const char blue[] = {0x1b, '[', '1', ';', '3', '4', 'm', 0};
static const char purple[] = {0x1b, '[', '1', ';', '3', '5', 'm', 0};
static const char normal[] = {0x1b, '[', '0', ';', '3', '9', 'm', 0};

const char *strerror_tl(int savedErrno) //根据错误号savedErrno，可以得到具体的错误代码存储在t_errnobuf数组中
{
    return strerror_r(savedErrno, t_errnobuf, sizeof(t_errnobuf));
}

Logger::LogLevel g_logLevel = Logger::TRACE; //初始化全局日志级别

void Logger::setLogLevel(LogLevel level)
{
    g_logLevel = level;
}

Logger::LogLevel Logger::logLevel()
{
    return g_logLevel;
}

const char *LogLevelName[Logger::NUM_LOG_LEVELS] =
    {
        "[TRACE]",
        "[DEBUG]",
        "[INFO ]",
        "[WARN ]",
        "[ERROR]",
        "[FATAL]",
};

// helper class for known string length at compile time
//用一个指针存储一个字符串，再用一个长度存取字符串长度
// class T
// {
// public:
//     T(const char *str, unsigned len)
//         : m_str(str),
//           m_len(len)
//     {
//         assert(strlen(str) == m_len);
//     }

//     const char *m_str;
//     const unsigned m_len;
// };

void defaultOutput(const char *msg, int len) //默认向标准输出写
{
    size_t n = fwrite(msg, 1, len, stdout); //数据源地址，每个单元的字节数，单元个数，文件流指针
    (void)n;
}

void defaultFlush()
{
    fflush(stdout);
}

Logger::outputFunc g_output = defaultOutput;
Logger::flushFunc g_flush = defaultFlush;

void Logger::setOutput(outputFunc out)
{
    g_output = out;
}

void Logger::setFlush(flushFunc flush)
{
    g_flush = flush;
}

Logger::Logger(SourceFile file, int line)
    : m_impl(INFO, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, LogLevel level)
    : m_impl(level, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, bool toAbort)
    : m_impl(toAbort ? FATAL : ERROR, errno, file, line)
{
}

Logger::Logger(SourceFile file, int line, LogLevel level, const char *func)
    : m_impl(level, 0, file, line)
{
    m_impl.m_stream << '[' << func << "] ";
}

Logger::~Logger()
{
    m_impl.finish();
    const LogStream::Buffer &buf(stream().buffer());
    g_output(buf.data(), buf.length());
    if (m_impl.m_level == FATAL)
    {
        g_flush();
        abort();
    }
}

Logger::Impl::Impl(LogLevel level, int savedErrno, const SourceFile &file, int line)
    : m_time(TimeStamp::now()),
      m_stream(),
      m_level(level),
      m_line(line),
      m_fileBaseName(file)
{
    formatTime(); //输出时间

    switch (level) //输出  ： [等级]
    {
    case TRACE:
        m_stream << green << LogLevelName[level] << normal << ' ';
        break;
    case DEBUG:
        m_stream << blue << LogLevelName[level] << normal << ' ';
        break;
    case INFO:
        m_stream << black << LogLevelName[level] << normal << ' ';
        break;
    case WARN:
        m_stream << yellow << LogLevelName[level] << normal << ' ';
        break;
    case ERROR:
        m_stream << purple << LogLevelName[level] << normal << ' ';
        break;
    case FATAL:
        m_stream << red << LogLevelName[level] << normal << ' ';
        break;
    default:
        m_stream << LogLevelName[level] << ' ';
        break;
    }

    //输出所在的文件名和行号
    m_stream << '[' << m_fileBaseName.m_data << ':' << m_line << "] ";
    if (savedErrno != 0)
    {
        m_stream << strerror_tl(savedErrno) << " (errno=" << savedErrno << ") ";
    }
}

void Logger::Impl::finish() //向缓冲区中输入结束符
{
    m_stream << '\n';
}

void Logger::Impl::formatTime() //组织时间格式，年月日 小时:分钟：秒输出到缓冲区
{
    int64_t microSecondsSinceEpoch = m_time.microSecondsSinceEpoch();
    time_t seconds = static_cast<time_t>(microSecondsSinceEpoch / TimeStamp::kMicroSecondsPerSecond);
    int microseconds = static_cast<int>(microSecondsSinceEpoch % TimeStamp::kMicroSecondsPerSecond);
    if (seconds != t_lastSecond)
    {
        t_lastSecond = seconds;
        struct tm tm_time;

        ::gmtime_r(&seconds, &tm_time); // FIXME TimeZone::fromUtcTime
                                        //tm结构计算年就是从1900年开始算起
        int len = snprintf(t_time, sizeof(t_time), "%4d-%02d-%02d %02d:%02d:%02d",
                           tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                           tm_time.tm_hour + 8, tm_time.tm_min, tm_time.tm_sec);
        assert(len == 19);
        (void)len;
    }

    Fmt us(".%06d ", microseconds);
    assert(us.length() == 8);
    m_stream << t_time << us.data();
}
