// #ifndef __VIRTIO_INPUT_H__
// #define __VIRTIO_INPUT_H__

// #include "ktypes.h"
// #include "virtio.h"

// #define BTN_LEFT_MASK   (1u << 0)

// static void bzero(void* p,size_t n){ unsigned char* d=p; while(n--)*d++=0; }
// static void bcopy(const void* s,void* d,size_t n){ const unsigned char* a=s; unsigned char* b=d; while(n--)*b++=*a++; }

// /* ---- 常量 / 协议 ---- */
// enum { VIRTIO_ID_INPUT = 18 };

// /* evdev 事件 */
// struct virtio_input_event { uint16_t type, code; uint32_t value; } __attribute__((packed));
// #define EV_SYN 0x00
// #define EV_KEY 0x01
// #define EV_REL 0x02
// #define EV_ABS 0x03
// /* 常用码 */
// #define BTN_LEFT  0x110
// #define REL_X     0x00
// #define REL_Y     0x01
// #define ABS_X     0x00
// #define ABS_Y     0x01

// /* config 空间（按规范） */
// struct virtio_input_absinfo { uint32_t min, max, fuzz, flat, res; } __attribute__((packed));
// struct virtio_input_config {
//     uint8_t  select;   /* 见下面 CFG_* */
//     uint8_t  subsel;   /* 子选择：比如 EV 类型或 ABS 轴号 */
//     uint8_t  size;     /* 有效 payload 字节数 */
//     uint8_t  _rsv[5];
//     union {
//         char                       string[128]; /* 读名字/序列等用 */
//         struct virtio_input_absinfo abs;
//         uint8_t                    bits[128];   /* 读能力位时当字节数组用 */
//     } u;
// } __attribute__((packed));

// /* config select 值（按 Virtio 1.1+ 规范） */
// #define VIRTIO_INPUT_CFG_UNSET      0x00
// #define VIRTIO_INPUT_CFG_ID_NAME    0x01
// #define VIRTIO_INPUT_CFG_ID_SERIAL  0x02
// #define VIRTIO_INPUT_CFG_ID_DEVIDS  0x03
// #define VIRTIO_INPUT_CFG_PROP_BITS  0x10
// #define VIRTIO_INPUT_CFG_EV_BITS    0x11
// #define VIRTIO_INPUT_CFG_ABS_INFO   0x12

// /* ---- 一个设备（keyboard / tablet / mouse） ---- */
// #define VR_ALIGN 4u
// #define DESC_SZ   (16*QUEUE_SIZE)
// #define AVAIL_SZ  (2 + 2 + 2*QUEUE_SIZE)
// #define USED_SZ   (2 + 2 + 8*QUEUE_SIZE)
// #define ALIGN_UP(x,a) (((x)+((a)-1)) & ~((a)-1))
// #define OFF_DESC  0
// #define OFF_AVAIL ALIGN_UP(OFF_DESC + DESC_SZ, 2)
// #define OFF_USED  ALIGN_UP(OFF_AVAIL + AVAIL_SZ, VR_ALIGN)
// #define AREA_SZ   (OFF_USED + USED_SZ)

// typedef struct {
//     volatile uint8_t* base;
//     uint32_t version; /* 1=legacy, >=2=modern */

//     /* event queue */
//     uint8_t area_event[AREA_SZ] __attribute__((aligned(4096)));
//     struct virtq_desc  *desc;
//     struct virtq_avail *avail;
//     struct virtq_used  *used;
//     uint16_t avail_idx, last_used;

//     struct virtio_input_event evbuf[QUEUE_SIZE];

//     /* 能力与标识 */
//     char     name[64];
//     uint8_t  has_ev_key, has_ev_abs, has_ev_rel;

//     /* 指针状态 / 轴范围 */
//     uint32_t abs_x_max, abs_y_max;
//     uint32_t cur_x, cur_y;
//     uint32_t buttons;

//     int alive;
// } vinp_dev_t;

// /* 最多 8 台输入设备 */
// #define MAX_INPUT_DEVS 8
// static vinp_dev_t g_devs[MAX_INPUT_DEVS];
// static int g_ndevs = 0;

// int  input_init(void);
// void input_poll(void);

// void input_on_pointer(uint32_t x, uint32_t y, uint32_t buttons);
// void input_on_key(uint16_t code, int pressed);

// #endif