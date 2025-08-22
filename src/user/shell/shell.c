#include "shell.h"
#include "uart.h"
#include "app.h"
#include "task.h"
#include "fs.h"

#define MAX_ARGC 8
static char shell_buf[RING_BUF_SIZE];
const char *OS_ART[] = {
    " ██████  ██    ██  █████  ██████  ██   ██  ██████  ███████ ",
    "██       ██    ██ ██   ██ ██   ██ ██  ██  ██    ██ ██      ",
    "██   ███ ██    ██ ███████ ██████  █████   ██    ██ ███████ ",
    "██    ██ ██    ██ ██   ██ ██   ██ ██  ██  ██    ██      ██ ",
    " ██████   ██████  ██   ██ ██   ██ ██   ██  ██████  ███████ ",
    NULL
};

static struct shell_command cmd_table[] = {
    {"help",   cmd_help,   "show all cmd"},
    {"echo",   cmd_echo,   "echo"},
    {"start",  cmd_start,  "start a app"},
    {"ls",     cmd_ls,     "list directory"},
    {"mkdir",  cmd_mkdir,  "create directory"},
    {"touch",  cmd_touch,  "create empty file"},
    {NULL, NULL, NULL}
};

int cmd_ls(int argc, char** argv) {
    const char* p = (argc >= 2) ? argv[1] : "/";
    return fs_ls(p);
}

int cmd_mkdir(int argc, char** argv) {
    if (argc < 2) { SHELL_PRINTF("usage: mkdir /path\r\n"); return -1; }
    return fs_mkdir(argv[1]);
}

int cmd_touch(int argc, char** argv) {
    if (argc < 2) { SHELL_PRINTF("usage: touch /path\r\n"); return -1; }
    return fs_touch(argv[1]);
}

int cmd_start(int argc, char **argv) {
    if (argc < 2) {
        SHELL_PRINTF("Usage: start <app_name>\r\n");
        return -1;
    }

    struct shell_app *app;
    for (app = app_table; app->name != NULL; app++) {
        if (strcmp(argv[1], app->name) == 0) {
            app->entry((void*)argv);
            return 0;
        }
    }

    SHELL_PRINTF("App [%s] not found!\r\n", argv[1]);
    return -1;
}

int cmd_help(int argc, char **argv) {
    struct shell_command *cmd;
    for (cmd = cmd_table; cmd->name != NULL; cmd++) {
        SHELL_PRINTF("\"%s\" usage: %s\r\n", cmd->name, cmd->help);
    }
    return 0;
}

int cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        SHELL_PRINTF("%s ", argv[i]);
    }
    SHELL_PRINTF("\r\n");
    return 0;
}

static int shell_parse_command(char *cmdline, char *argv[]) {
    int argc = 0;
    char *p = cmdline;
    bool in_quote = false;

    while (*p && argc < MAX_ARGC) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            *p++ = '\0';
        }

        if (!*p) break;

        if (*p == '"') {
            in_quote = true;
            *p++ = '\0';
            argv[argc++] = p;
            while (*p && (*p != '"' || !in_quote)) {
                if (*p == '\\' && *(p+1) == '"') p++;
                p++;
            }
            if (*p == '"') *p++ = '\0';
            in_quote = false;
            continue;
        }

        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
            p++;
        }
    }

    return argc;
}

void shell_main() {
    SHELL_PRINTF("\r\n%s-%s\r\n", OS_NAME, VERSION(0.1));
    const char **line = OS_ART;
    while (*line != NULL) {
        SHELL_PRINTF("%s\n", *line);
        line++;
    }
    
    while(1) {
        char *argv[MAX_ARGC];
        int argc;
        memset(shell_buf, 0, sizeof(RING_BUF_SIZE));
        SHELL_PRINTF("%s", SHELL_INFO);
        
        shell_uart_fflush(shell_buf);

        argc = shell_parse_command(shell_buf, argv);

        if (argc == 0) continue;
        
        struct shell_command *cmd;
        for (cmd = cmd_table; cmd->name != NULL; cmd++) {
            if (strcmp(argv[0], cmd->name) == 0) {
                cmd->handler(argc, argv);
                break;
            }
        }
        if (cmd->name == NULL) {
            SHELL_PRINTF("Unknown command: %s\r\n", argv[0]);
        }
    }
}