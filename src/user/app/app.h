#ifndef __APP_H__
#define __APP_H__
#include "shell.h"
#include "task.h"

void app1_entry(void *arg);
void app2_entry(void *arg);
void app3_entry(void *arg);
void game_entry(void *arg);

static struct shell_app app_table[] = {
    {"app1", app1_entry, "example"},
    {"app2", app2_entry, "example"},
    {"app3", app3_entry, "desktop"},
    {"game", game_entry, "tic-tac-toe"},
    {NULL, NULL, NULL}
};

#endif /* __APP_H__ */
