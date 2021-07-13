#ifndef NET_CALLBACKS_H
#define NET_CALLBACKS_H

#include <functional>
#include <memory>

#include "TimeStamp.hpp"

namespace muduo
{

    class Buffer;
    class TcpConnection;
    typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
    typedef std::weak_ptr<TcpConnection> TcpConnectionWeakPtr;
    namespace NetCallBacks
    {

        // All client visible callbacks go here.

        typedef std::function<void()> TimerCallBack;
        typedef std::function<void(const TcpConnectionPtr &)> ConnectionCallBack;
        typedef std::function<void(const TcpConnectionPtr &, Buffer *, ssize_t)> MessageCallBack;
        typedef std::function<void(const TcpConnectionPtr &)> CloseCallBack;
        typedef std::function<void(const TcpConnectionPtr &)> WriteCompleteCallBack;
        void defaultConnectionCallback();

    } // namespace NetCallBacks

}; // namespace muduo

#endif // NET_CALLBACKS_H
