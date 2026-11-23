#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char* argv[])
{

  char *xargs[MAXARG], buf[512];
  int p, s, t;
  int n, m;

  if (argc < 2) {
    fprintf(2, "Usage: xargs <cmd> ...\n");
    exit(1);
  }

  for (int i = 1; i <= argc; i++)
    xargs[i-1] = argv[i];

  p = argc-1;

  // 将 stdin 的输入读入 buf
  n = 0;
  while (n < sizeof(buf)-1 &&
        (m = read(0, buf+n, sizeof(buf)-n-1)) > 0)
    n += m;
  buf[n] = 0;

  // 通过 '\n' 从 buf 中分割出参数并放入 xargs
  for (s = 0, t = 0; t < n; t++) {
    while (buf[t] != '\n' && buf[t] != 0)
      t++;
    buf[t] = 0;
    xargs[p++] = &buf[s];
    s = t + 1;
  }
  xargs[p] = 0;

  // 子进程调用 exec
  if (fork() == 0) {
    exec(xargs[0], xargs);
    exit(1);
  }

  // 父进程等待子进程的结束
  wait(0);
  exit(0);
}
