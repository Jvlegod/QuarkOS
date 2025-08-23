// virtio_tablet.c
#include "virtio_tablet.h"


static void _tablet_evt(void* user, uint16_t type, uint16_t code, int32_t value)
{
    virtio_tablet_t* t = (virtio_tablet_t*)user;

    switch (type) {
    case EV_ABS:
        if (code == ABS_X) {
            t->x = value;
            if (!t->calib_inited){ t->minx=t->maxx=value; } 
            else { if (value<t->minx) t->minx=value; if (value>t->maxx) t->maxx=value; }
        } else if (code == ABS_Y) {
            t->y = value;
            if (!t->calib_inited){ t->miny=t->maxy=value; } 
            else { if (value<t->miny) t->miny=value; if (value>t->maxy) t->maxy=value; }
        }
        break;

    case EV_KEY:
        if (code == BTN_LEFT)   { if (value) t->buttons |= 1u; else t->buttons &= ~1u; }
        if (code == BTN_RIGHT)  { if (value) t->buttons |= 2u; else t->buttons &= ~2u; }
        if (code == BTN_MIDDLE) { if (value) t->buttons |= 4u; else t->buttons &= ~4u; }
        break;

    case EV_SYN:
        if (code == SYN_REPORT) {
            t->dirty = 1;
            if (!t->calib_inited) t->calib_inited = 1;
        }
        break;

    default: break;
    }

    if (type == EV_ABS && t->on_abs) t->on_abs(t->user, code, value);
    else if (type == EV_KEY && t->on_btn) t->on_btn(t->user, code, value != 0);
    else if (type == EV_SYN && code == SYN_REPORT && t->on_syn) t->on_syn(t->user);
}

int virtio_tablet_init_static(virtio_tablet_t* t, uintptr_t mmio_base,
                              virtio_input_storage_t* storage,
                              tablet_on_abs_cb on_abs,
                              tablet_on_btn_cb on_btn,
                              tablet_on_syn_cb on_syn,
                              void* user)
{
    if (!t || !storage) return -1;
    t->on_abs = on_abs;
    t->on_btn = on_btn;
    t->on_syn = on_syn;
    t->user   = user;

    t->x = t->y = 0;
    t->buttons = 0;
    t->dirty = 0;
    t->calib_inited = 0;
    t->minx = t->maxx = 0;
    t->miny = t->maxy = 0;

    return virtio_input_init_legacy_static(&t->in, mmio_base, _tablet_evt, t, storage);
}

int virtio_tablet_read_raw(virtio_tablet_t* t, int* x, int* y, unsigned* buttons)
{
    if (!t || !t->dirty) return 0;
    if (x) *x = t->x;
    if (y) *y = t->y;
    if (buttons) *buttons = t->buttons;
    t->dirty = 0;  // 消费一帧
    return 1;
}

int virtio_tablet_read_scaled(virtio_tablet_t* t, int sw, int sh,
                              int* sx, int* sy, unsigned* buttons)
{
    int rx, ry, x, y;
    unsigned b;
    if (!virtio_tablet_read_raw(t, &x, &y, &b)) return 0;

    int minx = t->minx, maxx = t->maxx, miny = t->miny, maxy = t->maxy;
    rx = maxx - minx; if (rx <= 0) rx = 1;
    ry = maxy - miny; if (ry <= 0) ry = 1;

    int64_t nx = x - minx; if (nx < 0) nx = 0; if (nx > rx) nx = rx;
    int64_t ny = y - miny; if (ny < 0) ny = 0; if (ny > ry) ny = ry;

    int tx = (int)(nx * (sw - 1) / rx);
    int ty = (int)(ny * (sh - 1) / ry);

    if (sx) *sx = tx;
    if (sy) *sy = ty;
    if (buttons) *buttons = b;
    return 1;
}
