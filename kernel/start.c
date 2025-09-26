#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

// entry.S需要每个CPU有一个栈。
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// entry.S在机器模式下通过stack0跳转到这里。
void
start()
{
  // 将M Previous Privilege模式设置为Supervisor，用于mret。
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);

  // 将M Exception Program Counter设置为main，用于mret。
  // 需要gcc -mcmodel=medany选项
  w_mepc((uint64)main);

  // 暂时禁用分页。
  w_satp(0);

  // 将所有中断和异常委托给supervisor模式。
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE);

  // 配置物理内存保护，使supervisor模式
  // 能够访问所有物理内存。
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // 请求时钟中断。
  timerinit();

  // 保持每个CPU的hartid在其tp寄存器中，用于cpuid()函数。
  int id = r_mhartid();
  w_tp(id);

  // 切换到supervisor模式并跳转到main()函数。
  asm volatile("mret");
}

// 请求每个hart生成定时器中断。
void
timerinit()
{
  // 启用supervisor模式定时器中断。
  w_mie(r_mie() | MIE_STIE);
  
  // 启用sstc扩展（即stimecmp功能）。
  w_menvcfg(r_menvcfg() | (1L << 63)); 
  
  // 允许supervisor使用stimecmp和time指令。
  w_mcounteren(r_mcounteren() | 2);
  
  // 请求第一个定时器中断。
  w_stimecmp(r_time() + 1000000);
}
