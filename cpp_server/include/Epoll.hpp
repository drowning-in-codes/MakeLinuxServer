#include <cstdint>
#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>
#include<vector>
#include <utils/utils.hpp>
class Epoll {
public:
  Epoll() : Epoll(10){};
  Epoll(int max_events) : MAX_EVENTS(max_events) {
    epoll_fd = epoll_create1(0);
    errif(epoll_fd <= 0, Message::EPOLL_CREATE_FAILED);
    events = new epoll_event[max_events];
    bzero(events, sizeof(epoll_event) * max_events);
  }
  ~Epoll();

  void addfd(int fd);
  void addfd(int fd,uint32_t op);
  std::vector<epoll_event> poll(int timeout);

private:
  epoll_event *events;
  int MAX_EVENTS;
  int epoll_fd;
  std::vector<epoll_event> active_events;
};