OUTPUT_ARCH(riscv)
ENTRY(_start)

HART_STACK_SIZE = 1024;
MAX_HARTS = 4;

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

    .stack : {
        _stack_start = .;
        . += HART_STACK_SIZE * MAX_HARTS;
        _stack_end = .;
    } > RAM

    _end = .;
}