// Test that fork fails gracefully.
// forktest.c - 进程创建测试工具
// 小型可执行文件，用于测试进程表填充的限制

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define N  1000  // 尝试创建的进程数量

// 简单的打印函数
// 参数：s-要打印的字符串
void
print(const char *s)
{
  write(1, s, strlen(s));
}

// 测试fork系统调用的功能和限制
void
forktest(void)
{
  int n, pid;

  print("fork测试\n");

  // 尝试创建N个子进程
  for(n=0; n<N; n++){
    pid = fork();
    if(pid < 0)  // 创建失败，达到限制
      break;
    if(pid == 0)  // 子进程立即退出
      exit(0);
  }

  // 检查是否真的创建了N个子进程（这应该不会发生，因为进程表有限）
  if(n == N){
    print("fork声称成功工作了N次！\n");
    exit(1);
  }

  // 等待所有子进程退出
  for(; n > 0; n--){
    if(wait(0) < 0){
      print("wait提前停止\n");
      exit(1);
    }
  }

  // 确认所有子进程都已退出
  if(wait(0) != -1){
    print("wait获取了太多进程\n");
    exit(1);
  }

  print("fork测试通过\n");
}

// 主函数
int
main(void)
{
  forktest();
  exit(0);
}
