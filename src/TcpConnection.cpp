#include <assert.h>

#include "TcpConnection.hpp"
#include "EventLoop.hpp"
#include "Logger.hpp"
#include "SocketHelp.hpp"
// #include "WeakCallback.hpp"
#include "HttpContext.hpp"
#include <signal.h>
using namespace muduo;

TcpConnection::Method_ METHOD_ = TcpConnection::LT; //初始化
void TcpConnection::setConnMethod(Method_ m)
{
    METHOD_ = m;
}

TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &name,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : p_loop(loop),
      m_name(name),
      m_state(kConnecting),
      p_socket(new Socket(sockfd)),
      p_channel(new Channel(p_loop, sockfd)),
      m_localAddr(localAddr),
      m_peerAddr(peerAddr)
//m_lastTime(lastTime)
{ ///////////////////////////////////////////////////////////////////////////////

    if (METHOD_ == LT)
    {
        p_channel->setReadCallBack(std::bind(&TcpConnection::handleReadLT, this));   //连接可读事件发生
        p_channel->setWriteCallBack(std::bind(&TcpConnection::handleWriteLT, this)); //连接可写事件发生
    }

    else
    {
        //                                                 //设置ET模式
        p_channel->setReadCallBack(std::bind(&TcpConnection::handleReadET, this));   //连接可读事件发生
        p_channel->setWriteCallBack(std::bind(&TcpConnection::handleWriteET, this)); //连接可写事件发生
        //p_channel->enableEpollET();
    }

    p_channel->setCloseCallBack(std::bind(&TcpConnection::forceClose, this));  //连接关闭事件发生
    p_channel->setErrorCallBack(std::bind(&TcpConnection::handleError, this)); //连接错误事件发生

    LOG_DEBUG << "TcpConnection::ctor[" << m_name << "] at " << this
              << " fd=" << sockfd;

    p_socket->setKeepAlive(true);
    //connectEstablished();  do not in Constructor call shared_from_this();
}

TcpConnection::~TcpConnection()
{
    LOG_DEBUG << "TcpConnection::dtor[" << m_name << "] at "
              << this << " fd=" << p_channel->fd()
              << " state=" << stateToString();

    assert(m_state == kDisConnected);
}

void TcpConnection::connectEstablished()
{
    LOG_TRACE << "TcpConnection::connectEstablished()";
    int64_t now = TimeStamp::now_t();
    setTime(now);
    p_loop->assertInLoopThread();
    assert(m_state == kConnecting);
    setState(kConnected);
    LOG_TRACE << "[3] usecount=" << shared_from_this().use_count();
    p_channel->tie(shared_from_this());
    /////////////////////////////////////////////////////////////////////////////////////
    if (METHOD_ == ET)
        p_channel->enableEpollET();
    p_channel->enableReading();

    LOG_TRACE << m_localAddr.toIpPort() << " -> "
              << m_peerAddr.toIpPort() << " is "
              << (isConnected() ? "UP" : "DOWN");
    //HttpServer注册的。连接到来时要干嘛（打印一句话）
    if (m_connectionCallBack)
    {
        m_connectionCallBack(shared_from_this());
    }

    LOG_TRACE << "[4] usecount=" << shared_from_this().use_count();
}

void TcpConnection::send(const void *message, size_t len)
{
    if (m_state == kConnected)
    {
        if (p_loop->isInloopThread())
        {

            sendInLoop(message, len);
        }
        else
        {

            p_loop->runInLoop(std::bind(&TcpConnection::sendInLoop, this, message, len));
        }
    }
}

void TcpConnection::send(const std::string &message)
{
    if (m_state == kConnected)
    {
        if (p_loop->isInloopThread())
        {
            sendInLoop(message.data(), message.size());
        }
        else
        {

            p_loop->runInLoop(std::bind(&TcpConnection::sendInLoop, this, message.data(), message.size()));
        }
    }
}

void TcpConnection::send(Buffer *message)
{
    if (m_state == kConnected)
    {
        if (p_loop->isInloopThread())
        {
            // 跑得这段！
            sendInLoop(message->peek(), message->readableBytes());
            message->retrieveAll();
        }
        else
        {
            //assert(0);
            p_loop->runInLoop(std::bind(&TcpConnection::sendInLoop, this, message->peek(), message->readableBytes()));
            message->retrieveAll();
        }
    }
}

