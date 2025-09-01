#include "fs.h"
#include "virtio_blk.h"
#include "ktypes.h"
#include "kprintf.h"
#include "task.h"

static char g_cwd[FS_PATH_MAX] = "/";

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
    unsigned char bm[FS_BLOCK_SIZE];
    if(read_data_bitmap(bm)!=0) return -1;

    uint32_t max_data_blocks = (g_sb.total_blocks - g_sb.data_start_block);
    if(max_data_blocks > FS_BLOCK_SIZE*8) max_data_blocks = FS_BLOCK_SIZE * 8;

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

static int inode_has_perm(const struct fs_inode* ino, int write){
    uid_t uid = task_get_current_uid();
    if(uid == 0) return 1; // root
    uint16_t mode = ino->mode;
    uint16_t bits = (uid == ino->owner) ? (mode >> 6) & 7 : mode & 7;
    return write ? (bits & 2) : (bits & 4);
}

static int lookup_file_abs(const char* abs_path, uint32_t* out_ino_id, struct fs_inode* out_ino);

static void fs_to_abs(const char* path, char out[FS_PATH_MAX]) {
    char tmp[FS_PATH_MAX];

    if (!path || !path[0]) {
        strcpy(out, g_cwd);
        return;
    }
    if (path[0] == '/') {
        strcpy(tmp, path);
    } else {
        strcpy(tmp, g_cwd);
        size_t n = strlen(tmp);
        if (n == 0) { tmp[0] = '/'; tmp[1] = '\0'; n = 1; }
        if (tmp[n-1] != '/') strcat(tmp, "/");
        strcat(tmp, path);
    }

    int i = 0, j = 0;

    out[j++] = '/';
    while (tmp[i] == '/') i++;

    while (tmp[i]) {
        char seg[FS_NAME_MAX];
        int k = 0;

        while (tmp[i] && tmp[i] != '/' && k < (int)FS_NAME_MAX - 1) {
            seg[k++] = tmp[i++];
        }
        seg[k] = '\0';
        while (tmp[i] == '/') i++;

        if (k == 0 || (k == 1 && seg[0] == '.')) {
            continue;
        }
        if (k == 2 && seg[0] == '.' && seg[1] == '.') {
            if (j > 1) {
                j--; while (j > 0 && out[j] != '/') j--;
                j = (j == 0) ? 1 : j + 1;
            }
            continue;
        }

        for (int t = 0; t < k && j < (int)FS_PATH_MAX - 1; ++t) {
            out[j++] = seg[t];
        }

        if (tmp[i] && j < (int)FS_PATH_MAX - 1) out[j++] = '/';
    }

    if (j > 1 && out[j-1] == '/') j--;
    out[j] = '\0';
}

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

    if(dir->nblocks >= 10) return -1;
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

