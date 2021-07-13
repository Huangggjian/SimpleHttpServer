#include "AsyncLogging.hpp"
#include "LogFile.hpp"
#include <assert.h>
#include <stdio.h>

AsyncLogging::AsyncLogging(const std::string filePath, off_t rollSize, double flushInterval)
    : m_filePath(filePath),
      m_rollSize(rollSize),
      m_flushInterval(flushInterval),
      m_isRunning(false),
      m_thread(std::bind(&AsyncLogging::threadRoutine, this)),
      m_mutex(),
      m_cond(),
      m_currentBuffer(new Buffer), //异步日志用的前端或者后端buffer都是4M
      m_nextBuffer(new Buffer),
      m_buffers()
{
}

AsyncLogging::~AsyncLogging()
{
    if (m_isRunning)
        stop();
}

void AsyncLogging::append(const char *logline, std::size_t len) //将logline字符串加入进缓存中
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_currentBuffer->avail() > len) //如果当前缓存足够，就直接加入进当前缓存
    {
        m_currentBuffer->append(logline, len);
    }
    else //如果第一块缓存不够
    {
        m_buffers.push_back(m_currentBuffer.release()); //m_currentBuffer已经满了，要将之存放到buffers_中。m_buffer存放已经满了的前台buffer。

        if (m_nextBuffer) //m_currentBuffer接管m_nextBuffer所指向的区域
        {
            m_currentBuffer = std::move(m_nextBuffer);
        }
        else //下一块缓存是空的就新建一块缓存. Rarely happens，可以不用考虑这情况
        {
            m_currentBuffer.reset(new Buffer);
        }

        m_currentBuffer->append(logline, len);
        m_cond.notify(); //通知后台线程，已经有一个满了的buffer了。
    }
}

void AsyncLogging::threadRoutine()
{
    assert(m_isRunning == true);
    LogFile output(m_filePath, m_rollSize, false); //定义一个直接进行IO的类。
    BufferPtr backupBuffer1(new Buffer);           //这两个是后台线程的buffer1
    BufferPtr backupBuffer2(new Buffer);           //buffer2
    BufferVector buffersToWrite;                   //用来和前台线程的buffers_进行swap.
    buffersToWrite.reserve(8);

    while (m_isRunning)
    {
        assert(buffersToWrite.empty());
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            if (m_buffers.empty())
            {
                m_cond.waitForSeconds(lock, m_flushInterval);
            }

            m_buffers.push_back(m_currentBuffer.release());
            m_currentBuffer = std::move(backupBuffer1); /*---归还一个buffer---*/
            m_buffers.swap(buffersToWrite);             //交换
            if (!m_nextBuffer)                          /*-----假如需要，归还第二个----*/
                m_nextBuffer = std::move(backupBuffer2);
        }

        assert(!buffersToWrite.empty());

        //将已经满了的buffer内容写入到LogFile中。由LogFile进行IO操作。
        for (size_t i = 0; i < buffersToWrite.size(); ++i)
        {
            output.append(buffersToWrite[i]->data(), buffersToWrite[i]->length());
        }

        if (buffersToWrite.size() > 2)
        {
            // drop non-bzero-ed buffers, avoid trashing
            buffersToWrite.resize(2);
        }
        //前台buffer是由backupBuffer1 2 归还的。现在把buffersToWrite的buffer归还给后台buffer
        if (!backupBuffer1)
        {
            assert(!buffersToWrite.empty());
            backupBuffer1 = std::move(buffersToWrite.pop_back()); //由于之前把backupBuffer1的值移动给了（归还前台Buffer）m_currentBuffer，所以如果再次调用就会出错，
                                                                  //所以需要使用给backupBuffer1赋另外一个值(归还后台Buffer)，然后把缓存中的数据清空
            backupBuffer1->reset();
        }

        if (!backupBuffer2)
        {
            assert(!buffersToWrite.empty());
            backupBuffer2 = std::move(buffersToWrite.pop_back());
            backupBuffer2->reset();
        }

        buffersToWrite.clear();
        output.flush();
    }

    output.flush();
}
