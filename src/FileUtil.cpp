#include "FileUtil.hpp"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static off_t fileSize(const std::string &path);

FileUtil::AppendFile::AppendFile(StringArg filePath)
    : m_fp(::fopen(filePath.c_str(), "ae")), // 'e' for O_CLOEXEC
      m_writtenBytes(fileSize(filePath.c_str()))
{
    assert(m_fp);
    ::setbuffer(m_fp, m_buffer, sizeof(m_buffer)); //修改FILE结构体默认提供的缓冲区。主要原因可能是因为默认提供的buffer比较小。
}

FileUtil::AppendFile::~AppendFile()
{
    ::fclose(m_fp);
}

void FileUtil::AppendFile::append(const char *logline, const size_t len)
{
    size_t nread = write(logline, len);
    size_t remain = len - nread;
    while (remain > 0) //循环调用write成员函数，直到把logline的所有数据都写入去
    {
        size_t n = write(logline + nread, remain);
        if (0 == n)
        {
            int err = ferror(m_fp);
            if (err)
            {
                fprintf(stderr, "AppendFile::append failed : %s\n", strerror(err));
            }
            break;
        }
        nread += n;
        remain = len - nread;
    }

    m_writtenBytes += len;
}

size_t FileUtil::AppendFile::write(const char *logline, const size_t len)
{
    //为了快速，使用unlocked(无锁)的fwrite函数。平时我们使用的C语言IO函数，都是线程安全的，
    //为了做到线程安全，会在函数的内部加锁。这会拖慢速度。而对于这个类。可以保证，从
    //始到终只有一个线程能访问，所以无需进行加锁操作。
    return ::fwrite_unlocked(logline, 1, len, m_fp);
}

void FileUtil::AppendFile::flush()
{
    ::fflush(m_fp);
}

static off_t fileSize(const std::string &path) // get file size
{
    struct stat fileInfo;
    if (stat(path.c_str(), &fileInfo) < 0)
    {
        switch (errno)
        {
        case ENOENT:
            return 0;
        default:
            fprintf(stderr, "stat fileInfo failed : %s\n", strerror(errno));
            abort();
        }
    }
    else
    {
        return fileInfo.st_size;
    }
}