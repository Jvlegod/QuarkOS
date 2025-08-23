// virtio_tablet.h
#ifndef __VIRTIO_TABLET_H__
#define __VIRTIO_TABLET_H__
#include "virtio_input.h"


#ifndef EV_ABS
#define EV_ABS 0x03
#endif
#ifndef EV_KEY
#define EV_KEY 0x01
#endif
#ifndef EV_SYN
#define EV_SYN 0x00
#endif
#ifndef ABS_X
#define ABS_X 0x00
#define ABS_Y 0x01
#endif
#ifndef SYN_REPORT
#define SYN_REPORT 0
#endif
#ifndef BTN_LEFT
#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112
#endif

typedef void (*tablet_on_abs_cb)(void* user, int code, int value);
typedef void (*tablet_on_btn_cb)(void* user, int code, int pressed);
typedef void (*tablet_on_syn_cb)(void* user);

typedef struct virtio_tablet {
    virtio_input_dev_t in;
    tablet_on_abs_cb on_abs;
    tablet_on_btn_cb on_btn;
    tablet_on_syn_cb on_syn;
    void* user;

    int x, y;
    unsigned buttons;

    int dirty;

    int calib_inited;
    int minx, maxx, miny, maxy;
} virtio_tablet_t;

int  virtio_tablet_init_static(virtio_tablet_t* t, uintptr_t mmio_base,
                               virtio_input_storage_t* storage,
                               tablet_on_abs_cb on_abs,
                               tablet_on_btn_cb on_btn,
                               tablet_on_syn_cb on_syn,
                               void* user);


int  virtio_tablet_read_raw(virtio_tablet_t* t, int* x, int* y, unsigned* buttons);

int  virtio_tablet_read_scaled(virtio_tablet_t* t, int sw, int sh,
                               int* sx, int* sy, unsigned* buttons);

static inline void virtio_tablet_set_fixed_range(virtio_tablet_t* t,
                                                 int minx, int maxx, int miny, int maxy){
    if (!t) return;
    t->minx=minx; t->maxx=maxx; t->miny=miny; t->maxy=maxy; t->calib_inited=1;
}

static inline void virtio_tablet_irq(virtio_tablet_t* t){ virtio_input_handle_irq(&t->in); }
static inline void virtio_tablet_poll(virtio_tablet_t* t){ virtio_input_poll(&t->in); }

#endif
