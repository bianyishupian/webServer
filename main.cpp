#include <iostream>
#include <arpa/inet.h>
#include <exception>
#include <libgen.h>
#include "thread_pool/thread_pool.h"
#include "http_conn/http_conn.h"

#define MAX_FD 65535    // 最大的文件描述符数量
#define MAX_EVENT_NUM 1000  // 最大监听数量

// 添加信号捕捉
void add_sig(int sig, void(handler)(int))
{
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig,&sa,NULL);
}

// 添加fd到epoll中，在http_conn中实现
extern void add_fd(int epollfd,int fd,bool one_shot);
// 删除fd
extern void remove_fd(int epollfd,int fd);
// 修改fd
extern void mod_fd(int epollfd,int fd,int ev);
int main(int argc, char* argv[])
{
    if(argc <= 1)
    {
        printf("请按照以下格式输入：%s port_number\n",basename(argv[0]));
        exit(-1);
    }
    int port = atoi(argv[1]);

    add_sig(SIGPIPE,SIG_IGN);
    // 创建线程池，初始化
    thread_pool<http_conn>*pool = NULL;
    try
    {
        pool = new thread_pool<http_conn>;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        exit(-1);
    }
    
    // 定义一个数组以保存所有的客户端信息
    http_conn* users = new http_conn[MAX_FD];
    // socket
    int listenfd = socket(PF_INET,SOCK_STREAM,0);
    if(listenfd != 0)
    {
        perror("socket");
        exit(-1);
    }
    // 设置端口复用
    int reuse = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEPORT,&reuse,sizeof(reuse));
    // bind
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(port);

    bind(listenfd,(sockaddr*)&saddr,sizeof(saddr));
    // listen
    listen(listenfd,5);

    // epoll
    epoll_event events[MAX_EVENT_NUM];
    int epollfd = epoll_create(1);
    // 添加listenfd到epoll
    add_fd(epollfd,listenfd,false);
    http_conn::m_epollfd = epollfd;
    while (true)
    {
        int ret = epoll_wait(epollfd,events,MAX_EVENT_NUM,-1);
        if((ret < 0) && (errno != EINTR))
        {
            printf("epoll fail!\n");
            break;
        }
        for(int i = 0; i < ret; ++i)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd)
            {
                // 有新的客户端进来
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                int connfd = accept(sockfd, (struct sockaddr*)&client_addr,&client_addr_len);
                if(http_conn::m_user_count >= MAX_FD)
                {
                    // 连接满了
                    // 告知客户端  TODO
                    printf("connection is full!\n");    // need reWriting
                    close(connfd);
                    continue;
                }
                // 将新的客户数据初始化，放到数组中
                users[connfd].init(connfd,client_addr);
            }
            else if(events[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            {
                // 对方异常断开或发生错误
                users[sockfd].conn_close();
            }
            else if(events[i].events & EPOLLIN)
            {
                if(users[sockfd].read())
                {
                    // 一次性全部读完
                    pool->append(users + sockfd);
                }
                else users[sockfd].conn_close();    // 如果失败，断开连接
            }
            else if(events[i].events & EPOLLOUT)
            {
                if(!users[sockfd].write())
                {
                    // 如果失败，断开连接
                    users[sockfd].conn_close();
                }
            }
        }
    }

    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;
    

    return 0;
}