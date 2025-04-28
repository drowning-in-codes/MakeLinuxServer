#pragma once

#include <InetAddress.hpp>
#include <asm-generic/socket.h>
#include <fcntl.h>
#include <stdexcept>
#include <sys/socket.h>
class CliSocket {
public:
  CliSocket() : CliSocket(AF_INET, SOCK_STREAM, 0) {}
  CliSocket(int domain, int type, int protocol);
  CliSocket(int t_fd) : fd(t_fd){};
  ~CliSocket();

  void setReuseAddr() {
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
      throw std::runtime_error("Failed to set socket option");
    }
  }
  void connect(InetAddress *addr) {
    if (::connect(fd, (struct sockaddr *)&addr->addr, addr->addr_len) < 0) {
      throw std::runtime_error("Failed to connect to server");
    }
  }
  void connect(InetAddress &addr) {
    if (::connect(fd, (struct sockaddr *)&addr.addr, addr.addr_len) < 0) {
      throw std::runtime_error("Failed to connect to server");
    }
  }
  int get_fd() const { return fd; }
  void setNonBlocking() {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
      throw std::runtime_error("Failed to get socket flags");
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
      throw std::runtime_error("Failed to set socket to non-blocking");
    }
  }

private:
  int fd;
};