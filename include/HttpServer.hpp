#pragma once

#include "muduo_server"
#include "TimeStamp.hpp"
#include "TcpServer.hpp"

// #include "Atomic.hpp"
namespace muduo
{

    class HttpRequest;
    class HttpResponse;
    class HttpContext;

    class HttpServer
    {
    public:
        typedef std::function<void(const HttpRequest &,
                                   HttpResponse *)>
            HttpCallback;

        HttpServer(EventLoop *loop,
                   const InetAddress &listenAddr,
                   const std::string &name = "HuangJian's HttpSerVer");

        EventLoop *getLoop() const { return server_.getLoop(); }

        /// Not thread safe, callback be registered before calling start().
        void setHttpCallback(const HttpCallback &cb)
        {
            httpCallback_ = cb;
        }

        void setThreadNum(int numThreads)
        {
            server_.setThreadNum(numThreads);
        }

        void start();
        void onCheckTimer(TcpServer *server, double);
        //void dumpConnectionList() const;
        void execute_cgi(const char *, std::string &, const muduo::TcpConnectionPtr &tmp);

    private:
        void onConnection(const TcpConnectionPtr &conn);
        void onMessage(const muduo::TcpConnectionPtr &conn, muduo::Buffer *buffer, ssize_t len);
        void onRequest(const muduo::TcpConnectionPtr &conn, HttpRequest &req, HttpContext *context);
        TcpServer server_;
        HttpCallback httpCallback_;

        const double timeOut_;
    };

} // namespace muduo
