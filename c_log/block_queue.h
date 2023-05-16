#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <queue>
#include <sys/time.h>
#include "../thread_pool/locker.h"


template<class T>
class block_queue
{
public:
    block_queue(int max_size = 1000)
    {
        if(max_size <= 0) exit(-1);

        m_max_size = max_size;
        m_queue = new std::queue<T>;
    }
    ~block_queue()
    {
        m_mutex.lock();
        if(m_queue != NULL)
        {
            delete m_queue;
        }
        m_mutex.unlock();
    }

    bool full()
    {
        m_mutex.lock();
        if(m_queue->size() >= m_max_size)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    bool empty()
    {
        m_mutex.lock();
        if(m_queue->empty())
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    bool front(T &value)
    {
        m_mutex.lock();
        if(m_queue->empty())
        {
            m_mutex.unlock();
            return false;
        }
        value = m_queue->front();
        m_mutex.unlock();
        return true;
    }

    bool back(T &value)
    {
        m_mutex.lock();
        if(m_queue->empty())
        {
            m_mutex.unlock();
            return false;
        }
        value = m_queue->back();
        m_mutex.unlock();
        return true;
    }

    int size()
    {
        int temp = 0;
        m_mutex.lock();
        temp = m_queue->size();
        m_mutex.unlock();
        return temp;
    }

    int max_size()
    {
        int temp = 0;
        m_mutex.lock();
        temp = m_max_size;
        m_mutex.unlock();
        return temp;
    }

    bool push(const T &item)
    {
        m_mutex.lock();
        // 满了
        if(m_queue->size() >= m_max_size)
        {
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }

        m_queue->push(item);
        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }

    bool pop(T &item)
    {
        m_mutex.lock();
        while(m_queue->empty())
        {
            if(!m_cond.wait(m_mutex.get()))
            {
                m_mutex.unlock();
                return false;
            }
        }

        item = m_queue->front();
        m_queue->pop();
    
        m_mutex.unlock();
        return true;
    }
    // 带有超时处理的pop
    bool pop(T &item, int ms_timeout)
    {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);

        m_mutex.lock();
        if(m_queue->empty())
        {
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if(!m_cond.timewait(m_mutex.get(), t))
            {
                m_mutex.unlock();
                return false;
            }
            m_mutex.unlock();
            return false;
        }
        
        item = m_queue->front();
        m_queue.pop();

        m_mutex.unlock();
        return true;
    }

private:
    locker m_mutex;
    cond m_cond;

    std::queue<T>* m_queue;
    int m_max_size;

};




#endif