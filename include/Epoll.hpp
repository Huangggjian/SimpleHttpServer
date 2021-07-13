#ifndef _NET_EPOLL_HH
#define _NET_EPOLL_HH

#include <vector>
#include <map>

#include "TimeStamp.hpp"
#include "Poller.hpp"

struct epoll_event;

namespace muduo
{

    class Channel;
    class EventLoop;

    class Epoll : public Poller
    {
    public:
        Epoll(EventLoop *loop);
        ~Epoll();

        TimeStamp poll(int timeoutMs, ChannelList *activeChannels);

        void updateChannel(Channel *channel);
        void removeChannel(Channel *channel);

    private:
        const Epoll &operator=(const Epoll &);
        Epoll(const Epoll &);

        static const int kMaxEpollConcurrencySize;
        static const int kInitEpollEventsSize;

        void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

        typedef std::vector<struct epoll_event> EpollFdList;

        int m_epfd;
        std::size_t m_maxEventsSize;
        EpollFdList m_epollEvents;
    };

} // namespace muduo

#endif
