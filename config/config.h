#ifndef CONFIG_H
#define CONFIG_H

#include "../m_server/server.h"

class Config
{
public:
    Config();
    ~Config() {};

    // 解析命令行
    void parse_arg(int argc, char *argv[]);

    int port;             // 端口号
    int log_write;          // 日志写入方式
    int trig_mod;;          // 触发组合模式
    int listen_trigmod;     // listenfd触发模式
    int conn_trigmod;       // onnfd触发模式
    int opt_linger;         // 优雅关闭连接

    int thread_num;         // 线程池内的线程数量
    int close_log;          // 是否关闭日志
    int actor_model;        // 发模型选择

};

#endif