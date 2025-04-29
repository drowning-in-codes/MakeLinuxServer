
#include <arpa/inet.h>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
int main() {
  int ep = epoll_create1(0);
  if (ep == -1) {
    perror("epoll_create1");
    return 1;
  }
  struct epoll_event ev;
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    perror("socket");
    return 1;
  }
  // Set the server socket to non-blocking mode
  int flags = fcntl(server_fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl");
    return 1;
  }
  if (fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl");
    return 1;
  }
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_port = htons(8089);
  ev.events = EPOLLIN;
  ev.data.fd = server_fd; // stdin
  int ret = epoll_ctl(ep, EPOLL_CTL_ADD, server_fd, &ev);
  ret = ::bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
  if (ret == -1) {
    perror("bind");
    return 1;
  }
  ::listen(server_fd, 10);
  auto evs = new epoll_event[10];
  while (true) {
    int evnums = epoll_wait(ep, evs, 10, 100);
    if (evnums == -1) {
      perror("epoll_wait");
      return 1;
    }
    for (int i = 0; i < evnums; i++) {
      if (evs[i].events & EPOLLIN) {
        if (evs[i].data.fd == server_fd) {
          printf("有连接请求\n");
          sockaddr_in client_addr;
          socklen_t client_addr_len = sizeof(client_addr);
          int client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                                 &client_addr_len);
          if (client_fd == -1) {
            perror("accept");
            continue;
          }
          printf("Accepted connection from %s:%d\n",
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        }
      } else {
        printf("other things happen\n");
      }
    }
  }
}