#include "kernel/types.h"
// umalloc.c - 用户空间内存分配器
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

// 基于Kernighan和Ritchie的内存分配器，
// 来自《C程序设计语言》第二版第8.7节。

// 用于内存对齐的类型
typedef long Align;

// 空闲块头部联合体
// 包含指向下一个空闲块的指针和块大小
// Align字段确保头部按long类型对齐
union header {
  struct {
    union header *ptr;  // 指向下一个空闲块
    uint size;          // 块的大小（以Header单元为单位）
  } s;
  Align x;              // 用于对齐
};

typedef union header Header;

// 空闲链表的哨兵节点
static Header base;
// 空闲链表的头指针
static Header *freep;

// 释放内存函数
// 参数：ap-要释放的内存块指针
void
free(void *ap)
{
  Header *bp, *p;

  // 指向内存块的头部（用户指针前一个位置）
  bp = (Header*)ap - 1;
  // 遍历空闲链表，寻找合适的位置插入释放的块
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    // 处理链表末尾（环形链表）的情况
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  // 如果释放的块与后一个空闲块相邻，合并它们
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  // 如果释放的块与前一个空闲块相邻，合并它们
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  // 更新空闲链表头指针
  freep = p;
}

// 向操作系统请求更多内存
// 参数：nu-需要的Header单元数量
// 返回：更新后的空闲链表头指针
static Header*
morecore(uint nu)
{
  char *p;
  Header *hp;

  // 确保至少分配4096个Header单元
  if(nu < 4096)
    nu = 4096;
  // 调用sbrk系统调用扩展进程内存空间
  p = sbrk(nu * sizeof(Header));
  if(p == SBRK_ERROR)
    return 0;  // 内存分配失败
  // 设置新分配内存块的头部
  hp = (Header*)p;
  hp->s.size = nu;
  // 将新分配的内存（除头部外）加入空闲链表
  free((void*)(hp + 1));
  return freep;
}

// 分配内存函数
// 参数：nbytes-需要分配的字节数
// 返回：指向分配内存的指针，如果失败返回NULL
void*
malloc(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;

  // 计算需要的Header单元数量（包括头部）
  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  // 初始化空闲链表（如果尚未初始化）
  if((prevp = freep) == 0){
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  // 遍历空闲链表寻找足够大的块
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= nunits){
      // 找到足够大的块
      if(p->s.size == nunits)
        // 如果块大小恰好合适，直接从链表中移除
        prevp->s.ptr = p->s.ptr;
      else {
        // 如果块大小大于需求，分割块
        p->s.size -= nunits;
        p += p->s.size;
        p->s.size = nunits;
      }
      // 更新空闲链表头指针
      freep = prevp;
      // 返回用户可用内存的指针（跳过头部）
      return (void*)(p + 1);
    }
    // 如果遍历完整个链表都没找到足够大的块，请求更多内存
    if(p == freep)
      if((p = morecore(nunits)) == 0)
        return 0;  // 内存分配失败
  }
}
