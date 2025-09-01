// virtio gpu lengency
#include "gfx_driver.h"
#include "virtio.h"
#include "kprintf.h"
#include "gfx.h"

enum { VIRTIO_ID_GPU = 16 };
enum {
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO        = 0x0100,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D      = 0x0101,
    VIRTIO_GPU_CMD_SET_SCANOUT             = 0x0103,
    VIRTIO_GPU_CMD_RESOURCE_FLUSH          = 0x0104,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D     = 0x0105,
    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING = 0x0106,
    VIRTIO_GPU_RESP_OK_NODATA              = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO        = 0x1101
};
struct virtio_gpu_ctrl_hdr {
    uint32_t type,flags;
    uint64_t fence_id;
    uint32_t ctx_id,padding;
} __attribute__((packed));

struct virtio_gpu_rect {
    uint32_t x,y,width,height;
} __attribute__((packed));

struct virtio_gpu_resp_display_info {
    struct virtio_gpu_ctrl_hdr hdr;
    struct { struct virtio_gpu_rect r; uint32_t enabled, flags; } __attribute__((packed)) pmodes[16];
} __attribute__((packed));

struct virtio_gpu_resource_create_2d { 
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id,format,width,height; 
} __attribute__((packed));

struct virtio_gpu_mem_entry {
    uint64_t addr;
    uint32_t length,padding;
} __attribute__((packed));

struct virtio_gpu_resource_attach_backing {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id,nr_entries;
} __attribute__((packed));

struct virtio_gpu_set_scanout {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint32_t scanout_id,resource_id;
} __attribute__((packed));

struct virtio_gpu_transfer_to_host_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint64_t offset;
    uint32_t resource_id,padding;
} __attribute__((packed));

struct virtio_gpu_resource_flush {
    struct virtio_gpu_ctrl_hdr hdr;
    struct virtio_gpu_rect r;
    uint32_t resource_id,padding;
} __attribute__((packed));

#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM 1

#define VRING_ALIGN 4u
#define VR_DESC_SZ   (16*QUEUE_SIZE)
#define VR_AVAIL_SZ  (2+2+2*QUEUE_SIZE)
#define VR_USED_SZ   (2+2+8*QUEUE_SIZE)
#define ALIGN_UP(x,a) (((x)+((a)-1)) & ~((a)-1))
#define OFF_DESC   0
#define OFF_AVAIL  ALIGN_UP(OFF_DESC+VR_DESC_SZ,2)
#define OFF_USED   ALIGN_UP(OFF_AVAIL+VR_AVAIL_SZ,VRING_ALIGN)
#define AREA_SZ    (OFF_USED+VR_USED_SZ)

static uint8_t g_area[AREA_SZ] __attribute__((aligned(4096)));

static struct {
    volatile uint8_t* base;
    struct virtq_desc  *desc;
    struct virtq_avail *avail;
    struct virtq_used  *used;
    uint16_t avail_idx,last_used_idx;

    uint32_t width,height,pitch;
    uint32_t* fb;
    uint32_t  res_id;
} g;

