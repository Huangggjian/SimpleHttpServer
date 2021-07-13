#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <functional>

#include "SocketHelp.hpp"
#include "InetAddress.hpp"
#include "EventLoop.hpp"
#include "Logger.hpp"
#include "Acceptor.hpp"

using namespace muduo;
Acceptor::Method METHOD = Acceptor::LT; //设置全局默认LT模式
void Acceptor::setListenMethod(Method method)
{
    METHOD = method;
}
Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : p_loop(loop),
      m_acceptSocket(sockets::createNonblockingOrDie(listenAddr.family())),
      m_acceptChannel(loop, m_acceptSocket.fd()),         //构造Accept所在类的Channel
      m_listenning(false),                                //初始值listening: false
      m_idleFd(::open("/dev/null", O_RDONLY | O_CLOEXEC)) //占位
{
    assert(m_idleFd >= 0);

    m_acceptSocket.setReuseAddr(reuseport);
    m_acceptSocket.bindAddress(listenAddr);
    if (METHOD == LT)
    {
        m_acceptChannel.setReadCallBack(
            std::bind(&Acceptor::handleReadLT, this));
    }

    else
    {

        m_acceptChannel.setReadCallBack(
            std::bind(&Acceptor::handleReadET, this));
    }
    //
}

Acceptor::~Acceptor()
{
    m_acceptChannel.disableAll();
    m_acceptChannel.remove();
    ::close(m_idleFd);
}

void Acceptor::listen()
{
    p_loop->assertInLoopThread();
    m_listenning = true;
    if (METHOD == ET)
        m_acceptChannel.enableEpollET(); //监听套接字设置为ET模式
    m_acceptSocket.listen();
    ///////////////////////////////////////

    m_acceptChannel.enableReading();
}

//监听套接字LT模式下用的可读事件
void Acceptor::handleReadLT()
{
    LOG_TRACE << "Acceptor::handleRead()";

    p_loop->assertInLoopThread();
    InetAddress peerAddr;
    int connfd = m_acceptSocket.accept(&peerAddr);
    if (connfd >= 0)
    {
        if (m_newConnectionCallBack)
        {
            m_newConnectionCallBack(connfd, peerAddr);
        }
        else
        {
            sockets::close(connfd);
        }
    }
    else
    {
        LOG_SYSERR << "in Acceptor::handleRead";
        if (errno == EMFILE)
        {
            ::close(m_idleFd);
            m_idleFd = ::accept(m_acceptSocket.fd(), NULL, NULL);
            ::close(m_idleFd);
            m_idleFd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}

void Acceptor::handleReadET()
{
    LOG_TRACE << "Acceptor::handleRead()";
    for (;;)
    {
        p_loop->assertInLoopThread();
        InetAddress peerAddr;
        int connfd = m_acceptSocket.accept(&peerAddr);
        if (connfd >= 0)
        {
            if (m_newConnectionCallBack)
            {
                m_newConnectionCallBack(connfd, peerAddr);
            }
            else
            {
                sockets::close(connfd);
            }
        }
        else
        {
            if (errno == EMFILE)
            {
                ::close(m_idleFd);
                m_idleFd = ::accept(m_acceptSocket.fd(), NULL, NULL);
                ::close(m_idleFd);
                m_idleFd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
            }
            break;
        }
    }
}
