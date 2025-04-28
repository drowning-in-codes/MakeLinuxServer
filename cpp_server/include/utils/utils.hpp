#pragma once
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>

#define DISALLOW_COPY(cname)                                                   \
  cname(const cname &) = delete;                                               \
  cname &operator=(const cname &) = delete;

#define DISALLOW_MOVE(cname)                                                   \
  cname(cname &&) = delete;                                                    \
  cname &operator=(cname &&) = delete;

#define DISALLOW_COPY_AND_MOVE(cname)                                          \
  DISALLOW_COPY(cname);                                                        \
  DISALLOW_MOVE(cname);

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
inline void setBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    throw std::runtime_error("Failed to get socket flags");
  }
  if (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
    throw std::runtime_error("Failed to set socket to blocking");
  }
}
inline void setBlock(int fd, bool blocking) {
  if (blocking) {
    setBlocking(fd);
  } else {
    setNonBlocking(fd);
  }
}
inline void reuseAddr(int fd) {
  int opt = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    throw std::runtime_error("Failed to set socket option");
  }
}
class Message {
public:
  static inline const std::string EPOLL_CREATE_FAILED = "epoll_create failed";
};

class ConfiguraionConstants {
public:
  static const int MAX_EVENTS = 1024;
  static const int MAX_BUFFER_SIZE = 1024;
  static const int MAX_CONNECTIONS = 100;
  static const uint16_t DEFAULT_PORT = 2233;
  static inline const char *DEFAULT_IP = "127.0.0.1";
};

class TimeUtils {
public:
  static std::string getCurrentTime(const char *format = "%Y-%m-%d %H:%M:%S") {
    auto time = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(time);
    std::tm *ltm = std::localtime(&currentTime);
    // Format the time using strftime
    char buffer[80];
    strftime(buffer, sizeof(buffer), format, ltm);
    return std::string(buffer);
  }
};