

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
const int PORT = 8080;
const int BUFFER_SIZE = 1024;
int main() {
  int sockfd;
  char buffer[BUFFER_SIZE];
  sockaddr_in servaddr, cliaddr;
  socklen_t len = sizeof(cliaddr);
  const char *response = "Hello from server";
  // 创建套接字 SOCK_DGRAM
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&servaddr, 0, sizeof(servaddr));
  memset(&cliaddr, 0, sizeof(cliaddr));

  // 绑定地址和端口
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(PORT);
  if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("Bind failed");
    close(sockfd);
    exit(EXIT_FAILURE);
  }
  printf("Server is listening on port %d...\n", PORT);

  // 服务端绑定套接字后 直接开始读了
  // 接收客户端数据
  int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0,
                   (struct sockaddr *)&cliaddr, &len);
  buffer[n] = '\0';
  printf("Client: %s\n", buffer);

  // 发送响应
  sendto(sockfd, (const char *)response, strlen(response), 0,
         (const struct sockaddr *)&cliaddr, len);
  printf("Response sent to client.\n");

  close(sockfd);
  return 0;
}