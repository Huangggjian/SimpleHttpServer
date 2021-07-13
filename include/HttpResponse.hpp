#pragma once

#include <map>
#include "async_logging"
namespace muduo
{

}

namespace muduo
{
    class Buffer;
    class HttpResponse
    {
    public:
        enum HttpStatusCode
        {
            kUnknown,
            k200Ok = 200,
            k301MovedPermanently = 301,
            k400BadRequest = 400,
            k404NotFound = 404,
        };

        explicit HttpResponse(bool close)
            : statusCode_(kUnknown),
              m_bodyLen(0),
              closeConnection_(close)
        {
        }

        void setStatusCode(HttpStatusCode code)
        {
            statusCode_ = code;
        }

        void setStatusMessage(const std::string &message)
        {
            statusMessage_ = message;
        }

        void setCloseConnection(bool on)
        {
            closeConnection_ = on;
        }

        bool closeConnection() const
        {
            return closeConnection_;
        }

        void setContentType(const std::string &contentType)
        {
            addHeader("Content-Type", contentType);
        }

        // FIXME: replace std::string with std::stringPiece
        void addHeader(const std::string &key, const std::string &value)
        {
            headers_[key] = value;
        }
        const std::string &getContentType()
        {
            return headers_["Content-Type"];
        }
        // void setBody(const std::string &body)
        // {
        //     body_ = body;
        // }
        void setBody(const char *body, int body_len) //
        {
            body_ = body;

            m_bodyLen = body_len;
        }
        void setBodypic(const std::string &fileName, Buffer *output); //图片解析

        void appendToBuffer(muduo::Buffer *output, bool isPic = false) const;
        //void appendToBufferPic(Buffer *output) const;

    private:
        std::map<std::string, std::string> headers_;
        HttpStatusCode statusCode_;
        // FIXME: add http version
        std::string statusMessage_;
        bool closeConnection_; //判断是长连接还是短连接！
        //std::string body_;
        const char *body_;
        size_t m_bodyLen;
    };

} // namespace muduo
