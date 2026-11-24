#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"

// Find first character after last slash.
static char*
filename(char *path)
{
  char *p;

  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  return p;
}


void
find(char* path, char* name)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  // open 系统调用，为 path 新分配一个文件描述符
  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  // fstat 系统调用，填充 stat 结构体
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type) {
  case T_FILE:
    if (strcmp(filename(path), name) == 0)
      printf("%s\n", path);
    break;
  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: path too long\n");
      break;
    }

    // 在目录 path 下递归查找所有名称为 name 的文件
    while (read(fd, &de, sizeof (de)) == sizeof(de)) {
      // 跳过被删除的目录项
      if (de.inum == 0)
        continue;
      // 跳过 "." 和 ".."
      if (strcmp(".", de.name) == 0 || strcmp("..", de.name) == 0)
        continue;

      strcpy(buf, path);
      p = buf+strlen(buf);
      *p++ = '/';
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      find(buf, name);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char* argv[])
{
  if (argc != 3) {
    fprintf(2, "Usage: <path> <name>\n");
    exit(1);
  }
  find(argv[1], filename(argv[2]));
  exit(0);
}
