#include <stdint.h>
#include <assert.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "Logger.hpp"
#include "EventLoop.hpp"
#include "Timer.hpp"
#include "TimerQueue.hpp"

using namespace muduo;

namespace TimerFd
{

    int createTimerfd()
    {
        int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                       TFD_NONBLOCK | TFD_CLOEXEC);
        LOG_TRACE << "createTimerfd() fd : " << timerfd;
        if (timerfd < 0)
        {
            LOG_SYSFATAL << "Failed in timerfd_create";
        }
        return timerfd;
    }

    struct timespec howMuchTimeFromNow(TimeStamp when)
    {
        int64_t microseconds = when.microSecondsSinceEpoch() - TimeStamp::now().microSecondsSinceEpoch();
        if (microseconds < 100)
        {
            microseconds = 100;
        }
        struct timespec ts;
        ts.tv_sec = static_cast<time_t>(
            microseconds / TimeStamp::kMicroSecondsPerSecond);
        ts.tv_nsec = static_cast<long>(
            (microseconds % TimeStamp::kMicroSecondsPerSecond) * 1000);
        return ts;
    }

    void readTimerfd(int timerfd, TimeStamp now)
    {
        uint64_t howmany;
        ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
        LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
        if (n != sizeof howmany)
        {
            LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
        }
    }

    void resetTimerfd(int timerfd, TimeStamp expiration) //根据expiration，设置新的超时时间
    {
        // wake up loop by timerfd_settime()
        LOG_TRACE << "resetTimerfd()";
        struct itimerspec newValue;
        struct itimerspec oldValue;
        bzero(&newValue, sizeof newValue);
        bzero(&oldValue, sizeof oldValue);
        newValue.it_value = howMuchTimeFromNow(expiration);
        int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
        if (ret)
        {
            LOG_SYSERR << "timerfd_settime()";
        }
    }

}; // namespace TimerFd

using namespace TimerFd;

TimerQueue::TimerQueue(EventLoop *loop)
    : p_loop(loop),
      m_timerfd(createTimerfd()),
      m_timerfdChannel(p_loop, m_timerfd),
      //m_timers(),
      m_timerspq(),
      m_callingExpiredTimers(false)
{
    m_timerfdChannel.setReadCallBack(std::bind(&TimerQueue::handleRead, this));
    m_timerfdChannel.enableReading();
}

