#ifndef __VIRTIO_BLK_H__
#define __VIRTIO_BLK_H__

#include "virtio.h"

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

#endif