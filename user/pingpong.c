// pingpong.c

#include "kernel/types.h"
#include "user/user.h"

#define RD 0
#define WR 1

int main()
{
    int exit_status = 0;
    char buf = 'p';

    // 创建两个管道
    int p2c[2], c2p[2];
    pipe(p2c);
    pipe(c2p);

    // 创建进程
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork failed!\n");
        exit_status = 1;
    } else if (pid == 0) { // 子进程
        // 子进程不会通过 p2c 向父进程写
        close(p2c[WR]);
        // 子进程不会通过 c2p 从父进程读
        close(c2p[RD]);

        // 子进程读
        if (read(p2c[RD], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "child: read error!\n");
            exit_status = 1;
        } else {
            printf("%d: received ping\n", getpid());
        }
        // 子进程写
        if (write(c2p[WR], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "child: write error!\n");
            exit_status = 1;
        }
        // 及时关闭管道的端
        close(p2c[RD]);
        close(c2p[WR]);
    } else { // 父进程
        close(p2c[RD]);
        close(c2p[WR]);

        // 父进程写
        if (write(p2c[WR], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "parent: write error!\n");
            exit_status = 1;
        }
        // 父进程读
        if (read(c2p[RD], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "parent: read error\n");
            exit_status = 1;
        } else {
            printf("%d: received pong\n", getpid());
        }
        // 及时关闭管道的端
        close(p2c[WR]);
        close(c2p[RD]);

        // 父进程等待子进程的结束
        wait((int*)0);
    }

    exit(exit_status);
}
