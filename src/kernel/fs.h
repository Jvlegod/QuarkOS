#ifndef FS_H
#define FS_H

#include "ktypes.h"
#include "virtio.h"
#include "cstdlib.h"
#include "virtio.h"

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

int fs_mount(void);

int fs_format(uint64_t total_sectors);

int fs_mount_or_mkfs(uint64_t total_sectors);

int fs_ls(const char* path);
int fs_mkdir(const char* path);
int fs_touch(const char* path);
int fs_chdir(const char* path);
const char* fs_get_cwd(void);

#endif /* FS_H */
