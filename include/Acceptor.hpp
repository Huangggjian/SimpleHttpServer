#ifndef _NET_ACCEPTOR_HH
#define _NET_ACCEPTOR_HH
#include <functional>

#include "Channel.hpp"
#include "Socket.hpp"

class EventLoop;
class InetAddress;

namespace muduo
{

    class Acceptor
    {
    public:
        enum Method
        {
            LT,
            ET,
        };
        typedef std::function<void(int sockfd, const InetAddress &)> NewConnectionCallBack;

        Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport = true);
        ~Acceptor();

        void listen();
        bool listenning() const { return m_listenning; } // get listen status.
        //在TcpServer类中注册NewConnectionCallBack
        void setNewConnectionCallBack(const NewConnectionCallBack &cb) { m_newConnectionCallBack = cb; }
        static void setListenMethod(Method m);

    private:
        Acceptor &operator=(const Acceptor &) = delete;
        Acceptor(const Acceptor &) = delete;

        void handleReadLT();
        void handleReadET();
        EventLoop *p_loop;

        Socket m_acceptSocket;
        Channel m_acceptChannel;
        NewConnectionCallBack m_newConnectionCallBack;
        bool m_listenning;
        int m_idleFd;
        // static bool isET;
    };

} // namespace muduo

#endif