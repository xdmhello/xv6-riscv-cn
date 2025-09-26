#include "kernel/types.h"
// dorphan.c - 孤立目录测试工具
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

// 创建一个孤立目录并测试test-xv6.py是否能够回收它

#define BUFSZ 500  // 缓冲区大小

char buf[BUFSZ];  // 缓冲区

// 主函数 - 创建孤立目录
// 参数：argc-命令行参数数量，argv-命令行参数数组
int
main(int argc, char **argv)
{
  char *s = argv[0];  // 程序名称

  // 创建名为"dd"的目录
  if(mkdir("dd") != 0){
    printf("%s: 创建目录dd失败\n", s);
    exit(1);
  }

  // 进入"dd"目录
  if(chdir("dd") != 0){
    printf("%s: 切换到目录dd失败\n", s);
    exit(1);
  }

  // 删除父目录中对"dd"的引用，创建孤立目录
  if (unlink("../dd") < 0) {
    printf("%s: 删除链接失败\n", s);
    exit(1);
  }
  printf("等待被杀死并回收\n");
  // 进入无限循环，直到被杀死
  for(;;) pause(1000);
}
