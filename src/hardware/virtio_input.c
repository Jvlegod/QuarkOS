// #include "virtio.h"
// #include "virtio_input.h"
// #include "gfx.h"
// #include "kprintf.h"

// // 弱符号 hooks
// __attribute__((weak)) void input_on_pointer(uint32_t x, uint32_t y, uint32_t buttons){ (void)x;(void)y;(void)buttons; }
// __attribute__((weak)) void input_on_key(uint16_t code, int pressed){ (void)code;(void)pressed; }

// static vinp_dev_t g_inp;

// static volatile uint8_t* probe_input_base(void){
//     const uintptr_t START=(uintptr_t)VIRTIO_MMIO_BASE, STRIDE=0x1000, SLOTS=128;
//     for (uintptr_t i=0;i<SLOTS;i++){
//         volatile uint8_t* b=(volatile uint8_t*)(START + i*STRIDE);
//         if (r32(b+MMIO_MAGIC)   != 0x74726976) continue;
//         if (r32(b+MMIO_DEVICE_ID) != VIRTIO_ID_INPUT) continue;
//         return b;
//     }
//     return 0;
// }

// static int vq_init_legacy(vinp_dev_t* d, uint16_t qsel, void* area_base){
//     uintptr_t base=(uintptr_t)area_base;
//     struct virtq_desc  *desc  = (struct virtq_desc *)(base+OFF_DESC);
//     struct virtq_avail *avail = (struct virtq_avail*)(base+OFF_AVAIL);
//     struct virtq_used  *used  = (struct virtq_used *)(base+OFF_USED);
//     bzero(desc, DESC_SZ); bzero(avail, AVAIL_SZ); bzero(used, USED_SZ);

//     w32(d->base+MMIO_GUEST_PAGE_SIZE,4096);
//     w32(d->base+MMIO_QUEUE_SEL,qsel);
//     uint32_t qmax=r32(d->base+MMIO_QUEUE_NUM_MAX);
//     if (qmax==0 || QUEUE_SIZE>qmax) return -1;
//     w32(d->base+MMIO_QUEUE_NUM,QUEUE_SIZE);
//     w32(d->base+MMIO_QUEUE_ALIGN,VR_ALIGN);
//     uintptr_t pfn=(uintptr_t)VIRT_TO_PHYS((void*)base)>>12;
//     w32(d->base+MMIO_QUEUE_PFN,(uint32_t)pfn);

//     if (qsel==0){ d->e_desc=desc; d->e_avail=avail; d->e_used=used; d->e_avail_idx=d->e_last_used=0; }
//     return 0;
// }
// static int vq_init_modern(vinp_dev_t* d, uint16_t qsel){
//     w32(d->base+MMIO_QUEUE_SEL,qsel);
//     uint32_t qmax=r32(d->base+MMIO_QUEUE_NUM_MAX);
//     if (qmax==0 || QUEUE_SIZE>qmax) return -1;
//     w32(d->base+MMIO_QUEUE_NUM,QUEUE_SIZE);

//     uintptr_t base=(uintptr_t)d->area_event;
//     struct virtq_desc  *desc  = (struct virtq_desc *)(base+OFF_DESC);
//     struct virtq_avail *avail = (struct virtq_avail*)(base+OFF_AVAIL);
//     struct virtq_used  *used  = (struct virtq_used *)(base+OFF_USED);
//     bzero(desc, DESC_SZ); bzero(avail, AVAIL_SZ); bzero(used, USED_SZ);

//     wr64(d->base, MMIO_QUEUE_DESC_LOW,  (uint64_t)VIRT_TO_PHYS(desc));
//     wr64(d->base, MMIO_QUEUE_AVAIL_LOW, (uint64_t)VIRT_TO_PHYS(avail));
//     wr64(d->base, MMIO_QUEUE_USED_LOW,  (uint64_t)VIRT_TO_PHYS(used));
//     w32(d->base+MMIO_QUEUE_READY, 1);

//     d->e_desc=desc; d->e_avail=avail; d->e_used=used; d->e_avail_idx=d->e_last_used=0;
//     return 0;
// }
// static void eventq_prime(vinp_dev_t* d){
//     for (uint16_t i=0;i<QUEUE_SIZE;i++){
//         d->e_desc[i].addr  = (uint64_t)VIRT_TO_PHYS(&d->evbuf[i]);
//         d->e_desc[i].len   = sizeof(struct virtio_input_event);
//         d->e_desc[i].flags = VIRTQ_DESC_F_WRITE;
//         d->e_desc[i].next  = 0;

//         uint16_t slot = d->e_avail_idx % QUEUE_SIZE;
//         WRITE_ONCE(d->e_avail->ring[slot], i);
//         cpu_wmb();
//         WRITE_ONCE(d->e_avail->idx, (uint16_t)(d->e_avail_idx+1));
//         d->e_avail_idx++;
//     }
//     cpu_wmb();
//     w32(d->base+MMIO_QUEUE_NOTIFY, 0);
// }

// static void read_abs_range(vinp_dev_t* d){
//     volatile struct virtio_input_config* cfg = (volatile struct virtio_input_config*)(d->base + MMIO_CONFIG);

//     /* X 轴 */
//     ((struct virtio_input_config*)cfg)->select = 0x02; /* VIRTIO_INPUT_CFG_ABS */
//     ((struct virtio_input_config*)cfg)->subsel = ABS_X;
//     uint8_t size = cfg->size;
//     if (size >= sizeof(struct virtio_input_absinfo)){
//         d->abs_x_max = cfg->u.abs.max ? cfg->u.abs.max : 65535u;
//     } else d->abs_x_max = 65535u;

