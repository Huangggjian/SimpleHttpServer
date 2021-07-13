#include <Buffer.hpp>
#include "HttpContext.hpp"

using namespace muduo;
bool HttpContext::processRequestLine(const char *begin, const char *end) //解析请求行
{
    bool succeed = false;
    const char *start = begin;
    const char *space = std::find(start, end, ' ');
    if (space != end && request_.setMethod(start, space))
    {
        start = space + 1;
        space = std::find(start, end, ' ');
        if (space != end)
        {
            const char *question = std::find(start, space, '?'); //url中的查询参数
            if (question != space)                               //
            {
                request_.setPath(start, question);
                request_.setQuery(question, space);
            }
            else
            {
                request_.setPath(start, space);
            }
            start = space + 1;
            succeed = end - start == 8 && std::equal(start, end - 1, "HTTP/1."); //HTTP/1.?
            if (succeed)
            {
                if (*(end - 1) == '1')
                {
                    request_.setVersion(HttpRequest::kHttp11);
                }
                else if (*(end - 1) == '0')
                {
                    request_.setVersion(HttpRequest::kHttp10);
                }
                else
                {
                    succeed = false;
                }
            }
        }
    }
    return succeed;
}

// return false if any error
bool HttpContext::parseRequest(muduo::Buffer *buf, TimeStamp receiveTime)
{
    bool ok = true;
    bool hasMore = true;
    int body_lenth;
    while (hasMore)
    {
        if (state_ == kExpectRequestLine) //解析请求行
        {
            const char *crlf = buf->findCRLF(); //buffer找\r\n(行末)
            if (crlf)
            {
                ok = processRequestLine(buf->peek(), crlf); //请求行数据拿去解析
                if (ok)
                {
                    request_.setReceiveTime(receiveTime);
                    buf->retrieveUntil(crlf + 2);
                    state_ = kExpectHeaders; //即将解析头部数据
                }
                else
                {
                    hasMore = false;
                }
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectHeaders)
        {
            const char *crlf = buf->findCRLF();
            if (crlf)
            {
                const char *colon = std::find(buf->peek(), crlf, ':');
                if (colon != crlf)
                {
                    request_.addHeader(buf->peek(), colon, crlf);
                }
                else
                {
                    // empty line, end of header
                    // FIXME:
                    // state_ = kGotAll; //全部完毕
                    // hasMore = false;
                    if (request_.getHeader("Content-Length").size() == 0)
                    {
                        state_ = kGotAll;
                        hasMore = false;
                    }
                    else
                    {
                        body_lenth = stoi(request_.getHeader("Content-Length"));
                        state_ = kExpectBody;
                    }
                }
                buf->retrieveUntil(crlf + 2);
            }
            else
            {
                hasMore = false;
            }
        }
        else if (state_ == kExpectBody)
        {
            // FIXME:
            // LOG_DEBUG << "buf->readableBytes(): " << buf->readableBytes();
            // LOG_DEBUG << "body_lenth: " << body_lenth;
            /// if not quit for next more data
            if (buf->readableBytes() >= static_cast<size_t>(body_lenth)) //has /r/n before body
            {
                const char *begin = buf->peek();
                const char *end = begin + static_cast<size_t>(body_lenth);

                request_.setBody(begin, end);
                state_ = kGotAll;
                hasMore = false;
            }
            else
            {
                hasMore = false;
            }
        }
    }

    return ok;
}
