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

/* ====== 输入与命中检测/拖动状态 ====== */
static unsigned s_btn_state = 0;
static int      s_drag_idx  = -1;
static int      s_drag_offx = 0;
static int      s_drag_offy = 0;

static inline int clampi(int v, int lo, int hi){ if (v<lo) return lo; if (v>hi) return hi; return v; }

static int hit_window_body(int x, int y){
    for (int i = s_nwins-1; i >= 0; --i){
        const desk_window_t* w = &s_wins[i];
        int bx = (int)w->x;
        int by = (int)w->y + (int)DESKTOP_TASKBAR_H;
        if (x>=bx && x<bx+(int)w->w && y>=by && y<by+(int)w->h) return i;
    }
    return -1;
}
static int hit_window_title(int x, int y){
    for (int i = s_nwins-1; i >= 0; --i){
        const desk_window_t* w = &s_wins[i];
        int bx = (int)w->x;
        int by = (int)w->y + (int)DESKTOP_TASKBAR_H;
        if (x>=bx && x<bx+(int)w->w && y>=by-20 && y<by-2) return i;
    }
    return -1;
}

static int hit_taskbar_start(int x, int y){
    return (x>=6 && x<6+54 && y>=4 && y<(int)DESKTOP_TASKBAR_H-4);
}

static void bring_to_front(int idx){
    if (idx < 0 || idx >= s_nwins || s_nwins <= 1) return;
    desk_window_t w = s_wins[idx];
    for (int i = idx; i < s_nwins-1; ++i) s_wins[i] = s_wins[i+1];
    s_wins[s_nwins-1] = w;
    if (s_drag_idx == idx) s_drag_idx = s_nwins-1;
}

/* =========================
 * 光标
 * ========================= */

static void desktop_cursor_draw_at (uint32_t x, uint32_t y) {
    static const uint16_t mouse[19] = {
        0b1, 0b11, 0b111, 0b1111, 0b11111, 0b111111, 0b1111111, 0b11111110, 0b111111100,
        0b111111000, 0b111110000, 0b111100000, 0b111000000, 0b110000000, 0b110000000,
        0b100000000, 0b100000000, 0b000000000, 0b000000000,
    };

    uint32_t W = gfx_width();
    uint32_t H = gfx_height();
    uint32_t P = gfx_pitch_bytes() / 4;
    uint32_t * fb = gfx_fb();
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

    uint32_t W = gfx_width();
    uint32_t H = gfx_height();

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
    uint32_t W = gfx_width();
    uint32_t H = gfx_height();

    for (uint32_t y = 0; y < H; ++y) {
        uint8_t r = (uint8_t) (20 + (y * 40  / (H ? H : 1)));
        uint8_t g = (uint8_t) (30 + (y * 70  / (H ? H : 1)));
        uint8_t b = (uint8_t) (60 + (y * 160 / (H ? H : 1)));
        gfx_hline (0, y, W, desktop_color (b, g, r));
    }

    desktop_draw_centered_quarkos();
}

/* =========================
 * 任务栏
 * ========================= */

/* ====== Start 菜单 ====== */
static start_item_t s_start_items[START_MAX];
static int          s_start_count = 0;
static int          s_start_open  = 0;

int desktop_startmenu_add(const char* label, desk_menu_cb cb, void* user){
    if (s_start_count >= START_MAX) return -1;
    s_start_items[s_start_count] = (start_item_t){ label, cb, user };
    return s_start_count++;
}
void desktop_startmenu_clear(void){ s_start_count=0; }

static int start_menu_x(void){ return 6; }
static int start_menu_y(void){ return DESKTOP_TASKBAR_H + 2; }
static int start_menu_h(void){ return s_start_count * START_ROW + 4; }

static void draw_start_menu(void)
{
    if (!s_start_open || s_start_count==0) return;
    int x = start_menu_x(), y = start_menu_y();
    int h = start_menu_h();
    gfx_fill_rect(x, y, START_W, h, desktop_color(0xF0,0xF0,0xF0));
    gfx_hline(x, y, START_W, desktop_color(0,0,0));
    gfx_hline(x, y+h-1, START_W, desktop_color(0,0,0));
    for (int i=0;i<s_start_count;i++){
        int ry = y + 2 + i*START_ROW;
        desktop_draw_text_6x8(x+8, ry+4, s_start_items[i].label, desktop_color(0,0,0));
        gfx_hline(x, ry+START_ROW-1, START_W, desktop_color(0xC0,0xC0,0xC0));
    }
}
static int hit_start_menu(int x, int y){
    if (!s_start_open) return -1;
    int sx = start_menu_x(), sy = start_menu_y();
    int h  = start_menu_h();
    if (x<sx || x>=sx+START_W || y<sy || y>=sy+h) return -1;
    int idx = (y - (sy+2)) / START_ROW;
    if (idx < 0 || idx >= s_start_count) return -1;
    return idx;
}

