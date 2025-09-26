//
// grind.c - 系统调用压力测试工具
// 持续并行运行随机系统调用来测试xv6操作系统的稳定性

#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/memlayout.h"
#include "kernel/riscv.h"

// 随机数生成函数 - 源自FreeBSD
// 参数：ctx-随机数上下文
// 返回：生成的随机数
int
do_rand(unsigned long *ctx)
{/*
 * 计算 x = (7^5 * x) mod (2^31 - 1)
 * 不溢出31位：
 *      (2^31 - 1) = 127773 * (7^5) + 2836
 * 源自 "Random number generators: good ones are hard to find",
 * Park and Miller, Communications of the ACM, vol. 31, no. 10,
 * October 1988, p. 1195.
 */
    long hi, lo, x;

    /* 转换到 [1, 0x7ffffffe] 范围 */
    x = (*ctx % 0x7ffffffe) + 1;
    hi = x / 127773;
    lo = x % 127773;
    x = 16807 * lo - 2836 * hi;
    if (x < 0)
        x += 0x7fffffff;
    /* 转换到 [0, 0x7ffffffd] 范围 */
    x--;
    *ctx = x;
    return (x);
}

unsigned long rand_next = 1;  // 随机数种子

// 生成随机数的简单包装函数
// 返回：生成的随机数
int
rand(void)
{
    return (do_rand(&rand_next));
}

