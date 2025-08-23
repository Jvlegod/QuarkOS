// virtio_input_legacy_static.c
#include "virtio_input.h"
#include "virtio_keyboard.h"
#include "virtio_tablet.h"
#include "cstdlib.h"

#define KBD_MMIO_BASE    0x10002000u
#define TABLET_MMIO_BASE 0x10003000u

static virtio_input_storage_t g_kbd_store;
static virtio_input_storage_t g_tab_store;

virtio_kbd_t    g_kbd;
virtio_tablet_t g_tab;

static inline void mmio_status_or(volatile uint8_t* m, uint32_t bits) {
    uint32_t v = r32(m + MMIO_STATUS);
    w32(m + MMIO_STATUS, v | bits);
}

/* 提交 legacy 队列（queue 0），使用静态 ring_area */
static int vq_setup_legacy_static(virtio_input_dev_t* d, struct vqueue* q,
                                  uint16_t qsel, virtio_input_storage_t* st)
{
    volatile uint8_t* m = d->mmio;

    w32(m + MMIO_QUEUE_SEL, qsel);
    uint32_t qmax = r32(m + MMIO_QUEUE_NUM_MAX);
    if (!qmax) return -1;

    uint16_t qsz = (INPUT_QUEUE_SIZE <= qmax) ? INPUT_QUEUE_SIZE : (uint16_t)qmax;
    w32(m + MMIO_QUEUE_NUM, qsz);

    q->qsize = qsz;
    q->last_used_idx = 0;

    /* legacy 必需：告诉设备页大小与对齐 */
    w32(m + MMIO_GUEST_PAGE_SIZE, 4096u);
    w32(m + MMIO_QUEUE_ALIGN,     VIRTIO_RING_ALIGN);

    /* 绑定静态区域到三个 ring 结构 */
    uintptr_t base = (uintptr_t)st->ring_area;
    q->ring_area_virt  = (void*)base;
    q->ring_area_phys  = VIRT_TO_PHYS((void*)base);
    q->ring_area_bytes = VQ_AREA_BYTES;

    q->desc  = (struct virtq_desc*)(base + VQ_OFF_DESC);
    q->avail = (struct virtq_avail*)(base + VQ_OFF_AVAIL);
    q->used  = (struct virtq_used*)(base + VQ_OFF_USED);

    /* 清零 */
    memset(q->desc,  0, VQ_DESC_BYTES);
    memset(q->avail, 0, VQ_AVAIL_BYTES);
    memset(q->used,  0, VQ_USED_BYTES);

    /* legacy：提交 PFN（物理页框号） */
    uint32_t pfn = (uint32_t)(q->ring_area_phys >> 12);
    w32(m + MMIO_QUEUE_PFN, pfn);

    return 0;
}

/* 投放所有可写 buffer 到 event 队列（设备写事件） */
static void eventq_post_all(virtio_input_dev_t* d)
{
    struct vqueue* q = &d->eventq;

    for (uint16_t i = 0; i < q->qsize; ++i) {
        /* 用静态缓冲区作为 desc[i] 的写入目标 */
        q->desc[i].addr  = (uint64_t)VIRT_TO_PHYS(&d->event_bufs[i]);
        q->desc[i].len   = sizeof(struct input_buf);
        q->desc[i].flags = VIRTQ_DESC_F_WRITE;
        q->desc[i].next  = 0;

        uint16_t ai = q->avail->idx % q->qsize;
        q->avail->ring[ai] = i;
        q->avail->idx++;
    }

    /* 通知设备队列 0 */
    w32(d->mmio + MMIO_QUEUE_NOTIFY, 0);
}

/* legacy: 只读 32 位特性，并写回想启用的（这里全 0） */
static void legacy_negotiate_features(volatile uint8_t* m)
{
    (void)r32(m + MMIO_DEVICE_FEATURES); /* devf 可读取但当前不启用可选位 */
    w32(m + MMIO_DRIVER_FEATURES, 0);
}

