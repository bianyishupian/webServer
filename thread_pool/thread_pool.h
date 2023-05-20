#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <iostream>
#include <pthread.h>
#include <exception>
#include <list>
#include "./locker.h"

// 线程池模板类
template <typename T>
class thread_pool
{
public:
    thread_pool(int actor_model = 0, int thread_num = 8, int max_requests = 10000);
    ~thread_pool();
    // 添加任务
    bool append(T* request, int stat);
    bool append_p(T* request);

private:
    // 工作线程运行的函数，它不断从工作队列中取出任务并执行
    static void* worker(void *arg); //静态函数
    void run();
    /*  
        游双书上第303页：“值得一提的是，在c++程序中使用pthread_creat时，
        该函数的第3个参数必须指向一个静态函数”
    */
    /*  
        pthread_create的函数原型中第三个参数的类型为函数指针，
        指向的线程处理函数参数类型为(void *),若线程函数为类成员函数，
        则this指针会作为默认的参数被传进函数中，从而和线程函数参数(void*)不能匹配，
        不能通过编译。
        静态成员函数就没有这个问题，里面没有this指针。

    */

private:
    
    int m_thread_num;           // 线程的数量
    pthread_t *m_threads;       // 线程池数组
    int m_max_requests;         // 请求队列最大容量
    std::list<T*> m_workQueue;  // 请求队列
    locker m_locker_workQueue;  // 互斥锁
    sem m_sem_workQueue;        // 信号量
    int m_actor_model;          // 模型切换
};

template<typename T>
thread_pool<T>::thread_pool(int actor_model, int thread_num, int max_requests)
:m_actor_model(actor_model), m_thread_num(thread_num), m_max_requests(max_requests),m_threads(NULL)
{
    if((thread_num <= 0) || max_requests <= 0)
    {
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_num]; // 初始化线程池数组
    if (!m_threads)
    {
        throw std::exception();
    }

    // 创建thread_number个线程，并将它们设置为线程脱离
    for (int i = 0; i < thread_num; ++i)
    {
        // LOG_INFO("creating no.%d", i);
        // 创建thread_number个线程
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        // 设置为线程脱离
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }

}
template<typename T>
thread_pool<T>::~thread_pool()
{
    delete[] m_threads;
    // m_falg_stop = true;
}
template<typename T>
bool thread_pool<T>::append(T* request,int state)
{
    m_locker_workQueue.lock();
    if (m_workQueue.size() >= m_max_requests)
    {
        m_locker_workQueue.unlock();
        return false;
    }
    request->m_state = state;
    m_workQueue.push_back(request);
    m_locker_workQueue.unlock();
    m_sem_workQueue.post();
    return true;
}
template<typename T>
bool thread_pool<T>::append_p(T* request)
{
    m_locker_workQueue.lock();
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

template<typename T>
void* thread_pool<T>::worker(void* arg)
{
    // 将参数转为(thread_pool*)类型
    thread_pool* pool = (thread_pool*)arg;
    pool->run();
    return pool;    // 返回其实没什么意义
}
template<typename T>
void thread_pool<T>::run()
{
    while (true)
    {
        // 信号量等待
        m_sem_workQueue.wait();
        // 被唤醒
        m_locker_workQueue.lock();
        if(m_workQueue.empty())
        {
            m_locker_workQueue.unlock();
            continue;
        }

        T* request = m_workQueue.front();
        m_workQueue.pop_front();
        m_locker_workQueue.unlock();
        if(!request)
        {
            continue;
        }
        
        if (1 == m_actor_model)
        {
            if (0 == request->m_state)
            {
                if (request->read())
                {
                    request->improv = 1;
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if (request->write())
                {
                    request->improv = 1;
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {
            request->process();
        }
    }
    
}

#endif