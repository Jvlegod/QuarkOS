#ifndef __SHELL_H__
#define __SHELL_H__
#include "uart.h"
#include "ktypes.h"
#include "cstdlib.h"

#define MAX_CMD_LEN 64
#define MAX_ARGS 8

typedef int (*cmd_handler)(int, char**);

struct builtin_cmd {
    const char *name;
    cmd_handler handler;
    const char *help;
};

int cmd_clear(int argc, char **argv);
int cmd_echo(int argc, char **argv);
int cmd_help(int argc, char **argv);
void parse_command(void);
void read_line(void);
char shell_getc(void);
void shell_main(void);



#endif // __SHELL_H__