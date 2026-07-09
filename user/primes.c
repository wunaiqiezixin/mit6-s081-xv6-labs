// primes.c

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define RD 0
#define WR 1

// 筛选质数的函数, 传入一个管道
void sieve(int pleft[2])
{
    // 传入 pleft 前，所在进程就已经关闭了管道 pleft 的写端, 后面无需重复关闭

    // 读入第一个数字: 从管道中读取的第一个数字一定是质数
    int p;
    read(pleft[RD], &p, sizeof(p));
    if (p == -1) { // 如果已经读到了末尾
        exit(0);
    }
    printf("prime %d\n", p); // 否则输出该质数

    // 创建一个新的管道, 向下一个进程传入筛选后的数字(递归调用)
    int pright[2];
    pipe(pright);

    if (fork() == 0) { // 右邻居(下一个进程)
        close(pright[WR]); // 该进程不会用到 prihgt 的写端
        close(pleft[RD]); // 该进程不会用到 pleft 的读端
        // close(pleft[WR]);   每次递归调用 sieve() 前，就已经 close 掉了

        sieve(pright); // 递归调用
        exit(0);
    } else { // 当前进程
        close(pright[RD]); // 当前进程不会用到 pright 的读端
        // 读入第一个数字之后的所有数字，
        // 并将不是第一个数字的倍数的数传给下一个进程
        int buf;
        while (read(pleft[RD], &buf, sizeof(buf)) && buf != -1) {
            if (buf % p != 0) {
                write(pright[WR], &buf, sizeof(buf));
            }
        }
        // 循环结束， buf = -1 , 将结束标记也传给下一个进程
        write(pright[WR], &buf, sizeof(buf));
        // 等待当前子进程的结束
        wait(0);
        exit(0);
    }

}

int main()
{
    // 创建初始管道
    int input_pipe[2];
    pipe(input_pipe);

    if (fork() == 0) { // 右邻居
        close(input_pipe[WR]); // 右邻居用不到该管道的写端
        sieve(input_pipe);
        exit(0);
    } else {
        close(input_pipe[RD]); // 当前进程用不到管道的读端
        // 向管道的写端传入数字
        int i;
        for (i = 2; i <= 35; ++i) {
            write(input_pipe[WR], &i, sizeof(i));
        }
        // 传入结束标记
        i = -1;
        write(input_pipe[WR], &i, sizeof(i));
        // 等待当前子进程的结束
        wait(0);
        exit(0);
    }
}
