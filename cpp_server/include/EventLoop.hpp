#pragma once
#include "ThreadPool.hpp"
#include "utils/utils.hpp"
#include <Channel.hpp>
#include <Epoll.hpp>
class EventLoop {
  DISALLOW_COPY_AND_MOVE(EventLoop);

private:
  std::unique_ptr<Epoll> _ep;
  std::vector<std::function<void()>> toDoList;
  std::mutex mutex_;
  bool calling_functors;

public:
  void addChannel(Channel *channel) {
    _ep->addfd(channel->getFd(), channel, channel->getListenEvents());
  }
  void handleRead();
  EventLoop();
  ~EventLoop();
  int _tid;
  int wakeupFd;
  std::unique_ptr<Channel> wakeUpChannel;
  bool isInLoopThread();
  void DoToDoList();
  void runOneFunc(const std::function<void()> &func);
  void QueueOneFunc(const std::function<void()> &func);
  void loop();
  void updateChannel(Channel *channel);
  void deleteChannel(Channel *channel);
  Epoll *getEpoll() { return _ep.get(); }
};