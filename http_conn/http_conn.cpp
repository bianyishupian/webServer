#include "http_conn.h"


// 静态成员变量初始化
int http_conn::m_epollfd = -1;         // 所有的socket上的事件都注册到一个epoll中
int http_conn::m_user_count = 0;       // 统计用户的数量

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

// const char* web_root = "/home/user/cpp/webServer/root";

// 设置fd非阻塞
void setNonblocking(int fd)
{
    int old_flag = fcntl(fd,F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_flag);
}

// 添加fd到epoll中
void add_fd(int epollfd,int fd,bool one_shot,int trigMod)
{
    epoll_event event;
    if(trigMod == 1)
    {
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    }
    else
    {
        event.events = EPOLLIN | EPOLLRDHUP;
    } 
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
void mod_fd(int epollfd,int fd,int ev,int trigMod)
{
    epoll_event event;
    event.data.fd = fd;
    if(trigMod == 1)
    {
        event.events = ev | EPOLLRDHUP | EPOLLET | EPOLLONESHOT;
    }
    else
    {
        event.events = ev | EPOLLRDHUP | EPOLLONESHOT;
    }
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
}



// 初始化连接
void http_conn::init(int sockfd, const sockaddr_in & addr, char* root, int trigMod, int close_log)
{
    m_sockfd = sockfd;
    m_address = addr;
    m_trig_mode = trigMod;
    web_root = root;
    m_close_log = close_log;
    // 添加到epoll中
    add_fd(m_epollfd,m_sockfd,true,m_trig_mode);
    ++m_user_count; // 总用户加一

    init();
}

// 初始化其余数据
void http_conn::init()
{
    m_bytes_to_send = 0;
    m_bytes_have_send = 0;

    m_check_state = CHECK_STATE_REQUESTLINE;
    m_checked_index = 0;
    m_start_line = 0;
    m_read_index = 0;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_host = 0;
    m_user_agent = 0;
    m_linger = false;
    m_content_length = 0;
    // m_string = 0;
    m_write_index = 0;
    m_post = false;
    timer_flag = 0;
    improv = 0;
    m_state = 0;


    memset(m_read_buf,'\0',READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0',FILENAME_LEN);
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
    if(m_read_index >= READ_BUFFER_SIZE)
    {
        return false;
    }
    // 读取到的字节
    int bytes_read = 0;

    // LT
    if(m_trig_mode == 0)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_index, READ_BUFFER_SIZE - m_read_index, 0);
        m_read_index += bytes_read;

        if (bytes_read <= 0)
        {
            return false;
        }

        return true;
    }
    // ET
    else
    {
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
        // printf("read data:\n%s",m_read_buf);
        return true;
    }
    
}
// 写HTTP响应
bool http_conn::write()
{
    int temp = 0;
    
    if(m_bytes_to_send == 0)
    {
        mod_fd(m_epollfd,m_sockfd,EPOLLIN,m_trig_mode);
        init();
        return true;
    }
    while(true)
    {
        // 分散写
        temp = writev(m_sockfd,m_iv,m_iv_count);
        if ( temp <= -1 )
        {
            /*
                如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，
                虽然在此期间，服务器无法立即接收到同一客户的下一个请求，
                但可以保证连接的完整性。
            */
            if( errno == EAGAIN )
            {
                mod_fd( m_epollfd, m_sockfd, EPOLLOUT, m_trig_mode);
                return true;
            }
            unmap();    // 释放内存映射
            return false;
        }
        // 更新状态
        m_bytes_to_send -= temp;
        m_bytes_have_send += temp;

        if(m_bytes_have_send >= m_iv[0].iov_len)
        {
            // 响应头写入完毕
            m_iv[0].iov_len = 0;
            // 内存映射的地址 = 初始地址 + (全部已经发送的 - 写缓冲区发送的)
            m_iv[1].iov_base = m_file_address + (m_bytes_have_send - m_write_index);
            m_iv[1].iov_len = m_bytes_to_send;
        }
        else
        {
            // 响应头没有写完
            m_iv[0].iov_base = m_write_buf + m_bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - temp;
        }
        if (m_bytes_to_send <= 0)
        {
            // 没有数据要发送了
            unmap();
            mod_fd(m_epollfd, m_sockfd, EPOLLIN, m_trig_mode);
            // 根据m_linger判断是否要保持连接
            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    return true;
}


// 主状态机
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;

    char* text = 0;
    while(( (m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK) ) || ( (line_status = parse_line()) == LINE_OK ))
    {
        // 解析了一行完整的数据
        text = get_line();
        // 更新一下下一次需要读取的开始位置
        m_start_line = m_checked_index;
        // printf("got 1 http line : %s\n",text);

        switch (m_check_state)
        {
        // 解析请求行
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if(ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            break;
        }
        // 解析请求头
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if(ret == BAD_REQUEST)
            {
                return BAD_REQUEST;
            }else if(ret == GET_REQUEST){
                return do_request();
            }
            break;
        }
        // 解析请求体
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if(ret == GET_REQUEST)
            {
                return do_request();
            }
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
            break;
        }
    }

    return NO_REQUEST;
}

