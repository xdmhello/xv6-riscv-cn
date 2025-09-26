#include "kernel/types.h"
// logstress.c - xv6日志系统压力测试工具
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

// 通过多个进程并发写入各自的文件来测试xv6日志系统（例如：logstress f1 f2 f3 f4）

#define BUFSZ 500  // 缓冲区大小

char buf[BUFSZ];

// 主函数
// 参数：argc-命令行参数数量，argv-命令行参数数组（包含要创建的文件名）
int
main(int argc, char **argv)
{
  int fd, n;
  enum { N = 250, SZ=2000 };  // N-每个文件写入次数，SZ-每次写入大小
  
  // 为每个命令行参数创建一个子进程
  for (int i = 1; i < argc; i++){
    int pid1 = fork();
    if(pid1 < 0){
      printf("%s: 创建进程失败\n", argv[0]);
      exit(1);
    }
    if(pid1 == 0) {
      // 子进程：创建文件并写入数据
      fd = open(argv[i], O_CREATE | O_RDWR);
      if(fd < 0){
        printf("%s: 创建文件 %s 失败\n", argv[0], argv[i]);
        exit(1);
      }
      // 用与参数索引相关的字符填充缓冲区
      memset(buf, '0'+i, SZ);
      // 重复写入文件多次
      for(i = 0; i < N; i++){
        if((n = write(fd, buf, SZ)) != SZ){
          printf("写入失败 %d\n", n);
          exit(1);
        }
      }
      exit(0);
    }
  }
  // 父进程：等待所有子进程完成
  int xstatus;
  for(int i = 1; i < argc; i++){
    wait(&xstatus);
    if(xstatus != 0)
      exit(xstatus);
  }
  return 0;
}
