OUTPUT_ARCH(riscv)
ENTRY(_start)

HART_STACK_SIZE = 1024;
MAX_HARTS = 4;
HEAP_SIZE = 64M; /* change heap size */

/*
0x80000000 +-----------------+
           |     .text       |
           +-----------------+
           |   .rodata       |
           +-----------------+
           |    .data        |
           +-----------------+
           |     .bss        |
           +-----------------+
           |     .heap       | ← _heap_start
           | (HEAP_SIZE)     |
           +-----------------+
           |    .stack       | ← _stack_start
           | (HART_STACKS)   |
0x88000000 +-----------------+ (assume 128MB)
*/

MEMORY {
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS {
    .text : {
        *(.text.init)
        *(.text .text.*)
    } > RAM

    .rodata : {
        *(.rodata .rodata.*)
    } > RAM

    .data : {
        *(.data .data.*)
    } > RAM

    .bss : {
        _sbss = .;
        *(.bss .bss.*)
        *(COMMON)
        _ebss = .;
    } > RAM

    .heap : {
        _heap_start = .;
        . += HEAP_SIZE;
        _heap_end = .;
    } > RAM

    .stack : {
        _stack_start = .;
        . += HART_STACK_SIZE * MAX_HARTS;
        _stack_end = .;
    } > RAM

    _end = .;
}