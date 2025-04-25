

#include <arpa/inet.h>
#include <cstdio>
#include <netinet/in.h>
#include <strings.h>
#include <unistd.h>
void errif(bool err, const char *msg) {
  if (err) {
    perror(msg);
  }
}

#include <sys/socket.h>
int main() {
  // create
  int client_fd = socket(AF_INET, SOCK_STREAM, 0);
  errif(client_fd == -1, "");

  sockaddr_in server_addr;
  bzero(&server_addr, sizeof(server_addr));

  server_addr.sin_family = AF_INET;
  int ret = inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr.s_addr);
  errif(ret != 1, "");
  server_addr.sin_port = htons(8090);

  // connect
  ret =
      connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  errif(ret == -1, "");

  // write and read

  while (true) {
    char buf[1024];
    bzero(&buf, sizeof(buf));
    scanf("%s", buf);
    ssize_t write_bytes = write(client_fd, buf, sizeof(buf));
    if (write_bytes == -1) {
      printf("socket already disconnected, can't write any more!\n");
      break;
    }
    bzero(&buf, sizeof(buf));
    ssize_t read_bytes = read(client_fd, buf, sizeof(buf));
    if (read_bytes > 0) {
      printf("message from server: %s\n", buf);
    } else if (read_bytes == 0) {
      printf("server socket disconnected!\n");
      break;
    } else if (read_bytes == -1) {
      close(client_fd);
      errif(true, "socket read error");
    }
  }
  close(client_fd);
  return 0;
}