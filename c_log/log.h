#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include "block_queue.h"

class Log
{
public:
    static Log* get_instance()
    {
        static Log instance;
        return &instance;
    }

    static void* flush_log_thread(void *args)
    {
        Log::get_instance()->async_write_log();
    }
    // 初始化
    bool init(const char *file_name,int close_log,int log_buf_size = 8192,int split_lines = 5000000,int max_queue_size = 0);
    // 将输出内容按照标准格式整理
    void write_log(int level, const char *format, ... );
    // 刷新缓冲区
    void flush(void);

private:
    Log();
    virtual ~Log();
    // 异步写日志
    void* async_write_log()
    {
        std::string single_log;
        // 在阻塞队列中取出一条日志内容，写入文件
        while(m_log_queue->pop(single_log))
        {
            m_mutex.lock();
            fputs(single_log.c_str(),m_fp);
            m_mutex.unlock();
        }
    }

private:
    char dir_name[128];                     // 路径名
    char log_name[128];                     // 文件名
    int m_split_lines;                      // 日志最大行数
    int m_log_buf_size;                     // 日志缓冲区大小
    long long m_count;                      // 日志行数记录
    int m_today;                            // 按天分类，当前是第几天
    FILE *m_fp;                             // 打开log的文件指针
    char *m_buf;                            // 要输出的内容
    block_queue<std::string> *m_log_queue;  // 阻塞队列
    bool m_is_async;                        // 异步标志
    locker m_mutex;                         // 同步类
    int m_close_log;                        // 关闭日志
};

// 用于在其他文件中输出不同类型的日志输出
#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}


#endif