static void desktop_draw_taskbar (void) {
    uint32_t W = gfx_width();
    gfx_fill_rect (0, 0, W, DESKTOP_TASKBAR_H, desktop_color (0x90, 0x90, 0x30));
    gfx_hline (0, DESKTOP_TASKBAR_H - 1, W, desktop_color (0x00, 0x00, 0x00));

    gfx_fill_rect (6, 4, 54, DESKTOP_TASKBAR_H - 8, desktop_color (0x40, 0x80, 0xF0));
    desktop_draw_text_6x8 (12, 9, "Start", desktop_color (0x00, 0x00, 0x00));
}

/* =========================
 * 窗口
 * ========================= */

/* ====== 窗口内按钮控件 ====== */
#define DESKTOP_MAX_BTNS 64
typedef struct {
    int win; uint32_t x,y,w,h;
    const char* label; uint32_t color;
    desk_button_cb cb; void* user;
    int visible;
} desk_button_t;

static desk_button_t s_btns[DESKTOP_MAX_BTNS];
static int           s_nbtns = 0;

static int text_width_6x8(const char* s){ int n=0; while(s && s[n]) n++; return n*6; }

int desktop_add_button(int win, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                       const char* label, uint32_t color, desk_button_cb cb, void* user)
{
    if (s_nbtns >= DESKTOP_MAX_BTNS) return -1;
    s_btns[s_nbtns] = (desk_button_t){ win,x,y,w,h,label,color,cb,user,1 };
    return s_nbtns++;
}
void desktop_button_set_visible(int id, int visible){
    if (id<0 || id>=s_nbtns) return; s_btns[id].visible = visible?1:0;
}

static void draw_button_abs(int ax, int ay, const desk_button_t* b)
{
    int x = ax + (int)b->x, y = ay + (int)b->y;
    gfx_fill_rect(x, y, b->w, b->h, b->color);
    // 边框
    gfx_hline(x, y, b->w, desktop_color(0,0,0));
    gfx_hline(x, y+b->h-1, b->w, desktop_color(0,0,0));
    for (uint32_t r=0;r<b->h;r++){
        gfx_hline(x, y+r, 1, desktop_color(0,0,0));
        gfx_hline(x+b->w-1, y+r, 1, desktop_color(0,0,0));
    }
    int tw = text_width_6x8(b->label);
    int tx = x + ((int)b->w - tw)/2;
    int ty = y + ((int)b->h - 8)/2;
    desktop_draw_text_6x8((uint32_t)tx, (uint32_t)ty, b->label, desktop_color(0,0,0));
}

static int hit_button(int sx, int sy, int win_idx)
{
    for (int i=s_nbtns-1; i>=0; --i){
        desk_button_t* b = &s_btns[i];
        if (!b->visible || b->win != win_idx) continue;
        const desk_window_t* w = &s_wins[win_idx];
        int ax = (int)w->x;
        int ay = (int)w->y + (int)DESKTOP_TASKBAR_H;

        int x = ax + (int)b->x, y = ay + (int)b->y;
        if (sx>=x && sx<x+(int)b->w && sy>=y && sy<y+(int)b->h) return i;
    }
    return -1;
}

/* ====== 窗口标题栏上的 X 按钮 ====== */
#define TITLE_H   18
#define CLOSE_W   16
#define CLOSE_H   16
#define CLOSE_PAD 2

static void draw_close_btn(uint32_t bx, uint32_t by, uint32_t bw, uint32_t color)
{
    // bx/by 是内容区左上（by 已含 taskbar 高度），bw 是窗口宽
    uint32_t x = bx + bw - (CLOSE_W + CLOSE_PAD);
    uint32_t y = by - (TITLE_H - CLOSE_PAD);
    gfx_fill_rect(x, y, CLOSE_W, CLOSE_H, desktop_color(0x80,0x40,0x40)); // 底色
    // 边框
    gfx_hline(x, y, CLOSE_W, desktop_color(0x00,0x00,0x00));
    gfx_hline(x, y+CLOSE_H-1, CLOSE_W, desktop_color(0x00,0x00,0x00));
    for (uint32_t r=0;r<CLOSE_H;r++){ // 竖边
        gfx_hline(x, y+r, 1, desktop_color(0x00,0x00,0x00));
        gfx_hline(x+CLOSE_W-1, y+r, 1, desktop_color(0x00,0x00,0x00));
    }
    // 白色 X
    for (uint32_t i=3;i<CLOSE_W-3;i++){
        uint32_t xx1 = x + i,        yy1 = y + i;
        uint32_t xx2 = x + (CLOSE_W-1-i), yy2 = y + i;
        if (yy1<y+CLOSE_H) desktop_put_pixel(xx1, yy1, desktop_color(0xFF,0xFF,0xFF));
        if (yy2<y+CLOSE_H) desktop_put_pixel(xx2, yy2, desktop_color(0xFF,0xFF,0xFF));
    }
}

