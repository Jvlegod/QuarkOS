#ifndef     __SHELL_H__
#define     __SHELL_H__
#include "kprintf.h"
#include "ktypes.h"
#include "cstdlib.h"

#define OS_NAME "QuarkOs"
#define VERSION(id) "Version: "#id
#define SHELL_INFO "sh> "

#define SHELL_PRINTF(fmt, ...) \
    kprintf(fmt , ##__VA_ARGS__)

typedef int (*shell_handler_t)(int argc, char **argv);

struct shell_command {
    const char *name;
    shell_handler_t handler;
    const char *help;
};

struct shell_app {
    const char *name;
    void (*entry)(void *);
    const char *desc;
};

void shell_main();

// commands
int cmd_help(int argc, char **argv);
int cmd_echo(int argc, char **argv);
int cmd_start(int argc, char **argv);
int cmd_ls(int argc, char** argv);
int cmd_mkdir(int argc, char** argv);
int cmd_touch(int argc, char** argv);
int cmd_cd(int argc, char** argv);
int cmd_pwd(int argc, char** argv);

#endif
