#include "Acceptor.hpp"
#include "InetAddress.hpp"
#include "SerSocket.hpp"
#include "utils/utils.hpp"
#include <Channel.hpp>
#include <Epoll.hpp>
#include <Server.hpp>
#include <cassert>
#include <memory>
#include <utils/utils.hpp>
Server::Server(uint16_t port, const char *ip) : nextConnId(1) {
  // 创建服务器  包括套接字和绑定监听地址
  logger = std::make_unique<ConsoleLogger>();
  mainReactor = std::make_unique<EventLoop>();
  //   设置server的channel属性,包括fd和所属epoll实例
  serAcceptor = std::make_unique<Acceptor>(
      mainReactor.get(), port, ip); // 创建Acceptor,将fd放入epoll监听
  serAcceptor->setNewConnectionCallback(
      [this](int fd) { HandleNewConenction(fd); });
  // 主线程池 添加HandleNewConenction
  threadPool = std::make_unique<ThreadPool>();
  for (int i = 0; i < threadPool->thread_num(); i++) {
    // 每个线程创建一个eventloop 每个eventLoop都是一个subReactor
    subReactor.emplace_back(new EventLoop());
  }
  logger->logInfo("Server started on " + std::string(ip) + ":" +
                  std::to_string(port));
}

void Server::Start() {
  for (int i = 0; i < subReactor.size(); i++) {
    // 添加任务队列,循环监听subreactor的epoll
    threadPool->add([this, i]() { subReactor[i]->loop(); }); // 启动事件循环
  }
  mainReactor->loop();
}
void Server::HandleNewConenction(int client_fd) {
  assert(client_fd != -1);
  int connIndex = client_fd % subReactor.size();
  // 创建客户端连接,根据对应的subReactor(epoll实例)创建连接类
  Connection *conn =
      new Connection(subReactor[connIndex].get(), client_fd, nextConnId++);
  conn->SetOnConnectCallback(onConnectCallback);
  //   创建channel,设置fd和所属epoll实例,回调函数
  conn->setCloseCallback([this](int t_fd) { HandleClose(t_fd); });
  connections[client_fd] = conn;
}

void Server::HandleClose(int fd) {
  //  找到对应的connection
  auto it = connections.find(fd);
  if (it != connections.end()) {
    connections.erase(it); // 从map中删除
    auto conn = it->second;
    ::close(fd);
    conn->getEventLoop()->deleteChannel(conn->getChannel());
    conn = nullptr;
  }
}
void Server::onConnect(std::function<void(Connection *)> fn) {
  onConnectCallback = std::move(fn);
}

Server::~Server() {
  for (auto conn : connections) {
    delete conn.second;
  }
}