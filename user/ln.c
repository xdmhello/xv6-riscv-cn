#include "kernel/types.h"
// ln.c - 创建文件链接工具
#include "kernel/stat.h"
#include "user/user.h"

// 主函数 - 创建文件硬链接
// 参数：argc-命令行参数数量，argv-命令行参数数组
// argv[1]-源文件，argv[2]-目标链接文件
int
main(int argc, char *argv[])
{
  // 检查参数数量是否正确
  if(argc != 3){
    fprintf(2, "用法：ln 源文件 目标文件\n");
    exit(1);
  }
  // 调用link系统调用创建硬链接
  if(link(argv[1], argv[2]) < 0)
    fprintf(2, "创建链接 %s %s 失败\n", argv[1], argv[2]);
  exit(0);
}
