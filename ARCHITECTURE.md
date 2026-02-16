# Neutron Bootloader - Project Architecture

**Project**: Neutron Bootloader - Project Atom  
**Organization**: serene brew  
**Author**: mintRaven-05  
**License**: BSD-3-Clause  
**Target**: ARMv8 AArch64 bare-metal bootloader + test kernel for QEMU  
**Platform**: QEMU `-machine virt`

---

## Project Directory Structure

```mermaid
graph TD
    ROOT["Neutron/"]
    ROOT --> BOOT["boot/"]
    ROOT --> DRIVER["driver/"]
    ROOT --> INCLUDE["include/"]
    ROOT --> LINKER["linker/"]
    ROOT --> NEUTRON_DIR["neutron/"]
    ROOT --> TEST_KERNEL["test_kernel/"]
    
    BOOT --> BOOT_S["start.S<br/>Power-on Assembly"]
    
    DRIVER --> UART_C["uart.c<br/>UART Driver"]
    
    INCLUDE --> BOOTLOADER_H["bootloader.h<br/>Bootloader API"]
    INCLUDE --> UART_H["uart.h<br/>UART Header"]
    
    LINKER --> BOOTLOADER_LD["bootloader.ld<br/>Bootloader Layout"]
    LINKER --> KERNEL_LD["kernel.ld<br/>Kernel Layout"]
    
    NEUTRON_DIR --> BOOTLOADER_C["bootloader.c<br/>Kernel Loading"]
    NEUTRON_DIR --> MAIN_C["main.c<br/>Bootloader Entry"]
    
    TEST_KERNEL --> KERNEL_MAIN["kernel_main.c<br/>Kernel Entry"]
    TEST_KERNEL --> KERNEL_START["kernel_start.S<br/>Kernel Assembly"]
```

---

## Directory Overview

### **root/** — Project Root
- **Contains**: Build configuration and project metadata
- **Key Files**:
  - `Makefile` - Build system configuration for cross-compilation
  - `LICENSE` - BSD-3-Clause licensing information
- **Purpose**: Entry point for building both bootloader and test kernel

---

### **boot/** — Bootloader Assembly
- **Contains**: Power-on initialization code in assembly
- **Key Files**:
  - `start.S` - ARM assembly entry point executed immediately after CPU reset
- **Purpose**: 
  - CPU initialization and bare-metal setup
  - Sets up exception handlers
  - Jumps to bootloader main code (C code in `neutron/`)
- **Architecture**: ARMv8 AArch64

---

### **driver/** — Hardware Drivers
- **Contains**: Device driver implementations
- **Key Files**:
  - `uart.c` - PL011 UART driver for serial communication
- **Purpose**:
  - UART driver provides serial output (debug messages)
  - Located at `0x09000000` on QEMU virt machine
  - Supports 115200 baud rate
- **Used by**: Both bootloader and test kernel

---

### **include/** — Header Files
- **Contains**: Public API definitions and interfaces
- **Key Files**:
  - `bootloader.h` - Bootloader data structures and function declarations
  - `uart.h` - UART driver interface
- **Purpose**: Interface contracts between modules
- **Main Structures**:
  - `bootloader_info_t` - Bootloader information (DTB address, version, flags)

---

### **linker/** — Linker Scripts
- **Contains**: Memory layout definitions for both bootloader and kernel
- **Key Files**:
  - `bootloader.ld` - Memory layout for bootloader binary
  - `kernel.ld` - Memory layout for test kernel binary
- **Purpose**:
  - Defines memory sections (text, rodata, data, bss, stack)
  - Sets load addresses for executables
  - Bootloader typically loads at low memory (0x40000000+)
  - Kernel typically loads at (0x40200000+)

---

### **neutron/** — Bootloader Core
- **Contains**: Main bootloader implementation
- **Key Files**:
  - `main.c` - Bootloader entry point and main logic
  - `bootloader.c` - Kernel loading and jumping mechanism
- **Purpose**:
  - UART initialization for debug output
  - Bootloader information initialization with DTB address
  - Kernel image location and loading logic
  - Jump to kernel at 0x40200000 with DTB in x0 register
- **Key Responsibilities**:
  1. Print bootloader version and information
  2. Locate kernel image in storage/memory
  3. Load kernel to memory
  4. Jump to kernel with proper register setup

---

### **test_kernel/** — Test/Sample Kernel
- **Contains**: Minimal kernel for bootloader validation
- **Key Files**:
  - `kernel_main.c` - Kernel entry point (bare-metal minimal implementation)
  - `kernel_start.S` - Kernel assembly entry
  - `ascii.txt` - Test data
- **Purpose**:
  - Validates bootloader's kernel loading mechanism
  - Tests kernel execution in bootloader-provided environment
  - Minimal implementation showing UART communication
- **Functionality**:
  1. Initialize UART
  2. Print greeting message
  3. Infinite loop (proof of execution)

