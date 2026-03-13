# =============================================================================
# Neutron Bootloader - Project Atom
# Makefile  -  Neutron Bootloader + Minimal Test Kernel + SD Image (ARMv8-A)
#
# Organization : serene brew
# Author       : mintRaven-05
# License      : BSD-3-Clause
#
# Target  : Raspberry Pi Zero 2W  (BCM2710, ARMv8 / AArch64)
# Toolchain: aarch64-none-elf-, parted, mtools, dosfstools, qemu-system-aarch64
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
INCLUDE_DIR := internal
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

KERNEL_FILENAME ?= atom.bin
KERNEL_VERSION ?= v1.0
KERNEL_VERSION_MAJOR ?= 1
KERNEL_VERSION_MINOR ?= 0
EMBED_KERNEL ?= 0
$(info [CFG] EMBED_KERNEL: $(if $(filter 1,$(EMBED_KERNEL)),true,false))

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
K_ASM_SRCS  = $(KERNEL_DIR)/boot/kernel_start.S
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
# Packed kernel image to boot (host file).
#
# - Default (no override): build the bundled test_kernel and pack it to:
#     bin/$(KERNEL_FILENAME)
# - If you pass K_BIN on the command line, Neutron will use that *prebuilt*
#   packed NKRN image and will NOT build test_kernel:
#     make all K_BIN=/path/to/prebuilt_packed.bin
K_BIN_BUILT := $(BIN_DIR)/$(KERNEL_FILENAME)
K_BIN       ?= $(K_BIN_BUILT)

USE_PREBUILT_KERNEL := 0
ifeq ($(origin K_BIN),command line)
USE_PREBUILT_KERNEL := 1
endif

# PACK=1  alongside K_BIN=<path> : treat the provided path as a raw (unpacked)
#          binary and run pack_kernel.py on it before use.
# PACK=0  (default) : treat K_BIN as an already-packed NKRN image.
# NOTE: When using neutron.sh / neutron.ps1, pass --pack instead of PACK=1.
PACK ?= 0

ifeq ($(USE_PREBUILT_KERNEL),1)
ifeq ($(PACK),1)
K_BIN_SRC := $(K_BIN_BUILT)
else
K_BIN_SRC := $(K_BIN)
endif
else
K_BIN_SRC := $(K_BIN_BUILT)
endif

# Fixed path used for embedded-kernel builds to keep objcopy-generated
# symbols stable regardless of the host path of the packed kernel.
KERNEL_EMBED_BIN := $(BUILD_DIR)/kernel_embed.bin

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
               -Iinclude \
               -isystem $(shell $(CC) -print-file-name=include)

ASFLAGS     := $(ARCH_FLAGS) \
               -ffreestanding \
               -nostdlib \
               -I$(INCLUDE_DIR) \
               -Iinclude \
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
        qemu-rpi qemu-rpi-debug disasm size help \
        check-tools check-cross-tools check-python check-sd-tools check-qemu

# ----------------------------------------------------------------
# Default target
# ----------------------------------------------------------------
ifeq ($(EMBED_KERNEL),1)
all: check-tools build.mk bootloader kernel
else
all: check-tools build.mk bootloader kernel sd-image
endif
	@echo ""
	@echo "==========================================="
	@echo "Neutron build complete!"
	@echo "Run         : make qemu-rpi"
	@echo "Kernel      : $(KERNEL_FILENAME) ($(if $(filter 1,$(EMBED_KERNEL)),embedded,inside sd.img))"
	@echo "Bootloader  : kernel8.img"
	@echo "EMBED_KERNEL: $(if $(filter 1,$(EMBED_KERNEL)),true,false)"
	@echo "==========================================="

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
$(KERNEL_EMBED_BIN): $(K_BIN_SRC)
	@mkdir -p $(dir $@)
	@echo "[EMBED] staging $< -> $@"
	@cp $< $@

$(BL_EMBED_OBJ): $(KERNEL_EMBED_BIN)
	@mkdir -p $(dir $@)
	@echo "[EMBED] $< -> $@"
	@$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 $< $@
endif

# ----------------------------------------------------------------
# Kernel
# ----------------------------------------------------------------
ifeq ($(USE_PREBUILT_KERNEL),1)
ifeq ($(PACK),1)
kernel: $(K_BIN_BUILT)
$(K_BIN_BUILT): $(K_BIN)
	@mkdir -p $(dir $@)
	@echo "[PKG] $@ (packing external kernel: $<)"
	@python3 pack_kernel.py $< -o $@ -n "External Kernel" \
		--version-major $(KERNEL_VERSION_MAJOR) --version-minor $(KERNEL_VERSION_MINOR)
else
kernel:
	@test -f "$(K_BIN_SRC)" || (echo "[KERNEL] FATAL: prebuilt packed kernel not found: $(K_BIN_SRC)" && exit 1)
	@echo "[KERNEL] Using prebuilt packed kernel: $(K_BIN_SRC)"
endif
else
kernel: $(K_BIN_BUILT)
endif

$(K_RAW_ELF): $(K_OBJS)
	@mkdir -p $(BUILD_DIR)
	@echo "[LD]  $@"
	@$(LD) $(K_LDFLAGS) -o $@ $(K_OBJS)
	@$(SIZE) $@

