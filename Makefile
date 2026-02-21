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
# ================================================================

# ----------------------------------------------------------------
# Toolchain
# ----------------------------------------------------------------
CROSS   ?= aarch64-linux-gnu-

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

BL_ASM_OBJS := $(patsubst %.S, $(BUILD_DIR)/%.o, $(BL_ASM_SRCS))
BL_C_OBJS   := $(patsubst %.c, $(BUILD_DIR)/%.o, $(BL_C_SRCS))
BL_OBJS     := $(BL_ASM_OBJS) $(BL_C_OBJS)

# ----------------------------------------------------------------
# Kernel sources
# ----------------------------------------------------------------
K_ASM_SRCS  := $(KERNEL_DIR)/boot/kernel_start.S
K_C_SRCS    := $(KERNEL_DIR)/kernel_main.c

K_ASM_OBJS  := $(patsubst %.S, $(BUILD_DIR)/%.o, $(K_ASM_SRCS))
K_C_OBJS    := $(patsubst %.c, $(BUILD_DIR)/%.o, $(K_C_SRCS))
K_OBJS      := $(K_ASM_OBJS) $(K_C_OBJS)

# ----------------------------------------------------------------
# Output artefacts
# ----------------------------------------------------------------
BL_ELF      := $(BUILD_DIR)/neutron.elf
BL_IMG      := kernel8.img

K_RAW_ELF   := $(BUILD_DIR)/kernel_raw.elf
K_RAW_BIN   := $(BUILD_DIR)/kernel_raw.bin
K_BIN       := atom.bin

SD_IMG      := sd.img
SD_SIZE_MB  := 64

# ----------------------------------------------------------------
# Compiler / Assembler flags
# ----------------------------------------------------------------
ARCH_FLAGS  := -march=armv8-a           \
               -mtune=cortex-a53        \
               -mgeneral-regs-only      \
               -mlittle-endian

CFLAGS      := $(ARCH_FLAGS)            \
               -ffreestanding           \
               -fno-builtin             \
               -fno-stack-protector     \
               -fno-pie                 \
               -fno-pic                 \
               -nostdlib                \
               -nostdinc                \
               -std=gnu11               \
               -Wall                    \
               -Wextra                  \
               -Werror                  \
               -O2                      \
               -g                       \
               -I$(INCLUDE_DIR)         \
               -isystem $(shell $(CC) -print-file-name=include)

ASFLAGS     := $(ARCH_FLAGS)            \
               -ffreestanding           \
               -nostdlib                \
               -I$(INCLUDE_DIR)         \
               -D__ASSEMBLER__

BL_LDFLAGS  := -nostdlib                \
               -T $(LINKER_DIR)/bootloader.ld   \
               -Map=$(BUILD_DIR)/neutron.map    \
               --no-dynamic-linker              \
               --no-warn-rwx-segments

K_LDFLAGS   := -nostdlib                \
               -T $(KERNEL_DIR)/linker/kernel.ld \
               -Map=$(BUILD_DIR)/kernel.map      \
               --no-dynamic-linker

# ----------------------------------------------------------------
# QEMU  - kernel8.img boots from -kernel, sd.img is the SD card
# ----------------------------------------------------------------
QEMU            := qemu-system-aarch64
QEMU_MACHINE    := raspi3b
QEMU_CPU        := cortex-a53
QEMU_MEM        := 1G
QEMU_GDB_PORT   := 1234

QEMU_FLAGS  := -machine $(QEMU_MACHINE) \
               -cpu $(QEMU_CPU)         \
               -m $(QEMU_MEM)           \
               -kernel $(BL_IMG)        \
               -drive file=$(SD_IMG),if=sd,format=raw \
							 -serial mon:stdio            \
               -display none

# ----------------------------------------------------------------
# Phony targets
# ----------------------------------------------------------------
.PHONY: all bootloader kernel sd-image clean \
        qemu-rpi qemu-rpi-debug disasm size help

# ----------------------------------------------------------------
# all  -  build bootloader, kernel, and sd image
# ----------------------------------------------------------------
all: bootloader kernel sd-image
	@echo ""
	@echo "Neutron build complete!"
	@echo "Bootloader : kernel8.img"
	@echo "Kernel     : atom.bin (inside sd.img)"
	@echo "Run        : make qemu-rpi"

# ----------------------------------------------------------------
# bootloader  -  kernel8.img
# ----------------------------------------------------------------
bootloader: $(BL_IMG)

$(BL_ELF): $(BL_OBJS) $(LINKER_DIR)/bootloader.ld
	@echo "[LD]  $@"
	@$(LD) $(BL_LDFLAGS) -o $@ $(BL_OBJS)
	@$(SIZE) $@

$(BL_IMG): $(BL_ELF)
	@echo "[IMG] $@"
	@$(OBJCOPY) -O binary $< $@

# ----------------------------------------------------------------
# kernel  -  atom.bin (NKRN-packed raw binary)
# ----------------------------------------------------------------
kernel: $(K_BIN)

$(K_RAW_ELF): $(K_OBJS) $(KERNEL_DIR)/linker/kernel.ld
	@echo "[LD]  $@"
	@$(LD) $(K_LDFLAGS) -o $@ $(K_OBJS)
	@$(SIZE) $@

