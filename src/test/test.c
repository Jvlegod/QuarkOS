#include "test.h"
#include "mem.h"
#include "uart.h"
#include "task.h"
#include "platform.h"
#include "kprintf.h"

#define TEST_PRINTF(fmt, ...) \
    kprintf("[TEST]: " fmt , ##__VA_ARGS__)

void mem_test() {
    int *arr = (int *)mem_alloc(1024 * sizeof(int));
    if (arr) {
        arr[0] = 0x1234;
        mem_free(arr);
    }
}

void task_test() {

}

void uart_test() {
    int a = 1;
    int b = -23;
    TEST_PRINTF("Hello, QuarkOS!\r\n");
    TEST_PRINTF("Hello, QuarkOS! %d %d\r\n", a, b);
}