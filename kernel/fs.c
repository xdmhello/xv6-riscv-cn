// 文件系统实现。五层架构：
//   + 块：原始磁盘块分配器。
//   + 日志：用于多步更新的崩溃恢复。
//   + 文件：inode分配器，读，写，元数据。
//   + 目录：具有特殊内容的inode（其他inode的列表！）
//   + 名称：如/usr/rtm/xv6/fs.c这样的路径，方便命名。
//
// 此文件包含底层文件系统操作例程。
// （更高级别的）系统调用实现位于sysfile.c中。

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
// 每个磁盘设备应该有一个超级块，但我们只运行一个设备
struct superblock sb; 

// 读取超级块。
static void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;

  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  brelse(bp);
}

// 初始化文件系统
void
fsinit(int dev) {
  readsb(dev, &sb);
  if(sb.magic != FSMAGIC)
    panic("invalid file system");
  initlog(dev, &sb);
  ireclaim(dev);
}

// 将块清零。
static void
bzero(int dev, int bno)
{
  struct buf *bp;

  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}

// 块管理

// 分配一个已清零的磁盘块。
// 如果磁盘空间不足，返回0。
static uint
balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  for(b = 0; b < sb.size; b += BPB){
    bp = bread(dev, BBLOCK(b, sb));
    for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
      m = 1 << (bi % 8);
      if((bp->data[bi/8] & m) == 0){  // Is block free?
        bp->data[bi/8] |= m;  // Mark block in use.
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    brelse(bp);
  }
  printf("balloc: out of blocks\n");
  return 0;
}

// 释放磁盘块。
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;

  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");
  bp->data[bi/8] &= ~m;
  log_write(bp);
  brelse(bp);
}

// Inode管理
//
// 一个inode描述了一个无名称的文件。
// inode磁盘结构包含元数据：文件类型、
// 大小、引用该文件的链接数，以及
// 保存文件内容的块列表。
//
// inode在磁盘上按顺序排列在块sb.inodestart处。
// 每个inode都有一个编号，表示它在磁盘上的位置。
//
// 内核在内存中维护一个正在使用的inode表，
// 为多个进程使用的inode提供同步访问的地方。
// 内存中的inode包含不存储在磁盘上的记账信息：
// ip->ref和ip->valid。
//
// inode及其内存表示在被文件系统其他代码使用前，
// 会经历一系列状态。
//
// * 分配：如果inode的类型（在磁盘上）非零，则表示已分配。
//   ialloc()负责分配，如果引用计数和链接计数都降至零，
//   iput()会释放inode。
//
// * 表中的引用：如果ip->ref为零，则inode表中的条目是空闲的。
//   否则ip->ref跟踪内存中指向该条目的指针数量（打开的文件
//   和当前目录）。iget()查找或创建表条目并增加其ref；
//   iput()减少ref。
//
// * 有效：inode表条目中的信息（类型、大小等）只有在ip->valid为1时才正确。
//   ilock()从磁盘读取inode并设置ip->valid，而iput()在ip->ref降至零时
//   清除ip->valid。
//
// * 锁定：文件系统代码只有先锁定inode，才能检查和修改
//   inode中的信息及其内容。
//
// 因此，典型的使用序列是：
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... 检查和修改ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock()与iget()分离，这样系统调用可以获得对inode的长期引用
//（如打开的文件），而只在短时间内锁定它（例如，在read()中）。
// 这种分离还有助于避免在路径名查找期间出现死锁和竞争条件。
// iget()增加ip->ref，使inode保持在表中，并且指向它的指针保持有效。
//
// 许多内部文件系统函数期望调用者已经锁定了相关的inode；
// 这允许调用者创建多步原子操作。
//
// itable.lock自旋锁保护itable条目的分配。由于ip->ref表示条目是否空闲，
// 而ip->dev和ip->inum表示条目持有哪个i-node，
// 在使用这些字段时必须持有itable.lock。
//
// ip->lock睡眠锁保护除ref、dev和inum之外的所有ip->字段。
// 必须持有ip->lock才能读取或写入该inode的ip->valid、ip->size、ip->type等。

struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} itable;

void
iinit()
{
  int i = 0;
  
  initlock(&itable.lock, "itable");
  for(i = 0; i < NINODE; i++) {
    initsleeplock(&itable.inode[i].lock, "inode");
  }
}

static struct inode* iget(uint dev, uint inum);

// 在设备dev上分配一个inode。
// 通过设置其类型来标记它为已分配。
// 返回一个未锁定但已分配和引用的inode，
// 如果没有空闲inode，则返回NULL。
struct inode*
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  for(inum = 1; inum < sb.ninodes; inum++){
    bp = bread(dev, IBLOCK(inum, sb));
    dip = (struct dinode*)bp->data + inum%IPB;
    if(dip->type == 0){  // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp);   // 在磁盘上标记为已分配
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  printf("ialloc: no inodes\n");
  return 0;
}

// 将修改后的内存中的inode复制到磁盘。
// 每次修改ip->xxx字段（存在于磁盘上的）后，必须调用此函数。
// 调用者必须持有ip->lock。
void
iupdate(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  dip = (struct dinode*)bp->data + ip->inum%IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  log_write(bp);
  brelse(bp);
}

// 查找设备dev上编号为inum的inode，
// 并返回内存中的副本。不锁定inode，也不从磁盘读取它。
static struct inode*
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  acquire(&itable.lock);

  // 检查inode是否已经在表中？
  empty = 0;
  for(ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      release(&itable.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // 记住空槽位
      empty = ip;
  }

  // 回收一个inode条目
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release(&itable.lock);

  return ip;
}

// 增加ip的引用计数。
// 返回ip以支持ip = idup(ip1)的惯用法。
struct inode*
idup(struct inode *ip)
{
  acquire(&itable.lock);
  ip->ref++;
  release(&itable.lock);
  return ip;
}

// 锁定给定的inode。
// 必要时从磁盘读取inode。
void
ilock(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  if(ip == 0 || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  if(ip->valid == 0){
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
    if(ip->type == 0)
      panic("ilock: no type");
  }
}

// 解锁给定的inode。
void
iunlock(struct inode *ip)
{
  if(ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

// 减少对内存中inode的引用。
// 如果是最后一个引用，inode表条目可以被回收。
// 如果是最后一个引用且inode没有链接指向它，释放磁盘上的inode（及其内容）。
// 所有对iput()的调用必须在事务内，以防它必须释放inode。
void
iput(struct inode *ip)
{
  acquire(&itable.lock);

  if(ip->ref == 1 && ip->valid && ip->nlink == 0){
    // inode has no links and no other references: truncate and free.

    // ip->ref == 1意味着没有其他进程可以持有ip的锁，
    // 所以这个acquiresleep()不会阻塞（或死锁）。
    acquiresleep(&ip->lock);

    release(&itable.lock);

    itrunc(ip);
    ip->type = 0;
    iupdate(ip);
    ip->valid = 0;

    releasesleep(&ip->lock);

    acquire(&itable.lock);
  }

  ip->ref--;
  release(&itable.lock);
}

// 常见惯用法：解锁，然后释放。
void
iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}

void
ireclaim(int dev)
{
  for (int inum = 1; inum < sb.ninodes; inum++) {
    struct inode *ip = 0;
    struct buf *bp = bread(dev, IBLOCK(inum, sb));
    struct dinode *dip = (struct dinode *)bp->data + inum % IPB;
    if (dip->type != 0 && dip->nlink == 0) {  // is an orphaned inode
      printf("ireclaim: orphaned inode %d\n", inum);
      ip = iget(dev, inum);
    }
    brelse(bp);
    if (ip) {
      begin_op();
      ilock(ip);
      iunlock(ip);
      iput(ip);
      end_op();
    }
  }
}

// Inode内容
//
// 与每个inode关联的内容（数据）存储在磁盘上的块中。
// 前NDIRECT个块编号列在ip->addrs[]中。
// 接下来的NINDIRECT个块列在块ip->addrs[NDIRECT]中。

// 返回inode ip中第n个块的磁盘块地址。
// 如果没有这样的块，bmap会分配一个。
// 如果磁盘空间不足，返回0。
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0){
      addr = balloc(ip->dev);
      if(addr == 0)
        return 0;
      ip->addrs[bn] = addr;
    }
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // 加载间接块，必要时进行分配
    if((addr = ip->addrs[NDIRECT]) == 0){
      addr = balloc(ip->dev);
      if(addr == 0)
        return 0;
      ip->addrs[NDIRECT] = addr;
    }
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      addr = balloc(ip->dev);
      if(addr){
        a[bn] = addr;
        log_write(bp);
      }
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

// 截断inode（丢弃内容）。
// 调用者必须持有ip->lock。
void
itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  ip->size = 0;
  iupdate(ip);
}

// 从inode复制状态信息。
// 调用者必须持有ip->lock。
void
stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

// 从inode读取数据。
// 调用者必须持有ip->lock。
// 如果user_dst==1，则dst是用户虚拟地址；
// 否则，dst是内核地址。
int
readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(off > ip->size || off + n < off)
    return 0;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    uint addr = bmap(ip, off/BSIZE);
    if(addr == 0)
      break;
    bp = bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off%BSIZE);
    if(either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
      brelse(bp);
      tot = -1;
      break;
    }
    brelse(bp);
  }
  return tot;
}

