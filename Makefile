# ===== 工具链 =====
BUILD_DIR = ./build
ENTRY_POINT = 0xc0001500
AS = nasm
CC = gcc
LD = ld

# ===== 参数 =====
LIB = -I lib/ -I lib/kernel/ -I kernel/

# ASFLAGS = -f elf (此参数在mbr和loader完全完成后加入)
CFLAGS = -Wall $(LIB) -c -fno-builtin -m32 -fno-stack-protector \
         -W -Wstrict-prototypes -Wmissing-prototypes

LDFLAGS = -m elf_i386 -Ttext $(ENTRY_POINT) -e main

# ===== 目标文件 =====
KERNEL_OBJ = $(BUILD_DIR)/main.o
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
MBR_BIN    = $(BUILD_DIR)/mbr.bin
LOADER_BIN = $(BUILD_DIR)/loader.bin

# ===== 默认目标 =====
all : mk_dir $(MBR_BIN) $(LOADER_BIN) $(KERNEL_BIN)

# ===== 创建目录 =====
mk_dir:
	if [ ! -d $(BUILD_DIR) ];then mkdir $(BUILD_DIR);fi

# ===== boot ===== (这一部分在mbr和loader完全完成后删除)
$(MBR_BIN): boot/mbr.S
	$(AS) -I boot/include/ -o $@ $<

$(LOADER_BIN): boot/loader.S
	$(AS) -I boot/include/ -o $@ $<

# ===== kernel =====
$(KERNEL_OBJ): kernel/main.c
	$(CC) $(CFLAGS) $< -o $@

$(KERNEL_BIN): $(KERNEL_OBJ)
	$(LD) $(LDFLAGS) $< -o $@

# ===== 写入虚拟磁盘 =====
hd:
	dd if=$(MBR_BIN) of=vdisk.img bs=512 count=1 conv=notrunc
	dd if=$(LOADER_BIN) of=vdisk.img bs=512 count=4 seek=2 conv=notrunc
	dd if=$(KERNEL_BIN) of=vdisk.img bs=512 count=200 seek=9 conv=notrunc

# ===== 清理 =====
clean:
	find $(BUILD_DIR) -mindepth 1 -delete