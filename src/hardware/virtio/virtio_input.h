// virtio_input.h —— legacy-only, static-only
#ifndef __VIRTIO_INPUT_H__
#define __VIRTIO_INPUT_H__
#include "virtio.h"
#include "ktypes.h"
#include "kprintf.h"

#ifndef VIRTIO_DEVICE_ID_INPUT
#define VIRTIO_DEVICE_ID_INPUT 18
#endif

#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03
#define SYN_REPORT 0

#define REL_X 0x00
#define REL_Y 0x01
#define ABS_X 0x00
#define ABS_Y 0x01

#ifndef INPUT_QUEUE_SIZE
#define INPUT_QUEUE_SIZE QUEUE_SIZE
#endif

#ifndef INPUT_BUF_EVENTS
#define INPUT_BUF_EVENTS 64
#endif

#ifndef VIRTIO_RING_ALIGN
#define VIRTIO_RING_ALIGN 4096u
#endif

/* 事件 */
struct virtio_input_event {
    uint16_t type;
    uint16_t code;
    int32_t  value;
} __attribute__((packed));

struct input_buf {
    struct virtio_input_event ev[INPUT_BUF_EVENTS];
} __attribute__((packed));

struct vqueue {
    uint16_t qsize;
    struct virtq_desc*  desc;
    struct virtq_avail* avail;
    struct virtq_used*  used;
    uint16_t last_used_idx;

    void*     ring_area_virt;
    uintptr_t ring_area_phys;
    size_t    ring_area_bytes;
};

typedef void (*virtio_input_event_cb)(void* user, uint16_t type, uint16_t code, int32_t value);

typedef struct virtio_input_dev {
    volatile uint8_t* mmio;
    struct vqueue eventq;

    struct input_buf* event_bufs;
    uint16_t          event_buf_count;

    virtio_input_event_cb cb;
    void* cb_user;
} virtio_input_dev_t;

#define VQ_DESC_BYTES   (sizeof(struct virtq_desc) * (INPUT_QUEUE_SIZE))
#define VQ_AVAIL_BYTES  (sizeof(struct virtq_avail))
#define VQ_USED_BYTES   (sizeof(struct virtq_used))

#define ALIGN_UP(x,a)   ( ((x)+((a)-1)) & ~((a)-1) )

#define VQ_OFF_DESC   (0u)
#define VQ_OFF_AVAIL  (VQ_OFF_DESC + VQ_DESC_BYTES)
#define VQ_OFF_USED   (ALIGN_UP(VQ_OFF_AVAIL + VQ_AVAIL_BYTES, VIRTIO_RING_ALIGN))
#define VQ_AREA_BYTES (VQ_OFF_USED + VQ_USED_BYTES)

typedef struct virtio_input_storage {
    uint8_t ring_area[VQ_AREA_BYTES] __attribute__((aligned(VIRTIO_RING_ALIGN)));
    struct input_buf bufs[INPUT_QUEUE_SIZE] __attribute__((aligned(64)));
} virtio_input_storage_t;

/* init legacy */
int virtio_input_init_legacy_static(virtio_input_dev_t* dev,
                                    uintptr_t mmio_base,
                                    virtio_input_event_cb cb,
                                    void* cb_user,
                                    virtio_input_storage_t* storage);


void virtio_input_handle_irq(virtio_input_dev_t* dev);
static inline void virtio_input_poll(virtio_input_dev_t* dev){ virtio_input_handle_irq(dev); }

void input_init_all(void);
// void input_irq_handler(void);
// void input_poll_all(void);

#endif