#include "kernel/types.h"
#include "user/user.h"

#define RD 0
#define WR 1

static void
sieve(int* inpipe)
{
  // 注意：对于该进程 inpipe 的写端已经关闭

  // 接收到的第一个数字一定是素数
  int prime;
  if (read(inpipe[RD], &prime, sizeof(prime)) == 0 || prime == -1)
    exit(0);
  printf("prime %d\n", prime);

  // 创建一个子进程和管道，将不能整除 prime 的数字通过管道传给子进程
  int outpipe[2];
  pipe(outpipe);

  if (fork() > 0) {
    // 及时 close 掉不需要的文件描述符
    close(outpipe[RD]);

    int n = 0;
    while (read(inpipe[RD], &n, sizeof(n)) && n != -1) {
      if (n % prime)
        write(outpipe[WR], &n, sizeof(n));
    }
    close(inpipe[RD]);
    write(outpipe[WR], &n, sizeof(n));

    // write 完成后，立即关闭写端，避免造成读端阻塞等待
    close(outpipe[WR]);

    // 阻塞等待当前子进程的结束
    wait(0);
  } else {
    close(inpipe[RD]);
    close(outpipe[WR]);

    sieve(outpipe);
  }
}

int
main(int argc, char* argv[])
{
  // 将 2~35 输入
  int inpipe[2];
  pipe(inpipe);
  if (fork() > 0) {
    close(inpipe[RD]);

    int n;
    for (n = 2; n <= 35; n++)
      write(inpipe[WR], &n, sizeof(n));
    n = -1;
    write(inpipe[WR], &n, sizeof(n));
    close(inpipe[WR]);

    wait(0);
  } else {
    close(inpipe[WR]); // 在同一个进程里，调用 sieve 之前就关闭写端
    sieve(inpipe);
  }
  exit(0);
}
