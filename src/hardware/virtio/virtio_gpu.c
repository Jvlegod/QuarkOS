// this file temp not used
// 示范文件, 存留对照修改

// legacy virtio-mmio: version == 1
#include "virtio.h"
#include "virtio_gpu.h"
#include "kprintf.h"
#include "ktypes.h"

static volatile uint8_t* probe_gpu_base(void)
{
    const uintptr_t START  = (uintptr_t)VIRTIO_MMIO_BASE;
    const uintptr_t STRIDE = 0x1000u;
    const uintptr_t SLOTS  = 128u;

    for (uintptr_t i=0;i<SLOTS;i++){
        volatile uint8_t* b = (volatile uint8_t*)(START + i*STRIDE);
        uint32_t magic = r32(b + MMIO_MAGIC);
        if (magic != MMIO_MAGIC_HEADER) continue; /* "virt" */
        uint32_t ver = r32(b + MMIO_VERSION);
        uint32_t did = r32(b + MMIO_DEVICE_ID);
        kprintf("[virtio-mmio] slot=%u base=0x%lx ver=%u did=%u\r\n",
                (unsigned)i, (unsigned long)(START + i*STRIDE), ver, did);
        if ((ver==1) && did == VIRTIO_ID_GPU) return b;
    }
    return NULL;
}

static int vq_init_legacy(uint16_t qsel)
{
    uintptr_t base    = (uintptr_t)g_vring_area;
    uintptr_t desc_va = base + VRING_OFF_DESC;
    uintptr_t avail_va= base + VRING_OFF_AVAIL;
    uintptr_t used_va = base + VRING_OFF_USED;

    g.desc  = (struct virtq_desc *)desc_va;
    g.avail = (struct virtq_avail*)avail_va;
    g.used  = (struct virtq_used *)used_va;

    bzero(g.desc,  VRING_DESC_SIZE);
    bzero(g.avail, VRING_AVAIL_SIZE);
    bzero(g.used,  VRING_USED_SIZE);
    g.avail_idx = g.last_used_idx = 0;

    w32(g.base + MMIO_GUEST_PAGE_SIZE, 4096);
    w32(g.base + MMIO_QUEUE_SEL, qsel);
    uint32_t qmax = r32(g.base + MMIO_QUEUE_NUM_MAX);
    if (qmax == 0 || QUEUE_SIZE > qmax) return -1;
    w32(g.base + MMIO_QUEUE_NUM, QUEUE_SIZE);

    w32(g.base + MMIO_QUEUE_ALIGN, VRING_ALIGN);

    uintptr_t pfn = (uintptr_t)VIRT_TO_PHYS((void*)base) >> 12;
    w32(g.base + MMIO_QUEUE_PFN, (uint32_t)pfn);

    kprintf("[vgpu] legacy q%u ready: align=%u pfn=0x%lx desc=%lx avail=%lx used=%lx\n",
            qsel, VRING_ALIGN, (unsigned long)pfn,
            (unsigned long)VIRT_TO_PHYS((void*)desc_va),
            (unsigned long)VIRT_TO_PHYS((void*)avail_va),
            (unsigned long)VIRT_TO_PHYS((void*)used_va));
    return 0;
}

