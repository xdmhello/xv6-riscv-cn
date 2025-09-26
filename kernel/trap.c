#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[];

// 在 kernelvec.S 中定义，调用 kerneltrap()。
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// 设置内核模式下接收异常和陷阱
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// 处理来自用户空间的中断、异常或系统调用。
// 从 trampoline.S 调用并返回。
// 返回值是用户 satp，供 trampoline.S 切换使用。
//
uint64
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: 非用户模式调用");

  // 发送中断和异常到 kerneltrap()，
  // 因为我们现在在内核中。
  w_stvec((uint64)kernelvec);  //DOC: kernelvec

  struct proc *p = myproc();
  
  // 保存用户程序计数器。
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // 系统调用

    if(killed(p))
      kexit(-1);

    // sepc 指向 ecall 指令，
    // 但我们想返回到下一条指令。
    p->trapframe->epc += 4;

    // 中断会改变 sepc、scause 和 sstatus，
    // 所以现在我们处理完这些寄存器后再启用中断。
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // 正常
  } else if((r_scause() == 15 || r_scause() == 13) &&
            vmfault(p->pagetable, r_stval(), (r_scause() == 13)? 1 : 0) != 0) {
    // 延迟分配页面的页错误
  } else {
    printf("usertrap(): 意外的 scause 0x%lx pid=%d\n", r_scause(), p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    kexit(-1);

  // 如果是定时器中断，则放弃 CPU。
  if(which_dev == 2)
    yield();

  prepare_return();

  // 用户页表，供 trampoline.S 切换使用
  uint64 satp = MAKE_SATP(p->pagetable);

  // 返回 trampoline.S；satp 值在 a0 中。
  return satp;
}

//
// 设置 trapframe 和控制寄存器，为返回用户空间做准备
//
void
prepare_return(void)
{
  struct proc *p = myproc();

  // 我们即将把陷阱的目标从
  // kerneltrap() 切换到 usertrap()。由于从内核代码到 usertrap 的陷阱会是灾难，所以关闭中断。
  intr_off();

  // 发送系统调用、中断和异常到 trampoline.S 中的 uservec
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // 设置 trapframe 值，当进程下次陷入内核时 uservec 将需要这些值。
  p->trapframe->kernel_satp = r_satp();         // 内核页表
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // 进程的内核栈
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // 用于 cpuid() 的 hartid

  // 设置 trampoline.S 的 sret 将使用的寄存器
  // 以进入用户空间。
  
  // 设置 S Previous Privilege 模式为用户模式。
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // 清除 SPP 为 0，表示用户模式
  x |= SSTATUS_SPIE; // 启用用户模式下的中断
  w_sstatus(x);

  // 设置 S Exception Program Counter 为保存的用户 pc。
  w_sepc(p->trapframe->epc);
}

// 内核代码的中断和异常通过 kernelvec 进入这里，
// 在当前的内核栈上。
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: 非超级用户模式调用");
  if(intr_get() != 0)
    panic("kerneltrap: 中断已启用");

  if((which_dev = devintr()) == 0){
    // 来自未知来源的中断或陷阱
    printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // 如果是定时器中断，则放弃 CPU。
  if(which_dev == 2 && myproc() != 0)
    yield();

  // yield() 可能导致一些陷阱发生，
  // 因此恢复陷阱寄存器以供 kernelvec.S 的 sepc 指令使用。
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  if(cpuid() == 0){
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
  }

  // 请求下一次定时器中断。这也会清除
  // 中断请求。1000000 大约是十分之一秒。
  w_stimecmp(r_time() + 1000000);
}

// 检查是否是外部中断或软件中断，并处理它。
// 如果是定时器中断，返回 2；
// 如果是其他设备，返回 1；
// 如果未识别，返回 0。
int
devintr()
{
  uint64 scause = r_scause();

  if(scause == 0x8000000000000009L){
    // 这是通过 PLIC 的超级用户外部中断。

    // irq 表示哪个设备中断了。
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("意外的中断 irq=%d\n", irq);
    }

    // PLIC 允许每个设备一次最多引发一个中断；
    // 告诉 PLIC 该设备现在可以再次中断。
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000005L){
    // 定时器中断。
    clockintr();
    return 2;
  } else {
    return 0;
  }
}

