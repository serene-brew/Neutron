# =============================================================================
# Neutron Bootloader - Project Atom
# Makefile  -  Neutron Bootloader + Minimal Test Kernel + SD Image (ARMv8-A)
#
# Organization : serene brew
# Author       : mintRaven-05
# License      : BSD-3-Clause
#
# Target  : Raspberry Pi Zero 2W  (BCM2710, ARMv8 / AArch64)
# Toolchain: aarch64-linux-gnu-  (or aarch64-none-elf-)
# QEMU    : qemu-system-aarch64  -machine raspi3b
# =============================================================================

# ----------------------------------------------------------------
# Toolchain
# ----------------------------------------------------------------
CROSS   ?= aarch64-none-elf-

CC      := $(CROSS)gcc
AS      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump
SIZE    := $(CROSS)size

# ----------------------------------------------------------------
# Directories
# ----------------------------------------------------------------
BOOT_DIR    := boot
DRIVER_DIR  := driver
NEUTRON_DIR := neutron
INCLUDE_DIR := include
LINKER_DIR  := linker
KERNEL_DIR  := test_kernel
BUILD_DIR   := build
BIN_DIR     := bin

# ----------------------------------------------------------------
# Configuration (generate include/config.h and build.mk from build.cfg)
# ----------------------------------------------------------------
CONFIG_H := $(INCLUDE_DIR)/config.h
BUILD_MK := build.mk

# Generate both config.h and build.mk from build.cfg. build.mk is included
# below; making it a target and a prerequisite of 'all' ensures it is rebuilt
# when build.cfg changes, and Make re-execs so the new KERNEL_FILENAME etc.
# are used for the rest of the build.
build.mk $(CONFIG_H): build.cfg
	@echo "[CFG] Generating $(CONFIG_H) and $(BUILD_MK) from build.cfg"
	@python3 gen_config.py

-include $(BUILD_MK)

KERNEL_FILENAME ?= ATOM.BIN
EMBED_KERNEL ?= 0

# ----------------------------------------------------------------
# Bootloader sources
# ----------------------------------------------------------------
BL_ASM_SRCS := $(BOOT_DIR)/start.S
BL_C_SRCS   := $(DRIVER_DIR)/gpio.c        \
                $(DRIVER_DIR)/uart.c        \
                $(DRIVER_DIR)/mbox.c        \
                $(DRIVER_DIR)/sdcard.c      \
                $(DRIVER_DIR)/fat32.c       \
                $(NEUTRON_DIR)/bootloader.c \
                $(NEUTRON_DIR)/main.c

BL_ASM_OBJS := $(patsubst %.S,$(BUILD_DIR)/%.o,$(BL_ASM_SRCS))
BL_C_OBJS   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(BL_C_SRCS))
BL_OBJS     := $(BL_ASM_OBJS) $(BL_C_OBJS)

ifeq ($(EMBED_KERNEL),1)
BL_EMBED_OBJ := $(BUILD_DIR)/embedded_kernel.o
BL_OBJS      += $(BL_EMBED_OBJ)
endif

# ----------------------------------------------------------------
# Kernel sources
# ----------------------------------------------------------------
K_ASM_SRCS  := $(KERNEL_DIR)/boot/kernel_start.S
K_C_SRCS    := $(KERNEL_DIR)/kernel_main.c

K_ASM_OBJS  := $(patsubst %.S,$(BUILD_DIR)/%.o,$(K_ASM_SRCS))
K_C_OBJS    := $(patsubst %.c,$(BUILD_DIR)/%.o,$(K_C_SRCS))
K_OBJS      := $(K_ASM_OBJS) $(K_C_OBJS)

# ----------------------------------------------------------------
# Output artefacts
# ----------------------------------------------------------------
BL_ELF      := $(BUILD_DIR)/neutron.elf
BL_IMG      := $(BIN_DIR)/kernel8.img

