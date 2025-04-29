#include <Channel.hpp>
#include <CurrentThread.hpp>
#include <Epoll.hpp>
#include <EventLoop.hpp>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <unistd.h>
/*
    事件循环
*/
EventLoop::EventLoop()
    : _ep(std::make_unique<Epoll>()),
      wakeupFd(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {
  wakeUpChannel = std::make_unique<Channel>(this, wakeupFd, true);
  calling_functors = false;
  wakeUpChannel->setReadCallback([this] { handleRead(); });
  wakeUpChannel->enableRead();
}

void EventLoop::handleRead() {
  uint64_t one = 1;
  ssize_t n = ::read(wakeupFd, &one, sizeof(one));
  assert(n == sizeof(one));
}
EventLoop::~EventLoop() { ::close(wakeupFd); }
void EventLoop::loop() {
  _tid = CurrentThread::gettid();
  while (true) {
    // 轮询事件
    for (auto ch : _ep->poll_channel()) {
      // 可读事件
      ch->handleEvent();
    }
    // 事件遍历完毕,开始处理可能的结束事件
    // 在一个连接被delete时,会将一个回调加入to_do_list中,这个回调将epoll中的fd移除
    DoToDoList();
  }
}
void EventLoop::DoToDoList() {
  calling_functors = true;
  std::vector<std::function<void()>> tmp;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    // 交换任务队列
    toDoList.swap(tmp);
  }
  for (const auto &func : tmp) {
    // 执行任务
    func();
  }
  calling_functors = false;
}
void EventLoop::QueueOneFunc(const std::function<void()> &func) {
  // 将函数添加到任务队列中
  {
    std::unique_lock<std::mutex> lock(mutex_);
    toDoList.emplace_back(std::move(func));
  }
  // 唤醒事件循环
  if (!isInLoopThread() || calling_functors) {
    printf("  // 唤醒事件循环");
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd, &one, sizeof(one));
    assert(n == sizeof(one));
  }
}
bool EventLoop::isInLoopThread() {
  // 判断当前线程是否是事件循环线程
  return CurrentThread::gettid() == _tid;
}
void EventLoop::runOneFunc(const std::function<void()> &func) {
  // 直接执行函数
  if (isInLoopThread()) {
    printf("EventLoop::runOneFunc in loop thread\n");
    func();
  } else {
    // 将关闭回调放在自己的队列中
    QueueOneFunc(func);
  }
}

void EventLoop::updateChannel(Channel *channel) { _ep->updateChannel(channel); }
void EventLoop::deleteChannel(Channel *channel) { _ep->deleteChannel(channel); }
