#include <aio.h>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <libaio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
int MAX_LSIT = 2;
void handler(int sig, siginfo_t *si, void *unused) {
  printf("Async I/O completed.\n");
}
int main() {
  struct aiocb cb;
  int fd, ret, counter;
  fd = open("myfile.txt", O_WRONLY | O_CREAT | O_APPEND);
  if (fd == -1) {
    std::cout << "open file error" << std::endl;
    return -1;
  }

  char msg[] = "hello world";
  // 设置信号处理
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = handler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);

  // 设置异步I/O控制块
  cb.aio_fildes = fd;
  cb.aio_lio_opcode = LIO_WRITE;
  cb.aio_buf = msg;
  cb.aio_nbytes = strlen(msg);
  cb.aio_offset = 0;
  //   设置通知方式
  cb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
  //   当使用信号通知,设置信号值等参数
  cb.aio_sigevent.sigev_signo = SIGUSR1;
  //   如果使用在SIGEV_THREAD 作为通知方式时，此字段应指向一个函数
  //   cb.aio_sigevent.sigev_notify_function = NULL;
  // 传递字符串

  //   if (aio_read(&cb) == -1) {
  //     std::cout << "aio_read error" << std::endl;
  //     perror("aio_read");
  //     return -1;
  //   }
  if (-1 == aio_write(&cb)) {
    std::cout << "aio_write error" << std::endl;
    perror("aio_write");
    return -1;
  }

  int couter = 0;
  while (aio_error(&cb) == EINPROGRESS) {
    printf("第%d次\n", ++couter);
  }

  int bytesRead = aio_return(&cb);
  printf("Read %d bytes\n", bytesRead);
  std::cout << msg;
  fprintf(stdout, "read: %10.10s\n", cb.aio_buf);
  std::cout<<"write: " << (char*)(cb.aio_buf) << std::endl;
  close(fd);
  return 0;
}
