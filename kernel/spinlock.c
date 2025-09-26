// 互斥自旋锁

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// 获取锁。
// 循环（自旋）直到锁被获取。
void
acquire(struct spinlock *lk)
{
  push_off(); // 禁用中断以避免死锁。
  if(holding(lk))
    panic("acquire");

  // 在RISC-V上，sync_lock_test_and_set会转换为原子交换：
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;

  // 告诉C编译器和处理器不要在这个点之后移动加载或存储操作，
  // 以确保临界区的内存引用严格在获取锁之后发生。
  // 在RISC-V上，这会发出一个fence指令。
  __sync_synchronize();

  // 记录锁获取的信息，用于holding()和调试。
  lk->cpu = mycpu();
}

// 释放锁。
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->cpu = 0;

  // 告诉C编译器和CPU不要在这个点之后移动加载或存储操作，
  // 以确保在释放锁之前，临界区中的所有存储对其他CPU可见，
  // 并且临界区中的加载严格在释放锁之前发生。
  // 在RISC-V上，这会发出一个fence指令。
  __sync_synchronize();

  // 释放锁，等同于lk->locked = 0。
  // 这段代码不使用C赋值，因为C标准暗示赋值可能用多个存储指令实现。
  // 在RISC-V上，sync_lock_release会转换为原子交换：
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  __sync_lock_release(&lk->locked);

  pop_off();
}

// 检查此CPU是否持有该锁。
// 中断必须关闭。
int
holding(struct spinlock *lk)
{
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// push_off/pop_off类似于intr_off()/intr_on()，但它们是匹配的：
// 需要两次pop_off()来撤销两次push_off()。此外，如果中断最初是关闭的，
// 那么push_off和pop_off操作后它们仍然是关闭的。

void
push_off(void)
{
  int old = intr_get();

  // 禁用中断以防止在使用mycpu()时发生非自愿的上下文切换。
  intr_off();

  if(mycpu()->noff == 0)
    mycpu()->intena = old;
  mycpu()->noff += 1;
}

void
pop_off(void)
{
  struct cpu *c = mycpu();
  if(intr_get())
    panic("pop_off - interruptible");
  if(c->noff < 1)
    panic("pop_off");
  c->noff -= 1;
  if(c->noff == 0 && c->intena)
    intr_on();
}