TimerQueue::~TimerQueue()
{
    m_timerfdChannel.disableAll();
    m_timerfdChannel.remove();
    ::close(m_timerfd);
    // for (TimerList::iterator it = m_timers.begin();
    //      it != m_timers.end(); ++it)
    // {
    //     delete it->second;
    // }
    while (!m_timerspq.empty())
    {
        const Entry &top = m_timerspq.top();
        delete top.second;
        m_timerspq.pop();
    }
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(TimeStamp now) //返回到期的定时器
{
    std::vector<Entry> expired;
    //Entry sentry = std::make_pair(now, reinterpret_cast<Timer *> UINTPTR_MAX);

    // TimerList::iterator it = m_timers.lower_bound(sentry);//
    // assert(it == m_timers.end() || now < it->first);
    //std::copy(m_timers.begin(), it, back_inserter(expired));
    // m_timers.erase(m_timers.begin(), it); //到期的，从所有定时器set删除
    while (!m_timerspq.empty())
    {
        const Entry &top = m_timerspq.top();
        if (top.first <= now)
        {
            expired.emplace_back(std::move(top));
            m_timerspq.pop();
        }
        else
        {
            break;
        }
    }

    for (std::vector<Entry>::iterator it = expired.begin();
         it != expired.end(); ++it)
    {
        ActiveTimer timer(it->second, it->second->sequence());
        size_t n = m_activeTimers.erase(timer); //到期的，从Active列表中定时器删除。
        assert(n == 1);
        (void)n;
    }

    assert(m_timerspq.size() == m_activeTimers.size());

    return expired;
}

TimerId TimerQueue::addTimer(const NetCallBacks::TimerCallBack &cb, TimeStamp when, double interval)
{
    Timer *timer = new Timer(cb, when, interval); //when: 到期时间，interval周期性
    p_loop->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
    return TimerId(timer, timer->sequence());
}

void TimerQueue::addTimerInLoop(Timer *timer)
{
    p_loop->assertInLoopThread();
    bool earliestChanged = insert(timer);

    if (earliestChanged) //插入的定时器恰好是最早到期的
    {
        resetTimerfd(m_timerfd, timer->expiration()); //那就重置超时时间
    }
}

void TimerQueue::cancel(TimerId timerId)
{
    p_loop->runInLoop(std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    // p_loop->assertInLoopThread();
    // assert(m_timerspq.size() == m_activeTimers.size());
    // ActiveTimer timer(timerId.m_timer, timerId.m_sequence);
    // ActiveTimerSet::iterator it = m_activeTimers.find(timer);
    // if (it != m_activeTimers.end())
    // {
    //     size_t n = m_timers.erase(Entry(it->first->expiration(), it->first));
    //     assert(n == 1);
    //     delete it->first;
    // }
    // else if (m_callingExpiredTimers)
    // {
    //     m_cancelingTimers.insert(timer);
    // }
    // assert(m_timerspq.size() == m_activeTimers.size());
}

bool TimerQueue::insert(Timer *timer) //插入定时器
{
    p_loop->assertInLoopThread();
    assert(m_timerspq.size() == m_activeTimers.size());
    bool earliestChanged = false;
    TimeStamp when = timer->expiration();
    //TimerList::iterator it = m_timers.begin();
    //Entry top = m_timerspq.top();
    if (m_timerspq.empty() || when < m_timerspq.top().first)
    {
        earliestChanged = true;
    }
    {
        //std::pair<TimerList::iterator, bool> result =
        m_timerspq.push(Entry(when, timer)); //插入
        // assert(result.second);
        // (void)result;
    }
    {
        std::pair<ActiveTimerSet::iterator, bool> result = m_activeTimers.insert(ActiveTimer(timer, timer->sequence())); //活跃列表也插入
        assert(result.second);
        (void)result;
    }

    LOG_TRACE << "TimerQueue::insert() "
              << "m_timers.size() : "
              << m_timerspq.size() << " m_activeTimers.size() : " << m_activeTimers.size();

    assert(m_timerspq.size() == m_activeTimers.size());
    return earliestChanged;
}

void TimerQueue::handleRead()
{
    p_loop->assertInLoopThread();
    TimeStamp now(TimeStamp::now());
    readTimerfd(m_timerfd, now);

    std::vector<Entry> expired = getExpired(now);

    LOG_TRACE << "Expired Timer size " << expired.size() << "  ";

    m_callingExpiredTimers = true;
    m_cancelingTimers.clear();

    for (std::vector<Entry>::iterator it = expired.begin();
         it != expired.end(); ++it)
    {
        it->second->run();
    }

    m_callingExpiredTimers = false;

    reset(expired, now);
}

void TimerQueue::reset(const std::vector<Entry> &expired, TimeStamp now)
{
    TimeStamp nextExpire;

    for (std::vector<Entry>::const_iterator it = expired.begin();
         it != expired.end(); ++it)
    {
        ActiveTimer timer(it->second, it->second->sequence());
        if (it->second->repeat() && m_cancelingTimers.find(timer) == m_cancelingTimers.end())
        { //如果是周期定时器则重新设定时间插入. 否则delete.
            it->second->restart(now);
            insert(it->second);
        }
        else
        { // FIXME move to a free list no delete please
            delete it->second;
        }
    }

    if (!m_timerspq.empty())
    {
        nextExpire = m_timerspq.top().second->expiration(); //下一个超时时间
    }

    if (nextExpire.valid())
    {
        resetTimerfd(m_timerfd, nextExpire);
    }
}