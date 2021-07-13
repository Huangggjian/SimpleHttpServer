#include <async_logging>
#include <muduo_server>

#include <HttpContext.hpp>
#include <HttpResponse.hpp>
// #include <iostream>
#include "HttpServer.hpp"
static AsyncLogging *g_asyncLog = NULL;
const int kRollSize = 1000 * 1024 * 1024; //1G回滚
using namespace muduo;
void asyncOutput(const char *msg, int len)
{
    g_asyncLog->append(msg, len);
}

int main(int argc, char *argv[])
{
    int threadNum = 4;
    int port = 8888;
    int opt;
    const char *str = "t:l:p:a:b:";
    std::string logPath = "WebServer";
    //Acceptor::setListenMethod(Acceptor::LT);
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 't':
        {
            threadNum = atoi(optarg);
            break;
        }
        case 'l':
        {
            logPath = optarg;
            break;
        }
        case 'p':
        {
            port = atoi(optarg);
            break;
        }
        case 'a':
        {
            if (atoi(optarg) == 0)
            {
                Acceptor::setListenMethod(Acceptor::LT);
            }
            else
            {
                Acceptor::setListenMethod(Acceptor::ET);
            }
            break;
        }
        case 'b':
        {
            if (atoi(optarg) == 0)
            {
                TcpConnection::setConnMethod(TcpConnection::LT);
            }
            else
            {
                TcpConnection::setConnMethod(TcpConnection::ET);
                //Acceptor::setListenMethod(Acceptor::ET);
            }
            break;
        }
        default:
            break;
        }
    }

    //指定日志文件名
    char name[256];
    strncpy(name, logPath.c_str(), 256);
    //初始化全局日志等级
    Logger::setLogLevel(Logger::TRACE);
    //构建异步日志类
    AsyncLogging log(::basename(name), kRollSize, 3.0);
    g_asyncLog = &log;
    //输出重定向
    Logger::setOutput(asyncOutput);
    //开启日志
    log.start();
    //baseloop
    muduo::EventLoop baseloop;
    //绑定端口
    InetAddress localAddr(port);
    //创建线程池
    muduo::EventLoopThreadPool pool(&baseloop);
    muduo::ex_event_loop_thread_pool.reset(&pool);
    //创建HttpServer
    muduo::HttpServer http_server(&baseloop, localAddr);
    //设置线程池线程数
    http_server.setThreadNum(threadNum);
    //开启线程池
    pool.start();
    //开启HttpSer
    http_server.start();
    //start loop
    baseloop.loop();
}
