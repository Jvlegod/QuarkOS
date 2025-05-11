OUTPUT_ARCH(riscv)
ENTRY(_start)

USER_STACK_SIZE = 4096;     /* user stack */
KERNEL_STACK_SIZE = 1024;   /* kernel stack */
MAX_HARTS = 4;
MAX_TASKS = 1024;
HEAP_SIZE = 64M; /* heap size */

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
           | (KERNEL_STACKS) |
           | (USER_STACKS)   |
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

    .kernel_stacks (NOLOAD) : {
        PROVIDE(_kernel_stack_start = .);
        . += KERNEL_STACK_SIZE * MAX_HARTS;
        PROVIDE(_kernel_stack_top = .);
    } > RAM

    .user_stack (NOLOAD) : {
        . = ALIGN(16);
        PROVIDE(_user_stack_start = .);
        . += USER_STACK_SIZE * MAX_TASKS;
        PROVIDE(_user_stack_top = .);
    } > RAM

    .shell_commands : {
        _shell_command_start = .;
        KEEP(*(.shell_commands))
        _shell_command_end = .;
    }

    _end = .;
}