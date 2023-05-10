#include "http_conn.h"


// 静态成员变量初始化
int http_conn::m_epollfd = -1;         // 所有的socket上的事件都注册到一个epoll中
int http_conn::m_user_count = 0;       // 统计用户的数量


// 添加fd到epoll中，在http_conn中实现
void add_fd(int epollfd,int fd,bool one_shot)
{
    epoll_event event;
    event.events = EPOLLIN | EPOLLRDHUP;
    event.data.fd = fd;
    if(one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
}
// 从epoll中删除fd
void remove_fd(int epollfd,int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

// 设置fd非阻塞
void setNonblocking(int fd)
{
    int old_flag = fcntl(fd,F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_flag);
}
// 修改fd，重置EPOLLONESHOT事件
void mod_fd(int epollfd,int fd,int ev)
{
    epoll_event event;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLONESHOT;
    event.data.fd = fd;
    event.events |= ev;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
    // 设置文件描述符非阻塞
    setNonblocking(fd);
}


http_conn::http_conn()
{

}
http_conn::~http_conn()
{

}

// 由线程池的工作线程调用，处理HTTP请求的入口函数
void http_conn::process()
{
    // 解析HTTP请求

    // need reWrite

    // 生成相应
}

// 初始化连接
void http_conn::init(int sockfd, const sockaddr_in & addr)
{
    this->m_sockfd = sockfd;
    this->m_address = addr;
    // 设置端口复用
    int reuse = 1;
    setsockopt(m_sockfd,SOL_SOCKET,SO_REUSEPORT,&reuse,sizeof(reuse));
    // 添加到epoll中
    add_fd(m_epollfd,m_sockfd,true);
    ++m_user_count; // 总用户加一
}

// 关闭连接
void http_conn::conn_close()
{
    if(m_sockfd != -1)
    {
        remove_fd(m_epollfd,m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

// 非阻塞读
bool http_conn::read()
{
    printf("read\n");   // need reWriting

    return true;
}
// 非阻塞写
bool http_conn::write()
{
    printf("write\n");  // need reWriting

    return true;
}



