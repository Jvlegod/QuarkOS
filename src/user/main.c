#include "kprintf.h"
#include "ktypes.h"
#include "shell.h"
#include "desktop.h"
#include "task.h"

void main(void *arg) {
    desktop_init();
    desktop_show_init();
    desktop_app_init();

    shell_main();
}