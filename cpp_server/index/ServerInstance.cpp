#include "Channel.hpp"
#include "Epoll.hpp"
#include "EventLoop.hpp"
#include "InetAddress.hpp"
#include "SerSocket.hpp"
#include "Server.hpp"
#include <asm-generic/errno-base.h>
#include <cerrno>
#include <iostream>
#include <strings.h>
#include <sys/types.h>
constexpr int MAX_EVENTS = 1024;
constexpr int READ_BUFFER = 1024;

void handleReadEvent(int fd) {
  char buffer[READ_BUFFER];
  while (true) {
    bzero(buffer, sizeof(buffer));
    // 读取数据
    ssize_t bytes_read = recv(fd, buffer, sizeof(buffer), 0);
    if (bytes_read > 0) {
      // 有数据可读
      printf("Received data: %s\n", buffer);
      write(fd, buffer, bytes_read); // echo
    } else if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // 重试
      continue;
    } else if (bytes_read == -1 && (errno == EINTR)) {
      // 重试
      continue;
    } else if (bytes_read == 0) {
      // 客户端关闭连接
      printf("Client closed connection\n");
      close(fd);
      break;
    } else {
      // 其他错误
      perror("recv");
      close(fd);
      break;
    }
  }
}

void handleListenEvent(Epoll epoll, int server_fd, SerSocket server) {
  while (true) {
    const auto &epoll_events = epoll.poll(1000);
    for (int i = 0; i < epoll_events.size(); i++) {
      // epoll 轮询
      if (epoll_events[i].data.fd == server_fd) {
        // 监听到有连接请求
        InetAddress client_addr;
        int client_fd = server.accept(client_addr);
        setNonBlocking(client_fd);
        // 加入epoll轮询
        // epoll.addfd(client_fd);
        auto *channel = new Channel(client_fd, &epoll);
        epoll.addfd(client_fd, channel, EPOLLIN);
      } else if (epoll_events[i].events & EPOLLIN) {
        // 可读事件
        handleReadEvent(epoll_events[i].data.fd);
      } else {
        printf("Unknown event\n");
      }
    }
  }
}
void setUpserver() {
  InetAddress addr{"127.0.0.1", 2233};
  SerSocket server;
  server.bind(addr);
  server.setReuseAddr();
  server.listen();
  std::cout << "Server started on address " << addr.getIp() << std::endl;
  std::cout << "Server started on port " << addr.getPort() << std::endl;
  server.setNonBlocking();
  Epoll epoll;
  int server_fd = server.get_fd();
  auto *serChannel = new Channel(server_fd, &epoll);
  epoll.addfd(server_fd, serChannel, EPOLLIN);
  while (true) {
    const auto &epoll_channel = epoll.poll_channel(1000);
    for (int i = 0; i < epoll_channel.size(); i++) {
      if (epoll_channel[i].fd == server_fd) {
        // 监听到有连接请求
        InetAddress client_addr;
        int client_fd = server.accept(client_addr);
        setNonBlocking(client_fd);
        // 加入epoll轮询
        // epoll.addfd(client_fd);
        auto *channel = new Channel(client_fd, &epoll);
        epoll.addfd(client_fd, channel, EPOLLIN);
      } else if (epoll_channel[i].revents & EPOLLIN) {
        // 可读事件
        handleReadEvent(epoll_channel[i].fd);
      } else {
        printf("Unknown event\n");
      }
    }
  }
}
int main() {
  setUpserver();
  return 0;
}
