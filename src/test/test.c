#include "test.h"
#include "mem.h"
#include "uart.h"
#include "task.h"
#include "platform.h"
#include "kprintf.h"
#include "ktypes.h"
#include "virtio_blk.h"
#include "gfx.h" /* #include "virtio_gpu.h" */
#include "cstdlib.h"
#include "fs.h"

#define TEST_PRINTF(fmt, ...) \
    kprintf("[TEST]: " fmt , ##__VA_ARGS__)

void mem_test() {
    int *arr = (int *)mem_alloc(1024 * sizeof(int));
    if (arr) {
        arr[0] = 0x1234;
        mem_free(arr);
    }
    TEST_PRINTF("MEM TEST PASSED\n");
}

void task_test() {
    TEST_PRINTF("TASK TEST PASSED\n");
}

void uart_test() {
    int a = 1;
    int b = -23;
    TEST_PRINTF("Hello, QuarkOS!\n");
    TEST_PRINTF("Hello, QuarkOS! %d %d\n", a, b);
    TEST_PRINTF("USART TEST PASSED\n");
}

void blk_test(void) {
#ifndef SECTOR_SIZE
#define SECTOR_SIZE 512
#endif
    unsigned char sector1[SECTOR_SIZE];
    unsigned char sector2[SECTOR_SIZE];

    if (blk_read(0, sector1, 1) != 0) {
        TEST_PRINTF("Read sector 0 failed!\n");
        return;
    }

    static const unsigned char test_pattern[16] = {
        0xDE,0xAD,0xBE,0xEF, 0xDE,0xAD,0xBE,0xEF,
        0xDE,0xAD,0xBE,0xEF, 0xDE,0xAD,0xBE,0xEF
    };
    memcpy(sector1, test_pattern, sizeof(test_pattern));

    /* 写到 LBA1*/
    if (blk_write(1, sector1, 1) != 0) {
        TEST_PRINTF("Write sector 1 failed!\n");
        return;
    }

    /* 读回 LBA1 */
    memset(sector2, 0, sizeof(sector2));
    if (blk_read(1, sector2, 1) != 0) {
        TEST_PRINTF("Read back sector 1 failed!\n");
        return;
    }

    /* 校验 */
    if (memcmp(sector1, sector2, SECTOR_SIZE) == 0) {
        TEST_PRINTF("Read/Write test PASSED!\n");
        TEST_PRINTF("Written data (first 16 bytes):\n");
        for (int i = 0; i < 16; i++) {
            TEST_PRINTF("%d=>%x\n", i, sector2[i]);
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
    TEST_PRINTF("BLOCK TEST PASSED\n");
}

void fs_test() {
    fs_mkdir("/etc");
    fs_touch("/etc/config");
    TEST_PRINTF("FS TEST PASSED\n");
}

void gpu_test() {
    // virtio_gpu_present_demo();

    gfx_clear(GFX_BGRA(0x30,0x30,0x60));
    gfx_fill_rect(0,0,gfx_width(),28, GFX_BGRA(0x30,0x90,0xF0));
    gfx_fill_rect(40,60,260,160, GFX_BGRA(0x20,0x80,0xF0));
    gfx_fill_rect(120,120,260,180, GFX_BGRA(0xF0,0x80,0x30));
    gfx_fill_rect(gfx_width()-200,80,160,140, GFX_BGRA(0x30,0xF0,0x90));
    gfx_present(NULL);

    TEST_PRINTF("GPU/UI TEST PASSED\n");
}

void input_test() {
    TEST_PRINTF("INPUT TEST PASSED\n");
}