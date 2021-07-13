#ifndef _NET_TCPSERVER_HH
#define _NET_TCPSERVER_HH

#include <set>
#include <memory>
#include <functional>

#include "CallBacks.hpp"
#include "Acceptor.hpp"
#include "TcpConnection.hpp"
#include "EventLoopThreadPool.hpp"

namespace muduo
{

    class EventLoop;

    extern std::unique_ptr<muduo::EventLoopThreadPool> ex_event_loop_thread_pool;
    typedef std::set<TcpConnectionPtr> ConnectionsSet_t;
    class TcpServer
    {
    public:
        TcpServer(EventLoop *loop, const InetAddress &listenAddr, const std::string &name = "Serv ");
        ~TcpServer();

        void start();
        EventLoop *getLoop() const { return p_loop; }
        const std::string &ipPort() const { return m_ipPort; }
        const std::string &name() const { return m_name; }
        void setConnectionCallBack(const NetCallBacks::ConnectionCallBack &cb) { m_connectionCallBack = cb; }
        void setMessageCallBack(const NetCallBacks::MessageCallBack &cb) { m_messageCallBack = cb; }
        void setCloseCallBack(const NetCallBacks::CloseCallBack &cb) { m_closeCallBack = cb; }
        void setThreadNum(int numThreads)
        {
            assert(0 <= numThreads);
            ex_event_loop_thread_pool->setThreadNum(numThreads);
        }
        ConnectionsSet_t &getConnectionSet() { return m_connectionsSet; };

    private:
        TcpServer &operator=(const TcpServer &);
        TcpServer(const TcpServer &);

        void newConnetion(int sockfd, const InetAddress &peerAddr);
        void removeConnection(const TcpConnectionPtr &conn);
        void removeConnectionInLoop(const TcpConnectionPtr &conn);

        //typedef std::map<std::string, TcpConnectionPtr> ConnectionMap;

        EventLoop *p_loop;
        const std::string m_name;
        std::string m_ipPort;
        std::unique_ptr<Acceptor> p_acceptor;
        //ConnectionMap m_connectionsMap;
        ConnectionsSet_t m_connectionsSet;

        NetCallBacks::ConnectionCallBack m_connectionCallBack;
        NetCallBacks::MessageCallBack m_messageCallBack;
        NetCallBacks::CloseCallBack m_closeCallBack;
        NetCallBacks::WriteCompleteCallBack m_writecompleteCallBack;
        int m_nextConnId;
    };

} // namespace muduo

#endif