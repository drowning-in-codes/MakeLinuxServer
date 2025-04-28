#pragma once

#include "EventLoop.hpp"
#include "SerSocket.hpp"
#include "utils/utils.hpp"
#include <Acceptor.hpp>
#include <Connection.hpp>
#include <cstdint>
#include <unordered_map>
#include <utils/log.hpp>
class Server {
private:
  DISALLOW_COPY_AND_MOVE(Server);
  std::unique_ptr<EventLoop> mainReactor;
  std::vector<std::unique_ptr<EventLoop>> subReactor;
  std::unique_ptr<ThreadPool> threadPool;
  const static inline uint16_t m_port = ConfiguraionConstants::DEFAULT_PORT;
  const static inline char *m_ip = ConfiguraionConstants::DEFAULT_IP;
  const static uint32_t BUFFER_SIZE = 1024;
  std::unique_ptr<Logger> logger;
  std::unique_ptr<Acceptor> serAcceptor;
  std::unordered_map<int, Connection *> connections;
  int nextConnId;
  std::function<void(Connection *)> onConnectCallback;

public:
  Server(uint16_t port = m_port, const char *ip = m_ip);
  ~Server();
  void Start();
  void HandleNewConenction(int fd);
  void HandleClose(int fd);
  void onConnect(std::function<void(Connection *)> fn);
  // getter and setter
  uint32_t getBufferSize() const { return BUFFER_SIZE; }
  Acceptor *getAcceptor() { return serAcceptor.get(); }
  Logger *getLogger() { return logger.get(); }
};