static int submit_cmd(void* req, uint32_t req_len, void* resp, uint32_t resp_len)
{
    g.desc[0].addr  = (uint64_t)VIRT_TO_PHYS(req);
    g.desc[0].len   = req_len;
    g.desc[0].flags = VIRTQ_DESC_F_NEXT;
    g.desc[0].next  = 1;

    g.desc[1].addr  = (uint64_t)VIRT_TO_PHYS(resp);
    g.desc[1].len   = resp_len;
    g.desc[1].flags = VIRTQ_DESC_F_WRITE;
    g.desc[1].next  = 0;

    /* fullfill avail */
    uint16_t slot = g.avail_idx % QUEUE_SIZE;
    WRITE_ONCE(g.avail->ring[slot], 0);
    cpu_wmb();
    WRITE_ONCE(g.avail->idx, (uint16_t)(g.avail_idx + 1));
    g.avail_idx++;
    cpu_wmb();

    /* notify queue 0 */
    w32(g.base + MMIO_QUEUE_NOTIFY, 0);

    uint64_t spin=0;
    const uint64_t SPIN_MAX=((uint64_t)1<<22), PRINT_EACH=((uint64_t)1<<18);
    while (g.last_used_idx == READ_ONCE(g.used->idx)) {
        cpu_rmb();
        if ((++spin % PRINT_EACH) == 0) {
            kprintf("[vgpu][poll] used.idx=%u last=%u istat=0x%x\n",
                    (unsigned)READ_ONCE(g.used->idx),
                    (unsigned)g.last_used_idx,
                    r32(g.base + MMIO_INTERRUPT_STATUS));
        }
        if (spin == SPIN_MAX) {
            kprintf("[vgpu] submit timeout: req_pa=%lx len=%u resp_pa=%lx len=%u\n",
                    (unsigned long)g.desc[0].addr, g.desc[0].len,
                    (unsigned long)g.desc[1].addr, g.desc[1].len);
            kprintf("[vgpu] vring PA: base=%lx desc=%lx avail=%lx used=%lx\n",
                    (unsigned long)VIRT_TO_PHYS(g_vring_area),
                    (unsigned long)VIRT_TO_PHYS(g.desc),
                    (unsigned long)VIRT_TO_PHYS(g.avail),
                    (unsigned long)VIRT_TO_PHYS(g.used));
            return -1;
        }
    }
    g.last_used_idx++;
    return 0;
}

/* ===== public API ===== */
int virtio_gpu_init(void)
{
    kprintf("[vgpu] init (legacy, version=1) enter\n");

    g.base = probe_gpu_base();
    if (!g.base) { kprintf("[vgpu] no virtio-gpu (ver=1) found\n"); return -1; }

    g.version = r32(g.base + MMIO_VERSION);
    kprintf("[vgpu] mmio version=%u\n", g.version);
    if (g.version != 1) { kprintf("[vgpu] not legacy device\n"); return -1; }

    w32(g.base + MMIO_STATUS, 0);
    w32(g.base + MMIO_STATUS, VIRTIO_STATUS_ACK);
    w32(g.base + MMIO_STATUS, r32(g.base + MMIO_STATUS) | VIRTIO_STATUS_DRIVER);

    w32(g.base + MMIO_DEVICE_FEATURES_SEL, 0); (void)r32(g.base + MMIO_DEVICE_FEATURES);
    w32(g.base + MMIO_DEVICE_FEATURES_SEL, 1); (void)r32(g.base + MMIO_DEVICE_FEATURES);
    w32(g.base + MMIO_DRIVER_FEATURES_SEL, 0); w32(g.base + MMIO_DRIVER_FEATURES, 0);
    w32(g.base + MMIO_DRIVER_FEATURES_SEL, 1); w32(g.base + MMIO_DRIVER_FEATURES, 0);
    w32(g.base + MMIO_STATUS, r32(g.base + MMIO_STATUS) | VIRTIO_STATUS_FEATURES_OK);

    if (!(r32(g.base + MMIO_STATUS) & VIRTIO_STATUS_FEATURES_OK)) {
        kprintf("[vgpu] FEATURES_OK not accepted (legacy)\n");
        return -1;
    }

    if (vq_init_legacy(0) != 0) { kprintf("[vgpu] vq_init_legacy failed\n"); return -1; }

    /* DRIVER_OK */
    w32(g.base + MMIO_STATUS, r32(g.base + MMIO_STATUS) | VIRTIO_STATUS_DRIVER_OK);

    static uint32_t framebuffer[VGPU_WIDTH * VGPU_HEIGHT] __attribute__((aligned(64)));
    g.fb    = framebuffer;
    g.pitch = VGPU_WIDTH * 4;
    g.width = VGPU_WIDTH;
    g.height= VGPU_HEIGHT;
    g.res_id= 1;

    /* ---- 1) GET_DISPLAY_INFO ---- */
    static struct virtio_gpu_ctrl_hdr           S_REQ;
    static struct virtio_gpu_resp_display_info  S_RSP;
    bzero(&S_RSP, sizeof S_RSP);
    S_REQ.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    if (submit_cmd(&S_REQ, sizeof S_REQ, &S_RSP, sizeof S_RSP) != 0) {
        kprintf("[vgpu] GET_DISPLAY_INFO timeout\n"); return -1;
    }
    if (S_RSP.hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO) {
        kprintf("[vgpu] GET_DISPLAY_INFO bad resp=0x%x\n", S_RSP.hdr.type); return -1;
    }
    kprintf("[vgpu] head0 enabled=%u host=%ux%u\n",
            S_RSP.pmodes[0].enabled, S_RSP.pmodes[0].r.width, S_RSP.pmodes[0].r.height);

    /* ---- 2) RESOURCE_CREATE_2D ---- */
    struct virtio_gpu_resource_create_2d c2d = {0};
    c2d.hdr.type    = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    c2d.resource_id = g.res_id;
    c2d.format      = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
    c2d.width       = g.width;
    c2d.height      = g.height;
    struct virtio_gpu_ctrl_hdr resp0;
    if (submit_cmd(&c2d, sizeof c2d, &resp0, sizeof resp0) != 0 || resp0.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("[vgpu] CREATE_2D resp=0x%x\n", resp0.type); return -1;
    }

    /* ---- 3) ATTACH_BACKING ---- */
    struct {
        struct virtio_gpu_resource_attach_backing req;
        struct virtio_gpu_mem_entry entry;
    } __attribute__((packed)) ab = {0};
    ab.req.hdr.type    = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    ab.req.resource_id = g.res_id;
    ab.req.nr_entries  = 1;
    ab.entry.addr      = (uint64_t)VIRT_TO_PHYS(g.fb);
    ab.entry.length    = g.pitch * g.height;
    struct virtio_gpu_ctrl_hdr resp1;
    if (submit_cmd(&ab, sizeof ab, &resp1, sizeof resp1) != 0 || resp1.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("[vgpu] ATTACH_BACKING resp=0x%x\n", resp1.type); return -1;
    }

    /* ---- 4) SET_SCANOUT ---- */
    struct virtio_gpu_set_scanout set = {0};
    set.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    set.r.x=0; set.r.y=0; set.r.width=g.width; set.r.height=g.height;
    set.scanout_id  = 0;
    set.resource_id = g.res_id;
    struct virtio_gpu_ctrl_hdr resp2;
    if (submit_cmd(&set, sizeof set, &resp2, sizeof resp2) != 0 || resp2.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("[vgpu] SET_SCANOUT resp=0x%x\n", resp2.type); return -1;
    }

    kprintf("[vgpu] init OK (legacy) %ux%u\n", g.width, g.height);
    return 0;
}

