#include "test.h"
#include "mem.h"
#include "uart.h"
#include "task.h"
#include "platform.h"
#include "kprintf.h"
#include "ktypes.h"
#include "virtio.h"
#include "cstdlib.h"

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
    TEST_PRINTF("Hello, QuarkOS!\n");
    TEST_PRINTF("Hello, QuarkOS! %d %d\n", a, b);
}

void blk_read_write_test() {
    uint8_t sector1[512], sector2[512];
    
    if (blk_read(0, sector1, 1) != 0) {
        TEST_PRINTF("Read sector 0 failed!\n");
        return;
    }
    
    uint8_t test_pattern[16] = {0xDE, 0xAD, 0xBE, 0xEF};
    memcpy(sector1, test_pattern, sizeof(test_pattern));
    
    if (blk_write(1, sector1, 1) != 0) {
        TEST_PRINTF("Write sector 1 failed!\n");
        return;
    }
    
    memset(sector2, 0, sizeof(sector2));
    if (blk_read(1, sector2, 1) != 0) {
        TEST_PRINTF("Read back sector 1 failed!\n");
        return;
    }
    
    if (memcmp(sector1, sector2, 512) == 0) {
        TEST_PRINTF("Read/Write test PASSED!\n");
        
        TEST_PRINTF("Written data (first 16 bytes): ");
        for (int i = 0; i < 16; i++) {
            TEST_PRINTF("%x ", sector1[i]);
        }
        TEST_PRINTF("\n");
    } else {
        TEST_PRINTF("Data mismatch! Test FAILED!\n");
        
        for (int i = 0; i < 16; i++) {
            if (sector1[i] != sector2[i]) {
                TEST_PRINTF("Diff @ offset %d: %x vs %x\n", 
                           i, sector1[i], sector2[i]);
            }
        }
    }
}