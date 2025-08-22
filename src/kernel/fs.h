#ifndef FS_H
#define FS_H

#include "ktypes.h"
#include "virtio.h"
#include "cstdlib.h"
#include "virtio.h"

// ==== 配置 ====
#define FS_BLOCK_SIZE     4096u
#define FS_SECTOR_SIZE     512u
#define FS_SECTORS_PER_BLK (FS_BLOCK_SIZE / FS_SECTOR_SIZE)

#define FS_MAX_INODES     1024u
#define FS_INODE_SIZE      128u
#define FS_INODES_PER_BLK  (FS_BLOCK_SIZE / FS_INODE_SIZE)
#define FS_INODE_TABLE_BLKS (FS_MAX_INODES / FS_INODES_PER_BLK) // 32

#define FS_NAME_MAX         60u    // 目录项名字最大长度（不含 '\0'）
#define FS_DIRENTS_PER_BLK  (FS_BLOCK_SIZE / 64u)

#define FS_MAGIC 0x31534651u /* "FS1" little endian */

enum fs_inode_type { FS_IT_NONE = 0, FS_IT_FILE = 1, FS_IT_DIR = 2 };

// ==== 对外 API ====
// total_sectors: 整个磁盘的 512B 扇区数（从 virtio capacity 拿）
// mount: 读取 superblock；若魔数不匹配返回 <0
int  fs_mount(void);
// format: 重新格式化为 FS（会破坏内容）；需要 total_sectors
int  fs_format(uint64_t total_sectors);
// 如果没格式化则自动 mkfs 再 mount
int  fs_mount_or_mkfs(uint64_t total_sectors);

// 基础操作（路径从根开始，“/”、“/a”、“/a/b”）：
int  fs_ls(const char* path);          // 打印目录项
int  fs_mkdir(const char* path);       // 创建目录
int  fs_touch(const char* path);       // 创建空文件（已存在则返回 0）

#endif /* FS_H */
