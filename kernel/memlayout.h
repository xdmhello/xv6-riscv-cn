// 物理内存布局

// qemu -machine virt的设置如下，
// 基于qemu的hw/riscv/virt.c:
//
// 00001000 -- 引导ROM，由qemu提供
// 02000000 -- CLINT（核心本地中断控制器）
// 0C000000 -- PLIC（平台级中断控制器）
// 10000000 -- uart0 
// 10001000 -- virtio磁盘 
// 80000000 -- qemu的引导ROM在此处加载内核，
//             然后跳转到此。
// 80000000之后是未使用的RAM。

// 内核使用物理内存如下：
// 80000000 -- entry.S，然后是内核文本和数据
// end -- 内核页面分配区域的开始
// PHYSTOP -- 内核使用的RAM结束

// qemu在此处的物理内存中放置UART寄存器。
#define UART0 0x10000000L
#define UART0_IRQ 10

// virtio mmio接口
#define VIRTIO0 0x10001000
#define VIRTIO0_IRQ 1

// qemu在此处放置平台级中断控制器（PLIC）。
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// 内核期望有RAM
// 供内核和用户页面使用
// 从物理地址0x80000000到PHYSTOP。
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)

// 将trampoline页面映射到最高地址，
// 同时在用户和内核空间中。
#define TRAMPOLINE (MAXVA - PGSIZE)

// 将内核栈映射到trampoline下方，
// 每个栈都被无效的保护页面包围。
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)

// 用户内存布局。
// 地址从0开始：
//   文本
//   原始数据和bss段
//   固定大小的栈
//   可扩展的堆
//   ...
//   TRAPFRAME（p->trapframe，由trampoline使用）
//   TRAMPOLINE（与内核中的同一页面）
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
