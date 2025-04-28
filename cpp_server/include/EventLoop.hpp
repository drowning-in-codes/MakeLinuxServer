#pragma once
#include "ThreadPool.hpp"
#include "utils/utils.hpp"
#include <Channel.hpp>
#include <Epoll.hpp>
class EventLoop {
  DISALLOW_COPY_AND_MOVE(EventLoop);

private:
  std::unique_ptr<Epoll> _ep;

public:
  void addChannel(Channel *channel) {
    _ep->addfd(channel->getFd(), channel, channel->getListenEvents());
  }
  EventLoop();
  void loop();
  void updateChannel(Channel *channel);
  void deleteChannel(Channel *channel);
  Epoll *getEpoll() { return _ep.get(); }
};