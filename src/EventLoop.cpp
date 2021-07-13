#include <assert.h>
#include <poll.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "EventLoop.hpp"
#include "Poller.hpp"
#include "Epoll.hpp"
#include "Logger.hpp"
#include "SocketHelp.hpp"

using namespace muduo;

__thread EventLoop *t_loopInThisThread = 0;

const int kPollTimeMs = -1; //epoll_wait 阻塞

int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    LOG_TRACE << "createEventfd() fd : " << evtfd;

    if (evtfd < 0)
    {
        LOG_SYSERR << "Failed in eventfd";
        abort();
    }

    return evtfd;
}
#pragma GCC diagnostic ignored "-Wold-style-cast"
class IgnoreSigPipe
{
public:
    IgnoreSigPipe() //屏蔽SIGPIPE信号
    {
        ::signal(SIGPIPE, SIG_IGN);

        LOG_TRACE << "Ignore SIGPIPE";
    }
};
#pragma GCC diagnostic error "-Wold-style-cast"
IgnoreSigPipe initObj; //定义一个全局的IgnoreSigPipe变量，表明在这个源文件中SIGPIPE信号是被屏蔽的

EventLoop::EventLoop()
    : m_looping(false),
      m_threadId(CurrentThread::tid()),               //本eventloop所在的线程id
      m_poller(new Epoll(this)),                      //本eventloop创建epoll
      m_timerQueue(new TimerQueue(this)),             //本eventloop创建时钟类
      m_wakeupFd(createEventfd()),                    //本eventloop创建计数器fd
      p_wakeupChannel(new Channel(this, m_wakeupFd)), //计数器fd的channel类
      m_callingPendingFunctors(false)

{
    LOG_INFO << "EventLoop Create " << this << " in thread " << m_threadId;
    if (t_loopInThisThread)
    { //每个线程只有一个EventLoop对象 , 如果当前线程创建了其他 EventLoop对象,则终止程序.
        LOG_FATAL << "Anthor EventLoop " << t_loopInThisThread
                  << " exists in this thread " << m_threadId;
    }
    else
    {
        t_loopInThisThread = this;
    }
    p_wakeupChannel->setReadCallBack(std::bind(&EventLoop::handleRead, this)); //给计数器fd的channel类注册可读函数
    p_wakeupChannel->enableReading();                                          //同时设置读感兴趣事件
    // ::signal(SIGPIPE, SIG_IGN);
    // LOG_TRACE << "Ignore SIGPIPE";
}

EventLoop::~EventLoop()
{
    LOG_TRACE << "EventLoop::~EventLoop()";
    assert(!m_looping);
    p_wakeupChannel->disableAll(); //eventloop负责计数器fd。取消计数器fd所有的感兴趣事件
    p_wakeupChannel->remove();     //从epoll关注文件描述符表中移除
    ::close(m_wakeupFd);           //close 计数器fd
    t_loopInThisThread = NULL;
}

void EventLoop::loop()
{
    assert(!m_looping);
    assertInLoopThread();
    m_quit = false;
    m_looping = true;

    LOG_DEBUG << "EventLoop " << this << " start looping";

    while (!m_quit)
    {
        m_activeChannels.clear();
        m_poller->poll(kPollTimeMs, &m_activeChannels);

        printActiveChannels(); //打印活跃的channel事件

        for (ChannelList::iterator it = m_activeChannels.begin();
             it != m_activeChannels.end(); ++it)
        {
            (*it)->handleEvent();
        }
        doPendingFunctors();
    }

    LOG_DEBUG << "EventLoop " << this << " stop looping";
    m_looping = false;
}

bool EventLoop::isInloopThread() const
{
    return m_threadId == CurrentThread::tid();
}

void EventLoop::assertInLoopThread()
{
    if (!isInloopThread())
    {
        abortNotInLoopThread();
    }
}

void EventLoop::runInLoop(const Functor &cb)
{
    if (isInloopThread())
    {
        // 如果是当前IO线程调用runInLoop，则同步调用cb
        cb();
    }
    else
    {
        // 如果是其它线程调用runInLoop，则异步地将cb添加到队列
        queueInLoop(cb);
    }
}

void EventLoop::queueInLoop(const Functor &cb)
{
    assert(!isInloopThread()); //主线程会调用这个函数，把事件写进队列
    LOG_TRACE << "EventLoop::queueInLoop()";
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingFunctors.push_back(std::move(cb));
    }

    if (!isInloopThread() || m_callingPendingFunctors)
    {
        wakeup();
    }
}

void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = sockets::write(m_wakeupFd, &one, sizeof one); //将缓冲区写入的8字节整形值加到内核计数器上
    if (n != sizeof one)
    {
        LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

void EventLoop::handleRead() //handle wakeup Fd
{
    LOG_TRACE << "EventLoop::handleRead() handle wakeup Fd";
    uint64_t one = 1;
    ssize_t n = sockets::read(m_wakeupFd, &one, sizeof one); // 读取8字节值， 并把计数器重设为0
    if (n != sizeof one)
    {
        LOG_ERROR << "EventLoop::handleRead() reads " << n << "bytes instead of 8";
    }
}

/*
EventLoop::doPendingFunctors()不是简单地在临界区依次调用Functor，而是把回调列表swap()到局部变量functors中，这样做，
一方面减小了临界区的长度(不会阻塞其他线程调用queueInLoop())，另一方面避免了死锁(因为Functor可能再调用queueInLoop())。

muduo这里没有反复执行doPendingFunctors()直到pendingFunctors_为空，反复执行可能会使IO线程陷入死循环，无法处理IO事件。（只处理swap过来的）
*/

void EventLoop::doPendingFunctors()
{
    //LOG_TRACE << "";
    std::vector<Functor> functors;
    m_callingPendingFunctors = true;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        functors.swap(m_pendingFunctors);
    }

    for (size_t i = 0; i < functors.size(); ++i)
    {
        functors[i]();
    }

    m_callingPendingFunctors = false;
}

void EventLoop::abortNotInLoopThread()
{
    LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
              << " was created in threadId_ = " << m_threadId
              << ", current thread id = " << CurrentThread::tid();
}

EventLoop *EventLoop::getEventLoopOfCurrentThread()
{
    return t_loopInThisThread;
}

void EventLoop::quit()
{
    LOG_TRACE << "EventLoop::quit()";
    assert(m_looping == true);

    m_quit = true;

    if (!isInloopThread())
    {
        wakeup();
    }
}

void EventLoop::updateChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    m_poller->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    // if (0)
    // {
    // }

    m_poller->removeChannel(channel);
}

TimerId EventLoop::runAt(const TimeStamp &time, const NetCallBacks::TimerCallBack &cb)
{
    return m_timerQueue->addTimer(cb, time, 0.0);
}

TimerId EventLoop::runAfter(double delay, const NetCallBacks::TimerCallBack &cb)
{
    TimeStamp time(TimeStamp::addTime(TimeStamp::now(), delay));
    return runAt(time, cb);
}

TimerId EventLoop::runEvery(double interval, const NetCallBacks::TimerCallBack &cb)
{
    TimeStamp time(TimeStamp::addTime(TimeStamp::now(), interval));
    return m_timerQueue->addTimer(cb, time, interval);
}

void EventLoop::printActiveChannels() const
{
    for (ChannelList::const_iterator it = m_activeChannels.begin();
         it != m_activeChannels.end(); ++it)
    {
        const Channel *ch = *it;
        LOG_TRACE << "{" << ch->reventsToString() << "} ";
    }
}