---
```mermaid
flowchart TD
    Start([Power On / CPU Reset]) --> ASM["Assembly Init<br/>(boot/start.S)<br/>- Setup CPU state<br/>- Configure exception handlers<br/>- Initialize stack"]
    
    ASM --> UART["Initialize UART<br/>(driver/uart.c)<br/>- Configure PL011 controller<br/>- Set baud rate 115200"]
    
    UART --> PRINT1["Print Debug Info<br/>- Bootloader version<br/>- System info"]
    
    PRINT1 --> INIT_BI["Initialize Bootloader Info<br/>- Store DTB address<br/>- Set bootloader version<br/>- Setup flags"]
    
    INIT_BI --> LOCATE["Locate Kernel Image at 0x40400000 (temp load from RAM)<br/>- Check memory at 0x40200000<br/>- Validate kernel header"]
    
    LOCATE --> VALIDATE{Kernel Valid?}
    
    VALIDATE -->|No| ERROR["Print Error<br/>- Kernel not found<br/>- Halt system"]
    ERROR --> HALT1([System Halt])
    
    VALIDATE -->|Yes| LOAD["Load Kernel<br/>- Copy kernel image<br/>- Setup kernel sections<br/>- Initialize kernel BSS"]
    
    LOAD --> SETUP_REG["Setup CPU Registers<br/>- x0 = DTB address<br/>- x1-x7 = reserved<br/>- Stack pointer setup"]
    
    SETUP_REG --> JUMP["Jump to Kernel<br/>at 0x40200000"]
    
    JUMP --> KERNEL_RUN["Kernel Execution<br/>(test_kernel/)<br/>- Bootloader done<br/>- Kernel takes control"]
    
    KERNEL_RUN --> END([Kernel Running])
```
## Build Pipeline

```mermaid
graph LR
    subgraph Sources
        AS["boot/start.S"]
        BC["neutron/bootloader.c"]
        MC["neutron/main.c"]
        UC["driver/uart.c"]
        KS["test_kernel/kernel_start.S"]
        KM["test_kernel/kernel_main.c"]
    end
    
    subgraph Compilation
        ASO["boot/start.o"]
        BCO["bootloader.o"]
        MCO["main.o"]
        UCO["uart.o"]
        KSO["kernel_start.o"]
        KMO["kernel_main.o"]
    end
    
    subgraph Linking
        BLLD["bootloader.ld"]
        KMLD["kernel.ld"]
        BLE["bootloader.elf"]
        KBE["kernel.elf"]
    end
    
    subgraph Finalization
        BLB["bootloader.bin"]
        KBB["kernel.bin"]
    end
    
    AS -->|aarch64-gcc| ASO
    BC -->|aarch64-gcc| BCO
    MC -->|aarch64-gcc| MCO
    UC -->|aarch64-gcc| UCO
    KS -->|aarch64-gcc| KSO
    KM -->|aarch64-gcc| KMO
    
    ASO -->|aarch64-ld + BLLD| BLE
    BCO --> BLE
    MCO --> BLE
    UCO --> BLE
    
    KSO -->|aarch64-ld + KMLD| KBE
    KMO --> KBE
    UCO --> KBE
    
    BLE -->|objcopy| BLB
    KBE -->|objcopy| KBB
    
    BLB --> QEMU["QEMU Simulation"]
    KBB --> QEMU
    
    style Sources fill:#e3f2fd
    style Compilation fill:#fff3e0
    style Linking fill:#f3e5f5
    style Finalization fill:#e8f5e9
    style QEMU fill:#fce4ec
```

---

## Memory Layout

### Bootloader Memory Map
```
0x40000000 +--------------------+
           | Bootloader Code    |  (start.S, main.c, bootloader.c)
           | BSS Segment        |  (uninitialized data)
           +--------------------+
           | Bootloader Stack   |
0x40100000 +--------------------+
           | (unused)           |
0x40200000 +--------------------+
           | Test Kernel Code   |  (kernel_start.S, kernel_main.c)
           | Kernel BSS         |
           | Kernel Stack       |
0x40300000 +--------------------+
           | DTB (Device Tree)  |  (provided by QEMU)
```

---

## Key Components & Responsibilities

| Component | Location | Responsibility |
|-----------|----------|-----------------|
| **CPU Init** | `boot/start.S` | Exception setup, CPU state initialization, memory fence |
| **Bootloader Main** | `neutron/main.c` | UART init, bootloader info, kernel discovery |
| **Kernel Loading** | `neutron/bootloader.c` | Load kernel image, validate, setup x0 (DTB), jump to kernel |
| **UART Driver** | `driver/uart.c` | PL011 initialization, character I/O for debugging |
| **Linker Scripts** | `linker/*.ld` | Memory section layout, symbol definitions, load addresses |
| **Test Kernel** | `test_kernel/kernel_main.c` | Accept control from bootloader, prove execution with output |

---

## Cross-Compilation Toolchain

The project uses AArch64 cross-compiler (auto-detected):
- **Options**: `aarch64-none-elf-*`, `aarch64-linux-gnu-*`, or `aarch64-elf-*`
- **Key Tools**:
  - `aarch64-*-gcc` - C compiler
  - `aarch64-*-as` - Assembler
  - `aarch64-*-ld` - Linker
  - `aarch64-*-objcopy` - Binary generator
  - `aarch64-*-objdump` - Disassembler

---

## References

- **QEMU virt machine**: UART at `0x09000000` (PL011)
- **ARM Documentation**: ARMv8 AArch64 ISA
- **QEMU Parameters**: `-machine virt -cpu cortex-a53 -m 256M`
- **License**: BSD-3-Clause (see LICENSE file)
