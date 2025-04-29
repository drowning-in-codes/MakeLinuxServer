#include <CurrentThread.hpp>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
void boo(std::function<void()> callback) {
  std::cout << CurrentThread::tid() << '\n';

  callback();
}
class ThreadLocal {
public:
  void callMyName() {
    std::cout << "Thread ID: " << CurrentThread::tid() << std::endl;
  }
};

int main() {
  std::cout << CurrentThread::tid() << '\n';
  auto threadLocal = std::make_shared<ThreadLocal>();
  threadLocal->callMyName();
  auto getId = [threadLocal]() {
    std::cout << CurrentThread::tid() << '\n';
    threadLocal->callMyName();
  };
  std::thread t1(boo, getId);
  t1.join();

  return 0;
}