K_RAW_ELF   := $(BUILD_DIR)/kernel_raw.elf
K_RAW_BIN   := $(BUILD_DIR)/kernel_raw.bin
K_BIN       := $(BIN_DIR)/atom.bin

SD_IMG      := $(BIN_DIR)/sd.img
SD_SIZE_MB  := 64

# ----------------------------------------------------------------
# Compiler / Assembler flags
# ----------------------------------------------------------------
ARCH_FLAGS  := -march=armv8-a \
               -mtune=cortex-a53 \
               -mgeneral-regs-only \
               -mlittle-endian

CFLAGS      := $(ARCH_FLAGS) \
               -ffreestanding \
               -fno-builtin \
               -fno-stack-protector \
               -fno-pie \
               -fno-pic \
               -nostdlib \
               -nostdinc \
               -std=gnu11 \
               -Wall \
               -Wextra \
               -Werror \
               -O2 \
               -g \
               -I$(INCLUDE_DIR) \
               -isystem $(shell $(CC) -print-file-name=include)

ASFLAGS     := $(ARCH_FLAGS) \
               -ffreestanding \
               -nostdlib \
               -I$(INCLUDE_DIR) \
               -D__ASSEMBLER__

BL_LDFLAGS  := -nostdlib \
               -T $(LINKER_DIR)/bootloader.ld \
               -Map=$(BUILD_DIR)/neutron.map \
               --no-dynamic-linker \
               --no-warn-rwx-segments

K_LDFLAGS   := -nostdlib \
               -T $(KERNEL_DIR)/linker/kernel.ld \
               -Map=$(BUILD_DIR)/kernel.map \
               --no-dynamic-linker

# ----------------------------------------------------------------
# QEMU
# ----------------------------------------------------------------
QEMU            := qemu-system-aarch64
QEMU_MACHINE    := raspi3b
QEMU_CPU        := cortex-a53
QEMU_MEM        := 1G
QEMU_GDB_PORT   := 1234

QEMU_SD_DRIVE   := -drive file=$(SD_IMG),if=sd,format=raw
ifeq ($(EMBED_KERNEL),1)
QEMU_SD_DRIVE   :=
endif

QEMU_FLAGS  := -machine $(QEMU_MACHINE) \
               -cpu $(QEMU_CPU) \
               -m $(QEMU_MEM) \
               -kernel $(BL_IMG) \
               $(QEMU_SD_DRIVE) \
               -serial mon:stdio \
               -display none

# ----------------------------------------------------------------
# Phony targets
# ----------------------------------------------------------------
.PHONY: all bootloader kernel sd-image clean \
        qemu-rpi qemu-rpi-debug disasm size help

# ----------------------------------------------------------------
# Default target
# ----------------------------------------------------------------
ifeq ($(EMBED_KERNEL),1)
all: build.mk bootloader kernel
else
all: build.mk bootloader kernel sd-image
endif
	@echo ""
	@echo "Neutron build complete!"
	@echo "Bootloader : kernel8.img"
	@echo "Kernel     : $(KERNEL_FILENAME) ($(if $(filter 1,$(EMBED_KERNEL)),embedded,inside sd.img))"
	@echo "Run        : make qemu-rpi"

# ----------------------------------------------------------------
# Bootloader
# ----------------------------------------------------------------
bootloader: $(CONFIG_H) $(BL_IMG)

$(BL_C_OBJS): $(CONFIG_H)

$(BL_ELF): $(BL_OBJS)
	@mkdir -p $(BUILD_DIR)
	@echo "[LD]  $@"
	@$(LD) $(BL_LDFLAGS) -o $@ $(BL_OBJS)
	@$(SIZE) $@

$(BL_IMG): $(BL_ELF)
	@mkdir -p $(BIN_DIR)
	@echo "[IMG] $@"
	@$(OBJCOPY) -O binary $< $@

ifeq ($(EMBED_KERNEL),1)
$(BL_EMBED_OBJ): $(K_BIN)
	@mkdir -p $(dir $@)
	@echo "[EMBED] $< -> $@"
	@$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 $< $@
