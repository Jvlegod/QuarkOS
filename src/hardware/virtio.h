#ifndef __VIRTIO_H__
#define __VIRTIO_H__

#include "platform.h"  // must define VIRTIO_MMIO_BASE
#include "kprintf.h"
#include "ktypes.h"    // must provide uint32_t/uint64_t or your aliases

/* ---- Block request types ---- */
#define VIRTIO_BLK_T_IN   0  /* read */
#define VIRTIO_BLK_T_OUT  1  /* write */

/* ---- Virtqueue desc flags ---- */
#define VIRTQ_DESC_F_NEXT    1
#define VIRTQ_DESC_F_WRITE   2

/* ---- Device status ---- */
#define VIRTIO_STATUS_ACK         1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_DRIVER_OK   4
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_FAILED      0x80

/* VERSION_1 位在 features 的 word1 bit0（全局 bit32） */
#define VIRTIO_F_VERSION_1_WORD1_BIT0  (1u << 0)

/* ---- Queue size ---- */
#ifndef QUEUE_SIZE
#define QUEUE_SIZE 16   /* 8/16/32 均可；16 足够起步 */
#endif

/* ---- Virtqueue structs ---- */
struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[QUEUE_SIZE];
    /* uint16_t used_event; // optional */
} __attribute__((packed));

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[QUEUE_SIZE];
    /* uint16_t avail_event; // optional */
} __attribute__((packed));

/* ---- Block request header ---- */
struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;   /* LBA in 512B sectors */
} __attribute__((packed));

/* ---- API ---- */
int virtio_blk_init(void);
uint64_t virtio_capacity_sectors(void);
int blk_read(uint64_t sector, void *buf, uint16_t count);
int blk_write(uint64_t sector, const void *buf, uint16_t count);

#endif /* __VIRTIO_H__ */
