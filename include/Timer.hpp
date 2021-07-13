#ifndef _NET_TIMER_HH
#define _NET_TIMER_HH

#include "CallBacks.hpp"
#include "TimeStamp.hpp"
// #include "Atomic.hpp"
#include <atomic>

namespace muduo
{

    class Timer //定时器类
    {
    public:
        Timer(const NetCallBacks::TimerCallBack &cb, TimeStamp when, double interval)
            : m_callBack(cb),
              m_expiration(when),        //超时时间
              m_interval(interval),      //周期性间隔
              m_repeat(interval > 0.0),  //周期性大于0则是周期性时钟
              m_sequence(++s_numCreated) //定时器编号
        {
        }

        void run() const
        {
            m_callBack();
        }

        TimeStamp expiration() const { return m_expiration; }
        bool repeat() const { return m_repeat; }
        int64_t sequence() const { return m_sequence; }
        void restart(TimeStamp now);

        static int64_t numCreated() { return s_numCreated.load(); }

    private:
        Timer &operator=(const Timer &);
        Timer(const Timer &);

        const NetCallBacks::TimerCallBack m_callBack;
        TimeStamp m_expiration;
        const double m_interval;
        const bool m_repeat;
        const int64_t m_sequence;
        static std::atomic_int64_t s_numCreated;
        //static AtomicInt64 s_numCreated;
    };

} // namespace muduo

#endif
