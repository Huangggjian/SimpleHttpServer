#include "HttpResponse.hpp"
#include <Buffer.hpp>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "async_logging"
#include "SocketHelp.hpp"
using namespace muduo;
//using namespace http;

void HttpResponse::appendToBuffer(Buffer *output, bool ifPic) const
{
    char buf[32];
    snprintf(buf, sizeof buf, "HTTP/1.1 %d ", statusCode_);
    output->append(buf);
    output->append(statusMessage_);
    output->append("\r\n");

    if (closeConnection_)
    {
        output->append("Connection: close\r\n");
    }
    else
    {
        snprintf(buf, sizeof buf, "Content-Length: %zd\r\n", m_bodyLen);
        output->append(buf);
        output->append("Connection: Keep-Alive\r\n");
    }

    for (const auto &header : headers_)
    {
        output->append(header.first);
        output->append(": ");
        output->append(header.second);
        output->append("\r\n");
    }

    output->append("\r\n");
    if (!ifPic)
        output->append(body_, m_bodyLen);
}

void HttpResponse::setBodypic(const std::string &_fileName, Buffer *output)
{
    struct stat statbuf;
    stat(_fileName.c_str(), &statbuf);
    m_bodyLen = statbuf.st_size;

    appendToBuffer(output, true);

    int filefd = open(_fileName.c_str(), O_RDONLY);

    const size_t SIZE = 5000;
    char buf[SIZE];

    memset(buf, 0, SIZE * sizeof(char));

    size_t read_len = sockets::read(filefd, buf, SIZE);
    size_t totalLen = 0;
    while (read_len > 0)
    {
        output->append(buf, read_len);
        totalLen += read_len;
        memset(buf, 0, SIZE * sizeof(char));
        while ((read_len = sockets::read(filefd, buf, SIZE)) < 0)
        {
            if (errno == EAGAIN)
            {
                continue;
            }
            else
            {
                LOG_ERROR << "Read file error";
                break;
            }
        }
    }
    assert(totalLen == m_bodyLen);
    assert(read_len == 0);
    sockets::close(filefd);
}