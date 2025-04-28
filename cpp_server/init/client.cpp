#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/getopt_core.h>
#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utils/utils.hpp>

constexpr int BUFFER_SIZE = 1024;
int main(int argc, char *argv[]) {

  int ret;
  int port = 2025;
  bool fixbinding{false};
  const char *ip = "127.0.0.1";
  while ((ret = getopt(argc, argv, "hba:p:")) != -1) {
    switch (ret) {
    case 'a':
      ip = optarg;
      break;
    case 'b':
      fixbinding = true;
      break;
    case 'p':
      port = std::stoi(optarg);
      break;
    case 'h':
      printf("This is # Author: proanimer \
#Email : < bukalala174 @gmail.com> \
#License : MIT ");
    default:
      fprintf(stderr,
              "Usage: %s [-b <bool:fix binding> -a <ipaddr> -p <port.]. "
              "default to localhost on port "
              "2025\n",
              argv[0]);
      return -1;
    }
  }
  // 创建套接字
  int client_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (client_fd < 0) {
    perror("socket");
    return -1;
  }
  //  创建客户端地址
  sockaddr_in client_addr;
  client_addr.sin_family = AF_INET;
  client_addr.sin_port = htons(port);
  int retcode = inet_pton(AF_INET, ip, &client_addr.sin_addr);
  int opt = 1;
  // 设置套接字选项
  setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  if (retcode != 1) {
    perror("inet_pton");
    close(client_fd);
    return -1;
  }
  if (fixbinding) {
    // 绑定套接字
    errif(bind(client_fd, (struct sockaddr *)&client_addr,
               sizeof(client_addr)) < 0,
          "bind");
  }

  // 连接服务器
  sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(2233);
  retcode = inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
  if (retcode != 1) {
    perror("inet_pton");
    close(client_fd);
    return -1;
  }
  socklen_t len = sizeof(server_addr);
  errif(connect(client_fd, (sockaddr *)&server_addr, len) < 0,
        "connect failed");
  // 发送数据
  while (true) {
    char buf[BUFFER_SIZE];
    bzero(buf, sizeof(buf));
    printf("Enter message: ");
    scanf("%s", buf);
    ssize_t bytes_sent = send(client_fd, buf, sizeof(buf), 0);
    if (bytes_sent < 0) {
      perror("send");
      break;
    }
    bzero(buf, sizeof(buf));
    ssize_t bytes_received = recv(client_fd, buf, sizeof(buf), 0);
    if (bytes_received < 0) {
      perror("recv");
      break;
    } else if (bytes_received == 0) {
      printf("Server closed connection\n");
      break;
    }
    printf("Received from server: %s\n", buf);
  }
  close(client_fd);
  return 0;
}