//     /* Y 轴 */
//     ((struct virtio_input_config*)cfg)->select = 0x02;
//     ((struct virtio_input_config*)cfg)->subsel = ABS_Y;
//     size = cfg->size;
//     if (size >= sizeof(struct virtio_input_absinfo)){
//         d->abs_y_max = cfg->u.abs.max ? cfg->u.abs.max : 65535u;
//     } else d->abs_y_max = 65535u;

//     if (d->abs_x_max==0) d->abs_x_max=65535;
//     if (d->abs_y_max==0) d->abs_y_max=65535;
// }

// int input_init(void){
//     bzero(&g_inp, sizeof(g_inp));
//     volatile uint8_t* base = probe_input_base();
//     if (!base){
//         kprintf("[vinp] no virtio-input found\n");
//         return -1;
//     }
//     g_inp.base = base;
//     g_inp.version = r32(base + MMIO_VERSION);

//     /* reset + DRIVER */
//     w32(base+MMIO_STATUS, 0);
//     w32(base+MMIO_STATUS, VIRTIO_STATUS_ACK);
//     w32(base+MMIO_STATUS, r32(base+MMIO_STATUS)|VIRTIO_STATUS_DRIVER);

//     /* features：都置 0（legacy 环境足够） */
//     w32(base+MMIO_DEVICE_FEATURES_SEL,0); (void)r32(base+MMIO_DEVICE_FEATURES);
//     w32(base+MMIO_DEVICE_FEATURES_SEL,1); (void)r32(base+MMIO_DEVICE_FEATURES);
//     w32(base+MMIO_DRIVER_FEATURES_SEL,0); w32(base+MMIO_DRIVER_FEATURES,0);
//     w32(base+MMIO_DRIVER_FEATURES_SEL,1); w32(base+MMIO_DRIVER_FEATURES,0);
//     w32(base+MMIO_STATUS, r32(base+MMIO_STATUS)|VIRTIO_STATUS_FEATURES_OK);
//     if (!(r32(base+MMIO_STATUS)&VIRTIO_STATUS_FEATURES_OK)) return -1;

//     /* 队列 0 = eventq */
//     int ok = (g_inp.version>=2) ? vq_init_modern(&g_inp, 0) : vq_init_legacy(&g_inp, 0, g_inp.area_event);
//     if (ok!=0) return -1;

//     /* DRIVER_OK */
//     w32(base+MMIO_STATUS, r32(base+MMIO_STATUS)|VIRTIO_STATUS_DRIVER_OK);

//     /* prime event queue */
//     eventq_prime(&g_inp);

//     read_abs_range(&g_inp);

//     g_inp.alive = 1;
//     kprintf("[vinp] ready: ver=%u abs_max=(%u,%u)\n", g_inp.version, g_inp.abs_x_max, g_inp.abs_y_max);
//     return 0;
// }

// void input_poll(void){
//     if (!g_inp.alive) return;

//     while (g_inp.e_last_used != READ_ONCE(g_inp.e_used->idx)){
//         uint16_t used_idx = g_inp.e_last_used % QUEUE_SIZE;
//         uint32_t id = g_inp.e_used->ring[used_idx].id; /* desc idx */
//         struct virtio_input_event *e = &g_inp.evbuf[id];

//         if (e->type == EV_ABS){
//             if (e->code == ABS_X){
//                 uint32_t W = gfx_width(); if (!W) W = 800;
//                 uint32_t max = g_inp.abs_x_max ? g_inp.abs_x_max : 65535u;
//                 if (max==0) max=65535u;
//                 uint64_t v = e->value; if (v > max) v = max;
//                 g_inp.cur_x = (uint32_t)(v * (uint64_t)(W-1) / max);
//             } else if (e->code == ABS_Y){
//                 uint32_t H = gfx_height(); if (!H) H = 600;
//                 uint32_t max = g_inp.abs_y_max ? g_inp.abs_y_max : 65535u;
//                 if (max==0) max=65535u;
//                 uint64_t v = e->value; if (v > max) v = max;
//                 g_inp.cur_y = (uint32_t)(v * (uint64_t)(H-1) / max);
//             }
//         } else if (e->type == EV_KEY){
//             if (e->code == BTN_LEFT){
//                 if (e->value) g_inp.buttons |= BTN_LEFT_MASK;
//                 else          g_inp.buttons &= ~BTN_LEFT_MASK;
//                 input_on_key(BTN_LEFT, e->value ? 1 : 0);
//             } else {
//                 input_on_key(e->code, e->value ? 1 : 0);
//             }
//         } else if (e->type == EV_SYN){
//             input_on_pointer(g_inp.cur_x, g_inp.cur_y, g_inp.buttons);
//         }

//         g_inp.e_desc[id].len   = sizeof(struct virtio_input_event);
//         g_inp.e_desc[id].flags = VIRTQ_DESC_F_WRITE;
//         g_inp.e_desc[id].next  = 0;
//         uint16_t slot = g_inp.e_avail_idx % QUEUE_SIZE;
//         WRITE_ONCE(g_inp.e_avail->ring[slot], id);
//         cpu_wmb();
//         WRITE_ONCE(g_inp.e_avail->idx, (uint16_t)(g_inp.e_avail_idx+1));
//         g_inp.e_avail_idx++;
//         cpu_wmb();

//         g_inp.e_last_used++;
//     }
// }
