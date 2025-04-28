#pragma once
#include <Channel.hpp>
#include <cstdint>
#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>
#include <utils/utils.hpp>
#include <vector>
class Channel;
class Epoll {
public:
  DISALLOW_COPY_AND_MOVE(Epoll);
  Epoll() : Epoll(10){};
  Epoll(int max_events) : MAX_EVENTS(max_events) {
    epoll_fd = epoll_create1(0);
    errif(epoll_fd <= 0, Message::EPOLL_CREATE_FAILED);
    events = new epoll_event[max_events];
    bzero(events, sizeof(epoll_event) * max_events);
  }
  ~Epoll();
  void updateChannel(Channel *channel);
  void deleteChannel(Channel *channel);
  void addfd(int fd);
  void addfd(int fd, uint32_t op);
  void addfd(int fd, void *ptr, uint32_t op);
  std::vector<Channel *> poll_channel(int timeout = 100);

private:
  epoll_event *events;
  int MAX_EVENTS;
  int epoll_fd;
};