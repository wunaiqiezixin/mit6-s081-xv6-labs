// user/xargs.c

#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"
#include "kernel/stat.h"

// 执行命令，等待子进程的结束
void run(char* program, char* args[])
{
    int pid = fork();
    if (pid == 0) {
        exec(program, args);
        exit(1);
    } else {
        wait(0);
    }
}

int main(int argc, char* argv[])
{
    char buf[2048];
    char *p = buf, *last_p = buf;
    char* argsbuf[MAXARG];
    char** args = argsbuf;

    // 填充起始参数
    for (int i = 1; i < argc; ++i) {
        *args = argv[i];
        ++args;
    }
    // 记录每一行填充参数的起始位置
    char** pa = args;

    // 从终端读入
    char c;
    while (read(0, &c, sizeof(c)) == sizeof(c)) {
        *p = c;
        if (c == ' ' || c == '\n') {
            // 遇到空格或换行，截断字符串获取参数
            *p = 0;
            if (last_p != p) {
                *pa++ = last_p;
            }
            last_p = p + 1;

            // 遇到换行，立马执行一次命令
            if (c == '\n') {
                *pa = 0;
                run(argsbuf[0], argsbuf);
                pa = args; // 更新起始位置
            }
        }
        ++p;
    }

    // 如果终端的输入不以换行结束
    if (pa != args || last_p != p) {
        *p = 0;
        *pa++ = last_p;
        *pa = 0;
        run(argsbuf[0], argsbuf);
    }
    exit(0);
}