// 将数据写入inode。
// 调用者必须持有ip->lock。
// 如果user_src==1，则src是用户虚拟地址；
// 否则，src是内核地址。
// 返回成功写入的字节数。
// 如果返回值小于请求的n，说明出现了某种错误。
int
writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  if(off > ip->size || off + n < off)
    return -1;
  if(off + n > MAXFILE*BSIZE)
    return -1;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    uint addr = bmap(ip, off/BSIZE);
    if(addr == 0)
      break;
    bp = bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off%BSIZE);
    if(either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
      brelse(bp);
      break;
    }
    log_write(bp);
    brelse(bp);
  }

  if(off > ip->size)
    ip->size = off;

  // 即使大小没有改变，也要将inode写回磁盘
  // 因为上面的循环可能调用了bmap()并向ip->addrs[]添加了新块。
  iupdate(ip);

  return tot;
}

// 目录管理

int
namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

// 在目录中查找目录项。
// 如果找到，将*poff设置为条目的字节偏移量。
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    if(de.inum == 0)
      continue;
    if(namecmp(name, de.name) == 0){
      // entry matches path element
      if(poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

// 将新的目录项（name, inum）写入目录dp。
// 成功返回0，失败返回-1（例如，磁盘块用完）。
int
dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // 检查名称是否已存在
  if((ip = dirlookup(dp, name, 0)) != 0){
    iput(ip);
    return -1;
  }

  // 查找一个空的目录项
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    return -1;

  return 0;
}

// 路径管理

// 将路径中的下一个路径元素复制到name中。
// 返回一个指向复制后的下一个元素的指针。
// 返回的路径没有前导斜杠，
// 因此调用者可以检查*path=='\0'来确定name是否是最后一个元素。
// 如果没有要移除的名称，返回0。
//
// 示例：
//   skipelem("a/bb/c", name) = "bb/c"，设置name = "a"
//   skipelem("///a//bb", name) = "bb"，设置name = "a"
//   skipelem("a", name) = ""，设置name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while(*path == '/')
    path++;
  return path;
}

// 查找并返回路径名对应的inode。
// 如果parent != 0，返回父目录的inode，并将最后的
// 路径元素复制到name中，name必须有DIRSIZ字节的空间。
// 由于它调用iput()，因此必须在事务内调用。
static struct inode*
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  if(*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(myproc()->cwd);

  while((path = skipelem(path, name)) != 0){
    ilock(ip);
    if(ip->type != T_DIR){
      iunlockput(ip);
      return 0;
    }
    if(nameiparent && *path == '\0'){
      // 提前一级停止
      iunlock(ip);
      return ip;
    }
    if((next = dirlookup(ip, name, 0)) == 0){
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if(nameiparent){
    iput(ip);
    return 0;
  }
  return ip;
}

struct inode*
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

struct inode*
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}
