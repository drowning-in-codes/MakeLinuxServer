#include <Epoll.hpp>
#include <sys/epoll.h>
#include <unistd.h>

Epoll::~Epoll() {
  if (epoll_fd != -1) {
    close(epoll_fd);
    epoll_fd = -1;
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
std::vector<epoll_event> Epoll::poll(int timeout) {
  int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout);
  if (num_events == -1) {
    throw std::runtime_error("Epoll wait failed");
  }
  active_events.assign(events, events + num_events);
  return active_events;
}