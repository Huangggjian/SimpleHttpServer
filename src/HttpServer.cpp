//

#include "HttpServer.hpp"

#include "async_logging"
#include "HttpContext.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "TimeStamp.hpp"
#include <sys/types.h>
#include <sys/wait.h>
using namespace muduo;
// namespace muduo
// {
//     void execute_cgi(const char *path, std::string postdata, const muduo::TcpConnectionPtr &tmp);
// };
class HttpResponse;

namespace muduo
{
    //void execute_cgi(const char *path, std::string &postdata, const muduo::TcpConnectionPtr &tmp);
    namespace net
    {
        namespace detail
        {

            void defaultHttpCallback(const muduo::HttpRequest &, muduo::HttpResponse *resp)
            {
                resp->setStatusCode(HttpResponse::k404NotFound);
                resp->setStatusMessage("Not Found");
                resp->setCloseConnection(true);
            }

        } // namespace detail
    }     // namespace net
} // namespace muduo

HttpServer::HttpServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const std::string &name)
    : server_(loop, listenAddr, name),
      httpCallback_(muduo::net::detail::defaultHttpCallback),
      timeOut_(20.0)
{
    // time_out = 100;
    server_.setConnectionCallBack(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallBack(
        std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    loop->runEvery(10, std::bind(&HttpServer::onCheckTimer, this, &server_, timeOut_));
}

void HttpServer::start()
{
    LOG_WARN << "HttpServer[" << server_.name()
             << "] starts listenning on " << server_.ipPort();
    server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr &conn)
{
    if (conn->isConnected())
    {
        // HttpContext context;
        // conn->setContext(context);
        LOG_DEBUG << "new conn from " << conn->peerAddress().toIpPort();
    }
}

void HttpServer::onMessage(const muduo::TcpConnectionPtr &conn, muduo::Buffer *buffer, ssize_t len)
{
    LOG_DEBUG << "on message : " << len << " bytes " << buffer->peek();

    LOG_TRACE << "readAbleBytes:" << buffer->readableBytes() << " len:" << len;

    HttpContext context;

    if (!context.parseRequest(buffer, TimeStamp::now()))
    {
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
    }

    if (context.gotAll()) //解析请求行+头部
    {
        LOG_TRACE << "request version: " << context.request().getVersion();
        LOG_TRACE << "request method : " << context.request().methodString();
        LOG_TRACE << "request path   : " << context.request().path();
        onRequest(conn, context.request(), &context);
        context.reset();
    }

    LOG_TRACE << "readAbleBytes:" << buffer->readableBytes() << " len:" << len;
}

void HttpServer::onRequest(const muduo::TcpConnectionPtr &conn, HttpRequest &req, HttpContext *context)
{
    const std::string &connection = req.getHeader("Connection");
    bool close = connection == "close" || connection == "Close" ||
                 (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");
    context->setClose(close);
    HttpResponse response(close);
    muduo::Buffer buf;
    //response.setStatusCode(HttpResponse::k200Ok);
    if (req.path() == "/")
    {
        // 可以分块传输
        // const char *data =
        //     "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: Keep-Alive\r\nContent-Type: \\
        // text/html\r\nServer: Muduo\r\n\r\n28\r\n<html><head><title>This is title</title>";
        // conn->send((void *)data, strlen(data));
        // data = "4E\r\n</head><body><h1>Hello</h1><h2>This is the my chunk version</h2></body></html>";
        // conn->send((void *)data, strlen(data));
        // data = "0\r\n\r\n";
        // conn->send((void *)data, strlen(data));

        response.setStatusCode(HttpResponse::k200Ok);
        response.setStatusMessage("OK");
        response.setContentType("text/html");
        response.setBodypic("./httpdocs/hello.html", &buf);
        LOG_DEBUG << "http buffer :\n"
                  << buf.peek();
        conn->send(&buf);
    }
    else if (req.path() == "/pic")
    {
        response.setStatusCode(HttpResponse::k200Ok);
        response.setStatusMessage("OK");
        response.setContentType("image/png");
        response.setBodypic("./httpdocs/test.jpg", &buf);
        conn->send(&buf);
    }
    else if (req.path() == "/hello")
    {
        std::string body_s = "hello, world!\n";
        response.setStatusCode(HttpResponse::k200Ok);
        response.setStatusMessage("OK");
        response.setContentType("text/plain");
        response.addHeader("Server", "Muduo");
        response.setBody(body_s.c_str(), body_s.size());
        response.appendToBuffer(&buf);
        LOG_DEBUG << "http buffer :\n"
                  << buf.peek();
        conn->send(&buf);
    }
    else if (req.path() == "/post")
    {
        response.setStatusCode(HttpResponse::k200Ok);
        response.setStatusMessage("OK");
        response.setContentType("text/html");
        response.setBodypic("./httpdocs/post.html", &buf);
        LOG_DEBUG << "http buffer :\n"
                  << buf.peek();
        conn->send(&buf);
    }
    else if (req.path() == "/upload")
    {
        response.setStatusCode(HttpResponse::k200Ok);
        response.setStatusMessage("OK");
        response.setContentType("text/html");
        response.setBodypic("./httpdocs/upload.html", &buf);
        LOG_DEBUG << "http buffer :\n"
                  << buf.peek();
        conn->send(&buf);
    }
    else if (req.path().find("cgi") != std::string::npos) //cgi
    {
        //execute_cgi("httpdocs/post.cgi", _request.postdata, conn);
        std::string &body = req.getBody();
        //LOG_INFO << body;
        execute_cgi(req.path().substr(1).c_str(), body, conn);
    }
    else
    {
        response.setStatusCode(HttpResponse::k404NotFound);
        response.setStatusMessage("Not Found");
        response.setCloseConnection(true);
        response.appendToBuffer(&buf);
        LOG_DEBUG << "http buffer :\n"
                  << buf.peek();
        conn->send(&buf);
    }

    if (response.closeConnection())
    {
        conn->shutdown();
    }
}

void HttpServer::onCheckTimer(TcpServer *server, double time_out)
{

    int64_t now = TimeStamp::now_t();
    ConnectionsSet_t &connectionsSet = server->getConnectionSet();
    for (ConnectionsSet_t::iterator it = connectionsSet.begin();
         it != connectionsSet.end();)
    {
        TcpConnectionWeakPtr w_conn(*it);
        TcpConnectionPtr conn = w_conn.lock();
        if (conn)
        {
            double age = timeDifference_t(now, it->get()->getTime());
            if (age >= time_out) // already time_out
            {
                if (conn->isConnected())
                {
                    conn->shutdown();
                    LOG_INFO << "TimeOut Shutting Down " << conn->name();
                    conn->forceClose();
                }
            }
            else if (age < time_out)
            {
                LOG_INFO << "Time jump";
            }

            ++it;
        }
        else
        {
            LOG_WARN << "Expired";
            it = connectionsSet.erase(it);
        }
    }
}
void HttpServer::execute_cgi(const char *path, std::string &postdata, const muduo::TcpConnectionPtr &conn)
{
    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];
    ssize_t n;
    pid_t pid;
    int status;

    int content_length = postdata.size();

    sprintf(buf, "HTTP/1.1 200 OK\r\nServer: Tiny Web Server\r\nContent-type: text/html\r\n");

    if (pipe(cgi_output) < 0)
    {
        LOG_ERROR << "PIPE error!";
        return;
    }
    if (pipe(cgi_input) < 0)
    {
        LOG_ERROR << "PIPE error!";
        return;
    }

    if ((pid = fork()) < 0)
    {
        LOG_ERROR << "FORK error!";
        return;
    }
    if (pid == 0) /* 子进程: 运行CGI 脚本 */
    {
        char length_env[25];
        dup2(cgi_output[1], 1); //cgi_output写到标准输出
        dup2(cgi_input[0], 0);  //cgi_input读端从标准输入读
        close(cgi_output[0]);   //关闭了cgi_output中的读通道
        close(cgi_input[1]);    //关闭了cgi_input中的写通道
        sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
        putenv(length_env);

        execl(path, path, NULL);

        exit(0);
    }
    else //父进程
    {

        close(cgi_output[1]);
        close(cgi_input[0]);

        LOG_INFO << postdata;

        //LOG_INFO << len << " == " << content_length;
        write(cgi_input[1], postdata.c_str(), content_length);
        //assert(len == content_length);
        conn->send(buf, strlen(buf));
        while ((n = read(cgi_output[0], buf, sizeof buf)) > 0) //cgi_output[1]输入的数据（标准输出）
                                                               //发送给浏览器
        {
            conn->send(buf, n);
        }

        //运行结束关闭
        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0);
    }
}