void TcpConnection::sendInLoop(const void *data, size_t len)
{
    LOG_WARN << data;
    p_loop->assertInLoopThread();
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if (m_state == kDisConnected)
    {
        LOG_WARN << "disconnected, give up writing.";
        return;
    }

    // 通道没有关注可写事件并且发送缓冲区没有数据，直接write
    if (!p_channel->isWriting() && m_outputBuffer.readableBytes() == 0)
    {
        nwrote = sockets::write(p_channel->fd(), data, len);
        if (nwrote >= 0)
        {
            remaining = len - nwrote;
            if (remaining == 0) //&& m_writeCompleteCallBack)
            {
                LOG_TRACE << "TcpConnection::sendInLoop() writeCompleteCallBack()";
                //fixd need
                //p_loop->queueInLoop(std::bind())
            }
        }
        else // nwrote < 0
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK) // EAGAIN
            {
                LOG_SYSERR << "TcpConnection::sendInLoop()";
                if (errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
    }

    assert(remaining <= len);
    // 没有错误，并且还有未写完的数据（说明内核发送缓冲区满，要将未写完的数据添加到output buffer中）
    if (!faultError && remaining > 0)
    {
        m_outputBuffer.append(static_cast<const char *>(data) + nwrote, remaining);
        if (!p_channel->isWriting())
        {
            //关注EPOLLOUT事件
            p_channel->enableWriting();
        }
    }
}
//关闭连接
void TcpConnection::shutdown()
{
    if (m_state == kConnected)
    {
        setState(kDisConnecting);
        p_loop->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}
//在loop中关闭写半边，还是可以读数据
void TcpConnection::shutdownInLoop()
{
    p_loop->assertInLoopThread();
    if (!p_channel->isWriting())
    {
        p_socket->shutdownWrite();
    }
}

void TcpConnection::handleReadLT()
{
    //LOG_TRACE << "TcpConnection::handleRead()";

    int savedErrno = 0;
    //直接将数据读到inputBuffer_缓冲区
    int64_t now = TimeStamp::now_t();
    setTime(now);
    ssize_t n = m_inputBuffer.readFdLT(p_channel->fd(), &savedErrno);

    if (n > 0)
    {
        //读到的数据解析
        m_messageCallBack(shared_from_this(), &m_inputBuffer, n);
    }
    else if (n == 0)
    {
        //如果读到的数据为0，就自动退出
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_SYSERR << "TcpConnection::handleRead()";
        handleError();
    }
}
void TcpConnection::handleReadET()
{
    int savedErrno = 0;
    int64_t now = TimeStamp::now_t();
    setTime(now);
    ssize_t n = m_inputBuffer.readFdET(p_channel->fd(), &savedErrno);

    if (n > 0)
    {
        m_messageCallBack(shared_from_this(), &m_inputBuffer, n);
    }
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_SYSERR << "TcpConnection::handleRead()";
        handleError();
    }
}
void TcpConnection::handleWriteLT()
{
    LOG_TRACE << "TcpConnection::handleWrite()";
    //int savedErrno = 0;
    int64_t now = TimeStamp::now_t();
    setTime(now);
    p_loop->assertInLoopThread();
    if (p_channel->isWriting()) //查看是否有写事件需要关注
    {
        // ssize_t n = sockets::write(p_channel->fd(), m_outputBuffer.peek(), m_outputBuffer.readableBytes());
        ssize_t n = m_outputBuffer.writeFdLT(p_channel->fd() /*, &savedErrno*/);
        if (n > 0)
        {
            //m_outputBuffer.retrieve(n);
            if (m_outputBuffer.readableBytes() == 0) //全部写完
            {
                p_channel->disableWriting(); // 停止关注POLLOUT事件，以免出现busy loop
                if (m_writecompleteCallBack)
                {
                    p_loop->queueInLoop(std::bind(m_writecompleteCallBack, shared_from_this()));
                }
                if (m_state == kDisConnecting) // 发送缓冲区已清空并且连接状态是kDisconnecting, 要关闭连接
                {
                    //几乎不会进入
                    shutdownInLoop(); //关闭写一半
                }
            }
            else
            {
                LOG_SYSERR << "TcpConnection::handleWrite";
            }
        }
    }
    else
    {
        LOG_ERROR << "Connection fd = " << p_channel->fd()
                  << " is down, no more writing";
    }
}
void TcpConnection::handleWriteET()
{
    LOG_TRACE << "TcpConnection::handleWrite()";
    //int savedErrno = 0;
    int64_t now = TimeStamp::now_t();
    setTime(now);
    p_loop->assertInLoopThread();
    if (p_channel->isWriting()) //查看是否有写事件需要关注
    {
        ssize_t n = m_outputBuffer.writeFdET(p_channel->fd() /*, &savedErrno*/);
        if (n > 0)
        {
            //m_outputBuffer.retrieve(n);
            if (m_outputBuffer.readableBytes() == 0) //全部写完
            {
                p_channel->disableWriting(); // 停止关注POLLOUT事件，以免出现busy loop
                if (m_writecompleteCallBack)
                {
                    p_loop->queueInLoop(std::bind(m_writecompleteCallBack, shared_from_this()));
                }
                if (m_state == kDisConnecting) // 发送缓冲区已清空并且连接状态是kDisconnecting, 要关闭连接
                {
                    //几乎不会进入
                    shutdownInLoop(); //关闭写一半
                }
            }
            else
            {
                LOG_SYSERR << "TcpConnection::handleWrite";
            }
        }
    }
    else
    {
        LOG_ERROR << "Connection fd = " << p_channel->fd()
                  << " is down, no more writing";
    }
}
void TcpConnection::handleError()
{
    int64_t now = TimeStamp::now_t();
    setTime(now);
    int err = sockets::getSocketError(p_channel->fd());
    LOG_ERROR << "TcpConnection::handleError [" << m_name
              << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}

void TcpConnection::handleClose()
{
    LOG_TRACE << "TcpConnection::handleClose()";
    p_loop->assertInLoopThread();
    LOG_TRACE << "fd = " << p_channel->fd() << " state = " << stateToString();

    assert(m_state == kConnected || m_state == kDisConnecting);

    setState(kDisConnected);
    p_channel->disableAll();

    TcpConnectionPtr guardThis(shared_from_this());

    LOG_TRACE << "[7] usecount=" << guardThis.use_count();
    m_connectionCallBack(guardThis);
    //在TcpServer::newConnetion注册，调用TcpServer::removeConnection函数
    m_closeCallBack(guardThis);
    //LOG_TRACE << "trace conn use " << name() << " used count " << guardThis.use_count();
    LOG_TRACE << "[11] usecount=" << guardThis.use_count();
}

//handleclose()中m_closeCallBack， 由TcpServer注册， 实际调TcpServer::removeConnection ==> TcpConnection::connectDestroyed
void TcpConnection::connectDestroyed()
{
    LOG_TRACE << "TcpConnection::connectDestroyed()";
    p_loop->assertInLoopThread();

    if (m_state == kConnected)
    {
        setState(kDisConnected);
        p_channel->disableAll();
        m_connectionCallBack(shared_from_this());
        LOG_TRACE << m_localAddr.toIpPort() << " -> "
                  << m_peerAddr.toIpPort() << " is "
                  << (isConnected() ? "UP" : "DOWN");
    }
    //p_channel->remove==>p_loop->removeChannel(this)==>m_poller->removeChannel(channel);
    //epoll_delete文件描述符，以及poller类map移除该fd对应channel
    p_channel->remove();
}

void TcpConnection::forceClose()
{
    LOG_TRACE << "TcpConnection::forceClose()";
    // FIXME: use compare and swap
    if (m_state == kConnected || m_state == kDisConnecting)
    {
        setState(kDisConnecting);
        p_loop->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
    }
}

void TcpConnection::forceCloseInLoop()
{
    p_loop->assertInLoopThread();
    if (m_state == kConnected || m_state == kDisConnecting)
    {
        // as if we received 0 byte in handleRead();
        handleClose();
    }
}
// void TcpConnection::forceCloseWithDelay(double seconds)
// {
//     if (m_state == kConnected || m_state == kDisConnecting)
//     {
//         setState(kDisConnecting);
//         p_loop->runAfter(
//             seconds,
//             makeWeakCallback(shared_from_this(),
//                              &TcpConnection::forceClose)); // not forceCloseInLoop to avoid race condition
//     }
// }
const char *TcpConnection::stateToString() const
{
    switch (m_state)
    {
    case kDisConnected:
        return "kDisConnected";
    case kConnecting:
        return "kConnecting";
    case kConnected:
        return "kConnected";
    case kDisConnecting:
        return "kDisConnecting";
    default:
        return "Unknown State";
    }
}