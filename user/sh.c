// Shell.
// sh.c - 简单的shell命令解释器
// 实现了基本的命令解析、执行、重定向、管道和后台执行功能

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// 解析后的命令表示类型
#define EXEC  1  // 执行命令：普通的外部程序执行
#define REDIR 2  // 重定向命令：包含输入/输出重定向
#define PIPE  3  // 管道命令：通过管道连接两个命令
#define LIST  4  // 命令列表：多个命令用分号分隔
#define BACK  5  // 后台命令：命令在后台执行（&符号）

#define MAXARGS 10  // 命令行参数的最大数量

// 命令基类结构
// 所有具体命令类型都基于这个结构扩展
struct cmd {
  int type;  // 命令类型，取值为上面定义的EXEC、REDIR等
};

// 执行命令结构
// 表示一个简单的程序执行命令
struct execcmd {
  int type;
  char *argv[MAXARGS];  // 命令参数列表，第一个元素是程序名
  char *eargv[MAXARGS]; // 每个参数的结束位置指针
};

// 重定向命令结构
// 表示带有输入/输出重定向的命令
struct redircmd {
  int type;
  struct cmd *cmd;  // 被重定向的命令
  char *file;       // 重定向的目标文件名
  char *efile;      // 文件名的结束位置指针
  int mode;         // 文件打开模式（读/写/创建等）
  int fd;           // 要重定向的文件描述符（0=标准输入，1=标准输出等）
};

// 管道命令结构
// 表示通过管道连接的两个命令
struct pipecmd {
  int type;
  struct cmd *left;   // 管道左侧命令（输出到管道）
  struct cmd *right;  // 管道右侧命令（从管道读取）
};

// 命令列表结构
// 表示用分号分隔的多个命令
struct listcmd {
  int type;
  struct cmd *left;   // 左侧命令（先执行）
  struct cmd *right;  // 右侧命令（后执行）
};

// 后台命令结构
// 表示在后台执行的命令
struct backcmd {
  int type;
  struct cmd *cmd;  // 要在后台执行的命令
};

// 函数声明
int fork1(void);  // 安全的进程创建函数，失败时会引发panic
void panic(char*);  // 处理无法恢复的错误，打印错误信息并退出
struct cmd *parsecmd(char*);  // 解析完整的命令行字符串
void runcmd(struct cmd*) __attribute__((noreturn));  // 执行解析后的命令（永不返回）

// 执行命令。永不返回。
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(1);

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(1);
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "执行 %s 失败\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      fprintf(2, "打开 %s 失败\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

// 从标准输入获取命令
// 参数：buf-存储命令的缓冲区，nbuf-缓冲区大小
// 返回：0表示成功获取命令，-1表示EOF
int
getcmd(char *buf, int nbuf)
{
  write(2, "$ ", 2);
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

// 主函数 - shell的主循环
int
main(void)
{
  static char buf[100];
  int fd;

  // 确保至少有三个文件描述符是打开的
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // 读取并执行输入的命令
  while(getcmd(buf, sizeof(buf)) >= 0){
    char *cmd = buf;
    while (*cmd == ' ' || *cmd == '\t')
      cmd++;
    if (*cmd == '\n') // 空白命令
      continue;
    // cd命令必须由父进程执行，不能在子进程中执行
    if(cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' '){
      cmd[strlen(cmd)-1] = 0;  // 去掉换行符
      if(chdir(cmd+3) < 0)
        fprintf(2, "无法切换到目录 %s\n", cmd+3);
    } else {
      // 创建子进程执行其他命令
      if(fork1() == 0)
        runcmd(parsecmd(cmd));
      wait(0);
    }
  }
  exit(0);
}

// 当遇到无法恢复的错误时调用
// 参数：s-错误消息
void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

// 安全的fork实现，失败时会panic
// 返回：子进程ID
int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("创建进程失败");
  return pid;
}

//PAGEBREAK!
// 命令构造函数

// 创建执行命令对象
// 返回：指向新创建的EXEC类型命令的指针
struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

// 创建重定向命令对象
// 参数：subcmd-要重定向的命令，file-文件名，efile-文件名结束位置，mode-打开模式，fd-文件描述符
// 返回：指向新创建的REDIR类型命令的指针
struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

// 创建管道命令对象
// 参数：left-管道左侧命令，right-管道右侧命令
// 返回：指向新创建的PIPE类型命令的指针
struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

// 创建命令列表对象
// 参数：left-左侧命令，right-右侧命令
// 返回：指向新创建的LIST类型命令的指针
struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

// 创建后台命令对象
// 参数：subcmd-要在后台执行的命令
// 返回：指向新创建的BACK类型命令的指针
struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
//PAGEBREAK!
// 命令解析

char whitespace[] = " \t\r\n\v";  // 定义空白字符集合
char symbols[] = "<|>&;()";       // 定义特殊符号字符集合

// 获取下一个标记（token）
// 参数：ps-当前位置指针，es-结束位置，q-标记开始位置，eq-标记结束位置
// 返回：标记类型
int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  // 跳过空白字符
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  // 根据字符类型处理
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';  // 普通字符
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  // 跳过后续空白字符
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

// 查看下一个标记是否在指定的字符集中
// 参数：ps-当前位置指针，es-结束位置，toks-要检查的字符集
// 返回：1表示下一个标记在字符集中，0表示不在
int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

// 命令解析函数声明
struct cmd *parseline(char**, char*);  // 解析命令行，处理后台命令和命令列表
struct cmd *parsepipe(char**, char*);  // 解析管道命令
struct cmd *parseexec(char**, char*);  // 解析执行命令和重定向
struct cmd *nulterminate(struct cmd*);  // 为命令字符串添加NUL终止符

// 解析整个命令行字符串
// 参数：s-命令行字符串
// 返回：解析后的命令结构
struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(2, "剩余未解析内容: %s\n", s);
    panic("语法错误");
  }
  nulterminate(cmd);
  return cmd;
}

// 解析命令行，处理后台命令和命令列表
// 参数：ps-当前位置指针，es-结束位置
// 返回：解析后的命令结构
struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  // 处理后台命令（&）
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  // 处理命令列表（;）
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

// 解析管道命令（|）
// 参数：ps-当前位置指针，es-结束位置
// 返回：解析后的命令结构
struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

// 解析重定向命令（<, >, >>）
// 参数：cmd-要添加重定向的命令，ps-当前位置指针，es-结束位置
// 返回：包含重定向的命令结构
struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("重定向缺少文件名");
    switch(tok){
    case '<':  // 输入重定向（从文件读取）
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':  // 输出重定向（覆盖文件）
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE|O_TRUNC, 1);
      break;
    case '+':  // 输出重定向（追加到文件）>>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

// 解析命令块（括号内的命令）
// 参数：ps-当前位置指针，es-结束位置
// 返回：解析后的命令结构
struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("语法错误 - 缺少右括号");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

// 解析执行命令
// 参数：ps-当前位置指针，es-结束位置
// 返回：解析后的命令结构
struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  // 检查是否是命令块
  if(peek(ps, es, "("))
    return parseblock(ps, es);

  // 创建执行命令
  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  // 解析参数
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("语法错误");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("参数过多");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// 为所有计数的字符串添加NUL终止符
// 参数：cmd-命令结构
// 返回：处理后的命令结构
struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
