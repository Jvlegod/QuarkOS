#include "desktop.h"
#include "gfx.h"
#include "kprintf.h"
#include "ui_config.h"
#include "cstdlib.h"

static inline void draw_char6x8(uint32_t x, uint32_t y, char ch, uint32_t color) {
    if (ch < 32 || ch > 126) return;
    const uint8_t* col = g_font6x8[ch - 32];
    for (int cx = 0; cx < 6; ++ cx){
        uint8_t bits = col[cx];
        for (int cy = 0; cy < 8; ++ cy){
            if (bits & (1u << cy)) gfx_put_pixel(x+cx, y+cy, color);
        }
    }
}
static void draw_text(uint32_t x, uint32_t y, const char* s, uint32_t color) {
    if (!s) return;
    while (*s){
        draw_char6x8(x, y, *s++, color);
        x += 6;
    }
}

static desk_window_t g_wins[DESKTOP_MAX_WINS];
static int g_nwins = 0;
static uint32_t g_mouse_x = 16, g_mouse_y = 16;

static void draw_mouse_cursor(uint32_t x, uint32_t y) {
    static const uint16_t mouse[19] = {
        0b1,
        0b11,
        0b111,
        0b1111,
        0b11111,
        0b111111,
        0b1111111,
        0b11111110,
        0b111111100,
        0b111111000,
        0b111110000,
        0b111100000,
        0b111000000,
        0b110000000,
        0b110000000,
        0b100000000,
        0b100000000,
        0b000000000,
        0b000000000,
    };
    uint32_t W = gfx_width(), H=gfx_height();
    uint32_t P = gfx_pitch_bytes()/4;
    uint32_t *fb = gfx_fb(); if(!fb) return;
    for (uint32_t r=0; r<19; ++r){
        uint16_t bits = (r < 19) ? mouse[r] : 0;
        for (uint32_t c=0; c<12; ++c){
            if (bits & (1u<<c)){
                uint32_t xx = x + c, yy = y + r;
                if (xx<W && yy<H){
                    uint32_t *px = fb + yy*P + xx;
                    *px ^= 0x00FFFFFFu;
                }
            }
        }
    }
}

static inline uint32_t C(uint8_t b,uint8_t g,uint8_t r){ return (uint32_t)b | ((uint32_t)g<<8) | ((uint32_t)r<<16) | 0xFF000000u; }

static void draw_wallpaper(void){
    uint32_t W=gfx_width(), H=gfx_height();
    for (uint32_t y=0; y<H; ++y){
        uint8_t r = (uint8_t)(20 + (y * 40 / (H?H:1)));
        uint8_t g = (uint8_t)(30 + (y * 70 / (H?H:1)));
        uint8_t b = (uint8_t)(60 + (y * 160/ (H?H:1)));
        gfx_hline(0, y, W, C(b,g,r));
    }
}
static void draw_taskbar(void){
    uint32_t W=gfx_width();
    gfx_fill_rect(0, 0, W, DESKTOP_TASKBAR_H, C(0x90,0x90,0x30));
    gfx_hline(0, DESKTOP_TASKBAR_H - 1, W, C(0x00,0x00,0x00));

    gfx_fill_rect(6, 4, 54, DESKTOP_TASKBAR_H - 8, C(0x40,0x80,0xF0));
    draw_text(12, 9, "Start", C(0x00,0x00,0x00));
}

static void draw_window(const desk_window_t* w){
    if (!w) return;
    uint32_t bx = w->x, by = w->y + DESKTOP_TASKBAR_H, bw = w->w, bh = w->h;
    /* border */
    gfx_fill_rect(bx - 1, by - 21, bw + 2, bh + 22, C(0x10,0x10,0x10));
    /* title */
    gfx_fill_rect(bx, by - 20, bw, 18, w->color_title);
    if (w->title) draw_text(bx + 6, by - 18, w->title, C(0x00,0x00,0x00));
    /* content */
    gfx_fill_rect(bx, by, bw, bh, w->color_body);
}

static void redraw_everything(void) {
    draw_wallpaper();
    draw_taskbar();
    for (int i = 0; i < g_nwins; i ++) {
        draw_window(&g_wins[i]);
    }
}


int desktop_init(void) {
    if (gfx_init()!=0) return -1;

    redraw_everything();
    gfx_present(NULL);

    g_mouse_x = 16; g_mouse_y = 40;
    draw_mouse_cursor(g_mouse_x, g_mouse_y);
    struct gfx_rect r = { g_mouse_x, g_mouse_y, 16, 22 };
    gfx_present(&r);
    return 0;
}

int desktop_add_window(const desk_window_t* w) {
    if (!w || g_nwins >= DESKTOP_MAX_WINS) return -1;
    g_wins[g_nwins] = *w;
    int idx = g_nwins++;
    redraw_everything();
    gfx_present(NULL);
    return idx;
}

void desktop_redraw_full(void) {
    redraw_everything();
    gfx_present(NULL);

    draw_mouse_cursor(g_mouse_x, g_mouse_y);
    struct gfx_rect r = { g_mouse_x, g_mouse_y, 16, 22 };
    gfx_present(&r);
}

void desktop_cursor_move(uint32_t x, uint32_t y) {
    g_mouse_x = x; g_mouse_y = y;
    redraw_everything();
    draw_mouse_cursor(g_mouse_x, g_mouse_y);
    gfx_present(NULL);
}

void desktop_show_init(void) {
    desk_window_t a = { .x=40,  .y=60,  .w=280, .h=160,
                        .color_body=C(0xF0,0xE8,0xD8), .color_title=C(0xA0,0xD0,0xFF),
                        .title="Welcome" };
    desk_window_t b = { .x=140, .y=120, .w=320, .h=200,
                        .color_body=C(0xF8,0xF0,0xF0), .color_title=C(0xFF,0xB0,0x70),
                        .title="Settings" };
    desk_window_t c = { .x=gfx_width()-220, .y=90, .w=200, .h=140,
                        .color_body=C(0xE8,0xFF,0xF0), .color_title=C(0x90,0xF0,0xC0),
                        .title="About" };

    desktop_add_window(&a);
    desktop_add_window(&b);
    desktop_add_window(&c);
}


// relative to virtio input
// void input_on_pointer(uint32_t x, uint32_t y, uint32_t buttons){
//     (void)buttons;
//     desktop_cursor_move(x, y);
// }