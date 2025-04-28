#pragma once
#include "Channel.hpp"
#include "InetAddress.hpp"
#include "utils/log.hpp"
#include <EventLoop.hpp>
#include <SerSocket.hpp>
#include <utils/utils.hpp>
class Acceptor {
  // Acceptor类负责接受连接
private:
  EventLoop *eventLoop;
  SerSocket *socket;
  InetAddress *addr;
  Channel *acceptChannel;

  // 当获取到客户fd时用于创建连接的回调函数
  std::function<void(int)> newConnectionCallback;
  Logger *logger = new ConsoleLogger();

public:
  DISALLOW_COPY_AND_MOVE(Acceptor);
  Acceptor(EventLoop *t_loop,
           uint16_t port = ConfiguraionConstants::DEFAULT_PORT,
           std::string_view ip = ConfiguraionConstants::DEFAULT_IP,
           SerSocket *t_socket = new SerSocket());
  Acceptor(EventLoop *t_loop, InetAddress *t_addr,
           SerSocket *t_socket = new SerSocket())
      : Acceptor(t_loop, t_addr->getPort(), t_addr->getIp(), t_socket){};
  Acceptor(EventLoop *t_loop, int fd, InetAddress t_addr)
      : Acceptor(t_loop, &t_addr, new SerSocket(fd)){};
  Acceptor(EventLoop *t_loop, int fd, InetAddress *t_addr)
      : Acceptor(t_loop, fd, *t_addr){};
  Acceptor(EventLoop *t_loop, int fd, uint16_t port, std::string_view ip);
  ~Acceptor();

  // 当客户端进行连接的回调函数,是对应channel的readCallback
  void acceptConnection();

  // getter and setter
  void setNewConnectionCallback(std::function<void(int)> callback) {
    newConnectionCallback = callback;
  }
  void setSocket(SerSocket *t_socket) { socket = t_socket; }
  void setAddr(InetAddress *t_addr) { addr = t_addr; }
  void setAcceptChannel(Channel *t_acceptChannel) {
    acceptChannel = t_acceptChannel;
  }
  SerSocket *getSocket() { return socket; }
  InetAddress *getAddr() { return addr; }
  Channel *getAcceptChannel() { return acceptChannel; }
  void setEventLoop(EventLoop *t_eventLoop) { eventLoop = t_eventLoop; }
  void setNonBlocking() { socket->setNonBlocking(); }
  void setReuseAddr() { socket->setReuseAddr(); }
};