// 从状态机
// 解析一行，检测\r\n
http_conn::LINE_STATUS http_conn::parse_line()
{
    char temp;
    for( ; m_checked_index < m_read_index; ++m_checked_index)
    {       
        temp = m_read_buf[m_checked_index];
        if(temp == '\r')
        {
            if((m_checked_index + 1) == m_read_index)
            {
                return LINE_OPEN;
            }else if(m_read_buf[m_checked_index + 1] == '\n')
            {
                // 将‘\r’‘\n’变为‘\0’，同时将m_cheaked_index设置为下一行首位
                m_read_buf[m_checked_index++] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n')
        {
            if((m_checked_index > 1) && (m_read_buf[m_checked_index - 1] == '\r'))
            {
                m_read_buf[m_checked_index - 1] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    // 表示数据不完整，没有找到\r\n需要继续接收数据
    return LINE_OPEN;
}

// 将客户端请求的资源映射到内存中
http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file,web_root);
    // m_read_file:/home/user/cpp/webServer/root
    int len = strlen(web_root);

    if(m_post)
    {
        // post方法 need reWriting
    }
    else
    {
        strncpy(m_real_file + len,m_url,FILENAME_LEN - len -1);
        // m_read_file:/home/user/cpp/webServer/root/index.html
        if(stat(m_real_file,&m_file_stat) < 0)
        {
            return NO_RESOURCE;
        }
        // 判断访问权限
        if(!(m_file_stat.st_mode & S_IROTH))
        {
            return FORBIDDEN_REQUEST;
        }
        // 判断是否为目录
        if(S_ISDIR(m_file_stat.st_mode))
        {
            return BAD_REQUEST;
        }
        // 只读方式打开文件
        int fd = open(m_real_file, O_RDONLY);
        // 创建内存映射
        m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        return FILE_REQUEST;
    }
    

}

// munmap操作
void http_conn::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address,m_file_stat.st_size);
        m_file_address = 0;
    }
}

