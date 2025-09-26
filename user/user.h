#define SBRK_ERROR ((char *)-1)

struct stat;

// 系统调用
extern int fork(void);              // 创建一个新进程
extern int exit(int) __attribute__((noreturn)); // 终止当前进程
extern int wait(int*);              // 等待子进程结束
extern int pipe(int*);              // 创建管道
extern int write(int, const void*, int); // 写入文件描述符
extern int read(int, void*, int);   // 从文件描述符读取
extern int close(int);              // 关闭文件描述符
extern int kill(int);               // 终止指定进程
extern int exec(const char*, char**); // 执行程序
extern int open(const char*, int);  // 打开文件
extern int mknod(const char*, short, short); // 创建设备文件
extern int unlink(const char*);     // 删除文件
extern int fstat(int fd, struct stat*); // 获取文件状态
extern int link(const char*, const char*); // 创建硬链接
extern int mkdir(const char*);      // 创建目录
extern int chdir(const char*);      // 改变当前目录
extern int dup(int);                // 复制文件描述符
extern int getpid(void);            // 获取进程ID
extern char* sys_sbrk(int,int);     // 系统调用：调整进程内存空间
extern int pause(int);              // 暂停执行
extern int uptime(void);            // 获取系统运行时间

// ulib.c - 用户库函数
extern int stat(const char*, struct stat*); // 获取文件状态
extern char* strcpy(char*, const char*);    // 字符串复制
extern void *memmove(void*, const void*, int); // 内存块移动
extern char* strchr(const char*, char c);   // 查找字符在字符串中首次出现的位置
extern int strcmp(const char*, const char*); // 字符串比较
extern char* gets(char*, int max);          // 读取字符串
extern uint strlen(const char*);            // 字符串长度
extern void* memset(void*, int, uint);      // 内存设置
extern int atoi(const char*);               // 字符串转整数
extern int memcmp(const void *, const void *, uint); // 内存比较
extern void *memcpy(void *, const void *, uint);    // 内存复制
extern char* sbrk(int);                     // 调整进程内存空间
extern char* sbrklazy(int);                 // 惰性分配内存空间

// printf.c - 格式化输出函数
extern void fprintf(int, const char*, ...) __attribute__ ((format (printf, 2, 3))); // 输出到文件描述符
extern void printf(const char*, ...) __attribute__ ((format (printf, 1, 2)));       // 格式化输出到控制台

// umalloc.c - 内存分配函数
extern void* malloc(uint);                  // 分配内存
extern void free(void*);                    // 释放内存
