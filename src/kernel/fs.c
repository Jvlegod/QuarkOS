#include "fs.h"
#include "ktypes.h"
#include "kprintf.h"

struct fs_super {
    uint32_t magic;
    uint32_t blk_size;
    uint32_t total_blocks;
    uint32_t inode_map_block;    // = 2
    uint32_t data_map_block;     // = 3 .. 3+data_map_blocks-1（当前实现只用 1 块）
    uint32_t inode_table_start;  // = 4
    uint32_t inode_table_blocks; // = 32
    uint32_t data_start_block;   // = inode_table_start + inode_table_blocks
    uint32_t max_inodes;         // = 1024
    uint32_t root_ino;           // = 1
};
STATIC_ASSERT(sizeof(struct fs_super) <= 512, "super fits");

struct fs_inode {
    uint16_t type;       // enum fs_inode_type
    uint16_t links;      // 链接计数（目录/文件引用）
    uint32_t size;       // 字节数（目录则为目录项数*64 或者使用计数）
    uint32_t nblocks;    // 数据块数量
    uint32_t direct[10]; // 直接块号（绝对块号）
    uint32_t reserved[19];// 预留给将来的间接块等
} __attribute__((packed));

STATIC_ASSERT(sizeof(struct fs_inode) == FS_INODE_SIZE, "inode size 128");

struct fs_dirent {
    uint32_t ino;             // 0 = 空
    char     name[FS_NAME_MAX]; // 60 bytes, 不含 '\0' 也可，但我们会写 '\0'
} __attribute__((packed));
STATIC_ASSERT(sizeof(struct fs_dirent) == 64, "dirent 64B");

/* ===== 全局 ===== */
static struct fs_super g_sb;
static int g_mounted = 0;

static int fs_read_block(uint32_t blk_no, void* buf){
    return blk_read((uint64_t)blk_no * FS_SECTORS_PER_BLK, buf, FS_SECTORS_PER_BLK);
}
static int fs_write_block(uint32_t blk_no, const void* buf){
    return blk_write((uint64_t)blk_no * FS_SECTORS_PER_BLK, buf, FS_SECTORS_PER_BLK);
}

// ====== 位图工具（每个 bit 表示 1 项，0=空闲，1=已用）======
static int bitmap_test(const unsigned char* bm, unsigned idx){ return (bm[idx>>3] >> (idx&7)) & 1; }
static void bitmap_set (unsigned char* bm, unsigned idx){ bm[idx>>3] |=  (1u << (idx&7)); }
static void bitmap_clear(unsigned char* bm, unsigned idx){ bm[idx>>3] &= ~(1u << (idx&7)); }

// ====== inode I/O ======
static int inode_read(uint32_t ino, struct fs_inode* out){
    if(ino==0 || ino >= g_sb.max_inodes) return -1;
    uint32_t idx = ino; // 我们从 1..max-1 使用
    uint32_t blk = g_sb.inode_table_start + (idx / FS_INODES_PER_BLK);
    uint32_t off = (idx % FS_INODES_PER_BLK) * FS_INODE_SIZE;
    unsigned char buf[FS_BLOCK_SIZE];
    if(fs_read_block(blk, buf) != 0) return -1;
    memcpy(out, buf + off, sizeof(*out));
    return 0;
}
static int inode_write(uint32_t ino, const struct fs_inode* in){
    if(ino==0 || ino >= g_sb.max_inodes) return -1;
    uint32_t idx = ino;
    uint32_t blk = g_sb.inode_table_start + (idx / FS_INODES_PER_BLK);
    uint32_t off = (idx % FS_INODES_PER_BLK) * FS_INODE_SIZE;
    unsigned char buf[FS_BLOCK_SIZE];
    if(fs_read_block(blk, buf) != 0) return -1;
    memcpy(buf + off, in, sizeof(*in));
    if(fs_write_block(blk, buf) != 0) return -1;
    return 0;
}

// ====== 读取整个位图块 ======
static int read_inode_bitmap(unsigned char* bm){ return fs_read_block(g_sb.inode_map_block, bm); }
static int write_inode_bitmap(const unsigned char* bm){ return fs_write_block(g_sb.inode_map_block, bm); }
static int read_data_bitmap (unsigned char* bm){ return fs_read_block(g_sb.data_map_block, bm); }
static int write_data_bitmap(const unsigned char* bm){ return fs_write_block(g_sb.data_map_block, bm); }

