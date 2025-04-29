#include <Channel.hpp>
#include <Epoll.hpp>
#include <strings.h>
#include <sys/epoll.h>
#include <unistd.h>

#ifdef __linux__

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
#endif

#ifdef __MAC__
void Epoll::updateChannel(Channel *channel) {
  kevent ev[2];
  bzero(ev, 2 * sizeof(ev));
  int n = 0;
  int fd = channel->getFd();
  int op = EV_ADD;
  EV_SET(&ev[n++], fd, EVFILT_READ, op, 0, 0, ch);
  EV_SET(&ev[n++], fd, EVFILT_WRITE, op, 0, 0, ch);
  kevent(fd, ev, n, NULL, 0, NULL);
}

void Epoll::deleteChannel(Channel *ch) {
  struct kevent ev[2];
  int n = 0;
  int fd = ch->getFd();
  EV_SET(&ev[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, ch);
  EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, ch);
 kevent(fd_, ev, n, NULL, 0, NULL);
}

std::vector<Channel *> Poller::Poll(int timeout) {
  std::vector<Channel *> active_channels;
  struct timespec ts;
  memset(&ts, 0, sizeof(ts));
  if (timeout != -1) {
    ts.tv_sec = timeout / 1000;
    ts.tv_nsec = (timeout % 1000) * 1000 * 1000;
  }
  int nfds = 0;
  if (timeout == -1) {
    nfds = kevent(fd_, NULL, 0, events_, MAX_EVENTS, NULL);
  } else {
    nfds = kevent(fd_, NULL, 0, events_, MAX_EVENTS, &ts);
  }
  for (int i = 0; i < nfds; ++i) {
    Channel *ch = (Channel *)events_[i].udata;
    int events = events_[i].filter;
    if (events == EVFILT_READ) {
      ch->SetReadyEvents( /*ch->READ_EVENT*/ 1 | 4 /*ch->ET*/);
    }
    if (events == EVFILT_WRITE) {
      ch->SetReadyEvents(/*ch->WRITE_EVENT*/ 2| 4 /*ch->ET*/);
    }
    active_channels.push_back(ch);
  }
  return active_channels;
}
#endif
Epoll::~Epoll() {
  if (epoll_fd != -1) {
    ::close(epoll_fd);
  }
  delete[] events;
}