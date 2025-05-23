#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include "../lock/locker.h"
#include <iostream>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>

template <typename T> class block_queue {
public:
  block_queue(int max_size = 1000) {
    if (max_size <= 0) {
      exit(-1);
    }
    m_max_size = max_size;
    m_array = new T[max_size];
    m_size = 0;
    m_front = -1;
    m_back = -1;
  }
  void clear() {
    m_mutex.lock();
    m_size = 0;
    m_front = -1;
    m_back = -1;
    m_mutex.unlock();
  }
  ~block_queue() {
    // 释放内存
    m_mutex.lock();
    if (m_array != NULL) {
      delete[] m_array;
    }

    m_mutex.unlock();
  }

  bool full() {
    // 判断队列是否满
    m_mutex.lock();
    if (m_size >= m_max_size) {
      m_mutex.unlock();
      return true;
    }
    m_mutex.unlock();
    return false;
  }

  // 判断队列是否空
  bool empty() {
    m_mutex.lock();
    if (m_size == 0) {
      m_mutex.unlock();
      return true;
    }
    m_mutex.unlock();
    return false;
  }

  // 返回队首元素
  bool front(T &value) {
    m_mutex.lock();
    if (m_size == 0) {
      m_mutex.unlock();
      return false;
    } else {
      value = m_array[m_front];
      m_mutex.unlock();
      return true;
    }
  }

  // 返回队尾元素
  bool back(T &value) {
    m_mutex.lock();
    if (m_size == 0) {
      m_mutex.unlock();
      return false;
    } else {
      value = m_array[m_back];
      m_mutex.unlock();
      return true;
    }
  }

  int size() {
    int tmp = 0;
    m_mutex.lock();
    tmp = m_size;
    m_mutex.unlock();
    return tmp;
  }

  int max_size() {
    int tmp = 0;
    m_mutex.lock();
    tmp = m_max_size;
    m_mutex.unlock();
    return tmp;
  }

  // 向队列添加元素
  bool push(const T &item) {
    //往队列添加元素，需要将所有使用队列的线程先唤醒
    //当有元素push进队列,相当于生产者生产了一个元素
    //若当前没有线程等待条件变量,则唤醒无意义
    m_mutex.lock();
    if (m_size >= m_max_size) {
      // 队列已满 阻塞 唤醒消费者
      m_cond.broadcast();
      m_mutex.unlock();
      return false;
    }
    m_back = (m_back + 1) % m_max_size;
    m_array[m_back] = item;
    m_size++;

    m_cond.broadcast();
    m_mutex.unlock();
    return true;
  }

  // pop时,如果当前队列没有元素,将会等待条件变量
  bool pop(T &item) {
    m_mutex.lock();
    while (m_size <= 0) {
      if (!m_cond.wait(m_mutex.get())) {
        // 等待失败
        m_mutex.unlock();
        return false;
      }
    }
    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
  }

  bool pop(T &item, int ms_timeout) {
    // 超时处理
    // 等待ms_timeout 如果
    timespec t = {0, 0};
    timeval now = {0, 0};
    gettimeofday(&now, NULL);
    m_mutex.lock();
    if (m_size <= 0) {
      // 队列为空时
      t.tv_sec = now.tv_sec + ms_timeout / 1000;
      t.tv_nsec = (ms_timeout % 1000) * 1000;
      if (!m_cond.timewait(m_mutex.get(), t)) {
        // 等待失败
        m_mutex.unlock();
        return false;
      }
      /*
         if (m_size <= 0) {
            // 队列依旧为空时
            m_mutex.unlock();
            return false;
        }
      */
    }
    if (m_size <= 0) {
      // 队列依旧为空时
      m_mutex.unlock();
      return false;
    }
    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
  }

private:
  locker m_mutex;
  cond m_cond;
  T *m_array;
  int m_size;
  int m_max_size;
  int m_front;
  int m_back;
};

#endif