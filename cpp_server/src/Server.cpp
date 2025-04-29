#include "Acceptor.hpp"
#include "Connection.hpp"
#include "InetAddress.hpp"
#include "SerSocket.hpp"
#include "utils/utils.hpp"
#include <Channel.hpp>
#include <CurrentThread.hpp>
#include <Epoll.hpp>
#include <Server.hpp>
#include <cassert>
#include <csignal>
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
      [this](int fd) { HandleNewConnection(fd); });
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
  // ctrl-c 信号处理事件
  std::signal(SIGINT, [](int signum) {
    std::cout << "\nServer Stopped. Have a nice day!😄" << std::endl;
    exit(0);
  });
  mainReactor->loop();
}
void Server::HandleNewConnection(int client_fd) {
  assert(client_fd != -1);
  int connIndex = client_fd % subReactor.size();
  // 创建客户端连接,根据对应的subReactor(epoll实例)创建连接类
  auto conn = std::make_shared<Connection>(subReactor[connIndex].get(),
                                           client_fd, nextConnId++);
  conn->SetOnConnectCallback(onConnectCallback);
  conn->setOnMessageCallback(onMessageCallback);
  //   创建channel,设置fd和所属epoll实例,回调函数
  conn->setOnCloseCallback(
      [this](const std::shared_ptr<Connection> &conn) { HandleClose(conn); });
  connections[client_fd] = conn;
  // 将connection分配给Channel的tie,增加计数 并开始监听读事件
  conn->ConnectionEstablished();
}
void Server::HandleClose(const std::shared_ptr<Connection> &conn) {
  // 让主线程的eventloop执行方法
  mainReactor->runOneFunc([this, conn]() { HandleCloseInLoop(conn); });
}

void Server::HandleCloseInLoop(const std::shared_ptr<Connection> &conn) {
  std::cout << CurrentThread::tid()
            << " TcpServer::HandleCloseInLoop - Remove connection id: "
            << conn->getConid() << " and fd: " << conn->getFd() << std::endl;
  auto it = connections.find(conn->getFd());
  if (it != connections.end()) {
    //  引用减少
    connections.erase(it);
  }
  auto loop = conn->getEventLoop();
  loop->QueueOneFunc([conn]() {
    // 关闭连接 在对应的connection中关闭
    conn->ConnectionDestructor();
  });
}

void Server::setConnectionCallback(
    const std::function<void(const std::shared_ptr<Connection> &)> &fn) {
  onConnectCallback = std::move(fn);
}
void Server::setOnMessageCallback(
    const std::function<void(const std::shared_ptr<Connection> &)> &fn) {
  onMessageCallback = std::move(fn);
}

Server::~Server() {}