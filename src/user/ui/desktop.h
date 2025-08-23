#ifndef __DESKTOP_H__
#define __DESKTOP_H__

#include "ktypes.h"
#include "gfx.h"
#include "virtio_input.h"
#include "virtio_keyboard.h"
#include "virtio_tablet.h"

#define DESKTOP_MAX_WINS 16
#define DESKTOP_TASKBAR_H 28

#define START_W   180
#define START_ROW 20
#define START_MAX 16

typedef struct {
    uint32_t x, y, w, h;
    uint32_t color_body;
    uint32_t color_title;
    const char* title;
} desk_window_t;

typedef void (*desk_button_cb)(int win, void* user);

typedef void (*desk_menu_cb)(void* user);

enum {
    DESK_BTN_LEFT  = 1,
    DESK_BTN_RIGHT = 2,
    DESK_BTN_MID   = 4,
};

typedef struct {
    const char* label;
    desk_menu_cb cb;
    void* user; 
} start_item_t;

int  desktop_init(void);

void desktop_redraw_full(void);

void desktop_cursor_move(uint32_t x, uint32_t y);

void desktop_show_init(void);

// poll
void desktop_poll_cursor(virtio_tablet_t* g_tab);
void desktop_poll_keyboard(virtio_kbd_t* g_kbd);

// public api
void desktop_pointer_abs(uint32_t x, uint32_t y, unsigned buttons);
void desktop_pointer_rel(int dx, int dy, int wheel, unsigned buttons);
int  desktop_add_window(const desk_window_t* win);
void desktop_close_window(int idx);
int  desktop_add_button(int win, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                        const char* label, uint32_t color, desk_button_cb cb, void* user);
void desktop_button_set_visible(int btn_id, int visible);
int  desktop_startmenu_add(const char* label, desk_menu_cb cb, void* user);
void desktop_startmenu_clear(void);

#endif