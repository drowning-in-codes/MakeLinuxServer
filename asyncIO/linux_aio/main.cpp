#include <cstdlib>
#include <libaio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define MAX_EVENTS 128

int main() {
    io_context_t ctx = 0;
    struct iocb cb;
    struct iocb *cbs[1];
    struct io_event events[MAX_EVENTS];
    void* data;
    char msg[] = "Hello, libaio async write!\n";
    size_t data_len = 1024;

    if(posix_memalign(&data, 1024, data_len)<0) {
        perror("posix_memalign");
        return 1;
    }
    memcpy(data, msg, data_len);
    int fd;
    int ret;

    // 打开文件
    fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0644);
    // 如果不使用O_DIRECT
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // 初始化 io_context
    ret = io_setup(MAX_EVENTS, &ctx);
    if (ret < 0) {
        fprintf(stderr, "io_setup error: %s\n", strerror(-ret));
        close(fd);
        return 1;
    }

    // 准备异步写操作
    io_prep_pwrite(&cb, fd, data, data_len, 0);
    cbs[0] = &cb;

    // 提交异步写请求
    ret = io_submit(ctx, 1, cbs);
    if (ret != 1) {
        fprintf(stderr, "io_submit error: %s\n", strerror(-ret));
        io_destroy(ctx);
        close(fd);
        return 1;
    }

    // 等待事件完成
    ret = io_getevents(ctx, 1, MAX_EVENTS, events, NULL);
    if (ret < 0) {
        fprintf(stderr, "io_getevents error: %s\n", strerror(-ret));
        io_destroy(ctx);
        close(fd);
        return 1;
    }

    // 检查事件结果
    for (int i = 0; i < ret; i++) {
        struct io_event *ev = &events[i];
        if (ev->res < 0) {
            fprintf(stderr, "I/O error: %s\n", strerror(-ev->res));
        } else {
            printf("Write completed: %ld bytes\n", ev->res);
        }
    }

    // 清理资源
    io_destroy(ctx);
    close(fd);
    return 0;
}

