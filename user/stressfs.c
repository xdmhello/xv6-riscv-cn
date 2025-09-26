// Demonstrate that moving the "acquire" in iderw after the loop that
// stressfs.c - 文件系统压力测试工具
// 通过多进程并发读写文件来测试xv6文件系统的稳定性
// 主要用于测试IDE队列追加操作中可能存在的竞争条件

// 为了使测试生效，还需要在iderw的idequeue遍历循环中添加自旋操作
// 添加以下代码可在QEMU(2.1GHz CPU)中运行stressfs约5次后触发panic:
//    for (i = 0; i < 40000; i++)
//      asm volatile("");

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

// 主函数 - 通过多进程并发读写文件测试文件系统
// 参数：argc-命令行参数数量，argv-命令行参数数组
int
main(int argc, char *argv[])
{
  int fd;    // 文件描述符
  int i;     // 循环计数器
  char path[] = "stressfs0";  // 测试文件名模板
  char data[512];  // 写入文件的数据缓冲区

  printf("文件系统压力测试开始\n");
  memset(data, 'a', sizeof(data));  // 初始化数据缓冲区，填充字符'a'

  // 创建3个子进程，总共4个进程并发执行
  for(i = 0; i < 4; i++)
    if(fork() > 0)  // 父进程跳出循环
      break;

  printf("进程 %d 开始写入文件\n", i);

  // 为每个进程创建不同的文件名（stressfs0, stressfs1等）
  path[8] += i;
  // 打开（创建）文件
  fd = open(path, O_CREATE | O_RDWR);
  // 向文件写入数据20次
  for(i = 0; i < 20; i++)
//    printf(fd, "%d\n", i); // 注释掉的调试代码
    write(fd, data, sizeof(data));  // 写入缓冲区内容
  close(fd);  // 关闭文件

  printf("读取文件内容\n");

  // 以只读方式打开文件
  fd = open(path, O_RDONLY);
  // 从文件读取数据20次
  for (i = 0; i < 20; i++)
    read(fd, data, sizeof(data));  // 读取文件内容到缓冲区
  close(fd);  // 关闭文件

  // 等待子进程完成
  wait(0);

  exit(0);
}
