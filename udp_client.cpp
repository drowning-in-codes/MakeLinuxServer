#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
  int sockfd;
  char buffer[BUFFER_SIZE];
  struct sockaddr_in servaddr;
  const char *message = "Hello from client";

  // 创建套接字
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&servaddr, 0, sizeof(servaddr));

  // 设置服务器地址
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(PORT);
  servaddr.sin_addr.s_addr = INADDR_ANY;

  // 将 IP 地址转换为二进制形式
  if (inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr) <= 0) {
    perror("Invalid address/ Address not supported");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  // 创建套接字 后直接发送消息 需要连接的地址,不需要connect
  // 发送消息
  sendto(sockfd, (const char *)message, strlen(message), 0,
         (const struct sockaddr *)&servaddr, sizeof(servaddr));
  printf("Message sent to server.\n");

  // 接收响应
  socklen_t len = sizeof(servaddr);
  int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0,
                   (struct sockaddr *)&servaddr, &len);
  buffer[n] = '\0';
  printf("Server: %s\n", buffer);

  close(sockfd);
  return 0;
}