#ifndef _NET_TIMERQUEUE_HH
#define _NET_TIMERQUEUE_HH
#include "TimerId.hpp"
#include "CallBacks.hpp"
#include "TimeStamp.hpp"
#include "Channel.hpp"
#include <set>
#include <vector>
#include <queue>
namespace muduo
{

    class EventLoop;

    class TimerQueue
    {
    public:
        TimerQueue(EventLoop *loop);
        ~TimerQueue();

        // Schedules the callback to be run at given time,

        TimerId addTimer(const NetCallBacks::TimerCallBack &cb, TimeStamp when, double interval = 0.0);

        void cancel(TimerId timerId);

    private:
        typedef std::pair<TimeStamp, Timer *> Entry;
        //typedef std::set<Entry> TimerList;
        typedef std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> TimerPq; //小根堆
        typedef std::pair<Timer *, int64_t> ActiveTimer;
        typedef std::set<ActiveTimer> ActiveTimerSet;

        void addTimerInLoop(Timer *timer);
        void cancelInLoop(TimerId timerId);
        //called when timerfd alarms
        void handleRead();
        //move out all expired timers and return they.
        std::vector<Entry> getExpired(TimeStamp now);
        bool insert(Timer *timer);
        void reset(const std::vector<Entry> &expired, TimeStamp now);

        EventLoop *p_loop;
        const int m_timerfd;
        Channel m_timerfdChannel;

        //Timer List sorted by expiration
        //TimerList m_timers;
        TimerPq m_timerspq;
        ActiveTimerSet m_activeTimers;

        bool m_callingExpiredTimers; /*atomic*/
        ActiveTimerSet m_cancelingTimers;
    };

} // namespace muduo

#endif
