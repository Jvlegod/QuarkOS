#include "shell.h"
#include "uart.h"
#include "app.h"
#include "task.h"
#include "fs.h"
#include "editor.h"

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
    {"cd",     cmd_cd,     "change dir to file"},
    {"pwd",    cmd_pwd,    "show current path"},
    {"ed",     cmd_ed,     "Quark editor"},
    {"rm",     cmd_rm,     "remove file/dir"},
    {NULL, NULL, NULL}
};

int cmd_rm(int argc, char** argv) {
    if (argc < 2 || argc > 2) {
        SHELL_PRINTF("usage: rm <path>\r\n");
        return -1;
    }

    if(fs_is_dir(argv[1])) {
        fs_rmdir(argv[1]);
    } else if (fs_is_file(argv[1])) {
        fs_rm(argv[1]);
    } else {
        SHELL_PRINTF("usage: rm <path>\r\n");
        return -1;
    }

    return 0;
}

int cmd_ed(int argc, char** argv) {
    if (argc < 2) {
        SHELL_PRINTF("usage: ed <path>\r\n");
        return -1;
    }
    editor_run(argv[1]);
    return 0;
}

int cmd_ls(int argc, char** argv) {
    return fs_ls(argc >= 2 ? argv[1] : NULL);
}
int cmd_touch(int argc, char** argv) {
    if (argc < 2) { SHELL_PRINTF("usage: touch <name|/path>\r\n"); return -1; }
    return fs_touch(argv[1]);
}
int cmd_mkdir(int argc, char** argv) {
    if (argc < 2) { SHELL_PRINTF("usage: mkdir <name|/path>\r\n"); return -1; }
    return fs_mkdir(argv[1]);
}
int cmd_cd(int argc, char** argv) {
    const char* p = (argc>=2) ? argv[1] : "/";
    int rc = fs_chdir(p);
    if (rc==0) SHELL_PRINTF("cwd: %s\r\n", fs_get_cwd());
    return rc;
}
int cmd_pwd(int argc, char** argv) {
    SHELL_PRINTF("%s\r\n", fs_get_cwd());
    return 0;
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

        for (int i = 0; i < 32; i ++) {
            if (shell_buf[i] == '\r' ||
                shell_buf[i] == '\n') {
                shell_buf[i] = '\0';
            }
        }


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