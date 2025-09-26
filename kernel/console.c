//
// 控制台输入和输出，通过UART接口。
// 一次读取一行。
// 实现特殊输入字符：
//   换行符 -- 行结束
//   control-h -- 退格
//   control-u -- 清除整行
//   control-d -- 文件结束
//   control-p -- 打印进程列表
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

#define BACKSPACE 0x100
#define C(x)  ((x)-'@')  // Control-x

//
// 向UART发送一个字符。
// 被printf()调用，用于回显输入字符，
// 但不是从write()调用。
//
void
consputc(int c)
{
  if(c == BACKSPACE){
    // 如果用户输入了退格键，用空格覆盖。
    uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
  } else {
    uartputc_sync(c);
  }
}

struct {
  struct spinlock lock;
  
  // input
#define INPUT_BUF_SIZE 128
  char buf[INPUT_BUF_SIZE];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} cons;

//
// 用户对控制台的write()调用进入这里。
//
int
consolewrite(int user_src, uint64 src, int n)
{
  char buf[32];
  int i = 0;

  while(i < n){
    int nn = sizeof(buf);
    if(nn > n - i)
      nn = n - i;
    if(either_copyin(buf, user_src, src+i, nn) == -1)
      break;
    uartwrite(buf, nn);
    i += nn;
  }

  return i;
}

//
// 用户从控制台的read()调用进入这里。
// 将（最多）一整行输入复制到dst。
// user_dst表示dst是用户地址还是内核地址。
//
int
consoleread(int user_dst, uint64 dst, int n)
{
  uint target;
  int c;
  char cbuf;

  target = n;
  acquire(&cons.lock);
  while(n > 0){
    // 等待中断处理程序将一些输入放入cons.buffer。
    while(cons.r == cons.w){
      if(killed(myproc())){
        release(&cons.lock);
        return -1;
      }
      sleep(&cons.r, &cons.lock);
    }

    c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

    if(c == C('D')){  // end-of-file
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        cons.r--;
      }
      break;
    }

    // copy the input byte to the user-space buffer.
    cbuf = c;
    if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;

    dst++;
    --n;

    if(c == '\n'){
      // 一整行已到达，返回到用户级别的read()。
      break;
    }
  }
  release(&cons.lock);

  return target - n;
}

//
// 控制台输入中断处理程序。
// uartintr()为输入字符调用此函数。
// 执行删除/清除处理，追加到cons.buf，
// 如果一整行到达，唤醒consoleread()。
//
void
consoleintr(int c)
{
  acquire(&cons.lock);

  switch(c){
  case C('P'):  // 打印进程列表。
    procdump();
    break;
  case C('U'):  // 清除整行。
    while(cons.e != cons.w &&
          cons.buf[(cons.e-1) % INPUT_BUF_SIZE] != '\n'){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  case C('H'): // 退格
  case '\x7f': // 删除键
    if(cons.e != cons.w){
      cons.e--;
      consputc(BACKSPACE);
    }
    break;
  default:
    if(c != 0 && cons.e-cons.r < INPUT_BUF_SIZE){
      c = (c == '\r') ? '\n' : c;

      // 回显给用户。
      consputc(c);

      // 存储以供consoleread()使用。
      cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

      if(c == '\n' || c == C('D') || cons.e-cons.r == INPUT_BUF_SIZE){
        // 如果整行（或文件结束符）已到达，则唤醒consoleread()
        cons.w = cons.e;
        wakeup(&cons.r);
      }
    }
    break;
  }
  
  release(&cons.lock);
}

void
consoleinit(void)
{
  initlock(&cons.lock, "cons");

  uartinit();

  // 将读和写系统调用连接到
  // consoleread和consolewrite。
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
}
