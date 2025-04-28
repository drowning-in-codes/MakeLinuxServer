#include <CliSocket.hpp>
#include <cstring>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
CliSocket::CliSocket(int domain, int type, int protocol) {
  fd = socket(domain, type, protocol);
  if (fd < 0) {
    throw std::runtime_error("Failed to create socket");
  }
}


CliSocket::~CliSocket() {
  if (fd != -1) {
    close(fd);
    fd = -1;
  }
}