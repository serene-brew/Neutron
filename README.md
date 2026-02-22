<div align=center>
<img width="500" height="500" alt="Neutron_Logo_for_Github" src="https://github.com/user-attachments/assets/8b634af5-5973-4994-9cdc-d485141269e3" />

### A Piece of Project atom
### v1.1.2
</div>

---
<div align=center>
<a href="https://discord.gg/vdUtjTSQF4"><img src="https://invidget.switchblade.xyz/vdUtjTSQF4"></a>
</div>

ARMv8 AArch64 bare-metal bootloader for **Raspberry Pi Zero 2W** (and compatible boards). Runs in **QEMU** using the **raspi3b** machine, which emulates the same BCM2837-style peripherals and address map. Loads a kernel from the SD card (FAT32) and transfers control with a boot-info structure.

>[!IMPORTANT]
> ## For Developer Manual and Cross Platform Compilation (Docker Build over Windows, Linux and MacOS)
> There are build options as well for developers which can be configured from `build.cfg`. For a detailed guide check out the following documentation below <br>
> https://neutron-wiki.pages.dev/ 

## Overview

Neutron is a minimal yet functional bootloader that:

- Initializes CPU state on ARMv8 AArch64 (EL2 to EL1, parks secondary cores)
- Sets up serial communication (PL011 UART at **0x3F201000**, 115200 baud, 48 MHz clock)
- Queries board revision and ARM memory size via the VideoCore mailbox
- Initializes the SD card and mounts the first FAT32 partition
- Loads a packed kernel image (**ATOM.BIN**) from the SD card into a staging area at **0x100000**
- Validates the NKRN header (magic, CRC32), copies the payload to the load address (**0x200000**), and fills a `boot_info_t` at **0x1000**
- Jumps to the kernel entry point with `x0` = pointer to `boot_info_t`
- Provides debug output throughout execution

Designed for educational purposes, QEMU simulation (`-machine raspi3b`), and deployment on Raspberry Pi Zero 2W (or Pi 3B) with an SD card.

---

## Host Machine setup

### Requirements

- **AArch64 cross-compiler** (default prefix: `aarch64-none-elf-`)

```bash
# Install required tools
sudo apt update
sudo apt install -y wget xz-utils ca-certificates

# Download the toolchain (direct link)
wget https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-aarch64-none-elf.tar.xz

# Extract to /opt
sudo mkdir -p /opt/aarch64-none-elf
sudo tar -xf arm-gnu-toolchain-14.2.rel1-x86_64-aarch64-none-elf.tar.xz \
    --strip-components=1 -C /opt/aarch64-none-elf

# Add to PATH permanently
echo 'export PATH=/opt/aarch64-none-elf/bin:$PATH' >> ~/.bashrc
source ~/.bashrc

# Verify
aarch64-none-elf-gcc --version
```

- **QEMU** with ARM support (`qemu-system-aarch64`)
  - Ubuntu/Debian: `sudo apt install qemu-system`
  - Arch: `sudo pacman -S qemu-system-aarch64`
  - Fedora: `sudo dnf install qemu-system-aarch64`
- **GNU Make**
- **SD image tools** (for building the FAT32 disk image):
  - `parted`, `mtools`, `dosfstools`
  - Ubuntu/Debian: `sudo apt install parted mtools dosfstools`
  - Arch: `sudo pacman -S parted mtools dosfstools`
  - Fedora: `sudo dnf install parted mtools dosfstools`

### Building

```bash
make clean
make all        # build bootloader + kernel (+ sd.img unless embed-kernel = true)
make bootloader # compile only kernel8.img
make kernel     # compile only atom.bin (raw kernel + NKRN pack)
make sd-image   # create sd.img (FAT32; kernel filename from build.cfg)
```

Build behaviour depends on **`build.cfg`**: the kernel filename (e.g. `ATOM.BIN` or `CUSTOM.BIN`) and **`embed-kernel`** (if `true`, the kernel is embedded in `kernel8.img` and no SD image is built by default). This generates:

- **`bin/kernel8.img`** — Bootloader binary (loaded by GPU / QEMU at 0x80000); with `embed-kernel = true` it contains the packed kernel.
- **`bin/atom.bin`** — Packed test kernel (NKRN header + payload). On SD it must be named as in `kernel_filename` (e.g. `ATOM.BIN`).
- **`bin/sd.img`** — 64 MiB FAT32 disk with the kernel in the root (built by `make sd-image` or `make all` when `embed-kernel = false`).

### Running in QEMU

```bash
make qemu-rpi   # build all, then boot with kernel8.img + sd.img
```

QEMU is invoked with `-machine raspi3b`, `-cpu cortex-a53`, 1 GiB RAM, and `-kernel bin/kernel8.img`. If `embed-kernel` is `false`, a drive is added: `-drive file=bin/sd.img,if=sd,format=raw`. Serial I/O goes to the terminal (`-serial mon:stdio`).

### Running on real hardware (Raspberry Pi Zero 2W / Pi 3B)

