# QuarkOS

## 1 Graph SUPPORT

### 1.1 Graph

![alt text](assets/README/image.jpg)

### 1.2 Terminal

![alt text](assets/README/image-1.png)

## 2 Usage

step1: add your app in `app/app.h`

```c
static struct shell_app app_table[] = {
    {"app1", app1_entry, "example"},
    ...
    {NULL, NULL, NULL}
};
```

step2: code in dir `app/`, such as `app.c`.

```c
// you should include `shell.h` and `app.h`
#include "shell.h"
#include "app.h"

// entry relative to `app_table[]`
void app1_entry(void *arg) {
    SHELL_PRINTF("App1 running!\r\n");
}
```

## 3 Debug

step1: Compile and debug version

```bash
make BUILD_TYPE=debug
```
step2: Starting a debugging session

terminal1

```bash
make debug
```

terminal2

```bash
make gdb
```

## 4 Release

```bash
make run
```