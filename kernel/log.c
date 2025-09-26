#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// 支持并发文件系统系统调用的简单日志机制。
//
// 一个日志事务包含多个文件系统系统调用的更新。日志系统只在
// 没有活跃的文件系统系统调用时才提交。因此，无需考虑
// 提交是否可能将未提交的系统调用的更新写入磁盘。
//
// 系统调用应该调用begin_op()/end_op()来标记其开始和结束。
// 通常begin_op()只是增加进行中的文件系统系统调用计数并返回。
// 但如果它认为日志空间即将耗尽，它会休眠直到最后一个未完成的
// end_op()提交。
//
// 日志是包含磁盘块的物理重做日志。磁盘上的日志格式：
//   头部块，包含块A、B、C等的块号
//   块A
//   块B
//   块C
//   ...
// 日志追加是同步的。

// 头部块的内容，既用于磁盘上的头部块，也用于在提交前
// 在内存中跟踪已记录的块号。
struct logheader {
  int n;
  int block[LOGBLOCKS];
};

struct log {
  struct spinlock lock;
  int start;
  int outstanding; // 有多少个文件系统系统调用正在执行
  int committing;  // 在commit()中，请等待
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void
initlog(int dev, struct superblock *sb)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.dev = dev;
  recover_from_log();
}

// 将已提交的块从日志复制到它们的主位置
static void
install_trans(int recovering)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    if(recovering) {
      printf("recovering tail %d dst %d\n", tail, log.lh.block[tail]);
    }
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst
    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
    bwrite(dbuf);  // write dst to disk
    if(recovering == 0)
      bunpin(dbuf);
    brelse(lbuf);
    brelse(dbuf);
  }
}

// 从磁盘读取日志头部到内存中的日志头部
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// 将内存中的日志头部写入磁盘。
// 这是当前事务真正提交的时刻。
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void
recover_from_log(void)
{
  read_head();
  install_trans(1); // if committed, copy from log to disk
  log.lh.n = 0;
  write_head(); // clear the log
}

// 在每个文件系统系统调用开始时调用。
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){
      sleep(&log, &log.lock);
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGBLOCKS){
      // 此操作可能会耗尽日志空间；等待提交
      sleep(&log, &log.lock);
    } else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// 在每个文件系统系统调用结束时调用。
// 如果这是最后一个未完成的操作，则提交。
void
end_op(void)
{
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;
  if(log.committing)
    panic("log.committing");
  if(log.outstanding == 0){
    do_commit = 1;
    log.committing = 1;
  } else {
    // begin_op()可能正在等待日志空间，
    // 减少log.outstanding已减少了保留空间的数量。
    wakeup(&log);
  }
  release(&log.lock);

  if(do_commit){
    // 在不持有锁的情况下调用commit，因为不允许
    // 持有锁时睡眠
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// 将修改过的块从缓存复制到日志。
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1); // 日志块
    struct buf *from = bread(log.dev, log.lh.block[tail]); // 缓存块
    memmove(to->data, from->data, BSIZE);
    bwrite(to);  // 写入日志
    brelse(from);
    brelse(to);
  }
}

static void
commit()
{
  if (log.lh.n > 0) {
    write_log();     // 将修改过的块从缓存写入日志
    write_head();    // 将头部写入磁盘——真正的提交
    install_trans(0); // 现在将写入安装到最终位置
    log.lh.n = 0;
    write_head();    // 从日志中擦除事务
  }
}

// 调用者已修改b->data并已完成对缓冲区的操作。
// 记录块号并通过增加引用计数将其固定在缓存中。
// commit()/write_log()将执行磁盘写入。
//
// log_write()替代bwrite()；典型用法为：
//   bp = bread(...)
//   修改bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  acquire(&log.lock);
  if (log.lh.n >= LOGBLOCKS)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // 日志吸收（已存在于日志中）
      break;
  }
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n) {  // 需要将新块添加到日志吗？
    bpin(b);
    log.lh.n++;
  }
  release(&log.lock);
}