// demo
void virtio_gpu_present_demo(void)
{
    if (!g.fb) return;

    for (uint32_t y=0; y<g.height; ++y){
        for (uint32_t x=0; x<g.width; ++x){
            uint8_t r = (uint8_t)((x * 255) / (g.width  ? g.width  : 1));
            uint8_t gg= (uint8_t)((y * 255) / (g.height ? g.height : 1));
            uint8_t b = (uint8_t)((x+y) & 0xFF);
            g.fb[y*(g.pitch/4) + x] = BGRA(b, gg, r);
        }
    }

    struct virtio_gpu_ctrl_hdr resp;

    struct virtio_gpu_transfer_to_host_2d xfer = {0};
    xfer.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    xfer.r.x=0; xfer.r.y=0; xfer.r.width=g.width; xfer.r.height=g.height;
    xfer.offset=0; xfer.resource_id=g.res_id;
    if (submit_cmd(&xfer, sizeof xfer, &resp, sizeof resp) != 0 || resp.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("[vgpu] TRANSFER resp=0x%x\n", resp.type); return;
    }

    struct virtio_gpu_resource_flush fl = {0};
    fl.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    fl.r.x=0; fl.r.y=0; fl.r.width=g.width; fl.r.height=g.height;
    fl.resource_id = g.res_id;
    if (submit_cmd(&fl, sizeof fl, &resp, sizeof resp) != 0 || resp.type != VIRTIO_GPU_RESP_OK_NODATA) {
        kprintf("[vgpu] FLUSH resp=0x%x\n", resp.type); return;
    }

    kprintf("[vgpu] frame presented\n");
}