// ====== 分配 inode / data block ======
static int alloc_inode(uint32_t* out_ino){
    unsigned char bm[FS_BLOCK_SIZE];
    if(read_inode_bitmap(bm)!=0) return -1;
    for(uint32_t i=1;i<g_sb.max_inodes;i++){ // 0 保留
        if(!bitmap_test(bm, i)){
            bitmap_set(bm, i);
            if(write_inode_bitmap(bm)!=0) return -1;
            *out_ino = i;
            return 0;
        }
    }
    return -1;
}
static int free_inode(uint32_t ino){
    unsigned char bm[FS_BLOCK_SIZE];
    if(read_inode_bitmap(bm)!=0) return -1;
    bitmap_clear(bm, ino);
    return write_inode_bitmap(bm);
}
static int alloc_dblock(uint32_t* out_blk){
    // 数据区块号范围：g_sb.data_start_block .. g_sb.total_blocks-1
    // data bitmap 的 bit 0 对应 data_start_block
    unsigned char bm[FS_BLOCK_SIZE];
    if(read_data_bitmap(bm)!=0) return -1;

    uint32_t max_data_blocks = (g_sb.total_blocks - g_sb.data_start_block);
    if(max_data_blocks > FS_BLOCK_SIZE*8) max_data_blocks = FS_BLOCK_SIZE*8; // 本实现只用一块位图

    for(uint32_t i=0;i<max_data_blocks;i++){
        if(!bitmap_test(bm, i)){
            bitmap_set(bm, i);
            if(write_data_bitmap(bm)!=0) return -1;
            *out_blk = g_sb.data_start_block + i;
            return 0;
        }
    }
    return -1;
}
static int free_dblock(uint32_t abs_blk){
    unsigned char bm[FS_BLOCK_SIZE];
    if(read_data_bitmap(bm)!=0) return -1;
    uint32_t i = abs_blk - g_sb.data_start_block;
    bitmap_clear(bm, i);
    return write_data_bitmap(bm);
}

// ====== 目录工具 ======
static int dir_find_entry(const struct fs_inode* dir, const char* name, struct fs_dirent* out, uint32_t* out_blk, uint32_t* out_idx){
    if(dir->type != FS_IT_DIR) return -1;
    unsigned char buf[FS_BLOCK_SIZE];

    for(uint32_t b=0; b<dir->nblocks && b<10; b++){
        if(dir->direct[b]==0) continue;
        if(fs_read_block(dir->direct[b], buf)!=0) return -1;

        struct fs_dirent* de = (struct fs_dirent*)buf;
        for(uint32_t i=0;i<FS_DIRENTS_PER_BLK;i++){
            if(de[i].ino != 0){
                if(strncmp(de[i].name, name, FS_NAME_MAX)==0){
                    if(out) *out = de[i];
                    if(out_blk) *out_blk = dir->direct[b];
                    if(out_idx) *out_idx = i;
                    return 0;
                }
            }
        }
    }
    return -1;
}
static int dir_add_entry(struct fs_inode* dir, uint32_t dir_ino, const char* name, uint32_t child_ino){
    unsigned char buf[FS_BLOCK_SIZE];

    // 先找空位
    for(uint32_t b=0; b<dir->nblocks && b<10; b++){
        if(dir->direct[b]==0) continue;
        if(fs_read_block(dir->direct[b], buf)!=0) return -1;
        struct fs_dirent* de = (struct fs_dirent*)buf;
        for(uint32_t i=0;i<FS_DIRENTS_PER_BLK;i++){
            if(de[i].ino == 0){
                de[i].ino = child_ino;
                memset(de[i].name, 0, FS_NAME_MAX);
                unsigned n = strlen(name);
                if(n >= FS_NAME_MAX) n = FS_NAME_MAX-1;
                memcpy(de[i].name, name, n);
                if(fs_write_block(dir->direct[b], buf)!=0) return -1;
                dir->size += 64;
                if(inode_write(dir_ino, dir)!=0) return -1;
                return 0;
            }
        }
    }

    // 没空位：分配新数据块
    if(dir->nblocks >= 10) return -1; // 太满了
    uint32_t newblk;
    if(alloc_dblock(&newblk)!=0) return -1;
    memset(buf, 0, sizeof(buf));
    struct fs_dirent* de = (struct fs_dirent*)buf;
    de[0].ino = child_ino;
    memset(de[0].name, 0, FS_NAME_MAX);
    unsigned n = strlen(name); if(n >= FS_NAME_MAX) n = FS_NAME_MAX-1;
    memcpy(de[0].name, name, n);
    if(fs_write_block(newblk, buf)!=0) { free_dblock(newblk); return -1; }

    dir->direct[dir->nblocks++] = newblk;
    dir->size += 64;
    return inode_write(dir_ino, dir);
}

