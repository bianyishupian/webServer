#ifndef TIMER_H
#define TIMER_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <time.h>


class Timer;    // 先前向声明
// 封装客户端信息
struct client_data
{
    sockaddr_in address;
    int sockfd;
    Timer *timer;
};

// 定时器类
class Timer
{
public:
    Timer() : prev(NULL), next(NULL) {}
    ~Timer() {};

public:
    time_t expire;                      // 超时时间
    void(*callback_func)(client_data*); // 回调函数，执行定时事件
    client_data* user_data;

    Timer* prev;                        // 前向定时器
    Timer* next;                        // 后向定时器
};

// 定时器容器类。用了双向链表，按照超时时间升序排列
class Sort_timer_list
{
public:
    Sort_timer_list():head(NULL),tail(NULL) {}
    ~Sort_timer_list();

    void add_timer(Timer* timer);       // 添加定时器
    void adjust_timer(Timer* timer);    // 调整定时器
    void del_timer(Timer* timer);       // 删除定时器
    void tick();                        // 定时任务处理函数

private:
    void add_timer(Timer* timer, Timer* list_head); // 调整链表内部结点

    Timer* head;
    Timer* tail;
};


class some_tool
{
public:
    some_tool() {}
    ~some_tool() {}

    void init(int timeout)
    {
        m_TIMESLOT = timeout;
    }

    // 定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler()
    {
        m_timer_list.tick();
        alarm(m_TIMESLOT);
    }

    void show_error(int connfd, const char* info)
    {
        send(connfd,info,strlen(info),0);
        close(connfd);
    }

    int setnonblocking(int fd)
    {
        int old_option = fcntl(fd, F_GETFL);
        int new_option = old_option | O_NONBLOCK;
        fcntl(fd, F_SETFL, new_option);
        return old_option;
    }

    void add_fd(int epollfd, int fd, bool one_shot, int TRIGMode)
    {
        epoll_event event;
        event.data.fd = fd;

        if (1 == TRIGMode)
            event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
        else
            event.events = EPOLLIN | EPOLLRDHUP;

        if (one_shot)
            event.events |= EPOLLONESHOT;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
        setnonblocking(fd);
    }

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void add_sig(int sig, void(handler)(int), bool restart = true)
    {
        struct sigaction sa;
        memset(&sa, '\0', sizeof(sa));
        sa.sa_handler = handler;
        if (restart)
            sa.sa_flags |= SA_RESTART;
        sigfillset(&sa.sa_mask);
        assert(sigaction(sig, &sa, NULL) != -1);
    }


public:
    static int *u_pipefd;
    Sort_timer_list m_timer_list;
    static int u_epollfd;
    int m_TIMESLOT;
};



void callback_func(client_data *user_data);


#endif