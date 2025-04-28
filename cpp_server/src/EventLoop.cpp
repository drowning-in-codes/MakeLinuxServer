#include <Channel.hpp>
#include <Epoll.hpp>
#include <EventLoop.hpp>
#include <unistd.h>
/*
    事件循环
*/
void EventLoop::loop() {
  while (true) {
    // 轮询事件
    for (auto ch : _ep->poll_channel()) {
      // 可读事件
      ch->handleEvent();
    }
  }
}

EventLoop::EventLoop() { _ep = std::make_unique<Epoll>(); }

void EventLoop::updateChannel(Channel *channel) { _ep->updateChannel(channel); }
void EventLoop::deleteChannel(Channel *channel) { _ep->deleteChannel(channel); }
