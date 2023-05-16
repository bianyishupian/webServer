#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "../http_conn/http_conn.h"
#include "../thread_pool/thread_pool.h"
#include "../c_log/log.h"

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUM = 10000;    //最大事件数
const int TIMESLOT = 5;             //最小超时单位

class Server
{
public:
    Server();                           // 构造函数，新建http_conn对象、定时器，root路径
    ~Server();                          // 关闭fd，释放空间

    void init(int port, int opt_linger, int trigmode, int thread_num, int actor_model);                // 初始化

    void thread_pool_c();               // 线程池创建
    void trig_mode();                   // epoll触发模式
    void log_write();                   // 写log
    void event_listen();                // 监听
    void event_loop();                  // epoll_wait
    void timer(int connfd, struct sockaddr_in client_address);
    void adjust_timer(Timer *timer);    // 调整定时器与容器
    void deal_timer(Timer *timer, int sockfd);  // 调用回调函数
    bool deal_clientdata();             // accept
    bool deal_signal(bool& timeout, bool& stop_server); // 处理信号
    void deal_read(int sockfd);         // 处理读事件
    void deal_write(int sockfd);        // 处理写事件

    
    // need writing log

public:
    //基础
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actor_mod;

    int m_pipefd[2];
    int m_epollfd;
    http_conn *users;

    //线程池相关
    thread_pool<http_conn>* m_pool;

    int m_thread_num;

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUM];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_trig_mode;
    int m_LISTEN_mode;
    int m_CONN_mode;

    //定时器相关
    client_data *users_timer;
    some_tool tools;
};




#endif