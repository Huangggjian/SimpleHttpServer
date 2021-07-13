
#include "Buffer.hpp"
#include "SocketHelp.hpp"

/*#include <sys/uio.h>
#include <iostream>*/

using namespace muduo;

const ssize_t kExtraBufferSize = 10240; //栈空间10k

const char Buffer::kCRLF[] = "\r\n";

const size_t Buffer::kCheapPrepend;
const size_t Buffer::kInitialSize;

ssize_t Buffer::readFdLT(int fd, int *savedErrno)
{
    // saved an ioctl()/FIONREAD call to tell how much to read
    char extrabuf[kExtraBufferSize];
    struct iovec vec[2];
    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + m_writerIndex;
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;
    // when there is enough space in this buffer, don't read into extrabuf.
    // when extrabuf is used, we read 128k-1 bytes at most.
    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = sockets::readv(fd, vec, iovcnt);
    //printf("Buffer::readFd() : len writable %d len %d capcity %d\n", writable, n - writable, internalCapacity());
    if (n < 0)
    {
        *savedErrno = errno;
    }
    else if (static_cast<size_t>(n) <= writable)
    {
        m_writerIndex += n;
    }
    else
    {
        m_writerIndex = m_buffer.size(); //缓冲区不够了，要加上栈空间
        append(extrabuf, n - writable);  //
    }

    return n;
}
// 支持ET模式下缓冲区的数据读取
ssize_t Buffer::readFdET(int fd, int *savedErrno)
{
    char extrabuf[kExtraBufferSize];
    struct iovec vec[2];

    size_t writable = writableBytes();
    vec[0].iov_base = begin() + m_writerIndex;
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;
    int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    ssize_t readLen = 0;
    // 不断调用read读取数据
    for (;;)
    {
        ssize_t n = sockets::readv(fd, vec, iovcnt);
        if (n < 0)
        {
            if (errno == EAGAIN)
            {
                //读完了
                *savedErrno = errno;
                break;
            }
            //读出错
            return -1;
        }
        else if (n == 0)
        {
            // 没有读取到数据，认为对端已经关闭
            return 0;
        }
        else if (static_cast<size_t>(n) <= writable)
        {
            // 还没有写满缓冲区
            m_writerIndex += n;
        }
        else
        {
            // 已经写满缓冲区, 则需要把剩余的buf写进去
            m_writerIndex = m_buffer.size();
            append(extrabuf, n - writable);
        }

        // 写完后需要更新 vec[0] 便于下一次读入
        writable = writableBytes();
        vec[0].iov_base = begin() + m_writerIndex;
        vec[0].iov_len = writable;
        iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
        readLen += n;
    }
    return readLen;
}

ssize_t Buffer::writeFdLT(int fd)
{
    // 从可读位置开始读取
    ssize_t n = sockets::write(fd, peek(), readableBytes());
    if (n > 0)
    {
        retrieve(n);
    }

    //(void *)savedErrno;
    return n;
}

// ET 模式下处理写事件
ssize_t Buffer::writeFdET(int fd)
{
    ssize_t writesum = 0;

    for (;;)
    {
        ssize_t n = sockets::write(fd, peek(), readableBytes());
        if (n > 0)
        {
            writesum += n;
            retrieve(n); // 更新可读索引
            if (readableBytes() == 0)
            {
                return writesum;
            }
        }
        else if (n < 0)
        {
            if (errno == EAGAIN) //系统缓冲区满，非阻塞返回
            {
                break;
            }
            // 暂未考虑其他错误
            else
            {
                return -1;
            }
        }
        else
        {
            // 返回0的情况，查看write的man，可以发现，一般是不会返回0的
            return writesum;
        }
    }
    return writesum;
}