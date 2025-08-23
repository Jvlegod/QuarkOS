#include "kprintf.h"
#include "ktypes.h"
#include "shell.h"
#include "desktop.h"

void main(void *arg) {
    desktop_init();
    desktop_show_init();

    shell_main();
}