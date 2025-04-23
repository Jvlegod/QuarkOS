#include "test.h"
#include "mem.h"
#include "uart.h"
#include "task.h"

#define TEST_PRINTF(fmt, ...) \
    uart_printf("[TEST]: " fmt , ##__VA_ARGS__)

void mem_test() {
    int *arr = (int *)mem_alloc(1024 * sizeof(int));
    if (arr) {
        arr[0] = 0x1234;
        mem_free(arr);
    }
}

void task1() {
    while(1) {
        TEST_PRINTF("Task1 running\n");
        task_yield();
    }
}

void task2() {
    while(1) {
        TEST_PRINTF("Task2 running\n");
        task_yield();
    }
}

void task_test() {
    task_create(task1, 9);
    task_create(task2, 10);
    int d = 0;
    while(1) {
        task_yield();
        TEST_PRINTF("Hello, QuarkOS!, %d\n", d);
        d = (d + 1) % 100;
    }
}

void uart_test() {
    int a = 1;
    int b = -23;
    TEST_PRINTF("Hello, QuarkOS!\n");
    TEST_PRINTF("Hello, QuarkOS! %d %d\n", a, b);
}