int virtio_input_init_legacy_static(virtio_input_dev_t* dev,
                                    uintptr_t mmio_base,
                                    virtio_input_event_cb cb,
                                    void* cb_user,
                                    virtio_input_storage_t* storage)
{
    if (!dev || !storage) return -1;
    memset(dev, 0, sizeof(*dev));

    dev->mmio = (volatile uint8_t*)mmio_base;
    dev->cb   = cb;
    dev->cb_user = cb_user;

    /* 事件缓冲数组使用静态 storage->bufs */
    dev->event_bufs      = storage->bufs;
    dev->event_buf_count = INPUT_QUEUE_SIZE;

    volatile uint8_t* m = dev->mmio;

    uint32_t magic   = r32(m + MMIO_MAGIC);
    uint32_t version = r32(m + MMIO_VERSION);
    uint32_t devid   = r32(m + MMIO_DEVICE_ID);

    if (magic != 0x74726976u) {
        kprintf("[virtio-input] bad magic @%p\n", m);
        return -2;
    }
    if (version != 1) { /* 只接受 legacy */
        kprintf("[virtio-input] need legacy mmio (ver=1), got %u\n", version);
        return -3;
    }
    if (devid != VIRTIO_DEVICE_ID_INPUT) {
        kprintf("[virtio-input] not input device (id=%u)\n", devid);
        return -4;
    }

    w32(m + MMIO_STATUS, 0);
    mmio_status_or(m, VIRTIO_STATUS_ACK);
    mmio_status_or(m, VIRTIO_STATUS_DRIVER);

    legacy_negotiate_features(m);

    if (vq_setup_legacy_static(dev, &dev->eventq, 0, storage) != 0) {
        kprintf("[virtio-input] eventq setup failed\n");
        return -5;
    }

    eventq_post_all(dev);

    mmio_status_or(m, VIRTIO_STATUS_DRIVER_OK);
    kprintf("[virtio-input] legacy ready (static), qsize=%u\n", dev->eventq.qsize);
    return 0;
}

// input irq
void virtio_input_handle_irq(virtio_input_dev_t* dev)
{
    struct vqueue* q = &dev->eventq;
    volatile uint8_t* m = dev->mmio;

    uint32_t isr = r32(m + MMIO_INTERRUPT_STATUS);
    if (isr) {
        w32(m + MMIO_INTERRUPT_ACK, isr);
    }

    while (q->last_used_idx != q->used->idx) {
        uint16_t uix = q->last_used_idx % q->qsize;
        struct virtq_used_elem* ue = &q->used->ring[uix];

        uint32_t id  = ue->id;
        uint32_t len = ue->len;

        if (id < q->qsize) {
            struct input_buf* b = &dev->event_bufs[id];
            uint32_t cnt = len / sizeof(struct virtio_input_event);
            
            if (cnt > INPUT_BUF_EVENTS) {
                cnt = INPUT_BUF_EVENTS;
            }

            for (uint32_t i = 0; i < cnt; ++i) {
                struct virtio_input_event* e = &b->ev[i];
                if (dev->cb) dev->cb(dev->cb_user, e->type, e->code, e->value);
            }

            uint16_t ai = q->avail->idx % q->qsize;
            q->avail->ring[ai] = (uint16_t)id;
            q->avail->idx++;
        }
        q->last_used_idx++;
    }

    w32(m + MMIO_QUEUE_NOTIFY, 0);
}

static void on_key(void* u, uint16_t key, int pressed) {
    kprintf("[kbd] key=%u %s\n", key, pressed ? "down" : "up");
}
static void on_abs(void* u, int code, int val) {
    if (code == ABS_X || code == ABS_Y) {
        kprintf("[tab] %s=%d\n", (code==ABS_X)?"X":"Y", val);
    }
}
static void on_btn(void* u, int code, int pressed) {
    kprintf("[tab] BTN %d %s\n", code, pressed?"down":"up");
}
static void on_syn(void* u) {
    /* 一批事件结束，可在这里刷新光标到 g_tab.x/g_tab.y */
    // desktop_set_cursor(g_tab.x, g_tab.y);
}

void input_init_all() {
    virtio_kbd_init_static(&g_kbd, KBD_MMIO_BASE, &g_kbd_store, on_key, NULL);
    virtio_tablet_init_static(&g_tab, TABLET_MMIO_BASE, &g_tab_store, on_abs, on_btn, on_syn, NULL);
}

// 弃用
// void input_irq_handler(void) {
//     virtio_kbd_irq(&g_kbd);
//     virtio_tablet_irq(&g_tab);
// }

// 弃用
// void input_poll_all() {
//     virtio_kbd_poll(&g_kbd);
//     virtio_tablet_poll(&g_tab);
// }
