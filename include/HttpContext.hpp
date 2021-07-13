#ifndef MUDUO_NET_HTTP_HTTPCONTEXT_H
#define MUDUO_NET_HTTP_HTTPCONTEXT_H

#include <HttpRequest.hpp>
#include <list>
#include <memory>

namespace muduo
{

    class Buffer;
    class TcpConnection;

    class HttpContext // : public copyable
    {
    public:
        enum HttpRequestParseState
        {
            kExpectRequestLine,
            kExpectHeaders,
            kExpectBody,
            kGotAll,
        };

        HttpContext()
            : state_(kExpectRequestLine)

        {
        }

        // default copy-ctor, dtor and assignment are fine

        // return false if any error
        bool parseRequest(Buffer *buf, TimeStamp receiveTime);

        bool gotAll() const
        {
            return state_ == kGotAll;
        }

        void reset()
        {
            state_ = kExpectRequestLine;
            HttpRequest dummy;
            request_.swap(dummy);
        }

        const HttpRequest &request() const
        {
            return request_;
        }

        HttpRequest &request()
        {
            return request_;
        }

        void setClose(bool close)
        {
            close_ = close;
        }

        bool getClose() const
        {
            return close_;
        }

    private:
        bool processRequestLine(const char *begin, const char *end);

        HttpRequestParseState state_;
        HttpRequest request_;

        bool close_;
    };

    //  } // namespace net
} // namespace muduo

#endif // MUDUO_NET_HTTP_HTTPCONTEXT_H
