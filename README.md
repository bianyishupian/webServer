# webServer

## 0. 目录

[1 概述](#1. 概述)

[2 线程池](#2. 实现线程同步机制的封装与线程池)

[3 http连接处理](#3. http连接处理)

[4 定时器](#4. 定时器处理非活动连接)

[5 日志](#5. 同步/异步日志)

[6 解析命令行](#6. 解析命令行)

[7 流程封装](#7. 流程封装)

[8 压力测试](#8. 压力测试)

## 1. 概述

文件目录详情：

```c++
.
├── m_server			流程封装
│   ├── server.cpp
│   └── server.h
├── thread_pool			线程池
│   ├── thread_pool
│   └── locker.h		封装互斥锁、信号量、条件变量
├── http_conn			http连接处理
│   ├── http_conn.h
│   └── http_conn.cpp
├── config				解析命令行配置
│   ├── config.h
│   └── config.cpp
├── test_presure		压力测试
│   └── webbench-1.5
├── root				静态资源
│   ├── index.html
│   └── image
├── timer				定时器
│   ├── timer.h
│   └── timer.cpp
├── c_log				同步/异步日志
│   ├── log.h
│   ├── log.cpp
│   └── block_queue.h	阻塞队列，实现异步日志
├── main.cpp			main
├── server				可执行文件
├── c_log				日志文件
├── Makefile			
├── LICENSE
└── README.md
```

流程图：



整体流程：

1. 解析命令行，获取一系列参数
2. 创建http连接类对象数组，创建定时器类对象
3. 初始化参数（没有获取到的就用默认）
4. 启用日志系统（如果在启动服务器时选择不启用，那么就空实现）
5. 创建线程池
6. 根据参数设置epoll的触发模式
7. 网络编程的基础步骤
   1. 创建监听的socket，设置端口复用，绑定bind，listen
   2. 设置超时时间
   3. 创建epoll，注册epoll检测事件
   4. 创建管道fd（日志系统用）
8. 主循环
   1. 调用epoll_wait检测
   2. 对监听到事件的事件进行处理（详见 [7. 流程封装](#7. 流程封装)）
   3. 如果循环结束标志为false，继续循环执行1，2

## 2. 实现线程同步机制的封装与线程池
### 2.1 线程同步机制的封装
主要实现互斥锁、条件变量与信号量的封装
```cpp
// 对互斥锁的封装
class locker
- locker()
    - 构造函数，调用了pthread_mutex_init()
- ~locker()
    - 析构函数，调用了pthread_mutex_destory()
- bool lock()
    - 上锁，调用了pthread_mutex_lock()，返回bool类型
- bool unlock()
    - 解锁，调用了pthread_mutex_unlock()，返回bool类型
- pthread_mutex_t* get()
    - 获得一个互斥锁，return &m_mutex

// 对条件变量的封装
class cond
- cond()
    - 构造函数，调用pthread_cond_init()
- ~cond()
    - 析构函数，调用了pthread_cond_destroy()
- bool wait(pthread_mutex_t* m_mutex)
    - 等待，调用了pthread_cond_wait()
- bool timewait(pthread_mutex_t* m_mutex,const struct timespec abstime)
    - 等待多长时间，调用了pthread_cond_timedwait()
- bool signal()
    - 唤醒一个或者多个等待的线程，调用了pthread_cond_signal()
- bool broadcast()
    - 唤醒所有的等待的线程，调用了pthread_cond_broadcast()
    
// 对信号量的封装
class sem
- sem()
    - 构造函数，调用sem_init()，第二个参数为0表示用在线程间，第三个参数默认为0
- sem(int num)
    - 有参构造函数，调用sem_init()，第三个参数为传进来的num，为信号量中的值
- ~sem()
    - 析构函数，调用了sem_destroy()
- bool wait()
    - 对信号量加锁，调用了sem_wait
- bool post()
    - 对信号量解锁，调用了sem_post()

```


### 2.2 线程池

**为什么要用线程池？**

当你需要限制你应用程序中同时运行的线程数时，线程池非常有用。因为启动一个新线程会带来性能开销，每个线程也会为其堆栈分配一些内存等。为了任务的并发执行，我们可以将这些任务任务传递到线程池，而不是为每个任务动态开启一个新的线程。

**关于线程池：**

- 所谓线程池，就是一个`pthread_t`类型的普通数组，通过`pthread_create()`函数创建`m_thread_number`个**线程**，用来执行`worker()`函数以执行每个请求处理函数（HTTP请求的`process`函数），通过`pthread_detach()`将线程设置成脱离态（detached）后，当这一线程运行结束时，它的资源会被系统自动回收，而不再需要在其它线程中对其进行 `pthread_join()` 操作。

- 空间换时间,浪费服务器的硬件资源,换取运行效率。
- 池是一组资源的集合,这组资源在服务器启动之初就被完全创建好并初始化,这称为静态资源。
- 当服务器进入正式运行阶段,开始处理客户请求的时候,如果它需要相关的资源,可以直接从池中获取,无需动态分配。
- 当服务器处理完一个客户连接后,可以把相关的资源放回池中,无需执行系统调用释放资源。

**线程池中的线程数量是依据什么确定的？**

线程池中的线程数量最直接的限制因素是中央处理器(CPU)的处理器(processors/cores)的数量`N`：如果你的CPU是4-cores的，对于CPU密集型的任务(如视频剪辑等消耗CPU计算资源的任务)来说，那线程池中的线程数量最好也设置为4（或者+1防止其他因素造成的线程阻塞）；对于IO密集型的任务，一般要多于CPU的核数，因为线程间竞争的不是CPU的计算资源而是IO，IO的处理一般较慢，多于cores数的线程将为CPU争取更多的任务，不至在线程处理IO的过程造成CPU空闲导致资源浪费，公式：`最佳线程数 = CPU当前可使用的Cores数 * 当前CPU的利用率 * (1 + CPU等待时间 / CPU处理时间)`



![](./image/thread_pool.png)

**worker函数为什么要定义为静态函数？**

游双书上第303页：“值得一提的是，在c++程序中使用`pthread_creat`时，该函数的第3个参数必须指向一个静态函数”

`pthread_create`的函数原型中第三个参数的类型为函数指针，指向的线程处理函数参数类型为`(void *)`,若线程函数为类成员函数，则this指针会作为默认的参数被传进函数中，从而和线程函数参数`(void*)`不能匹配，不能通过编译。静态成员函数就没有这个问题，里面没有`this`指针。



## 3. http连接处理





## 4. 定时器处理非活动连接



## 5. 同步/异步日志



## 6. 解析命令行



## 7. 流程封装



## 8. 压力测试