1. Prepare an SD card with the official Raspberry Pi boot files: `bootcode.bin`, `start.elf`, `fixup.dat` (and optionally `config.txt`).
2. Copy **`bin/kernel8.img`** to the SD card as the kernel image (rename if your setup expects a specific name).
3. Ensure the first partition is FAT32 and contains the packed kernel in the root directory under the name set in `build.cfg` (e.g. **ATOM.BIN** or **CUSTOM.BIN**). You can use `bin/sd.img`’s first partition as a reference, or copy `bin/atom.bin` onto the card with that name.
4. Boot the board; the GPU will load `kernel8.img` at 0x80000 and hand off to Neutron, which then loads the kernel from the SD card and jumps to it.

---

## Project Structure

```
Neutron/
  boot/              - Power-on assembly (start.S): EL2→EL1, BSS, entry
  driver/            - Hardware drivers (UART, GPIO, mailbox, SD card, FAT32)
  include/           - Headers (platform.h, bootloader.h, uart.h, etc.)
  linker/            - Bootloader linker script (0x80000)
  neutron/           - Bootloader C (main.c, bootloader.c)
  test_kernel/       - Minimal test kernel (boot + linker + kernel_main.c)
  Makefile           - Build configuration
  build.cfg          - Build configuration (kernel filename, addresses, logging, branding)
  gen_config.py      - Generates include/config.h from build.cfg
  pack_kernel.py     - NKRN kernel image packer (header + CRC32)
  neutron.ps1        - Docker-based CLI for Windows (build, run, emu, shell)
  Dockerfile         - Build environment (Ubuntu, aarch64 toolchain, mtools)
  LICENSE            - BSD-3-Clause
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed documentation. For build-time configuration (kernel filename, addresses, logging, embedding) see [Neutron Developer's Manual](https://neutron-wiki.pages.dev).

---

## Key Components

| Component | Role |
|-----------|------|
| **boot/start.S** | CPU init, exception level drop (EL2→EL1), park secondaries, BSS zero, call `neutron_main` |
| **neutron/main.c** | UART init, banner, mailbox (board rev / ARM mem), SD init, FAT32 mount, load ATOM.BIN, validate NKRN, `bl_load_kernel`, `bl_boot_kernel` |
| **neutron/bootloader.c** | NKRN validation, CRC32 check, copy payload to load address, fill `boot_info_t` at 0x1000, `bl_boot_kernel` (jump with x0 = boot_info) |
| **driver/uart.c** | PL011 UART at 0x3F201000 (GPIO 14/15 ALT0), 115200 8N1 |
| **driver/gpio.c** | BCM2837 GPIO (function select, pull-up/down) |
| **driver/mbox.c** | VideoCore mailbox (board revision, ARM memory size) |
| **driver/sdcard.c** | SD/MMC card init and block read |
| **driver/fat32.c** | Read-only FAT32 (MBR, BPB, root dir, file read) |
| **include/platform.h** | BCM2710/BCM2837 memory map (0x80000, 0x100000, 0x200000, MMIO 0x3F000000) |
| **test_kernel/** | Minimal kernel: prints `boot_info_t`, then heartbeat dots over UART |

---

## Memory and Address Map

- **0x80000** — Bootloader (`kernel8.img`) load address; stack grows downward from here.
- **0x100000** — Staging: FAT32 file **ATOM.BIN** is read here; bootloader then parses the NKRN header and copies the payload to the load address.
- **0x200000** — Kernel load and entry address (payload of ATOM.BIN).
- **0x1000** — `boot_info_t` filled by the bootloader (magic, board revision, ARM memory size, kernel load/entry/size, bootloader version string).
- **0x3F000000** — BCM2837 peripheral base (GPIO, UART0, SDHOST, mailbox, etc.).
- **0x3F201000** — PL011 UART0 (used for serial console).

---

## Configuration

Per-build options are set in **`build.cfg`** at the project root. Run `make` to regenerate `include/config.h` and `build.mk` from `build.cfg`. Key options:

- **`kernel_filename`** — Filename of the packed kernel on the FAT32 root (e.g. `"ATOM.BIN"` or `"CUSTOM.BIN"`). The Makefile uses this when creating the SD image.
- **`embed-kernel`** — If `true`, the packed kernel is embedded into `kernel8.img` and the bootloader boots without SD/FAT32; if `false`, the kernel is loaded from the SD card and `make all` also builds `sd.img`.

Other options: staging/load addresses, log level, ANSI colours, banner and version text. See [Neutron Developer's Manual](https://neutron-wiki.pages.dev) for the full key list and an example matching the current `build.cfg`.

---

## Contributions

Contributions are welcome. For system flow, memory layout, build pipeline, and guidelines see:

- [ARCHITECTURE.md](ARCHITECTURE.md)
- [CONTRIBUTING.md](CONTRIBUTING.md)
- [CODE-OF-CONDUCT.md](CODE-OF-CONDUCT.md)

All changes are accepted via Pull Requests.

---

<p align="center">Copyright &copy; 2026 <a href="https://github.com/serene-brew" target="_blank">Serene Brew</a>
<p align="center"><a href="https://github.com/serene-brew/Neutron/blob/main/LICENSE"><img src="https://img.shields.io/static/v1.svg?style=for-the-badge&label=License&message=BSD-3-clause&logoColor=d9e0ee&colorA=363a4f&colorB=b7bdf8"/></a></p>
