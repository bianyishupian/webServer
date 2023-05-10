#ifndef HTTP_COON_H
#define HTTP_COON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>

#include "../thread_pool/locker.h"

class http_conn
{
public:
    http_conn();
    ~http_conn();

    void process();     // 处理客户端请求
    void init(int sockfd, const sockaddr_in & addr);    // 初始化新建立的连接
    void conn_close();  // 关闭连接
    bool read();        // 非阻塞读
    bool write();       // 非阻塞写
public:
    static int m_epollfd;       // 所有的socket上的事件都注册到一个epoll中
    static int m_user_count;    // 统计用户的数量
private:
    int m_sockfd;               // 该http连接的socket
    sockaddr_in m_address;      // 通信的socket地址
};



#endif