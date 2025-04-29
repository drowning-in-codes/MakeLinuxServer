#pragma once
#include "SerSocket.hpp"
#include "utils/utils.hpp"
#include <cstdint>
#include <functional>
#include <memory>
class EventLoop;
class Channel {
  /**
    Channel类负责处理事件的回调

  */

private:
  EventLoop *loop;
  bool useThreadPool = false;
  std::function<void()> readCallback;
  std::function<void()> writeCallback;
  bool useET = false;
  uint32_t listenEvents;
  uint32_t readyEvents;
  bool inEpoll;
  int fd;
  std::weak_ptr<void> m_tie;
public:
  DISALLOW_COPY_AND_MOVE(Channel);
  Channel(EventLoop *t_loop, int fd, bool blocking = false);
  Channel(EventLoop *t_loop, const SerSocket *socket, bool blocking = false)
      : Channel(t_loop, socket->get_fd(), blocking) {}

  // channel回调事件
  void handleEvent() const;
  void handleEventWithGuard() const;
  void Tie(const std::shared_ptr<void>& ptr);
  // 设置epoll相关事件
  void enableRead();
  void enableReadAndET();
  void enableWrite();
  void enableET();
  void enableOpts(uint32_t opts);

  // 禁止epoll相关事件
  void disableRead();
  void disableWrite();
  void disableET();
  void disableOpts(uint32_t opts);

  // 
  bool tied;

  // getter and setter
  void setUseThreadPool(bool use) { useThreadPool = use; }
  void setWriteCallback(std::function<void()> callback) {
    writeCallback = callback;
  }
  void setReadCallback(std::function<void()> callback) {
    readCallback = callback;
  }
  EventLoop *getEpoll() { return loop; }
  void setEpoll(EventLoop *t_ep) { loop = t_ep; }
  int getFd() const { return fd; }
  void setFd(int t_fd) { fd = t_fd; }
  void setInEpoll(bool in) { inEpoll = in; }
  bool getInEpoll() const { return inEpoll; }
  void setUseET(bool et) { useET = et; }
  bool getUseET() const { return useET; }
  uint32_t getListenEvents() const { return listenEvents; }
  uint32_t getReadyEvents() const { return readyEvents; }
  void setListenEvents(uint32_t events) { listenEvents = events; }
  void setReadyEvents(uint32_t events) { readyEvents = events; }
};