#include "virtio.h"

static struct virtio_mmio_regs *regs = (void*)VIRTIO_MMIO_BASE;

int virtio_blk_init() {
    if (regs->magic != 0x74726976 || regs->device_id != 0x02) {
        return -1;
    }

    regs->status = 0;

    regs->host_features_sel = 0;
    uint32_t features = regs->host_features;
    features &= (1 << 5);
    regs->guest_features_sel = 0;
    regs->guest_features = features;

    regs->queue_sel = 0;
    if (regs->queue_num_max < QUEUE_SIZE) {
        return -2;
    }
    regs->queue_num = QUEUE_SIZE;

    uint32_t pfn = (uint32_t)((uintptr_t)desc >> 12);
    regs->queue_pfn = pfn;

    regs->status |= 0x04; // ACKNOWLEDGE
    regs->status |= 0x02; // DRIVER
    regs->status |= 0x01; // DRIVER_OK

    uint64_t sector_count = regs->config.capacity;
    kprintf("Disk size: %lu sectors\n", sector_count);

    return 0;
}

int blk_read(uint64_t sector, void *buf, uint16_t count) {
    if (count == 0 || buf == NULL) return -1;

    struct virtio_blk_req req = {
        .type = VIRTIO_BLK_T_IN,
        .sector = sector
    };

    uint16_t idx = 0;
    desc[idx].addr = (uintptr_t)&req;
    desc[idx].len = sizeof(req);
    desc[idx].flags = VIRTQ_DESC_F_NEXT;
    desc[idx].next = ++idx;

    desc[idx].addr = (uintptr_t)buf;
    desc[idx].len = 512 * count;
    desc[idx].flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;
    desc[idx].next = ++idx;

    uint8_t status;
    desc[idx].addr = (uintptr_t)&status;
    desc[idx].len = sizeof(status);
    desc[idx].flags = VIRTQ_DESC_F_WRITE;
    desc[idx].next = 0;

    avail_idx = (avail_idx + 1) % QUEUE_SIZE;

    regs->queue_notify = 0;

    while (used_idx == avail_idx) {}

    return (status == 0) ? 0 : -1;
}

int blk_write(uint64_t sector, const void *buf, uint16_t count) {
    if (count == 0 || buf == NULL) return -1;

    struct virtio_blk_req req = {
        .type = VIRTIO_BLK_T_OUT,
        .sector = sector
    };

    uint8_t status;
    uint16_t idx = 0;

    desc[idx].addr = (uintptr_t)&req;
    desc[idx].len = sizeof(req);
    desc[idx].flags = VIRTQ_DESC_F_NEXT;
    desc[idx].next = ++idx;

    desc[idx].addr = (uintptr_t)buf;
    desc[idx].len = 512 * count;
    desc[idx].flags = VIRTQ_DESC_F_NEXT;
    desc[idx].next = ++idx;

    desc[idx].addr = (uintptr_t)&status;
    desc[idx].len = 1;
    desc[idx].flags = VIRTQ_DESC_F_WRITE;
    desc[idx].next = 0;

    avail_idx = (avail_idx + 1) % QUEUE_SIZE;

    regs->queue_notify = 0;

    while (used_idx == avail_idx);

    return (status == 0) ? 0 : -1;
}