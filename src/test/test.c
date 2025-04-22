#include "test.h"
#include "mem.h"
#include "uart.h"

void mem_test() {
    int *arr = (int *)mem_alloc(1024 * sizeof(int));
    if (arr) {
        arr[0] = 0x1234;
        mem_free(arr);
    }
}

void uart_test() {

}