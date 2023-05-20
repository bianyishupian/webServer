#include "./config/config.h"


int main(int argc, char* argv[])
{
    

    // 解析命令行
    Config config;
    config.parse_arg(argc, argv);

    Server server;
    server.init(config.port,config.opt_linger,config.trig_mod,
                config.thread_num,config.actor_model,config.log_write,config.close_log);
    
    // 日志
    server.log_write();

    // 线程池
    server.thread_pool_c();
    // 触发模式
    server.trig_mode();
    // 监听
    server.event_listen();
    // 运行
    server.event_loop();

    return 0;
}