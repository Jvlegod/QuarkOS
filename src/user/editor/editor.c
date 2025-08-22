#include "ktypes.h"
#include "editor.h"
#include "uart.h"
#include "fs.h"
#include "cstdlib.h"

#define ED_BUF_SIZE   (8192)
static char ed_buf[ED_BUF_SIZE];
static unsigned ed_len = 0;

static void ed_print_help(void) {
    EDITOR_PRINTF("ed cmds: p=print, e=edit, a=append, w=write, r=reload, q=quit, h=help\r\n");
}

static void ed_print(void) {
    for (unsigned i = 0; i < ed_len; ++i) {
        char c = ed_buf[i];
        if (c == '\n') EDITOR_PRINTF("\r\n");
        else EDITOR_PRINTF("%c", c);
    }
    if (ed_len == 0) EDITOR_PRINTF("\r\n");
}

static void ed_load(const char* path) {
    int n = fs_read_all(path, ed_buf, ED_BUF_SIZE-1);
    if (n < 0) { EDITOR_PRINTF("[ed] read failed\r\n"); ed_len = 0; return; }
    ed_len = (unsigned)n;
    ed_buf[ed_len] = '\0';
    EDITOR_PRINTF("[ed] loaded %u bytes\r\n", ed_len);
}

static void ed_write(const char* path) {
    int n = fs_write_all(path, ed_buf, ed_len);
    if (n < 0) { EDITOR_PRINTF("[ed] write failed\r\n"); return; }
    EDITOR_PRINTF("[ed] wrote %d bytes\r\n", n);
}

static void ed_capture_into_end(void) {
    char line[256];
    for (;;) {
        EDITOR_PRINTF("... ");
        int n = editor_uart_fflush(line);
        if (line[0] == '.') break;
        if (ed_len + (unsigned)n + 1 >= ED_BUF_SIZE) {
            EDITOR_PRINTF("[ed] buffer full (max %d)\r\n", ED_BUF_SIZE);
            break;
        }
        memcpy(ed_buf + ed_len, line, (unsigned)n);
        ed_len += (unsigned)n;
        ed_buf[ed_len++] = '\n';
        ed_buf[ed_len] = '\0';
    }
}

static void ed_capture_replace(void) {
    ed_len = 0; ed_buf[0] = '\0';
    ed_capture_into_end();
}

void editor_run(const char* path) {
    char cmd[32];
    char curpath[FS_PATH_MAX];

    strncpy(curpath, path ? path : "", sizeof(curpath)-1);
    curpath[sizeof(curpath)-1] = '\0';

    EDITOR_PRINTF("[ed] open %s\r\n", curpath[0] ? curpath : "(empty)");
    if (path && path[0]) ed_load(path); else { ed_len = 0; ed_buf[0] = '\0'; }
    ed_print_help();

    for (;;) {
        EDITOR_PRINTF("ed> ");
        editor_uart_fflush(cmd);
        for (int i = 0; i < 32; i ++) {
            if (cmd[i] == '\r' ||
                cmd[i] == '\n' ||
                cmd[i] == ' ') {
                cmd[i] = '\0';
            }
        }

        if (strcmp(cmd, "p") == 0) {
            ed_print();
        } else if (strcmp(cmd, "h") == 0 || strcmp(cmd, "help") == 0) {
            ed_print_help();
        } else if (strcmp(cmd, "e") == 0) {
            EDITOR_PRINTF("-- edit (end with '.') --\r\n");
            ed_capture_replace();
        } else if (strcmp(cmd, "a") == 0) {
            EDITOR_PRINTF("-- append (end with '.') --\r\n");
            ed_capture_into_end();
        } else if (strcmp(cmd, "w") == 0) {
            if (!path || !path[0]) { EDITOR_PRINTF("[ed] no path\r\n"); continue; }
            ed_write(path);
        } else if (strcmp(cmd, "r") == 0) {
            if (!path || !path[0]) { EDITOR_PRINTF("[ed] no path\r\n"); continue; }
            ed_load(path);
        } else if (strcmp(cmd, "q") == 0) {
            EDITOR_PRINTF("[ed] quit\r\n");
            return;
        } else {
            EDITOR_PRINTF("[ed] unknown: %s (h for help)\r\n", cmd);
        }
    }
}
