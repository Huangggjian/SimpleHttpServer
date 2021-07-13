#ifndef _NET_CHANNEL_H
#define _NET_CHANNEL_H

#include <functional>
#include <memory>
namespace muduo
{

    class EventLoop;

    class Channel
    {
    public:
        typedef std::function<void()> EventCallBack;
        Channel(EventLoop *loop, int fd);
        ~Channel();

        void handleEvent();
        void handleEventWithGuard();
        void setReadCallBack(const EventCallBack &cb) { m_readCallBack = cb; }
        void setWriteCallBack(const EventCallBack &cb) { m_writeCallBack = cb; }
        void setErrorCallBack(const EventCallBack &cb) { m_errorCallBack = cb; }
        void setCloseCallBack(const EventCallBack &cb) { m_closeCallBack = cb; }

        int fd() const { return m_fd; }
        uint32_t events() const { return m_events; }
        void set_revents(uint32_t revt) { m_revents = revt; }
        bool isNoneEvent() const { return m_events == kNoneEvent; }

        void enableReading()
        {
            m_events |= kReadEvent;
            update();
        }
        void disableReading()
        {
            m_events &= ~kReadEvent;
            update();
        }
        void enableWriting()
        {
            m_events |= kWriteEvent;
            update();
        }
        void enableEpollET()
        {
            m_events |= kEpollET;
            update();
        }
        bool isWriting() { return m_events &= kWriteEvent; }
        bool isReading() { return m_events &= kReadEvent; }
        void disableWriting()
        {
            m_events &= ~kWriteEvent;
            update();
        }
        void disableAll()
        {
            m_events = kNoneEvent;
            update();
        }

        int index() { return m_index; }
        void set_index(int idx) { m_index = idx; }

        // for debug
        std::string reventsToString() const;
        std::string eventsToString() const;
        void tie(const std::shared_ptr<void> &); //将一个shared_ptr指针的值赋给tie_
        EventLoop *ownerLoop() { return p_loop; }
        void remove();

    private:
        Channel &operator=(const Channel &);
        Channel(const Channel &);

        void update();

        //used for r/eventsToString()
        std::string eventsToString(int fd, int ev) const;

        static const uint32_t kNoneEvent;
        static const uint32_t kReadEvent;
        static const uint32_t kWriteEvent;
        static const uint32_t kEpollET;
        EventLoop *p_loop;
        const int m_fd;
        uint32_t m_events;  // 等待的事件
        uint32_t m_revents; // 实际发生了的事件
        int m_index;
        bool m_addedToLoop;
        std::weak_ptr<void> m_tie; //保证channel所在的类
        bool m_tied;
        EventCallBack m_readCallBack;
        EventCallBack m_writeCallBack;
        EventCallBack m_errorCallBack;
        EventCallBack m_closeCallBack;
    };

} // namespace muduo

#endif
