#include "shell.h"
#include "uart.h"
#include "app.h"
#include "task.h"
#include "fs.h"
#include "editor.h"
#include "user.h"

#define MAX_ARGC 8
#define SHELL_EXIT 1

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
    {"clear",  cmd_clear,  "clear the screen"},
    {"ps",     cmd_ps,     "list tasks"},
    {"perm",   cmd_perm,   "show file permissions"},
    {"login",  cmd_login,  "login user"},
    {"su",     cmd_su,     "switch user"},
    {"logout", cmd_logout, "logout"},
    {"useradd",cmd_useradd,"add user"},
    {"whoami", cmd_whoami, "current user"},
    {"users",  cmd_users,  "list users"},
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
int cmd_clear(int argc, char** argv) {
    uart_clear();
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

int cmd_ps(int argc, char **argv) {
    const struct task *tasks = task_get_tasks();
    int count = task_get_count();
    SHELL_PRINTF("ID   STATUS\r\n");
    for (int i = 0; i < count; i++) {
        const struct task *t = &tasks[i];
        if (t->status == TASK_UNCREATE) {
            continue;
        }
        const char *state = "UNKNOWN";
        switch (t->status) {
            case TASK_UNCREATE: state = "UNCREATE"; break;
            case TASK_CREATE: state = "CREATE"; break;
            case TASK_READY: state = "READY"; break;
            case TASK_RUNNING: state = "RUNNING"; break;
            case TASK_BLOCKED: state = "BLOCKED"; break;
            case TASK_SLEEPING: state = "SLEEPING"; break;
            case TASK_ZOMBIE: state = "ZOMBIE"; break;
        }
        SHELL_PRINTF("%d   %s\r\n", t->task_id, state);
    }
    return 0;
}

int cmd_perm(int argc, char **argv) {
    if (argc < 2) {
        SHELL_PRINTF("usage: perm <path>\r\n");
        return -1;
    }
    uid_t owner; uint16_t mode;
    if (fs_stat(argv[1], &owner, &mode) != 0) {
        SHELL_PRINTF("perm: failed\r\n");
        return -1;
    }
    char perms[10];
    perms[0] = (mode & 0400) ? 'r' : '-';
    perms[1] = (mode & 0200) ? 'w' : '-';
    perms[2] = (mode & 0100) ? 'x' : '-';
    perms[3] = (mode & 0040) ? 'r' : '-';
    perms[4] = (mode & 0020) ? 'w' : '-';
    perms[5] = (mode & 0010) ? 'x' : '-';
    perms[6] = (mode & 0004) ? 'r' : '-';
    perms[7] = (mode & 0002) ? 'w' : '-';
    perms[8] = (mode & 0001) ? 'x' : '-';
    perms[9] = '\0';
    const char *name = user_get_name(owner);
    SHELL_PRINTF("%s %d %s\r\n", perms, owner, name ? name : "");
    return 0;
}

static int do_login(const char *name, const char *password) {
    uid_t uid;
    if (user_auth(name, password, &uid) != 0) {
        SHELL_PRINTF("auth failed\r\n");
        return -1;
    }
    uid_t old = getuid();
    char old_cwd[FS_PATH_MAX];
    strcpy(old_cwd, fs_get_cwd());
    setuid(uid);
    char home[FS_PATH_MAX];
    snprintf(home, sizeof(home), "/home/%s", name);
    fs_mkdir(home);
    fs_chdir(home);
    shell_main();
    fs_chdir(old_cwd);
    setuid(old);
    return 0;
}

int cmd_login(int argc, char **argv) {
    if (argc < 3) {
        SHELL_PRINTF("usage: login <user> <password>\r\n");
        return -1;
    }
    return do_login(argv[1], argv[2]);
}

int cmd_su(int argc, char **argv) {
    if (argc < 3) {
        SHELL_PRINTF("usage: su <user> <password>\r\n");
        return -1;
    }
    return do_login(argv[1], argv[2]);
}

int cmd_logout(int argc, char **argv) {
    return SHELL_EXIT;
}

int cmd_useradd(int argc, char **argv) {
    if (argc < 3) {
        SHELL_PRINTF("usage: useradd <user> <password>\r\n");
        return -1;
    }
    uid_t uid;
    if (user_add(argv[1], argv[2], &uid) != 0) {
        SHELL_PRINTF("useradd failed\r\n");
        return -1;
    }
    char home[FS_PATH_MAX];
    snprintf(home, sizeof(home), "/home/%s", argv[1]);
    uid_t old = getuid();
    setuid(uid);
    fs_mkdir(home);
    setuid(old);
    SHELL_PRINTF("user %s created with uid %d\r\n", argv[1], uid);
    return 0;
}

int cmd_whoami(int argc, char **argv) {
    (void)argc; (void)argv;
    const char *name = user_get_name(getuid());
    if (name) {
        SHELL_PRINTF("%s\r\n", name);
    } else {
        SHELL_PRINTF("%d\r\n", getuid());
    }
    return 0;
}

int cmd_users(int argc, char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; i < MAX_USERS; i++) {
        if (user_table[i].name[0] != '\0') {
            SHELL_PRINTF("%d:%s\r\n", user_table[i].uid, user_table[i].name);
        }
    }
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
    SHELL_PRINTF("\r\n%s-%s\r\n", OS_NAME, VERSION(2.0));
    const char **line = OS_ART;
    while (*line != NULL) {
        SHELL_PRINTF("%s\n", *line);
        line++;
    }
    
    while(1) {
        char *argv[MAX_ARGC];
        int argc;
        memset(shell_buf, 0, sizeof(RING_BUF_SIZE));
        SHELL_PRINTF("%s/%s> ", SHELL_INFO, user_get_name(getuid()));
        
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
                int rc = cmd->handler(argc, argv);
                if (rc == SHELL_EXIT) {
                    return;
                }
                break;
            }
        }
        if (cmd->name == NULL) {
            SHELL_PRINTF("Unknown command: %s\r\n", argv[0]);
        }
    }
}