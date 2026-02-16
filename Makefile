# =============================================================================
# Neutron Bootloader - Project Atom
# Makefile  —  ARMv8 AArch64 bare-metal Bootloader + Test Kernel  /  QEMU -machine virt
#
# Organization : serene brew
# Author       : mintRaven-05
# License      : BSD-3-Clause
#
# Install toolchain (once):
#   Arch   : sudo pacman -S aarch64-linux-gnu-gcc aarch64-linux-gnu-binutils
#   Fedora : sudo dnf install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
#   Ubuntu : sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
#   ARM    : https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
# =============================================================================

# Auto-detect toolchain prefix
ifneq ($(shell command -v aarch64-none-elf-as 2>/dev/null),)
	CROSS := aarch64-none-elf-
else ifneq ($(shell command -v aarch64-linux-gnu-as 2>/dev/null),)
	CROSS := aarch64-linux-gnu-
else ifneq ($(shell command -v aarch64-elf-as 2>/dev/null),)
	CROSS := aarch64-elf-
else
	$(error No AArch64 cross-assembler found.)
endif

CC      := $(CROSS)gcc
AS      := $(CROSS)as
LD      := $(CROSS)gcc
CPP     := $(CROSS)cpp
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump
SIZE    := $(CROSS)size

BUILD   := build

# ============================================================================
# BOOTLOADER CONFIGURATION
# ============================================================================
BOOTLOADER_SRCS_C := neutron/main.c neutron/bootloader.c driver/uart.c
BOOTLOADER_SRCS_S := boot/start.S
BOOTLOADER_LD     := linker/bootloader.ld

# KERNEL CONFIGURATION
# ============================================================================
KERNEL_SRCS_C := test_kernel/kernel_main.c driver/uart.c
KERNEL_SRCS_S := test_kernel/kernel_start.S
KERNEL_LD     := linker/kernel.ld

CPU        := cortex-a53
ARCH_FLAGS := -mcpu=$(CPU)

CFLAGS := $(ARCH_FLAGS) -std=gnu11 -O1 -Wall -Wextra \
	      -ffreestanding -fno-builtin -fno-stack-protector \
	      -fno-pie -fno-pic -nostdlib -I include

CPPFLAGS := -E -x assembler-with-cpp -D__ASSEMBLER__ -I include

ASFLAGS  := $(ARCH_FLAGS) -g

# ============================================================================
# BOOTLOADER LINKER FLAGS
# ============================================================================
BOOTLOADER_LDFLAGS := -nostdlib $(ARCH_FLAGS)          \
	                    -Wl,--no-dynamic-linker          \
	                    -Wl,-T,$(BOOTLOADER_LD)          \
	                    -Wl,-Map,$(BUILD)/bootloader.map \
	                    -lgcc

# ============================================================================
# KERNEL LINKER FLAGS
# ============================================================================
KERNEL_LDFLAGS := -nostdlib $(ARCH_FLAGS)          \
	               -Wl,--no-dynamic-linker          \
	               -Wl,-T,$(KERNEL_LD)              \
	               -Wl,-Map,$(BUILD)/kernel.map     \
	               -lgcc

# ============================================================================
# OBJECT FILES
# ============================================================================
BOOTLOADER_OBJS_C := $(patsubst %.c, $(BUILD)/%.o, $(BOOTLOADER_SRCS_C))
BOOTLOADER_OBJS_S := $(patsubst %.S, $(BUILD)/%.o, $(BOOTLOADER_SRCS_S))
BOOTLOADER_OBJS   := $(BOOTLOADER_OBJS_S) $(BOOTLOADER_OBJS_C)

KERNEL_OBJS_C := $(patsubst %.c, $(BUILD)/%.o, $(KERNEL_SRCS_C))
KERNEL_OBJS_S := $(patsubst %.S, $(BUILD)/%.o, $(KERNEL_SRCS_S))
KERNEL_OBJS   := $(KERNEL_OBJS_S) $(KERNEL_OBJS_C)

# ============================================================================
# OUTPUT FILES
# ============================================================================
BOOTLOADER_ELF := $(BUILD)/bootloader.elf
BOOTLOADER_BIN := $(BUILD)/bootloader.bin
KERNEL_ELF     := $(BUILD)/kernel.elf
KERNEL_BIN     := $(BUILD)/kernel.bin

.PHONY: all bootloader kernel qemu qemu-kernel qemu-gui dump dump-kernel size size-kernel clean

