#include <Buffer.hpp>
#include <EventLoop.hpp>
#include <Server.hpp>
#include <memory>
#include <utils/utils.hpp>

class EchoServer{
  public:
  std::unique_ptr<Server> server;
  EchoServer():server(std::make_unique<Server>()){}
  void setOnMessageCallback(std::function<void(const std::shared_ptr<Connection> &)> callback){
    server->setOnMessageCallback(callback);
  }
  void Start() {
    server->Start();
  }
};
int main() {
  auto echoServer = std::make_unique<EchoServer>();
  echoServer->setOnMessageCallback([](const std::shared_ptr<Connection> &conn) {
    // 处理接收到的消息
    std::cout << "Received message: " << conn->GetReadData() << std::endl;
    std::string message = conn->GetReadData();
    conn->Send("Echo: " + message);
  });
  echoServer->Start();
  return 0;
}