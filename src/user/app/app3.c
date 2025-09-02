#include "shell.h"
#include "app.h"
#include "desktop.h"
#include "QuarkOS.h"

static void demo_click(void *user) {
    SHELL_PRINTF("desktop app clicked!\r\n");
    (void)user;
}

void app3_entry(void *arg) {
    SHELL_PRINTF("app3 running!\r\n");
    desktop_add_app(80, DESKTOP_TASKBAR_H + 40, "demo", QuarkOS_icon_data, demo_click, NULL);
    
}