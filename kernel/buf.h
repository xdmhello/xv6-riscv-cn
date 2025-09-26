struct buf {
  int valid;   // 是否已从磁盘读取数据？
  int disk;    // 磁盘是否"拥有"此缓冲区？
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
};

