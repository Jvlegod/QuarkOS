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

#ifndef VIRT_TO_PHYS
#define VIRT_TO_PHYS(p) ((uintptr_t)(p))
#endif

#define MMIO_MAGIC                0x000
#define MMIO_VERSION              0x004
#define MMIO_DEVICE_ID            0x008
#define MMIO_VENDOR_ID            0x00c
#define MMIO_DEVICE_FEATURES      0x010
#define MMIO_DRIVER_FEATURES      0x020
#define MMIO_QUEUE_SEL            0x030
#define MMIO_QUEUE_NUM_MAX        0x034
#define MMIO_QUEUE_NUM            0x038
#define MMIO_QUEUE_NOTIFY         0x050
#define MMIO_INTERRUPT_STATUS     0x060
#define MMIO_INTERRUPT_ACK        0x064
#define MMIO_STATUS               0x070
#define MMIO_CONFIG               0x100

#define MMIO_DEVICE_FEATURES_SEL  0x014
#define MMIO_DRIVER_FEATURES_SEL  0x024
#define MMIO_QUEUE_READY          0x044
#define MMIO_QUEUE_DESC_LOW       0x080
#define MMIO_QUEUE_DESC_HIGH      0x084
#define MMIO_QUEUE_AVAIL_LOW      0x090
#define MMIO_QUEUE_AVAIL_HIGH     0x094
#define MMIO_QUEUE_USED_LOW       0x0a0
#define MMIO_QUEUE_USED_HIGH      0x0a4

#define MMIO_GUEST_PAGE_SIZE      0x028
#define MMIO_QUEUE_ALIGN          0x03c
#define MMIO_QUEUE_PFN            0x040

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

static volatile uint8_t *mmio = (volatile uint8_t*)VIRTIO_MMIO_BASE;
static inline void w32(volatile void *a, uint32_t v){ *(volatile uint32_t*)a = v; barrier(); }
static inline uint32_t r32(volatile void *a){ uint32_t v = *(volatile uint32_t*)a; barrier(); return v; }
static inline void wr64(volatile uint8_t *base, uint32_t off_low, uint64_t val){
    w32(base + off_low,     (uint32_t)(val & 0xffffffffu));
    w32(base + off_low + 4, (uint32_t)(val >> 32));
}

#endif /* __VIRTIO_H__ */
