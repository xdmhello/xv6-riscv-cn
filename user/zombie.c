// Create a zombie process that
// zombie.c - 僵尸进程测试程序
// 测试子进程退出时必须被重新父进程领养的机制

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 主函数
// 创建一个僵尸进程并观察其行为
int
main(void)
{
  // 创建子进程
  if(fork() > 0)
    pause(5);  // 让父进程暂停5秒，确保子进程先退出
  exit(0);
}
