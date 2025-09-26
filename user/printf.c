#include "kernel/types.h"
// printf.c - 格式化输出函数实现
// 提供类似C标准库的printf系列函数，支持格式化字符串输出
#include "kernel/stat.h"
#include "user/user.h"

#include <stdarg.h>

// 用于数字转换的字符表（包含十进制和十六进制的所有可能字符）
static char digits[] = "0123456789ABCDEF";

// 向指定文件描述符写入单个字符
// 参数：fd-文件描述符，c-要写入的字符
static void
putc(int fd, char c)
{
  write(fd, &c, 1);
}

// 打印整数到指定文件描述符
// 参数：fd-文件描述符，xx-要打印的整数，base-进制（如10或16），sgn-是否为有符号数
// 功能：将整数转换为指定进制的字符串并写入文件描述符
static void
printint(int fd, long long xx, int base, int sgn)
{
  char buf[20];  // 存储转换后的字符
  int i, neg;    // i是缓冲区索引，neg标记是否为负数
  unsigned long long x;  // 使用无符号类型处理数字转换

  // 处理负数情况
  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;  // 取绝对值
  } else {
    x = xx;   // 正数或无符号数直接使用
  }

  // 将数字转换为字符并存储在缓冲区（逆序）
  i = 0;
  do{
    buf[i++] = digits[x % base];  // 取余并转换为字符
  }while((x /= base) != 0);  // 继续处理商
  
  // 如果是负数，添加负号
  if(neg)
    buf[i++] = '-';

  // 逆序打印字符（因为转换是逆序存储的）
  while(--i >= 0)
    putc(fd, buf[i]);
}

// 打印指针值（以十六进制格式）到指定文件描述符
// 参数：fd-文件描述符，x-指针值
// 功能：将64位指针值以0x开头的十六进制格式输出
static void
printptr(int fd, uint64 x) {
  int i;
  putc(fd, '0');
  putc(fd, 'x');  // 输出十六进制前缀
  
  // 打印64位指针的每个字节（每个字节两个十六进制数字）
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4){
    // 每次取最高位的4位（一个十六进制数字）
    putc(fd, digits[x >> (sizeof(uint64) * 8 - 4)]);
  }
}

// 格式化输出到指定文件描述符（可变参数列表版本）
// 参数：fd-文件描述符，fmt-格式化字符串，ap-可变参数列表
// 支持的格式说明符：%d（十进制整数）、%x（十六进制整数）、%p（指针）、%c（字符）、%s（字符串）、%%（百分号）
// 以及long和long long版本：%ld、%lld、%lx、%llx、%lu、%llu
void
vprintf(int fd, const char *fmt, va_list ap)
{
  char *s;
  int c0, c1, c2, i, state;
  
  // state=0表示正常字符，state='%'表示遇到了百分号
  state = 0;
  for(i = 0; fmt[i]; i++){
    c0 = fmt[i] & 0xff;  // 当前字符
    if(state == 0){
      if(c0 == '%'){
        state = '%';  // 遇到百分号，进入格式说明符处理状态
      } else {
        putc(fd, c0);  // 普通字符直接输出
      }
    } else if(state == '%'){
      c1 = c2 = 0;
      if(c0) c1 = fmt[i+1] & 0xff;  // 下一个字符
      if(c1) c2 = fmt[i+2] & 0xff;  // 下下个字符
      
      // 处理不同的格式说明符
      if(c0 == 'd'){
        // 十进制整数（有符号）
        printint(fd, va_arg(ap, int), 10, 1);
      } else if(c0 == 'l' && c1 == 'd'){
        // 长整型十进制整数（有符号）
        printint(fd, va_arg(ap, uint64), 10, 1);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'd'){
        // 长整型十进制整数（有符号）
        printint(fd, va_arg(ap, uint64), 10, 1);
        i += 2;
      } else if(c0 == 'u'){
        // 十进制整数（无符号）
        printint(fd, va_arg(ap, uint32), 10, 0);
      } else if(c0 == 'l' && c1 == 'u'){
        // 长整型十进制整数（无符号）
        printint(fd, va_arg(ap, uint64), 10, 0);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'u'){
        // 长整型十进制整数（无符号）
        printint(fd, va_arg(ap, uint64), 10, 0);
        i += 2;
      } else if(c0 == 'x'){
        // 十六进制整数（无符号）
        printint(fd, va_arg(ap, uint32), 16, 0);
      } else if(c0 == 'l' && c1 == 'x'){
        // 长整型十六进制整数（无符号）
        printint(fd, va_arg(ap, uint64), 16, 0);
        i += 1;
      } else if(c0 == 'l' && c1 == 'l' && c2 == 'x'){
        // 长整型十六进制整数（无符号）
        printint(fd, va_arg(ap, uint64), 16, 0);
        i += 2;
      } else if(c0 == 'p'){
        // 指针值（以十六进制格式输出）
        printptr(fd, va_arg(ap, uint64));
      } else if(c0 == 'c'){
        // 单个字符
        putc(fd, va_arg(ap, uint32));
      } else if(c0 == 's'){
        // 字符串
        if((s = va_arg(ap, char*)) == 0)
          s = "(null)";
        for(; *s; s++)
          putc(fd, *s);
      } else if(c0 == '%'){
        // 输出百分号本身
        putc(fd, '%');
      } else {
        // 未知的格式序列，原样打印以引起注意
        putc(fd, '%');
        putc(fd, c0);
      }

      state = 0;  // 重置状态
    }
  }
}

// 格式化输出到指定文件描述符
// 参数：fd-文件描述符，fmt-格式化字符串，...-要格式化的可变参数
void
fprintf(int fd, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);  // 初始化可变参数列表
  vprintf(fd, fmt, ap);  // 调用vprintf执行实际的格式化输出
}

// 格式化输出到标准输出（文件描述符1）
// 参数：fmt-格式化字符串，...-要格式化的可变参数
void
printf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);  // 初始化可变参数列表
  vprintf(1, fmt, ap);  // 调用vprintf输出到标准输出
}
