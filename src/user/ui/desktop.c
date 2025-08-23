#include "desktop.h"
#include "gfx.h"
#include "kprintf.h"
#include "ui_config.h"
#include "cstdlib.h"

/* =========================
 * 工具与通用
 * ========================= */

static inline uint32_t desktop_color (uint8_t b, uint8_t g, uint8_t r) {
    return (uint32_t) b | ((uint32_t) g << 8) | ((uint32_t) r << 16) | 0xFF000000u;
}

static inline void desktop_put_pixel (uint32_t x, uint32_t y, uint32_t c) {
    gfx_hline (x, y, 1, c);
}

/* =========================
 * 字体：6x8 等宽
 * ========================= */

extern const uint8_t g_font6x8[][6]; /* 你的 6x8 字体表需在别处定义 */

static void desktop_draw_char_6x8 (uint32_t x, uint32_t y, char ch, uint32_t color) {
    if (ch < 32 || ch > 126) return;
    const uint8_t * col = g_font6x8[ch - 32];

    for (int cx = 0; cx < 6; ++cx) {
        uint8_t bits = col[cx];
        for (int cy = 0; cy < 8; ++cy) {
            if (bits & (1u << cy)) {
                desktop_put_pixel (x + (uint32_t) cx, y + (uint32_t) cy, color);
            }
        }
    }
}

static void desktop_draw_text_6x8 (uint32_t x, uint32_t y, const char * s, uint32_t color) {
    if (!s) return;
    while (*s) {
        desktop_draw_char_6x8 (x, y, *s++, color);
        x += 6;
    }
}

/* =========================
 * 字体：5x7(only QuarkOS)
 * ========================= */

typedef struct { char ch; uint8_t rows[7]; } desktop_glyph_5x7_t;

static const desktop_glyph_5x7_t DESKTOP_FONT_5X7[] = {
    { 'Q', { 0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101 } },
    { 'u', { 0b00000, 0b00000, 0b10001, 0b10001, 0b10001, 0b10011, 0b01101 } },
    { 'a', { 0b00000, 0b00000, 0b01110, 0b00001, 0b01111, 0b10001, 0b01111 } },
    { 'r', { 0b00000, 0b00000, 0b10110, 0b11001, 0b10000, 0b10000, 0b10000 } },
    { 'k', { 0b10000, 0b10000, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010 } },
    { 'O', { 0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 } },
    { 'S', { 0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110 } },
};

static const desktop_glyph_5x7_t * desktop_find_glyph_5x7 (char c) {
    for (unsigned i = 0; i < sizeof (DESKTOP_FONT_5X7) / sizeof (DESKTOP_FONT_5X7[0]); ++i) {
        if (DESKTOP_FONT_5X7[i].ch == c) return &DESKTOP_FONT_5X7[i];
    }
    return NULL;
}

static void desktop_draw_char_5x7 (int x, int y, char c, int scale, uint32_t color) {
    if (scale < 1) scale = 1;
    const desktop_glyph_5x7_t * g = desktop_find_glyph_5x7 (c);
    if (!g) return;

    for (int ry = 0; ry < 7; ++ry) {
        uint8_t row = g->rows[ry];
        for (int rx = 0; rx < 5; ++rx) {
            if (row & (1u << (4 - rx))) {
                for (int sy = 0; sy < scale; ++sy) {
                    gfx_hline ((uint32_t) (x + rx * scale),
                               (uint32_t) (y + ry * scale + sy),
                               (uint32_t) scale,
                               color);
                }
            }
        }
    }
}

static int desktop_text_width_5x7 (const char * s, int scale, int spacing) {
    int n = 0; while (s[n]) n++;
    int cw = 5 * scale;
    return n ? (n * cw + (n - 1) * spacing) : 0;
}

static int desktop_text_height_5x7 (int scale) {
    return 7 * scale;
}

static void desktop_draw_text_5x7 (int x, int y, const char * s, int scale, int spacing, uint32_t color) {
    int cx = x;
    for (const char * p = s; *p; ++p) {
        if (*p == ' ') { cx += 3 * scale + spacing; continue; }
        desktop_draw_char_5x7 (cx, y, *p, scale, color);
        cx += 5 * scale + spacing;
    }
}

/* =========================
 * 窗口与鼠标
 * ========================= */

static desk_window_t s_wins[DESKTOP_MAX_WINS];
static int           s_nwins   = 0;
static uint32_t      s_mouse_x = 16;
static uint32_t      s_mouse_y = 16;

/* =========================
 * 光标
 * ========================= */

static void desktop_cursor_draw_at (uint32_t x, uint32_t y) {
    static const uint16_t mouse[19] = {
        0b1, 0b11, 0b111, 0b1111, 0b11111, 0b111111, 0b1111111, 0b11111110, 0b111111100,
        0b111111000, 0b111110000, 0b111100000, 0b111000000, 0b110000000, 0b110000000,
        0b100000000, 0b100000000, 0b000000000, 0b000000000,
    };

    uint32_t W = gfx_width ();
    uint32_t H = gfx_height ();
    uint32_t P = gfx_pitch_bytes () / 4;
    uint32_t * fb = gfx_fb ();
    if (!fb) return;

    for (uint32_t r = 0; r < 19; ++r) {
        uint16_t bits = mouse[r];
        for (uint32_t c = 0; c < 12; ++c) {
            if (bits & (1u << c)) {
                uint32_t xx = x + c;
                uint32_t yy = y + r;
                if (xx < W && yy < H) {
                    uint32_t * px = fb + yy * P + xx;
                    *px ^= 0x00FFFFFFu; /* XOR 光标 */
                }
            }
        }
    }
}

static void desktop_cursor_draw (void) {
    desktop_cursor_draw_at (s_mouse_x, s_mouse_y);
}

