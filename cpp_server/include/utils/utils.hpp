#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
inline void errif(bool condition, const std::string &msg) {
  if (condition) {
    std::string prefix = std::string(strerror(errno)) + ":";
    throw std::runtime_error(prefix + msg);
  }
}
inline void setNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    throw std::runtime_error("Failed to get socket flags");
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    throw std::runtime_error("Failed to set socket to non-blocking");
  }
}
class Message {
public:
  static inline const std::string EPOLL_CREATE_FAILED = "epoll_create failed";
};