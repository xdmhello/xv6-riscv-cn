// 磁盘文件系统格式。
// 内核和用户程序都使用此头文件。


#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// 磁盘布局：
// [ 引导块 | 超级块 | 日志 | inode块 |
//                                          空闲位图 | 数据块]
//
// mkfs计算超级块并构建初始文件系统。
// 超级块描述磁盘布局：
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x10203040

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// 磁盘上的inode结构
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+1];   // Data block addresses
};

// 每个块中的inode数量。
#define IPB           (BSIZE / sizeof(struct dinode))

// 包含inode i的块
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// 每个块的位图位数
#define BPB           (BSIZE*8)

// 包含块b位图位的空闲映射块
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// 目录是包含一系列dirent结构的文件。
#define DIRSIZ 14

// 名称字段可能有DIRSIZ个字符，不以NUL结尾
// 字符。
struct dirent {
  ushort inum;
  char name[DIRSIZ] __attribute__((nonstring));
};

