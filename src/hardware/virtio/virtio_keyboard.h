// virtio_keyboard.h
#ifndef __VIRTIO_KEYBOARD_H__
#define __VIRTIO_KEYBOARD_H__
#include "virtio_input.h"

#ifndef EV_KEY
#define EV_KEY 0x01
#endif
#ifndef KEY_LEFTSHIFT
#define KEY_LEFTSHIFT  42
#define KEY_RIGHTSHIFT 54
#define KEY_ENTER      28
#define KEY_SPACE      57
#define KEY_BACKSPACE  14
#define KEY_A          30
#define KEY_Z          44
#define KEY_1          2
#define KEY_2          3
#define KEY_3          4
#define KEY_4          5
#define KEY_5          6
#define KEY_6          7
#define KEY_7          8
#define KEY_8          9
#define KEY_9          10
#define KEY_0          11
#endif

typedef void (*kbd_on_key_cb)(void* user, uint16_t keycode, int pressed); /* 0/1/2 */

typedef struct virtio_kbd {
#ifndef KBD_RB_CAP
#define KBD_RB_CAP 128
#endif
    virtio_input_dev_t in;
    kbd_on_key_cb on_key;
    void* user;

    volatile uint16_t head, tail;
    char rb[KBD_RB_CAP];
    int  shift;
} virtio_kbd_t;

int  virtio_kbd_init_static(virtio_kbd_t* kbd, uintptr_t mmio_base,
                            virtio_input_storage_t* storage,
                            kbd_on_key_cb cb, void* user);

static inline void virtio_kbd_irq(virtio_kbd_t* kbd){ virtio_input_handle_irq(&kbd->in); }
static inline void virtio_kbd_poll(virtio_kbd_t* kbd){ virtio_input_poll(&kbd->in); }

int virtio_kbd_getchar(virtio_kbd_t* kbd);

#endif