$(K_RAW_BIN): $(K_RAW_ELF)
	@echo "[BIN] $@"
	@$(OBJCOPY) -O binary $< $@

$(K_BIN): $(K_RAW_BIN)
	@echo "[PKG] $@ (packing NKRN header)"
	@python3 pack_kernel.py $< -o $@ -n "Neutron Test Kernel"

# ----------------------------------------------------------------
# sd-image  -  create sd.img with FAT32 partition + atom.bin
#
# Tools required: dd  parted  mkfs.vfat  mcopy (mtools)
#   sudo apt install parted mtools dosfstools
# ----------------------------------------------------------------
sd-image: $(SD_IMG)

$(SD_IMG): $(K_BIN)
	@echo "[SD]  Creating $(SD_SIZE_MB) MiB SD image: $@"
	@# Create blank image
	dd if=/dev/zero of=$@ bs=1M count=$(SD_SIZE_MB) status=none

	@# Partition: MBR + one primary FAT32 partition (1MiB offset for alignment)
	@echo "[SD]  Writing MBR partition table..."
	parted -s $@                    \
	    mklabel msdos               \
	    mkpart primary fat32 1MiB 100%

	@# Format the partition in-place using mformat (no loopback needed)
	@# Partition starts at sector 2048 (1MiB / 512B)
	@echo "[SD]  Formatting FAT32..."
	mformat -i $@@@1M -F -v NEUTRON ::

	@# Copy atom.bin into the root of the FAT32 partition
	@echo "[SD]  Copying $(K_BIN) to SD image root..."
	mcopy -i $@@@1M $(K_BIN) ::ATOM.BIN

	@echo "[SD]  $(SD_IMG) ready."
	@echo "[SD]  Contents:"
	@mdir -i $@@@1M ::

# ----------------------------------------------------------------
# Compile C  (bootloader uses -I include/; kernel is self-contained)
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
# Assemble .S
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
# qemu-rpi  -  bootloader reads atom.bin from sd.img
# ----------------------------------------------------------------
qemu-rpi: all
	@echo ""
	@echo "[QEMU] Booting: kernel8.img  reading atom.bin from sd.img"
	@echo "[QEMU] Press Ctrl+A then X to quit"
	@echo ""
	$(QEMU) $(QEMU_FLAGS)

# ----------------------------------------------------------------
# qemu-rpi-debug  -  GDB stub on :$(QEMU_GDB_PORT)
# ----------------------------------------------------------------
qemu-rpi-debug: all
	@echo "[QEMU] GDB stub on port $(QEMU_GDB_PORT)"
	@echo "[GDB]  aarch64-linux-gnu-gdb $(BL_ELF)"
	@echo "       (gdb) target remote :$(QEMU_GDB_PORT)"
	$(QEMU) $(QEMU_FLAGS)           \
	    -gdb tcp::$(QEMU_GDB_PORT)  \
	    -S

# ----------------------------------------------------------------
# disasm
# ----------------------------------------------------------------
disasm: $(BL_ELF) $(K_RAW_ELF)
	@$(OBJDUMP) -d -S --demangle $(BL_ELF)    > $(BUILD_DIR)/neutron.lst
	@$(OBJDUMP) -d -S --demangle $(K_RAW_ELF) > $(BUILD_DIR)/kernel.lst
	@echo "Wrote $(BUILD_DIR)/neutron.lst and $(BUILD_DIR)/kernel.lst"

# ----------------------------------------------------------------
# size
# ----------------------------------------------------------------
size: $(BL_ELF) $(K_RAW_ELF)
	@echo "--- Bootloader ---"
	@$(SIZE) -A $(BL_ELF)
	@echo "--- Kernel ---"
	@$(SIZE) -A $(K_RAW_ELF)

# ----------------------------------------------------------------
# clean
# ----------------------------------------------------------------
clean:
	@echo "[CLN] Removing build artefacts..."
	@rm -rf $(BUILD_DIR)
	@rm -f  $(BL_IMG) $(K_BIN) $(SD_IMG)
	@echo "Clean done."

# ----------------------------------------------------------------
# help
# ----------------------------------------------------------------
help:
	@echo ""
	@echo "Neutron Bootloader - available targets:"
	@echo ""
	@echo "  make all              Build bootloader + kernel + sd.img (default)"
	@echo "  make bootloader       Build kernel8.img only"
	@echo "  make kernel           Build atom.bin only"
	@echo "  make sd-image         Create sd.img FAT32 disk with atom.bin"
	@echo "  make qemu-rpi         Boot in QEMU (SD card path)"
	@echo "  make qemu-rpi-debug   Boot with GDB stub on :$(QEMU_GDB_PORT)"
	@echo "  make disasm           Annotated disassembly for both"
	@echo "  make size             Section sizes for both"
	@echo "  make clean            Remove all build artefacts"
	@echo ""
	@echo "  SD image tools needed:"
	@echo "    sudo apt install parted mtools dosfstools"
	@echo ""
	@echo "Toolchain: CROSS=$(CROSS)"
	@echo "  Override: make CROSS=aarch64-none-elf- all"
	@echo ""