// 分割路径 "/a/b" -> 依次查找
static int resolve_parent(const char* path, uint32_t* out_parent_ino, const char** out_basename){
    // 路径必须以 '/' 开头；根目录 "/" 的 parent 视为自己
    if(!path || path[0] != '/') return -1;

    // 跳过连续 '/'
    unsigned i=0; while(path[i]=='/') i++;
    if(path[i]==0){ // 根目录
        *out_parent_ino = 1; *out_basename = "/"; return 0;
    }

    // 找最后一个组件名（basename）
    int last_slash = -1;
    for(unsigned j=i; path[j]; j++){
        if(path[j]=='/') last_slash = (int)j;
    }
    const char* basename = (last_slash==-1)? path+i : path+last_slash+1;

    // 父目录前缀
    char temp[256];
    if((unsigned)strlen(path) >= sizeof(temp)) return -1;
    memset(temp, 0, sizeof(temp));
    memcpy(temp, path, last_slash==-1 ? i : (unsigned)(last_slash));

    // 从根遍历
    uint32_t cur = 1;
    struct fs_inode ino;
    if(inode_read(cur, &ino)!=0) return -1;

    unsigned p=1; // 已经跳过第一个 '/'
    while(temp[p]){
        // 取下一个组件
        while(temp[p]=='/') p++;
        if(!temp[p]) break;
        char name[FS_NAME_MAX]; unsigned k=0;
        while(temp[p] && temp[p]!='/'){
            if(k+1<FS_NAME_MAX) name[k++]=temp[p];
            p++;
        }
        name[k]=0;

        // 在 cur 目录中查找
        if(ino.type != FS_IT_DIR) return -1;
        struct fs_dirent de; uint32_t b,iidx;
        if(dir_find_entry(&ino, name, &de, &b, &iidx)!=0) return -1;
        cur = de.ino;
        if(inode_read(cur, &ino)!=0) return -1;
    }

    *out_parent_ino = cur;
    *out_basename = basename;
    return 0;
}

/* ===== mount / format ===== */
int fs_mount(void){
    unsigned char buf[FS_BLOCK_SIZE];
    if(fs_read_block(1, buf)!=0) return -1;
    memcpy(&g_sb, buf, sizeof(g_sb));
    if(g_sb.magic != FS_MAGIC || g_sb.blk_size != FS_BLOCK_SIZE){
        return -1;
    }
    g_mounted = 1;
    return 0;
}

int fs_format(uint64_t total_sectors){
    uint32_t total_blocks = (uint32_t)(total_sectors / FS_SECTORS_PER_BLK);
    if(total_blocks < 64) return -1; // 太小

    // superblock
    memset(&g_sb, 0, sizeof(g_sb));
    g_sb.magic = FS_MAGIC;
    g_sb.blk_size = FS_BLOCK_SIZE;
    g_sb.total_blocks = total_blocks;
    g_sb.inode_map_block = 2;
    g_sb.data_map_block  = 3;  // 当前实现只用 1 块数据位图
    g_sb.inode_table_start = 4;
    g_sb.inode_table_blocks = FS_INODE_TABLE_BLKS; // 32
    g_sb.data_start_block = g_sb.inode_table_start + g_sb.inode_table_blocks;
    g_sb.max_inodes = FS_MAX_INODES;
    g_sb.root_ino = 1;

    // 写 super
    unsigned char buf[FS_BLOCK_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, &g_sb, sizeof(g_sb));
    if(fs_write_block(1, buf)!=0) return -1;

    // 清空位图、inode 表、数据区起始若干块
    memset(buf, 0, sizeof(buf));
    if(fs_write_block(g_sb.inode_map_block, buf)!=0) return -1;
    if(fs_write_block(g_sb.data_map_block,  buf)!=0) return -1;
    for(uint32_t b=0;b<g_sb.inode_table_blocks;b++){
        if(fs_write_block(g_sb.inode_table_start + b, buf)!=0) return -1;
    }

    // 分配根目录 inode = 1
    // 标记 inode #1 占用
    unsigned char ib[FS_BLOCK_SIZE]; if(read_inode_bitmap(ib)!=0) return -1;
    bitmap_set(ib, 1); if(write_inode_bitmap(ib)!=0) return -1;

    struct fs_inode root;
    memset(&root, 0, sizeof(root));
    root.type = FS_IT_DIR;
    root.links = 1;
    root.size = 0;
    root.nblocks = 0;
    if(inode_write(1, &root)!=0) return -1;

    g_mounted = 1;
    return 0;
}

