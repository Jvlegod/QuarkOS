#ifndef __virtio_h__
#define __virtio_h__
#include "platform.h"
#include "kprintf.h"

#define VIRTQ_DESC_F_NEXT   0x1
#define VIRTQ_DESC_F_WRITE  0x2

struct virtio_mmio_regs {
    volatile uint32_t magic;
    volatile uint32_t version;
    volatile uint32_t device_id;
    volatile uint32_t vendor_id;
    volatile uint32_t host_features;
    volatile uint32_t host_features_sel;
    volatile uint32_t guest_features;
    volatile uint32_t guest_features_sel;
    volatile uint32_t guest_page_size;
    volatile uint32_t queue_sel;
    volatile uint32_t queue_num_max;
    volatile uint32_t queue_num;
    volatile uint32_t queue_align;
    volatile uint32_t queue_pfn;
    volatile uint32_t queue_notify;
    volatile uint32_t interrupt_status;
    volatile uint32_t interrupt_ack;
    volatile uint32_t status;
    struct {
        volatile uint64_t capacity;
        volatile uint32_t size_max;
        volatile uint32_t seg_max;
    } config __attribute__((aligned(4)));
};

#define QUEUE_SIZE 8
struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtio_blk_req {
#define VIRTIO_BLK_T_IN 0
#define VIRTIO_BLK_T_OUT 1
    uint32_t type;   // 0: read, 1: write
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

static struct virtq_desc desc[QUEUE_SIZE] __attribute__((aligned(4096)));
static uint16_t avail_idx __attribute__((aligned(4096)));
static uint16_t used_idx __attribute__((aligned(4096)));

int virtio_blk_init();
int blk_read(uint64_t sector, void *buf, uint16_t count);
int blk_write(uint64_t sector, const void *buf, uint16_t count);

#endif /* __virtio_h__ */