#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utils/utils.hpp>
class ThreadPool {
private:
  std::vector<std::thread> workers;
  std::queue<std::function<void()>> taskQueue;
  std::mutex queueMutex;
  std::condition_variable cv;
  bool stop;

public:
  DISALLOW_COPY_AND_MOVE(ThreadPool);
  ThreadPool(int numThreads = 2);
  ~ThreadPool();
  // void add(std::function<void()> task);
  int thread_num() { return workers.size(); }
  template <class F, class... Args> auto add(F &&f, Args &&...args) {
    using return_type = typename std::result_of<F(Args...)>::type;
    // auto task = std::make_shared<std::packaged_task<return_type()>>(
    //     [&args..., &f]() { return f(args...); });
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto res = task->get_future(); // 获取返回值
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      if (stop) {
        throw std::runtime_error("ThreadPool is stopped");
      }
      taskQueue.emplace([task]() { (*task)(); });
    }
    cv.notify_one(); // 通知一个线程
    return res;      // 返回future对象
  }
};