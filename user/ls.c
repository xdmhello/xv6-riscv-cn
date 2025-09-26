#include "kernel/types.h"
// ls.c - 目录和文件列表工具
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

// 格式化文件名
// 参数：path-文件路径
// 返回：格式化后的文件名（去除路径部分）
char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // 找到最后一个斜杠后的第一个字符
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // 返回用空格填充的文件名
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  buf[sizeof(buf)-1] = '\0';
  return buf;
}

// 列出目录或文件信息
// 参数：path-要列出的路径
void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "ls: 无法打开 %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: 无法获取 %s 的状态\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_DEVICE:
  case T_FILE:
    // 显示文件信息：文件名、类型、inode号、大小
    printf("%s %d %d %d\n", fmtname(path), st.type, st.ino, (int) st.size);
    break;

  case T_DIR:
    // 检查路径长度是否超过缓冲区大小
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("ls: 路径太长\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    // 遍历目录中的所有条目
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)  // 跳过未使用的条目
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("ls: 无法获取 %s 的状态\n", buf);
        continue;
      }
      // 显示目录条目的信息
      printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, (int) st.size);
    }
    break;
  }
  close(fd);
}

// 主函数
// 参数：argc-命令行参数数量，argv-命令行参数数组
int
main(int argc, char *argv[])
{
  int i;

  // 如果没有参数，列出当前目录
  if(argc < 2){
    ls(".");
    exit(0);
  }
  // 列出每个指定路径的内容
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit(0);
}
