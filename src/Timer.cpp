#include "Timer.hpp"

using namespace muduo;

std::atomic_int64_t Timer::s_numCreated;

void Timer::restart(TimeStamp now)
{
    if (m_repeat)
    {
        m_expiration = TimeStamp::addTime(now, m_interval);
    }
    else
    {
        m_expiration = TimeStamp::invalid();
    }
}