// 执行随机系统调用的函数
// 参数：which_child-子进程标识符（0或1）
void
go(int which_child)
{
  int fd = -1;
  static char buf[999];
  char *break0 = sbrk(0);  // 记录当前堆顶位置
  uint64 iters = 0;

  mkdir("grindir");
  if(chdir("grindir") != 0){
    printf("grind: 切换到目录grindir失败\n");
    exit(1);
  }
  chdir("/");
  
  while(1){
    iters++;
    // 每500次迭代输出一个字符以显示进度
    if((iters % 500) == 0)
      write(1, which_child?"B":"A", 1);
    
    // 随机选择要执行的系统调用
    int what = rand() % 23;
    if(what == 1){
      // 创建文件a
      close(open("grindir/../a", O_CREATE|O_RDWR));
    } else if(what == 2){
      // 创建文件b
      close(open("grindir/../grindir/../b", O_CREATE|O_RDWR));
    } else if(what == 3){
      // 删除文件a
      unlink("grindir/../a");
    } else if(what == 4){
      // 切换目录并删除文件b
      if(chdir("grindir") != 0){
        printf("grind: 切换到目录grindir失败\n");
        exit(1);
      }
      unlink("../b");
      chdir("/");
    } else if(what == 5){
      // 打开文件a
      close(fd);
      fd = open("/grindir/../a", O_CREATE|O_RDWR);
    } else if(what == 6){
      // 打开文件b
      close(fd);
      fd = open("/./grindir/./../b", O_CREATE|O_RDWR);
    } else if(what == 7){
      // 写入文件
      write(fd, buf, sizeof(buf));
    } else if(what == 8){
      // 读取文件
      read(fd, buf, sizeof(buf));
    } else if(what == 9){
      // 创建目录a并在其中创建文件
      mkdir("grindir/../a");
      close(open("a/../a/./a", O_CREATE|O_RDWR));
      unlink("a/a");
    } else if(what == 10){
      // 创建目录b并在其中创建文件
      mkdir("/../b");
      close(open("grindir/../b/b", O_CREATE|O_RDWR));
      unlink("b/b");
    } else if(what == 11){
      // 删除并重新链接文件b
      unlink("b");
      link("../grindir/./../a", "../b");
    } else if(what == 12){
      // 删除并重新链接文件a
      unlink("../grindir/../a");
      link(".././b", "/grindir/../a");
    } else if(what == 13){
      // 测试fork和wait
      int pid = fork();
      if(pid == 0){
        exit(0);
      } else if(pid < 0){
        printf("grind: 创建进程失败\n");
        exit(1);
      }
      wait(0);
    } else if(what == 14){
      // 测试嵌套fork
      int pid = fork();
      if(pid == 0){
        fork();
        fork();
        exit(0);
      } else if(pid < 0){
        printf("grind: 创建进程失败\n");
        exit(1);
      }
      wait(0);
    } else if(what == 15){
      // 扩展堆内存
      sbrk(6011);
    } else if(what == 16){
      // 收缩堆内存
      if(sbrk(0) > break0)
        sbrk(-(sbrk(0) - break0));
    } else if(what == 17){
      // 测试创建、杀死进程
      int pid = fork();
      if(pid == 0){
        close(open("a", O_CREATE|O_RDWR));
        exit(0);
      } else if(pid < 0){
        printf("grind: 创建进程失败\n");
        exit(1);
      }
      if(chdir("../grindir/..") != 0){
        printf("grind: 切换目录失败\n");
        exit(1);
      }
      kill(pid);
      wait(0);
    } else if(what == 18){
      // 测试进程自杀
      int pid = fork();
      if(pid == 0){
        kill(getpid());
        exit(0);
      } else if(pid < 0){
        printf("grind: 创建进程失败\n");
        exit(1);
      }
      wait(0);
    } else if(what == 19){
      // 测试管道
      int fds[2];
      if(pipe(fds) < 0){
        printf("grind: 创建管道失败\n");
        exit(1);
      }
      int pid = fork();
      if(pid == 0){
        fork();
        fork();
        if(write(fds[1], "x", 1) != 1)
          printf("grind: 管道写入失败\n");
        char c;
        if(read(fds[0], &c, 1) != 1)
          printf("grind: 管道读取失败\n");
        exit(0);
      } else if(pid < 0){
        printf("grind: 创建进程失败\n");
        exit(1);
      }
      close(fds[0]);
      close(fds[1]);
      wait(0);
    } else if(what == 20){
      // 测试孤立文件和目录
      int pid = fork();
      if(pid == 0){
        unlink("a");
        mkdir("a");
        chdir("a");
        unlink("../a");
        fd = open("x", O_CREATE|O_RDWR);
        unlink("x");
        exit(0);
      } else if(pid < 0){
        printf("grind: 创建进程失败\n");
        exit(1);
      }
      wait(0);
    } else if(what == 21){
      // 测试文件系统基本操作
      unlink("c");
      // 这应该总是成功。检查是否有空闲的inode、
      // 文件描述符和块。
      int fd1 = open("c", O_CREATE|O_RDWR);
      if(fd1 < 0){
        printf("grind: 创建文件c失败\n");
        exit(1);
      }
      if(write(fd1, "x", 1) != 1){
        printf("grind: 写入文件c失败\n");
        exit(1);
      }
      struct stat st;
      if(fstat(fd1, &st) != 0){
        printf("grind: 获取文件状态失败\n");
        exit(1);
      }
      if(st.size != 1){
        printf("grind: 文件状态报告错误的大小 %d\n", (int)st.size);
        exit(1);
      }
      if(st.ino > 200){
        printf("grind: 文件状态报告不合理的inode号 %d\n", st.ino);
        exit(1);
      }
      close(fd1);
      unlink("c");
    } else if(what == 22){
      // 测试管道命令执行：echo hi | cat
      int aa[2], bb[2];
      if(pipe(aa) < 0){
        fprintf(2, "grind: 创建管道失败\n");
        exit(1);
      }
      if(pipe(bb) < 0){
        fprintf(2, "grind: 创建管道失败\n");
        exit(1);
      }
      int pid1 = fork();
      if(pid1 == 0){
        close(bb[0]);
        close(bb[1]);
        close(aa[0]);
        close(1);
        if(dup(aa[1]) != 1){
          fprintf(2, "grind: 文件描述符复制失败\n");
          exit(1);
        }
        close(aa[1]);
        char *args[3] = { "echo", "hi", 0 };
        exec("grindir/../echo", args);
        fprintf(2, "grind: echo: 未找到\n");
        exit(2);
      } else if(pid1 < 0){
        fprintf(2, "grind: 创建进程失败\n");
        exit(3);
      }
      int pid2 = fork();
      if(pid2 == 0){
        close(aa[1]);
        close(bb[0]);
        close(0);
        if(dup(aa[0]) != 0){
          fprintf(2, "grind: 文件描述符复制失败\n");
          exit(4);
        }
        close(aa[0]);
        close(1);
        if(dup(bb[1]) != 1){
          fprintf(2, "grind: 文件描述符复制失败\n");
          exit(5);
        }
        close(bb[1]);
        char *args[2] = { "cat", 0 };
        exec("/cat", args);
        fprintf(2, "grind: cat: 未找到\n");
        exit(6);
      } else if(pid2 < 0){
        fprintf(2, "grind: 创建进程失败\n");
        exit(7);
      }
      close(aa[0]);
      close(aa[1]);
      close(bb[1]);
      char buf[4] = { 0, 0, 0, 0 };
      read(bb[0], buf+0, 1);
      read(bb[0], buf+1, 1);
      read(bb[0], buf+2, 1);
      close(bb[0]);
      int st1, st2;
      wait(&st1);
      wait(&st2);
      if(st1 != 0 || st2 != 0 || strcmp(buf, "hi\n") != 0){
        printf("grind: 命令管道执行失败 %d %d \"%s\"\n", st1, st2, buf);
        exit(1);
      }
    }
  }
}

// 创建两个子进程运行go函数进行并行测试
void
iter()
{
  // 清理可能存在的测试文件
  unlink("a");
  unlink("b");
  
  // 创建第一个子进程
  int pid1 = fork();
  if(pid1 < 0){
    printf("grind: 创建进程失败\n");
    exit(1);
  }
  if(pid1 == 0){
    // 更改随机数种子以产生不同的随机序列
    rand_next ^= 31;
    go(0);
    exit(0);
  }

  // 创建第二个子进程
  int pid2 = fork();
  if(pid2 < 0){
    printf("grind: 创建进程失败\n");
    exit(1);
  }
  if(pid2 == 0){
    // 更改随机数种子以产生不同的随机序列
    rand_next ^= 7177;
    go(1);
    exit(0);
  }

  // 等待子进程完成
  int st1 = -1;
  wait(&st1);
  // 如果第一个子进程失败，杀死第二个子进程
  if(st1 != 0){
    kill(pid1);
    kill(pid2);
  }
  int st2 = -1;
  wait(&st2);

  exit(0);
}

// 主函数 - 持续运行iter函数进行压力测试
int
main()
{
  while(1){
    int pid = fork();
    if(pid == 0){
      iter();
      exit(0);
    }
    if(pid > 0){
      wait(0);
    }
    pause(20);
    rand_next += 1;
  }
}
