#include "virtio_net.h"
#include "virtio_net.h"
#include "kprintf.h"

#include "cstdlib.h"

#ifndef PAGE
#define PAGE 4096
#endif

/* ring storage for two queues (legacy layout) */
static uint8_t  __attribute__((aligned(PAGE))) rx_ring[PAGE * 2];
static uint8_t  __attribute__((aligned(PAGE))) tx_ring[PAGE * 2];

static struct virtq_desc  *rx_desc;
static struct virtq_avail *rx_avail;
static struct virtq_used  *rx_used;
static uint16_t rx_last_used;

static struct virtq_desc  *tx_desc;
static struct virtq_avail *tx_avail;
static struct virtq_used  *tx_used;
static uint16_t tx_free_head, tx_num_free, tx_last_used;

struct net_buf {
    struct virtio_net_hdr hdr;
    uint8_t data[NET_MAX_PKT];
};

static struct net_buf rx_bufs[QUEUE_SIZE];
static struct net_buf tx_bufs[QUEUE_SIZE];

static volatile uint8_t *g_net_mmio = NULL;

static volatile uint8_t *probe_net_base(void) {
    const uintptr_t START  = (uintptr_t)VIRTIO_MMIO_BASE;
    const uintptr_t STRIDE = 0x1000u;
    const uintptr_t SLOTS  = 128u;

    for (uintptr_t i = 0; i < SLOTS; ++i) {
        volatile uint8_t *b = (volatile uint8_t *)(START + i * STRIDE);
        uint32_t magic = r32(b + MMIO_MAGIC);
        if (magic != MMIO_MAGIC_HEADER) continue;

        uint32_t ver = r32(b + MMIO_VERSION);
        uint32_t did = r32(b + MMIO_DEVICE_ID);
        kprintf("[virtio-net] slot=%u base=0x%lx ver=%u did=%u\r\n",
                (unsigned)i, (unsigned long)(START + i * STRIDE), ver, did);

        if (ver == 1 && did == VIRTIO_ID_NET) return b;
    }
    return NULL;
}

static int setup_queue_legacy(int qsel,
                              uint8_t *ring,
                              struct virtq_desc **desc,
                              struct virtq_avail **avail,
                              struct virtq_used **used) {
    volatile uint8_t *m = g_net_mmio;

    w32(m + MMIO_GUEST_PAGE_SIZE, PAGE);
    w32(m + MMIO_QUEUE_SEL, qsel);
    uint32_t qmax = r32(m + MMIO_QUEUE_NUM_MAX);
    if (!qmax || qmax < QUEUE_SIZE) return -1;
    w32(m + MMIO_QUEUE_NUM, QUEUE_SIZE);
    w32(m + MMIO_QUEUE_ALIGN, PAGE);

    *desc  = (struct virtq_desc*)ring;
    *avail = (struct virtq_avail*)(ring + sizeof(struct virtq_desc)*QUEUE_SIZE);
    *used  = (struct virtq_used*)(ring + PAGE);

    bzero(*desc,  sizeof(struct virtq_desc)*QUEUE_SIZE);
    bzero(*avail, sizeof(struct virtq_avail));
    bzero(*used,  sizeof(struct virtq_used));

    uint32_t pfn = (uint32_t)(VIRT_TO_PHYS(ring) >> 12);
    w32(m + MMIO_QUEUE_PFN, pfn);
    return 0;
}

static void tx_desc_init(void) {
    tx_free_head = 0;
    tx_num_free  = QUEUE_SIZE;
    for (uint16_t i = 0; i < QUEUE_SIZE; ++i) {
        tx_desc[i].next = i + 1;
    }
    tx_desc[QUEUE_SIZE - 1].next = 0xffff;
    tx_avail->flags = 0; tx_avail->idx = 0;
    tx_used->flags  = 0; tx_used->idx  = 0;
    tx_last_used    = 0;
}

static int tx_desc_alloc(void) {
    if (!tx_num_free) return -1;
    uint16_t i = tx_free_head;
    tx_free_head = tx_desc[i].next;
    tx_num_free--;
    return i;
}

static void tx_desc_free_chain(uint16_t head) {
    tx_desc[head].next = tx_free_head;
    tx_free_head = head;
    tx_num_free++;
}