/* --- 探测 ver=1 的 GPU --- */
static volatile uint8_t* probe_base(void){
    const uintptr_t S=(uintptr_t)VIRTIO_MMIO_BASE, STEP=0x1000, N=128;
    for (uintptr_t i=0;i<N;i++){
        volatile uint8_t* b=(volatile uint8_t*)(S+i*STEP);
        if (r32(b+MMIO_MAGIC)!=0x74726976) continue;
        if (r32(b+MMIO_VERSION)!=1) continue;
        if (r32(b+MMIO_DEVICE_ID)==VIRTIO_ID_GPU) return b;
    }
    return 0;
}
static int vq_init_legacy(uint16_t q){
    uintptr_t base=(uintptr_t)g_area;
    g.desc  =(struct virtq_desc *)(base+OFF_DESC);
    g.avail =(struct virtq_avail*)(base+OFF_AVAIL);
    g.used  =(struct virtq_used *)(base+OFF_USED);
    bzero(g.desc,VR_DESC_SZ); bzero(g.avail,VR_AVAIL_SZ); bzero(g.used,VR_USED_SZ);
    g.avail_idx=g.last_used_idx=0;

    w32(g.base+MMIO_GUEST_PAGE_SIZE,4096);
    w32(g.base+MMIO_QUEUE_SEL,q);
    uint32_t qmax=r32(g.base+MMIO_QUEUE_NUM_MAX);
    if (qmax==0 || QUEUE_SIZE>qmax) return -1;
    w32(g.base+MMIO_QUEUE_NUM,QUEUE_SIZE);
    w32(g.base+MMIO_QUEUE_ALIGN,VRING_ALIGN);
    uintptr_t pfn=(uintptr_t)VIRT_TO_PHYS((void*)base)>>12;
    w32(g.base+MMIO_QUEUE_PFN,(uint32_t)pfn);
    kprintf("[gfx/virtio-legacy] q%u ready align=%u pfn=0x%lx\n",q,VRING_ALIGN,(unsigned long)pfn);
    return 0;
}
static int submit(void* req,uint32_t rl,void* resp,uint32_t pl){
    g.desc[0].addr=(uint64_t)VIRT_TO_PHYS(req);   g.desc[0].len=rl; g.desc[0].flags=VIRTQ_DESC_F_NEXT; g.desc[0].next=1;
    g.desc[1].addr=(uint64_t)VIRT_TO_PHYS(resp);  g.desc[1].len=pl; g.desc[1].flags=VIRTQ_DESC_F_WRITE; g.desc[1].next=0;
    uint16_t s=g.avail_idx%QUEUE_SIZE; WRITE_ONCE(g.avail->ring[s],0); cpu_wmb();
    WRITE_ONCE(g.avail->idx,(uint16_t)(g.avail_idx+1)); g.avail_idx++; cpu_wmb();
    w32(g.base+MMIO_QUEUE_NOTIFY,0);
    uint64_t spin=0;
    const uint64_t MAX = ((uint64_t)1<<22);
    while(g.last_used_idx==READ_ONCE(g.used->idx)){ cpu_rmb(); if(++spin==MAX) return -1; }
    g.last_used_idx++;
    return 0;
}

