#ifndef _NET_BUFFER_H
#define _NET_BUFFER_H

#include <algorithm>
#include <vector>
#include <string>

#include <assert.h>
#include <string.h>

namespace muduo
{
    class Buffer
    {
    public:
        static const size_t kCheapPrepend = 8;
        static const size_t kInitialSize = 4096;

        explicit Buffer(size_t initialSize = kInitialSize)
            : m_buffer(kCheapPrepend + initialSize),
              m_readerIndex(kCheapPrepend),
              m_writerIndex(kCheapPrepend)
        {
            assert(readableBytes() == 0);
            assert(writableBytes() == initialSize);
            assert(prependableBytes() == kCheapPrepend);
        }

        size_t readableBytes() const
        {
            return m_writerIndex - m_readerIndex;
        }

        size_t writableBytes() const
        {
            return m_buffer.size() - m_writerIndex;
        }

        size_t prependableBytes() const //前面预留出来的字节数，(s-(s-rI));
        {
            return m_readerIndex;
        }

        const char *peek() const
        {
            return begin() + m_readerIndex;
        }

        const char *beginWrite() const
        {
            return begin() + m_writerIndex;
        }

        char *beginWrite()
        {
            return begin() + m_writerIndex;
        }

        void hasWritten(size_t len) //写指针后移（写了几个往后移动几个）
        {
            assert(len <= writableBytes());
            m_writerIndex += len;
        }

        const char *findCRLF() const //可读数据部分有没有"\r\n", 有的话返回地址
        {
            // FIXME: replace with memmem()?
            const char *crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
            return crlf == beginWrite() ? NULL : crlf;
        }

        void unwrite(size_t len)
        {
            assert(len <= readableBytes());
            m_writerIndex -= len;
        }

        // retrieve returns void, to prevent
        // string str(retrieve(readableBytes()), readableBytes());
        // the evaluation of two functions are unspecified
        void retrieve(size_t len) //读出去len个字节
        {
            assert(len <= readableBytes());
            if (len < readableBytes())
            {
                m_readerIndex += len;
            }
            else
            {
                retrieveAll(); //全部读完，初始化一下
            }
        }

        void retrieveUntil(const char *end) //读出去 peek~end之间的数据
        {
            assert(peek() <= end);
            assert(end <= beginWrite());
            retrieve(end - peek());
        }

        void retrieveAll()
        {
            m_readerIndex = kCheapPrepend;
            m_writerIndex = kCheapPrepend;
        }

        std::string retrieveAsString(size_t len)
        {
            assert(len <= readableBytes());
            std::string result(peek(), len);
            retrieve(len);
            return result;
        }

        void makeSpace(int len)
        {
            if (writableBytes() + prependableBytes() < len + kCheapPrepend) //不够写的
            {
                m_buffer.resize(m_writerIndex + len);
            }
            else
            {
                // move readable data to the front, make space inside buffer
                assert(kCheapPrepend < m_readerIndex);
                size_t readable = readableBytes();
                std::copy(begin() + m_readerIndex, begin() + m_writerIndex, begin() + kCheapPrepend); //可读字段前移，放到kCheapPrepend开始的位置
                m_readerIndex = kCheapPrepend;
                m_writerIndex = m_readerIndex + readable;
                assert(readable == readableBytes());
            }
        }

        void append(const std::string &str) //往缓冲区写入数据
        {
            append(str.c_str(), str.size());
        }

        void append(const char *data /*restrict data*/, size_t len) //往缓冲区写入数据
        {
            if (writableBytes() < len)
            {
                makeSpace(len);
            }
            assert(writableBytes() >= len);
            std::copy(data, data + len, beginWrite());
            hasWritten(len);
        }

        ssize_t readFdLT(int fd, int *savedErrno);
        ssize_t readFdET(int fd, int *savedErrno);
        ssize_t writeFdLT(int fd);
        ssize_t writeFdET(int fd);
        size_t internalCapacity() const
        {
            return m_buffer.capacity();
        }

    private:
        char *begin()
        {
            return &*m_buffer.begin();
        }

        const char *begin() const
        {
            return &*m_buffer.begin();
        }

    private:
        static const char kCRLF[];

        std::vector<char> m_buffer;
        size_t m_readerIndex;
        size_t m_writerIndex;
    };

} // namespace muduo

#endif // _NET_BUFFER_H
