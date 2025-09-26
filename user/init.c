// init: The initial user-level program
// init.c - xv6系统初始化进程
// 这是用户空间的第一个进程，负责启动shell并收养所有的孤儿进程

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/fs.h"
#include "kernel/file.h"
#include "user/user.h"
#include "kernel/fcntl.h"

char *argv[] = { "sh", 0 };  // shell命令的参数列表

// 主函数
// xv6系统启动后执行的第一个用户空间进程
int
main(void)
{
  int pid, wpid;

  // 确保控制台设备存在并打开
  if(open("console", O_RDWR) < 0){
    mknod("console", CONSOLE, 0);  // 创建控制台设备节点
    open("console", O_RDWR);       // 打开控制台设备
  }
  dup(0);  // 复制文件描述符0到1（标准输出）
  dup(0);  // 复制文件描述符0到2（标准错误）

  // 无限循环：启动shell并等待其退出后重新启动
  for(;;){
    printf("init: 启动shell\n");
    pid = fork();
    if(pid < 0){
      printf("init: 创建进程失败\n");
      exit(1);
    }
    if(pid == 0){
      // 在子进程中执行shell
      exec("sh", argv);
      printf("init: 执行shell失败\n");
      exit(1);
    }

    // 等待子进程退出或收养孤儿进程
    for(;;){
      // wait()调用在shell退出或有孤儿进程退出时返回
      wpid = wait((int *) 0);
      if(wpid == pid){
        // shell退出了，重新启动它
        break;
      } else if(wpid < 0){
        printf("init: wait返回错误\n");
        exit(1);
      } else {
        // 这是一个孤儿进程，无需做任何处理
        // （init进程作为所有孤儿进程的父进程，调用wait会自动清理它们的资源）
      }
    }
  }
}
