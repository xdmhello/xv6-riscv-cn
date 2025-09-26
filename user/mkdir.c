#include "kernel/types.h"
// mkdir.c - 目录创建工具
#include "kernel/stat.h"
#include "user/user.h"

// 主函数
// 参数：argc-命令行参数数量，argv-命令行参数数组
int
main(int argc, char *argv[])
{
  int i;

  // 检查命令行参数数量
  if(argc < 2){
    fprintf(2, "用法：mkdir 目录...\n");
    exit(1);
  }

  // 为每个参数创建目录
  for(i = 1; i < argc; i++){
    if(mkdir(argv[i]) < 0){
      fprintf(2, "mkdir: 创建 %s 失败\n", argv[i]);
      break;
    }
  }

  exit(0);
}