static int hit_close_btn(int x, int y, const desk_window_t* w)
{
    int bx = (int)w->x, by = (int)w->y + (int)DESKTOP_TASKBAR_H;
    int rx = bx + (int)w->w - (CLOSE_W + CLOSE_PAD);
    int ry = by - (TITLE_H - CLOSE_PAD);
    return (x>=rx && x<rx+CLOSE_W && y>=ry && y<ry+CLOSE_H);
}

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
    
    for (int i=0;i<s_nbtns;i++){
        if (s_btns[i].visible && s_btns[i].win == -1) {}
    }
    
    for (int i=0;i<s_nbtns;i++){
        if (!s_btns[i].visible) continue;
        if (&s_wins[0] <= w && w <= &s_wins[s_nwins-1]) {
            int idx = (int)(w - &s_wins[0]);
            if (s_btns[i].win == idx) draw_button_abs((int)bx, (int)by, &s_btns[i]);
        }
    }

    draw_close_btn(bx, by, bw, w->color_title);
}

/* =========================
 * 重绘调度
 * ========================= */

static void desktop_redraw_all (void) {
    desktop_draw_wallpaper();
    desktop_draw_taskbar();
    for (int i = 0; i < s_nwins; ++i) {
        desktop_draw_window(&s_wins[i]);
    }
    if (s_start_open) draw_start_menu();
}

/* =========================
 * Public API
 * ========================= */

int desktop_init (void) {
    if (gfx_init() != 0) return -1;

    desktop_redraw_all();
    gfx_present (NULL);

    s_mouse_x = 16;
    s_mouse_y = 40;
    desktop_cursor_draw();
    struct gfx_rect r = { s_mouse_x, s_mouse_y, 16, 22 };
    gfx_present (&r);
    return 0;
}

int desktop_add_window (const desk_window_t * w) {
    if (!w || s_nwins >= DESKTOP_MAX_WINS) return -1;

    s_wins[s_nwins] = *w;
    int idx = s_nwins++;
    desktop_redraw_all();
    gfx_present (NULL);
    return idx;
}

void desktop_redraw_full (void) {
    desktop_redraw_all();
    gfx_present (NULL);

    desktop_cursor_draw();
    struct gfx_rect r = { s_mouse_x, s_mouse_y, 16, 22 };
    gfx_present (&r);
}

void desktop_cursor_move (uint32_t x, uint32_t y) {
    uint32_t W = gfx_width(), H = gfx_height();
    if (W == 0 || H == 0) return;
    if (x >= W) x = W - 1;
    if (y >= H) y = H - 1;

    desktop_cursor_draw_at(s_mouse_x, s_mouse_y);

    uint32_t old_x = s_mouse_x, old_y = s_mouse_y;
    s_mouse_x = x; s_mouse_y = y;
    desktop_cursor_draw_at(s_mouse_x, s_mouse_y);

    uint32_t minx = old_x < s_mouse_x ? old_x : s_mouse_x;
    uint32_t miny = old_y < s_mouse_y ? old_y : s_mouse_y;
    uint32_t maxx = (old_x > s_mouse_x ? old_x : s_mouse_x) + 12;
    uint32_t maxy = (old_y > s_mouse_y ? old_y : s_mouse_y) + 19;
    if (maxx > W) maxx = W;
    if (maxy > H) maxy = H;

    struct gfx_rect r = { minx, miny, maxx - minx, maxy - miny };
    gfx_present(&r);
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
        .x = gfx_width() - 220, .y = 90, .w = 200, .h = 140,
        .color_body  = desktop_color (0xE8, 0xFF, 0xF0),
        .color_title = desktop_color (0x90, 0xF0, 0xC0),
        .title       = "About"
    };

    desktop_add_window (&a);
    desktop_add_window (&b);
    desktop_add_window (&c);
}

void desktop_poll_keyboard(virtio_kbd_t* g_kbd) {
    for (;;) {
        int c = virtio_kbd_getchar(g_kbd);
        if (c < 0) break;
    }
}

