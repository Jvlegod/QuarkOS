#include "app.h"
#include "desktop.h"
#include "ktypes.h"
#include "cstdlib.h"

static unsigned int seed = 123456789;
static int simple_rand(void) {
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 16) & 0x7FFFu);
}

static int board[9];
static int moves;
static int finished;
static desk_window_t g_win;
static int g_win_id;
static uint32_t g_x0, g_y0, g_cell_w, g_cell_h;

static void draw_x(int idx) {
    uint32_t x = g_x0 + (uint32_t)(idx % 3) * g_cell_w;
    uint32_t y = g_y0 + (uint32_t)(idx / 3) * g_cell_h;
    uint32_t r = g_cell_w - 4;
    uint32_t color = desktop_color(0x00, 0x00, 0xFF); /* red */
    desktop_draw_line(x + 2, y + 2, x + r, y + r, color);
    desktop_draw_line(x + 2, y + r, x + r, y + 2, color);
}

static void draw_o(int idx) {
    uint32_t x = g_x0 + (uint32_t)(idx % 3) * g_cell_w;
    uint32_t y = g_y0 + (uint32_t)(idx / 3) * g_cell_h;
    uint32_t cx = x + g_cell_w / 2;
    uint32_t cy = y + g_cell_h / 2;
    uint32_t radius = g_cell_w / 2 - 2;
    uint32_t color = desktop_color(0xFF, 0x00, 0x00); /* blue */
    desktop_draw_circle(cx, cy, radius, color);
}

static void redraw_board(void) {
    for (int i = 0; i < 9; ++i) {
        if (board[i] == 1) draw_x(i);
        else if (board[i] == 2) draw_o(i);
    }
}

static int check_win(int p) {
    static const int lines[8][3] = {
        {0,1,2},{3,4,5},{6,7,8},
        {0,3,6},{1,4,7},{2,5,8},
        {0,4,8},{2,4,6}
    };
    for (int i = 0; i < 8; ++i) {
        int a = lines[i][0], b = lines[i][1], c = lines[i][2];
        if (board[a] == p && board[b] == p && board[c] == p) return 1;
    }
    return 0;
}

static void cpu_turn(void) {
    if (moves >= 9) return;
    int idx;
    do {
        idx = simple_rand() % 9;
    } while (board[idx] != 0);
    board[idx] = 2;
    draw_o(idx);
    moves++;
    if (check_win(2)) finished = 1;
}

static void cell_cb(int win, void* user) {
    if (win != g_win_id) return;
    redraw_board();
    if (finished) return;
    int idx = (int)(intptr_t)user;
    if (board[idx] != 0) return;
    board[idx] = 1;
    draw_x(idx);
    moves++;
    if (check_win(1)) { finished = 1; gfx_present(NULL); return; }
    if (moves == 9) { finished = 1; gfx_present(NULL); return; }
    cpu_turn();

    gfx_present(NULL);
}

void game_entry(void *arg) {
    (void)arg;
    memset(board, 0, sizeof(board));
    moves = 0;
    finished = 0;

    g_win.x = 100;
    g_win.y = 80;
    g_win.w = 180;
    g_win.h = 180;
    g_win.color_body  = desktop_color(0xFF, 0xFF, 0xFF);
    g_win.color_title = desktop_color(0xA0, 0xD0, 0xFF);
    g_win.title = "TicTacToe";
    g_win_id = desktop_add_window(&g_win);

    g_x0 = g_win.x;
    g_y0 = g_win.y + DESKTOP_TASKBAR_H;
    g_cell_w = g_win.w / 3;
    g_cell_h = g_win.h / 3;

    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            int idx = y * 3 + x;
            /* button coordinates are relative to window content */
            desktop_add_button(g_win_id, x * g_cell_w, y * g_cell_h,
                               g_cell_w, g_cell_h, "", g_win.color_body,
                               cell_cb, (void*)(intptr_t)idx);
        }
    }

    /* draw initial board */
    desktop_redraw_full();
}
