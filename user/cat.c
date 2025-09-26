#include "kernel/types.h"
// cat.c - 文件查看工具
#include "kernel/fcntl.h"
#include "user/user.h"

char buf[512];

// 读取并输出文件内容
// 参数：fd-文件描述符
void
cat(int fd)
{
  int n;

  // 循环读取文件内容并输出到标准输出
  while((n = read(fd, buf, sizeof(buf))) > 0) {
    if (write(1, buf, n) != n) {
      fprintf(2, "cat: 写入错误\n");
      exit(1);
    }
  }
  if(n < 0){
    fprintf(2, "cat: 读取错误\n");
    exit(1);
  }
}

// 主函数
// 参数：argc-命令行参数数量，argv-命令行参数数组
int
main(int argc, char *argv[])
{
  int fd, i;

  // 如果没有参数，从标准输入读取
  if(argc <= 1){
    cat(0);
    exit(0);
  }

  // 依次处理每个文件参数
  for(i = 1; i < argc; i++){
    if((fd = open(argv[i], O_RDONLY)) < 0){
      fprintf(2, "cat: 无法打开 %s\n", argv[i]);
      exit(1);
    }
    cat(fd);
    close(fd);
  }
  exit(0);
}