void desktop_poll_cursor(virtio_tablet_t* g_tab) {
    int sx, sy; unsigned btn;
    if (virtio_tablet_read_scaled(g_tab, (int)gfx_width(), (int)gfx_height(), &sx, &sy, &btn)) {
        desktop_pointer_abs((uint32_t)sx, (uint32_t)sy, btn);

        static unsigned last = 0;
        if ((btn ^ last) & 1) { /* 左键 */ }
        if ((btn ^ last) & 2) { /* 右键 */ }
        if ((btn ^ last) & 4) { /* 中键 */ }
        last = btn;
    }
}

void desktop_pointer_abs(uint32_t x, uint32_t y, unsigned buttons)
{
    desktop_cursor_move(x, y);

    unsigned old = s_btn_state, now = buttons;
    unsigned down = (now & ~old), up = (~now & old);

    if (s_start_open) {
        if (down & DESK_BTN_LEFT) {
            int idx = hit_start_menu((int)x, (int)y);
            if (idx >= 0) {
                desk_menu_cb cb = s_start_items[idx].cb;
                void* user = s_start_items[idx].user;
                s_start_open = 0;
                desktop_redraw_full();
                if (cb) cb(user);
                s_btn_state = now;
                return;
            } else {
                s_start_open = 0;
                desktop_redraw_full();
            }
        }
    }


    if (down & DESK_BTN_LEFT) {
        for (int i=s_nwins-1; i>=0; --i){
            if (hit_close_btn((int)x,(int)y,&s_wins[i])) {
                desktop_close_window(i);
                s_btn_state = now;
                return;
            }
        }

        for (int i=s_nwins-1; i>=0; --i){
            int bid = hit_button((int)x,(int)y, i);
            if (bid >= 0) {

                bring_to_front(i);
                desktop_redraw_full();
                if (s_btns[bid].cb) s_btns[bid].cb(i, s_btns[bid].user);
                s_btn_state = now;
                return;
            }
        }

        if (hit_taskbar_start((int)x,(int)y)) {
            s_start_open = !s_start_open;
            desktop_redraw_full();
            s_btn_state = now;
            return;
        }

        int ti = hit_window_title((int)x,(int)y);
        if (ti >= 0) {
            bring_to_front(ti);
            desktop_redraw_full();

            desk_window_t* w = &s_wins[s_nwins-1];
            s_drag_offx = (int)x - (int)w->x;
            s_drag_offy = (int)y - ((int)w->y + (int)DESKTOP_TASKBAR_H - 20);
            s_drag_idx  = s_nwins - 1;
        }
    }

    if (s_drag_idx >= 0 && (now & DESK_BTN_LEFT)) {
        desk_window_t* w = &s_wins[s_drag_idx];
        int W = (int)gfx_width(), H = (int)gfx_height();

        int nx = (int)x - s_drag_offx;
        int ny_title_top = (int)y - s_drag_offy;
        int ny = ny_title_top - (int)DESKTOP_TASKBAR_H + 20;

        nx = clampi(nx, 0, W - (int)w->w);
        ny = clampi(ny, 0, H - (int)DESKTOP_TASKBAR_H - (int)w->h);

        if (nx != (int)w->x || ny != (int)w->y) {
            w->x = (uint32_t)nx; w->y = (uint32_t)ny;
            desktop_redraw_full();
        }
    }

    if (up & DESK_BTN_LEFT) {
        s_drag_idx = -1;
    }

    if (down & DESK_BTN_RIGHT) kprintf("[desktop] RMB down @(%u,%u)\n", x, y);
    if (down & DESK_BTN_MID)   kprintf("[desktop] MMB down @(%u,%u)\n", x, y);

    s_btn_state = now;
}

void desktop_pointer_rel(int dx, int dy, int wheel, unsigned buttons)
{
    int nx = (int)s_mouse_x + dx;
    int ny = (int)s_mouse_y + dy;
    int W = (int)gfx_width(), H = (int)gfx_height();
    nx = clampi(nx, 0, W-1);
    ny = clampi(ny, 0, H-1);

    desktop_pointer_abs((uint32_t)nx, (uint32_t)ny, buttons);

    if (wheel) {
        kprintf("[desktop] wheel %+d @(%d,%d)\n", wheel, nx, ny);
    }
}

void desktop_close_window(int idx)
{
    if (idx < 0 || idx >= s_nwins) return;
    for (int i = idx; i < s_nwins-1; ++i) s_wins[i] = s_wins[i+1];
    s_nwins--;
    if (s_drag_idx == idx) s_drag_idx = -1;
    else if (s_drag_idx > idx) s_drag_idx--;
    desktop_redraw_full();
}
