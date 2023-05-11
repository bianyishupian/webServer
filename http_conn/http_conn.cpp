#include "http_conn.h"


// 静态成员变量初始化
int http_conn::m_epollfd = -1;         // 所有的socket上的事件都注册到一个epoll中
int http_conn::m_user_count = 0;       // 统计用户的数量

// 设置fd非阻塞
void setNonblocking(int fd)
{
    int old_flag = fcntl(fd,F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_flag);
}

// 添加fd到epoll中，在http_conn中实现
void add_fd(int epollfd,int fd,bool one_shot)
{
    epoll_event event;
    event.events = EPOLLIN | EPOLLRDHUP;
    // event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    event.data.fd = fd;
    if(one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    // 设置fd非阻塞
    setNonblocking(fd);
}

// 从epoll中删除fd
void remove_fd(int epollfd,int fd)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

// 修改fd，重置EPOLLONESHOT事件
void mod_fd(int epollfd,int fd,int ev)
{
    epoll_event event;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLONESHOT;
    event.data.fd = fd;
    event.events |= ev;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}


http_conn::http_conn()
{

}
http_conn::~http_conn()
{

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

    m_read_index = 0;

    m_bytes_to_send = 0;
    m_bytes_have_send = 0;
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
    if(m_read_index >= READ_BUFFER_SIZE)
    {
        return false;
    }
    // 读取到的字节
    int bytes_read = 0;
    while (true)
    {
        bytes_read = recv(m_sockfd,m_read_buf + m_read_index,READ_BUFFER_SIZE - m_read_index,0);
        if(bytes_read == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 没有数据可读
                break;
            }
            return false;
        }
        else if(bytes_read == 0)
        {
            // 对方关闭连接
            return false;
        }
        m_read_index += bytes_read;
    }
    printf("read data:\n%s",m_read_buf);
    return true;
}
// 非阻塞写
bool http_conn::write()
{
    printf("write\n");  // need reWriting

    return true;
}

// 由线程池的工作线程调用，处理HTTP请求的入口函数
void http_conn::process()
{
    // 解析HTTP请求

    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST)
    {
        mod_fd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }

    // 生成相应
}

