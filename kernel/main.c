#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start()函数在所有CPU的监管模式下跳转到这里。
void
main()
{
  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 内核正在启动\n");
    printf("\n");
    kinit();         // 物理页分配器
    kvminit();       // 创建内核页表
    kvminithart();   // 开启分页
    procinit();      // 进程表
    trapinit();      // 陷阱向量
    trapinithart();  // 安装内核陷阱向量
    plicinit();      // 设置中断控制器
    plicinithart();  // 向PLIC请求设备中断
    binit();         // 缓冲区缓存
    iinit();         // inode表
    fileinit();      // 文件表
    virtio_disk_init(); // 模拟硬盘
    userinit();      // 第一个用户进程
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    printf("处理器 %d 正在启动\n", cpuid());
    kvminithart();    // 开启分页
    trapinithart();   // 安装内核陷阱向量
    plicinithart();   // 向PLIC请求设备中断
  }

  scheduler();        
}
