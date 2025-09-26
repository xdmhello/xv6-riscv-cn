// 用于内核上下文切换的保存寄存器。
struct context {
  uint64 ra;
  uint64 sp;

  // 被调用者保存的寄存器
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// 每个CPU的状态。
struct cpu {
  struct proc *proc;          // 在该CPU上运行的进程，或为空。
  struct context context;     // 切换到这里进入调度器。
  int noff;                   // push_off()嵌套深度。
  int intena;                 // push_off()前中断是否启用？
};

extern struct cpu cpus[NCPU];

// trampoline.S中陷阱处理代码的进程数据。
// 单独位于用户页表中trampoline页面下方的一页。
// 在内核页表中没有特殊映射。
// trampoline.S中的uservec将用户寄存器保存在陷阱帧中，
// 然后从陷阱帧的kernel_sp、kernel_hartid、kernel_satp初始化寄存器，
// 并跳转到kernel_trap。
// trampoline.S中的usertrapret()和userret设置陷阱帧的kernel_*，
// 从陷阱帧恢复用户寄存器，切换到用户页表，进入用户空间。
// 陷阱帧包含被调用者保存的用户寄存器如s0-s11，因为通过usertrapret()
// 返回用户空间的路径不会通过整个内核调用栈。
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // 内核页表
  /*   8 */ uint64 kernel_sp;     // 进程内核栈的顶部
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // 保存的用户程序计数器
  /*  32 */ uint64 kernel_hartid; // 保存的内核tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// 每个进程的状态
struct proc {
  struct spinlock lock;

  // 使用这些时必须持有p->lock：
  enum procstate state;        // 进程状态
  void *chan;                  // 非零时，表示在该通道上睡眠
  int killed;                  // 非零时，表示已被杀死
  int xstate;                  // 返回给父进程wait的退出状态
  int pid;                     // 进程ID

  // 使用这些时必须持有wait_lock：
  struct proc *parent;         // 父进程

  // 这些是进程私有的，因此不需要持有p->lock。
  uint64 kstack;               // 内核栈的虚拟地址
  uint64 sz;                   // 进程内存大小（字节）
  pagetable_t pagetable;       // 用户页表
  struct trapframe *trapframe; // trampoline.S的数据页
  struct context context;      // 切换到这里运行进程
  struct file *ofile[NOFILE];  // 打开的文件
  struct inode *cwd;           // 当前目录
  char name[16];               // 进程名称（调试用）
};
