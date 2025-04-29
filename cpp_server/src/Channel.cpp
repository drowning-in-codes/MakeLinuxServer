#include <Channel.hpp>
#include <EventLoop.hpp>
#include <strings.h>
#include <thread>

Channel::Channel(EventLoop *t_loop, int fd, bool blocking)
    : loop(t_loop), fd(fd), listenEvents(0), readyEvents(0), inEpoll(false) {
  if (!blocking) {
    setNonBlocking(fd);
  }
}

void Channel::enableReadAndET() {
  listenEvents |= EPOLLIN | EPOLLPRI | EPOLLET;
  //   将server的channel添加到epoll中
  loop->updateChannel(this);
}
void Channel::enableRead() {
  listenEvents |= EPOLLIN | EPOLLPRI;
  //   将server的channel添加到epoll中
  loop->updateChannel(this);
}
void Channel::enableWrite() {
  listenEvents |= EPOLLOUT;
  //   将server的channel添加到epoll中
  loop->updateChannel(this);
}

void Channel::enableET() {
  listenEvents |= EPOLLET;
  loop->updateChannel(this);
}
void Channel::disableRead() {
  listenEvents &= ~(EPOLLIN | EPOLLPRI);
  loop->updateChannel(this);
}
void Channel::disableWrite() {
  listenEvents &= ~EPOLLOUT;
  loop->updateChannel(this);
}
void Channel::disableET() {
  listenEvents &= ~EPOLLET;
  loop->updateChannel(this);
}
void Channel::disableOpts(uint32_t opts) {
  listenEvents &= ~opts;
  loop->updateChannel(this);
}
void Channel::enableOpts(uint32_t opts) {
  listenEvents |= opts;
  loop->updateChannel(this);
}
void Channel::handleEvent() const {
  // 根据事件类型处理对应的回调函数
  if (tied) {
    // 增加引用计数
    auto guard = m_tie.lock();
    handleEventWithGuard();
  }else{
    handleEventWithGuard();
  }
}
void Channel::Tie(const std::shared_ptr<void> &ptr) {
  tied = true;
  m_tie = ptr;
}

void Channel::handleEventWithGuard() const {
  if (readyEvents & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
    if (readCallback) {
      readCallback();
    }
  }
  if (readyEvents & EPOLLOUT) {
    if (writeCallback) {
      printf("writeCallback\n");
      writeCallback();
    }
  }
}