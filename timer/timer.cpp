#include "timer.h"
#include "../http_conn/http_conn.h"


int some_tool::u_epollfd = 0;
int *some_tool::u_pipefd = 0;

Sort_timer_list::~Sort_timer_list()
{
    // 循环销毁链表节点
    Timer* temp = head;
    while(temp)
    {
        head = temp->next;
        delete temp;
        temp = head;
    }
}

// 添加定时器
void Sort_timer_list::add_timer(Timer* timer)
{
    // 若当前链表中只有头尾结点，直接插入
    // 否则，将定时器按照升序插入
    if(!timer)
    {
        return;
    }
    if(!head)
    {
        head = tail = timer;
        head->prev = NULL;
        tail->next = NULL;
        return;
    }
    if(timer->expire <= head->expire)
    {
        // 如果要插入的结点超时时间比头结点小，则直接头插
        timer->next = head;
        head->prev = timer;
        head = timer;
        head->prev = NULL;
        return;
    }
    if(timer->expire >= tail->expire)
    {
        // 果要插入的结点超时时间比尾结点大，则直接尾插
        tail->next = timer;
        timer->prev = tail;
        timer->next = NULL;
        tail = timer;
        return;
    }
    add_timer(timer,head);
}
// 调整定时器
void Sort_timer_list::adjust_timer(Timer* timer)
{
    if(!timer)
    {
        return;
    }
    Timer* temp = timer->next;
    // 此定时器的超时时间仍然小于下一个定时器，不调整
    if(!temp || (timer->expire < temp->expire))
    {
        return;
    }
    // 此定时器是头结点，取出，重新添加
    if(timer == head)
    {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer);
    }
    // 此定时器在内部，取出，重新添加
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        timer->next = NULL;
        timer->prev = NULL;
        add_timer(timer);
    }
}
// 删除定时器
void Sort_timer_list::del_timer(Timer* timer)
{
    if(!timer)
    {
        return;
    }
    // 此定时器是唯一一个定时器
    if((timer == head) && (timer == tail))
    {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    // 此定时器是头结点
    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    // 此定时器是尾结点
    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    // 此定时器在内部
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}
void Sort_timer_list::add_timer(Timer* timer,Timer* list_head)
{
    Timer* pprev = list_head;
    Timer* temp = pprev->next;
    // 遍历当前结点之后的链表，按照超时时间找到目标定时器对应的位置
    while(temp)
    {   
        if (timer->expire <= temp->expire)
        {
            pprev->next = timer;
            timer->next = temp;
            temp->prev = timer;
            timer->prev = pprev;
            break;
        }
        pprev = temp;
        temp = temp->next;
    }
    // if (!temp)
    // {
    //     pprev->next = timer;
    //     timer->prev = pprev;
    //     timer->next = NULL;
    //     tail = timer;
    // }
}

// 定时任务处理函数
void Sort_timer_list::tick()
{
    if(!head)
    {
        return;
    }
    // 获取当前时间
    time_t cur = time(NULL);
    Timer* temp = head;

    // 遍历定时器容器
    while(temp)
    {
        // 当前时间小于头结点定时器的到期时间
        if(cur < temp->expire)
        {
            break;
        }
        // 当前定时器到期，调用回调函数
        temp->callback_func(temp->user_data);
        // 将处理后的定时器从容器中删除，并重置头结点
        // 如果当前的定时器之前不是唯一结点，需要使重置后的头结点前向指针指向NULL
        head = temp->next;
        if(head)
        {
            head->prev = NULL;
        }
        delete temp;
        temp = head;
    }
}

void some_tool::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

// 回调函数
void callback_func(client_data *user_data)
{
    // 删除非活动连接在socket上的注册事件
    epoll_ctl(some_tool::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    // 关闭fd
    close(user_data->sockfd);
    // 减少连接数
    http_conn::m_user_count--;
}


