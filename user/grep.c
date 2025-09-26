// Simple grep.  Only supports ^ . * $ operators.

// grep.c - 文本搜索工具
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char buf[1024];  // 缓冲区用于存储文件内容
int match(char*, char*);  // 模式匹配函数声明

// 在文件中搜索指定模式
// 参数：pattern-要搜索的模式，fd-文件描述符
void
grep(char *pattern, int fd)
{
  int n, m;
  char *p, *q;

  m = 0;
  // 循环读取文件内容
  while((n = read(fd, buf+m, sizeof(buf)-m-1)) > 0){
    m += n;
    buf[m] = '\0';
    p = buf;
    // 按行处理文件内容
    while((q = strchr(p, '\n')) != 0){
      *q = 0;
      // 检查当前行是否匹配模式
      if(match(pattern, p)){
        *q = '\n';
        write(1, p, q+1 - p);  // 输出匹配的行
      }
      p = q+1;
    }
    // 处理未完成的行
    if(m > 0){
      m -= p - buf;
      memmove(buf, p, m);
    }
  }
}

// 主函数
// 参数：argc-命令行参数数量，argv-命令行参数数组
// argv[1]-搜索模式，argv[2...]要搜索的文件
int
main(int argc, char *argv[])
{
  int fd, i;
  char *pattern;

  // 检查是否提供了搜索模式
  if(argc <= 1){
    fprintf(2, "用法：grep 模式 [文件 ...]\n");
    exit(1);
  }
  pattern = argv[1];

  // 如果没有指定文件，从标准输入读取
  if(argc <= 2){
    grep(pattern, 0);
    exit(0);
  }

  // 遍历所有指定的文件
  for(i = 2; i < argc; i++){
    if((fd = open(argv[i], O_RDONLY)) < 0){
      printf("grep: 无法打开 %s\n", argv[i]);
      exit(1);
    }
    grep(pattern, fd);
    close(fd);
  }
  exit(0);
}

// 正则表达式匹配器 - 源自 Kernighan & Pike
// 《编程实践》第九章，或参考
// https://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html

int matchhere(char*, char*);  // 在文本开头搜索正则表达式
int matchstar(int, char*, char*);  // 处理*通配符

// 匹配正则表达式和文本
// 参数：re-正则表达式，text-要搜索的文本
// 返回：1表示匹配成功，0表示匹配失败
int
match(char *re, char *text)
{
  // 如果以^开头，仅匹配文本开头
  if(re[0] == '^')
    return matchhere(re+1, text);
  // 否则尝试从文本的每个位置开始匹配
  do{  // 必须检查空字符串
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

// 在文本开头搜索正则表达式
// 参数：re-正则表达式，text-要搜索的文本
// 返回：1表示匹配成功，0表示匹配失败
int matchhere(char *re, char *text)
{
  // 空正则表达式匹配任何内容
  if(re[0] == '\0')
    return 1;
  // 处理*通配符
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  // 如果以$结尾，仅匹配文本结尾
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  // 匹配单个字符
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

// 处理c*模式，匹配零个或多个c字符
// 参数：c-要匹配的字符，re-后续的正则表达式，text-要搜索的文本
// 返回：1表示匹配成功，0表示匹配失败
int matchstar(int c, char *re, char *text)
{
  do{  // * 匹配零个或多个实例
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}

