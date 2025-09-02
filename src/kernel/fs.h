#ifndef FS_H
#define FS_H

#include "ktypes.h"
#include "virtio.h"
#include "cstdlib.h"
#include "virtio.h"
#include "user.h"

// block 0         : 未用/保留
// block 1         : 超级块(superblock)
// block 2         : inode 位图(1 bit 表示一个 inode 占用)
// block 3         : 数据块位图(1 bit 表示一个数据块占用)
// block 4..(4+N-1): inode 表(N = FS_INODE_TABLE_BLKS，注释里是 32)
// block data_start: 数据区起始(= 4 + inode_table_blocks)
// ...
// block total-1   : 数据区结束


#define FS_BLOCK_SIZE     4096u
#define FS_SECTOR_SIZE     512u
#define FS_SECTORS_PER_BLK (FS_BLOCK_SIZE / FS_SECTOR_SIZE)

#define FS_MAX_INODES     1024u
#define FS_INODE_SIZE      128u
#define FS_INODES_PER_BLK  (FS_BLOCK_SIZE / FS_INODE_SIZE)
#define FS_INODE_TABLE_BLKS (FS_MAX_INODES / FS_INODES_PER_BLK) // 32

#define FS_NAME_MAX         60u
#define FS_DIRENTS_PER_BLK  (FS_BLOCK_SIZE / 64u)

#define FS_MAGIC 0x31534651u

#define FS_PATH_MAX 256

enum fs_inode_type { FS_IT_NONE = 0, FS_IT_FILE = 1, FS_IT_DIR = 2 };

#define FS_O_READ  1
#define FS_O_WRITE 2

struct fs_inode {
    uint16_t type;        // enum fs_inode_type
    uint16_t links;       // 链接计数
    uid_t    owner;       // 文件所有者
    uint16_t mode;        // 权限位
    uint16_t pad;         // 对齐填充
    uint32_t size;        // 字节数
    uint32_t nblocks;     // 数据块数量
    uint32_t direct[10];  // 直接块
    uint32_t reserved[17];// 预留
} __attribute__((packed));

STATIC_ASSERT(sizeof(struct fs_inode) == FS_INODE_SIZE, "inode size 128");

int fs_mount(void);

int fs_format(uint64_t total_sectors);

int fs_mount_or_mkfs(uint64_t total_sectors);

int fs_ls(const char* path);
int fs_mkdir(const char* path);
int fs_touch(const char* path);
int fs_create(const char* path, uint16_t mode);
int fs_chdir(const char* path);
int fs_stat(const char* path, uid_t* owner, uint16_t* mode);
int fs_read_all(const char* path, void* buf, unsigned cap);
int fs_write_all(const char* path, const void* data, unsigned n);
const char* fs_get_cwd(void);
int fs_rm(const char* path);
int fs_rmdir(const char* path);
int fs_typeof(const char* path, uint16_t* out_type);
int fs_is_file(const char* path);
int fs_is_dir(const char* path);
int fs_open(const char* path, int flags);

#endif /* FS_H */
