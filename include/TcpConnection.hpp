#ifndef _NET_TCPCONNECTION_HH
#define _NET_TCPCONNECTION_HH

#include <memory>
#include <string>

#include "InetAddress.hpp"
#include "Socket.hpp"
#include "CallBacks.hpp"
#include "Channel.hpp"
#include "Buffer.hpp"
//#include "HttpContext.hpp"
#include <atomic>
// class HttpContext;
namespace muduo
{

    class TcpConnection : public std::enable_shared_from_this<TcpConnection>
    {

    public:
        enum Method_
        {
            ET,
            LT,
        };
        TcpConnection(EventLoop *loop,
                      const std::string &name,
                      int sockfd,
                      const InetAddress &localAddr,
                      const InetAddress &peerAddr);
        ~TcpConnection();

        EventLoop *getLoop() const { return p_loop; }
        const std::string &name() const { return m_name; }
        void setConnectionName(const std::string &name) { m_name = name; }
        void setConnectionCallBack(const NetCallBacks::ConnectionCallBack &cb) { m_connectionCallBack = cb; }
        void setMessageCallBack(const NetCallBacks::MessageCallBack &cb) { m_messageCallBack = cb; }
        void setCloseCallBack(const NetCallBacks::CloseCallBack &cb) { m_closeCallBack = cb; }

        const InetAddress &localAddress() const { return m_localAddr; }
        const InetAddress &peerAddress() const { return m_peerAddr; }

        // called when TcpServer accepts a new connection
        void connectEstablished(); // should be called only once
        // called when TcpServer has removed me from its map
        void connectDestroyed(); // should be called only once

        // void send(string&& message); // C++11
        void send(const void *message, size_t len);
        void send(const std::string &message);
        // void send(Buffer&& message); // C++11
        void send(Buffer *message); // this one will swap data

        void shutdown();
        void forceClose();
        //void forceCloseWithDelay(double seconds);
        bool isConnected() const { return m_state == kConnected; }
        bool isDisConnected() const { return m_state == kDisConnected; }
        const char *stateToString() const;
        // void setContext(HttpContext &context) { m_context = context; }
        // const HttpContext &getContext() const { return m_context; }
        // HttpContext *getMutableContext() { return &m_context; }
        int64_t getTime() { return m_lastTime.load(); }
        void setTime(int64_t t) { m_lastTime.store(t); }
        static void setConnMethod(Method_ m);

    private:
        enum StateE
        {
            kDisConnected,
            kConnecting,
            kDisConnecting,
            kConnected,
        };

        void setState(StateE s) { m_state = s; }
        void handleReadLT();
        void handleReadET();
        void handleWriteLT();
        void handleWriteET();
        void handleError();
        void handleClose();
        void sendInLoop(const void *data, size_t len);
        void shutdownInLoop();
        void forceCloseInLoop();

        EventLoop *p_loop;
        std::string m_name;
        StateE m_state;
        std::unique_ptr<Socket> p_socket;
        std::unique_ptr<Channel> p_channel;

        //TimeStamp m_lastTime;
        std::atomic_int64_t m_lastTime;
        InetAddress m_localAddr;
        InetAddress m_peerAddr;

        NetCallBacks::ConnectionCallBack m_connectionCallBack;
        NetCallBacks::MessageCallBack m_messageCallBack;
        NetCallBacks::CloseCallBack m_closeCallBack;
        NetCallBacks::WriteCompleteCallBack m_writecompleteCallBack;
        Buffer m_inputBuffer;
        Buffer m_outputBuffer;

    }; // namespace muduo

} // namespace muduo

#endif