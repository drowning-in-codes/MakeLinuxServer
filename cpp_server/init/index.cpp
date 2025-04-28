#include <Buffer.hpp>
#include <EventLoop.hpp>
#include <Server.hpp>
#include <utils/utils.hpp>
int main() {
  auto server = new Server();
  server->onConnect([](Connection *conn) {
    // 当创建连接后,如果有可读事件
    conn->Send("You sent-> " + std::string(conn->GetReadData()));

  });              // 设置连接回调函数
  server->Start(); // 启动服务器
  delete server;
  return 0;
}