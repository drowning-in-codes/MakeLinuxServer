#include <SerSocket.hpp>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
SerSocket::SerSocket(int domain, int type, int protocol) {
  fd = socket(domain, type, protocol);
  if (fd < 0) {
    throw std::runtime_error("Failed to create socket");
  }
}

void SerSocket::bind(InetAddress &addr) {
  if (::bind(fd, (struct sockaddr *)&addr.addr, addr.addr_len) < 0) {
    throw std::runtime_error(std::string(strerror(errno)) + "Failed to bind socket");
  }
}
void SerSocket::bind(InetAddress* addr) {
  if (::bind(fd, (struct sockaddr *)&addr->addr, addr->addr_len) < 0) {
    throw std::runtime_error(std::string(strerror(errno)) + "Failed to bind socket");
  }
}
void SerSocket::listen(int backlog) {
  if (::listen(fd, backlog) < 0) {
    throw std::runtime_error("Failed to listen on socket");
  }
}

int SerSocket::accept(InetAddress &addr) {
  socklen_t addr_len = sizeof(addr.addr);
  int new_fd = ::accept(fd, (struct sockaddr *)&addr.addr, &addr_len);
  if (new_fd < 0) {
    throw std::runtime_error("Failed to accept connection");
  }
  return new_fd;
}

int SerSocket::accept(InetAddress* addr) {
  socklen_t addr_len = sizeof(addr->addr);
  int new_fd = ::accept(fd, (struct sockaddr *)&addr->addr, &addr_len);
  if (new_fd < 0) {
    throw std::runtime_error("Failed to accept connection");
  }
  return new_fd;
}


SerSocket::~SerSocket() {
  if (fd != -1) {
    close(fd);
    fd = -1;
  }
}