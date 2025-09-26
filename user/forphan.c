#include "kernel/types.h"
// forphan.c - 孤立文件测试工具
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

// 创建一个孤立文件并测试test-xv6.py是否能够回收它

#define BUFSZ 500  // 缓冲区大小

char buf[BUFSZ];  // 缓冲区

// 主函数 - 创建孤立文件
// 参数：argc-命令行参数数量，argv-命令行参数数组
int
main(int argc, char **argv)
{
  int fd = 0;         // 文件描述符
  char *s = argv[0];  // 程序名称
  struct stat st;     // 文件状态结构
  char *ff = "file0"; // 要创建的文件名
  
  // 创建文件
  if ((fd = open(ff, O_CREATE|O_WRONLY)) < 0) {
    printf("%s: 打开文件失败\n", s);
    exit(1);
  }
  // 获取文件状态
  if(fstat(fd, &st) < 0){
    fprintf(2, "%s: 无法获取文件状态 %s\n", s, "ff");
    exit(1);
  }
  // 删除文件的目录项，创建孤立文件
  if (unlink(ff) < 0) {
    printf("%s: 删除链接失败\n", s);
    exit(1);
  }
  // 验证文件确实无法通过路径打开
  if (open(ff, O_RDONLY) != -1) {
    printf("%s: 打开文件成功（预期失败）\n", s);
    exit(1);
  }
  printf("等待被杀死并回收文件，inode号: %d\n", st.ino);
  // 进入无限循环，直到被杀死
  for(;;) pause(1000);
}
