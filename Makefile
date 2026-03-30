# ===== 工具链 =====
BUILD_DIR = ./build
AS = nasm
CC = gcc
LD = ld

# ===== 参数 =====
LIB = -I lib/ -I kernel/include/ -I device/include/

ASFLAGS = -f elf
CFLAGS = -Wall $(LIB) -m32 \
    -ffreestanding \
    -fno-builtin \
    -fno-stack-protector \
    -fno-pic -fno-pie \
    -fno-asynchronous-unwind-tables \
    -fno-unwind-tables \
    -fno-exceptions \
    -fno-ident \
    -nostdlib -nostartfiles -nodefaultlibs \
    -Wall -Wextra \
    -Wstrict-prototypes -Wmissing-prototypes \
    -O2 -g -c

LDFLAGS = -m elf_i386 \
   -T linker.ld \
   -nostdlib \
   -z noexecstack \
   -Map build/kernel.map

# ===== 确保 build 目录存在
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ===== 所有目标文件 =====
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/print.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/interrupt.o $(BUILD_DIR)/init.o \
    $(BUILD_DIR)/timer.o $(BUILD_DIR)/debug.o $(BUILD_DIR)/string.o $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/memory.o \
    $(BUILD_DIR)/memfunc.o

# ===== 编译汇编文件
$(BUILD_DIR)/print.o : lib/kernel/print.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/kernel.o : kernel/kernel.S
	$(AS) $(ASFLAGS) $< -o $@

# ===== 编译 C  =====
$(BUILD_DIR)/main.o: kernel/main.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/interrupt.o: kernel/interrupt.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/init.o: kernel/init.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/timer.o: device/timer.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/debug.o: kernel/debug.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/string.o: lib/string.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/memfunc.o: lib/memfunc.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/bitmap.o: lib/kernel/bitmap.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/memory.o: kernel/memory.c
	$(CC) $(CFLAGS) $< -o $@

# ===== 链接所有目标文件
$(BUILD_DIR)/kernel.bin : $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

build: $(BUILD_DIR)/kernel.bin

vd:
	dd if=$(BUILD_DIR)/kernel.bin of=vdisk.img bs=512 count=200 seek=9 conv=notrunc

clean:
	rm -rf $(BUILD_DIR)/*

all: build