/* =========================
 * 壁纸与 Logo
 * ========================= */

static void desktop_draw_centered_quarkos (void) {
    const char * title = "QuarkOS";
    const int    SCALE = 20;
    const int    SP    = 2;

    uint32_t W = gfx_width ();
    uint32_t H = gfx_height ();

    int tw = desktop_text_width_5x7 (title, SCALE, SP);
    int th = desktop_text_height_5x7 (SCALE);
    int x  = (int) ((int32_t) W - (int32_t) tw) / 2;
    int y  = (int) ((int32_t) H - (int32_t) th) / 2;

    uint32_t shadow = desktop_color (0x20, 0x20, 0x30);
    uint32_t mainc  = desktop_color (0xF8, 0xF0, 0xFF);

    desktop_draw_text_5x7 (x + 1, y + 1, title, SCALE, SP, shadow);
    desktop_draw_text_5x7 (x,     y,     title, SCALE, SP, mainc);
}

static void desktop_draw_wallpaper (void) {
    uint32_t W = gfx_width ();
    uint32_t H = gfx_height ();

    for (uint32_t y = 0; y < H; ++y) {
        uint8_t r = (uint8_t) (20 + (y * 40  / (H ? H : 1)));
        uint8_t g = (uint8_t) (30 + (y * 70  / (H ? H : 1)));
        uint8_t b = (uint8_t) (60 + (y * 160 / (H ? H : 1)));
        gfx_hline (0, y, W, desktop_color (b, g, r));
    }

    desktop_draw_centered_quarkos ();
}

/* =========================
 * 任务栏
 * ========================= */

static void desktop_draw_taskbar (void) {
    uint32_t W = gfx_width ();
    gfx_fill_rect (0, 0, W, DESKTOP_TASKBAR_H, desktop_color (0x90, 0x90, 0x30));
    gfx_hline (0, DESKTOP_TASKBAR_H - 1, W, desktop_color (0x00, 0x00, 0x00));

    gfx_fill_rect (6, 4, 54, DESKTOP_TASKBAR_H - 8, desktop_color (0x40, 0x80, 0xF0));
    desktop_draw_text_6x8 (12, 9, "Start", desktop_color (0x00, 0x00, 0x00));
}

/* =========================
 * 窗口
 * ========================= */

static void desktop_draw_window (const desk_window_t * w) {
    if (!w) return;

    uint32_t bx = w->x;
    uint32_t by = w->y + DESKTOP_TASKBAR_H;
    uint32_t bw = w->w;
    uint32_t bh = w->h;

    /* border */
    gfx_fill_rect (bx - 1, by - 21, bw + 2, bh + 22, desktop_color (0x10, 0x10, 0x10));
    /* title */
    gfx_fill_rect (bx, by - 20, bw, 18, w->color_title);
    if (w->title) {
        desktop_draw_text_6x8 (bx + 6, by - 18, w->title, desktop_color (0x00, 0x00, 0x00));
    }
    /* content */
    gfx_fill_rect (bx, by, bw, bh, w->color_body);
}

/* =========================
 * 重绘调度
 * ========================= */

static void desktop_redraw_all (void) {
    desktop_draw_wallpaper ();
    desktop_draw_taskbar ();
    for (int i = 0; i < s_nwins; ++i) {
        desktop_draw_window (&s_wins[i]);
    }
}

/* =========================
 * Public API
 * ========================= */

int desktop_init (void) {
    if (gfx_init () != 0) return -1;

    desktop_redraw_all ();
    gfx_present (NULL);

    s_mouse_x = 16;
    s_mouse_y = 40;
    desktop_cursor_draw ();
    struct gfx_rect r = { s_mouse_x, s_mouse_y, 16, 22 };
    gfx_present (&r);
    return 0;
}

int desktop_add_window (const desk_window_t * w) {
    if (!w || s_nwins >= DESKTOP_MAX_WINS) return -1;

    s_wins[s_nwins] = *w;
    int idx = s_nwins++;
    desktop_redraw_all ();
    gfx_present (NULL);
    return idx;
}

void desktop_redraw_full (void) {
    desktop_redraw_all ();
    gfx_present (NULL);

    desktop_cursor_draw ();
    struct gfx_rect r = { s_mouse_x, s_mouse_y, 16, 22 };
    gfx_present (&r);
}

void desktop_cursor_move (uint32_t x, uint32_t y) {
    s_mouse_x = x;
    s_mouse_y = y;
    desktop_redraw_all ();
    desktop_cursor_draw ();
    gfx_present (NULL);
}

void desktop_show_init (void) {
    desk_window_t a = {
        .x = 40, .y = 60, .w = 280, .h = 160,
        .color_body  = desktop_color (0xF0, 0xE8, 0xD8),
        .color_title = desktop_color (0xA0, 0xD0, 0xFF),
        .title       = "Welcome"
    };
    desk_window_t b = {
        .x = 140, .y = 120, .w = 320, .h = 200,
        .color_body  = desktop_color (0xF8, 0xF0, 0xF0),
        .color_title = desktop_color (0xFF, 0xB0, 0x70),
        .title       = "Settings"
    };
    desk_window_t c = {
        .x = gfx_width () - 220, .y = 90, .w = 200, .h = 140,
        .color_body  = desktop_color (0xE8, 0xFF, 0xF0),
        .color_title = desktop_color (0x90, 0xF0, 0xC0),
        .title       = "About"
    };

    desktop_add_window (&a);
    desktop_add_window (&b);
    desktop_add_window (&c);
}


// start when need
// void input_on_pointer (uint32_t x, uint32_t y, uint32_t buttons) {
//     (void) buttons;
//     desktop_cursor_move (x, y);
// }