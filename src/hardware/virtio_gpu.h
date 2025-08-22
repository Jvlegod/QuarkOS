#ifndef __VIRTIO_GPU_H__
#define __VIRTIO_GPU_H__

#include "ktypes.h"
#include "cstdlib.h"
#include "kprintf.h"

#ifndef VGPU_WIDTH
#define VGPU_WIDTH  800u
#endif
#ifndef VGPU_HEIGHT
#define VGPU_HEIGHT 600u
#endif

#define VRING_ALIGN 4u

#define VRING_DESC_SIZE   (16 * QUEUE_SIZE)
#define VRING_AVAIL_SIZE  (2 /*flags*/ + 2 /*idx*/ + 2*QUEUE_SIZE /*ring[]*/)
#define VRING_USED_SIZE   (2 /*flags*/ + 2 /*idx*/ + 8*QUEUE_SIZE /*ring[]*/)

#define ALIGN_UP(x,a)     (((x)+((a)-1)) & ~((a)-1))

#define VRING_OFF_DESC    0
#define VRING_OFF_AVAIL   ALIGN_UP(VRING_OFF_DESC + VRING_DESC_SIZE, 2)
#define VRING_OFF_USED    ALIGN_UP(VRING_OFF_AVAIL + VRING_AVAIL_SIZE, VRING_ALIGN)
#define VRING_AREA_SIZE   (VRING_OFF_USED + VRING_USED_SIZE)

static void bzero(void* p, size_t n){ unsigned char* d=(unsigned char*)p; while(n--) *d++=0; }
static inline uint32_t BGRA(uint8_t b,uint8_t g,uint8_t r){
    return (uint32_t)b | ((uint32_t)g<<8) | ((uint32_t)r<<16) | 0xFF000000u;
}

enum { VIRTIO_ID_GPU = 16 };

enum {
    VIRTIO_GPU_UNDEFINED                   = 0x0000,
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO        = 0x0100,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D      = 0x0101,
    VIRTIO_GPU_CMD_RESOURCE_UNREF          = 0x0102,
    VIRTIO_GPU_CMD_SET_SCANOUT             = 0x0103,
    VIRTIO_GPU_CMD_RESOURCE_FLUSH          = 0x0104,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D     = 0x0105,
    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING = 0x0106,
    VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING = 0x0107,

    VIRTIO_GPU_RESP_OK_NODATA              = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO        = 0x1101,
    VIRTIO_GPU_RESP_ERR_UNSPEC             = 0x1200
};

struct virtio_gpu_ctrl_hdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
} __attribute__((packed));

struct virtio_gpu_rect {
    uint32_t x,y,width,height;
} __attribute__((packed));

struct virtio_gpu_resp_display_info {
    struct virtio_gpu_ctrl_hdr hdr;
    struct {
        struct virtio_gpu_rect r;
        uint32_t enabled;
        uint32_t flags;
    } __attribute__((packed)) pmodes[16];
} __attribute__((packed));

struct virtio_gpu_resource_create_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} __attribute__((packed));

struct virtio_gpu_mem_entry {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} __attribute__((packed));

struct virtio_gpu_resource_attach_backing {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
} __attribute__((packed));

struct virtio_gpu_set_scanout {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint32_t scanout_id;
    uint32_t resource_id;
} __attribute__((packed));

struct virtio_gpu_transfer_to_host_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
} __attribute__((packed));

struct virtio_gpu_resource_flush {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint32_t resource_id;
    uint32_t padding;
} __attribute__((packed));

#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM 1

#define ALIGN_UP(x,a)   (((x)+((a)-1)) & ~((a)-1))

#define DESC_BYTES   (16 * QUEUE_SIZE)
#define AVAIL_BYTES  (2 /*flags*/ + 2 /*idx*/ + 2*QUEUE_SIZE /*ring[]*/)
#define USED_BYTES   (2 /*flags*/ + 2 /*idx*/ + 8*QUEUE_SIZE /*ring[]*/)

static uint8_t g_vring_area[VRING_AREA_SIZE] __attribute__((aligned(4096)));

static struct {
    volatile uint8_t* base;         /* 本设备 MMIO 基址 */
    uint32_t version;               /* 1 = legacy */

    struct virtq_desc  *desc;
    struct virtq_avail *avail;
    struct virtq_used  *used;
    uint16_t avail_idx;
    uint16_t last_used_idx;

    uint32_t* fb;
    uint32_t  pitch, width, height, res_id;
} g;

int  virtio_gpu_init(void);
void virtio_gpu_present_demo(void);


#endif