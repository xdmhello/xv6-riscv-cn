#include "kernel/types.h"
// ulib.c - 用户库函数实现
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/riscv.h"
#include "kernel/vm.h"
#include "user/user.h"

// 启动函数包装器，确保即使main()没有调用exit()也能正确退出
void
start(int argc, char **argv)
{
  int r;
  extern int main(int argc, char **argv);
  r = main(argc, argv);
  exit(r);
}

// 字符串复制函数
// 参数：s-目标字符串，t-源字符串
// 返回：指向目标字符串的指针
char*
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

// 字符串比较函数
// 参数：p-第一个字符串，q-第二个字符串
// 返回：如果p<q返回负数，如果p>q返回正数，如果p==q返回0
int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

// 计算字符串长度
// 参数：s-要计算的字符串
// 返回：字符串的长度（不包括终止符）
uint
strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}

// 内存设置函数
// 参数：dst-目标内存区域，c-要设置的字符，n-要设置的字节数
// 返回：指向目标内存区域的指针
void*
memset(void *dst, int c, uint n)
{
  char *cdst = (char *) dst;
  int i;
  for(i = 0; i < n; i++){
    cdst[i] = c;
  }
  return dst;
}

// 在字符串中查找字符
// 参数：s-要搜索的字符串，c-要查找的字符
// 返回：指向字符首次出现位置的指针，如果未找到返回NULL
char*
strchr(const char *s, char c)
{
  for(; *s; s++)
    if(*s == c)
      return (char*)s;
  return 0;
}

// 从标准输入读取字符串
// 参数：buf-存储读取结果的缓冲区，max-缓冲区最大容量
// 返回：指向缓冲区的指针
char*
gets(char *buf, int max)
{
  int i, cc;
  char c;

  for(i=0; i+1 < max; ){
    cc = read(0, &c, 1);
    if(cc < 1)
      break;
    buf[i++] = c;
    if(c == '\n' || c == '\r')
      break;
  }
  buf[i] = '\0';
  return buf;
}

// 获取文件状态
// 参数：n-文件名，st-存储状态信息的结构体指针
// 返回：成功返回0，失败返回-1
int
stat(const char *n, struct stat *st)
{
  int fd;
  int r;

  fd = open(n, O_RDONLY);
  if(fd < 0)
    return -1;
  r = fstat(fd, st);
  close(fd);
  return r;
}

// 将字符串转换为整数
// 参数：s-包含数字的字符串
// 返回：转换后的整数
int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

// 内存块移动函数（处理重叠内存）
// 参数：vdst-目标内存区域，vsrc-源内存区域，n-要移动的字节数
// 返回：指向目标内存区域的指针
void*
memmove(void *vdst, const void *vsrc, int n)
{
  char *dst;
  const char *src;

  dst = vdst;
  src = vsrc;
  if (src > dst) {
    // 源地址高于目标地址，正向复制
    while(n-- > 0)
      *dst++ = *src++;
  } else {
    // 源地址低于或等于目标地址，反向复制以防止重叠
    dst += n;
    src += n;
    while(n-- > 0)
      *--dst = *--src;
  }
  return vdst;
}

// 内存比较函数
// 参数：s1-第一个内存块，s2-第二个内存块，n-要比较的字节数
// 返回：如果s1<s2返回负数，如果s1>s2返回正数，如果s1==s2返回0
int
memcmp(const void *s1, const void *s2, uint n)
{
  const char *p1 = s1, *p2 = s2;
  while (n-- > 0) {
    if (*p1 != *p2) {
      return *p1 - *p2;
    }
    p1++;
    p2++;
  }
  return 0;
}

// 内存复制函数（调用memmove处理重叠情况）
// 参数：dst-目标内存区域，src-源内存区域，n-要复制的字节数
// 返回：指向目标内存区域的指针
void *
memcpy(void *dst, const void *src, uint n)
{
  return memmove(dst, src, n);
}

// 调整进程内存空间（立即分配）
// 参数：n-要增加的字节数（负数表示减少）
// 返回：调整前的程序断点指针
char *
sbrk(int n) {
  return sys_sbrk(n, SBRK_EAGER); // SBRK_EAGER=1，立即分配物理内存
}

// 调整进程内存空间（惰性分配）
// 参数：n-要增加的字节数（负数表示减少）
// 返回：调整前的程序断点指针
char *
sbrklazy(int n) {
  return sys_sbrk(n, SBRK_LAZY); // SBRK_LAZY=2，延迟分配物理内存
}

