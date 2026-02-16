<div align=center>
<img width="500" height="500" alt="Neutron_Logo_for_Github" src="https://github.com/user-attachments/assets/8b634af5-5973-4994-9cdc-d485141269e3" />

### A Piece of Project Atom
</div>

---

ARMv8 AArch64 bare-metal bootloader for QEMU virt machine. Loads and transfers control to a test kernel.

## Overview

Neutron is a minimal yet functional bootloader that:

- Initializes CPU state on ARMv8 AArch64 architecture
- Sets up serial communication (UART PL011 at 0x09000000)
- Locates and validates kernel image in memory at 0x40400000 
- Loads kernel to predefined memory location (0x40200000)
- Transfers control with Device Tree Blob address in x0 register
- Provides debug output throughout execution

Designed for educational purposes and QEMU simulation.

---

## Quick Start

### Requirements

- AArch64 cross-compiler toolchain (auto-detected)
  - Arch: `sudo pacman -S aarch64-linux-gnu-gcc aarch64-linux-gnu-binutils`
  - Fedora: `sudo dnf install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu`
  - Ubuntu: `sudo apt install gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu`
- QEMU with ARM support
- GNU Make

### Building

```bash
make clean
make all #build both bootloader and the test kernel
make bootloader #compile just the bootloader
make kernel #compile just the kernel
```

This generates:
- `build/bootloader.bin` - Compiled bootloader
- `build/kernel.bin` - Compiled test kernel

### Running in QEMU

```bash
make qemu # for running just the bootloader
make qemu-kernel #for running the bootloader along with the testing kernel
```

---

## Project Structure

```
Neutron/
  boot/              - Power-on assembly initialization
  driver/            - Hardware drivers (UART)
  include/           - Header files and APIs
  linker/            - Memory layout scripts
  neutron/           - Bootloader main implementation
  test_kernel/       - Minimal test kernel
  Makefile           - Build configuration
  LICENSE            - BSD-3-Clause
```

See [architecture.md](architecture.md) for detailed documentation.

---

## Key Components

| Component | Role |
|-----------|------|
| boot/start.S | CPU initialization and exception setup |
| neutron/main.c | Bootloader entry point and UART init |
| neutron/bootloader.c | Kernel loading and transfer logic |
| driver/uart.c | PL011 UART driver for debug output |
| test_kernel/ | Minimal kernel for validation |

---

## Build Configuration

The Makefile auto-detects the AArch64 cross-compiler:
- `aarch64-none-elf-*`
- `aarch64-linux-gnu-*`
- `aarch64-elf-*`

Supported targets:
- `make` - Build bootloader and kernel
- `make clean` - Remove build artifacts
- `make distclean` - Full clean including built binaries

---

## Features

- Bare-metal bootloader for ARMv8 AArch64
- UART serial communication at 115200 baud
- Kernel validation before execution
- DTB (Device Tree Blob) support
- Debug output for troubleshooting
- Minimal test kernel for validation

---

## Contributions

Contributions are welcome.

For detailed system flow, memory layout, build pipeline, and component responsibilities, and guidelines please see:

* [`ARCHITECTURE.md`](ARCHITECTURE.md)
* [`CONTRIBUTING.md`](CONTRIBUTING.md)
* [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md)

All changes are accepted via Pull Requests.

---

<p align="center">Copyright &copy; 2026 <a href="https://github.com/serene-brew" target="_blank">Serene Brew</a>
<p align="center"><a href="https://github.com/serene-brew/Neutron/blob/main/LICENSE"><img src="https://img.shields.io/static/v1.svg?style=for-the-badge&label=License&message=BSD-3-clause&logoColor=d9e0ee&colorA=363a4f&colorB=b7bdf8"/></a></p>
