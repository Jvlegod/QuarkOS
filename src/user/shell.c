#include "shell.h"

static char cmd_buf[MAX_CMD_LEN];
int cmd_pos = 0;
static char *argv[MAX_ARGS];
int argc = 0;

struct builtin_cmd cmd_table[] = {
    {"help",  cmd_help,  "help information"},
    {"echo",  cmd_echo,  "args parameters"},
    {"clear", cmd_clear, "clear screen"},
    {NULL, NULL, NULL}
};

char shell_getc(void) {
    char c = uart_getc();
    uart_putc(c);
    return c;
}

void read_line(void) {
    cmd_pos = 0;
    while (1) {
        char c = shell_getc();
        
        if (c == '\r' || c == '\n') {
            uart_puts("\r\n");
            cmd_buf[cmd_pos] = '\0';
            return;
        } else if (c == 0x7F || c == '\b') {
            if (cmd_pos > 0) {
                cmd_pos--;
                uart_puts("\b \b");
            }
        } else if (cmd_pos < MAX_CMD_LEN-1) {
            cmd_buf[cmd_pos++] = c;
        }
    }
}

void parse_command(void) {
    argc = 0;
    char *p = cmd_buf;
    
    while (*p && argc < MAX_ARGS) {
        while (*p == ' ') p++;
        
        if (!*p) break;
        
        argv[argc++] = p;
        
        while (*p && *p != ' ') p++;
        if (*p) *p++ = '\0';
    }
}

int cmd_help(int argc, char **argv) {
    uart_puts("Available commands:\r\n");
    for (struct builtin_cmd *c = cmd_table; c->name; c++) {
        uart_puts(" - ");
        uart_puts(c->name);
        uart_puts(": ");
        uart_puts(c->help);
        uart_puts("\r\n");
    }
    return 0;
}

int cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        uart_puts(argv[i]);
        uart_puts(" ");
    }
    uart_puts("\r\n");
    return 0;
}

int cmd_clear(int argc, char **argv) {
    uart_puts("\x1B[2J\x1B[H");
    return 0;
}

void shell_main(void) {
    uart_puts("\r\nMyOS Shell v0.1\r\n");
    
    while (1) {
        // uart_puts("sh> ");
        // read_line();
        // parse_command();
        
        // if (argc == 0) continue;

        // int found = 0;
        // for (struct builtin_cmd *c = cmd_table; c->name; c++) {
        //     if (strcmp(argv[0], c->name) == 0) {
        //         c->handler(argc, argv);
        //         found = 1;
        //         break;
        //     }
        // }
        
        // if (!found) {
        //     uart_puts("Unknown command: ");
        //     uart_puts(argv[0]);
        //     uart_puts("\r\n");
        // }
    }
}