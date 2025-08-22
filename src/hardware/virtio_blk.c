#include "virtio.h"
#include "virtio_blk.h"

static uint64_t g_virtio_capacity_sectors = 0;

#ifndef PAGE
#define PAGE 4096
#endif

static uint8_t  __attribute__((aligned(PAGE))) ring_modern[PAGE];
static struct virtq_desc  *desc_m  = (struct virtq_desc*) ring_modern;
static struct virtq_avail *avail_m = (struct virtq_avail*)(ring_modern + sizeof(struct virtq_desc)*QUEUE_SIZE);
static struct virtq_used  *used_m  = (struct virtq_used*)
                                     (ring_modern + sizeof(struct virtq_desc)*QUEUE_SIZE + sizeof(struct virtq_avail));

static uint8_t  __attribute__((aligned(PAGE))) ring_legacy[PAGE*2];
static struct virtq_desc  *desc_l  = (struct virtq_desc*) ring_legacy;
static struct virtq_avail *avail_l = (struct virtq_avail*)(ring_legacy + sizeof(struct virtq_desc)*QUEUE_SIZE);
static struct virtq_used  *used_l  = (struct virtq_used*)(ring_legacy + PAGE);

static struct virtq_desc  *vq_desc;
static struct virtq_avail *vq_avail;
static struct virtq_used  *vq_used;

static uint16_t free_head, num_free, last_used_idx;

static void desc_init_common(void){
    free_head = 0;
    num_free  = QUEUE_SIZE;
    for (uint16_t i=0;i<QUEUE_SIZE;i++){
        vq_desc[i].next  = i+1;
        vq_desc[i].addr  = 0;
        vq_desc[i].len   = 0;
        vq_desc[i].flags = 0;
    }
    vq_desc[QUEUE_SIZE-1].next = 0xffff;
    vq_avail->flags = 0; vq_avail->idx = 0;
    vq_used->flags  = 0; vq_used->idx  = 0;
    last_used_idx   = 0;
}
static int  desc_alloc(void){ if(!num_free) return -1; uint16_t i=free_head; free_head=vq_desc[i].next; num_free--; return i; }
static void desc_free_chain(uint16_t head){
    uint16_t i=head;
    while(1){
        uint16_t nx = vq_desc[i].next;
        vq_desc[i].next = free_head;
        free_head = i; num_free++;
        if(!(vq_desc[i].flags & VIRTQ_DESC_F_NEXT)) break;
        i = nx;
    }
}