static int resolve_parent(const char* path, uint32_t* out_parent_ino, const char** out_basename){
    if(!path || path[0] != '/') return -1;

    unsigned i=0; while(path[i]=='/') i++;
    if(path[i]==0){
        *out_parent_ino = 1; *out_basename = "/"; return 0;
    }

    int last_slash = -1;
    for(unsigned j=i; path[j]; j++){
        if(path[j]=='/') last_slash = (int)j;
    }
    const char* basename = (last_slash==-1)? path+i : path+last_slash+1;

    char temp[256];
    if((unsigned)strlen(path) >= sizeof(temp)) return -1;
    memset(temp, 0, sizeof(temp));
    memcpy(temp, path, last_slash==-1 ? i : (unsigned)(last_slash));

    uint32_t cur = 1;
    struct fs_inode ino;
    if(inode_read(cur, &ino)!=0) return -1;

    unsigned p=1;
    while(temp[p]){
        while(temp[p]=='/') p++;
        if(!temp[p]) break;
        char name[FS_NAME_MAX]; unsigned k=0;
        while(temp[p] && temp[p]!='/'){
            if(k+1<FS_NAME_MAX) name[k++]=temp[p];
            p++;
        }
        name[k]=0;

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
    if(total_blocks < 64) return -1;

    // superblock
    memset(&g_sb, 0, sizeof(g_sb));
    g_sb.magic = FS_MAGIC;
    g_sb.blk_size = FS_BLOCK_SIZE;
    g_sb.total_blocks = total_blocks;
    g_sb.inode_map_block = 2;
    g_sb.data_map_block  = 3;
    g_sb.inode_table_start = 4;
    g_sb.inode_table_blocks = FS_INODE_TABLE_BLKS; // 32
    g_sb.data_start_block = g_sb.inode_table_start + g_sb.inode_table_blocks;
    g_sb.max_inodes = FS_MAX_INODES;
    g_sb.root_ino = 1;

    unsigned char buf[FS_BLOCK_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, &g_sb, sizeof(g_sb));
    if(fs_write_block(1, buf)!=0) return -1;

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
    root.owner = 0;
    root.mode  = 0755;
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

static int create_empty(const char* path, int as_dir){
    if(!g_mounted) return -1;

    uint32_t pino; const char* name;
    if(resolve_parent(path, &pino, &name)!=0) return -1;
    if(strcmp(name, "/")==0) return -1;

    struct fs_inode parent;
    if(inode_read(pino, &parent)!=0) return -1;
    if(parent.type != FS_IT_DIR) return -1;

    struct fs_dirent exist;
    if(dir_find_entry(&parent, name, &exist, 0,0)==0){
        struct fs_inode tmp;
        if(inode_read(exist.ino, &tmp)!=0) return -1;
        if(as_dir && tmp.type!=FS_IT_DIR) return -1;
        if(!as_dir && tmp.type!=FS_IT_FILE) return -1;
        return 0;
    }

    uint32_t cino;
    if(alloc_inode(&cino)!=0) return -1;

    struct fs_inode node;
    memset(&node, 0, sizeof(node));
    node.type = as_dir ? FS_IT_DIR : FS_IT_FILE;
    node.links = 1;
    node.size = 0;
    node.nblocks = 0;
    node.owner = task_get_current_uid();
    node.mode  = as_dir ? 0755 : 0644;
    if(inode_write(cino, &node)!=0){ free_inode(cino); return -1; }

    if(dir_add_entry(&parent, pino, name, cino)!=0){
        free_inode(cino); return -1;
    }
    return 0;
}

static int create_empty_abs(const char* abs_path, int as_dir){
    if(!g_mounted) return -1;

    uint32_t pino; const char* name;
    if(resolve_parent(abs_path, &pino, &name)!=0) return -1;
    if(strcmp(name, "/")==0) return -1;

    struct fs_inode parent;
    if(inode_read(pino, &parent)!=0) return -1;
    if(parent.type != FS_IT_DIR) return -1;

    struct fs_dirent exist;
    if(dir_find_entry(&parent, name, &exist, 0,0)==0){
        struct fs_inode tmp;
        if(inode_read(exist.ino, &tmp)!=0) return -1;
        if(as_dir && tmp.type!=FS_IT_DIR) return -1;
        if(!as_dir && tmp.type!=FS_IT_FILE) return -1;
        return 0;
    }

    uint32_t cino;
    if(alloc_inode(&cino)!=0) return -1;

    struct fs_inode node;
    memset(&node, 0, sizeof(node));
    node.type = as_dir ? FS_IT_DIR : FS_IT_FILE;
    node.links = 1;
    node.size = 0;
    node.nblocks = 0;
    node.owner = task_get_current_uid();
    node.mode  = as_dir ? 0755 : 0644;
    if(inode_write(cino, &node)!=0){ free_inode(cino); return -1; }

    if(dir_add_entry(&parent, pino, name, cino)!=0){
        free_inode(cino); return -1;
    }
    return 0;
}

int fs_mkdir(const char* path){
    char abs[FS_PATH_MAX]; fs_to_abs(path, abs);
    return create_empty_abs(abs, /*as_dir=*/1);
}

int fs_create(const char* path, uint16_t mode){
    char abs[FS_PATH_MAX]; fs_to_abs(path, abs);
    if(create_empty_abs(abs, /*as_dir=*/0)!=0) return -1;
    struct fs_inode ino; uint32_t ino_id;
    if(lookup_file_abs(abs, &ino_id, &ino)!=0) return -1;
    ino.mode = mode;
    ino.owner = task_get_current_uid();
    return inode_write(ino_id, &ino);
}

int fs_touch(const char* path){
    return fs_create(path, 0644);
}

int fs_open(const char* path, int flags){
    if(!g_mounted) return -1;
    char abs[FS_PATH_MAX]; fs_to_abs(path, abs);
    struct fs_inode ino; uint32_t ino_id;
    if(lookup_file_abs(abs, &ino_id, &ino)!=0) return -1;
    if((flags & FS_O_WRITE) && !inode_has_perm(&ino,1)) return -1;
    if((flags & FS_O_READ) && !inode_has_perm(&ino,0)) return -1;
    return 0;
}


int fs_ls(const char* path){
    char abs[FS_PATH_MAX];
    if (!path || !path[0]) path = g_cwd;
    fs_to_abs(path, abs);

    if(!g_mounted) return -1;

    uint32_t ino_id = 1;
    struct fs_inode ino;

    if(strcmp(abs, "/")!=0){
        uint32_t pino; const char* name;
        if(resolve_parent(abs, &pino, &name)!=0) return -1;
        struct fs_inode parent;
        if(inode_read(pino, &parent)!=0) return -1;

        struct fs_dirent de;
        if(dir_find_entry(&parent, name, &de, 0,0)!=0){
            kprintf("ls: not found: %s\r\n", abs); return -1;
        }
        ino_id = de.ino;
    }

    if(inode_read(ino_id, &ino)!=0) return -1;

    if(ino.type == FS_IT_FILE){
        kprintf("%s\r\n", abs);
        return 0;
    }

    static unsigned char buf[FS_BLOCK_SIZE];
    for(uint32_t b=0;b<ino.nblocks && b<10;b++){
        if(ino.direct[b]==0) continue;
        if(fs_read_block(ino.direct[b], buf)!=0) return -1;
        struct fs_dirent* de = (struct fs_dirent*)buf;
        for(uint32_t i=0;i<FS_DIRENTS_PER_BLK;i++){
            if(de[i].ino) kprintf("%s\r\n", de[i].name);
        }
    }
    return 0;
}

int fs_chdir(const char* path){
    char abs[FS_PATH_MAX]; fs_to_abs(path, abs);

    uint32_t ino_id = 1;
    if(strcmp(abs, "/") != 0){
        uint32_t pino; const char* name;
        if(resolve_parent(abs, &pino, &name)!=0) return -1;
        struct fs_inode parent;
        if(inode_read(pino, &parent)!=0) return -1;

        struct fs_dirent de;
        if(dir_find_entry(&parent, name, &de, 0,0)!=0) return -1;
        ino_id = de.ino;
    }

    struct fs_inode ino;
    if(inode_read(ino_id, &ino)!=0) return -1;
    if(ino.type != FS_IT_DIR) return -1;

    strcpy(g_cwd, abs);
    return 0;
}

const char* fs_get_cwd(void){
    return g_cwd;
}

static int lookup_file_abs(const char* abs_path, uint32_t* out_ino_id, struct fs_inode* out_ino){
    if (strcmp(abs_path, "/") == 0) return -1;
    uint32_t pino; const char* name;
    if (resolve_parent(abs_path, &pino, &name) != 0) return -1;
    struct fs_inode parent;
    if (inode_read(pino, &parent) != 0) return -1;
    struct fs_dirent de;
    if (dir_find_entry(&parent, name, &de, 0, 0) != 0) return -1;
    if (inode_read(de.ino, out_ino) != 0) return -1;
    if (out_ino->type != FS_IT_FILE) return -1;
    if (out_ino_id) *out_ino_id = de.ino;
    return 0;
}

int fs_read_all(const char* path, void* buf, unsigned cap) {
    if (!g_mounted) return -1;
    char abs[FS_PATH_MAX]; fs_to_abs(path, abs);
    if (cap == 0) return 0;

    struct fs_inode ino;
    uint32_t ino_id;
    if (lookup_file_abs(abs, &ino_id, &ino) != 0) {
        return 0;
    }

    if(!inode_has_perm(&ino, 0)) return -1;

    unsigned to_read = ino.size;
    if (to_read > cap) to_read = cap;

    static unsigned char blk[FS_BLOCK_SIZE];
    unsigned copied = 0;
    for (unsigned bi = 0; bi < ino.nblocks && copied < to_read && bi < 10; ++bi) {
        if (ino.direct[bi] == 0) break;
        if (fs_read_block(ino.direct[bi], blk) != 0) break;
        unsigned left = to_read - copied;
        unsigned take = (left < FS_BLOCK_SIZE) ? left : FS_BLOCK_SIZE;
        memcpy((unsigned char*)buf + copied, blk, take);
        copied += take;
    }
    return (int)copied;
}

int fs_write_all(const char* path, const void* data, unsigned n) {
    if (!g_mounted) return -1;
    char abs[FS_PATH_MAX]; fs_to_abs(path, abs);

    struct fs_inode ino;
    uint32_t ino_id;
    if (lookup_file_abs(abs, &ino_id, &ino) != 0) {
        if (fs_touch(abs) != 0) return -1;
        if (lookup_file_abs(abs, &ino_id, &ino) != 0) return -1;
    } else {
        if(!inode_has_perm(&ino, 1)) return -1;
    }

    unsigned need = (n + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    if (need > 10) need = 10;

    uint32_t blk_ids[10];
    for (unsigned i = 0; i < 10; ++i) blk_ids[i] = 0;

    for (unsigned i = 0; i < need && i < ino.nblocks && i < 10; ++i) {
        blk_ids[i] = ino.direct[i];
    }

    for (unsigned i = ino.nblocks; i < need && i < 10; ++i) {
        if (blk_ids[i] == 0) {
            if (alloc_dblock(&blk_ids[i]) != 0) {
                for (unsigned j = ino.nblocks; j < i; ++j)
                    if (j < 10 && blk_ids[j] && (j >= ino.nblocks)) free_dblock(blk_ids[j]);
                return -1;
            }
        }
    }

    static unsigned char blk[FS_BLOCK_SIZE];
    unsigned written = 0;
    const unsigned char* p = (const unsigned char*)data;
    for (unsigned i = 0; i < need; ++i) {
        unsigned left = n - written;
        unsigned take = left < FS_BLOCK_SIZE ? left : FS_BLOCK_SIZE;
        if (take < FS_BLOCK_SIZE) {
            if (fs_read_block(blk_ids[i], blk) != 0) return -1;
            memcpy(blk, p + written, take);
        } else {
            memcpy(blk, p + written, FS_BLOCK_SIZE);
        }
        if (fs_write_block(blk_ids[i], blk) != 0) return -1;
        written += take;
    }

    if (ino.nblocks > need) {
        for (unsigned i = need; i < ino.nblocks && i < 10; ++i) {
            if (ino.direct[i]) { free_dblock(ino.direct[i]); ino.direct[i] = 0; }
        }
    }

    for (unsigned i = 0; i < 10; ++i) ino.direct[i] = (i < need) ? blk_ids[i] : 0;
    ino.nblocks = need;
    ino.size    = n;
    if (inode_write(ino_id, &ino) != 0) return -1;

    return (int)written;
}

static int dir_remove_entry(struct fs_inode* dir, uint32_t dir_ino,
                            const char* name, uint32_t* removed_ino) {
    if (dir->type != FS_IT_DIR) return -1;

    unsigned char buf[FS_BLOCK_SIZE];

    for (uint32_t b = 0; b < dir->nblocks && b < 10; ++b) {
        if (dir->direct[b] == 0) continue;
        if (fs_read_block(dir->direct[b], buf) != 0) return -1;

        struct fs_dirent* de = (struct fs_dirent*)buf;
        for (uint32_t i = 0; i < FS_DIRENTS_PER_BLK; ++i) {
            if (de[i].ino != 0 && strncmp(de[i].name, name, FS_NAME_MAX) == 0) {
                if (removed_ino) *removed_ino = de[i].ino;

                de[i].ino = 0;
                memset(de[i].name, 0, FS_NAME_MAX);

                if (fs_write_block(dir->direct[b], buf) != 0) return -1;

                if (dir->size >= 64) dir->size -= 64;
                if (inode_write(dir_ino, dir) != 0) return -1;
                return 0;
            }
        }
    }
    return -1;
}

static int dir_is_empty(const struct fs_inode* dir) {
    if (dir->type != FS_IT_DIR) return 0;

    if (dir->nblocks == 0) return 1;

    unsigned char buf[FS_BLOCK_SIZE];
    for (uint32_t b = 0; b < dir->nblocks && b < 10; ++b) {
        if (dir->direct[b] == 0) continue;
        if (fs_read_block(dir->direct[b], buf) != 0) return 0;

        struct fs_dirent* de = (struct fs_dirent*)buf;
        for (uint32_t i = 0; i < FS_DIRENTS_PER_BLK; ++i) {
            if (de[i].ino != 0) return 0;
        }
    }
    return 1;
}

int fs_rm(const char* path) {
    if (!g_mounted) return -1;

    char abs[FS_PATH_MAX];
    fs_to_abs(path, abs);

    if (strcmp(abs, "/") == 0) return -1;

    uint32_t pino; const char* name;
    if (resolve_parent(abs, &pino, &name) != 0) return -1;
    if (strcmp(name, "/") == 0) return -1;

    struct fs_inode parent;
    if (inode_read(pino, &parent) != 0) return -1;
    if (parent.type != FS_IT_DIR) return -1;

    struct fs_dirent de;
    if (dir_find_entry(&parent, name, &de, 0, 0) != 0) return -1;

    struct fs_inode ino;
    if (inode_read(de.ino, &ino) != 0) return -1;
    if (ino.type != FS_IT_FILE) return -1;

    uid_t uid = task_get_current_uid();
    if(uid != 0 && uid != ino.owner) return -1;

    for (uint32_t i = 0; i < ino.nblocks && i < 10; ++i) {
        if (ino.direct[i]) {
            if (free_dblock(ino.direct[i]) != 0) return -1;
        }
    }

    uint32_t removed_ino = 0;
    if (dir_remove_entry(&parent, pino, name, &removed_ino) != 0) return -1;

    if (free_inode(de.ino) != 0) return -1;

    return 0;
}

int fs_rmdir(const char* path) {
    if (!g_mounted) return -1;

    char abs[FS_PATH_MAX];
    fs_to_abs(path, abs);

    if (strcmp(abs, "/") == 0) return -1;

    uint32_t pino; const char* name;
    if (resolve_parent(abs, &pino, &name) != 0) return -1;
    if (strcmp(name, "/") == 0) return -1;

    struct fs_inode parent;
    if (inode_read(pino, &parent) != 0) return -1;
    if (parent.type != FS_IT_DIR) return -1;

    struct fs_dirent de;
    if (dir_find_entry(&parent, name, &de, 0, 0) != 0) return -1;

    struct fs_inode dino;
    if (inode_read(de.ino, &dino) != 0) return -1;
    if (dino.type != FS_IT_DIR) return -1;

    uid_t uid = task_get_current_uid();
    if(uid != 0 && uid != dino.owner) return -1;

    if (!dir_is_empty(&dino)) return -1;

    for (uint32_t i = 0; i < dino.nblocks && i < 10; ++i) {
        if (dino.direct[i]) {
            if (free_dblock(dino.direct[i]) != 0) return -1;
        }
    }

    uint32_t removed_ino = 0;
    if (dir_remove_entry(&parent, pino, name, &removed_ino) != 0) return -1;

    if (free_inode(de.ino) != 0) return -1;

    return 0;
}

int fs_typeof(const char* path, uint16_t* out_type) {
    if (!g_mounted) return -1;
    if (!path || !out_type) return -1;

    char abs[FS_PATH_MAX];
    fs_to_abs(path, abs);

    if (strcmp(abs, "/") == 0) {
        *out_type = FS_IT_DIR;
        return 0;
    }

    uint32_t pino; const char* name;
    if (resolve_parent(abs, &pino, &name) != 0) return -1;
    if (strcmp(name, "/") == 0) return -1;

    struct fs_inode parent;
    if (inode_read(pino, &parent) != 0) return -1;
    if (parent.type != FS_IT_DIR) return -1;

    struct fs_dirent de;
    if (dir_find_entry(&parent, name, &de, 0, 0) != 0) return -1;

    struct fs_inode node;
    if (inode_read(de.ino, &node) != 0) return -1;

    *out_type = node.type;
    return 0;
}

int fs_is_file(const char* path) {
    uint16_t t;
    if (fs_typeof(path, &t) != 0) return -1;
    return (t == FS_IT_FILE) ? 1 : 0;
}

int fs_is_dir(const char* path) {
    uint16_t t;
    if (fs_typeof(path, &t) != 0) return -1;
    return (t == FS_IT_DIR) ? 1 : 0;
}
