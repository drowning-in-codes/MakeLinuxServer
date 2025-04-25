

/*
    server

*/

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <netinet/in.h>
#include <string>
#include <string_view>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

void errif(bool err, const char *msg) {
  if (err) {
    perror(msg);
    exit(EXIT_FAILURE);
  }
}
int main() {
  // create
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  errif(sockfd == -1, "socket create error");

  // 设置套接字可重用
  //   使用 SO_REUSEADDR 和 SO_REUSEPORT，可以在服务器重启时快速重新绑定端口
  int opt = 1;
  int err = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                       sizeof(opt));
  errif(err != 0, "setsockopt failed");
  // bind
  sockaddr_in sockaddr;
  sockaddr.sin_family = AF_INET;
  //   sockaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  //   inet_aton("127.0.0.1", &sockaddr.sin_addr.s_addr)
  int ret = inet_pton(AF_INET, "127.0.0.1", &sockaddr.sin_addr.s_addr);
  errif(ret != 1, "convert address failed");
  sockaddr.sin_port = htons(8080);
  // listen
  errif(listen(sockfd, SOMAXCONN) == -1, "socket listen error");

  // 接收到client_fd后读取
  // accept
  // sockaddr_in client_addr;
  // memset(&client_addr, 0, sizeof(client_addr));
  // socklen_t client_addr_len = sizeof(client_addr);
  // //   bzero(&client_addr, sizeof(client_addr));
  // int client_fd =
  //     accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
  // errif(client_fd == -1, "accept failed");

  // read and write
  //   while (true) {
  //     char buf[1024];
  //     bzero(&buf, sizeof(buf));
  //     ssize_t read_bytes = read(client_fd, buf, sizeof(buf));
  //     if (read_bytes > 0) {
  //       printf("message from client fd %d: %s\n", client_fd, buf);
  //       write(client_fd, buf, sizeof(buf));
  //     } else if (read_bytes == 0) {
  //       printf("client fd %d disconnected\n", client_fd);
  //       close(client_fd);
  //       break;
  //     } else if (read_bytes == -1) {
  //       close(client_fd);
  //       errif(true, "socket read error");
  //     }
  //   }

  // 1. 使用while循环轮询accept,当接收到连接创建线程读取.
  // 每一个连接都需要一个线程,如果网络连接多,创建太多线程影响内存

  // 2. 使用select
  fd_set read_fds, active_fds;
  int max_fd = sockfd;
  std::vector<unsigned int> clients_fd;
  FD_ZERO(&read_fds);
  FD_SET(sockfd, &read_fds);
  while (true) {
    // 轮询select 直到有连接
    timeval timeout{1, 0};
    read_fds = active_fds;
    // select阻塞,直到相应文件描述符就绪(可读,可写或异常)
    // 成功返回时(>=1),fd_set内容更新,仅保留就绪的fd
    int ret = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
    errif(ret < 0, "select failed");

    if (FD_ISSET(sockfd, &read_fds)) {
      // 服务器可读(有connect连接)
      sockaddr_in client_addr;
      socklen_t client_addr_len = sizeof(client_addr);
      bzero(&client_addr, client_addr_len);
      int client_sockfd =
          accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
      FD_SET(client_sockfd, &active_fds);
      max_fd = std::max(client_sockfd, max_fd);
      clients_fd.push_back(client_sockfd);

      std::cout << "New connection, socket fd: " << client_sockfd
                << ", IP: " << inet_ntoa(client_addr.sin_addr)
                << ", Port: " << ntohs(client_addr.sin_port) << std::endl;
    }
    for (auto it = clients_fd.begin(); it != clients_fd.end();) {
      unsigned int client_fd = *it;
      if (FD_ISSET(client_fd, &read_fds)) {
        //客户端可读数据
        // 读取数据
        char read_bytes[1024]{};
        size_t bytes_len = read(client_fd, read_bytes, sizeof(read_bytes));
        if (bytes_len <= 0) {
          std::cout << "client closed...\n";
          close(client_fd);
          FD_CLR(client_fd, &active_fds);
          it = clients_fd.erase(it);
          continue;
        } else {
          std::string resp =
              "你好,你发送了" + std::string(read_bytes, bytes_len);
          errif(write(client_fd, resp.c_str(), resp.size()) == -1,
                "write socket failed");
          ++it;
        }
      } else {
        ++it;
      }
    }
  }
  for (auto &client_fd : clients_fd) {
    // 关闭所有客户端
    close(client_fd);
  }

  // 3.使用poll
  std::vector<pollfd> poll_fds;
  pollfd server_pollfd;
  server_pollfd.fd = sockfd;
  server_pollfd.events = POLLIN; // 设置事件
  poll_fds.push_back(server_pollfd);
  while (true) {
    int ret = poll(poll_fds.data(), poll_fds.size(), -1);
    errif(ret < 0, "poll error");
    if (poll_fds.at(0).revents & POLLIN) {
      // 数据读入事件
      // serverfd可读,即可accept
      sockaddr_in client_addr{};
      socklen_t addr_len = sizeof(client_addr);
      int client_fd = accept(sockfd, (struct sockaddr *)&client_addr,
                             (socklen_t *)&addr_len);
      errif(client_fd == -1, "accept failed");

      pollfd client_pollfd;
      client_pollfd.fd = client_fd;
      client_pollfd.events = POLLIN;
      poll_fds.push_back(client_pollfd);
      std::cout << "New connection, socket fd: " << client_fd
                << ", IP: " << inet_ntoa(client_addr.sin_addr)
                << ", Port: " << ntohs(client_addr.sin_port) << std::endl;
    }
    for (auto it = poll_fds.begin() + 1; it != poll_fds.end();) {
      char bytes_read[1024]{};
      pollfd &client_pollfd = *it;
      size_t read_bytes_len =
          read(client_pollfd.fd, bytes_read, sizeof(bytes_read));
      if (read_bytes_len <= 0) {
        // 关闭
        std::cout << "client closed...\n";
        close(client_pollfd.fd);
        it = poll_fds.erase(it);
      } else {
        // echo
        std::string resp =
            "你好,你的请求是" + std::string(bytes_read, read_bytes_len);
        write(client_pollfd.fd, resp.data(), resp.size());
        ++it;
      }
    }
  }

  // 4. epoll
  int epoll_fd = epoll_create1(0);
  errif(epoll_fd == -1, "epoll creation failed");
  const int MAX_EVENTS = 10;
  epoll_event ev, events[MAX_EVENTS];
  ev.events = POLLIN | EPOLLET; // 水平触发模式
  ev.data.fd = sockfd;
  // 添加服务端fd
  ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev);
  errif(ret == -1, "ctl failed");
  while (true) {
    // 等待事件
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    errif(nfds == -1, "epoll waut failed");
    for (int i = 0; i < nfds; i++) {
      if (events[i].data.fd == sockfd &&
          (events[i].events & (EPOLLIN | EPOLLET))) {
        // 服务端 accept就绪
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd =
            accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
        epoll_event ev;
        ev.events = POLLIN | EPOLLET; // 水平触发模式
        ev.data.fd = client_fd;
        // 将客户端连接加入epoll实例
        ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
        errif(ret == -1, "ctl failed");
      } else if (events[i].data.fd != sockfd &&
                 (events[i].events & (EPOLLIN | EPOLLET))) {
        // 客户端连接
        int client_fd = events[i].data.fd;
        // read就绪
        char bytes_read[1024]{};
        size_t bytes_read_len = read(client_fd, bytes_read, sizeof(bytes_read));
        if (bytes_read_len <= 0) {
          std::cout << "client connection closed...\n";
          close(client_fd);
          epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
        } else {
          std::string resp =
              "你好,你发送了:" + std::string(bytes_read, bytes_read_len);
          write(client_fd, resp.data(), resp.size());
        }
      }
    }
  }

  // close 关闭服务端
  close(sockfd);

  return 0;
}