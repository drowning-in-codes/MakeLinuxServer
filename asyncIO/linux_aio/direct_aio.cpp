
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <libaio.h>

#define BUFFER_SIZE 4096

int main() {
    io_context_t ctx;
    struct iocb iocb;
    struct iocb *iocbs[1] = {&iocb};
    struct io_event events[1];
    void *buffer ; // 使用 new 分配缓冲区

    // 分配对齐的缓冲区
    if (posix_memalign(&buffer, 4096, BUFFER_SIZE)) {
        perror("posix_memalign");
        return 1;
    }
    memset(buffer, 'A', BUFFER_SIZE); // 填充缓冲区为字符 'A'

    // 打开文件（创建或截断，使用 O_DIRECT）
    int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT , 0644);
    if (fd == -1) {
        perror("open");
        free(buffer);
        return 1;
    }

    // 初始化 AIO 上下文
    if (io_setup(1, &ctx) < 0) {
        perror("io_setup");
        close(fd);
        free(buffer);
        return 1;
    }

    // 准备异步写入请求
    io_prep_pwrite(&iocb, fd, buffer, BUFFER_SIZE, 0);

    // 提交请求
    if (io_submit(ctx, 1, iocbs) < 0) {
        perror("io_submit");
        io_destroy(ctx);
        close(fd);
        free(buffer);
        return 1;
    }

    printf("Asynchronous write operation initiated.\n");

    // 等待完成事件
    if (io_getevents(ctx, 1, 1, events, NULL) < 0) {
        perror("io_getevents");
        io_destroy(ctx);
        close(fd);
        free(buffer);
        return 1;
    }

    // 处理完成事件
    if (events[0].res >= 0) {
        printf("Asynchronous write completed successfully. Bytes written: %u\n", events[0].res);
    } else {
        fprintf(stderr, "Asynchronous write failed with error: %d\n", -events[0].res);
    }

    // 清理资源
    io_destroy(ctx);
    close(fd);
    free(buffer);

    return 0;
}