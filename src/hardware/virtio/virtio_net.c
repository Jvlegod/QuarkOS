#include "virtio_net.h"
#include "virtio_net.h"
#include "kprintf.h"

static volatile uint8_t *g_net_mmio = NULL;

static volatile uint8_t *probe_net_base(void) {
    const uintptr_t START  = (uintptr_t)VIRTIO_MMIO_BASE;
    const uintptr_t STRIDE = 0x1000u;
    const uintptr_t SLOTS  = 128u;
    for (uintptr_t i = 0; i < SLOTS; ++i) {
        volatile uint8_t *b = (volatile uint8_t *)(START + i * STRIDE);
        uint32_t magic = r32(b + MMIO_MAGIC);
        if (magic != MMIO_MAGIC_HEADER) continue;
        uint32_t did = r32(b + MMIO_DEVICE_ID);
        if (did == VIRTIO_ID_NET) return b;
    }
    return NULL;
}

int virtio_net_init(void) {
    g_net_mmio = probe_net_base();
    if (!g_net_mmio) {
        kprintf("virtio-net: device not found\r\n");
        return -1;
    }

    /* reset */
    w32(g_net_mmio + MMIO_STATUS, 0);
    /* acknowledge and set driver status */
    w32(g_net_mmio + MMIO_STATUS, r32(g_net_mmio + MMIO_STATUS) | VIRTIO_STATUS_ACK);
    w32(g_net_mmio + MMIO_STATUS, r32(g_net_mmio + MMIO_STATUS) | VIRTIO_STATUS_DRIVER);

    /* for now we don't negotiate advanced features */
    w32(g_net_mmio + MMIO_STATUS, r32(g_net_mmio + MMIO_STATUS) | VIRTIO_STATUS_FEATURES_OK);
    if (!(r32(g_net_mmio + MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK)) {
        w32(g_net_mmio + MMIO_STATUS, r32(g_net_mmio + MMIO_STATUS) | VIRTIO_STATUS_FAILED);
        kprintf("virtio-net: FEATURES_OK not accepted\r\n");
        return -1;
    }

    /*TODO: queues would normally be configured here */

    w32(g_net_mmio + MMIO_STATUS, r32(g_net_mmio + MMIO_STATUS) | VIRTIO_STATUS_DRIVER_OK);
    kprintf("virtio-net: initialized (stub)\r\n");
    return 0;
}

int virtio_net_send(const void *data, uint16_t len) {
    (void)data;
    (void)len;
    /* TODO: implement transmit using virtqueue */
    return -1;
}

int virtio_net_recv(void *buf, uint16_t buflen) {
    (void)buf;
    (void)buflen;
    /* TODO: implement receive using virtqueue */
    return -1;
}