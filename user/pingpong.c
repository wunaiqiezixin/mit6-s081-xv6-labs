#include "kernel/types.h"
#include "user/user.h"

// 管道的读写端
#define RD 0
#define WR 1

int main(int argc, char* argv[])
{
  char buf = 'a';
  // pipe 系统调用，创建两个管道
  int p2c[2], c2p[2];
  pipe(p2c);
  pipe(c2p);

  // fork 系统调用，创建子进程
  if (fork() > 0) {
    close(p2c[RD]);
    close(c2p[WR]);

    // 父进程 ping
    write(p2c[WR], &buf, 1);
    close(p2c[WR]);

    // 父进程接收 pong
    read(c2p[RD], &buf, 1);
    printf("%d: received pong\n", getpid());
    close(c2p[RD]);
  } else {
    close(p2c[WR]);
    close(c2p[RD]);

    // 子进程接收 ping
    read(p2c[RD], &buf, 1);
    printf("%d: received ping\n", getpid());
    close(p2c[RD]);

    // 子进程 pong
    write(c2p[WR], &buf, 1);
    close(c2p[WR]);
  }

  exit(0);
}
