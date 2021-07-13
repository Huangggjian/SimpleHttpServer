#include <assert.h>

#include "Acceptor.hpp"
#include "TcpServer.hpp"
#include "EventLoop.hpp"
#include "Logger.hpp"
#include "SocketHelp.hpp"

using namespace muduo;

std::unique_ptr<EventLoopThreadPool> muduo::ex_event_loop_thread_pool = nullptr; //给用户提供的接口，创建线程池后地址赋值给这个指针

void NetCallBacks::defaultConnectionCallback()
{
    LOG_TRACE << "defaultConnectionCallback ";
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &name)
    : p_loop(loop),
      m_name(name),
      p_acceptor(new Acceptor(loop, listenAddr)),
      m_nextConnId(1),
      m_ipPort(std::to_string(listenAddr.toPort()))
{
    p_acceptor->setNewConnectionCallBack(
        std::bind(&TcpServer::newConnetion, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
}

void TcpServer::start()
{
    assert(!p_acceptor->listenning());
    p_loop->runInLoop(
        std::bind(&Acceptor::listen, p_acceptor.get()));
}

void TcpServer::newConnetion(int sockfd, const InetAddress &peerAddr)
{
    LOG_TRACE << "TcpServer::newConnetion() ";
    p_loop->assertInLoopThread();

    char buf[64];
    snprintf(buf, sizeof buf, "#%d", m_nextConnId);
    ++m_nextConnId;
    std::string connName = m_name + buf;

    LOG_INFO << "TcpServer::newConnetion() [" << m_name
             << "] - new connection [" << connName
             << "] from " << peerAddr.toIpPort();

    InetAddress localAddr(sockets::getLocalAddr(sockfd));

    if (ex_event_loop_thread_pool)
    {
        EventLoop *loop;
        loop = ex_event_loop_thread_pool->getNextLoop();
        LOG_INFO << "Loop " << loop;
        //TimeStamp time = TimeStamp::now();
        TcpConnectionPtr conn(new TcpConnection(loop,
                                                connName, sockfd, localAddr, peerAddr));

        LOG_TRACE << "[1] usecount=" << conn.use_count(); //1
        m_connectionsSet.insert(conn);
        LOG_TRACE << "[2] usecount=" << conn.use_count(); //2
        conn->setConnectionCallBack(m_connectionCallBack);
        conn->setMessageCallBack(m_messageCallBack);
        conn->setCloseCallBack(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
        loop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
        LOG_TRACE << "[5] usecount=" << conn.use_count(); //3
    }
    else
    {
        //TimeStamp time = TimeStamp::now();
        TcpConnectionPtr conn(new TcpConnection(p_loop,
                                                connName, sockfd, localAddr, peerAddr));
        //m_connectionsMap[connName] = conn;
        m_connectionsSet.insert(conn);
        conn->setConnectionCallBack(m_connectionCallBack);
        conn->setMessageCallBack(m_messageCallBack);
        conn->setCloseCallBack(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
        conn->connectEstablished();
    }
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    // FIXME: unsafe
    //LOG_TRACE << "trace conn use " << conn->name() << " used count " << conn.use_count();
    p_loop->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
    //LOG_TRACE << "trace conn use " << conn->name() << " used count " << conn.use_count();
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    p_loop->assertInLoopThread();
    LOG_INFO << "TcpServer::removeConnectionInLoop [" << m_name
             << "] - connection " << conn->name();

    //LOG_TRACE << "trace conn use " << conn->name() << " used count " << conn.use_count();
    LOG_TRACE << "[8] usecount=" << conn.use_count();
    size_t n = m_connectionsSet.erase(conn);
    LOG_TRACE << "[9] usecount=" << conn.use_count(); //1
    (void)n;
    assert(n == 1);
    //LOG_TRACE << "trace conn use " << conn->name() << " used count " << conn.use_count();

    if (m_closeCallBack) //由HttpServer注册，一般不注册
    {
        m_closeCallBack(conn);
    }

    //LOG_TRACE << "trace conn use " << conn->name() << " used count " << conn.use_count();

    //assert(conn.use_count() == 2);

    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));
    LOG_TRACE << "[10] usecount=" << conn.use_count();
}
