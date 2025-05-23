#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "../CGImysql/sql_connection_pool.h"
#include "../lock/locker.h"
#include <cstdio>
#include <exception>
#include <list>
#include <pthread.h>
// 半同步/半反应堆线程池
// ===============
// 使用一个工作队列完全解除了主线程和工作线程的耦合关系：主线程往工作队列中插入任务，工作线程通过竞争来取得任务并执行它。
// > * 同步I/O模拟proactor模式
// > * 半同步/半反应堆
// > * 线程池

template <typename T> class threadpool {
public:
  threadpool(int actor_model, connection_pool *connPool, int thread_number = 8,
             int max_request = 10000);
  ~threadpool();
  bool append(T *request, int state);
  bool append_p(T *request);

private:
  /*工作线程运行的函数,不断从工作队列中取出并运行 */
  static void *worker(void *arg);
  void run();

  int m_thread_number;  // 线程池中的线程数
  int m_max_requests;   // 请求队列中允许的最大请求数
  pthread_t *m_threads; // 描述线程池的数组,其大小为m_thread_number
  std::list<T *> m_workqueue;  // 请求队列
  locker m_queuelocker;        // 保护请求队列的互斥锁
  sem m_queuestat;             // 是否有任务需要处理
  connection_pool *m_connPool; // 数据库
  int m_actor_model;           // 模型切换
};

template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool,
                          int thread_number, int max_requests)
    : m_actor_model(actor_model), m_thread_number(thread_number),
      m_max_requests(max_requests), m_threads(NULL), m_connPool(connPool) {
  if (thread_number <= 0 || max_requests <= 0) {
    throw std::exception();
  }
  m_threads = new pthread_t[m_thread_number];
  if (!m_threads) {
    throw std::exception();
  }
  for (int i = 0; i < thread_number; ++i) {
    if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
      // 创建线程失败
      delete[] m_threads;
      throw std::exception();
    }
    if (pthread_detach(m_threads[i])) {
      // 分离线程
      // TODO: 最好通过join
      delete[] m_threads;
      throw std::exception();
    }
  }
};

template <typename T> threadpool<T>::~threadpool() { delete[] m_threads; }

template <typename T> bool threadpool<T>::append(T *request, int state) {
  // 加入请求队列
  m_queuelocker.lock();
  if (m_workqueue.size() >= m_max_requests) {
    // 工作请求已满
    m_queuelocker.unlock();
    return false;
  }
  request->m_state = state;
  // 加入请求队列
  m_workqueue.push_back(request);
  m_queuelocker.unlock();

  //请求队列增加 信号量+1 并且唤醒其他线程
  m_queuestat.post();
  return true;
}
template <typename T> bool threadpool<T>::append_p(T *request) {
  m_queuelocker.lock();
  if (m_workqueue.size() >= m_max_requests) {
    m_queuelocker.unlock();
    return false;
  }
  m_workqueue.push_back(request);
  m_queuelocker.unlock();
  m_queuestat.post();
  return true;
}

template <typename T> void *threadpool<T>::worker(void *arg) {
  // 线程运行的函数
  threadpool *pool = (threadpool *)arg;
  pool->run();
  return pool;
}

template <typename T> void threadpool<T>::run() {
  // 去除队列中的任务运行
  while (true) {
    // 请求队列信号量-1
    m_queuestat.wait();
    m_queuelocker.lock();
    if (m_workqueue.empty()) {
      m_queuelocker.unlock();
      continue;
    }
    T *request = m_workqueue.front();
    m_workqueue.pop_front();
    m_queuelocker.unlock();

    if (!request) {
      continue;
    }
    if (1 == m_actor_model) {
      // reactor模式 
      if (0 == request->m_state) {
        // 读
        if (request->read_once()) {
          // 读取数据成功
          request->improv = 1;
          connectionRAII mysqlcon(&request->mysql, m_connPool);
          // 处理数据
          request->process();
        } else {
          // 读失败
          request->improv = 1;
          request->timer_flag = 1;
        }
      } else {
        // 写
        if (request->write()) {
          request->imporv = 1;
        } else {
          request->improv = 1;
          request->timer_flag = 1;
        }
      }
    } else {
      // proactor模式
      connectionRAII mysqlcon(&request->mysql, m_connPool);
      request->process();
    }
  }
}

#endif