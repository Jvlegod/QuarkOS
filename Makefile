# QuarkOS Makefile

TOOLCHAIN  = riscv64-unknown-elf-

# target arch（default riscv）
ARCH ?= riscv
BUILD_TYPE ?= debug

# General flags
COMMON_FLAGS = -ffreestanding -nostdlib -Wall -Wextra
WARNINGS     = -Wno-unused-parameter -Wno-missing-field-initializers

# arch configs
ifeq ($(ARCH),riscv)
    CC         = $(TOOLCHAIN)gcc
    AS         = $(TOOLCHAIN)as
    LD         = $(TOOLCHAIN)ld
    OBJCOPY    = $(TOOLCHAIN)objcopy
    CFLAGS     = -march=rv64gc -mabi=lp64 -mcmodel=medany
    ASFLAGS    = -march=rv64gc -mabi=lp64
    LINK_SCRIPT = src/arch/riscv64/linker.ld
    # maybe we can change here to support other platform.
    PLATFORM   = qemu-virt
else
    $(error Unsupported architecture: $(ARCH))
endif

# optimals
ifeq ($(BUILD_TYPE),debug)
    CFLAGS += -O2 -ggdb3
    LDFLAGS += -nostdlib
else ifeq ($(BUILD_TYPE),release)
    CFLAGS += -O2 -DNDEBUG
    LDFLAGS += -flto
endif

# dir path
SRC_DIR     = src
BUILD_DIR   = build/$(ARCH)
KERNEL_ELF  = $(BUILD_DIR)/quarkos.elf
KERNEL_BIN  = $(BUILD_DIR)/quarkos.bin

# src code
C_SRCS     := $(shell find $(SRC_DIR) -name '*.c')
ASM_SRCS   := $(shell find $(SRC_DIR) -name '*.S')
OBJS       := $(addprefix $(BUILD_DIR)/,$(C_SRCS:.c=.o) $(ASM_SRCS:.S=.o))

# include file
INCLUDE_DIRS := $(shell find $(SRC_DIR) -type d)
INCLUDE_FLAGS := $(addprefix -I,$(INCLUDE_DIRS))

CFLAGS += $(INCLUDE_FLAGS)

all: $(KERNEL_BIN)

# gdb
QEMU_GDB_PORT = 1234
GDBINIT_FILE = $(BUILD_DIR)/.gdbinit

# gdb init
$(GDBINIT_FILE):
	@echo "Automatic generation of GDB initialization files..."
	@echo "set architecture riscv:rv64" > $@
	@echo "target remote :$(QEMU_GDB_PORT)" >> $@
	@echo "file $(KERNEL_ELF)" >> $@
	@echo "b _start" >> $@

debug: $(KERNEL_ELF) $(GDBINIT_FILE)
	@echo "Start QEMU Debug Server (Ctrl+A X to exit)..."
	@echo "In another terminal: make gdb"
	qemu-system-riscv64 -bios none -machine virt -kernel $(KERNEL_ELF) \
	    -nographic -S -gdb tcp::$(QEMU_GDB_PORT)

# GDB remote
GDB = riscv64-unknown-elf-gdb
gdb:
	@echo "Starting the GDB Debugger..."
	$(GDB) -x $(GDBINIT_FILE)


$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(COMMON_FLAGS) $(WARNINGS) -I$(SRC_DIR) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) $(COMMON_FLAGS) -I$(SRC_DIR) -c $< -o $@

$(KERNEL_ELF): $(OBJS)
	$(LD) -T $(LINK_SCRIPT) $(LDFLAGS) $^ -o $@

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

.PHONY: all clean run


clean:
	rm -rf build

run: all
	/usr/bin/qemu-system-riscv64 -bios none \
	-machine virt \
	-m 256M \
	-smp 1 \
	-drive id=hd0,file=disk.img,if=none,format=raw \
	-device virtio-blk-device,drive=hd0,bus=virtio-mmio-bus.0 \
	-device virtio-keyboard-device \
	-kernel $(KERNEL_ELF) \
	-monitor stdio \

DEPFILES := $(OBJS:.o=.d)
-include $(DEPFILES)