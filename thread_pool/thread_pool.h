#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <iostream>
#include <pthread.h>
#include <exception>
#include <list>
#include "locker.h"

// 线程池模板类
template<class T>
class thread_pool
{
public:
    thread_pool(int thread_num = 8, int max_requests = 10000);
    ~thread_pool();
    // 添加任务
    bool append(T* request);

private:
    // 工作线程运行的函数，它不断从工作队列中取出任务并执行
    static void *worker(void *arg);
    void run();

private:
    
    int m_thread_num;           // 线程的数量
    pthread_t *m_threads;       // 线程池数组
    int m_max_requests;         // 请求队列最大容量
    std::list<T*> m_workQueue;  // 请求队列
    locker m_locker_workQueue;  // 互斥锁
    sem m_sem_workQueue;        // 信号量
    bool m_falg_stop;           // 是否结束线程
};

template<class T>
thread_pool<T>::thread_pool(int thread_num, int max_requests)
:m_thread_num(thread_num), m_max_requests(max_requests),m_falg_stop(false),m_threads(NULL)
{
    if((thread_num <= 0) || max_requests <= 0)
    {
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number]; // 初始化线程池数组
    if (!m_threads)
    {
        throw std::exception();
    }

    // 创建thread_number个线程，并将它们设置为线程脱离
    for (int i = 0; i < thread_num; ++i)
    {
        std::cout<<"creating no."<<i<<" thread...";
        // 创建thread_number个线程
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            std::cout<<"- default"<<end;
            delete[] m_threads;
            throw std::exception();
        }
        // 设置为线程脱离
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
        std::cout<<"- success"<<end;
    }
    
}
template<class T>
thread_pool<T>::~thread_pool()
{
    delete[] m_threads;
    m_falg_stop = true;
}
template<class T>
bool thread_pool<T>::append(T* request)
{
    m_workQueue.lock();
    if (m_workQueue.size() >= m_max_requests)
    {
        m_locker_workQueue.unlock();
        return false;
    }
    m_workQueue.push_back(request);
    m_locker_workQueue.unlock();
    m_sem_workQueue.post();
    return true;
}
#endif