// 解析请求行
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    // text:GET / HTTP/1.1
    m_url = strpbrk(text," \t");
    // m_url: / HTTP/1.1
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    
    *m_url++ = '\0';
    // text:GET'\0'/ HTTP/1.1
    // m_url:/ HTTP/1.1

    char* method = text;    //method:GET
    if(strcasecmp(method,"GET") == 0)
    {
        m_method = GET;
    }else if(strcasecmp(method,"POST") == 0)
    {
        m_method = POST;
        m_post = true;
    }
    else return BAD_REQUEST;

    m_version = strpbrk(m_url," \t");   // m_version: HTTP/1.1
    if (!m_version)
    {
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    // m_version:HTTP/1.1
    // m_url:/
    if(strcasecmp(m_version,"HTTP/1.1") != 0) return BAD_REQUEST;

    // http://192.168.1.164:10000/xxx
    if(strncasecmp(m_url,"http://",7) == 0)
    {
        // m_url忽略掉前面的http://
        m_url += 7; // m_url:192.168.1.164:10000/xxx
        // 接下来要找到xxx
        m_url = strchr(m_url,'/');  // m_url:/xxx
    }

    if(!m_url || m_url[0] != '/') return BAD_REQUEST;

    m_check_state = CHECK_STATE_HEADER; // 改变主状态机的检查状态为检查请求头

    return NO_REQUEST;    
}
// 解析请求头
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    // 判断是空行还是请求头
    if(text[0] == '\0')
    {
        // 如果是空行检测contest-length是否为0，如果为0则代表GET请求解析结束，否则代表还有请求体
        if(m_content_length == 0)
        {
            return GET_REQUEST;
        }
        else
        {   // 去解析请求体
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
    }
    else if(strncasecmp(text,"Connection:",11) == 0)
    {
        // Connection: keep-alive
        text += 11;
        text += strspn(text," \t"); // 跳过空格
        // text:keep-alive
        if(strcasecmp(text,"keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if(strncasecmp(text,"Content-Length:",15) == 0)
    {
        // Content-length: xxx
        text += 15;
        text += strspn(text," \t"); // 跳过空格
        // text:xxx
        m_content_length = atol(text);  // 转换
    }
    else if(strncasecmp(text,"Host:",5) == 0)
    {
        // Host: xxx
        text += 5;
        text += strspn(text," \t"); // 跳过空格
        // text:xxx
        m_host = text;
    }
    else if(strncasecmp(text,"User-Agent:",11) == 0)
    {
        // User-Agent: xxx
        text += 11;
        text += strspn(text," \t"); // 跳过空格
        // text:xxx
        m_user_agent = text;
    }
    else
    {
        // printf("oop!unknow header: %s\n",text);
    }
    return NO_REQUEST;
}
// 解析请求体
// 只是检测数据是否被完整读入，没有真正解析
// need reWriting
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_index >= (m_content_length + m_checked_index))
    {
        text[m_content_length] = '\0';
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}



// 往缓冲区中写入待发送的数据
bool http_conn::add_response( const char* format, ... )
{
    if(m_write_index >= WRITE_BUFFER_SIZE)
    {
        return false;
    }
    va_list arg_list;
    va_start(arg_list,format);
    int len = vsnprintf(m_write_buf + m_write_index,WRITE_BUFFER_SIZE - 1 - m_write_index,format,arg_list);
    if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_index))
    {
        return false;
    }
    m_write_index += len;   // 更新索引
    va_end(arg_list);
    return true;
}
// 添加状态行
bool http_conn::add_status_line( int status, const char* title )
{
    return add_response("%s %d %s\r\n","HTTP/1.1",status,title);
}
// 添加响应头
bool http_conn::add_headers( int content_length )
{
    add_content_length(content_length);
    add_content_type();
    add_linger();
    add_blank_line();
}
// 添加Content_Length
bool http_conn::add_content_length( int content_length )
{
    return add_response("Content_Length: %d\r\n",content_length);
}
// 添加Connection
bool http_conn::add_linger()
{
    return add_response("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
// 添加响应正文
bool http_conn::add_content( const char* content )
{
    return add_response("%s", content);
}
// 添加响应类型
bool http_conn::add_content_type()
{
    return add_response("Content_Type: %s\r\n","text/html");
}
// 添加空行
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}



// 生成响应
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case NO_RESOURCE:
    {
        add_status_line(404,error_404_title);
        add_headers(strlen(error_404_form));
        if(!add_content(error_404_form))
        {
            return false;
        }
        break;
    }
    case INTERNAL_ERROR:
    {
        add_status_line(500,error_500_title);
        add_headers(strlen(error_500_form));
        if(!add_content(error_500_form))
        {
            return false;
        }
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(400,error_400_title);
        add_headers(strlen(error_400_form));
        if(!add_content(error_400_form))
        {
            return false;
        }
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403,error_403_title);
        add_headers(strlen(error_403_form));
        if(!add_content(error_403_form))
        {
            return false;
        }
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200,ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size);
            // 写缓冲区
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_index;
            // 内存映射
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            m_bytes_to_send = m_write_index + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if (!add_content(ok_string))
                return false;
        }
        break;
    }


    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_index;
    m_iv_count = 1;
    m_bytes_to_send = m_write_index;
    return true;
}

// 由线程池的工作线程调用，处理HTTP请求的入口函数
void http_conn::process()
{
    // 解析HTTP请求

    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST)
    {
        mod_fd(m_epollfd, m_sockfd, EPOLLIN, m_trig_mode);
        return;
    }

    // 生成响应
    bool write_ret = process_write(read_ret);
    if(!write_ret)
    {
        conn_close();
    }
    mod_fd(m_epollfd, m_sockfd, EPOLLOUT, m_trig_mode);
}