int fs_mount_or_mkfs(uint64_t total_sectors){
    if(fs_mount()==0) return 0;
    kprintf("[FS] no fs found, formatting...\r\n");
    if(fs_format(total_sectors)!=0) return -1;
    return fs_mount();
}

/* ===== 基本操作：mkdir / touch / ls ===== */

static int create_empty(const char* path, int as_dir){
    if(!g_mounted) return -1;

    uint32_t pino; const char* name;
    if(resolve_parent(path, &pino, &name)!=0) return -1;
    if(strcmp(name, "/")==0) return -1; // 不允许对根操作

    // 父目录必须存在
    struct fs_inode parent;
    if(inode_read(pino, &parent)!=0) return -1;
    if(parent.type != FS_IT_DIR) return -1;

    // 已存在则直接返回
    struct fs_dirent exist;
    if(dir_find_entry(&parent, name, &exist, 0,0)==0){
        struct fs_inode tmp;
        if(inode_read(exist.ino, &tmp)!=0) return -1;
        if(as_dir && tmp.type!=FS_IT_DIR) return -1;
        if(!as_dir && tmp.type!=FS_IT_FILE) return -1;
        return 0;
    }

    // 分配新 inode
    uint32_t cino;
    if(alloc_inode(&cino)!=0) return -1;

    struct fs_inode node;
    memset(&node, 0, sizeof(node));
    node.type = as_dir ? FS_IT_DIR : FS_IT_FILE;
    node.links = 1;
    node.size = 0;
    node.nblocks = 0;
    if(inode_write(cino, &node)!=0){ free_inode(cino); return -1; }

    // 加到父目录
    if(dir_add_entry(&parent, pino, name, cino)!=0){
        free_inode(cino); return -1;
    }
    return 0;
}

int fs_mkdir(const char* path){ return create_empty(path, /*as_dir=*/1); }
int fs_touch(const char* path){ return create_empty(path, /*as_dir=*/0); }

int fs_ls(const char* path){
    if(!g_mounted) return -1;

    // 解析到目标目录（或文件所在目录并打印那个文件）
    if(!path || path[0]==0) path = "/";

    // 根目录快捷
    uint32_t ino_id = 1;
    struct fs_inode ino;
    if(strcmp(path, "/")!=0){
        // 先找到父，再找 basename
        uint32_t pino; const char* name;
        if(resolve_parent(path, &pino, &name)!=0) return -1;
        struct fs_inode parent;
        if(inode_read(pino, &parent)!=0) return -1;

        struct fs_dirent de;
        if(dir_find_entry(&parent, name, &de, 0,0)!=0){
            kprintf("ls: not found: %s\r\n", path); return -1;
        }
        ino_id = de.ino;
    }

    if(inode_read(ino_id, &ino)!=0) return -1;

    if(ino.type == FS_IT_FILE){
        kprintf("%s\r\n", path);
        return 0;
    }

    // 目录：列出全部非 0 项
    unsigned char buf[FS_BLOCK_SIZE];
    for(uint32_t b=0;b<ino.nblocks && b<10;b++){
        if(ino.direct[b]==0) continue;
        if(fs_read_block(ino.direct[b], buf)!=0) return -1;
        struct fs_dirent* de = (struct fs_dirent*)buf;
        for(uint32_t i=0;i<FS_DIRENTS_PER_BLK;i++){
            if(de[i].ino){
                // 简单打印名字（已 '\0' 终止）
                kprintf("%s\r\n", de[i].name);
            }
        }
    }
    return 0;
}
