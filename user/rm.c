#include "kernel/types.h"
// rm.c - 文件删除工具
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
    fprintf(2, "用法：rm 文件...\n");
    exit(1);
  }

  // 删除每个指定的文件
  for(i = 1; i < argc; i++){
    if(unlink(argv[i]) < 0){
      fprintf(2, "rm: 删除 %s 失败\n", argv[i]);
      break;
    }
  }

  exit(0);
}
