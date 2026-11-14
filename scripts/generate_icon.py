#!/usr/bin/env python3
"""Generate a 512x512 *24-bit RGB* PNG without transparency for PS4 icon0.png."""
import pathlib
import struct
import sys
import zlib

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)

def png():
    width = height = 512
    scanlines = bytearray()
    for y in range(height):
        scanlines.append(0)
        for x in range(width):
            glow = max(0, 80 - abs(x - y) // 4)
            scanlines.extend((8, 20 + glow // 2, 48 + glow))
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(scanlines), 9))
            + chunk(b"IEND", b""))

if __name__ == "__main__":
    dest = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "sce_sys/icon0.png")
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(png())
    print("Generated 24-bit opaque RGB PS4 icon:", dest)
