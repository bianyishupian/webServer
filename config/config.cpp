#include "config.h"

Config::Config()
{
    port = 10000;           // 端口号，默认10000
    log_write = 0;          // 日志写入方式，默认同步
    trig_mod = 0;           // 触发模式组合，默认 LT + LT
    listen_trigmod = 0;     // listenfd触发模式，默认LT
    conn_trigmod = 0;       // connfd触发模式，默认LT
    opt_linger = 0;         // 优雅关闭链接，默认不使用

    thread_num = 8;         // 线程池线程数量，默认为8
    close_log = 0;          // 是否关闭日志，默认不关闭
    actor_model = 0;        // 并发模型，默认是同步I/O模拟的proactor
}

// 解析命令行
void Config::parse_arg(int argc, char *argv[])
{
    int opt;
    const char *str = "p:l:m:o:s:t:c:a:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'p':
            port = atoi(optarg);
            break;
        case 'l':
            log_write = atoi(optarg);
            break;
        case 'm':
            trig_mod = atoi(optarg);
            break;
        case 'o':
            opt_linger = atoi(optarg);
            break;
        case 't':
            thread_num = atoi(optarg);
            break;
        case 'c':
            close_log = atoi(optarg);
            break;
        case 'a':
            actor_model = atoi(optarg);
            break;
        default:
            break;
        }
    }
    
}