endif

# ----------------------------------------------------------------
# Kernel
# ----------------------------------------------------------------
kernel: $(K_BIN)

$(K_RAW_ELF): $(K_OBJS)
	@mkdir -p $(BUILD_DIR)
	@echo "[LD]  $@"
	@$(LD) $(K_LDFLAGS) -o $@ $(K_OBJS)
	@$(SIZE) $@

$(K_RAW_BIN): $(K_RAW_ELF)
	@echo "[BIN] $@"
	@$(OBJCOPY) -O binary $< $@

$(K_BIN): $(K_RAW_BIN)
	@mkdir -p $(BIN_DIR)
	@echo "[PKG] $@ (packing NKRN header)"
	@python3 pack_kernel.py $< -o $@ -n "Neutron Test Kernel"

# ----------------------------------------------------------------
# SD Image
# ----------------------------------------------------------------
sd-image: $(SD_IMG)

$(SD_IMG): $(K_BIN)
	@mkdir -p $(BIN_DIR)
	@echo "[SD]  Creating $(SD_SIZE_MB) MiB SD image: $@"
	dd if=/dev/zero of=$@ bs=1M count=$(SD_SIZE_MB) status=none
	parted -s $@ mklabel msdos
	parted -s $@ mkpart primary fat32 1MiB 100%
	mformat -i $@@@1M -F -v NEUTRON ::
	mcopy -i $@@@1M $(K_BIN) ::$(KERNEL_FILENAME)
	@echo "[SD]  Contents:"
	@mdir -i $@@@1M ::

# ----------------------------------------------------------------
# Compile C
# ----------------------------------------------------------------
$(BUILD_DIR)/$(KERNEL_DIR)/%.o: $(KERNEL_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "[CC]  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "[CC]  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

# ----------------------------------------------------------------
# Assemble
# ----------------------------------------------------------------
$(BUILD_DIR)/$(KERNEL_DIR)/%.o: $(KERNEL_DIR)/%.S
	@mkdir -p $(dir $@)
	@echo "[AS]  $<"
	@$(AS) $(ASFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "[AS]  $<"
	@$(AS) $(ASFLAGS) -c -o $@ $<

# ----------------------------------------------------------------
# QEMU
# ----------------------------------------------------------------
qemu-rpi: all
	$(QEMU) $(QEMU_FLAGS)

# ----------------------------------------------------------------
# Size
# ----------------------------------------------------------------
size: $(BL_ELF) $(K_RAW_ELF)
	@echo "--- Bootloader ---"
	@$(SIZE) -A $(BL_ELF)
	@echo "--- Kernel ---"
	@$(SIZE) -A $(K_RAW_ELF)

# ----------------------------------------------------------------
# Clean
# ----------------------------------------------------------------
clean:
	@rm -rf $(BUILD_DIR)/*
	@rm -rf $(BIN_DIR)/*
	@echo "Clean done."

# ----------------------------------------------------------------
# Help
# ----------------------------------------------------------------
help:
		@echo ""
		@echo "Neutron Bootloader - available targets:"
		@echo ""
		@echo "  make all              Build bootloader + kernel $(if $(filter 1,$(EMBED_KERNEL)),(embedded kernel),+ sd.img) (default)"
		@echo "  make bootloader       Build kernel8.img only"
		@echo "  make kernel           Build atom.bin only"
		@echo "  make sd-image         Create sd.img FAT32 disk with $(KERNEL_FILENAME)"
		@echo "  make qemu-rpi         Boot in QEMU (SD card path)"
		@echo "  make size             Section sizes for both"
		@echo "  make clean            Remove all build artefacts"
		@echo ""
		@echo "  SD image tools needed:"
		@echo "    sudo apt install parted mtools dosfstools"
		@echo ""
		@echo "Toolchain: CROSS=$(CROSS)"
		@echo "  Override: make CROSS=aarch64-none-elf- all"
		@echo ""
