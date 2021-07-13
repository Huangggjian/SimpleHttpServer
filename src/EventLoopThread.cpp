#include <assert.h>

#include "Logger.hpp"
#include "EventLoop.hpp"
#include "EventLoopThread.hpp"

using namespace muduo;

EventLoopThread::EventLoopThread()
    : p_loop(NULL),
      m_exiting(false),
      m_thread(std::bind(&EventLoopThread::threadFunc, this)),
      m_mutex(),
      m_cond()
{
}

EventLoopThread::~EventLoopThread()
{
    LOG_TRACE << "EventLoopThread::~EventLoopThread()";

    m_exiting = true;
    if (p_loop != NULL)
    {
        p_loop->quit();
        m_thread.join();
    }
}

EventLoop *EventLoopThread::startLoop()
{
    assert(!m_thread.isStarted());
    m_thread.start(); //主要创建了线程，线程运行函数threadFunc

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (p_loop == NULL)
        {
            LOG_TRACE << "EventLoopThread::startLoop() wait()";
            m_cond.wait(lock); //等着，直到threadFunc函数给loop赋值
        }
    }

    LOG_TRACE << "EventLoopThread::startLoop() wakeup";

    return p_loop;
}

void EventLoopThread::threadFunc()
{
    EventLoop loop; //在线程池中的一个线程创建一个Eventloop类（此EventLoop类每个线程池线程都会创建一个）

    if (m_threadInitCallBack)
    {
        m_threadInitCallBack(&loop);
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        p_loop = &loop;
        m_cond.notify(); //唤醒startLoop中的阻塞
        LOG_TRACE << "EventLoopThread::threadFunc() notify()";
    }

    loop.loop();

    p_loop = NULL;
}