#include <Channel.hpp>
#include <Epoll.hpp>
#include <strings.h>
#include <sys/epoll.h>
#include <unistd.h>
Epoll::~Epoll() {
  if (epoll_fd != -1) {
    ::close(epoll_fd);
  }
  delete[] events;
}

void Epoll::addfd(int fd) { addfd(fd, EPOLLIN | EPOLLOUT); }
void Epoll::addfd(int fd, uint32_t op) {
  epoll_event event;
  event.data.fd = fd;
  event.events = op;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
    throw std::runtime_error("Failed to add file descriptor to epoll");
  }
}
void Epoll::addfd(int fd, void *ptr, uint32_t op) {
  epoll_event event;
  event.data.ptr = ptr;
  event.events = op;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
    throw std::runtime_error("Failed to add file descriptor to epoll");
  }
}

std::vector<Channel *> Epoll::poll_channel(int timeout) {
  int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout);
  if (num_events == -1) {
    throw std::runtime_error("Epoll wait failed");
  }
  std::vector<Channel *> active_channels;
  for (int i = 0; i < num_events; i++) {
    Channel *channel = static_cast<Channel *>(events[i].data.ptr);
    channel->setReadyEvents(events[i].events);
    active_channels.push_back(channel);
  }
  return active_channels;
}
void Epoll::updateChannel(Channel *channel) {
  epoll_event event;
  bzero(&event, sizeof(event));
  event.data.ptr = channel;
  event.events = channel->getListenEvents();
  if (channel->getInEpoll()) {
    // 如果在epoll中,修改事件
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, channel->getFd(), &event);
  } else {
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, channel->getFd(), &event);
    channel->setInEpoll(true);
  }
}

void Epoll::deleteChannel(Channel *channel) {
  if (channel->getInEpoll()) {
    // 如果在epoll中,修改事件
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, channel->getFd(), nullptr);
  }
  channel->setInEpoll(false);
}