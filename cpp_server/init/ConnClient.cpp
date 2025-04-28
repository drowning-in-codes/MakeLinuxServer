#include "CliSocket.hpp"
#include <Buffer.hpp>
#include <Connection.hpp>
#include <memory>
int main() {
  auto client = std::make_unique<CliSocket>();
  client->connect("127.0.0.1", 2233);
//   client->setNonBlocking();
  auto conn = std::make_unique<Connection>(nullptr, client->get_fd(), 1);
  while (true) {
    auto buf = conn->GetSendBuffer();
    printf("Enter message: ");
    buf->getline();
    conn->Write();
    conn->Read();
    std::cout << "Message from server: " << conn->GetReadData() << std::endl;
  }
  return 0;
}