/* ----------------- features ----------------- */
static void negotiate_features(uint32_t ver){
    if (ver >= 2){
        w32(mmio + MMIO_DEVICE_FEATURES_SEL, 0);
        (void)r32(mmio + MMIO_DEVICE_FEATURES);
        w32(mmio + MMIO_DEVICE_FEATURES_SEL, 1);
        uint32_t f1 = r32(mmio + MMIO_DEVICE_FEATURES);

        uint32_t out0 = 0;
        uint32_t out1 = f1 & VIRTIO_F_VERSION_1_WORD1_BIT0;

        w32(mmio + MMIO_DRIVER_FEATURES_SEL, 0);
        w32(mmio + MMIO_DRIVER_FEATURES, out0);
        w32(mmio + MMIO_DRIVER_FEATURES_SEL, 1);
        w32(mmio + MMIO_DRIVER_FEATURES, out1);

        w32(mmio + MMIO_STATUS, r32(mmio + MMIO_STATUS) | VIRTIO_STATUS_FEATURES_OK);
    } else {
        w32(mmio + MMIO_DRIVER_FEATURES, 0);

        w32(mmio + MMIO_STATUS, r32(mmio + MMIO_STATUS) | VIRTIO_STATUS_FEATURES_OK);
        if (!(r32(mmio + MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK)) {
            kprintf("virtio-blk: legacy FEATURES_OK not accepted\n");
            w32(mmio + MMIO_STATUS, r32(mmio + MMIO_STATUS) | VIRTIO_STATUS_FAILED);
        }
    }
}

static int setup_queue_modern(void){
    w32(mmio + MMIO_QUEUE_SEL, 0);
    uint32_t qmax = r32(mmio + MMIO_QUEUE_NUM_MAX);
    if(qmax == 0 || qmax < QUEUE_SIZE) return -1;
    w32(mmio + MMIO_QUEUE_NUM, QUEUE_SIZE);

    vq_desc  = desc_m;
    vq_avail = avail_m;
    vq_used  = used_m;
    desc_init_common();

    wr64(mmio, MMIO_QUEUE_DESC_LOW,  (uint64_t)(uintptr_t)vq_desc);
    wr64(mmio, MMIO_QUEUE_AVAIL_LOW, (uint64_t)(uintptr_t)vq_avail);
    wr64(mmio, MMIO_QUEUE_USED_LOW,  (uint64_t)(uintptr_t)vq_used);
    w32(mmio + MMIO_QUEUE_READY, 1);
    return 0;
}

static int setup_queue_legacy(void){
    /* legacy 需要 page_size/alignment/PFN */
    w32(mmio + MMIO_GUEST_PAGE_SIZE, PAGE);

    w32(mmio + MMIO_QUEUE_SEL, 0);
    uint32_t qmax = r32(mmio + MMIO_QUEUE_NUM_MAX);
    if(qmax == 0 || qmax < QUEUE_SIZE) return -1;
    w32(mmio + MMIO_QUEUE_NUM, QUEUE_SIZE);
    w32(mmio + MMIO_QUEUE_ALIGN, PAGE);

    vq_desc  = desc_l;
    vq_avail = avail_l;
    vq_used  = used_l;
    desc_init_common();

    uint32_t pfn = (uint32_t)(VIRT_TO_PHYS(ring_legacy) >> 12);
    w32(mmio + MMIO_QUEUE_PFN, pfn);
    return 0;
}

int virtio_blk_init(void){
    uint32_t magic = r32(mmio + MMIO_MAGIC);
    uint32_t ver   = r32(mmio + MMIO_VERSION);
    uint32_t did   = r32(mmio + MMIO_DEVICE_ID);

    if (magic != 0x74726976u || did != 2){
        kprintf("virtio-blk: not found @%p magic=0x%x ver=%u id=%u\n", mmio, magic, ver, did);
        return -1;
    }
    kprintf("virtio-blk: found @%p magic=0x%x ver=%u id=%u\n", mmio, magic, ver, did);

    /* reset + ack + driver */
    w32(mmio + MMIO_STATUS, 0);
    w32(mmio + MMIO_STATUS, r32(mmio + MMIO_STATUS) | VIRTIO_STATUS_ACK);
    w32(mmio + MMIO_STATUS, r32(mmio + MMIO_STATUS) | VIRTIO_STATUS_DRIVER);

    negotiate_features(ver);

    int rc = (ver >= 2) ? setup_queue_modern() : setup_queue_legacy();
    if (rc < 0){
        kprintf("virtio-blk: setup queue failed (ver=%u)\n", ver);
        return -1;
    }

    uint32_t cap_lo = r32(mmio + MMIO_CONFIG + 0);
    uint32_t cap_hi = r32(mmio + MMIO_CONFIG + 4);
    uint64_t capacity = ((uint64_t)cap_hi << 32) | cap_lo;
    g_virtio_capacity_sectors = capacity;
    kprintf("virtio-blk: capacity=%lu sectors (512B)\n", (unsigned long long)capacity);

    w32(mmio + MMIO_STATUS, r32(mmio + MMIO_STATUS) | VIRTIO_STATUS_DRIVER_OK);
    return 0;
}

static int virtio_blk_do(uint32_t type, uint64_t sector, void *data, uint16_t count){
    static struct virtio_blk_req hdr;
    static uint8_t status;

    int h = desc_alloc(); if(h < 0) return -1;
    int b = desc_alloc(); if(b < 0){ desc_free_chain(h); return -1; }
    int s = desc_alloc(); if(s < 0){ desc_free_chain(h); desc_free_chain(b); return -1; }

    hdr.type = type; hdr.reserved=0; hdr.sector=sector;
    status = 0xff;

    /* hdr */
    vq_desc[h].addr  = (uint64_t)VIRT_TO_PHYS(&hdr);
    vq_desc[h].len   = sizeof(hdr);
    vq_desc[h].flags = VIRTQ_DESC_F_NEXT;
    vq_desc[h].next  = (uint16_t)b;

    /* data */
    vq_desc[b].addr  = (uint64_t)VIRT_TO_PHYS(data);
    vq_desc[b].len   = (uint32_t)count * 512;
    vq_desc[b].flags = VIRTQ_DESC_F_NEXT | (type==VIRTIO_BLK_T_IN ? VIRTQ_DESC_F_WRITE : 0);
    vq_desc[b].next  = (uint16_t)s;

    /* status */
    vq_desc[s].addr  = (uint64_t)VIRT_TO_PHYS(&status);
    vq_desc[s].len   = 1;
    vq_desc[s].flags = VIRTQ_DESC_F_WRITE;
    vq_desc[s].next  = 0;

    /* push 到 avail */
    uint16_t aidx = vq_avail->idx;
    vq_avail->ring[aidx % QUEUE_SIZE] = (uint16_t)h;
    barrier();
    vq_avail->idx = aidx + 1;
    barrier();

    /* notify 0 */
    w32(mmio + MMIO_QUEUE_NOTIFY, 0);
    size_t spin = 0;

    while (vq_used->idx == last_used_idx) {
        if ((++spin & ((1<<20)-1)) == 0) {
            kprintf("polling... avail=%u used=%u\n", vq_avail->idx, vq_used->idx);
        }
    }
    uint16_t u = last_used_idx % QUEUE_SIZE;
    struct virtq_used_elem *ue = &vq_used->ring[u];
    last_used_idx++;

    desc_free_chain((uint16_t)ue->id);

    if (status != 0){
        kprintf("virtio-blk: request failed, status=%u\n", status);
        return -1;
    }
    return 0;
}

/* ---- Public APIs ---- */
int blk_read(uint64_t sector, void *buf, uint16_t count){
    return virtio_blk_do(VIRTIO_BLK_T_IN, sector, buf, count);
}
int blk_write(uint64_t sector, const void *buf, uint16_t count){
    return virtio_blk_do(VIRTIO_BLK_T_OUT, sector, (void*)buf, count);
}

uint64_t virtio_capacity_sectors(void) {
    return g_virtio_capacity_sectors;
}