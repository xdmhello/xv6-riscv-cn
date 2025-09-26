// Buffer cache.
//
// 缓冲区缓存是一个由buf结构体组成的链表，保存着磁盘块内容的缓存副本。
// 在内存中缓存磁盘块可以减少磁盘读取次数，同时为多个进程使用的磁盘块提供同步点。
//
// 接口：
// * 要获取特定磁盘块的缓冲区，请调用bread。
// * 更改缓冲区数据后，调用bwrite将其写入磁盘。
// * 使用完缓冲区后，调用brelse。
// * 调用brelse后不要再使用该缓冲区。
// * 同一时间只有一个进程可以使用缓冲区，
//   因此不要长时间持有缓冲区。


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;  // 保护缓存列表的自旋锁
  struct buf buf[NBUF];  // 缓冲区数组

  // 所有缓冲区组成的链表，通过prev/next指针连接。
  // 按照最近使用时间排序。
  // head.next是最近使用的，head.prev是最久未使用的。
  struct buf head;
} bcache;

// 初始化缓冲区缓存
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");  // 初始化缓存锁

  // 创建缓冲区链表
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");  // 初始化每个缓冲区的睡眠锁
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// 在缓冲区缓存中查找设备dev上的块。
// 如果未找到，则分配一个缓冲区。
// 无论哪种情况，都返回锁定的缓冲区。
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);  // 获取缓存锁

  // 该块是否已缓存？
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;  // 增加引用计数
      release(&bcache.lock);
      acquiresleep(&b->lock);  // 获取缓冲区的睡眠锁
      return b;
    }
  }

  // 未缓存。
  // 回收最久未使用(LRU)的未使用缓冲区。
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;  // 标记为无效，需要从磁盘读取
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);  // 获取缓冲区的睡眠锁
      return b;
    }
  }
  panic("bget: 没有可用缓冲区");
}

// 返回包含指定块内容的锁定缓冲区。
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);  // 获取或分配缓冲区
  if(!b->valid) {  // 如果缓冲区内容无效（需要从磁盘读取）
    virtio_disk_rw(b, 0);  // 从磁盘读取数据（0表示读）
    b->valid = 1;  // 标记为有效
  }
  return b;
}

// 将缓冲区内容写入磁盘。必须持有锁。
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");  // 调用者必须持有缓冲区的睡眠锁
  virtio_disk_rw(b, 1);  // 写入磁盘（1表示写）
}

// 释放锁定的缓冲区。
// 将其移到最近使用列表的头部。
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");  // 调用者必须持有缓冲区的睡眠锁

  releasesleep(&b->lock);  // 释放缓冲区的睡眠锁

  acquire(&bcache.lock);  // 获取缓存锁
  b->refcnt--;  // 减少引用计数
  if (b->refcnt == 0) {  // 如果没有进程引用该缓冲区
    // 无人在等待它
    // 从当前位置移除
    b->next->prev = b->prev;
    b->prev->next = b->next;
    // 插入到最近使用列表的头部
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);  // 释放缓存锁
}

// 固定缓冲区（增加引用计数，防止被回收）
void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

// 取消固定缓冲区（减少引用计数）
void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


