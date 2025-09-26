#include "kernel/types.h"
// wc.c - 文本统计工具（统计行数、字数、字符数）
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

char buf[512];

// 统计文件的行数、字数和字符数
// 参数：fd-文件描述符，name-文件名
void
wc(int fd, char *name)
{
  int i, n;
  int l, w, c, inword;  // l-行数，w-字数，c-字符数，inword-是否在单词内

  // 初始化计数器
  l = w = c = 0;
  inword = 0;
  // 读取文件内容并进行统计
  while((n = read(fd, buf, sizeof(buf))) > 0){
    for(i=0; i<n; i++){
      c++;  // 统计字符数
      if(buf[i] == '\n')  // 统计行数
        l++;
      // 检查是否为空白字符
      if(strchr(" \r\t\n\v", buf[i]))
        inword = 0;
      else if(!inword){  // 新单词开始
        w++;  // 统计字数
        inword = 1;
      }
    }
  }
  if(n < 0){
    printf("wc: 读取错误\n");
    exit(1);
  }
  // 输出统计结果：行数 字数 字符数 文件名
  printf("%d %d %d %s\n", l, w, c, name);
}

// 主函数
// 参数：argc-命令行参数数量，argv-命令行参数数组
int
main(int argc, char *argv[])
{
  int fd, i;

  // 如果没有参数，从标准输入读取
  if(argc <= 1){
    wc(0, "");
    exit(0);
  }

  // 依次处理每个文件参数
  for(i = 1; i < argc; i++){
    if((fd = open(argv[i], O_RDONLY)) < 0){
      printf("wc: 无法打开 %s\n", argv[i]);
      exit(1);
    }
    wc(fd, argv[i]);
    close(fd);
  }
  exit(0);
}
