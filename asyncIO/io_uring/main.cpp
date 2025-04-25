#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <unistd.h>

int main() {
  constexpr int BUFFER_SIZE = 4096;
  // 初始化io_uring实例
  struct io_uring ring;
  char buffer[BUFFER_SIZE];
  int fd;
  if (io_uring_queue_init(8, &ring, IORING_SETUP_IOPOLL) < 0) {
    perror("io_uring_queue_init");
    return 1;
  }

  fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    perror("open");
    io_uring_queue_exit(&ring);
    return 1;
  }
  memset(buffer, 'A', BUFFER_SIZE); // 填充缓冲区为字符 'A'
  struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
  if (!sqe) {
    perror("io_uring_get_sqe");
    close(fd);
    io_uring_queue_exit(&ring);
    return 1;
  }
  // 准备写入请求
  io_uring_prep_write(sqe, fd, buffer, BUFFER_SIZE, 0);
  sqe->user_data = 1; // 设置用户数据
  // 提交请求
  if (io_uring_submit(&ring) < 0) {
    perror("io_uring_submit");
    close(fd);
    io_uring_queue_exit(&ring);
    return 1;
  }
  printf("Asynchronous write operation initiated.\n");

  struct io_uring_cqe *cqe;
  if (io_uring_wait_cqe(&ring, &cqe) < 0) {
    perror("io_uring_wait_cqe");
    close(fd);
    io_uring_queue_exit(&ring);
    return 1;
  }
  if (cqe->res < 0) {
    fprintf(stderr, "Error: %s\n", strerror(-cqe->res));
  } else {
    printf("Write completed successfully.\n");
  }
  io_uring_cqe_seen(&ring, cqe); // 标记完成事件为已处理
  close(fd);
  io_uring_queue_exit(&ring); // 清理io_uring实例
  return 0;
}