static int drv_probe(void){ return probe_base()?1:0; }
static int drv_init(void){
    g.base=probe_base(); if(!g.base) return -1;
    w32(g.base+MMIO_STATUS,0);
    w32(g.base+MMIO_STATUS,VIRTIO_STATUS_ACK);
    w32(g.base+MMIO_STATUS,r32(g.base+MMIO_STATUS)|VIRTIO_STATUS_DRIVER);

    /* legacy：features=0 */
    w32(g.base+MMIO_DEVICE_FEATURES_SEL,0); (void)r32(g.base+MMIO_DEVICE_FEATURES);
    w32(g.base+MMIO_DEVICE_FEATURES_SEL,1); (void)r32(g.base+MMIO_DEVICE_FEATURES);
    w32(g.base+MMIO_DRIVER_FEATURES_SEL,0); w32(g.base+MMIO_DRIVER_FEATURES,0);
    w32(g.base+MMIO_DRIVER_FEATURES_SEL,1); w32(g.base+MMIO_DRIVER_FEATURES,0);
    w32(g.base+MMIO_STATUS,r32(g.base+MMIO_STATUS)|VIRTIO_STATUS_FEATURES_OK);
    if(!(r32(g.base+MMIO_STATUS)&VIRTIO_STATUS_FEATURES_OK)) return -1;

    if (vq_init_legacy(0)!=0) return -1;
    w32(g.base+MMIO_STATUS,r32(g.base+MMIO_STATUS)|VIRTIO_STATUS_DRIVER_OK);

    static struct virtio_gpu_ctrl_hdr           Q1;
    static struct virtio_gpu_resp_display_info  A1;
    Q1.type=VIRTIO_GPU_CMD_GET_DISPLAY_INFO; bzero(&A1,sizeof A1);
    if (submit(&Q1,sizeof Q1,&A1,sizeof A1)!=0) return -1;

    uint32_t W = A1.pmodes[0].r.width  ? A1.pmodes[0].r.width  : 800;
    uint32_t H = A1.pmodes[0].r.height ? A1.pmodes[0].r.height : 600;

    static uint32_t framebuffer[1920*1080];
    g.width=W; g.height=H; g.pitch=W*4; g.fb=framebuffer; g.res_id=1;

    struct virtio_gpu_resource_create_2d c2d={0};
    c2d.hdr.type=VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    c2d.resource_id=g.res_id; c2d.format=VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
    c2d.width=W; c2d.height=H;
    struct virtio_gpu_ctrl_hdr R0;
    if (submit(&c2d,sizeof c2d,&R0,sizeof R0)!=0 || R0.type!=VIRTIO_GPU_RESP_OK_NODATA) return -1;

    struct { struct virtio_gpu_resource_attach_backing req; struct virtio_gpu_mem_entry e; } __attribute__((packed)) ab={0};
    ab.req.hdr.type=VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING; ab.req.resource_id=g.res_id; ab.req.nr_entries=1;
    ab.e.addr=(uint64_t)VIRT_TO_PHYS(g.fb); ab.e.length=g.pitch*g.height;
    struct virtio_gpu_ctrl_hdr R1;
    if (submit(&ab,sizeof ab,&R1,sizeof R1)!=0 || R1.type!=VIRTIO_GPU_RESP_OK_NODATA) return -1;

    struct virtio_gpu_set_scanout set={0};
    set.hdr.type=VIRTIO_GPU_CMD_SET_SCANOUT; set.r.x=0; set.r.y=0; set.r.width=W; set.r.height=H; set.scanout_id=0; set.resource_id=g.res_id;
    struct virtio_gpu_ctrl_hdr R2;
    if (submit(&set,sizeof set,&R2,sizeof R2)!=0 || R2.type!=VIRTIO_GPU_RESP_OK_NODATA) return -1;

    kprintf("[gfx/virtio-legacy] ready %ux%u pitch=%u\n",W,H,g.pitch);
    return 0;
}
static uint32_t* get_fb(void){ return g.fb; }
static uint32_t  get_w(void){ return g.width; }
static uint32_t  get_h(void){ return g.height; }
static uint32_t  get_pitch(void){ return g.pitch; }

static int present(const struct gfx_rect* r){
    struct virtio_gpu_ctrl_hdr resp;
    struct virtio_gpu_transfer_to_host_2d xfer={0};
    struct virtio_gpu_resource_flush      fl={0};

    uint32_t x=0,y=0,w=g.width,h=g.height;
    if (r){ x=r->x; y=r->y; w=r->w; h=r->h;
            if (x>=g.width||y>=g.height) return 0;
            if (x+w>g.width) w=g.width-x; if (y+h>g.height) h=g.height-y; }

    xfer.hdr.type=VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    xfer.r.x=x; xfer.r.y=y; xfer.r.width=w; xfer.r.height=h;
    xfer.offset = (uint64_t)(y*(uint64_t)g.pitch + x*4u); /* byte offset：y*pitch + x*4 */
    xfer.resource_id=g.res_id;
    if (submit(&xfer,sizeof xfer,&resp,sizeof resp)!=0 || resp.type!=VIRTIO_GPU_RESP_OK_NODATA) return -1;

    fl.hdr.type=VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    fl.r.x=x; fl.r.y=y; fl.r.width=w; fl.r.height=h; fl.resource_id=g.res_id;
    if (submit(&fl,sizeof fl,&resp,sizeof resp)!=0 || resp.type!=VIRTIO_GPU_RESP_OK_NODATA) return -1;

    return 0;
}

static const struct gfx_driver_ops ops = {
    .name = "virtio-gpu-legacy",
    .probe = drv_probe,
    .init  = drv_init,
    .get_fb = get_fb,
    .get_w  = get_w,
    .get_h  = get_h,
    .get_pitch = get_pitch,
    .present = present,
};
void gfx_register_virtio_gpu_legacy(void){ gfx_register_driver(&ops); }
