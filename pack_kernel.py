#!/usr/bin/env python3
"""
pack_kernel.py  -  Neutron Bootloader kernel image packer
=========================================================
Prepends a 64-byte NKRN header to a raw AArch64 binary so the
Neutron bootloader can validate, CRC-check, and load it.

Kernel image layout after packing:
  0x00  4B  Magic      "NKRN" (0x4E4B524E)
  0x04  4B  Version    major<<16 | minor
  0x08  4B  Load addr  where the bootloader copies the payload
  0x0C  4B  Entry addr first instruction to jump to
  0x10  4B  Image size payload byte count (not including header)
  0x14  4B  CRC32      IEEE 802.3 CRC of payload bytes only
  0x18 40B  Name       null-terminated OS name string
  0x40   -  Payload    your raw kernel binary

Memory map (must match platform.h in the bootloader):
  0x080000  Bootloader (kernel8.img) - loaded by GPU
  0x100000  Staging area             - QEMU loads kernel.bin here
  0x200000  Final load address       - bootloader copies payload here

Usage:
  python3 pack_kernel.py <raw_binary> [options]

Examples:
  python3 pack_kernel.py mykernel.bin
  python3 pack_kernel.py mykernel.bin -o kernel.bin -n "MyOS v0.1"
  python3 pack_kernel.py mykernel.bin --load 0x200000 --entry 0x200000
"""

import argparse
import struct
import zlib
import sys
import os

# ----------------------------------------------------------------
# Constants - must match bootloader's platform.h / bootloader.h
# ----------------------------------------------------------------
KERNEL_MAGIC       = 0x4E4B524E   # "NKRN"
KERNEL_HEADER_SIZE = 0x40         # 64 bytes
DEFAULT_LOAD_ADDR  = 0x00200000   # where bootloader copies payload
DEFAULT_ENTRY_ADDR = 0x00200000   # where bootloader jumps
DEFAULT_VERSION    = (1 << 16) | 0  # 1.0


def crc32_ieee(data: bytes) -> int:
    """IEEE 802.3 CRC32 - matches the bootloader's crc32() implementation."""
    return zlib.crc32(data) & 0xFFFFFFFF


def pack(raw: bytes, name: str, version: int,
         load_addr: int, entry_addr: int) -> bytes:
    """Return the complete packed kernel image (header + payload)."""

    payload_size = len(raw)
    crc          = crc32_ieee(raw)

    # Name field: 40 bytes, null-terminated, zero-padded
    name_bytes = name.encode('utf-8')[:39] + b'\x00'
    name_bytes = name_bytes.ljust(40, b'\x00')

    header = struct.pack('<IIIIII40s',
        KERNEL_MAGIC,
        version,
        load_addr,
        entry_addr,
        payload_size,
        crc,
        name_bytes,
    )

    assert len(header) == KERNEL_HEADER_SIZE, \
        f"Header size mismatch: {len(header)} != {KERNEL_HEADER_SIZE}"

    return header + raw


def verify(image: bytes) -> bool:
    """Verify magic and CRC of an already-packed image."""
    if len(image) < KERNEL_HEADER_SIZE:
        print("ERROR: image too small to contain a header")
        return False

    magic, version, load_addr, entry_addr, img_size, stored_crc = \
        struct.unpack_from('<IIIIII', image, 0)
    name = image[0x18:0x40].rstrip(b'\x00').decode('utf-8', errors='replace')

    if magic != KERNEL_MAGIC:
        print(f"ERROR: bad magic 0x{magic:08X} (expected 0x{KERNEL_MAGIC:08X})")
        return False

    payload = image[KERNEL_HEADER_SIZE : KERNEL_HEADER_SIZE + img_size]
    computed_crc = crc32_ieee(payload)

    print(f"  Magic      : 0x{magic:08X}  OK")
    print(f"  Version    : {version >> 16}.{version & 0xFFFF}")
    print(f"  Name       : {name}")
    print(f"  Load addr  : 0x{load_addr:08X}")
    print(f"  Entry addr : 0x{entry_addr:08X}")
    print(f"  Payload    : {img_size} bytes")
    print(f"  CRC32 exp  : 0x{stored_crc:08X}")
    print(f"  CRC32 calc : 0x{computed_crc:08X}  {'OK' if stored_crc == computed_crc else 'MISMATCH!'}")

    if stored_crc != computed_crc:
        print("ERROR: CRC32 mismatch - image is corrupt")
        return False

    return True


def main():
    parser = argparse.ArgumentParser(
        description='Pack a raw AArch64 binary with an NKRN header for the Neutron bootloader.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument('input',
        help='Raw input binary (e.g. mykernel.bin)')
    parser.add_argument('-o', '--output', default='kernel.bin',
        help='Output file name (default: kernel.bin)')
    parser.add_argument('-n', '--name', default='Neutron Kernel',
        help='Kernel name string, max 39 chars (default: "Neutron Kernel")')
    parser.add_argument('--load', default=hex(DEFAULT_LOAD_ADDR),
        help=f'Load address in hex (default: {hex(DEFAULT_LOAD_ADDR)})')
    parser.add_argument('--entry', default=hex(DEFAULT_ENTRY_ADDR),
        help=f'Entry address in hex (default: {hex(DEFAULT_ENTRY_ADDR)})')
    parser.add_argument('--version-major', type=int, default=1)
    parser.add_argument('--version-minor', type=int, default=0)
    parser.add_argument('--verify', action='store_true',
        help='Verify an existing packed image instead of packing')

    args = parser.parse_args()

    # ---- Verify mode ----
    if args.verify:
        print(f"Verifying {args.input} ...")
        with open(args.input, 'rb') as f:
            data = f.read()
        ok = verify(data)
        sys.exit(0 if ok else 1)

    # ---- Pack mode ----
    if not os.path.isfile(args.input):
        print(f"ERROR: input file not found: {args.input}")
        sys.exit(1)

    with open(args.input, 'rb') as f:
        raw = f.read()

    load_addr  = int(args.load,  0)
    entry_addr = int(args.entry, 0)
    version    = (args.version_major << 16) | (args.version_minor & 0xFFFF)

    packed = pack(raw, args.name, version, load_addr, entry_addr)

    with open(args.output, 'wb') as f:
        f.write(packed)

    crc = crc32_ieee(raw)
    print(f"Packed kernel image written to: {args.output}")
    print(f"  Name       : {args.name}")
    print(f"  Version    : {args.version_major}.{args.version_minor}")
    print(f"  Load addr  : {hex(load_addr)}")
    print(f"  Entry addr : {hex(entry_addr)}")
    print(f"  Payload    : {len(raw)} bytes")
    print(f"  CRC32      : 0x{crc:08X}")
    print(f"  Total size : {len(packed)} bytes  (header={KERNEL_HEADER_SIZE} + payload={len(raw)})")
    print()

if __name__ == '__main__':
    main()
