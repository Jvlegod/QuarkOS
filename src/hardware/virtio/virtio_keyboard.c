// virtio_keyboard.c
#include "virtio_keyboard.h"

static inline void rb_push(virtio_kbd_t* kbd, char c){
    uint16_t nxt = (uint16_t)((kbd->head + 1) % KBD_RB_CAP);
    if (nxt != kbd->tail){ kbd->rb[kbd->head] = c; kbd->head = nxt; }
}
int virtio_kbd_getchar(virtio_kbd_t* kbd){
    if (!kbd) return -1;
    if (kbd->head == kbd->tail) return -1;
    char c = kbd->rb[kbd->tail];
    kbd->tail = (uint16_t)((kbd->tail + 1) % KBD_RB_CAP);
    return (int)(unsigned char)c;
}

static char kc_to_ascii(uint16_t code, int shift){
    if (code == KEY_SPACE)     return ' ';
    if (code == KEY_ENTER)     return '\n';
    if (code == KEY_BACKSPACE) return '\b';
    if (code >= KEY_A && code <= KEY_Z) {
        return (char)((shift ? 'A':'a') + (code - KEY_A));
    }
    switch (code){
        case KEY_1: return shift ? '!' : '1';
        case KEY_2: return shift ? '@' : '2';
        case KEY_3: return shift ? '#' : '3';
        case KEY_4: return shift ? '$' : '4';
        case KEY_5: return shift ? '%' : '5';
        case KEY_6: return shift ? '^' : '6';
        case KEY_7: return shift ? '&' : '7';
        case KEY_8: return shift ? '*' : '8';
        case KEY_9: return shift ? '(' : '9';
        case KEY_0: return shift ? ')' : '0';
        default: return 0;
    }
}

static void _kbd_evt(void* user, uint16_t type, uint16_t code, int32_t value)
{
    virtio_kbd_t* kbd = (virtio_kbd_t*)user;
    if (type != EV_KEY) goto out_cb;

    if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) {
        kbd->shift = (value != 0);
        goto out_cb;
    }

    if (value == 1 || value == 2) { // press/repeat
        char ch = kc_to_ascii(code, kbd->shift);
        if (ch) rb_push(kbd, ch);
    }

out_cb:
    if (kbd->on_key) kbd->on_key(kbd->user, code, (int)value);
}

int virtio_kbd_init_static(virtio_kbd_t* kbd, uintptr_t mmio_base,
                           virtio_input_storage_t* storage,
                           kbd_on_key_cb cb, void* user)
{
    if (!kbd || !storage) return -1;
    kbd->on_key = cb;
    kbd->user   = user;
    kbd->head = kbd->tail = 0;
    kbd->shift = 0;
    return virtio_input_init_legacy_static(&kbd->in, mmio_base, _kbd_evt, kbd, storage);
}
