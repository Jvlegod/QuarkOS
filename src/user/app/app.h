#ifndef __APP_H__
#define __APP_H__
#include "shell.h"
#include "task.h"

void app1_entry(void *arg);

static struct shell_app app_table[] = {
    {"app1", app1_entry, "example1"},
    {NULL, NULL, NULL}
};

#endif /* __APP_H__ */