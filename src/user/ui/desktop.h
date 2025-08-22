#ifndef __DESKTOP_H__
#define __DESKTOP_H__

#include "ktypes.h"
#include "gfx.h"
#include "virtio_input.h"

typedef struct {
    uint32_t x, y, w, h;
    uint32_t color_body;
    uint32_t color_title;
    const char* title;
} desk_window_t;

#define DESKTOP_MAX_WINS 16
#define DESKTOP_TASKBAR_H 28

int  desktop_init(void);

int  desktop_add_window(const desk_window_t* win);

void desktop_redraw_full(void);

void desktop_cursor_move(uint32_t x, uint32_t y);

void desktop_show_init(void);

#endif