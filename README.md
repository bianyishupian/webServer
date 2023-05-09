# webServer

## 0. 目录

## 1. 规划

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
- 

// 对信号量的封装
class sem
- 

```


### 2.2 线程池

