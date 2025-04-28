#include <ThreadPool.hpp>
#include <future>
#include <mutex>
ThreadPool::ThreadPool(int numThreads) : stop(false) {
  for (int i = 0; i < numThreads; i++) {
    workers.emplace_back([this]() {
      while (true) {
        // 每个线程都在循环中等待任务
        // 从任务阻塞队列中取出任务并执行
        // 如果队列为空就阻塞
        auto task = std::function<void()>();
        {
          std::unique_lock<std::mutex> lock(queueMutex);
          // 阻塞 除非有任务或者线程池停止
          cv.wait(lock, [this]() { return stop || !taskQueue.empty(); });
          if (stop && taskQueue.empty()) {
            return;
          }
          // 获取锁后
          task = taskQueue.front();
          taskQueue.pop();
        }
        task();
      }
    });
  }
}

// void ThreadPool::add(std::function<void()> task) {
//   {
//     std::unique_lock<std::mutex> lock(queueMutex);
//     if (stop) {
//       throw std::runtime_error("ThreadPool is stopped");
//     }
//     taskQueue.emplace(task);
//   }
//   cv.notify_one(); // 通知一个线程
// }


ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queueMutex);
    stop = true; // 设置停止标志
  }
  cv.notify_all(); // 通知所有线程
  for (auto &worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}