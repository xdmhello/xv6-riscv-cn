#include "kernel/types.h"
// kill.c - 进程信号发送工具
#include "kernel/stat.h"
#include "user/user.h"

// 主函数
// 参数：argc-命令行参数数量，argv-命令行参数数组
int
main(int argc, char **argv)
{
  int i;

  // 检查命令行参数数量
  if(argc < 2){
    fprintf(2, "用法：kill 进程ID...\n");
    exit(1);
  }
  // 向每个指定的进程发送信号
  for(i=1; i<argc; i++)
    kill(atoi(argv[i]));
  exit(0);
}
