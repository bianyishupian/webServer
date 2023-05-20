#include "server.h"

Server::Server()
{
    //http_conn类对象
    users = new http_conn[MAX_FD];

    //root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    //定时器
    users_timer = new client_data[MAX_FD];
}

Server::~Server()
{
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

void Server::init(int port, int opt_linger, int trigmode, int thread_num, int actor_model, int log_write, int close_log)
{
    m_port = port;
    m_thread_num = thread_num;
    m_OPT_LINGER = opt_linger;
    m_trig_mode = trigmode;
    m_actor_mod = actor_model;
    m_log_write = log_write;
    m_close_log = close_log;
}

void Server::trig_mode()
{
    //LT + LT
    if (m_trig_mode == 0)
    {
        m_LISTEN_mode = 0;
        m_CONN_mode = 0;
    }
    //LT + ET
    else if (m_trig_mode == 1)
    {
        m_LISTEN_mode = 0;
        m_CONN_mode = 1;
    }
    //ET + LT
    else if (m_trig_mode == 2)
    {
        m_LISTEN_mode = 1;
        m_CONN_mode = 0;
    }
    //ET + ET
    else if (m_trig_mode == 3)
    {
        m_LISTEN_mode = 1;
        m_CONN_mode = 1;
    }
}

void Server::log_write()
{
    if(m_close_log == 0)
    {
        if(m_log_write == 1)    // 异步
        {
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        }
        else
        {
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
        }
    }
}


void Server::thread_pool_c()
{
    //线程池
    m_pool = new thread_pool<http_conn>(m_actor_mod, m_thread_num);
}

void Server::event_listen()
{
    //网络编程基础步骤
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    //优雅关闭连接
    if (m_OPT_LINGER == 0)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (m_OPT_LINGER == 1)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    // 设置端口复用
    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    tools.init(TIMESLOT);

    //epoll创建内核事件表
    epoll_event events[MAX_EVENT_NUM];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    tools.add_fd(m_epollfd, m_listenfd, false, m_LISTEN_mode);
    http_conn::m_epollfd = m_epollfd;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    tools.setnonblocking(m_pipefd[1]);
    tools.add_fd(m_epollfd, m_pipefd[0], false, 0);

    tools.add_sig(SIGPIPE, SIG_IGN);
    tools.add_sig(SIGALRM, tools.sig_handler, false);
    tools.add_sig(SIGTERM, tools.sig_handler, false);

    alarm(TIMESLOT);

    //工具类，信号和描述符基础操作
    some_tool::u_pipefd = m_pipefd;
    some_tool::u_epollfd = m_epollfd;
}

void Server::timer(int connfd, struct sockaddr_in client_address)
{
    users[connfd].init(connfd, client_address, m_root, m_CONN_mode, m_close_log);

    //初始化client_data数据
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    Timer *timer = new Timer;
    timer->user_data = &users_timer[connfd];
    timer->callback_func = callback_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    tools.m_timer_list.add_timer(timer);
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void Server::adjust_timer(Timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    tools.m_timer_list.adjust_timer(timer);

    LOG_INFO("%s", "adjust timer once");
}

void Server::deal_timer(Timer *timer, int sockfd)
{
    timer->callback_func(&users_timer[sockfd]);
    if (timer)
    {
        tools.m_timer_list.del_timer(timer);
    }
    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

bool Server::deal_clientdata()
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    if (m_LISTEN_mode == 0)
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
        if (connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (http_conn::m_user_count >= MAX_FD)
        {
            tools.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    }
    else
    {
        while (1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            if (connfd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (http_conn::m_user_count >= MAX_FD)
            {
                tools.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }
    return true;
}

bool Server::deal_signal(bool &timeout, bool &stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
                timeout = true;
                break;
            case SIGTERM:
                stop_server = true;
                break;
            }
        }
    }
    return true;
}

void Server::deal_read(int sockfd)
{
    Timer *timer = users_timer[sockfd].timer;

    //reactor
    if (m_actor_mod == 1)
    {
        if (timer)
        {
            adjust_timer(timer);
        }
        //若监测到读事件，将该事件放入请求队列
        m_pool->append(users + sockfd, 0);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    //proactor
    else
    {
        if (users[sockfd].read())
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            //若监测到读事件，将该事件放入请求队列
            m_pool->append_p(users + sockfd);

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void Server::deal_write(int sockfd)
{
    Timer *timer = users_timer[sockfd].timer;
    //reactor
    if (m_actor_mod == 1)
    {
        if (timer)
        {
            adjust_timer(timer);
        }

        m_pool->append(users + sockfd, 1);

        while (true)
        {
            if (1 == users[sockfd].improv)
            {
                if (1 == users[sockfd].timer_flag)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    }
    else
    {
        //proactor
        if (users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}

void Server::event_loop()
{
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUM, -1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            //处理新到的客户连接
            if (sockfd == m_listenfd)
            {
                bool flag = deal_clientdata();
                if (false == flag)
                    continue;
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //服务器端关闭连接，移除对应的定时器
                Timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            //处理信号
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = deal_signal(timeout, stop_server);
                if (false == flag)
                {
                    LOG_ERROR("%s", "dealclientdata failure");
                }
            }
            //处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                deal_read(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                deal_write(sockfd);
            }
        }
        if (timeout)
        {
            tools.timer_handler();

            LOG_INFO("%s", "timer tick");

            timeout = false;
        }
    }
}