# ============================================================================
# DEFAULT TARGET: Build both bootloader and kernel
# ============================================================================
all: bootloader kernel
	@echo ""
	@echo "=== BOOTLOADER ==="
	@$(SIZE) $(BOOTLOADER_ELF)
	@echo ""
	@echo "=== KERNEL ==="
	@$(SIZE) $(KERNEL_ELF)
	@echo ""
	@echo "Build complete!"
	@echo "  Bootloader ELF : $(BOOTLOADER_ELF)"
	@echo "  Kernel ELF     : $(KERNEL_ELF)"
	@echo ""
	@echo "Run options:"
	@echo "  make qemu          # Run bootloader only"
	@echo "  make qemu-kernel   # Run bootloader + kernel"
	@echo "  make dump          # Disassemble bootloader"
	@echo "  make dump-kernel   # Disassemble kernel"

# ============================================================================
# BOOTLOADER TARGETS
# ============================================================================
bootloader: $(BOOTLOADER_ELF) $(BOOTLOADER_BIN)

$(BOOTLOADER_ELF): $(BOOTLOADER_OBJS) $(BOOTLOADER_LD)
	@echo "[LD]  $@"
	@$(LD) $(BOOTLOADER_OBJS) $(BOOTLOADER_LDFLAGS) -o $@

$(BOOTLOADER_BIN): $(BOOTLOADER_ELF)
	@echo "[BIN] $@"
	@$(OBJCOPY) -O binary $< $@

# ============================================================================
# KERNEL TARGETS
# ============================================================================
kernel: $(KERNEL_ELF) $(KERNEL_BIN)

$(KERNEL_ELF): $(KERNEL_OBJS) $(KERNEL_LD)
	@echo "[LD]  $@"
	@$(LD) $(KERNEL_OBJS) $(KERNEL_LDFLAGS) -o $@

$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "[BIN] $@"
	@$(OBJCOPY) -O binary $< $@

# ============================================================================
# COMPILATION RULES
# ============================================================================
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "[CC]  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "[CPP] $<"
	@$(CPP) $(CPPFLAGS) $< -o $(BUILD)/$*.s
	@echo "[AS]  $(BUILD)/$*.s"
	@$(AS) $(ASFLAGS) $(BUILD)/$*.s -o $@

# ============================================================================
# QEMU TARGETS
# ============================================================================

# Test bootloader only (loads at 0x40000000)
qemu: $(BOOTLOADER_ELF)
	@echo "[QEMU] Bootloader test - Ctrl-A X to quit"
	qemu-system-aarch64       \
	    -machine virt         \
	    -cpu cortex-a53       \
	    -m 256M               \
	    -nographic            \
	    -kernel $(BOOTLOADER_ELF) \
	    -serial mon:stdio

# Test both bootloader and kernel together
qemu-kernel: $(BOOTLOADER_ELF) $(KERNEL_ELF)
	@echo "[QEMU] Bootloader + Kernel test - Ctrl-A X to quit"
	qemu-system-aarch64                          \
	    -machine virt                            \
	    -cpu cortex-a53                          \
	    -m 256M                                  \
	    -nographic                               \
	    -kernel $(BOOTLOADER_ELF)                \
	    -device loader,file=$(KERNEL_BIN),addr=0x40400000 \
	    -serial mon:stdio

# GUI mode for bootloader
qemu-gui: $(BOOTLOADER_ELF)
	@echo "[QEMU] GUI mode - Bootloader"
	qemu-system-aarch64       \
	    -machine virt         \
	    -cpu cortex-a53       \
	    -m 256M               \
	    -kernel $(BOOTLOADER_ELF) \
	    -device ramfb         \
	    -serial stdio         \
	    -display gtk

# ============================================================================
# DEBUG TARGETS
# ============================================================================

# Disassemble bootloader
dump: $(BOOTLOADER_ELF)
	@$(OBJDUMP) -d -S $(BOOTLOADER_ELF) | less

# Disassemble kernel
dump-kernel: $(KERNEL_ELF)
	@$(OBJDUMP) -d -S $(KERNEL_ELF) | less

# Show bootloader size
size: $(BOOTLOADER_ELF)
	@$(SIZE) $(BOOTLOADER_ELF)

# Show kernel size
size-kernel: $(KERNEL_ELF)
	@$(SIZE) $(KERNEL_ELF)

# ============================================================================
# CLEAN TARGET
# ============================================================================
clean:
	@rm -rf $(BUILD)
	@echo "Cleaned all build artifacts."
