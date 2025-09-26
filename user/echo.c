#include "kernel/types.h"
// echo.c - 命令行参数输出工具
#include "kernel/stat.h"
#include "user/user.h"

// 主函数
// 参数：argc-命令行参数数量，argv-命令行参数数组
int
main(int argc, char *argv[])
{
  int i;

  // 遍历并输出所有命令行参数
  for(i = 1; i < argc; i++){
    write(1, argv[i], strlen(argv[i]));
    // 在参数之间添加空格
    if(i + 1 < argc){
      write(1, " ", 1);
    } else {
      // 最后一个参数后添加换行符
      write(1, "\n", 1);
    }
  }
  exit(0);
}
