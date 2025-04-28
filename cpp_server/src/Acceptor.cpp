
#include "InetAddress.hpp"
#include "SerSocket.hpp"
#include <Acceptor.hpp>
#include <functional>
#include <iostream>
Acceptor::Acceptor(EventLoop *t_loop, uint16_t port, std::string_view ip,SerSocket *t_socket)
    : eventLoop(t_loop), socket(t_socket),
      addr(new InetAddress(ip, port)) {
  socket->setReuseAddr();
  socket->bind(addr);
  socket->listen();
  acceptChannel = new Channel(eventLoop, socket->get_fd(),true);
  acceptChannel->setReadCallback([this]() { acceptConnection(); });
  acceptChannel->enableRead();
}

void Acceptor::acceptConnection() {
  InetAddress *addr = new InetAddress();
  //   获得连接的客户端的地址以及fd
  int fd = socket->accept(addr);
  if (fd < 0) {
    throw std::runtime_error("Failed to accept connection");
  }
  logger->logInfo("Accepted connection from " + addr->getIp() + ":" +
                  std::to_string(addr->getPort()));
  delete addr;
  newConnectionCallback(fd);
}

Acceptor::~Acceptor() {
  delete socket;
  delete addr;
  delete acceptChannel;
}