int virtio_net_init(void) {
    g_net_mmio = probe_net_base();
    if (!g_net_mmio) {
        kprintf("virtio-net: device not found\r\n");
        return -1;
    }

    /* reset */
    w32(g_net_mmio + MMIO_STATUS, 0);
    w32(g_net_mmio + MMIO_STATUS, r32(g_net_mmio + MMIO_STATUS) | VIRTIO_STATUS_ACK);
    w32(g_net_mmio + MMIO_STATUS, r32(g_net_mmio + MMIO_STATUS) | VIRTIO_STATUS_DRIVER);

    /* negotiate no optional features */
    (void)r32(g_net_mmio + MMIO_DEVICE_FEATURES);
    w32(g_net_mmio + MMIO_DRIVER_FEATURES, 0);
    w32(g_net_mmio + MMIO_STATUS, r32(g_net_mmio + MMIO_STATUS) | VIRTIO_STATUS_FEATURES_OK);
    if (!(r32(g_net_mmio + MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK)) {
        w32(g_net_mmio + MMIO_STATUS, r32(g_net_mmio + MMIO_STATUS) | VIRTIO_STATUS_FAILED);
        kprintf("virtio-net: FEATURES_OK not accepted\r\n");
        return -1;
    }

    if (setup_queue_legacy(NET_RXQ, rx_ring, &rx_desc, &rx_avail, &rx_used) != 0) {
        kprintf("virtio-net: rx queue setup failed\r\n");
        return -1;
    }
    rx_last_used = 0;

    for (uint16_t i = 0; i < QUEUE_SIZE; ++i) {
        rx_desc[i].addr  = (uint64_t)VIRT_TO_PHYS(&rx_bufs[i]);
        rx_desc[i].len   = sizeof(struct net_buf);
        rx_desc[i].flags = VIRTQ_DESC_F_WRITE;
        rx_desc[i].next  = 0;

        uint16_t ai = rx_avail->idx % QUEUE_SIZE;
        rx_avail->ring[ai] = i;
        rx_avail->idx++;
    }

    w32(g_net_mmio + MMIO_QUEUE_NOTIFY, NET_RXQ);

    if (setup_queue_legacy(NET_TXQ, tx_ring, &tx_desc, &tx_avail, &tx_used) != 0) {
        kprintf("virtio-net: tx queue setup failed\r\n");
        return -1;
    }
    tx_desc_init();

    w32(g_net_mmio + MMIO_STATUS, r32(g_net_mmio + MMIO_STATUS) | VIRTIO_STATUS_DRIVER_OK);
    kprintf("virtio-net: legacy ready\r\n");
    return 0;
}

int virtio_net_send(const void *data, uint16_t len) {
    if (len > NET_MAX_PKT) len = NET_MAX_PKT;
    int h = tx_desc_alloc();
    if (h < 0) return -1;

    struct net_buf *b = &tx_bufs[h];
    bzero(&b->hdr, sizeof(b->hdr));
    memcpy(b->data, data, len);

    tx_desc[h].addr  = (uint64_t)VIRT_TO_PHYS(b);
    tx_desc[h].len   = sizeof(b->hdr) + len;
    tx_desc[h].flags = 0;
    tx_desc[h].next  = 0;

    uint16_t aidx = tx_avail->idx;
    tx_avail->ring[aidx % QUEUE_SIZE] = (uint16_t)h;
    barrier();
    tx_avail->idx = aidx + 1;
    barrier();

    w32(g_net_mmio + MMIO_QUEUE_NOTIFY, NET_TXQ);

    while (tx_used->idx == tx_last_used) { }
    uint16_t u = tx_last_used % QUEUE_SIZE;
    struct virtq_used_elem *ue = &tx_used->ring[u];
    tx_last_used++;
    tx_desc_free_chain((uint16_t)ue->id);

    return 0;
}

int virtio_net_recv(void *buf, uint16_t buflen) {
    if (rx_used->idx == rx_last_used) return -1;

    uint16_t u = rx_last_used % QUEUE_SIZE;
    struct virtq_used_elem *ue = &rx_used->ring[u];
    uint16_t id = (uint16_t)ue->id;
    uint32_t len = ue->len;
    rx_last_used++;

    uint32_t pkt_len = (len > sizeof(struct virtio_net_hdr)) ?
                        len - sizeof(struct virtio_net_hdr) : 0;
    if (pkt_len > buflen) pkt_len = buflen;
    memcpy(buf, rx_bufs[id].data, pkt_len);

    /* re-post buffer */
    uint16_t ai = rx_avail->idx % QUEUE_SIZE;
    rx_avail->ring[ai] = id;
    barrier();
    rx_avail->idx++;
    barrier();
    w32(g_net_mmio + MMIO_QUEUE_NOTIFY, NET_RXQ);

    return (int)pkt_len;
}