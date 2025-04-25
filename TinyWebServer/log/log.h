#ifndef LOG_H
#define LOG_H

#include "block_queue.h"
#include <iostream>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>
// 同步/异步日志系统
// ===============
// 同步/异步日志系统主要涉及了两个模块，一个是日志模块，一个是阻塞队列模块,其中加入阻塞队列模块主要是解决异步写入日志做准备.
// > * 自定义阻塞队列
// > * 单例模式创建日志
// > * 同步日志
// > * 异步日志
// > * 实现按天、超行分类

class Log {
public:
  static Log *get_instance() {
    static Log instance;
    return &instance;
  }
  static void *flush_log_thread(void *args) {
    Log::get_instance()->async_write_log();
    return nullptr;
  }
  bool init(const char *file_name, int close_log, int log_buf_size = 8192,
            int split_lines = 5000000, int max_queue_size = 0);

  void write_log(int level, const char *format, ...);
  void flush(void);

private:
  Log();
  virtual ~Log();
  void async_write_log() {
    std::string single_log;
    while (m_log_queue->pop(single_log)) {
      m_mutex.lock();
      fputs(single_log.c_str(), m_fp);
      m_mutex.unlock();
    }
  }

private:
  char dir_name[128]; // 路径名
  char log_name[128]; // log文件名
  int m_split_lines;  // 日志最大行数
  int m_log_buf_size; //日志缓冲区大小
  long long m_count;  // 日志行数记录
  int m_today;        // 记录当前时间
  FILE *m_fp;
  char *m_buf;
  block_queue<std::string> *m_log_queue; //阻塞队列
  bool m_is_async;                       // 是否同步标志位
  locker m_mutex;
  int m_close_log; // 关闭日志
};

// 如果 __VA_ARGS__ 没有参数，## 会移除前面的逗号，避免语法错误。 gun c
// extension
#define LOG_DEBUG(format, ...)                                                 \
  if (0 == m_close_log) {                                                      \
    LOG::get_instance()->write_log(0, format, ##__VA_ARGS__);                  \
    Log::get_instance()->flush();                                              \
  }
#define LOG_INFO(format, ...)                                                  \
  if (0 == m_close_log) {                                                      \
    Log::get_instance()->write_log(1, format, ##__VA_ARGS__);                  \
    Log::get_instance()->flush();                                              \
  }
#define LOG_WARN(format, ...)                                                  \
  if (0 == m_close_log) {                                                      \
    Log::get_instance()->write_log(2, format, ##__VA_ARGS__);                  \
    Log::get_instance()->flush();                                              \
  }
#define LOG_ERROR(format, ...)                                                 \
  if (0 == m_close_log) {                                                      \
    Log::get_instance()->write_log(3, format, ##__VA_ARGS__);                  \
    Log::get_instance()->flush();                                              \
  }

#endif