$(K_RAW_BIN): $(K_RAW_ELF)
	@echo ""
	@echo "[BIN] $@"
	@$(OBJCOPY) -O binary $< $@


# Only build K_BIN_BUILT from the test_kernel sources when NOT using a
# prebuilt/external kernel. When USE_PREBUILT_KERNEL=1 + PACK=1 the rule
# above (K_BIN_BUILT: K_BIN) already covers this; leaving the rule
# unconditional would give Make two recipes for the same target.
ifeq ($(USE_PREBUILT_KERNEL),0)
$(K_BIN_BUILT): $(K_RAW_BIN)
	@mkdir -p $(dir $@)
	@echo "[PKG] $@ (packing NKRN header)"
	@python3 pack_kernel.py $< -o $@ -n "Neutron Test Kernel" \
		--version-major $(KERNEL_VERSION_MAJOR) --version-minor $(KERNEL_VERSION_MINOR)
endif

# ----------------------------------------------------------------
# SD Image
# ----------------------------------------------------------------
sd-image: $(SD_IMG)

$(SD_IMG): $(K_BIN_SRC)
	@mkdir -p $(dir $@)
	@echo "[SD]  Creating $(SD_SIZE_MB) MiB SD image: $@"
	@echo "  -- dd if=/dev/zero of=$@ bs=1M count=$(SD_SIZE_MB) status=none"
	@dd if=/dev/zero of=$@ bs=1M count=$(SD_SIZE_MB) status=none
	@echo "  -- parted -s $@ mklabel msdos"
	@parted -s $@ mklabel msdos
	@echo "  -- parted -s $@ mkpart primary fat32 1MiB 100%"
	@parted -s $@ mkpart primary fat32 1MiB 100%
	@echo "  -- mformat -i $@@@1M -F -v NEUTRON ::"
	@mformat -i $@@@1M -F -v NEUTRON ::
	@echo "  -- mcopy -i $@@@1M $(K_BIN_SRC) ::$(KERNEL_FILENAME)"
	@mcopy -i $@@@1M $(K_BIN_SRC) ::$(KERNEL_FILENAME)
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
qemu-rpi: check-qemu
	$(QEMU) $(QEMU_FLAGS)

qemu-rpi-build: all check-qemu
	$(QEMU) $(QEMU_FLAGS)
# ----------------------------------------------------------------
# Tool checks
# ----------------------------------------------------------------
check-tools: check-cross-tools check-python
ifeq ($(EMBED_KERNEL),0)
check-tools: check-sd-tools
endif
	@echo "[CHK] All required tools present."
	@echo ""

check-cross-tools:
	@echo "[CHK] Checking cross-toolchain (CROSS=$(CROSS))..."
	@for tool in '$(CC)' '$(LD)' '$(OBJCOPY)' '$(OBJDUMP)' '$(SIZE)'; do \
	    command -v $$tool >/dev/null 2>&1 \
	        || { echo "  [MISSING] $$tool"; \
	             echo "  Hint: install the aarch64-none-elf- toolchain"; \
	             exit 1; }; \
	    echo "  [OK] $$tool"; \
	done

check-python:
	@echo "[CHK] Checking Python..."
	@command -v python3 >/dev/null 2>&1 \
	    || { echo "  [MISSING] python3"; \
	         echo "  Hint: sudo apt install python3"; \
	         exit 1; }
	@echo "  [OK] python3"

check-sd-tools:
	@echo "[CHK] Checking SD image tools..."
	@for tool in dd parted mformat mcopy mdir; do \
	    command -v $$tool >/dev/null 2>&1 \
	        || { echo "  [MISSING] $$tool"; \
	             echo "  Hint: sudo apt install parted mtools dosfstools"; \
	             exit 1; }; \
	    echo "  [OK] $$tool"; \
	done

check-qemu:
	@echo "[CHK] Checking QEMU..."
	@command -v $(QEMU) >/dev/null 2>&1 \
	    || { echo "  [MISSING] $(QEMU)"; \
	         echo "  Hint: sudo apt install qemu-system-arm"; \
	         exit 1; }
	@echo "  [OK] $(QEMU)"

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
		@echo "  make kernel           Build kernel.bin only"
		@echo "  make sd-image         Create sd.img FAT32 disk with $(KERNEL_FILENAME)"
		@echo "  make qemu-rpi         Boot in QEMU (SD card path)"
		@echo "  make size             Section sizes for both"
		@echo "  make clean            Remove all build artefacts"
		@echo ""
		@echo "External kernel options:"
		@echo "  K_BIN=<path>          Use a prebuilt packed NKRN kernel image (skips test_kernel build)"
		@echo "  K_BIN=<path> PACK=1   Use a raw (unpacked) binary and pack it with pack_kernel.py"
		@echo ""
		@echo "  SD image tools needed:"
		@echo "    sudo apt install parted mtools dosfstools"
		@echo ""
		@echo "Toolchain: CROSS=$(CROSS)"
		@echo "  Override: make CROSS=aarch64-none-elf- all"
		@echo ""
